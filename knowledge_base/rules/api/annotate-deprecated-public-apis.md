---
name: webrtc/api/annotate-deprecated-public-apis
description: When deprecating public API or SDK declarations, annotate legacy methods, preserve shim signatures, provide interface defaults, and mark replacement protocol methods @optional
rubric: design
severity: blocking
---

**Rule.** When deprecating or superseding public API or SDK declarations (in
api/, sdk/, or rtc_base/), annotate legacy declarations with [[deprecated]] (in
C++), @Deprecated (in Java), or RTC_OBJC_DEPRECATED / DEPRECATED_MSG_ATTRIBUTE
(in Objective-C) citing the replacement API. Do not modify the signatures of
existing public methods; retain legacy methods as signature-preserving shims
delegating to new implementations. For Java SDK interfaces, provide a default
method implementation (e.g., throwing UnsupportedOperationException). For
Objective-C protocols, mark replacement methods @optional (conforming to
<NSObject>) and check respondsToSelector: at call sites with a fallback to the
deprecated method until implementations migrate.

**Why.** Downstream projects and external applications rely on existing public
signatures and cannot be updated atomically. Modifying legacy signatures or
omitting deprecation annotations breaks callers and implementers during rolling
updates, while interface defaults, shims, and @optional protocol methods with
runtime fallbacks allow safe, non-breaking downstream migrations.

## Examples

Bad:

```cpp
// C++/ObjC class: Modifying signature while deprecating breaks callers & overrides
- (BOOL)addIceCandidate:(RTC_OBJC_TYPE(RTCIceCandidate) *)candidate
    DEPRECATED_MSG_ATTRIBUTE("Please use addIceCandidate:completionHandler: instead");

// Java interface: Abstract deprecated method without default implementation
@Deprecated
@Nullable VideoCodecInfo onEncodingBitrate(int kbps);

// ObjC protocol: Adding replacement method as required breaks conforming classes
@protocol RTC_OBJC_TYPE(RTCNativeVideoDecoderBuilder)
- (std::unique_ptr<webrtc::VideoDecoder>)build:(const webrtc::Environment&)env
    RTC_OBJC_DEPRECATED("Use buildWithEnvironment:format: instead");
- (std::unique_ptr<webrtc::VideoDecoder>)buildWithEnvironment:(const webrtc::Environment&)env
                                                       format:(const webrtc::SdpVideoFormat&)format;
@end
```

Good:

```cpp
// C++/ObjC class: Preserve original signature on deprecated forwarding shim
- (void)addIceCandidate:(RTC_OBJC_TYPE(RTCIceCandidate) *)candidate
    DEPRECATED_MSG_ATTRIBUTE("Please use addIceCandidate:completionHandler: instead");

// Java interface: Provide default implementation throwing UnsupportedOperationException
@Deprecated @Nullable
default VideoCodecInfo onEncodingBitrate(int kbps) {
  throw new UnsupportedOperationException("Not implemented.");
}

// ObjC protocol: Mark replacement method @optional and adopt <NSObject>
@protocol RTC_OBJC_TYPE(RTCNativeVideoDecoderBuilder)<NSObject>
- (std::unique_ptr<webrtc::VideoDecoder>)build:(const webrtc::Environment&)env
    RTC_OBJC_DEPRECATED("Use buildWithEnvironment:format: instead");
@optional
- (std::unique_ptr<webrtc::VideoDecoder>)buildWithEnvironment:(const webrtc::Environment&)env
                                                       format:(const webrtc::SdpVideoFormat&)format;
@end
```
