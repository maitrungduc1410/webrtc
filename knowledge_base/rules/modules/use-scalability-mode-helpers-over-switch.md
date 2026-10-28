---
name: webrtc/modules/use-scalability-mode-helpers-over-switch
description: When querying, adapting, or constructing ScalabilityMode values, use scalability_mode_util helpers instead of manual switch statements, string mutations, or string re-parsing
rubric: readability
severity: should_fix
---

**Rule.** When inspecting, transforming, or constructing ScalabilityMode values
(such as querying temporal/spatial layer counts, limiting spatial layers, or
extracting inter-layer prediction modes), use dedicated helper functions from
modules/video_coding/svc/scalability_mode_util.h (e.g.,
ScalabilityModeToNumSpatialLayers, ScalabilityModeToNumTemporalLayers,
ScalabilityModeToInterLayerPredMode, LimitNumSpatialLayers,
ScalabilityModeToResolutionRatio, or MakeScalabilityMode). Avoid manual switch
statements over individual ScalabilityMode enum cases as well as converting
enums to strings (ScalabilityModeToString) for slicing, character mutation, and
re-parsing (ScalabilityModeStringToEnum).

**Why.** Manual switch statements over ScalabilityMode enumerators are verbose,
fragile, and bypass compiler exhaustiveness checks when new modes are added.
Converting ScalabilityMode to strings for manual slicing and re-parsing is
error-prone across different mode naming schemes (such as shift or key-frame
suffixes) and loses type safety. Centralized scalability_mode_util.h helpers
provide safe, concise, and tested transformations across all supported
scalability structures.

## Examples

Bad:

```cpp
// Variant 1: Manual switch statements over enumerators
if (codec_specific.scalability_mode) {
  switch (*codec_specific.scalability_mode) {
    case ScalabilityMode::kL1T1:
    case ScalabilityMode::kS2T1:
    case ScalabilityMode::kS3T1:
      codec_specific.scalability_mode = ScalabilityMode::kL1T1;
      break;
    default:
      break;
  }
}

// Variant 2: String mutation and re-parsing
std::string mode_str = ScalabilityModeToString(*config.GetScalabilityMode());
mode_str[1] = '1' + highest_active_spatial_layer_id;
culled_config.SetScalabilityMode(*ScalabilityModeStringToEnum(mode_str));
```

Good:

```cpp
// Variant 1: Dedicated query and construction helper
if (codec_specific.scalability_mode) {
  int num_temporal =
      ScalabilityModeToNumTemporalLayers(*codec_specific.scalability_mode);
  if (num_temporal == 1) {
    codec_specific.scalability_mode = ScalabilityMode::kL1T1;
  }
}

// Variant 2: Dedicated layer limiting helper
culled_config.SetScalabilityMode(LimitNumSpatialLayers(
    *config.GetScalabilityMode(), highest_active_spatial_layer_id + 1));
```
