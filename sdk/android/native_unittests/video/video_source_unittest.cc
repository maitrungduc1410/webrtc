/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/native_api/video/video_source.h"

#include <jni.h>

#include <cstdint>
#include <vector>

#include "api/media_stream_interface.h"
#include "api/scoped_refptr.h"
#include "api/video/video_frame.h"
#include "api/video/video_rotation.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "rtc_base/thread.h"
#include "sdk/android/generated_native_unittests_jni/JavaVideoSourceTestHelper_jni.h"
#include "sdk/android/native_api/jni/jvm.h"
#include "test/create_test_environment.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

namespace {
class TestVideoSink : public webrtc::VideoSinkInterface<VideoFrame> {
 public:
  void OnFrame(const VideoFrame& frame) { frames_.push_back(frame); }

  std::vector<VideoFrame> GetFrames() {
    std::vector<VideoFrame> temp = frames_;
    frames_.clear();
    return temp;
  }

 private:
  std::vector<VideoFrame> frames_;
};
}  // namespace

TEST(JavaVideoSourceTest, CreateJavaVideoSource) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  // Wrap test thread so it can be used as the signaling thread.
  webrtc::ThreadManager::Instance()->WrapCurrentThread();

  webrtc::scoped_refptr<JavaVideoTrackSourceInterface> video_track_source =
      CreateJavaVideoSource(env,
                            webrtc::ThreadManager::Instance()->CurrentThread(),
                            /*is_screencast=*/false, /*align_timestamps=*/true,
                            CreateTestEnvironment());

  ASSERT_NE(nullptr, video_track_source);
  EXPECT_NE(nullptr,
            video_track_source->GetJavaVideoCapturerObserver(env).obj());
}

TEST(JavaVideoSourceTest, OnFrameCapturedFrameIsDeliveredToSink) {
  TestVideoSink test_video_sink;

  JNIEnv* env = AttachCurrentThreadIfNeeded();
  // Wrap test thread so it can be used as the signaling thread.
  webrtc::ThreadManager::Instance()->WrapCurrentThread();

  webrtc::scoped_refptr<JavaVideoTrackSourceInterface> video_track_source =
      CreateJavaVideoSource(env,
                            webrtc::ThreadManager::Instance()->CurrentThread(),
                            /*is_screencast=*/false, /*align_timestamps=*/true,
                            CreateTestEnvironment());
  video_track_source->AddOrUpdateSink(&test_video_sink,
                                      webrtc::VideoSinkWants());

  jni::Java_JavaVideoSourceTestHelper_startCapture(
      env, video_track_source->GetJavaVideoCapturerObserver(env),
      /*success=*/true);
  const int width = 20;
  const int height = 32;
  const int rotation = 180;
  const int64_t timestamp = 987654321;
  jni::Java_JavaVideoSourceTestHelper_deliverFrame(
      env, width, height, rotation, timestamp,
      video_track_source->GetJavaVideoCapturerObserver(env));

  std::vector<VideoFrame> frames = test_video_sink.GetFrames();
  ASSERT_EQ(1u, frames.size());
  webrtc::VideoFrame frame = frames[0];
  EXPECT_EQ(width, frame.width());
  EXPECT_EQ(height, frame.height());
  EXPECT_EQ(rotation, frame.rotation());
}

TEST(JavaVideoSourceTest,
     OnFrameCapturedFrameIsDeliveredToSinkWithPreservedTimestamp) {
  TestVideoSink test_video_sink;

  JNIEnv* env = AttachCurrentThreadIfNeeded();
  // Wrap test thread so it can be used as the signaling thread.
  webrtc::ThreadManager::Instance()->WrapCurrentThread();

  webrtc::scoped_refptr<JavaVideoTrackSourceInterface> video_track_source =
      CreateJavaVideoSource(env,
                            webrtc::ThreadManager::Instance()->CurrentThread(),
                            /*is_screencast=*/false, /*align_timestamps=*/false,
                            CreateTestEnvironment());
  video_track_source->AddOrUpdateSink(&test_video_sink,
                                      webrtc::VideoSinkWants());

  jni::Java_JavaVideoSourceTestHelper_startCapture(
      env, video_track_source->GetJavaVideoCapturerObserver(env),
      /*success=*/true);
  const int width = 20;
  const int height = 32;
  const int rotation = 180;
  const int64_t timestamp = 987654321;
  jni::Java_JavaVideoSourceTestHelper_deliverFrame(
      env, width, height, rotation, 987654321,
      video_track_source->GetJavaVideoCapturerObserver(env));

  std::vector<VideoFrame> frames = test_video_sink.GetFrames();
  ASSERT_EQ(1u, frames.size());
  webrtc::VideoFrame frame = frames[0];
  EXPECT_EQ(width, frame.width());
  EXPECT_EQ(height, frame.height());
  EXPECT_EQ(rotation, frame.rotation());
  EXPECT_EQ(timestamp / 1000, frame.timestamp_us());
}

