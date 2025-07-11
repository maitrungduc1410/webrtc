/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "api/array_view.h"
#include "api/call/transport.h"
#include "api/rtp_parameters.h"
#include "api/task_queue/task_queue_base.h"
#include "api/test/simulated_network.h"
#include "api/transport/bitrate_settings.h"
#include "api/units/time_delta.h"
#include "call/audio_receive_stream.h"
#include "call/audio_send_stream.h"
#include "call/call.h"
#include "call/fake_network_pipe.h"
#include "call/video_receive_stream.h"
#include "call/video_send_stream.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/numerics/sequence_number_unwrapper.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"
#include "test/call_test.h"
#include "test/direct_transport.h"
#include "test/fake_videorenderer.h"
#include "test/frame_generator_capturer.h"
#include "test/gtest.h"
#include "test/network/simulated_network.h"
#include "test/rtcp_packet_parser.h"
#include "test/rtp_rtcp_observer.h"
#include "test/video_test_constants.h"
#include "video/config/video_encoder_config.h"
#include "video/end_to_end_tests/multi_stream_tester.h"

namespace webrtc {
namespace {
enum : int {  // The first valid value is 1.
  kTransportSequenceNumberExtensionId = 1,
};
}  // namespace

TEST(TransportFeedbackMultiStreamTest, AssignsTransportSequenceNumbers) {
  static constexpr int kSendRtxPayloadType = 98;
  static constexpr TimeDelta kDefaultTimeout = TimeDelta::Seconds(30);
  static constexpr int kNackRtpHistoryMs = 1000;
  static constexpr uint32_t kSendRtxSsrcs[MultiStreamTester::kNumStreams] = {
      0xBADCAFD, 0xBADCAFE, 0xBADCAFF};

  class RtpExtensionHeaderObserver : public test::DirectTransport {
   public:
    RtpExtensionHeaderObserver(
        TaskQueueBase* task_queue,
        Call* sender_call,
        const std::map<uint32_t, uint32_t>& ssrc_map,
        const std::map<uint8_t, MediaType>& payload_type_map,
        ArrayView<const RtpExtension> audio_extensions,
        ArrayView<const RtpExtension> video_extensions)
        : DirectTransport(task_queue,
                          std::make_unique<FakeNetworkPipe>(
                              Clock::GetRealTimeClock(),
                              std::make_unique<SimulatedNetwork>(
                                  BuiltInNetworkBehaviorConfig())),
                          sender_call,
                          payload_type_map,
                          audio_extensions,
                          video_extensions),
          rtx_to_media_ssrcs_(ssrc_map),
          rtx_padding_observed_(false),
          retransmit_observed_(false),
          started_(false) {
      extensions_.Register<TransportSequenceNumber>(
          kTransportSequenceNumberExtensionId);
    }
    ~RtpExtensionHeaderObserver() override {}

    bool SendRtp(ArrayView<const uint8_t> data,
                 const PacketOptions& options) override {
      {
        MutexLock lock(&lock_);

        if (IsDone())
          return false;

        if (started_) {
          RtpPacket rtp_packet(&extensions_);
          EXPECT_TRUE(rtp_packet.Parse(data));
          bool drop_packet = false;

          uint16_t transport_sequence_number = 0;
          EXPECT_TRUE(rtp_packet.GetExtension<TransportSequenceNumber>(
              &transport_sequence_number));
          EXPECT_EQ(options.packet_id, transport_sequence_number);
          if (!streams_observed_.empty()) {
            // Unwrap packet id and verify uniqueness.
            int64_t packet_id = unwrapper_.Unwrap(options.packet_id);
            EXPECT_TRUE(received_packed_ids_.insert(packet_id).second);
          }

          // Drop (up to) every 17th packet, so we get retransmits.
          // Only drop media, do not drop padding packets.
          if (rtp_packet.PayloadType() != kSendRtxPayloadType &&
              rtp_packet.payload_size() > 0 &&
              transport_sequence_number % 17 == 0) {
            dropped_seq_[rtp_packet.Ssrc()].insert(rtp_packet.SequenceNumber());
            drop_packet = true;
          }

          if (rtp_packet.payload_size() == 0) {
            // Ignore padding packets.
          } else if (rtp_packet.PayloadType() == kSendRtxPayloadType) {
            uint16_t original_sequence_number =
                ByteReader<uint16_t>::ReadBigEndian(
                    rtp_packet.payload().data());
            uint32_t original_ssrc =
                rtx_to_media_ssrcs_.find(rtp_packet.Ssrc())->second;
            std::set<uint16_t>* seq_no_map = &dropped_seq_[original_ssrc];
            auto it = seq_no_map->find(original_sequence_number);
            if (it != seq_no_map->end()) {
              retransmit_observed_ = true;
              seq_no_map->erase(it);
            } else {
              rtx_padding_observed_ = true;
            }
          } else {
            streams_observed_.insert(rtp_packet.Ssrc());
          }

          if (IsDone())
            done_.Set();

          if (drop_packet)
            return true;
        }
      }

      return test::DirectTransport::SendRtp(data, options);
    }

    bool IsDone() {
      bool observed_types_ok =
          streams_observed_.size() == MultiStreamTester::kNumStreams &&
          retransmit_observed_ && rtx_padding_observed_;
      if (!observed_types_ok)
        return false;
      // We should not have any gaps in the sequence number range.
      size_t seqno_range =
          *received_packed_ids_.rbegin() - *received_packed_ids_.begin() + 1;
      return seqno_range == received_packed_ids_.size();
    }

    bool Wait() {
      {
        // Can't be sure until this point that rtx_to_media_ssrcs_ etc have
        // been initialized and are OK to read.
        MutexLock lock(&lock_);
        started_ = true;
      }
      return done_.Wait(kDefaultTimeout);
    }

   private:
    Mutex lock_;
    Event done_;
    RtpHeaderExtensionMap extensions_;
    RtpSequenceNumberUnwrapper unwrapper_;
    std::set<int64_t> received_packed_ids_;
    std::set<uint32_t> streams_observed_;
    std::map<uint32_t, std::set<uint16_t>> dropped_seq_;
    const std::map<uint32_t, uint32_t>& rtx_to_media_ssrcs_;
    bool rtx_padding_observed_;
    bool retransmit_observed_;
    bool started_;
  };

  class TransportSequenceNumberTester : public MultiStreamTester {
   public:
    TransportSequenceNumberTester() : observer_(nullptr) {}
    ~TransportSequenceNumberTester() override = default;

   protected:
    void Wait() override {
      RTC_DCHECK(observer_);
      EXPECT_TRUE(observer_->Wait());
    }

    void UpdateSendConfig(
        size_t stream_index,
        VideoSendStream::Config* send_config,
        VideoEncoderConfig* encoder_config,
        test::FrameGeneratorCapturer** frame_generator) override {
      send_config->rtp.extensions.clear();
      send_config->rtp.extensions.push_back(
          RtpExtension(RtpExtension::kTransportSequenceNumberUri,
                       kTransportSequenceNumberExtensionId));

      // Force some padding to be sent. Note that since we do send media
      // packets we can not guarantee that a padding only packet is sent.
      // Instead, padding will most likely be send as an RTX packet.
      const int kPaddingBitrateBps = 50000;
      encoder_config->max_bitrate_bps = 200000;
      encoder_config->min_transmit_bitrate_bps =
          encoder_config->max_bitrate_bps + kPaddingBitrateBps;

      // Configure RTX for redundant payload padding.
      send_config->rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
      send_config->rtp.rtx.ssrcs.push_back(kSendRtxSsrcs[stream_index]);
      send_config->rtp.rtx.payload_type = kSendRtxPayloadType;
      rtx_to_media_ssrcs_[kSendRtxSsrcs[stream_index]] =
          send_config->rtp.ssrcs[0];
    }

    void UpdateReceiveConfig(
        size_t stream_index,
        VideoReceiveStreamInterface::Config* receive_config) override {
      receive_config->rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
      receive_config->renderer = &fake_renderer_;
    }

    std::unique_ptr<test::DirectTransport> CreateSendTransport(
        TaskQueueBase* task_queue,
        Call* sender_call) override {
      std::map<uint8_t, MediaType> payload_type_map =
          MultiStreamTester::payload_type_map_;
      RTC_DCHECK(payload_type_map.find(kSendRtxPayloadType) ==
                 payload_type_map.end());
      payload_type_map[kSendRtxPayloadType] = MediaType::VIDEO;
      std::vector<RtpExtension> extensions = {
          RtpExtension(RtpExtension::kTransportSequenceNumberUri,
                       kTransportSequenceNumberExtensionId)};
      auto observer = std::make_unique<RtpExtensionHeaderObserver>(
          task_queue, sender_call, rtx_to_media_ssrcs_, payload_type_map,
          extensions, extensions);
      observer_ = observer.get();
      return observer;
    }

   private:
    test::FakeVideoRenderer fake_renderer_;
    std::map<uint32_t, uint32_t> rtx_to_media_ssrcs_;
    RtpExtensionHeaderObserver* observer_;
  } tester;

  tester.RunTest();
}

class TransportFeedbackEndToEndTest : public test::CallTest {
 public:
  TransportFeedbackEndToEndTest() {
    RegisterRtpExtension(RtpExtension(RtpExtension::kTransportSequenceNumberUri,
                                      kTransportSequenceNumberExtensionId));
  }
};

class TransportFeedbackTester : public test::EndToEndTest {
 public:
  TransportFeedbackTester(size_t num_video_streams, size_t num_audio_streams)
      : EndToEndTest(test::VideoTestConstants::kDefaultTimeout),
        num_video_streams_(num_video_streams),
        num_audio_streams_(num_audio_streams),
        receiver_call_(nullptr) {
    // Only one stream of each supported for now.
    EXPECT_LE(num_video_streams, 1u);
    EXPECT_LE(num_audio_streams, 1u);
  }

