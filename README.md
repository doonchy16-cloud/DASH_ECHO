# DASH ECHO 👻

**Every attempt leaves a trace.**

DASH ECHO is a Geometry Dash / Geode mod focused on attempt recording, synchronized ghost replays, attempt history, interactive replay timelines, death intelligence, and cinematic replay tools.

## Project authority

- Authoritative branch: **main only**
- Development rule: **no feature branches**
- Gameplay/build testing: **deferred until v1.0 by project decision**

## Current development state

- Tooling: **Geode CLI 3.7.4**
- Current Windows SDK/loader baseline: **Geode v5.10.1**
- Geometry Dash Windows baseline: **2.2081**
- Current mod version: **v0.8.0**
- Current milestone: **Interactive Replay Studio**

## Implemented through v0.8

- **v0.1:** bounded dual-player attempt recorder
- **v0.2:** translucent previous-attempt ghost with mode/color reconstruction
- **v0.3:** recorder-authoritative synchronization, interpolation, and discontinuity guards
- **v0.4:** configurable 0–6 multiverse ghost fleet with PB preservation
- **v0.5:** confirmed-death intelligence, clustering, heatmap, and optional markers
- **v0.6:** immutable 4,096-entry attempt-history authority independent from replay retention
- **v0.7:** owned immutable replay clip + deterministic replay timeline + dedicated replay ghost session
- **v0.8:** interactive Replay Studio with pause/resume, restart, normalized scrubbing, distinct-frame stepping, five playback speeds, and recorded viewport reproduction

### v0.8 key architecture

- `EchoReplayTimeline` remains the only replay-time authority
- playback speed modifies replay time only; it never changes GD physics or the active-attempt recorder clock
- seeking and frame stepping pause playback so user input cannot fight an advancing cursor
- `EchoReplaySession` immediately re-synchronizes the dedicated replay ghost after every cursor-changing command
- `EchoReplayControls` is presentation-only: launcher, labels, buttons, and native Geometry Dash `Slider`
- normalized slider values are commands into the timeline; slider state is refreshed back from timeline authority
- every recorded frame now carries the Geometry Dash object-layer viewport transform
- Replay Studio reproduces/interpolates that recorded viewport from the same authoritative replay cursor
- the active attempt's camera transform is snapshotted on Studio open and restored exactly on close
- Replay Studio hides the multighost fleet while the dedicated replay ghost is active
- DASH ECHO active-attempt recording time does not advance while Studio mode is open
- Studio-mode death callbacks are excluded from DASH ECHO analytics to avoid contaminating historical data
- reset, completion, and exit force Studio closed before normal attempt lifecycle finalization

## Roadmap

| Version | Milestone |
|---|---|
| v0.1 | Attempt-state recorder foundation |
| v0.2 | Previous-attempt ghost |
| v0.3 | Ghost synchronization + interpolation |
| v0.4 | Multiple ghosts / Multiverse fleet |
| v0.5 | Death markers / heatmap foundation |
| v0.6 | Attempt history authority |
| v0.7 | Owned replay timeline |
| **v0.8** | **Playback speed / scrubbing / Replay Studio — source implemented** |
| v0.9 | Cinematic camera |
| v1.0 | Integrated DASH ECHO release candidate + first gameplay/build test |

## Safety boundary

DASH ECHO does not intentionally modify Geometry Dash save files, account data, physics authority, collision authority, or unrelated Geode mods.

## Verification language

Until the v1.0 test gate, milestones may be marked **SOURCE IMPLEMENTED** but must not be called runtime PASS.