TEST(JavaVideoSourceTest, OnFrameCapturedAppliesRotationWhenSinkWantsIt) {
  TestVideoSink test_video_sink;

  JNIEnv* env = AttachCurrentThreadIfNeeded();
  // Wrap test thread so it can be used as the signaling thread.
  webrtc::ThreadManager::Instance()->WrapCurrentThread();

  webrtc::scoped_refptr<JavaVideoTrackSourceInterface> video_track_source =
      CreateJavaVideoSource(env,
                            webrtc::ThreadManager::Instance()->CurrentThread(),
                            /*is_screencast=*/false, /*align_timestamps=*/true,
                            CreateTestEnvironment());
  webrtc::VideoSinkWants wants;
  wants.rotation_applied = true;
  video_track_source->AddOrUpdateSink(&test_video_sink, wants);

  jni::Java_JavaVideoSourceTestHelper_startCapture(
      env, video_track_source->GetJavaVideoCapturerObserver(env),
      /*success=*/true);
  const int width = 20;
  const int height = 32;
  jni::Java_JavaVideoSourceTestHelper_deliverFrame(
      env, width, height, /*rotation=*/90, /*timestampNs=*/987654321,
      video_track_source->GetJavaVideoCapturerObserver(env));

  std::vector<VideoFrame> frames = test_video_sink.GetFrames();
  ASSERT_EQ(1u, frames.size());
  webrtc::VideoFrame frame = frames[0];
  EXPECT_EQ(height, frame.width());
  EXPECT_EQ(width, frame.height());
  EXPECT_EQ(kVideoRotation_0, frame.rotation());
}

TEST(JavaVideoSourceTest, CapturerStartedSuccessStateBecomesLive) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  // Wrap test thread so it can be used as the signaling thread.
  webrtc::ThreadManager::Instance()->WrapCurrentThread();

  webrtc::scoped_refptr<JavaVideoTrackSourceInterface> video_track_source =
      CreateJavaVideoSource(env,
                            webrtc::ThreadManager::Instance()->CurrentThread(),
                            /*is_screencast=*/false, /*align_timestamps=*/true,
                            CreateTestEnvironment());

  jni::Java_JavaVideoSourceTestHelper_startCapture(
      env, video_track_source->GetJavaVideoCapturerObserver(env),
      /*success=*/true);

  EXPECT_EQ(VideoTrackSourceInterface::SourceState::kLive,
            video_track_source->state());
}

TEST(JavaVideoSourceTest, CapturerStartedFailureStateBecomesEnded) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  // Wrap test thread so it can be used as the signaling thread.
  webrtc::ThreadManager::Instance()->WrapCurrentThread();

  webrtc::scoped_refptr<JavaVideoTrackSourceInterface> video_track_source =
      CreateJavaVideoSource(env,
                            webrtc::ThreadManager::Instance()->CurrentThread(),
                            /*is_screencast=*/false, /*align_timestamps=*/true,
                            CreateTestEnvironment());

  jni::Java_JavaVideoSourceTestHelper_startCapture(
      env, video_track_source->GetJavaVideoCapturerObserver(env),
      /*success=*/false);

  EXPECT_EQ(VideoTrackSourceInterface::SourceState::kEnded,
            video_track_source->state());
}

TEST(JavaVideoSourceTest, CapturerStoppedStateBecomesEnded) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  // Wrap test thread so it can be used as the signaling thread.
  webrtc::ThreadManager::Instance()->WrapCurrentThread();

  webrtc::scoped_refptr<JavaVideoTrackSourceInterface> video_track_source =
      CreateJavaVideoSource(env,
                            webrtc::ThreadManager::Instance()->CurrentThread(),
                            /*is_screencast=*/false, /*align_timestamps=*/true,
                            CreateTestEnvironment());

  jni::Java_JavaVideoSourceTestHelper_startCapture(
      env, video_track_source->GetJavaVideoCapturerObserver(env),
      /*success=*/true);
  jni::Java_JavaVideoSourceTestHelper_stopCapture(
      env, video_track_source->GetJavaVideoCapturerObserver(env));

  EXPECT_EQ(VideoTrackSourceInterface::SourceState::kEnded,
            video_track_source->state());
}

}  // namespace test
}  // namespace webrtc

DEFINE_JNI(JavaVideoSourceTestHelper)
