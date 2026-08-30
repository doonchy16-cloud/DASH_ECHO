# DASH ECHO 👻

**Every attempt leaves a trace.**

DASH ECHO is a Geometry Dash / Geode mod focused on attempt recording, ghost replays, death analysis, and cinematic replay tools.

## Project authority

- Authoritative branch: **main only**
- Development rule: **no feature branches**
- Gameplay/build testing: **deferred until v1.0 by project decision**

## Current development state

- Tooling: **Geode CLI 3.7.4**
- Current Windows SDK/loader baseline: **Geode v5.10.1**
- Geometry Dash Windows baseline: **2.2081**
- Current mod version: **v0.2.0**
- Current milestone: **Previous-Attempt Ghost**

> Note: `3.7.4` is the Geode CLI version. The Geode loader/SDK has its own 5.x version line.

## Implemented through v0.2

### v0.1 — Recorder Foundation

- bounded in-memory attempt recording
- player 1 + player 2 snapshots
- timestamps and progress
- transform / visibility capture
- attempt reset, completion, and exit finalization
- retention and diagnostics

### v0.2 — Previous-Attempt Ghost

- one previous finalized attempt selected on reset
- translucent ghost player 1 + player 2 visuals
- timestamp-gated discrete playback
- cube / ship / ball / UFO / wave / robot / spider / swing mode reconstruction
- recorded primary / secondary colors
- recorded position / rotation / scale / visibility
- renderer separated from recorder authority

## Roadmap

| Version | Milestone |
|---|---|
| v0.1 | Attempt-state recorder foundation |
| **v0.2** | **Previous-attempt ghost — source implemented** |
| v0.3 | Ghost synchronization + interpolation |
| v0.4 | Multiple ghosts |
| v0.5 | Death markers / heatmap foundation |
| v0.6 | Attempt history |
| v0.7 | Replay timeline |
| v0.8 | Playback speed / scrubbing |
| v0.9 | Cinematic camera |
| v1.0 | Integrated DASH ECHO release candidate + first gameplay/build test |

## Safety boundary

DASH ECHO does not intentionally modify Geometry Dash save files, account data, physics authority, collision authority, or unrelated Geode mods.

## Verification language

Until the v1.0 test gate, milestones may be marked **SOURCE IMPLEMENTED** but must not be called runtime PASS.
