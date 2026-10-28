---
name: webrtc/prefer-timedelta-offsets-for-simulated-clock-advancement
description: When updating simulated clocks in tests, advance using TimeDelta offsets or call SetTime directly instead of subtracting clock time from target timestamps
rubric: readability
severity: blocking
---

**Rule.** When manipulating simulated or fake clocks (`SimulatedClock`,
`FakeClock`, `TimeController`, `GlobalSimulatedTimeController`) in unit and
integration tests, avoid computing advancement deltas by subtracting the current
clock time from an absolute target timestamp (e.g., `target_timestamp -
clock.CurrentTime()` or `TimeDelta::Millis(target_ms -
clock.TimeInMilliseconds())`). Instead:
1. If setting or seeking the clock to an absolute timestamp, call `SetTime(Timestamp::Millis(...))` or `SetTime(Timestamp::Micros(...))` directly.
2. If advancing time by an elapsed interval or offset, define time increments as `TimeDelta` constants and pass the delta directly to `AdvanceTime(delta)` (deriving target timestamps as `CurrentTime() + delta` when needed).

**Why.** Subtracting current clock time from hardcoded absolute timestamps or
calculating relative deltas against clock sampling is roundabout and
error-prone. It implicitly assumes target timestamps are strictly in the future,
risking negative deltas or ordering bugs if test setup changes. Using
`SetTime()` clearly conveys the intent to jump to an absolute timestamp, while
explicit `TimeDelta` constants with `AdvanceTime()` keep interval-based time
progression safe, readable, and deterministic.

## Examples

Bad:

```cpp
// Bad: Subtracting current clock time from an absolute timestamp to advance time.
const Timestamp kReportBlockTimestampUtc = Timestamp::Micros(123456789);
clock_.AdvanceTime(kReportBlockTimestampUtc - clock_.CurrentTime());

// Bad: Calculating a relative delta to match an absolute timestamp.
clock_.AdvanceTime(
    TimeDelta::Millis(timestamp_ms - env_.clock().TimeInMilliseconds()));
```

Good:

```cpp
// Good: Advancing simulated clock directly with an explicit TimeDelta offset.
constexpr TimeDelta kReportBlockTimestampUtcOffset =
    TimeDelta::Micros(123456789);
const Timestamp kReportBlockTimestampUtc =
    time_controller_.GetClock()->CurrentTime() +
    kReportBlockTimestampUtcOffset;
time_controller_.AdvanceTime(kReportBlockTimestampUtcOffset);

// Good: Setting the target absolute timestamp directly.
clock_.SetTime(Timestamp::Millis(timestamp_ms));
```
