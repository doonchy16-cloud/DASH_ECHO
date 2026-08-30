# DASH ECHO 👻

**Every attempt leaves a trace.**

DASH ECHO is a Geometry Dash / Geode mod focused on attempt recording, ghost replays, death analysis, and cinematic replay tools.

## Current development state

- Tooling: **Geode CLI 3.7.4**
- Current Windows SDK/loader baseline: **Geode v5.10.1**
- Geometry Dash Windows baseline: **2.2081**
- Current mod version: **v0.1.0**
- Current milestone: **Recorder Foundation**
- Gameplay/build testing: **deferred until v1.0 by project decision**

> Note: `3.7.4` is the Geode CLI version. The Geode loader/SDK has its own 5.x version line.

## v0.1 goal

Build the authoritative in-memory attempt recorder that later ghost, replay, analytics, and cinematic systems can consume.

The v0.1 recorder is intentionally renderer-agnostic. It captures structured per-frame player state and owns attempt/session lifecycle without changing Geometry Dash save data.

## Roadmap

| Version | Milestone |
|---|---|
| v0.1 | Attempt-state recorder foundation |
| v0.2 | Previous-attempt ghost |
| v0.3 | Ghost synchronization |
| v0.4 | Multiple ghosts |
| v0.5 | Death markers / heatmap foundation |
| v0.6 | Attempt history |
| v0.7 | Replay timeline |
| v0.8 | Playback speed / scrubbing |
| v0.9 | Cinematic camera |
| v1.0 | Integrated DASH ECHO release candidate + first gameplay/build test |

## Safety boundary

DASH ECHO does not intentionally modify Geometry Dash save files, account data, or unrelated Geode mods.
