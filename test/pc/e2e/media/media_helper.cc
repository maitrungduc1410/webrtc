/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/pc/e2e/media/media_helper.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "api/make_ref_counted.h"
#include "api/media_stream_interface.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/rtp_sender_interface.h"
#include "api/scoped_refptr.h"
#include "api/test/frame_generator_interface.h"
#include "api/test/pclf/media_configuration.h"
#include "api/test/pclf/media_quality_test_params.h"
#include "api/test/pclf/peer_configurer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "test/frame_generator_capturer.h"
#include "test/pc/e2e/media/test_video_capturer_video_track_source.h"
#include "test/pc/e2e/test_peer.h"
#include "test/platform_video_capturer.h"
#include "test/test_video_capturer.h"

namespace webrtc {
namespace webrtc_pc_e2e {

namespace {

bool IsScreencast(const VideoConfig& video_config) {
  return video_config.content_hint == VideoTrackInterface::ContentHint::kText ||
         video_config.content_hint ==
             VideoTrackInterface::ContentHint::kDetailed;
}

}  // namespace

void MediaHelper::MaybeAddAudio(TestPeer* peer) {
  if (!peer->params().audio_config) {
    return;
  }
  const AudioConfig& audio_config = peer->params().audio_config.value();
  scoped_refptr<AudioSourceInterface> source =
      peer->pc_factory()->CreateAudioSource(audio_config.audio_options);
  scoped_refptr<AudioTrackInterface> track =
      peer->pc_factory()->CreateAudioTrack(*audio_config.stream_label,
                                           source.get());
  std::string sync_group = audio_config.sync_group
                               ? audio_config.sync_group.value()
                               : audio_config.stream_label.value() + "-sync";
  peer->AddTrack(track, {sync_group, *audio_config.stream_label});
}

std::vector<scoped_refptr<TestVideoCapturerVideoTrackSource>>
MediaHelper::MaybeAddVideo(TestPeer* peer) {
  // Params here valid because of pre-run validation.
  const Params& params = peer->params();
  const ConfigurableParams& configurable_params = peer->configurable_params();
  std::vector<scoped_refptr<TestVideoCapturerVideoTrackSource>> out;
  for (size_t i = 0; i < configurable_params.video_configs.size(); ++i) {
    const VideoConfig& video_config = configurable_params.video_configs[i];
    // Setup input video source into peer connection.
    std::unique_ptr<test::TestVideoCapturer> capturer = CreateVideoCapturer(
        video_config, peer->ReleaseVideoSource(i),
        video_quality_analyzer_injection_helper_->CreateFramePreprocessor(
            params.name.value(), video_config));
    scoped_refptr<TestVideoCapturerVideoTrackSource> source =
        make_ref_counted<TestVideoCapturerVideoTrackSource>(
            std::move(capturer), IsScreencast(video_config),
            video_config.stream_label);
    out.push_back(source);
    RTC_LOG(LS_INFO) << "Adding video with video_config.stream_label="
                     << video_config.stream_label.value();
    scoped_refptr<VideoTrackInterface> track =
        peer->pc_factory()->CreateVideoTrack(source,
                                             video_config.stream_label.value());
    if (video_config.content_hint.has_value()) {
      track->set_content_hint(video_config.content_hint.value());
    }
    std::string sync_group = video_config.sync_group
                                 ? video_config.sync_group.value()
                                 : video_config.stream_label.value() + "-sync";
    RTCErrorOr<scoped_refptr<RtpSenderInterface>> sender =
        peer->AddTrack(track, {sync_group, *video_config.stream_label});
    RTC_CHECK(sender.ok());
    if (video_config.temporal_layers_count ||
        video_config.degradation_preference) {
      RtpParameters rtp_parameters = sender.value()->GetParameters();
      if (video_config.temporal_layers_count) {
        for (auto& encoding_parameters : rtp_parameters.encodings) {
          encoding_parameters.num_temporal_layers =
              video_config.temporal_layers_count;
        }
      }
      if (video_config.degradation_preference) {
        rtp_parameters.degradation_preference =
            video_config.degradation_preference;
      }
      RTCError res = sender.value()->SetParameters(rtp_parameters);
      RTC_CHECK(res.ok()) << "Failed to set RTP parameters";
    }
  }
  return out;
}

std::unique_ptr<test::TestVideoCapturer> MediaHelper::CreateVideoCapturer(
    const VideoConfig& video_config,
    PeerConfigurer::VideoSource source,
    std::unique_ptr<test::TestVideoCapturer::FramePreprocessor>
        frame_preprocessor) {
  CapturingDeviceIndex* capturing_device_index =
      std::get_if<CapturingDeviceIndex>(&source);
  if (capturing_device_index != nullptr) {
    std::unique_ptr<test::TestVideoCapturer> capturer =
        test::CreateVideoCapturer(video_config.width, video_config.height,
                                  video_config.fps,
                                  static_cast<size_t>(*capturing_device_index));
    RTC_CHECK(capturer)
        << "Failed to obtain input stream from capturing device #"
        << *capturing_device_index;
    capturer->SetFramePreprocessor(std::move(frame_preprocessor));
    return capturer;
  }

  auto capturer = std::make_unique<test::FrameGeneratorCapturer>(
      clock_,
      std::get<std::unique_ptr<test::FrameGeneratorInterface>>(
          std::move(source)),
      video_config.fps, *task_queue_factory_, IsScreencast(video_config));
  capturer->SetFramePreprocessor(std::move(frame_preprocessor));
  capturer->Init();
  return capturer;
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
