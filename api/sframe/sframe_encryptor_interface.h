/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_SFRAME_SFRAME_ENCRYPTOR_INTERFACE_H_
#define API_SFRAME_SFRAME_ENCRYPTOR_INTERFACE_H_

#include <cstdint>
#include <span>

#include "absl/base/nullability.h"
#include "api/ref_count.h"
#include "api/rtc_error.h"
#include "api/sframe/sframe_types.h"
#include "rtc_base/checks.h"

namespace webrtc {

struct SframeEncryptorInit {
  SframeMode mode;
  SframeCipherSuite cipher_suite;
};

// Key management handle for Sframe sender encryption. Key updates are applied
// asynchronously, in the order they were requested, and their results are
// reported on the signaling thread. The `SframeEncryptorInterface` instance
// must be released on the signaling thread; no result is reported after that.
class SframeEncryptorInterface : public RefCountInterface {
 public:
  // TODO(bugs.webrtc.org/479862368): Make pure virtual once all implementations
  // are updated.
  virtual void SetEncryptionKey(uint64_t key_id,
                                std::span<const uint8_t> key_material,
                                absl_nonnull SframeKeyUpdateCallback callback) {
    RTC_DCHECK_NOTREACHED();
  }

  // Applies the key update right away, ahead of the key updates that are still
  // pending.
  // TODO(bugs.webrtc.org/479862368): Remove once Chromium uses the overload
  // that takes a callback.
  [[deprecated("Use the overload that takes a callback")]]
  virtual RTCError SetEncryptionKey(uint64_t key_id,
                                    std::span<const uint8_t> key_material) {
    RTC_DCHECK_NOTREACHED();
    return RTCError::InternalError();
  }

 protected:
  ~SframeEncryptorInterface() override = default;
};

}  // namespace webrtc

#endif  // API_SFRAME_SFRAME_ENCRYPTOR_INTERFACE_H_
