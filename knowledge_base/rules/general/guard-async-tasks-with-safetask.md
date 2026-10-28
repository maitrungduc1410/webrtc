---
name: webrtc/guard-async-tasks-with-safetask
description: When posting asynchronous tasks across threads or queues, guard closures with SafeTask and ScopedTaskSafety instead of capturing raw this, scoped_refptr, or WeakPtr
rubric: safety
severity: blocking
---

**Rule.** When posting asynchronous tasks (via PostTask, PostDelayedTask, or
RequestCallOnNextTick) or registering cross-thread/OS callbacks, wrap the
callable in webrtc::SafeTask (or SafeInvocable) using webrtc::ScopedTaskSafety
or PendingTaskSafetyFlag.
1. Do not capture raw `this` or instance references across task queues or threads.
2. Do not capture a `scoped_refptr<T>` to `this` solely to prevent use-after-free, as ref-counted task captures artificially prolong object lifetime past teardown and deliver stale/zombie callbacks to downstream observers.
3. Do not dereference or validate `rtc::WeakPtr` instances on arbitrary background threads or off-sequence callbacks (WeakPtr is sequence-bound). Post tasks back to the owning sequence or pass thread-safe synchronization primitives (such as `scoped_refptr<RefCountedEvent>`).
4. In destructor cleanup tasks posted to background queues, never capture `this`; move owned resources directly into the capture closure.

**Why.** Asynchronous tasks executing on background threads or task queues can
run after the originating object has started destruction or on an arbitrary
thread. Capturing raw `this` causes use-after-free crashes; capturing
`scoped_refptr<T>(this)` keeps objects alive past their intended lifecycle and
fires late notifications during teardown; and dereferencing `rtc::WeakPtr`
off-sequence violates thread checks and causes data races. `webrtc::SafeTask`
automatically checks flag validity (`alive()`) before invocation on the target
queue and drops the task if the originating object was destroyed or invalidated,
ensuring safe cancellation without prolonging object lifetime.

## Examples

Bad:

```cpp
// Anti-pattern 1: Raw `this` captured across threads/queues or metronomes
network_thread()->PostTask([this] { ProcessPacket(); });
metronome_->RequestCallOnNextTick([this] { EncodeFrame(); });

// Anti-pattern 2: Capturing scoped_refptr<this> prolongs object lifetime past teardown
signaling_thread_->PostTask(
    [this_ref = scoped_refptr<AndroidVideoTrackSource>(this)] {
      this_ref->FireOnChanged();
    });

// Anti-pattern 3: Dereferencing WeakPtr on an arbitrary OS worker thread or queue
void Worker::OnEvent(WeakPtr<Session> session) {
  if (session) session->Process();  // Race: WeakPtr dereferenced off-sequence
}

// Anti-pattern 4: Destructor cleanup task capturing dangling `this`
FrameCadenceAdapterImpl::~FrameCadenceAdapterImpl() {
  worker_queue_->PostTask([this, cleanup = std::move(cleanup)] {
    RTC_DCHECK_RUN_ON(worker_queue_);
  });
}
```

Good:

```cpp
// Guard async tasks and metronome callbacks with SafeTask and ScopedTaskSafety:
network_thread()->PostTask(
    SafeTask(safety_.flag(), [this] { ProcessPacket(); }));
metronome_->RequestCallOnNextTick(
    SafeTask(safety_.flag(), [this] { EncodeFrame(); }));

// Use SafeTask / PendingTaskSafetyFlag instead of refptr to cancel on teardown:
signaling_thread_->PostTask(
    SafeTask(safety_.flag(), [this] { FireOnChanged(); }));

// Decouple OS callbacks via thread-safe scoped_refptr or post back to sequence:
task_queue_->PostTask([session = weak_factory_.GetWeakPtr()] {
  if (session) session->Process();
});

// In destructors, offload member destruction without capturing `this`:
FrameCadenceAdapterImpl::~FrameCadenceAdapterImpl() {
  worker_queue_->PostTask([cleanup = std::move(cleanup)] {});
}
```
