---
name: webrtc/api/document-public-enum-values
description: When declaring public C++ API or SDK interfaces, document contracts, lifecycle, side effects, parameter ranges, field defaults, and C++ or platform API references in Javadoc
rubric: readability
severity: should_fix
---

**Rule.** When defining public C++ API (api/) or platform SDK (sdk/)
declarations (classes, interfaces, methods, enums, fields, or bridge intents),
document their semantics, parameters, return values, callback timing, lifecycle
constraints, dynamic side effects, and integration architecture on public types.

For fields and event structures, document population conditions and default or
sentinel values when unpopulated. For enums, document each constant; when
methods accept primitive types (such as integer constants or floats representing
platform dimensions, modes, or ranges) instead of strongly typed enums, document
expected values, valid ranges, and underlying platform constants. For SDK types
and methods abstracting platform features or mirroring C++ definitions, cite
corresponding platform APIs (e.g., android.media.*) or counterpart C++ headers
and types to guide implementers and keep synchronized. In Java SDK files, format
documentation comments using Javadoc blocks (/** ... */) rather than single-line
(//) comments.

**Why.** Public C++ headers and SDK interfaces form the external API boundary
for library embedders and platform bindings. Without explanatory comments
covering semantics, parameter ranges, valid constants, field population
conditions, default values, callback timing, lifecycle constraints, side
effects, and underlying platform APIs or counterpart C++ definitions, consumers
and implementers cannot determine contracts or maintain cross-language
consistency without inspecting internal native code. In Java SDK code, standard
Javadoc block comments (/** ... */) are required for parser and IDE support.

## Examples

Bad:

```cpp
// Undocumented enum constants and missing C++ counterpart reference:
public static enum ConnectionType {
  CONNECTION_UNKNOWN,
  CONNECTION_ETHERNET,
  CONNECTION_WIFI,
}

// Undocumented field population conditions and defaults:
public final class CandidatePairChangeEvent {
  public final int estimatedDisconnectedTimeMs;
}

// Undocumented primitive parameter representing platform constants/dimensions:
@RequiresApi(Build.VERSION_CODES.M)
public boolean setPreferredMicrophoneFieldDimension(float dimension) {
  return audioInput.setPreferredMicrophoneFieldDimension(dimension);
}

// Undocumented integer constants on interface method:
public void onNetworkPreference(List<ConnectionType> types, int preference);

// Ambiguous platform bridge intent documentation:
/** NetworkPreference is an intent sent from OS/firmware when signal is weak. */
```

Good:

```cpp
/**
 * Java equivalent of C++ android_network_monitor.h / NetworkType.
 * Keep in sync with native NetworkType enum.
 */
public static enum ConnectionType {
  /** Unknown network connection. */
  CONNECTION_UNKNOWN,
  /** Ethernet wired connection. */
  CONNECTION_ETHERNET,
  /** Wi-Fi wireless connection. */
  CONNECTION_WIFI,
}

public final class CandidatePairChangeEvent {
  /**
   * Estimated disconnect duration in ms before switching candidate pairs.
   * Equal to 0 when the candidate pair is first signaled.
   */
  public final int estimatedDisconnectedTimeMs;
}

/**
 * Sets the preferred field dimension for the built-in microphone.
 * See android.media.MicrophoneDirection#setPreferredMicrophoneFieldDimension.
 */
public boolean setPreferredMicrophoneFieldDimension(float dimension) {
  return audioInput.setPreferredMicrophoneFieldDimension(dimension);
}

/**
 * Called when receiving a NetworkPreference intent where network preference
 * is |CONNECTION_PREFERENCE_NOT_PREFERRED| or |CONNECTION_PREFERENCE_NEUTRAL|.
 */
public void onNetworkPreference(List<ConnectionType> types, int preference);

/** Platform intent bridging OS/firmware network signal info to WebRTC. */
```
