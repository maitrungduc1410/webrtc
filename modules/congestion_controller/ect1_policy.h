/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_CONGESTION_CONTROLLER_ECT1_POLICY_H_
#define MODULES_CONGESTION_CONTROLLER_ECT1_POLICY_H_

namespace webrtc {

// Decides if RTP packets should be sent with an ECT(1) marking, for example to
// support L4S as described in RFC 9331.
//
// ECT(1) is used if the negotiated feedback format reports the ECN marking of
// received packets and the congestion controller adapts to ECN. If feedback
// shows that the network path bleaches the marking to Not-ECT, it is disabled
// until the network route changes. Bleaching is observed directly, so it is not
// retried on the same route.
class Ect1Policy {
 public:
  // Whether the remote peer reports the ECN marking of received packets back to
  // the sender, for example with the RTCP feedback message in RFC 8888.
  void SetFeedbackSupportsEcn(bool supports_ecn);
  // Whether the congestion controller reacts to reported CE markings.
  void SetCongestionControllerSupportsEcn(bool supports_ecn);
  void OnNetworkRouteChanged();
  // `has_bleached_ect1` is true if a feedback report showed at least one
  // packet that was sent as ECT(1) arriving as Not-ECT.
  void OnPacketsFeedback(bool has_bleached_ect1);

  bool ShouldSendEct1() const;

 private:
  void LogIfDecisionChanged();

  bool feedback_supports_ecn_ = false;
  bool congestion_controller_supports_ecn_ = false;
  bool route_bleaches_ect1_ = false;
  bool logged_send_ect1_ = false;
};

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_ECT1_POLICY_H_
