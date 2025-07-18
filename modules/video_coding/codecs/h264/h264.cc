/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include "modules/video_coding/codecs/h264/include/h264.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/inlined_vector.h"
#include "api/environment/environment.h"
#include "api/video_codecs/h264_profile_level_id.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_encoder.h"
#include "media/base/media_constants.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/trace_event.h"

#if defined(WEBRTC_USE_H264)
#include "modules/video_coding/codecs/h264/h264_decoder_impl.h"
#include "modules/video_coding/codecs/h264/h264_encoder_impl.h"
#endif

namespace webrtc {

namespace {

#if defined(WEBRTC_USE_H264)
bool g_rtc_use_h264 = true;
#endif

// If H.264 OpenH264/FFmpeg codec is supported.
bool IsH264CodecSupported() {
#if defined(WEBRTC_USE_H264)
  return g_rtc_use_h264;
#else
  return false;
#endif
}

constexpr ScalabilityMode kSupportedScalabilityModes[] = {
    ScalabilityMode::kL1T1, ScalabilityMode::kL1T2, ScalabilityMode::kL1T3};

}  // namespace

SdpVideoFormat CreateH264Format(H264Profile profile,
                                H264Level level,
                                const std::string& packetization_mode,
                                bool add_scalability_modes) {
  const std::optional<std::string> profile_string =
      H264ProfileLevelIdToString(H264ProfileLevelId(profile, level));
  RTC_CHECK(profile_string);
  absl::InlinedVector<ScalabilityMode, kScalabilityModeCount> scalability_modes;
  if (add_scalability_modes) {
    for (const auto scalability_mode : kSupportedScalabilityModes) {
      scalability_modes.push_back(scalability_mode);
    }
  }
  return SdpVideoFormat(kH264CodecName,
                        {{kH264FmtpProfileLevelId, *profile_string},
                         {kH264FmtpLevelAsymmetryAllowed, "1"},
                         {kH264FmtpPacketizationMode, packetization_mode}},
                        scalability_modes);
}

void DisableRtcUseH264() {
#if defined(WEBRTC_USE_H264)
  g_rtc_use_h264 = false;
#endif
}

std::vector<SdpVideoFormat> SupportedH264Codecs(bool add_scalability_modes) {
  TRACE_EVENT0("webrtc", __func__);
  if (!IsH264CodecSupported())
    return std::vector<SdpVideoFormat>();
  // We only support encoding Constrained Baseline Profile (CBP), but the
  // decoder supports more profiles. We can list all profiles here that are
  // supported by the decoder and that are also supersets of CBP, i.e. the
  // decoder for that profile is required to be able to decode CBP. This means
  // we can encode and send CBP even though we negotiated a potentially
  // higher profile. See the H264 spec for more information.
  //
  // We support both packetization modes 0 (mandatory) and 1 (optional,
  // preferred).
  return {CreateH264Format(H264Profile::kProfileBaseline, H264Level::kLevel3_1,
                           "1", add_scalability_modes),
          CreateH264Format(H264Profile::kProfileBaseline, H264Level::kLevel3_1,
                           "0", add_scalability_modes),
          CreateH264Format(H264Profile::kProfileConstrainedBaseline,
                           H264Level::kLevel3_1, "1", add_scalability_modes),
          CreateH264Format(H264Profile::kProfileConstrainedBaseline,
                           H264Level::kLevel3_1, "0", add_scalability_modes),
          CreateH264Format(H264Profile::kProfileMain, H264Level::kLevel3_1, "1",
                           add_scalability_modes),
          CreateH264Format(H264Profile::kProfileMain, H264Level::kLevel3_1, "0",
                           add_scalability_modes)};
}

std::vector<SdpVideoFormat> SupportedH264DecoderCodecs() {
  TRACE_EVENT0("webrtc", __func__);
  if (!IsH264CodecSupported())
    return std::vector<SdpVideoFormat>();

  std::vector<SdpVideoFormat> supportedCodecs = SupportedH264Codecs();

  // OpenH264 doesn't yet support High Predictive 4:4:4 encoding but it does
  // support decoding.
  supportedCodecs.push_back(CreateH264Format(
      H264Profile::kProfilePredictiveHigh444, H264Level::kLevel3_1, "1"));
  supportedCodecs.push_back(CreateH264Format(
      H264Profile::kProfilePredictiveHigh444, H264Level::kLevel3_1, "0"));

  return supportedCodecs;
}

H264EncoderSettings H264EncoderSettings::Parse(const SdpVideoFormat& format) {
  if (auto it = format.parameters.find(kH264FmtpPacketizationMode);
      it != format.parameters.end()) {
    if (it->second == "0") {
      return {.packetization_mode = H264PacketizationMode::SingleNalUnit};
    } else if (it->second == "1") {
      return {.packetization_mode = H264PacketizationMode::NonInterleaved};
    }
  }
  return {};
}

absl_nonnull std::unique_ptr<VideoEncoder> CreateH264Encoder(
    [[maybe_unused]] const Environment& env,
    [[maybe_unused]] H264EncoderSettings settings) {
#if defined(WEBRTC_USE_H264)
  RTC_CHECK(g_rtc_use_h264);
  RTC_LOG(LS_INFO) << "Creating H264EncoderImpl.";
  return std::make_unique<H264EncoderImpl>(env, settings);
#else
  RTC_CHECK_NOTREACHED();
#endif
}

bool H264Encoder::IsSupported() {
  return IsH264CodecSupported();
}

bool H264Encoder::SupportsScalabilityMode(ScalabilityMode scalability_mode) {
  for (const auto& entry : kSupportedScalabilityModes) {
    if (entry == scalability_mode) {
      return true;
    }
  }
  return false;
}

std::unique_ptr<H264Decoder> H264Decoder::Create() {
  RTC_DCHECK(H264Decoder::IsSupported());
#if defined(WEBRTC_USE_H264)
  RTC_CHECK(g_rtc_use_h264);
  RTC_LOG(LS_INFO) << "Creating H264DecoderImpl.";
  return std::make_unique<H264DecoderImpl>();
#else
  RTC_DCHECK_NOTREACHED();
  return nullptr;
#endif
}

bool H264Decoder::IsSupported() {
  return IsH264CodecSupported();
}

}  // namespace webrtc
