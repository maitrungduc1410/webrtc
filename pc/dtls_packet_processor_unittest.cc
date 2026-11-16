/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "pc/dtls_packet_processor.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "api/environment/environment.h"
#include "api/transport/ecn_marking.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "pc/congestion_control_feedback_block.h"
#include "pc/congestion_control_feedback_report.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "system_wrappers/include/clock.h"
#include "test/create_test_environment.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::IsEmpty;
using ::testing::Le;
using ::testing::SizeIs;

constexpr Timestamp kStartTime = Timestamp::Seconds(1000);
constexpr TimeDelta kMinFeedbackInterval = TimeDelta::Millis(25);
constexpr TimeDelta kMaxFeedbackInterval = TimeDelta::Millis(250);

// Blocks that fit a 1200 byte feedback packet after its 10 byte header.
constexpr size_t kMaxFeedbackBlocksPerPacket = 595;

// Type byte plus transport sequence number.
constexpr size_t kDataHeaderSize = 3;

std::vector<uint8_t> ToVector(const CopyOnWriteBuffer& buffer) {
  return std::vector<uint8_t>(buffer.begin(), buffer.end());
}

uint16_t TransportSeq(const CopyOnWriteBuffer& buffer) {
  return static_cast<uint16_t>((static_cast<uint16_t>(buffer[1]) << 8) |
                               buffer[2]);
}

CopyOnWriteBuffer DataPacket(uint16_t seq) {
  return CopyOnWriteBuffer(std::vector<uint8_t>{
      DtlsPacketProcessor::kPacketTypeData, static_cast<uint8_t>(seq >> 8),
      static_cast<uint8_t>(seq & 0xFF), 0xAA});
}

class DtlsPacketProcessorTest : public ::testing::Test {
 public:
  DtlsPacketProcessorTest()
      : clock_(kStartTime),
        env_(CreateTestEnvironment({.time = &clock_})),
        processor_(env_, [this](CopyOnWriteBuffer packet) {
          sent_packets_.push_back(packet);
        }) {}

 protected:
  void ReceiveData(uint16_t seq, EcnMarking ecn = EcnMarking::kNotEct) {
    processor_.ProcessIncomingPacket(DataPacket(seq), clock_.CurrentTime(),
                                     ecn);
  }

  CopyOnWriteBuffer SendData(std::span<const uint8_t> payload) {
    return processor_.ProcessOutgoingPacket(payload, clock_.CurrentTime());
  }

  TimeDelta AdvanceAndProcess(TimeDelta delta) {
    clock_.AdvanceTime(delta);
    return processor_.ProcessTimers(clock_.CurrentTime());
  }

  CongestionControlFeedbackReport DecodeFeedback(
      const CopyOnWriteBuffer& packet) {
    EXPECT_EQ(packet[0], DtlsPacketProcessor::kPacketTypeFeedback);
    std::optional<CongestionControlFeedbackReport> report =
        CongestionControlFeedbackReport::Decode(
            std::span<const uint8_t>(packet.cdata(), packet.size()).subspan(1));
    EXPECT_TRUE(report.has_value());
    return report.value_or(CongestionControlFeedbackReport());
  }

  CongestionControlFeedbackReport DecodeFeedback(size_t index) {
    return DecodeFeedback(sent_packets_[index]);
  }

  CongestionControlFeedbackReport DecodePiggybackedFeedback(
      const CopyOnWriteBuffer& packet) {
    EXPECT_EQ(packet[0], DtlsPacketProcessor::kPacketTypeDataWithFeedback);
    std::optional<CongestionControlFeedbackReport> report =
        CongestionControlFeedbackReport::Decode(
            std::span<const uint8_t>(packet.cdata(), packet.size())
                .subspan(kDataHeaderSize));
    EXPECT_TRUE(report.has_value());
    return report.value_or(CongestionControlFeedbackReport());
  }

  SimulatedClock clock_;
  const Environment env_;
  std::vector<CopyOnWriteBuffer> sent_packets_;
  DtlsPacketProcessor processor_;
};

