/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/wayland/screencast_stream_utils.h"

#include <libdrm/drm_fourcc.h>
#include <pipewire/pipewire.h>
#include <spa/param/format.h>
#include <spa/param/param.h>
#include <spa/param/video/format.h>
#include <spa/pod/builder.h>
#include <spa/pod/iter.h>
#include <spa/pod/pod.h>
#include <spa/pod/vararg.h>
#include <spa/utils/type.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <tuple>
#include <vector>

#include "modules/desktop_capture/linux/wayland/egl_dmabuf.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {

void AddModifierParams(spa_pod_builder* builder,
                       const std::vector<uint64_t>& modifiers) {
  if (modifiers.size() == 1 && modifiers[0] == DRM_FORMAT_MOD_INVALID) {
    spa_pod_builder_prop(builder, SPA_FORMAT_VIDEO_modifier,
                         SPA_POD_PROP_FLAG_MANDATORY);
    spa_pod_builder_long(builder, modifiers[0]);
  } else {
    struct spa_pod_frame modifier_frame;
    spa_pod_builder_prop(
        builder, SPA_FORMAT_VIDEO_modifier,
        SPA_POD_PROP_FLAG_MANDATORY | SPA_POD_PROP_FLAG_DONT_FIXATE);
    spa_pod_builder_push_choice(builder, &modifier_frame, SPA_CHOICE_Enum, 0);

    // Add the first modifier twice as the very first value is the default
    // option.
    spa_pod_builder_long(builder, modifiers[0]);
    // Add modifiers from the array.
    for (int64_t val : modifiers) {
      spa_pod_builder_long(builder, val);
    }
    spa_pod_builder_pop(builder, &modifier_frame);
  }
}

void FinishFormatPod(spa_pod_builder* builder,
                     spa_pod_frame* frame,
                     std::vector<uint32_t>& pod_offsets) {
  if (spa_pod_builder_pop(builder, frame)) {
    pod_offsets.push_back(frame->offset);
  } else {
    RTC_LOG(LS_WARNING)
        << "Failed to build PipeWire format pod: buffer exhausted (offset="
        << builder->state.offset << ", size=" << builder->size << ")";
  }
}

void AppendPodsFromOffsets(spa_pod_builder* builder,
                           const std::vector<uint32_t>& pod_offsets,
                           std::vector<const spa_pod*>& params) {
  params.reserve(params.size() + pod_offsets.size());
  for (uint32_t offset : pod_offsets) {
    if (const spa_pod* pod = spa_pod_builder_deref(builder, offset)) {
      params.push_back(pod);
    }
  }
}

void AppendFormatPods(spa_pod_builder* builder,
                      uint32_t format,
                      const struct spa_rectangle* resolution,
                      const struct spa_fraction* frame_rate,
                      const std::vector<uint64_t>& modifiers,
                      bool add_exact_max_frame_rate,
                      std::vector<uint32_t>& pod_offsets) {
  auto append_pod = [&](bool exact_max_frame_rate) {
    struct spa_pod_frame frame;
    spa_pod_builder_push_object(builder, &frame, SPA_TYPE_OBJECT_Format,
                                SPA_PARAM_EnumFormat);
    BuildBaseFormatParams(builder, format, resolution, frame_rate,
                          exact_max_frame_rate);
    if (!modifiers.empty()) {
      AddModifierParams(builder, modifiers);
    }
    FinishFormatPod(builder, &frame, pod_offsets);
  };

  if (add_exact_max_frame_rate) {
    append_pod(/*exact_max_frame_rate=*/true);
  }
  append_pod(/*exact_max_frame_rate=*/false);
}

}  // namespace

SpaPodDynamicBuilder::SpaPodDynamicBuilder() {
  builder_ = SPA_POD_BUILDER_INIT(initial_buffer_, kInitialBufferSize);
  callbacks_ = {
      .version = SPA_VERSION_POD_BUILDER_CALLBACKS,
      .overflow = &SpaPodDynamicBuilder::OnOverflow,
  };
  spa_pod_builder_set_callbacks(&builder_, &callbacks_, this);
}

SpaPodDynamicBuilder::~SpaPodDynamicBuilder() {
  if (builder_.data != initial_buffer_) {
    std::free(builder_.data);
  }
}

// static
int SpaPodDynamicBuilder::OnOverflow(void* data, uint32_t size) {
  auto* self = static_cast<SpaPodDynamicBuilder*>(data);
  const uint64_t rounded_size =
      (static_cast<uint64_t>(size) + kExtendSize - 1) / kExtendSize *
      kExtendSize;
  if (rounded_size > UINT32_MAX) {
    return -ENOSPC;
  }
  const uint64_t doubled_size = static_cast<uint64_t>(self->builder_.size) * 2;
  const uint32_t new_size = static_cast<uint32_t>(
      std::min<uint64_t>(UINT32_MAX, std::max(rounded_size, doubled_size)));
  const bool using_initial_buffer =
      (self->builder_.data == self->initial_buffer_);
  void* new_data = std::realloc(
      using_initial_buffer ? nullptr : self->builder_.data, new_size);
  if (!new_data) {
    return -ENOMEM;
  }
  if (using_initial_buffer) {
    std::memcpy(new_data, self->initial_buffer_, self->builder_.size);
  }
  self->builder_.data = new_data;
  self->builder_.size = new_size;
  return 0;
}

