/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef PC_CONGESTION_CONTROL_FEEDBACK_REPORTER_H_
#define PC_CONGESTION_CONTROL_FEEDBACK_REPORTER_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "api/transport/ecn_marking.h"
#include "api/units/timestamp.h"
#include "pc/congestion_control_feedback_report.h"
#include "rtc_base/numerics/sequence_number_unwrapper.h"

namespace webrtc {

// Receiver side of the congestion control feedback of
// WireProtocol::kDtlsWithFeedback: records packet arrivals by transport
// sequence number and turns them into reports. Consecutive reports cover
// consecutive sequence numbers, except that a packet arriving after the report
// covering it extends the next report back over it.
class CongestionControlFeedbackReporter {
 public:
  // According to RFC 8888 section 3.1, it must not report on more than one
  // quarter of the sequence number space in a single feedback report.
  static constexpr size_t kMaxReportBlocks = 16384;

  // Number of blocks in the next report, unless `max_report_blocks` caps it.
  size_t PendingReportBlocks() const;

  void OnPacketReceived(uint16_t transport_seq,
                        Timestamp arrival_time,
                        EcnMarking ecn);

  // `report_timestamp` is `report_time` in compact NTP format. Sequence
  // numbers beyond `max_report_blocks` are left to the next report.
  CongestionControlFeedbackReport CreateReport(
      Timestamp report_time,
      uint32_t report_timestamp,
      size_t max_report_blocks = kMaxReportBlocks);

 private:
  struct Arrival {
    bool received() const { return arrival_time.IsFinite(); }

    Timestamp arrival_time = Timestamp::MinusInfinity();
    EcnMarking ecn = EcnMarking::kNotEct;
  };

  // Returns null if `sequence_number` is outside the tracked range. Tracking
  // restarts at a jump forward if nothing is left to report.
  Arrival* FindOrCreateArrival(int64_t sequence_number);

  SeqNumUnwrapper<uint16_t> unwrapper_;
  // Entry `i` describes sequence number `first_sequence_number_ + i`.
  std::vector<Arrival> arrivals_;
  int64_t first_sequence_number_ = 0;
  int64_t next_sequence_number_to_report_ = 0;
  // Packets outside the tracked range since the last report.
  int ignored_packets_ = 0;
  uint8_t feedback_count_ = 0;
};

}  // namespace webrtc

#endif  // PC_CONGESTION_CONTROL_FEEDBACK_REPORTER_H_
