/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/ssl_adapter.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "api/test/rtc_error_matchers.h"
#include "api/units/time_delta.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/logging.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/ssl_identity.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/thread.h"
#include "rtc_base/virtual_socket_server.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/wait_until.h"

using ::testing::_;
using ::testing::Return;

static const webrtc::TimeDelta kTimeout = webrtc::TimeDelta::Millis(5000);

static webrtc::Socket* CreateSocket() {
  webrtc::SocketAddress address(webrtc::IPAddress(INADDR_ANY), 0);

  webrtc::Socket* socket =
      webrtc::Thread::Current()->socketserver()->CreateSocket(address.family(),
                                                              SOCK_STREAM);
  socket->Bind(address);

  return socket;
}

// Simple mock for the certificate verifier.
class MockCertVerifier : public webrtc::SSLCertificateVerifier {
 public:
  ~MockCertVerifier() override = default;
  MOCK_METHOD(bool, Verify, (const webrtc::SSLCertificate&), (override));
};

// TODO(benwright) - Move to using INSTANTIATE_TEST_SUITE_P instead of using
// duplicate test cases for simple parameter changes.
class SSLAdapterTestDummy : public sigslot::has_slots<> {
 public:
  explicit SSLAdapterTestDummy() : socket_(CreateSocket()) {}
  ~SSLAdapterTestDummy() override = default;

  void CreateSSLAdapter(webrtc::Socket* socket, webrtc::SSLRole role) {
    ssl_adapter_.reset(webrtc::SSLAdapter::Create(socket));

    // Ignore any certificate errors for the purpose of testing.
    // Note: We do this only because we don't have a real certificate.
    // NEVER USE THIS IN PRODUCTION CODE!
    ssl_adapter_->SetIgnoreBadCert(true);

    ssl_adapter_->SignalReadEvent.connect(
        this, &SSLAdapterTestDummy::OnSSLAdapterReadEvent);
    ssl_adapter_->SignalCloseEvent.connect(
        this, &SSLAdapterTestDummy::OnSSLAdapterCloseEvent);
    ssl_adapter_->SetRole(role);
  }

  void SetIgnoreBadCert(bool ignore_bad_cert) {
    ssl_adapter_->SetIgnoreBadCert(ignore_bad_cert);
  }

  void SetCertVerifier(webrtc::SSLCertificateVerifier* ssl_cert_verifier) {
    ssl_adapter_->SetCertVerifier(ssl_cert_verifier);
  }

  void SetAlpnProtocols(const std::vector<std::string>& protos) {
    ssl_adapter_->SetAlpnProtocols(protos);
  }

  void SetEllipticCurves(const std::vector<std::string>& curves) {
    ssl_adapter_->SetEllipticCurves(curves);
  }

  webrtc::SocketAddress GetAddress() const {
    return ssl_adapter_->GetLocalAddress();
  }

  webrtc::Socket::ConnState GetState() const {
    return ssl_adapter_->GetState();
  }

  const std::string& GetReceivedData() const { return data_; }

  int Close() { return ssl_adapter_->Close(); }

  int Send(absl::string_view message) {
    RTC_LOG(LS_INFO) << "Sending '" << message << "'";

    return ssl_adapter_->Send(message.data(), message.length());
  }

  void OnSSLAdapterReadEvent(webrtc::Socket* socket) {
    char buffer[4096] = "";

    // Read data received from the server and store it in our internal buffer.
    int read = socket->Recv(buffer, sizeof(buffer) - 1, nullptr);
    if (read != -1) {
      buffer[read] = '\0';

      RTC_LOG(LS_INFO) << "Received '" << buffer << "'";

      data_ += buffer;
    }
  }

  void OnSSLAdapterCloseEvent(webrtc::Socket* socket, int error) {
    // OpenSSLAdapter signals handshake failure with a close event, but without
    // closing the socket! Let's close the socket here. This way GetState() can
    // return CS_CLOSED after failure.
    if (socket->GetState() != webrtc::Socket::CS_CLOSED) {
      socket->Close();
    }
  }

