/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <limits>
#include <vector>

#include "api/rtp_parameters.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "call/video_receive_stream.h"
#include "call/video_send_stream.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "test/call_test.h"
#include "test/frame_generator_capturer.h"
#include "test/gtest.h"
#include "test/video_test_constants.h"
#include "video/config/video_encoder_config.h"

namespace webrtc {
namespace {
constexpr int kWidth = 1280;
constexpr int kHeight = 720;
constexpr int kFps = 28;
}  // namespace

// Minimal normal usage at start, then 60s overuse.
class CpuOveruseTest : public test::CallTest {
 protected:
  CpuOveruseTest()
      : CallTest("WebRTC-ForceSimulatedOveruseIntervalMs/1-60000-60000/") {}

  void RunTestAndCheckForAdaptation(
      const DegradationPreference& degradation_preference,
      bool expect_adaptation);
};

void CpuOveruseTest::RunTestAndCheckForAdaptation(
    const DegradationPreference& degradation_preference,
    bool expect_adaptation) {
  class OveruseObserver
      : public test::SendTest,
        public test::FrameGeneratorCapturer::SinkWantsObserver {
   public:
    OveruseObserver(const DegradationPreference& degradation_preference,
                    bool expect_adaptation)
        : SendTest(expect_adaptation
                       ? test::VideoTestConstants::kLongTimeout
                       : test::VideoTestConstants::kDefaultTimeout),
          degradation_preference_(degradation_preference),
          expect_adaptation_(expect_adaptation) {}

   private:
    void OnFrameGeneratorCapturerCreated(
        test::FrameGeneratorCapturer* frame_generator_capturer) override {
      frame_generator_capturer->SetSinkWantsObserver(this);
      // Set initial resolution.
      frame_generator_capturer->ChangeResolution(kWidth, kHeight);
    }

    // Called when FrameGeneratorCapturer::AddOrUpdateSink is called.
    void OnSinkWantsChanged(VideoSinkInterface<VideoFrame>* sink,
                            const VideoSinkWants& wants) override {
      if (wants.max_pixel_count == std::numeric_limits<int>::max() &&
          wants.max_framerate_fps == kFps) {
        // Max configured framerate is initially set.
        return;
      }
      switch (degradation_preference_) {
        case DegradationPreference::MAINTAIN_FRAMERATE:
          EXPECT_LT(wants.max_pixel_count, kWidth * kHeight);
          observation_complete_.Set();
          break;
        case DegradationPreference::MAINTAIN_RESOLUTION:
          EXPECT_LT(wants.max_framerate_fps, kFps);
          observation_complete_.Set();
          break;
        case DegradationPreference::BALANCED:
          if (wants.max_pixel_count == std::numeric_limits<int>::max() &&
              wants.max_framerate_fps == std::numeric_limits<int>::max()) {
            // `adapt_counters_` map in VideoStreamEncoder is reset when
            // balanced mode is set.
            break;
          }
          EXPECT_TRUE(wants.max_pixel_count < kWidth * kHeight ||
                      wants.max_framerate_fps < kFps);
          observation_complete_.Set();
          break;
        default:
          RTC_DCHECK_NOTREACHED();
      }
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStreamInterface::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      EXPECT_FALSE(encoder_config->simulcast_layers.empty());
      encoder_config->simulcast_layers[0].max_framerate = kFps;
    }

    void ModifyVideoDegradationPreference(
        DegradationPreference* degradation_preference) override {
      *degradation_preference = degradation_preference_;
    }

    void PerformTest() override {
      EXPECT_EQ(expect_adaptation_, Wait())
          << "Timed out while waiting for a scale down.";
    }

    const DegradationPreference degradation_preference_;
    const bool expect_adaptation_;
  } test(degradation_preference, expect_adaptation);

  RunBaseTest(&test);
}

TEST_F(CpuOveruseTest, AdaptsDownInResolutionOnOveruse) {
  RunTestAndCheckForAdaptation(DegradationPreference::MAINTAIN_FRAMERATE, true);
}

TEST_F(CpuOveruseTest, AdaptsDownInFpsOnOveruse) {
  RunTestAndCheckForAdaptation(DegradationPreference::MAINTAIN_RESOLUTION,
                               true);
}

TEST_F(CpuOveruseTest, AdaptsDownInResolutionOrFpsOnOveruse) {
  RunTestAndCheckForAdaptation(DegradationPreference::BALANCED, true);
}

TEST_F(CpuOveruseTest, NoAdaptDownOnOveruse) {
  RunTestAndCheckForAdaptation(DegradationPreference::DISABLED, false);
}
}  // namespace webrtc
