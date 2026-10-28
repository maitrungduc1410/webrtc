---
name: webrtc/modules/check-scalability-mode-before-legacy-codec-structs
description: When reading or modifying VideoCodec layer configurations, query GetScalabilityMode() before legacy structs and update SetScalabilityMode when culling spatial layers
rubric: correctness
severity: should_fix
---

**Rule.** When determining layer counts from VideoCodec or SimulcastStream,
query codec.GetScalabilityMode() first (using ScalabilityModeToNumTemporalLayers
or ScalabilityModeToNumSpatialLayers) before falling back to legacy structs
(e.g., VP8().numberOfTemporalLayers). When modifying or culling spatial layers
in VideoCodec configurations, update SetScalabilityMode (e.g., using
LimitNumSpatialLayers) so scalability structures remain consistent with the
culled layer count.

**Why.** Modern codecs (such as AV1 and VP9 WebRTC-SVC) define stream structure
via ScalabilityMode rather than legacy codec structs. Querying only legacy
structs misses modern scalability modes, while modifying spatial layer counts
without updating SetScalabilityMode causes downstream encoders and rate
allocators to use an inconsistent scalability structure.

## Examples

Bad:

```cpp
// Bad (reading): Querying legacy struct directly
num_temporal_layers = codec.VP9().numberOfTemporalLayers;

// Bad (writing): Modifying spatial layer count without updating ScalabilityMode
case kVideoCodecVP9:
  culled_config.VP9()->numberOfSpatialLayers =
      highest_active_spatial_layer_id + 1;
  break;
```

Good:

```cpp
// Good (reading): Query ScalabilityMode before legacy struct
if (auto scalability_mode = codec.GetScalabilityMode(); scalability_mode) {
  num_temporal_layers = ScalabilityModeToNumTemporalLayers(*scalability_mode);
} else {
  num_temporal_layers = codec.VP9().numberOfTemporalLayers;
}

// Good (writing): Update ScalabilityMode with LimitNumSpatialLayers
case kVideoCodecVP9: {
  culled_config.VP9()->numberOfSpatialLayers = reduced_num_spatial_layers;
  if (auto scalability_mode = config.GetScalabilityMode(); scalability_mode) {
    culled_config.SetScalabilityMode(LimitNumSpatialLayers(
        *scalability_mode, reduced_num_spatial_layers));
  }
} break;
```
