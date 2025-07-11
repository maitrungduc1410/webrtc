/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/keyframe_interval_settings.h"

#include "api/field_trials.h"
#include "test/gtest.h"

namespace webrtc {
namespace {


TEST(KeyframeIntervalSettingsTest, ParsesMinKeyframeSendIntervalMs) {
  EXPECT_FALSE(
      KeyframeIntervalSettings(FieldTrials("")).MinKeyframeSendIntervalMs());

  FieldTrials field_trials(
      "WebRTC-KeyframeInterval/min_keyframe_send_interval_ms:100/");
  EXPECT_EQ(KeyframeIntervalSettings(field_trials).MinKeyframeSendIntervalMs(),
            100);
}

TEST(KeyframeIntervalSettingsTest, DoesNotParseIncorrectValues) {
  FieldTrials field_trials(
      "WebRTC-KeyframeInterval/min_keyframe_send_interval_ms:a/");
  EXPECT_FALSE(
      KeyframeIntervalSettings(field_trials).MinKeyframeSendIntervalMs());
}

}  // namespace
}  // namespace webrtc
