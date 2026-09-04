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
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "api/environment/environment.h"
#include "api/scoped_refptr.h"
#include "api/units/timestamp.h"
#include "api/video/video_frame_buffer.h"
#include "api/video_codecs/libaom_av1_encoder_factory.h"
#include "api/video_codecs/test/video_codec_test_utils.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_builders.h"
#include "api/video_codecs/video_encoder_builders_for_test.h"
#include "api/video_codecs/video_encoder_factory_interface.h"
#include "api/video_codecs/video_encoder_interface.h"
#include "api/video_codecs/video_encoding_general.h"
#include "test/create_test_environment.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/qp_parser_for_test.h"
#include "test/testsupport/frame_reader.h"

namespace webrtc {
namespace {

class VideoEncoderRateControlTest
    : public ::testing::TestWithParam<FactoryCreator> {
 protected:
  VideoEncoderRateControlTest() : env_(CreateTestEnvironment()) {}

  void SetUp() override {
    factory_ = GetParam()();
    decoder_factory_ = CreateTestDecoderFactory();
  }

  Environment env_;
  std::unique_ptr<VideoEncoderFactoryInterface> factory_;
  std::unique_ptr<VideoDecoderFactory> decoder_factory_;
};

TEST_P(VideoEncoderRateControlTest, ConstantQpMatchesBitstreamAndEncoderQp) {
  VideoEncoderFactoryInterface::Capabilities capabilities =
      factory_->GetEncoderCapabilities();
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
      factory_->CreateEncoder(static_settings, {});
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
        factory_->CodecName(), /*spatial_idx=*/0, out.bitstream);
    ASSERT_TRUE(parsed_qp.has_value())
        << "Failed to parse QP from bitstream for codec "
        << factory_->CodecName() << " at target QP " << qp;
    EXPECT_EQ(*parsed_qp, static_cast<uint32_t>(ed.encoded_qp));
  }
}

// TODO(bugs.webrt.org/496266459): Add CBR tests.

std::unique_ptr<VideoEncoderFactoryInterface> CreateLibaomAv1EncoderFactory() {
  return std::make_unique<LibaomAv1EncoderFactory>();
}

INSTANTIATE_TEST_SUITE_P(LibaomAv1,
                         VideoEncoderRateControlTest,
                         ::testing::Values(CreateLibaomAv1EncoderFactory));

}  // namespace
}  // namespace webrtc
