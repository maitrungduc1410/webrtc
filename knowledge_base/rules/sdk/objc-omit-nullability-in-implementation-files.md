---
name: webrtc/sdk/objc-omit-nullability-in-implementation-files
description: In Objective-C NS_ASSUME_NONNULL blocks, annotate only nullable pointers and error out-parameters with _Nullable in headers, omitting redundant nonnull and implementation annotations
rubric: readability
severity: should_fix
---

**Rule.** In Objective-C and Objective-C++ headers enclosed by
NS_ASSUME_NONNULL_BEGIN and NS_ASSUME_NONNULL_END, explicitly annotate only
pointers that accept or return nil (including method parameters, return types,
properties, and error out-parameters like NSError **_Nullable) with nullable or
_Nullable. Do not add redundant nonnull or _Nonnull annotations within
NS_ASSUME_NONNULL blocks. Additionally, omit all nullability annotations
(nullable, nonnull, _Nullable, _Nonnull) from method definitions in
implementation files (.m and .mm).

**Why.** Within NS_ASSUME_NONNULL regions, the compiler implicitly defaults all
unannotated pointer types to non-null for Swift bridging and static analysis,
making explicit nonnull or _Nonnull annotations redundant boilerplate. Pointers
accepting or returning nil (such as optional error out-pointers) must be
explicitly marked _Nullable so Swift and analyzers do not treat them as
required. Repeating nullability annotations in .m or .mm implementation files is
non-idiomatic, redundant with header declarations, and adds unnecessary
maintenance overhead.

## Examples

Bad:

```cpp
// In Header.h (enclosed by NS_ASSUME_NONNULL_BEGIN)
- (nonnull RTC_OBJC_TYPE(RTCVideoEncoderCodecSupport) *)
    queryCodecSupport:(nonnull RTC_OBJC_TYPE(RTCVideoCodecInfo) *)info
      scalabilityMode:(nullable NSString *)scalabilityMode;
- (BOOL)setCodecPreferences:(NSArray<RTC_OBJC_TYPE(RTCRtpCodecCapability) *> *)codecs
                      error:(NSError **)error;

// In Implementation.mm
- (instancetype)initWithAudioDeviceModule:
    (nullable webrtc::AudioDeviceModule *)audioDeviceModule { ... }
```

Good:

```cpp
// In Header.h (enclosed by NS_ASSUME_NONNULL_BEGIN)
- (RTC_OBJC_TYPE(RTCVideoEncoderCodecSupport) *)
    queryCodecSupport:(RTC_OBJC_TYPE(RTCVideoCodecInfo) *)info
      scalabilityMode:(nullable NSString *)scalabilityMode;
- (BOOL)setCodecPreferences:(NSArray<RTC_OBJC_TYPE(RTCRtpCodecCapability) *> *)codecs
                      error:(NSError **_Nullable)error;

// In Implementation.mm
- (instancetype)initWithAudioDeviceModule:
    (webrtc::AudioDeviceModule *)audioDeviceModule { ... }
```
