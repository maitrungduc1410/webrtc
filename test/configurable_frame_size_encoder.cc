/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/configurable_frame_size_encoder.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "api/environment/environment.h"
#include "api/fec_controller_override.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_image.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_type.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/byte_order.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace test {
namespace {

constexpr uint32_t kMagic = 0x43465346;  // "CFSF"
constexpr uint8_t kKeyFrameFlag = 0x01;
constexpr size_t kFlagsOffset = 4;
constexpr size_t kFrameIdOffset = 8;
constexpr size_t kCaptureTimeOffset = 16;

uint8_t FillerByte(uint64_t frame_id) {
  return static_cast<uint8_t>(frame_id * 7 + 1);
}

std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateDeliveryQueue(
    const Environment& env) {
  return env.task_queue_factory().CreateTaskQueue(
      "ConfigurableFrameSizeEncoder", TaskQueueFactory::Priority::kNormal);
}

}  // namespace

void ConfigurableFrameSizeEncoder::WriteFrame(const FrameHeader& header,
                                              std::span<uint8_t> payload) {
  RTC_CHECK_GE(payload.size(), FrameHeader::kSize);
  SetBE32(payload, kMagic);
  std::fill(payload.begin() + kFlagsOffset, payload.begin() + kFrameIdOffset,
            0);
  payload[kFlagsOffset] = header.is_key_frame ? kKeyFrameFlag : 0;
  SetBE64(payload.subspan(kFrameIdOffset), header.frame_id);
  SetBE64(payload.subspan(kCaptureTimeOffset),
          static_cast<uint64_t>(header.capture_time.us()));
  std::fill(payload.begin() + FrameHeader::kSize, payload.end(),
            FillerByte(header.frame_id));
}

std::optional<ConfigurableFrameSizeEncoder::FrameHeader>
ConfigurableFrameSizeEncoder::ParseFrame(std::span<const uint8_t> payload) {
  if (payload.size() < FrameHeader::kSize || GetBE32(payload) != kMagic ||
      (payload[kFlagsOffset] & ~kKeyFrameFlag) != 0) {
    return std::nullopt;
  }
  const int64_t capture_time_us =
      static_cast<int64_t>(GetBE64(payload.subspan(kCaptureTimeOffset)));
  if (capture_time_us < 0) {
    return std::nullopt;
  }
  FrameHeader header;
  header.frame_id = GetBE64(payload.subspan(kFrameIdOffset));
  header.capture_time = Timestamp::Micros(capture_time_us);
  header.is_key_frame = (payload[kFlagsOffset] & kKeyFrameFlag) != 0;
  const uint8_t filler = FillerByte(header.frame_id);
  if (!std::all_of(payload.begin() + FrameHeader::kSize, payload.end(),
                   [filler](uint8_t b) { return b == filler; })) {
    return std::nullopt;
  }
  return header;
}

ConfigurableFrameSizeEncoder::ConfigurableFrameSizeEncoder(
    size_t max_frame_size)
    : ConfigurableFrameSizeEncoder(std::nullopt,
                                   Config{.frame_size = max_frame_size}) {}

ConfigurableFrameSizeEncoder::ConfigurableFrameSizeEncoder(
    const Environment& env,
    const Config& config)
    : ConfigurableFrameSizeEncoder(std::optional<Environment>(env), config) {}

ConfigurableFrameSizeEncoder::ConfigurableFrameSizeEncoder(
    std::optional<Environment> env,
    const Config& config)
    : env_(std::move(env)),
      config_(config),
      current_frame_size_(config.frame_size) {
  // Delayed delivery needs a task queue factory and a clock.
  RTC_CHECK(env_.has_value() || !config_.encode_delay.has_value());
  RTC_CHECK_GT(config_.max_frames_in_flight, 0);
  RTC_CHECK(!config_.encode_delay.has_value() ||
            *config_.encode_delay > TimeDelta::Zero());
}

ConfigurableFrameSizeEncoder::~ConfigurableFrameSizeEncoder() {
  delivery_queue_ = nullptr;
}

void ConfigurableFrameSizeEncoder::SetFecControllerOverride(
    FecControllerOverride* fec_controller_override) {
  // Ignored.
}

