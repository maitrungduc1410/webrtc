---
name: webrtc/omit-redundant-gn-target-names
description: When referencing GN dependency labels in BUILD.gn and .gni files, omit the redundant :target_name suffix when the target matches its enclosing directory
rubric: readability
severity: blocking
---

**Rule.** In GN build files (`BUILD.gn` and .gni), omit the redundant
`:target_name` suffix from dependency labels in deps, public_deps, or
visibility when the target name matches its enclosing directory name. For
example, use `../../api/environment` or `"//third_party/abseil-cpp/absl/strings"`
instead of `../../api/environment:environment`. Explicit `:target_name` suffixes
should only be used when the target name differs from its containing directory.

**Why.** GN target syntax automatically expands directory paths of the form
`//path/to/target` to `//path/to/target:target`. Omitting the redundant target
suffix keeps dependency lists concise, avoids visual clutter, adheres to the GN
style guide and WebRTC build conventions, and aligns with automated build
dependency tooling (`tools_webrtc/gn_check_autofix.py`).

## Examples

Bad:

```cpp
rtc_library("transport_adapter") {
  sources = [ "transport_adapter.cc" ]
  deps = [
    "../../api/environment:environment",
    "//modules/video_coding:video_coding",
    "//third_party/abseil-cpp/absl/strings:strings",
  ]
}
```

Good:

```cpp
rtc_library("transport_adapter") {
  sources = [ "transport_adapter.cc" ]
  deps = [
    "../../api/environment",
    "//modules/video_coding",
    "//third_party/abseil-cpp/absl/strings",
  ]
}
```
