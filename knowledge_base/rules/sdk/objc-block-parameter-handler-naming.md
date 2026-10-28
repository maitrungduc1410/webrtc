---
name: webrtc/sdk/objc-block-parameter-handler-naming
description: 'When declaring Objective-C methods taking block callbacks, name selectors with WithCompletionHandler: or Handler/Callback suffixes rather than bare action names or callback parameter names'
rubric: readability
severity: should_fix
---

**Rule.** In Objective-C method declarations, name selector arguments accepting
block callbacks with explicit ...Handler:, ...Callback:, or
...WithCompletionHandler: suffixes. When an asynchronous method takes only a
completion block, name the selector ...WithCompletionHandler: rather than using
a bare action name (e.g., setLocalDescriptionWithCompletionHandler: instead of
setLocalDescription:), and never name selector arguments after the parameters
that the block receives (e.g., startWithMessageAndSeverityHandler: instead of
startWithSeverity:).

**Why.** In Objective-C/Cocoa conventions, selector keywords describe their
corresponding arguments at call sites. Using bare action names for completion
blocks misleadingly implies passing entity objects, and naming selector pieces
after a block's parameters implies passing scalar or enum configuration values
rather than an asynchronous callback block.

## Examples

Bad:

```cpp
// Misleading bare action selector implies passing an RTCSessionDescription object.
- (void)setLocalDescription:(nullable void (^)(NSError *_Nullable error))completionHandler;

// Misleading selector implies passing an RTCLoggingSeverity enum value.
- (void)startWithSeverity:(nullable CallbackWithSeverityLoggerHandler)callback;
```

Good:

```cpp
// Selector clearly communicates that the argument is a completion handler.
- (void)setLocalDescriptionWithCompletionHandler:
    (nullable void (^)(NSError *_Nullable error))completionHandler;

// Selector clearly communicates that the argument is a callback handler.
- (void)startWithMessageAndSeverityHandler:
    (nullable RTCCallbackLoggerMessageAndSeverityHandler)handler;
```
