---
name: webrtc/sdk/objc-assert-nonnull-parameters
description: In Objective-C method implementations and native observer adapters, assert non-null preconditions at entry for required parameters and completion handler blocks using RTC_DCHECK
rubric: correctness
severity: should_fix
---

**Rule.** In Objective-C and Objective-C++ method implementations (.m and .mm)
and native observer adapter wrappers, assert that required parameters and
callback blocks declared as non-null (such as those enclosed by
NS_ASSUME_NONNULL_BEGIN) are non-nil at method entry using RTC_DCHECK(param) or
RTC_DCHECK(param != nil). Do not omit runtime precondition checks when accepting
required model/configuration objects or adapting, storing, and invoking
non-nullable completion handlers.

**Why.** Objective-C nullability annotations are compile-time hints for Swift
interoperability and static analysis, but they are not enforced at runtime and
do not prevent callers from passing nil. Invoking a nil completion block or
passing nil into native C++ observer adapters causes silent failures, no-op
message dispatches, or runtime crashes. Asserting non-null preconditions with
RTC_DCHECK at method entry catches unsupplied arguments early in debug builds
before reaching native layers.

## Examples

Bad:

```cpp
- (void)addIceCandidate:(RTC_OBJC_TYPE(RTCIceCandidate) *)candidate
      completionHandler:(RTCCallbackBlock)completionHandler {
  // Missing RTC_DCHECK for required candidate and completionHandler.
  rtc::scoped_refptr<webrtc::CreateSessionDescriptionObserverAdapter>
      observer(new rtc::RefCountedObject
          <webrtc::CreateSessionDescriptionObserverAdapter>(completionHandler));
  _peerConnection->AddIceCandidate(candidate.nativeCandidate, observer.get());
}
```

Good:

```cpp
- (void)addIceCandidate:(RTC_OBJC_TYPE(RTCIceCandidate) *)candidate
      completionHandler:(RTCCallbackBlock)completionHandler {
  RTC_DCHECK(candidate != nil);
  RTC_DCHECK(completionHandler != nil);
  rtc::scoped_refptr<webrtc::CreateSessionDescriptionObserverAdapter>
      observer(new rtc::RefCountedObject
          <webrtc::CreateSessionDescriptionObserverAdapter>(completionHandler));
  _peerConnection->AddIceCandidate(candidate.nativeCandidate, observer.get());
}
```
