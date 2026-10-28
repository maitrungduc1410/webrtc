---
name: webrtc/sdk/place-public-android-sdk-classes-under-api
description: When declaring Android SDK Java classes and methods, place public APIs under sdk/android/api and keep internal helper classes and methods package-private
rubric: readability
severity: should_fix
---

**Rule.** In the Android SDK (`sdk/android`), place public API classes,
interfaces, and enums under `sdk/android/api/org/webrtc/` rather than internal
implementation directories (`sdk/android/src/java/org/webrtc/`). Keep internal
helper classes, implementation types, and their package-internal methods located
under `sdk/android/src/java/` package-private (omit the `public` modifier) when
they are not part of the public SDK API.

**Why.** The WebRTC Android SDK strictly enforces boundary separation between
the public consumer API surface (`sdk/android/api/`) and internal implementation
utilities (`sdk/android/src/java/`). Placing public classes in internal source
folders or declaring internal helper classes and methods as `public`
unnecessarily pollutes the public SDK API surface, exposes internal
implementation details, and misleads callers about API boundaries.

## Examples

Bad:

```cpp
// File: sdk/android/src/java/org/webrtc/WrappedNativeVideoDecoder.java
// Bad: Public API class placed in internal src/java directory instead of api/
public abstract class WrappedNativeVideoDecoder implements VideoDecoder {}

// File: sdk/android/src/java/org/webrtc/audio/WebRtcAudioRecord.java
package org.webrtc.audio;

class WebRtcAudioRecord {
  // Bad: Method in package-private internal class declared public
  public void setPreferredDevice(@Nullable AudioDeviceInfo preferredDevice) {}
}
```

Good:

```cpp
// File: sdk/android/api/org/webrtc/WrappedNativeVideoDecoder.java
// Good: Public API class placed in public api/ directory
public abstract class WrappedNativeVideoDecoder implements VideoDecoder {}

// File: sdk/android/src/java/org/webrtc/audio/WebRtcAudioRecord.java
package org.webrtc.audio;

class WebRtcAudioRecord {
  // Good: Package-private method restricts access to internal package callers
  void setPreferredDevice(@Nullable AudioDeviceInfo preferredDevice) {}
}
```
