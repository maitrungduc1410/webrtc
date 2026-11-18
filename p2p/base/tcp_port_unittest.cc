/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/tcp_port.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/candidate.h"
#include "api/environment/environment.h"
#include "api/packet_socket_factory.h"
#include "api/rtc_error.h"
#include "api/test/mock_packet_socket_factory.h"
#include "api/test/rtc_error_matchers.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "p2p/base/basic_packet_socket_factory.h"
#include "p2p/base/connection.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/base/port.h"
#include "p2p/base/transport_description.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/checks.h"
#include "rtc_base/crypto_random.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/network.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/thread.h"
#include "rtc_base/virtual_socket_server.h"
#include "test/create_test_environment.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"
#include "test/wait_until.h"

namespace webrtc {
namespace {

using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::IsTrue;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Return;

int kTimeout = 1000;
const SocketAddress kLocalAddr("11.11.11.11", 0);
const SocketAddress kLocalIPv6Addr("2401:fa00:4:1000:be30:5bff:fee5:c3", 0);
const SocketAddress kAlternateLocalAddr("1.2.3.4", 0);
const SocketAddress kRemoteAddr("22.22.22.22", 0);
const SocketAddress kRemoteIPv6Addr("2401:fa00:4:1000:be30:5bff:fee5:c4", 0);

constexpr uint64_t kTiebreakerDefault = 44444;

class ConnectionObserver {
 public:
  explicit ConnectionObserver(Connection* conn) : conn_(conn) {
    conn->SubscribeDestroyed(
        this, [this](Connection* connection) { OnDestroyed(connection); });
  }

  ~ConnectionObserver() {
    if (!connection_destroyed_) {
      RTC_DCHECK(conn_);
      conn_->UnsubscribeDestroyed(this);
    }
  }

  bool connection_destroyed() { return connection_destroyed_; }

 private:
  void OnDestroyed(Connection*) { connection_destroyed_ = true; }

  Connection* const conn_;
  bool connection_destroyed_ = false;
};

class TCPPortTest : public ::testing::Test {
 public:
  TCPPortTest()
      : ss_(new VirtualSocketServer()),
        time_controller_(Timestamp::Millis(1000), ss_.get()),
        main_(time_controller_.GetMainThread()),
        env_(CreateTestEnvironment({.time = &time_controller_})),
        socket_factory_(ss_.get()),
        username_(CreateRandomString(ICE_UFRAG_LENGTH)),
        password_(CreateRandomString(ICE_PWD_LENGTH)) {}

  Network* MakeNetwork(const SocketAddress& addr) {
    networks_.emplace_back("unittest", "unittest", addr.ipaddr(), 32);
    networks_.back().AddIP(addr.ipaddr());
    return &networks_.back();
  }

  std::unique_ptr<TCPPort> CreateTCPPort(const SocketAddress& addr,
                                         bool allow_listen = true,
                                         int port_number = 0) {
    return std::unique_ptr<TCPPort>(
        TCPPort::Create({.env = env_,
                         .network_thread = main_,
                         .socket_factory = &socket_factory_,
                         .network = MakeNetwork(addr),
                         .ice_username_fragment = username_,
                         .ice_password = password_,
                         .ice_tiebreaker = kTiebreakerDefault},
                        port_number, port_number, allow_listen));
  }

  std::unique_ptr<TCPPort> CreateTCPPort(const Network* network) {
    return std::unique_ptr<TCPPort>(
        TCPPort::Create({.env = env_,
                         .network_thread = main_,
                         .socket_factory = &socket_factory_,
                         .network = network,
                         .ice_username_fragment = username_,
                         .ice_password = password_,
                         .ice_tiebreaker = kTiebreakerDefault},
                        0, 0, true));
  }

  // Returns a TCPPort that accepts incoming connections.
  std::unique_ptr<TCPPort> CreateListeningPort() {
    std::unique_ptr<TCPPort> port = CreateTCPPort(kRemoteAddr);
    port->PrepareAddress();
    return port;
  }

