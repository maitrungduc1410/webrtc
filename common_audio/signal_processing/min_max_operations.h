/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_SIGNAL_PROCESSING_MIN_MAX_OPERATIONS_H_
#define COMMON_AUDIO_SIGNAL_PROCESSING_MIN_MAX_OPERATIONS_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Minimum and maximum operation functions.
// Implementation in min_max_operations.c.

// Returns the largest absolute value in a signed 16-bit vector.
//
// Input:
//      - vector : 16-bit input vector.
//      - length : Number of samples in vector.
//
// Return value  : Maximum absolute value in vector.
int16_t WebRtcSpl_MaxAbsValueW16(const int16_t* vector, size_t length);
int16_t WebRtcSpl_MaxAbsValueW16C(const int16_t* vector, size_t length);
#if defined(WEBRTC_HAS_NEON)
int16_t WebRtcSpl_MaxAbsValueW16Neon(const int16_t* vector, size_t length);
#elif defined(MIPS32_LE)
int16_t WebRtcSpl_MaxAbsValueW16_mips(const int16_t* vector, size_t length);
#endif

// Returns the largest absolute value in a signed 32-bit vector.
//
// Input:
//      - vector : 32-bit input vector.
//      - length : Number of samples in vector.
//
// Return value  : Maximum absolute value in vector.
int32_t WebRtcSpl_MaxAbsValueW32(const int32_t* vector, size_t length);
int32_t WebRtcSpl_MaxAbsValueW32C(const int32_t* vector, size_t length);
#if defined(WEBRTC_HAS_NEON)
int32_t WebRtcSpl_MaxAbsValueW32Neon(const int32_t* vector, size_t length);
#elif defined(MIPS_DSP_R1_LE)
int32_t WebRtcSpl_MaxAbsValueW32_mips(const int32_t* vector, size_t length);
#endif

// Returns the maximum value of a 16-bit vector.
//
// Input:
//      - vector : 16-bit input vector.
//      - length : Number of samples in vector.
//
// Return value  : Maximum sample value in `vector`.
int16_t WebRtcSpl_MaxValueW16(const int16_t* vector, size_t length);
int16_t WebRtcSpl_MaxValueW16C(const int16_t* vector, size_t length);
#if defined(WEBRTC_HAS_NEON)
int16_t WebRtcSpl_MaxValueW16Neon(const int16_t* vector, size_t length);
#elif defined(MIPS32_LE)
int16_t WebRtcSpl_MaxValueW16_mips(const int16_t* vector, size_t length);
#endif

// Returns the maximum value of a 32-bit vector.
//
// Input:
//      - vector : 32-bit input vector.
//      - length : Number of samples in vector.
//
// Return value  : Maximum sample value in `vector`.
int32_t WebRtcSpl_MaxValueW32(const int32_t* vector, size_t length);
int32_t WebRtcSpl_MaxValueW32C(const int32_t* vector, size_t length);
#if defined(WEBRTC_HAS_NEON)
int32_t WebRtcSpl_MaxValueW32Neon(const int32_t* vector, size_t length);
#elif defined(MIPS32_LE)
int32_t WebRtcSpl_MaxValueW32_mips(const int32_t* vector, size_t length);
#endif

// Returns the minimum value of a 16-bit vector.
//
// Input:
//      - vector : 16-bit input vector.
//      - length : Number of samples in vector.
//
// Return value  : Minimum sample value in `vector`.
int16_t WebRtcSpl_MinValueW16(const int16_t* vector, size_t length);
int16_t WebRtcSpl_MinValueW16C(const int16_t* vector, size_t length);
#if defined(WEBRTC_HAS_NEON)
int16_t WebRtcSpl_MinValueW16Neon(const int16_t* vector, size_t length);
#elif defined(MIPS32_LE)
int16_t WebRtcSpl_MinValueW16_mips(const int16_t* vector, size_t length);
#endif

// Returns the minimum value of a 32-bit vector.
//
// Input:
//      - vector : 32-bit input vector.
//      - length : Number of samples in vector.
//
// Return value  : Minimum sample value in `vector`.
int32_t WebRtcSpl_MinValueW32(const int32_t* vector, size_t length);
int32_t WebRtcSpl_MinValueW32C(const int32_t* vector, size_t length);
#if defined(WEBRTC_HAS_NEON)
int32_t WebRtcSpl_MinValueW32Neon(const int32_t* vector, size_t length);
#elif defined(MIPS32_LE)
int32_t WebRtcSpl_MinValueW32_mips(const int32_t* vector, size_t length);
#endif

// Returns both the minimum and maximum values of a 16-bit vector.
//
// Input:
//      - vector : 16-bit input vector.
//      - length : Number of samples in vector.
// Output:
//      - max_val : Maximum sample value in `vector`.
//      - min_val : Minimum sample value in `vector`.
void WebRtcSpl_MinMaxW16(const int16_t* vector,
                         size_t length,
                         int16_t* min_val,
                         int16_t* max_val);
#if defined(WEBRTC_HAS_NEON)
void WebRtcSpl_MinMaxW16Neon(const int16_t* vector,
                             size_t length,
                             int16_t* min_val,
                             int16_t* max_val);
#endif

// Returns the vector index to the largest absolute value of a 16-bit vector.
//
// Input:
//      - vector : 16-bit input vector.
//      - length : Number of samples in vector.
//
// Return value  : Index to the maximum absolute value in vector.
//                 If there are multiple equal maxima, return the index of the
//                 first. -32768 will always have precedence over 32767 (despite
//                 -32768 presenting an int16 absolute value of 32767).
size_t WebRtcSpl_MaxAbsIndexW16(const int16_t* vector, size_t length);

// Returns the element with the largest absolute value of a 16-bit vector. Note
// that this function can return a negative value.
//
// Input:
//      - vector : 16-bit input vector.
//      - length : Number of samples in vector.
//
// Return value  : The element with the largest absolute value. Note that this
//                 may be a negative value.
int16_t WebRtcSpl_MaxAbsElementW16(const int16_t* vector, size_t length);

// Returns the vector index to the maximum sample value of a 16-bit vector.
//
// Input:
//      - vector : 16-bit input vector.
//      - length : Number of samples in vector.
//
// Return value  : Index to the maximum value in vector (if multiple
//                 indexes have the maximum, return the first).
size_t WebRtcSpl_MaxIndexW16(const int16_t* vector, size_t length);

// Returns the vector index to the maximum sample value of a 32-bit vector.
//
// Input:
//      - vector : 32-bit input vector.
//      - length : Number of samples in vector.
//
// Return value  : Index to the maximum value in vector (if multiple
//                 indexes have the maximum, return the first).
size_t WebRtcSpl_MaxIndexW32(const int32_t* vector, size_t length);

// Returns the vector index to the minimum sample value of a 16-bit vector.
//
// Input:
//      - vector : 16-bit input vector.
//      - length : Number of samples in vector.
//
// Return value  : Index to the minimum value in vector (if multiple
//                 indexes have the minimum, return the first).
size_t WebRtcSpl_MinIndexW16(const int16_t* vector, size_t length);

// Returns the vector index to the minimum sample value of a 32-bit vector.
//
// Input:
//      - vector : 32-bit input vector.
//      - length : Number of samples in vector.
//
// Return value  : Index to the minimum value in vector (if multiple
//                 indexes have the minimum, return the first).
size_t WebRtcSpl_MinIndexW32(const int32_t* vector, size_t length);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // COMMON_AUDIO_SIGNAL_PROCESSING_MIN_MAX_OPERATIONS_H_
