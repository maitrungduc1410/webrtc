/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/test/conversational_speech/wavreader_factory.h"

#include <cstddef>
#include <cstdint>
#include <memory>

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "common_audio/wav_file.h"
#include "modules/audio_processing/test/conversational_speech/wavreader_interface.h"

namespace webrtc {
namespace test {
namespace {

using conversational_speech::WavReaderInterface;

class WavReaderAdaptor final : public WavReaderInterface {
 public:
  explicit WavReaderAdaptor(absl::string_view filepath)
      : wav_reader_(filepath) {}
  ~WavReaderAdaptor() override = default;

  size_t ReadFloatSamples(ArrayView<float> samples) override {
    return wav_reader_.ReadSamples(samples.size(), samples.begin());
  }

  size_t ReadInt16Samples(ArrayView<int16_t> samples) override {
    return wav_reader_.ReadSamples(samples.size(), samples.begin());
  }

  int SampleRate() const override { return wav_reader_.sample_rate(); }

  size_t NumChannels() const override { return wav_reader_.num_channels(); }

  size_t NumSamples() const override { return wav_reader_.num_samples(); }

 private:
  WavReader wav_reader_;
};

}  // namespace

namespace conversational_speech {

WavReaderFactory::WavReaderFactory() = default;

WavReaderFactory::~WavReaderFactory() = default;

std::unique_ptr<WavReaderInterface> WavReaderFactory::Create(
    absl::string_view filepath) const {
  return std::unique_ptr<WavReaderAdaptor>(new WavReaderAdaptor(filepath));
}

}  // namespace conversational_speech
}  // namespace test
}  // namespace webrtc
