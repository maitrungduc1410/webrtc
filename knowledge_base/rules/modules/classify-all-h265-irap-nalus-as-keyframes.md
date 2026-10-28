---
name: webrtc/modules/classify-all-h265-irap-nalus-as-keyframes
description: When depacketizing or buffering H.265 streams, classify all IRAP NALUs as keyframes using IRAP terminology, and never mark parameter sets as keyframes
rubric: correctness
severity: should_fix
---

**Rule.** When depacketizing, parsing, or buffering H.265/HEVC video streams,
classify all Intra Random Access Point (IRAP) NAL unit types (NALU types
16-23) as key frames (`VideoFrameType::kVideoFrameKey`). This includes all BLA
variants (kBlaWLp, kBlaWRadl, kBlaNLp), IDR (kIdrWRadl, kIdrNLp), CRA (kCra),
and reserved IRAP types (kRsvIrapVcl22, kRsvIrapVcl23); use IRAP terminology
(such as `has_irap` or `is_irap`) rather than `idr` or `has_idr`. In video RTP
depacketizers, do not mark parameter set NALUs (such as VPS, SPS, PPS) or
non-VCL NALUs as key frames; only mark intra-coded / IRAP slice NALUs as key
frames.

**Why.** In H.265/HEVC (RFC 7798 and ITU-T H.265 Table 7-1), BLA, IDR, and CRA
slices are all Intra Random Access Point (IRAP) pictures. Omitting BLA or CRA
variants causes packet buffers and decoders to misclassify intra refresh frames
as delta frames, while naming IRAP variables `has_idr` is misleading because
IRAP pictures include non-IDR random access points. Conversely, parameter sets
(VPS, SPS, PPS) provide codec configuration metadata rather than keyframe slice
data, and marking them as keyframes in RTP depacketizers corrupts frame type
classification before packet assembly.

## Examples

Bad:

```cpp
// 1. Missing BLA/CRA or incorrectly marking parameter sets as key frames:
switch (nalu_type) {
  case H265::NaluType::kVps:
  case H265::NaluType::kSps:
  case H265::NaluType::kPps:
  case H265::NaluType::kIdrWRadl:
  case H265::NaluType::kCra:
    parsed_payload->video_header.frame_type = VideoFrameType::kVideoFrameKey;
    break;
  case H265::NaluType::kBlaWLp:
    break;
}

// 2. Conflating IRAP range with IDR in variable naming:
has_idr |= (nalu_type >= H265::NaluType::kBlaWLp &&
            nalu_type <= H265::NaluType::kRsvIrapVcl23);
```

Good:

```cpp
// 1. Classifying all IRAP NALUs as key frames (not parameter sets):
switch (nalu_type) {
  case H265::NaluType::kBlaWLp:
  case H265::NaluType::kBlaWRadl:
  case H265::NaluType::kBlaNLp:
  case H265::NaluType::kIdrWRadl:
  case H265::NaluType::kIdrNLp:
  case H265::NaluType::kCra:
  case H265::NaluType::kRsvIrapVcl22:
  case H265::NaluType::kRsvIrapVcl23:
    parsed_payload->video_header.frame_type = VideoFrameType::kVideoFrameKey;
    break;
  case H265::NaluType::kVps:
  case H265::NaluType::kSps:
  case H265::NaluType::kPps:
    break;  // Parameter sets are not marked as key frames.
}

// 2. Using IRAP terminology for the full IRAP range:
has_irap |= (nalu_type >= H265::NaluType::kBlaWLp &&
             nalu_type <= H265::NaluType::kRsvIrapVcl23);
```
