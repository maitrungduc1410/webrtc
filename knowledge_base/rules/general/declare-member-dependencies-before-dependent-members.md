---
name: webrtc/declare-member-dependencies-before-dependent-members
description: When declaring class data members, declare threads, task queues, callbacks, and foundational dependencies before dependent objects to ensure safe reverse-declaration destruction order
rubric: safety
severity: blocking
---

**Rule.** Declare class data members in dependency order: place execution
contexts (such as `rtc::Thread` or `webrtc::TaskQueueBase`), callbacks (such as
`absl::AnyInvocable` or base::RepeatingCallback), and foundational objects
before dependent members. Any member object that invokes, references, or
dispatches tasks to them (such as RTCPSender, DtlsSrtpTransport, or
PeerConnectionInterface) must appear afterward. In constructors, initialize
members in that same declaration order in initializer lists.

**Why.** In C++, non-static class data members are initialized in declaration
order and destroyed in reverse declaration order. If a dependent member is
declared before an object, callback, or thread it relies on, the dependency will
be destroyed first during teardown. Any destructor, task, or callback executing
during object destruction will access dangling references or dispatch to
terminated threads, causing use-after-free bugs and crashes.

## Examples

Bad:

```cpp
class Conductor {
 private:
  // BAD: Dependent objects and callers declared before their dependencies/threads.
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  RTCPSender rtcp_sender_;
  const std::unique_ptr<DtlsSrtpTransport> dtls_srtp_transport_;

  const scoped_refptr<DtlsTransport> dtls_transport_;
  absl::AnyInvocable<uint32_t() const> recv_ssrc_callback_;
  std::unique_ptr<rtc::Thread> signaling_thread_;
};
```

Good:

```cpp
class Conductor {
 private:
  // GOOD: Threads, callbacks, and dependencies declared before dependent objects.
  std::unique_ptr<rtc::Thread> signaling_thread_;
  absl::AnyInvocable<uint32_t() const> recv_ssrc_callback_;
  const scoped_refptr<DtlsTransport> dtls_transport_;

  const std::unique_ptr<DtlsSrtpTransport> dtls_srtp_transport_;
  RTCPSender rtcp_sender_;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
};
```
