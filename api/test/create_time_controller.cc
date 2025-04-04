/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/create_time_controller.h"

#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "api/enable_media_with_defaults.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "api/peer_connection_interface.h"
#include "api/test/time_controller.h"
#include "api/units/timestamp.h"
#include "call/call.h"
#include "call/call_config.h"
#include "media/base/media_engine.h"
#include "pc/media_factory.h"
#include "rtc_base/checks.h"
#include "system_wrappers/include/clock.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {

std::unique_ptr<TimeController> CreateSimulatedTimeController() {
  return std::make_unique<GlobalSimulatedTimeController>(
      Timestamp::Seconds(10000));
}

void EnableMediaWithDefaultsAndTimeController(
    TimeController& time_controller,
    PeerConnectionFactoryDependencies& deps) {
  class TimeControllerBasedFactory : public MediaFactory {
   public:
    TimeControllerBasedFactory(
        Clock* absl_nonnull clock,
        absl_nonnull std::unique_ptr<MediaFactory> media_factory)
        : clock_(clock), media_factory_(std::move(media_factory)) {}

    std::unique_ptr<Call> CreateCall(CallConfig config) override {
      EnvironmentFactory env_factory(config.env);
      env_factory.Set(clock_);

      config.env = env_factory.Create();
      return media_factory_->CreateCall(std::move(config));
    }

    std::unique_ptr<cricket::MediaEngineInterface> CreateMediaEngine(
        const Environment& env,
        PeerConnectionFactoryDependencies& dependencies) override {
      return media_factory_->CreateMediaEngine(env, dependencies);
    }

   private:
    Clock* absl_nonnull clock_;
    absl_nonnull std::unique_ptr<MediaFactory> media_factory_;
  };

  EnableMediaWithDefaults(deps);
  RTC_CHECK(deps.media_factory);
  deps.media_factory = std::make_unique<TimeControllerBasedFactory>(
      time_controller.GetClock(), std::move(deps.media_factory));
}

}  // namespace webrtc
