/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_SFRAME_VIDEO_RECEIVE_DELEGATE_H_
#define VIDEO_SFRAME_VIDEO_RECEIVE_DELEGATE_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/base/nullability.h"
#include "api/scoped_refptr.h"
#include "api/video/video_codec_type.h"
#include "modules/rtp_rtcp/source/frame_object.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/sframe/sframe_decrypt_error_reporter.h"
#include "modules/sframe/sframe_decryption_config.h"
#include "modules/sframe/sframe_media_decryptor_interface.h"

namespace webrtc {

// Decrypts the Sframe media of one video receive stream and reports the
// failures to the application. Which of the two entry points applies is decided
// by the Sframe wire mode: per-packet mode (T=1) decrypts each packet before
// depacketization, per-frame mode (T=0) decrypts the frame once it is
// reassembled.
//
// The decryptor and the error reporter in `config` hold all the crypto and
// reporting state, and are shared with the other receive streams of the same
// receiver.
//
// All methods must be called on the sequence that receives the media.
class SframeVideoReceiveDelegate {
 public:
  // Describes the frame that the media belongs to. In per-packet mode, all
  // packets of a frame are decrypted with the same FrameInfo.
  struct FrameInfo {
    VideoCodecType codec = kVideoCodecGeneric;
    // True if the media is part of a key frame. False when it is not known.
    bool is_key_frame = false;
  };

  SframeVideoReceiveDelegate(SframeDecryptionConfig config, uint32_t ssrc);

  // Returns `packet` with its payload replaced by the plaintext, or nullptr if
  // it did not decrypt, in which case the packet is dropped. `frame_info` is
  // only used to report failures.
  absl_nullable std::unique_ptr<RtpPacketReceived> DecryptPacket(
      absl_nonnull std::unique_ptr<RtpPacketReceived> packet,
      const FrameInfo& frame_info);

  // Returns `frame` with its bitstream replaced by the plaintext, or nullptr if
  // it did not decrypt, in which case the frame is dropped.
  absl_nullable std::unique_ptr<RtpFrameObject> DecryptFrame(
      absl_nonnull std::unique_ptr<RtpFrameObject> frame);

 private:
  const absl_nonnull scoped_refptr<SframeMediaDecryptorInterface> decryptor_;
  const absl_nonnull scoped_refptr<SframeDecryptErrorReporter> error_reporter_;
  const uint32_t ssrc_;
  // Scratch buffer reused to avoid allocating per packet.
  std::vector<uint8_t> packet_plaintext_;
};

}  // namespace webrtc

#endif  // VIDEO_SFRAME_VIDEO_RECEIVE_DELEGATE_H_
