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

#include <cstddef>
#include <cstdint>
#include <set>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/functional/any_invocable.h"
#include "api/location.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "rtc_base/checks.h"
#include "rtc_base/crypto_random.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::IsEmpty;
using ::testing::Test;

namespace webrtc {
namespace {
// Utility class that registers itself as the currently active task queue.
class FakeTaskQueue : public TaskQueueBase {
 public:
  FakeTaskQueue() : task_queue_setter_(this) {}

  void Delete() override {}
  void PostTaskImpl(absl::AnyInvocable<void() &&> task,
                    const PostTaskTraits& traits,
                    const Location& location) override {}
  void PostDelayedTaskImpl(absl::AnyInvocable<void() &&> task,
                           TimeDelta delay,
                           const PostDelayedTaskTraits& traits,
                           const Location& location) override {}

 private:
  CurrentTaskQueueSetter task_queue_setter_;
};
}  // namespace

template <typename Generator>
class UniqueIdGeneratorTest : public Test {};

using test_types = ::testing::Types<UniqueNumberGenerator<uint8_t>,
                                    UniqueNumberGenerator<uint16_t>,
                                    UniqueNumberGenerator<uint32_t>,
                                    UniqueNumberGenerator<int>,
                                    UniqueRandomIdGenerator,
                                    UniqueStringGenerator>;

TYPED_TEST_SUITE(UniqueIdGeneratorTest, test_types);

TYPED_TEST(UniqueIdGeneratorTest, ElementsDoNotRepeat) {
  typedef TypeParam Generator;
  const size_t num_elements = 255;
  Generator generator;
  std::vector<typename Generator::value_type> values;
  for (size_t i = 0; i < num_elements; i++) {
    values.push_back(generator.Generate());
  }

  EXPECT_EQ(num_elements, values.size());
  // Use a set to check uniqueness.
  std::set<typename Generator::value_type> set(values.begin(), values.end());
  EXPECT_EQ(values.size(), set.size()) << "Returned values were not unique.";
}

TYPED_TEST(UniqueIdGeneratorTest, KnownElementsAreNotGenerated) {
  typedef TypeParam Generator;
  const size_t num_elements = 100;
  InitRandom(0);
  Generator generator1;
  std::vector<typename Generator::value_type> known_values;
  for (size_t i = 0; i < num_elements; i++) {
    known_values.push_back(generator1.Generate());
  }
  EXPECT_EQ(num_elements, known_values.size());

  InitRandom(0);
  Generator generator2(known_values);

  std::vector<typename Generator::value_type> values;
  for (size_t i = 0; i < num_elements; i++) {
    values.push_back(generator2.Generate());
  }
  EXPECT_THAT(values, ::testing::SizeIs(num_elements));
  absl::c_sort(values);
  absl::c_sort(known_values);
  std::vector<typename Generator::value_type> intersection;
  absl::c_set_intersection(values, known_values,
                           std::back_inserter(intersection));
  EXPECT_THAT(intersection, IsEmpty());
}

TYPED_TEST(UniqueIdGeneratorTest, AddedElementsAreNotGenerated) {
  typedef TypeParam Generator;
  const size_t num_elements = 100;
  InitRandom(0);
  Generator generator1;
  std::vector<typename Generator::value_type> known_values;
  for (size_t i = 0; i < num_elements; i++) {
    known_values.push_back(generator1.Generate());
  }
  EXPECT_EQ(num_elements, known_values.size());

  InitRandom(0);
  Generator generator2;

  for (const typename Generator::value_type& value : known_values) {
    generator2.AddKnownId(value);
  }

  std::vector<typename Generator::value_type> values;
  for (size_t i = 0; i < num_elements; i++) {
    values.push_back(generator2.Generate());
  }
  EXPECT_THAT(values, ::testing::SizeIs(num_elements));
  absl::c_sort(values);
  absl::c_sort(known_values);
  std::vector<typename Generator::value_type> intersection;
  absl::c_set_intersection(values, known_values,
                           std::back_inserter(intersection));
  EXPECT_THAT(intersection, IsEmpty());
}

TYPED_TEST(UniqueIdGeneratorTest, AddKnownIdOnNewIdReturnsTrue) {
  typedef TypeParam Generator;

  InitRandom(0);
  Generator generator1;
  const typename Generator::value_type id = generator1.Generate();

  InitRandom(0);
  Generator generator2;
  EXPECT_TRUE(generator2.AddKnownId(id));
}

TYPED_TEST(UniqueIdGeneratorTest, AddKnownIdCalledAgainForSameIdReturnsFalse) {
  typedef TypeParam Generator;

  InitRandom(0);
  Generator generator1;
  const typename Generator::value_type id = generator1.Generate();

  InitRandom(0);
  Generator generator2;
  ASSERT_TRUE(generator2.AddKnownId(id));
  EXPECT_FALSE(generator2.AddKnownId(id));
}

TYPED_TEST(UniqueIdGeneratorTest,
           AddKnownIdOnIdProvidedAsKnownToCtorReturnsFalse) {
  typedef TypeParam Generator;

  InitRandom(0);
  Generator generator1;
  const typename Generator::value_type id = generator1.Generate();
  std::vector<typename Generator::value_type> known_values = {id};

  InitRandom(0);
  Generator generator2(known_values);
  EXPECT_FALSE(generator2.AddKnownId(id));
}

// Tests that it's OK to construct the generator in one execution environment
// (thread/task queue) but use it in another.
TEST(UniqueNumberGenerator, UsedOnSecondaryThread) {
  const auto* current_tq = TaskQueueBase::Current();
  // Construct the generator before `fake_task_queue` to ensure that it is
  // constructed in a different execution environment than what
  // `fake_task_queue` will represent.
  UniqueNumberGenerator<uint32_t> generator;

  FakeTaskQueue fake_task_queue;
  // Sanity check to make sure we're in a different runtime environment.
  ASSERT_NE(current_tq, TaskQueueBase::Current());

  // Generating an id should be fine in this context.
  generator.Generate();
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(UniqueNumberGeneratorDeathTest, FailsWhenUsedInWrongContext) {
  // Instantiate the generator before the `loop`. This ensures that
  // thread/sequence checkers will pick up a different thread environment than
  // `fake_task_queue` will represent.
  UniqueNumberGenerator<uint32_t> generator;

  // Instantiate a fake task queue that will register itself as the current tq.
  FakeTaskQueue initial_fake_task_queue;
  // Generate an ID on the current thread. This causes the generator to attach
  // to the current thread context.
  generator.Generate();

  // Instantiate a fake task queue that will register itself as the current tq.
  FakeTaskQueue fake_task_queue;

  // Attempting to generate an id should now trigger a dcheck.
  EXPECT_DEATH(generator.Generate(), "");
}
#endif

}  // namespace webrtc
