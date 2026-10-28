---
name: webrtc/avoid-default-arguments-when-adding-parameters
description: When adding configuration parameters, avoid default arguments on internal functions to enforce explicit call-site updates, but default temporary rollout flags with cleanup TODOs
rubric: readability
severity: blocking
---

**Rule.** When introducing new configuration flags, modes, or parameters to
constructors and internal functions, avoid assigning default argument values so
that all call sites, platform branches, and factory methods are explicitly
updated and reviewed at compile time. Conversely, when adding temporary rollout
flags or optional parameters to existing shared methods or factory functions to
prevent breaking callers during rollout, provide a default argument value
accompanied by an attributed cleanup TODO tracking its removal.

**Why.** Omitting default values on internal APIs ensures the compiler catches
all call sites that need intentional configuration updates. For temporary
rollout flags in shared code, providing a default value avoids breaking external
or downstream consumers during incremental rollout, while a cleanup TODO
prevents permanent API clutter.

## Examples

Bad:

```cpp
// Bad: Default argument on internal constructor masks call sites that should be updated.
FullScreenWindowDetector(
    ApplicationHandlerFactory application_handler_factory,
    bool use_heuristic_fullscreen_powerpoint_windows = true);

// Bad: Mandatory rollout parameter on shared factory breaks all existing callers without notice.
static std::unique_ptr<RtpPacketizer> Create(
    absl::optional<VideoCodecType> type,
    rtc::ArrayView<const uint8_t> payload,
    PayloadSizeLimits limits,
    const RTPVideoHeader& rtp_video_header,
    bool enable_av1_even_split);
```

Good:

```cpp
// Good: Explicit parameter forces compiler errors at all call sites across platforms.
FullScreenWindowDetector(
    ApplicationHandlerFactory application_handler_factory,
    bool use_heuristic_fullscreen_powerpoint_windows);

// Good: Default value with cleanup TODO preserves call-site compatibility during rollout.
static std::unique_ptr<RtpPacketizer> Create(
    absl::optional<VideoCodecType> type,
    rtc::ArrayView<const uint8_t> payload,
    PayloadSizeLimits limits,
    const RTPVideoHeader& rtp_video_header,
    // TODO(bugs.webrtc.org/15927): remove after rollout.
    bool enable_av1_even_split = false);
```
