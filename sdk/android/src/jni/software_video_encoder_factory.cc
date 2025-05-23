/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/environment/environment.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/video_encoder.h"
#include "sdk/android/generated_swcodecs_jni/SoftwareVideoEncoderFactory_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/video_codec_info.h"
#include "third_party/jni_zero/jni_zero.h"

namespace webrtc {
namespace jni {

static jlong JNI_SoftwareVideoEncoderFactory_CreateFactory(JNIEnv* env) {
  return NativeToJavaPointer(CreateBuiltinVideoEncoderFactory().release());
}

jboolean JNI_SoftwareVideoEncoderFactory_IsSupported(
    JNIEnv* env,
    jlong j_factory,
    const jni_zero::JavaParamRef<jobject>& j_info) {
  return VideoCodecInfoToSdpVideoFormat(env, j_info)
      .IsCodecInList(reinterpret_cast<VideoEncoderFactory*>(j_factory)
                         ->GetSupportedFormats());
}

jlong JNI_SoftwareVideoEncoderFactory_Create(
    JNIEnv* env,
    jlong j_factory,
    jlong j_webrtc_env_ref,
    const jni_zero::JavaParamRef<jobject>& j_info) {
  return NativeToJavaPointer(
      reinterpret_cast<VideoEncoderFactory*>(j_factory)
          ->Create(*reinterpret_cast<const Environment*>(j_webrtc_env_ref),
                   VideoCodecInfoToSdpVideoFormat(env, j_info))
          .release());
}

static jni_zero::ScopedJavaLocalRef<jobject>
JNI_SoftwareVideoEncoderFactory_GetSupportedCodecs(JNIEnv* env,
                                                   jlong j_factory) {
  auto* const native_factory =
      reinterpret_cast<VideoEncoderFactory*>(j_factory);

  return NativeToJavaList(env, native_factory->GetSupportedFormats(),
                          &jni::SdpVideoFormatToVideoCodecInfo);
}

}  // namespace jni
}  // namespace webrtc
