# ECHO_DASH 👻🔥

**Every attempt leaves a trace.**

ECHO_DASH is a local Geometry Dash replay and training studio built around historical ghosts, persistent attempt intelligence, death analysis, Replay Studio controls, and cinematic cameras.

## Project authority

- Authoritative branch: **main only**
- Development rule: **no feature branches**
- Current development version: **v1.1.0**
- Legacy Geode ID intentionally preserved for upgrade/save compatibility: **`doonchy.dash-echo`**
- Tooling: **Geode CLI 3.7.4**
- SDK/loader baseline: **Geode 5.10.1**
- Geometry Dash Windows baseline: **2.2081**

## v1.1.0 — Multiverse & Personalization Rebuild

v1.1.0 replaces the original six-ghost prototype ceiling with a scalable architecture designed for **100+ historical ghosts**, with a supported target ceiling of **256 selected ghosts**.

The player-facing identity is also explicit:

- 🔵 **Last Attempt** — blue spectral priority aura/trail
- 🟡 **Best Recorded Echo** — golden spectral priority aura/trail
- 👻 **Older Attempts** — configurable age-faded historical echoes
- 🎮 **Current Player** — remains visually dominant and authoritative

### Truthful PB model

ECHO_DASH keeps four different concepts separate:

1. **GD Level PB** — Geometry Dash's own saved result.
2. **Best Recorded Echo** — best run for which ECHO_DASH owns replay data.
3. **Session Best** — best run in the current PlayLayer session.
4. **Latest Attempt** — newest finalized attempt.

A session-best or best-recorded replay is never mislabeled as the game's real PB.

### v1.1 architecture targets

- dynamic reusable 0–256 ghost pool instead of a fixed six-slot array
- configurable 30–240 Hz recorder sampling, default 120 Hz
- event/death samples that may bypass the regular sampling gate
- per-level/per-mode persistent summary + replay archive
- up to 4,096 attempt summaries with bounded replay retention
- categorized ECHO_DASH Control Center settings
- priority-only glow/trail effects so large ghost counts remain tractable
- stronger death markers visible on the first death
- 100-bucket screen-space death heat strip
- discoverable Replay Studio launcher, settings access, and attempt navigation
- existing single authoritative replay timeline preserved
- existing Recorded / Follow / Smooth / Drone / Dynamic Zoom / Death Cam modes preserved

## Implemented foundation through v1.0

- **v0.1:** bounded dual-player attempt recorder
- **v0.2:** previous-attempt ghost
- **v0.3:** recorder-authoritative synchronization and discontinuity-aware interpolation
- **v0.4:** initial multighost fleet and session-best preservation
- **v0.5:** confirmed-death intelligence, clustering and 100-bucket heatmap data
- **v0.6:** immutable attempt-history summaries
- **v0.7:** owned replay clip + deterministic replay timeline
- **v0.8:** Replay Studio pause/resume, restart, scrubbing, frame stepping, five speeds, recorded viewport reproduction
- **v0.9:** cinematic replay cameras
- **v1.0:** first real Windows build/package verification and first gameplay evidence
- **v1.1:** scalability, persistence, personalization and visual-truth rebuild

## Safety boundary

ECHO_DASH does not intentionally modify player inputs, physics authority, collision authority, death authority, completion authority, Geometry Dash account data, or unrelated mods. Persistent ECHO_DASH data is stored in the Geode-provided mod save area.

## Verification language

Source implementation, automated contract tests, Windows build/package verification, and in-game runtime verification are separate gates. **v1.1.0 is not runtime PASS until the new package is tested in Geometry Dash with 100+/256-ghost stress cases, persistence, Replay Studio, blue/gold priority identities, death overlays and camera modes.**
