/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/configurable_frame_size_encoder.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "api/environment/environment.h"
#include "api/scoped_refptr.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_bitrate_allocation.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_type.h"
#include "api/video/video_timing.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "test/create_test_environment.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace test {
namespace {

using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::SizeIs;

constexpr int kFps = 60;
constexpr TimeDelta kFrameInterval = TimeDelta::Seconds(1) / kFps;
// 6 Mbps at 60 fps results in 12500 byte delta frames.
constexpr DataRate kBitrate = DataRate::KilobitsPerSec(6'000);
constexpr size_t kDeltaFrameSize = 12'500;
constexpr double kKeyFrameSizeFactor = 2.2;
constexpr TimeDelta kEncodeDelay = TimeDelta::Micros(3500);
constexpr VideoEncoder::Settings kEncoderSettings(/*number_of_cores=*/1,
                                                  /*max_payload_size=*/1200);

// All options enabled, as for tests of calls at high bitrates.
ConfigurableFrameSizeEncoder::Config HighBitrateConfig() {
  return {.frame_size_from_target_bitrate = true,
          .key_frame_size_factor = kKeyFrameSizeFactor,
          .key_frames_on_request_only = true,
          .write_frame_header = true,
          .encode_delay = kEncodeDelay,
          .playout_delay = VideoPlayoutDelay::Minimal(),
          .is_hardware_accelerated = false};
}

class RecordingEncodedImageCallback : public EncodedImageCallback {
 public:
  Result OnEncodedImage(const EncodedImage& image,
                        const CodecSpecificInfo* codec_specific_info) override {
    images.push_back(image);
    codec_types.push_back(codec_specific_info->codecType);
    return Result(Result::OK);
  }
  void OnFrameDropped(uint32_t rtp_timestamp,
                      int /* spatial_id */,
                      bool /* is_end_of_temporal_unit */) override {
    dropped_rtp_timestamps.push_back(rtp_timestamp);
  }

  std::vector<EncodedImage> images;
  std::vector<VideoCodecType> codec_types;
  std::vector<uint32_t> dropped_rtp_timestamps;
};

std::optional<ConfigurableFrameSizeEncoder::FrameHeader> Parse(
    const EncodedImage& image) {
  return ConfigurableFrameSizeEncoder::ParseFrame(
      std::span(image.data(), image.size()));
}

class ConfigurableFrameSizeEncoderTest : public ::testing::Test {
 protected:
  ConfigurableFrameSizeEncoderTest()
      : time_controller_(Timestamp::Seconds(10'000)),
        env_(CreateTestEnvironment({.time = &time_controller_})),
        buffer_(I420Buffer::Create(320, 180)) {}

  std::unique_ptr<VideoEncoder> CreateEncoder(
      const ConfigurableFrameSizeEncoder::Config& config =
          HighBitrateConfig()) {
    auto encoder = std::make_unique<ConfigurableFrameSizeEncoder>(env_, config);
    VideoCodec codec_settings;
    codec_settings.codecType = kVideoCodecGeneric;
    codec_settings.width = 320;
    codec_settings.height = 180;
    codec_settings.maxFramerate = kFps;
    codec_settings.startBitrate = kBitrate.kbps();
    EXPECT_EQ(encoder->InitEncode(&codec_settings, kEncoderSettings),
              WEBRTC_VIDEO_CODEC_OK);
    EXPECT_EQ(encoder->RegisterEncodeCompleteCallback(&encoded_),
              WEBRTC_VIDEO_CODEC_OK);
    VideoBitrateAllocation allocation;
    allocation.SetBitrate(0, 0, kBitrate.bps());
    encoder->SetRates(
        VideoEncoder::RateControlParameters(allocation, kFps, kBitrate));
    return encoder;
  }

  VideoFrame NextFrame() {
    rtp_timestamp_ += 90'000 / kFps;
    return VideoFrame::Builder()
        .set_video_frame_buffer(buffer_)
        .set_rtp_timestamp(rtp_timestamp_)
        .set_timestamp_us(env_.clock().TimeInMicroseconds())
        .build();
  }

  // Encodes `num_frames` frames at the nominal frame rate and waits for them
  // to be delivered.
  void EncodeFrames(VideoEncoder& encoder, int num_frames) {
    for (int i = 0; i < num_frames; ++i) {
      ASSERT_EQ(encoder.Encode(NextFrame(), /*frame_types=*/nullptr),
                WEBRTC_VIDEO_CODEC_OK);
      time_controller_.AdvanceTime(kFrameInterval);
    }
  }

  GlobalSimulatedTimeController time_controller_;
  const Environment env_;
  const scoped_refptr<I420Buffer> buffer_;
  uint32_t rtp_timestamp_ = 0;
  RecordingEncodedImageCallback encoded_;
};

TEST_F(ConfigurableFrameSizeEncoderTest,
       ByDefaultDeliversZeroFilledKeyFramesOfFixedSizeSynchronously) {
  ConfigurableFrameSizeEncoder encoder(/*max_frame_size=*/1000);
  int post_encode_calls = 0;
  encoder.RegisterPostEncodeCallback([&] { ++post_encode_calls; });
  ASSERT_EQ(encoder.InitEncode(/*codec_settings=*/nullptr, kEncoderSettings),
            WEBRTC_VIDEO_CODEC_OK);
  ASSERT_EQ(encoder.RegisterEncodeCompleteCallback(&encoded_),
            WEBRTC_VIDEO_CODEC_OK);

  ASSERT_EQ(encoder.Encode(NextFrame(), /*frame_types=*/nullptr),
            WEBRTC_VIDEO_CODEC_OK);
  encoder.SetFrameSize(500);
  ASSERT_EQ(encoder.Encode(NextFrame(), /*frame_types=*/nullptr),
            WEBRTC_VIDEO_CODEC_OK);

  EXPECT_EQ(post_encode_calls, 2);
  ASSERT_THAT(encoded_.images, SizeIs(2));
  EXPECT_THAT(encoded_.codec_types,
              ElementsAre(kVideoCodecGeneric, kVideoCodecGeneric));
  EXPECT_EQ(encoded_.images[0].size(), 1000u);
  EXPECT_EQ(encoded_.images[1].size(), 500u);
  for (const EncodedImage& image : encoded_.images) {
    EXPECT_TRUE(image.IsKey());
    std::span<const uint8_t> payload(image.data(), image.size());
    EXPECT_THAT(std::vector<uint8_t>(payload.begin(), payload.end()), Each(0));
  }
}

TEST_F(ConfigurableFrameSizeEncoderTest, EncoderInfoFollowsConfig) {
  const VideoEncoder::EncoderInfo default_info =
      ConfigurableFrameSizeEncoder(/*max_frame_size=*/1000).GetEncoderInfo();
  EXPECT_TRUE(default_info.is_hardware_accelerated);
  EXPECT_FALSE(default_info.has_trusted_rate_controller);

  const VideoEncoder::EncoderInfo high_bitrate_info =
      ConfigurableFrameSizeEncoder(env_, HighBitrateConfig()).GetEncoderInfo();
  EXPECT_FALSE(high_bitrate_info.is_hardware_accelerated);
  EXPECT_TRUE(high_bitrate_info.has_trusted_rate_controller);
}

TEST_F(ConfigurableFrameSizeEncoderTest, DeliversFrameAfterEncodeDelay) {
  std::unique_ptr<VideoEncoder> encoder = CreateEncoder();
  ASSERT_EQ(encoder->Encode(NextFrame(), /*frame_types=*/nullptr),
            WEBRTC_VIDEO_CODEC_OK);
  EXPECT_THAT(encoded_.images, IsEmpty());
  time_controller_.AdvanceTime(kEncodeDelay - TimeDelta::Micros(100));
  EXPECT_THAT(encoded_.images, IsEmpty());
  time_controller_.AdvanceTime(TimeDelta::Micros(100));
  EXPECT_THAT(encoded_.images, SizeIs(1));
  EXPECT_THAT(encoded_.codec_types, ElementsAre(kVideoCodecGeneric));
}

TEST_F(ConfigurableFrameSizeEncoderTest, OnlyFirstFrameIsKeyFrame) {
  std::unique_ptr<VideoEncoder> encoder = CreateEncoder();
  EncodeFrames(*encoder, 3);
  ASSERT_THAT(encoded_.images, SizeIs(3));
  EXPECT_EQ(encoded_.images[0].frame_type(), VideoFrameType::kVideoFrameKey);
  EXPECT_EQ(encoded_.images[1].frame_type(), VideoFrameType::kVideoFrameDelta);
  EXPECT_EQ(encoded_.images[2].frame_type(), VideoFrameType::kVideoFrameDelta);
}

TEST_F(ConfigurableFrameSizeEncoderTest, FrameSizesFollowBitrate) {
  std::unique_ptr<VideoEncoder> encoder = CreateEncoder();
  EncodeFrames(*encoder, 3);
  ASSERT_THAT(encoded_.images, SizeIs(3));
  EXPECT_EQ(encoded_.images[0].size(),
            static_cast<size_t>(kDeltaFrameSize * kKeyFrameSizeFactor));
  EXPECT_EQ(encoded_.images[1].size(), kDeltaFrameSize);
  EXPECT_EQ(encoded_.images[2].size(), kDeltaFrameSize);
}

TEST_F(ConfigurableFrameSizeEncoderTest, ProducesKeyFrameOnRequest) {
  std::unique_ptr<VideoEncoder> encoder = CreateEncoder();
  EncodeFrames(*encoder, 2);
  const std::vector<VideoFrameType> key_frame = {
      VideoFrameType::kVideoFrameKey};
  ASSERT_EQ(encoder->Encode(NextFrame(), &key_frame), WEBRTC_VIDEO_CODEC_OK);
  time_controller_.AdvanceTime(kFrameInterval);
  EncodeFrames(*encoder, 1);
  ASSERT_THAT(encoded_.images, SizeIs(4));
  EXPECT_TRUE(encoded_.images[0].IsKey());
  EXPECT_TRUE(encoded_.images[1].IsDelta());
  EXPECT_TRUE(encoded_.images[2].IsKey());
  EXPECT_TRUE(encoded_.images[3].IsDelta());
}

TEST_F(ConfigurableFrameSizeEncoderTest, SetsPlayoutDelayAndEndOfTemporalUnit) {
  std::unique_ptr<VideoEncoder> encoder = CreateEncoder();
  EncodeFrames(*encoder, 2);
  ASSERT_THAT(encoded_.images, SizeIs(2));
  for (const EncodedImage& image : encoded_.images) {
    EXPECT_EQ(image.PlayoutDelay(), VideoPlayoutDelay::Minimal());
    EXPECT_TRUE(image.is_end_of_temporal_unit());
  }
}

TEST_F(ConfigurableFrameSizeEncoderTest, WritesFrameIdAndCaptureTime) {
  std::unique_ptr<VideoEncoder> encoder = CreateEncoder();
  const Timestamp start = env_.clock().CurrentTime();
  EncodeFrames(*encoder, 3);
  ASSERT_THAT(encoded_.images, SizeIs(3));
  for (int i = 0; i < 3; ++i) {
    std::optional<ConfigurableFrameSizeEncoder::FrameHeader> header =
        Parse(encoded_.images[i]);
    ASSERT_TRUE(header.has_value());
    EXPECT_EQ(header->frame_id, static_cast<uint64_t>(i));
    EXPECT_EQ(header->capture_time, start + i * kFrameInterval);
    EXPECT_EQ(header->is_key_frame, i == 0);
  }
}

TEST_F(ConfigurableFrameSizeEncoderTest, ParseFrameRejectsCorruptFiller) {
  std::vector<uint8_t> payload(100);
  ConfigurableFrameSizeEncoder::WriteFrame(
      {.frame_id = 5, .capture_time = Timestamp::Millis(1234)}, payload);
  std::optional<ConfigurableFrameSizeEncoder::FrameHeader> header =
      ConfigurableFrameSizeEncoder::ParseFrame(payload);
  ASSERT_TRUE(header.has_value());
  EXPECT_EQ(header->frame_id, 5u);
  EXPECT_EQ(header->capture_time, Timestamp::Millis(1234));
  EXPECT_FALSE(header->is_key_frame);

  payload.back() ^= 0xff;
  EXPECT_FALSE(ConfigurableFrameSizeEncoder::ParseFrame(payload).has_value());
}

TEST_F(ConfigurableFrameSizeEncoderTest, DropsFramesWhenTooManyAreInFlight) {
  ConfigurableFrameSizeEncoder::Config config = HighBitrateConfig();
  config.encode_delay = TimeDelta::Millis(40);
  config.max_frames_in_flight = 2;
  std::unique_ptr<VideoEncoder> encoder = CreateEncoder(config);
  // The first two frames are in flight when the third is passed to the
  // encoder 33 ms later.
  for (int i = 0; i < 3; ++i) {
    ASSERT_EQ(encoder->Encode(NextFrame(), /*frame_types=*/nullptr),
              WEBRTC_VIDEO_CODEC_OK);
    time_controller_.AdvanceTime(kFrameInterval);
  }
  time_controller_.AdvanceTime(TimeDelta::Millis(100));
  EXPECT_THAT(encoded_.dropped_rtp_timestamps, ElementsAre(rtp_timestamp_));
  ASSERT_THAT(encoded_.images, SizeIs(2));

  // Dropped frames don't break the chain of frame ids.
  EncodeFrames(*encoder, 1);
  time_controller_.AdvanceTime(TimeDelta::Millis(100));
  ASSERT_THAT(encoded_.images, SizeIs(3));
  std::optional<ConfigurableFrameSizeEncoder::FrameHeader> header =
      Parse(encoded_.images[2]);
  ASSERT_TRUE(header.has_value());
  EXPECT_EQ(header->frame_id, 2u);
  EXPECT_FALSE(header->is_key_frame);
}

TEST_F(ConfigurableFrameSizeEncoderTest, DropsFramesWhenTargetBitrateIsZero) {
  std::unique_ptr<VideoEncoder> encoder = CreateEncoder();
  EncodeFrames(*encoder, 1);

  encoder->SetRates(VideoEncoder::RateControlParameters(
      VideoBitrateAllocation(), kFps, DataRate::Zero()));
  const VideoFrame dropped = NextFrame();
  ASSERT_EQ(encoder->Encode(dropped, /*frame_types=*/nullptr),
            WEBRTC_VIDEO_CODEC_OK);
  time_controller_.AdvanceTime(kFrameInterval);
  EXPECT_THAT(encoded_.dropped_rtp_timestamps,
              ElementsAre(dropped.rtp_timestamp()));
  ASSERT_THAT(encoded_.images, SizeIs(1));

  VideoBitrateAllocation allocation;
  allocation.SetBitrate(0, 0, kBitrate.bps());
  encoder->SetRates(
      VideoEncoder::RateControlParameters(allocation, kFps, kBitrate));
  EncodeFrames(*encoder, 1);
  ASSERT_THAT(encoded_.images, SizeIs(2));
  std::optional<ConfigurableFrameSizeEncoder::FrameHeader> header =
      Parse(encoded_.images[1]);
  ASSERT_TRUE(header.has_value());
  EXPECT_EQ(header->frame_id, 1u);
}

TEST_F(ConfigurableFrameSizeEncoderTest, ReleaseDropsPendingFrames) {
  std::unique_ptr<VideoEncoder> encoder = CreateEncoder();
  ASSERT_EQ(encoder->Encode(NextFrame(), /*frame_types=*/nullptr),
            WEBRTC_VIDEO_CODEC_OK);
  EXPECT_EQ(encoder->Release(), WEBRTC_VIDEO_CODEC_OK);
  time_controller_.AdvanceTime(TimeDelta::Millis(100));
  EXPECT_THAT(encoded_.images, IsEmpty());
}

}  // namespace
}  // namespace test
}  // namespace webrtc
