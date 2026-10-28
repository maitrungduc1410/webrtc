---
name: webrtc/prefer-constexpr-absl-string-view-for-string-constants
description: When defining compile-time string constants or fixed arrays, use constexpr absl::string_view or constexpr char[] instead of std::string or pointer constants
rubric: readability
severity: blocking
---

**Rule.** Define compile-time string constants and fixed lookup arrays using
`constexpr absl::string_view` (or `constexpr std::array<absl::string_view, N>`)
rather than `constexpr std::string`, std::array<std::string, N>, or raw const
char* pointers. When constants or arrays must bind implicitly to `const
std::string&` parameters in callees that cannot be updated to string views, use
character arrays (`constexpr char[]` or constexpr char[][N]).

**Why.** Using `constexpr absl::string_view` ensures compile-time
initialization, avoiding startup dynamic initialization and non-portable
`constexpr std::string` compilation issues. Because `absl::string_view` does not
convert implicitly to `const std::string&`, `constexpr char[]` arrays provide a
compile-time constant fallback for APIs requiring `const std::string&`.

## Examples

Bad:

```cpp
// String constant with pointer decay or non-constexpr initialization:
const char kAudioMid1[] = "a1";
const char* kCodecName = "VP8";

// std::string triggers dynamic initialization or is not portably constexpr:
const std::array<std::string, 4> kValidIceServiceTypes{"stun", "stuns", "turn",
                                                       "turns"};
constexpr std::array<std::string, 2> kStreams{"stream1", "stream2"};
```

Good:

```cpp
// Compile-time string constants:
constexpr absl::string_view kAudioMid1 = "a1";
constexpr absl::string_view kCodecName = "VP8";

// Compile-time string view array avoids runtime dynamic initialization:
constexpr std::array<absl::string_view, 4> kValidIceServiceTypes{
    "stun", "stuns", "turn", "turns"};

// Use constexpr char arrays when parameters require const std::string&:
constexpr char kStreams[][8] = {"stream1", "stream2"};
```
