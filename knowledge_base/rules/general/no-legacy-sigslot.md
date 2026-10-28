---
name: webrtc/no-legacy-sigslot
description: When implementing event handling, callbacks, or socket listeners, do not use legacy sigslot; use Subscribe* methods, CallbackList, or absl::AnyInvocable instead
rubric: design
severity: blocking
---

**Rule.** Do not introduce new usages of legacy sigslot (such as inheriting from
sigslot::has_slots<>, declaring sigslot::signal*, or calling .connect()). When
listening to events on transports, rtc_base::Socket, AsyncSocket, or socket
adapters (SSLAdapter, OpenSSLAdapter), use dedicated Subscribe* methods (e.g.,
SubscribeCandidateGathered, SubscribeReadEvent, SubscribeWriteEvent,
SubscribeConnectEvent, SubscribeCloseEvent) or modern callback mechanisms such
as webrtc::CallbackList, absl::AnyInvocable, and std::bind_front rather than
connecting to Signal* or Signal*Event member variables.

**Why.** WebRTC has actively deprecated and eliminated sigslot across the
codebase, including networking, socket, and transport layers. Using Subscribe* /
Unsubscribe* methods with lambdas, std::bind_front, or CallbackList decouples
callers from sigslot::has_slots inheritance, simplifies object lifetime
management, and aligns with modern C++ callable conventions.

## Examples

Bad:

```cpp
// Legacy sigslot inheritance and Signal*.connect usage:
class DatagramConnectionInternal : public sigslot::has_slots<> {
  void Init(IceTransport* ice, SSLAdapter* ssl) {
    ice->SignalCandidateGathered.connect(
        this, &DatagramConnectionInternal::OnCandidateGathered);
    ssl->SignalConnectEvent.connect(
        this, &DatagramConnectionInternal::OnConnectEvent);
  }
};
```

Good:

```cpp
// Modern Subscribe* methods without sigslot::has_slots:
class DatagramConnectionInternal {
  void Init(IceTransport* ice, SSLAdapter* ssl) {
    ice->SubscribeCandidateGathered(
        this, std::bind_front(&DatagramConnectionInternal::OnCandidateGathered, this));
    ssl->SubscribeConnectEvent(
        this, [this](Socket* s) { OnConnectEvent(s); });
  }
};
```
