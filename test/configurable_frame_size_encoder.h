/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_CONFIGURABLE_FRAME_SIZE_ENCODER_H_
#define TEST_CONFIGURABLE_FRAME_SIZE_ENCODER_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "api/environment/environment.h"
#include "api/fec_controller_override.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_image.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_type.h"
#include "api/video/video_timing.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {
namespace test {

// An encoder that produces frames of a configurable size without encoding
// anything. By default, every frame is a key frame of the size set with the
// constructor or SetFrameSize(), filled with zeros and delivered synchronously
// from Encode().
//
// `Config` adds what tests of calls at high bitrates and frame rates need:
// frame sizes that follow the target bitrate, key frames only on request, a
// frame header that ConfigurableFrameSizeDecoder verifies, and delivery of the
// encoded frames after an encode delay.
class ConfigurableFrameSizeEncoder : public VideoEncoder {
 public:
  struct Config {
    // Size of the frames until SetFrameSize() is called. Unused if
    // `frame_size_from_target_bitrate` is set.
    size_t frame_size = 0;
    // If true, frame sizes follow the target bitrate and frame rate set with
    // SetRates(), and key frames are `key_frame_size_factor` times as large as
    // delta frames. Input frames are dropped while the target bitrate is zero.
    bool frame_size_from_target_bitrate = false;
    double key_frame_size_factor = 1.0;
    // If true, only the first frame after InitEncode() or Release() and the
    // frames that are requested with `frame_types` are key frames, which makes
    // every key frame request visible in tests. Otherwise every frame is a key
    // frame.
    bool key_frames_on_request_only = false;
    // If true, every frame starts with a FrameHeader, see WriteFrame().
    // Otherwise frames are filled with zeros.
    bool write_frame_header = false;
    // If set (must be positive), encoded frames are delivered on a separate
    // task queue this long after Encode(), and at most `max_frames_in_flight`
    // frames are encoded at a time: input frames that arrive while that many
    // frames are in flight are dropped. Otherwise frames are delivered
    // synchronously from Encode().
    std::optional<TimeDelta> encode_delay;
    int max_frames_in_flight = 10;
    // If set, attached to every frame.
    std::optional<VideoPlayoutDelay> playout_delay;
    // Reported in `EncoderInfo::is_hardware_accelerated`. Among other things,
    // VideoStreamEncoder uses higher CPU overuse thresholds for hardware
    // encoders.
    bool is_hardware_accelerated = true;
  };

  // Metadata at the start of every frame if `Config::write_frame_header` is
  // set. Layout, with numbers in big endian byte order:
  //   bytes 0-3:   magic number 0x43465346 ("CFSF")
  //   byte 4:      flags, 0x01 means key frame
  //   bytes 5-7:   zero
  //   bytes 8-15:  frame id, incremented for every frame
  //   bytes 16-23: capture time in microseconds, not negative
  //   bytes 24-:   filler bytes, all with a value that depends on the frame
  //                id, so that a payload of another frame is detected as
  //                corrupt.
  struct FrameHeader {
    static constexpr size_t kSize = 24;

    uint64_t frame_id = 0;
    Timestamp capture_time = Timestamp::Zero();
    bool is_key_frame = false;
  };

  // Writes `header` followed by filler bytes to `payload`, which must be at
  // least FrameHeader::kSize bytes.
  static void WriteFrame(const FrameHeader& header, std::span<uint8_t> payload);

  // Returns the header of `payload` if the payload is a well-formed frame.
  static std::optional<FrameHeader> ParseFrame(
      std::span<const uint8_t> payload);

  explicit ConfigurableFrameSizeEncoder(size_t max_frame_size);
  ConfigurableFrameSizeEncoder(const Environment& env, const Config& config);
  ~ConfigurableFrameSizeEncoder() override;

  void SetFecControllerOverride(
      FecControllerOverride* fec_controller_override) override;

  int32_t InitEncode(const VideoCodec* codec_settings,
                     const Settings& settings) override;

  int32_t Encode(const VideoFrame& input_image,
                 const std::vector<VideoFrameType>* frame_types) override;

  int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) override;

  int32_t Release() override;

  void SetRates(const RateControlParameters& parameters) override;

  EncoderInfo GetEncoderInfo() const override;

  int32_t SetFrameSize(size_t size);

  void SetCodecType(VideoCodecType codec_type_);

  // `post_encode_callback` is called at the end of every Encode() call that
  // produces a frame.
  void RegisterPostEncodeCallback(
      std::function<void(void)> post_encode_callback);

 private:
  ConfigurableFrameSizeEncoder(std::optional<Environment> env,
                               const Config& config);

  EncodedImage CreateEncodedImage(const VideoFrame& frame, bool is_key_frame);
  size_t NextFrameSize(bool is_key_frame) const;
  // Returns false if an input frame that arrives now must be dropped because
  // `Config::max_frames_in_flight` frames are being encoded. Otherwise adds the
  // frame to the frames in flight and returns true.
  bool TryAddFrameInFlight();
  void Deliver(EncodedImage image);
  // Reports the frame with `rtp_timestamp` as dropped, in order with the
  // delivered frames.
  void ReportDroppedFrame(uint32_t rtp_timestamp);

  // Only set if the encoder was created with an Environment.
  const std::optional<Environment> env_;
  const Config config_;
  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_checker_{
      SequenceChecker::kDetached};
  EncodedImageCallback* callback_ RTC_GUARDED_BY(sequence_checker_) = nullptr;
  std::optional<std::function<void(void)>> post_encode_callback_;

  size_t current_frame_size_;
  VideoCodecType codec_type_ = kVideoCodecGeneric;

  int max_framerate_ RTC_GUARDED_BY(sequence_checker_) = 0;
  double framerate_fps_ RTC_GUARDED_BY(sequence_checker_) = 0.0;
  DataRate target_bitrate_ RTC_GUARDED_BY(sequence_checker_) = DataRate::Zero();
  uint64_t next_frame_id_ RTC_GUARDED_BY(sequence_checker_) = 0;
  bool key_frame_pending_ RTC_GUARDED_BY(sequence_checker_) = true;
  // Delivery times of the frames currently being encoded.
  std::deque<Timestamp> frames_in_flight_ RTC_GUARDED_BY(sequence_checker_);
  // Encoded frames are delivered on this queue if `Config::encode_delay` is
  // set. Deleting the queue guarantees that no deliveries are running or will
  // run afterwards.
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> delivery_queue_
      RTC_GUARDED_BY(sequence_checker_);
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_CONFIGURABLE_FRAME_SIZE_ENCODER_H_
