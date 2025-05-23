/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/task_queue/pending_task_safety_flag.h"

#include "absl/base/nullability.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_base.h"
#include "rtc_base/checks.h"

namespace webrtc {

// static
scoped_refptr<PendingTaskSafetyFlag> PendingTaskSafetyFlag::CreateInternal(
    bool alive) {
  // Explicit new, to access private constructor.
  return scoped_refptr<PendingTaskSafetyFlag>(new PendingTaskSafetyFlag(alive));
}

// static
scoped_refptr<PendingTaskSafetyFlag> PendingTaskSafetyFlag::Create() {
  return CreateInternal(true);
}

scoped_refptr<PendingTaskSafetyFlag> PendingTaskSafetyFlag::CreateDetached() {
  scoped_refptr<PendingTaskSafetyFlag> safety_flag = CreateInternal(true);
  safety_flag->main_sequence_.Detach();
  return safety_flag;
}

// Creates a flag, but with its SequenceChecker explicitly initialized for
// a given task queue and the `alive()` flag specified.
scoped_refptr<PendingTaskSafetyFlag>
PendingTaskSafetyFlag::CreateAttachedToTaskQueue(bool alive,
                                                 TaskQueueBase* absl_nonnull
                                                     attached_queue) {
  RTC_DCHECK(attached_queue) << "Null TaskQueue provided";
  return scoped_refptr<PendingTaskSafetyFlag>(
      new PendingTaskSafetyFlag(alive, attached_queue));
}

scoped_refptr<PendingTaskSafetyFlag>
PendingTaskSafetyFlag::CreateDetachedInactive() {
  scoped_refptr<PendingTaskSafetyFlag> safety_flag = CreateInternal(false);
  safety_flag->main_sequence_.Detach();
  return safety_flag;
}

void PendingTaskSafetyFlag::SetNotAlive() {
  RTC_DCHECK_RUN_ON(&main_sequence_);
  alive_ = false;
}

void PendingTaskSafetyFlag::SetAlive() {
  RTC_DCHECK_RUN_ON(&main_sequence_);
  alive_ = true;
}

bool PendingTaskSafetyFlag::alive() const {
  RTC_DCHECK_RUN_ON(&main_sequence_);
  return alive_;
}

}  // namespace webrtc
