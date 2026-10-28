---
name: webrtc/include-tracking-bug-in-deprecated-attribute
description: When deprecating declarations or struct fields, annotate with [[deprecated("...")]] with replacement guidance or bug URL, or ABSL_DEPRECATE_AND_INLINE(), avoiding comments alone or bare attributes
rubric: readability
severity: blocking
---

**Rule.** When deprecating functions, methods, constructors, classes, type
aliases, or configuration fields in options/settings structs, annotate
declarations with active `[[deprecated("...")]]` specifying explicit replacement
guidance or a tracking bug URL (e.g., `[[deprecated("Use NewMethod instead; see
b/12345")]]`). Either delete dead or obsolete fields/declarations completely, or
annotate them if temporarily retained for downstream compatibility. Do not leave
deprecation attributes commented out, do not rely solely on informal `//
DEPRECATED` or `TODO` comments without compiler annotations, avoid bare
`[[deprecated]]` attributes lacking migration guidance, and omit redundant `//
Deprecated` comment banners above annotated declarations.

For trivial wrapper functions or type aliases defined inline in header files,
annotate them with `ABSL_DEPRECATE_AND_INLINE()` to enable automated downstream
migration. Conversely, use `[[deprecated("...")]]` (not
`ABSL_DEPRECATE_AND_INLINE()`) for out-of-line declarations or functions
containing non-trivial implementation logic (such as raw casts or complex
operations) that refactoring tools should not inline into callers.

**Why.** Informal comments (such as `// DEPRECATED`) and commented-out
attributes are not enforced by the compiler, misleading callers into believing
dead options or functions are still supported. Bare `[[deprecated]]` attributes
produce uninformative compiler diagnostics without migration direction, while
redundant comment banners add maintenance clutter.

Providing explicit
replacement guidance or tracking bug URLs in active `[[deprecated("...")]]`
attributes ensures compile-time errors in WebRTC and actionable warnings for
downstream projects. Completely removing dead struct fields keeps configuration
interfaces clean. Using `ABSL_DEPRECATE_AND_INLINE()` on trivial inline wrappers
and type aliases enables automated refactoring tooling to inline or rewrite call
sites across codebases while inline definitions prevent ODR violations, whereas
`[[deprecated("...")]]` on non-trivial functions prevents tooling from
mechanically inlining raw casts or unsafe logic into callers.

## Examples

Bad:

```cpp
// 1. Informal comment on deprecated struct/config field:
struct DcsctpOptions {
  // DEPRECATED: See `zero_checksum_alternate_error_detection_method`.
  bool enable_zero_checksum = false;

  ZeroChecksumAlternative zero_checksum_alternate_error_detection_method =
      ZeroChecksumAlternative::kNone;
};

// 2. Bare [[deprecated]], commented-out attribute, or comment-only deprecation:
// [[deprecated("Use Encode(clip, bitrate) instead")]]
virtual RTCErrorOr<std::string> Encode2(const TestClip& clip,
                                        DataRate bitrate);

// Deprecated... Use SetPacerConfig.
[[deprecated]]
void SetSendBurstInterval(TimeDelta burst_interval);

// 3. Plain [[deprecated]] on trivial inline wrapper or alias where automated inlining is desirable:
[[deprecated("Use sdesc->Clone()")]]
inline std::unique_ptr<SessionDescriptionInterface> CloneSessionDescription(
    const SessionDescriptionInterface* sdesc) {
  return sdesc->Clone();
}

// 4. Using ABSL_DEPRECATE_AND_INLINE on non-trivial implementation logic:
ABSL_DEPRECATE_AND_INLINE()
inline void Set8(void* memory, size_t offset, uint8_t v) {
  static_cast<uint8_t*>(memory)[offset] = v;
}
```

Good:

```cpp
// 1. Active [[deprecated]] on retained config field, or remove dead fields:
struct DcsctpOptions {
  [[deprecated("Use zero_checksum_alternate_error_detection_method")]]
  bool enable_zero_checksum = false;

  ZeroChecksumAlternative zero_checksum_alternate_error_detection_method =
      ZeroChecksumAlternative::kNone;
};

// 2. Active [[deprecated]] with replacement guidance/bug URL and no redundant comment banner:
[[deprecated("Use Encode(clip, bitrate) instead; see b/12345")]]
virtual RTCErrorOr<std::string> Encode2(const TestClip& clip,
                                        DataRate bitrate);

[[deprecated("Use SetPacerConfig instead; see b/446768451")]]
void SetSendBurstInterval(TimeDelta burst_interval);

// 3. Inlined definition with ABSL_DEPRECATE_AND_INLINE enables automated refactoring:
ABSL_DEPRECATE_AND_INLINE()
inline std::unique_ptr<SessionDescriptionInterface> CloneSessionDescription(
    const SessionDescriptionInterface* sdesc) {
  return sdesc->Clone();
}

using OldSessionDescription ABSL_DEPRECATE_AND_INLINE() =
    SessionDescriptionInterface;

// 4. Use [[deprecated("...")]] when callers should migrate without inlining raw logic:
[[deprecated("Use version with ArrayView")]]
inline void Set8(void* memory, size_t offset, uint8_t v) {
  static_cast<uint8_t*>(memory)[offset] = v;
}
```
