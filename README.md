# DASH ECHO 👻

**Every attempt leaves a trace.**

DASH ECHO is a Geometry Dash / Geode mod focused on attempt recording, ghost replays, death analysis, and cinematic replay tools.

## Current development state

- Target loader: **Geode 3.7.4**
- Current mod version: **v0.1.0**
- Current milestone: **Recorder Foundation**
- Gameplay testing: **deferred until v1.0 by project decision**

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
| v1.0 | Integrated DASH ECHO release candidate + first gameplay test |

## Safety boundary

DASH ECHO does not intentionally modify Geometry Dash save files, account data, or unrelated Geode mods.
