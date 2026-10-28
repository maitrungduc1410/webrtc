---
name: webrtc/delete-move-for-signal-and-callback-targets
description: When defining classes or callback containers registered with signals, observers, or C-style hooks, explicitly delete move operations instead of making them movable
rubric: safety
severity: blocking
---

**Rule.** Do not implement or default move constructors or move assignment
operators for classes, callback containers (CallbackList,
CallbackListReceivers, CallbackListWithLocks), signal/slot targets
(sigslot::has_slots, SignalTrampoline), or objects embedding external
C-style callback hooks (such as PipeWire spa_hook or OS listeners). Explicitly
delete move operations (`= delete`). When instances must be stored in containers
or transferred, manage them behind `std::unique_ptr` (or in node-based
containers like `std::deque`) to guarantee pointer stability.

**Why.** Signal dispatchers, observer lists, and external C callback APIs
register listeners and trampoline callbacks using raw object addresses (`this`)
or internal hook structs. Moving or relocating an instance in memory invalidates
these registered addresses without updating active subscriptions, leading to
dangling pointers, broken notifications, or use-after-free crashes when
callbacks fire.

## Examples

Bad:

```cpp
// Bad: Defaulting or implementing move operations for address-sensitive callback targets/dispatchers
class RTC_EXPORT CallbackListReceivers {
 public:
  CallbackListReceivers(CallbackListReceivers&&) = default;
  CallbackListReceivers& operator=(CallbackListReceivers&&) = default;
};

PipeWireNode& PipeWireNode::operator=(PipeWireNode&& other) noexcept {
  spa_hook_remove(&node_listener_);
  proxy_ = other.proxy_;
  node_listener_ = std::move(other.node_listener_);
  return *this;
}
```

Good:

```cpp
// Good: Explicitly delete move operations and use unique_ptr for pointer stability
class RTC_EXPORT CallbackListReceivers {
 public:
  CallbackListReceivers(CallbackListReceivers&&) = delete;
  CallbackListReceivers& operator=(CallbackListReceivers&&) = delete;
};

class PipeWireNode {
 public:
  using PipeWireNodePtr = std::unique_ptr<PipeWireNode, PipeWireNodeDeleter>;
  PipeWireNode(PipeWireNode&&) = delete;
  PipeWireNode& operator=(PipeWireNode&&) = delete;
};
std::deque<PipeWireNode::PipeWireNodePtr> nodes_;
```
