---
name: webrtc/net/avoid-serializing-redundant-socket-handover-state
description: When implementing dcSCTP socket handover serialization and restoration (AddHandoverState and RestoreFromState), restore all fields, omit redundant state, and ensure backward compatibility for legacy snapshots
rubric: correctness
severity: should_fix
---

**Rule.** In dcSCTP socket handover implementations (AddHandoverState and
RestoreFromState), ensure that every field populated into
DcSctpSocketHandoverState during serialization is read and restored in the
corresponding RestoreFromState method, and do not serialize redundant fields
that can be deterministically recomputed upon restoration. When adding new
handover fields or opt-in capabilities, guard restoration behind the
corresponding feature flag and provide fallback reconstruction when restoring
legacy state snapshots where newly added fields are absent or empty.

**Why.** Omitting serialized fields during deserialization leaves restored
components in a default or inconsistent state, while persisting redundant
derivable fields inflates snapshot payloads and risks desynchronization.
Restoring newly added fields without feature flag guards or backward-compatible
fallbacks corrupts socket state when handling legacy snapshots from older
versions where those fields are absent.

## Examples

Bad:

```cpp
// 1. Unconditional restoration ignoring feature flags and legacy snapshots:
void RetransmissionQueue::RestoreFromState(
    Timestamp now, const DcSctpSocketHandoverState& state) {
  outstanding_data_.RestoreFromState(
      now, tsn_unwrapper_.Unwrap(TSN(state.tx.last_cumulative_tsn_ack)), state);
}

// 2. Missing deserialization of a serialized field:
void OutstandingData::RestoreFromState(const DcSctpSocketHandoverState& state) {
  for (const auto& msg : state.tx.outstanding_data) {
    // msg.retransmission_count is ignored and lost!
    outstanding_data_.emplace_back(msg.message_id, std::move(data));
  }
}

// 3. Serializing redundant derivable state:
void RetransmissionQueue::AddHandoverState(DcSctpSocketHandoverState& state) {
  state.tx.next_tsn = next_tsn().value();
  state.tx.last_cumulative_tsn_ack = last_cumulative_tsn_ack_.Wrap().value();
}
```

Good:

```cpp
// 1. Guarding new features and falling back for legacy/empty state:
void RetransmissionQueue::RestoreFromState(
    Timestamp now, const DcSctpSocketHandoverState& state) {
  if (options_.enable_handover_with_outstanding_data) {
    UnwrappedTSN last_ack = state.tx.outstanding_data.empty()
        ? tsn_unwrapper_.Unwrap(TSN(state.tx.next_tsn - 1))
        : tsn_unwrapper_.Unwrap(TSN(state.tx.last_cumulative_tsn_ack));
    outstanding_data_.RestoreFromState(now, last_ack, state);
  } else {
    outstanding_data_.ResetSequenceNumbers(
        tsn_unwrapper_.Unwrap(TSN(state.tx.next_tsn - 1)));
  }
}

// 2. Restoring all populated serialized fields:
void OutstandingData::RestoreFromState(const DcSctpSocketHandoverState& state) {
  for (const auto& msg : state.tx.outstanding_data) {
    outstanding_data_.emplace_back(msg.message_id, std::move(data),
                                   msg.retransmission_count);
  }
}

// 3. Omitting redundant derivable fields from handover state:
void RetransmissionQueue::AddHandoverState(DcSctpSocketHandoverState& state) {
  state.tx.next_tsn = next_tsn().value();
}
```
