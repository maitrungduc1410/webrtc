/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/congestion_controller/scream/scream_v2.h"

#include "api/environment/environment.h"
#include "api/transport/ecn_marking.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/congestion_controller/scream/test/cc_feedback_generator.h"
#include "system_wrappers/include/clock.h"
#include "test/create_test_environment.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::TestWithParam;

constexpr DataSize kPacketSize = DataSize::Bytes(1000);

TransportPacketsFeedback CreateFeedback(Timestamp feedback_time,
                                        TimeDelta smoothed_rtt,
                                        int number_of_ect1_packets,
                                        int number_of_packets_in_flight) {
  TransportPacketsFeedback feedback;
  feedback.feedback_time = feedback_time;
  feedback.smoothed_rtt = smoothed_rtt;
  Timestamp send_time = feedback_time - smoothed_rtt;

  feedback.data_in_flight = kPacketSize * number_of_packets_in_flight;

  for (int i = 0; i < number_of_ect1_packets; ++i) {
    PacketResult result;
    result.sent_packet.send_time = send_time;
    result.sent_packet.size = kPacketSize;
    result.ecn = EcnMarking::kEct1;
    result.receive_time = send_time;
    feedback.packet_feedbacks.push_back(result);
  }

  return feedback;
}

TEST(ScreamV2Test, TargetRateIncreaseToMaxOnUnConstrainedNetwork) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  ScreamV2 scream(env);
  const DataRate kMaxDataRate = DataRate::KilobitsPerSec(2000);
  scream.SetTargetBitrateConstraints(DataRate::Zero(), kMaxDataRate);
  DataRate send_rate = DataRate::KilobitsPerSec(100);
  // Configure a feedback generator simulating a network with infinite
  // capacity but 25ms one way delay.
  CcFeedbackGenerator feedback_generator(
      {.network_config = {.queue_delay_ms = 25}});

  for (int i = 0; i < 10; ++i) {
    TransportPacketsFeedback feedback =
        feedback_generator.ProcessUntilNextFeedback(send_rate, clock);
    send_rate = scream.OnTransportPacketsFeedback(feedback);
  }
  EXPECT_EQ(send_rate, kMaxDataRate);
}

TEST(ScreamV2Test,
     ReferenceWindowDoesNotDecreaseAfterLowerSendRateOnUnconstrainedNetwork) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  ScreamV2 scream(env);
  const DataRate kMaxDataRate = DataRate::KilobitsPerSec(2000);
  scream.SetTargetBitrateConstraints(DataRate::Zero(), kMaxDataRate);
  DataRate send_rate = DataRate::KilobitsPerSec(100);
  // Configure a feedback generator simulating a network with infinite
  // capacity but 25ms one way delay.
  CcFeedbackGenerator feedback_generator(
      {.network_config = {.queue_delay_ms = 25}});

  for (int i = 0; i < 8; ++i) {
    TransportPacketsFeedback feedback =
        feedback_generator.ProcessUntilNextFeedback(send_rate, clock);
    send_rate = scream.OnTransportPacketsFeedback(feedback);
  }
  DataSize ref_window = scream.ref_window();

  // Half the send rate, but the network is still unconstrained.
  send_rate = send_rate / 2;
  for (int i = 0; i < 8; ++i) {
    TransportPacketsFeedback feedback =
        feedback_generator.ProcessUntilNextFeedback(send_rate, clock);
    scream.OnTransportPacketsFeedback(feedback);
  }
  // Still the same ref_window.
  EXPECT_EQ(ref_window, scream.ref_window());
}

TEST(ScreamV2Test, ReferenceWindowIncreaseLessPerStepOnLowRtt) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  ScreamV2 scream_1(env);
  ScreamV2 scream_2(env);

  TransportPacketsFeedback feedback =
      CreateFeedback(clock.CurrentTime(), /*smoothed_rtt=*/
                     TimeDelta::Millis(10),
                     /*number_of_ect1_packets=*/20,
                     /*number_of_packets_in_flight=*/20);

  TransportPacketsFeedback high_rtt_feedback = feedback;
  high_rtt_feedback.smoothed_rtt = TimeDelta::Millis(100);
  TransportPacketsFeedback low_rtt_feedback = feedback;
  low_rtt_feedback.smoothed_rtt = TimeDelta::Millis(1);

  scream_1.OnTransportPacketsFeedback(high_rtt_feedback);
  scream_2.OnTransportPacketsFeedback(low_rtt_feedback);

  EXPECT_GT(scream_1.ref_window(), scream_2.ref_window());
}

TEST(ScreamV2Test, ReferenceWindowIncreaseLessPerStepIfCeDetected) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  ScreamV2 scream_1(env);
  ScreamV2 scream_2(env);

  TransportPacketsFeedback feedback =
      CreateFeedback(clock.CurrentTime(), /*smoothed_rtt=*/
                     TimeDelta::Millis(10),
                     /*number_of_ect1_packets=*/20,
                     /*number_of_packets_in_flight=*/20);

  TransportPacketsFeedback ce_detected_feedback = feedback;
  ce_detected_feedback.packet_feedbacks[0].ecn = EcnMarking::kCe;

  scream_1.OnTransportPacketsFeedback(feedback);
  scream_2.OnTransportPacketsFeedback(ce_detected_feedback);

  EXPECT_GT(scream_1.ref_window(), scream_2.ref_window());
}

TEST(ScreamV2Test, ReferenceWindowIncreaseTo2xDataInflight) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  ScreamV2 scream(env);

  Timestamp start_time = clock.CurrentTime();
  TimeDelta feedback_interval = TimeDelta::Millis(25);

  TransportPacketsFeedback feedback =
      CreateFeedback(clock.CurrentTime(), /*smoothed_rtt=*/
                     TimeDelta::Millis(10),
                     /*number_of_ect1_packets=*/20,
                     /*number_of_packets_in_flight=*/10);

  while (clock.CurrentTime() < start_time + TimeDelta::Seconds(1)) {
    feedback.feedback_time = clock.CurrentTime();
    scream.OnTransportPacketsFeedback(feedback);
    clock.AdvanceTime(feedback_interval);
  }
  // Target rate can increase up to 2 * data_in_flight + Max Segment Size(
  // default 1000 bytes) when no max target rate has been set.
  EXPECT_EQ(scream.ref_window(),
            2 * feedback.data_in_flight + DataSize::Bytes(1000));
}

TEST(ScreamV2Test, CalculatesL4sAlpha) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});

  ScreamV2 scream(env);

  Timestamp start_time = clock.CurrentTime();
  TimeDelta feedback_interval = TimeDelta::Millis(25);

  TransportPacketsFeedback feedback =
      CreateFeedback(clock.CurrentTime(), /*smoothed_rtt=*/
                     TimeDelta::Millis(10),
                     /*number_of_ect1_packets=*/20,
                     /*number_of_packets_in_flight=*/20);
  // CE mark 20% of packets.
  for (int i = 0; i < 4; ++i) {
    feedback.packet_feedbacks[i].ecn = EcnMarking::kCe;
  }

  double l4s_alpha = scream.l4s_alpha();
  while (clock.CurrentTime() < start_time + TimeDelta::Seconds(2)) {
    feedback.feedback_time = clock.CurrentTime();
    scream.OnTransportPacketsFeedback(feedback);
    EXPECT_GT(scream.l4s_alpha(), l4s_alpha);
    clock.AdvanceTime(feedback_interval);
  }

  EXPECT_NEAR(scream.l4s_alpha(), 0.2, 0.01);
}

}  // namespace
}  // namespace webrtc
