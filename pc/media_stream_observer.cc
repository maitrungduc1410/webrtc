/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/media_stream_observer.h"

#include <functional>
#include <utility>

#include "absl/algorithm/container.h"
#include "api/media_stream_interface.h"
#include "api/sequence_checker.h"
#include "rtc_base/weak_ptr.h"

namespace webrtc {
namespace {

// Returns the tracks in `tracks` that have no track with the same id in
// `other`.
template <typename TrackVector>
TrackVector TracksNotIn(const TrackVector& tracks, const TrackVector& other) {
  TrackVector result;
  for (const auto& track : tracks) {
    if (absl::c_none_of(other, [&track](const auto& other_track) {
          return other_track->id() == track->id();
        })) {
      result.push_back(track);
    }
  }
  return result;
}

}  // namespace

MediaStreamObserver::MediaStreamObserver(
    MediaStreamInterface* stream,
    std::function<void(AudioTrackInterface*, MediaStreamInterface*)>
        audio_track_added_callback,
    std::function<void(AudioTrackInterface*, MediaStreamInterface*)>
        audio_track_removed_callback,
    std::function<void(VideoTrackInterface*, MediaStreamInterface*)>
        video_track_added_callback,
    std::function<void(VideoTrackInterface*, MediaStreamInterface*)>
        video_track_removed_callback)
    : stream_(stream),
      cached_audio_tracks_(stream->GetAudioTracks()),
      cached_video_tracks_(stream->GetVideoTracks()),
      audio_track_added_callback_(std::move(audio_track_added_callback)),
      audio_track_removed_callback_(std::move(audio_track_removed_callback)),
      video_track_added_callback_(std::move(video_track_added_callback)),
      video_track_removed_callback_(std::move(video_track_removed_callback)) {
  stream_->RegisterObserver(this);
}

MediaStreamObserver::~MediaStreamObserver() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  stream_->UnregisterObserver(this);
}

void MediaStreamObserver::OnChanged() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  AudioTrackVector new_audio_tracks = stream_->GetAudioTracks();
  VideoTrackVector new_video_tracks = stream_->GetVideoTracks();

  // Find the changes and update the cached tracks before running callbacks,
  // since a callback may change the stream again or destroy this object.
  AudioTrackVector removed_audio_tracks =
      TracksNotIn(cached_audio_tracks_, new_audio_tracks);
  AudioTrackVector added_audio_tracks =
      TracksNotIn(new_audio_tracks, cached_audio_tracks_);
  VideoTrackVector removed_video_tracks =
      TracksNotIn(cached_video_tracks_, new_video_tracks);
  VideoTrackVector added_video_tracks =
      TracksNotIn(new_video_tracks, cached_video_tracks_);
  cached_audio_tracks_ = std::move(new_audio_tracks);
  cached_video_tracks_ = std::move(new_video_tracks);

  WeakPtr<MediaStreamObserver> self = weak_ptr_factory_.GetWeakPtr();
  // Runs `callback` for each of `tracks`. Returns false if a callback destroyed
  // this object, after which no member may be accessed.
  auto run_callbacks = [&](const auto& tracks, const auto& callback) {
    for (const auto& track : tracks) {
      callback(track.get(), stream_.get());
      if (!self) {
        return false;
      }
    }
    return true;
  };
  if (run_callbacks(removed_audio_tracks, audio_track_removed_callback_) &&
      run_callbacks(added_audio_tracks, audio_track_added_callback_) &&
      run_callbacks(removed_video_tracks, video_track_removed_callback_)) {
    run_callbacks(added_video_tracks, video_track_added_callback_);
  }
}

}  // namespace webrtc