TEST_F(DtlsPacketProcessorTest, FramesAndStripsDataOnlyPacket) {
  std::vector<uint8_t> payload = {1, 2, 3, 4, 5};
  CopyOnWriteBuffer wire =
      processor_.ProcessOutgoingPacket(payload, clock_.CurrentTime());

  EXPECT_EQ(wire[0], DtlsPacketProcessor::kPacketTypeData);
  EXPECT_EQ(wire.size(), payload.size() + 3);

  std::optional<CopyOnWriteBuffer> app_payload =
      processor_.ProcessIncomingPacket(wire, clock_.CurrentTime(),
                                       EcnMarking::kNotEct);
  ASSERT_TRUE(app_payload.has_value());
  EXPECT_EQ(ToVector(*app_payload), payload);
}

TEST_F(DtlsPacketProcessorTest, TransportSequenceNumberIncrements) {
  std::vector<uint8_t> payload = {9};
  CopyOnWriteBuffer p0 =
      processor_.ProcessOutgoingPacket(payload, clock_.CurrentTime());
  CopyOnWriteBuffer p1 =
      processor_.ProcessOutgoingPacket(payload, clock_.CurrentTime());

  EXPECT_EQ(TransportSeq(p1), static_cast<uint16_t>(TransportSeq(p0) + 1));
}

TEST_F(DtlsPacketProcessorTest, ReservedTypesAreDropped) {
  for (uint8_t type : std::vector<uint8_t>{0x00, 0x04, 0xFF}) {
    CopyOnWriteBuffer junk(std::vector<uint8_t>{type, 1, 2, 3});
    EXPECT_FALSE(processor_
                     .ProcessIncomingPacket(junk, clock_.CurrentTime(),
                                            EcnMarking::kNotEct)
                     .has_value())
        << "type " << static_cast<int>(type);
  }
}

TEST_F(DtlsPacketProcessorTest, EmptyPacketIsDropped) {
  EXPECT_FALSE(processor_
                   .ProcessIncomingPacket(CopyOnWriteBuffer(),
                                          clock_.CurrentTime(),
                                          EcnMarking::kNotEct)
                   .has_value());
}

TEST_F(DtlsPacketProcessorTest, TruncatedDataPacketIsDropped) {
  // Type byte present but missing part of the sequence number.
  CopyOnWriteBuffer truncated(
      std::vector<uint8_t>{DtlsPacketProcessor::kPacketTypeData, 0x00});
  EXPECT_FALSE(processor_
                   .ProcessIncomingPacket(truncated, clock_.CurrentTime(),
                                          EcnMarking::kNotEct)
                   .has_value());
}

TEST_F(DtlsPacketProcessorTest, SendsNoFeedbackBeforeTheMinimumInterval) {
  ReceiveData(0);

  AdvanceAndProcess(kMinFeedbackInterval - TimeDelta::Millis(1));

  EXPECT_THAT(sent_packets_, IsEmpty());
}

TEST_F(DtlsPacketProcessorTest, SendsFeedbackAfterTheMinimumInterval) {
  ReceiveData(0);

  AdvanceAndProcess(kMinFeedbackInterval);

  EXPECT_THAT(sent_packets_, SizeIs(1));
}

TEST_F(DtlsPacketProcessorTest, SendsNoKeepAliveBeforeTheMaximumInterval) {
  AdvanceAndProcess(kMaxFeedbackInterval - TimeDelta::Millis(1));

  EXPECT_THAT(sent_packets_, IsEmpty());
}

TEST_F(DtlsPacketProcessorTest, SendsKeepAliveAfterTheMaximumInterval) {
  AdvanceAndProcess(kMaxFeedbackInterval);

  ASSERT_THAT(sent_packets_, SizeIs(1));
  EXPECT_THAT(DecodeFeedback(0).blocks, IsEmpty());
}

TEST_F(DtlsPacketProcessorTest, SendsFeedbackOnArrivalOnceItIsDue) {
  clock_.AdvanceTime(kMinFeedbackInterval);

  ReceiveData(0);

  EXPECT_THAT(sent_packets_, SizeIs(1));
}

