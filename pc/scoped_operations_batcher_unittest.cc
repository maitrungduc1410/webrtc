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

#include <memory>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "rtc_base/thread.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(ScopedOperationsBatcherTest, ExecutesTasksOnWorkerThread) {
  auto worker_thread = Thread::Create();
  worker_thread->Start();

  bool task_executed = false;
  bool worker_checked = false;

  {
    ScopedOperationsBatcher batcher(worker_thread.get());
    batcher.push_back([&] {
      task_executed = true;
      worker_checked = worker_thread->IsCurrent();
    });
  }

  EXPECT_TRUE(task_executed);
  EXPECT_TRUE(worker_checked);
}

TEST(ScopedOperationsBatcherTest, ExecutesReturnedTasksOnCallingThread) {
  // Use current thread as the calling (signaling) thread. This test runs
  // without an explicit RunLoop, because the returned tasks are executed
  // synchronously within the caller's context of `Run()` or
  // `~ScopedOperationsBatcher()`.
  auto signaling_thread = Thread::Current();

  auto worker_thread = Thread::Create();
  worker_thread->Start();

  bool return_task_executed = false;
  Thread* return_task_thread = nullptr;
  bool task_executed = false;
  Thread* task_thread = nullptr;

  {
    ScopedOperationsBatcher batcher(worker_thread.get());
    absl::AnyInvocable<absl::AnyInvocable<void() &&>() &&> task =
        [&]() -> absl::AnyInvocable<void() &&> {
      task_executed = true;
      task_thread = Thread::Current();
      return [&]() {
        return_task_executed = true;
        return_task_thread = Thread::Current();
      };
    };
    batcher.push_back(std::move(task));
  }

  EXPECT_TRUE(task_executed);
  EXPECT_EQ(task_thread, worker_thread.get());
  EXPECT_TRUE(return_task_executed);
  EXPECT_EQ(return_task_thread, signaling_thread);
}

}  // namespace
}  // namespace webrtc
