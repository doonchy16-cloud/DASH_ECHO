# DASH ECHO v0.9 Cinematic Camera Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add deterministic cinematic Replay Studio camera modes on top of the recorded v0.8 viewport without creating a second replay clock or touching live-game camera authority outside Studio.

**Architecture:** `EchoReplayTimeline` remains replay-data/time authority and gains interpolated player sampling. `EchoCinematicCamera` consumes timeline-owned camera/player/history data and outputs a pointer-free `CameraPose`. `DashEchoPlayLayer` alone applies the pose; `EchoReplayControls` only issues camera-mode commands.

**Tech Stack:** C++23, Geode SDK v5.10.1, Geometry Dash 2.2081 bindings, Cocos2d camera transform through `m_objectLayer`.

**Spec:** `docs/superpowers/specs/2026-08-30-v0-9-cinematic-camera-design.md`

## Global Constraints

- Authoritative branch is `main` only; do not create branches.
- No Geometry Dash save/account mutation.
- No gameplay input, collision, physics, or completion authority changes.
- `EchoReplayTimeline` remains the sole replay-time authority.
- Runtime/build verification remains deferred until the v1.0 gate.
- Invalid cinematic calculations fall back to the recorded v0.8 viewport.

---

### Task 1: Replay player sampling

**Files:**
- Modify: `src/EchoReplayTimeline.hpp`
- Modify: `src/EchoReplayTimeline.cpp`

**Interfaces:**
- Produces: `PlayerSnapshot playerAtCursor(std::uint8_t playerIndex) const`
- Produces: `PlayerSnapshot playerAtTime(std::uint8_t playerIndex, double timeSeconds) const`

- [ ] Add timeline-level player sampling declarations.
- [ ] Implement timestamp bracketing with the same recorded continuity flags used by ghost interpolation.
- [ ] Interpolate position, scale, rotation and color only across continuous segments; otherwise snap to the earlier sample.
- [ ] Preserve mode/visibility/presence semantics without consulting ghost nodes.

### Task 2: Cinematic camera engine

**Files:**
- Create: `src/EchoCinematicCamera.hpp`
- Create: `src/EchoCinematicCamera.cpp`

**Interfaces:**
- Consumes: `EchoReplayTimeline const&`
- Produces: `CameraPose evaluate(EchoReplayTimeline const& timeline)`
- Produces: `CinematicCameraMode mode() const`
- Produces: `void cycleMode(bool deathAvailable)`
- Produces: `void setMode(CinematicCameraMode mode)`
- Produces: `void reset()`

- [ ] Define modes `Recorded`, `Follow`, `Smooth`, `Drone`, `DynamicZoom`, `DeathCam` and pointer-free `CameraPose`.
- [ ] Implement Recorded as exact v0.8 camera baseline.
- [ ] Implement single/dual Follow target selection.
- [ ] Implement deterministic Smooth state with snap reset on backward/large seek, source change or mode change.
- [ ] Implement bounded Drone look-ahead from neighboring timeline player samples.
- [ ] Implement bounded Dynamic Zoom based on speed/dual separation while preserving recorded rotation.
- [ ] Implement Death Cam from real history death position/time and skip/fallback when death metadata is unavailable.
- [ ] Make every invalid calculation return/fall back to the recorded baseline.

### Task 3: Replay session camera authority

**Files:**
- Modify: `src/EchoReplaySession.hpp`
- Modify: `src/EchoReplaySession.cpp`

**Interfaces:**
- Produces: `CameraPose cameraPose() const`
- Produces: `CinematicCameraMode cameraMode() const`
- Produces: `void cycleCameraMode()`
- Produces: `void resetCameraMode()`

- [ ] Own one `EchoCinematicCamera` inside the replay session.
- [ ] Reset cinematic state on load/restart/seek/frame-step/source change.
- [ ] Keep camera evaluation subordinate to the existing timeline cursor.

### Task 4: Replay Studio camera controls

**Files:**
- Modify: `src/EchoReplayControls.hpp`
- Modify: `src/EchoReplayControls.cpp`

**Interfaces:**
- Consumes: replay-session camera-mode commands/state.

- [ ] Add one camera-mode cycle button/label.
- [ ] Display canonical mode names.
- [ ] Skip Death Cam in the cycle when loaded history has no death event.
- [ ] Keep UI presentation-only; no local camera pose/cursor state.

### Task 5: PlayLayer camera application and v0.9 metadata

**Files:**
- Modify: `src/main.cpp`
- Modify: `mod.json`
- Modify: `CMakeLists.txt`
- Modify: `README.md`
- Modify: `about.md`
- Create: `docs/V0_9_CINEMATIC_CAMERA_ARCHITECTURE.md`

- [ ] Replace direct `cameraAtCursor()` application with `replay.cameraPose()`.
- [ ] Preserve v0.8 active-viewport save/restore exactly.
- [ ] Ensure cinematic pose applies only while Replay Studio is open.
- [ ] Version to `v0.9.0`.
- [ ] Document source-level acceptance boundaries and v1.0 runtime targets.

## Deferred verification

No build or runtime command is executed during v0.9. The v1.0 release gate will run the first compile/build verification and fix every resulting source/API defect before release-candidate status is claimed.