  // Connects a plain TCP client socket to the candidate of `server`.
  std::unique_ptr<AsyncPacketSocket> ConnectClient(const TCPPort& server) {
    return socket_factory_.CreateClientTcpSocket(
        env_, SocketAddress(kLocalAddr.ipaddr(), 0),
        server.Candidates()[0].address(), PacketSocketTcpOptions());
  }

 protected:
  // When a "create port" helper method is called with an IP, we create a
  // Network with that IP and add it to this list. Using a list instead of a
  // vector so that when it grows, pointers aren't invalidated.
  std::list<Network> networks_;
  std::unique_ptr<VirtualSocketServer> ss_;
  GlobalSimulatedTimeController time_controller_;
  Thread* main_;
  const Environment env_;
  BasicPacketSocketFactory socket_factory_;
  std::string username_;
  std::string password_;
};

TEST_F(TCPPortTest, TestTCPPortWithLocalhostAddress) {
  SocketAddress local_address("127.0.0.1", 0);
  // After calling this, when TCPPort attempts to get a socket bound to
  // kLocalAddr, it will end up using localhost instead.
  ss_->SetAlternativeLocalAddress(kLocalAddr.ipaddr(), local_address.ipaddr());
  auto local_port = CreateTCPPort(kLocalAddr);
  auto remote_port = CreateTCPPort(kRemoteAddr);
  local_port->PrepareAddress();
  remote_port->PrepareAddress();
  Connection* conn = local_port->CreateConnection(remote_port->Candidates()[0],
                                                  Port::ORIGIN_MESSAGE);
  EXPECT_TRUE(WaitUntil(
      [&] { return conn->connected(); },
      {.timeout = TimeDelta::Millis(kTimeout), .clock = &time_controller_}));
  // Verify that the socket actually used localhost, otherwise this test isn't
  // doing what it meant to.
  ASSERT_EQ(local_address.ipaddr(),
            local_port->Candidates()[0].address().ipaddr());
}

// If the address the socket ends up bound to does not match any address of the
// TCPPort's Network, then the socket should be discarded and no candidates
// should be signaled. In the context of ICE, where one TCPPort is created for
// each Network, when this happens it's likely that the unexpected address is
// associated with some other Network, which another TCPPort is already
// covering.
TEST_F(TCPPortTest, TCPPortDiscardedIfBoundAddressDoesNotMatchNetwork) {
  // Sockets bound to kLocalAddr will actually end up with kAlternateLocalAddr.
  ss_->SetAlternativeLocalAddress(kLocalAddr.ipaddr(),
                                  kAlternateLocalAddr.ipaddr());

  // Create ports (local_port is the one whose IP will end up reassigned).
  auto local_port = CreateTCPPort(kLocalAddr);
  auto remote_port = CreateTCPPort(kRemoteAddr);
  local_port->PrepareAddress();
  remote_port->PrepareAddress();

  // Tell port to create a connection; it should be destroyed when it's
  // realized that it's using an unexpected address.
  Connection* conn = local_port->CreateConnection(remote_port->Candidates()[0],
                                                  Port::ORIGIN_MESSAGE);
  ConnectionObserver observer(conn);
  EXPECT_TRUE(WaitUntil(
      [&] { return observer.connection_destroyed(); },
      {.timeout = TimeDelta::Millis(kTimeout), .clock = &time_controller_}));
}

// A caveat for the above logic: if the socket ends up bound to one of the IPs
// associated with the Network, just not the "best" one, this is ok.
TEST_F(TCPPortTest, TCPPortNotDiscardedIfNotBoundToBestIP) {
  // Sockets bound to kLocalAddr will actually end up with kAlternateLocalAddr.
  ss_->SetAlternativeLocalAddress(kLocalAddr.ipaddr(),
                                  kAlternateLocalAddr.ipaddr());

  // Set up a network with kLocalAddr1 as the "best" IP, and kAlternateLocalAddr
  // as an alternate.
  Network* network = MakeNetwork(kLocalAddr);
  network->AddIP(kAlternateLocalAddr.ipaddr());
  ASSERT_EQ(kLocalAddr.ipaddr(), network->GetBestIP());

  // Create ports (using our special 2-IP Network for local_port).
  auto local_port = CreateTCPPort(network);
  auto remote_port = CreateTCPPort(kRemoteAddr);
  local_port->PrepareAddress();
  remote_port->PrepareAddress();

  // Expect connection to succeed.
  Connection* conn = local_port->CreateConnection(remote_port->Candidates()[0],
                                                  Port::ORIGIN_MESSAGE);
  EXPECT_TRUE(WaitUntil(
      [&] { return conn->connected(); },
      {.timeout = TimeDelta::Millis(kTimeout), .clock = &time_controller_}));

  // Verify that the socket actually used the alternate address, otherwise this
  // test isn't doing what it meant to.
  ASSERT_EQ(kAlternateLocalAddr.ipaddr(),
            local_port->Candidates()[0].address().ipaddr());
}

// Regression test for crbug.com/webrtc/8972, caused by buggy comparison
// between IPAddress and InterfaceAddress.
TEST_F(TCPPortTest, TCPPortNotDiscardedIfBoundToTemporaryIP) {
  networks_.emplace_back("unittest", "unittest", kLocalIPv6Addr.ipaddr(), 32);
  networks_.back().AddIP(
      InterfaceAddress(kLocalIPv6Addr.ipaddr(), IPV6_ADDRESS_FLAG_TEMPORARY));

  auto local_port = CreateTCPPort(&networks_.back());
  auto remote_port = CreateTCPPort(kRemoteIPv6Addr);
  local_port->PrepareAddress();
  remote_port->PrepareAddress();

  // Connection should succeed if the port isn't discarded.
  Connection* conn = local_port->CreateConnection(remote_port->Candidates()[0],
                                                  Port::ORIGIN_MESSAGE);
  ASSERT_NE(nullptr, conn);
  EXPECT_TRUE(WaitUntil(
      [&] { return conn->connected(); },
      {.timeout = TimeDelta::Millis(kTimeout), .clock = &time_controller_}));
}

class SentPacketCounter {
 public:
  explicit SentPacketCounter(TCPPort* p) {
    p->SubscribeSentPacket(
        this, [this](const SentPacketInfo& info) { OnSentPacket(info); });
  }

