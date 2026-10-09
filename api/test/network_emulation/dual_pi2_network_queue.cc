/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/test/network_emulation/dual_pi2_network_queue.h"

#include <cstddef>
#include <optional>
#include <queue>

#include "api/sequence_checker.h"
#include "api/test/simulated_network.h"
#include "api/transport/ecn_marking.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

DualPi2NetworkQueue::DualPi2NetworkQueue(const Config& config)
    : config_(config), random_(config.seed) {
  sequence_checker_.Detach();
}

void DualPi2NetworkQueue::SetMaxPacketCapacity(size_t max_packet_capacity) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  max_packet_capacity_ = max_packet_capacity;
  // Hack to allow SetMaxpPacketCapactiy to be called before the queue is being
  // used on another sequence.
  sequence_checker_.Detach();
}

bool DualPi2NetworkQueue::EnqueuePacket(const PacketInFlightInfo& packet_info) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (max_packet_capacity_.has_value() &&
      l4s_queue_.size() + classic_queue_.size() >= *max_packet_capacity_) {
    RTC_LOG(LS_WARNING)
        << "DualPi2NetworkQueue::EnqueuePacket: Dropping packet "
           "because max packet capacity is reached.";
    return false;
  }

  if (packet_info.ecn == EcnMarking::kNotEct ||
      packet_info.ecn == EcnMarking::kEct0) {
    if (HasStandingQueueBacklog() &&
        random_.Rand<double>() < classic_drop_probability()) {
      RTC_DLOG(LS_WARNING)
          << "DualPi2NetworkQueue::EnqueuePacket: Dropping classic packet "
          << packet_info.packet_id << ". Classic drop probability is "
          << classic_drop_probability()
          << " L4S queue size: " << l4s_queue_.size()
          << " classic queue size: " << classic_queue_.size();
      return false;
    }
    classic_queue_.push(packet_info);
    return true;
  }
  RTC_DCHECK(packet_info.ecn == EcnMarking::kEct1 ||
             packet_info.ecn == EcnMarking::kCe);
  l4s_queue_.push(packet_info);
  return true;
}

std::optional<PacketInFlightInfo> DualPi2NetworkQueue::PeekNextPacket() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (!l4s_queue_.empty()) {
    return l4s_queue_.front();
  }
  if (!classic_queue_.empty()) {
    return classic_queue_.front();
  }
  return std::nullopt;
}

std::optional<PacketInFlightInfo> DualPi2NetworkQueue::DequeuePacket(
    Timestamp time_now) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  std::queue<PacketInFlightInfo>& queue =
      l4s_queue_.empty() ? classic_queue_ : l4s_queue_;
  if (queue.empty()) {
    return std::nullopt;
  }

  PacketInFlightInfo packet_info = queue.front();
  TimeDelta sojourn_time = time_now - packet_info.send_time();
  UpdateBaseMarkingProbability(time_now, sojourn_time);
  if (packet_info.ecn == EcnMarking::kEct1 && HasStandingQueueBacklog()) {
    if (sojourn_time > config_.step_threshold ||
        random_.Rand<double>() < l4s_marking_probability()) {
      packet_info.ecn = EcnMarking::kCe;
    }
  }
  queue.pop();
  return packet_info;
}

bool DualPi2NetworkQueue::empty() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return classic_queue_.empty() && l4s_queue_.empty();
}

void DualPi2NetworkQueue::UpdateBaseMarkingProbability(Timestamp time_now,
                                                       TimeDelta sojourn_time) {
  if (time_now - config_.probability_update_interval <
      last_probability_update_time_) {
    return;
  }
  last_probability_update_time_ = time_now;
  TimeDelta proportional_update =
      config_.alpha * (sojourn_time - config_.target_delay);
  TimeDelta integral_update =
      config_.beta * (sojourn_time - previous_sojourn_time_);
  previous_sojourn_time_ = sojourn_time;
  base_marking_probability_ +=
      proportional_update.seconds<double>() + integral_update.seconds<double>();

  if (base_marking_probability_ < 0) {
    base_marking_probability_ = 0;
  }
  if (base_marking_probability_ > 1.0) {
    base_marking_probability_ = 1.0;
  }
  RTC_DLOG(LS_VERBOSE) << "base_marking_probability_: "
                       << base_marking_probability_;
}

bool DualPi2NetworkQueue::HasStandingQueueBacklog() const {
  return l4s_queue_.size() + classic_queue_.size() >=
         config_.min_backlog_packets;
}

}  // namespace webrtc
