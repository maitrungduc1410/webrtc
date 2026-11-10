/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_TEST_TURN_SERVER_H_
#define P2P_TEST_TURN_SERVER_H_

#include <cstddef>
#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/environment/environment.h"
#include "api/packet_socket_factory.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "api/transport/ecn_marking.h"
#include "api/transport/stun.h"
#include "api/units/time_delta.h"
#include "p2p/base/port_interface.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/byte_buffer.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/memory/less_unique_ptr.h"
#include "rtc_base/net_helper.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/ssl_adapter.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

constexpr int kMinTurnChannelNumber = 0x4000;
constexpr int kMaxTurnChannelNumber = 0x7FFF;

class TurnServer;

// The default server port for TURN, as specified in RFC5766.
const int TURN_SERVER_PORT = 3478;

// Encapsulates the client's connection to the server.
class TurnServerConnection {
 public:
  TurnServerConnection() : proto_(PROTO_UDP), socket_(NULL) {}
  TurnServerConnection(const SocketAddress& src,
                       ProtocolType proto,
                       AsyncPacketSocket* socket);
  const SocketAddress& src() const { return src_; }
  AsyncPacketSocket* socket() { return socket_; }
  bool operator==(const TurnServerConnection& t) const;
  bool operator<(const TurnServerConnection& t) const;
  std::string ToString() const;

 private:
  SocketAddress src_;
  SocketAddress dst_;
  ProtocolType proto_;
  AsyncPacketSocket* socket_;
};

// Encapsulates a TURN allocation.
// The object is created when an allocation request is received, and then
// handles TURN messages (via HandleTurnMessage) and channel data messages
// (via HandleChannelData) for this allocation when received by the server.
// The object informs the server when its lifetime timer expires.
class TurnServerAllocation final {
 public:
  TurnServerAllocation(TurnServer* server_,
                       TaskQueueBase* thread,
                       const TurnServerConnection& conn,
                       std::unique_ptr<AsyncPacketSocket> server_socket,
                       absl::string_view key);
  ~TurnServerAllocation();

  TurnServerConnection* conn() { return &conn_; }
  const std::string& key() const { return key_; }
  const std::string& transaction_id() const { return transaction_id_; }
  const std::string& username() const { return username_; }
  const std::string& last_nonce() const { return last_nonce_; }
  void set_last_nonce(absl::string_view nonce) {
    last_nonce_ = std::string(nonce);
  }

  std::string ToString() const;

  void HandleTurnMessage(const TurnMessage* msg, EcnMarking ecn);
  void HandleChannelData(std::span<const uint8_t> payload, EcnMarking ecn);
  bool HasPermission(const IPAddress& addr);

 private:
  struct Channel {
    ScopedTaskSafety pending_delete;
    const uint16_t id;
    const SocketAddress peer;
  };
  struct Permission {
    ScopedTaskSafety pending_delete;
    IPAddress peer;
  };
  using PermissionList = std::list<Permission>;
  using ChannelList = std::list<Channel>;

  void PostDeleteSelf(TimeDelta delay);

  void HandleAllocateRequest(const TurnMessage* msg);
  void HandleRefreshRequest(const TurnMessage* msg);
  void HandleSendIndication(const TurnMessage* msg, EcnMarking ecn);
  void HandleCreatePermissionRequest(const TurnMessage* msg);
  void HandleChannelBindRequest(const TurnMessage* msg);

  void OnExternalPacket(AsyncPacketSocket* socket,
                        const ReceivedIpPacket& packet);

  static TimeDelta ComputeLifetime(const TurnMessage& msg);
  void AddPermission(const IPAddress& addr);
  PermissionList::iterator FindPermission(const IPAddress& addr);
  ChannelList::iterator FindChannel(int channel_id);
  ChannelList::iterator FindChannel(const SocketAddress& addr);