 protected:
  Action OnSendRtcp(ArrayView<const uint8_t> data) override {
    EXPECT_FALSE(HasTransportFeedback(data));
    return SEND_PACKET;
  }

  Action OnReceiveRtcp(ArrayView<const uint8_t> data) override {
    if (HasTransportFeedback(data))
      observation_complete_.Set();
    return SEND_PACKET;
  }

  bool HasTransportFeedback(ArrayView<const uint8_t> data) const {
    test::RtcpPacketParser parser;
    EXPECT_TRUE(parser.Parse(data));
    return parser.transport_feedback()->num_packets() > 0;
  }

  void PerformTest() override {
    EXPECT_TRUE(
        observation_complete_.Wait(test::VideoTestConstants::kDefaultTimeout));
  }

  void OnCallsCreated(Call* sender_call, Call* receiver_call) override {
    receiver_call_ = receiver_call;
  }

  size_t GetNumVideoStreams() const override { return num_video_streams_; }
  size_t GetNumAudioStreams() const override { return num_audio_streams_; }

  void ModifyAudioConfigs(AudioSendStream::Config* send_config,
                          std::vector<AudioReceiveStreamInterface::Config>*
                              receive_configs) override {
    send_config->rtp.extensions.clear();
    send_config->rtp.extensions.push_back(
        RtpExtension(RtpExtension::kTransportSequenceNumberUri,
                     kTransportSequenceNumberExtensionId));
  }

