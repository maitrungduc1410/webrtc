---
name: webrtc/modules/hoist-channel-invariant-computations
description: Hoist loop-invariant calculations, buffer lengths, lookup tables, and helper lambdas outside iteration loops or cache them on state updates.
rubric: safety
severity: should_fix
---

**Rule.** Hoist loop-invariant expressions, buffer lengths, lookup tables, and
helper lambdas out of iteration loops (including multichannel audio processing
loops and per-element filtering helpers). Precompute shared tables (such as
random sine indices or window coefficients) and immutable values outside the
loop, qualify them as const, and pass them into loop iterations rather than
recalculating them per iteration. When an invariant calculation depends only on
configuration or state updates, precompute and cache it in member variables
instead of evaluating it repeatedly in predicate helpers (such as ShouldNack).
Keep helper lambdas inside loop bodies only when they capture per-iteration
variables that change on every iteration.

**Why.** Recalculating loop-invariant expressions, buffer lengths, config
conversions, or shared tables (such as random sine table indices) and
reconstructing invariant lambda closures on every iteration introduces
unnecessary CPU overhead and visual clutter in latency-sensitive audio
pipelines, hot media processing loops, and packet routing paths. Precomputing
shared tables, hoisting invariant constants, caching values on state changes,
and defining closures once clarifies variable lifetimes and eliminates redundant
computation.

## Examples

Bad:

```cpp
// Bad: Redefining invariant helper lambda inside loop
for (size_t i = 1; i < kFftLengthBy2Plus1; ++i) {
  auto downsample = [kFactor](const float* m) { return *std::max_element(m, m + kFactor); };
  downsampled[i] = downsample(&mask[kFactor * (i - 1) + 1]);
}

// Bad: Recomputing invariant length and tables in multichannel loop
for (size_t ch = 0; ch < num_channels_; ++ch) {
  size_t temp_len = current_lag + overlap_length_;
  GenerateComfortNoise(opt_, N2[ch], &seed_[ch], &noise[ch]);
}

// Bad: Recalculating invariant threshold per packet in filter helper
bool NackTracker::ShouldNack(const NackElement& packet, int64_t rtt_ms) {
  int max_wait_ms = 100.0 * config_.loss_rate * rtt_ms / (1 << 30);
  return packet.time_to_play_ms > rtt_ms || packet.time_since_last_ms + rtt_ms < max_wait_ms;
}
```

Good:

```cpp
// Good: Define invariant helper lambda outside loop
auto downsample = [kFactor](const float* m) { return *std::max_element(m, m + kFactor); };
for (size_t i = 1; i < kFftLengthBy2Plus1; ++i) {
  downsampled[i] = downsample(&mask[kFactor * (i - 1) + 1]);
}

// Good: Precompute const length and shared tables outside channel loop
const size_t temp_len = current_lag + overlap_length_;
GenerateRandomSinTableIndices(seed_, re_table, im_table);
for (size_t ch = 0; ch < num_channels_; ++ch) {
  GenerateComfortNoise(opt_, N2[ch], re_table, im_table, &noise[ch]);
}

// Good: Precompute and cache threshold on state/config update
void NackTracker::UpdateLossRate(int packets_lost) {
  max_wait_ms_ = 100.0 * config_.loss_rate * rtt_ms_ / (1 << 30);
}
```
