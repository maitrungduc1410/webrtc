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

#include "modules/video_coding/utility/vp8_header_parser.h"

namespace webrtc {
void FuzzOneInput(const uint8_t* data, size_t size) {
  int qp;
  vp8::GetQp(data, size, &qp);
}
}  // namespace webrtc
