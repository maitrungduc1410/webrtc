---
name: webrtc/modules/guard-empty-transport-feedback-before-delay-update
description: When processing TransportPacketsFeedback in congestion controllers, treat empty feedback as a normal no-op early return and omit defensive infinite feedback_time checks
rubric: readability
severity: should_fix
---

**Rule.** When processing transport feedback messages (such as
`TransportPacketsFeedback`) in congestion controllers, treat empty received
packet data (e.g. via `msg.PacketsWithFeedback().empty()` or
`msg.ReceivedWithSendInfo().empty()`) as a normal no-op early return without
updating delay metrics or logging warnings. Do not add redundant defensive
checks for `msg.feedback_time.IsInfinite()`, as `feedback_time` is the local
arrival timestamp and is guaranteed to be finite for incoming feedback.

**Why.** In transport feedback protocols (such as RFC 8888), feedback reports
containing only lost packets or keepalives are protocol-valid, so treating them
as invalid or updating delay metrics corrupts estimators and spams logs.
Conversely, `feedback_time` records the local reception clock time and is always
finite, making defensive infinity checks redundant.

## Examples

Bad:

```cpp
// Bad: Conflating empty feedback with invalid data, missing early returns, or checking feedback_time infinity.
bool ScreamV2::IsFeedbackValid(const TransportPacketsFeedback& msg) const {
  if (msg.ReceivedWithSendInfo().empty()) {
    RTC_LOG(LS_WARNING) << "Received feedback without any received packets.";
    return false;
  }
  if (msg.feedback_time.IsInfinite()) {
    return false;
  }
  return true;
}

void DelayController::OnTransportPacketsFeedback(const TransportPacketsFeedback& msg) {
  last_smoothed_rtt_ = msg.smoothed_rtt;
  for (const PacketResult& packet : msg.SortedByReceiveTime()) {
    UpdateDelayEstimates(packet.receive_time - packet.sent_packet.send_time);
  }
}
```

Good:

```cpp
// Good: Treat empty feedback as a normal no-op return; use feedback_time directly.
void DelayController::OnTransportPacketsFeedback(const TransportPacketsFeedback& msg) {
  if (msg.ReceivedWithSendInfo().empty()) {
    return;
  }
  last_smoothed_rtt_ = msg.smoothed_rtt;
  last_feedback_time_ = msg.feedback_time;
  for (const PacketResult& packet : msg.SortedByReceiveTime()) {
    UpdateDelayEstimates(packet.receive_time - packet.sent_packet.send_time);
  }
}
```
