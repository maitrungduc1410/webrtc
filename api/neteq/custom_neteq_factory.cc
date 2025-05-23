/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/neteq/custom_neteq_factory.h"

#include <memory>
#include <utility>

#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/environment/environment.h"
#include "api/neteq/neteq.h"
#include "api/neteq/neteq_controller_factory.h"
#include "api/scoped_refptr.h"
#include "modules/audio_coding/neteq/neteq_impl.h"

namespace webrtc {

CustomNetEqFactory::CustomNetEqFactory(
    std::unique_ptr<NetEqControllerFactory> controller_factory)
    : controller_factory_(std::move(controller_factory)) {}

CustomNetEqFactory::~CustomNetEqFactory() = default;

std::unique_ptr<NetEq> CustomNetEqFactory::Create(
    const Environment& env,
    const NetEq::Config& config,
    scoped_refptr<AudioDecoderFactory> decoder_factory) const {
  return std::make_unique<NetEqImpl>(
      config, NetEqImpl::Dependencies(env, config, std::move(decoder_factory),
                                      *controller_factory_));
}

}  // namespace webrtc
