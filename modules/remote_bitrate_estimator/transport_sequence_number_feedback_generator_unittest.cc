/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/remote_bitrate_estimator/transport_sequence_number_feedback_generator.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "api/rtp_header_extension_id.h"
#include "api/rtp_headers.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/rtcp_packet.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/buffer.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::_;
using ::testing::ElementsAre;

using ::testing::MockFunction;
using ::testing::Return;
using ::testing::SizeIs;

constexpr uint32_t kMediaSsrc = 456;
constexpr uint16_t kBaseSeq = 10;
constexpr Timestamp kBaseTime = Timestamp::Millis(123);
constexpr TimeDelta kBaseTimeWrapAround =
    rtcp::TransportFeedback::kDeltaTick * (int64_t{1} << 32);
constexpr TimeDelta kMaxSmallDelta = rtcp::TransportFeedback::kDeltaTick * 0xFF;

constexpr TimeDelta kBackWindow = TimeDelta::Millis(500);
constexpr TimeDelta kMinSendInterval = TimeDelta::Millis(50);
constexpr TimeDelta kMaxSendInterval = TimeDelta::Millis(250);
constexpr TimeDelta kDefaultSendInterval = TimeDelta::Millis(100);

std::vector<uint16_t> SequenceNumbers(
    const rtcp::TransportFeedback& feedback_packet) {
  std::vector<uint16_t> sequence_numbers;
  for (const auto& rtp_packet_received : feedback_packet.GetReceivedPackets()) {
    sequence_numbers.push_back(rtp_packet_received.sequence_number());
  }
  return sequence_numbers;
}

std::vector<Timestamp> Timestamps(
    const rtcp::TransportFeedback& feedback_packet) {
  std::vector<Timestamp> timestamps;
  Timestamp timestamp = feedback_packet.BaseTime();
  // rtcp::TransportFeedback makes no promises about epoch of the base time,
  // It may add several kBaseTimeWrapAround periods to make it large enough and
  // thus to support negative deltas. Align it close to the kBaseTime to make
  // tests expectations simpler.
  if (timestamp > kBaseTime) {
    timestamp -= (timestamp - kBaseTime).RoundTo(kBaseTimeWrapAround);
  }
  for (const auto& rtp_packet_received : feedback_packet.GetReceivedPackets()) {
    timestamp += rtp_packet_received.delta();
    timestamps.push_back(timestamp);
  }
  return timestamps;
}

// The maximum packet size of RTP modules sending media, to which the feedback
// generator limits feedback packets.
constexpr size_t kMaxRtcpPacketSizeBytes = 1200;

// What the receiver of feedback packets parses from them.
struct ParsedFeedback {
  int num_feedback_packets = 0;
  // Number of RTP packets reported as received or as lost.
  size_t num_statuses = 0;
  std::vector<uint16_t> received_sequence_numbers;
};

class TransportSequenceNumberFeedbackGeneneratorTest : public ::testing::Test {
 public:
  TransportSequenceNumberFeedbackGeneneratorTest()
      : clock_(0), feedback_generator_(feedback_sender_.AsStdFunction()) {}

 protected:
  void IncomingPacket(uint16_t seq,
                      Timestamp arrival_time,
                      std::optional<uint32_t> abs_send_time = std::nullopt) {
    RtpHeaderExtensionMap map;
    map.Register<TransportSequenceNumber>(RtpHeaderExtensionId(1));
    map.Register<AbsoluteSendTime>(RtpHeaderExtensionId(2));
    RtpPacketReceived packet(&map, arrival_time);
    packet.SetSsrc(kMediaSsrc);
    packet.SetExtension<TransportSequenceNumber>(seq);
    if (abs_send_time) {
      packet.SetExtension<AbsoluteSendTime>(*abs_send_time);
    }
    feedback_generator_.OnReceivedPacket(packet);
  }

  void IncomingPacketV2(
      uint16_t seq,
      Timestamp arrival_time,
      std::optional<FeedbackRequest> feedback_request = std::nullopt) {
    RtpHeaderExtensionMap map;
    map.Register<TransportSequenceNumberV2>(RtpHeaderExtensionId(1));
    RtpPacketReceived packet(&map, arrival_time);
    packet.SetSsrc(kMediaSsrc);
    packet.SetExtension<TransportSequenceNumberV2>(seq, feedback_request);
    feedback_generator_.OnReceivedPacket(packet);
  }

