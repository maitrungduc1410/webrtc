---
name: webrtc/guard-platform-specific-gn-targets
description: When defining platform- or feature-specific GN targets, test suites, or dependencies, enclose target declarations and dependent deps inside matching OS and feature flag conditionals
rubric: correctness
severity: blocking
---

**Rule.** In GN build files (`BUILD.gn`, .gni), when a target (such as
`rtc_library`, rtc_source_set, rtc_test, or tool) or dependency applies only
to specific platforms (e.g., is_ios, is_android, is_mac, is_win, is_linux,
is_chromeos) or feature flags (e.g., rtc_use_pipewire, rtc_enable_protobuf),
enclose the target definition and referencing `deps` or test group lists inside
matching conditionals (e.g., `if (is_ios) { ... }` or `if ((is_linux ||
is_chromeos) && rtc_use_pipewire) { deps += [ ... ] }`). Do not declare
single-platform targets unconditionally or reference platform-/feature-specific
targets in unconditional generic dependency groups.

**Why.** Declaring single-platform targets unconditionally or leaving empty
target bodies when sources are stripped can cause build and link failures on
unsupported platforms (such as empty archive errors with macOS `libtool` or
missing symbol errors). Likewise, unconditionally referencing platform- or
feature-dependent targets in generic test groups or top-level dependency lists
fails builds on platforms or configurations where underlying OS APIs, SDKs, or
feature flags are unavailable.

## Examples

Bad:

```cpp
# Unconditional target declaration leaves an empty target on macOS
rtc_library("opengl_ui_objc") {
  if (is_ios) {
    sources = [ "RTCEAGLVideoView.m" ]
  }
}

rtc_library("app_framework") {
  deps = [ ":opengl_ui_objc" ]  # Fails on non-iOS platforms
}

# Unconditionally adding platform/feature-specific test target to test group
if (rtc_include_tests) {
  deps += [
    "modules/audio_processing:audio_processing_tests",
    "modules/desktop_capture:shared_screencast_stream_test",
  ]
}
```

Good:

```cpp
# Enclose platform-specific target and referencing deps in platform conditionals
if (is_ios) {
  rtc_library("opengl_ui_objc") {
    sources = [ "RTCEAGLVideoView.m" ]
  }
}

rtc_library("app_framework") {
  if (is_ios) {
    deps += [ ":opengl_ui_objc" ]
  }
}

# Guard platform/feature test targets with OS and feature conditionals
if (rtc_include_tests) {
  deps += [ "modules/audio_processing:audio_processing_tests" ]
  if ((is_linux || is_chromeos) && rtc_use_pipewire) {
    deps += [ "modules/desktop_capture:shared_screencast_stream_test" ]
  }
}
```
