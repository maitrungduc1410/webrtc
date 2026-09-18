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

#include "test/gtest.h"

namespace webrtc {
namespace {

Ect1Policy CreateEnabledPolicy() {
  Ect1Policy policy;
  policy.SetFeedbackSupportsEcn(true);
  policy.SetCongestionControllerSupportsEcn(true);
  return policy;
}

TEST(Ect1PolicyTest, DoesNotSendEct1ByDefault) {
  Ect1Policy policy;
  EXPECT_FALSE(policy.ShouldSendEct1());
}

TEST(Ect1PolicyTest, SendsEct1IfFeedbackAndCongestionControllerSupportEcn) {
  EXPECT_TRUE(CreateEnabledPolicy().ShouldSendEct1());
}

TEST(Ect1PolicyTest, DoesNotSendEct1IfFeedbackDoesNotSupportEcn) {
  Ect1Policy policy;
  policy.SetFeedbackSupportsEcn(false);
  policy.SetCongestionControllerSupportsEcn(true);
  EXPECT_FALSE(policy.ShouldSendEct1());
}

TEST(Ect1PolicyTest, DoesNotSendEct1IfCongestionControllerDoesNotSupportEcn) {
  Ect1Policy policy;
  policy.SetFeedbackSupportsEcn(true);
  policy.SetCongestionControllerSupportsEcn(false);
  EXPECT_FALSE(policy.ShouldSendEct1());
}

TEST(Ect1PolicyTest, StopsSendingEct1IfFeedbackReportsBleaching) {
  Ect1Policy policy = CreateEnabledPolicy();
  policy.OnPacketsFeedback(/*has_bleached_ect1=*/true);
  EXPECT_FALSE(policy.ShouldSendEct1());
}

TEST(Ect1PolicyTest, KeepsSendingEct1IfFeedbackReportsNoBleaching) {
  Ect1Policy policy = CreateEnabledPolicy();
  policy.OnPacketsFeedback(/*has_bleached_ect1=*/false);
  EXPECT_TRUE(policy.ShouldSendEct1());
}

TEST(Ect1PolicyTest, RouteChangeReEnablesEct1AfterBleaching) {
  Ect1Policy policy = CreateEnabledPolicy();
  policy.OnPacketsFeedback(/*has_bleached_ect1=*/true);
  ASSERT_FALSE(policy.ShouldSendEct1());

  policy.OnNetworkRouteChanged();
  EXPECT_TRUE(policy.ShouldSendEct1());
}

TEST(Ect1PolicyTest, RenegotiationDoesNotReEnableEct1AfterBleaching) {
  Ect1Policy policy = CreateEnabledPolicy();
  policy.OnPacketsFeedback(/*has_bleached_ect1=*/true);
  ASSERT_FALSE(policy.ShouldSendEct1());

  policy.SetFeedbackSupportsEcn(true);
  EXPECT_FALSE(policy.ShouldSendEct1());
}

}  // namespace
}  // namespace webrtc
