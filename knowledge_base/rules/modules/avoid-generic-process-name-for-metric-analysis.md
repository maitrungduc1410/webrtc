---
name: webrtc/modules/avoid-generic-process-name-for-metric-analysis
description: In audio processing components, reserve Process for mutating audio buffers; name metric, level, state, or read-only routines Analyze, Recommend, Update, or Handle
rubric: readability
severity: should_fix
---

**Rule.** In audio processing submodules and controllers (such as
InputVolumeController, AGC, or VAD), reserve Process() or PreProcess() for
methods that perform signal transformations directly on mutable audio frame
buffers. Do not name methods Process(), PreProcess(), or AnalyzePreProcess()
when they only update internal controller state, calculate metrics/statistics,
inspect levels, or analyze read-only audio buffers (const AudioBuffer&).
Instead, use descriptive action names reflecting their specific role and data
flow, such as AnalyzeInputAudio(), RecommendInputVolume(),
UpdateRecommendedInputVolume(), EstimateLevel(), or Handle...().

**Why.** In audio processing pipelines, Process() conventionally signifies
signal processing and data transformation on audio frame buffers. Using generic
processing terms for non-mutating routines that merely update internal state,
compute metrics, or inspect read-only audio buffers obscures the audio buffer
lifecycle, makes signal data flow ambiguous, and misleads callers about
potential side effects.

## Examples

Bad:

```cpp
// Misleading: does not transform audio buffers; only inspects read-only buffers or updates metrics/state.
void Process(std::optional<float> speech_probability,
             std::optional<float> speech_level_dbfs);
void Process(std::optional<int> rms_error_override);
void AnalyzePreProcess(const AudioBuffer& audio_buffer);
```

Good:

```cpp
// Descriptively named based on the computed recommendation, state update, or read-only stage.
std::optional<int> RecommendInputVolume(
    float speech_probability,
    std::optional<float> speech_level_dbfs);
void UpdateRecommendedInputVolume(std::optional<int> rms_error_override);
void AnalyzeInputAudio(int applied_input_volume,
                       const AudioBuffer& audio_buffer);
```