TEST_F(DtlsPacketProcessorTest, TimesFeedbackOnArrivalByTheClock) {
  clock_.AdvanceTime(kMaxFeedbackInterval);

  // Processed a while after it arrived.
  processor_.ProcessIncomingPacket(DataPacket(0), kStartTime,
                                   EcnMarking::kNotEct);

  ASSERT_THAT(sent_packets_, SizeIs(1));
  CongestionControlFeedbackReport report = DecodeFeedback(0);
  ASSERT_THAT(report.blocks, SizeIs(1));
  EXPECT_EQ(report.blocks[0].arrival_time_offset, kMaxFeedbackInterval);
}

TEST_F(DtlsPacketProcessorTest, ReturnsTimeUntilTheNextFeedback) {
  ReceiveData(0);

  EXPECT_EQ(AdvanceAndProcess(TimeDelta::Millis(10)), TimeDelta::Millis(15));
}

TEST_F(DtlsPacketProcessorTest, WaitsAtMostTheMinimumIntervalWhileIdle) {
  EXPECT_EQ(processor_.ProcessTimers(clock_.CurrentTime()),
            kMinFeedbackInterval);
}

TEST_F(DtlsPacketProcessorTest, FeedbackReportsArrivalTimeAndEcn) {
  ReceiveData(/*seq=*/5, EcnMarking::kCe);

  // A whole number of arrival time offset ticks, which are 1/1024 seconds.
  AdvanceAndProcess(TimeDelta::Millis(250));

  ASSERT_THAT(sent_packets_, SizeIs(1));
  CongestionControlFeedbackReport report = DecodeFeedback(0);
  EXPECT_EQ(report.begin_seq, 5);
  ASSERT_THAT(report.blocks, SizeIs(1));
  EXPECT_TRUE(report.blocks[0].received);
  EXPECT_EQ(report.blocks[0].ecn, EcnMarking::kCe);
  EXPECT_EQ(report.blocks[0].arrival_time_offset, TimeDelta::Millis(250));
}

TEST_F(DtlsPacketProcessorTest, FeedbackReportsMissingPacketsAsNotReceived) {
  ReceiveData(1);
  ReceiveData(4);

  AdvanceAndProcess(kMinFeedbackInterval);

  ASSERT_THAT(sent_packets_, SizeIs(1));
  CongestionControlFeedbackReport report = DecodeFeedback(0);
  EXPECT_EQ(report.begin_seq, 1);
  ASSERT_THAT(report.blocks, SizeIs(4));
  EXPECT_TRUE(report.blocks[0].received);
  EXPECT_FALSE(report.blocks[1].received);
  EXPECT_FALSE(report.blocks[2].received);
  EXPECT_TRUE(report.blocks[3].received);
}

TEST_F(DtlsPacketProcessorTest, FeedbackCountAdvancesPerReport) {
  ReceiveData(0);
  AdvanceAndProcess(kMinFeedbackInterval);
  ReceiveData(1);
  AdvanceAndProcess(kMinFeedbackInterval);

  ASSERT_THAT(sent_packets_, SizeIs(2));
  EXPECT_EQ(DecodeFeedback(0).feedback_count, 0);
  EXPECT_EQ(DecodeFeedback(1).feedback_count, 1);
}

TEST_F(DtlsPacketProcessorTest, SplitsAnOversizedReportAcrossPackets) {
  ReceiveData(0);
  ReceiveData(700);

  AdvanceAndProcess(kMinFeedbackInterval);

  ASSERT_THAT(sent_packets_, SizeIs(1));
  EXPECT_THAT(sent_packets_[0], SizeIs(Le(1200u)));
  CongestionControlFeedbackReport first = DecodeFeedback(0);
  EXPECT_EQ(first.begin_seq, 0);
  EXPECT_THAT(first.blocks, SizeIs(kMaxFeedbackBlocksPerPacket));

  AdvanceAndProcess(kMinFeedbackInterval);

  ASSERT_THAT(sent_packets_, SizeIs(2));
  CongestionControlFeedbackReport second = DecodeFeedback(1);
  EXPECT_EQ(second.begin_seq, kMaxFeedbackBlocksPerPacket);
  ASSERT_THAT(second.blocks, SizeIs(701 - kMaxFeedbackBlocksPerPacket));
  EXPECT_TRUE(second.blocks.back().received);
  EXPECT_EQ(second.feedback_count, 1);
}