  void Process() {
    clock_.AdvanceTime(kDefaultSendInterval);
    feedback_generator_.Process(clock_.CurrentTime());
  }

  // Expects feedback packets of at most kMaxRtcpPacketSizeBytes, one per call
  // of the feedback sender, and adds what their receiver parses to `feedback`.
  void ExpectFeedbackInPacketsThatFitIntoOneRtcpPacket(
      ParsedFeedback& feedback) {
    EXPECT_CALL(feedback_sender_, Call)
        .WillRepeatedly(
            [&feedback](
                std::vector<std::unique_ptr<rtcp::RtcpPacket>> packets) {
              // Packets passed in one call may be combined into one compound
              // RTCP packet, which receivers may not handle.
              ASSERT_THAT(packets, SizeIs(1));
              Buffer buffer = packets[0]->Build();
              EXPECT_LE(buffer.size(), kMaxRtcpPacketSizeBytes);
              std::unique_ptr<rtcp::TransportFeedback> parsed =
                  rtcp::TransportFeedback::ParseFrom(buffer.data(),
                                                     buffer.size());
              ASSERT_NE(parsed, nullptr);
              ++feedback.num_feedback_packets;
              feedback.num_statuses += parsed->GetPacketStatusCount();
              for (uint16_t sequence_number : SequenceNumbers(*parsed)) {
                feedback.received_sequence_numbers.push_back(sequence_number);
              }
            });
  }

  SimulatedClock clock_;
  MockFunction<void(std::vector<std::unique_ptr<rtcp::RtcpPacket>>)>
      feedback_sender_;
  TransportSequenceNumberFeedbackGenenerator feedback_generator_;
};

TEST_F(TransportSequenceNumberFeedbackGeneneratorTest,
       SendsSinglePacketFeedback) {
  IncomingPacket(kBaseSeq, kBaseTime);

  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(
          [](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq, feedback_packet->GetBaseSequence());
            EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq));
            EXPECT_THAT(Timestamps(*feedback_packet), ElementsAre(kBaseTime));
          });

  Process();
}

TEST_F(TransportSequenceNumberFeedbackGeneneratorTest, DuplicatedPackets) {
  IncomingPacket(kBaseSeq, kBaseTime);
  IncomingPacket(kBaseSeq, kBaseTime + TimeDelta::Seconds(1));

  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(
          [](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq, feedback_packet->GetBaseSequence());
            EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq));
            EXPECT_THAT(Timestamps(*feedback_packet), ElementsAre(kBaseTime));
            return true;
          });

  Process();
}

TEST_F(TransportSequenceNumberFeedbackGeneneratorTest,
       FeedbackWithMissingStart) {
  // First feedback.
  IncomingPacket(kBaseSeq, kBaseTime);
  IncomingPacket(kBaseSeq + 1, kBaseTime + TimeDelta::Seconds(1));
  EXPECT_CALL(feedback_sender_, Call);
  Process();

  // Second feedback starts with a missing packet (DROP kBaseSeq + 2).
  IncomingPacket(kBaseSeq + 3, kBaseTime + TimeDelta::Seconds(3));

  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(
          [](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq + 2, feedback_packet->GetBaseSequence());
            EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq + 3));
            EXPECT_THAT(Timestamps(*feedback_packet),
                        ElementsAre(kBaseTime + TimeDelta::Seconds(3)));
          });

  Process();
}

TEST_F(TransportSequenceNumberFeedbackGeneneratorTest,
       SendsFeedbackWithVaryingDeltas) {
  IncomingPacket(kBaseSeq, kBaseTime);
  IncomingPacket(kBaseSeq + 1, kBaseTime + kMaxSmallDelta);
  IncomingPacket(kBaseSeq + 2,
                 kBaseTime + (2 * kMaxSmallDelta) + TimeDelta::Millis(1));

  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(
          [](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq, feedback_packet->GetBaseSequence());
            EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq, kBaseSeq + 1, kBaseSeq + 2));
            EXPECT_THAT(Timestamps(*feedback_packet),
                        ElementsAre(kBaseTime, kBaseTime + kMaxSmallDelta,
                                    kBaseTime + (2 * kMaxSmallDelta) +
                                        TimeDelta::Millis(1)));
          });

  Process();
}

