/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/media_stream_observer.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "api/make_ref_counted.h"
#include "api/media_stream_interface.h"
#include "api/scoped_refptr.h"
#include "api/test/mock_media_stream_interface.h"
#include "pc/audio_track.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::NiceMock;
using ::testing::Return;

class MediaStreamObserverTest : public ::testing::Test {
 protected:
  MediaStreamObserverTest() {
    ON_CALL(*stream_, GetAudioTracks()).WillByDefault([this] {
      return audio_tracks_;
    });
    ON_CALL(*stream_, GetVideoTracks())
        .WillByDefault(Return(VideoTrackVector()));
  }

  // Creates an observer that runs `on_audio_track_added` for added audio
  // tracks.
  std::unique_ptr<MediaStreamObserver> CreateObserver(
      std::function<void(AudioTrackInterface*)> on_audio_track_added) {
    auto ignore = [](auto* /*track*/, MediaStreamInterface* /*stream*/) {};
    return std::make_unique<MediaStreamObserver>(
        stream_.get(),
        [on_audio_track_added](AudioTrackInterface* track,
                               MediaStreamInterface* /*stream*/) {
          on_audio_track_added(track);
        },
        ignore, ignore, ignore);
  }

  const scoped_refptr<NiceMock<MockMediaStream>> stream_ =
      make_ref_counted<NiceMock<MockMediaStream>>();
  AudioTrackVector audio_tracks_;
};

TEST_F(MediaStreamObserverTest, CallbackCanDestroyObserver) {
  std::vector<std::string> added_track_ids;
  std::unique_ptr<MediaStreamObserver> observer;
  observer = CreateObserver([&](AudioTrackInterface* track) {
    added_track_ids.push_back(track->id());
    observer = nullptr;
  });

  audio_tracks_ = {AudioTrack::Create("audio1", nullptr),
                   AudioTrack::Create("audio2", nullptr)};
  observer->OnChanged();
  // No callbacks run after the observer is destroyed.
  EXPECT_EQ(observer, nullptr);
  EXPECT_THAT(added_track_ids, ElementsAre("audio1"));
}

TEST_F(MediaStreamObserverTest, NestedNotificationCanDestroyObserver) {
  std::vector<std::string> added_track_ids;
  std::unique_ptr<MediaStreamObserver> observer;
  observer = CreateObserver([&](AudioTrackInterface* track) {
    added_track_ids.push_back(track->id());
    if (track->id() == "audio1") {
      // Change the stream again from the callback.
      audio_tracks_.push_back(AudioTrack::Create("audio3", nullptr));
      observer->OnChanged();
    } else {
      observer = nullptr;
    }
  });

  audio_tracks_ = {AudioTrack::Create("audio1", nullptr),
                   AudioTrack::Create("audio2", nullptr)};
  observer->OnChanged();
  // The outer notification stops once the nested one destroyed the observer.
  EXPECT_EQ(observer, nullptr);
  EXPECT_THAT(added_track_ids, ElementsAre("audio1", "audio3"));
}

}  // namespace
}  // namespace webrtc
