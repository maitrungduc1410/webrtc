/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "api/array_view.h"
#include "api/environment/environment.h"
#include "api/make_ref_counted.h"
#include "api/rtp_parameters.h"
#include "api/test/video/function_video_encoder_factory.h"
#include "api/transport/bitrate_settings.h"
#include "api/units/time_delta.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "call/video_receive_stream.h"
#include "call/video_send_stream.h"
#include "modules/video_coding/codecs/h264/include/h264.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/encoder_info_settings.h"
#include "rtc_base/strings/string_builder.h"
#include "test/call_test.h"
#include "test/frame_generator_capturer.h"
#include "test/gtest.h"
#include "test/rtp_rtcp_observer.h"
#include "test/video_test_constants.h"
#include "video/config/video_encoder_config.h"

namespace webrtc {
namespace {
constexpr int kInitialWidth = 1280;
constexpr int kInitialHeight = 720;
constexpr int kLowStartBps = 100000;
constexpr int kHighStartBps = 1000000;
constexpr int kDefaultVgaMinStartBps = 500000;  // From video_stream_encoder.cc
constexpr TimeDelta kTimeout =
    TimeDelta::Seconds(10);  // Some tests are expected to time out.

void SetEncoderSpecific(VideoEncoderConfig* encoder_config,
                        VideoCodecType type,
                        bool automatic_resize,
                        size_t num_spatial_layers) {
  if (type == kVideoCodecVP8) {
    VideoCodecVP8 vp8 = VideoEncoder::GetDefaultVp8Settings();
    vp8.automaticResizeOn = automatic_resize;
    encoder_config->encoder_specific_settings =
        make_ref_counted<VideoEncoderConfig::Vp8EncoderSpecificSettings>(vp8);
  } else if (type == kVideoCodecVP9) {
    VideoCodecVP9 vp9 = VideoEncoder::GetDefaultVp9Settings();
    vp9.automaticResizeOn = automatic_resize;
    vp9.numberOfSpatialLayers = num_spatial_layers;
    encoder_config->encoder_specific_settings =
        make_ref_counted<VideoEncoderConfig::Vp9EncoderSpecificSettings>(vp9);
  }
}
}  // namespace

class QualityScalingTest : public test::CallTest {
 protected:
  struct QualityScalingParams {
    int vp8_low = 0;
    int vp8_high = 0;
    int vp9_low = 0;
    int vp9_high = 0;
    int h264_low = 0;
    int h264_high = 0;
  };
  void SetQualityScalingTrialQP(QualityScalingParams p) {
    StringBuilder sb;
    sb << "Enabled-" << p.vp8_low << "," << p.vp8_high  //
       << "," << p.vp9_low << "," << p.vp9_high         //
       << "," << p.h264_low << "," << p.h264_high       //
       << ",0,0,0.9995,0.9999,1";
    field_trials().Set("WebRTC-Video-QualityScaling", sb.str());
  }

  const std::optional<VideoEncoder::ResolutionBitrateLimits>
      kSinglecastLimits720pVp8 =
          EncoderInfoSettings::GetDefaultSinglecastBitrateLimitsForResolution(
              kVideoCodecVP8,
              1280 * 720);
  const std::optional<VideoEncoder::ResolutionBitrateLimits>
      kSinglecastLimits360pVp9 =
          EncoderInfoSettings::GetDefaultSinglecastBitrateLimitsForResolution(
              kVideoCodecVP9,
              640 * 360);
  const std::optional<VideoEncoder::ResolutionBitrateLimits>
      kSinglecastLimits720pVp9 =
          EncoderInfoSettings::GetDefaultSinglecastBitrateLimitsForResolution(
              kVideoCodecVP9,
              1280 * 720);
};

class ScalingObserver : public test::SendTest {
 protected:
  struct TestParams {
    bool active;
    std::optional<ScalabilityMode> scalability_mode;
  };
  ScalingObserver(const std::string& payload_name,
                  const std::vector<TestParams>& test_params,
                  int start_bps,
                  bool automatic_resize,
                  bool expect_scaling)
      : SendTest(expect_scaling ? kTimeout * 4 : kTimeout),
        encoder_factory_(
            [](const Environment& env,
               const SdpVideoFormat& format) -> std::unique_ptr<VideoEncoder> {
              if (format.name == "VP8")
                return CreateVp8Encoder(env);
              if (format.name == "VP9")
                return CreateVp9Encoder(env);
              if (format.name == "H264")
                return CreateH264Encoder(env);
              RTC_DCHECK_NOTREACHED() << format.name;
              return nullptr;
            }),
        payload_name_(payload_name),
        test_params_(test_params),
        start_bps_(start_bps),
        automatic_resize_(automatic_resize),
        expect_scaling_(expect_scaling) {}

