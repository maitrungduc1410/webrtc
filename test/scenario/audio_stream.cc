/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/scenario/audio_stream.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/call/transport.h"
#include "api/media_types.h"
#include "api/rtp_headers.h"
#include "api/rtp_parameters.h"
#include "api/scoped_refptr.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "call/audio_receive_stream.h"
#include "call/audio_send_stream.h"
#include "rtc_base/checks.h"
#include "rtc_base/strings/string_builder.h"
#include "test/scenario/call_client.h"
#include "test/scenario/column_printer.h"
#include "test/scenario/scenario_config.h"
#include "test/video_test_constants.h"

#if WEBRTC_ENABLE_PROTOBUF
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/modules/audio_coding/audio_network_adaptor/config.pb.h"
#else
#include "modules/audio_coding/audio_network_adaptor/config.pb.h"
#endif
#endif

namespace webrtc {
namespace test {
namespace {
enum : int {  // The first valid value is 1.
  kTransportSequenceNumberExtensionId = 1,
  kAbsSendTimeExtensionId
};

std::optional<std::string> CreateAdaptationString(
    AudioStreamConfig::NetworkAdaptation config) {
#if WEBRTC_ENABLE_PROTOBUF

  audio_network_adaptor::config::ControllerManager cont_conf;
  if (config.frame.max_rate_for_60_ms.IsFinite()) {
    auto controller =
        cont_conf.add_controllers()->mutable_frame_length_controller();
    controller->set_fl_decreasing_packet_loss_fraction(
        config.frame.min_packet_loss_for_decrease);
    controller->set_fl_increasing_packet_loss_fraction(
        config.frame.max_packet_loss_for_increase);

    controller->set_fl_20ms_to_60ms_bandwidth_bps(
        config.frame.min_rate_for_20_ms.bps<int32_t>());
    controller->set_fl_60ms_to_20ms_bandwidth_bps(
        config.frame.max_rate_for_60_ms.bps<int32_t>());

    if (config.frame.max_rate_for_120_ms.IsFinite()) {
      controller->set_fl_60ms_to_120ms_bandwidth_bps(
          config.frame.min_rate_for_60_ms.bps<int32_t>());
      controller->set_fl_120ms_to_60ms_bandwidth_bps(
          config.frame.max_rate_for_120_ms.bps<int32_t>());
    }
  }
  cont_conf.add_controllers()->mutable_bitrate_controller();
  std::string config_string = cont_conf.SerializeAsString();
  return config_string;
#else
  RTC_LOG(LS_ERROR) << "audio_network_adaptation is enabled"
                       " but WEBRTC_ENABLE_PROTOBUF is false.\n"
                       "Ignoring settings.";
  return std::nullopt;
#endif  // WEBRTC_ENABLE_PROTOBUF
}
}  // namespace

std::vector<RtpExtension> GetAudioRtpExtensions(
    const AudioStreamConfig& config) {
  std::vector<RtpExtension> extensions;
  if (config.stream.in_bandwidth_estimation) {
    extensions.push_back({RtpExtension::kTransportSequenceNumberUri,
                          kTransportSequenceNumberExtensionId});
  }
  if (config.stream.abs_send_time) {
    extensions.push_back(
        {RtpExtension::kAbsSendTimeUri, kAbsSendTimeExtensionId});
  }
  return extensions;
}

SendAudioStream::SendAudioStream(
    CallClient* sender,
    AudioStreamConfig config,
    scoped_refptr<AudioEncoderFactory> encoder_factory,
    Transport* send_transport)
    : sender_(sender), config_(config) {
  AudioSendStream::Config send_config(send_transport);
  ssrc_ = sender->GetNextAudioSsrc();
  send_config.rtp.ssrc = ssrc_;
  CodecParameterMap sdp_params;
  if (config.source.channels == 2)
    sdp_params["stereo"] = "1";
  if (config.encoder.initial_frame_length != TimeDelta::Millis(20))
    sdp_params["ptime"] =
        std::to_string(config.encoder.initial_frame_length.ms());
  if (config.encoder.enable_dtx)
    sdp_params["usedtx"] = "1";

  // SdpAudioFormat::num_channels indicates that the encoder is capable of
  // stereo, but the actual channel count used is based on the "stereo"
  // parameter.
  send_config.send_codec_spec = AudioSendStream::Config::SendCodecSpec(
      VideoTestConstants::kAudioSendPayloadType,
      {"opus", 48000, 2, sdp_params});
  RTC_DCHECK_LE(config.source.channels, 2);
  send_config.encoder_factory = encoder_factory;

  bool use_fixed_rate = !config.encoder.min_rate && !config.encoder.max_rate;
  if (use_fixed_rate)
    send_config.send_codec_spec->target_bitrate_bps =
        config.encoder.fixed_rate.bps();
  if (!config.adapt.binary_proto.empty()) {
    send_config.audio_network_adaptor_config = config.adapt.binary_proto;
  } else if (config.network_adaptation) {
    send_config.audio_network_adaptor_config =
        CreateAdaptationString(config.adapt);
  }
  if (config.encoder.allocate_bitrate ||
      config.stream.in_bandwidth_estimation) {
    DataRate min_rate = DataRate::Infinity();
    DataRate max_rate = DataRate::Infinity();
    if (use_fixed_rate) {
      min_rate = config.encoder.fixed_rate;
      max_rate = config.encoder.fixed_rate;
    } else {
      min_rate = *config.encoder.min_rate;
      max_rate = *config.encoder.max_rate;
    }
    send_config.min_bitrate_bps = min_rate.bps();
    send_config.max_bitrate_bps = max_rate.bps();
  }

  send_config.rtp.extensions = GetAudioRtpExtensions(config);

  sender_->SendTask([&] {
    send_stream_ = sender_->call_->CreateAudioSendStream(send_config);
    sender->call_->OnAudioTransportOverheadChanged(
        sender_->transport_->packet_overhead().bytes());
  });
}

SendAudioStream::~SendAudioStream() {
  sender_->SendTask(
      [this] { sender_->call_->DestroyAudioSendStream(send_stream_); });
}

void SendAudioStream::Start() {
  sender_->SendTask([this] {
    send_stream_->Start();
    sender_->call_->SignalChannelNetworkState(MediaType::AUDIO, kNetworkUp);
  });
}

void SendAudioStream::Stop() {
  sender_->SendTask([this] { send_stream_->Stop(); });
}

void SendAudioStream::SetMuted(bool mute) {
  sender_->SendTask([this, mute] { send_stream_->SetMuted(mute); });
}

ColumnPrinter SendAudioStream::StatsPrinter() {
  return ColumnPrinter::Lambda(
      "audio_target_rate",
      [this](SimpleStringBuilder& sb) {
        sender_->SendTask([this, &sb] {
          AudioSendStream::Stats stats = send_stream_->GetStats();
          sb.AppendFormat("%.0lf", stats.target_bitrate_bps / 8.0);
        });
      },
      64);
}

ReceiveAudioStream::ReceiveAudioStream(
    CallClient* receiver,
    AudioStreamConfig config,
    SendAudioStream* send_stream,
    scoped_refptr<AudioDecoderFactory> decoder_factory,
    Transport* feedback_transport)
    : receiver_(receiver), config_(config) {
  AudioReceiveStreamInterface::Config recv_config;
  recv_config.rtp.local_ssrc = receiver_->GetNextAudioLocalSsrc();
  recv_config.rtcp_send_transport = feedback_transport;
  recv_config.rtp.remote_ssrc = send_stream->ssrc_;
  receiver->ssrc_media_types_[recv_config.rtp.remote_ssrc] = MediaType::AUDIO;
  recv_config.decoder_factory = decoder_factory;
  recv_config.decoder_map = {
      {VideoTestConstants::kAudioSendPayloadType, {"opus", 48000, 2}}};
  recv_config.sync_group = config.render.sync_group;
  receiver_->SendTask([&] {
    receive_stream_ = receiver_->call_->CreateAudioReceiveStream(recv_config);
  });
}
ReceiveAudioStream::~ReceiveAudioStream() {
  receiver_->SendTask(
      [&] { receiver_->call_->DestroyAudioReceiveStream(receive_stream_); });
}

void ReceiveAudioStream::Start() {
  receiver_->SendTask([&] {
    receive_stream_->Start();
    receiver_->call_->SignalChannelNetworkState(MediaType::AUDIO, kNetworkUp);
  });
}

void ReceiveAudioStream::Stop() {
  receiver_->SendTask([&] { receive_stream_->Stop(); });
}

AudioReceiveStreamInterface::Stats ReceiveAudioStream::GetStats() const {
  AudioReceiveStreamInterface::Stats result;
  receiver_->SendTask([&] {
    result = receive_stream_->GetStats(/*get_and_clear_legacy_stats=*/true);
  });
  return result;
}

AudioStreamPair::~AudioStreamPair() = default;

AudioStreamPair::AudioStreamPair(
    CallClient* sender,
    scoped_refptr<AudioEncoderFactory> encoder_factory,
    CallClient* receiver,
    scoped_refptr<AudioDecoderFactory> decoder_factory,
    AudioStreamConfig config)
    : config_(config),
      send_stream_(sender, config, encoder_factory, sender->transport_.get()),
      receive_stream_(receiver,
                      config,
                      &send_stream_,
                      decoder_factory,
                      receiver->transport_.get()) {}

}  // namespace test
}  // namespace webrtc
