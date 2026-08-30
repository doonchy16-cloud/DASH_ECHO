# DASH ECHO v0.6 Attempt History Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a bounded immutable attempt-history ledger that preserves finalized attempt facts independently from replay-frame retention.

**Architecture:** Extend recorder/death authorities only with lookup data needed by history, then add `EchoAttemptHistory` as a copy-based summary ledger. `PlayLayer` commits history immediately after each recorder finalization. History stores attempt IDs rather than replay pointers so later UI can resolve current replay availability dynamically.

**Tech Stack:** C++23, Geode SDK/loader v5.10.1, Geometry Dash Windows 2.2081, Geode CLI 3.7.4.

**Spec:** `docs/superpowers/specs/2026-08-30-v0-6-attempt-history-design.md`

## Global Constraints

- Authoritative branch: `main` only; create no branches.
- Do not modify Geometry Dash saves, account state, physics, or collision authority.
- No compile, launch, gameplay, or runtime testing until v1.0 by project decision.
- Source milestone may be called SOURCE IMPLEMENTED, never runtime PASS.
- History retention: 4,096 entries.

---

### Task 1: Recorder history-support metadata

**Files:**
- Modify: `src/EchoRecorder.hpp`
- Modify: `src/EchoRecorder.cpp`

**Interfaces:**
- Produces: `AttemptRecord::framesDropped`, `EchoRecorder::attemptById(std::uint64_t) const`.

- [ ] Add per-attempt `framesDropped` to `AttemptRecord`.
- [ ] Increment it whenever the attempt frame cap drops a sample.
- [ ] Add const lookup by attempt ID without changing retention ownership.
- [ ] Source-review lookup and drop accounting for invalid/stale-pointer behavior.
- [ ] Commit on `main`.

### Task 2: Death-event attempt lookup

**Files:**
- Modify: `src/EchoDeathAnalytics.hpp`
- Modify: `src/EchoDeathAnalytics.cpp`

**Interfaces:**
- Produces: `DeathEvent const* deathForAttempt(std::uint64_t attemptId) const`.

- [ ] Add reverse lookup over retained raw death events.
- [ ] Return null for attempt ID zero or missing/aged-out raw events.
- [ ] Preserve aggregate analytics authority unchanged.
- [ ] Commit on `main`.

### Task 3: Attempt-history ledger

**Files:**
- Create: `src/EchoAttemptHistory.hpp`
- Create: `src/EchoAttemptHistory.cpp`

**Interfaces:**
- Consumes: finalized `AttemptRecord`, optional `DeathEvent`, prior PB progress, current PB attempt ID.
- Produces: `AttemptHistoryEntry`, `AttemptHistoryStats`, `commitFinalizedAttempt(...)`, `entryForAttempt(...)`, `entries()`, `stats()`.

- [ ] Define outcome and immutable death-summary types.
- [ ] Define bounded 4,096-entry history storage and monotonic counters.
- [ ] Implement duplicate-attempt rejection.
- [ ] Implement outcome resolution with Death taking precedence over Reset when a death event exists.
- [ ] Implement PB-at-finalization and positive improvement calculation.
- [ ] Copy replay capture metadata without storing recorder/death pointers.
- [ ] Trim oldest summaries only after committing aggregate counters.
- [ ] Commit on `main`.

### Task 4: PlayLayer lifecycle integration

**Files:**
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: recorder, death analytics, attempt history.
- Produces: exactly one history commit for every finalized attempt.

- [ ] Add `EchoAttemptHistory` to PlayLayer fields.
- [ ] Add one helper that snapshots prior PB, finalizes the active attempt, resolves death context, and commits history.
- [ ] Use helper from reset, level completion, and layer exit.
- [ ] Preserve fleet reference release before retention mutation.
- [ ] Keep normal Geometry Dash lifecycle calls in their existing relative order.
- [ ] Extend exit diagnostics with history stats.
- [ ] Commit on `main`.

### Task 5: Version/docs and source audit

**Files:**
- Modify: `mod.json`
- Modify: `CMakeLists.txt`
- Modify: `README.md`
- Modify: `about.md`
- Create: `docs/V0_6_ATTEMPT_HISTORY_ARCHITECTURE.md`

**Interfaces:**
- Produces: v0.6.0 source milestone documentation.

- [ ] Set mod/project version to `0.6.0`.
- [ ] Document ledger fields, retention, lifecycle, PB semantics, death correlation, and replay availability.
- [ ] Document runtime uncertainties and v1.0 verification obligations.
- [ ] Verify GitHub source tree contains new history files.
- [ ] Verify GitHub branch list still contains only `main`.
- [ ] Do not compile or launch Geometry Dash.
