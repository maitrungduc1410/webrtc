/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <iostream>

#include "p2p/test/stun_server.h"
#include "rtc_base/async_udp_socket.h"
#include "rtc_base/checks.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/socket_server.h"
#include "rtc_base/thread.h"

using ::webrtc::StunServer;

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "usage: stunserver address" << std::endl;
    return 1;
  }

  webrtc::SocketAddress server_addr;
  if (!server_addr.FromString(argv[1])) {
    std::cerr << "Unable to parse IP address: " << argv[1];
    return 1;
  }

  webrtc::Thread* pthMain =
      webrtc::ThreadManager::Instance()->WrapCurrentThread();
  RTC_DCHECK(pthMain);

  webrtc::AsyncUDPSocket* server_socket =
      webrtc::AsyncUDPSocket::Create(pthMain->socketserver(), server_addr);
  if (!server_socket) {
    std::cerr << "Failed to create a UDP socket" << std::endl;
    return 1;
  }

  StunServer* server = new StunServer(server_socket);

  std::cout << "Listening at " << server_addr.ToString() << std::endl;

  pthMain->Run();

  delete server;
  return 0;
}
