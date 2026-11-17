/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/rtp_file_reader.h"

#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "modules/rtp_rtcp/source/rtp_util.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {

namespace {

std::vector<uint8_t> CreateVlanPcap(std::span<const uint16_t> tags) {
  // Little-endian PCAP global header: version 2.4, Ethernet link type.
  std::vector<uint8_t> data = {0xd4, 0xc3, 0xb2, 0xa1, 2, 0, 4, 0,
                               0,    0,    0,    0,    0, 0, 0, 0,
                               0xff, 0xff, 0,    0,    1, 0, 0, 0};
  // Packet header: timestamp 1s, captured and original lengths.
  const uint8_t packet_length = 14 + 4 * tags.size() + 20 + 8 + 12;
  data.insert(data.end(), {1, 0, 0, 0, 0, 0, 0, 0, packet_length, 0, 0, 0,
                           packet_length, 0, 0, 0});
  data.insert(data.end(), 12, 0);  // Destination and source MAC addresses.
  for (uint16_t tpid : tags) {
    // VLAN ID and priority are zero (priority-tagged frame).
    data.insert(data.end(), {static_cast<uint8_t>(tpid >> 8),
                             static_cast<uint8_t>(tpid), 0, 0});
  }
  data.insert(data.end(), {0x08, 0x00});  // Encapsulated EtherType: IPv4.
  // IPv4: 20-byte header, 40-byte packet, UDP, no fragmentation.
  data.insert(data.end(), {0x45, 0, 0,  40, 0, 0, 0x40, 0, 64, 17,
                           0,    0, 10, 0,  0, 1, 10,   0, 0,  2});
  // UDP: ports 10000/10001, 20-byte datagram.
  data.insert(data.end(), {0x27, 0x10, 0x27, 0x11, 0, 20, 0, 0});
  // RTP: PT 96, sequence 1, timestamp 960, SSRC 0x12345678.
  data.insert(data.end(),
              {0x80, 96, 0, 1, 0, 0, 3, 0xc0, 0x12, 0x34, 0x56, 0x78});
  return data;
}

}  // namespace

class TestRtpFileReader : public ::testing::Test {
 public:
  void Init(const std::string& filename, bool headers_only_file) {
    std::string filepath =
        test::ResourcePath("video_coding/" + filename, "rtp");
    rtp_packet_source_.reset(
        test::RtpFileReader::Create(test::RtpFileReader::kRtpDump, filepath));
    ASSERT_TRUE(rtp_packet_source_.get() != nullptr);
    headers_only_file_ = headers_only_file;
  }

  int CountRtpPackets() {
    test::RtpPacket packet;
    int c = 0;
    while (rtp_packet_source_->NextPacket(&packet)) {
      if (headers_only_file_) {
        EXPECT_LT(packet.length(), packet.original_length);
      } else {
        EXPECT_EQ(packet.length(), packet.original_length);
      }
      c++;
    }
    return c;
  }

 private:
  std::unique_ptr<test::RtpFileReader> rtp_packet_source_;
  bool headers_only_file_;
};

TEST_F(TestRtpFileReader, Test60Packets) {
  Init("pltype103", false);
  EXPECT_EQ(60, CountRtpPackets());
}

TEST_F(TestRtpFileReader, Test60PacketsHeaderOnly) {
  Init("pltype103_header_only", true);
  EXPECT_EQ(60, CountRtpPackets());
}

typedef std::map<uint32_t, int> PacketsPerSsrc;

class TestPcapFileReader : public ::testing::Test {
 public:
  void Init(const std::string& filename) {
    std::string filepath =
        test::ResourcePath("video_coding/" + filename, "pcap");
    rtp_packet_source_.reset(
        test::RtpFileReader::Create(test::RtpFileReader::kPcap, filepath));
    ASSERT_TRUE(rtp_packet_source_.get() != nullptr);
  }

  int CountRtpPackets() {
    int c = 0;
    test::RtpPacket packet;
    while (rtp_packet_source_->NextPacket(&packet)) {
      EXPECT_EQ(packet.length(), packet.original_length);
      c++;
    }
    return c;
  }

