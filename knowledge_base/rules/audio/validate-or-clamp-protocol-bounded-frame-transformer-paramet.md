---
name: webrtc/audio/validate-or-clamp-protocol-bounded-frame-transformer-paramet
description: When implementing frame transformer setters or updating composite RTP extensions like AudioLevel, clamp bounded values and preserve existing metadata flags
rubric: correctness
severity: should_fix
---

**Rule.** When implementing or overriding frame transformer setters for
protocol-bounded attributes (such as RTP audio level in dBov per RFC 6464),
validate and clamp values to their valid specification range (e.g., clamping
dBov to [0, 127]) before storing or propagating them. Additionally, when
updating or reconstructing composite RTP header extensions (such as AudioLevel),
preserve existing extension metadata flags (such as voice_activity) from the
frame header rather than discarding or hardcoding them.

**Why.** Downstream wire protocol encoders and RTP header extension serializers
enforce strict range invariants (such as RTC_CHECK_LE on extension levels)
during packet creation. Hardcoding or omitting composite extension fields when
updating attributes discards existing frame state such as voice activity
indicators.

## Examples

Bad:

```cpp
void SetAudioLevel(std::optional<uint8_t> audio_level_dbov) override {
  audio_level_dbov_ = audio_level_dbov;
}

// Discarding voice_activity when updating AudioLevel from transformed frame:
if (transformed_frame->AudioLevel().has_value()) {
  header.extension.set_audio_level(AudioLevel(
      /*voice_activity=*/false, *transformed_frame->AudioLevel()));
}
```

Good:

```cpp
void SetAudioLevel(std::optional<uint8_t> audio_level_dbov) override {
  if (audio_level_dbov.has_value() && *audio_level_dbov > 127u) {
    audio_level_dbov = 127u;
  }
  audio_level_dbov_ = audio_level_dbov;
}

// Preserving existing voice_activity when updating AudioLevel:
if (transformed_frame->AudioLevel().has_value()) {
  bool current_voice_activity =
      header.extension.audio_level()
          ? header.extension.audio_level()->voice_activity()
          : false;
  header.extension.set_audio_level(
      AudioLevel(current_voice_activity, *transformed_frame->AudioLevel()));
}
```
