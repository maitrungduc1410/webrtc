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
#include "api/sframe/sframe_types.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "modules/sframe/sframe_media_encryptor_interface.h"

namespace webrtc {

// Implements the public key management API on top of an encryptor that is used
// on `encryptor_queue`. Results are reported on `callback_queue`.
// `callback_queue` and `encryptor_queue` must be non-null and outlive instances
// of this class, and `callback_queue` must also outlive tasks pending on
// `encryptor_queue`.
class SframeEncryptorHandle : public SframeEncryptorInterface {
 public:
  static absl_nonnull scoped_refptr<SframeEncryptorHandle> Create(
      TaskQueueBase* absl_nonnull callback_queue,
      TaskQueueBase* absl_nonnull encryptor_queue,
      absl_nonnull scoped_refptr<SframeMediaEncryptorInterface> encryptor);

  // Must be destroyed on `callback_queue`. Key updates that are still pending
  // are applied, but their callbacks are dropped.
  ~SframeEncryptorHandle() override;

  // SframeEncryptorInterface implementation.
  void SetEncryptionKey(uint64_t key_id,
                        std::span<const uint8_t> key_material,
                        absl_nonnull SframeKeyUpdateCallback callback) override;
  // Calls `encryptor` on the calling thread, so `encryptor` must be
  // thread-safe.
  RTCError SetEncryptionKey(uint64_t key_id,
                            std::span<const uint8_t> key_material) override;

 protected:
  SframeEncryptorHandle(
      TaskQueueBase* absl_nonnull callback_queue,
      TaskQueueBase* absl_nonnull encryptor_queue,
      absl_nonnull scoped_refptr<SframeMediaEncryptorInterface> encryptor);

 private:
  TaskQueueBase& callback_queue_;
  TaskQueueBase& encryptor_queue_;

  // Alive until the handle is destroyed; no callback runs after that.
  const absl_nonnull scoped_refptr<PendingTaskSafetyFlag> callback_safety_;

  const absl_nonnull scoped_refptr<SframeMediaEncryptorInterface> encryptor_;
};

}  // namespace webrtc

#endif  // MODULES_SFRAME_SFRAME_ENCRYPTOR_HANDLE_H_