  void SendResponse(TurnMessage* msg);
  void SendBadRequestResponse(const TurnMessage* req);
  void SendErrorResponse(const TurnMessage* req,
                         int code,
                         absl::string_view reason);
  void SendExternal(const void* data,
                    size_t size,
                    const SocketAddress& peer,
                    EcnMarking ecn);

  TurnServer* const server_;
  TaskQueueBase* const thread_;
  TurnServerConnection conn_;
  std::unique_ptr<AsyncPacketSocket> external_socket_;
  std::string key_;
  std::string transaction_id_;
  std::string username_;
  std::string last_nonce_;
  PermissionList perms_;
  ChannelList channels_;
  ScopedTaskSafety safety_;
};

// An interface through which the MD5 credential hash can be retrieved.
class TurnAuthInterface {
 public:
  // Gets HA1 for the specified user and realm.
  // HA1 = MD5(A1) = MD5(username:realm:password).
  // Return true if the given username and realm are valid, or false if not.
  virtual bool GetKey(absl::string_view username,
                      absl::string_view realm,
                      std::string* key) = 0;
  // Gets the long-term credential key for `algorithm` (RFC 8489 section
  // 9.2.2): MD5(A1) or SHA-256(A1). The default only supports MD5, via
  // GetKey().
  virtual bool GetKeyForAlgorithm(StunPasswordAlgorithm algorithm,
                                  absl::string_view username,
                                  absl::string_view realm,
                                  std::string* key) {
    return algorithm == StunPasswordAlgorithm::kMd5 &&
           GetKey(username, realm, key);
  }
  virtual ~TurnAuthInterface() = default;
};

// An interface enables Turn Server to control redirection behavior.
class TurnRedirectInterface {
 public:
  virtual bool ShouldRedirect(const SocketAddress& address,
                              SocketAddress* out) = 0;
  virtual ~TurnRedirectInterface() {}
};

class StunMessageObserver {
 public:
  virtual void ReceivedMessage(const TurnMessage* msg) = 0;
  virtual void ReceivedChannelData(std::span<const uint8_t> payload) = 0;
  virtual ~StunMessageObserver() {}
};

// The core TURN server class. Give it a socket to listen on via
// AddInternalServerSocket, and a factory to create external sockets via
// SetExternalSocketFactory, and it's ready to go.
// Not yet wired up: TCP support.
class TurnServer {
 public:
  typedef std::map<TurnServerConnection, std::unique_ptr<TurnServerAllocation>>
      AllocationMap;

  TurnServer(const Environment& env, TaskQueueBase* thread);
  virtual ~TurnServer();

  // Gets/sets the realm value to use for the server.
  const std::string& realm() const {
    RTC_DCHECK_RUN_ON(thread_);
    return realm_;
  }
  void set_realm(absl::string_view realm) {
    RTC_DCHECK_RUN_ON(thread_);
    realm_ = std::string(realm);
  }

  // Gets/sets the value for the SOFTWARE attribute for TURN messages.
  const std::string& software() const {
    RTC_DCHECK_RUN_ON(thread_);
    return software_;
  }
  void set_software(absl::string_view software) {
    RTC_DCHECK_RUN_ON(thread_);
    software_ = std::string(software);
  }

  const AllocationMap& allocations() const {
    RTC_DCHECK_RUN_ON(thread_);
    return allocations_;
  }

  // Sets the authentication callback; does not take ownership.
  void set_auth_hook(TurnAuthInterface* auth_hook) {
    RTC_DCHECK_RUN_ON(thread_);
    auth_hook_ = auth_hook;
  }

  void set_redirect_hook(TurnRedirectInterface* redirect_hook) {
    RTC_DCHECK_RUN_ON(thread_);
    redirect_hook_ = redirect_hook;
  }

  void set_enable_otu_nonce(bool enable) {
    RTC_DCHECK_RUN_ON(thread_);
    enable_otu_nonce_ = enable;
  }

