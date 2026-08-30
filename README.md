# DASH ECHO 👻

**Every attempt leaves a trace.**

DASH ECHO is a Geometry Dash / Geode mod focused on attempt recording, synchronized ghost replays, attempt history, death intelligence, interactive Replay Studio controls, and cinematic replay cameras.

## Project authority

- Authoritative branch: **main only**
- Development rule: **no feature branches**
- First build/runtime verification gate: **v1.0**

## Current development state

- Tooling: **Geode CLI 3.7.4**
- Windows SDK/loader baseline: **Geode v5.10.1**
- Geometry Dash Windows baseline: **2.2081**
- Current mod version: **v0.9.0**
- Current milestone: **Cinematic Replay Camera**

## Implemented through v0.9

- **v0.1:** bounded dual-player attempt recorder
- **v0.2:** translucent previous-attempt ghost with mode/color reconstruction
- **v0.3:** recorder-authoritative synchronization, interpolation, and discontinuity guards
- **v0.4:** configurable 0–6 multiverse ghost fleet with PB preservation
- **v0.5:** confirmed-death intelligence, clustering, 1% heatmap, and optional markers
- **v0.6:** immutable 4,096-entry attempt-history authority independent from replay retention
- **v0.7:** owned immutable replay clip + deterministic replay timeline + dedicated replay ghost
- **v0.8:** Replay Studio with pause/resume, restart, normalized scrubbing, frame stepping, five speeds, and recorded viewport reproduction
- **v0.9:** Recorded / Follow / Smooth / Drone / Dynamic Zoom / Death Cam cinematic modes driven from replay data

### v0.9 camera architecture

- `EchoReplayTimeline` remains replay-time and replay-data authority
- timeline-level `playerAtCursor` / `playerAtTime` queries reuse recorded continuity boundaries
- `EchoCinematicCamera` consumes recorded camera/player/history data and outputs a pointer-free `CameraPose`
- cinematic camera modes never inspect rendered ghost nodes
- invalid cinematic data falls back to the recorded viewport
- Smooth resets after non-monotonic/large seeks, restart, source load, or mode changes
- Drone and Dynamic Zoom are explicitly bounded
- Death Cam uses real death position/time and is skipped when unavailable
- Replay Studio UI owns only camera-mode commands/labels
- `DashEchoPlayLayer` is the only code that applies the final viewport pose
- the active-attempt camera is still restored exactly when Replay Studio closes

## Roadmap

| Version | Milestone |
|---|---|
| v0.1–v0.6 | Recording, ghosts, synchronization, multiverse, death intelligence, history |
| v0.7 | Owned replay timeline |
| v0.8 | Interactive Replay Studio |
| **v0.9** | **Cinematic camera — source implemented** |
| v1.0 | Integrated release candidate + first build/runtime verification gate |

## Safety boundary

DASH ECHO does not intentionally modify Geometry Dash save files, account data, player inputs, physics authority, collision authority, completion authority, or unrelated Geode mods.

## Verification language

v0.9 is a source milestone only. v1.0 is the first milestone allowed to run the build/runtime verification gate; no runtime PASS is claimed before that evidence exists.
