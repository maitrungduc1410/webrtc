/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_SIGNAL_PROCESSING_DOWNSAMPLE_FAST_H_
#define COMMON_AUDIO_SIGNAL_PROCESSING_DOWNSAMPLE_FAST_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Performs a MA down sampling filter on a vector.
// Input:
//      - data_in            : Input samples (state in positions
//                               data_in[-order] .. data_in[-1])
//      - data_in_length     : Number of samples in `data_in` to be filtered.
//                               This must be at least
//                               `delay` + `factor`*(`out_vector_length`-1) + 1)
//      - data_out_length    : Number of down sampled samples desired
//      - coefficients       : Filter coefficients (in Q12)
//      - coefficients_length: Number of coefficients (order+1)
//      - factor             : Decimation factor
//      - delay              : Delay of filter (compensated for in out_vector)
// Output:
//      - data_out           : Filtered samples
// Return value              : 0 if OK, -1 if `in_vector` is too short
int WebRtcSpl_DownsampleFast(const int16_t* data_in,
                             size_t data_in_length,
                             int16_t* data_out,
                             size_t data_out_length,
                             const int16_t* __restrict coefficients,
                             size_t coefficients_length,
                             int factor,
                             size_t delay);
int WebRtcSpl_DownsampleFastC(const int16_t* data_in,
                              size_t data_in_length,
                              int16_t* data_out,
                              size_t data_out_length,
                              const int16_t* __restrict coefficients,
                              size_t coefficients_length,
                              int factor,
                              size_t delay);
#if defined(WEBRTC_HAS_NEON)
int WebRtcSpl_DownsampleFastNeon(const int16_t* data_in,
                                 size_t data_in_length,
                                 int16_t* data_out,
                                 size_t data_out_length,
                                 const int16_t* __restrict coefficients,
                                 size_t coefficients_length,
                                 int factor,
                                 size_t delay);
#elif defined(MIPS32_LE)
int WebRtcSpl_DownsampleFast_mips(const int16_t* data_in,
                                  size_t data_in_length,
                                  int16_t* data_out,
                                  size_t data_out_length,
                                  const int16_t* __restrict coefficients,
                                  size_t coefficients_length,
                                  int factor,
                                  size_t delay);
#endif

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // COMMON_AUDIO_SIGNAL_PROCESSING_DOWNSAMPLE_FAST_H_