TEST_F(TransportSequenceNumberFeedbackGeneneratorTest,
       SendsFragmentedFeedback) {
  static constexpr TimeDelta kTooLargeDelta =
      rtcp::TransportFeedback::kDeltaTick * (1 << 16);

  IncomingPacket(kBaseSeq, kBaseTime);
  IncomingPacket(kBaseSeq + 1, kBaseTime + kTooLargeDelta);

  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(
          [](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq, feedback_packet->GetBaseSequence());
            EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq));
            EXPECT_THAT(Timestamps(*feedback_packet), ElementsAre(kBaseTime));
          })
      .WillOnce(
          [](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq + 1, feedback_packet->GetBaseSequence());
            EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq + 1));
            EXPECT_THAT(Timestamps(*feedback_packet),
                        ElementsAre(kBaseTime + kTooLargeDelta));
          });

  Process();
}

TEST_F(TransportSequenceNumberFeedbackGeneneratorTest,
       HandlesReorderingAndWrap) {
  const TimeDelta kDelta = TimeDelta::Seconds(1);
  const uint16_t kLargeSeq = 62762;
  IncomingPacket(kBaseSeq, kBaseTime);
  IncomingPacket(kLargeSeq, kBaseTime + kDelta);

  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(
          [&](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kLargeSeq, feedback_packet->GetBaseSequence());
            EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

            EXPECT_THAT(Timestamps(*feedback_packet),
                        ElementsAre(kBaseTime + kDelta, kBaseTime));
          });

  Process();
}

TEST_F(TransportSequenceNumberFeedbackGeneneratorTest,
       HandlesMalformedSequenceNumbers) {
  // This test generates incoming packets with large jumps in sequence numbers.
  // When unwrapped, the sequeunce numbers of these 30 incoming packets, will
  // span a range of roughly 650k packets. Test that we only send feedback for
  // the last packets. Test for regression found in chromium:949020.
  const TimeDelta kDelta = TimeDelta::Seconds(1);
  for (int i = 0; i < 10; ++i) {
    IncomingPacket(kBaseSeq + i, kBaseTime + 3 * i * kDelta);
    IncomingPacket(kBaseSeq + 20000 + i, kBaseTime + (3 * i + 1) * kDelta);
    IncomingPacket(kBaseSeq + 40000 + i, kBaseTime + (3 * i + 2) * kDelta);
  }

  // Only expect feedback for the last two packets.
  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(
          [&](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());
            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq + 20009, kBaseSeq + 40009));
            EXPECT_THAT(
                Timestamps(*feedback_packet),
                ElementsAre(kBaseTime + 28 * kDelta, kBaseTime + 29 * kDelta));
          });

  Process();
}

TEST_F(TransportSequenceNumberFeedbackGeneneratorTest,
       HandlesBackwardsWrappingSequenceNumbers) {
  // This test is like HandlesMalformedSequenceNumbers but for negative wrap
  // arounds. Test that we only send feedback for the packets with highest
  // sequence numbers.  Test for regression found in chromium:949020.
  const TimeDelta kDelta = TimeDelta::Seconds(1);
  for (int i = 0; i < 10; ++i) {
    IncomingPacket(kBaseSeq + i, kBaseTime + 3 * i * kDelta);
    IncomingPacket(kBaseSeq + 40000 + i, kBaseTime + (3 * i + 1) * kDelta);
    IncomingPacket(kBaseSeq + 20000 + i, kBaseTime + (3 * i + 2) * kDelta);
  }

  // Only expect feedback for the first two packets.
  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(
          [&](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq + 40000, feedback_packet->GetBaseSequence());
            EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());
            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq + 40000, kBaseSeq));
            EXPECT_THAT(Timestamps(*feedback_packet),
                        ElementsAre(kBaseTime + kDelta, kBaseTime));
          });

  Process();
}

