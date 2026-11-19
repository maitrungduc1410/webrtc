/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/perf_test_histogram_writer.h"
#include "test/testsupport/perf_test_result_writer.h"

namespace webrtc {
namespace test {

// Built when rtc_use_catapult_histograms = false: there is no histogram
// writer, callers must handle nullptr.
PerfTestResultWriter* CreateHistogramWriter() {
  return nullptr;
}

}  // namespace test
}  // namespace webrtc
