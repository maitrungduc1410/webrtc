---
name: webrtc/sdk/check-responds-to-selector-when-evolving-objc-protocols
description: When invoking optional or evolving Objective-C protocol and delegate methods, check [target respondsToSelector:@selector(...)] before calling to prevent unrecognized selector crashes
rubric: correctness
severity: should_fix
---

**Rule.** In Objective-C and Objective-C++ callers, delegate adapters, and
protocol wrappers, verify `[target respondsToSelector:@selector(...)]` before
invoking `@optional` or newly introduced protocol or delegate methods. When
evolving an existing protocol with a replacement method, check if the conforming
object responds to the new selector and provide a fallback call to the legacy
deprecated method (suppressing deprecation warnings with `#pragma clang
diagnostic ignored "-Wdeprecated-declarations"`) to maintain backward
compatibility during migration.

**Why.** Objective-C protocol methods are dynamically dispatched at runtime.
Sending an unhandled selector to a delegate, observer, or conforming object that
has not implemented an `@optional` or newly introduced method triggers an
`NSInvalidArgumentException` (unrecognized selector sent to instance) runtime
crash. Checking `respondsToSelector:` prevents crashes on optional delegate
callbacks and ensures safe rolling migrations when evolving protocols.

## Examples

Bad:

```cpp
// Bad (delegate): Sending optional delegate message without respondsToSelector: check
[peer_connection.delegate peerConnection:peer_connection
                 didChangeLocalCandidate:local_candidate
                didChangeRemoteCandidate:remote_candidate
                          lastReceivedMs:last_data_received_ms
                           didHaveReason:nsstr_reason];

// Bad (protocol evolution): Calling new method directly without checking or fallback
if ([decoder conformsToProtocol:@protocol(RTC_OBJC_TYPE(RTCNativeVideoDecoderBuilder))]) {
  id<RTC_OBJC_TYPE(RTCNativeVideoDecoderBuilder)> builder =
      (id<RTC_OBJC_TYPE(RTCNativeVideoDecoderBuilder)>)decoder;
  return [builder buildWithEnvironment:env format:format];
}
```

Good:

```cpp
// Good (delegate): Verify respondsToSelector: before calling optional delegate method
if ([peer_connection.delegate
        respondsToSelector:@selector(peerConnection:
                                didChangeLocalCandidate:didChangeRemoteCandidate:
                                         lastReceivedMs:didHaveReason:)]) {
  [peer_connection.delegate peerConnection:peer_connection
                   didChangeLocalCandidate:local_candidate
                  didChangeRemoteCandidate:remote_candidate
                            lastReceivedMs:last_data_received_ms
                             didHaveReason:nsstr_reason];
}

// Good (protocol evolution): Check respondsToSelector: and fall back to legacy method
if ([decoder conformsToProtocol:@protocol(RTC_OBJC_TYPE(RTCNativeVideoDecoderBuilder))]) {
  id<RTC_OBJC_TYPE(RTCNativeVideoDecoderBuilder)> builder =
      (id<RTC_OBJC_TYPE(RTCNativeVideoDecoderBuilder)>)decoder;
  if ([builder respondsToSelector:@selector(buildWithEnvironment:format:)]) {
    return [builder buildWithEnvironment:env format:format];
  }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  return [builder build:env];
#pragma clang diagnostic pop
}
```
