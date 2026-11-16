/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "pc/congestion_control_feedback_reporter.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "api/transport/ecn_marking.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "pc/congestion_control_feedback_report.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::IsEmpty;
using ::testing::SizeIs;

constexpr Timestamp kBaseTime = Timestamp::Seconds(1000);
constexpr uint32_t kReportTimestamp = 0xAABBCCDD;

TEST(CongestionControlFeedbackReporterTest, ReportIsEmptyBeforeAnyPacket) {
  CongestionControlFeedbackReporter reporter;

  CongestionControlFeedbackReport report =
      reporter.CreateReport(kBaseTime, kReportTimestamp);

  EXPECT_THAT(report.blocks, IsEmpty());
  EXPECT_EQ(report.report_timestamp, kReportTimestamp);
}

TEST(CongestionControlFeedbackReporterTest, ReportsArrivalTimeOffset) {
  CongestionControlFeedbackReporter reporter;
  reporter.OnPacketReceived(7, kBaseTime, EcnMarking::kEct1);

  CongestionControlFeedbackReport report = reporter.CreateReport(
      kBaseTime + TimeDelta::Millis(20), kReportTimestamp);

  EXPECT_EQ(report.begin_seq, 7);
  ASSERT_THAT(report.blocks, SizeIs(1));
  EXPECT_TRUE(report.blocks[0].received);
  EXPECT_EQ(report.blocks[0].ecn, EcnMarking::kEct1);
  EXPECT_EQ(report.blocks[0].arrival_time_offset, TimeDelta::Millis(20));
}

TEST(CongestionControlFeedbackReporterTest,
     ReportsMissingPacketsAsNotReceived) {
  CongestionControlFeedbackReporter reporter;
  reporter.OnPacketReceived(5, kBaseTime, EcnMarking::kNotEct);
  reporter.OnPacketReceived(8, kBaseTime, EcnMarking::kNotEct);

  CongestionControlFeedbackReport report =
      reporter.CreateReport(kBaseTime, kReportTimestamp);

  EXPECT_EQ(report.begin_seq, 5);
  ASSERT_THAT(report.blocks, SizeIs(4));
  EXPECT_TRUE(report.blocks[0].received);
  EXPECT_FALSE(report.blocks[1].received);
  EXPECT_FALSE(report.blocks[2].received);
  EXPECT_TRUE(report.blocks[3].received);
}

TEST(CongestionControlFeedbackReporterTest, SecondReportCoversOnlyNewPackets) {
  CongestionControlFeedbackReporter reporter;
  reporter.OnPacketReceived(1, kBaseTime, EcnMarking::kNotEct);
  reporter.CreateReport(kBaseTime, kReportTimestamp);
  reporter.OnPacketReceived(2, kBaseTime, EcnMarking::kNotEct);

  CongestionControlFeedbackReport report =
      reporter.CreateReport(kBaseTime, kReportTimestamp);

  EXPECT_EQ(report.begin_seq, 2);
  EXPECT_THAT(report.blocks, SizeIs(1));
}

TEST(CongestionControlFeedbackReporterTest, ReportIsEmptyWithoutNewPackets) {
  CongestionControlFeedbackReporter reporter;
  reporter.OnPacketReceived(1, kBaseTime, EcnMarking::kNotEct);
  reporter.CreateReport(kBaseTime, kReportTimestamp);

  CongestionControlFeedbackReport report =
      reporter.CreateReport(kBaseTime, kReportTimestamp);

  EXPECT_THAT(report.blocks, IsEmpty());
}

TEST(CongestionControlFeedbackReporterTest, LatePacketExtendsNextReportBack) {
  CongestionControlFeedbackReporter reporter;
  reporter.OnPacketReceived(10, kBaseTime, EcnMarking::kNotEct);
  reporter.OnPacketReceived(12, kBaseTime, EcnMarking::kNotEct);
  reporter.CreateReport(kBaseTime, kReportTimestamp);

  reporter.OnPacketReceived(11, kBaseTime, EcnMarking::kNotEct);
  CongestionControlFeedbackReport report =
      reporter.CreateReport(kBaseTime, kReportTimestamp);

  EXPECT_EQ(report.begin_seq, 11);
  ASSERT_THAT(report.blocks, SizeIs(2));
  EXPECT_TRUE(report.blocks[0].received);
  EXPECT_TRUE(report.blocks[1].received);
}

