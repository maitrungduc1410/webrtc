/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_RATE_TRACKER_H_
#define RTC_BASE_RATE_TRACKER_H_

#include <stdint.h>
#include <stdlib.h>

#include "absl/base/macros.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"

namespace webrtc {

// Computes units per second over a given interval by tracking the units over
// each bucket of a given size and calculating the instantaneous rate assuming
// that over each bucket the rate was constant.
class RateTracker {
 public:
  RateTracker(int64_t bucket_milliseconds, size_t bucket_count);
  virtual ~RateTracker();

  // Computes the average rate over the most recent interval_milliseconds,
  // or if the first sample was added within this period, computes the rate
  // since the first sample was added.
  double ComputeRateForInterval(Timestamp current_time,
                                TimeDelta interval) const;
  [[deprecated]]
  double ComputeRateForInterval(int64_t interval_milliseconds) const {
    return ComputeRateForInterval(Timestamp::Millis(Time()),
                                  TimeDelta::Millis(interval_milliseconds));
  }

  // Computes the average rate over the rate tracker's recording interval
  // of bucket_milliseconds * bucket_count.
  double Rate(Timestamp current_time) const {
    return ComputeRateForInterval(
        current_time, TimeDelta::Millis(bucket_milliseconds_) * bucket_count_);
  }

  [[deprecated]]
  double ComputeRate() const {
    return Rate(Timestamp::Millis(Time()));
  }

  // The total number of samples added.
  int64_t TotalSampleCount() const;

  // Increment count for bucket at `current_time`.
  void Update(int64_t sample_count, Timestamp now);

  // Reads the current time in order to determine the appropriate bucket for
  // these samples, and increments the count for that bucket by sample_count.
  [[deprecated]]
  void AddSamples(int64_t sample_count) {
    Update(sample_count, Timestamp::Millis(Time()));
  }

  ABSL_DEPRECATE_AND_INLINE()
  void AddSamplesAtTime(int64_t current_time_ms, int64_t sample_count) {
    Update(sample_count, Timestamp::Millis(current_time_ms));
  }

 protected:
  // overrideable for tests
  // TODO: bugs.webrtc.org/42223992 - Delete after Oct 27, 2025 together with
  // deprecated functions that do not take current time as a parameter
  virtual int64_t Time() const;

 private:
  void EnsureInitialized(int64_t current_time_ms);
  size_t NextBucketIndex(size_t bucket_index) const;

  const int64_t bucket_milliseconds_;
  const size_t bucket_count_;
  int64_t* sample_buckets_;
  size_t total_sample_count_;
  size_t current_bucket_;
  int64_t bucket_start_time_milliseconds_;
  int64_t initialization_time_milliseconds_;
};

}  //  namespace webrtc


#endif  // RTC_BASE_RATE_TRACKER_H_
