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
#include <memory>
#include <span>
#include <utility>
#include <variant>
#include <vector>

#include "absl/base/nullability.h"
#include "api/scoped_refptr.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/sframe/sframe_decrypt_error_reporter.h"
#include "modules/sframe/sframe_decryption_config.h"
#include "modules/sframe/sframe_media_decryptor_interface.h"

namespace webrtc {

SframeVideoReceiveDelegate::SframeVideoReceiveDelegate(
    SframeDecryptionConfig config)
    : decryptor_(std::move(config.decryptor)),
      error_reporter_(std::move(config.error_reporter)) {}

absl_nullable std::unique_ptr<RtpPacketReceived>
SframeVideoReceiveDelegate::DecryptPacket(
    absl_nonnull std::unique_ptr<RtpPacketReceived> packet) {
  packet_plaintext_.resize(
      decryptor_->GetMaxPlaintextByteSize(packet->payload_size()));

  SframeDecryptResult result = decryptor_->Decrypt(
      packet->payload(), /*additional_data=*/{}, packet_plaintext_);
  if (const SframeDecryptFailure* failure =
          std::get_if<SframeDecryptFailure>(&result);
      failure != nullptr) {
    error_reporter_->ReportError(
        {.error_type = failure->type, .key_id = failure->key_id});
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