TEST(CongestionControlFeedbackReporterTest, KeepsArrivalTimeOfFirstCopy) {
  CongestionControlFeedbackReporter reporter;
  reporter.OnPacketReceived(1, kBaseTime, EcnMarking::kNotEct);
  reporter.OnPacketReceived(1, kBaseTime + TimeDelta::Millis(5),
                            EcnMarking::kNotEct);

  CongestionControlFeedbackReport report = reporter.CreateReport(
      kBaseTime + TimeDelta::Millis(10), kReportTimestamp);

  ASSERT_THAT(report.blocks, SizeIs(1));
  EXPECT_EQ(report.blocks[0].arrival_time_offset, TimeDelta::Millis(10));
}

TEST(CongestionControlFeedbackReporterTest, DuplicateUpgradesEcnToCe) {
  CongestionControlFeedbackReporter reporter;
  reporter.OnPacketReceived(1, kBaseTime, EcnMarking::kEct0);
  reporter.OnPacketReceived(1, kBaseTime, EcnMarking::kCe);

  CongestionControlFeedbackReport report =
      reporter.CreateReport(kBaseTime, kReportTimestamp);

  ASSERT_THAT(report.blocks, SizeIs(1));
  EXPECT_EQ(report.blocks[0].ecn, EcnMarking::kCe);
}

TEST(CongestionControlFeedbackReporterTest, DuplicateDoesNotClearCe) {
  CongestionControlFeedbackReporter reporter;
  reporter.OnPacketReceived(1, kBaseTime, EcnMarking::kCe);
  reporter.OnPacketReceived(1, kBaseTime, EcnMarking::kEct0);

  CongestionControlFeedbackReport report =
      reporter.CreateReport(kBaseTime, kReportTimestamp);

  ASSERT_THAT(report.blocks, SizeIs(1));
  EXPECT_EQ(report.blocks[0].ecn, EcnMarking::kCe);
}

TEST(CongestionControlFeedbackReporterTest,
     CeMarkedDuplicateExtendsNextReportBack) {
  CongestionControlFeedbackReporter reporter;
  reporter.OnPacketReceived(1, kBaseTime, EcnMarking::kEct0);
  reporter.OnPacketReceived(2, kBaseTime, EcnMarking::kEct0);
  reporter.CreateReport(kBaseTime, kReportTimestamp);

  reporter.OnPacketReceived(1, kBaseTime, EcnMarking::kCe);
  CongestionControlFeedbackReport report =
      reporter.CreateReport(kBaseTime, kReportTimestamp);

  EXPECT_EQ(report.begin_seq, 1);
  ASSERT_THAT(report.blocks, SizeIs(2));
  EXPECT_EQ(report.blocks[0].ecn, EcnMarking::kCe);
  EXPECT_EQ(report.blocks[1].ecn, EcnMarking::kEct0);
}

TEST(CongestionControlFeedbackReporterTest,
     DuplicateWithoutNewCeIsNotReportedAgain) {
  CongestionControlFeedbackReporter reporter;
  reporter.OnPacketReceived(1, kBaseTime, EcnMarking::kEct0);
  reporter.CreateReport(kBaseTime, kReportTimestamp);

  reporter.OnPacketReceived(1, kBaseTime, EcnMarking::kEct0);
  CongestionControlFeedbackReport report =
      reporter.CreateReport(kBaseTime, kReportTimestamp);

  EXPECT_THAT(report.blocks, IsEmpty());
}

TEST(CongestionControlFeedbackReporterTest, FeedbackCountAdvancesPerReport) {
  CongestionControlFeedbackReporter reporter;

  EXPECT_EQ(reporter.CreateReport(kBaseTime, kReportTimestamp).feedback_count,
            0);
  EXPECT_EQ(reporter.CreateReport(kBaseTime, kReportTimestamp).feedback_count,
            1);
  EXPECT_EQ(reporter.CreateReport(kBaseTime, kReportTimestamp).feedback_count,
            2);
}

