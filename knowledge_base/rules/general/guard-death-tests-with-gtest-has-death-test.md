---
name: webrtc/guard-death-tests-with-gtest-has-death-test
description: 'When writing unit tests using EXPECT_DEATH or ASSERT_DEATH, wrap test cases, assertions, and death-test-only symbols in #if GTEST_HAS_DEATH_TEST guards'
rubric: correctness
severity: blocking
---

**Rule.** When writing unit tests or assertions that verify process termination
or crash behavior using EXPECT_DEATH or ASSERT_DEATH, wrap the test cases,
assertion blocks, and any symbols or using declarations used exclusively by
death tests in #if GTEST_HAS_DEATH_TEST ... #endif // GTEST_HAS_DEATH_TEST
preprocessor guards.

**Why.** Death tests rely on process forking and execution primitives that are
not supported on all target platforms or build configurations (such as iOS).
Guarding death tests along with death-test-specific matchers or using
declarations prevents compilation failures (such as unused symbol warnings) and
test execution errors on unsupported platforms.

## Examples

Bad:

```cpp
using ::testing::_;

TEST(UtilsTest, IfCodecDoesNotExistRaiseError) {
  EXPECT_DEATH(GetVideoCodecType(/*codec_name=*/"Not_a_codec"), _);
}
```

Good:

```cpp
#if GTEST_HAS_DEATH_TEST
using ::testing::_;

TEST(UtilsTest, IfCodecDoesNotExistRaiseError) {
  EXPECT_DEATH(GetVideoCodecType(/*codec_name=*/"Not_a_codec"), _);
}
#endif  // GTEST_HAS_DEATH_TEST
```
