/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "api/audio_options.h"
#include "api/create_modular_peer_connection_factory.h"
#include "api/enable_media_with_defaults.h"
#include "api/jsep.h"
#include "api/make_ref_counted.h"
#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/stats/rtc_stats.h"
#include "api/stats/rtc_stats_report.h"
#include "api/stats/rtcstats_objects.h"
#include "api/test/metrics/global_metrics_logger_and_exporter.h"
#include "api/test/metrics/metric.h"
#include "api/test/rtc_error_matchers.h"
#include "api/video_codecs/video_decoder_factory_template.h"
#include "api/video_codecs/video_decoder_factory_template_dav1d_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_open_h264_adapter.h"
#include "api/video_codecs/video_encoder_factory_template.h"
#include "api/video_codecs/video_encoder_factory_template_libaom_av1_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_open_h264_adapter.h"
#include "p2p/base/port_interface.h"
#include "p2p/test/test_turn_server.h"
#include "pc/peer_connection.h"
#include "pc/peer_connection_wrapper.h"
#include "pc/test/fake_audio_capture_module.h"
#include "pc/test/frame_generator_capturer_video_track_source.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/checks.h"
#include "rtc_base/crypto_random.h"
#include "rtc_base/fake_network.h"
#include "rtc_base/firewall_socket_server.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/socket_factory.h"
#include "rtc_base/task_queue_for_test.h"
#include "rtc_base/test_certificate_verifier.h"
#include "rtc_base/thread.h"
#include "rtc_base/virtual_socket_server.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/wait_until.h"

namespace webrtc {
namespace {

using test::GetGlobalMetricsLogger;
using test::ImprovementDirection;
using test::Unit;

const int kDefaultTestTimeMs = 15000;
const int kRampUpTimeMs = 5000;
const int kPollIntervalTimeMs = 50;
const SocketAddress kDefaultLocalAddress("1.1.1.1", 0);
const char kTurnInternalAddress[] = "88.88.88.0";
const char kTurnExternalAddress[] = "88.88.88.1";
const int kTurnInternalPort = 3478;
const int kTurnExternalPort = 0;
// The video's configured max bitrate in webrtcvideoengine.cc is 1.7 Mbps.
// Setting the network bandwidth to 1 Mbps allows the video's bitrate to push
// the network's limitations.
const int kNetworkBandwidth = 1000000;

}  // namespace

using RTCConfiguration = PeerConnectionInterface::RTCConfiguration;

// This is an end to end test to verify that BWE is functioning when setting
// up a one to one call at the PeerConnection level. The intention of the test
// is to catch potential regressions for different ICE path configurations. The
// test uses a VirtualSocketServer for it's underlying simulated network and
// fake audio and video sources. The test is based upon rampup_tests.cc, but
// instead is at the PeerConnection level and uses a different fake network
// (rampup_tests.cc uses SimulatedNetwork). In the future, this test could
// potentially test different network conditions and test video quality as well
// (video_quality_test.cc does this, but at the call level).
//
// The perf test results are printed using the perf test support. If the
// isolated_script_test_perf_output flag is specified in test_main.cc, then
// the results are written to a JSON formatted file for the Chrome perf
// dashboard. Since this test is a webrtc_perf_test, it will be run in the perf
// console every webrtc commit.
class PeerConnectionWrapperForRampUpTest : public PeerConnectionWrapper {
 public:
  using PeerConnectionWrapper::PeerConnectionWrapper;

  PeerConnectionWrapperForRampUpTest(
      scoped_refptr<PeerConnectionFactoryInterface> pc_factory,
      scoped_refptr<PeerConnectionInterface> pc,
      std::unique_ptr<MockPeerConnectionObserver> observer)
      : PeerConnectionWrapper::PeerConnectionWrapper(pc_factory,
                                                     pc,
                                                     std::move(observer)) {}

  bool AddIceCandidates(std::vector<const IceCandidate*> candidates) {
    bool success = true;
    for (const auto candidate : candidates) {
      if (!pc()->AddIceCandidate(candidate)) {
        success = false;
      }
    }
    return success;
  }

