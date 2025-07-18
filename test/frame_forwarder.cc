/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/frame_forwarder.h"

#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "rtc_base/checks.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {
namespace test {

FrameForwarder::FrameForwarder() : sink_(nullptr) {}
FrameForwarder::~FrameForwarder() {}

void FrameForwarder::IncomingCapturedFrame(const VideoFrame& video_frame) {
  MutexLock lock(&mutex_);
  if (sink_)
    sink_->OnFrame(video_frame);
}

void FrameForwarder::AddOrUpdateSink(VideoSinkInterface<VideoFrame>* sink,
                                     const VideoSinkWants& wants) {
  MutexLock lock(&mutex_);
  AddOrUpdateSinkLocked(sink, wants);
}

void FrameForwarder::AddOrUpdateSinkLocked(VideoSinkInterface<VideoFrame>* sink,
                                           const VideoSinkWants& wants) {
  RTC_DCHECK(!sink_ || sink_ == sink);
  sink_ = sink;
  sink_wants_ = wants;
}

void FrameForwarder::RemoveSink(VideoSinkInterface<VideoFrame>* sink) {
  MutexLock lock(&mutex_);
  RTC_DCHECK_EQ(sink, sink_);
  sink_ = nullptr;
}

VideoSinkWants FrameForwarder::sink_wants() const {
  MutexLock lock(&mutex_);
  return sink_wants_;
}

VideoSinkWants FrameForwarder::sink_wants_locked() const {
  return sink_wants_;
}

bool FrameForwarder::has_sinks() const {
  MutexLock lock(&mutex_);
  return sink_ != nullptr;
}

}  // namespace test
}  // namespace webrtc
