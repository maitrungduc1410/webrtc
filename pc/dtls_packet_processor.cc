/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "pc/dtls_packet_processor.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>

#include "api/environment/environment.h"
#include "api/transport/ecn_marking.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/ntp_time_util.h"
#include "pc/congestion_control_feedback_block.h"
#include "pc/congestion_control_feedback_report.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

// Data header: 1 byte type + 2 byte transport sequence number.
constexpr size_t kDataHeaderSize = 3;

// Feedback goes out at least every kMaxFeedbackInterval, even with nothing to
// report, so that a quiet path is distinguishable from a dead one.
constexpr TimeDelta kMinFeedbackInterval = TimeDelta::Millis(25);
constexpr TimeDelta kMaxFeedbackInterval = TimeDelta::Millis(250);

// Feedback packets fit the MTU budget; any excess goes in the next report.
constexpr size_t kMtuBudgetBytes = 1200;
constexpr size_t kMaxFeedbackBlocksPerPacket =
    (kMtuBudgetBytes - 1 - CongestionControlFeedbackReport::kHeaderBytes) /
    CongestionControlFeedbackBlock::kBytes;

// One MTU per kMinFeedbackInterval stays within the 500 kbit/s default budget
// of RTCP congestion control feedback, so no further rate limit is needed.
static_assert(kMtuBudgetBytes * 8 * 1000 / kMinFeedbackInterval.ms() < 500'000,
              "Feedback could exceed the RTCP bandwidth budget");

}  // namespace

DtlsPacketProcessor::DtlsPacketProcessor(const Environment& env,
                                         TransportPacketSender packet_sender)
    : env_(env),
      packet_sender_(std::move(packet_sender)),
      last_feedback_time_(env.clock().CurrentTime()) {}

CopyOnWriteBuffer DtlsPacketProcessor::ProcessOutgoingPacket(
    std::span<const uint8_t> payload,
    Timestamp send_time) {
  const uint16_t seq = next_transport_seq_++;
  const std::optional<CongestionControlFeedbackReport> report =
      TakeFeedbackToPiggyback(send_time, payload.size());
  if (!report.has_value()) {
    // Feedback that is due but does not fit this packet is sent on its own.
    SendFeedbackIfDue(send_time);
  }

  const size_t header_size =
      kDataHeaderSize + (report.has_value() ? report->SizeBytes() : 0);
  CopyOnWriteBuffer out(/*size=*/header_size,
                        /*capacity=*/header_size + payload.size());
  const std::span<uint8_t> header(out.MutableData(), header_size);
  header[0] =
      report.has_value() ? kPacketTypeDataWithFeedback : kPacketTypeData;
  ByteWriter<uint16_t>::WriteBigEndian(&header[1], seq);
  if (report.has_value()) {
    report->Encode(header.subspan(kDataHeaderSize));
  }
  out.AppendData(payload.data(), payload.size());
  return out;
}

std::optional<CopyOnWriteBuffer> DtlsPacketProcessor::ProcessIncomingPacket(
    CopyOnWriteBuffer packet,
    Timestamp receive_time,
    EcnMarking ecn) {
  if (packet.empty()) {
    return std::nullopt;
  }
  const uint8_t type = packet.cdata()[0];
  const bool has_sequence_number =
      type == kPacketTypeData || type == kPacketTypeDataWithFeedback;
  const bool has_feedback =
      type == kPacketTypeFeedback || type == kPacketTypeDataWithFeedback;
  if (!has_sequence_number && !has_feedback) {
    RTC_LOG(LS_WARNING) << "Dropping DTLS packet with reserved type "
                        << static_cast<int>(type);
    return std::nullopt;
  }

  std::span<const uint8_t> remaining =
      std::span<const uint8_t>(packet.cdata(), packet.size()).subspan(1);
  uint16_t transport_seq = 0;
  if (has_sequence_number) {
    if (remaining.size() < sizeof(transport_seq)) {
      RTC_LOG(LS_WARNING) << "Dropping truncated DTLS data packet";
      return std::nullopt;
    }
    transport_seq = ByteReader<uint16_t>::ReadBigEndian(remaining.data());
    remaining = remaining.subspan(sizeof(transport_seq));
  }
  if (has_feedback) {
    // Decode() bounds-checks the report. A feedback-only packet must hold
    // nothing else.
    const std::optional<CongestionControlFeedbackReport> report =
        CongestionControlFeedbackReport::Decode(remaining);
    if (!report.has_value() ||
        (!has_sequence_number && report->SizeBytes() != remaining.size())) {
      RTC_LOG(LS_WARNING)
          << "Dropping DTLS packet with malformed congestion control feedback";
      return std::nullopt;
    }
    remaining = remaining.subspan(report->SizeBytes());
    // TODO(crbug.com/443019066): Act on the peer's report on our packets.
  }
  if (!has_sequence_number) {
    return std::nullopt;
  }

  // Feedback is timed by the clock, not `receive_time`, as in ProcessTimers().
  const Timestamp now = env_.clock().CurrentTime();
  reporter_.OnPacketReceived(transport_seq,
                             receive_time.IsFinite() ? receive_time : now, ecn);
  SendFeedbackIfDue(now);

  // Zero-copy view of the app payload (extends to the end of the record).
  return packet.Slice(packet.size() - remaining.size(), remaining.size());
}

TimeDelta DtlsPacketProcessor::ProcessTimers(Timestamp now) {
  SendFeedbackIfDue(now);
  // A packet arriving before the next call can make feedback due sooner, so
  // wait no longer than kMinFeedbackInterval.
  return std::min(NextFeedbackTime() - now, kMinFeedbackInterval);
}

Timestamp DtlsPacketProcessor::NextFeedbackTime() const {
  return last_feedback_time_ + (reporter_.PendingReportBlocks() > 0
                                    ? kMinFeedbackInterval
                                    : kMaxFeedbackInterval);
}

uint32_t DtlsPacketProcessor::ReportTimestamp(Timestamp now) const {
  return CompactNtp(env_.clock().ConvertTimestampToNtpTime(now));
}

std::optional<CongestionControlFeedbackReport>
DtlsPacketProcessor::TakeFeedbackToPiggyback(Timestamp send_time,
                                             size_t payload_size) {
  const size_t overhead =
      kDataHeaderSize + CongestionControlFeedbackReport::kHeaderBytes;
  if (send_time < NextFeedbackTime() ||
      payload_size + overhead > kMtuBudgetBytes) {
    return std::nullopt;
  }
  const size_t max_report_blocks = (kMtuBudgetBytes - overhead - payload_size) /
                                   CongestionControlFeedbackBlock::kBytes;
  if (reporter_.PendingReportBlocks() > max_report_blocks) {
    return std::nullopt;
  }

  CongestionControlFeedbackReport report = reporter_.CreateReport(
      send_time, ReportTimestamp(send_time), max_report_blocks);
  last_feedback_time_ = send_time;
  return report;
}

void DtlsPacketProcessor::SendFeedbackIfDue(Timestamp now) {
  if (now < NextFeedbackTime()) {
    return;
  }
  CongestionControlFeedbackReport report = reporter_.CreateReport(
      now, ReportTimestamp(now), kMaxFeedbackBlocksPerPacket);

  CopyOnWriteBuffer packet(/*size=*/1 + report.SizeBytes());
  const std::span<uint8_t> wire(packet.MutableData(), packet.size());
  wire[0] = kPacketTypeFeedback;
  report.Encode(wire.subspan(1));

  last_feedback_time_ = now;
  packet_sender_(std::move(packet));
}

}  // namespace webrtc