  int sent_packets() const { return sent_packets_; }

 private:
  void OnSentPacket(const SentPacketInfo&) { ++sent_packets_; }

  int sent_packets_ = 0;
};

// Test that SignalSentPacket is fired when a packet is successfully sent, for
// both TCP client and server sockets.
TEST_F(TCPPortTest, SignalSentPacket) {
  std::unique_ptr<TCPPort> client(CreateTCPPort(kLocalAddr));
  std::unique_ptr<TCPPort> server(CreateTCPPort(kRemoteAddr));
  client->SetIceRole(ICEROLE_CONTROLLING);
  server->SetIceRole(ICEROLE_CONTROLLED);
  client->PrepareAddress();
  server->PrepareAddress();

  Connection* client_conn =
      client->CreateConnection(server->Candidates()[0], Port::ORIGIN_MESSAGE);
  ASSERT_NE(nullptr, client_conn);
  ASSERT_TRUE(WaitUntil(
      [&] { return client_conn->connected(); },
      {.timeout = TimeDelta::Millis(kTimeout), .clock = &time_controller_}));

  // Need to get the port of the actual outgoing socket, not the server socket..
  Candidate client_candidate = client->Candidates()[0];
  client_candidate.set_address(
      static_cast<TCPConnection*>(client_conn)->socket()->GetLocalAddress());
  Connection* server_conn =
      server->CreateConnection(client_candidate, Port::ORIGIN_THIS_PORT);
  ASSERT_NE(nullptr, server_conn);
  ASSERT_TRUE(WaitUntil(
      [&] { return server_conn->connected(); },
      {.timeout = TimeDelta::Millis(kTimeout), .clock = &time_controller_}));

  client_conn->Ping(env_.clock().CurrentTime());
  server_conn->Ping(env_.clock().CurrentTime());
  ASSERT_TRUE(WaitUntil(
      [&] { return client_conn->writable(); },
      {.timeout = TimeDelta::Millis(kTimeout), .clock = &time_controller_}));
  ASSERT_TRUE(WaitUntil(
      [&] { return server_conn->writable(); },
      {.timeout = TimeDelta::Millis(kTimeout), .clock = &time_controller_}));

  SentPacketCounter client_counter(client.get());
  SentPacketCounter server_counter(server.get());
  static constexpr uint8_t kData[] = {'h', 'e', 'l', 'l', 'o', '\0'};
  for (int i = 0; i < 10; ++i) {
    client_conn->Send(kData, AsyncSocketPacketOptions());
    server_conn->Send(kData, AsyncSocketPacketOptions());
  }
  EXPECT_THAT(WaitUntil([&] { return client_counter.sent_packets(); }, Eq(10),
                        {.timeout = TimeDelta::Millis(kTimeout),
                         .clock = &time_controller_}),
              IsRtcOk());
  EXPECT_THAT(WaitUntil([&] { return server_counter.sent_packets(); }, Eq(10),
                        {.timeout = TimeDelta::Millis(kTimeout),
                         .clock = &time_controller_}),
              IsRtcOk());
}

// Test that SignalSentPacket is fired when a packet is successfully sent, even
// after a remote server has been restarted.
TEST_F(TCPPortTest, SignalSentPacketAfterReconnect) {
  std::unique_ptr<TCPPort> client(
      CreateTCPPort(kLocalAddr, /*allow_listen=*/false));
  constexpr int kServerPort = 123;
  std::unique_ptr<TCPPort> server(
      CreateTCPPort(kRemoteAddr, /*allow_listen=*/true, kServerPort));
  client->SetIceRole(ICEROLE_CONTROLLING);
  server->SetIceRole(ICEROLE_CONTROLLED);
  client->PrepareAddress();
  server->PrepareAddress();

  Connection* client_conn =
      client->CreateConnection(server->Candidates()[0], Port::ORIGIN_MESSAGE);
  ASSERT_NE(nullptr, client_conn);
  ASSERT_TRUE(WaitUntil(
      [&] { return client_conn->connected(); },
      {.timeout = TimeDelta::Millis(kTimeout), .clock = &time_controller_}));

  // Need to get the port of the actual outgoing socket.
  Candidate client_candidate = client->Candidates()[0];
  client_candidate.set_address(
      static_cast<TCPConnection*>(client_conn)->socket()->GetLocalAddress());
  client_candidate.set_tcptype("");
  Connection* server_conn =
      server->CreateConnection(client_candidate, Port::ORIGIN_THIS_PORT);
  ASSERT_TRUE(WaitUntil(
      [&] { return server_conn->connected(); },
      {.timeout = TimeDelta::Millis(kTimeout), .clock = &time_controller_}));
  EXPECT_FALSE(client_conn->writable());
  client_conn->Ping(env_.clock().CurrentTime());
  ASSERT_TRUE(WaitUntil(
      [&] { return client_conn->writable(); },
      {.timeout = TimeDelta::Millis(kTimeout), .clock = &time_controller_}));

  SentPacketCounter client_counter(client.get());
  static constexpr uint8_t kData[] = {'h', 'e', 'l', 'l', 'o', '\0'};
  int result = client_conn->Send(kData, AsyncSocketPacketOptions());
  EXPECT_EQ(result, 6);

  // Deleting the server port should break the current connection.
  server = nullptr;
  server_conn = nullptr;
  ASSERT_TRUE(WaitUntil(
      [&] { return !client_conn->connected(); },
      {.timeout = TimeDelta::Millis(kTimeout), .clock = &time_controller_}));

  // Recreate the server port with the same port number.
  server = CreateTCPPort(kRemoteAddr, /*allow_listen=*/true, kServerPort);
  server->SetIceRole(ICEROLE_CONTROLLED);
  server->PrepareAddress();

  // Sending a packet from the client will trigger a reconnect attempt but the
  // packet will be discarded.
  result = client_conn->Send(kData, AsyncSocketPacketOptions());
  EXPECT_EQ(result, SOCKET_ERROR);
  ASSERT_TRUE(WaitUntil(
      [&] { return client_conn->connected(); },
      {.timeout = TimeDelta::Millis(kTimeout), .clock = &time_controller_}));
  // For unknown reasons, connection is still supposed to be writable....
  EXPECT_TRUE(client_conn->writable());
  for (int i = 0; i < 10; ++i) {
    // All sent packets still fail to send.
    EXPECT_EQ(client_conn->Send(kData, AsyncSocketPacketOptions()),
              SOCKET_ERROR);
  }
  // And are not reported as sent.
  EXPECT_THAT(WaitUntil([&] { return client_counter.sent_packets(); }, Eq(1),
                        {.timeout = TimeDelta::Millis(kTimeout),
                         .clock = &time_controller_}),
              IsRtcOk());

  // Create the server connection again so server can reply to STUN pings.
  // Client outgoing socket port will have changed since the client create a new
  // socket when it reconnect.
  client_candidate = client->Candidates()[0];
  client_candidate.set_address(
      static_cast<TCPConnection*>(client_conn)->socket()->GetLocalAddress());
  client_candidate.set_tcptype("");
  server_conn =
      server->CreateConnection(client_candidate, Port::ORIGIN_THIS_PORT);
  ASSERT_TRUE(WaitUntil(
      [&] { return server_conn->connected(); },
      {.timeout = TimeDelta::Millis(kTimeout), .clock = &time_controller_}));
  EXPECT_THAT(WaitUntil([&] { return client_counter.sent_packets(); }, Eq(1),
                        {.timeout = TimeDelta::Millis(kTimeout),
                         .clock = &time_controller_}),
              IsRtcOk());

  // Send Stun Binding request.
  client_conn->Ping(env_.clock().CurrentTime());
  // The Stun Binding request is reported as sent.
  EXPECT_THAT(WaitUntil([&] { return client_counter.sent_packets(); }, Eq(2),
                        {.timeout = TimeDelta::Millis(kTimeout),
                         .clock = &time_controller_}),
              IsRtcOk());
  // Wait a bit for the Stun response to be received.
  time_controller_.AdvanceTime(TimeDelta::Millis(100));

  // After the Stun Ping response has been received, packets can be sent again
  // and SignalSentPacket should be invoked.
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(client_conn->Send(kData, AsyncSocketPacketOptions()), 6);
  }
  EXPECT_THAT(
      WaitUntil(
          [&] { return client_counter.sent_packets(); }, Eq(2 + 5),
          {.timeout = TimeDelta::Millis(kTimeout), .clock = &time_controller_}),
      IsRtcOk());
}

TEST_F(TCPPortTest, ReconnectTimeoutAfterShutdownDoesNotDestroy) {
  std::unique_ptr<TCPPort> client =
      CreateTCPPort(kLocalAddr, /*allow_listen=*/false);
  std::unique_ptr<TCPPort> server = CreateTCPPort(kRemoteAddr);
  client->SetIceRole(ICEROLE_CONTROLLING);
  server->SetIceRole(ICEROLE_CONTROLLED);
  client->PrepareAddress();
  server->PrepareAddress();

  TCPConnection* client_conn = static_cast<TCPConnection*>(
      client->CreateConnection(server->Candidates()[0], Port::ORIGIN_MESSAGE));
  ASSERT_THAT(client_conn, NotNull());
  ASSERT_TRUE(WaitUntil(
      [&] { return client_conn->connected(); },
      {.timeout = TimeDelta::Millis(kTimeout), .clock = &time_controller_}));

  ConnectionObserver observer(client_conn);

  client_conn->socket()->NotifyClosedForTest(0);
  ASSERT_FALSE(client_conn->connected());

  // Runs the reconnect timer between Shutdown() and the deferred delete.
  main_->PostTask(
      [&client, client_conn] { client->DestroyConnectionAsync(client_conn); });
  time_controller_.SkipForwardBy(kConnectionWriteConnectTimeout);
  time_controller_.AdvanceTime(TimeDelta::Zero());

  EXPECT_TRUE(observer.connection_destroyed());
}

TEST_F(TCPPortTest, ActiveCandidateAcceptedOnlyFromThisPort) {
  webrtc::RTCErrorOr<webrtc::Candidate> parsed =
      webrtc::Candidate::ParseCandidateString(
          "candidate:1 1 tcp 2122252543 192.168.1.1 5000 typ prflx "
          "raddr 0.0.0.0 rport 0 tcptype active generation 0");
  ASSERT_TRUE(parsed.ok());
  webrtc::Candidate candidate = parsed.MoveValue();
  // TCPPort::CreateConnection only accepts active candidates learned from
  // incoming traffic on this port (ORIGIN_THIS_PORT), even if typed as prflx.
  std::unique_ptr<TCPPort> port(CreateTCPPort(kLocalAddr));
  port->SetIceRole(webrtc::ICEROLE_CONTROLLING);
  port->PrepareAddress();
  EXPECT_EQ(nullptr, port->CreateConnection(candidate, Port::ORIGIN_MESSAGE));
  EXPECT_NE(nullptr, port->CreateConnection(candidate, Port::ORIGIN_THIS_PORT));
  EXPECT_EQ(nullptr,
            port->CreateConnection(candidate, Port::ORIGIN_OTHER_PORT));
}

TEST_F(TCPPortTest, VerifyCandidateChecksTcpActiveAndZeroPorts) {
  auto parse = [](absl::string_view str) {
    RTCErrorOr<Candidate> parsed = Candidate::ParseCandidateString(str);
    RTC_CHECK(parsed.ok());
    return parsed.MoveValue();
  };
  // Active TCP candidates with DISCARD_PORT (9) or 0 are allowed.
  EXPECT_THAT(VerifyCandidate(
                  parse("candidate:1 1 tcp 2122252543 192.168.1.1 9 typ host "
                        "tcptype active generation 0")),
              IsRtcOk());
  EXPECT_THAT(VerifyCandidate(
                  parse("candidate:1 1 tcp 2122252543 192.168.1.1 0 typ host "
                        "tcptype active generation 0")),
              IsRtcOk());
  // Active TCP candidates on other privileged ports (<1024, other than 0 or 9)
  // are rejected.
  EXPECT_THAT(VerifyCandidate(
                  parse("candidate:1 1 tcp 2122252543 192.168.1.1 22 typ prflx "
                        "raddr 0.0.0.0 rport 0 tcptype active generation 0")),
              IsRtcErrorWithType(RTCErrorType::INVALID_PARAMETER));
  // Active TCP candidates on private 80/443 are rejected.
  EXPECT_THAT(VerifyCandidate(
                  parse("candidate:1 1 tcp 2122252543 192.168.1.1 80 typ host "
                        "tcptype active generation 0")),
              IsRtcErrorWithType(RTCErrorType::INVALID_PARAMETER));
  // Passive TCP candidates on port 0 are rejected.
  EXPECT_THAT(VerifyCandidate(
                  parse("candidate:1 1 tcp 2122252543 192.168.1.1 0 typ host "
                        "tcptype passive generation 0")),
              IsRtcErrorWithType(RTCErrorType::INVALID_PARAMETER));
  // TCP candidates on 80/443 with a private IP are rejected, regardless of
  // tcptype.
  EXPECT_THAT(VerifyCandidate(
                  parse("candidate:1 1 tcp 2122252543 192.168.1.1 80 typ host "
                        "tcptype passive generation 0")),
              IsRtcErrorWithType(RTCErrorType::INVALID_PARAMETER));
  EXPECT_THAT(VerifyCandidate(
                  parse("candidate:1 1 tcp 2122252543 192.168.1.1 443 typ host "
                        "tcptype passive generation 0")),
              IsRtcErrorWithType(RTCErrorType::INVALID_PARAMETER));
  // TCP candidates on 80/443 with a public IP are allowed, regardless of
  // tcptype.
  EXPECT_THAT(
      VerifyCandidate(parse("candidate:1 1 tcp 2122252543 1.2.3.4 80 typ host "
                            "tcptype passive generation 0")),
      IsRtcOk());
  EXPECT_THAT(
      VerifyCandidate(parse("candidate:1 1 tcp 2122252543 1.2.3.4 443 typ host "
                            "tcptype active generation 0")),
      IsRtcOk());
}

TEST_F(TCPPortTest, UnclaimedIncomingSocketsDeletedOnPeerClose) {
  std::unique_ptr<TCPPort> server = CreateListeningPort();
  ASSERT_THAT(server->Candidates(), Not(IsEmpty()));

  constexpr size_t kNumClients = TCPPort::kMaxPendingIncomingSockets;
  std::vector<std::unique_ptr<AsyncPacketSocket>> clients;
  for (size_t i = 0; i < kNumClients; ++i) {
    clients.push_back(ConnectClient(*server));
    ASSERT_THAT(clients.back(), NotNull());
  }

  ASSERT_THAT(
      WaitUntil(
          [&] { return server->PendingIncomingSocketsForTesting(); },
          Eq(kNumClients),
          {.timeout = TimeDelta::Millis(kTimeout), .clock = &time_controller_}),
      IsRtcOk());

  for (auto& client : clients) {
    client->Close();
  }
  EXPECT_THAT(
      WaitUntil(
          [&] { return server->PendingIncomingSocketsForTesting(); }, Eq(0u),
          {.timeout = TimeDelta::Millis(kTimeout), .clock = &time_controller_}),
      IsRtcOk());
}

TEST_F(TCPPortTest, PendingIncomingSocketsCappedAndEvictsOldest) {
  std::unique_ptr<TCPPort> server = CreateListeningPort();
  ASSERT_THAT(server->Candidates(), Not(IsEmpty()));

  constexpr size_t kExtraSockets = 8;
  constexpr size_t kTotalSockets =
      TCPPort::kMaxPendingIncomingSockets + kExtraSockets;
  std::vector<std::unique_ptr<AsyncPacketSocket>> attackers;
  for (size_t i = 0; i < kTotalSockets; ++i) {
    attackers.push_back(ConnectClient(*server));
    ASSERT_THAT(attackers.back(), NotNull());
    // Connect one at a time so that the server accepts them in order.
    ASSERT_THAT(WaitUntil(
                    [&] {
                      return attackers.back()->GetState() ==
                             AsyncPacketSocket::STATE_CONNECTED;
                    },
                    IsTrue(),
                    {.timeout = TimeDelta::Millis(kTimeout),
                     .clock = &time_controller_}),
                IsRtcOk());
  }
  EXPECT_EQ(server->PendingIncomingSocketsForTesting(),
            TCPPort::kMaxPendingIncomingSockets);

  // The oldest sockets are closed and the newest ones are still connected.
  auto all_in_state =
      [](std::span<const std::unique_ptr<AsyncPacketSocket>> sockets,
         AsyncPacketSocket::State state) {
        return std::all_of(sockets.begin(), sockets.end(),
                           [state](const auto& socket) {
                             return socket->GetState() == state;
                           });
      };
  std::span<const std::unique_ptr<AsyncPacketSocket>> sockets(attackers);
  EXPECT_THAT(
      WaitUntil(
          [&] {
            return all_in_state(sockets.first(kExtraSockets),
                                AsyncPacketSocket::STATE_CLOSED);
          },
          IsTrue(),
          {.timeout = TimeDelta::Millis(kTimeout), .clock = &time_controller_}),
      IsRtcOk());
  EXPECT_TRUE(all_in_state(sockets.subspan(kExtraSockets),
                           AsyncPacketSocket::STATE_CONNECTED));
}

class FakeListenSocket : public AsyncListenSocket {
 public:
  explicit FakeListenSocket(const SocketAddress& local_address)
      : local_address_(local_address) {}
  State GetState() const override { return State::kBound; }
  SocketAddress GetLocalAddress() const override { return local_address_; }