  scoped_refptr<VideoTrackInterface> CreateLocalVideoTrack(
      FrameGeneratorCapturerVideoTrackSource::Config config,
      Clock* clock) {
    video_track_sources_.emplace_back(
        make_ref_counted<FrameGeneratorCapturerVideoTrackSource>(
            config, clock, /*is_screencast=*/false));
    video_track_sources_.back()->Start();
    return scoped_refptr<VideoTrackInterface>(pc_factory()->CreateVideoTrack(
        video_track_sources_.back(), CreateRandomUuid()));
  }

  scoped_refptr<AudioTrackInterface> CreateLocalAudioTrack(
      const AudioOptions options) {
    scoped_refptr<AudioSourceInterface> source =
        pc_factory()->CreateAudioSource(options);
    return pc_factory()->CreateAudioTrack(CreateRandomUuid(), source.get());
  }

 private:
  std::vector<scoped_refptr<FrameGeneratorCapturerVideoTrackSource>>
      video_track_sources_;
};

// TODO(shampson): Paramaterize the test to run for both Plan B & Unified Plan.
class PeerConnectionRampUpTest : public ::testing::Test {
 public:
  PeerConnectionRampUpTest()
      : clock_(Clock::GetRealTimeClock()),
        firewall_socket_server_(&virtual_socket_server_),
        network_thread_(&firewall_socket_server_),
        worker_thread_(Thread::Create()) {
    network_thread_.SetName("PCNetworkThread", this);
    worker_thread_->SetName("PCWorkerThread", this);
    RTC_CHECK(network_thread_.Start());
    RTC_CHECK(worker_thread_->Start());

    virtual_socket_server_.set_bandwidth(kNetworkBandwidth / 8);
  }

  ~PeerConnectionRampUpTest() override {
    SendTask(network_thread(), [this] { turn_servers_.clear(); });
  }

  bool CreatePeerConnectionWrappers(const RTCConfiguration& caller_config,
                                    const RTCConfiguration& callee_config) {
    caller_ = CreatePeerConnectionWrapper(caller_config);
    callee_ = CreatePeerConnectionWrapper(callee_config);
    return caller_ && callee_;
  }

  std::unique_ptr<PeerConnectionWrapperForRampUpTest>
  CreatePeerConnectionWrapper(const RTCConfiguration& config) {
    PeerConnectionFactoryDependencies pcf_deps;
    pcf_deps.network_thread = network_thread();
    pcf_deps.worker_thread = worker_thread_.get();
    pcf_deps.signaling_thread = Thread::Current();
    pcf_deps.socket_factory = &firewall_socket_server_;
    auto network_manager =
        std::make_unique<FakeNetworkManager>(network_thread());
    network_manager->AddInterface(kDefaultLocalAddress);
    pcf_deps.network_manager = std::move(network_manager);
    pcf_deps.adm = FakeAudioCaptureModule::Create();
    pcf_deps.video_encoder_factory =
        std::make_unique<VideoEncoderFactoryTemplate<
            LibvpxVp8EncoderTemplateAdapter, LibvpxVp9EncoderTemplateAdapter,
            OpenH264EncoderTemplateAdapter, LibaomAv1EncoderTemplateAdapter>>();
    pcf_deps.video_decoder_factory =
        std::make_unique<VideoDecoderFactoryTemplate<
            LibvpxVp8DecoderTemplateAdapter, LibvpxVp9DecoderTemplateAdapter,
            OpenH264DecoderTemplateAdapter, Dav1dDecoderTemplateAdapter>>();
    EnableMediaWithDefaults(pcf_deps);
    scoped_refptr<PeerConnectionFactoryInterface> pc_factory =
        CreateModularPeerConnectionFactory(std::move(pcf_deps));

    auto observer = std::make_unique<MockPeerConnectionObserver>();
    PeerConnectionDependencies dependencies(observer.get());
    dependencies.tls_cert_verifier =
        std::make_unique<TestCertificateVerifier>();

    auto result = pc_factory->CreatePeerConnectionOrError(
        config, std::move(dependencies));
    if (!result.ok()) {
      return nullptr;
    }

    return std::make_unique<PeerConnectionWrapperForRampUpTest>(
        std::move(pc_factory), result.MoveValue(), std::move(observer));
  }

