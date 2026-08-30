# DASH ECHO v0.9 — Cinematic Camera Design

Status: **PROPOSED / READY FOR USER GO / NOT IMPLEMENTED**

## Goal

Add cinematic replay-camera modes to Replay Studio without changing replay-time authority, live-attempt gameplay authority, or the v0.8 guarantee that the active camera is restored exactly when Studio closes.

## Architecture choice

### Recommended: cinematic transform over recorded viewport baseline

v0.8 already records the Geometry Dash object-layer camera transform per replay frame. v0.9 should treat `EchoReplayTimeline::cameraAtCursor()` as the baseline camera truth and calculate a reversible cinematic transform on top.

This is preferred over:

1. **Hooking internal GD camera-control methods directly** — higher compatibility risk and more likely to fight camera triggers/other mods.
2. **Building a separate render-to-texture camera** — far more complexity than required and unnecessary for v1.0.

The cinematic engine therefore outputs a `CameraPose`; `DashEchoPlayLayer` remains the only place that applies that pose to the Replay Studio viewport.

## Authority hierarchy

```text
ReplayClip frames
      ↓
EchoReplayTimeline cursor
      ↓
Recorded CameraSnapshot baseline
      ↓
EchoCinematicCamera transform
      ↓
CameraPose
      ↓
DashEchoPlayLayer applies pose
```

The camera engine never owns replay time.

## New component: EchoCinematicCamera

Proposed responsibilities:

- consume the current timeline cursor
- consume recorded baseline camera pose
- consume interpolated replay player position/state
- select one camera mode
- calculate final camera pose
- maintain only visual smoothing state where a mode requires it
- reset smoothing deterministically after backward seek, large seek, restart, or mode change
- never mutate replay frames
- never mutate live-attempt recorder state

## Camera modes

### 1. Recorded

Exact v0.8 recorded viewport reproduction.

This is the deterministic fallback and compatibility mode.

### 2. Follow

Center camera framing around the replay subject while preserving baseline scale/rotation unless explicitly overridden.

Target selection:

- player 1 when only player 1 is present
- midpoint of player 1/player 2 when dual players are both present
- available player when only one dual player is visible/present
- recorded baseline when no replay player target is usable

### 3. Smooth

Follow target with cinematic damping rather than hard centering.

Rules:

- smoothing is visual state, not replay time
- backward/non-monotonic seek snaps smoothing state to the new deterministic target
- large cursor jumps snap rather than animating across unrelated replay sections
- pause leaves the camera stable

### 4. Drone

Look ahead in the replay travel direction.

Direction/velocity are derived from neighboring recorded player samples; no gameplay velocity injection is required.

Look-ahead distance is bounded so teleports/portals cannot launch the camera far away.

### 5. Dynamic Zoom

Adjust framing from replay context without mutating GD physics.

Candidate inputs:

- replay subject speed
- dual-player separation
- death proximity
- baseline recorded zoom

Zoom must remain bounded and return to baseline deterministically on mode exit.

### 6. Death Cam

When the loaded history entry has a death marker, frame the death location near the terminal replay window.

Camera behavior may include:

- tighter framing approaching death
- short ease toward the recorded death position
- hold/focus at terminal frame

If the product preset includes slow motion, **Replay Studio UI/session** changes the authoritative timeline rate (for example to 0.25x). `EchoCinematicCamera` itself never modifies playback rate.

## Replay player sampling requirement

v0.9 should add a timeline-level interpolated replay-player query, for example:

- `playerAtCursor(1)`
- `playerAtCursor(2)`

It must use the same continuity rules already recorded for v0.3 ghost interpolation.

The camera engine must not inspect `EchoGhost` visual nodes to discover player position; recorded replay data remains authority.

## Deterministic seek behavior

Cinematic smoothing can become history-dependent unless seek boundaries are explicit.

The camera engine should track the last synchronized replay cursor and treat the following as discontinuities:

- cursor moves backward
- cursor jump exceeds a bounded threshold
- timeline restart
- camera mode change
- source replay attempt changes

At a discontinuity, cinematic state snaps to the target pose before future smoothing resumes.

## Replay Studio controls

v0.9 should extend `EchoReplayControls` with presentation-only camera controls:

- camera-mode cycle button
- current camera-mode label
- optional reset-to-Recorded control if needed for clarity

Suggested cycle:

`Recorded → Follow → Smooth → Drone → Dynamic Zoom → Death Cam → Recorded`

Death Cam should be unavailable or skipped when the replay history has no death event.

## Camera pose model

Proposed `CameraPose` fields:

- `bool valid`
- `float x`
- `float y`
- `float rotation`
- `float scaleX`
- `float scaleY`

No live GD pointer belongs in the pose.

## Compatibility / failure behavior

If any cinematic calculation lacks valid data:

1. fall back to the v0.8 recorded `CameraSnapshot`
2. if no recorded camera is available, do not mutate the current viewport

Camera errors must fail open to replay controls/ghost playback rather than breaking the level.

## Safety boundaries

v0.9 must not intentionally:

- modify Geometry Dash saves or account data
- inject or alter gameplay input
- change live-attempt collision/physics
- change completion authority
- alter replay frame data
- leave the live-attempt camera transformed after Studio closes
- affect camera behavior outside Replay Studio

## Source acceptance criteria

- one replay-time authority remains `EchoReplayTimeline`
- camera engine consumes timeline data, not ghost-node state
- Recorded mode exactly preserves v0.8 baseline semantics
- Follow target supports single and dual replay subjects
- smoothing resets deterministically on non-monotonic seeks
- Drone look-ahead is bounded
- dynamic zoom is bounded
- Death Cam uses real v0.6/v0.7 death metadata
- camera-mode UI owns no camera state beyond issuing mode commands
- invalid cinematic data falls back to recorded viewport
- Studio close restores the active-attempt viewport through the existing v0.8 restore path
- no runtime/build testing until v1.0

## v1.0 runtime verification targets inherited by v0.9

- camera correctness across normal levels and camera triggers
- backwards scrub behavior in every camera mode
- dual-mode framing
- portal/teleport discontinuities
- death-cam timing/framing
- ultrawide/window aspect behavior
- compatibility with other visual/camera mods

## Explicitly out of scope for v0.9

- video export/encoding
- free-roam editor camera with arbitrary keyboard flight
- keyframed user-authored camera tracks
- render-to-texture composition
- online/shareable replay files
- live gameplay camera enhancement outside Replay Studio

Those can be post-v1.0 expansions if the core replay product proves stable.
