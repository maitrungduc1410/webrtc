/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_SFRAME_SFRAME_ENCRYPTOR_HANDLE_H_
#define MODULES_SFRAME_SFRAME_ENCRYPTOR_HANDLE_H_

#include <cstdint>
#include <span>

#include "absl/base/nullability.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/sframe/sframe_encryptor_interface.h"
#include "modules/sframe/sframe_media_encryptor_interface.h"

namespace webrtc {

// Implements the public key management API on top of an encryptor that is
// called on the calling thread, so `encryptor` must be thread-safe.
class SframeEncryptorHandle : public SframeEncryptorInterface {
 public:
  static absl_nonnull scoped_refptr<SframeEncryptorHandle> Create(
      absl_nonnull scoped_refptr<SframeMediaEncryptorInterface> encryptor);

  ~SframeEncryptorHandle() override;

  // SframeEncryptorInterface implementation.
  RTCError SetEncryptionKey(uint64_t key_id,
                            std::span<const uint8_t> key_material) override;

 protected:
  explicit SframeEncryptorHandle(
      absl_nonnull scoped_refptr<SframeMediaEncryptorInterface> encryptor);

 private:
  const absl_nonnull scoped_refptr<SframeMediaEncryptorInterface> encryptor_;
};

}  // namespace webrtc

#endif  // MODULES_SFRAME_SFRAME_ENCRYPTOR_HANDLE_H_