TEST_F(DtlsPacketProcessorTest, SpreadsALargeBacklogAcrossFeedbackIntervals) {
  ReceiveData(0);
  ReceiveData(2000);

  EXPECT_EQ(AdvanceAndProcess(kMinFeedbackInterval), kMinFeedbackInterval);
  EXPECT_THAT(sent_packets_, SizeIs(1));

  EXPECT_EQ(AdvanceAndProcess(kMinFeedbackInterval), kMinFeedbackInterval);
  EXPECT_THAT(sent_packets_, SizeIs(2));
}

TEST_F(DtlsPacketProcessorTest, FeedbackPacketCarriesNoApplicationPayload) {
  ReceiveData(0);
  AdvanceAndProcess(kMinFeedbackInterval);
  ASSERT_THAT(sent_packets_, SizeIs(1));

  EXPECT_FALSE(processor_
                   .ProcessIncomingPacket(sent_packets_[0],
                                          clock_.CurrentTime(),
                                          EcnMarking::kNotEct)
                   .has_value());
}

TEST_F(DtlsPacketProcessorTest, ReceivedFeedbackIsNotReportedAsAnArrival) {
  ReceiveData(0);
  AdvanceAndProcess(kMinFeedbackInterval);
  ASSERT_THAT(sent_packets_, SizeIs(1));
  CopyOnWriteBuffer feedback = sent_packets_[0];
  sent_packets_.clear();

  processor_.ProcessIncomingPacket(feedback, clock_.CurrentTime(),
                                   EcnMarking::kNotEct);
  AdvanceAndProcess(kMaxFeedbackInterval);

  ASSERT_THAT(sent_packets_, SizeIs(1));
  EXPECT_THAT(DecodeFeedback(0).blocks, IsEmpty());
}

TEST_F(DtlsPacketProcessorTest, TruncatedFeedbackPacketIsDropped) {
  ReceiveData(0);
  AdvanceAndProcess(kMinFeedbackInterval);
  ASSERT_THAT(sent_packets_, SizeIs(1));
  CopyOnWriteBuffer truncated = sent_packets_[0].Slice(
      0, sent_packets_[0].size() - CongestionControlFeedbackBlock::kBytes);

  EXPECT_FALSE(processor_
                   .ProcessIncomingPacket(truncated, clock_.CurrentTime(),
                                          EcnMarking::kNotEct)
                   .has_value());
}

TEST_F(DtlsPacketProcessorTest, AcceptsAPeerReportOnThePacketsItSent) {
  std::vector<CopyOnWriteBuffer> peer_packets;
  DtlsPacketProcessor peer(
      env_, [&](CopyOnWriteBuffer packet) { peer_packets.push_back(packet); });

  std::vector<uint8_t> payload = {1, 2, 3};
  CopyOnWriteBuffer sent = SendData(payload);
  peer.ProcessIncomingPacket(sent, clock_.CurrentTime(), EcnMarking::kEct1);
  clock_.AdvanceTime(kMinFeedbackInterval);
  peer.ProcessTimers(clock_.CurrentTime());

  ASSERT_THAT(peer_packets, SizeIs(1));
  CongestionControlFeedbackReport report = DecodeFeedback(peer_packets[0]);
  EXPECT_EQ(report.begin_seq, TransportSeq(sent));
  ASSERT_THAT(report.blocks, SizeIs(1));
  EXPECT_TRUE(report.blocks[0].received);
  EXPECT_EQ(report.blocks[0].ecn, EcnMarking::kEct1);

  EXPECT_FALSE(processor_
                   .ProcessIncomingPacket(peer_packets[0], clock_.CurrentTime(),
                                          EcnMarking::kNotEct)
                   .has_value());
}