  void SetupOneWayCall() {
    ASSERT_TRUE(caller_);
    ASSERT_TRUE(callee_);
    FrameGeneratorCapturerVideoTrackSource::Config config;
    caller_->AddTrack(caller_->CreateLocalVideoTrack(config, clock_));
    // Disable highpass filter so that we can get all the test audio frames.
    AudioOptions options;
    options.highpass_filter = false;
    caller_->AddTrack(caller_->CreateLocalAudioTrack(options));

    // Do the SDP negotiation, and also exchange ice candidates.
    ASSERT_TRUE(caller_->ExchangeOfferAnswerWith(callee_.get()));
    ASSERT_THAT(WaitUntil([&] { return caller_->signaling_state(); },
                          ::testing::Eq(PeerConnectionInterface::kStable)),
                IsRtcOk());
    ASSERT_THAT(WaitUntil([&] { return caller_->IsIceGatheringDone(); },
                          ::testing::IsTrue()),
                IsRtcOk());
    ASSERT_THAT(WaitUntil([&] { return callee_->IsIceGatheringDone(); },
                          ::testing::IsTrue()),
                IsRtcOk());

    // Connect an ICE candidate pairs.
    ASSERT_TRUE(
        callee_->AddIceCandidates(caller_->observer()->GetAllCandidates()));
    ASSERT_TRUE(
        caller_->AddIceCandidates(callee_->observer()->GetAllCandidates()));
    // This means that ICE and DTLS are connected.
    ASSERT_THAT(WaitUntil([&] { return callee_->IsIceConnected(); },
                          ::testing::IsTrue()),
                IsRtcOk());
    ASSERT_THAT(WaitUntil([&] { return caller_->IsIceConnected(); },
                          ::testing::IsTrue()),
                IsRtcOk());
  }

  void CreateTurnServer(ProtocolType type,
                        const std::string& common_name = "test turn server") {
    Thread* thread = network_thread();
    SocketFactory* factory = &firewall_socket_server_;
    std::unique_ptr<TestTurnServer> turn_server;
    SendTask(network_thread(), [&] {
      static const SocketAddress turn_server_internal_address{
          kTurnInternalAddress, kTurnInternalPort};
      static const SocketAddress turn_server_external_address{
          kTurnExternalAddress, kTurnExternalPort};
      turn_server = std::make_unique<TestTurnServer>(
          thread, factory, turn_server_internal_address,
          turn_server_external_address, type, true /*ignore_bad_certs=*/,
          common_name);
    });
    turn_servers_.push_back(std::move(turn_server));
  }

  // First runs the call for kRampUpTimeMs to ramp up the bandwidth estimate.
  // Then runs the test for the remaining test time, grabbing the bandwidth
  // estimation stat, every kPollIntervalTimeMs. When finished, averages the
  // bandwidth estimations and prints the bandwidth estimation result as a perf
  // metric.
  void RunTest(const std::string& test_string) {
    Thread::Current()->ProcessMessages(kRampUpTimeMs);
    int number_of_polls =
        (kDefaultTestTimeMs - kRampUpTimeMs) / kPollIntervalTimeMs;
    int total_bwe = 0;
    for (int i = 0; i < number_of_polls; ++i) {
      Thread::Current()->ProcessMessages(kPollIntervalTimeMs);
      total_bwe += static_cast<int>(GetCallerAvailableBitrateEstimate());
    }
    double average_bandwidth_estimate = total_bwe / number_of_polls;
    std::string value_description =
        "bwe_after_" + std::to_string(kDefaultTestTimeMs / 1000) + "_seconds";
    GetGlobalMetricsLogger()->LogSingleValueMetric(
        "peerconnection_ramp_up_" + test_string, value_description,
        average_bandwidth_estimate, Unit::kUnitless,
        ImprovementDirection::kNeitherIsBetter);
  }

