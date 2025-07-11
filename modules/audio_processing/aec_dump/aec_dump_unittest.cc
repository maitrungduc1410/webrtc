/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/include/aec_dump.h"

#include <stdio.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

#include "api/audio/audio_processing.h"
#include "modules/audio_processing/aec_dump/aec_dump_factory.h"
#include "rtc_base/task_queue_for_test.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {

TEST(AecDumper, APICallsDoNotCrash) {
  // Note order of initialization: Task queue has to be initialized
  // before AecDump.
  TaskQueueForTest file_writer_queue("file_writer_queue");

  const std::string filename =
      test::TempFilename(test::OutputPath(), "aec_dump");

  {
    std::unique_ptr<AecDump> aec_dump =
        AecDumpFactory::Create(filename, -1, file_writer_queue.Get());

    constexpr int kNumChannels = 1;
    constexpr int kNumSamplesPerChannel = 160;
    std::array<int16_t, kNumSamplesPerChannel * kNumChannels> frame;
    frame.fill(0.f);
    aec_dump->WriteRenderStreamMessage(frame.data(), kNumChannels,
                                       kNumSamplesPerChannel);

    aec_dump->AddCaptureStreamInput(frame.data(), kNumChannels,
                                    kNumSamplesPerChannel);
    aec_dump->AddCaptureStreamOutput(frame.data(), kNumChannels,
                                     kNumSamplesPerChannel);

    aec_dump->WriteCaptureStreamMessage();

    InternalAPMConfig apm_config;
    aec_dump->WriteConfig(apm_config);

    ProcessingConfig api_format;
    constexpr int64_t kTimeNowMs = 123456789ll;
    aec_dump->WriteInitMessage(api_format, kTimeNowMs);
  }
  // Remove file after the AecDump d-tor has finished.
  ASSERT_EQ(0, remove(filename.c_str()));
}

TEST(AecDumper, WriteToFile) {
  TaskQueueForTest file_writer_queue("file_writer_queue");

  const std::string filename =
      test::TempFilename(test::OutputPath(), "aec_dump");

  {
    std::unique_ptr<AecDump> aec_dump =
        AecDumpFactory::Create(filename, -1, file_writer_queue.Get());

    constexpr int kNumChannels = 1;
    constexpr int kNumSamplesPerChannel = 160;
    std::array<int16_t, kNumSamplesPerChannel * kNumChannels> frame;
    frame.fill(0.f);

    aec_dump->WriteRenderStreamMessage(frame.data(), kNumChannels,
                                       kNumSamplesPerChannel);
  }

  // Verify the file has been written after the AecDump d-tor has
  // finished.
  FILE* fid = fopen(filename.c_str(), "r");
  ASSERT_TRUE(fid != nullptr);

  // Clean it up.
  ASSERT_EQ(0, fclose(fid));
  ASSERT_EQ(0, remove(filename.c_str()));
}

}  // namespace webrtc