 protected:
  std::unique_ptr<webrtc::SSLAdapter> ssl_adapter_;
  std::unique_ptr<webrtc::Socket> socket_;

 private:
  std::string data_;
};

class SSLAdapterTestDummyClient : public SSLAdapterTestDummy {
 public:
  explicit SSLAdapterTestDummyClient() : SSLAdapterTestDummy() {
    CreateSSLAdapter(socket_.release(), webrtc::SSL_CLIENT);
  }

  int Connect(absl::string_view hostname,
              const webrtc::SocketAddress& address) {
    RTC_LOG(LS_INFO) << "Initiating connection with " << address.ToString();
    int rv = ssl_adapter_->Connect(address);

    if (rv == 0) {
      RTC_LOG(LS_INFO) << "Starting TLS handshake with " << hostname;

      if (ssl_adapter_->StartSSL(hostname) != 0) {
        return -1;
      }
    }

    return rv;
  }
};

class SSLAdapterTestDummyServer : public SSLAdapterTestDummy {
 public:
  explicit SSLAdapterTestDummyServer(const webrtc::KeyParams& key_params)
      : SSLAdapterTestDummy(),
        ssl_identity_(webrtc::SSLIdentity::Create(GetHostname(), key_params)) {
    socket_->Listen(1);
    socket_->SignalReadEvent.connect(this,
                                     &SSLAdapterTestDummyServer::OnReadEvent);

    RTC_LOG(LS_INFO) << "TCP server listening on "
                     << socket_->GetLocalAddress().ToString();
  }

  webrtc::SocketAddress GetAddress() const {
    return socket_->GetLocalAddress();
  }

  std::string GetHostname() const {
    // Since we don't have a real certificate anyway, the value here doesn't
    // really matter.
    return "example.com";
  }

 protected:
  void OnReadEvent(webrtc::Socket* socket) {
    CreateSSLAdapter(socket_->Accept(nullptr), webrtc::SSL_SERVER);
    ssl_adapter_->SetIdentity(ssl_identity_->Clone());
    if (ssl_adapter_->StartSSL(GetHostname()) != 0) {
      RTC_LOG(LS_ERROR) << "Starting SSL from server failed.";
    }
  }

 private:
  std::unique_ptr<webrtc::SSLIdentity> ssl_identity_;
};

class SSLAdapterTestBase : public ::testing::Test, public sigslot::has_slots<> {
 public:
  explicit SSLAdapterTestBase(const webrtc::KeyParams& key_params)
      : vss_(new webrtc::VirtualSocketServer()),
        thread_(vss_.get()),
        server_(new SSLAdapterTestDummyServer(key_params)),
        client_(new SSLAdapterTestDummyClient()),
        handshake_wait_(webrtc::TimeDelta::Millis(kTimeout.ms())) {}

  void SetHandshakeWait(int wait) {
    handshake_wait_ = webrtc::TimeDelta::Millis(wait);
  }

  void SetIgnoreBadCert(bool ignore_bad_cert) {
    client_->SetIgnoreBadCert(ignore_bad_cert);
  }

  void SetCertVerifier(webrtc::SSLCertificateVerifier* ssl_cert_verifier) {
    client_->SetCertVerifier(ssl_cert_verifier);
  }

  void SetAlpnProtocols(const std::vector<std::string>& protos) {
    client_->SetAlpnProtocols(protos);
  }

  void SetEllipticCurves(const std::vector<std::string>& curves) {
    client_->SetEllipticCurves(curves);
  }

  void SetMockCertVerifier(bool return_value) {
    auto mock_verifier = std::make_unique<MockCertVerifier>();
    EXPECT_CALL(*mock_verifier, Verify(_)).WillRepeatedly(Return(return_value));
    cert_verifier_ = std::unique_ptr<webrtc::SSLCertificateVerifier>(
        std::move(mock_verifier));

    SetIgnoreBadCert(false);
    SetCertVerifier(cert_verifier_.get());
  }

