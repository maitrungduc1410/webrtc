---
name: webrtc/provide-default-methods-for-new-java-interface-methods
description: When evolving public Java interfaces, provide default implementations for backward compatibility, and omit removal TODOs on permanent optional capability fallbacks
rubric: readability
severity: blocking
---

**Rule.** When adding new methods, callbacks, or listener events to existing
public Java interfaces, provide `default` implementations (such as an empty body
`{}` for void observer/callback methods, or a sensible fallback return value)
rather than declaring them as abstract methods without bodies. Additionally,
when providing default implementations for interface methods that represent
optional features or hardware-specific capabilities, do not add a `TODO` to
remove the default body or make the method abstract/pure virtual, as optional
capabilities are intended to have permanent fallbacks rather than temporary
migration shims.

**Why.** Adding abstract methods to existing public Java interfaces breaks
source and binary compatibility for external implementers and downstream
applications during SDK updates. Providing default implementations allows new
methods and callbacks to land safely without forcing immediate subclass
modifications. Furthermore, default methods for optional capabilities serve as
permanent fallbacks for implementations lacking the feature; adding removal
TODOs on optional APIs creates false migration churn by suggesting that all
subclasses must eventually implement the method.

## Examples

Bad:

```cpp
public interface AudioDeviceObserver {
  // Bad: Adding abstract method to existing public interface breaks downstream implementers.
  @CalledByNative("AudioDeviceObserver")
  void onRemoveTrack(RtpReceiver receiver);

  // Bad: Optional capability default is marked with a removal TODO.
  /** TODO(b/279738239): Remove default implementation. */
  default boolean setNoiseSuppressorEnabled(boolean enabled) {
    return false;
  }
}
```

Good:

```cpp
public interface AudioDeviceObserver {
  // Good: Default empty body preserves backward compatibility for existing implementers.
  @CalledByNative("AudioDeviceObserver")
  default void onRemoveTrack(RtpReceiver receiver) {}

  // Good: Permanent default fallback for optional capability without a removal TODO.
  default boolean setNoiseSuppressorEnabled(boolean enabled) {
    return false;
  }
}
```
