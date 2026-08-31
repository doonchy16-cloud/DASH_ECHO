# ECHO_DASH Unified Ghost Playback Engine Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace role-specific ghost synchronization with one canonical playback engine and continue every selected historical ghost after live-player death until its own recorded attempt ends.

**Architecture:** Introduce a role-agnostic `EchoGhostPlaybackEngine` with Tracking and Continuing phases. `EchoGhostFleet` owns one engine and uses it for every slot; `GhostRole` remains presentation-only. `main.cpp` orchestrates confirmed-death continuation and defers Geometry Dash reset until the fleet reports all selected replay timelines finished.

**Tech Stack:** C++23, Geode CLI 3.7.4, Geode SDK 5.10.1, Geometry Dash Windows 2.2081, Python `unittest` contract tests, GitHub Actions Windows Release build.

**Spec:** `docs/superpowers/specs/2026-08-30-v1-1-unified-ghost-playback-engine-design.md`

## Global Constraints

- Work directly on `main`; do not create branches or worktrees.
- Preserve internal mod ID `doonchy.dash-echo` and visible version `v1.1.1` during runtime certification.
- Preserve 0–256 ghost support.
- Preserve archive pointer safety: no archive mutation while fleet slots reference archive-owned replay records.
- Ghost role may affect presentation only; timing must be role-agnostic.
- Runtime PASS requires user evidence; CI/package green alone is not runtime PASS.

---

### Task 1: Strengthen the regression contract

**Files:**
- Modify: `tests/test_v1_1_contract.py`

**Interfaces:**
- Consumes: current v1.1.1 source tree.
- Produces: a RED contract requiring `EchoGhostPlaybackEngine`, role-agnostic timing, continuation APIs, and deferred reset lifecycle.

- [ ] **Step 1: Replace the obsolete Best-only alignment contract**

Add assertions equivalent to:

```python
engine_h = self.read("src/EchoGhostPlaybackEngine.hpp")
engine_cpp = self.read("src/EchoGhostPlaybackEngine.cpp")
fleet_h = self.read("src/EchoGhostFleet.hpp")
fleet_cpp = self.read("src/EchoGhostFleet.cpp")
main = self.read("src/main.cpp")

self.assertIn("class EchoGhostPlaybackEngine", engine_h)
self.assertIn("GhostPlaybackPhase", engine_h)
self.assertIn("Tracking", engine_h)
self.assertIn("Continuing", engine_h)
self.assertNotIn("GhostRole", engine_h + engine_cpp)
self.assertIn("EchoGhostPlaybackEngine m_playbackEngine", fleet_h)
self.assertIn("beginContinuation", fleet_h)
self.assertIn("advanceContinuation", fleet_h)
self.assertIn("continuationComplete", fleet_h)
self.assertNotIn("alignBestIdentity", fleet_cpp)
self.assertNotIn("carriesBestIdentity", fleet_cpp)
self.assertIn("deferredResetRequested", main)
self.assertIn("beginContinuation", main)
self.assertIn("continuationComplete", main)
```

- [ ] **Step 2: Run contract to prove RED**

Run through the existing GitHub Actions push gate.

Expected: contract failure before SDK/compiler steps because playback-engine files/APIs do not yet exist.

- [ ] **Step 3: Commit**

Commit message:

```text
test: require unified ghost playback engine
```

---

### Task 2: Add the canonical playback engine

**Files:**
- Create: `src/EchoGhostPlaybackEngine.hpp`
- Create: `src/EchoGhostPlaybackEngine.cpp`

**Interfaces:**
- Consumes: `AttemptRecord` / `FrameRecord` from `EchoRecorder.hpp`.
- Produces:
  - `enum class GhostPlaybackPhase { Tracking, Continuing };`
  - `void reset();`
  - `void track(double liveElapsedSeconds, float liveProgressPercent, bool progressAuthority);`
  - `void beginContinuation(double liveElapsedSeconds, float liveProgressPercent, bool progressAuthority);`
  - `void advance(double dt);`
  - `double resolveTime(AttemptRecord const&, bool progressAlignmentSafe) const;`
  - `bool finished(AttemptRecord const&, bool progressAlignmentSafe) const;`
  - `bool isContinuing() const;`
  - `static bool supportsProgressAlignment(AttemptRecord const&);`

