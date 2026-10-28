---
name: webrtc/explicit-deps-for-included-headers-in-build-gn
description: Explicitly declare direct GN dependencies in deps for included headers (including Abseil; avoid absl_deps), export wrappers via public_deps, and maintain accurate nogncheck annotations
rubric: design
severity: blocking
---

**Rule.** When adding or modifying `#include` or `#import` directives in
C/C++/Obj-C files or adding source files to GN targets:
1. Explicitly list the direct build target providing each included header in `deps` (or `public_deps`) in `BUILD.gn` (or `.gni`), using absolute paths for Abseil dependencies (e.g. `"//third_party/abseil-cpp/absl/strings:string_view"`), rather than relying on transitive dependencies, automated build cleaners in test targets, or the deprecated `absl_deps` list.
2. When source or header files are conditionally compiled inside feature blocks (such as `if (rtc_use_h265)`), declare corresponding dependencies in that same block using `deps +=`.
3. For header-forwarding wrapper targets (such as protobuf include wrappers), export the underlying dependency via `public_deps` annotated with `# no-presubmit-check TODO(webrtc:8603)` instead of adding workaround dependencies to consumer targets.
4. When headers are conditionally included under preprocessor guards (#if, #ifdef) whose GN dependencies are conditionally omitted in BUILD.gn, append `// nogncheck` to the `#include` line to prevent `gn check` failures. Conversely, remove obsolete nogncheck annotations when the corresponding dependency is explicitly and unconditionally declared in `deps`.

**Why.** GN enforces strict direct dependency boundaries via `gn check` (`gn gen
--check`). Undeclared direct dependencies, reliance on transitive dependencies,
or deprecated GN variables like `absl_deps` cause fragile modular builds and GN
template assertion failures. Automated cleaners may miss undeclared Abseil
dependencies in test targets. Because gn check inspects include directives
without evaluating preprocessor branches, conditionally omitted includes need
`// nogncheck` to suppress false positives, whereas retaining it on headers with
explicit `deps` bypasses dependency enforcement. Forwarding wrappers require
`public_deps` so consumers inherit header search paths and configurations.

## Examples

Bad:

```cpp
// 1. Missing direct Abseil dep, deprecated absl_deps, and consumer workaround:
rtc_library("consumer") {
  sources = [ "consumer.cc" ]  // includes "absl/strings/string_view.h" and "wrapper.h"
  deps = [
    ":wrapper_proto_include",
    ":underlying_proto",  // Bad: workaround dep for wrapper's headers
  ]
  absl_deps = [  // Bad: absl_deps is deprecated
    "//third_party/abseil-cpp/absl/types:optional",
  ]
}

rtc_source_set("wrapper_proto_include") {
  sources = [ "wrapper.h" ]
  deps = [ ":underlying_proto" ]  // Bad: fails to export headers via public_deps
}

// 2. Missing // nogncheck on conditional include / obsolete // nogncheck on declared dep:
#if defined(RTC_USE_PERFETTO)
#include "third_party/perfetto/include/perfetto/tracing/event_context.h"  // Bad: missing // nogncheck
#endif

#import "sdk/objc/components/renderer/metal/RTCMTLVideoView.h"  // nogncheck (Bad: dep already in deps)
```

Good:

```cpp
// 1. Explicit direct deps, Abseil in deps, and forwarding wrapper using public_deps:
rtc_library("consumer") {
  sources = [ "consumer.cc" ]
  deps = [
    ":wrapper_proto_include",
    "//third_party/abseil-cpp/absl/strings:string_view",
    "//third_party/abseil-cpp/absl/types:optional",
  ]
}

rtc_source_set("wrapper_proto_include") {
  sources = [ "wrapper.h" ]
  public_deps = [  # no-presubmit-check TODO(webrtc:8603)
    ":underlying_proto",
  ]
}

// 2. nogncheck only on conditionally omitted deps; removed from standard includes:
#if defined(RTC_USE_PERFETTO)
#include "third_party/perfetto/include/perfetto/tracing/event_context.h"  // nogncheck
#endif

#import "sdk/objc/components/renderer/metal/RTCMTLVideoView.h"
```
