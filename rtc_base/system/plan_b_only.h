/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SYSTEM_PLAN_B_ONLY_H_
#define RTC_BASE_SYSTEM_PLAN_B_ONLY_H_

#if defined(WEBRTC_DEPRECATE_PLAN_B)
#define PLAN_B_ONLY [[deprecated]]
#else
#define PLAN_B_ONLY
#endif

#endif  // RTC_BASE_SYSTEM_PLAN_B_ONLY_H_
