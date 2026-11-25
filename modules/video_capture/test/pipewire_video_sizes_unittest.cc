/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_capture/linux/pipewire_video_sizes.h"

#include <spa/param/format.h>
#include <spa/param/param.h>
#include <spa/pod/builder.h>
#include <spa/pod/filter.h>
#include <spa/pod/iter.h>

#include <array>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <utility>
#include <vector>

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace videocapturemodule {
namespace {

using Size = std::pair<uint32_t, uint32_t>;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::UnorderedElementsAre;

// A value followed by padding, so that each value of a choice built from it
// takes twice the size of the value.
template <typename T>
struct Padded {
  T value;
  T padding;
};

// Builds in |data| a format whose property |key| is a choice of padded values
// of |type|.
template <typename T>
const spa_pod* BuildPaddedChoice(std::array<uint8_t, 4096>& data,
                                 uint32_t key,
                                 uint32_t type,
                                 uint32_t choice,
                                 std::initializer_list<Padded<T>> values) {
  spa_pod_builder builder =
      SPA_POD_BUILDER_INIT(data.data(), static_cast<uint32_t>(data.size()));
  spa_pod_frame object, frame;
  spa_pod_builder_push_object(&builder, &object, SPA_TYPE_OBJECT_Format,
                              SPA_PARAM_EnumFormat);
  spa_pod_builder_prop(&builder, key, 0);
  spa_pod_builder_push_choice(&builder, &frame, choice, 0);
  spa_pod_builder_child(&builder, sizeof(Padded<T>), type);
  for (const Padded<T>& value : values) {
    spa_pod_builder_raw(&builder, &value, sizeof(value));
  }
  spa_pod_builder_pop(&builder, &frame);
  return static_cast<const spa_pod*>(spa_pod_builder_pop(&builder, &object));
}

class PipeWireVideoSizesTest : public ::testing::Test {
 protected:
  void SetChoice(uint32_t choice, std::initializer_list<Size> sizes) {
    spa_pod_builder builder =
        SPA_POD_BUILDER_INIT(data_.data(), static_cast<uint32_t>(data_.size()));
    spa_pod_frame object, values;
    spa_pod_builder_push_object(&builder, &object, SPA_TYPE_OBJECT_Format,
                                SPA_PARAM_EnumFormat);
    spa_pod_builder_prop(&builder, SPA_FORMAT_VIDEO_size, 0);
    spa_pod_builder_push_choice(&builder, &values, choice, 0);
    for (const auto& [width, height] : sizes) {
      spa_pod_builder_rectangle(&builder, width, height);
    }
    spa_pod_builder_pop(&builder, &values);
    format_ = static_cast<spa_pod*>(spa_pod_builder_pop(&builder, &object));
    ASSERT_NE(format_, nullptr);
  }

  std::vector<Size> Sizes() const {
    const spa_pod_prop* prop = spa_pod_object_find_prop(
        reinterpret_cast<const spa_pod_object*>(format_), nullptr,
        SPA_FORMAT_VIDEO_size);
    std::vector<Size> sizes;
    for (const auto& size : GetPipeWireVideoSizes(&prop->value)) {
      sizes.emplace_back(size.width, size.height);
    }
    return sizes;
  }

  void ExpectNegotiable() const {
    const auto sizes = Sizes();
    ASSERT_FALSE(sizes.empty());
    for (const auto& [width, height] : sizes) {
      alignas(8) std::array<uint8_t, 1024> request_data, result_data;
      spa_pod_builder request = SPA_POD_BUILDER_INIT(
          request_data.data(), static_cast<uint32_t>(request_data.size()));
      spa_pod_builder result = SPA_POD_BUILDER_INIT(
          result_data.data(), static_cast<uint32_t>(result_data.size()));
      const spa_rectangle size = SPA_RECTANGLE(width, height);
      const auto* filter =
          static_cast<const spa_pod*>(spa_pod_builder_add_object(
              &request, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
              SPA_FORMAT_VIDEO_size, SPA_POD_Rectangle(&size)));
      spa_pod* filtered = nullptr;
      EXPECT_EQ(spa_pod_filter(&result, &filtered, format_, filter), 0)
          << width << "x" << height;
    }
  }

  alignas(8) std::array<uint8_t, 4096> data_;
  const spa_pod* format_ = nullptr;
};

TEST_F(PipeWireVideoSizesTest, KeepsFixedSize) {
  SetChoice(SPA_CHOICE_None, {{640, 480}});
  EXPECT_THAT(Sizes(), ElementsAre(Size(640, 480)));
  ExpectNegotiable();
}

TEST_F(PipeWireVideoSizesTest, OffersEachEnumAlternativeOnce) {
  SetChoice(SPA_CHOICE_Enum,
            {{640, 480}, {1280, 720}, {640, 480}, {1280, 720}});
  EXPECT_THAT(Sizes(), UnorderedElementsAre(Size(640, 480), Size(1280, 720)));
  ExpectNegotiable();
}

TEST_F(PipeWireVideoSizesTest, RangeIncludesDefaultBoundsAndCommonSize) {
  SetChoice(SPA_CHOICE_Range, {{1296, 736}, {320, 240}, {1920, 1080}});
  EXPECT_THAT(Sizes(), Contains(Size(1296, 736)));
  EXPECT_THAT(Sizes(), Contains(Size(320, 240)));
  EXPECT_THAT(Sizes(), Contains(Size(1920, 1080)));
  EXPECT_THAT(Sizes(), Contains(Size(640, 480)));
  ExpectNegotiable();
}

TEST_F(PipeWireVideoSizesTest, DefaultDoesNotLimitRangeMaximum) {
  SetChoice(SPA_CHOICE_Range, {{640, 480}, {320, 240}, {3840, 2160}});
  EXPECT_THAT(Sizes(), Contains(Size(1920, 1080)));
  EXPECT_THAT(Sizes(), Contains(Size(3840, 2160)));
  ExpectNegotiable();
}

TEST_F(PipeWireVideoSizesTest, RangeChecksWidthAndHeightIndependently) {
  SetChoice(SPA_CHOICE_Range, {{640, 480}, {320, 240}, {1000, 1000}});
  for (const auto& [width, height] : Sizes()) {
    EXPECT_GE(width, 320u);
    EXPECT_LE(width, 1000u);
    EXPECT_GE(height, 240u);
    EXPECT_LE(height, 1000u);
  }
  ExpectNegotiable();
}

TEST_F(PipeWireVideoSizesTest, StepUsesAbsoluteMultiplesWithUnalignedMinimum) {
  SetChoice(SPA_CHOICE_Step, {{648, 488}, {321, 241}, {1296, 736}, {8, 8}});
  EXPECT_THAT(Sizes(), Contains(Size(640, 480)));
  EXPECT_THAT(Sizes(), Contains(Size(648, 488)));
  ExpectNegotiable();
}

TEST_F(PipeWireVideoSizesTest, StepKeepsOnlyNegotiableCandidates) {
  // Older PipeWire versions report the maximum as the step for this camera.
  SetChoice(SPA_CHOICE_Step,
            {{1296, 736}, {320, 240}, {1296, 736}, {1296, 736}});
  EXPECT_THAT(Sizes(), ElementsAre(Size(1296, 736)));
  ExpectNegotiable();
}

TEST_F(PipeWireVideoSizesTest, RejectsZeroStep) {
  SetChoice(SPA_CHOICE_Step, {{640, 480}, {320, 240}, {1296, 736}, {0, 1}});
  EXPECT_THAT(Sizes(), IsEmpty());
  SetChoice(SPA_CHOICE_Step, {{640, 480}, {320, 240}, {1296, 736}, {1, 0}});
  EXPECT_THAT(Sizes(), IsEmpty());
}

TEST_F(PipeWireVideoSizesTest, OffersFourKMaximum) {
  SetChoice(SPA_CHOICE_Range, {{1280, 720}, {320, 240}, {4096, 2160}});
  EXPECT_THAT(Sizes(), Contains(Size(4096, 2160)));
  ExpectNegotiable();
}

TEST_F(PipeWireVideoSizesTest, OffersEightKMaximum) {
  SetChoice(SPA_CHOICE_Range, {{1920, 1080}, {320, 240}, {7680, 4320}});
  EXPECT_THAT(Sizes(), Contains(Size(7680, 4320)));
  ExpectNegotiable();
}

TEST_F(PipeWireVideoSizesTest, DoesNotCapMaximumAboveEightK) {
  SetChoice(SPA_CHOICE_Range, {{1920, 1080}, {320, 240}, {16384, 8640}});
  EXPECT_THAT(Sizes(), Contains(Size(16384, 8640)));
  ExpectNegotiable();
}

TEST_F(PipeWireVideoSizesTest, DegenerateRangeDoesNotDuplicateSize) {
  SetChoice(SPA_CHOICE_Range, {{640, 480}, {640, 480}, {640, 480}});
  EXPECT_THAT(Sizes(), ElementsAre(Size(640, 480)));
  ExpectNegotiable();
}

TEST_F(PipeWireVideoSizesTest, IgnoresDefaultOutsideRange) {
  SetChoice(SPA_CHOICE_Range, {{3840, 2160}, {320, 240}, {1920, 1080}});
  EXPECT_THAT(Sizes(), Contains(Size(320, 240)));
  EXPECT_THAT(Sizes(), Contains(Size(1920, 1080)));
  for (const auto& [width, height] : Sizes()) {
    EXPECT_LE(width, 1920u);
    EXPECT_LE(height, 1080u);
  }
  ExpectNegotiable();
}

TEST_F(PipeWireVideoSizesTest, RejectsNonRectangleValue) {
  const spa_pod_int integer = {{sizeof(int32_t), SPA_TYPE_Int}, 640};
  EXPECT_THAT(GetPipeWireVideoSizes(&integer.pod), IsEmpty());
}

TEST_F(PipeWireVideoSizesTest, RejectsValuesOfAnotherSize) {
  // Read as an array of spa_rectangle, the alternatives of these values would
  // be 320x240, the padding of the first value, and 1280x720, the first
  // alternative, while 1920x1080 would be lost.
  format_ = BuildPaddedChoice<spa_rectangle>(
      data_, SPA_FORMAT_VIDEO_size, SPA_TYPE_Rectangle, SPA_CHOICE_Enum,
      {{{640, 480}, {320, 240}},
       {{1280, 720}, {160, 120}},
       {{1920, 1080}, {0, 0}}});
  ASSERT_NE(format_, nullptr);
  EXPECT_THAT(Sizes(), IsEmpty());
}

using Rate = std::pair<uint32_t, uint32_t>;

class PipeWireRangeFrameRatesTest : public ::testing::Test {
 protected:
  void SetChoice(uint32_t choice, std::initializer_list<Rate> rates) {
    spa_pod_builder builder =
        SPA_POD_BUILDER_INIT(data_.data(), static_cast<uint32_t>(data_.size()));
    spa_pod_frame object, values;
    spa_pod_builder_push_object(&builder, &object, SPA_TYPE_OBJECT_Format,
                                SPA_PARAM_EnumFormat);
    spa_pod_builder_prop(&builder, SPA_FORMAT_VIDEO_framerate, 0);
    spa_pod_builder_push_choice(&builder, &values, choice, 0);
    for (const auto& [num, denom] : rates) {
      spa_pod_builder_fraction(&builder, num, denom);
    }
    spa_pod_builder_pop(&builder, &values);
    format_ = static_cast<spa_pod*>(spa_pod_builder_pop(&builder, &object));
    ASSERT_NE(format_, nullptr);
  }

  std::vector<Rate> Rates() const {
    const spa_pod_prop* prop = spa_pod_object_find_prop(
        reinterpret_cast<const spa_pod_object*>(format_), nullptr,
        SPA_FORMAT_VIDEO_framerate);
    std::vector<Rate> rates;
    for (const auto& rate : GetPipeWireRangeFrameRates(&prop->value)) {
      rates.emplace_back(rate.num, rate.denom);
    }
    return rates;
  }

  // Each rate, requested as an exact frame rate, must fit the range.
  void ExpectNegotiable() const {
    const auto rates = Rates();
    ASSERT_FALSE(rates.empty());
    for (const Rate& rate : rates) {
      ExpectNegotiable(rate);
    }
  }

  // |rate|, requested as an exact frame rate, must fit the format.
  void ExpectNegotiable(const Rate& rate) const {
    alignas(8) std::array<uint8_t, 1024> request_data, result_data;
    spa_pod_builder request = SPA_POD_BUILDER_INIT(
        request_data.data(), static_cast<uint32_t>(request_data.size()));
    spa_pod_builder result = SPA_POD_BUILDER_INIT(
        result_data.data(), static_cast<uint32_t>(result_data.size()));
    const spa_fraction fraction = SPA_FRACTION(rate.first, rate.second);
    const auto* filter = static_cast<const spa_pod*>(spa_pod_builder_add_object(
        &request, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
        SPA_FORMAT_VIDEO_framerate, SPA_POD_Fraction(&fraction)));
    spa_pod* filtered = nullptr;
    EXPECT_EQ(spa_pod_filter(&result, &filtered, format_, filter), 0)
        << rate.first << "/" << rate.second;
  }

  alignas(8) std::array<uint8_t, 4096> data_;
  const spa_pod* format_ = nullptr;
};

TEST_F(PipeWireRangeFrameRatesTest, OffersDefaultMinimumAndMaximum) {
  SetChoice(SPA_CHOICE_Range, {{30, 1}, {15, 1}, {60, 1}});
  EXPECT_THAT(Rates(), ElementsAre(Rate(30, 1), Rate(15, 1), Rate(60, 1)));
  ExpectNegotiable();
}

TEST_F(PipeWireRangeFrameRatesTest, LeavesOutZeroMinimum) {
  SetChoice(SPA_CHOICE_Range, {{30, 1}, {0, 1}, {60, 1}});
  EXPECT_THAT(Rates(), ElementsAre(Rate(30, 1), Rate(60, 1)));
  ExpectNegotiable();
}

TEST_F(PipeWireRangeFrameRatesTest, KeepsExactFractions) {
  SetChoice(SPA_CHOICE_Range, {{30000, 1001}, {15, 1}, {60, 1}});
  EXPECT_THAT(Rates(),
              ElementsAre(Rate(30000, 1001), Rate(15, 1), Rate(60, 1)));
  ExpectNegotiable();
}

TEST_F(PipeWireRangeFrameRatesTest, OffersHighFrameRates) {
  SetChoice(SPA_CHOICE_Range, {{60, 1}, {30, 1}, {120, 1}});
  EXPECT_THAT(Rates(), ElementsAre(Rate(60, 1), Rate(30, 1), Rate(120, 1)));
  ExpectNegotiable();
  SetChoice(SPA_CHOICE_Range, {{120, 1}, {30, 1}, {240, 1}});
  EXPECT_THAT(Rates(), ElementsAre(Rate(120, 1), Rate(30, 1), Rate(240, 1)));
  ExpectNegotiable();
  SetChoice(SPA_CHOICE_Range, {{60000, 1001}, {30000, 1001}, {120000, 1001}});
  EXPECT_THAT(Rates(), ElementsAre(Rate(60000, 1001), Rate(30000, 1001),
                                   Rate(120000, 1001)));
  ExpectNegotiable();
}

TEST_F(PipeWireRangeFrameRatesTest, KeepsLargerOfRatesRoundingToSameValue) {
  SetChoice(SPA_CHOICE_Range, {{30000, 1001}, {15, 1}, {30, 1}});
  EXPECT_THAT(Rates(), ElementsAre(Rate(30, 1), Rate(15, 1)));
  ExpectNegotiable();
  SetChoice(SPA_CHOICE_Range, {{30, 1}, {30000, 1001}, {60, 1}});
  EXPECT_THAT(Rates(), ElementsAre(Rate(30, 1), Rate(60, 1)));
  ExpectNegotiable();
  SetChoice(SPA_CHOICE_Range, {{120000, 1001}, {30, 1}, {120, 1}});
  EXPECT_THAT(Rates(), ElementsAre(Rate(120, 1), Rate(30, 1)));
  ExpectNegotiable();
}

TEST_F(PipeWireRangeFrameRatesTest, DegenerateRangeDoesNotDuplicateRate) {
  SetChoice(SPA_CHOICE_Range, {{30, 1}, {30, 1}, {30, 1}});
  EXPECT_THAT(Rates(), ElementsAre(Rate(30, 1)));
  ExpectNegotiable();
}

TEST_F(PipeWireRangeFrameRatesTest, LeavesOutDefaultOutsideRange) {
  SetChoice(SPA_CHOICE_Range, {{90, 1}, {40, 1}, {60, 1}});
  EXPECT_THAT(Rates(), ElementsAre(Rate(40, 1), Rate(60, 1)));
  ExpectNegotiable();
  SetChoice(SPA_CHOICE_Range, {{10, 1}, {40, 1}, {60, 1}});
  EXPECT_THAT(Rates(), ElementsAre(Rate(40, 1), Rate(60, 1)));
  ExpectNegotiable();
  SetChoice(SPA_CHOICE_Range, {{30, 0}, {15, 1}, {60, 1}});
  EXPECT_THAT(Rates(), ElementsAre(Rate(15, 1), Rate(60, 1)));
  ExpectNegotiable();
  SetChoice(SPA_CHOICE_Range, {{0, 0}, {15, 1}, {60, 1}});
  EXPECT_THAT(Rates(), ElementsAre(Rate(15, 1), Rate(60, 1)));
  ExpectNegotiable();
}

TEST_F(PipeWireRangeFrameRatesTest, RejectsBoundWithZeroDenominator) {
  SetChoice(SPA_CHOICE_Range, {{30, 1}, {15, 0}, {60, 1}});
  EXPECT_THAT(Rates(), IsEmpty());
  SetChoice(SPA_CHOICE_Range, {{30, 1}, {15, 1}, {60, 0}});
  EXPECT_THAT(Rates(), IsEmpty());
  // 0/0 passes the comparison of the bounds, so only the check of the
  // denominators rejects it.
  SetChoice(SPA_CHOICE_Range, {{30, 1}, {0, 0}, {60, 1}});
  EXPECT_THAT(Rates(), IsEmpty());
}

TEST_F(PipeWireRangeFrameRatesTest, RejectsInvalidBounds) {
  SetChoice(SPA_CHOICE_Range, {{30, 1}, {60, 1}, {15, 1}});
  EXPECT_THAT(Rates(), IsEmpty());
  SetChoice(SPA_CHOICE_Range, {{0, 1}, {0, 1}, {0, 1}});
  EXPECT_THAT(Rates(), IsEmpty());
}

TEST_F(PipeWireRangeFrameRatesTest, LeavesOpenRangeFromZeroUnspecified) {
  // videotestsrc advertises {25; 0..INT32_MAX}.
  SetChoice(SPA_CHOICE_Range, {{25, 1}, {0, 1}, {INT32_MAX, 1}});
  EXPECT_THAT(Rates(), IsEmpty());
  SetChoice(SPA_CHOICE_Range, {{25, 1}, {0, 1}, {UINT32_MAX, 1}});
  EXPECT_THAT(Rates(), IsEmpty());
}

TEST_F(PipeWireRangeFrameRatesTest, OffersMinimumOfRangeWithoutUpperBound) {
  SetChoice(SPA_CHOICE_Range, {{60, 1}, {40, 1}, {INT32_MAX, 1}});
  EXPECT_THAT(Rates(), ElementsAre(Rate(40, 1)));
  ExpectNegotiable();
  SetChoice(SPA_CHOICE_Range, {{60, 1}, {40, 1}, {4294967294u, 2}});
  EXPECT_THAT(Rates(), ElementsAre(Rate(40, 1)));
  ExpectNegotiable();
  SetChoice(SPA_CHOICE_Range, {{30, 1}, {15, 1}, {4294967293u, 2}});
  EXPECT_THAT(Rates(), ElementsAre(Rate(15, 1)));
  ExpectNegotiable();
  SetChoice(SPA_CHOICE_Range, {{30, 1}, {15, 1}, {UINT32_MAX, 1}});
  EXPECT_THAT(Rates(), ElementsAre(Rate(15, 1)));
  ExpectNegotiable();
  SetChoice(SPA_CHOICE_Range, {{60, 1}, {30000, 1001}, {INT32_MAX, 1}});
  EXPECT_THAT(Rates(), ElementsAre(Rate(30000, 1001)));
  ExpectNegotiable();
}

TEST_F(PipeWireRangeFrameRatesTest, LeavesTooLargeMinimumUnspecified) {
  SetChoice(SPA_CHOICE_Range,
            {{INT32_MAX, 1}, {INT32_MAX, 1}, {UINT32_MAX, 1}});
  EXPECT_THAT(Rates(), IsEmpty());
}

TEST_F(PipeWireRangeFrameRatesTest, OffersLargestRepresentableMaximum) {
  SetChoice(SPA_CHOICE_Range, {{30, 1}, {15, 1}, {INT32_MAX - 1, 1}});
  EXPECT_THAT(Rates(),
              ElementsAre(Rate(30, 1), Rate(15, 1), Rate(INT32_MAX - 1, 1)));
}

TEST_F(PipeWireRangeFrameRatesTest, IgnoresOtherChoices) {
  SetChoice(SPA_CHOICE_None, {{30, 1}});
  EXPECT_THAT(Rates(), IsEmpty());
  SetChoice(SPA_CHOICE_Enum, {{30, 1}, {15, 1}, {30, 1}, {60, 1}});
  EXPECT_THAT(Rates(), IsEmpty());
  SetChoice(SPA_CHOICE_Step, {{30, 1}, {15, 1}, {60, 1}, {1, 1}});
  EXPECT_THAT(Rates(), IsEmpty());
}

TEST_F(PipeWireRangeFrameRatesTest, RejectsNonFractionValue) {
  const spa_pod_int integer = {{sizeof(int32_t), SPA_TYPE_Int}, 30};
  EXPECT_THAT(GetPipeWireRangeFrameRates(&integer.pod), IsEmpty());
}

TEST_F(PipeWireRangeFrameRatesTest, RejectsValuesOfAnotherSize) {
  // Read as an array of spa_fraction, these values would give the range
  // {30/1; 10/1..40/1}, whose default and minimum the source does not offer.
  format_ = BuildPaddedChoice<spa_fraction>(
      data_, SPA_FORMAT_VIDEO_framerate, SPA_TYPE_Fraction, SPA_CHOICE_Range,
      {{{30, 1}, {10, 1}}, {{40, 1}, {0, 0}}, {{90, 1}, {0, 0}}});
  ASSERT_NE(format_, nullptr);
  EXPECT_THAT(Rates(), IsEmpty());
}

class PipeWireEnumFrameRateTest : public PipeWireRangeFrameRatesTest {
 protected:
  std::optional<Rate> EnumRate() const {
    const spa_pod_prop* prop = spa_pod_object_find_prop(
        reinterpret_cast<const spa_pod_object*>(format_), nullptr,
        SPA_FORMAT_VIDEO_framerate);
    const std::optional<spa_fraction> rate =
        GetPipeWireEnumFrameRate(&prop->value);
    if (!rate) {
      return std::nullopt;
    }
    return Rate(rate->num, rate->denom);
  }
};

TEST_F(PipeWireEnumFrameRateTest, OffersLargestAlternative) {
  // The default is not an alternative of its own.
  SetChoice(SPA_CHOICE_Enum, {{120, 1}, {15, 1}, {60, 1}, {30, 1}});
  EXPECT_THAT(EnumRate(), Optional(Rate(60, 1)));
  ExpectNegotiable(Rate(60, 1));
  SetChoice(SPA_CHOICE_Enum,
            {{30000, 1001}, {30000, 1001}, {60000, 1001}, {60, 1}});
  EXPECT_THAT(EnumRate(), Optional(Rate(60, 1)));
}

TEST_F(PipeWireEnumFrameRateTest, KeepsFirstOfEqualRates) {
  SetChoice(SPA_CHOICE_Enum, {{30, 1}, {30, 1}, {60, 2}});
  EXPECT_THAT(EnumRate(), Optional(Rate(30, 1)));
  SetChoice(SPA_CHOICE_Enum, {{0, 1}, {0, 1}, {0, 2}});
  EXPECT_THAT(EnumRate(), Optional(Rate(0, 1)));
}

TEST_F(PipeWireEnumFrameRateTest, ComparesRatesAsFractions) {
  // These rates differ, but are equal as doubles.
  SetChoice(SPA_CHOICE_Enum, {{1000000001, 1000000000},
                              {1000000001, 1000000000},
                              {1000000000, 999999999}});
  EXPECT_THAT(EnumRate(), Optional(Rate(1000000000, 999999999)));
}

TEST_F(PipeWireEnumFrameRateTest, SkipsAlternativesThatDoNotFit) {
  SetChoice(SPA_CHOICE_Enum, {{60, 1}, {60, 1}, {UINT32_MAX, 1}});
  EXPECT_THAT(EnumRate(), Optional(Rate(60, 1)));
  ExpectNegotiable(Rate(60, 1));
  SetChoice(SPA_CHOICE_Enum, {{30, 1}, {90, 0}, {30, 1}});
  EXPECT_THAT(EnumRate(), Optional(Rate(30, 1)));
}

TEST_F(PipeWireEnumFrameRateTest, LeavesRateUnspecifiedWhenNoneFits) {
  SetChoice(SPA_CHOICE_Enum, {{UINT32_MAX, 1}, {UINT32_MAX, 1}, {30, 0}});
  EXPECT_EQ(EnumRate(), std::nullopt);
}

TEST_F(PipeWireEnumFrameRateTest, RejectsOtherValues) {
  SetChoice(SPA_CHOICE_Enum, {{30, 1}});
  EXPECT_EQ(EnumRate(), std::nullopt);
  SetChoice(SPA_CHOICE_Range, {{30, 1}, {15, 1}, {60, 1}});
  EXPECT_EQ(EnumRate(), std::nullopt);
  format_ = BuildPaddedChoice<spa_fraction>(
      data_, SPA_FORMAT_VIDEO_framerate, SPA_TYPE_Fraction, SPA_CHOICE_Enum,
      {{{30, 1}, {10, 1}}, {{40, 1}, {0, 0}}, {{90, 1}, {0, 0}}});
  ASSERT_NE(format_, nullptr);
  EXPECT_EQ(EnumRate(), std::nullopt);
}

TEST(PipeWireRoundedUpRateTest, RoundsUp) {
  EXPECT_THAT(RoundedUpRate({30, 1}), Optional(30));
  EXPECT_THAT(RoundedUpRate({30000, 1001}), Optional(30));
  EXPECT_THAT(RoundedUpRate({0, 1}), Optional(0));
}

TEST(PipeWireRoundedUpRateTest, RejectsZeroDenominator) {
  EXPECT_EQ(RoundedUpRate({30, 0}), std::nullopt);
  EXPECT_EQ(RoundedUpRate({0, 0}), std::nullopt);
}

TEST(PipeWireRoundedUpRateTest, RejectsRatesThatDoNotFitInt32) {
  EXPECT_THAT(RoundedUpRate({INT32_MAX, 1}), Optional(INT32_MAX));
  EXPECT_THAT(RoundedUpRate({4294967293u, 2}), Optional(INT32_MAX));
  EXPECT_EQ(RoundedUpRate({uint32_t{INT32_MAX} + 1, 1}), std::nullopt);
  EXPECT_EQ(RoundedUpRate({4294967295u, 2}), std::nullopt);
  EXPECT_EQ(RoundedUpRate({UINT32_MAX, 1}), std::nullopt);
}

}  // namespace
}  // namespace videocapturemodule
}  // namespace webrtc
