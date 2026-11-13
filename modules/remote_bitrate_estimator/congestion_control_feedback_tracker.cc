
/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/remote_bitrate_estimator/congestion_control_feedback_tracker.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

#include "api/transport/ecn_marking.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/source/rtcp_packet/congestion_control_feedback.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

constexpr int kMaxPacketsPerSsrc = 16384;
constexpr int kMaxPacketsToKeepForReorderCalculation = 64;
// Size of the header of the report block of one SSRC: SSRC (4), begin_seq (2)
// and num_reports (2).
constexpr DataSize kReportBlockHeaderSize = DataSize::Bytes(8);

// Size of the report block of one SSRC with `num_reports` packets, two bytes
// per packet, padded to a multiple of four bytes.
DataSize ReportBlockSize(int64_t num_reports) {
  if (num_reports <= 0) {
    return DataSize::Zero();
  }
  return kReportBlockHeaderSize + DataSize::Bytes((num_reports + 1) / 2 * 4);
}

int64_t MaxReportsThatFit(DataSize max_size) {
  if (max_size.IsInfinite()) {
    return kMaxPacketsPerSsrc;
  }
  if (max_size < kReportBlockHeaderSize + DataSize::Bytes(4)) {
    return 0;
  }
  return std::min<int64_t>((max_size - kReportBlockHeaderSize).bytes() / 4 * 2,
                           kMaxPacketsPerSsrc);
}

}  // namespace

CongestionControlFeedbackTracker::PacketInfo*
CongestionControlFeedbackTracker::FindOrCreatePacketInfo(
    int64_t sequence_number,
    int64_t max_reports) {
  if (packets_.empty()) {
    if (max_reports == 0) {
      // Can happen when receiving the first packet for this SSRC if other SSRCs
      // have already filled the feedback packet.
      return nullptr;
    }
    // First packet ever, or sequence number reset.
    // To avoid special logic to distinguish out of order packets at the start
    // of the call, and too old packets in the middle of a call, ensure
    // `packets_.size()` is always >= `kMaxPacketsToKeepForReorderCalculation`
    // once at least one packet is received.
    packets_.resize(kMaxPacketsToKeepForReorderCalculation);
    first_sequence_number_in_packets_ =
        sequence_number - kMaxPacketsToKeepForReorderCalculation + 1;
    next_sequence_number_in_feedback_ = sequence_number;
    return &packets_.back();
  }

  if (sequence_number < first_sequence_number_in_packets_) {
    RTC_LOG(LS_VERBOSE) << "Received too old packet ssrc:" << ssrc_
                        << " seq:" << sequence_number << ". Expected seq >= "
                        << first_sequence_number_in_packets_
                        << ". Ignoring the packet.";
    ++num_ignored_packets_since_last_feedback_;
    return nullptr;
  }

  if (sequence_number >=
      first_sequence_number_in_packets_ + std::ssize(packets_)) {
    int64_t new_size = sequence_number - first_sequence_number_in_packets_ + 1;
    if (new_size > kMaxPacketsPerSsrc) {
      RTC_LOG(LS_VERBOSE)
          << "Received too new packet ssrc:" << ssrc_
          << " seq:" << sequence_number
          << " that would increase number of packet to report to " << new_size
          << " from current " << packets_.size()
          << ", first seq:" << first_sequence_number_in_packets_
          << ". Ignoring the packet.";
      ++num_ignored_packets_since_last_feedback_;
      return nullptr;
    }
    int64_t max_size = next_sequence_number_in_feedback_ + max_reports -
                       first_sequence_number_in_packets_;
    if (new_size > max_size) {
      // Fill the remaining space in the current feedback packet with lost
      // packets so the caller can send it and retry recording `sequence_number`
      // in the next feedback packet.
      packets_.resize(max_size);
      return nullptr;
    }

    packets_.resize(new_size);
    return &packets_.back();
  }

  // `PacketInfo` for `sequence_number` already exists.
  return &packets_[sequence_number - first_sequence_number_in_packets_];
}

