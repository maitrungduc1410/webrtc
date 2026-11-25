/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CAPTURE_LINUX_PIPEWIRE_VIDEO_SIZES_H_
#define MODULES_VIDEO_CAPTURE_LINUX_PIPEWIRE_VIDEO_SIZES_H_

#include <spa/pod/pod.h>
#include <spa/utils/defs.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace webrtc {
namespace videocapturemodule {

// Returns |rate| rounded up to an integer, as VideoCaptureCapability::maxFPS
// holds it, or nothing if the denominator is zero or the rounded rate does not
// fit in an int32_t.
std::optional<int32_t> RoundedUpRate(const spa_fraction& rate);

// Selects concrete sizes from a SPA_FORMAT_VIDEO_size value. Ranges yield a
// finite selection, filtered by their bounds and absolute SPA step multiples.
std::vector<spa_rectangle> GetPipeWireVideoSizes(const spa_pod* value);

// Selects the frame rates to offer from a SPA_FORMAT_VIDEO_framerate range:
// its default, minimum and maximum, leaving out zero rates. Of two rates that
// round up to the same integer, the larger one is kept. A range without an
// upper bound yields its minimum alone. A value that is not a valid range
// yields no rate.
std::vector<spa_fraction> GetPipeWireRangeFrameRates(const spa_pod* value);

// Selects the frame rate to offer from a SPA_FORMAT_VIDEO_framerate enum: the
// largest of its alternatives that RoundedUpRate() accepts, the first one of
// equal rates. A value that is not a valid enum, or that has no such
// alternative, yields no rate.
std::optional<spa_fraction> GetPipeWireEnumFrameRate(const spa_pod* value);

}  // namespace videocapturemodule
}  // namespace webrtc

#endif  // MODULES_VIDEO_CAPTURE_LINUX_PIPEWIRE_VIDEO_SIZES_H_
