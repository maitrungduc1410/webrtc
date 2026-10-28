---
name: webrtc/api/link-specifications-and-annotate-nonstandard-extensions
description: When defining public API types, enums, or stats governed by W3C/IETF standards, link the specification, accurately describe spec semantics, and annotate nonstandard extensions
rubric: readability
severity: should_fix
---

**Rule.** When defining or updating public API enums, structs, configuration
fields, interfaces, or standardized RTCStats classes governed by or proposed for
W3C/IETF specifications (e.g., WebRTC-PC, WebRTC-Stats, JSEP, RFCs), add a
header comment linking directly to the corresponding specification section or
tracking standard proposal issue (such as a W3C GitHub issue or IETF draft).
Ensure doc comments accurately state the specification semantics and constraints
rather than oversimplified or misleading summaries, and explicitly annotate any
values, stats members, or configuration options that are custom WebRTC
extensions or provisional proposals beyond the finalized standard.

**Why.** Public WebRTC API types, configuration options, and stats dictionaries
mirror external standards whose schemas, semantics, and allowed values are
specified or tracked by W3C and IETF documents. Linking the relevant
specification section or tracking proposal issue clarifies expected behavior and
standardization status, accurately describing spec semantics prevents misleading
API callers about runtime behavior under resource constraints, and documenting
nonstandard extensions prevents confusion between standard and proprietary
features.

## Examples

Bad:

```cpp
// Missing spec link and unannotated nonstandard extension
enum class IceTransportPolicy {
  kIceTransportPolicyNone,
  kIceTransportPolicyRelay,
  kIceTransportPolicyNoHost,
  kIceTransportPolicyAll
};

// Oversimplified and inaccurate summary of W3C spec behavior
enum class DegradationPreference {
  // Don't take any actions based on over-utilization signals.
  MAINTAIN_FRAMERATE_AND_RESOLUTION,
};

struct RTCConfiguration {
  // Always negotiate SCTP even if no datachannel exists currently.
  bool always_negotiate_sctp = false;
};

class RTC_EXPORT RTCAudioPlayoutStats final : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();
  RTCStatsMember<uint64_t> synthesized_samples_events;
};
```

Good:

```cpp
// https://w3c.github.io/webrtc-pc/#rtcicetransportpolicy-enum
enum class IceTransportPolicy {
  kIceTransportPolicyNone,    // Useful nonstandard extension
  kIceTransportPolicyRelay,
  kIceTransportPolicyNoHost,  // Useful nonstandard extension
  kIceTransportPolicyAll
};

// https://w3c.github.io/webrtc-pc/#dom-rtpdegradationpreference
enum class DegradationPreference {
  // Maintain framerate and resolution regardless of video quality. Frames may
  // be dropped before encoding if necessary not to overuse network resources.
  MAINTAIN_FRAMERATE_AND_RESOLUTION,
};

struct RTCConfiguration {
  // Always negotiate SCTP even if no datachannel exists currently.
  // https://github.com/w3c/webrtc-pc/issues/3072
  bool always_negotiate_sctp = false;
};

// https://w3c.github.io/webrtc-stats/#playoutstats-dict*
class RTC_EXPORT RTCAudioPlayoutStats final : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();
  RTCStatsMember<uint64_t> synthesized_samples_events;
};
```