TEST_F(TransportSequenceNumberFeedbackGeneneratorTest,
       ResendsTimestampsOnReordering) {
  IncomingPacket(kBaseSeq, kBaseTime);
  IncomingPacket(kBaseSeq + 2, kBaseTime + TimeDelta::Millis(2));

  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(
          [](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq, feedback_packet->GetBaseSequence());
            EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq, kBaseSeq + 2));
            EXPECT_THAT(
                Timestamps(*feedback_packet),
                ElementsAre(kBaseTime, kBaseTime + TimeDelta::Millis(2)));
          });

  Process();

  IncomingPacket(kBaseSeq + 1, kBaseTime + TimeDelta::Millis(1));

  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(
          [](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq + 1, feedback_packet->GetBaseSequence());
            EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq + 1, kBaseSeq + 2));
            EXPECT_THAT(Timestamps(*feedback_packet),
                        ElementsAre(kBaseTime + TimeDelta::Millis(1),
                                    kBaseTime + TimeDelta::Millis(2)));
          });

  Process();
}

TEST_F(TransportSequenceNumberFeedbackGeneneratorTest,
       RemovesTimestampsOutOfScope) {
  const Timestamp kTimeoutTime = kBaseTime + kBackWindow;

  IncomingPacket(kBaseSeq + 2, kBaseTime);

  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(
          [](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq + 2, feedback_packet->GetBaseSequence());

            EXPECT_THAT(Timestamps(*feedback_packet), ElementsAre(kBaseTime));
          });

  Process();

  IncomingPacket(kBaseSeq + 3, kTimeoutTime);  // kBaseSeq + 2 times out here.

  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(
          [&](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq + 3, feedback_packet->GetBaseSequence());

            EXPECT_THAT(Timestamps(*feedback_packet),
                        ElementsAre(kTimeoutTime));
          });

  Process();

  // New group, with sequence starting below the first so that they may be
  // retransmitted.
  IncomingPacket(kBaseSeq, kBaseTime - TimeDelta::Millis(1));
  IncomingPacket(kBaseSeq + 1, kTimeoutTime - TimeDelta::Millis(1));

  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(
          [&](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq, feedback_packet->GetBaseSequence());

            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq, kBaseSeq + 1, kBaseSeq + 3));
            EXPECT_THAT(
                Timestamps(*feedback_packet),
                ElementsAre(kBaseTime - TimeDelta::Millis(1),
                            kTimeoutTime - TimeDelta::Millis(1), kTimeoutTime));
          });

  Process();
}

TEST_F(TransportSequenceNumberFeedbackGeneneratorTest,
       TimeUntilNextProcessIsDefaultOnUnkownBitrate) {
  EXPECT_EQ(feedback_generator_.Process(clock_.CurrentTime()),
            kDefaultSendInterval);
}

