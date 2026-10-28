---
name: webrtc/net/match-handover-state-integer-types-to-protocol-spec
description: When defining socket handover state structs (DcSctpSocketHandoverState), use integer field widths matching SCTP protocol specifications and internal types (like uint16_t for SSN)
rubric: correctness
severity: should_fix
---

**Rule.** In dcSCTP handover state structs (`DcSctpSocketHandoverState`),
declare primitive integer field widths that match the underlying SCTP protocol
specification (e.g. RFC 4960) and internal type aliases (such as `uint16_t` for
16-bit Stream Sequence Numbers `SSN` and stream identifiers `stream_id`, rather
than `uint32_t`). Avoid over-allocating field widths for standard 16-bit
protocol quantities.

**Why.** SCTP Stream Sequence Numbers (SSN) and stream IDs are defined as 16-bit
unsigned integers in RFC 4960 and internal type aliases (`dcsctp::SSN`). Using
oversized integer types (like `uint32_t`) introduces type mismatches with
internal aliases, obscures the protocol bounds of the field, and causes
unnecessary serialization bloat in handover state payloads.

## Examples

Bad:

```cpp
struct DcSctpSocketHandoverState {
  struct OutstandingData {
    uint32_t tsn = 0;
    uint32_t mid = 0;
    uint16_t stream_id = 0;
    // BAD: SSN is a 16-bit protocol value defined as uint16_t in dcsctp::SSN.
    uint32_t ssn = 0;
    uint32_t fsn = 0;
  };
};
```

Good:

```cpp
struct DcSctpSocketHandoverState {
  struct OutstandingData {
    uint32_t tsn = 0;
    uint32_t mid = 0;
    uint16_t stream_id = 0;
    // GOOD: Uses uint16_t matching RFC 4960 and dcsctp::SSN width.
    uint16_t ssn = 0;
    uint32_t fsn = 0;
  };
};
```
