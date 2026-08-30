# DASH ECHO v0.8 Playback Controls Implementation Plan

> **For agentic workers:** Execute directly on the authoritative `main` branch for this project. Do not create branches or worktrees. Runtime/build testing is deferred to v1.0 by explicit project rule.

**Goal:** Add interactive Replay Studio controls, playback-rate authority, normalized scrubbing, and frame stepping on top of the v0.7 owned replay timeline.

**Architecture:** `EchoReplayTimeline` remains the sole replay-time authority. `EchoReplaySession` delegates commands and re-synchronizes the dedicated ghost. `EchoReplayControls` is presentation-only. `DashEchoPlayLayer` owns Studio-mode isolation and freezes DASH ECHO active-attempt recording while replay review is open.

**Tech Stack:** C++23, Geode v5.10.1 SDK/loader baseline, Geode CLI 3.7.4 tooling, Geometry Dash Windows 2.2081.

**Spec:** `docs/superpowers/specs/2026-08-30-v0-8-playback-controls-design.md`

## Global Constraints

- Authoritative branch: `main` only.
- No feature branches or worktrees.
- No build, Geometry Dash launch, or gameplay testing before v1.0.
- UI never owns a parallel replay clock.
- Playback rates: 0.10x, 0.25x, 0.50x, 1.00x, 2.00x only.
- Seeking/stepping pauses replay.
- Replay Studio must never mutate GD save/account data, collision, or physics authority.

---

### Task 1: Timeline control authority

**Files:**
- Modify: `src/EchoReplayTimeline.hpp`
- Modify: `src/EchoReplayTimeline.cpp`

**Produces:** pause/resume/toggle, playback-rate presets, absolute/normalized seek, distinct-frame stepping, authoritative cursor state.

- [ ] Add `Paused` timeline state and five playback-rate presets.
- [ ] Add pause/resume/toggle and rate methods.
- [ ] Add seek by seconds and normalized position; seeking pauses.
- [ ] Add previous/next distinct-frame stepping; stepping pauses.
- [ ] Multiply sanitized replay `dt` by timeline-owned playback rate only.
- [ ] Keep all seeks clamped to owned clip bounds.

### Task 2: Replay session command delegation

**Files:**
- Modify: `src/EchoReplaySession.hpp`
- Modify: `src/EchoReplaySession.cpp`

**Produces:** session-level control methods that always re-synchronize the dedicated replay ghost after cursor/state changes.

- [ ] Add pause/resume/toggle/rate/seek/step methods.
- [ ] Synchronize ghost after every command that can move cursor.
- [ ] Preserve loaded candidate when stopped/closed.

### Task 3: Presentation-only Replay Studio controls

**Files:**
- Create: `src/EchoReplayControls.hpp`
- Create: `src/EchoReplayControls.cpp`

**Produces:** ECHO launcher, bottom control panel, labels, buttons, native GD Slider scrubber, and open/close callback.

- [ ] Create launcher and panel on a screen-space node attached to PlayLayer.
- [ ] Add play/pause, restart, previous-frame, next-frame, speed-cycle, close controls.
- [ ] Create native `Slider`; read `SliderThumb::getValue()` in callback.
- [ ] Derive slider/time/progress/speed labels from timeline authority during refresh.
- [ ] Keep launcher hidden unless a replay candidate is loaded.

### Task 4: PlayLayer Studio isolation

**Files:**
- Modify: `src/main.cpp`

**Produces:** Studio mode that advances replay only, freezes DASH ECHO active-attempt recording time, hides fleet while open, and resumes fleet synchronization when closed.

- [ ] Add replay controls and `replayStudioOpen` field.
- [ ] Lazily attach controls to PlayLayer.
- [ ] When Studio is open, skip normal `PlayLayer::postUpdate`, advance only replay, refresh controls, and return.
- [ ] Hide multighost fleet on open; re-synchronize fleet from recorder clock on close.
- [ ] Exclude Studio-mode callbacks from DASH ECHO death analytics capture.
- [ ] Force Studio closed before reset/completion/exit lifecycle transitions.

### Task 5: Version/docs/source audit

**Files:**
- Modify: `mod.json`
- Modify: `CMakeLists.txt`
- Modify: `README.md`
- Modify: `about.md`
- Create: `docs/V0_8_PLAYBACK_CONTROLS_ARCHITECTURE.md`

- [ ] Set project/mod version to `0.8.0` / `v0.8.0`.
- [ ] Document control/time authority, Studio isolation, and runtime uncertainties.
- [ ] Verify repository still exposes only `main`.
- [ ] Perform source-only adversarial review; do not claim runtime PASS.
