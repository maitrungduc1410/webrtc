/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/rendering_simulator.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "api/environment/environment.h"
#include "api/sequence_checker.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_frame.h"
#include "api/video/video_frame.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/sequence_number_unwrapper.h"
#include "rtc_base/thread_annotations.h"
#include "video/timing/simulator/assembler.h"
#include "video/timing/simulator/rendering_tracker.h"
#include "video/timing/simulator/rtc_event_log_driver.h"

namespace webrtc::video_timing_simulator {

namespace {

// Observes the `Assembler` and `RenderedTimingTracker` in order to collect
// frame metadata for rendered frames.
class RenderedFrameCollector : public AssemblerEvents,
                               public RenderingTrackerEvents {
 public:
  RenderedFrameCollector(const Environment& env, uint32_t ssrc)
      : env_(env), ssrc_(ssrc) {
    RTC_DCHECK_NE(ssrc_, 0);
  }
  ~RenderedFrameCollector() override = default;

  RenderedFrameCollector(const RenderedFrameCollector&) = delete;
  RenderedFrameCollector& operator=(const RenderedFrameCollector&) = delete;

  // Implements `AssemblerEvents`.
  void OnAssembledFrame(const EncodedFrame& assembled_frame) override {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    Timestamp now = env_.clock().CurrentTime();
    if (!creation_timestamp_) {
      creation_timestamp_ = now;
    }
    int64_t frame_id = assembled_frame.Id();
    if (frames_.contains(frame_id)) {
      RTC_LOG(LS_WARNING) << "Assembled frame_id=" << frame_id
                          << " on ssrc=" << ssrc_
                          << " had already been collected. Dropping it."
                          << " (simulated_ts=" << env_.clock().CurrentTime()
                          << ")";
      return;
    }
    auto& frame = frames_[frame_id];
    RTC_DCHECK(!assembled_frame.PacketInfos().empty());
    frame.num_packets = static_cast<int>(assembled_frame.PacketInfos().size());
    frame.size = DataSize::Bytes(assembled_frame.size());
    frame.payload_type = static_cast<int>(assembled_frame.PayloadType());
    frame.rtp_timestamp = assembled_frame.RtpTimestamp();
    frame.unwrapped_rtp_timestamp =
        rtp_timestamp_unwrapper_.Unwrap(frame.rtp_timestamp);
    frame.frame_id = assembled_frame.Id();
    frame.spatial_id = assembled_frame.SpatialIndex().value_or(0);
    frame.temporal_id = assembled_frame.TemporalIndex().value_or(0);
    frame.num_references = static_cast<int>(assembled_frame.num_references);
    for (const auto& rtp_packet_info : assembled_frame.PacketInfos()) {
      Timestamp receive_time = rtp_packet_info.receive_time();
      if (receive_time.IsFinite()) {
        frame.first_packet_arrival_timestamp =
            std::min(frame.first_packet_arrival_timestamp, receive_time);
        frame.last_packet_arrival_timestamp =
            std::max(frame.last_packet_arrival_timestamp, receive_time);
      }
    }
    frame.assembled_timestamp = now;
  }

  // Implements `RenderedFrameEvents`.
  void OnDecodedFrame(const EncodedFrame& decoded_frame,
                      int frames_dropped,
                      TimeDelta jitter_buffer_minimum_delay,
                      TimeDelta jitter_buffer_target_delay,
                      TimeDelta jitter_buffer_delay) override {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    int64_t frame_id = decoded_frame.Id();
    auto it = frames_.find(frame_id);
    if (it == frames_.end()) {
      RTC_LOG(LS_WARNING)
          << "Decoded frame_id=" << frame_id << " on ssrc=" << ssrc_
          << " had no assembly information collected. Dropping it."
          << " (simulated_ts=" << env_.clock().CurrentTime() << ")";
      return;
    }
    auto& frame = it->second;
    RTC_DCHECK_EQ(frame_id, frame.frame_id);
    if (decoded_frame.RenderTimestamp().has_value()) {
      frame.render_timestamp = *decoded_frame.RenderTimestamp();
    }
    frame.decoded_timestamp = env_.clock().CurrentTime();
    frame.frames_dropped = frames_dropped;
    frame.jitter_buffer_minimum_delay = jitter_buffer_minimum_delay;
    frame.jitter_buffer_target_delay = jitter_buffer_target_delay;
    frame.jitter_buffer_delay = jitter_buffer_delay;
  }

  void OnRenderedFrame(const VideoFrame& rendered_frame) override {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    int64_t unwrapped_frame_id =
        rendered_frame_id_unwrapper_.Unwrap(rendered_frame.id());
    auto it = frames_.find(unwrapped_frame_id);
    if (it == frames_.end()) {
      RTC_LOG(LS_WARNING)
          << "Rendered frame_id=" << unwrapped_frame_id << " on ssrc=" << ssrc_
          << " had no decode information collected. Dropping it."
          << " (simulated_ts=" << env_.clock().CurrentTime() << ")";
      return;
    }
    auto& frame = it->second;
    RTC_DCHECK_EQ(unwrapped_frame_id, frame.frame_id);
    frame.rendered_timestamp = env_.clock().CurrentTime();
  }

  RenderingSimulator::Stream GetStream() const {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    RenderingSimulator::Stream stream;
    stream.ssrc = ssrc_;
    if (creation_timestamp_) {
      stream.creation_timestamp = *creation_timestamp_;
    }
    stream.frames.reserve(frames_.size());
    for (const auto& [key, value] : frames_) {
      RTC_DCHECK_EQ(key, value.frame_id);
      stream.frames.push_back(value);
    }
    absl::c_sort(stream.frames);
    return stream;
  }

 private:
  SequenceChecker sequence_checker_;
  const Environment env_;
  const uint32_t ssrc_;

  std::optional<Timestamp> creation_timestamp_
      RTC_GUARDED_BY(sequence_checker_);
  SeqNumUnwrapper<uint32_t> rtp_timestamp_unwrapper_
      RTC_GUARDED_BY(sequence_checker_);
  SeqNumUnwrapper<uint16_t> rendered_frame_id_unwrapper_
      RTC_GUARDED_BY(sequence_checker_);
  absl::flat_hash_map</*frame_id*/ int64_t, RenderingSimulator::Frame> frames_
      RTC_GUARDED_BY(sequence_checker_);
};

// Combines all objects needed to perform rendering simulation of a single
// stream. Inserts the streams results to the `results` pointer when `Close()`
// is called (at the end of simulation).
class RenderingSimulatorStream : public RtcEventLogDriver::StreamInterface {
 public:
  RenderingSimulatorStream(const RenderingSimulator::Config& config,
                           const Environment& env,
                           uint32_t ssrc,
                           RenderingSimulator::Results* absl_nonnull results)
      : collector_(env, ssrc),
        tracker_(env,
                 RenderingTracker::Config{
                     .ssrc = ssrc,
                     .render_delay = RenderingSimulator::kRenderDelay},
                 config.video_timing_factory(env),
                 &collector_),
        assembler_(env, ssrc, &collector_, &tracker_),
        results_(*results) {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    tracker_.SetDecodedFrameIdCallback(&assembler_);
  }
  ~RenderingSimulatorStream() override = default;