std::optional<DataSize> CongestionControlFeedbackTracker::ReceivedPacket(
    const RtpPacketReceived& packet,
    DataSize remaining_size) {
  RTC_DCHECK_EQ(packet.Ssrc(), ssrc_);
  DataSize prev_size = FeedbackBlockSize();
  int64_t max_reports = MaxReportsThatFit(prev_size + remaining_size);
  int prev_ignored = num_ignored_packets_since_last_feedback_;

  int64_t sequence_number = unwrapper_.Unwrap(packet.SequenceNumber());
  PacketInfo* info = FindOrCreatePacketInfo(sequence_number, max_reports);
  if (info == nullptr) {
    if (num_ignored_packets_since_last_feedback_ > prev_ignored) {
      return DataSize::Zero();
    }
    return std::nullopt;
  }

  if (info->received()) {
    // Duplicate copy of an RTP packet is received. According to RFC8888:
    // arrival time of the first copy to arrive MUST be reported.
    // If any of the copies of the duplicated packet are ECN-CE marked, then an
    // ECN-CE mark MUST be reported for that packet; otherwise, the ECN mark of
    // the first copy to arrive is reported.
    if (packet.ecn() != EcnMarking::kCe || info->ecn == EcnMarking::kCe) {
      // No information to report is updated for this packet, so no need to
      // try to report it in the next feedback message.
      return DataSize::Zero();
    }
  }

  // Newly received packet, or new information about an old packet - ensure such
  // new information is included in the next report.
  if (sequence_number < next_sequence_number_in_feedback_) {
    int64_t needed_reports = first_sequence_number_in_packets_ +
                             std::ssize(packets_) - sequence_number;
    if (needed_reports > max_reports) {
      return std::nullopt;
    }
    RTC_LOG(LS_WARNING)
        << "Received packet unorderered between feeedback. SSRC: "
        << packet.Ssrc() << " Seq: " << packet.SequenceNumber()
        << " last feedback: " << next_sequence_number_in_feedback_;
    next_sequence_number_in_feedback_ = sequence_number;
  }

  if (!info->received()) {
    // Packet was received for the 1st time.
    info->arrival_time = packet.arrival_time();
  }
  info->ecn = packet.ecn();
  return FeedbackBlockSize() - prev_size;
}

DataSize CongestionControlFeedbackTracker::FeedbackBlockSize() const {
  if (packets_.empty()) {
    return DataSize::Zero();
  }
  RTC_DCHECK_GE(next_sequence_number_in_feedback_,
                first_sequence_number_in_packets_);
  RTC_DCHECK_LE(next_sequence_number_in_feedback_,
                first_sequence_number_in_packets_ + std::ssize(packets_));
  return ReportBlockSize(first_sequence_number_in_packets_ +
                         std::ssize(packets_) -
                         next_sequence_number_in_feedback_);
}

void CongestionControlFeedbackTracker::AddPacketsToFeedback(
    Timestamp feedback_time,
    std::vector<rtcp::CongestionControlFeedback::PacketInfo>& packet_feedback) {
  if (packets_.empty()) {
    // No packets received since last reset.
    return;
  }

  RTC_DCHECK_GE(next_sequence_number_in_feedback_,
                first_sequence_number_in_packets_);
  RTC_DCHECK_LE(next_sequence_number_in_feedback_,
                first_sequence_number_in_packets_ + std::ssize(packets_));

  if (next_sequence_number_in_feedback_ ==
      first_sequence_number_in_packets_ + std::ssize(packets_)) {
    // No packets to report received since last produced feedback.
    if (num_ignored_packets_since_last_feedback_ > 0) {
      // There were packets received, but all of them were discarared due to
      // reorder. That likely indicates sequence number reset. Reset the state
      // so that next feedback could be produced.
      RTC_LOG(LS_WARNING)
          << num_ignored_packets_since_last_feedback_
          << " received packets were discarded while no packets were accepted "
             "to produce feedback for SSRC: "
          << ssrc_
          << ". Assuming sequence numbers were reset, "
             "reset state and next sequence number in feedback from "
          << next_sequence_number_in_feedback_;
      // Clear packets_, rest of the state will be reset on first packet
      // arrived after that.
      packets_.clear();
      num_ignored_packets_since_last_feedback_ = 0;
    }
    return;
  }
  num_ignored_packets_since_last_feedback_ = 0;

  uint16_t rtp_sequence_number =
      static_cast<uint16_t>(next_sequence_number_in_feedback_);
  for (auto it = packets_.begin() + (next_sequence_number_in_feedback_ -
                                     first_sequence_number_in_packets_);
       it != packets_.end(); ++it, ++rtp_sequence_number) {
    PacketInfo& info = *it;

    if (!info.received() && !info.last_reported_as_lost) {
      ++stats_.num_packets_reported_lost;
      info.last_reported_as_lost = true;
    }
    if (info.received() && info.last_reported_as_lost) {
      ++stats_.num_packets_reported_recovered;
      info.last_reported_as_lost = false;
    }

    packet_feedback.push_back(
        {.ssrc = ssrc_,
         .sequence_number = rtp_sequence_number,
         .arrival_time_offset = info.received()
                                    ? feedback_time - info.arrival_time
                                    : TimeDelta::MinusInfinity(),
         .ecn = info.ecn});
  }

  next_sequence_number_in_feedback_ =
      first_sequence_number_in_packets_ + std::ssize(packets_);

  // Reduce `packets_` to store just `kMaxKeepForReorderCalculation` latest
  // entries.
  int64_t num_elements_to_erase =
      std::ssize(packets_) - kMaxPacketsToKeepForReorderCalculation;
  first_sequence_number_in_packets_ += num_elements_to_erase;
  packets_.erase(packets_.begin(), packets_.begin() + num_elements_to_erase);
}

}  // namespace webrtc