- [ ] **Step 1: Implement phase state and finite/clamped inputs**

Use one state object for all ghosts. `track()` updates the canonical live tuple and sets phase Tracking. `beginContinuation()` stores the death anchor tuple and resets continuation elapsed to zero. `advance()` accepts only finite non-negative `dt` clamped to `0.25` seconds per update.

- [ ] **Step 2: Move progress mapping into the engine**

Move the current monotonic-progress check and binary-search progress-to-time interpolation out of `EchoGhostFleet`. The engine must have no dependency on `GhostRole`.

Tracking source-time rule:

```cpp
if (m_progressAuthority && progressAlignmentSafe) {
    return timeForProgress(attempt, m_anchorProgressPercent, m_anchorElapsedSeconds);
}
return m_anchorElapsedSeconds;
```

Continuing source-time rule:

```cpp
return trackingAnchorTime(attempt, progressAlignmentSafe) + m_continuationElapsedSeconds;
```

If live/death progress is beyond an attempt's last recorded progress, return a value greater than the attempt's final timestamp so `EchoGhost` hides it.

- [ ] **Step 3: Implement completion**

`finished()` returns true when the resolved source time is greater than the attempt's final recorded timestamp. Empty/invalid attempts are treated as finished.

- [ ] **Step 4: Commit**

Commit message:

```text
feat: add unified ghost playback engine
```

---

### Task 3: Make fleet use exactly one engine for every ghost

**Files:**
- Modify: `src/EchoGhostFleet.hpp`
- Modify: `src/EchoGhostFleet.cpp`

**Interfaces:**
- Consumes: `EchoGhostPlaybackEngine` from Task 2.
- Produces:
  - `void track(double liveElapsedSeconds, float progressPercent, bool progressAuthority);`
  - `bool beginContinuation(double liveElapsedSeconds, float progressPercent, bool progressAuthority);`
  - `void advanceContinuation(double dt);`
  - `bool isContinuing() const;`
  - `bool continuationComplete() const;`

- [ ] **Step 1: Add one engine member**

`EchoGhostFleet` contains exactly one:

```cpp
EchoGhostPlaybackEngine m_playbackEngine;
```

Each slot keeps only the cached `progressAlignmentSafe` bit plus its last resolved source time for trail rendering.

- [ ] **Step 2: Remove fleet-owned timing algorithms**

Delete `progressIsMonotonic`, `timeForProgress`, `synchronize(double)`, and the role-specific `alignBestIdentity` / `carriesBestIdentity` synchronization path.

- [ ] **Step 3: Route every active slot through the same engine**

Create a private `renderFromPlaybackEngine()` loop:

```cpp
for (std::size_t i = 0; i < m_activeGhosts; ++i) {
    auto& slot = *m_slots[i];
    if (!slot.attempt) continue;
    slot.synchronizedTimeSeconds = m_playbackEngine.resolveTime(
        *slot.attempt,
        slot.progressAlignmentSafe
    );
    slot.ghost.synchronize(slot.synchronizedTimeSeconds);
}
rebuildPriorityTrails();
```

There must be no `GhostRole` condition in this timing loop.

- [ ] **Step 4: Implement continuation completion**

`continuationComplete()` is true only when every active selected attempt is finished according to the same engine. If no active ghosts can continue, `beginContinuation()` returns false and resets the engine.

- [ ] **Step 5: Preserve visual roles only**

Keep blue/gold/older role logic solely in opacity/trail presentation.

- [ ] **Step 6: Commit**

Commit message:

```text
refactor: run every ghost on shared engine
```

---

### Task 4: Integrate confirmed-death continuation into PlayLayer

**Files:**
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: fleet Tracking/Continuing APIs from Task 3.
- Produces: one death-continuation lifecycle with deferred reset.

- [ ] **Step 1: Add lifecycle state**

Add fields:

```cpp
bool confirmedDeath = false;
bool deferredResetRequested = false;
```

Reset both in `startNewAttempt()`.

