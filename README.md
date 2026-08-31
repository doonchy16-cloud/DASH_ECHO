# ECHO_DASH 👻🔥

**Every attempt leaves a trace.**

ECHO_DASH is a local Geometry Dash replay and training studio built around historical ghosts, persistent attempt intelligence, death analysis, Replay Studio controls, and cinematic cameras.

## Project authority

- Authoritative branch: **main only**
- Development rule: **no feature branches**
- Current development version: **v1.1.2**
- Legacy Geode ID intentionally preserved for upgrade/save compatibility: **`doonchy.dash-echo`**
- Tooling: **Geode CLI 3.7.4**
- SDK/loader baseline: **Geode 5.10.1**
- Geometry Dash Windows baseline: **2.2081**

## v1.1.2 — Runtime Reliability Hardening

v1.1.2 strengthens the persistence and trust boundary beneath the Unified Ghost Engine without changing ghost-role playback authority.

- 💾 **Persistent known-good archive backup.** A successful save keeps the previous validated archive generation as `.bak` instead of deleting it.
- ♻️ **Automatic backup recovery.** If the primary archive cannot be trusted, ECHO_DASH attempts the retained backup before falling back to an empty safe archive.
- 🔬 **Semantic replay validation.** Loaded replay frames must have finite transforms/progress/timestamps, monotonic time, and strictly increasing frame sequence numbers.
- 🛡️ **Replay quarantine.** A structurally readable but semantically invalid replay is omitted from trusted playback without poisoning unrelated valid replays in the same archive.
- 📊 **Recovery telemetry.** Diagnostics can distinguish backup recovery and quarantined replay counts.
- ⚙️ **Replay Studio settings refresh remains live.** Settings polling happens before the Studio early-return, so settings can refresh without closing Replay Studio.
- 👻 **Unified Ghost Engine preserved.** Last, Best, Last+Best, and Older remain visual roles only; all historical ghosts still run through one canonical playback engine.

## v1.1.1 — Runtime UX & Persistence Repair

v1.1.1 repairs the first issues found during real v1.1 gameplay:

- 🎬 **Replay Studio entry lives in the Geometry Dash pause menu.** There is no persistent ECHO_DASH launcher covering the live level.
- 🔵 **Last Attempt** — blue priority trail, no aura/halo.
- 🟡 **Best Recorded Echo** — golden priority trail, no aura/halo.
- 👻 **Older Attempts** — configurable age-faded historical echoes.
- 🧠 **Attempt #1 uses the same explicit creation path as every later attempt.** It is begun and sampled immediately after PlayLayer initialization.
- 💾 **Every successful finalized attempt is archived and saved before the next attempt begins.**
- 📚 Default replay retention is raised to 10,000 runs, with a 100,000-run safety ceiling and storage-budget protection.
- 🎛️ Replay Studio has a clearer two-row control hierarchy for attempt navigation, frame stepping, playback, speed, camera, settings, and close.

### Truthful PB model

ECHO_DASH keeps four different concepts separate:

1. **GD Level PB** — Geometry Dash's own saved result.
2. **Best Recorded Echo** — best run for which ECHO_DASH owns replay data.
3. **Session Best** — best run in the current PlayLayer session.
4. **Latest Attempt** — newest finalized attempt.

A session-best or best-recorded replay is never mislabeled as the game's real PB.

### v1.1 architecture

- dynamic reusable 0–256 ghost pool instead of a fixed six-slot array
- configurable 30–240 Hz recorder sampling, default 120 Hz
- event/death samples that may bypass the regular sampling gate
- per-level/per-mode persistent summary + replay archive
- very high all-run retention safety ceilings, with user-controlled disk budget
- categorized ECHO_DASH settings
- priority colored trails without distracting aura rendering
- stronger death markers visible on the first death
- 100-bucket screen-space death heat strip
- pause-menu Replay Studio entry and archived attempt navigation
- one canonical role-agnostic historical ghost playback engine
- post-death continuation until selected historical ghosts finish, with vanilla reset request preserved
- existing single authoritative Replay Studio timeline preserved
- existing Recorded / Follow / Smooth / Drone / Dynamic Zoom / Death Cam modes preserved

## Implemented foundation

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
- **v1.1.0:** scalability, persistence, personalization and visual-truth rebuild
- **v1.1.1:** pause-menu UX, trail-only priority identity, first-attempt lifecycle repair, high all-run retention, and unified ghost playback continuation architecture
- **v1.1.2:** retained known-good backups, automatic recovery, semantic replay validation/quarantine, and runtime reliability telemetry

## Safety boundary

ECHO_DASH does not intentionally modify player inputs, physics authority, collision authority, death authority, completion authority, Geometry Dash account data, or unrelated mods. Persistent ECHO_DASH data is stored in the Geode-provided mod save area.

## Verification language

Source implementation, automated contract tests, Windows build/package verification, installer verification, and in-game runtime verification are separate gates. **v1.1.2 is not runtime PASS until its package is tested in Geometry Dash. The frozen v1.1.1 Unified Ghost Engine v4 candidate also remains a separate runtime baseline until tested.**