  DegradationPreference degradation_preference_ =
      DegradationPreference::MAINTAIN_FRAMERATE;

 private:
  void ModifySenderBitrateConfig(BitrateConstraints* bitrate_config) override {
    bitrate_config->start_bitrate_bps = start_bps_;
  }

  void ModifyVideoDegradationPreference(
      DegradationPreference* degradation_preference) override {
    *degradation_preference = degradation_preference_;
  }

  size_t GetNumVideoStreams() const override {
    return (payload_name_ == "VP9") ? 1 : test_params_.size();
  }

  void ModifyVideoConfigs(
      VideoSendStream::Config* send_config,
      std::vector<VideoReceiveStreamInterface::Config>* receive_configs,
      VideoEncoderConfig* encoder_config) override {
    send_config->encoder_settings.encoder_factory = &encoder_factory_;
    send_config->rtp.payload_name = payload_name_;
    send_config->rtp.payload_type =
        test::VideoTestConstants::kVideoSendPayloadType;
    encoder_config->video_format.name = payload_name_;
    const VideoCodecType codec_type = PayloadStringToCodecType(payload_name_);
    encoder_config->codec_type = codec_type;
    encoder_config->max_bitrate_bps =
        std::max(start_bps_, encoder_config->max_bitrate_bps);
    if (payload_name_ == "VP9") {
      // Simulcast layers indicates which spatial layers are active.
      encoder_config->simulcast_layers.resize(test_params_.size());
      encoder_config->simulcast_layers[0].max_bitrate_bps =
          encoder_config->max_bitrate_bps;
    }
    double scale_factor = 1.0;
    for (int i = test_params_.size() - 1; i >= 0; --i) {
      VideoStream& stream = encoder_config->simulcast_layers[i];
      stream.active = test_params_[i].active;
      stream.scalability_mode = test_params_[i].scalability_mode;
      stream.scale_resolution_down_by = scale_factor;
      scale_factor *= (payload_name_ == "VP9") ? 1.0 : 2.0;
    }
    encoder_config->frame_drop_enabled = true;
    SetEncoderSpecific(encoder_config, codec_type, automatic_resize_,
                       test_params_.size());
  }

  Action OnSendRtp(ArrayView<const uint8_t> packet) override {
    // The tests are expected to send at the configured start bitrate. Do not
    // send any packets to avoid receiving REMB and possibly go down in target
    // bitrate. A low bitrate estimate could result in downgrading due to other
    // reasons than low/high QP-value (e.g. high frame drop percent) or not
    // upgrading due to bitrate constraint.
    return DROP_PACKET;
  }

  void PerformTest() override { EXPECT_EQ(expect_scaling_, Wait()); }

