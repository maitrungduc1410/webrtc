---
name: webrtc/omit-redundant-enclosing-namespace-qualifiers-2
description: 'Declare symbols directly in namespace webrtc and omit redundant webrtc:: or obsolete rtc::/cricket:: qualifiers except in macros or shadowed types'
rubric: readability
severity: blocking
---

**Rule.** Place all WebRTC types, functions, classes, and enums directly inside
`namespace webrtc` rather than defining them with file-scope qualification, in
obsolete namespaces (`rtc`, `cricket`), or in legacy nested subsystem namespaces
(such as `webrtc::video_coding`). When writing code, comments, or deprecation
attributes inside enclosing namespace blocks (including nested namespaces like
`webrtc::test` or anonymous namespaces):
1. Omit redundant enclosing namespace prefixes (such as `webrtc::`, `::webrtc::`, or `rtcp::`) on types, template arguments, functions, constants, utilities (e.g., make_ref_counted, scoped_refptr, SocketAddress, PacketSocketOptions, StrJoin), and mock declarations (MOCK_METHOD).
2. Do not use legacy, removed namespace qualifiers or aliases such as `rtc::` or `cricket::`.

Exceptions and special cases:
1. Preprocessor Macros: In `#define` macro definitions, retain full namespace qualification (e.g., `webrtc::`, `webrtc::internal::`) because macros expand textually in arbitrary caller scopes without lexical scoping.
2. Member or Identifier Shadowing: When referencing a type shadowed by a member function, variable, or local identifier in scope (e.g., method ColorSpace() vs type ColorSpace, or method EncodedImage() vs type EncodedImage), explicitly qualify the type with its namespace (`webrtc::ColorSpace` or `::webrtc::EncodedImage`). Do not use C-style elaborated type specifiers (such as `class ColorSpace` or struct EncodedImage).
3. Disambiguation: Explicitly qualify symbols when strictly necessary to resolve base-class overload collisions or name shadowing.

**Why.** The `rtc` and `cricket` namespaces were removed in 2025, and legacy
nested subsystem namespaces (e.g., `webrtc::video_coding`) have been flattened
into `namespace webrtc`. Placing declarations directly in the webrtc namespace
ensures proper lexical scoping and type hierarchy consistency. Redundant
`webrtc::` qualifiers and obsolete rtc::/cricket:: prefixes in code and
comments add visual noise and boilerplate. Conversely, macros expand in
arbitrary caller scopes where unqualified lookup can fail or collide, requiring
full qualification. When member identifiers shadow type names, explicit
namespace qualification cleanly resolves ambiguity without relying on misleading
elaborated type specifiers that resemble forward declarations.

## Examples

Bad:

```cpp
// Bad: Declaring in legacy/nested namespaces or file-scope qualification:
namespace rtc {
enum class EcnMarking { kNotEct = 0, kEct1 = 1 };
}  // namespace rtc

namespace webrtc {
namespace video_coding {
class H26xPacketBuffer { public: void Clear(); };
}  // namespace video_coding

// Bad: Redundant webrtc:: and obsolete rtc:: qualifiers:
void Process(const webrtc::VideoSinkWants& wants,
             rtc::scoped_refptr<webrtc::I420BufferInterface> buf);

// Bad: Using elaborated type specifier to bypass member shadowing:
class Frame {
 public:
  const ColorSpace* ColorSpace() const;
  void SetColorSpace(const std::optional<class ColorSpace>& cs);
  const EncodedImage& EncodedImage() const {
    return static_cast<const class EncodedImage&>(*this);
  }
};

// Bad: Unqualified symbol in macro definition:
#define RTC_CHECK_RUNS(x) internal::RaceCheckerScope checker(x);
}  // namespace webrtc
```

Good:

```cpp
namespace webrtc {

// Good: Declaring directly in namespace webrtc:
enum class EcnMarking { kNotEct = 0, kEct1 = 1 };
class H26xPacketBuffer { public: void Clear(); };

// Good: Unqualified types without redundant or obsolete qualifiers:
void Process(const VideoSinkWants& wants,
             scoped_refptr<I420BufferInterface> buf);

// Good: Explicit namespace qualification resolves member shadowing:
class Frame {
 public:
  const webrtc::ColorSpace* ColorSpace() const;
  void SetColorSpace(const std::optional<webrtc::ColorSpace>& cs);
  const EncodedImage& EncodedImage() const {
    return static_cast<const ::webrtc::EncodedImage&>(*this);
  }
};

// Good: Full namespace qualification in macro definitions:
#define RTC_CHECK_RUNS(x) webrtc::internal::RaceCheckerScope checker(x);

}  // namespace webrtc
```
