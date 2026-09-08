/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
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
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "api/environment/environment.h"
#include "api/scoped_refptr.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/i420_buffer.h"
#include "api/video/resolution.h"
#include "api/video/video_frame_buffer.h"
#include "api/video_codecs/libaom_av1_encoder_factory.h"
#include "api/video_codecs/test/video_codec_test_utils.h"
#include "api/video_codecs/video_encoder_builders.h"
#include "api/video_codecs/video_encoder_builders_for_test.h"
#include "api/video_codecs/video_encoder_factory_interface.h"
#include "api/video_codecs/video_encoder_interface.h"
#include "api/video_codecs/video_encoding_general.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "test/create_test_environment.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/qp_parser_for_test.h"
#include "test/testsupport/frame_reader.h"

// This file contains tests evaluating the rate control requirements in
// `api/video_codecs/g3doc/video_encoder_api_v2.md`.
// These are not meant to be exhaustive and are not intended for continuous
// performance testing. See e.g. test/video_codec_tester.h for such purposes.

namespace webrtc {
namespace {

// Tracks accumulated data sizes and duration for actual encoded bytes and ideal
// CBR bytes.
struct AccumulatedData {
  DataSize actual = DataSize::Zero();
  DataSize ideal = DataSize::Zero();
  TimeDelta duration = TimeDelta::Zero();

  void Add(const AccumulatedData& other) {
    actual += other.actual;
    ideal += other.ideal;
    duration += other.duration;
  }

  void Subtract(const AccumulatedData& other) {
    actual -= other.actual;
    ideal -= other.ideal;
    duration -= other.duration;
  }

  double deviation_pct() const { return 100.0 * (actual / ideal - 1.0); }
};

class VideoEncoderRateControlTestBase : public ::testing::Test {
 protected:
  VideoEncoderRateControlTestBase() : env_(CreateTestEnvironment()) {}

  bool SupportsCbr() const {
    VideoEncoderFactoryInterface::Capabilities capabilities =
        encoder_factory_->GetEncoderCapabilities();
    const std::vector<VideoEncoderFactoryInterface::RateControlMode>& rc_modes =
        capabilities.bitrate_control().rc_modes();
    return std::find(rc_modes.begin(), rc_modes.end(),
                     VideoEncoderFactoryInterface::RateControlMode::kCbr) !=
           rc_modes.end();
  }

  void SetUpCbrEncoder(Resolution resolution) {
    ASSERT_TRUE(SupportsCbr());

    VideoEncoderFactoryInterface::StaticEncoderSettings static_settings =
        StaticEncoderSettingsBuilder()
            .MaxEncodeDimensions(resolution)
            .EncodingFormat({.sub_sampling = EncodingFormat::SubSampling::k420,
                             .bit_depth = 8})
            .CbrRcMode(TimeDelta::Millis(1000), TimeDelta::Millis(600))
            .MaxNumberOfThreads(1)
            .Build();

    encoder_ = encoder_factory_->CreateEncoder(static_settings, {});
    RTC_CHECK(encoder_ != nullptr);
    frame_reader_ = CreateFrameReader();
    RTC_CHECK(frame_reader_ != nullptr);
    current_timestamp_ = Timestamp::Zero();
    is_first_frame_ = true;
    encoded_frames_.clear();
  }

