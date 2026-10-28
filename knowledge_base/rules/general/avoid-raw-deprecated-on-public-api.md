---
name: webrtc/avoid-raw-deprecated-on-public-api
description: 'When deprecating public C++ APIs or modernizing signatures in headers, avoid raw [[deprecated]] and #pragma suppressions; use TODO comments or ABSL_DEPRECATE_AND_INLINE delegating wrappers'
rubric: readability
severity: blocking
---

**Rule.** When deprecating or replacing public API constructors, methods,
functions, enum values, type aliases, or header transition shims in C++ headers,
do not annotate legacy declarations with raw `[[deprecated]]` or
`ABSL_DEPRECATED` while downstream consumers (such as Chromium) still invoke or
reference them under `-Werror`, and do not add `#pragma clang diagnostic ignored
"-Wdeprecated-declarations"` suppression blocks around header delegating
fallbacks.

Instead:
1. Retain an unannotated delegating shim, virtual fallback, or enum alias accompanied by a tracking bug comment (e.g. `// TODO(bugs.webrtc.org/12345): Deprecated in favor of ...`).
2. Alternatively, use `ABSL_DEPRECATE_AND_INLINE()` for inline functions, signature modernizations (such as migrating raw pointer and length pairs to ArrayView or span), and type aliases (`using Foo ABSL_DEPRECATE_AND_INLINE() = Bar;`). When doing so, include `"absl/base/macros.h"` and add `//third_party/abseil-cpp/absl/base:core_headers` to `BUILD.gn` deps.
3. Migrate downstream callers first, then remove legacy symbols in a dedicated follow-up CL.

**Why.** Downstream consumers such as Chromium compile against WebRTC public
headers with compiler warnings treated as errors (`-Werror`). Marking active
public symbols, methods, enum values, type aliases, or header transition shims
with raw `[[deprecated]]` or `ABSL_DEPRECATED` triggers immediate compilation
failures (`-Wdeprecated-declarations`) upon dependency rolls before downstream
callers migrate, or introduces unsightly compiler pragma suppression blocks into
public headers. Retaining unannotated shims with tracking TODOs or using
`ABSL_DEPRECATE_AND_INLINE()` (which is patched in Chromium and WebRTC not to
emit `[[deprecated]]`) enables clean, safe, multi-step migrations without
breaking downstream builds.

## Examples

Bad:

```cpp
// Bad: Raw [[deprecated]] breaks downstream -Werror builds:
[[deprecated("Use Timestamp overload instead")]]
RTCStats(std::string id, int64_t timestamp_us);

// Bad: Adding deprecation attributes and #pragma suppressions to header shims:
ABSL_DEPRECATED("Use SendPacketWithStatus instead")
virtual void SendPacket(rtc::ArrayView<const uint8_t> data) {}

virtual SendPacketStatus SendPacketWithStatus(rtc::ArrayView<const uint8_t> data) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  SendPacket(data);
#pragma clang diagnostic pop
  return SendPacketStatus::kSuccess;
}

// Bad: Modernizing signature without preserving legacy delegating wrapper:
std::vector<uint8_t> ParseRbsp(rtc::ArrayView<const uint8_t> data);
```

Good:

```cpp
// Good: Retain unannotated delegating shim with tracking TODO:
// TODO(webrtc:12345): Remove once downstream consumers migrate.
RTCStats(std::string id, int64_t timestamp_us)
    : RTCStats(std::move(id), Timestamp::Micros(timestamp_us)) {}

// Good: Document deprecation in TODO comment without compiler attributes or #pragma:
// TODO(bugs.webrtc.org/12345): Deprecated in favor of SendPacketWithStatus.
virtual void SendPacket(rtc::ArrayView<const uint8_t> data) {}

virtual SendPacketStatus SendPacketWithStatus(rtc::ArrayView<const uint8_t> data) {
  SendPacket(data);
  return SendPacketStatus::kSuccess;
}

// Good: Use ABSL_DEPRECATE_AND_INLINE wrapper forwarding to modernized signature:
#include "absl/base/macros.h"
std::vector<uint8_t> ParseRbsp(rtc::ArrayView<const uint8_t> data);

ABSL_DEPRECATE_AND_INLINE()
inline std::vector<uint8_t> ParseRbsp(const uint8_t* data, size_t length) {
  return ParseRbsp(rtc::MakeArrayView(data, length));
}
```
