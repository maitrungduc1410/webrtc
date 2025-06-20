/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/unique_id_generator.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/crypto_random.h"
#include "rtc_base/string_to_number.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {

UniqueRandomIdGenerator::UniqueRandomIdGenerator() : known_ids_() {}
UniqueRandomIdGenerator::UniqueRandomIdGenerator(ArrayView<uint32_t> known_ids)
    : known_ids_(known_ids.begin(), known_ids.end()) {}

UniqueRandomIdGenerator::~UniqueRandomIdGenerator() = default;

uint32_t UniqueRandomIdGenerator::GenerateId() {
  MutexLock lock(&mutex_);

  RTC_CHECK_LT(known_ids_.size(), std::numeric_limits<uint32_t>::max() - 1);
  while (true) {
    auto pair = known_ids_.insert(CreateRandomNonZeroId());
    if (pair.second) {
      return *pair.first;
    }
  }
}

bool UniqueRandomIdGenerator::AddKnownId(uint32_t value) {
  MutexLock lock(&mutex_);
  return known_ids_.insert(value).second;
}

UniqueStringGenerator::UniqueStringGenerator() : unique_number_generator_() {}
UniqueStringGenerator::UniqueStringGenerator(ArrayView<std::string> known_ids) {
  for (const std::string& str : known_ids) {
    AddKnownId(str);
  }
}

UniqueStringGenerator::~UniqueStringGenerator() = default;

std::string UniqueStringGenerator::GenerateString() {
  return absl::StrCat(unique_number_generator_.GenerateNumber());
}

bool UniqueStringGenerator::AddKnownId(absl::string_view value) {
  // TODO(webrtc:13579): remove string copy here once absl::string_view version
  // of StringToNumber is available.
  std::optional<uint32_t> int_value =
      StringToNumber<uint32_t>(std::string(value));
  // The underlying generator works for uint32_t values, so if the provided
  // value is not a uint32_t it will never be generated anyway.
  if (int_value.has_value()) {
    return unique_number_generator_.AddKnownId(int_value.value());
  }
  return false;
}

}  // namespace webrtc