  void Encode(int num_frames,
              TimeDelta frame_duration,
              DataRate target_bitrate,
              Resolution resolution) {
    VideoEncoderInterface::FrameEncodeSettings::Cbr cbr_settings{
        .duration = frame_duration,
        .target_bitrate = target_bitrate,
    };

    for (int i = 0; i < num_frames; ++i) {
      scoped_refptr<I420Buffer> raw_frame = frame_reader_->PullFrame();
      ASSERT_TRUE(raw_frame);
      scoped_refptr<I420Buffer> frame;
      if (raw_frame->width() == resolution.width &&
          raw_frame->height() == resolution.height) {
        frame = raw_frame;
      } else {
        frame = I420Buffer::Create(resolution.width, resolution.height);
        frame->ScaleFrom(*raw_frame);
      }

      EncOut out;
      if (is_first_frame_) {
        encoder_->Encode(frame, TemporalUnitSettings(current_timestamp_),
                         ToVec({Fb().Res(resolution)
                                    .Upd(0)
                                    .Key()
                                    .Cbr(cbr_settings)
                                    .Out(out)}));
        is_first_frame_ = false;
      } else {
        encoder_->Encode(frame, TemporalUnitSettings(current_timestamp_),
                         ToVec({Fb().Res(resolution)
                                    .Ref({0})
                                    .Upd(0)
                                    .Delta()
                                    .Cbr(cbr_settings)
                                    .Out(out)}));
      }

      ASSERT_THAT(out, HasBitstreamAndMetaData());
      encoded_frames_.push_back(
          {.actual = DataSize::Bytes(out.bitstream.size()),
           .ideal = target_bitrate * frame_duration,
           .duration = frame_duration});
      current_timestamp_ += frame_duration;
    }
  }

  void VerifyTotalDeviation(double max_deviation_pct) {
    AccumulatedData total;
    for (const auto& frame : encoded_frames_) {
      total.Add(frame);
    }
    double total_deviation_pct = total.deviation_pct();
    RTC_LOG(LS_VERBOSE) << "total_bytes=" << total.actual.bytes()
                        << " optimal=" << total.ideal.bytes()
                        << " deviation=" << total_deviation_pct << "%";
    EXPECT_NEAR(total_deviation_pct, 0.0, max_deviation_pct)
        << "Bitrate deviation " << total_deviation_pct
        << "% exceeded tolerance " << max_deviation_pct
        << "% (actual: " << total.actual.bytes()
        << " bytes, target: " << total.ideal.bytes() << " bytes)";
  }

  void VerifyFrameBasedSlidingWindowBitrateDeviation(
      int window_frames,
      double min_allowed_dev_pct,
      double max_allowed_dev_pct) {
    AccumulatedData window_data;
    std::deque<AccumulatedData> window;
    std::optional<double> max_window_dev_pct;
    std::optional<double> min_window_dev_pct;

    for (const auto& frame : encoded_frames_) {
      window.push_back(frame);
      window_data.Add(frame);

      if (window.size() > static_cast<size_t>(window_frames)) {
        window_data.Subtract(window.front());
        window.pop_front();
      }

      if (window.size() == static_cast<size_t>(window_frames)) {
        double window_dev_pct = window_data.deviation_pct();
        if (!max_window_dev_pct || window_dev_pct > *max_window_dev_pct) {
          max_window_dev_pct = window_dev_pct;
        }
        if (!min_window_dev_pct || window_dev_pct < *min_window_dev_pct) {
          min_window_dev_pct = window_dev_pct;
        }
      }
    }

    ASSERT_TRUE(min_window_dev_pct.has_value());
    ASSERT_TRUE(max_window_dev_pct.has_value());

    RTC_LOG(LS_VERBOSE) << "sliding " << window_frames
                        << "-frame window deviation range: ["
                        << *min_window_dev_pct << "%, " << *max_window_dev_pct
                        << "%]";

    EXPECT_GE(*min_window_dev_pct, min_allowed_dev_pct);
    EXPECT_LE(*max_window_dev_pct, max_allowed_dev_pct);
  }

  void VerifyTimeBasedSlidingWindowBitrateDeviation(
      TimeDelta window_duration,
      double min_allowed_dev_pct,
      double max_allowed_dev_pct) {
    AccumulatedData window_data;
    std::deque<AccumulatedData> window;
    std::optional<double> max_window_dev_pct;
    std::optional<double> min_window_dev_pct;

    for (const auto& frame : encoded_frames_) {
      window.push_back(frame);
      window_data.Add(frame);

      while (window_data.duration >= window_duration) {
        double window_dev_pct = window_data.deviation_pct();
        if (!max_window_dev_pct || window_dev_pct > *max_window_dev_pct) {
          max_window_dev_pct = window_dev_pct;
        }
        if (!min_window_dev_pct || window_dev_pct < *min_window_dev_pct) {
          min_window_dev_pct = window_dev_pct;
        }

        window_data.Subtract(window.front());
        window.pop_front();
      }
    }

    ASSERT_TRUE(min_window_dev_pct.has_value());
    ASSERT_TRUE(max_window_dev_pct.has_value());

    RTC_LOG(LS_VERBOSE) << "sliding " << window_duration.seconds()
                        << "s window deviation range: [" << *min_window_dev_pct
                        << "%, " << *max_window_dev_pct << "%]";

    EXPECT_GE(*min_window_dev_pct, min_allowed_dev_pct);
    EXPECT_LE(*max_window_dev_pct, max_allowed_dev_pct);
  }

