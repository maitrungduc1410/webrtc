---
name: webrtc/fire-deprecated-signals-during-migration-or-remove
description: When migrating from legacy signals to CallbackList or AnyInvocable, update dispatchers to invoke Send, fire retained signals without asserting !is_empty(), and assert legacy emptiness
rubric: correctness
severity: blocking
---

**Rule.** When migrating event notification interfaces from legacy signals (such
as sigslot::signal) to modern callbacks (CallbackList or absl::AnyInvocable),
update notification dispatcher methods (e.g., Notify*()) to invoke
callback_list_.Send(...) or the modern callback. If backward compatibility is
required, ensure dispatchers continue firing retained deprecated signals without
asserting non-emptiness (!signal.is_empty()) and assert
RTC_DCHECK(legacy_signal.is_empty()) upon registering new callbacks (verified
via death tests); otherwise, remove deprecated signal declarations completely.

**Why.** Failing to update notification methods leaves events dispatching to
legacy signals rather than firing new CallbackList receivers. Retaining
deprecated signals without firing them breaks unmigrated listeners, asserting
non-emptiness before firing causes false assertion crashes since signals require
no listeners, and omitting legacy emptiness checks risks duplicate handlers.

## Examples

Bad:

```cpp
// Anti-pattern 1: Dispatcher calls legacy signal instead of new CallbackList
void NotifyRoleConflict(IceTransportInternal* transport) {
  SignalRoleConflict(transport);  // Missed updating to role_conflict_callbacks_.Send(transport)
}

// Anti-pattern 2: Asserting !is_empty() before firing signal, or dropping dispatch
void Port::NotifyRoleConflict() {
  if (role_conflict_callback_) {
    role_conflict_callback_();
  } else {
    RTC_DCHECK(!SignalRoleConflict.is_empty());  // Bad: Signals may have 0 listeners.
    SignalRoleConflict(this);
  }
}

// Anti-pattern 3: Omitting legacy signal emptiness check when registering callback
void Port::SubscribeRoleConflict(absl::AnyInvocable<void()> callback) {
  RTC_DCHECK(!role_conflict_callback_);
  role_conflict_callback_ = std::move(callback);
}
```

Good:

```cpp
// Pattern 1: Dispatcher updated to send via CallbackList and remove obsolete signal
void NotifyRoleConflict(IceTransportInternal* transport) {
  role_conflict_callbacks_.Send(transport);
}

// Pattern 2: Fire retained deprecated signal without asserting non-emptiness
void Port::NotifyRoleConflict() {
  if (role_conflict_callback_) {
    RTC_DCHECK(SignalRoleConflict.is_empty());
    role_conflict_callback_();
  } else {
    SignalRoleConflict(this);  // Safe to fire with 0 listeners.
  }
}

// Pattern 3: Enforce mutual exclusivity with emptiness check and death tests
void Port::SubscribeRoleConflict(absl::AnyInvocable<void()> callback) {
  RTC_DCHECK(callback != nullptr);
  RTC_DCHECK(!role_conflict_callback_);
  RTC_DCHECK(SignalRoleConflict.is_empty());
  role_conflict_callback_ = std::move(callback);
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST_F(PortDeathTest, AddSignalThenCallback) {
  DeathChannel dc(CreateRawTestPort());
  dc.AddSignal();
  EXPECT_DEATH(dc.AddCallback(), "");
}
#endif
```
