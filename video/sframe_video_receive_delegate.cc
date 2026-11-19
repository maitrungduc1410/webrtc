/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/sframe_video_receive_delegate.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/base/nullability.h"
#include "api/frame_transformer_interface.h"
#include "api/payload_type.h"
#include "api/rtp_headers.h"
#include "api/scoped_refptr.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_content_type.h"
#include "api/video/video_frame_metadata.h"
#include "api/video/video_frame_type.h"
#include "api/video/video_rotation.h"
#include "api/video_codecs/video_codec.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/sframe/sframe_decrypt_error_reporter.h"
#include "modules/sframe/sframe_decryption_config.h"
#include "modules/sframe/sframe_media_decryptor_interface.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/strings/string_builder.h"
#include "system_wrappers/include/ntp_time.h"

namespace webrtc {

// Snapshot of the video media that failed to decrypt, reported to the
// application together with the failure. It owns a copy of the ciphertext and
// is detached from the receive pipeline, which drops the media.
class SframeEncryptedVideoFrame : public TransformableVideoFrameInterface {
 public:
  // Reported before depacketization, so what the codec payload descriptor
  // would carry - resolution, dependencies - is still unknown and stays at its
  // default. Only the RTP header and `frame_info` are available.
  SframeEncryptedVideoFrame(
      const RtpPacketReceived& packet,
      const SframeVideoReceiveDelegate::FrameInfo& frame_info,
      uint32_t ssrc)
      : TransformableVideoFrameInterface(Passkey()),
        data_(packet.PayloadBuffer()),
        payload_type_(packet.PayloadType()),
        codec_(frame_info.codec),
        rtp_timestamp_(packet.Timestamp()),
        is_key_frame_(frame_info.is_key_frame),
        receive_time_(packet.arrival_time()),
        absolute_capture_time_(
            packet.GetExtension<AbsoluteCaptureTimeExtension>()) {
    metadata_.SetSsrc(ssrc);
    metadata_.SetCsrcs(packet.Csrcs());
    metadata_.SetCodec(frame_info.codec);

    if (frame_info.is_key_frame) {
      metadata_.SetFrameType(VideoFrameType::kVideoFrameKey);
    }

    VideoRotation rotation;
    if (packet.GetExtension<VideoOrientation>(&rotation)) {
      metadata_.SetRotation(rotation);
    }

    VideoContentType content_type;
    if (packet.GetExtension<VideoContentTypeExtension>(&content_type)) {
      metadata_.SetContentType(content_type);
    }
  }

  ~SframeEncryptedVideoFrame() override = default;

  std::span<const uint8_t> GetData() const override { return data_; }
  void SetData(std::span<const uint8_t> data) override { data_.Set(data); }

  uint8_t GetPayloadType() const override {
    return static_cast<uint8_t>(payload_type_);
  }
  uint32_t GetSsrc() const override { return metadata_.GetSsrc(); }
  uint32_t GetTimestamp() const override { return rtp_timestamp_; }
  RtpTimestampInfo GetRtpTimestampInfo() const override {
    return RtpTimestampWithOffset(rtp_timestamp_);
  }
  void SetRTPTimestamp(uint32_t rtp_timestamp) override {
    rtp_timestamp_ = rtp_timestamp;
  }

  bool IsKeyFrame() const override { return is_key_frame_; }

  VideoFrameMetadata Metadata() const override { return metadata_; }
  void SetMetadata(const VideoFrameMetadata& metadata) override {
    metadata_ = metadata;
  }

  Direction GetDirection() const override { return Direction::kReceiver; }
  std::string GetMimeType() const override {
    return (StringBuilder() << "video/" << CodecTypeToPayloadString(codec_))
        .Release();
  }

  std::optional<Timestamp> ReceiveTime() const override {
    return receive_time_;
  }

  std::optional<Timestamp> CaptureTime() const override {
    if (!absolute_capture_time_.has_value() ||
        absolute_capture_time_->absolute_capture_timestamp == 0) {
      return std::nullopt;
    }

    return Timestamp::Micros(
        UQ32x32ToInt64Us(absolute_capture_time_->absolute_capture_timestamp));
  }

  std::optional<TimeDelta> SenderCaptureTimeOffset() const override {
    if (!absolute_capture_time_.has_value() ||
        !absolute_capture_time_->estimated_capture_clock_offset.has_value()) {
      return std::nullopt;
    }

    return TimeDelta::Micros(Q32x32ToInt64Us(
        *absolute_capture_time_->estimated_capture_clock_offset));
  }

 private:
  CopyOnWriteBuffer data_;
  VideoFrameMetadata metadata_;
  const PayloadType payload_type_;
  const VideoCodecType codec_;
  uint32_t rtp_timestamp_;
  const bool is_key_frame_;
  const std::optional<Timestamp> receive_time_;
  const std::optional<AbsoluteCaptureTime> absolute_capture_time_;
};

SframeVideoReceiveDelegate::SframeVideoReceiveDelegate(
    SframeDecryptionConfig config,
    uint32_t ssrc)
    : decryptor_(std::move(config.decryptor)),
      error_reporter_(std::move(config.error_reporter)),
      ssrc_(ssrc) {}

absl_nullable std::unique_ptr<RtpPacketReceived>
SframeVideoReceiveDelegate::DecryptPacket(
    absl_nonnull std::unique_ptr<RtpPacketReceived> packet,
    const FrameInfo& frame_info) {
  packet_plaintext_.resize(
      decryptor_->GetMaxPlaintextByteSize(packet->payload_size()));

  SframeDecryptResult result = decryptor_->Decrypt(
      packet->payload(), /*additional_data=*/{}, packet_plaintext_);
  if (const SframeDecryptFailure* failure =
          std::get_if<SframeDecryptFailure>(&result);
      failure != nullptr) {
    error_reporter_->ReportError(
        {.error_type = failure->type,
         .key_id = failure->key_id,
         .frame = std::make_unique<SframeEncryptedVideoFrame>(
             *packet, frame_info, ssrc_)});
    return nullptr;
  }

  const size_t plaintext_size =
      std::get<SframeDecryptSuccess>(result).bytes_written;
  // Bandwidth estimation has already accounted for the padding.
  packet->SetPadding(0);
  packet->SetPayload(std::span(packet_plaintext_).first(plaintext_size));
  error_reporter_->ReportSuccess();
  return packet;
}

}  // namespace webrtc