  Environment env_;
  std::unique_ptr<VideoEncoderFactoryInterface> encoder_factory_;
  std::unique_ptr<VideoEncoderInterface> encoder_;
  std::unique_ptr<test::FrameReader> frame_reader_;
  std::vector<AccumulatedData> encoded_frames_;
  Timestamp current_timestamp_ = Timestamp::Zero();
  bool is_first_frame_ = true;
};

class VideoEncoderRateControlTest
    : public VideoEncoderRateControlTestBase,
      public ::testing::WithParamInterface<FactoryCreator> {
 protected:
  void SetUp() override { encoder_factory_ = GetParam()(); }
};

TEST_P(VideoEncoderRateControlTest, ConstantQpMatchesBitstreamAndEncoderQp) {
  VideoEncoderFactoryInterface::Capabilities capabilities =
      encoder_factory_->GetEncoderCapabilities();
  const std::vector<VideoEncoderFactoryInterface::RateControlMode>& rc_modes =
      capabilities.bitrate_control().rc_modes();
  if (std::find(rc_modes.begin(), rc_modes.end(),
                VideoEncoderFactoryInterface::RateControlMode::kCqp) ==
      rc_modes.end()) {
    GTEST_SKIP() << "Encoder does not support CQP mode.";
  }

  int min_qp = capabilities.bitrate_control().min_qp();
  int max_qp = capabilities.bitrate_control().max_qp();

  VideoEncoderFactoryInterface::StaticEncoderSettings static_settings =
      StaticEncoderSettingsBuilder()
          .MaxEncodeDimensions(kDefaultResolution)
          .EncodingFormat({.sub_sampling = EncodingFormat::SubSampling::k420,
                           .bit_depth = 8})
          .CqpRcMode()
          .MaxNumberOfThreads(1)
          .Build();

  QpParserForTest qp_parser;
  std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();
  std::unique_ptr<VideoEncoderInterface> enc =
      encoder_factory_->CreateEncoder(static_settings, {});
  ASSERT_NE(enc, nullptr);

  int64_t timestamp_ms = 0;
  bool is_first_frame = true;

  for (int qp = min_qp; qp <= max_qp; ++qp) {
    scoped_refptr<VideoFrameBuffer> frame = frame_reader->PullFrame();
    EncOut out;
    if (is_first_frame) {
      enc->Encode(
          frame, TemporalUnitSettings(Timestamp::Millis(timestamp_ms)),
          ToVec({Fb().Cqp(qp).Res(kDefaultResolution).Upd(0).Key().Out(out)}));
      is_first_frame = false;
    } else {
      enc->Encode(
          frame, TemporalUnitSettings(Timestamp::Millis(timestamp_ms)),
          ToVec(
              {Fb().Cqp(qp).Res(kDefaultResolution).Ref({0}).Upd(0).Out(out)}));
    }
    timestamp_ms += 100;

    ASSERT_THAT(out, HasBitstreamAndMetaData());
    const EncodedData& ed = std::get<EncodedData>(out.res);
    // libaom quantizer resolution has step 4 across the 0-255 qindex range.
    EXPECT_NEAR(ed.encoded_qp, qp, 4);

    std::optional<uint32_t> parsed_qp = qp_parser.Parse(
        encoder_factory_->CodecName(), /*spatial_idx=*/0, out.bitstream);
    ASSERT_TRUE(parsed_qp.has_value())
        << "Failed to parse QP from bitstream for codec "
        << encoder_factory_->CodecName() << " at target QP " << qp;
    EXPECT_EQ(*parsed_qp, static_cast<uint32_t>(ed.encoded_qp));
  }
}

constexpr Resolution kQvgaResolution = {.width = 320, .height = 180};
constexpr Resolution kVgaResolution = {.width = 640, .height = 360};
constexpr Resolution kHdResolution = {.width = 1280, .height = 720};

struct FixedBitrateTestParams {
  std::string name;
  Resolution resolution;
  DataRate target_bitrate;
  TimeDelta duration;
  double max_deviation_pct;
};

class FixedBitrateRateControlTest
    : public VideoEncoderRateControlTestBase,
      public ::testing::WithParamInterface<
          std::tuple<FactoryCreator, FixedBitrateTestParams>> {
 protected:
  void SetUp() override { encoder_factory_ = std::get<0>(GetParam())(); }
};

TEST_P(FixedBitrateRateControlTest, AdheresToTargetBitrate) {
  if (!SupportsCbr()) {
    GTEST_SKIP() << "Encoder does not support CBR mode.";
  }
  const FixedBitrateTestParams& params = std::get<1>(GetParam());
  SetUpCbrEncoder(params.resolution);

  constexpr int kFps = 30;
  constexpr TimeDelta kFrameDuration = TimeDelta::Seconds(1) / kFps;
  const int num_frames =
      (params.duration.us() + kFrameDuration.us() / 2) / kFrameDuration.us();

  Encode(num_frames, kFrameDuration, params.target_bitrate, params.resolution);
  VerifyTotalDeviation(params.max_deviation_pct);
}

// Verifies that dynamically changing the bitrate target follows the target
// within reasonable bounds, evaluated over a 2-second sliding window and
// overall accumulated bytes.
TEST_P(VideoEncoderRateControlTest, ChangingBitrateTargetVga) {
  if (!SupportsCbr()) {
    GTEST_SKIP() << "Encoder does not support CBR mode.";
  }
  SetUpCbrEncoder(kVgaResolution);

  constexpr int kFps = 30;
  constexpr TimeDelta kFrameDuration = TimeDelta::Seconds(1) / kFps;
  constexpr int kWindowFrames = 2 * kFps;  // 2-second sliding window.

  // 1. 500 kbps for 2s (60 frames).
  Encode(60, kFrameDuration, DataRate::KilobitsPerSec(500), kVgaResolution);
  // 2. Drop to 100 kbps for 1s (30 frames).
  Encode(30, kFrameDuration, DataRate::KilobitsPerSec(100), kVgaResolution);
  // 3. Increase by 50 kbps every 200 ms (6 frames) until reaching 500 kbps.
  for (int rate_kbps = 150; rate_kbps < 500; rate_kbps += 50) {
    Encode(6, kFrameDuration, DataRate::KilobitsPerSec(rate_kbps),
           kVgaResolution);
  }
  // 4. Stay at 500 kbps for 1s (30 frames).
  Encode(30, kFrameDuration, DataRate::KilobitsPerSec(500), kVgaResolution);

  // (1) Check that deviation from optimal behavior over any 2-second sliding
  // window does not exceed 20%.
  VerifyFrameBasedSlidingWindowBitrateDeviation(kWindowFrames,
                                                /*min_allowed_dev_pct=*/-20.0,
                                                /*max_allowed_dev_pct=*/20.0);

  // (2) Check that the sum of bytes sent is within 5% of the optimal behavior.
  VerifyTotalDeviation(/*max_deviation_pct=*/5.0);
}

// Verifies that the CBR targets are adhered to even when input frame rate
// changes. Evaluated over a 2-second sliding window and overall accumulated
// bytes.
TEST_P(VideoEncoderRateControlTest, ChangingFramerateVga) {
  if (!SupportsCbr()) {
    GTEST_SKIP() << "Encoder does not support CBR mode.";
  }
  SetUpCbrEncoder(kVgaResolution);

  constexpr DataRate kTargetBitrate = DataRate::KilobitsPerSec(500);

  auto encode_segment = [&](int fps, TimeDelta duration) {
    TimeDelta frame_duration = TimeDelta::Seconds(1) / fps;
    int num_frames =
        (duration.us() + frame_duration.us() / 2) / frame_duration.us();
    Encode(num_frames, frame_duration, kTargetBitrate, kVgaResolution);
  };

  // 1. 30 fps for 2s.
  encode_segment(30, TimeDelta::Seconds(2));
  // 2. Drop to 10 fps for 1s.
  encode_segment(10, TimeDelta::Seconds(1));
  // 3. Step up gradually back to 30 fps (15 fps, 20 fps, 25 fps for 200ms
  // each).
  encode_segment(15, TimeDelta::Millis(200));
  encode_segment(20, TimeDelta::Millis(200));
  encode_segment(25, TimeDelta::Millis(200));
  // 4. Stay at 30 fps for 2s.
  encode_segment(30, TimeDelta::Seconds(2));

  // (1) Check that deviation from optimal behavior over any 1-second sliding
  // window does not exceed 20%.
  VerifyTimeBasedSlidingWindowBitrateDeviation(TimeDelta::Seconds(1),
                                               /*min_allowed_dev_pct=*/-20.0,
                                               /*max_allowed_dev_pct=*/20.0);

  // (2) Check that the sum of bytes sent is within 5% of the optimal behavior.
  VerifyTotalDeviation(/*max_deviation_pct=*/5.0);
}

// TODO(bugs.webrtc.org/496266459): Run at least a subset with varying content.

// TODO(bugs.webrtc.org/496266459): Implement camera switching test.

// TODO(bugs.webrtc.org/496266459): Add temporal/spatial layer allocation test.

// TODO(bugs.webrtc.org/496266459): Add screenshare tests.

// TODO(bugs.webrtc.org/496266459): Add ZeroHz screenshare tests.

std::unique_ptr<VideoEncoderFactoryInterface> CreateLibaomAv1EncoderFactory() {
  return std::make_unique<LibaomAv1EncoderFactory>();
}

INSTANTIATE_TEST_SUITE_P(LibaomAv1,
                         VideoEncoderRateControlTest,
                         ::testing::Values(CreateLibaomAv1EncoderFactory));

const FixedBitrateTestParams kFixedBitrateConfigs[] = {
    {"VgaNormalBitrate", kVgaResolution, DataRate::KilobitsPerSec(500),
     TimeDelta::Seconds(5), 5.0},
    {"VgaLowBitrate", kVgaResolution, DataRate::KilobitsPerSec(100),
     TimeDelta::Seconds(10), 5.0},
    {"VgaHighBitrate", kVgaResolution, DataRate::KilobitsPerSec(1500),
     TimeDelta::Seconds(5), 5.0},
    {"QvgaNormalBitrate", kQvgaResolution, DataRate::KilobitsPerSec(125),
     TimeDelta::Seconds(5), 6.0},
    {"QvgaLowBitrate", kQvgaResolution, DataRate::KilobitsPerSec(25),
     TimeDelta::Seconds(10), 10.0},
    {"QvgaHighBitrate", kQvgaResolution, DataRate::KilobitsPerSec(375),
     TimeDelta::Seconds(5), 5.0},
    {"HdNormalBitrate", kHdResolution, DataRate::KilobitsPerSec(2000),
     TimeDelta::Seconds(5), 5.0},
    {"HdLowBitrate", kHdResolution, DataRate::KilobitsPerSec(400),
     TimeDelta::Seconds(10), 5.0},
    {"HdHighBitrate", kHdResolution, DataRate::KilobitsPerSec(6000),
     TimeDelta::Seconds(5), 5.0},
};

std::string FixedBitrateTestName(
    const ::testing::TestParamInfo<
        std::tuple<FactoryCreator, FixedBitrateTestParams>>& info) {
  return std::get<1>(info.param).name;
}

INSTANTIATE_TEST_SUITE_P(
    LibaomAv1,
    FixedBitrateRateControlTest,
    ::testing::Combine(::testing::Values(CreateLibaomAv1EncoderFactory),
                       ::testing::ValuesIn(kFixedBitrateConfigs)),
    FixedBitrateTestName);

}  // namespace
}  // namespace webrtc
