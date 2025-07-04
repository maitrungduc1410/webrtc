/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/peer_scenario/scenario_connection.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "api/candidate.h"
#include "api/environment/environment.h"
#include "api/jsep.h"
#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/test/network_emulation/network_emulation_interfaces.h"
#include "api/test/network_emulation_manager.h"
#include "api/transport/data_channel_transport_interface.h"
#include "api/transport/enums.h"
#include "call/payload_type_picker.h"
#include "call/rtp_demuxer.h"
#include "call/rtp_packet_sink_interface.h"
#include "media/base/rtp_utils.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "p2p/base/basic_packet_socket_factory.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/base/port_allocator.h"
#include "p2p/base/transport_description.h"
#include "p2p/client/basic_port_allocator.h"
#include "p2p/dtls/dtls_transport_internal.h"
#include "pc/dtls_transport.h"
#include "pc/jsep_transport_controller.h"
#include "pc/rtp_transport_internal.h"
#include "pc/session_description.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/crypto_random.h"
#include "rtc_base/network.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/ssl_fingerprint.h"
#include "rtc_base/ssl_identity.h"
#include "rtc_base/task_queue_for_test.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "test/network/network_emulation_manager.h"

namespace webrtc {
class ScenarioIceConnectionImpl : public ScenarioIceConnection,
                                  public sigslot::has_slots<>,
                                  private JsepTransportController::Observer,
                                  private RtpPacketSinkInterface {
 public:
  ScenarioIceConnectionImpl(const Environment& env,
                            test::NetworkEmulationManagerImpl* net,
                            IceConnectionObserver* observer);
  ~ScenarioIceConnectionImpl() override;

  void SendRtpPacket(ArrayView<const uint8_t> packet_view) override;
  void SendRtcpPacket(ArrayView<const uint8_t> packet_view) override;

  void SetRemoteSdp(SdpType type, const std::string& remote_sdp) override;
  void SetLocalSdp(SdpType type, const std::string& local_sdp) override;

  EmulatedEndpoint* endpoint() override { return endpoint_; }
  const TransportDescription& transport_description() const override {
    return transport_description_;
  }

 private:
  JsepTransportController::Config CreateJsepConfig();
  bool OnTransportChanged(
      const std::string& mid,
      RtpTransportInternal* rtp_transport,
      scoped_refptr<DtlsTransport> dtls_transport,
      DataChannelTransportInterface* data_channel_transport) override;

  void OnRtpPacket(const RtpPacketReceived& packet) override;
  void OnCandidates(const std::string& mid,
                    const std::vector<Candidate>& candidates);

  IceConnectionObserver* const observer_;
  EmulatedEndpoint* const endpoint_;
  EmulatedNetworkManagerInterface* const manager_;
  Thread* const signaling_thread_;
  Thread* const network_thread_;
  scoped_refptr<RTCCertificate> const certificate_
      RTC_GUARDED_BY(network_thread_);
  TransportDescription const transport_description_
      RTC_GUARDED_BY(signaling_thread_);
  std::unique_ptr<NetworkManager> network_manager_;
  BasicPacketSocketFactory packet_socket_factory_;
  std::unique_ptr<BasicPortAllocator> port_allocator_
      RTC_GUARDED_BY(network_thread_);
  PayloadTypePicker payload_type_picker_;
  std::unique_ptr<JsepTransportController> jsep_controller_;
  RtpTransportInternal* rtp_transport_ RTC_GUARDED_BY(network_thread_) =
      nullptr;
  std::unique_ptr<SessionDescriptionInterface> remote_description_
      RTC_GUARDED_BY(signaling_thread_);
  std::unique_ptr<SessionDescriptionInterface> local_description_
      RTC_GUARDED_BY(signaling_thread_);
};

std::unique_ptr<ScenarioIceConnection> ScenarioIceConnection::Create(
    const Environment& env,
    test::NetworkEmulationManagerImpl* net,
    IceConnectionObserver* observer) {
  return std::make_unique<ScenarioIceConnectionImpl>(env, net, observer);
}

ScenarioIceConnectionImpl::ScenarioIceConnectionImpl(
    const Environment& env,
    test::NetworkEmulationManagerImpl* net,
    IceConnectionObserver* observer)
    : observer_(observer),
      endpoint_(net->CreateEndpoint(EmulatedEndpointConfig())),
      manager_(net->CreateEmulatedNetworkManagerInterface({endpoint_})),
      signaling_thread_(Thread::Current()),
      network_thread_(manager_->network_thread()),
      certificate_(RTCCertificate::Create(SSLIdentity::Create("", KT_DEFAULT))),
      transport_description_(
          /*transport_options*/ {},
          CreateRandomString(ICE_UFRAG_LENGTH),
          CreateRandomString(ICE_PWD_LENGTH),
          IceMode::ICEMODE_FULL,
          ConnectionRole::CONNECTIONROLE_PASSIVE,
          SSLFingerprint::CreateFromCertificate(*certificate_.get()).get()),
      network_manager_(manager_->ReleaseNetworkManager()),
      packet_socket_factory_(manager_->socket_factory()),
      port_allocator_(
          std::make_unique<BasicPortAllocator>(env,
                                               network_manager_.get(),
                                               &packet_socket_factory_)),
      jsep_controller_(
          new JsepTransportController(env,
                                      network_thread_,
                                      port_allocator_.get(),
                                      /*async_resolver_factory*/ nullptr,
                                      /*lna_permission_factory*/ nullptr,
                                      payload_type_picker_,
                                      CreateJsepConfig())) {
  SendTask(network_thread_, [this] {
    RTC_DCHECK_RUN_ON(network_thread_);
    uint32_t flags = PORTALLOCATOR_DISABLE_TCP;
    port_allocator_->set_flags(port_allocator_->flags() | flags);
    port_allocator_->Initialize();
    RTC_CHECK(port_allocator_->SetConfiguration(/*stun_servers*/ {},
                                                /*turn_servers*/ {}, 0,
                                                NO_PRUNE));
    jsep_controller_->SetLocalCertificate(certificate_);
  });
}

ScenarioIceConnectionImpl::~ScenarioIceConnectionImpl() {
  SendTask(network_thread_, [this] {
    RTC_DCHECK_RUN_ON(network_thread_);
    jsep_controller_.reset();
    port_allocator_.reset();
    rtp_transport_ = nullptr;
  });
}

JsepTransportController::Config ScenarioIceConnectionImpl::CreateJsepConfig() {
  JsepTransportController::Config config;
  config.transport_observer = this;
  config.bundle_policy =
      PeerConnectionInterface::BundlePolicy::kBundlePolicyMaxBundle;
  config.rtcp_handler = [this](const CopyOnWriteBuffer& packet,
                               int64_t packet_time_us) {
    RTC_DCHECK_RUN_ON(network_thread_);
    observer_->OnPacketReceived(packet);
  };
  return config;
}

void ScenarioIceConnectionImpl::SendRtpPacket(
    ArrayView<const uint8_t> packet_view) {
  CopyOnWriteBuffer packet(packet_view.data(), packet_view.size(),
                           kMaxRtpPacketLen);
  network_thread_->PostTask([this, packet = std::move(packet)]() mutable {
    RTC_DCHECK_RUN_ON(network_thread_);
    if (rtp_transport_ != nullptr)
      rtp_transport_->SendRtpPacket(&packet, AsyncSocketPacketOptions(),
                                    PF_SRTP_BYPASS);
  });
}

void ScenarioIceConnectionImpl::SendRtcpPacket(
    ArrayView<const uint8_t> packet_view) {
  CopyOnWriteBuffer packet(packet_view.data(), packet_view.size(),
                           kMaxRtpPacketLen);
  network_thread_->PostTask([this, packet = std::move(packet)]() mutable {
    RTC_DCHECK_RUN_ON(network_thread_);
    if (rtp_transport_ != nullptr)
      rtp_transport_->SendRtcpPacket(&packet, AsyncSocketPacketOptions(),
                                     PF_SRTP_BYPASS);
  });
}
void ScenarioIceConnectionImpl::SetRemoteSdp(SdpType type,
                                             const std::string& remote_sdp) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  remote_description_ = CreateSessionDescription(type, remote_sdp);
  jsep_controller_->SubscribeIceCandidateGathered(
      [this](const std::string& transport,
             const std::vector<Candidate>& candidate) {
        ScenarioIceConnectionImpl::OnCandidates(transport, candidate);
      });

