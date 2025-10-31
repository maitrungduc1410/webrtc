/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_ASYNC_PACKET_SOCKET_H_
#define RTC_BASE_ASYNC_PACKET_SOCKET_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "api/sequence_checker.h"
#include "rtc_base/callback_list.h"
#include "rtc_base/checks.h"
#include "rtc_base/dscp.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/sigslot_trampoline.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

// This structure holds the info needed to update the packet send time header
// extension, including the information needed to update the authentication tag
// after changing the value.
struct PacketTimeUpdateParams {
  PacketTimeUpdateParams();
  PacketTimeUpdateParams(const PacketTimeUpdateParams& other);
  ~PacketTimeUpdateParams();

  int rtp_sendtime_extension_id = -1;  // extension header id present in packet.
  std::vector<char> srtp_auth_key;     // Authentication key.
  int srtp_auth_tag_len = -1;          // Authentication tag length.
  int64_t srtp_packet_index = -1;  // Required for Rtp Packet authentication.
};

// This structure holds meta information for the packet which is about to send
// over network.
struct RTC_EXPORT AsyncSocketPacketOptions {
  AsyncSocketPacketOptions();
  explicit AsyncSocketPacketOptions(DiffServCodePoint dscp);
  AsyncSocketPacketOptions(const AsyncSocketPacketOptions& other);
  ~AsyncSocketPacketOptions();

  DiffServCodePoint dscp = DSCP_NO_CHANGE;

  // Packet will be sent with ECT(1), RFC-3168, Section 5.
  // Intended to be used with L4S
  // https://www.rfc-editor.org/rfc/rfc9331.html
  bool ect_1 = false;

  // When used with RTP packets (for example, PacketOptions), the value
  // should be 16 bits. A value of -1 represents "not set".
  int64_t packet_id = -1;
  PacketTimeUpdateParams packet_time_params;
  // PacketInfo is passed to SentPacket when signaling this packet is sent.
  PacketInfo info_signaled_after_sent;
  // True if this is a batchable packet. Batchable packets are collected at low
  // levels and sent first when their AsyncPacketSocket receives a
  // OnSendBatchComplete call.
  bool batchable = false;
  // True if this is the last packet of a batch.
  bool last_packet_in_batch = false;
};

// Provides the ability to receive packets asynchronously. Sends are not
// buffered since it is acceptable to drop packets under high load.
class RTC_EXPORT AsyncPacketSocket : public sigslot::has_slots<> {
 public:
  enum State {
    STATE_CLOSED,
    STATE_BINDING,
    STATE_BOUND,
    STATE_CONNECTING,
    STATE_CONNECTED
  };

  AsyncPacketSocket()
      : connect_trampoline_(this),
        sent_packet_trampoline_(this),
        ready_to_send_trampoline_(this),
        address_ready_trampoline_(this) {}
  ~AsyncPacketSocket() override;

  AsyncPacketSocket(const AsyncPacketSocket&) = delete;
  AsyncPacketSocket& operator=(const AsyncPacketSocket&) = delete;

  // Returns current local address. Address may be set to null if the
  // socket is not bound yet (GetState() returns STATE_BINDING).
  virtual SocketAddress GetLocalAddress() const = 0;

  // Returns remote address. Returns zeroes if this is not a client TCP socket.
  virtual SocketAddress GetRemoteAddress() const = 0;

  // Send a packet.
  virtual int Send(const void* pv,
                   size_t cb,
                   const AsyncSocketPacketOptions& options) = 0;
  virtual int SendTo(const void* pv,
                     size_t cb,
                     const SocketAddress& addr,
                     const AsyncSocketPacketOptions& options) = 0;

  // Close the socket.
  virtual int Close() = 0;

  // Returns current state of the socket.
  virtual State GetState() const = 0;

  // Get/set options.
  virtual int GetOption(Socket::Option opt, int* value) = 0;
  virtual int SetOption(Socket::Option opt, int value) = 0;

  // Get/Set current error.
  // TODO: Remove SetError().
  virtual int GetError() const = 0;
  virtual void SetError(int error) = 0;

  // Register a callback to be called when the socket is closed.
  void SubscribeCloseEvent(
      const void* removal_tag,
      std::function<void(AsyncPacketSocket*, int)> callback);
  void UnsubscribeCloseEvent(const void* removal_tag);

  void RegisterReceivedPacketCallback(
      absl::AnyInvocable<void(AsyncPacketSocket*, const ReceivedIpPacket&)>
          received_packet_callback);
  void DeregisterReceivedPacketCallback();

  // Emitted each time a packet is sent.
  sigslot::signal2<AsyncPacketSocket*, const SentPacketInfo&> SignalSentPacket;
  void SubscribeSentPacket(
      void* tag,
      absl::AnyInvocable<void(AsyncPacketSocket*, const SentPacketInfo&)>
          callback) {
    sent_packet_trampoline_.Subscribe(tag, std::move(callback));
  }
  void UnsubscribeSentPacket(void* tag) {
    sent_packet_trampoline_.Unsubscribe(tag);
  }
  void NotifySentPacket(AsyncPacketSocket* socket, const SentPacketInfo& info) {
    SignalSentPacket(socket, info);
  }