TEST_F(TransportSequenceNumberFeedbackGeneneratorTest,
       TimeUntilNextProcessIsMinIntervalOn300kbps) {
  feedback_generator_.OnSendBandwidthEstimateChanged(
      DataRate::BitsPerSec(300'000), /*is_bandwidth_limited=*/true,
      /*transport_overhead=*/std::nullopt);
  EXPECT_EQ(feedback_generator_.Process(clock_.CurrentTime()),
            kMinSendInterval);
}

TEST_F(TransportSequenceNumberFeedbackGeneneratorTest,
       TimeUntilNextProcessIsMaxIntervalOn0kbps) {
  // TimeUntilNextProcess should be limited by `kMaxSendIntervalMs` when
  // bitrate is small. We choose 0 bps as a special case, which also tests
  // erroneous behaviors like division-by-zero.
  feedback_generator_.OnSendBandwidthEstimateChanged(
      DataRate::Zero(), /*is_bandwidth_limited=*/true,
      /*transport_overhead=*/std::nullopt);
  EXPECT_EQ(feedback_generator_.Process(clock_.CurrentTime()),
            kMaxSendInterval);
}

TEST_F(TransportSequenceNumberFeedbackGeneneratorTest,
       TimeUntilNextProcessIsMaxIntervalOn20kbps) {
  feedback_generator_.OnSendBandwidthEstimateChanged(
      DataRate::BitsPerSec(20'000), /*is_bandwidth_limited=*/true,
      /*transport_overhead=*/std::nullopt);
  EXPECT_EQ(feedback_generator_.Process(clock_.CurrentTime()),
            kMaxSendInterval);
}

TEST_F(TransportSequenceNumberFeedbackGeneneratorTest,
       TwccReportsUse5PercentOfAvailableBandwidth) {
  feedback_generator_.OnSendBandwidthEstimateChanged(
      DataRate::BitsPerSec(80'000), /*is_bandwidth_limited=*/true,
      /*transport_overhead=*/std::nullopt);
  // 80kbps * 0.05 = TwccReportSize(68B * 8b/B) * 1000ms / SendInterval(136ms)
  EXPECT_EQ(feedback_generator_.Process(clock_.CurrentTime()),
            TimeDelta::Millis(136));
}

TEST_F(TransportSequenceNumberFeedbackGeneneratorTest,
       SplitsFeedbackIntoPacketsThatFitIntoOneRtcpPacket) {
  // More RTP packets than one feedback packet of at most
  // kMaxRtcpPacketSizeBytes can report, as received at high packet rates.
  constexpr int kNumPackets = 3000;
  std::vector<uint16_t> sequence_numbers;
  for (int i = 0; i < kNumPackets; ++i) {
    sequence_numbers.push_back(static_cast<uint16_t>(kBaseSeq + i));
    IncomingPacket(sequence_numbers.back(),
                   kBaseTime + i * TimeDelta::Micros(40));
  }

  ParsedFeedback feedback;
  ExpectFeedbackInPacketsThatFitIntoOneRtcpPacket(feedback);
  Process();

  EXPECT_GT(feedback.num_feedback_packets, 1);
  EXPECT_EQ(feedback.received_sequence_numbers, sequence_numbers);
  EXPECT_EQ(feedback.num_statuses, sequence_numbers.size());
}

TEST_F(TransportSequenceNumberFeedbackGeneneratorTest,
       ReportsLostPacketsOnceWhenSplittingFeedback) {
  // Every other RTP packet is lost, so that the RTP packet that doesn't fit
  // into a feedback packet follows a lost RTP packet.
  constexpr int kNumReceived = 2000;
  std::vector<uint16_t> sequence_numbers;
  for (int i = 0; i < kNumReceived; ++i) {
    sequence_numbers.push_back(static_cast<uint16_t>(kBaseSeq + 2 * i));
    IncomingPacket(sequence_numbers.back(),
                   kBaseTime + i * TimeDelta::Micros(80));
  }

  ParsedFeedback feedback;
  ExpectFeedbackInPacketsThatFitIntoOneRtcpPacket(feedback);
  Process();

  EXPECT_GT(feedback.num_feedback_packets, 1);
  EXPECT_EQ(feedback.received_sequence_numbers, sequence_numbers);
  // Each RTP packet from the first to the last received one is reported once,
  // either as received or as lost.
  EXPECT_EQ(feedback.num_statuses, size_t{2 * kNumReceived - 1});
}

//////////////////////////////////////////////////////////////////////////////
// Tests for the extended protocol where the feedback is explicitly requested
// by the sender.
//////////////////////////////////////////////////////////////////////////////
typedef TransportSequenceNumberFeedbackGeneneratorTest
    RemoteEstimatorProxyOnRequestTest;
TEST_F(RemoteEstimatorProxyOnRequestTest, DisablesPeriodicProcess) {
  IncomingPacketV2(kBaseSeq, kBaseTime);
  EXPECT_EQ(feedback_generator_.Process(clock_.CurrentTime()),
            TimeDelta::PlusInfinity());
}

TEST_F(RemoteEstimatorProxyOnRequestTest, ProcessDoesNotSendFeedback) {
  IncomingPacketV2(kBaseSeq, kBaseTime);
  EXPECT_CALL(feedback_sender_, Call).Times(0);
  Process();
}

TEST_F(RemoteEstimatorProxyOnRequestTest, RequestSinglePacketFeedback) {
  IncomingPacketV2(kBaseSeq, kBaseTime);
  IncomingPacketV2(kBaseSeq + 1, kBaseTime + kMaxSmallDelta);
  IncomingPacketV2(kBaseSeq + 2, kBaseTime + 2 * kMaxSmallDelta);

  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(
          [](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq + 3, feedback_packet->GetBaseSequence());
            EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq + 3));
            EXPECT_THAT(Timestamps(*feedback_packet),
                        ElementsAre(kBaseTime + 3 * kMaxSmallDelta));
          });

  constexpr FeedbackRequest kSinglePacketFeedbackRequest = {
      .include_timestamps = true, .sequence_count = 1};
  IncomingPacketV2(kBaseSeq + 3, kBaseTime + 3 * kMaxSmallDelta,
                   kSinglePacketFeedbackRequest);
}

