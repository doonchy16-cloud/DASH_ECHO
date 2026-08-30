# DASH ECHO v0.6 — Attempt History Design

Status: **APPROVED FOR IMPLEMENTATION / MAIN ONLY / RUNTIME TESTING DEFERRED TO v1.0**

## Goal

Create a first-class, bounded, in-memory attempt-history authority that preserves lightweight attempt facts independently from heavy replay-frame retention.

## Authority boundaries

- `EchoRecorder` remains authority for active attempt capture and retained replay frames.
- `EchoDeathAnalytics` remains authority for death events/clusters/heatmap data.
- `EchoAttemptHistory` becomes authority for immutable finalized-attempt summaries.
- History entries never own or retain pointers to `AttemptRecord` or `DeathEvent` objects.
- Replay availability is resolved dynamically by `attemptId` against `EchoRecorder`; it is not stored as a permanent history fact.

## Attempt-history entry

Each finalized attempt summary records:

- attempt ID
- outcome: Death / Reset / Completed / LayerExit
- source `AttemptEndReason`
- max progress percentage
- duration
- captured frame count
- per-attempt dropped-frame count
- first and last captured sample timestamps
- whether the attempt completed the level
- whether it became the personal best when finalized
- prior best progress
- improvement over prior best
- optional immutable death summary: event ID, player index, death progress, X/Y, hazard object ID and hazard X/Y

## Personal-best semantics

Before an active attempt is finalized, the integration snapshots the prior finalized PB progress. After finalization, history compares the newly finalized attempt against the recorder's current PB identity.

An attempt is `personalBestAtFinalization` only when the recorder identifies that exact attempt ID as current PB after finalization. `improvementPercent` is clamped to zero when no positive gain occurred.

## Retention

History is bounded independently from replay frames:

- maximum retained history entries: 4,096
- oldest history summaries roll out first
- total committed counters remain monotonic even if old summaries roll out

This allows replay frames to be evicted aggressively without erasing recent attempt history and allows history to outlive the recorder's 64-attempt replay window.

## Replay-source lookup

`EchoRecorder` exposes `attemptById(uint64_t)`.

A history consumer determines replay availability by checking whether the recorder still retains that attempt ID and whether the retained attempt has frames. No history entry stores a stale `replayAvailable` boolean.

## Death correlation

`EchoDeathAnalytics` exposes `deathForAttempt(uint64_t)` and returns the matching retained raw death event when available. Attempt history consumes and copies that death information immediately at finalization.

Because history finalization follows immediately after a death/reset lifecycle, the death event is expected to still be in the 4,096-event raw window. If no event is available, the history entry remains valid and records the non-death terminal outcome implied by `AttemptEndReason`.

## Lifecycle

At reset:

1. release fleet references
2. capture prior PB progress
3. capture active attempt ID
4. finalize recorder attempt as `Reset`
5. resolve finalized attempt by ID
6. resolve death event by attempt ID
7. append immutable history summary
8. run normal Geometry Dash reset
9. begin next attempt
10. rebuild fleet

At level completion and layer exit, the same history commit occurs after recorder finalization and before the PlayLayer lifecycle completes.

## Deduplication

History rejects duplicate commits for an already-recorded attempt ID. Attempt IDs are the authoritative deduplication key.

## v0.6 scope exclusions

- no persistent disk history yet
- no history popup/table yet
- no replay timeline UI yet
- no scrub controls
- no cross-session history merge
- no gameplay/runtime testing before v1.0

These remain later milestones so v0.6 stays focused on authoritative history data.

## Source-level acceptance criteria

- attempt summaries survive recorder replay-frame eviction until history's own 4,096-entry cap
- death/reset/completion/exit outcomes are distinguishable
- death context is copied, never referenced by pointer
- PB/improvement semantics are explicit
- replay availability cannot become stale
- duplicate finalization cannot create duplicate history entries
- per-attempt frame dropping is observable
- main remains the only branch

## Verification gate

Per project decision, compile, Geometry Dash launch, gameplay tests, and runtime UI validation remain deferred until v1.0. v0.6 may be labeled **SOURCE IMPLEMENTED**, never runtime PASS.
