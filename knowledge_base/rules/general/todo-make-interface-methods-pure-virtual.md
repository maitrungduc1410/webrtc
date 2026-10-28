---
name: webrtc/todo-make-interface-methods-pure-virtual
description: When evolving public APIs, SDK headers, or virtual interfaces, provide stateless delegating fallbacks or default stubs with 'make pure virtual' TODOs instead of `= 0`
rubric: design
severity: blocking
---

**Rule.** When modifying or evolving existing public C++ interfaces (`api/**`,
`*_interface.h`, `*_factory.h`), SDK headers (`sdk/**`), or shared virtual
classes:
1. Do not add pure-virtual (`= 0`) methods to existing interfaces or devirtualize existing methods in place; keep both legacy and replacement methods `virtual`.
2. For new virtual methods undergoing migration, provide a default implementation: either supply a stub asserting `RTC_DCHECK_NOTREACHED()` or `RTC_CHECK_NOTREACHED()` (with a fallback return value) accompanied by an explicit tracking TODO comment (`// TODO(bugs.webrtc.org/<id>): make pure virtual`; do not phrase as 'make virtual' or 'remove'), or provide a documented functional fallback implementation.
3. For deprecated virtual methods or modified SDK callback/method signatures, provide a stateless, thread-safe delegating default implementation (mutual delegation forwarding to the replacement signature). Annotate it with `[[deprecated("...")]]`, `RTC_DEPRECATED`, or `ABSL_DEPRECATE_AND_INLINE` (or RTC_OBJC_DEPRECATED in Obj-C), accurately naming the replacement API and referencing a tracking bug (e.g., TODO(bugs.webrtc.org/<id>): Remove once downstream migrates).
4. If providing a functional fallback default instead of a crashing stub, explicitly document its fallback behavior, limitations, and override guidance in header doc comments.
5. Conversely, when designing brand-new abstract classes or interfaces from scratch, declare required methods as pure virtual (`= 0`) rather than providing uncallable dummy bodies.
6. Isolate symbol and shim deletions in dedicated follow-up CLs after downstream consumers have migrated. Wrap necessary legacy or test call sites with `#pragma clang diagnostic ignored "-Wdeprecated-declarations"`.

**Why.** Downstream projects and closed-source consumers (such as Chromium and
internal repositories) build out-of-tree against moving WebRTC checkouts and
implement virtual interfaces across repository boundaries. Adding new
pure-virtual (`= 0`) methods, devirtualizing legacy methods, or modifying SDK
signatures in place immediately breaks downstream subclass compilation during
dependency rolls before implementers can add new overrides. Retaining delegating
shims, providing stateless and thread-safe default stubs
(`RTC_DCHECK_NOTREACHED()`) with explicit 'make pure virtual' tracking TODOs,
documenting functional fallback behavior, and isolating deletions in dedicated
follow-up CLs ensures binary/source compatibility and enables safe, staged
migrations across rolling checkouts.

## Examples

Bad:

```cpp
// 1. New virtual method on existing interface is pure virtual (= 0):
virtual std::unique_ptr<AsyncPacketSocket> CreateClientUdpSocket(
    const SocketAddress& local, const SocketAddress& remote) = 0;

// 2. Modifying SDK signature in place or devirtualizing legacy method:
- (void)start:(nullable RTCCallbackLoggerHandler)handler;
int64_t ReceivedTime() const { return ReceivedTimestamp().value_or(-1); }

// 3. Deprecated virtual overload remains pure virtual (= 0) or has inaccurate TODO:
// TODO(bugs.webrtc.org/1234): This can be virtual once old uses migrate.
[[deprecated("Use PacketOptions overload")]]
virtual bool SendRtcp(ArrayView<const uint8_t> packet) = 0;

// 4. Brand-new abstract interface uses dummy stub body instead of = 0:
virtual RTCStatsType StatsType() const { RTC_CHECK_NOTREACHED(); }
```

Good:

```cpp
// 1. Temporary default stub with explicit 'make pure virtual' tracking TODO:
// TODO(bugs.webrtc.org/1234): Make pure virtual once all subclasses migrate.
virtual std::unique_ptr<AsyncPacketSocket> CreateClientUdpSocket(
    const SocketAddress& local, const SocketAddress& remote) {
  RTC_DCHECK_NOTREACHED();
  return nullptr;
}

// 2. Retain SDK signature with deprecation tag alongside new overload:
- (void)start:(nullable RTCCallbackLoggerMessageHandler)handler;
- (void)startWithSeverity:(nullable RTCCallbackLoggerMessageAndSeverityHandler)h;

// 3. Mutual delegation between virtual overloads with stateless fallbacks:
virtual bool SendRtcp(ArrayView<const uint8_t> p, const PacketOptions& /*opt*/) {
  return SendRtcp(p);
}
[[deprecated("Pass PacketOptions")]]
// TODO(bugs.webrtc.org/1234): Remove once downstream migrates.
virtual bool SendRtcp(ArrayView<const uint8_t> p) {
  return SendRtcp(p, /*opt=*/{});
}

// 4. Brand-new abstract interface declares contract as pure virtual (= 0):
virtual RTCStatsType StatsType() const = 0;
```
