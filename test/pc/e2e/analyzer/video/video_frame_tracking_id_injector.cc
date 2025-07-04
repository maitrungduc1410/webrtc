/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/video_frame_tracking_id_injector.h"

#include <cstdint>

#include "api/video/encoded_image.h"
#include "rtc_base/checks.h"
#include "test/pc/e2e/analyzer/video/encoded_image_data_injector.h"

namespace webrtc {
namespace webrtc_pc_e2e {

EncodedImage VideoFrameTrackingIdInjector::InjectData(
    uint16_t id,
    bool unused_discard,
    const EncodedImage& source) {
  RTC_CHECK(!unused_discard);
  EncodedImage out = source;
  out.SetVideoFrameTrackingId(id);
  return out;
}

EncodedImageExtractionResult VideoFrameTrackingIdInjector::ExtractData(
    const EncodedImage& source) {
  return EncodedImageExtractionResult{source.VideoFrameTrackingId(), source,
                                      /*discard=*/false};
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
