/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <utility>
#include <vector>

#include "api/audio_options.h"
#include "api/create_modular_peer_connection_factory.h"
#include "api/enable_media_with_defaults.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "api/jsep.h"
#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_event_log/rtc_event_log_factory.h"
#include "api/scoped_refptr.h"
#include "api/test/network_emulation/network_emulation_interfaces.h"
#include "api/test/network_emulation_manager.h"
#include "api/test/rtc_error_matchers.h"
#include "api/test/simulated_network.h"
#include "modules/audio_device/include/test_audio_device.h"
#include "p2p/base/port_allocator.h"
#include "pc/peer_connection_wrapper.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/task_queue_for_test.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/network/network_emulation.h"
#include "test/network/network_emulation_manager.h"
#include "test/network/simulated_network.h"
#include "test/wait_until.h"

namespace webrtc {
namespace test {
namespace {

using ::testing::Eq;
using ::testing::IsTrue;

constexpr int kMaxAptitude = 32000;
constexpr int kSamplingFrequency = 48000;
constexpr char kSignalThreadName[] = "signaling_thread";

bool AddIceCandidates(PeerConnectionWrapper* peer,
                      std::vector<const IceCandidate*> candidates) {
  bool success = true;
  for (const auto candidate : candidates) {
    if (!peer->pc()->AddIceCandidate(candidate)) {
      success = false;
    }
  }
  return success;
}

scoped_refptr<PeerConnectionFactoryInterface> CreatePeerConnectionFactory(
    Thread* signaling_thread,
    EmulatedNetworkManagerInterface* network) {
  const Environment env = CreateEnvironment();
  PeerConnectionFactoryDependencies pcf_deps;
  pcf_deps.env = env;
  pcf_deps.event_log_factory = std::make_unique<RtcEventLogFactory>();
  pcf_deps.network_thread = network->network_thread();
  pcf_deps.signaling_thread = signaling_thread;
  pcf_deps.socket_factory = network->socket_factory();
  pcf_deps.network_manager = network->ReleaseNetworkManager();

  pcf_deps.adm = TestAudioDeviceModule::Create(
      env,
      TestAudioDeviceModule::CreatePulsedNoiseCapturer(kMaxAptitude,
                                                       kSamplingFrequency),
      TestAudioDeviceModule::CreateDiscardRenderer(kSamplingFrequency),
      /*speed=*/1.f);
  EnableMediaWithDefaults(pcf_deps);
  return CreateModularPeerConnectionFactory(std::move(pcf_deps));
}

scoped_refptr<PeerConnectionInterface> CreatePeerConnection(
    const scoped_refptr<PeerConnectionFactoryInterface>& pcf,
    PeerConnectionObserver* observer,
    EmulatedTURNServerInterface* turn_server = nullptr) {
  PeerConnectionDependencies pc_deps(observer);
  PeerConnectionInterface::RTCConfiguration rtc_configuration;
  rtc_configuration.sdp_semantics = SdpSemantics::kUnifiedPlan;
  // This test does not support TCP
  rtc_configuration.port_allocator_config.flags = PORTALLOCATOR_DISABLE_TCP;
  if (turn_server != nullptr) {
    PeerConnectionInterface::IceServer server;
    server.username = turn_server->GetIceServerConfig().username;
    server.password = turn_server->GetIceServerConfig().username;
    server.urls.push_back(turn_server->GetIceServerConfig().url);
    rtc_configuration.servers.push_back(server);
  }

  auto result =
      pcf->CreatePeerConnectionOrError(rtc_configuration, std::move(pc_deps));
  if (!result.ok()) {
    return nullptr;
  }
  return result.MoveValue();
}

}  // namespace

TEST(NetworkEmulationManagerPCTest, Run) {
  std::unique_ptr<Thread> signaling_thread = Thread::Create();
  signaling_thread->SetName(kSignalThreadName, nullptr);
  signaling_thread->Start();

  // Setup emulated network
  NetworkEmulationManagerImpl emulation({.time_mode = TimeMode::kRealTime});

  EmulatedNetworkNode* alice_node = emulation.CreateEmulatedNode(
      std::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedNetworkNode* bob_node = emulation.CreateEmulatedNode(
      std::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedEndpoint* alice_endpoint =
      emulation.CreateEndpoint(EmulatedEndpointConfig());
  EmulatedEndpoint* bob_endpoint =
      emulation.CreateEndpoint(EmulatedEndpointConfig());
  emulation.CreateRoute(alice_endpoint, {alice_node}, bob_endpoint);
  emulation.CreateRoute(bob_endpoint, {bob_node}, alice_endpoint);

  EmulatedNetworkManagerInterface* alice_network =
      emulation.CreateEmulatedNetworkManagerInterface({alice_endpoint});
  EmulatedNetworkManagerInterface* bob_network =
      emulation.CreateEmulatedNetworkManagerInterface({bob_endpoint});

  // Setup peer connections.
  scoped_refptr<PeerConnectionFactoryInterface> alice_pcf;
  scoped_refptr<PeerConnectionInterface> alice_pc;
  std::unique_ptr<MockPeerConnectionObserver> alice_observer =
      std::make_unique<MockPeerConnectionObserver>();

  scoped_refptr<PeerConnectionFactoryInterface> bob_pcf;
  scoped_refptr<PeerConnectionInterface> bob_pc;
  std::unique_ptr<MockPeerConnectionObserver> bob_observer =
      std::make_unique<MockPeerConnectionObserver>();

  SendTask(signaling_thread.get(), [&]() {
    alice_pcf =
        CreatePeerConnectionFactory(signaling_thread.get(), alice_network);
    alice_pc = CreatePeerConnection(alice_pcf, alice_observer.get());

    bob_pcf = CreatePeerConnectionFactory(signaling_thread.get(), bob_network);
    bob_pc = CreatePeerConnection(bob_pcf, bob_observer.get());
  });

  std::unique_ptr<PeerConnectionWrapper> alice =
      std::make_unique<PeerConnectionWrapper>(alice_pcf, alice_pc,
                                              std::move(alice_observer));
  std::unique_ptr<PeerConnectionWrapper> bob =
      std::make_unique<PeerConnectionWrapper>(bob_pcf, bob_pc,
                                              std::move(bob_observer));

  SendTask(signaling_thread.get(), [&]() {
    scoped_refptr<AudioSourceInterface> source =
        alice_pcf->CreateAudioSource(AudioOptions());
    scoped_refptr<AudioTrackInterface> track =
        alice_pcf->CreateAudioTrack("audio", source.get());
    alice->AddTransceiver(track);

    // Connect peers.
    ASSERT_TRUE(alice->ExchangeOfferAnswerWith(bob.get()));
    // Do the SDP negotiation, and also exchange ice candidates.
    ASSERT_THAT(WaitUntil([&] { return alice->signaling_state(); },
                          Eq(PeerConnectionInterface::kStable)),
                IsRtcOk());
    ASSERT_THAT(
        WaitUntil([&] { return alice->IsIceGatheringDone(); }, IsTrue()),
        IsRtcOk());
    ASSERT_THAT(WaitUntil([&] { return bob->IsIceGatheringDone(); }, IsTrue()),
                IsRtcOk());

    // Connect an ICE candidate pairs.
    ASSERT_TRUE(
        AddIceCandidates(bob.get(), alice->observer()->GetAllCandidates()));
    ASSERT_TRUE(
        AddIceCandidates(alice.get(), bob->observer()->GetAllCandidates()));
    // This means that ICE and DTLS are connected.
    ASSERT_THAT(WaitUntil([&] { return bob->IsIceConnected(); }, IsTrue()),
                IsRtcOk());
    ASSERT_THAT(WaitUntil([&] { return alice->IsIceConnected(); }, IsTrue()),
                IsRtcOk());

    // Close peer connections
    alice->pc()->Close();
    bob->pc()->Close();

    // Delete peers.
    alice.reset();
    bob.reset();
  });
}

TEST(NetworkEmulationManagerPCTest, RunTURN) {
  std::unique_ptr<Thread> signaling_thread = Thread::Create();
  signaling_thread->SetName(kSignalThreadName, nullptr);
  signaling_thread->Start();

  // Setup emulated network
  NetworkEmulationManagerImpl emulation({.time_mode = TimeMode::kRealTime});

  EmulatedNetworkNode* alice_node = emulation.CreateEmulatedNode(
      std::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedNetworkNode* bob_node = emulation.CreateEmulatedNode(
      std::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedNetworkNode* turn_node = emulation.CreateEmulatedNode(
      std::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedEndpoint* alice_endpoint =
      emulation.CreateEndpoint(EmulatedEndpointConfig());
  EmulatedEndpoint* bob_endpoint =
      emulation.CreateEndpoint(EmulatedEndpointConfig());
  EmulatedTURNServerInterface* alice_turn =
      emulation.CreateTURNServer(EmulatedTURNServerConfig());
  EmulatedTURNServerInterface* bob_turn =
      emulation.CreateTURNServer(EmulatedTURNServerConfig());

  emulation.CreateRoute(alice_endpoint, {alice_node},
                        alice_turn->GetClientEndpoint());
  emulation.CreateRoute(alice_turn->GetClientEndpoint(), {alice_node},
                        alice_endpoint);

  emulation.CreateRoute(bob_endpoint, {bob_node},
                        bob_turn->GetClientEndpoint());
  emulation.CreateRoute(bob_turn->GetClientEndpoint(), {bob_node},
                        bob_endpoint);

  emulation.CreateRoute(alice_turn->GetPeerEndpoint(), {turn_node},
                        bob_turn->GetPeerEndpoint());
  emulation.CreateRoute(bob_turn->GetPeerEndpoint(), {turn_node},
                        alice_turn->GetPeerEndpoint());

  EmulatedNetworkManagerInterface* alice_network =
      emulation.CreateEmulatedNetworkManagerInterface({alice_endpoint});
  EmulatedNetworkManagerInterface* bob_network =
      emulation.CreateEmulatedNetworkManagerInterface({bob_endpoint});

  // Setup peer connections.
  scoped_refptr<PeerConnectionFactoryInterface> alice_pcf;
  scoped_refptr<PeerConnectionInterface> alice_pc;
  std::unique_ptr<MockPeerConnectionObserver> alice_observer =
      std::make_unique<MockPeerConnectionObserver>();

  scoped_refptr<PeerConnectionFactoryInterface> bob_pcf;
  scoped_refptr<PeerConnectionInterface> bob_pc;
  std::unique_ptr<MockPeerConnectionObserver> bob_observer =
      std::make_unique<MockPeerConnectionObserver>();

  SendTask(signaling_thread.get(), [&]() {
    alice_pcf =
        CreatePeerConnectionFactory(signaling_thread.get(), alice_network);
    alice_pc =
        CreatePeerConnection(alice_pcf, alice_observer.get(), alice_turn);

    bob_pcf = CreatePeerConnectionFactory(signaling_thread.get(), bob_network);
    bob_pc = CreatePeerConnection(bob_pcf, bob_observer.get(), bob_turn);
  });

  std::unique_ptr<PeerConnectionWrapper> alice =
      std::make_unique<PeerConnectionWrapper>(alice_pcf, alice_pc,
                                              std::move(alice_observer));
  std::unique_ptr<PeerConnectionWrapper> bob =
      std::make_unique<PeerConnectionWrapper>(bob_pcf, bob_pc,
                                              std::move(bob_observer));

  SendTask(signaling_thread.get(), [&]() {
    scoped_refptr<AudioSourceInterface> source =
        alice_pcf->CreateAudioSource(AudioOptions());
    scoped_refptr<AudioTrackInterface> track =
        alice_pcf->CreateAudioTrack("audio", source.get());
    alice->AddTransceiver(track);

    // Connect peers.
    ASSERT_TRUE(alice->ExchangeOfferAnswerWith(bob.get()));
    // Do the SDP negotiation, and also exchange ice candidates.
    ASSERT_THAT(WaitUntil([&] { return alice->signaling_state(); },
                          Eq(PeerConnectionInterface::kStable)),
                IsRtcOk());
    ASSERT_THAT(
        WaitUntil([&] { return alice->IsIceGatheringDone(); }, IsTrue()),
        IsRtcOk());
    ASSERT_THAT(WaitUntil([&] { return bob->IsIceGatheringDone(); }, IsTrue()),
                IsRtcOk());

    // Connect an ICE candidate pairs.
    ASSERT_TRUE(
        AddIceCandidates(bob.get(), alice->observer()->GetAllCandidates()));
    ASSERT_TRUE(
        AddIceCandidates(alice.get(), bob->observer()->GetAllCandidates()));
    // This means that ICE and DTLS are connected.
    ASSERT_THAT(WaitUntil([&] { return bob->IsIceConnected(); }, IsTrue()),
                IsRtcOk());
    ASSERT_THAT(WaitUntil([&] { return alice->IsIceConnected(); }, IsTrue()),
                IsRtcOk());

    // Close peer connections
    alice->pc()->Close();
    bob->pc()->Close();

    // Delete peers.
    alice.reset();
    bob.reset();
  });
}

}  // namespace test
}  // namespace webrtc
