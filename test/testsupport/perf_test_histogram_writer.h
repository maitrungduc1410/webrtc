/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_TESTSUPPORT_PERF_TEST_HISTOGRAM_WRITER_H_
#define TEST_TESTSUPPORT_PERF_TEST_HISTOGRAM_WRITER_H_

#include "test/testsupport/perf_test_result_writer.h"

namespace webrtc {
namespace test {

// Returns a writer that serializes perf results as a Catapult HistogramSet
// proto, or nullptr if WebRTC was built with rtc_use_catapult_histograms =
// false. The caller takes ownership of the returned writer.
PerfTestResultWriter* CreateHistogramWriter();

}  // namespace test
}  // namespace webrtc

#endif  // TEST_TESTSUPPORT_PERF_TEST_HISTOGRAM_WRITER_H_
