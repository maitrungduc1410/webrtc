/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/scream/scream_network_controller.h"

#include <memory>

#include "api/transport/bandwidth_usage.h"
#include "api/transport/network_control.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_delay_based.h"
#include "rtc_base/logging.h"
namespace webrtc {

ScreamNetworkController::ScreamNetworkController(NetworkControllerConfig config)
    : env_(config.env),
      scream_(env_),
      target_rate_constraints_(config.constraints) {
  if (config.constraints.min_data_rate.has_value() ||
      config.constraints.max_data_rate.has_value()) {
    scream_.SetTargetBitrateConstraints(
        config.constraints.min_data_rate.value_or(DataRate::Zero()),
        config.constraints.max_data_rate.value_or(DataRate::PlusInfinity()));
  }
}

NetworkControlUpdate ScreamNetworkController::OnNetworkAvailability(
    NetworkAvailability msg) {
  return NetworkControlUpdate();
}

NetworkControlUpdate ScreamNetworkController::OnNetworkRouteChange(
    NetworkRouteChange msg) {
  return NetworkControlUpdate();
}

NetworkControlUpdate ScreamNetworkController::OnProcessInterval(
    ProcessInterval msg) {
  // Scream currently has no need for periodic processing.
  return NetworkControlUpdate();
}

NetworkControlUpdate ScreamNetworkController::OnRemoteBitrateReport(
    RemoteBitrateReport msg) {
  // TODO: bugs.webrtc.org/447037083 - Implement;
  return NetworkControlUpdate();
}

NetworkControlUpdate ScreamNetworkController::OnRoundTripTimeUpdate(
    RoundTripTimeUpdate msg) {
  // Scream uses Smoothed RTT from TransportFeedback.
  return NetworkControlUpdate();
}

NetworkControlUpdate ScreamNetworkController::OnSentPacket(SentPacket msg) {
  // Scream does not have to know about sent packets.
  return NetworkControlUpdate();
}

NetworkControlUpdate ScreamNetworkController::OnReceivedPacket(
    ReceivedPacket msg) {
  // Scream does not have to know about received packets.
  return NetworkControlUpdate();
}

NetworkControlUpdate ScreamNetworkController::OnStreamsConfig(
    StreamsConfig msg) {
  // TODO: bugs.webrtc.org/447037083 - Implement;
  RTC_LOG_F(LS_INFO) << "Not implemented";
  return NetworkControlUpdate();
}

NetworkControlUpdate ScreamNetworkController::OnTargetRateConstraints(
    TargetRateConstraints msg) {
  target_rate_constraints_ = msg;

  // TODO: bugs.webrtc.org/447037083 - We should use the minimum rate from
  // constraints, REMB and remote network state estimates.
  scream_.SetTargetBitrateConstraints(
      target_rate_constraints_.min_data_rate.value_or(DataRate::Zero()),
      target_rate_constraints_.max_data_rate.value_or(
          DataRate::PlusInfinity()));
  // No need to change target rate immediately. Wait until next feedback.
  return NetworkControlUpdate();
}

NetworkControlUpdate ScreamNetworkController::OnTransportLossReport(
    TransportLossReport msg) {
  return NetworkControlUpdate();
}

NetworkControlUpdate ScreamNetworkController::OnNetworkStateEstimate(
    NetworkStateEstimate msg) {
  // TODO: bugs.webrtc.org/447037083 - Implement;
  RTC_LOG_F(LS_INFO) << "Not implemented";
  return NetworkControlUpdate();
}

NetworkControlUpdate ScreamNetworkController::OnTransportPacketsFeedback(
    TransportPacketsFeedback msg) {
  DataRate target_rate = scream_.OnTransportPacketsFeedback(msg);

  // TODO: bugs.webrtc.org/447037083 - Should we add separate event for Scream?
  env_.event_log().Log(std::make_unique<RtcEventBweUpdateDelayBased>(
      target_rate.bps(), BandwidthUsage::kBwNormal));

  TargetTransferRate target_rate_msg;
  target_rate_msg.at_time = msg.feedback_time;
  target_rate_msg.target_rate = target_rate;

  target_rate_msg.network_estimate.at_time = msg.feedback_time;
  target_rate_msg.network_estimate.round_trip_time = msg.smoothed_rtt;

  // TODO: bugs.webrtc.org/447037083 - bwe_period must currently be set but
  // it seems like it is not used for anything sensible. Try to remove it.
  target_rate_msg.network_estimate.bwe_period = TimeDelta::Millis(25);

  NetworkControlUpdate update;
  update.target_rate = target_rate_msg;
  update.pacer_config = GetPacerConfig(target_rate);

  return update;
}

PacerConfig ScreamNetworkController::GetPacerConfig(
    DataRate target_rate) const {
  const double kPacingRateFactor = 1.5;
  // TODO: bugs.webrtc.org/447037083 - Currently, pacer will allow sending
  // bursts of packets with a total size up to 40ms * pacing rate.
  return {
      .data_window = kPacingRateFactor * target_rate * TimeDelta::Seconds(1),
      .time_window = TimeDelta::Seconds(1),
  };
}

}  // namespace webrtc