  void TestHandshake(bool expect_success) {
    int rv;

    // The initial state is CS_CLOSED
    ASSERT_EQ(webrtc::Socket::CS_CLOSED, client_->GetState());

    rv = client_->Connect(server_->GetHostname(), server_->GetAddress());
    ASSERT_EQ(0, rv);

    // Now the state should be CS_CONNECTING
    ASSERT_EQ(webrtc::Socket::CS_CONNECTING, client_->GetState());

    if (expect_success) {
      // If expecting success, the client should end up in the CS_CONNECTED
      // state after handshake.
      EXPECT_THAT(webrtc::WaitUntil([&] { return client_->GetState(); },
                                    ::testing::Eq(webrtc::Socket::CS_CONNECTED),
                                    {.timeout = handshake_wait_}),
                  webrtc::IsRtcOk());

      RTC_LOG(LS_INFO) << "TLS handshake complete.";

    } else {
      // On handshake failure the client should end up in the CS_CLOSED state.
      EXPECT_THAT(webrtc::WaitUntil([&] { return client_->GetState(); },
                                    ::testing::Eq(webrtc::Socket::CS_CLOSED),
                                    {.timeout = handshake_wait_}),
                  webrtc::IsRtcOk());

      RTC_LOG(LS_INFO) << "TLS handshake failed.";
    }
  }

  void TestTransfer(absl::string_view message) {
    int rv;

    rv = client_->Send(message);
    ASSERT_EQ(static_cast<int>(message.length()), rv);

    // The server should have received the client's message.
    EXPECT_THAT(
        webrtc::WaitUntil([&] { return server_->GetReceivedData(); },
                          ::testing::Eq(message), {.timeout = kTimeout}),
        webrtc::IsRtcOk());

    rv = server_->Send(message);
    ASSERT_EQ(static_cast<int>(message.length()), rv);

    // The client should have received the server's message.
    EXPECT_THAT(
        webrtc::WaitUntil([&] { return client_->GetReceivedData(); },
                          ::testing::Eq(message), {.timeout = kTimeout}),
        webrtc::IsRtcOk());

    RTC_LOG(LS_INFO) << "Transfer complete.";
  }

 protected:
  std::unique_ptr<webrtc::VirtualSocketServer> vss_;
  webrtc::AutoSocketServerThread thread_;
  std::unique_ptr<SSLAdapterTestDummyServer> server_;
  std::unique_ptr<SSLAdapterTestDummyClient> client_;
  std::unique_ptr<webrtc::SSLCertificateVerifier> cert_verifier_;

  webrtc::TimeDelta handshake_wait_;
};

class SSLAdapterTestTLS_RSA : public SSLAdapterTestBase {
 public:
  SSLAdapterTestTLS_RSA() : SSLAdapterTestBase(webrtc::KeyParams::RSA()) {}
};

class SSLAdapterTestTLS_ECDSA : public SSLAdapterTestBase {
 public:
  SSLAdapterTestTLS_ECDSA() : SSLAdapterTestBase(webrtc::KeyParams::ECDSA()) {}
};

// Test that handshake works, using RSA
TEST_F(SSLAdapterTestTLS_RSA, TestTLSConnect) {
  TestHandshake(true);
}

// Test that handshake works with a custom verifier that returns true. RSA.
TEST_F(SSLAdapterTestTLS_RSA, TestTLSConnectCustomCertVerifierSucceeds) {
  SetMockCertVerifier(/*return_value=*/true);
  TestHandshake(/*expect_success=*/true);
}

// Test that handshake fails with a custom verifier that returns false. RSA.
TEST_F(SSLAdapterTestTLS_RSA, TestTLSConnectCustomCertVerifierFails) {
  SetMockCertVerifier(/*return_value=*/false);
  TestHandshake(/*expect_success=*/false);
}

// Test that handshake works, using ECDSA
TEST_F(SSLAdapterTestTLS_ECDSA, TestTLSConnect) {
  SetMockCertVerifier(/*return_value=*/true);
  TestHandshake(/*expect_success=*/true);
}

// Test that handshake works with a custom verifier that returns true. ECDSA.
TEST_F(SSLAdapterTestTLS_ECDSA, TestTLSConnectCustomCertVerifierSucceeds) {
  SetMockCertVerifier(/*return_value=*/true);
  TestHandshake(/*expect_success=*/true);
}

