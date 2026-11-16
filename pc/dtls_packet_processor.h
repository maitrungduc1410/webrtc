/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef PC_DTLS_PACKET_PROCESSOR_H_
#define PC_DTLS_PACKET_PROCESSOR_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "api/environment/environment.h"
#include "api/transport/ecn_marking.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "pc/congestion_control_feedback_report.h"
#include "pc/congestion_control_feedback_reporter.h"
#include "pc/packet_processor.h"
#include "rtc_base/copy_on_write_buffer.h"

namespace webrtc {

// Implements the wire format of the DTLS-only RtcTransport mode
// (WireProtocol::kDtlsWithFeedback).
//
// Every DTLS payload begins with a 1-byte packet type:
//   0x01 Data only     : [type][seq(2)][app payload]
//   0x02 Feedback only : [type][feedback report]
//   0x03 Data+feedback : [type][seq(2)][feedback report][app payload]
// Every other type is dropped with a warning.
//
// The sender wraps each application payload in a transport header via
// ProcessOutgoingPacket(); the receiver strips it via ProcessIncomingPacket(),
// which returns the application payload (sharing storage with the input, no
// copy). All methods run on the network thread.
//
// Feedback that is due rides on an outgoing data packet when it fits, and is
// otherwise sent on its own through `packet_sender`.
class DtlsPacketProcessor : public PacketProcessor {
 public:
  DtlsPacketProcessor(const Environment& env,
                      TransportPacketSender packet_sender);
  ~DtlsPacketProcessor() override = default;

  DtlsPacketProcessor(const DtlsPacketProcessor&) = delete;
  DtlsPacketProcessor& operator=(const DtlsPacketProcessor&) = delete;

  // PacketProcessor implementation.
  CopyOnWriteBuffer ProcessOutgoingPacket(std::span<const uint8_t> payload,
                                          Timestamp send_time) override;
  std::optional<CopyOnWriteBuffer> ProcessIncomingPacket(
      CopyOnWriteBuffer packet,
      Timestamp receive_time,
      EcnMarking ecn) override;
  TimeDelta ProcessTimers(Timestamp now) override;

  // Wire format packet types.
  static constexpr uint8_t kPacketTypeData = 0x01;
  static constexpr uint8_t kPacketTypeFeedback = 0x02;
  static constexpr uint8_t kPacketTypeDataWithFeedback = 0x03;

 private:
  Timestamp NextFeedbackTime() const;
  uint32_t ReportTimestamp(Timestamp now) const;
  // Returns the due report to piggyback on a packet carrying `payload_size`
  // bytes, or nullopt if none is due or it does not fit.
  std::optional<CongestionControlFeedbackReport> TakeFeedbackToPiggyback(
      Timestamp send_time,
      size_t payload_size);
  void SendFeedbackIfDue(Timestamp now);

  const Environment env_;
  TransportPacketSender packet_sender_;
  CongestionControlFeedbackReporter reporter_;
  uint16_t next_transport_seq_ = 0;
  Timestamp last_feedback_time_;
};

}  // namespace webrtc
#endif  // PC_DTLS_PACKET_PROCESSOR_H_