  // Implements `RtcEventLogDriver::StreamInterface`.
  void InsertPacket(const RtpPacketReceived& rtp_packet) override {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    assembler_.InsertPacket(rtp_packet);
  }

  void Close() override {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    RenderingSimulator::Stream stream = collector_.GetStream();
    if (!stream.IsEmpty()) {
      RTC_DCHECK_NE(stream.ssrc, 0u);
      results_.streams.push_back(stream);
    }
  }

 private:
  SequenceChecker sequence_checker_;
  RenderedFrameCollector collector_ RTC_GUARDED_BY(sequence_checker_);
  RenderingTracker tracker_ RTC_GUARDED_BY(sequence_checker_);
  Assembler assembler_ RTC_GUARDED_BY(sequence_checker_);
  RenderingSimulator::Results& results_;
};

}  // namespace

RenderingSimulator::RenderingSimulator(Config config) : config_(config) {}

RenderingSimulator::~RenderingSimulator() = default;

RenderingSimulator::Results RenderingSimulator::Simulate(
    const ParsedRtcEventLog& parsed_log) const {
  // Outputs.
  Results results;
  results.config_name = config_.name;

  // Simulation.
  auto stream_factory = [this, &results](const Environment& env,
                                         uint32_t ssrc) {
    return std::make_unique<RenderingSimulatorStream>(config_, env, ssrc,
                                                      &results);
  };
  RtcEventLogDriver rtc_event_log_simulator(
      &parsed_log, config_.field_trials_string, std::move(stream_factory));
  rtc_event_log_simulator.Simulate();

  // Return.
  absl::c_sort(results.streams);
  return results;
}

}  // namespace webrtc::video_timing_simulator