 private:
  const size_t num_video_streams_;
  const size_t num_audio_streams_;
  Call* receiver_call_;
};

TEST_F(TransportFeedbackEndToEndTest, VideoReceivesTransportFeedback) {
  TransportFeedbackTester test(1, 0);
  RunBaseTest(&test);
}
TEST_F(TransportFeedbackEndToEndTest, AudioReceivesTransportFeedback) {
  TransportFeedbackTester test(0, 1);
  RunBaseTest(&test);
}

TEST_F(TransportFeedbackEndToEndTest, AudioVideoReceivesTransportFeedback) {
  TransportFeedbackTester test(1, 1);
  RunBaseTest(&test);
}

TEST_F(TransportFeedbackEndToEndTest,
       StopsAndResumesMediaWhenCongestionWindowFull) {
  field_trials().Set("WebRTC-CongestionWindow", "QueueSize:250");

  class TransportFeedbackTester : public test::EndToEndTest {
   public:
    TransportFeedbackTester(size_t num_video_streams, size_t num_audio_streams)
        : EndToEndTest(test::VideoTestConstants::kDefaultTimeout),
          num_video_streams_(num_video_streams),
          num_audio_streams_(num_audio_streams),
          media_sent_(0),
          media_sent_before_(0),
          padding_sent_(0) {
      // Only one stream of each supported for now.
      EXPECT_LE(num_video_streams, 1u);
      EXPECT_LE(num_audio_streams, 1u);
    }

   protected:
    Action OnSendRtp(ArrayView<const uint8_t> packet) override {
      RtpPacket rtp_packet;
      EXPECT_TRUE(rtp_packet.Parse(packet));
      const bool only_padding = rtp_packet.payload_size() == 0;
      MutexLock lock(&mutex_);
      // Padding is expected in congested state to probe for connectivity when
      // packets has been dropped.
      if (only_padding) {
        media_sent_before_ = media_sent_;
        ++padding_sent_;
      } else {
        ++media_sent_;
        if (padding_sent_ == 0) {
          ++media_sent_before_;
          EXPECT_LT(media_sent_, 40)
              << "Media sent without feedback when congestion window is full.";
        } else if (media_sent_ > media_sent_before_) {
          observation_complete_.Set();
        }
      }
      return SEND_PACKET;
    }

    Action OnReceiveRtcp(ArrayView<const uint8_t> data) override {
      MutexLock lock(&mutex_);
      // To fill up the congestion window we drop feedback on packets after 20
      // packets have been sent. This means that any packets that has not yet
      // received feedback after that will be considered as oustanding data and
      // therefore filling up the congestion window. In the congested state, the
      // pacer should send padding packets to trigger feedback in case all
      // feedback of previous traffic was lost. This test listens for the
      // padding packets and when 2 padding packets have been received, feedback
      // will be let trough again. This should cause the pacer to continue
      // sending meadia yet again.
      if (media_sent_ > 20 && HasTransportFeedback(data) && padding_sent_ < 2) {
        return DROP_PACKET;
      }
      return SEND_PACKET;
    }

    bool HasTransportFeedback(ArrayView<const uint8_t> data) const {
      test::RtcpPacketParser parser;
      EXPECT_TRUE(parser.Parse(data));
      return parser.transport_feedback()->num_packets() > 0;
    }
    void ModifySenderBitrateConfig(
        BitrateConstraints* bitrate_config) override {
      bitrate_config->max_bitrate_bps = 300000;
    }

    void PerformTest() override {
      constexpr TimeDelta kFailureTimeout = TimeDelta::Seconds(10);
      EXPECT_TRUE(observation_complete_.Wait(kFailureTimeout))
          << "Stream not continued after congestion window full.";
    }

    size_t GetNumVideoStreams() const override { return num_video_streams_; }
    size_t GetNumAudioStreams() const override { return num_audio_streams_; }

   private:
    const size_t num_video_streams_;
    const size_t num_audio_streams_;
    Mutex mutex_;
    int media_sent_ RTC_GUARDED_BY(mutex_);
    int media_sent_before_ RTC_GUARDED_BY(mutex_);
    int padding_sent_ RTC_GUARDED_BY(mutex_);
  } test(1, 0);
  RunBaseTest(&test);
}

TEST_F(TransportFeedbackEndToEndTest, TransportSeqNumOnAudioAndVideo) {
  static constexpr size_t kMinPacketsToWaitFor = 50;
  class TransportSequenceNumberTest : public test::EndToEndTest {
   public:
    TransportSequenceNumberTest()
        : EndToEndTest(test::VideoTestConstants::kDefaultTimeout),
          video_observed_(false),
          audio_observed_(false) {
      extensions_.Register<TransportSequenceNumber>(
          kTransportSequenceNumberExtensionId);
    }

    size_t GetNumVideoStreams() const override { return 1; }
    size_t GetNumAudioStreams() const override { return 1; }

    void ModifyAudioConfigs(AudioSendStream::Config* send_config,
                            std::vector<AudioReceiveStreamInterface::Config>*
                                receive_configs) override {
      send_config->rtp.extensions.clear();
      send_config->rtp.extensions.push_back(
          RtpExtension(RtpExtension::kTransportSequenceNumberUri,
                       kTransportSequenceNumberExtensionId));
    }

    Action OnSendRtp(ArrayView<const uint8_t> packet) override {
      RtpPacket rtp_packet(&extensions_);
      EXPECT_TRUE(rtp_packet.Parse(packet));
      uint16_t transport_sequence_number = 0;
      EXPECT_TRUE(rtp_packet.GetExtension<TransportSequenceNumber>(
          &transport_sequence_number));
      // Unwrap packet id and verify uniqueness.
      int64_t packet_id = unwrapper_.Unwrap(transport_sequence_number);
      EXPECT_TRUE(received_packet_ids_.insert(packet_id).second);

      if (rtp_packet.Ssrc() == test::VideoTestConstants::kVideoSendSsrcs[0])
        video_observed_ = true;
      if (rtp_packet.Ssrc() == test::VideoTestConstants::kAudioSendSsrc)
        audio_observed_ = true;
      if (audio_observed_ && video_observed_ &&
          received_packet_ids_.size() >= kMinPacketsToWaitFor) {
        size_t packet_id_range =
            *received_packet_ids_.rbegin() - *received_packet_ids_.begin() + 1;
        EXPECT_EQ(received_packet_ids_.size(), packet_id_range);
        observation_complete_.Set();
      }
      return SEND_PACKET;
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait()) << "Timed out while waiting for audio and video "
                             "packets with transport sequence number.";
    }

    void ExpectSuccessful() {
      EXPECT_TRUE(video_observed_);
      EXPECT_TRUE(audio_observed_);
      EXPECT_GE(received_packet_ids_.size(), kMinPacketsToWaitFor);
    }

   private:
    bool video_observed_;
    bool audio_observed_;
    RtpSequenceNumberUnwrapper unwrapper_;
    std::set<int64_t> received_packet_ids_;
    RtpHeaderExtensionMap extensions_;
  } test;

  RunBaseTest(&test);
  // Double check conditions for successful test to produce better error
  // message when the test fail.
  test.ExpectSuccessful();
}
}  // namespace webrtc