 private:
  const SocketAddress local_address_;
};

class DestructionTrackingPacketSocket : public AsyncPacketSocket {
 public:
  DestructionTrackingPacketSocket(const SocketAddress& remote_address,
                                  bool* destroyed)
      : remote_address_(remote_address), destroyed_(destroyed) {}
  ~DestructionTrackingPacketSocket() override { *destroyed_ = true; }

  SocketAddress GetLocalAddress() const override { return SocketAddress(); }
  SocketAddress GetRemoteAddress() const override { return remote_address_; }
  int Send(const void*, size_t, const AsyncSocketPacketOptions&) override {
    return 0;
  }
  int SendTo(const void*,
             size_t,
             const SocketAddress&,
             const AsyncSocketPacketOptions&) override {
    return 0;
  }
  int Close() override { return 0; }
  State GetState() const override { return STATE_CONNECTED; }
  int GetOption(Socket::Option, int*) override { return 0; }
  int SetOption(Socket::Option, int) override { return 0; }
  int GetError() const override { return 0; }
  void SetError(int) override {}

 private:
  const SocketAddress remote_address_;
  bool* const destroyed_;
};

TEST_F(TCPPortTest, ClosedIncomingSocketsDeletedOnTaskOrPortDestruction) {
  MockPacketSocketFactory factory;
  auto listen_socket_owner = std::make_unique<FakeListenSocket>(
      SocketAddress(kRemoteAddr.ipaddr(), 1));
  FakeListenSocket* listen_socket = listen_socket_owner.get();
  EXPECT_CALL(factory, CreateServerTcpSocket)
      .WillOnce(Return(std::move(listen_socket_owner)));

  std::unique_ptr<TCPPort> server =
      TCPPort::Create({.env = env_,
                       .network_thread = main_,
                       .socket_factory = &factory,
                       .network = MakeNetwork(kRemoteAddr),
                       .ice_username_fragment = username_,
                       .ice_password = password_,
                       .ice_tiebreaker = kTiebreakerDefault},
                      0, 0, /*allow_listen=*/true);

  bool socket1_destroyed = false;
  auto* socket1 = new DestructionTrackingPacketSocket(
      SocketAddress(kLocalAddr.ipaddr(), 1001), &socket1_destroyed);
  bool socket2_destroyed = false;
  auto* socket2 = new DestructionTrackingPacketSocket(
      SocketAddress(kLocalAddr.ipaddr(), 1002), &socket2_destroyed);
  listen_socket->NotifyNewConnection(listen_socket, socket1);
  listen_socket->NotifyNewConnection(listen_socket, socket2);
  ASSERT_EQ(server->PendingIncomingSocketsForTesting(), 2u);

  // Closing socket1 keeps it in `incoming_` until the queued task runs.
  socket1->NotifyClosedForTest(0);
  EXPECT_FALSE(socket1_destroyed);
  time_controller_.AdvanceTime(TimeDelta::Zero());
  EXPECT_TRUE(socket1_destroyed);
  EXPECT_EQ(server->PendingIncomingSocketsForTesting(), 1u);

  // Destroying TCPPort before the queued task runs deletes socket2 immediately.
  socket2->NotifyClosedForTest(0);
  EXPECT_FALSE(socket2_destroyed);
  server.reset();
  EXPECT_TRUE(socket2_destroyed);
  time_controller_.AdvanceTime(TimeDelta::Zero());
}

}  // namespace
}  // namespace webrtc
