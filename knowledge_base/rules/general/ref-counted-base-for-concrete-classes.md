---
name: webrtc/ref-counted-base-for-concrete-classes
description: When defining reference-counted classes, inherit standalone types from RefCountedBase or RefCountedNonVirtual, avoid manual AddRef/Release/RefCounter implementations, and instantiate RefCountInterface implementations with make_ref_counted
rubric: readability
severity: blocking
---

**Rule.** When defining reference-counted classes, avoid manually implementing
intrusive reference counting (such as embedding private webrtc_impl::RefCounter
/ RefCounter members, open-coding AddRef()/Release(), or calling delete this).
For standalone concrete classes without polymorphic interface requirements,
inherit from webrtc::RefCountedBase (or webrtc::RefCountedNonVirtual<T>) or wrap
concrete types in FinalRefCountedObject<T>. When implementing interfaces derived
from webrtc::RefCountInterface, do not override AddRef() and Release();
instantiate the class using webrtc::make_ref_counted<T>() or
RefCountedObject<T>.

**Why.** WebRTC provides standard reference-counting base classes and wrappers
in api/ref_count.h, api/make_ref_counted.h, and rtc_base/ref_counted_object.h.
Manually implementing AddRef, Release, and delete this with raw RefCounter
members introduces boilerplate, risks memory management bugs during destruction,
and bypasses optimizations. RefCountedBase and RefCountedNonVirtual (or
FinalRefCountedObject) provide efficient non-virtual ref-counting for standalone
classes, while webrtc::make_ref_counted<T> automatically generates standard
AddRef and Release overrides for classes derived from RefCountInterface without
virtual overhead where not needed.

## Examples

Bad:

```cpp
// Standalone concrete class with manual ref-counting
class RefCountedBuffer {
 public:
  void AddRef() { ref_count_.IncRef(); }
  void Release() {
    if (ref_count_.DecRef() == RefCountReleaseStatus::kDroppedLastRef) {
      delete this;
    }
  }
 private:
  webrtc_impl::RefCounter ref_count_;
};

// RefCountInterface implementation with manual AddRef/Release overrides
class Source : public webrtc::VideoTrackSourceInterface {
 public:
  void AddRef() const override { ref_count_.IncRef(); }
  RefCountReleaseStatus Release() const override { ... }
 private:
  mutable webrtc_impl::RefCounter ref_count_{0};
};
```

Good:

```cpp
// Standalone concrete class using RefCountedBase or FinalRefCountedObject
class Buffer final : public rtc::RefCountedBase { ... };
auto buffer = webrtc::make_ref_counted<Buffer>();

// RefCountInterface implementation omitting manual ref-count overrides
class Source : public webrtc::VideoTrackSourceInterface {
  // Omit manual AddRef/Release overrides and RefCounter member
};
auto source = webrtc::make_ref_counted<Source>();
```
