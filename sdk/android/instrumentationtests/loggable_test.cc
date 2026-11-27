/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <jni.h>

#include <string>

#include "rtc_base/logging.h"
#include "sdk/android/generated_instrumentationtests_jni/LoggableTest_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "third_party/jni_zero/jni_zero.h"

namespace webrtc {
namespace jni {

static void JNI_LoggableTest_LogInfoTestMessage(
    JNIEnv* jni,
    const jni_zero::JavaRef<jstring>& j_message) {
  std::string message = JavaToNativeString(jni, j_message);
  RTC_LOG(LS_INFO) << message;
}

static void JNI_LoggableTest_ResetLoggingForTesting(JNIEnv* jni) {
  ResetLoggingForTesting();
}

static jint JNI_LoggableTest_GetNativeDebugSeverity(JNIEnv* jni) {
  return static_cast<jint>(GetLoggingConfig().debug_severity());
}

static jint JNI_LoggableTest_GetNativeMinSeverity(JNIEnv* jni) {
  return static_cast<jint>(GetLoggingConfig().min_severity());
}

static jboolean JNI_LoggableTest_GetNativeLogThreads(JNIEnv* jni) {
  return GetLoggingConfig().log_thread();
}

static jboolean JNI_LoggableTest_GetNativeLogTimestamps(JNIEnv* jni) {
  return GetLoggingConfig().log_timestamp();
}

}  // namespace jni
}  // namespace webrtc

DEFINE_JNI(LoggableTest)
