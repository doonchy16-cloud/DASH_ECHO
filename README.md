# DASH ECHO 👻

**Every attempt leaves a trace.**

DASH ECHO is a Geometry Dash / Geode mod focused on attempt recording, synchronized ghost replays, attempt history, death intelligence, interactive Replay Studio controls, and cinematic replay cameras.

## Project authority

- Authoritative branch: **main only**
- Development rule: **no feature branches**
- Release milestone: **v1.0.0**
- Build/package gate: **PASS**
- Runtime/in-game gate: **HOLD pending first Geometry Dash verification**

## Current release state

- Tooling: **Geode CLI 3.7.4**
- Windows SDK/loader baseline: **Geode 5.10.1**
- Geometry Dash Windows baseline: **2.2081**
- Current mod version: **v1.0.0**
- Verified Windows build run: **GitHub Actions #3 / run 33302346780**
- Verified build source SHA: **2f35150cd72f597749f74455288110affca7c7d0**
- Verified artifact: **DASH-ECHO-v1.0.0-windows**
- Packaged mod: **doonchy.dash-echo.geode**

## Implemented v1.0 feature set

- **v0.1:** bounded dual-player attempt recorder
- **v0.2:** translucent previous-attempt ghost with mode/color reconstruction
- **v0.3:** recorder-authoritative synchronization, interpolation, and discontinuity guards
- **v0.4:** configurable 0–6 multiverse ghost fleet with PB preservation
- **v0.5:** confirmed-death intelligence, clustering, 1% heatmap, and optional markers
- **v0.6:** immutable 4,096-entry attempt-history authority independent from replay retention
- **v0.7:** owned immutable replay clip + deterministic replay timeline + dedicated replay ghost
- **v0.8:** Replay Studio with pause/resume, restart, normalized scrubbing, frame stepping, five speeds, and recorded viewport reproduction
- **v0.9:** Recorded / Follow / Smooth / Drone / Dynamic Zoom / Death Cam cinematic modes driven from replay data
- **v1.0:** pinned/reproducible Windows release build, `.geode` package verification, release hardening, and first runtime verification gate

## Replay and camera architecture

- `EchoReplayTimeline` remains replay-time and replay-data authority
- timeline-level player sampling reuses recorded continuity boundaries
- `EchoCinematicCamera` consumes recorded camera/player/history data and outputs a pointer-free `CameraPose`
- cinematic camera modes never inspect rendered ghost nodes
- invalid cinematic data falls back to the recorded viewport
- Smooth resets after non-monotonic/large seeks, restart, source load, or mode changes
- Drone and Dynamic Zoom are explicitly bounded
- Death Cam uses real death position/time and is skipped when unavailable
- Replay Studio UI owns only camera-mode commands/labels
- `DashEchoPlayLayer` is the only code that applies the final viewport pose
- the active-attempt camera is restored when Replay Studio closes

## v1.0 build verification

The first v1.0 verification cycle found and repaired two release blockers before obtaining a clean Windows package:

1. Geode CLI 3.7.4 requires a configured profile before SDK binary installation. CI now creates an isolated CI-only profile and explicitly installs the Windows 5.10.1 binaries.
2. `mod.json` used `v5.10.1`; Geode 5.10.1 requires `5.10.1`. The metadata was corrected.

GitHub Actions run **33302346780** then completed successfully: pinned CLI verification, pinned SDK/binary installation, Windows Release build, `.geode` package collection, and artifact upload all passed.

The produced package was independently inspected after download. It contains the DASH ECHO DLL and embedded metadata identifying `doonchy.dash-echo`, version `v1.0.0`, Geode `5.10.1`, and Geometry Dash Windows `2.2081`.

## Safety boundary

DASH ECHO does not intentionally modify Geometry Dash save files, account data, player inputs, physics authority, collision authority, completion authority, or unrelated Geode mods.

## Verification language

**Build/package PASS does not equal runtime PASS.** v1.0 remains runtime HOLD until the packaged mod is installed in Geometry Dash and its recording, ghost, Replay Studio, cinematic-camera, restoration, and lifecycle behavior are confirmed in-game.
