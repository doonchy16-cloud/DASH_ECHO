# ECHO_DASH v1.1.0 Multiverse & Personalization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver ECHO_DASH v1.1.0 with 256-ghost scalable rendering, blue Last/gold Best priority identities, persistent per-level archive, PB truth, rich settings, stronger death intelligence presentation, and discoverable Replay Studio UX.

**Architecture:** Keep the current recorder/timeline authority boundaries, but separate current-attempt capture from long-lived archive storage and render selection. Replace the six-slot fleet with a reusable dynamic pool; persist summaries separately from replay-capable tracks; reserve expensive aura/trail effects for Last/Best roles.

**Tech Stack:** C++23, Geode SDK 5.10.1, Geometry Dash 2.2081 bindings, cocos2d/CCDrawNode, Geode settings and save directories, GitHub Actions Windows Release build.

**Spec:** `docs/superpowers/specs/2026-08-30-v1-1-multiverse-personalization-design.md`

## Global Constraints

- Work on `main` only; do not create branches/worktrees.
- User-facing product name is `ECHO_DASH`.
- Preserve Geode ID `doonchy.dash-echo` in v1.1.0 for upgrade/save compatibility.
- Supported ghost ceiling is 256; do not claim 512 support in v1.1.0.
- Geode CLI remains 3.7.4; SDK remains 5.10.1; GD Windows remains 2.2081.
- ECHO_DASH remains read-only with respect to player input, physics, collision, death and completion authority.
- Build PASS and runtime PASS remain separate gates.

---

### Task 1: v1.1 contract regression gate

**Files:**
- Create: `tests/test_v1_1_contract.py`
- Modify: `.github/workflows/build-v1.yml`

**Interfaces:**
- Consumes: repository source/config files.
- Produces: deterministic source/config checks run before Windows build.

- [ ] Write Python `unittest` cases that require `ECHO_DASH`, version `v1.1.0`, legacy ID preservation, ghost max >=256, dynamic fleet source (no fixed six-slot array), required blue/gold settings/defaults, replay archive source files, heatmap overlay source files, recorder sample-rate API, and removal of hard-coded `DASH ECHO v0.9` production strings.
- [ ] Add a GitHub Actions step `python -m unittest tests.test_v1_1_contract -v` before `geode build`.
- [ ] Run the workflow and verify the contract gate fails because v1.1 production code/config has not yet been implemented.

### Task 2: Branding + categorized personalization model

**Files:**
- Modify: `mod.json`
- Modify: `CMakeLists.txt`
- Modify: `README.md`
- Modify: `about.md`
- Create: `changelog.md`
- Modify: production log/UI strings in `src/*.cpp`

**Interfaces:**
- Consumes: existing Geode settings IDs `ghost-count` and `death-markers`.
- Produces: v1.1 settings consumed by `DashEchoPlayLayer`, fleet, overlays, and Replay Studio.

- [ ] Rename visible product to `ECHO_DASH`, version to `v1.1.0`, CMake project to `EchoDash`, while preserving `id: doonchy.dash-echo`.
- [ ] Add categorized settings for Ghosts, Last Attempt, Best Recorded Echo, Trails, Death Intelligence, Replay Studio, History/Performance, and diagnostics.
- [ ] Set `ghost-count` range to 0–256, default 16.
- [ ] Set Last accent default RGB `#4aa3ff`; Best accent default RGB `#ffd54a`.
- [ ] Add sample-rate 30–240 default 120, replay-retention, disk-budget, launcher scale, marker scale, labels, heat strip, x-ray, trail, opacity, and camera/playback defaults.
- [ ] Update README/About/changelog to describe v1.1 truthfully and remove v0.9 release drift.
- [ ] Run contract gate; branding/settings tests should turn green while architecture tests remain red.

### Task 3: Recorder sampling boundary + event sampling

**Files:**
- Modify: `src/EchoRecorder.hpp`
- Modify: `src/EchoRecorder.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- Produces: `void setCaptureSampleRate(double hz)`, `double captureSampleRate() const`, and `captureEventFrame(...)`.
- Preserves: `activeElapsedSeconds()` as the every-update clock.

- [ ] Add configurable regular sampling rate clamped to 30–240 Hz.
- [ ] Advance attempt time/progress on every capture call but only append regular frames when the sampling deadline is reached.
- [ ] Add forced event-frame capture bypassing the regular sampling gate.
- [ ] Call event capture before confirmed death finalization so death-adjacent replay state is preserved.
- [ ] Reduce recorder to a bounded working-set role; archive becomes long-term authority.
- [ ] Build after implementation and fix compile evidence before proceeding.

### Task 4: Persistent summary/replay archive

**Files:**
- Create: `src/EchoReplayArchive.hpp`
- Create: `src/EchoReplayArchive.cpp`
- Modify: `src/EchoRecorder.hpp/.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- Produces: `EchoLevelContext`, `EchoReplayArchive`, `ingest`, `load`, `save`, `replayById`, `latestReplay`, `bestRecordedReplay`, `previousReplayId`, `nextReplayId`, `maxAttemptId`, `summaries`, `replays`.
- Consumes: finalized `AttemptRecord` + `AttemptHistoryEntry`.

