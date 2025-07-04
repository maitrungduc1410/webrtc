/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/fake_network_pipe.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include "api/test/simulated_network.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/network/simulated_network.h"

using ::testing::_;
using ::testing::Property;
using ::testing::WithArg;

namespace webrtc {
class MockReceiver : public PacketReceiver {
 public:
  MOCK_METHOD(void, DeliverRtcpPacket, (CopyOnWriteBuffer packet), (override));
  MOCK_METHOD(void,
              DeliverRtpPacket,
              (MediaType media_type,
               RtpPacketReceived packet,
               OnUndemuxablePacketHandler undemuxable_packet_handler),
              (override));
  ~MockReceiver() override = default;
};

class ReorderTestReceiver : public MockReceiver {
 public:
  void DeliverRtpPacket(
      MediaType /* media_type */,
      RtpPacketReceived packet,
      OnUndemuxablePacketHandler /* undemuxable_packet_handler */) override {
    RTC_DCHECK_GE(packet.size(), sizeof(int));
    delivered_sequence_numbers_.push_back(packet.SequenceNumber());
  }
  std::vector<int> delivered_sequence_numbers_;
};

class FakeNetworkPipeTest : public ::testing::Test {
 public:
  FakeNetworkPipeTest() : fake_clock_(12345) {}

 protected:
  void SendPackets(FakeNetworkPipe* pipe, int number_packets, int packet_size) {
    RTC_DCHECK_GE(packet_size, sizeof(int));
    for (int i = 0; i < number_packets; ++i) {
      RtpPacketReceived packet;
      constexpr size_t kFixedHeaderSize = 12;
      packet.AllocatePayload(packet_size - kFixedHeaderSize);
      packet.SetSequenceNumber(i);
      packet.set_arrival_time(fake_clock_.CurrentTime());
      RTC_DCHECK_EQ(packet.Buffer().size(), packet_size);
      pipe->DeliverRtpPacket(MediaType::ANY, std::move(packet),
                             [](const RtpPacketReceived&) { return false; });
    }
  }

  int PacketTimeMs(DataRate capacity, int packet_size) const {
    return 8 * packet_size / capacity.kbps();
  }