  // If set to true, reject CreatePermission requests to RFC1918 addresses.
  void set_reject_private_addresses(bool filter) {
    RTC_DCHECK_RUN_ON(thread_);
    reject_private_addresses_ = filter;
  }

  void set_reject_bind_requests(bool filter) {
    RTC_DCHECK_RUN_ON(thread_);
    reject_bind_requests_ = filter;
  }

  void set_enable_permission_checks(bool enable) {
    RTC_DCHECK_RUN_ON(thread_);
    enable_permission_checks_ = enable;
  }

  // RFC 8489 section 9.2 password algorithms. If empty (default), the server
  // behaves as a legacy RFC 5389 server: MD5 and MESSAGE-INTEGRITY only.
  // Otherwise nonces carry the "obMatJos2" cookie with the password algorithms
  // bit set, 401/438 responses carry PASSWORD-ALGORITHMS with this list (in
  // order), and requests are checked according to section 9.2.4. May be
  // changed at any time; each nonce remembers the list it was issued with.
  void set_password_algorithms(std::vector<StunPasswordAlgorithm> algorithms) {
    RTC_DCHECK_RUN_ON(thread_);
    password_algorithms_ = std::move(algorithms);
  }

  // For testing only. If set, the PASSWORD-ALGORITHMS attribute placed in
  // 401/438 responses is built from this list instead of the list recorded
  // for the nonce, simulating a modification by an on-path attacker.
  void set_tampered_password_algorithms(
      std::optional<std::vector<StunPasswordAlgorithm>> algorithms) {
    RTC_DCHECK_RUN_ON(thread_);
    tampered_password_algorithms_ = std::move(algorithms);
  }

  // For testing only. If true, 401/438 responses carry a nonce with the
  // password algorithms bit set but no PASSWORD-ALGORITHMS attribute.
  void set_omit_password_algorithms_attribute(bool omit) {
    RTC_DCHECK_RUN_ON(thread_);
    omit_password_algorithms_attribute_ = omit;
  }

  // For testing only. If set, this raw value is sent as PASSWORD-ALGORITHMS
  // in 401/438 responses (e.g. to send a malformed list).
  void set_raw_password_algorithms_override(
      std::optional<std::string> raw_value) {
    RTC_DCHECK_RUN_ON(thread_);
    raw_password_algorithms_override_ = std::move(raw_value);
  }

  // For testing only. If true, 300 (try alternate) responses also carry REALM,
  // NONCE and PASSWORD-ALGORITHMS as a 401 would.
  void set_add_challenge_to_try_alternate(bool add) {
    RTC_DCHECK_RUN_ON(thread_);
    add_challenge_to_try_alternate_ = add;
  }

  // For testing only. If true, the 401/438 challenges are sent as an
  // on-path attacker forcing a downgrade would modify them: the nonce loses
  // the password algorithms cookie and PASSWORD-ALGORITHMS is removed.
  void set_strip_password_algorithms_from_challenges(bool strip) {
    RTC_DCHECK_RUN_ON(thread_);
    strip_password_algorithms_ = strip;
  }

  // For testing only. Integrity attribute put in success responses to
  // requests that used a password algorithm.
  enum class SuccessResponseIntegrity {
    kMessageIntegritySha256,  // The correct one.
    kMessageIntegrity,
    kBoth,  // MESSAGE-INTEGRITY followed by MESSAGE-INTEGRITY-SHA256.
    kNone,
  };
  void set_success_response_integrity(SuccessResponseIntegrity integrity) {
    RTC_DCHECK_RUN_ON(thread_);
    success_response_integrity_ = integrity;
  }

  // For testing only. The next authenticated request that would otherwise be
  // accepted is answered with 438 (stale nonce) and a fresh nonce.
  void ExpireNonceForNextRequest() {
    RTC_DCHECK_RUN_ON(thread_);
    force_stale_nonce_ = true;
  }