int32_t ConfigurableFrameSizeEncoder::InitEncode(
    const VideoCodec* codec_settings,
    const Settings& settings) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (codec_settings != nullptr) {
    max_framerate_ = codec_settings->maxFramerate;
    framerate_fps_ = max_framerate_;
    target_bitrate_ = DataRate::KilobitsPerSec(codec_settings->startBitrate);
  }
  key_frame_pending_ = true;
  if (config_.encode_delay.has_value() && delivery_queue_ == nullptr) {
    delivery_queue_ = CreateDeliveryQueue(*env_);
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t ConfigurableFrameSizeEncoder::Encode(
    const VideoFrame& inputImage,
    const std::vector<VideoFrameType>* frame_types) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (callback_ == nullptr ||
      (config_.encode_delay.has_value() && delivery_queue_ == nullptr)) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (frame_types != nullptr &&
      absl::c_linear_search(*frame_types, VideoFrameType::kVideoFrameKey)) {
    key_frame_pending_ = true;
  }
  if ((config_.frame_size_from_target_bitrate && target_bitrate_.IsZero()) ||
      !TryAddFrameInFlight()) {
    ReportDroppedFrame(inputImage.rtp_timestamp());
    return WEBRTC_VIDEO_CODEC_OK;
  }

  const bool is_key_frame =
      !config_.key_frames_on_request_only || key_frame_pending_;
  key_frame_pending_ = false;
  Deliver(CreateEncodedImage(inputImage, is_key_frame));
  if (post_encode_callback_) {
    (*post_encode_callback_)();
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

EncodedImage ConfigurableFrameSizeEncoder::CreateEncodedImage(
    const VideoFrame& frame,
    bool is_key_frame) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  scoped_refptr<EncodedImageBuffer> buffer =
      EncodedImageBuffer::Create(NextFrameSize(is_key_frame));
  std::span<uint8_t> payload(buffer->data(), buffer->size());
  if (config_.write_frame_header) {
    WriteFrame({.frame_id = next_frame_id_++,
                .capture_time = Timestamp::Micros(frame.timestamp_us()),
                .is_key_frame = is_key_frame},
               payload);
  } else {
    std::fill(payload.begin(), payload.end(), 0);
  }

  EncodedImage image;
  image.SetEncodedData(buffer);
  image._encodedHeight = frame.height();
  image._encodedWidth = frame.width();
  image.set_frame_type(is_key_frame ? VideoFrameType::kVideoFrameKey
                                    : VideoFrameType::kVideoFrameDelta);
  image.SetRtpTimestamp(frame.rtp_timestamp());
  image.capture_time_ms_ = frame.render_time_ms();
  image.ntp_time_ms_ = frame.ntp_time_ms();
  image.SetPlayoutDelay(config_.playout_delay);
  image.set_end_of_temporal_unit(true);
  return image;
}

size_t ConfigurableFrameSizeEncoder::NextFrameSize(bool is_key_frame) const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  size_t size = current_frame_size_;
  if (config_.frame_size_from_target_bitrate) {
    const double framerate_fps = std::max(framerate_fps_, 1.0);
    double size_bytes = target_bitrate_.bps<double>() / (8 * framerate_fps);
    if (is_key_frame) {
      size_bytes *= config_.key_frame_size_factor;
    }
    size = static_cast<size_t>(size_bytes);
  }
  if (config_.write_frame_header) {
    size = std::max(size, FrameHeader::kSize);
  }
  return size;
}

bool ConfigurableFrameSizeEncoder::TryAddFrameInFlight() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (!config_.encode_delay.has_value()) {
    return true;
  }
  const Timestamp now = env_->clock().CurrentTime();
  while (!frames_in_flight_.empty() && frames_in_flight_.front() <= now) {
    frames_in_flight_.pop_front();
  }
  if (frames_in_flight_.size() >=
      static_cast<size_t>(config_.max_frames_in_flight)) {
    return false;
  }
  frames_in_flight_.push_back(now + *config_.encode_delay);
  return true;
}

void ConfigurableFrameSizeEncoder::Deliver(EncodedImage image) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (!config_.encode_delay.has_value()) {
    CodecSpecificInfo codec_specific;
    codec_specific.codecType = codec_type_;
    callback_->OnEncodedImage(image, &codec_specific);
    return;
  }
  delivery_queue_->PostDelayedHighPrecisionTask(
      [callback = callback_, codec_type = codec_type_,
       image = std::move(image)] {
        CodecSpecificInfo codec_specific;
        codec_specific.codecType = codec_type;
        callback->OnEncodedImage(image, &codec_specific);
      },
      *config_.encode_delay);
}

void ConfigurableFrameSizeEncoder::ReportDroppedFrame(uint32_t rtp_timestamp) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (!config_.encode_delay.has_value()) {
    callback_->OnFrameDropped(rtp_timestamp, /*spatial_id=*/0,
                              /*is_end_of_temporal_unit=*/true);
    return;
  }
  // Reported via the delivery queue so that the callback sees the frames in
  // the order they were passed to Encode().
  delivery_queue_->PostDelayedHighPrecisionTask(
      [callback = callback_, rtp_timestamp] {
        callback->OnFrameDropped(rtp_timestamp, /*spatial_id=*/0,
                                 /*is_end_of_temporal_unit=*/true);
      },
      *config_.encode_delay);
}

int32_t ConfigurableFrameSizeEncoder::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (callback != callback_ && delivery_queue_ != nullptr) {
    // Pending deliveries use the previous callback. Replacing the queue blocks
    // until a running delivery has completed and drops pending ones.
    delivery_queue_ = CreateDeliveryQueue(*env_);
    frames_in_flight_.clear();
  }
  callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t ConfigurableFrameSizeEncoder::Release() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  // Blocks until a running delivery has completed and drops pending ones.
  delivery_queue_ = nullptr;
  frames_in_flight_.clear();
  key_frame_pending_ = true;
  return WEBRTC_VIDEO_CODEC_OK;
}

void ConfigurableFrameSizeEncoder::SetRates(
    const RateControlParameters& parameters) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  target_bitrate_ = DataRate::BitsPerSec(parameters.bitrate.get_sum_bps());
  framerate_fps_ =
      parameters.framerate_fps > 0 ? parameters.framerate_fps : max_framerate_;
}

VideoEncoder::EncoderInfo ConfigurableFrameSizeEncoder::GetEncoderInfo() const {
  EncoderInfo info;
  info.implementation_name = "ConfigurableFrameSizeEncoder";
  // Frame sizes that follow the target bitrate exactly make the frame dropper
  // in the media optimization module unnecessary.
  info.has_trusted_rate_controller = config_.frame_size_from_target_bitrate;
  info.is_hardware_accelerated = config_.is_hardware_accelerated;
  return info;
}

int32_t ConfigurableFrameSizeEncoder::SetFrameSize(size_t size) {
  current_frame_size_ = size;
  return WEBRTC_VIDEO_CODEC_OK;
}

void ConfigurableFrameSizeEncoder::SetCodecType(VideoCodecType codec_type) {
  codec_type_ = codec_type;
}

void ConfigurableFrameSizeEncoder::RegisterPostEncodeCallback(
    std::function<void(void)> post_encode_callback) {
  post_encode_callback_ = std::move(post_encode_callback);
}

}  // namespace test
}  // namespace webrtc