  SimulatedClock fake_clock_;
};

// Test the capacity link and verify we get as many packets as we expect.
TEST_F(FakeNetworkPipeTest, CapacityTest) {
  BuiltInNetworkBehaviorConfig config;
  config.queue_length_packets = 20;
  config.link_capacity = DataRate::KilobitsPerSec(80);
  MockReceiver receiver;
  auto simulated_network = std::make_unique<SimulatedNetwork>(config);
  std::unique_ptr<FakeNetworkPipe> pipe(new FakeNetworkPipe(
      &fake_clock_, std::move(simulated_network), &receiver));

  // Add 10 packets of 1000 bytes, = 80 kb, and verify it takes one second to
  // get through the pipe.
  const int kNumPackets = 10;
  const int kPacketSize = 1000;
  SendPackets(pipe.get(), kNumPackets, kPacketSize);

  // Time to get one packet through the link.
  const int kPacketTimeMs = PacketTimeMs(config.link_capacity, kPacketSize);

  // Time haven't increased yet, so we souldn't get any packets.
  EXPECT_CALL(receiver, DeliverRtpPacket).Times(0);
  pipe->Process();

  // Advance enough time to release one packet.
  fake_clock_.AdvanceTimeMilliseconds(kPacketTimeMs);
  EXPECT_CALL(receiver, DeliverRtpPacket).Times(1);
  pipe->Process();

  // Release all but one packet
  fake_clock_.AdvanceTimeMilliseconds(9 * kPacketTimeMs - 1);
  EXPECT_CALL(receiver, DeliverRtpPacket).Times(8);
  pipe->Process();

  // And the last one.
  fake_clock_.AdvanceTimeMilliseconds(1);
  EXPECT_CALL(receiver, DeliverRtpPacket).Times(1);
  pipe->Process();
}

// Test the extra network delay.
TEST_F(FakeNetworkPipeTest, ExtraDelayTest) {
  BuiltInNetworkBehaviorConfig config;
  config.queue_length_packets = 20;
  config.queue_delay_ms = 100;
  config.link_capacity = DataRate::KilobitsPerSec(80);
  MockReceiver receiver;
  auto simulated_network = std::make_unique<SimulatedNetwork>(config);
  std::unique_ptr<FakeNetworkPipe> pipe(new FakeNetworkPipe(
      &fake_clock_, std::move(simulated_network), &receiver));

  const int kNumPackets = 2;
  const int kPacketSize = 1000;
  SendPackets(pipe.get(), kNumPackets, kPacketSize);

  // Time to get one packet through the link.
  const int kPacketTimeMs = PacketTimeMs(config.link_capacity, kPacketSize);

  // Increase more than kPacketTimeMs, but not more than the extra delay.
  fake_clock_.AdvanceTimeMilliseconds(kPacketTimeMs);
  EXPECT_CALL(receiver, DeliverRtpPacket).Times(0);
  pipe->Process();

  // Advance the network delay to get the first packet.
  fake_clock_.AdvanceTimeMilliseconds(config.queue_delay_ms);
  EXPECT_CALL(receiver, DeliverRtpPacket).Times(1);
  pipe->Process();

  // Advance one more kPacketTimeMs to get the last packet.
  fake_clock_.AdvanceTimeMilliseconds(kPacketTimeMs);
  EXPECT_CALL(receiver, DeliverRtpPacket).Times(1);
  pipe->Process();
}

// Test the number of buffers and packets are dropped when sending too many
// packets too quickly.
TEST_F(FakeNetworkPipeTest, QueueLengthTest) {
  BuiltInNetworkBehaviorConfig config;
  config.queue_length_packets = 2;
  config.link_capacity = DataRate::KilobitsPerSec(80);
  MockReceiver receiver;
  auto simulated_network = std::make_unique<SimulatedNetwork>(config);
  std::unique_ptr<FakeNetworkPipe> pipe(new FakeNetworkPipe(
      &fake_clock_, std::move(simulated_network), &receiver));

  const int kPacketSize = 1000;
  const int kPacketTimeMs = PacketTimeMs(config.link_capacity, kPacketSize);

  // Send three packets and verify only 2 are delivered.
  SendPackets(pipe.get(), 3, kPacketSize);

  // Increase time enough to deliver all three packets, verify only two are
  // delivered.
  fake_clock_.AdvanceTimeMilliseconds(3 * kPacketTimeMs);
  EXPECT_CALL(receiver, DeliverRtpPacket).Times(2);
  pipe->Process();
}

// Test we get statistics as expected.
TEST_F(FakeNetworkPipeTest, StatisticsTest) {
  BuiltInNetworkBehaviorConfig config;
  config.queue_length_packets = 2;
  config.queue_delay_ms = 20;
  config.link_capacity = DataRate::KilobitsPerSec(80);
  MockReceiver receiver;
  auto simulated_network = std::make_unique<SimulatedNetwork>(config);
  std::unique_ptr<FakeNetworkPipe> pipe(new FakeNetworkPipe(
      &fake_clock_, std::move(simulated_network), &receiver));

  const int kPacketSize = 1000;
  const int kPacketTimeMs = PacketTimeMs(config.link_capacity, kPacketSize);

  // Send three packets and verify only 2 are delivered.
  SendPackets(pipe.get(), 3, kPacketSize);
  fake_clock_.AdvanceTimeMilliseconds(3 * kPacketTimeMs +
                                      config.queue_delay_ms);

  EXPECT_CALL(receiver, DeliverRtpPacket).Times(2);
  pipe->Process();

  // Packet 1: kPacketTimeMs + config.queue_delay_ms,
  // packet 2: 2 * kPacketTimeMs + config.queue_delay_ms => 170 ms average.
  EXPECT_EQ(pipe->AverageDelay(), 170);
  EXPECT_EQ(pipe->SentPackets(), 2u);
  EXPECT_EQ(pipe->DroppedPackets(), 1u);
  EXPECT_EQ(pipe->PercentageLoss(), 1 / 3.f);
}

// Change the link capacity half-way through the test and verify that the
// delivery times change accordingly.
TEST_F(FakeNetworkPipeTest, ChangingCapacityWithEmptyPipeTest) {
  BuiltInNetworkBehaviorConfig config;
  config.queue_length_packets = 20;
  config.link_capacity = DataRate::KilobitsPerSec(80);
  MockReceiver receiver;
  std::unique_ptr<SimulatedNetwork> network(new SimulatedNetwork(config));
  SimulatedNetwork* simulated_network = network.get();
  std::unique_ptr<FakeNetworkPipe> pipe(
      new FakeNetworkPipe(&fake_clock_, std::move(network), &receiver));

  // Add 10 packets of 1000 bytes, = 80 kb, and verify it takes one second to
  // get through the pipe.
  const int kNumPackets = 10;
  const int kPacketSize = 1000;
  SendPackets(pipe.get(), kNumPackets, kPacketSize);

  // Time to get one packet through the link.
  int packet_time_ms = PacketTimeMs(config.link_capacity, kPacketSize);

  // Time hasn't increased yet, so we souldn't get any packets.
  EXPECT_CALL(receiver, DeliverRtpPacket).Times(0);
  pipe->Process();

  // Advance time in steps to release one packet at a time.
  for (int i = 0; i < kNumPackets; ++i) {
    fake_clock_.AdvanceTimeMilliseconds(packet_time_ms);
    EXPECT_CALL(receiver, DeliverRtpPacket).Times(1);
    pipe->Process();
  }

  // Change the capacity.
  config.link_capacity = config.link_capacity / 2;  // Reduce to 50%.
  simulated_network->SetConfig(config);

  // Add another 10 packets of 1000 bytes, = 80 kb, and verify it takes two
  // seconds to get them through the pipe.
  SendPackets(pipe.get(), kNumPackets, kPacketSize);

  // Time to get one packet through the link.
  packet_time_ms = PacketTimeMs(config.link_capacity, kPacketSize);

  // Time hasn't increased yet, so we souldn't get any packets.
  EXPECT_CALL(receiver, DeliverRtpPacket).Times(0);
  pipe->Process();

  // Advance time in steps to release one packet at a time.
  for (int i = 0; i < kNumPackets; ++i) {
    fake_clock_.AdvanceTimeMilliseconds(packet_time_ms);
    EXPECT_CALL(receiver, DeliverRtpPacket).Times(1);
    pipe->Process();
  }

  // Check that all the packets were sent.
  EXPECT_EQ(static_cast<size_t>(2 * kNumPackets), pipe->SentPackets());
  EXPECT_FALSE(pipe->TimeUntilNextProcess().has_value());
  fake_clock_.AdvanceTimeMilliseconds(1000);
  EXPECT_CALL(receiver, DeliverRtpPacket).Times(0);
  pipe->Process();
}

// Change the link capacity half-way through the test and verify that the
// delivery times change accordingly.
TEST_F(FakeNetworkPipeTest, ChangingCapacityWithPacketsInPipeTest) {
  BuiltInNetworkBehaviorConfig config;
  config.queue_length_packets = 20;
  config.link_capacity = DataRate::KilobitsPerSec(80);
  MockReceiver receiver;
  std::unique_ptr<SimulatedNetwork> network(new SimulatedNetwork(config));
  SimulatedNetwork* simulated_network = network.get();
  std::unique_ptr<FakeNetworkPipe> pipe(
      new FakeNetworkPipe(&fake_clock_, std::move(network), &receiver));

  // Add 20 packets of 1000 bytes, = 160 kb.
  const int kNumPackets = 20;
  const int kPacketSize = 1000;
  SendPackets(pipe.get(), kNumPackets, kPacketSize);

  // Time hasn't increased yet, so we souldn't get any packets.
  EXPECT_CALL(receiver, DeliverRtpPacket).Times(0);
  pipe->Process();

  // Advance time in steps to release half of the packets one at a time.
  int step_ms = PacketTimeMs(config.link_capacity, kPacketSize);
  for (int i = 0; i < kNumPackets / 2; ++i) {
    fake_clock_.AdvanceTimeMilliseconds(step_ms);
    EXPECT_CALL(receiver, DeliverRtpPacket).Times(1);
    pipe->Process();
  }

  // Change the capacity.
  config.link_capacity = 2 * config.link_capacity;
  simulated_network->SetConfig(config);

  // Advance time in steps to release remaining packets one at a time.
  step_ms = PacketTimeMs(config.link_capacity, kPacketSize);
  for (int i = 0; i < kNumPackets / 2; ++i) {
    fake_clock_.AdvanceTimeMilliseconds(step_ms);
    EXPECT_CALL(receiver, DeliverRtpPacket).Times(1);
    pipe->Process();
  }

  // Check that all the packets were sent.
  EXPECT_EQ(static_cast<size_t>(kNumPackets), pipe->SentPackets());
  EXPECT_FALSE(pipe->TimeUntilNextProcess().has_value());
  fake_clock_.AdvanceTimeMilliseconds(1000);
  EXPECT_CALL(receiver, DeliverRtpPacket).Times(0);
  pipe->Process();
}

// At first disallow reordering and then allow reordering.
TEST_F(FakeNetworkPipeTest, DisallowReorderingThenAllowReordering) {
  BuiltInNetworkBehaviorConfig config;
  config.queue_length_packets = 1000;
  config.link_capacity = DataRate::KilobitsPerSec(80);
  config.queue_delay_ms = 100;
  config.delay_standard_deviation_ms = 10;
  ReorderTestReceiver receiver;
  std::unique_ptr<SimulatedNetwork> network(new SimulatedNetwork(config));
  SimulatedNetwork* simulated_network = network.get();
  std::unique_ptr<FakeNetworkPipe> pipe(
      new FakeNetworkPipe(&fake_clock_, std::move(network), &receiver));

  const uint32_t kNumPackets = 100;
  const int kPacketSize = 10;
  SendPackets(pipe.get(), kNumPackets, kPacketSize);
  fake_clock_.AdvanceTimeMilliseconds(1000);
  pipe->Process();

  // Confirm that all packets have been delivered in order.
  EXPECT_EQ(kNumPackets, receiver.delivered_sequence_numbers_.size());
  int last_seq_num = -1;
  for (int seq_num : receiver.delivered_sequence_numbers_) {
    EXPECT_GT(seq_num, last_seq_num);
    last_seq_num = seq_num;
  }

  config.allow_reordering = true;
  simulated_network->SetConfig(config);
  SendPackets(pipe.get(), kNumPackets, kPacketSize);
  fake_clock_.AdvanceTimeMilliseconds(1000);
  receiver.delivered_sequence_numbers_.clear();
  pipe->Process();

  // Confirm that all packets have been delivered
  // and that reordering has occured.
  EXPECT_EQ(kNumPackets, receiver.delivered_sequence_numbers_.size());
  bool reordering_has_occured = false;
  last_seq_num = -1;
  for (int seq_num : receiver.delivered_sequence_numbers_) {
    if (last_seq_num > seq_num) {
      reordering_has_occured = true;
      break;
    }
    last_seq_num = seq_num;
  }
  EXPECT_TRUE(reordering_has_occured);
}

TEST_F(FakeNetworkPipeTest, BurstLoss) {
  const int kLossPercent = 5;
  const int kAvgBurstLength = 3;
  const int kNumPackets = 10000;
  const int kPacketSize = 10;

  BuiltInNetworkBehaviorConfig config;
  config.queue_length_packets = kNumPackets;
  config.loss_percent = kLossPercent;
  config.avg_burst_loss_length = kAvgBurstLength;
  ReorderTestReceiver receiver;
  auto simulated_network = std::make_unique<SimulatedNetwork>(config);
  std::unique_ptr<FakeNetworkPipe> pipe(new FakeNetworkPipe(
      &fake_clock_, std::move(simulated_network), &receiver));

  SendPackets(pipe.get(), kNumPackets, kPacketSize);
  fake_clock_.AdvanceTimeMilliseconds(1000);
  pipe->Process();

  // Check that the average loss is `kLossPercent` percent.
  int lost_packets = kNumPackets - receiver.delivered_sequence_numbers_.size();
  double loss_fraction = lost_packets / static_cast<double>(kNumPackets);

  EXPECT_NEAR(kLossPercent / 100.0, loss_fraction, 0.05);

  // Find the number of bursts that has occurred.
  size_t received_packets = receiver.delivered_sequence_numbers_.size();
  int num_bursts = 0;
  for (size_t i = 0; i < received_packets - 1; ++i) {
    int diff = receiver.delivered_sequence_numbers_[i + 1] -
               receiver.delivered_sequence_numbers_[i];
    if (diff > 1)
      ++num_bursts;
  }

  double average_burst_length = static_cast<double>(lost_packets) / num_bursts;

  EXPECT_NEAR(kAvgBurstLength, average_burst_length, 0.3);
}

TEST_F(FakeNetworkPipeTest, SetReceiver) {
  BuiltInNetworkBehaviorConfig config;
  config.link_capacity = DataRate::KilobitsPerSec(800);
  MockReceiver receiver;
  auto simulated_network = std::make_unique<SimulatedNetwork>(config);
  std::unique_ptr<FakeNetworkPipe> pipe(new FakeNetworkPipe(
      &fake_clock_, std::move(simulated_network), &receiver));

  const int kPacketSize = 1000;
  const int kPacketTimeMs = PacketTimeMs(config.link_capacity, kPacketSize);
  SendPackets(pipe.get(), 1, kPacketSize);
  fake_clock_.AdvanceTimeMilliseconds(kPacketTimeMs);
  EXPECT_CALL(receiver, DeliverRtpPacket).Times(1);
  pipe->Process();

  MockReceiver new_receiver;
  pipe->SetReceiver(&new_receiver);

  SendPackets(pipe.get(), 1, kPacketSize);
  fake_clock_.AdvanceTimeMilliseconds(kPacketTimeMs);
  EXPECT_CALL(receiver, DeliverRtpPacket).Times(0);
  EXPECT_CALL(new_receiver, DeliverRtpPacket).Times(1);
  pipe->Process();
}

TEST_F(FakeNetworkPipeTest, DeliverRtpPacketSetsCorrectArrivalTime) {
  BuiltInNetworkBehaviorConfig config;
  config.queue_delay_ms = 100;
  MockReceiver receiver;
  auto simulated_network = std::make_unique<SimulatedNetwork>(config);
  std::unique_ptr<FakeNetworkPipe> pipe(new FakeNetworkPipe(
      &fake_clock_, std::move(simulated_network), &receiver));

  Timestamp send_time = fake_clock_.CurrentTime();
  RtpPacketReceived packet(nullptr, send_time);
  packet.SetExtension<TransportSequenceNumber>(123);
  pipe->DeliverRtpPacket(MediaType::VIDEO, std::move(packet),
                         [](const RtpPacketReceived&) { return false; });

  // Advance the network delay to get the first packet.
  fake_clock_.AdvanceTimeMilliseconds(config.queue_delay_ms);
  EXPECT_CALL(receiver, DeliverRtpPacket(MediaType::VIDEO, _, _))
      .WillOnce(WithArg<1>([&](RtpPacketReceived packet) {
        EXPECT_EQ(packet.arrival_time(),
                  send_time + TimeDelta::Millis(config.queue_delay_ms));
      }));
  pipe->Process();
}

TEST_F(FakeNetworkPipeTest, DeliverRtpPacketPropagatesExtensions) {
  BuiltInNetworkBehaviorConfig config;
  config.queue_delay_ms = 100;
  MockReceiver receiver;
  auto simulated_network = std::make_unique<SimulatedNetwork>(config);
  std::unique_ptr<FakeNetworkPipe> pipe(new FakeNetworkPipe(
      &fake_clock_, std::move(simulated_network), &receiver));
  RtpHeaderExtensionMap extension_map;
  extension_map.Register<TransportSequenceNumber>(/*id=*/7);

  RtpPacketReceived packet(&extension_map, fake_clock_.CurrentTime());
  packet.SetExtension<TransportSequenceNumber>(123);
  pipe->DeliverRtpPacket(MediaType::VIDEO, std::move(packet),
                         [](const RtpPacketReceived&) { return false; });

  // Advance the network delay to get the first packet.
  fake_clock_.AdvanceTimeMilliseconds(config.queue_delay_ms);
  EXPECT_CALL(receiver, DeliverRtpPacket(MediaType::VIDEO, _, _))
      .WillOnce(WithArg<1>([](RtpPacketReceived packet) {
        EXPECT_EQ(packet.GetExtension<TransportSequenceNumber>(), 123);
      }));
  pipe->Process();
}

TEST_F(FakeNetworkPipeTest, DeliverRtcpPacket) {
  BuiltInNetworkBehaviorConfig config;
  config.queue_delay_ms = 100;
  MockReceiver receiver;
  auto simulated_network = std::make_unique<SimulatedNetwork>(config);
  std::unique_ptr<FakeNetworkPipe> pipe(new FakeNetworkPipe(
      &fake_clock_, std::move(simulated_network), &receiver));

  CopyOnWriteBuffer buffer(100);
  memset(buffer.MutableData(), 0, 100);
  pipe->DeliverRtcpPacket(std::move(buffer));

  // Advance the network delay to get the first packet.
  fake_clock_.AdvanceTimeMilliseconds(config.queue_delay_ms);
  EXPECT_CALL(receiver,
              DeliverRtcpPacket(Property(&CopyOnWriteBuffer::size, 100)));
  pipe->Process();
}

}  // namespace webrtc