- [ ] **Step 2: Feed live frames into the shared Tracking engine**

After recorder capture on a live attempt:

```cpp
m_fields->fleet.track(
    m_fields->recorder.activeElapsedSeconds(),
    this->getCurrentPercent(),
    !m_fields->levelContext.platformer
);
```

Use the same call after fleet rebuild/reset and after closing Replay Studio.

- [ ] **Step 3: Arm continuation only on confirmed death**

After `PlayLayer::destroyPlayer()` proves a new real death and after the death event frame/analytics are captured:

```cpp
m_fields->confirmedDeath = true;
m_fields->captureEnabled = false;
m_fields->fleet.beginContinuation(
    recorder.activeElapsedSeconds(),
    candidate.progressPercent,
    !m_fields->levelContext.platformer
);
```

Do not finalize or mutate the archive here.

- [ ] **Step 4: Advance continuation from `postUpdate()`**

Still call the original `PlayLayer::postUpdate(dt)` so Geometry Dash's death state can advance. If fleet is Continuing, do not record new live frames. Instead:

```cpp
m_fields->fleet.advanceContinuation(safeDt);
if (m_fields->fleet.continuationComplete()) {
    performResetLifecycle();
}
return;
```

- [ ] **Step 5: Defer reset while ghosts are continuing**

Factor the existing reset body into `performResetLifecycle()`.

`resetLevel()` becomes:

```cpp
if (m_fields->fleet.isContinuing()) {
    m_fields->deferredResetRequested = true;
    return;
}
performResetLifecycle();
```

`performResetLifecycle()` must preserve pointer safety order:

1. close Replay Studio
2. `fleet.stop()`
3. finalize/persist active attempt
4. call original `PlayLayer::resetLevel()`
5. restore capture and archive context/settings
6. start the new attempt
7. rebuild fleet
8. feed Tracking state at time/progress zero

- [ ] **Step 6: Preserve exit/completion behavior**

Level completion does not enter continuation. Layer exit cancels continuation through `fleet.stop()` before archive finalization/mutation.

- [ ] **Step 7: Commit**

Commit message:

```text
fix: continue all ghosts after player death
```

---

### Task 5: Verify contract, compiler, package, and release candidate

**Files:**
- Modify only if compiler evidence proves a defect.
- Package from GitHub Actions artifact after terminal green.

**Interfaces:**
- Consumes: completed source from Tasks 1–4.
- Produces: independently verified same-version v1.1.1 unified-engine hotfix package.

- [ ] **Step 1: Require final contract GREEN**

Run:

```text
python -m unittest tests.test_v1_1_contract -v
```

via the existing GitHub Actions job. Expected: all tests PASS.

- [ ] **Step 2: Require pinned Windows build GREEN**

Require terminal success for:

- Geode CLI 3.7.4
- Geode SDK 5.10.1
- Windows Release compile
- compiler evidence upload
- exactly-one `.geode` collection
- candidate upload

- [ ] **Step 3: Inspect artifact independently**

Verify:

- outer artifact ZIP integrity
- inner `.geode` ZIP integrity
- metadata `id = doonchy.dash-echo`
- metadata `name = ECHO_DASH`
- metadata `version = v1.1.1`
- Geode `5.10.1`
- GD Windows `2.2081`
- x64 PE DLL

- [ ] **Step 4: Re-add the approved ECHO_DASH icon without changing compiler-produced payloads**

Embed the previously approved `logo.png`, byte-compare every original CI entry, and record SHA-256 values.

- [ ] **Step 5: Build animated elevated installer**

Target the proven active directory:

```text
C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash\geode\mods
```

Installer must match packages by embedded ID, back up all matches, install exactly one cryptographically verified candidate, preserve rollback, and identify this same-version hotfix by package/DLL hash rather than version string alone.

- [ ] **Step 6: Runtime HOLD**

Do not call runtime PASS. Ask Doonchy to verify:

1. all ghosts are aligned from the same engine while alive;
2. after death, every historical ghost continues instead of freezing;
3. continuation lasts until each selected historical attempt ends;
4. the next attempt resets only after continuation completes;
5. blue/gold roles remain visual only.