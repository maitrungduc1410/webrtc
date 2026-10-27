/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "sdk/android/src/jni/logging/log_sink.h"

#include <jni.h>

#include "rtc_base/logging.h"
#include "sdk/android/generated_logging_jni/JNILogging_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/native_api/jni/scoped_java_ref.h"
#include "sdk/android/src/jni/jvm.h"

namespace webrtc {
namespace jni {

JNILogSink::JNILogSink(JNIEnv* env, const JavaRef<jobject>& j_logging)
    : j_logging_(env, j_logging) {}
JNILogSink::~JNILogSink() = default;

void JNILogSink::OnLogMessage(const LogLineRef& line) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_JNILogging_logToInjectable(
      env, j_logging_, NativeToJavaString(env, line.DefaultLogLine()),
      NativeToJavaInteger(env, line.severity()),
      NativeToJavaString(env, line.tag()));
}

}  // namespace jni
}  // namespace webrtc

DEFINE_JNI(JNILogging)
