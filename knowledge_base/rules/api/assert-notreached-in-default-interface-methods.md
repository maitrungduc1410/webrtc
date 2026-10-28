---
name: webrtc/api/assert-notreached-in-default-interface-methods
description: When providing default implementations for virtual methods in public interface base classes under api/ or *_interface.h, assert RTC_CHECK_NOTREACHED() or RTC_DCHECK_NOTREACHED() instead of empty bodies or silent dummy returns
rubric: correctness
severity: should_fix
---

**Rule.** When providing default implementations for virtual methods (such as
newly added methods or setters) in public interface base classes (under api/ or
*_interface.h), assert `RTC_CHECK_NOTREACHED()` or `RTC_DCHECK_NOTREACHED()`.
Do not leave empty no-op bodies or return silent dummy values (such as
`return false;`) that cannot fulfill the method's contract. If the method
returns a non-void value and debug assertions are non-fatal, return an
appropriate fallback value or use `RTC_CHECK_NOTREACHED()` to terminate
execution.

**Why.** Empty default implementations or dummy return values silently swallow
invocations and mask bugs when an unimplemented or unsupported method is called
at runtime. Asserting `RTC_CHECK_NOTREACHED()` or `RTC_DCHECK_NOTREACHED()`
preserves compile-time backward compatibility for downstream subclasses while
ensuring accidental or unsupported runtime calls fail fast during development
and testing.

## Examples

Bad:

```cpp
// Bad: Silent dummy return masks unimplemented calls that callers expect to succeed:
virtual bool RemoveIceCandidates(const std::vector<Candidate>& candidates) {
  return false;
}

// Bad: Empty no-op body silently ignores unsupported setter invocations:
virtual void SetCaptureTime(std::optional<Timestamp> capture_time) {}
virtual void SetPayloadType(uint8_t payload_type) {}
```

Good:

```cpp
// Good: Fails immediately if invoked on an un-overridden instance:
virtual bool RemoveIceCandidates(const std::vector<Candidate>& candidates) {
  RTC_CHECK_NOTREACHED();
}

// Good: Asserts in debug builds if invoked when capability is unsupported:
virtual void SetCaptureTime(std::optional<Timestamp> capture_time) {
  RTC_DCHECK_NOTREACHED();
}
virtual void SetPayloadType(uint8_t payload_type) {
  RTC_DCHECK_NOTREACHED();
}
```