TEST_F(RemoteEstimatorProxyOnRequestTest, RequestLastFivePacketFeedback) {
  int i = 0;
  for (; i < 10; ++i) {
    IncomingPacketV2(kBaseSeq + i, kBaseTime + i * kMaxSmallDelta);
  }

  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(
          [](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq + 6, feedback_packet->GetBaseSequence());
            EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq + 6, kBaseSeq + 7, kBaseSeq + 8,
                                    kBaseSeq + 9, kBaseSeq + 10));
            EXPECT_THAT(Timestamps(*feedback_packet),
                        ElementsAre(kBaseTime + 6 * kMaxSmallDelta,
                                    kBaseTime + 7 * kMaxSmallDelta,
                                    kBaseTime + 8 * kMaxSmallDelta,
                                    kBaseTime + 9 * kMaxSmallDelta,
                                    kBaseTime + 10 * kMaxSmallDelta));
          });

  constexpr FeedbackRequest kFivePacketsFeedbackRequest = {
      .include_timestamps = true, .sequence_count = 5};
  IncomingPacketV2(kBaseSeq + i, kBaseTime + i * kMaxSmallDelta,
                   kFivePacketsFeedbackRequest);
}

TEST_F(RemoteEstimatorProxyOnRequestTest,
       RequestLastFivePacketFeedbackMissingPackets) {
  int i = 0;
  for (; i < 10; ++i) {
    if (i != 7 && i != 9)
      IncomingPacketV2(kBaseSeq + i, kBaseTime + i * kMaxSmallDelta);
  }

  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(
          [](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq + 6, feedback_packet->GetBaseSequence());
            EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq + 6, kBaseSeq + 8, kBaseSeq + 10));
            EXPECT_THAT(Timestamps(*feedback_packet),
                        ElementsAre(kBaseTime + 6 * kMaxSmallDelta,
                                    kBaseTime + 8 * kMaxSmallDelta,
                                    kBaseTime + 10 * kMaxSmallDelta));
          });

  constexpr FeedbackRequest kFivePacketsFeedbackRequest = {
      .include_timestamps = true, .sequence_count = 5};
  IncomingPacketV2(kBaseSeq + i, kBaseTime + i * kMaxSmallDelta,
                   kFivePacketsFeedbackRequest);
}

TEST_F(RemoteEstimatorProxyOnRequestTest,
       SplitsRequestedFeedbackIntoPacketsThatFitIntoOneRtcpPacket) {
  constexpr int kNumPackets = 3000;
  std::vector<uint16_t> sequence_numbers;
  for (int i = 0; i < kNumPackets; ++i) {
    sequence_numbers.push_back(static_cast<uint16_t>(kBaseSeq + i));
  }
  for (int i = 0; i < kNumPackets - 1; ++i) {
    IncomingPacketV2(sequence_numbers[i],
                     kBaseTime + i * TimeDelta::Micros(40));
  }

  ParsedFeedback feedback;
  ExpectFeedbackInPacketsThatFitIntoOneRtcpPacket(feedback);
  constexpr FeedbackRequest kAllPacketsFeedbackRequest = {
      .include_timestamps = true, .sequence_count = kNumPackets};
  IncomingPacketV2(sequence_numbers.back(),
                   kBaseTime + (kNumPackets - 1) * TimeDelta::Micros(40),
                   kAllPacketsFeedbackRequest);

  EXPECT_GT(feedback.num_feedback_packets, 1);
  EXPECT_EQ(feedback.received_sequence_numbers, sequence_numbers);
}

}  // namespace
}  // namespace webrtc
