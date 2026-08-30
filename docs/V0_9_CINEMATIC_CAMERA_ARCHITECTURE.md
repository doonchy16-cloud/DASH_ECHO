# DASH ECHO v0.9 — Cinematic Camera Architecture

Status: **SOURCE IMPLEMENTED / RUNTIME HOLD UNTIL v1.0**

## Authority hierarchy

```text
ReplayClip
   ↓
EchoReplayTimeline cursor
   ├── recorded CameraSnapshot
   ├── playerAtCursor / playerAtTime
   └── AttemptHistoryEntry death metadata
          ↓
EchoCinematicCamera
          ↓
CameraPose
          ↓
DashEchoPlayLayer applies pose in Replay Studio only
```

`EchoReplayTimeline` remains the only replay-time authority. The cinematic camera owns only camera-mode selection and bounded visual smoothing state.

## Modes

1. **Recorded** — exact v0.8 recorded viewport baseline.
2. **Follow** — frames the visible replay subject; dual mode uses the player midpoint.
3. **Smooth** — damped Follow framing with deterministic snap reset after backward/large seeks, source changes, restarts, or mode changes.
4. **Drone** — bounded look-ahead derived from neighboring recorded player samples.
5. **Dynamic Zoom** — bounded zoom-out/in from recorded subject speed and dual-player separation.
6. **Death Cam** — terminal framing around the real v0.6/v0.7 death position/time. It is skipped when the replay has no death event.

## Determinism and safety

- Cinematic modes consume replay data, never rendered ghost-node positions.
- Invalid player/camera/death inputs fall back to the Recorded viewport.
- Drone look-ahead is capped at 110 replay-world units.
- Dynamic zoom multiplier is bounded to 0.72–1.08 before the generic pose guard.
- Generic cinematic scale multiplication is bounded to 0.65–1.25.
- Death Cam uses a bounded 1.35-second terminal window and never changes replay speed itself.
- Smooth mode does not continue damping while the replay cursor is stationary.
- Backward or >0.35-second cursor jumps snap Smooth state rather than animating across unrelated replay sections.
- `EchoReplaySession` resets camera smoothing after seek, frame step, restart, load, and stop.
- `DashEchoPlayLayer` remains the only component that mutates `m_objectLayer`.
- The v0.8 active-attempt viewport snapshot/restore path is unchanged; closing Replay Studio restores the exact live-attempt transform.

## Replay Studio presentation

`EchoReplayControls` adds one presentation-only camera-cycle label/button. It reads the canonical mode name from `EchoReplaySession` and sends only `cycleCameraMode()` commands.

Cycle order:

`Recorded → Follow → Smooth → Drone → Dynamic Zoom → Death Cam → Recorded`

When no death event exists:

`Recorded → Follow → Smooth → Drone → Dynamic Zoom → Recorded`

## Source-level uncertainties for v1.0

The first build/runtime gate must verify:

- API/compile correctness of the new camera/UI integration
- actual Cocos object-layer anchor/rotation behavior under Follow centering
- dual-subject framing
- camera triggers and teleports
- Smooth reset behavior after aggressive backward scrubbing
- camera-control label fit across window sizes
- Death Cam timing and framing
- compatibility with other camera/visual mods

No runtime PASS is claimed by v0.9 source implementation alone.