TEST(CongestionControlFeedbackReporterTest, CoversSequenceNumberWrap) {
  CongestionControlFeedbackReporter reporter;
  reporter.OnPacketReceived(0xFFFF, kBaseTime, EcnMarking::kNotEct);
  reporter.OnPacketReceived(0, kBaseTime, EcnMarking::kNotEct);

  CongestionControlFeedbackReport report =
      reporter.CreateReport(kBaseTime, kReportTimestamp);

  EXPECT_EQ(report.begin_seq, 0xFFFF);
  ASSERT_THAT(report.blocks, SizeIs(2));
  EXPECT_TRUE(report.blocks[0].received);
  EXPECT_TRUE(report.blocks[1].received);
}

TEST(CongestionControlFeedbackReporterTest, IgnoresPacketFarAheadOfTheRange) {
  CongestionControlFeedbackReporter reporter;
  reporter.OnPacketReceived(0, kBaseTime, EcnMarking::kNotEct);
  reporter.OnPacketReceived(20000, kBaseTime, EcnMarking::kNotEct);

  CongestionControlFeedbackReport report =
      reporter.CreateReport(kBaseTime, kReportTimestamp);

  EXPECT_EQ(report.begin_seq, 0);
  EXPECT_THAT(report.blocks, SizeIs(1));
}

TEST(CongestionControlFeedbackReporterTest, IgnoresPacketOlderThanTheRange) {
  CongestionControlFeedbackReporter reporter;
  for (uint16_t seq = 0; seq <= 200; ++seq) {
    reporter.OnPacketReceived(seq, kBaseTime, EcnMarking::kNotEct);
  }
  reporter.CreateReport(kBaseTime, kReportTimestamp);

  reporter.OnPacketReceived(10, kBaseTime, EcnMarking::kNotEct);
  CongestionControlFeedbackReport report =
      reporter.CreateReport(kBaseTime, kReportTimestamp);

  EXPECT_THAT(report.blocks, IsEmpty());
}

TEST(CongestionControlFeedbackReporterTest,
     IgnoredPacketsDoNotMoveTheUnwrapper) {
  CongestionControlFeedbackReporter reporter;
  reporter.OnPacketReceived(0, kBaseTime, EcnMarking::kNotEct);
  reporter.OnPacketReceived(20000, kBaseTime, EcnMarking::kNotEct);
  reporter.OnPacketReceived(40000, kBaseTime, EcnMarking::kNotEct);
  reporter.OnPacketReceived(1, kBaseTime, EcnMarking::kNotEct);

  CongestionControlFeedbackReport report =
      reporter.CreateReport(kBaseTime, kReportTimestamp);

  EXPECT_EQ(report.begin_seq, 0);
  ASSERT_THAT(report.blocks, SizeIs(2));
  EXPECT_TRUE(report.blocks[1].received);
}

TEST(CongestionControlFeedbackReporterTest, TracksAfreshAfterAJumpForward) {
  CongestionControlFeedbackReporter reporter;
  reporter.OnPacketReceived(0, kBaseTime, EcnMarking::kNotEct);
  reporter.CreateReport(kBaseTime, kReportTimestamp);
  // Too far ahead of the tracked range, as after a long outage.
  reporter.OnPacketReceived(20000, kBaseTime, EcnMarking::kNotEct);

  CongestionControlFeedbackReport report =
      reporter.CreateReport(kBaseTime, kReportTimestamp);

  EXPECT_EQ(report.begin_seq, 20000);
  ASSERT_THAT(report.blocks, SizeIs(1));
  EXPECT_TRUE(report.blocks[0].received);
}

