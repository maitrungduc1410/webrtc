/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/scoped_operations_batcher.h"

#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "rtc_base/checks.h"
#include "rtc_base/thread.h"

namespace webrtc {

ScopedOperationsBatcher::ScopedOperationsBatcher(Thread* worker_thread)
    : worker_thread_(worker_thread) {
  RTC_DCHECK(worker_thread_);
}

ScopedOperationsBatcher::~ScopedOperationsBatcher() {
  Run();
}

void ScopedOperationsBatcher::Run() {
  std::vector<absl::AnyInvocable<void() &&>> signaling_tasks;
  if (!tasks_.empty()) {
    worker_thread_->BlockingCall([&] {
      for (auto& task : tasks_) {
        if (task.void_task) {
          std::move(task.void_task)();
        } else {
          RTC_DCHECK(task.returning_task);
          auto ret = std::move(task.returning_task)();
          if (ret) {
            signaling_tasks.push_back(std::move(ret));
          }
        }
      }
    });
    tasks_.clear();
  }

  for (auto& task : signaling_tasks) {
    std::move(task)();
  }
}

void ScopedOperationsBatcher::push_back(absl::AnyInvocable<void() &&> task) {
  if (task) {
    tasks_.push_back({.void_task = std::move(task)});
  }
}

void ScopedOperationsBatcher::push_back(
    absl::AnyInvocable<absl::AnyInvocable<void() &&>() &&> task) {
  if (task) {
    tasks_.push_back({.returning_task = std::move(task)});
  }
}

}  // namespace webrtc
