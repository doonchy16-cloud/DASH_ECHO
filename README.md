# DASH ECHO 👻

**Every attempt leaves a trace.**

DASH ECHO is a Geometry Dash / Geode mod focused on attempt recording, synchronized ghost replays, attempt history, death intelligence, and cinematic replay tools.

## Project authority

- Authoritative branch: **main only**
- Development rule: **no feature branches**
- Gameplay/build testing: **deferred until v1.0 by project decision**

## Current development state

- Tooling: **Geode CLI 3.7.4**
- Current Windows SDK/loader baseline: **Geode v5.10.1**
- Geometry Dash Windows baseline: **2.2081**
- Current mod version: **v0.6.0**
- Current milestone: **Attempt History Authority**

> Note: `3.7.4` is the Geode CLI version. The Geode loader/SDK has its own 5.x version line.

## Implemented through v0.6

### v0.1 — Recorder Foundation

- bounded in-memory attempt recording
- player 1 + player 2 snapshots
- timestamps and progress
- transform / visibility capture
- attempt reset, completion, and exit finalization
- retention and diagnostics

### v0.2 — Previous-Attempt Ghost

- translucent historical player 1 + player 2 visuals
- cube / ship / ball / UFO / wave / robot / spider / swing reconstruction
- recorded primary / secondary colors
- recorded position / rotation / scale / visibility
- renderer separated from recorder authority

### v0.3 — Ghost Synchronization Engine

- recorder active-attempt time is the single authoritative playback clock
- timestamp-bracketed position / scale / color interpolation
- shortest-path rotation interpolation
- monotonic forward cursor + binary-search non-monotonic recovery
- recorded continuity/discontinuity boundaries
- teleport-like displacement, mode, visibility, scale, and long-gap snap guards
- explicit attempt lifecycle prevents phantom post-completion attempts

### v0.4 — Multiverse Ghost Fleet

- reusable fixed-capacity fleet of up to 6 historical attempts
- at most 12 ghost player nodes in dual mode
- Geode `Ghost Count` setting: 0–6, default 4
- newest-attempt selection with personal-best preservation
- recorder-owned PB identity and PB-pinned replay retention
- age-based ghost opacity with stronger PB visibility
- deterministic layering below the live player
- every ghost shares the recorder's authoritative clock and v0.3 synchronization engine
- fleet historical pointers released before recorder retention can evict attempts

### v0.5 — Death Intelligence / Heatmap Foundation

- observes `PlayLayer::destroyPlayer` and records only confirmed deaths
- one terminal death event per attempt
- recent raw death-event retention capped at 4,096
- lifetime-session incremental death clustering and 100-bucket progress heatmap
- bounded 512-cluster aggregate authority
- clustered in-level markers capped at 24 visuals
- Death Markers setting hides presentation without disabling analytics

### v0.6 — Attempt History Authority

- immutable finalized-attempt summaries independent of replay-frame retention
- Death / Reset / Completed / LayerExit outcome distinction
- max progress, duration, frame count, per-attempt dropped frames, first/last sample times
- copied death/hazard context with no retained death pointers
- PB-at-finalization, prior PB progress, and positive improvement tracking
- dynamic replay-source resolution by immutable attempt ID instead of stale replay booleans
- bounded 4,096-entry history with current-PB history pinning
- aggregate counters survive old history-detail eviction
- one centralized PlayLayer finalization/commit path for reset, completion, and exit

## Roadmap

| Version | Milestone |
|---|---|
| v0.1 | Attempt-state recorder foundation |
| v0.2 | Previous-attempt ghost |
| v0.3 | Ghost synchronization + interpolation |
| v0.4 | Multiple ghosts / Multiverse fleet |
| v0.5 | Death markers / heatmap foundation |
| **v0.6** | **Attempt history authority — source implemented** |
| v0.7 | Replay timeline |
| v0.8 | Playback speed / scrubbing |
| v0.9 | Cinematic camera |
| v1.0 | Integrated DASH ECHO release candidate + first gameplay/build test |

## Safety boundary

DASH ECHO does not intentionally modify Geometry Dash save files, account data, physics authority, collision authority, or unrelated Geode mods.

## Verification language

Until the v1.0 test gate, milestones may be marked **SOURCE IMPLEMENTED** but must not be called runtime PASS.
