/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/fake_video_codec_factory.h"

#include <memory>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "api/environment/environment.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_encoder.h"
#include "test/fake_decoder.h"
#include "test/fake_encoder.h"

namespace {

const char kFakeCodecFactoryCodecName[] = "FakeCodec";

}  // anonymous namespace

namespace webrtc {

std::vector<SdpVideoFormat> FakeVideoEncoderFactory::GetSupportedFormats()
    const {
  const absl::InlinedVector<ScalabilityMode, kScalabilityModeCount>
      kSupportedScalabilityModes = {ScalabilityMode::kL1T1,
                                    ScalabilityMode::kL1T2,
                                    ScalabilityMode::kL1T3};

  return std::vector<SdpVideoFormat>(
      1, SdpVideoFormat(kFakeCodecFactoryCodecName, {},
                        kSupportedScalabilityModes));
}

std::unique_ptr<VideoEncoder> FakeVideoEncoderFactory::Create(
    const Environment& env,
    const SdpVideoFormat& /* format */) {
  return std::make_unique<test::FakeEncoder>(env);
}

FakeVideoDecoderFactory::FakeVideoDecoderFactory() = default;

// static
std::unique_ptr<VideoDecoder> FakeVideoDecoderFactory::CreateVideoDecoder() {
  return std::make_unique<test::FakeDecoder>();
}

std::vector<SdpVideoFormat> FakeVideoDecoderFactory::GetSupportedFormats()
    const {
  return std::vector<SdpVideoFormat>(
      1, SdpVideoFormat(kFakeCodecFactoryCodecName));
}

std::unique_ptr<VideoDecoder> FakeVideoDecoderFactory::Create(
    const Environment& /* env */,
    const SdpVideoFormat& /* format */) {
  return std::make_unique<test::FakeDecoder>();
}

}  // namespace webrtc