TEST_F(DtlsPacketProcessorTest, RecordsArrivalWithoutATransportTimestamp) {
  processor_.ProcessIncomingPacket(DataPacket(0), Timestamp::MinusInfinity(),
                                   EcnMarking::kNotEct);

  AdvanceAndProcess(kMinFeedbackInterval);

  ASSERT_THAT(sent_packets_, SizeIs(1));
  CongestionControlFeedbackReport report = DecodeFeedback(0);
  ASSERT_THAT(report.blocks, SizeIs(1));
  EXPECT_TRUE(report.blocks[0].received);
}

TEST_F(DtlsPacketProcessorTest, PiggybacksDueFeedbackOnOutgoingData) {
  ReceiveData(0);
  clock_.AdvanceTime(kMinFeedbackInterval);

  std::vector<uint8_t> payload = {1, 2, 3};
  CopyOnWriteBuffer wire = SendData(payload);

  EXPECT_THAT(sent_packets_, IsEmpty());
  CongestionControlFeedbackReport report = DecodePiggybackedFeedback(wire);
  EXPECT_EQ(report.begin_seq, 0);
  ASSERT_THAT(report.blocks, SizeIs(1));
  EXPECT_TRUE(report.blocks[0].received);
}

TEST_F(DtlsPacketProcessorTest, SendsDataOnlyWhileNoFeedbackIsDue) {
  ReceiveData(0);

  std::vector<uint8_t> payload = {1, 2, 3};
  EXPECT_EQ(SendData(payload)[0], DtlsPacketProcessor::kPacketTypeData);
}

TEST_F(DtlsPacketProcessorTest, PiggybackedPacketRoundTripsItsPayload) {
  ReceiveData(0);
  clock_.AdvanceTime(kMinFeedbackInterval);
  std::vector<uint8_t> payload = {1, 2, 3, 4, 5};
  CopyOnWriteBuffer wire = SendData(payload);
  ASSERT_EQ(wire[0], DtlsPacketProcessor::kPacketTypeDataWithFeedback);

  std::optional<CopyOnWriteBuffer> app_payload =
      processor_.ProcessIncomingPacket(wire, clock_.CurrentTime(),
                                       EcnMarking::kNotEct);

  ASSERT_TRUE(app_payload.has_value());
  EXPECT_EQ(ToVector(*app_payload), payload);
}

TEST_F(DtlsPacketProcessorTest, PiggybackedFeedbackAdvancesTheCadence) {
  ReceiveData(0);
  clock_.AdvanceTime(kMinFeedbackInterval);
  std::vector<uint8_t> payload = {1};
  ASSERT_EQ(SendData(payload)[0],
            DtlsPacketProcessor::kPacketTypeDataWithFeedback);

  AdvanceAndProcess(kMinFeedbackInterval);
  EXPECT_THAT(sent_packets_, IsEmpty());

  AdvanceAndProcess(kMaxFeedbackInterval);
  EXPECT_THAT(sent_packets_, SizeIs(1));
}

TEST_F(DtlsPacketProcessorTest, PiggybackedReportSharesTheFeedbackCount) {
  ReceiveData(0);
  clock_.AdvanceTime(kMinFeedbackInterval);
  std::vector<uint8_t> payload = {1};
  EXPECT_EQ(DecodePiggybackedFeedback(SendData(payload)).feedback_count, 0);

  ReceiveData(1);
  AdvanceAndProcess(kMinFeedbackInterval);

  ASSERT_THAT(sent_packets_, SizeIs(1));
  EXPECT_EQ(DecodeFeedback(0).feedback_count, 1);
}

TEST_F(DtlsPacketProcessorTest, PiggybacksAKeepAliveReport) {
  clock_.AdvanceTime(kMaxFeedbackInterval);

  std::vector<uint8_t> payload = {1};
  EXPECT_THAT(DecodePiggybackedFeedback(SendData(payload)).blocks, IsEmpty());
}