constexpr uint32_t kSupportedPixelFormats[] = {
    SPA_VIDEO_FORMAT_BGRA, SPA_VIDEO_FORMAT_RGBA, SPA_VIDEO_FORMAT_BGRx,
    SPA_VIDEO_FORMAT_RGBx};

void BuildBaseFormatParams(spa_pod_builder* builder,
                           uint32_t format,
                           const struct spa_rectangle* resolution,
                           const struct spa_fraction* frame_rate,
                           bool exact_max_frame_rate) {
  spa_pod_builder_add(builder, SPA_FORMAT_mediaType,
                      SPA_POD_Id(SPA_MEDIA_TYPE_video), 0);
  spa_pod_builder_add(builder, SPA_FORMAT_mediaSubtype,
                      SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw), 0);
  spa_pod_builder_add(builder, SPA_FORMAT_VIDEO_format, SPA_POD_Id(format), 0);

  if (resolution) {
    spa_pod_builder_add(builder, SPA_FORMAT_VIDEO_size,
                        SPA_POD_Rectangle(resolution), 0);
  } else {
    spa_rectangle pw_min_screen_bounds = spa_rectangle{.width = 1, .height = 1};
    spa_rectangle pw_max_screen_bounds =
        spa_rectangle{.width = UINT32_MAX, .height = UINT32_MAX};
    spa_pod_builder_add(builder, SPA_FORMAT_VIDEO_size,
                        SPA_POD_CHOICE_RANGE_Rectangle(&pw_min_screen_bounds,
                                                       &pw_min_screen_bounds,
                                                       &pw_max_screen_bounds),
                        0);
  }
  if (frame_rate) {
    static const spa_fraction pw_min_frame_rate =
        spa_fraction{.num = 0, .denom = 1};
    spa_pod_builder_add(builder, SPA_FORMAT_VIDEO_framerate,
                        SPA_POD_CHOICE_RANGE_Fraction(
                            frame_rate, &pw_min_frame_rate, frame_rate),
                        0);
    if (exact_max_frame_rate) {
      spa_pod_builder_add(builder, SPA_FORMAT_VIDEO_maxFramerate,
                          SPA_POD_Fraction(frame_rate), 0);
    } else {
      spa_pod_builder_add(builder, SPA_FORMAT_VIDEO_maxFramerate,
                          SPA_POD_CHOICE_RANGE_Fraction(
                              frame_rate, &pw_min_frame_rate, frame_rate),
                          0);
    }
  }
}

void BuildBaseFormat(spa_pod_builder* builder,
                     const struct spa_rectangle* resolution,
                     const struct spa_fraction* frame_rate,
                     std::vector<const spa_pod*>& params,
                     bool include_exact_max_frame_rate) {
  const bool add_exact_max_frame_rate =
      include_exact_max_frame_rate && frame_rate && frame_rate->num != 0;
  std::vector<uint32_t> pod_offsets;
  for (uint32_t format : kSupportedPixelFormats) {
    AppendFormatPods(builder, format, resolution, frame_rate, /*modifiers=*/{},
                     add_exact_max_frame_rate, pod_offsets);
  }
  AppendPodsFromOffsets(builder, pod_offsets, params);
}

void BuildFullFormat(spa_pod_builder* builder,
                     EglDrmDevice* render_device,
                     const struct spa_rectangle* resolution,
                     const struct spa_fraction* frame_rate,
                     std::vector<const spa_pod*>& params,
                     bool include_exact_max_frame_rate) {
  const bool add_exact_max_frame_rate =
      include_exact_max_frame_rate && frame_rate && frame_rate->num != 0;
  std::vector<uint32_t> pod_offsets;
  for (uint32_t format : kSupportedPixelFormats) {
    std::vector<uint64_t> modifiers;
    if (render_device) {
      modifiers = render_device->QueryDmaBufModifiers(format);
    }

    if (!modifiers.empty()) {
      AppendFormatPods(builder, format, resolution, frame_rate, modifiers,
                       add_exact_max_frame_rate, pod_offsets);
    }
    AppendFormatPods(builder, format, resolution, frame_rate, /*modifiers=*/{},
                     add_exact_max_frame_rate, pod_offsets);
  }
  AppendPodsFromOffsets(builder, pod_offsets, params);
}

}  // namespace webrtc
