/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_SIGNAL_PROCESSING_CROSS_CORRELATION_H_
#define COMMON_AUDIO_SIGNAL_PROCESSING_CROSS_CORRELATION_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Calculates the cross-correlation between two sequences `seq1` and `seq2`.
// `seq1` is fixed and `seq2` slides as the pointer is increased with the
// amount `step_seq2`. Note the arguments should obey the relationship:
// `dim_seq` - 1 + `step_seq2` * (`dim_cross_correlation` - 1) <
//      buffer size of `seq2`
//
// Input:
//      - seq1           : First sequence (fixed throughout the correlation)
//      - seq2           : Second sequence (slides `step_vector2` for each
//                            new correlation)
//      - dim_seq        : Number of samples to use in the cross-correlation
//      - dim_cross_correlation : Number of cross-correlations to calculate (the
//                            start position for `vector2` is updated for each
//                            new one)
//      - right_shifts   : Number of right bit shifts to use. This will
//                            become the output Q-domain.
//      - step_seq2      : How many (positive or negative) steps the
//                            `vector2` pointer should be updated for each new
//                            cross-correlation value.
//
// Output:
//      - cross_correlation : The cross-correlation in Q(-right_shifts)
void WebRtcSpl_CrossCorrelation(int32_t* cross_correlation,
                                const int16_t* seq1,
                                const int16_t* seq2,
                                size_t dim_seq,
                                size_t dim_cross_correlation,
                                int right_shifts,
                                int step_seq2);
void WebRtcSpl_CrossCorrelationC(int32_t* cross_correlation,
                                 const int16_t* seq1,
                                 const int16_t* seq2,
                                 size_t dim_seq,
                                 size_t dim_cross_correlation,
                                 int right_shifts,
                                 int step_seq2);
#if defined(WEBRTC_HAS_NEON)
void WebRtcSpl_CrossCorrelationNeon(int32_t* cross_correlation,
                                    const int16_t* seq1,
                                    const int16_t* seq2,
                                    size_t dim_seq,
                                    size_t dim_cross_correlation,
                                    int right_shifts,
                                    int step_seq2);
#elif defined(MIPS32_LE)
void WebRtcSpl_CrossCorrelation_mips(int32_t* cross_correlation,
                                     const int16_t* seq1,
                                     const int16_t* seq2,
                                     size_t dim_seq,
                                     size_t dim_cross_correlation,
                                     int right_shifts,
                                     int step_seq2);
#endif

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // COMMON_AUDIO_SIGNAL_PROCESSING_CROSS_CORRELATION_H_