  auto res = jsep_controller_->SetRemoteDescription(
      remote_description_->GetType(),
      local_description_ ? local_description_->description() : nullptr,
      remote_description_->description());
  RTC_CHECK(res.ok()) << res.message();
  RtpDemuxerCriteria criteria;
  for (const auto& content : remote_description_->description()->contents()) {
    for (const auto& codec : content.media_description()->codecs()) {
      criteria.payload_types().insert(codec.id);
    }
  }

  network_thread_->PostTask([this, criteria]() {
    RTC_DCHECK_RUN_ON(network_thread_);
    RTC_DCHECK(rtp_transport_);
    rtp_transport_->RegisterRtpDemuxerSink(criteria, this);
  });
}

void ScenarioIceConnectionImpl::SetLocalSdp(SdpType type,
                                            const std::string& local_sdp) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  local_description_ = CreateSessionDescription(type, local_sdp);
  auto res = jsep_controller_->SetLocalDescription(
      local_description_->GetType(), local_description_->description(),
      remote_description_ ? remote_description_->description() : nullptr);
  RTC_CHECK(res.ok()) << res.message();
  jsep_controller_->MaybeStartGathering();
}

bool ScenarioIceConnectionImpl::OnTransportChanged(
    const std::string& mid,
    RtpTransportInternal* rtp_transport,
    scoped_refptr<DtlsTransport> dtls_transport,
    DataChannelTransportInterface* data_channel_transport) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (rtp_transport == nullptr) {
    rtp_transport_->UnregisterRtpDemuxerSink(this);
  } else {
    RTC_DCHECK(rtp_transport_ == nullptr || rtp_transport_ == rtp_transport);
    if (rtp_transport_ != rtp_transport) {
      rtp_transport_ = rtp_transport;
    }
    RtpDemuxerCriteria criteria(mid);
    rtp_transport_->RegisterRtpDemuxerSink(criteria, this);
  }
  return true;
}

void ScenarioIceConnectionImpl::OnRtpPacket(const RtpPacketReceived& packet) {
  RTC_DCHECK_RUN_ON(network_thread_);
  observer_->OnPacketReceived(packet.Buffer());
}

void ScenarioIceConnectionImpl::OnCandidates(
    const std::string& mid,
    const std::vector<Candidate>& candidates) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  observer_->OnIceCandidates(mid, candidates);
}

}  // namespace webrtc
