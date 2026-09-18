/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/congestion_controller/ect1_policy.h"

#include "rtc_base/logging.h"

namespace webrtc {

void Ect1Policy::SetFeedbackSupportsEcn(bool supports_ecn) {
  feedback_supports_ecn_ = supports_ecn;
  LogIfDecisionChanged();
}

void Ect1Policy::SetCongestionControllerSupportsEcn(bool supports_ecn) {
  congestion_controller_supports_ecn_ = supports_ecn;
  LogIfDecisionChanged();
}

void Ect1Policy::OnNetworkRouteChanged() {
  // The new path may preserve the ECT(1) marking even if the old one did not.
  route_bleaches_ect1_ = false;
  LogIfDecisionChanged();
}

void Ect1Policy::OnPacketsFeedback(bool has_bleached_ect1) {
  if (route_bleaches_ect1_ || !has_bleached_ect1) {
    return;
  }
  route_bleaches_ect1_ = true;
  RTC_LOG(LS_INFO) << "Transport does not preserve the ECT(1) marking. Stop "
                      "sending ECT(1) on this route.";
  LogIfDecisionChanged();
}

bool Ect1Policy::ShouldSendEct1() const {
  return feedback_supports_ecn_ && congestion_controller_supports_ecn_ &&
         !route_bleaches_ect1_;
}

void Ect1Policy::LogIfDecisionChanged() {
  bool send_ect1 = ShouldSendEct1();
  if (send_ect1 == logged_send_ect1_) {
    return;
  }
  logged_send_ect1_ = send_ect1;
  RTC_LOG(LS_INFO) << "Sending packets as "
                   << (send_ect1 ? "ECT(1)." : "Not-ECT.");
}

}  // namespace webrtc
