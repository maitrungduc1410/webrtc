---
name: webrtc/modules/qualify-codec-specific-packet-classes-and-cite-rfc
description: When defining RTCP or RTP protocol packet classes, name them after their specific specification message or codec and cite the governing RFC
rubric: readability
severity: should_fix
---

**Rule.** When defining or documenting protocol packet classes (such as RTCP
feedback packets), name the concrete class after its specific message name in
the governing specification (e.g., CongestionControlFeedback for RFC 8888) and
qualify codec- or format-specific payloads (e.g., H26xRpsi). Do not name classes
after generic category abstractions (such as TransportLayerFeedback for Rtpfb)
or cite only base framework specifications (such as RFC 4585) when implementing
a specific message format or codec profile; cite the governing specialization
RFC in header doc comments.

**Why.** Naming a packet class after a generic transport category or omitting
codec specializations creates ambiguity with base classes (like Rtpfb), obscures
specific packet semantics, and misleads callers about protocol compatibility.
Citing the governing message RFC directly in the header clarifies format
constraints and packet structure.

## Examples

Bad:

```cpp
namespace webrtc::rtcp {
// Reference Picture Selection Indication (RPSI) (RFC 4585).
class Rpsi : public Psfb {
 public:
  void SetPictureOrderCnt(uint32_t pic_order_cnt);
};

// Generic category name used for a concrete RFC 8888 feedback message.
class TransportLayerFeedback : public Rtpfb {
 public:
  static constexpr uint8_t kFeedbackMessageType = 11;
};
}  // namespace webrtc::rtcp
```

Good:

```cpp
namespace webrtc::rtcp {
// Reference Picture Selection Indication (RPSI) for H.26x (RFC 7798).
class H26xRpsi : public Psfb {
 public:
  void SetPictureOrderCnt(uint32_t pic_order_cnt);
};

// Specific RFC 8888 message name matching specification.
// See https://www.rfc-editor.org/rfc/rfc8888.html
class CongestionControlFeedback : public Rtpfb {
 public:
  static constexpr uint8_t kFeedbackMessageType = 11;
};
}  // namespace webrtc::rtcp
```
