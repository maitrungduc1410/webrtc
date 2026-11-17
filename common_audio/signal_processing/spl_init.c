/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Some code came from common/rtcd.c in the WebM project.

#include "common_audio/signal_processing/include/signal_processing_library.h"

// TODO(bugs.webrtc.org/9553): These function pointers are useless. Refactor
// things so that we simply have a bunch of regular functions with different
// implementations for different platforms.

#if defined(WEBRTC_HAS_NEON)

const CrossCorrelation WebRtcSpl_CrossCorrelation =
    WebRtcSpl_CrossCorrelationNeon;
const DownsampleFast WebRtcSpl_DownsampleFast = WebRtcSpl_DownsampleFastNeon;
const ScaleAndAddVectorsWithRound WebRtcSpl_ScaleAndAddVectorsWithRound =
    WebRtcSpl_ScaleAndAddVectorsWithRoundC;

#elif defined(MIPS32_LE)

const CrossCorrelation WebRtcSpl_CrossCorrelation =
    WebRtcSpl_CrossCorrelation_mips;
const DownsampleFast WebRtcSpl_DownsampleFast = WebRtcSpl_DownsampleFast_mips;
const ScaleAndAddVectorsWithRound WebRtcSpl_ScaleAndAddVectorsWithRound =
#ifdef MIPS_DSP_R1_LE
    WebRtcSpl_ScaleAndAddVectorsWithRound_mips;
#else
    WebRtcSpl_ScaleAndAddVectorsWithRoundC;
#endif

#else

const CrossCorrelation WebRtcSpl_CrossCorrelation = WebRtcSpl_CrossCorrelationC;
const DownsampleFast WebRtcSpl_DownsampleFast = WebRtcSpl_DownsampleFastC;
const ScaleAndAddVectorsWithRound WebRtcSpl_ScaleAndAddVectorsWithRound =
    WebRtcSpl_ScaleAndAddVectorsWithRoundC;

#endif