TEST_F(DtlsPacketProcessorTest,
       SendsFeedbackOnItsOwnWhenThePayloadFillsTheMtu) {
  ReceiveData(0);
  clock_.AdvanceTime(kMinFeedbackInterval);

  std::vector<uint8_t> payload(1200, 0xAB);
  CopyOnWriteBuffer wire = SendData(payload);

  EXPECT_EQ(wire[0], DtlsPacketProcessor::kPacketTypeData);
  ASSERT_THAT(sent_packets_, SizeIs(1));
  EXPECT_THAT(DecodeFeedback(0).blocks, SizeIs(1));
}

TEST_F(DtlsPacketProcessorTest, SendsAnOversizedReportOnItsOwn) {
  ReceiveData(0);
  ReceiveData(700);
  clock_.AdvanceTime(kMinFeedbackInterval);

  std::vector<uint8_t> payload = {1};
  CopyOnWriteBuffer wire = SendData(payload);

  EXPECT_EQ(wire[0], DtlsPacketProcessor::kPacketTypeData);
  ASSERT_THAT(sent_packets_, SizeIs(1));
  EXPECT_THAT(DecodeFeedback(0).blocks, SizeIs(kMaxFeedbackBlocksPerPacket));
}

TEST_F(DtlsPacketProcessorTest, RecordsTheArrivalOfADataWithFeedbackPacket) {
  CongestionControlFeedbackReport report;
  std::vector<uint8_t> wire(kDataHeaderSize + report.SizeBytes() + 2);
  wire[0] = DtlsPacketProcessor::kPacketTypeDataWithFeedback;
  wire[2] = 7;
  report.Encode(std::span<uint8_t>(wire).subspan(kDataHeaderSize));
  wire[wire.size() - 2] = 0xDE;
  wire[wire.size() - 1] = 0xAD;

  std::optional<CopyOnWriteBuffer> app_payload =
      processor_.ProcessIncomingPacket(
          CopyOnWriteBuffer(wire), clock_.CurrentTime(), EcnMarking::kNotEct);

  ASSERT_TRUE(app_payload.has_value());
  EXPECT_EQ(ToVector(*app_payload), std::vector<uint8_t>({0xDE, 0xAD}));
  AdvanceAndProcess(kMinFeedbackInterval);
  ASSERT_THAT(sent_packets_, SizeIs(1));
  EXPECT_EQ(DecodeFeedback(0).begin_seq, 7);
}

TEST_F(DtlsPacketProcessorTest,
       DataWithFeedbackPacketWithoutPayloadIsAccepted) {
  CongestionControlFeedbackReport report;
  std::vector<uint8_t> wire(kDataHeaderSize + report.SizeBytes());
  wire[0] = DtlsPacketProcessor::kPacketTypeDataWithFeedback;
  report.Encode(std::span<uint8_t>(wire).subspan(kDataHeaderSize));

  std::optional<CopyOnWriteBuffer> app_payload =
      processor_.ProcessIncomingPacket(
          CopyOnWriteBuffer(wire), clock_.CurrentTime(), EcnMarking::kNotEct);

  ASSERT_TRUE(app_payload.has_value());
  EXPECT_THAT(*app_payload, IsEmpty());
}

TEST_F(DtlsPacketProcessorTest, TruncatedDataWithFeedbackPacketIsDropped) {
  ReceiveData(0);
  clock_.AdvanceTime(kMinFeedbackInterval);
  std::vector<uint8_t> payload = {1};
  CopyOnWriteBuffer wire = SendData(payload);
  ASSERT_EQ(wire[0], DtlsPacketProcessor::kPacketTypeDataWithFeedback);

  CopyOnWriteBuffer truncated = wire.Slice(0, wire.size() - payload.size() - 1);

  EXPECT_FALSE(processor_
                   .ProcessIncomingPacket(truncated, clock_.CurrentTime(),
                                          EcnMarking::kNotEct)
                   .has_value());
}

}  // namespace
}  // namespace webrtc
