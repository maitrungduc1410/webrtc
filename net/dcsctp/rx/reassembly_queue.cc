/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/rx/reassembly_queue.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "net/dcsctp/common/internal_types.h"
#include "net/dcsctp/common/sequence_numbers.h"
#include "net/dcsctp/packet/chunk/forward_tsn_common.h"
#include "net/dcsctp/packet/data.h"
#include "net/dcsctp/public/dcsctp_handover_state.h"
#include "net/dcsctp/public/dcsctp_message.h"
#include "net/dcsctp/public/types.h"
#include "net/dcsctp/rx/interleaved_reassembly_streams.h"
#include "net/dcsctp/rx/reassembly_streams.h"
#include "net/dcsctp/rx/traditional_reassembly_streams.h"
#include "rtc_base/checks.h"
#include "rtc_base/containers/flat_set.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/str_join.h"
#include "rtc_base/strings/string_builder.h"

namespace dcsctp {
namespace {
std::unique_ptr<ReassemblyStreams> CreateStreams(
    absl::string_view log_prefix,
    ReassemblyStreams::OnAssembledMessage on_assembled_message,
    bool use_message_interleaving) {
  if (use_message_interleaving) {
    return std::make_unique<InterleavedReassemblyStreams>(
        log_prefix, std::move(on_assembled_message));
  }
  return std::make_unique<TraditionalReassemblyStreams>(
      log_prefix, std::move(on_assembled_message));
}
}  // namespace

ReassemblyQueue::ReassemblyQueue(absl::string_view log_prefix,
                                 size_t max_size_bytes,
                                 bool use_message_interleaving)
    : log_prefix_(log_prefix),
      max_size_bytes_(max_size_bytes),
      watermark_bytes_(max_size_bytes * kHighWatermarkLimit),
      streams_(CreateStreams(
          log_prefix_,
          [this](webrtc::ArrayView<const UnwrappedTSN> tsns,
                 DcSctpMessage message) {
            AddReassembledMessage(tsns, std::move(message));
          },
          use_message_interleaving)) {}

void ReassemblyQueue::Add(TSN tsn, Data data) {
  RTC_DLOG(LS_VERBOSE) << log_prefix_ << "added tsn=" << *tsn
                       << ", stream=" << *data.stream_id << ":" << *data.mid
                       << ":" << *data.fsn << ", type="
                       << (data.is_beginning && data.is_end ? "complete"
                           : data.is_beginning              ? "first"
                           : data.is_end                    ? "last"
                                                            : "middle");

  UnwrappedTSN unwrapped_tsn = tsn_unwrapper_.Unwrap(tsn);

  // If a stream reset has been received with a "sender's last assigned tsn" in
  // the future, the socket is in "deferred reset processing" mode and must
  // buffer chunks until it's exited.
  if (deferred_reset_streams_.has_value() &&
      unwrapped_tsn > deferred_reset_streams_->sender_last_assigned_tsn &&
      deferred_reset_streams_->streams.contains(data.stream_id)) {
    RTC_DLOG(LS_VERBOSE)
        << log_prefix_ << "Deferring chunk with tsn=" << *tsn
        << ", sid=" << *data.stream_id << " until tsn="
        << *deferred_reset_streams_->sender_last_assigned_tsn.Wrap();
    // https://tools.ietf.org/html/rfc6525#section-5.2.2
    // "In this mode, any data arriving with a TSN larger than the
    // Sender's Last Assigned TSN for the affected stream(s) MUST be queued
    // locally and held until the cumulative acknowledgment point reaches the
    // Sender's Last Assigned TSN."
    queued_bytes_ += data.size();
    deferred_reset_streams_->deferred_actions.push_back(
        [this, tsn, data = std::move(data)]() mutable {
          queued_bytes_ -= data.size();
          Add(tsn, std::move(data));
        });
  } else {
    queued_bytes_ += streams_->Add(unwrapped_tsn, std::move(data));
  }

  // https://tools.ietf.org/html/rfc4960#section-6.9
  // "Note: If the data receiver runs out of buffer space while still
  // waiting for more fragments to complete the reassembly of the message, it
  // should dispatch part of its inbound message through a partial delivery
  // API (see Section 10), freeing some of its receive buffer space so that
  // the rest of the message may be received."

  // TODO(boivie): Support EOR flag and partial delivery?
  RTC_DCHECK(IsConsistent());
}

void ReassemblyQueue::ResetStreamsAndLeaveDeferredReset(
    webrtc::ArrayView<const StreamID> stream_ids) {
  RTC_DLOG(LS_VERBOSE) << log_prefix_ << "Resetting streams: ["
                       << webrtc::StrJoin(stream_ids, ",",
                                          [](webrtc::StringBuilder& sb,
                                             StreamID sid) { sb << *sid; })
                       << "]";

  // https://tools.ietf.org/html/rfc6525#section-5.2.2
  // "... streams MUST be reset to 0 as the next expected SSN."
  streams_->ResetStreams(stream_ids);

  if (deferred_reset_streams_.has_value()) {
    RTC_DLOG(LS_VERBOSE) << log_prefix_
                         << "Leaving deferred reset processing, feeding back "
                         << deferred_reset_streams_->deferred_actions.size()
                         << " actions";
    // https://tools.ietf.org/html/rfc6525#section-5.2.2
    // "Any queued TSNs (queued at step E2) MUST now be released and processed
    // normally."
    auto deferred_actions =
        std::move(deferred_reset_streams_->deferred_actions);
    deferred_reset_streams_ = std::nullopt;

    for (auto& action : deferred_actions) {
      action();
    }
  }

  RTC_DCHECK(IsConsistent());
}

void ReassemblyQueue::EnterDeferredReset(
    TSN sender_last_assigned_tsn,
    webrtc::ArrayView<const StreamID> streams) {
  if (!deferred_reset_streams_.has_value()) {
    RTC_DLOG(LS_VERBOSE) << log_prefix_
                         << "Entering deferred reset; sender_last_assigned_tsn="
                         << *sender_last_assigned_tsn;
    deferred_reset_streams_ = std::make_optional<DeferredResetStreams>(
        tsn_unwrapper_.Unwrap(sender_last_assigned_tsn),
        webrtc::flat_set<StreamID>(streams.begin(), streams.end()));
  }
  RTC_DCHECK(IsConsistent());
}

std::optional<DcSctpMessage> ReassemblyQueue::GetNextMessage() {
  if (reassembled_messages_.empty()) {
    return std::nullopt;
  }
  DcSctpMessage ret = std::move(reassembled_messages_.front());
  reassembled_messages_.pop_front();
  queued_bytes_ -= ret.payload().size();
  return ret;
}

void ReassemblyQueue::AddReassembledMessage(
    webrtc::ArrayView<const UnwrappedTSN> tsns,
    DcSctpMessage message) {
  RTC_DLOG(LS_VERBOSE) << log_prefix_ << "Assembled message from TSN=["
                       << webrtc::StrJoin(
                              tsns, ",",
                              [](webrtc::StringBuilder& sb, UnwrappedTSN tsn) {
                                sb << *tsn.Wrap();
                              })
                       << "], message; stream_id=" << *message.stream_id()
                       << ", ppid=" << *message.ppid()
                       << ", payload=" << message.payload().size() << " bytes";

  queued_bytes_ += message.payload().size();
  reassembled_messages_.emplace_back(std::move(message));
}

void ReassemblyQueue::HandleForwardTsn(
    TSN new_cumulative_tsn,
    webrtc::ArrayView<const AnyForwardTsnChunk::SkippedStream>
        skipped_streams) {
  UnwrappedTSN tsn = tsn_unwrapper_.Unwrap(new_cumulative_tsn);

  if (deferred_reset_streams_.has_value() &&
      tsn > deferred_reset_streams_->sender_last_assigned_tsn) {
    RTC_DLOG(LS_VERBOSE) << log_prefix_ << "ForwardTSN to " << *tsn.Wrap()
                         << "- deferring.";
    deferred_reset_streams_->deferred_actions.emplace_back(
        [this, new_cumulative_tsn,
         streams = std::vector<AnyForwardTsnChunk::SkippedStream>(
             skipped_streams.begin(), skipped_streams.end())] {
          HandleForwardTsn(new_cumulative_tsn, streams);
        });
    RTC_DCHECK(IsConsistent());
    return;
  }

  RTC_DLOG(LS_VERBOSE) << log_prefix_ << "ForwardTSN to " << *tsn.Wrap()
                       << " - performing.";
  queued_bytes_ -= streams_->HandleForwardTsn(tsn, skipped_streams);
  RTC_DCHECK(IsConsistent());
}

bool ReassemblyQueue::IsConsistent() const {
  // Allow queued_bytes_ to be larger than max_size_bytes, as it's not actively
  // enforced in this class. But in case it wraps around (becomes negative, but
  // as it's unsigned, that would wrap to very big), this would trigger.
  return (queued_bytes_ <= 2 * max_size_bytes_);
}

HandoverReadinessStatus ReassemblyQueue::GetHandoverReadiness() const {
  HandoverReadinessStatus status = streams_->GetHandoverReadiness();
  if (deferred_reset_streams_.has_value()) {
    status.Add(HandoverUnreadinessReason::kStreamResetDeferred);
  }
  return status;
}

void ReassemblyQueue::AddHandoverState(DcSctpSocketHandoverState& state) {
  streams_->AddHandoverState(state);
}

void ReassemblyQueue::RestoreFromState(const DcSctpSocketHandoverState& state) {
  streams_->RestoreFromState(state);
}
}  // namespace dcsctp
