---
name: webrtc/preserve-link-time-gn-dependencies-with-keep
description: 'Retain GN dependencies for link-time symbols, SIMD definitions, and generated headers, linking declaration targets to definitions and annotating non-included deps with # keep'
rubric: design
severity: blocking
---

**Rule.** In GN build files (`BUILD.gn`, `.gni`), ensure that declaration
targets directly depend on their corresponding definition targets when symbol
declarations and implementations (such as SIMD optimizations or platform
implementations) are split across targets. Additionally, when declaring
dependencies on targets that generate header files or build artifacts, or
dependencies required only at link time (such as SIMD symbol definitions) or
cross-platform headers that lack direct `#include` directives, retain them in
`deps` and annotate them with ` # keep` (along with an explanatory comment or
TODO) so automated cleanup tools (e.g., `gn_check_autofix.py`) do not prune
them.

**Why.** GN dependency validation and automated cleanup tools (such as `gn
check`, `webrtc-include-cleaner`, and `tools_webrtc/gn_check_autofix.py -r`)
inspect direct `#include` directives in active C++ source files. If a definition
target (e.g., SIMD implementation) is separated from its declaration header
without a direct dependency link, or if link-time/SIMD dependencies and
generated headers lack direct `#include` statements, automated tools will prune
them. This causes missing symbol link errors or missing header build failures.
Annotating non-included dependencies with ` # keep` and ensuring declaration
targets pull in their implementations preserves required compilation ordering
and link integrity across all configurations.

## Examples

Bad:

```cpp
// Bad: declaration target does not depend on SIMD definition target
rtc_library("audio_c") {
  sources = [
    "filter.c",
    "filter.h",
  ]
}

rtc_library("audio_neon") {
  sources = [ "filter_neon.c" ]
}

// Bad: generated header or link-time dependency without # keep gets pruned
rtc_library("my_target") {
  sources = [ "my_target.cc" ]
  deps = [
    ":field_trials_view",
    "../../common_audio:common_audio_c",
  ]
}
```

Good:

```cpp
// Good: declaration target directly depends on the SIMD definition target
rtc_library("audio_c") {
  sources = [
    "filter.c",
    "filter.h",
  ]
  if (rtc_build_with_neon) {
    deps = [ ":audio_neon" ]
  }
}

rtc_library("audio_neon") {
  sources = [ "filter_neon.c" ]
}

// Good: retain non-included/link-time deps with # keep and explanatory comments
rtc_library("my_target") {
  sources = [ "my_target.cc" ]
  deps = [
    ":field_trials_view",
    "../experiments:registered_field_trials",  # keep
    # TODO(bugs.webrtc.org/496700738): Preserve link-time SIMD dependency.
    "../../common_audio",  # keep
    "../../common_audio:common_audio_c",
  ]
}
```
