/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/transport/goog_cc_factory.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "api/field_trials_view.h"
#include "api/rtp_parameters.h"
#include "api/transport/network_control.h"
#include "api/units/time_delta.h"
#include "modules/congestion_controller/goog_cc/goog_cc_network_control.h"
#include "modules/congestion_controller/scream/scream_network_controller.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/logging.h"
#include "rtc_base/system/ignore_warnings.h"

namespace webrtc {

namespace {

// If field trial value of the key WebRTC-Bwe-ScreamV2 contain:
//   Enabled - Scream is used if RFC 8888 congestion control is negotiated.
//   mode:always - Scream is used regardless of RTCP feedback.
constexpr char kScreamFieldTrial[] = "WebRTC-Bwe-ScreamV2";

enum class Mode { kGoogCc, kScreamWithTwccOrRfc8888, kScreamWithRfc8888 };

Mode ParseMode(const FieldTrialsView& field_trials) {
  if (field_trials.IsEnabled(kScreamFieldTrial)) {
    return Mode::kScreamWithRfc8888;
  }
  FieldTrialParameter<std::string> mode("mode", "");
  ParseFieldTrial({&mode}, field_trials.Lookup(kScreamFieldTrial));
  if (mode->empty()) {
    return Mode::kGoogCc;
  }
  if (mode.Get() == "always") {
    RTC_LOG(LS_INFO) << "ScreamV2 enabled always";
    return Mode::kScreamWithTwccOrRfc8888;
  }
  return Mode::kGoogCc;
}

}  // namespace

GoogCcNetworkControllerFactory::GoogCcNetworkControllerFactory(
    GoogCcFactoryConfig config)
    : factory_config_(std::move(config)) {}

std::unique_ptr<NetworkControllerInterface>
GoogCcNetworkControllerFactory::Create(NetworkControllerConfig config) {
  GoogCcConfig goog_cc_config;
  if (factory_config_.network_state_estimator_factory) {
    goog_cc_config.network_state_estimator =
        factory_config_.network_state_estimator_factory->Create(
            &config.env.field_trials());
  }
  if (factory_config_.network_state_predictor_factory) {
    goog_cc_config.network_state_predictor =
        factory_config_.network_state_predictor_factory
            ->CreateNetworkStatePredictor();
  }
  std::optional<RtcpFeedbackType> feedback_type = factory_config_.feedback_type;
  RTC_PUSH_IGNORING_WDEPRECATED_DECLARATIONS()
  if (!feedback_type.has_value() &&
      factory_config_.rfc_8888_feedback_negotiated) {
    feedback_type = RtcpFeedbackType::CCFB;
  }
  RTC_POP_IGNORING_WDEPRECATED_DECLARATIONS()
  Mode mode = ParseMode(config.env.field_trials());
  if (mode == Mode::kScreamWithTwccOrRfc8888 ||
      (mode == Mode::kScreamWithRfc8888 &&
       feedback_type == RtcpFeedbackType::CCFB)) {
    return std::make_unique<ScreamNetworkController>(config);
  }
  goog_cc_config.feedback_type = feedback_type;
  return std::make_unique<GoogCcNetworkController>(config,
                                                   std::move(goog_cc_config));
}

TimeDelta GoogCcNetworkControllerFactory::GetProcessInterval() const {
  const int64_t kUpdateIntervalMs = 25;
  return TimeDelta::Millis(kUpdateIntervalMs);
}

}  // namespace webrtc
