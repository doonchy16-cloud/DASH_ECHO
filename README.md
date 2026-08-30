# DASH ECHO 👻

**Every attempt leaves a trace.**

DASH ECHO is a Geometry Dash / Geode mod focused on attempt recording, synchronized ghost replays, death intelligence, and cinematic replay tools.

## Project authority

- Authoritative branch: **main only**
- Development rule: **no feature branches**
- Gameplay/build testing: **deferred until v1.0 by project decision**

## Current development state

- Tooling: **Geode CLI 3.7.4**
- Current Windows SDK/loader baseline: **Geode v5.10.1**
- Geometry Dash Windows baseline: **2.2081**
- Current mod version: **v0.5.0**
- Current milestone: **Death Intelligence / Heatmap Foundation**

> Note: `3.7.4` is the Geode CLI version. The Geode loader/SDK has its own 5.x version line.

## Implemented through v0.5

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
- recorder-owned personal-best identity and PB-pinned retention
- age-based ghost opacity with stronger PB visibility
- deterministic oldest-to-newest layering below the live player
- every ghost shares the recorder's authoritative clock and v0.3 synchronization engine
- fleet historical pointers released before recorder retention can evict attempts
- out-of-range playback hides rather than discards source history, preserving later backward-seek capability

### v0.5 — Death Intelligence / Heatmap Foundation

- verified `PlayLayer::destroyPlayer` observation path
- records only after Geometry Dash confirms the player is actually dead
- one authoritative terminal death event per attempt
- player index, attempt ID, attempt-relative time, progress, world position, and optional hazard object context
- recent raw death-event retention capped at 4,096
- lifetime-session aggregate death intelligence updates incrementally without full-history rebuilds
- bounded maximum of 512 spatial/progress death clusters
- repeated deaths form weighted world-space centroids and progress ranges
- 100 fixed progress heatmap buckets (1% each) with normalized intensity
- clustered world markers with repeat-count labels and intensity scaling
- marker renderer capped at 24 clusters to bound visual cost
- Geode `Death Markers` setting hides presentation without disabling analytics
- cluster-cap saturation is reported as unclustered deaths instead of corrupting cluster meaning

## Roadmap

| Version | Milestone |
|---|---|
| v0.1 | Attempt-state recorder foundation |
| v0.2 | Previous-attempt ghost |
| v0.3 | Ghost synchronization + interpolation |
| v0.4 | Multiple ghosts / Multiverse fleet |
| **v0.5** | **Death markers / heatmap foundation — source implemented** |
| v0.6 | Attempt history |
| v0.7 | Replay timeline |
| v0.8 | Playback speed / scrubbing |
| v0.9 | Cinematic camera |
| v1.0 | Integrated DASH ECHO release candidate + first gameplay/build test |

## Safety boundary

DASH ECHO does not intentionally modify Geometry Dash save files, account data, physics authority, collision authority, or unrelated Geode mods.

## Verification language

Until the v1.0 test gate, milestones may be marked **SOURCE IMPLEMENTED** but must not be called runtime PASS.