  Thread* network_thread() { return &network_thread_; }

  FirewallSocketServer* firewall_socket_server() {
    return &firewall_socket_server_;
  }

  PeerConnectionWrapperForRampUpTest* caller() { return caller_.get(); }

  PeerConnectionWrapperForRampUpTest* callee() { return callee_.get(); }

 private:
  // Gets the caller's outgoing available bitrate from the stats. Returns 0 if
  // something went wrong. It takes the outgoing bitrate from the current
  // selected ICE candidate pair's stats.
  double GetCallerAvailableBitrateEstimate() {
    auto stats = caller_->GetStats();
    auto transport_stats = stats->GetStatsOfType<RTCTransportStats>();
    if (transport_stats.empty() ||
        !transport_stats[0]->selected_candidate_pair_id.has_value()) {
      return 0;
    }
    std::string selected_ice_id =
        transport_stats[0]
            ->GetAttribute(transport_stats[0]->selected_candidate_pair_id)
            .ToString();
    // Use the selected ICE candidate pair ID to get the appropriate ICE stats.
    const RTCIceCandidatePairStats ice_candidate_pair_stats =
        stats->Get(selected_ice_id)->cast_to<const RTCIceCandidatePairStats>();
    if (ice_candidate_pair_stats.available_outgoing_bitrate.has_value()) {
      return *ice_candidate_pair_stats.available_outgoing_bitrate;
    }
    // We couldn't get the `available_outgoing_bitrate` for the active candidate
    // pair.
    return 0;
  }

  Clock* const clock_;
  // The turn servers should be accessed & deleted on the network thread to
  // avoid a race with the socket read/write which occurs on the network thread.
  std::vector<std::unique_ptr<TestTurnServer>> turn_servers_;
  // `virtual_socket_server_` is used by `network_thread_` so it must be
  // destroyed later.
  // TODO(bugs.webrtc.org/7668): We would like to update the virtual network we
  // use for this test. VirtualSocketServer isn't ideal because:
  // 1) It uses the same queue & network capacity for both directions.
  // 2) VirtualSocketServer implements how the network bandwidth affects the
  //    send delay differently than the SimulatedNetwork, used by the
  //    FakeNetworkPipe. It would be ideal if all of levels of virtual
  //    networks used in testing were consistent.
  // We would also like to update this test to record the time to ramp up,
  // down, and back up (similar to in rampup_tests.cc). This is problematic with
  // the VirtualSocketServer. The first ramp down time is very noisy and the
  // second ramp up time can take up to 300 seconds, most likely due to a built
  // up queue.
  VirtualSocketServer virtual_socket_server_;
  FirewallSocketServer firewall_socket_server_;

  Thread network_thread_;
  std::unique_ptr<Thread> worker_thread_;

