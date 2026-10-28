---
name: webrtc/pc/validate-rtp-header-extension-ids-against-history
description: When validating RTP header extension mappings during SDP renegotiations or channel updates, verify extension IDs against all historically negotiated mappings to prevent reassigning an ID to a different URI
rubric: correctness
severity: blocking
---

**Rule.** When validating or updating RTP header extension mappings during SDP
renegotiations, channel parameter updates, or transport configuration, validate
proposed extension IDs against all historically mapped extensions in the session
rather than only currently active extensions. Ensure an extension ID is never
reassigned to a different URI even if the extension was omitted or inactive in
an intermediate renegotiation round.

**Why.** Per RFC 5285 / RFC 8285 Section 4.1, an RTP header extension identifier
MUST NOT be re-associated with a different extension URI within the same RTP
session. Validating only against currently active extensions allows an
identifier omitted in an intermediate offer/answer round to be illegally reused
for a different extension, causing packet parsing corruption or mismatched
header decoding.

## Examples

Bad:

```cpp
for (const auto& active_ext : active_rtp_header_extensions_) {
  for (const auto& new_ext : proposed_extensions) {
    if (new_ext.id == active_ext.id && new_ext.uri != active_ext.uri) {
      return RTCError(RTCErrorType::INVALID_PARAMETER,
                      "Extension ID reassigned to different URI");
    }
  }
}
```

Good:

```cpp
for (const auto& new_ext : proposed_extensions) {
  auto it = absl::c_find_if(
      historical_rtp_header_extensions_,
      [&](const RtpExtension& ext) { return ext.id == new_ext.id; });
  if (it != historical_rtp_header_extensions_.end() &&
      it->uri != new_ext.uri) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "RTP extension ID reassignment not supported");
  }
}
```
