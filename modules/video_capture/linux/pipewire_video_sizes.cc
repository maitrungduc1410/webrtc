/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_capture/linux/pipewire_video_sizes.h"

#include <spa/pod/iter.h>
#include <spa/utils/type.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <vector>

namespace webrtc {
namespace videocapturemodule {
namespace {

// Common sizes offered from a size range, besides its default, minimum and
// maximum.
static constexpr spa_rectangle kCommonSizes[] = {
    {3840, 2160}, {2560, 1440}, {1920, 1080}, {1600, 1200}, {1280, 960},
    {1280, 720},  {1024, 768},  {800, 600},   {640, 480},   {640, 360},
    {352, 288},   {320, 240},   {320, 180},   {160, 120}};

// Appends |size| to |sizes| unless it is already there.
static void AddUniqueSize(std::vector<spa_rectangle>& sizes,
                          const spa_rectangle& size) {
  if (std::none_of(sizes.begin(), sizes.end(), [&size](const spa_rectangle& s) {
        return s.width == size.width && s.height == size.height;
      })) {
    sizes.push_back(size);
  }
}

// Returns the sizes to offer for a size range with default |def|, bounds
// |min| and |max| and, for a stepped range, |step|: those of the default,
// bounds and common sizes that the range allows. As in SPA, a size on a
// stepped range has a width and height that are multiples of the step; the
// bounds are checked on each side, which is stricter than SPA's comparison
// of areas: both the width and the height stay within the bounds.
static std::vector<spa_rectangle> SizesInRange(const spa_rectangle& def,
                                               const spa_rectangle& min,
                                               const spa_rectangle& max,
                                               const spa_rectangle* step) {
  auto allowed = [&min, &max, step](const spa_rectangle& size) {
    if (size.width < min.width || size.width > max.width ||
        size.height < min.height || size.height > max.height) {
      return false;
    }
    if (!step) {
      return true;
    }
    // A zero step allows no size, and the modulo below must not divide by it.
    if (step->width == 0 || step->height == 0) {
      return false;
    }
    return size.width % step->width == 0 && size.height % step->height == 0;
  };

  std::vector<spa_rectangle> candidates = {def, min, max};
  candidates.insert(candidates.end(), std::begin(kCommonSizes),
                    std::end(kCommonSizes));

  std::vector<spa_rectangle> sizes;
  for (const spa_rectangle& size : candidates) {
    if (allowed(size)) {
      AddUniqueSize(sizes, size);
    }
  }
  return sizes;
}

// Returns whether the rate |a| is lower than or equal to the rate |b|.
static bool FractionLessOrEqual(const spa_fraction& a, const spa_fraction& b) {
  return static_cast<uint64_t>(a.num) * b.denom <=
         static_cast<uint64_t>(b.num) * a.denom;
}

// Returns whether |rate| rounds up to INT32_MAX or more, or has a zero
// denominator.
static bool RoundsUpToInt32Max(const spa_fraction& rate) {
  const std::optional<int32_t> fps = RoundedUpRate(rate);
  return !fps || *fps == std::numeric_limits<int32_t>::max();
}

}  // namespace

std::optional<int32_t> RoundedUpRate(const spa_fraction& rate) {
  if (rate.denom == 0) {
    return std::nullopt;
  }
  const double rounded = std::ceil(static_cast<double>(rate.num) / rate.denom);
  if (rounded > std::numeric_limits<int32_t>::max()) {
    return std::nullopt;
  }
  return static_cast<int32_t>(rounded);
}

std::vector<spa_rectangle> GetPipeWireVideoSizes(const spa_pod* value) {
  uint32_t n_items, choice;
  spa_pod* val = spa_pod_get_values(value, &n_items, &choice);
  // The values are read as an array of spa_rectangle.
  if (val->type != SPA_TYPE_Rectangle || val->size != sizeof(spa_rectangle)) {
    return {};
  }
  const auto* rect = static_cast<const spa_rectangle*>(SPA_POD_BODY(val));
  std::vector<spa_rectangle> sizes;
  if (choice == SPA_CHOICE_None && n_items >= 1) {
    sizes.push_back(rect[0]);
  } else if (choice == SPA_CHOICE_Enum && n_items >= 2) {
    // The first value of an enum is its default; the alternatives follow.
    for (uint32_t i = 1; i < n_items; i++) {
      AddUniqueSize(sizes, rect[i]);
    }
  } else if ((choice == SPA_CHOICE_Range || choice == SPA_CHOICE_Step) &&
             n_items >= 3) {
    sizes = SizesInRange(
        rect[0], rect[1], rect[2],
        choice == SPA_CHOICE_Step && n_items >= 4 ? &rect[3] : nullptr);
  }
  return sizes;
}

std::vector<spa_fraction> GetPipeWireRangeFrameRates(const spa_pod* value) {
  uint32_t n_items, choice;
  spa_pod* val = spa_pod_get_values(value, &n_items, &choice);
  // The values are read as an array of spa_fraction.
  if (val->type != SPA_TYPE_Fraction || val->size != sizeof(spa_fraction) ||
      choice != SPA_CHOICE_Range || n_items < 3) {
    return {};
  }
  // The values of a range are its default, its minimum and its maximum.
  const auto* fract = static_cast<const spa_fraction*>(SPA_POD_BODY(val));
  const spa_fraction& def = fract[0];
  const spa_fraction& min = fract[1];
  const spa_fraction& max = fract[2];
  if (!min.denom || !max.denom || !max.num) {
    return {};
  }
  // A maximum whose rate rounds up to INT32_MAX or more is taken as no upper
  // bound. Such a range offers its minimum alone, or no rate when the minimum
  // is zero or too large as well, as for the {25; 0..INT32_MAX} that
  // videotestsrc advertises.
  if (RoundsUpToInt32Max(max)) {
    if (!min.num || RoundsUpToInt32Max(min)) {
      return {};
    }
    return {min};
  }
  if (!FractionLessOrEqual(min, max)) {
    return {};
  }
  // As for sizes, a default outside the range is left out, and the bounds
  // are still offered.
  std::vector<spa_fraction> candidates = {min, max};
  if (def.denom && FractionLessOrEqual(min, def) &&
      FractionLessOrEqual(def, max)) {
    candidates.insert(candidates.begin(), def);
  }
  std::vector<spa_fraction> rates;
  for (const spa_fraction& rate : candidates) {
    if (!rate.num) {
      continue;
    }
    auto same = std::find_if(rates.begin(), rates.end(),
                             [&rate](const spa_fraction& r) {
                               return RoundedUpRate(r) == RoundedUpRate(rate);
                             });
    if (same == rates.end()) {
      rates.push_back(rate);
    } else if (!FractionLessOrEqual(rate, *same)) {
      *same = rate;
    }
  }
  return rates;
}

std::optional<spa_fraction> GetPipeWireEnumFrameRate(const spa_pod* value) {
  uint32_t n_items, choice;
  spa_pod* val = spa_pod_get_values(value, &n_items, &choice);
  // The values are read as an array of spa_fraction.
  if (val->type != SPA_TYPE_Fraction || val->size != sizeof(spa_fraction) ||
      choice != SPA_CHOICE_Enum || n_items < 2) {
    return std::nullopt;
  }
  // The first value of an enum is its default; the alternatives follow. An
  // alternative that does not fit in maxFPS is skipped, so that it does not
  // hide one that does.
  const auto* fract = static_cast<const spa_fraction*>(SPA_POD_BODY(val));
  std::optional<spa_fraction> max_rate;
  for (uint32_t i = 1; i < n_items; i++) {
    if (!RoundedUpRate(fract[i])) {
      continue;
    }
    if (!max_rate || !FractionLessOrEqual(fract[i], *max_rate)) {
      max_rate = fract[i];
    }
  }
  return max_rate;
}

}  // namespace videocapturemodule
}  // namespace webrtc
