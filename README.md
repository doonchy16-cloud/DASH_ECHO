# DASH ECHO 👻

**Every attempt leaves a trace.**

DASH ECHO is a Geometry Dash / Geode mod focused on attempt recording, synchronized ghost replays, attempt history, replay timelines, death intelligence, and cinematic replay tools.

## Project authority

- Authoritative branch: **main only**
- Development rule: **no feature branches**
- Gameplay/build testing: **deferred until v1.0 by project decision**

## Current development state

- Tooling: **Geode CLI 3.7.4**
- Current Windows SDK/loader baseline: **Geode v5.10.1**
- Geometry Dash Windows baseline: **2.2081**
- Current mod version: **v0.7.0**
- Current milestone: **Owned Replay Timeline**

## Implemented through v0.7

- **v0.1:** bounded dual-player attempt recorder
- **v0.2:** translucent previous-attempt ghost with mode/color reconstruction
- **v0.3:** recorder-authoritative synchronization, interpolation, and discontinuity guards
- **v0.4:** configurable 0–6 multiverse ghost fleet with PB preservation
- **v0.5:** confirmed-death intelligence, clustering, heatmap, and optional markers
- **v0.6:** immutable 4,096-entry attempt-history authority independent from replay retention
- **v0.7:** owned immutable replay clip + deterministic 1x replay timeline + dedicated replay ghost session

### v0.7 key architecture

- selected replay frames are copied into one owned `ReplayClip`
- replay remains valid if recorder history later evicts the source attempt
- exact death timestamp is preserved in history and becomes a real replay marker
- replay clip validation rejects non-finalized, mismatched, non-finite, non-monotonic, or zero-duration sources
- timeline markers include start, end, death, completion, and PB
- the timeline cursor is the only replay-time authority
- replay ghost consumes that cursor and never accumulates its own replay clock
- finalized attempts are prepared as replay candidates but are not auto-played in v0.7

## Roadmap

| Version | Milestone |
|---|---|
| v0.1 | Attempt-state recorder foundation |
| v0.2 | Previous-attempt ghost |
| v0.3 | Ghost synchronization + interpolation |
| v0.4 | Multiple ghosts / Multiverse fleet |
| v0.5 | Death markers / heatmap foundation |
| v0.6 | Attempt history authority |
| **v0.7** | **Replay timeline — source implemented** |
| v0.8 | Playback speed / scrubbing |
| v0.9 | Cinematic camera |
| v1.0 | Integrated DASH ECHO release candidate + first gameplay/build test |

## Safety boundary

DASH ECHO does not intentionally modify Geometry Dash save files, account data, physics authority, collision authority, or unrelated Geode mods.

## Verification language

Until the v1.0 test gate, milestones may be marked **SOURCE IMPLEMENTED** but must not be called runtime PASS.
