/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/pc/logging.h"

#include <jni.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "rtc_base/base_java_jni/Logging_jni.h"
#include "rtc_base/logging.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/native_api/jni/scoped_java_ref.h"
#include "sdk/android/src/jni/jvm.h"
#include "third_party/jni_zero/jni_zero.h"

namespace webrtc {
namespace jni {

namespace {

// Passes log messages to a Java Loggable that was added with
// LoggingConfig.Builder.addSink().
class LoggableSink : public LogSink {
 public:
  LoggableSink(JNIEnv* jni, const jni_zero::JavaRef<jobject>& j_loggable)
      : j_loggable_(jni, j_loggable) {}

  void OnLogMessage(const LogLineRef& line) override {
    JNIEnv* jni = AttachCurrentThreadIfNeeded();
    Java_Logging_onNativeLogMessage(
        jni, j_loggable_, NativeToJavaString(jni, line.DefaultLogLine()),
        jint{line.severity()}, NativeToJavaString(jni, line.tag()));
  }

 private:
  const ScopedJavaGlobalRef<jobject> j_loggable_;
};

}  // namespace

void NotifyLoggingNativeLibraryLoaded(JNIEnv* jni) {
  Java_Logging_onNativeLibraryLoaded(jni);
}

static jboolean JNI_Logging_Initialize(
    JNIEnv* jni,
    const jni_zero::JavaRef<jobject>& j_debug_severity,
    const jni_zero::JavaRef<jobject>& j_min_severity,
    jboolean log_threads,
    jboolean log_timestamps,
    const jni_zero::JavaRef<jobjectArray>& j_sinks) {
  // Severities that are null in Java keep the default of LoggingConfig.
  LoggingConfig config;
  if (std::optional<int32_t> severity =
          JavaToNativeOptionalInt(jni, j_debug_severity)) {
    config.set_debug_severity(static_cast<LoggingSeverity>(*severity));
  }
  if (std::optional<int32_t> severity =
          JavaToNativeOptionalInt(jni, j_min_severity)) {
    config.set_min_severity(static_cast<LoggingSeverity>(*severity));
  }
  config.set_log_thread(log_threads);
  config.set_log_timestamp(log_timestamps);
  const jsize sink_count = jni->GetArrayLength(j_sinks.obj());
  for (jsize i = 0; i < sink_count; ++i) {
    config.AddSink(std::make_unique<LoggableSink>(
        jni, jni_zero::ScopedJavaLocalRef<jobject>::Adopt(
                 jni, jni->GetObjectArrayElement(j_sinks.obj(), i))));
  }
  return InitializeLogging(std::move(config));
}

static void JNI_Logging_Log(JNIEnv* jni,
                            jint j_severity,
                            const jni_zero::JavaRef<jstring>& j_tag,
                            const jni_zero::JavaRef<jstring>& j_message) {
  const LoggingSeverity severity = static_cast<LoggingSeverity>(j_severity);
  // Skip the string conversions for messages that no output receives.
  if (LogMessage::IsNoop(severity)) {
    return;
  }
  std::string message = JavaToNativeString(jni, j_message);
  std::string tag = JavaToNativeString(jni, j_tag);
  // `tag` must outlive `log_message`, which refers to it.
  LogMessage log_message(/*file=*/nullptr, /*line=*/0, severity);
  log_message.AddTag(tag.c_str());
  log_message.stream() << message;
}

}  // namespace jni
}  // namespace webrtc

DEFINE_JNI(Logging)
