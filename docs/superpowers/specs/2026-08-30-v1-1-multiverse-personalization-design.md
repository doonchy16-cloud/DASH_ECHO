# ECHO_DASH v1.1.0 — Multiverse & Personalization Rebuild Design

## Authority and approval

This design is the implementation authority for v1.1.0. It consolidates the owner-approved five-pass runtime audit and the explicit approval to implement all recommendations. Development remains **main-only**. The legacy Geode mod ID `doonchy.dash-echo` is preserved in v1.1.0 so existing installed settings/save ownership are not orphaned; all user-facing branding becomes **ECHO_DASH**.

## Release objective

Turn the v1.0 engineering prototype into a scalable, highly personalizable replay/training product without weakening the recorder/replay authority model that already survived the first runtime tests.

v1.1.0 must ship these pillars together:

1. **100+ ghost scalability:** support 0–256 selected historical attempts with a dynamic pooled renderer. 512 remains future/experimental until 256 has runtime frame-time evidence.
2. **Priority visual identity:** Last Attempt = blue spectral aura/trail; Best Recorded Echo = gold spectral aura/trail; if one attempt is both, render both identities coherently.
3. **PB truth:** Geometry Dash Level PB, Best Recorded Echo, Session Best, and Latest Attempt are separate concepts and labels.
4. **Persistence:** per-level, per-mode versioned archive with thousands of summaries and a bounded replay archive, using Geode's mod save directory and atomic replace.
5. **Personalization:** a categorized ECHO_DASH Control Center powered by Geode settings, reachable directly from Replay Studio.
6. **Death intelligence presentation:** stronger first-death markers plus a screen-space 100-bucket heat strip.
7. **Replay Studio discoverability/history:** a real ECHO_DASH launcher, first-replay hint, settings access, and previous/next archived-attempt selection.
8. **Performance discipline:** regular replay sampling becomes configurable and bounded; event frames remain force-capturable; expensive effects are priority-only by default.

## Non-negotiable authority boundaries

- ECHO_DASH never injects player input or becomes physics, collision, death, completion, or GD-save authority.
- `EchoRecorder` remains the current-attempt sampling authority.
- `EchoReplayTimeline` remains the sole replay-time cursor authority.
- Cinematic camera remains a consumer of timeline data and never owns replay time.
- The current player always has higher visual priority than historical ghosts.
- Invalid/corrupt persistent archives fail closed and never crash gameplay.
- Containment, source implementation, build verification, and runtime verification are reported separately.

## Product naming and migration

### User-facing

Every visible product label becomes `ECHO_DASH`: `mod.json` name, README/About/changelog copy, Replay Studio launcher, settings titles, build artifact naming, and logs.

### Compatibility

The v1.1.0 package retains `id: doonchy.dash-echo`. This is deliberate compatibility authority, not incomplete renaming. A future ID migration may use Geode's deprecation/supersession mechanisms after persistent-data migration has a dedicated release plan.

## Data architecture

### Current-attempt recorder

`EchoRecorder` no longer needs to be the long-term replay warehouse. It keeps a small working set and captures at a configurable regular sample rate (default 120 Hz, allowed 30–240 Hz), while always advancing authoritative attempt time on every `postUpdate`.

It exposes a force/event-sample path for death/discontinuity-critical captures.

### Persistent history vs replay archive

Introduce `EchoReplayArchive` with two bounded authorities:

- `AttemptHistoryEntry` summaries: up to 4,096 per level/mode.
- replay-capable `AttemptRecord` tracks: configurable, default 512, hard cap 2,048 subject to disk budget.

Every finalized attempt commits a summary. Replay frames are downsampled into the archive at the configured archive sample rate while preserving first frame, last frame, and discontinuity boundaries.

Best Recorded Echo and Latest Attempt replay tracks are pinned during replay-retention trimming.

### Level identity

Persistent data is keyed by:

- stable GD level ID when non-zero;
- stable fallback FNV-1a hash of level name for zero-ID/local cases;
- platformer/classic mode;
- practice/normal mode.

Archive format stores a schema version and level-context fingerprint. A mismatch or invalid count/size fails closed.

### Persistent file format

Use a versioned binary archive in `Mod::get()->getSaveDir() / "echo_dash"`.

Write flow:

1. serialize to `.tmp`;
2. flush/close;
3. replace destination atomically where filesystem semantics allow;
4. preserve the previous file on failed replacement.

The loader enforces hard count/frame/file-size limits before allocation.

## 256-ghost renderer

Replace fixed `std::array<6>` with a dynamically grown pool of stable `EchoGhost` instances.

- Supported selected ghost ceiling: 256.
- Default: 16.
- Pool grows only when needed and is reused across attempt boundaries.
- No per-frame allocation for ghost nodes.
- Normal synchronization cost is O(active ghosts), with each ghost retaining its amortized forward frame cursor.
- Fleet selection is rebuilt only at stable attempt/archive boundaries.

### Selection

Default selection: newest N replay-capable attempts, while ensuring Best Recorded Echo is included. Latest Attempt is always kept when available. Selection is deterministically ordered oldest→newest for layering.

### Roles

`GhostRole`:

- Older
- LastAttempt
- BestRecorded
- LastAndBest

Roles determine priority opacity/aura/trail, not replay authority.

## Priority ghost visual language

### Last Attempt

- blue aura/glow;
- blue trail;
- independently configurable color, opacity, aura radius/intensity, and trail visibility.

### Best Recorded Echo

- gold aura/glow;
- gold trail;
- independently configurable color, opacity, aura radius/intensity, and trail visibility.

### LastAndBest

Use a dual identity: blue outer aura + gold inner aura/trail accent so neither meaning disappears.

### Older attempts

