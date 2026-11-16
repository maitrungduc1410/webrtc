/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "pc/congestion_control_feedback_reporter.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>

#include "api/transport/ecn_marking.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "pc/congestion_control_feedback_report.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

// Blocks kept after a report, so that a packet arriving late can still be
// reported.
constexpr int64_t kReorderWindowBlocks = 64;

}  // namespace

size_t CongestionControlFeedbackReporter::PendingReportBlocks() const {
  return static_cast<size_t>(first_sequence_number_ + std::ssize(arrivals_) -
                             next_sequence_number_to_report_);
}

CongestionControlFeedbackReporter::Arrival*
CongestionControlFeedbackReporter::FindOrCreateArrival(
    int64_t sequence_number) {
  const int64_t index = sequence_number - first_sequence_number_;
  if (!arrivals_.empty() && index >= int64_t{kMaxReportBlocks} &&
      PendingReportBlocks() == 0) {
    // A jump forward, as after a long outage. Everything tracked has been
    // reported, so track afresh from this packet.
    RTC_LOG(LS_WARNING) << "Restarting feedback tracking after a jump of "
                        << sequence_number - next_sequence_number_to_report_
                        << " transport sequence numbers";
    arrivals_.clear();
  }
  if (arrivals_.empty()) {
    arrivals_.resize(kReorderWindowBlocks);
    first_sequence_number_ = sequence_number - kReorderWindowBlocks + 1;
    next_sequence_number_to_report_ = sequence_number;
    return &arrivals_.back();
  }

  if (index < 0 || index >= int64_t{kMaxReportBlocks}) {
    return nullptr;
  }
  if (index >= std::ssize(arrivals_)) {
    arrivals_.resize(index + 1);
  }
  return &arrivals_[index];
}

void CongestionControlFeedbackReporter::OnPacketReceived(uint16_t transport_seq,
                                                         Timestamp arrival_time,
                                                         EcnMarking ecn) {
  RTC_CHECK(arrival_time.IsFinite());
  const int64_t sequence_number = unwrapper_.PeekUnwrap(transport_seq);
  Arrival* arrival = FindOrCreateArrival(sequence_number);
  if (arrival == nullptr) {
    ++ignored_packets_;
    return;
  }
  unwrapper_.Unwrap(transport_seq);

  if (arrival->received()) {
    // RFC 8888 section 3.1: report the arrival time of the first copy of a
    // duplicated packet, and CE if any copy was CE marked.
    if (ecn != EcnMarking::kCe || arrival->ecn == EcnMarking::kCe) {
      return;
    }
    arrival->ecn = EcnMarking::kCe;
  } else {
    arrival->arrival_time = arrival_time;
    arrival->ecn = ecn;
  }

  // If a report already covered the packet, as lost or not CE marked, the next
  // one covers it again.
  next_sequence_number_to_report_ =
      std::min(next_sequence_number_to_report_, sequence_number);
}

CongestionControlFeedbackReport CongestionControlFeedbackReporter::CreateReport(
    Timestamp report_time,
    uint32_t report_timestamp,
    size_t max_report_blocks) {
  if (ignored_packets_ > 0) {
    RTC_LOG(LS_WARNING) << "Ignored " << ignored_packets_
                        << " packets outside the tracked transport sequence "
                           "numbers since the last report";
    if (PendingReportBlocks() == 0) {
      // Every packet since the last report was too old, as after a jump of
      // over half the sequence number space, so track afresh from the next.
      arrivals_.clear();
      first_sequence_number_ = next_sequence_number_to_report_;
    }
    ignored_packets_ = 0;
  }

  const int64_t end_sequence_number =
      std::min(first_sequence_number_ + std::ssize(arrivals_),
               next_sequence_number_to_report_ +
                   static_cast<int64_t>(max_report_blocks));
  RTC_CHECK_LE(next_sequence_number_to_report_, end_sequence_number);

  CongestionControlFeedbackReport report;
  report.feedback_count = feedback_count_++;
  report.report_timestamp = report_timestamp;
  report.begin_seq = static_cast<uint16_t>(next_sequence_number_to_report_);
  report.blocks.reserve(static_cast<size_t>(end_sequence_number -
                                            next_sequence_number_to_report_));
  for (int64_t sequence_number = next_sequence_number_to_report_;
       sequence_number < end_sequence_number; ++sequence_number) {
    const int64_t index = sequence_number - first_sequence_number_;
    RTC_CHECK_GE(index, 0);
    RTC_CHECK_LT(index, std::ssize(arrivals_));
    const Arrival& arrival = arrivals_[index];
    report.blocks.push_back(
        {.received = arrival.received(),
         .ecn = arrival.ecn,
         .arrival_time_offset = arrival.received()
                                    ? report_time - arrival.arrival_time
                                    : TimeDelta::Zero()});
  }
  next_sequence_number_to_report_ = end_sequence_number;

  const int64_t discarded = next_sequence_number_to_report_ -
                            kReorderWindowBlocks - first_sequence_number_;
  if (discarded > 0) {
    arrivals_.erase(arrivals_.begin(), arrivals_.begin() + discarded);
    first_sequence_number_ += discarded;
  }
  return report;
}

}  // namespace webrtc