  test::FunctionVideoEncoderFactory encoder_factory_;
  const std::string payload_name_;
  const std::vector<TestParams> test_params_;
  const int start_bps_;
  const bool automatic_resize_;
  const bool expect_scaling_;
};

class DownscalingObserver
    : public ScalingObserver,
      public test::FrameGeneratorCapturer::SinkWantsObserver {
 public:
  DownscalingObserver(const std::string& payload_name,
                      const std::vector<TestParams>& test_params,
                      int start_bps,
                      bool automatic_resize,
                      bool expect_downscale)
      : ScalingObserver(payload_name,
                        test_params,
                        start_bps,
                        automatic_resize,
                        expect_downscale) {}

 private:
  void OnFrameGeneratorCapturerCreated(
      test::FrameGeneratorCapturer* frame_generator_capturer) override {
    frame_generator_capturer->SetSinkWantsObserver(this);
    frame_generator_capturer->ChangeResolution(kInitialWidth, kInitialHeight);
  }

  void OnSinkWantsChanged(VideoSinkInterface<VideoFrame>* sink,
                          const VideoSinkWants& wants) override {
    if (wants.max_pixel_count < kInitialWidth * kInitialHeight)
      observation_complete_.Set();
  }
};

class UpscalingObserver
    : public ScalingObserver,
      public test::FrameGeneratorCapturer::SinkWantsObserver {
 public:
  UpscalingObserver(const std::string& payload_name,
                    const std::vector<TestParams>& test_params,
                    int start_bps,
                    bool automatic_resize,
                    bool expect_upscale)
      : ScalingObserver(payload_name,
                        test_params,
                        start_bps,
                        automatic_resize,
                        expect_upscale) {}

  void SetDegradationPreference(DegradationPreference preference) {
    degradation_preference_ = preference;
  }

 private:
  void OnFrameGeneratorCapturerCreated(
      test::FrameGeneratorCapturer* frame_generator_capturer) override {
    frame_generator_capturer->SetSinkWantsObserver(this);
    frame_generator_capturer->ChangeResolution(kInitialWidth, kInitialHeight);
  }

  void OnSinkWantsChanged(VideoSinkInterface<VideoFrame>* sink,
                          const VideoSinkWants& wants) override {
    if (wants.max_pixel_count > last_wants_.max_pixel_count) {
      if (wants.max_pixel_count == std::numeric_limits<int>::max())
        observation_complete_.Set();
    }
    last_wants_ = wants;
  }

  VideoSinkWants last_wants_;
};

TEST_F(QualityScalingTest, AdaptsDownForHighQp_Vp8) {
  // qp_low:1, qp_high:1 -> kHighQp
  SetQualityScalingTrialQP({.vp8_low = 1, .vp8_high = 1});

  DownscalingObserver test("VP8", {{.active = true}}, kHighStartBps,
                           /*automatic_resize=*/true,
                           /*expect_downscale=*/true);
  RunBaseTest(&test);
}

TEST_F(QualityScalingTest, NoAdaptDownForHighQpIfScalingOff_Vp8) {
  // qp_low:1, qp_high:1 -> kHighQp
  SetQualityScalingTrialQP({.vp8_low = 1, .vp8_high = 1});

  DownscalingObserver test("VP8", {{.active = true}}, kHighStartBps,
                           /*automatic_resize=*/false,
                           /*expect_downscale=*/false);
  RunBaseTest(&test);
}

TEST_F(QualityScalingTest, NoAdaptDownForNormalQp_Vp8) {
  // qp_low:1, qp_high:127 -> kNormalQp
  SetQualityScalingTrialQP({.vp8_low = 1, .vp8_high = 127});

  DownscalingObserver test("VP8", {{.active = true}}, kHighStartBps,
                           /*automatic_resize=*/true,
                           /*expect_downscale=*/false);
  RunBaseTest(&test);
}

TEST_F(QualityScalingTest, AdaptsDownForLowStartBitrate_Vp8) {
  // qp_low:1, qp_high:127 -> kNormalQp
  SetQualityScalingTrialQP({.vp8_low = 1, .vp8_high = 127});

  DownscalingObserver test("VP8", {{.active = true}}, kLowStartBps,
                           /*automatic_resize=*/true,
                           /*expect_downscale=*/true);
  RunBaseTest(&test);
}

TEST_F(QualityScalingTest, AdaptsDownForLowStartBitrateAndThenUp) {
  // qp_low:127, qp_high:127 -> kLowQp
  SetQualityScalingTrialQP({.vp8_low = 127, .vp8_high = 127});
  field_trials().Set(
      "WebRTC-Video-BalancedDegradationSettings",
      "pixels:230400|921600,fps:20|30,kbps:300|500");  // should not affect

  UpscalingObserver test("VP8", {{.active = true}}, kDefaultVgaMinStartBps - 1,
                         /*automatic_resize=*/true, /*expect_upscale=*/true);
  RunBaseTest(&test);
}

TEST_F(QualityScalingTest, AdaptsDownAndThenUpWithBalanced) {
  // qp_low:127, qp_high:127 -> kLowQp
  SetQualityScalingTrialQP({.vp8_low = 127, .vp8_high = 127});
  field_trials().Set("WebRTC-Video-BalancedDegradationSettings",
                     "pixels:230400|921600,fps:20|30,kbps:300|499");

  UpscalingObserver test("VP8", {{.active = true}}, kDefaultVgaMinStartBps - 1,
                         /*automatic_resize=*/true, /*expect_upscale=*/true);
  test.SetDegradationPreference(DegradationPreference::BALANCED);
  RunBaseTest(&test);
}

TEST_F(QualityScalingTest, AdaptsDownButNotUpWithBalancedIfBitrateNotEnough) {
  // qp_low:127, qp_high:127 -> kLowQp
  SetQualityScalingTrialQP({.vp8_low = 127, .vp8_high = 127});
  field_trials().Set("WebRTC-Video-BalancedDegradationSettings",
                     "pixels:230400|921600,fps:20|30,kbps:300|500");

  UpscalingObserver test("VP8", {{.active = true}}, kDefaultVgaMinStartBps - 1,
                         /*automatic_resize=*/true, /*expect_upscale=*/false);
  test.SetDegradationPreference(DegradationPreference::BALANCED);
  RunBaseTest(&test);
}

TEST_F(QualityScalingTest, NoAdaptDownForLowStartBitrate_Simulcast) {
  // qp_low:1, qp_high:127 -> kNormalQp
  SetQualityScalingTrialQP({.vp8_low = 1, .vp8_high = 127});

  DownscalingObserver test("VP8", {{.active = true}, {.active = true}},
                           kLowStartBps,
                           /*automatic_resize=*/false,
                           /*expect_downscale=*/false);
  RunBaseTest(&test);
}

TEST_F(QualityScalingTest, AdaptsDownForHighQp_HighestStreamActive_Vp8) {
  // qp_low:1, qp_high:1 -> kHighQp
  SetQualityScalingTrialQP({.vp8_low = 1, .vp8_high = 1});

  DownscalingObserver test(
      "VP8", {{.active = false}, {.active = false}, {.active = true}},
      kHighStartBps,
      /*automatic_resize=*/true,
      /*expect_downscale=*/true);
  RunBaseTest(&test);
}

TEST_F(QualityScalingTest,
       AdaptsDownForLowStartBitrate_HighestStreamActive_Vp8) {
  // qp_low:1, qp_high:127 -> kNormalQp
  SetQualityScalingTrialQP({.vp8_low = 1, .vp8_high = 127});

  DownscalingObserver test(
      "VP8", {{.active = false}, {.active = false}, {.active = true}},
      kSinglecastLimits720pVp8->min_start_bitrate_bps - 1,
      /*automatic_resize=*/true,
      /*expect_downscale=*/true);
  RunBaseTest(&test);
}

TEST_F(QualityScalingTest, AdaptsDownButNotUpWithMinStartBitrateLimit) {
  // qp_low:127, qp_high:127 -> kLowQp
  SetQualityScalingTrialQP({.vp8_low = 127, .vp8_high = 127});

  UpscalingObserver test("VP8", {{.active = false}, {.active = true}},
                         kSinglecastLimits720pVp8->min_start_bitrate_bps - 1,
                         /*automatic_resize=*/true, /*expect_upscale=*/false);
  RunBaseTest(&test);
}

TEST_F(QualityScalingTest, NoAdaptDownForLowStartBitrateIfBitrateEnough_Vp8) {
  // qp_low:1, qp_high:127 -> kNormalQp
  SetQualityScalingTrialQP({.vp8_low = 1, .vp8_high = 127});

  DownscalingObserver test(
      "VP8", {{.active = false}, {.active = false}, {.active = true}},
      kSinglecastLimits720pVp8->min_start_bitrate_bps,
      /*automatic_resize=*/true,
      /*expect_downscale=*/false);
  RunBaseTest(&test);
}

TEST_F(QualityScalingTest,
       NoAdaptDownForLowStartBitrateIfDefaultLimitsDisabled_Vp8) {
  // qp_low:1, qp_high:127 -> kNormalQp
  SetQualityScalingTrialQP({.vp8_low = 1, .vp8_high = 127});
  field_trials().Set("WebRTC-DefaultBitrateLimitsKillSwitch", "Enabled");

  DownscalingObserver test(
      "VP8", {{.active = false}, {.active = false}, {.active = true}},
      kSinglecastLimits720pVp8->min_start_bitrate_bps - 1,
      /*automatic_resize=*/true,
      /*expect_downscale=*/false);
  RunBaseTest(&test);
}

TEST_F(QualityScalingTest,
       NoAdaptDownForLowStartBitrate_OneStreamSinglecastLimitsNotUsed_Vp8) {
  // qp_low:1, qp_high:127 -> kNormalQp
  SetQualityScalingTrialQP({.vp8_low = 1, .vp8_high = 127});

  DownscalingObserver test("VP8", {{.active = true}},
                           kSinglecastLimits720pVp8->min_start_bitrate_bps - 1,
                           /*automatic_resize=*/true,
                           /*expect_downscale=*/false);
  RunBaseTest(&test);
}

TEST_F(QualityScalingTest, NoAdaptDownForHighQp_LowestStreamActive_Vp8) {
  // qp_low:1, qp_high:1 -> kHighQp
  SetQualityScalingTrialQP({.vp8_low = 1, .vp8_high = 1});

  DownscalingObserver test(
      "VP8", {{.active = true}, {.active = false}, {.active = false}},
      kHighStartBps,
      /*automatic_resize=*/true,
      /*expect_downscale=*/false);
  RunBaseTest(&test);
}

TEST_F(QualityScalingTest,
       NoAdaptDownForLowStartBitrate_LowestStreamActive_Vp8) {
  // qp_low:1, qp_high:127 -> kNormalQp
  SetQualityScalingTrialQP({.vp8_low = 1, .vp8_high = 127});

  DownscalingObserver test(
      "VP8", {{.active = true}, {.active = false}, {.active = false}},
      kLowStartBps,
      /*automatic_resize=*/true,
      /*expect_downscale=*/false);
  RunBaseTest(&test);
}

TEST_F(QualityScalingTest, NoAdaptDownForLowStartBitrateIfScalingOff_Vp8) {
  // qp_low:1, qp_high:127 -> kNormalQp
  SetQualityScalingTrialQP({.vp8_low = 1, .vp8_high = 127});

  DownscalingObserver test("VP8", {{.active = true}}, kLowStartBps,
                           /*automatic_resize=*/false,
                           /*expect_downscale=*/false);
  RunBaseTest(&test);
}

TEST_F(QualityScalingTest, AdaptsDownForHighQp_Vp9) {
  // qp_low:1, qp_high:1 -> kHighQp
  SetQualityScalingTrialQP({.vp9_low = 1, .vp9_high = 1});

  DownscalingObserver test("VP9", {{.active = true}}, kHighStartBps,
                           /*automatic_resize=*/true,
                           /*expect_downscale=*/true);
  RunBaseTest(&test);
}

TEST_F(QualityScalingTest, NoAdaptDownForHighQpIfScalingOff_Vp9) {
  // qp_low:1, qp_high:1 -> kHighQp
  SetQualityScalingTrialQP({.vp9_low = 1, .vp9_high = 1});
  field_trials().Set("WebRTC-VP9QualityScaler", "Disabled");

  DownscalingObserver test("VP9", {{.active = true}}, kHighStartBps,
                           /*automatic_resize=*/true,
                           /*expect_downscale=*/false);
  RunBaseTest(&test);
}

TEST_F(QualityScalingTest, AdaptsDownForLowStartBitrate_Vp9) {
  // qp_low:1, qp_high:255 -> kNormalQp
  SetQualityScalingTrialQP({.vp9_low = 1, .vp9_high = 255});

  DownscalingObserver test("VP9", {{.active = true}}, kLowStartBps,
                           /*automatic_resize=*/true,
                           /*expect_downscale=*/true);
  RunBaseTest(&test);
}

TEST_F(QualityScalingTest, NoAdaptDownForHighStartBitrate_Vp9) {
  DownscalingObserver test(
      "VP9", {{.active = false}, {.active = false}, {.active = true}},
      kHighStartBps,
      /*automatic_resize=*/true,
      /*expect_downscale=*/false);
  RunBaseTest(&test);
}

TEST_F(QualityScalingTest, NoAdaptDownForHighQp_LowestStreamActive_Vp9) {
  // qp_low:1, qp_high:1 -> kHighQp
  SetQualityScalingTrialQP({.vp9_low = 1, .vp9_high = 1});

  DownscalingObserver test(
      "VP9", {{.active = true}, {.active = false}, {.active = false}},
      kHighStartBps,
      /*automatic_resize=*/true,
      /*expect_downscale=*/false);
  RunBaseTest(&test);
}

TEST_F(QualityScalingTest,
       NoAdaptDownForLowStartBitrate_LowestStreamActive_Vp9) {
  // qp_low:1, qp_high:255 -> kNormalQp
  SetQualityScalingTrialQP({.vp9_low = 1, .vp9_high = 255});

  DownscalingObserver test(
      "VP9", {{.active = true}, {.active = false}, {.active = false}},
      kLowStartBps,
      /*automatic_resize=*/true,
      /*expect_downscale=*/false);
  RunBaseTest(&test);
}

TEST_F(QualityScalingTest, AdaptsDownForHighQp_MiddleStreamActive_Vp9) {
  // qp_low:1, qp_high:1 -> kHighQp
  SetQualityScalingTrialQP({.vp9_low = 1, .vp9_high = 1});

  DownscalingObserver test(
      "VP9", {{.active = false}, {.active = true}, {.active = false}},
      kHighStartBps,
      /*automatic_resize=*/true,
      /*expect_downscale=*/true);
  RunBaseTest(&test);
}

TEST_F(QualityScalingTest,
       AdaptsDownForLowStartBitrate_MiddleStreamActive_Vp9) {
  // qp_low:1, qp_high:255 -> kNormalQp
  SetQualityScalingTrialQP({.vp9_low = 1, .vp9_high = 255});

  DownscalingObserver test(
      "VP9", {{.active = false}, {.active = true}, {.active = false}},
      kSinglecastLimits360pVp9->min_start_bitrate_bps - 1,
      /*automatic_resize=*/true,
      /*expect_downscale=*/true);
  RunBaseTest(&test);
}

TEST_F(QualityScalingTest, NoAdaptDownForLowStartBitrateIfBitrateEnough_Vp9) {
  // qp_low:1, qp_high:255 -> kNormalQp
  SetQualityScalingTrialQP({.vp9_low = 1, .vp9_high = 255});

  DownscalingObserver test(
      "VP9", {{.active = false}, {.active = true}, {.active = false}},
      kSinglecastLimits360pVp9->min_start_bitrate_bps,
      /*automatic_resize=*/true,
      /*expect_downscale=*/false);
  RunBaseTest(&test);
}

TEST_F(QualityScalingTest,
       AdaptsDownButNotUpWithMinStartBitrateLimitWithScalabilityMode_VP9) {
  // qp_low:255, qp_high:255 -> kLowQp
  SetQualityScalingTrialQP({.vp9_low = 255, .vp9_high = 255});

  UpscalingObserver test(
      "VP9",
      {{.active = true, .scalability_mode = ScalabilityMode::kL1T3},
       {.active = false}},
      kSinglecastLimits720pVp9->min_start_bitrate_bps - 1,
      /*automatic_resize=*/true, /*expect_upscale=*/false);
  RunBaseTest(&test);
}

TEST_F(QualityScalingTest,
       NoAdaptDownForLowStartBitrateIfBitrateEnoughWithScalabilityMode_Vp9) {
  // qp_low:1, qp_high:255 -> kNormalQp
  SetQualityScalingTrialQP({.vp9_low = 1, .vp9_high = 255});

  DownscalingObserver test(
      "VP9",
      {{.active = true, .scalability_mode = ScalabilityMode::kL1T3},
       {.active = false},
       {.active = false}},
      kSinglecastLimits720pVp9->min_start_bitrate_bps,
      /*automatic_resize=*/true,
      /*expect_downscale=*/false);
  RunBaseTest(&test);
}

#if defined(WEBRTC_USE_H264)
TEST_F(QualityScalingTest, AdaptsDownForHighQp_H264) {
  // qp_low:1, qp_high:1 -> kHighQp
  SetQualityScalingTrialQP({.h264_low = 1, .h264_high = 1});

  DownscalingObserver test("H264", {{.active = true}}, kHighStartBps,
                           /*automatic_resize=*/true,
                           /*expect_downscale=*/true);
  RunBaseTest(&test);
}

TEST_F(QualityScalingTest, AdaptsDownForLowStartBitrate_H264) {
  // qp_low:1, qp_high:51 -> kNormalQp
  SetQualityScalingTrialQP({.h264_low = 1, .h264_high = 51});

  DownscalingObserver test("H264", {{.active = true}}, kLowStartBps,
                           /*automatic_resize=*/true,
                           /*expect_downscale=*/true);
  RunBaseTest(&test);
}
#endif  // defined(WEBRTC_USE_H264)

}  // namespace webrtc
