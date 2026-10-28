---
name: webrtc/avoid-implicit-bool-conversion-for-integer-return-codes
description: When evaluating or asserting integer return codes from C, POSIX, OpenSSL, or stdio APIs, explicitly compare against expected status constants instead of coercing to bool
rubric: readability
severity: blocking
---

**Rule.** Explicitly compare integer return codes and status values from C,
POSIX, standard I/O (e.g., fflush, fclose, fseek), OpenSSL (e.g.,
SSL_export_keying_material), and library APIs against expected constants (such
as == 0, != 0, or == 1) or use explicit equality assertions (e.g.,
RTC_CHECK_EQ(..., 0), ASSERT_EQ(..., 0)). Do not assign integer return codes
directly to bool variables, evaluate raw integer status expressions directly in
boolean conditions, or coerce them using boolean assertion macros like
RTC_CHECK(...), RTC_CHECK(!...), or ASSERT_TRUE(...).

**Why.** C and system library APIs use varying status return conventions: many
APIs (POSIX, C stdio, WebRTC codecs) return 0 for success and negative or
non-zero values for failure, while others (such as OpenSSL) return 1 for
success. Implicitly converting integer status codes to bool or passing them to
boolean assertions like RTC_CHECK(fflush(...)) or RTC_CHECK(!func()) obscures
the API contract, risks inverted logic (such as treating 0 as false and crashing
on successful I/O), and prevents assertion macros from logging the actual
received error code on failure.

## Examples

Bad:

```cpp
// Bad: assigning int status code from OpenSSL C API directly to bool.
bool success = SSL_export_keying_material(
    ssl_, keying_material.data(), keying_material.size(),
    kDtlsSrtpExporterLabel.data(), kDtlsSrtpExporterLabel.size(),
    nullptr, 0, false);
if (success) {
  return keying_material.size();
}

// Bad: fflush returns 0 on success; RTC_CHECK evaluates to false and crashes!
RTC_CHECK(fflush(stream));

// Bad: coercing 0-on-success integer status code with boolean negation.
RTC_CHECK(!WebRtcIlbcfix_DecodeImpl(&decoded[i * blockl], &dummy, inst, 0));
```

Good:

```cpp
// Good: store int return code and explicitly compare against status constant.
int result = SSL_export_keying_material(
    ssl_, keying_material.data(), keying_material.size(),
    kDtlsSrtpExporterLabel.data(), kDtlsSrtpExporterLabel.size(),
    nullptr, 0, false);
if (result != 0) {
  return keying_material.size();
}

// Good: explicitly assert return value equals 0 for success.
RTC_CHECK_EQ(fflush(stream), 0);

// Good: compare integer status code explicitly with RTC_CHECK_EQ.
int decode_status = WebRtcIlbcfix_DecodeImpl(&decoded[i * blockl], &dummy, inst, 0);
RTC_CHECK_EQ(decode_status, 0);
```
