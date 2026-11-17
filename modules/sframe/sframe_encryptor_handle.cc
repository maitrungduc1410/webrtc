/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/sframe/sframe_encryptor_handle.h"

#include <cstdint>
#include <span>
#include <utility>

#include "absl/base/nullability.h"
#include "api/make_ref_counted.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "modules/sframe/sframe_media_encryptor_interface.h"
#include "rtc_base/checks.h"

namespace webrtc {

scoped_refptr<SframeEncryptorHandle> SframeEncryptorHandle::Create(
    absl_nonnull scoped_refptr<SframeMediaEncryptorInterface> encryptor) {
  return make_ref_counted<SframeEncryptorHandle>(std::move(encryptor));
}

SframeEncryptorHandle::SframeEncryptorHandle(
    absl_nonnull scoped_refptr<SframeMediaEncryptorInterface> encryptor)
    : encryptor_(std::move(encryptor)) {
  RTC_DCHECK(encryptor_);
}

SframeEncryptorHandle::~SframeEncryptorHandle() = default;

RTCError SframeEncryptorHandle::SetEncryptionKey(
    uint64_t key_id,
    std::span<const uint8_t> key_material) {
  return encryptor_->SetEncryptionKey(key_id, key_material);
}

}  // namespace webrtc
