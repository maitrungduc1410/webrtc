---
name: webrtc/sdk/check-and-log-fallible-platform-apis
description: When invoking fallible Android platform audio or effect APIs, verify return status, log failures, and return boolean success through wrapper layers
rubric: correctness
severity: should_fix
---

**Rule.** When invoking fallible platform or subsystem API methods (such as
AudioRecord.setPreferredDevice, AudioTrack.setPreferredDevice,
AudioManager.setCommunicationDevice, or AudioEffect.setEnabled), do not silently
discard the returned status or return void from wrapper methods. Verify the
returned boolean status or error code (such as AudioEffect.SUCCESS), log an
error on failure (e.g. using Logging.e), and return or propagate boolean success
indicators across wrapper and interface layers so callers can detect failures.

**Why.** Android platform audio and hardware effect APIs return boolean flags or
status codes to signal when hardware, routing, or effect configuration requests
fail or when underlying effect instances are unavailable. Discarding return
values or returning void from wrapper layers masks platform errors, prevents
callers from handling failures, and makes audio routing or processing bugs
difficult to diagnose.

## Examples

Bad:

```cpp
// Anti-pattern 1: Silently discarding fallible platform API return status
if (audioRecord != null) {
  audioRecord.setPreferredDevice(preferredDevice);
}

// Anti-pattern 2: Returning void from effect toggling wrapper without checking status
public void toggleNS(boolean enable) {
  if (ns == null) {
    Logging.e(TAG, "Attempting to enable or disable nonexistent NoiseSuppressor.");
    return;
  }
  ns.setEnabled(enable);
}
```

Good:

```cpp
// Solution 1: Check return value and log error on failure
if (audioRecord != null) {
  if (!audioRecord.setPreferredDevice(preferredDevice)) {
    Logging.e(TAG, "setPreferredDevice failed");
  }
}

// Solution 2: Verify AudioEffect status code and propagate boolean success to callers
public boolean toggleNS(boolean enable) {
  if (ns == null) {
    Logging.e(TAG, "Attempting to enable or disable nonexistent NoiseSuppressor.");
    return false;
  }
  return ns.setEnabled(enable) == AudioEffect.SUCCESS;
}
```
