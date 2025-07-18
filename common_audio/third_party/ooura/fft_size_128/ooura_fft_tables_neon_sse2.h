/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_UTILITY_OOURA_FFT_TABLES_NEON_SSE2_H_
#define MODULES_AUDIO_PROCESSING_UTILITY_OOURA_FFT_TABLES_NEON_SSE2_H_

#include "common_audio/third_party/ooura/fft_size_128/ooura_fft.h" // IWYU pragma: keep
#include "rtc_base/system/arch.h"

#ifdef _MSC_VER /* visual c++ */
#define ALIGN16_BEG __declspec(align(16))
#define ALIGN16_END
#else /* gcc or icc */
#define ALIGN16_BEG
#define ALIGN16_END __attribute__((aligned(16)))
#endif

namespace webrtc {

// These tables used to be computed at run-time. For example, refer to:
// https://code.google.com/p/webrtc/source/browse/trunk/webrtc/modules/audio_processing/utility/apm_rdft.c?r=6564
// to see the initialization code.
#if defined(WEBRTC_ARCH_X86_FAMILY) || defined(WEBRTC_HAS_NEON)
// Constants used by SSE2 and NEON but initialized in the C path.
const ALIGN16_BEG float ALIGN16_END k_swap_sign[4] = {-1.f, 1.f, -1.f, 1.f};

ALIGN16_BEG const float ALIGN16_END rdft_wk1r[32] = {
    1.000000000f, 1.000000000f, 0.707106769f, 0.707106769f, 0.923879564f,
    0.923879564f, 0.382683456f, 0.382683456f, 0.980785251f, 0.980785251f,
    0.555570245f, 0.555570245f, 0.831469595f, 0.831469595f, 0.195090324f,
    0.195090324f, 0.995184720f, 0.995184720f, 0.634393334f, 0.634393334f,
    0.881921291f, 0.881921291f, 0.290284663f, 0.290284663f, 0.956940353f,
    0.956940353f, 0.471396744f, 0.471396744f, 0.773010433f, 0.773010433f,
    0.098017141f, 0.098017141f,
};
ALIGN16_BEG const float ALIGN16_END rdft_wk2r[32] = {
    1.000000000f,  1.000000000f,  -0.000000000f, -0.000000000f, 0.707106769f,
    0.707106769f,  -0.707106769f, -0.707106769f, 0.923879564f,  0.923879564f,
    -0.382683456f, -0.382683456f, 0.382683456f,  0.382683456f,  -0.923879564f,
    -0.923879564f, 0.980785251f,  0.980785251f,  -0.195090324f, -0.195090324f,
    0.555570245f,  0.555570245f,  -0.831469595f, -0.831469595f, 0.831469595f,
    0.831469595f,  -0.555570245f, -0.555570245f, 0.195090324f,  0.195090324f,
    -0.980785251f, -0.980785251f,
};
ALIGN16_BEG const float ALIGN16_END rdft_wk3r[32] = {
    1.000000000f,  1.000000000f,  -0.707106769f, -0.707106769f, 0.382683456f,
    0.382683456f,  -0.923879564f, -0.923879564f, 0.831469536f,  0.831469536f,
    -0.980785251f, -0.980785251f, -0.195090353f, -0.195090353f, -0.555570245f,
    -0.555570245f, 0.956940353f,  0.956940353f,  -0.881921172f, -0.881921172f,
    0.098017156f,  0.098017156f,  -0.773010492f, -0.773010492f, 0.634393334f,
    0.634393334f,  -0.995184720f, -0.995184720f, -0.471396863f, -0.471396863f,
    -0.290284693f, -0.290284693f,
};
ALIGN16_BEG const float ALIGN16_END rdft_wk1i[32] = {
    -0.000000000f, 0.000000000f,  -0.707106769f, 0.707106769f,  -0.382683456f,
    0.382683456f,  -0.923879564f, 0.923879564f,  -0.195090324f, 0.195090324f,
    -0.831469595f, 0.831469595f,  -0.555570245f, 0.555570245f,  -0.980785251f,
    0.980785251f,  -0.098017141f, 0.098017141f,  -0.773010433f, 0.773010433f,
    -0.471396744f, 0.471396744f,  -0.956940353f, 0.956940353f,  -0.290284663f,
    0.290284663f,  -0.881921291f, 0.881921291f,  -0.634393334f, 0.634393334f,
    -0.995184720f, 0.995184720f,
};
ALIGN16_BEG const float ALIGN16_END rdft_wk2i[32] = {
    -0.000000000f, 0.000000000f,  -1.000000000f, 1.000000000f,  -0.707106769f,
    0.707106769f,  -0.707106769f, 0.707106769f,  -0.382683456f, 0.382683456f,
    -0.923879564f, 0.923879564f,  -0.923879564f, 0.923879564f,  -0.382683456f,
    0.382683456f,  -0.195090324f, 0.195090324f,  -0.980785251f, 0.980785251f,
    -0.831469595f, 0.831469595f,  -0.555570245f, 0.555570245f,  -0.555570245f,
    0.555570245f,  -0.831469595f, 0.831469595f,  -0.980785251f, 0.980785251f,
    -0.195090324f, 0.195090324f,
};
ALIGN16_BEG const float ALIGN16_END rdft_wk3i[32] = {
    -0.000000000f, 0.000000000f,  -0.707106769f, 0.707106769f,  -0.923879564f,
    0.923879564f,  0.382683456f,  -0.382683456f, -0.555570245f, 0.555570245f,
    -0.195090353f, 0.195090353f,  -0.980785251f, 0.980785251f,  0.831469536f,
    -0.831469536f, -0.290284693f, 0.290284693f,  -0.471396863f, 0.471396863f,
    -0.995184720f, 0.995184720f,  0.634393334f,  -0.634393334f, -0.773010492f,
    0.773010492f,  0.098017156f,  -0.098017156f, -0.881921172f, 0.881921172f,
    0.956940353f,  -0.956940353f,
};
ALIGN16_BEG const float ALIGN16_END cftmdl_wk1r[4] = {
    0.707106769f,
    0.707106769f,
    0.707106769f,
    -0.707106769f,
};
#endif

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_UTILITY_OOURA_FFT_TABLES_NEON_SSE2_H_
