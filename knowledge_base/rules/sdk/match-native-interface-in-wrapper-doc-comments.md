---
name: webrtc/sdk/match-native-interface-in-wrapper-doc-comments
description: When documenting SDK wrapper classes or native pointer handles in Javadoc, accurately specify the C++ interface type, nullability, and ownership semantics
rubric: readability
severity: should_fix
---

**Rule.** When adding or updating Javadoc comments on platform SDK wrapper
classes or native pointer handles (such as class-level descriptions, long
handles in factory methods, or getNative...() accessors), ensure referenced C++
type names (e.g., webrtc::RtpReceiverInterface) accurately match the wrapped
object. Additionally, explicitly document nullability (non-null vs. nullable)
and ownership semantics (such as non-owning pointer or owning scoped_refptr) to
match the native contract and implementation guarantees.

**Why.** Inaccurate C++ type names, nullability annotations, or ownership
contracts in wrapper documentation mislead maintainers and SDK callers regarding
the underlying native C++ interface, lifetime requirements, and whether null
returns need handling, complicating JNI bridge maintenance and debugging.

## Examples

Bad:

```cpp
/** Java wrapper for a C++ RtpReceiverInterface. */
public class RtpReceiver {
  /** Returns a pointer to webrtc::RtpSenderInterface. */
  long getNativeRtpReceiver() {
    return nativeRtpReceiver;
  }
}

/**
 * Dynamically allocates a {@code webrtc::AudioProcessing} instance.
 *
 * @param webrtcEnvRef non-null non-owning pointer to {@code webrtc::Environment}
 * @return nullable owning scoped_refptr pointer to {@code webrtc::AudioProcessing}
 */
long createNative(long webrtcEnvRef);
```

Good:

```cpp
/** Java wrapper for a C++ RtpReceiverInterface. */
public class RtpReceiver {
  /** Returns a pointer to webrtc::RtpReceiverInterface. */
  long getNativeRtpReceiver() {
    return nativeRtpReceiver;
  }
}

/**
 * Dynamically allocates a {@code webrtc::AudioProcessing} instance.
 *
 * @param webrtcEnvRef non-null non-owning pointer to {@code webrtc::Environment}
 * @return non-null owning scoped_refptr pointer to {@code webrtc::AudioProcessing}
 */
long createNative(long webrtcEnvRef);
```
