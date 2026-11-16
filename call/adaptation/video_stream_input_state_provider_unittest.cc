/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/video_stream_input_state_provider.h"

#include <optional>
#include <utility>

#include "api/video/video_codec_type.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "call/adaptation/encoder_settings.h"
#include "call/adaptation/test/fake_frame_rate_provider.h"
#include "call/adaptation/video_stream_input_state.h"
#include "test/gtest.h"
#include "video/config/video_encoder_config.h"

namespace webrtc {

TEST(VideoStreamInputStateProviderTest, DefaultValues) {
  FakeFrameRateProvider frame_rate_provider;
  VideoStreamInputStateProvider input_state_provider(&frame_rate_provider);
  VideoStreamInputState input_state = input_state_provider.InputState();
  EXPECT_EQ(false, input_state.has_input());
  EXPECT_EQ(std::nullopt, input_state.frame_size_pixels());
  EXPECT_EQ(0, input_state.frames_per_second());
  EXPECT_EQ(VideoCodecType::kVideoCodecGeneric, input_state.video_codec_type());
  EXPECT_EQ(kDefaultMinPixelsPerFrame, input_state.min_pixels_per_frame());
  EXPECT_EQ(std::nullopt, input_state.single_active_stream_pixels());
  EXPECT_EQ(std::nullopt, input_state.max_active_stream_pixels());
}

TEST(VideoStreamInputStateProviderTest, ValuesSet) {
  FakeFrameRateProvider frame_rate_provider;
  VideoStreamInputStateProvider input_state_provider(&frame_rate_provider);
  input_state_provider.OnHasInputChanged(true);
  input_state_provider.OnFrameSizeObserved(42);
  frame_rate_provider.set_fps(123);
  VideoEncoder::EncoderInfo encoder_info;
  encoder_info.scaling_settings.min_pixels_per_frame = 1337;
  VideoEncoderConfig encoder_config;
  encoder_config.codec_type = VideoCodecType::kVideoCodecVP9;
  VideoCodec video_codec;
  video_codec.codecType = VideoCodecType::kVideoCodecVP8;
  video_codec.numberOfSimulcastStreams = 2;
  video_codec.simulcastStream[0].active = false;
  video_codec.simulcastStream[1].active = true;
  video_codec.simulcastStream[1].width = 111;
  video_codec.simulcastStream[1].height = 222;
  input_state_provider.OnEncoderSettingsChanged(EncoderSettings(
      std::move(encoder_info), std::move(encoder_config), video_codec));
  VideoStreamInputState input_state = input_state_provider.InputState();
  EXPECT_EQ(true, input_state.has_input());
  EXPECT_EQ(42, input_state.frame_size_pixels());
  EXPECT_EQ(123, input_state.frames_per_second());
  EXPECT_EQ(VideoCodecType::kVideoCodecVP9, input_state.video_codec_type());
  EXPECT_EQ(1337, input_state.min_pixels_per_frame());
  EXPECT_EQ(111 * 222, input_state.single_active_stream_pixels());
  EXPECT_EQ(std::nullopt, input_state.max_active_stream_pixels());
}

TEST(VideoStreamInputStateProviderTest,
     MaxActiveStreamPixelsSetWhenHasScaleResolutionDownTo) {
  FakeFrameRateProvider frame_rate_provider;
  VideoStreamInputStateProvider input_state_provider(&frame_rate_provider);
  VideoEncoder::EncoderInfo encoder_info;
  VideoEncoderConfig encoder_config;
  encoder_config.codec_type = VideoCodecType::kVideoCodecVP8;
  encoder_config.simulcast_layers.resize(3);
  encoder_config.simulcast_layers[0].scale_resolution_down_to = {.width = 320,
                                                                 .height = 180};
  encoder_config.simulcast_layers[1].scale_resolution_down_to = {.width = 640,
                                                                 .height = 360};
  encoder_config.simulcast_layers[2].scale_resolution_down_to = {.width = 1280,
                                                                 .height = 720};
  VideoCodec video_codec;
  video_codec.codecType = VideoCodecType::kVideoCodecVP8;
  video_codec.numberOfSimulcastStreams = 3;
  video_codec.simulcastStream[0].active = true;
  video_codec.simulcastStream[0].width = 320;
  video_codec.simulcastStream[0].height = 180;
  video_codec.simulcastStream[1].active = true;
  video_codec.simulcastStream[1].width = 640;
  video_codec.simulcastStream[1].height = 360;
  video_codec.simulcastStream[2].active = false;
  video_codec.simulcastStream[2].width = 1280;
  video_codec.simulcastStream[2].height = 720;
  input_state_provider.OnEncoderSettingsChanged(EncoderSettings(
      std::move(encoder_info), std::move(encoder_config), video_codec));
  VideoStreamInputState input_state = input_state_provider.InputState();
  EXPECT_EQ(input_state.single_active_stream_pixels(), std::nullopt);
  EXPECT_EQ(input_state.max_active_stream_pixels(), 640 * 360);
}

}  // namespace webrtc
