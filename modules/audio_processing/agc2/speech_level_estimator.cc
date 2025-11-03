/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/speech_level_estimator.h"

#include <memory>

#include "api/audio/audio_processing.h"
#include "modules/audio_processing/agc2/speech_level_estimator_impl.h"

namespace webrtc {

std::unique_ptr<SpeechLevelEstimator> SpeechLevelEstimator::Create(
    ApmDataDumper* apm_data_dumper,
    const AudioProcessing::Config::GainController2::AdaptiveDigital& config,
    int adjacent_speech_frames_threshold) {
  return std::make_unique<SpeechLevelEstimatorImpl>(
      apm_data_dumper, config, adjacent_speech_frames_threshold);
}

}  // namespace webrtc
