/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_ACM2_ACM_RESAMPLER_H_
#define MODULES_AUDIO_CODING_ACM2_ACM_RESAMPLER_H_

#include <stddef.h>
#include <stdint.h>

#include <array>

#include "api/audio/audio_frame.h"
#include "common_audio/resampler/include/push_resampler.h"

namespace webrtc {
namespace acm2 {

// Helper class to perform resampling if needed, meant to be used after
// receiving the audio_frame from NetEq. Provides reasonably glitch free
// transitions between different output sample rates from NetEq.
class ResamplerHelper {
 public:
  ResamplerHelper();

  // Resamples audio_frame if it is not already in desired_sample_rate_hz.
  bool MaybeResample(int desired_sample_rate_hz, AudioFrame* audio_frame);

 private:
  PushResampler<int16_t> resampler_;
  bool resampled_last_output_frame_ = true;
  std::array<int16_t, AudioFrame::kMaxDataSizeSamples> last_audio_buffer_;
};

}  // namespace acm2
}  // namespace webrtc

#endif  // MODULES_AUDIO_CODING_ACM2_ACM_RESAMPLER_H_
