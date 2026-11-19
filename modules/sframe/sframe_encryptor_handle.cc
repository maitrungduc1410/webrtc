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
#include "api/sequence_checker.h"
#include "api/sframe/sframe_types.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "modules/sframe/sframe_media_encryptor_interface.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"

namespace webrtc {

scoped_refptr<SframeEncryptorHandle> SframeEncryptorHandle::Create(
    TaskQueueBase* absl_nonnull callback_queue,
    TaskQueueBase* absl_nonnull encryptor_queue,
    absl_nonnull scoped_refptr<SframeMediaEncryptorInterface> encryptor) {
  return make_ref_counted<SframeEncryptorHandle>(
      callback_queue, encryptor_queue, std::move(encryptor));
}

SframeEncryptorHandle::SframeEncryptorHandle(
    TaskQueueBase* absl_nonnull callback_queue,
    TaskQueueBase* absl_nonnull encryptor_queue,
    absl_nonnull scoped_refptr<SframeMediaEncryptorInterface> encryptor)
    : callback_queue_(*callback_queue),
      encryptor_queue_(*encryptor_queue),
      callback_safety_(
          PendingTaskSafetyFlag::CreateAttachedToTaskQueue(/*alive=*/true,
                                                           callback_queue)),
      encryptor_(std::move(encryptor)) {
  RTC_DCHECK(encryptor_);
}

SframeEncryptorHandle::~SframeEncryptorHandle() {
  RTC_DCHECK_RUN_ON(&callback_queue_);
  callback_safety_->SetNotAlive();
}

void SframeEncryptorHandle::SetEncryptionKey(
    uint64_t key_id,
    std::span<const uint8_t> key_material,
    absl_nonnull SframeKeyUpdateCallback callback) {
  RTC_DCHECK(callback);
  // `key_material` may not outlive this call, so the task keeps its own copy,
  // which is wiped when the task is destroyed. Always post, even when already
  // on `encryptor_queue_`, so that updates are applied in the order they were
  // requested. The task holds its own reference to the encryptor, so the handle
  // may go away in the meantime.
  ZeroOnFreeBuffer<uint8_t> key(key_material.data(), key_material.size());
  scoped_refptr<SframeMediaEncryptorInterface> encryptor = encryptor_;
  TaskQueueBase* callback_queue = &callback_queue_;
  scoped_refptr<PendingTaskSafetyFlag> callback_safety = callback_safety_;

  encryptor_queue_.PostTask([encryptor, callback_queue, callback_safety, key_id,
                             key = std::move(key),
                             callback = std::move(callback)]() mutable {
    RTCError error = encryptor->SetEncryptionKey(key_id, key);

    callback_queue->PostTask(SafeTask(
        std::move(callback_safety),
        [callback = std::move(callback), error = std::move(error)]() mutable {
          std::move(callback)(std::move(error));
        }));
  });
}

RTCError SframeEncryptorHandle::SetEncryptionKey(
    uint64_t key_id,
    std::span<const uint8_t> key_material) {
  return encryptor_->SetEncryptionKey(key_id, key_material);
}

}  // namespace webrtc
