/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_PACKET_ERROR_CAUSE_OUT_OF_RESOURCE_ERROR_CAUSE_H_
#define NET_DCSCTP_PACKET_ERROR_CAUSE_OUT_OF_RESOURCE_ERROR_CAUSE_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "api/array_view.h"
#include "net/dcsctp/packet/parameter/parameter.h"
#include "net/dcsctp/packet/tlv_trait.h"

namespace dcsctp {

// https://tools.ietf.org/html/rfc4960#section-3.3.10.4
struct OutOfResourceParameterConfig : public ParameterConfig {
  static constexpr int kType = 4;
  static constexpr size_t kHeaderSize = 4;
  static constexpr size_t kVariableLengthAlignment = 0;
};

class OutOfResourceErrorCause : public Parameter,
                                public TLVTrait<OutOfResourceParameterConfig> {
 public:
  static constexpr int kType = OutOfResourceParameterConfig::kType;

  OutOfResourceErrorCause() {}

  static std::optional<OutOfResourceErrorCause> Parse(
      webrtc::ArrayView<const uint8_t> data);

  void SerializeTo(std::vector<uint8_t>& out) const override;
  std::string ToString() const override;
};

}  // namespace dcsctp

#endif  // NET_DCSCTP_PACKET_ERROR_CAUSE_OUT_OF_RESOURCE_ERROR_CAUSE_H_
