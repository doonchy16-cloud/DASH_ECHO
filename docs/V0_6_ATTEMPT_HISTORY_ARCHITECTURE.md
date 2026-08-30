# DASH ECHO v0.6 — Attempt History Architecture

Status: **SOURCE IMPLEMENTED / RUNTIME VERIFICATION DEFERRED TO v1.0**

## Mission

v0.6 introduces an immutable, bounded attempt-history authority that preserves lightweight attempt facts independently from replay-frame retention.

## Authority split

- `EchoRecorder` owns active capture and retained replay frames.
- `EchoDeathAnalytics` owns death-event truth and aggregate death intelligence.
- `EchoAttemptHistory` owns finalized attempt summaries.
- `EchoGhostFleet` remains presentation-only.

History never stores pointers to recorder attempts or death events. It copies immutable summary data at finalization.

## AttemptHistoryEntry

Each committed entry contains:

- attempt ID
- resolved outcome: Death / Reset / Completed / LayerExit
- source `AttemptEndReason`
- maximum progress
- duration
- captured frame count
- dropped frame count
- first/last captured timestamps
- completion flag
- PB-at-finalization flag
- prior best progress
- positive improvement amount
- optional copied death/hazard summary

## Replay availability

Replay availability is deliberately not stored as a boolean. Consumers resolve it dynamically:

1. read the history entry's `attemptId`
2. call `EchoRecorder::attemptById(attemptId)`
3. require a retained finalized attempt with non-empty frames

This prevents stale history state after recorder retention evicts heavy replay frames.

## Death correlation

`EchoDeathAnalytics::deathForAttempt(attemptId)` resolves the recent raw death event while the attempt is being finalized. History copies the event ID, player index, progress, position, and hazard context into the immutable history entry.

A retained death event takes precedence over a generic Reset outcome, allowing the history ledger to distinguish an actual death/reset cycle from a manual reset.

## Personal-best semantics

Before finalization, integration snapshots the prior recorder PB progress. After finalization, the recorder recomputes current PB identity. History marks the new entry as PB only when its exact attempt ID is the recorder's current PB.

Equal-progress PB ties follow recorder authority: newer tied attempts replace older tied attempts as current PB.

## Retention

- retained history entries: maximum 4,096
- replay attempts: recorder remains capped independently at 64 plus frame limits
- current PB history entry is pinned
- when over capacity, oldest non-PB history entry is evicted first
- aggregate history counters remain monotonic even after detail eviction

This lets history outlive replay data while preserving the most important historical run.

## Deduplication

Attempt ID is the history commit key. Duplicate commits are rejected and counted diagnostically.

## Lifecycle

All attempt-finalization paths use one helper in `DashEchoPlayLayer`:

1. capture active attempt ID
2. snapshot prior PB progress
3. finalize recorder attempt
4. resolve finalized attempt by ID
5. resolve current recorder PB identity
6. resolve recent death by attempt ID
7. commit immutable history entry

Reset additionally releases ghost-fleet references before finalization/retention mutation and rebuilds the fleet only after the new attempt begins.

## Diagnostics

Session-close logging now includes retained/committed history counts, deaths, completions, manual resets, current PB identity/progress, longest attempt, retained/dropped frames, ghost limit, and death-cluster count.

## v0.6 exclusions

- no disk persistence
- no history browser UI
- no cross-session merge
- no timeline cursor UI
- no playback controls

Those responsibilities begin in v0.7/v0.8.

## Runtime obligations deferred to v1.0

- verify exact reset/death callback ordering
- verify completion finalization timing
- verify platformer progress semantics
- verify long-session retention behavior in game
- verify diagnostics and Geode lifecycle interaction

No runtime PASS is claimed before those tests.
