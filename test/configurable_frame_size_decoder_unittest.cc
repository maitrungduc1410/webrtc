/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/configurable_frame_size_decoder.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "api/scoped_refptr.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_image.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_type.h"
#include "api/video_codecs/video_decoder.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "test/configurable_frame_size_encoder.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {
namespace {

using ::testing::IsEmpty;
using ::testing::SizeIs;

constexpr size_t kFrameSize = 100;
constexpr Timestamp kStartTime = Timestamp::Seconds(10'000);
constexpr TimeDelta kFrameInterval = TimeDelta::Millis(10);

class RecordingDecodedImageCallback : public DecodedImageCallback {
 public:
  int32_t Decoded(VideoFrame& frame) override {
    frames.push_back(frame);
    return 0;
  }

  std::vector<VideoFrame> frames;
};

Timestamp CaptureTime(uint64_t frame_id) {
  return kStartTime + static_cast<int64_t>(frame_id) * kFrameInterval;
}

uint32_t RtpTimestamp(uint64_t frame_id) {
  return static_cast<uint32_t>(frame_id * 900);
}

// Returns a frame as ConfigurableFrameSizeEncoder produces it with
// `Config::write_frame_header` set.
EncodedImage CreateFrame(uint64_t frame_id, bool is_key_frame) {
  scoped_refptr<EncodedImageBuffer> buffer =
      EncodedImageBuffer::Create(kFrameSize);
  ConfigurableFrameSizeEncoder::WriteFrame(
      {.frame_id = frame_id,
       .capture_time = CaptureTime(frame_id),
       .is_key_frame = is_key_frame},
      std::span(buffer->data(), buffer->size()));
  EncodedImage image;
  image.SetEncodedData(buffer);
  image.SetRtpTimestamp(RtpTimestamp(frame_id));
  image.set_frame_type(is_key_frame ? VideoFrameType::kVideoFrameKey
                                    : VideoFrameType::kVideoFrameDelta);
  return image;
}

class ConfigurableFrameSizeDecoderTest : public ::testing::Test {
 protected:
  ConfigurableFrameSizeDecoderTest() {
    EXPECT_TRUE(decoder_.Configure(VideoDecoder::Settings()));
    EXPECT_EQ(decoder_.RegisterDecodeCompleteCallback(&decoded_),
              WEBRTC_VIDEO_CODEC_OK);
  }

  ConfigurableFrameSizeDecoder decoder_;
  RecordingDecodedImageCallback decoded_;
};

TEST_F(ConfigurableFrameSizeDecoderTest, OutputsFrameIdAndCaptureTime) {
  for (uint64_t frame_id = 0; frame_id < 3; ++frame_id) {
    EXPECT_EQ(decoder_.Decode(CreateFrame(frame_id,
                                          /*is_key_frame=*/frame_id == 0),
                              /*render_time_ms=*/0),
              WEBRTC_VIDEO_CODEC_OK);
  }
  ASSERT_THAT(decoded_.frames, SizeIs(3));
  for (uint64_t frame_id = 0; frame_id < 3; ++frame_id) {
    const VideoFrame& frame = decoded_.frames[frame_id];
    EXPECT_EQ(frame.id(), frame_id);
    EXPECT_EQ(frame.reference_time(), CaptureTime(frame_id));
    EXPECT_EQ(frame.rtp_timestamp(), RtpTimestamp(frame_id));
  }
}

TEST_F(ConfigurableFrameSizeDecoderTest, RejectsBrokenChainUntilKeyFrame) {
  EXPECT_EQ(decoder_.Decode(CreateFrame(0, /*is_key_frame=*/true), 0),
            WEBRTC_VIDEO_CODEC_OK);
  // Frame 1 is missing.
  EXPECT_EQ(decoder_.Decode(CreateFrame(2, /*is_key_frame=*/false), 0),
            WEBRTC_VIDEO_CODEC_ERROR);
  EXPECT_EQ(decoder_.Decode(CreateFrame(3, /*is_key_frame=*/false), 0),
            WEBRTC_VIDEO_CODEC_ERROR);
  EXPECT_EQ(decoder_.Decode(CreateFrame(4, /*is_key_frame=*/true), 0),
            WEBRTC_VIDEO_CODEC_OK);
  ASSERT_THAT(decoded_.frames, SizeIs(2));
  EXPECT_EQ(decoded_.frames[1].id(), 4);
}

TEST_F(ConfigurableFrameSizeDecoderTest, RejectsCorruptPayload) {
  EncodedImage image = CreateFrame(0, /*is_key_frame=*/true);
  scoped_refptr<EncodedImageBuffer> corrupt =
      EncodedImageBuffer::Create(image.data(), image.size());
  std::span<uint8_t>(corrupt->data(), corrupt->size()).back() ^= 0xff;
  image.SetEncodedData(corrupt);

  EXPECT_EQ(decoder_.Decode(image, 0), WEBRTC_VIDEO_CODEC_ERROR);
  EXPECT_THAT(decoded_.frames, IsEmpty());
}

}  // namespace
}  // namespace test
}  // namespace webrtc