  // Starts listening for packets from internal clients.
  void AddInternalSocket(std::unique_ptr<AsyncPacketSocket> socket,
                         ProtocolType protocol);
  // Starts listening for the connections on this socket. When someone tries
  // to connect, the connection will be accepted and a new internal socket
  // will be added.
  void AddInternalServerSocket(
      std::unique_ptr<Socket> socket,
      ProtocolType protocol,
      std::unique_ptr<SSLAdapterFactory> ssl_adapter_factory = nullptr);
  // Specifies the factory to use for creating external sockets.
  void SetExternalSocketFactory(PacketSocketFactory* factory,
                                const SocketAddress& address);
  // For testing only.
  std::string SetTimestampForNextNonce(int64_t timestamp) {
    RTC_DCHECK_RUN_ON(thread_);
    ts_for_next_nonce_ = timestamp;
    return GenerateNonce(timestamp);
  }

  void SetStunMessageObserver(std::unique_ptr<StunMessageObserver> observer) {
    RTC_DCHECK_RUN_ON(thread_);
    stun_message_observer_ = std::move(observer);
  }

 private:
  using ServerSocketMap = std::
      map<std::unique_ptr<AsyncPacketSocket>, ProtocolType, less_unique_ptr>;

  // All private member functions and variables should have access restricted to
  // thread_. But compile-time annotations are missing for members access from
  // TurnServerAllocation (via friend declaration).

  std::string GenerateNonce(int64_t now) const RTC_RUN_ON(thread_);
  static std::string EncodePasswordAlgorithms(
      const std::vector<StunPasswordAlgorithm>& algorithms);
  void OnInternalPacket(AsyncPacketSocket* socket,
                        const ReceivedIpPacket& packet) RTC_RUN_ON(thread_);

  void OnNewInternalConnection(Socket* socket);
  void OnInternalSocketClose(AsyncPacketSocket* socket, int err);

  void HandleStunMessage(TurnServerConnection* conn,
                         std::span<const uint8_t> payload,
                         EcnMarking ecn) RTC_RUN_ON(thread_);
  void HandleBindingRequest(TurnServerConnection* conn, const StunMessage* msg)
      RTC_RUN_ON(thread_);
  void HandleAllocateRequest(TurnServerConnection* conn,
                             const TurnMessage* msg,
                             absl::string_view key,
                             EcnMarking ecn) RTC_RUN_ON(thread_);

  bool GetKey(const StunMessage* msg, std::string* key) RTC_RUN_ON(thread_);
  bool CheckAuthorization(TurnServerConnection* conn,
                          StunMessage* msg,
                          absl::string_view key) RTC_RUN_ON(thread_);
  bool ValidateNonce(absl::string_view nonce) const RTC_RUN_ON(thread_);

  TurnServerAllocation* FindAllocation(TurnServerConnection* conn)
      RTC_RUN_ON(thread_);
  TurnServerAllocation* CreateAllocation(TurnServerConnection* conn,
                                         int proto,
                                         absl::string_view key)
      RTC_RUN_ON(thread_);

  void SendErrorResponse(TurnServerConnection* conn,
                         const StunMessage* req,
                         int code,
                         absl::string_view reason);

  void AddChallengeAttributes(TurnMessage* resp) RTC_RUN_ON(thread_);
  void SendErrorResponseWithRealmAndNonce(TurnServerConnection* conn,
                                          const StunMessage* req,
                                          int code,
                                          absl::string_view reason)
      RTC_RUN_ON(thread_);

  void SendErrorResponseWithAlternateServer(TurnServerConnection* conn,
                                            const StunMessage* req,
                                            const SocketAddress& addr)
      RTC_RUN_ON(thread_);

  void SendStun(TurnServerConnection* conn, StunMessage* msg, EcnMarking ecn);
  void Send(TurnServerConnection* conn,
            const ByteBufferWriter& buf,
            EcnMarking ecn);

