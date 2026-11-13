/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SCREENCAST_STREAM_UTILS_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SCREENCAST_STREAM_UTILS_H_

#include <spa/pod/builder.h>
#include <stdint.h>

#include <vector>

struct spa_pod;
struct spa_rectangle;
struct spa_fraction;

namespace webrtc {

class EglDrmDevice;

// RAII wrapper around spa_pod_builder that uses an inline seed buffer and
// dynamically grows on the heap via an overflow callback if more space is
// required (e.g. when GPU drivers expose large DMA-BUF modifier lists).
//
// Note: Any spa_pod pointers materialized from this builder remain valid only
// until the builder is destroyed or further writes are made to it (which may
// reallocate the backing buffer).
class SpaPodDynamicBuilder {
 public:
  SpaPodDynamicBuilder();
  ~SpaPodDynamicBuilder();

  SpaPodDynamicBuilder(const SpaPodDynamicBuilder&) = delete;
  SpaPodDynamicBuilder& operator=(const SpaPodDynamicBuilder&) = delete;

  spa_pod_builder* get() { return &builder_; }

 private:
  static int OnOverflow(void* data, uint32_t size);

  static constexpr uint32_t kInitialBufferSize = 4096;
  static constexpr uint32_t kExtendSize = 4096;
  uint8_t initial_buffer_[kInitialBufferSize] = {};
  spa_pod_builder builder_ = {};
  spa_pod_builder_callbacks callbacks_ = {};
};

// Builds base video format parameters. The format parameter consists of:
// - SPA_FORMAT_mediaType with SPA_MEDIA_TYPE_video
// - SPA_FORMAT_mediaSubtype with SPA_MEDIA_SUBTYPE_raw
// - SPA_FORMAT_VIDEO_format with the specified format
// - SPA_FORMAT_VIDEO_size and SPA_FORMAT_VIDEO_framerate based on the
//   provided resolution and frame_rate arguments (if non-null)
// When `exact_max_frame_rate` is true, SPA_FORMAT_VIDEO_maxFramerate is set to
// an exact SPA_POD_Fraction rather than a [0/1 .. frame_rate] range.
void BuildBaseFormatParams(spa_pod_builder* builder,
                           uint32_t format,
                           const struct spa_rectangle* resolution,
                           const struct spa_fraction* frame_rate,
                           bool exact_max_frame_rate = false);

// Builds minimum video format parameters for all supported pixel formats:
// - SPA_VIDEO_FORMAT_BGRA
// - SPA_VIDEO_FORMAT_RGBA
// - SPA_VIDEO_FORMAT_BGRx
// - SPA_VIDEO_FORMAT_RGBx
// Each format is added as a separate parameter to the params vector. When
// `include_exact_max_frame_rate` is true and `frame_rate` is non-zero, a
// preferred pod with an exact maxFramerate is emitted ahead of the range pod.
//
// Pointer lifetime: Pointers appended to `params` reference `builder`'s backing
// buffer and remain valid only until `builder` is destroyed or written to
// again. When using a dynamic builder, do not pre-populate `params` with pods
// from the same `builder` or call `BuildBaseFormat`/`BuildFullFormat` multiple
// times on the same `builder` instance.
void BuildBaseFormat(spa_pod_builder* builder,
                     const struct spa_rectangle* resolution,
                     const struct spa_fraction* frame_rate,
                     std::vector<const spa_pod*>& params,
                     bool include_exact_max_frame_rate = true);

// Builds full video format parameters. Full video format consists of all the
// base parameters (media type, subtype, format, size, framerate), and also
// adds DMA-BUF modifiers from the provided render device. Modifiers are used
// with SPA_POD_PROP_FLAG_MANDATORY and SPA_POD_PROP_FLAG_DONT_FIXATE flags.
// A fallback format (without modifiers) is also provided in case the producer
// doesn't support DMA-BUFs. When `include_exact_max_frame_rate` is true and
// `frame_rate` is non-zero, preferred pods with an exact maxFramerate are
// emitted ahead of the range pods for both DMA-BUF and fallback formats.
//
// Pointer lifetime: Pointers appended to `params` reference `builder`'s backing
// buffer and remain valid only until `builder` is destroyed or written to
// again. When using a dynamic builder, do not pre-populate `params` with pods
// from the same `builder` or call `BuildBaseFormat`/`BuildFullFormat` multiple
// times on the same `builder` instance.
void BuildFullFormat(spa_pod_builder* builder,
                     EglDrmDevice* render_device,
                     const struct spa_rectangle* resolution,
                     const struct spa_fraction* frame_rate,
                     std::vector<const spa_pod*>& params,
                     bool include_exact_max_frame_rate = true);

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SCREENCAST_STREAM_UTILS_H_
