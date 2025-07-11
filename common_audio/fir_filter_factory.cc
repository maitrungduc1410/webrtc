/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/fir_filter_factory.h"

#include <cstddef>

#include "common_audio/fir_filter_c.h"
#include "rtc_base/checks.h"
#include "rtc_base/cpu_info.h"
#include "rtc_base/system/arch.h"

#if defined(WEBRTC_HAS_NEON)
#include "common_audio/fir_filter_neon.h"
#elif defined(WEBRTC_ARCH_X86_FAMILY)
#include "common_audio/fir_filter_avx2.h"
#include "common_audio/fir_filter_sse.h"
#endif

namespace webrtc {

FIRFilter* CreateFirFilter(const float* coefficients,
                           size_t coefficients_length,
                           size_t max_input_length) {
  if (!coefficients || coefficients_length <= 0 || max_input_length <= 0) {
    RTC_DCHECK_NOTREACHED();
    return nullptr;
  }

  FIRFilter* filter = nullptr;
// If we know the minimum architecture at compile time, avoid CPU detection.
#if defined(WEBRTC_ARCH_X86_FAMILY)
  // x86 CPU detection required.
  if (cpu_info::Supports(cpu_info::ISA::kAVX2)) {
    filter =
        new FIRFilterAVX2(coefficients, coefficients_length, max_input_length);
  } else if (cpu_info::Supports(cpu_info::ISA::kSSE2)) {
    filter =
        new FIRFilterSSE2(coefficients, coefficients_length, max_input_length);
  } else {
    filter = new FIRFilterC(coefficients, coefficients_length);
  }
#elif defined(WEBRTC_HAS_NEON)
  filter =
      new FIRFilterNEON(coefficients, coefficients_length, max_input_length);
#else
  filter = new FIRFilterC(coefficients, coefficients_length);
#endif

  return filter;
}

}  // namespace webrtc
