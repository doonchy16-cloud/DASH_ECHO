# DASH ECHO v0.7 Replay Timeline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build one owned immutable replay clip with a deterministic 1x timeline and dedicated ghost replay session.

**Architecture:** `EchoReplayTimeline` owns copied attempt/history data and authoritative cursor state. `EchoReplaySession` owns the timeline plus one replay-only `EchoGhost`. PlayLayer prepares the newest finalized attempt as a replay candidate but v0.7 exposes no controls.

**Tech Stack:** C++23, Geode v5.10.1, Geometry Dash Windows 2.2081, Geode CLI 3.7.4.

**Spec:** `docs/superpowers/specs/2026-08-30-v0-7-replay-timeline-design.md`

## Global Constraints

- `main` only; no branches.
- No runtime/build testing before v1.0.
- Replay is visual/read-only and must not own physics/collision/save authority.

---

### Task 1: Replay timeline authority

**Files:** Create `src/EchoReplayTimeline.hpp`, `src/EchoReplayTimeline.cpp`.

- [ ] Define replay states, marker types, marker records, clip metadata, load/clear/start/restart/advance APIs.
- [ ] Validate finalized matching attempt/history IDs and monotonic finite timestamps.
- [ ] Copy attempt/history into owned clip.
- [ ] Build Start/End/Death/Completion/PB markers.
- [ ] Implement clamped cursor, normalized position, and progress interpolation.
- [ ] Commit on `main`.

### Task 2: Replay session renderer

**Files:** Create `src/EchoReplaySession.hpp`, `src/EchoReplaySession.cpp`.

- [ ] Own `EchoReplayTimeline` and one `EchoGhost`.
- [ ] Attach replay ghost to world coordinate space.
- [ ] Load clip and bind ghost to the owned copied attempt.
- [ ] Advance timeline then synchronize ghost from the same cursor.
- [ ] Keep inactive/ready replay ghost hidden.
- [ ] Commit on `main`.

### Task 3: PlayLayer preparation

**Files:** Modify `src/main.cpp`.

- [ ] Add replay session field.
- [ ] Attach replay renderer with existing world render parent.
- [ ] After a history commit, prepare that exact finalized attempt/history entry when replay frames exist.
- [ ] Do not automatically start replay mode.
- [ ] Keep active gameplay clock/recording unchanged.
- [ ] Commit on `main`.

### Task 4: Version/docs/evidence

**Files:** Modify `mod.json`, `CMakeLists.txt`, `README.md`, `about.md`; create `docs/V0_7_REPLAY_TIMELINE_ARCHITECTURE.md`.

- [ ] Set version 0.7.0.
- [ ] Document replay ownership, timeline/marker semantics, integration, and v1.0 runtime obligations.
- [ ] Verify new source files and main-only branch state.
- [ ] Do not compile or launch Geometry Dash.
