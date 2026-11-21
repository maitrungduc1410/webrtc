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

#include <cstdint>
#include <optional>
#include <span>

#include "api/sequence_checker.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/render_resolution.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_decoder.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "test/configurable_frame_size_encoder.h"

namespace webrtc {
namespace test {

bool ConfigurableFrameSizeDecoder::Configure(const Settings& settings) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  const RenderResolution resolution = settings.max_render_resolution();
  if (resolution.Valid()) {
    width_ = resolution.Width();
    height_ = resolution.Height();
  }
  buffer_ = nullptr;
  last_frame_id_ = std::nullopt;
  return true;
}

int32_t ConfigurableFrameSizeDecoder::Decode(const EncodedImage& input,
                                             int64_t render_time_ms) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (callback_ == nullptr) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  std::optional<ConfigurableFrameSizeEncoder::FrameHeader> header =
      ConfigurableFrameSizeEncoder::ParseFrame(
          std::span(input.data(), input.size()));
  if (!header.has_value() ||
      (!header->is_key_frame && (!last_frame_id_.has_value() ||
                                 header->frame_id != *last_frame_id_ + 1))) {
    // A corrupt frame or a delta frame that does not continue the chain of
    // decoded frames. Returning an error triggers a key frame request.
    last_frame_id_ = std::nullopt;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  last_frame_id_ = header->frame_id;

  if (buffer_ == nullptr) {
    buffer_ = I420Buffer::Create(width_, height_);
    I420Buffer::SetBlack(buffer_.get());
  }
  VideoFrame frame = VideoFrame::Builder()
                         .set_video_frame_buffer(buffer_)
                         .set_timestamp_ms(render_time_ms)
                         .set_rtp_timestamp(input.RtpTimestamp())
                         .set_ntp_time_ms(input.ntp_time_ms_)
                         .set_id(static_cast<uint16_t>(header->frame_id))
                         .set_reference_time(header->capture_time)
                         .build();
  callback_->Decoded(frame);
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t ConfigurableFrameSizeDecoder::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t ConfigurableFrameSizeDecoder::Release() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  last_frame_id_ = std::nullopt;
  buffer_ = nullptr;
  return WEBRTC_VIDEO_CODEC_OK;
}

VideoDecoder::DecoderInfo ConfigurableFrameSizeDecoder::GetDecoderInfo() const {
  return {.implementation_name = "ConfigurableFrameSizeDecoder",
          .is_hardware_accelerated = false};
}

}  // namespace test
}  // namespace webrtc