- [ ] Maintain up to 4,096 summaries separately from replay-capable attempts.
- [ ] Downsample finalized attempts for archive storage while preserving first/last/discontinuity frames.
- [ ] Default replay retention 512; hard cap 2,048; preserve Best Recorded and Latest during trimming.
- [ ] Implement versioned binary load/save under `Mod::get()->getSaveDir()/echo_dash` with count/size validation and temporary-file replace.
- [ ] Use level ID or stable fallback hash plus platformer/practice flags for archive key.
- [ ] On level init/load, restore archive, rebuild death analytics from death summaries, and move recorder's next attempt ID above persisted max.
- [ ] On finalization, ingest the committed attempt into archive before renderer rebuild.
- [ ] Save archive on safe lifecycle boundaries.

### Task 5: Dynamic 256-ghost pool + Last/Best roles

**Files:**
- Modify: `src/EchoGhost.hpp/.cpp`
- Modify: `src/EchoGhostFleet.hpp/.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- `EchoGhostFleet::rebuild(EchoReplayArchive const&)` replaces recorder-based selection.
- Produces `GhostRole { Older, LastAttempt, BestRecorded, LastAndBest }` and configurable `GhostFleetVisualSettings`.

- [ ] Replace fixed six-slot array with dynamically grown `std::vector<std::unique_ptr<Slot>>`/equivalent stable pool; hard ceiling 256.
- [ ] Rebuild newest-N selection from archive while preserving Best Recorded and Latest.
- [ ] Assign deterministic role per selected attempt.
- [ ] Add lazy priority aura nodes to `EchoGhost`; Last uses blue, Best gold, LastAndBest uses blue outer + gold inner identity.
- [ ] Add shared bounded priority-trail `CCDrawNode` to fleet; only Last/Best trails by default.
- [ ] Apply configurable opacity/age fade and priority settings.
- [ ] Expose fleet diagnostics: configured, selected, active, latest ID, best-recorded ID.
- [ ] Build and fix compiler/API evidence before continuing.

### Task 6: PB truth + Replay Studio history/discoverability

**Files:**
- Modify: `src/EchoReplaySession.hpp/.cpp`
- Modify: `src/EchoReplayControls.hpp/.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- Replay session receives archive selection operations without owning archive time.
- Replay controls receive/update `gdLevelBestPercent`, `bestRecordedPercent`, `sessionBestPercent`.

- [ ] Read classic GD PB from `GJGameLevel::getNormalPercent()`; never substitute archive/session values.
- [ ] Track session-best separately.
- [ ] Load latest persisted replay on level entry when available.
- [ ] Add previous/next archived replay selection.
- [ ] Replace tiny text launcher with `ButtonSprite("ECHO_DASH")` and configurable scale.
- [ ] Add first-replay-ready hint.
- [ ] Add `SETTINGS` control using `geode::openSettingsPopup(Mod::get())`.
- [ ] Show GD PB, Best Echo and Session Best in Replay Studio.
- [ ] Add default playback-rate and camera-mode application from settings.

### Task 7: Death marker redesign + heat strip

**Files:**
- Modify: `src/EchoDeathOverlay.hpp/.cpp`
- Create: `src/EchoHeatmapOverlay.hpp`
- Create: `src/EchoHeatmapOverlay.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- World death overlay consumes `EchoDeathAnalytics` + display settings.
- Heatmap overlay consumes the same analytics but renders in screen space.

- [ ] Make first death visible using larger concentric hotspot/crosshair.
- [ ] Add configurable labels showing `DEATH <percent>%` for count 1 and `xN <percent>%` for repeats.
- [ ] Add configurable marker scale and x-ray z-order.
- [ ] Implement 100-bucket screen-space heat strip with configurable opacity.
- [ ] Ensure death markers and heat strip can be toggled independently without disabling analytics.

### Task 8: Settings application, diagnostics, CI/package verification

**Files:**
- Modify: `src/main.cpp`
- Modify: `.github/workflows/build-v1.yml`
- Modify: docs as needed

**Interfaces:**
- `applyEchoDashSettings()` translates Geode settings into recorder/fleet/overlay/session configuration.

- [ ] Poll settings at a bounded interval and apply safe live changes.
- [ ] Implement visual-profile mapping and rendering-quality bounds without violating explicit hard settings.
- [ ] Add optional compact diagnostics label with active/selected ghosts, replay count, summaries, frames/dropped frames.
- [ ] Run contract tests until all pass.
- [ ] Run pinned Windows Release workflow until terminal success.
- [ ] Verify exactly one `.geode` artifact, embedded `ECHO_DASH v1.1.0`, legacy ID, GD 2.2081 and Geode 5.10.1 metadata.
- [ ] Do not claim runtime PASS. Produce the v1.1.0 candidate and a focused runtime test matrix for 16/64/128/256 ghosts, persistence, blue/gold roles, Replay Studio, death overlays, and camera modes.
