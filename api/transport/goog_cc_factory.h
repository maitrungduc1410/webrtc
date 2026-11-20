/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TRANSPORT_GOOG_CC_FACTORY_H_
#define API_TRANSPORT_GOOG_CC_FACTORY_H_

#include <memory>
#include <optional>

#include "api/network_state_predictor.h"
#include "api/rtp_parameters.h"
#include "api/transport/network_control.h"
#include "api/units/time_delta.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

struct GoogCcFactoryConfig {
  // TODO(bugs.webrtc.org/447037083): Remove once downstream has migrated to
  // `feedback_type`.
  [[deprecated("Use feedback_type instead")]]
  bool rfc_8888_feedback_negotiated = false;
  // Negotiated RTCP feedback type for congestion control (e.g.
  // RtcpFeedbackType::TRANSPORT_CC or RtcpFeedbackType::CCFB), if known.
  std::optional<RtcpFeedbackType> feedback_type;
  std::unique_ptr<NetworkStateEstimatorFactory> network_state_estimator_factory;
  NetworkStatePredictorFactoryInterface* network_state_predictor_factory =
      nullptr;
};

class RTC_EXPORT GoogCcNetworkControllerFactory
    : public NetworkControllerFactoryInterface {
 public:
  GoogCcNetworkControllerFactory() = default;
  explicit GoogCcNetworkControllerFactory(GoogCcFactoryConfig config);

  std::unique_ptr<NetworkControllerInterface> Create(
      NetworkControllerConfig config) override;
  TimeDelta GetProcessInterval() const override;

 private:
  GoogCcFactoryConfig factory_config_;
};

}  // namespace webrtc

#endif  // API_TRANSPORT_GOOG_CC_FACTORY_H_