  void DestroyAllocation(TurnServerAllocation* allocation) RTC_RUN_ON(thread_);
  void DestroyInternalSocket(ServerSocketMap::iterator iter)
      RTC_RUN_ON(thread_);

  struct ServerSocketInfo {
    ProtocolType proto;
    // If non-null, used to wrap accepted sockets.
    std::unique_ptr<SSLAdapterFactory> ssl_adapter_factory;
  };

  const Environment env_;
  TaskQueueBase* const thread_;
  const std::string nonce_key_;
  std::string realm_ RTC_GUARDED_BY(thread_);
  std::string software_ RTC_GUARDED_BY(thread_);
  TurnAuthInterface* auth_hook_ RTC_GUARDED_BY(thread_);
  TurnRedirectInterface* redirect_hook_ RTC_GUARDED_BY(thread_);
  // otu - one-time-use. Server will respond with 438 if it's
  // sees the same nonce in next transaction.
  bool enable_otu_nonce_ RTC_GUARDED_BY(thread_);
  bool reject_private_addresses_ RTC_GUARDED_BY(thread_) = false;
  bool reject_bind_requests_ RTC_GUARDED_BY(thread_) = false;
  // Check for permission when receiving an external packet.
  bool enable_permission_checks_ = true;

  std::vector<StunPasswordAlgorithm> password_algorithms_
      RTC_GUARDED_BY(thread_);
  std::optional<std::vector<StunPasswordAlgorithm>>
      tampered_password_algorithms_ RTC_GUARDED_BY(thread_);
  bool omit_password_algorithms_attribute_ RTC_GUARDED_BY(thread_) = false;
  bool strip_password_algorithms_ RTC_GUARDED_BY(thread_) = false;
  bool add_challenge_to_try_alternate_ RTC_GUARDED_BY(thread_) = false;
  std::optional<std::string> raw_password_algorithms_override_
      RTC_GUARDED_BY(thread_);
  SuccessResponseIntegrity success_response_integrity_ RTC_GUARDED_BY(thread_) =
      SuccessResponseIntegrity::kMessageIntegritySha256;
  bool force_stale_nonce_ RTC_GUARDED_BY(thread_) = false;
  // Encoded PASSWORD-ALGORITHMS value sent together with each nonce.
  std::map<std::string, std::string> nonce_password_algorithms_
      RTC_GUARDED_BY(thread_);
  // If set, success responses to the request currently being handled carry
  // MESSAGE-INTEGRITY-SHA256 computed with this key instead of
  // MESSAGE-INTEGRITY. Only valid during synchronous request handling.
  std::optional<std::string> response_sha256_key_;
  // Only meaningful together with `response_sha256_key_`.
  SuccessResponseIntegrity response_integrity_ =
      SuccessResponseIntegrity::kMessageIntegritySha256;

  ServerSocketMap server_sockets_ RTC_GUARDED_BY(thread_);
  std::map<std::unique_ptr<Socket>, ServerSocketInfo, less_unique_ptr>
      server_listen_sockets_ RTC_GUARDED_BY(thread_);
  std::unique_ptr<PacketSocketFactory> external_socket_factory_
      RTC_GUARDED_BY(thread_);
  SocketAddress external_addr_ RTC_GUARDED_BY(thread_);

  AllocationMap allocations_ RTC_GUARDED_BY(thread_);

  // For testing only. If this is non-zero, the next NONCE will be generated
  // from this value, and it will be reset to 0 after generating the NONCE.
  int64_t ts_for_next_nonce_ RTC_GUARDED_BY(thread_) = 0;

  // For testing only. Used to observe STUN messages received.
  std::unique_ptr<StunMessageObserver> stun_message_observer_
      RTC_GUARDED_BY(thread_);

  friend class TurnServerAllocation;
};

}  //  namespace webrtc

#endif  // P2P_TEST_TURN_SERVER_H_
