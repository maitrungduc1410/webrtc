/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstddef>
#include <cstdint>
#include <vector>

#include "api/array_view.h"
#include "api/environment/environment_factory.h"
#include "api/field_trials.h"
#include "modules/rtp_rtcp/include/report_block_data.h"
#include "modules/rtp_rtcp/source/rtcp_packet/tmmb_item.h"
#include "modules/rtp_rtcp/source/rtcp_receiver.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_interface.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace {

constexpr int kRtcpIntervalMs = 1000;

// RTCP is typically sent over UDP, which has a maximum payload length
// of 65535 bytes. We err on the side of caution and check a bit above that.
constexpr size_t kMaxInputLenBytes = 66000;

class NullModuleRtpRtcp : public RTCPReceiver::ModuleRtpRtcp {
 public:
  void SetTmmbn(std::vector<rtcp::TmmbItem>) override {}
  void OnRequestSendReport() override {}
  void OnReceivedNack(const std::vector<uint16_t>&) override {}
  void OnReceivedRtcpReportBlocks(
      webrtc::ArrayView<const ReportBlockData> report_blocks) override {}
};

}  // namespace

void FuzzOneInput(const uint8_t* data, size_t size) {
  if (size > kMaxInputLenBytes) {
    return;
  }
  FieldTrials field_trials("WebRTC-RFC8888CongestionControlFeedback/Enabled/");
  NullModuleRtpRtcp rtp_rtcp_module;
  SimulatedClock clock(1234);

  RtpRtcpInterface::Configuration config;
  config.rtcp_report_interval_ms = kRtcpIntervalMs;
  config.local_media_ssrc = 1;

  RTCPReceiver receiver(CreateEnvironment(&clock, &field_trials), config,
                        &rtp_rtcp_module);

  receiver.IncomingPacket(webrtc::MakeArrayView(data, size));
}
}  // namespace webrtc