  // Emitted when the socket is currently able to send.
  sigslot::signal1<AsyncPacketSocket*> SignalReadyToSend;
  void SubscribeReadyToSend(
      void* tag,
      absl::AnyInvocable<void(AsyncPacketSocket*)> callback) {
    ready_to_send_trampoline_.Subscribe(tag, std::move(callback));
  }
  void UnsubscribeReadyToSend(void* tag) {
    ready_to_send_trampoline_.Unsubscribe(tag);
  }
  void NotifyReadyToSend(AsyncPacketSocket* socket) {
    SignalReadyToSend(socket);
  }

  // Emitted after address for the socket is allocated, i.e. binding
  // is finished. State of the socket is changed from BINDING to BOUND
  // (for UDP sockets).
  sigslot::signal2<AsyncPacketSocket*, const SocketAddress&> SignalAddressReady;
  void SubscribeAddressReady(
      void* tag,
      absl::AnyInvocable<void(AsyncPacketSocket*, const SocketAddress&)>
          callback) {
    address_ready_trampoline_.Subscribe(tag, std::move(callback));
  }
  void UnsubscribeAddressReady(void* tag) {
    address_ready_trampoline_.Unsubscribe(tag);
  }
  void NotifyAddressReady(AsyncPacketSocket* socket,
                          const SocketAddress& address) {
    SignalAddressReady(socket, address);
  }

  // Emitted for client TCP sockets when state is changed from
  // CONNECTING to CONNECTED.
  sigslot::signal1<AsyncPacketSocket*> SignalConnect;
  void NotifyConnect(AsyncPacketSocket* socket) { SignalConnect(socket); }
  void SubscribeConnect(absl::AnyInvocable<void(AsyncPacketSocket*)> callback) {
    connect_trampoline_.Subscribe(std::move(callback));
  }
  void SubscribeConnect(void* tag,
                        absl::AnyInvocable<void(AsyncPacketSocket*)> callback) {
    connect_trampoline_.Subscribe(tag, std::move(callback));
  }
  void UnsubscribeConnect(void* tag) { connect_trampoline_.Unsubscribe(tag); }

  void NotifyClosedForTest(int err) { NotifyClosed(err); }

 protected:
  // TODO(bugs.webrtc.org/11943): Remove after updating downstream code.
  void SignalClose(AsyncPacketSocket* s, int err) {
    RTC_DCHECK_EQ(s, this);
    NotifyClosed(err);
  }

  void NotifyClosed(int err) {
    RTC_DCHECK_RUN_ON(&network_checker_);
    on_close_.Send(this, err);
  }

  void NotifyPacketReceived(const ReceivedIpPacket& packet);

  RTC_NO_UNIQUE_ADDRESS SequenceChecker network_checker_{
      SequenceChecker::kDetached};

 private:
  CallbackList<AsyncPacketSocket*, int> on_close_
      RTC_GUARDED_BY(&network_checker_);
  absl::AnyInvocable<void(AsyncPacketSocket*, const ReceivedIpPacket&)>
      received_packet_callback_ RTC_GUARDED_BY(&network_checker_);
  SignalTrampoline<AsyncPacketSocket, &AsyncPacketSocket::SignalConnect>
      connect_trampoline_;
  SignalTrampoline<AsyncPacketSocket, &AsyncPacketSocket::SignalSentPacket>
      sent_packet_trampoline_;
  SignalTrampoline<AsyncPacketSocket, &AsyncPacketSocket::SignalReadyToSend>
      ready_to_send_trampoline_;
  SignalTrampoline<AsyncPacketSocket, &AsyncPacketSocket::SignalAddressReady>
      address_ready_trampoline_;
};

// Listen socket, producing an AsyncPacketSocket when a peer connects.
class RTC_EXPORT AsyncListenSocket : public sigslot::has_slots<> {
 public:
  enum class State {
    kClosed,
    kBound,
  };

  AsyncListenSocket() : new_connection_trampoline_(this) {}

  // Returns current state of the socket.
  virtual State GetState() const = 0;

  // Returns current local address. Address may be set to null if the
  // socket is not bound yet (GetState() returns kBinding).
  virtual SocketAddress GetLocalAddress() const = 0;

  sigslot::signal2<AsyncListenSocket*, AsyncPacketSocket*> SignalNewConnection;
  void SubscribeNewConnection(
      void* tag,
      absl::AnyInvocable<void(AsyncListenSocket*, AsyncPacketSocket*)>
          callback) {
    new_connection_trampoline_.Subscribe(tag, std::move(callback));
  }
  void UnsubscribeNewConnection(void* tag) {
    new_connection_trampoline_.Unsubscribe(tag);
  }
  void NotifyNewConnection(AsyncListenSocket* listen_socket,
                           AsyncPacketSocket* packet_socket) {
    SignalNewConnection(listen_socket, packet_socket);
  }

 private:
  SignalTrampoline<AsyncListenSocket, &AsyncListenSocket::SignalNewConnection>
      new_connection_trampoline_;
};

void CopySocketInformationToPacketInfo(size_t packet_size_bytes,
                                       const AsyncPacketSocket& socket_from,
                                       PacketInfo* info);

}  //  namespace webrtc


#endif  // RTC_BASE_ASYNC_PACKET_SOCKET_H_
