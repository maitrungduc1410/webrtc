/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_DTLS_SRTP_TRANSPORT_H_
#define PC_DTLS_SRTP_TRANSPORT_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "api/dtls_transport_interface.h"
#include "api/field_trials_view.h"
#include "p2p/base/packet_transport_internal.h"
#include "p2p/dtls/dtls_transport_internal.h"
#include "pc/srtp_transport.h"
#include "rtc_base/buffer.h"

namespace webrtc {

// The subclass of SrtpTransport is used for DTLS-SRTP. When the DTLS handshake
// is finished, it extracts the keying materials from DtlsTransport and
// configures the SrtpSessions in the base class.
class DtlsSrtpTransport : public SrtpTransport {
 public:
  DtlsSrtpTransport(bool rtcp_mux_enabled, const FieldTrialsView& field_trials);

  // Set P2P layer RTP/RTCP DtlsTransports. When using RTCP-muxing,
  // `rtcp_dtls_transport` is null.
  void SetDtlsTransports(DtlsTransportInternal* rtp_dtls_transport,
                         DtlsTransportInternal* rtcp_dtls_transport);
  void SetDtlsTransportsOwned(
      std::unique_ptr<DtlsTransportInternal> rtp_dtls_transport,
      std::unique_ptr<DtlsTransportInternal> rtcp_dtls_transport);

  void SetRtcpMuxEnabled(bool enable) override;

  // Set the header extension ids that should be encrypted.
  void UpdateSendEncryptedHeaderExtensionIds(
      const std::vector<int>& send_extension_ids);

  void UpdateRecvEncryptedHeaderExtensionIds(
      const std::vector<int>& recv_extension_ids);

  void SetOnDtlsStateChange(absl::AnyInvocable<void()> callback);

  DtlsTransportInternal* rtp_dtls_transport() const {
    return rtp_dtls_transport_;
  }

  DtlsTransportInternal* rtcp_dtls_transport() const {
    return rtcp_dtls_transport_;
  }

 private:
  bool IsDtlsActive();
  bool IsDtlsConnected();
  bool IsDtlsWritable();
  bool DtlsHandshakeCompleted();
  void MaybeSetupDtlsSrtp();
  void SetupRtpDtlsSrtp();
  void SetupRtcpDtlsSrtp();
  bool ExtractParams(DtlsTransportInternal* dtls_transport,
                     int* selected_crypto_suite,
                     ZeroOnFreeBuffer<uint8_t>* send_key,
                     ZeroOnFreeBuffer<uint8_t>* recv_key);
  // Updates the DTLS transport and manages the state subscription.
  // The `old_dtls_transport` parameter is a reference to the member variable
  // that holds the transport. It is updated in-place because `OnDtlsState`
  // (called internally) requires the member variable to be updated to the new
  // transport to verify state.
  void ConfigureDtlsTransport(DtlsTransportInternal* new_dtls_transport,
                              DtlsTransportInternal*& old_dtls_transport);
  void SetRtpDtlsTransport(DtlsTransportInternal* rtp_dtls_transport);
  void SetRtcpDtlsTransport(DtlsTransportInternal* rtcp_dtls_transport);

  void OnDtlsState(DtlsTransportInternal* dtls_transport,
                   DtlsTransportState state);

  // Override the SrtpTransport::OnWritableState.
  void OnWritableState(PacketTransportInternal* packet_transport) override;

  // Owned by the TransportController.
  DtlsTransportInternal* rtp_dtls_transport_ = nullptr;
  DtlsTransportInternal* rtcp_dtls_transport_ = nullptr;

  // The encrypted header extension IDs.
  std::optional<std::vector<int>> send_extension_ids_;
  std::optional<std::vector<int>> recv_extension_ids_;

  absl::AnyInvocable<void()> on_dtls_state_change_;
};

}  // namespace webrtc

#endif  // PC_DTLS_SRTP_TRANSPORT_H_