// Test that handshake fails with a custom verifier that returns false. ECDSA.
TEST_F(SSLAdapterTestTLS_ECDSA, TestTLSConnectCustomCertVerifierFails) {
  SetMockCertVerifier(/*return_value=*/false);
  TestHandshake(/*expect_success=*/false);
}

// Test transfer between client and server, using RSA
TEST_F(SSLAdapterTestTLS_RSA, TestTLSTransfer) {
  TestHandshake(true);
  TestTransfer("Hello, world!");
}

// Test transfer between client and server, using RSA with custom cert verifier.
TEST_F(SSLAdapterTestTLS_RSA, TestTLSTransferCustomCertVerifier) {
  SetMockCertVerifier(/*return_value=*/true);
  TestHandshake(/*expect_success=*/true);
  TestTransfer("Hello, world!");
}

TEST_F(SSLAdapterTestTLS_RSA, TestTLSTransferWithBlockedSocket) {
  TestHandshake(true);

  // Tell the underlying socket to simulate being blocked.
  vss_->SetSendingBlocked(true);

  std::string expected;
  int rv;
  // Send messages until the SSL socket adapter starts applying backpressure.
  // Note that this may not occur immediately since there may be some amount of
  // intermediate buffering (either in our code or in BoringSSL).
  for (int i = 0; i < 1024; ++i) {
    std::string message = "Hello, world: " + absl::StrCat(i);
    rv = client_->Send(message);
    if (rv != static_cast<int>(message.size())) {
      // This test assumes either the whole message or none of it is sent.
      ASSERT_EQ(-1, rv);
      break;
    }
    expected += message;
  }
  // Assert that the loop above exited due to Send returning -1.
  ASSERT_EQ(-1, rv);

  // Try sending another message while blocked. -1 should be returned again and
  // it shouldn't end up received by the server later.
  EXPECT_EQ(-1, client_->Send("Never sent"));

  // Unblock the underlying socket. All of the buffered messages should be sent
  // without any further action.
  vss_->SetSendingBlocked(false);
  EXPECT_THAT(webrtc::WaitUntil([&] { return server_->GetReceivedData(); },
                                ::testing::Eq(expected), {.timeout = kTimeout}),
              webrtc::IsRtcOk());

  // Send another message. This previously wasn't working
  std::string final_message = "Fin.";
  expected += final_message;
  EXPECT_EQ(static_cast<int>(final_message.size()),
            client_->Send(final_message));
  EXPECT_THAT(webrtc::WaitUntil([&] { return server_->GetReceivedData(); },
                                ::testing::Eq(expected), {.timeout = kTimeout}),
              webrtc::IsRtcOk());
}

// Test transfer between client and server, using ECDSA
TEST_F(SSLAdapterTestTLS_ECDSA, TestTLSTransfer) {
  TestHandshake(true);
  TestTransfer("Hello, world!");
}

// Test transfer between client and server, using ECDSA with custom cert
// verifier.
TEST_F(SSLAdapterTestTLS_ECDSA, TestTLSTransferCustomCertVerifier) {
  SetMockCertVerifier(/*return_value=*/true);
  TestHandshake(/*expect_success=*/true);
  TestTransfer("Hello, world!");
}

// Test transfer using ALPN with protos as h2 and http/1.1
TEST_F(SSLAdapterTestTLS_ECDSA, TestTLSALPN) {
  std::vector<std::string> alpn_protos{"h2", "http/1.1"};
  SetAlpnProtocols(alpn_protos);
  TestHandshake(true);
  TestTransfer("Hello, world!");
}

// Test transfer with TLS Elliptic curves set to "X25519:P-256:P-384:P-521"
TEST_F(SSLAdapterTestTLS_ECDSA, TestTLSEllipticCurves) {
  std::vector<std::string> elliptic_curves{"X25519", "P-256", "P-384", "P-521"};
  SetEllipticCurves(elliptic_curves);
  TestHandshake(true);
  TestTransfer("Hello, world!");
}
