/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef PC_PACKET_PROCESSOR_H_
#define PC_PACKET_PROCESSOR_H_

#include <cstdint>
#include <optional>
#include <span>

#include "absl/functional/any_invocable.h"
#include "api/transport/ecn_marking.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/copy_on_write_buffer.h"

namespace webrtc {

// Transport-protocol-agnostic hook that a DatagramConnection consults on every
// outgoing and incoming packet. Implementations encapsulate all protocol
// specifics (e.g. in-band framing for DTLS-only mode) so the connection itself
// stays protocol-agnostic and simply forwards packets through the two transform
// methods below.
//
// It frames the application payload with a transport header, and generates
// congestion control feedback for the packets it receives.
//
// All methods run on the network thread.
class PacketProcessor {
 public:
  // Sends congestion control feedback with no application packet to ride on.
  using TransportPacketSender = absl::AnyInvocable<void(CopyOnWriteBuffer)>;

  virtual ~PacketProcessor() = default;

  // Sender side. Returns the wire bytes to send for the application `payload`.
  // Implementations may wrap the payload in transport framing and record
  // whatever per-packet metadata is needed. `send_time` is when the packet is
  // handed to the wire. Protocols that need no framing may return a buffer that
  // simply copies `payload`.
  virtual CopyOnWriteBuffer ProcessOutgoingPacket(
      std::span<const uint8_t> payload,
      Timestamp send_time) = 0;

  // Receiver side. Consumes an incoming wire `packet` and returns the
  // application payload to deliver to the app, or nullopt when the packet
  // carries no application data (e.g. a packet dropped as malformed). The
  // returned buffer shares storage with `packet` (no copy of the payload
  // bytes).
  virtual std::optional<CopyOnWriteBuffer> ProcessIncomingPacket(
      CopyOnWriteBuffer packet,
      Timestamp receive_time,
      EcnMarking ecn) = 0;

  // Drives the congestion control feedback cadence. Returns how long to wait
  // before calling this again.
  virtual TimeDelta ProcessTimers(Timestamp now) = 0;
};

}  // namespace webrtc
#endif  // PC_PACKET_PROCESSOR_H_
