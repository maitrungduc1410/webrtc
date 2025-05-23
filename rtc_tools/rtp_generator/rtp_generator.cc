/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/rtp_generator/rtp_generator.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "api/call/transport.h"
#include "api/environment/environment_factory.h"
#include "api/make_ref_counted.h"
#include "api/media_types.h"
#include "api/rtp_headers.h"
#include "api/rtp_parameters.h"
#include "api/test/create_frame_generator.h"
#include "api/video/builtin_video_bitrate_allocator_factory.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_decoder_factory_template.h"
#include "api/video_codecs/video_decoder_factory_template_dav1d_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory_template.h"
#include "api/video_codecs/video_encoder_factory_template_libaom_av1_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp9_adapter.h"
#include "call/call.h"
#include "call/call_config.h"
#include "call/video_send_stream.h"
#include "media/base/media_constants.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/json.h"
#include "rtc_base/system/file_wrapper.h"
#include "rtc_base/thread.h"
#include "system_wrappers/include/clock.h"
#include "test/frame_generator_capturer.h"
#include "test/rtp_file_reader.h"
#include "test/rtp_file_writer.h"
#include "test/testsupport/file_utils.h"
#include "video/config/video_encoder_config.h"

namespace webrtc {
namespace {

// Payload types.
constexpr int kPayloadTypeVp8 = 125;
constexpr int kPayloadTypeVp9 = 124;
constexpr int kPayloadTypeH264 = 123;
constexpr int kFakeVideoSendPayloadType = 122;

// Defaults
constexpr int kDefaultSsrc = 1337;
constexpr int kMaxConfigBufferSize = 8192;

// Utility function to validate a correct codec type has been passed in.
bool IsValidCodecType(const std::string& codec_name) {
  return kVp8CodecName == codec_name || kVp9CodecName == codec_name ||
         kH264CodecName == codec_name;
}

// Utility function to return some base payload type for a codec_name.
int GetDefaultTypeForPayloadName(const std::string& codec_name) {
  if (kVp8CodecName == codec_name) {
    return kPayloadTypeVp8;
  }
  if (kVp9CodecName == codec_name) {
    return kPayloadTypeVp9;
  }
  if (kH264CodecName == codec_name) {
    return kPayloadTypeH264;
  }
  return kFakeVideoSendPayloadType;
}

// Creates a single VideoSendStream configuration.
std::optional<RtpGeneratorOptions::VideoSendStreamConfig>
ParseVideoSendStreamConfig(const Json::Value& json) {
  RtpGeneratorOptions::VideoSendStreamConfig config;

  // Parse video source settings.
  if (!GetIntFromJsonObject(json, "duration_ms", &config.duration_ms)) {
    RTC_LOG(LS_WARNING) << "duration_ms not specified using default: "
                        << config.duration_ms;
  }
  if (!GetIntFromJsonObject(json, "video_width", &config.video_width)) {
    RTC_LOG(LS_WARNING) << "video_width not specified using default: "
                        << config.video_width;
  }
  if (!GetIntFromJsonObject(json, "video_height", &config.video_height)) {
    RTC_LOG(LS_WARNING) << "video_height not specified using default: "
                        << config.video_height;
  }
  if (!GetIntFromJsonObject(json, "video_fps", &config.video_fps)) {
    RTC_LOG(LS_WARNING) << "video_fps not specified using default: "
                        << config.video_fps;
  }
  if (!GetIntFromJsonObject(json, "num_squares", &config.num_squares)) {
    RTC_LOG(LS_WARNING) << "num_squares not specified using default: "
                        << config.num_squares;
  }

  // Parse RTP settings for this configuration.
  config.rtp.ssrcs.push_back(kDefaultSsrc);
  Json::Value rtp_json;
  if (!GetValueFromJsonObject(json, "rtp", &rtp_json)) {
    RTC_LOG(LS_ERROR) << "video_streams must have an rtp section";
    return std::nullopt;
  }
  if (!GetStringFromJsonObject(rtp_json, "payload_name",
                               &config.rtp.payload_name)) {
    RTC_LOG(LS_ERROR) << "rtp.payload_name must be specified";
    return std::nullopt;
  }
  if (!IsValidCodecType(config.rtp.payload_name)) {
    RTC_LOG(LS_ERROR) << "rtp.payload_name must be VP8,VP9 or H264";
    return std::nullopt;
  }

  config.rtp.payload_type =
      GetDefaultTypeForPayloadName(config.rtp.payload_name);
  if (!GetIntFromJsonObject(rtp_json, "payload_type",
                            &config.rtp.payload_type)) {
    RTC_LOG(LS_WARNING)
        << "rtp.payload_type not specified using default for codec type"
        << config.rtp.payload_type;
  }

  return config;
}

}  // namespace

std::optional<RtpGeneratorOptions> ParseRtpGeneratorOptionsFromFile(
    const std::string& options_file) {
  if (!test::FileExists(options_file)) {
    RTC_LOG(LS_ERROR) << " configuration file does not exist";
    return std::nullopt;
  }

  // Read the configuration file from disk.
  FileWrapper config_file = FileWrapper::OpenReadOnly(options_file);
  std::vector<char> raw_json_buffer(kMaxConfigBufferSize, 0);
  size_t bytes_read =
      config_file.Read(raw_json_buffer.data(), raw_json_buffer.size() - 1);
  if (bytes_read == 0) {
    RTC_LOG(LS_ERROR) << "Unable to read the configuration file.";
    return std::nullopt;
  }

  // Parse the file as JSON
  Json::CharReaderBuilder builder;
  Json::Value json;
  std::string error_message;
  std::unique_ptr<Json::CharReader> json_reader(builder.newCharReader());
  if (!json_reader->parse(raw_json_buffer.data(),
                          raw_json_buffer.data() + raw_json_buffer.size(),
                          &json, &error_message)) {
    RTC_LOG(LS_ERROR) << "Unable to parse the corpus config json file. Error:"
                      << error_message;
    return std::nullopt;
  }

  RtpGeneratorOptions gen_options;
  for (const auto& video_stream_json : json["video_streams"]) {
    std::optional<RtpGeneratorOptions::VideoSendStreamConfig>
        video_stream_config = ParseVideoSendStreamConfig(video_stream_json);
    if (!video_stream_config.has_value()) {
      RTC_LOG(LS_ERROR) << "Unable to parse the corpus config json file";
      return std::nullopt;
    }
    gen_options.video_streams.push_back(*video_stream_config);
  }
  return gen_options;
}

RtpGenerator::RtpGenerator(const RtpGeneratorOptions& options)
    : options_(options),
      env_(CreateEnvironment()),
      video_encoder_factory_(
          std::make_unique<
              VideoEncoderFactoryTemplate<LibvpxVp8EncoderTemplateAdapter,
                                          LibvpxVp9EncoderTemplateAdapter,
                                          LibaomAv1EncoderTemplateAdapter>>()),
      video_decoder_factory_(
          std::make_unique<
              VideoDecoderFactoryTemplate<LibvpxVp8DecoderTemplateAdapter,
                                          LibvpxVp9DecoderTemplateAdapter,
                                          Dav1dDecoderTemplateAdapter>>()),
      video_bitrate_allocator_factory_(
          CreateBuiltinVideoBitrateAllocatorFactory()),
      call_(Call::Create(CallConfig(env_))) {
  constexpr int kMinBitrateBps = 30000;    // 30 Kbps
  constexpr int kMaxBitrateBps = 2500000;  // 2.5 Mbps

  int stream_count = 0;
  VideoEncoder::EncoderInfo encoder_info;
  for (const auto& send_config : options.video_streams) {
    VideoSendStream::Config video_config(this);
    video_config.encoder_settings.encoder_factory =
        video_encoder_factory_.get();
    video_config.encoder_settings.bitrate_allocator_factory =
        video_bitrate_allocator_factory_.get();
    video_config.rtp = send_config.rtp;
    // Update some required to be unique values.
    stream_count++;
    video_config.rtp.mid = "mid-" + std::to_string(stream_count);

    // Configure the video encoder configuration.
    VideoEncoderConfig encoder_config;
    encoder_config.content_type =
        VideoEncoderConfig::ContentType::kRealtimeVideo;
    encoder_config.codec_type =
        PayloadStringToCodecType(video_config.rtp.payload_name);
    if (video_config.rtp.payload_name == kVp8CodecName) {
      VideoCodecVP8 settings = VideoEncoder::GetDefaultVp8Settings();
      encoder_config.encoder_specific_settings =
          make_ref_counted<VideoEncoderConfig::Vp8EncoderSpecificSettings>(
              settings);
    } else if (video_config.rtp.payload_name == kVp9CodecName) {
      VideoCodecVP9 settings = VideoEncoder::GetDefaultVp9Settings();
      encoder_config.encoder_specific_settings =
          make_ref_counted<VideoEncoderConfig::Vp9EncoderSpecificSettings>(
              settings);
    } else if (video_config.rtp.payload_name == kH264CodecName) {
      encoder_config.encoder_specific_settings = nullptr;
    }
    encoder_config.video_format.name = video_config.rtp.payload_name;
    encoder_config.min_transmit_bitrate_bps = 0;
    encoder_config.max_bitrate_bps = kMaxBitrateBps;
    encoder_config.content_type =
        VideoEncoderConfig::ContentType::kRealtimeVideo;

    // Configure the simulcast layers.
    encoder_config.number_of_streams = video_config.rtp.ssrcs.size();
    encoder_config.bitrate_priority = 1.0;
    encoder_config.simulcast_layers.resize(encoder_config.number_of_streams);
    for (size_t i = 0; i < encoder_config.number_of_streams; ++i) {
      encoder_config.simulcast_layers[i].active = true;
      encoder_config.simulcast_layers[i].min_bitrate_bps = kMinBitrateBps;
      encoder_config.simulcast_layers[i].max_bitrate_bps = kMaxBitrateBps;
      encoder_config.simulcast_layers[i].max_framerate = send_config.video_fps;
    }

    // Setup the fake video stream for this.
    std::unique_ptr<test::FrameGeneratorCapturer> frame_generator =
        std::make_unique<test::FrameGeneratorCapturer>(
            &env_.clock(),
            test::CreateSquareFrameGenerator(send_config.video_width,
                                             send_config.video_height,
                                             std::nullopt, std::nullopt),
            send_config.video_fps, env_.task_queue_factory());
    frame_generator->Init();

    VideoSendStream* video_send_stream = call_->CreateVideoSendStream(
        std::move(video_config), std::move(encoder_config));
    video_send_stream->SetSource(frame_generator.get(),
                                 DegradationPreference::MAINTAIN_FRAMERATE);
    // Store these objects so we can destropy them at the end.
    frame_generators_.push_back(std::move(frame_generator));
    video_send_streams_.push_back(video_send_stream);
  }
}

RtpGenerator::~RtpGenerator() {
  for (VideoSendStream* send_stream : video_send_streams_) {
    call_->DestroyVideoSendStream(send_stream);
  }
}

void RtpGenerator::GenerateRtpDump(const std::string& rtp_dump_path) {
  rtp_dump_writer_.reset(test::RtpFileWriter::Create(
      test::RtpFileWriter::kRtpDump, rtp_dump_path));

  call_->SignalChannelNetworkState(MediaType::VIDEO, kNetworkUp);
  for (VideoSendStream* send_stream : video_send_streams_) {
    send_stream->Start();
  }

  // Spinlock until all the durations end.
  WaitUntilAllVideoStreamsFinish();

  call_->SignalChannelNetworkState(MediaType::VIDEO, kNetworkDown);
}

bool RtpGenerator::SendRtp(ArrayView<const uint8_t> packet,
                           const PacketOptions& /* options */) {
  test::RtpPacket rtp_packet = DataToRtpPacket(packet.data(), packet.size());
  rtp_dump_writer_->WritePacket(&rtp_packet);
  return true;
}

bool RtpGenerator::SendRtcp(ArrayView<const uint8_t> packet,
                            const PacketOptions& /* options */) {
  test::RtpPacket rtcp_packet = DataToRtpPacket(packet.data(), packet.size());
  rtp_dump_writer_->WritePacket(&rtcp_packet);
  return true;
}

int RtpGenerator::GetMaxDuration() const {
  int max_end_ms = 0;
  for (const auto& video_stream : options_.video_streams) {
    max_end_ms = std::max(video_stream.duration_ms, max_end_ms);
  }
  return max_end_ms;
}

void RtpGenerator::WaitUntilAllVideoStreamsFinish() {
  // Find the maximum duration required by the streams.
  start_ms_ = Clock::GetRealTimeClock()->TimeInMilliseconds();
  int64_t max_end_ms = start_ms_ + GetMaxDuration();

  int64_t current_time = 0;
  do {
    int64_t min_wait_time = 0;
    current_time = Clock::GetRealTimeClock()->TimeInMilliseconds();
    // Stop any streams that are no longer active.
    for (size_t i = 0; i < options_.video_streams.size(); ++i) {
      const int64_t end_ms = start_ms_ + options_.video_streams[i].duration_ms;
      if (current_time > end_ms) {
        video_send_streams_[i]->Stop();
      } else {
        min_wait_time = std::min(min_wait_time, end_ms - current_time);
      }
    }
    Thread::Current()->SleepMs(min_wait_time);
  } while (current_time < max_end_ms);
}

test::RtpPacket RtpGenerator::DataToRtpPacket(const uint8_t* packet,
                                              size_t packet_len) {
  test::RtpPacket rtp_packet;
  memcpy(rtp_packet.data, packet, packet_len);
  rtp_packet.length = packet_len;
  rtp_packet.original_length = packet_len;
  rtp_packet.time_ms =
      Clock::GetRealTimeClock()->TimeInMilliseconds() - start_ms_;
  return rtp_packet;
}

}  // namespace webrtc
