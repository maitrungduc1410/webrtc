/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_PC_LOGGING_H_
#define SDK_ANDROID_SRC_JNI_PC_LOGGING_H_

#include <jni.h>

namespace webrtc {
namespace jni {

// Tells org.webrtc.Logging that the native library has been loaded. This
// applies a LoggingConfig that was passed to Logging.initialize() before the
// library was loaded. Call this once the class loader is initialized and
// before native code logs.
void NotifyLoggingNativeLibraryLoaded(JNIEnv* jni);

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_PC_LOGGING_H_