  PacketsPerSsrc CountRtpPacketsPerSsrc() {
    PacketsPerSsrc pps;
    test::RtpPacket packet;
    while (rtp_packet_source_->NextPacket(&packet)) {
      if (IsRtpPacket(packet.buffer)) {
        pps[ParseRtpSsrc(packet.buffer)]++;
      }
    }
    return pps;
  }

 private:
  std::unique_ptr<test::RtpFileReader> rtp_packet_source_;
};

TEST_F(TestPcapFileReader, TestEthernetIIFrame) {
  Init("frame-ethernet-ii");
  EXPECT_EQ(368, CountRtpPackets());
}

TEST_F(TestPcapFileReader, TestLoopbackFrame) {
  Init("frame-loopback");
  EXPECT_EQ(491, CountRtpPackets());
}

TEST(TestPcapVlanFileReader, ReadsPriorityTaggedFrame) {
  // VLAN ID zero denotes a priority-tagged frame.
  const uint16_t tags[] = {0x8100};
  const std::vector<uint8_t> pcap = CreateVlanPcap(tags);
  std::unique_ptr<test::RtpFileReader> reader(test::RtpFileReader::Create(
      test::RtpFileReader::kPcap, pcap.data(), pcap.size(), {}));
  ASSERT_NE(reader, nullptr);
  test::RtpPacket packet;
  ASSERT_TRUE(reader->NextPacket(&packet));
  ASSERT_EQ(packet.length(), 12u);
  EXPECT_EQ(ParseRtpSsrc(packet.buffer), 0x12345678u);
  EXPECT_FALSE(reader->NextPacket(&packet));
}

TEST(TestPcapVlanFileReader, ReadsQinQFrame) {
  // An outer service tag encapsulates an inner customer tag.
  const uint16_t tags[] = {0x88a8, 0x8100};
  const std::vector<uint8_t> pcap = CreateVlanPcap(tags);
  std::unique_ptr<test::RtpFileReader> reader(test::RtpFileReader::Create(
      test::RtpFileReader::kPcap, pcap.data(), pcap.size(), {}));
  ASSERT_NE(reader, nullptr);
  test::RtpPacket packet;
  ASSERT_TRUE(reader->NextPacket(&packet));
  ASSERT_EQ(packet.length(), 12u);
  EXPECT_EQ(ParseRtpSsrc(packet.buffer), 0x12345678u);
  EXPECT_FALSE(reader->NextPacket(&packet));
}

TEST(TestPcapVlanFileReader, SkipsFrameWithMoreThanTwoTags) {
  const uint16_t tags[] = {0x88a8, 0x8100, 0x8100};
  const std::vector<uint8_t> pcap = CreateVlanPcap(tags);
  std::unique_ptr<test::RtpFileReader> reader(test::RtpFileReader::Create(
      test::RtpFileReader::kPcap, pcap.data(), pcap.size(), {}));
  ASSERT_NE(reader, nullptr);
  test::RtpPacket packet;
  EXPECT_FALSE(reader->NextPacket(&packet));
}

TEST_F(TestPcapFileReader, TestTwoSsrc) {
  Init("ssrcs-2");
  PacketsPerSsrc pps = CountRtpPacketsPerSsrc();
  EXPECT_EQ(2UL, pps.size());
  EXPECT_EQ(370, pps[0x78d48f61]);
  EXPECT_EQ(60, pps[0xae94130b]);
}

TEST_F(TestPcapFileReader, TestThreeSsrc) {
  Init("ssrcs-3");
  PacketsPerSsrc pps = CountRtpPacketsPerSsrc();
  EXPECT_EQ(3UL, pps.size());
  EXPECT_EQ(162, pps[0x938c5eaa]);
  EXPECT_EQ(113, pps[0x59fe6ef0]);
  EXPECT_EQ(61, pps[0xed2bd2ac]);
}
}  // namespace webrtc