  std::unique_ptr<PeerConnectionWrapperForRampUpTest> caller_;
  std::unique_ptr<PeerConnectionWrapperForRampUpTest> callee_;
};

TEST_F(PeerConnectionRampUpTest, Bwe_After_TurnOverTCP) {
  CreateTurnServer(ProtocolType::PROTO_TCP);
  PeerConnectionInterface::IceServer ice_server;
  std::string ice_server_url = "turn:" + std::string(kTurnInternalAddress) +
                               ":" + std::to_string(kTurnInternalPort) +
                               "?transport=tcp";
  ice_server.urls.push_back(ice_server_url);
  ice_server.username = "test";
  ice_server.password = "test";
  PeerConnectionInterface::RTCConfiguration client_1_config;
  client_1_config.sdp_semantics = SdpSemantics::kUnifiedPlan;
  client_1_config.servers.push_back(ice_server);
  client_1_config.type = PeerConnectionInterface::kRelay;
  PeerConnectionInterface::RTCConfiguration client_2_config;
  client_2_config.sdp_semantics = SdpSemantics::kUnifiedPlan;
  client_2_config.servers.push_back(ice_server);
  client_2_config.type = PeerConnectionInterface::kRelay;
  ASSERT_TRUE(CreatePeerConnectionWrappers(client_1_config, client_2_config));

  SetupOneWayCall();
  RunTest("turn_over_tcp");
}

TEST_F(PeerConnectionRampUpTest, Bwe_After_TurnOverUDP) {
  CreateTurnServer(ProtocolType::PROTO_UDP);
  PeerConnectionInterface::IceServer ice_server;
  std::string ice_server_url = "turn:" + std::string(kTurnInternalAddress) +
                               ":" + std::to_string(kTurnInternalPort);

  ice_server.urls.push_back(ice_server_url);
  ice_server.username = "test";
  ice_server.password = "test";
  PeerConnectionInterface::RTCConfiguration client_1_config;
  client_1_config.sdp_semantics = SdpSemantics::kUnifiedPlan;
  client_1_config.servers.push_back(ice_server);
  client_1_config.type = PeerConnectionInterface::kRelay;
  PeerConnectionInterface::RTCConfiguration client_2_config;
  client_2_config.sdp_semantics = SdpSemantics::kUnifiedPlan;
  client_2_config.servers.push_back(ice_server);
  client_2_config.type = PeerConnectionInterface::kRelay;
  ASSERT_TRUE(CreatePeerConnectionWrappers(client_1_config, client_2_config));

  SetupOneWayCall();
  RunTest("turn_over_udp");
}

TEST_F(PeerConnectionRampUpTest, Bwe_After_TurnOverTLS) {
  CreateTurnServer(ProtocolType::PROTO_TLS, kTurnInternalAddress);
  PeerConnectionInterface::IceServer ice_server;
  std::string ice_server_url = "turns:" + std::string(kTurnInternalAddress) +
                               ":" + std::to_string(kTurnInternalPort) +
                               "?transport=tcp";
  ice_server.urls.push_back(ice_server_url);
  ice_server.username = "test";
  ice_server.password = "test";
  PeerConnectionInterface::RTCConfiguration client_1_config;
  client_1_config.sdp_semantics = SdpSemantics::kUnifiedPlan;
  client_1_config.servers.push_back(ice_server);
  client_1_config.type = PeerConnectionInterface::kRelay;
  PeerConnectionInterface::RTCConfiguration client_2_config;
  client_2_config.sdp_semantics = SdpSemantics::kUnifiedPlan;
  client_2_config.servers.push_back(ice_server);
  client_2_config.type = PeerConnectionInterface::kRelay;

  ASSERT_TRUE(CreatePeerConnectionWrappers(client_1_config, client_2_config));

  SetupOneWayCall();
  RunTest("turn_over_tls");
}

TEST_F(PeerConnectionRampUpTest, Bwe_After_UDPPeerToPeer) {
  PeerConnectionInterface::RTCConfiguration client_1_config;
  client_1_config.sdp_semantics = SdpSemantics::kUnifiedPlan;
  client_1_config.tcp_candidate_policy =
      PeerConnection::kTcpCandidatePolicyDisabled;
  PeerConnectionInterface::RTCConfiguration client_2_config;
  client_2_config.sdp_semantics = SdpSemantics::kUnifiedPlan;
  client_2_config.tcp_candidate_policy =
      PeerConnection::kTcpCandidatePolicyDisabled;
  ASSERT_TRUE(CreatePeerConnectionWrappers(client_1_config, client_2_config));

  SetupOneWayCall();
  RunTest("udp_peer_to_peer");
}

TEST_F(PeerConnectionRampUpTest, Bwe_After_TCPPeerToPeer) {
  firewall_socket_server()->set_udp_sockets_enabled(false);
  PeerConnectionInterface::RTCConfiguration config;
  config.sdp_semantics = SdpSemantics::kUnifiedPlan;
  ASSERT_TRUE(CreatePeerConnectionWrappers(config, config));

  SetupOneWayCall();
  RunTest("tcp_peer_to_peer");
}

}  // namespace webrtc
