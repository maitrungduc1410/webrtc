---
name: webrtc/annotate-requiresapi-without-sdk-checks
description: When implementing Android Java methods calling newer platform APIs without runtime checks, annotate with @RequiresApi and specify the accurate Build.VERSION_CODES level
rubric: correctness
severity: blocking
---

**Rule.** When implementing Android Java methods that invoke platform APIs
requiring a newer SDK level without internal runtime version checks
(`Build.VERSION.SDK_INT`), annotate the method with `@RequiresApi` (optionally
alongside `@TargetApi`) rather than relying solely on `@TargetApi`.
Additionally, verify that the specified `Build.VERSION_CODES` constant
accurately matches the actual minimum API level where the underlying platform
methods were introduced.

**Why.** `@TargetApi` only suppresses Android Lint warnings within the method
body and fails to alert callers to verify the API level before invoking the
method. Furthermore, understating the required `Build.VERSION_CODES` level in
`@RequiresApi` bypasses static lint checks at call sites, leading to runtime
`NoSuchMethodError` crashes on intermediate Android OS versions.

## Examples

Bad:

```cpp
// Anti-pattern 1: Using @TargetApi alone leaves callers unchecked.
@TargetApi(Build.VERSION_CODES.M)
public void setPreferredDevice(@Nullable AudioDeviceInfo preferredDevice) {
  if (audioRecord != null) {
    audioRecord.setPreferredDevice(preferredDevice);
  }
}

// Anti-pattern 2: Understating the required API level (e.g., M instead of Q).
@RequiresApi(Build.VERSION_CODES.M)
public boolean setPreferredMicrophoneDirection(int direction) {
  return audioInput.setPreferredMicrophoneDirection(direction);
}
```

Good:

```cpp
// Idiomatic: @RequiresApi with the accurate Build.VERSION_CODES level ensures Lint checks callers.
@RequiresApi(Build.VERSION_CODES.M)
@TargetApi(Build.VERSION_CODES.M)
public void setPreferredDevice(@Nullable AudioDeviceInfo preferredDevice) {
  if (audioRecord != null) {
    audioRecord.setPreferredDevice(preferredDevice);
  }
}

@RequiresApi(Build.VERSION_CODES.Q)
public boolean setPreferredMicrophoneDirection(int direction) {
  return audioInput.setPreferredMicrophoneDirection(direction);
}
```
