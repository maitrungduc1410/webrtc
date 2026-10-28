---
name: webrtc/sdk/objc-group-optional-protocol-methods-at-bottom
description: When declaring Objective-C delegate protocols, place new callbacks under @optional for backward compatibility and group all @optional methods at the bottom
rubric: readability
severity: should_fix
---

**Rule.** In Objective-C protocol declarations (such as delegate or observer
protocols), group all @optional methods together in a single @optional section
at the bottom of the protocol instead of interleaving @optional and @required
directives. Additionally, when adding new callback methods to existing public
protocols, declare them as @optional to maintain backward compatibility for
existing conforming implementations.

**Why.** In Objective-C protocols, methods are @required by default. Grouping
@optional methods at the bottom avoids repetitive directive switching and keeps
the required interface contract distinct. Placing new delegate callbacks under
@optional ensures existing downstream implementations continue to compile and
work without breaking changes.

## Examples

Bad:

```cpp
@protocol RTCPeerConnectionDelegate <NSObject>
- (void)peerConnection:(RTCPeerConnection *)peerConnection
    didChangeIceConnectionState:(RTCIceConnectionState)newState;

// Bad: New callback added as required, breaking existing downstream implementors.
- (void)peerConnection:(RTCPeerConnection *)peerConnection
    didChangeIceGatheringState:(RTCIceGatheringState)newState;

// Bad: Interleaving @optional and @required directives.
@optional
- (void)peerConnection:(RTCPeerConnection *)peerConnection
    didChangeConnectionState:(RTCPeerConnectionState)newState;

@required
- (void)peerConnection:(RTCPeerConnection *)peerConnection
    didGenerateIceCandidate:(RTCIceCandidate *)candidate;
@end
```

Good:

```cpp
@protocol RTCPeerConnectionDelegate <NSObject>
- (void)peerConnection:(RTCPeerConnection *)peerConnection
    didChangeIceConnectionState:(RTCIceConnectionState)newState;
- (void)peerConnection:(RTCPeerConnection *)peerConnection
    didGenerateIceCandidate:(RTCIceCandidate *)candidate;

@optional
// All optional and newly added callbacks grouped together at the bottom.
- (void)peerConnection:(RTCPeerConnection *)peerConnection
    didChangeConnectionState:(RTCPeerConnectionState)newState;
- (void)peerConnection:(RTCPeerConnection *)peerConnection
    didChangeIceGatheringState:(RTCIceGatheringState)newState;
@end
```
