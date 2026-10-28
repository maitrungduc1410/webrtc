---
name: webrtc/use-elif-for-mutually-exclusive-preprocessor-blocks
description: 'When conditionally compiling mutually exclusive platform backends, architecture branches, or fallback returns, chain conditions with #elif and #else instead of separate #if blocks'
rubric: correctness
severity: blocking
---

**Rule.** When conditionally compiling mutually exclusive platform
implementations, architecture-specific logic, alternative backends, or
configuration declarations, chain branches using #elif and #else rather than
writing consecutive independent #if ... #endif blocks. Additionally, when
platform- or architecture-specific branches (#if, #elif, #ifdef) return
unconditionally or provide early exits, enclose subsequent fallback logic and
default returns within #elif or #else blocks rather than placing them unguarded
after #endif.

**Why.** Chaining mutually exclusive preprocessor branches with #elif and #else
makes mutual exclusion explicit, prevents contradictory declarations or
duplicate symbol definitions across platforms, and reduces boilerplate #endif
comments. Furthermore, because preprocessor branches and code outside #endif
reside in the same translation unit, placing fallback code after an
unconditional return in an active #if branch creates unreachable dead code and
triggers -Wunreachable-code compiler warnings or build failures.

## Examples

Bad:

```cpp
// Declarations / backends: separate #if blocks
#if defined(WEBRTC_USE_EPOLL)
void AddEpoll(Dispatcher* dispatcher, uint64_t key);
#endif  // WEBRTC_USE_EPOLL
#if defined(WEBRTC_USE_POLL)
bool WaitPoll(int cmsWait, bool process_io);
#endif  // WEBRTC_USE_POLL

// Unconditional return before unguarded fallback
#if defined(WEBRTC_USE_POLL)
  return WaitPoll(cmsWait, process_io);
#endif
#if defined(MIPS32_LE)
  return WebRtcSpl_MaxAbsValueW16_mips(vector, length);
#endif
  return WaitSelect(cmsWait, process_io);
```

Good:

```cpp
// Declarations / backends: chained #elif
#if defined(WEBRTC_USE_EPOLL)
void AddEpoll(Dispatcher* dispatcher, uint64_t key);
#elif defined(WEBRTC_USE_POLL)
bool WaitPoll(int cmsWait, bool process_io);
#endif  // WEBRTC_USE_POLL

// Fallback logic enclosed in #elif / #else
#if defined(WEBRTC_USE_POLL)
  return WaitPoll(cmsWait, process_io);
#elif defined(MIPS32_LE)
  return WebRtcSpl_MaxAbsValueW16_mips(vector, length);
#else
  return WaitSelect(cmsWait, process_io);
#endif
```
