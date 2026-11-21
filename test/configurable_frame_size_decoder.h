/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_CONFIGURABLE_FRAME_SIZE_DECODER_H_
#define TEST_CONFIGURABLE_FRAME_SIZE_DECODER_H_

#include <cstdint>
#include <optional>

#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video_codecs/video_decoder.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {
namespace test {

// Decodes the frames that ConfigurableFrameSizeEncoder produces when
// `ConfigurableFrameSizeEncoder::Config::write_frame_header` is set.
//
// The decoder verifies that delta frames form an unbroken chain and stamps
// each decoded frame with the frame id (`VideoFrame::id()`, truncated to 16
// bits) and the capture time (`VideoFrame::reference_time()`) from the frame
// header, which allows sinks to measure end-to-end latency and to detect gaps.
// The decoded frames are black.
class ConfigurableFrameSizeDecoder : public VideoDecoder {
 public:
  ConfigurableFrameSizeDecoder() = default;
  ~ConfigurableFrameSizeDecoder() override = default;

  bool Configure(const Settings& settings) override;
  int32_t Decode(const EncodedImage& input, int64_t render_time_ms) override;
  int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback) override;
  int32_t Release() override;
  DecoderInfo GetDecoderInfo() const override;

 private:
  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_checker_{
      SequenceChecker::kDetached};
  DecodedImageCallback* callback_ RTC_GUARDED_BY(sequence_checker_) = nullptr;
  // Resolution of the decoded frames.
  int width_ RTC_GUARDED_BY(sequence_checker_) = 16;
  int height_ RTC_GUARDED_BY(sequence_checker_) = 16;
  // Id of the last decoded frame. Unset until a key frame has been decoded and
  // after a broken chain has been detected.
  std::optional<uint64_t> last_frame_id_ RTC_GUARDED_BY(sequence_checker_);
  // The decoded frames carry no pixel data, so a single black buffer is shared
  // by all frames of the same resolution.
  scoped_refptr<I420Buffer> buffer_ RTC_GUARDED_BY(sequence_checker_);
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_CONFIGURABLE_FRAME_SIZE_DECODER_H_