Retain recorded icon colors but use configurable age-fade opacity. Do not allocate full aura/trail effects by default.

### Aura implementation

Priority aura rendering must not multiply the cost of all 256 ghosts. Each `EchoGhost` lazily owns small `CCDrawNode` aura nodes only when assigned a priority role. Aura uses bounded concentric translucent dots around each active replay player.

### Trails

`EchoGhostFleet` owns one shared `CCDrawNode` for priority trails. It samples a bounded look-back window from Last/Best replay tracks and draws short fading segments. Default applies only to Last/Best; bulk trails are not enabled in v1.1.0.

## Personalization / Control Center

The Geode settings popup becomes the v1.1 Control Center and is directly openable from Replay Studio using `geode::openSettingsPopup(Mod::get())`.

Settings are organized with title sections.

### Ghosts

- ghost count 0–256;
- visual profile: Clean / Competitive / Multiverse / Chaos / Custom;
- older minimum/maximum opacity;
- age fade strength;
- priority x-ray layering toggle.

### Last Attempt

- enabled;
- blue accent color (RGB);
- opacity;
- aura enabled;
- aura size;
- priority trail enabled.

### Best Recorded Echo

- enabled;
- gold accent color (RGB);
- opacity;
- aura enabled;
- aura size;
- priority trail enabled.

### Trails

- length seconds;
- width;
- opacity.

### Death Intelligence

- world markers enabled;
- marker scale;
- labels enabled;
- x-ray markers;
- heat strip enabled;
- heat strip opacity.

### Replay Studio

- launcher scale;
- first-replay hint;
- default playback speed;
- default camera mode.

### History / Performance

- recorder sample rate 30–240 Hz;
- replay-retention attempt limit;
- disk budget MB;
- rendering quality: Auto / Full / Balanced / Performance;
- diagnostics label toggle.

Settings are polled at a bounded interval while gameplay is active so changes made through the Control Center apply without requiring a level restart where safe.

## PB semantics

For classic levels:

- **GD Level PB:** `GJGameLevel::getNormalPercent()`; authoritative saved game result.
- **Best Recorded Echo:** highest progress summary for which ECHO_DASH owns replay data.
- **Session Best:** highest finalized attempt in the current PlayLayer session.
- **Latest Attempt:** newest finalized attempt.

The UI must never label Best Recorded Echo as the actual GD PB unless both values genuinely match and the replay is owned.

Platformer PB semantics are not converted into classic percent; UI labels classic PB as unavailable in platformer mode until time/points semantics are shown explicitly.

## Death intelligence presentation

### World markers

A first death is visible immediately. Marker presentation uses a stronger concentric hotspot plus crosshair and a compact label (`DEATH 37.1%` or `xN 37.1%`). Marker size/labels/x-ray are configurable.

### Heat strip

Introduce `EchoHeatmapOverlay`, a screen-space `CCDrawNode` rendering 100 progress buckets. It is independent from world markers and can remain visible while markers are disabled.

## Replay Studio v1.1 UX

- Replace tiny text launcher with a proper `ButtonSprite` labeled `ECHO_DASH`.
- Show a first-replay-ready hint after the first finalized replay if enabled.
- Add `SETTINGS` button opening the ECHO_DASH Control Center.
- Add previous/next attempt controls backed by `EchoReplayArchive`.
- Display `GD PB`, `Best Echo`, selected attempt, current cursor progress/time, playback speed, and camera mode.
- Persistent archive allows Replay Studio to be available immediately on level entry when replay data already exists.

## Runtime performance behavior

- Default 120-Hz recording rather than blindly persisting every 240-Hz update.
- Current attempt time/progress still updates every game update.
- Event/death sample path bypasses the regular sampling gate.
- 256 ghost pool uses shared priority trail renderer and priority-only aura nodes.
- Fleet does not update slots whose source replay has no sample at the current time beyond normal `EchoGhost::hide()` behavior.
- Diagnostics can expose selected/active ghosts, retained replay count, retained summaries, frame counts, and recorder dropped frames.

## Compatibility

- Continue using the normal GD/Geode node hierarchy; no global scheduler hook.
- Preserve observer-only behavior.
- Allow x-ray/high-z rendering to be turned off for compatibility with other visual mods.
- Launcher placement remains configurable enough to coexist with other overlays.

## Testing and release gates

### Automated contract gate

Add source/config regression tests that assert:

- ECHO_DASH branding/version;
- legacy mod ID preservation;
- ghost max >=256 and dynamic fleet source;
- required settings exist;
- Last blue and Best gold defaults;
- recorder configurable sample-rate API;
- replay archive and heatmap overlay are compiled into the project;
- stale hard-coded `DASH ECHO v0.9` strings are gone.

### Windows build gate

The existing pinned CLI 3.7.4 / Geode SDK 5.10.1 Windows workflow must run contract tests, compile Release, collect exactly one `.geode`, and upload the v1.1.0 candidate.

### Runtime gate

v1.1.0 is not runtime PASS until a new video/test verifies:

- 100+ selected ghosts on a suitable test level;
- Last blue aura and Best gold aura;
- persistent archive across level exit/re-entry;
- actual GD PB vs Best Echo labels;
- Replay Studio launcher/settings/history selection;
- death markers and heat strip;
- all playback controls/camera modes;
- camera restoration;
- frame-time behavior under 100/128/256 ghost loads.

## Out of scope for v1.1.0 implementation

These approved long-range ideas remain compatible with the architecture but are not required to claim the v1.1.0 source/build milestone: 512-ghost certified mode, arbitrary per-ghost manual color editing for all 256 entries, full density/percentile visualization modes, bookmarks, replay export/import, and platformer-specific time/points analytics. They require separate runtime-informed follow-up rather than being superficially bolted onto this release.