TEST(CongestionControlFeedbackReporterTest,
     TracksAfreshAfterAJumpForwardOnceAllIsReported) {
  CongestionControlFeedbackReporter reporter;
  reporter.OnPacketReceived(0, kBaseTime, EcnMarking::kNotEct);
  reporter.OnPacketReceived(20000, kBaseTime, EcnMarking::kNotEct);
  reporter.CreateReport(kBaseTime, kReportTimestamp);

  reporter.OnPacketReceived(20001, kBaseTime, EcnMarking::kNotEct);
  CongestionControlFeedbackReport report =
      reporter.CreateReport(kBaseTime, kReportTimestamp);

  EXPECT_EQ(report.begin_seq, 20001);
  ASSERT_THAT(report.blocks, SizeIs(1));
  EXPECT_TRUE(report.blocks[0].received);
}

TEST(CongestionControlFeedbackReporterTest,
     TracksAfreshAfterAReportWithOnlyTooOldPackets) {
  CongestionControlFeedbackReporter reporter;
  reporter.OnPacketReceived(0, kBaseTime, EcnMarking::kNotEct);
  reporter.CreateReport(kBaseTime, kReportTimestamp);
  // Unwraps to before the tracked range, as after a jump of over half the
  // sequence number space.
  reporter.OnPacketReceived(40000, kBaseTime, EcnMarking::kNotEct);
  reporter.CreateReport(kBaseTime, kReportTimestamp);

  reporter.OnPacketReceived(40001, kBaseTime, EcnMarking::kNotEct);
  CongestionControlFeedbackReport report =
      reporter.CreateReport(kBaseTime, kReportTimestamp);

  EXPECT_EQ(report.begin_seq, 40001);
  ASSERT_THAT(report.blocks, SizeIs(1));
  EXPECT_TRUE(report.blocks[0].received);
}

TEST(CongestionControlFeedbackReporterTest, ReportRoundTripsThroughTheCodec) {
  CongestionControlFeedbackReporter reporter;
  reporter.OnPacketReceived(4, kBaseTime, EcnMarking::kCe);
  reporter.OnPacketReceived(6, kBaseTime + TimeDelta::Millis(250),
                            EcnMarking::kEct0);

  CongestionControlFeedbackReport report = reporter.CreateReport(
      kBaseTime + TimeDelta::Millis(500), kReportTimestamp);
  std::vector<uint8_t> bytes(report.SizeBytes());
  report.Encode(bytes);
  std::optional<CongestionControlFeedbackReport> decoded =
      CongestionControlFeedbackReport::Decode(bytes);

  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->begin_seq, 4);
  EXPECT_EQ(decoded->report_timestamp, kReportTimestamp);
  ASSERT_THAT(decoded->blocks, SizeIs(3));
  EXPECT_EQ(decoded->blocks[0].ecn, EcnMarking::kCe);
  EXPECT_EQ(decoded->blocks[0].arrival_time_offset, TimeDelta::Millis(500));
  EXPECT_FALSE(decoded->blocks[1].received);
  EXPECT_EQ(decoded->blocks[2].arrival_time_offset, TimeDelta::Millis(250));
}

TEST(CongestionControlFeedbackReporterTest, LimitsReportToMaxReportBlocks) {
  CongestionControlFeedbackReporter reporter;
  for (uint16_t seq = 0; seq < 10; ++seq) {
    reporter.OnPacketReceived(seq, kBaseTime, EcnMarking::kNotEct);
  }

  CongestionControlFeedbackReport report = reporter.CreateReport(
      kBaseTime, kReportTimestamp, /*max_report_blocks=*/4);

  EXPECT_EQ(report.begin_seq, 0);
  EXPECT_THAT(report.blocks, SizeIs(4));
}

TEST(CongestionControlFeedbackReporterTest, NextReportResumesAfterTheLimit) {
  CongestionControlFeedbackReporter reporter;
  for (uint16_t seq = 0; seq < 10; ++seq) {
    reporter.OnPacketReceived(seq, kBaseTime, EcnMarking::kNotEct);
  }
  reporter.CreateReport(kBaseTime, kReportTimestamp, /*max_report_blocks=*/4);

  CongestionControlFeedbackReport report =
      reporter.CreateReport(kBaseTime, kReportTimestamp);

  EXPECT_EQ(report.begin_seq, 4);
  EXPECT_THAT(report.blocks, SizeIs(6));
}

}  // namespace
}  // namespace webrtc
