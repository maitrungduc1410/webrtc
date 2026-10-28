---
name: webrtc/preserve-backward-compatibility-with-deprecated-aliases
description: When renaming, refactoring, or replacing types, methods, units, or headers, provide backward-compatible `[[deprecated]]` aliases, shims, or forwarding headers with removal-tracking `TODO`s instead of abrupt removal
rubric: design
severity: blocking
---

**Rule.** When renaming, refactoring, replacing, or moving public or shared
types, classes, methods, functions, virtual/interface methods, units (such as
raw numeric units to strong types like webrtc::Timestamp, TimeDelta, DataRate,
or DataSize), or header files, preserve backward compatibility by providing
[[deprecated("...")]] or ABSL_DEPRECATED type aliases (e.g., using OldType
[[deprecated]] = NewType;), delegating shims, legacy overloads, or forwarding
headers (e.g., #include "new/path.h") instead of abruptly removing or breaking
them.

Mark all retained legacy entry points (constructors, factories, getters,
methods, overloads, and forwarding headers) with an explicit removal tracking
TODO(bugs.webrtc.org/<id>) or TODO(crbug.com/<id>) comment. When migrating units
on a class or struct, simultaneously deprecate all related legacy entry points
(constructors, factory methods, and accessors/getters) rather than deprecating
only constructors while leaving getters unannotated. Never delete deprecated
aliases, shims, overloads, or forwarding headers until downstream consumers have
completed migration and verified zero remaining references.

**Why.** WebRTC public headers and shared interfaces are consumed out-of-tree by
Chromium and external downstream repositories. Abruptly renaming symbols,
altering signatures, deleting legacy methods, or moving header files without
forwarding shims causes immediate downstream build breakages upon DEPS roll
before callers can migrate. Leaving legacy functions, overloads, or interface
methods unannotated creates API ambiguity, bloat, and confusion over which
methods are canonical, and risks leaving dead migration shims indefinitely
without tracking their scheduled cleanup. Deprecating constructors while leaving
legacy unit getters unannotated creates inconsistent public APIs.

## Examples

Bad:

```cpp
// 1. Deleting legacy constructor or renaming types without compatibility shims:
class RTC_EXPORT RTCCertificateStats final : public RTCStats {
 public:
  RTCCertificateStats(std::string id, Timestamp timestamp);
};
class RTC_EXPORT RTCInboundRtpStreamStats final : public RTCReceivedRtpStreamStats {};

// 2. Deprecating constructors/factories but leaving legacy unit accessors un-deprecated:
ABSL_DEPRECATED("Call Create with Timestamp instead")
static rtc::scoped_refptr<RTCStatsReport> Create(int64_t timestamp_us);
static rtc::scoped_refptr<RTCStatsReport> Create(Timestamp timestamp);
int64_t timestamp_us() const { return timestamp_.us_or(-1); }
Timestamp timestamp() const { return timestamp_; }

// 3. Superseding interface/helper methods without deprecation tracking TODOs:
virtual std::optional<uint64_t> AbsoluteCaptureTimestamp() const = 0;
virtual std::optional<Timestamp> CaptureTime() const = 0;
scoped_refptr<const RTCStatsReport> NewGetStats();
scoped_refptr<const RTCStatsReport> NewGetStats(test::RunLoop& run_loop);
```

Good:

```cpp
// 1. Retain legacy constructor and provide deprecated type alias shim:
class RTC_EXPORT RTCCertificateStats final : public RTCStats {
 public:
  RTCCertificateStats(std::string id, Timestamp timestamp);
  // TODO(bugs.webrtc.org/12345): Remove once downstream consumers migrate.
  [[deprecated("Use Timestamp overload instead")]]
  RTCCertificateStats(std::string id, int64_t timestamp_us)
      : RTCCertificateStats(std::move(id), Timestamp::Micros(timestamp_us)) {}
};
class RTC_EXPORT RTCInboundRtpStreamStats final : public RTCReceivedRtpStreamStats {};
// TODO(bugs.webrtc.org/12345): Remove once downstream references are updated.
using RTCInboundRTPStreamStats [[deprecated]] = RTCInboundRtpStreamStats;

// 2. Deprecate constructors, factories, and accessors simultaneously:
ABSL_DEPRECATED("Call Create with Timestamp instead")
static rtc::scoped_refptr<RTCStatsReport> Create(int64_t timestamp_us);
static rtc::scoped_refptr<RTCStatsReport> Create(Timestamp timestamp);
ABSL_DEPRECATED("Call timestamp() instead")
int64_t timestamp_us() const { return timestamp_.us_or(-1); }
Timestamp timestamp() const { return timestamp_; }

// 3. Annotate legacy interface methods and overloads with deprecation and removal TODOs:
// TODO(crbug.com/391114797): Remove once callers migrate to CaptureTime().
[[deprecated("Use CaptureTime() instead")]]
virtual std::optional<uint64_t> AbsoluteCaptureTimestamp() const = 0;
virtual std::optional<Timestamp> CaptureTime() const = 0;

// TODO(bugs.webrtc.org/501310910): Remove in favor of the RunLoop overload.
scoped_refptr<const RTCStatsReport> NewGetStats();
scoped_refptr<const RTCStatsReport> NewGetStats(test::RunLoop& run_loop);

// 4. For moved headers, leave a forwarding header with a tracking TODO:
// In source/time_util.h (forwarding header):
// TODO(bugs.webrtc.org/12345): Remove once downstream projects migrate.
#include "modules/rtp_rtcp/source/ntp_time_util.h"
```
