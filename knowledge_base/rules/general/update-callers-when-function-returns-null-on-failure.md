---
name: webrtc/update-callers-when-function-returns-null-on-failure
description: When modifying a function or factory to return null on failure (e.g. adding error out-parameters), preserve return contracts or update all callers
rubric: correctness
severity: blocking
---

**Rule.** When modifying a function, factory, constructor wrapper, or conversion
helper to return null (`nullptr`, `nullopt`, or null handle) on failure, such as
when introducing early failure exits or adding an optional error out-parameter
(`int* error = nullptr` or RTCError*), ensure all existing callers are audited
and updated to handle the null return safely. If existing callers cannot be
migrated or expect a non-null object (even wrapping an invalid internal state),
preserve the existing return value contract on failure rather than returning
null.

**Why.** Callers written against previous function contracts assume non-null
return values and will suffer null-pointer dereference crashes, null handle JNI
errors, or downstream invariant violations if a function starts returning null
unexpectedly. When adding optional error parameters or failure branches,
unmigrated call sites using default arguments or assuming guaranteed object
construction must be updated to check for null, or the function must maintain
its non-null return contract.

## Examples

Bad:

```cpp
// 1. Modifying a conversion helper to return null without updating callers:
ScopedJavaLocalRef<jobject> NativeToJavaVideoFrame(JNIEnv* jni,
                                                   const VideoFrame& frame) {
  auto i420_buffer = frame.video_frame_buffer()->ToI420();
  if (!i420_buffer) {
    return nullptr;
  }
  return Java_VideoFrame_Constructor(jni, ...);
}

// Caller not updated to handle null:
ScopedJavaLocalRef<jobject> j_frame = NativeToJavaVideoFrame(jni, frame);
Java_VideoEncoder_encode(jni, encoder_, j_frame, encode_info);

// 2. Modifying a factory with an optional error out-parameter to return nullptr:
std::unique_ptr<IvfFileWriter> IvfFileWriter::Wrap(absl::string_view filename,
                                                   size_t byte_limit,
                                                   int* error /*=nullptr*/) {
  FileWrapper ivf_file = FileWrapper::OpenWriteOnly(filename, error);
  // BAD: Alters return contract from non-null wrapper to nullptr on failure.
  if (error && *error != 0)
    return nullptr;
  return std::unique_ptr<IvfFileWriter>(
      new IvfFileWriter(std::move(ivf_file), byte_limit));
}
```

Good:

```cpp
// 1. Caller updated to check for null and handle failure safely:
ScopedJavaLocalRef<jobject> j_frame = NativeToJavaVideoFrame(jni, frame);
if (!j_frame) {
  RTC_LOG(LS_WARNING) << "NativeToJavaVideoFrame failed. Dropping frame.";
  return WEBRTC_VIDEO_CODEC_OK;
}
Java_VideoEncoder_encode(jni, encoder_, j_frame, encode_info);

// 2. Preserving return contract when adding an optional error out-parameter:
std::unique_ptr<IvfFileWriter> IvfFileWriter::Wrap(absl::string_view filename,
                                                   size_t byte_limit,
                                                   int* error /*=nullptr*/) {
  // GOOD: Preserves return contract; callers expecting an instance still receive one.
  return std::unique_ptr<IvfFileWriter>(new IvfFileWriter(
      FileWrapper::OpenWriteOnly(filename, error), byte_limit));
}
```
