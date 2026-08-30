# DASH ECHO v0.8 — Playback Controls / Replay Studio Architecture

Status: **SOURCE IMPLEMENTED / RUNTIME HOLD UNTIL v1.0**

## Mission

v0.8 turns the v0.7 owned replay clip into an interactive Replay Studio without introducing a second replay clock, allowing UI state to become authoritative, or leaving the historical replay trapped in the frozen active-attempt camera.

## Time authority

`EchoReplayTimeline` is the sole replay-time authority.

It owns:

- replay cursor
- timeline state (`Ready`, `Playing`, `Paused`, `Finished`)
- playback rate
- absolute seek
- normalized seek
- previous/next distinct-frame stepping

Supported playback rates are deliberately restricted to:

- 0.10x
- 0.25x
- 0.50x
- 1.00x
- 2.00x

Unsupported rates are rejected rather than silently creating arbitrary timing behavior.

## Seek and step semantics

- normalized 0 maps to the first recorded sample timestamp
- normalized 1 maps to the final recorded sample timestamp
- absolute and normalized seeks clamp to clip bounds
- seeking pauses playback
- stepping pauses playback
- previous-frame stepping selects the preceding distinct recorded timestamp
- next-frame stepping selects the following distinct recorded timestamp
- backward movement relies on the v0.3 ghost seek/binary-search behavior rather than rebinding the clip

## Session authority

`EchoReplaySession` delegates control commands to the timeline and then re-synchronizes the dedicated replay ghost immediately after any operation that can affect cursor state.

The session does not own a separate timer.

Closing Studio calls `stop()`, which returns the loaded replay candidate to its start position and hides/stops its visual ghost without clearing the owned replay clip. The candidate can therefore be reopened.

## Presentation authority

`EchoReplayControls` owns presentation only.

It provides:

- ECHO launcher
- Replay Studio bottom panel
- attempt label
- elapsed / duration label
- progress label
- play/pause control
- restart control
- previous-frame control
- next-frame control
- speed-cycle control
- close control
- native Geometry Dash `Slider`

The native slider callback reads `SliderThumb::getValue()` and sends that normalized value into `EchoReplaySession::seekNormalized()`.

On refresh, slider state is derived back from `EchoReplayTimeline::normalizedCursor()`.

## Recorded Replay Viewport

Adversarial review found that freezing the active attempt also freezes its camera. A historical ghost driven through world-space coordinates would therefore eventually run outside the frozen viewport.

v0.8 fixes this at the recording layer instead of patching the renderer:

- every `FrameRecord` now includes a minimal `CameraSnapshot` of the Geometry Dash object-layer transform
- camera position, rotation, scale X, and scale Y are recorded after normal GD frame update
- camera continuity is classified independently from player continuity
- long sample gaps, large viewport jumps, large zoom changes, and large rotation jumps create snap boundaries
- `EchoReplayTimeline::cameraAtCursor()` uses the exact same replay cursor as the ghost
- continuous camera segments interpolate position/scale and shortest-path rotation
- discontinuous camera segments snap rather than inventing fake camera movement

This is **recorded viewport reproduction**, not the v0.9 cinematic camera. v0.9 may override or transform this baseline intentionally.

## Active viewport restoration

When Studio opens, `DashEchoPlayLayer` snapshots the active attempt's object-layer position, rotation, and scale before applying the recorded replay viewport.

When Studio closes or a lifecycle transition forces closure, those exact active-attempt values are restored before normal gameplay continues.

This prevents replay review from permanently mutating the live attempt camera state.

## Studio isolation

When Replay Studio is open, `DashEchoPlayLayer::postUpdate(float dt)`:

1. does not call normal `PlayLayer::postUpdate(dt)`
2. does not capture active-attempt recorder frames
3. advances only the owned historical replay session
4. applies the recorded replay viewport from the same authoritative cursor
5. refreshes Replay Studio controls
6. returns

This keeps DASH ECHO's active-attempt recorder clock frozen while the user reviews a historical replay.

The historical multighost fleet is hidden while Studio is open. When Studio closes, the active viewport is restored and the fleet is re-synchronized from the unchanged active-attempt recorder clock.

If `destroyPlayer` is reached while Studio is open, Geometry Dash still receives its normal callback, but DASH ECHO deliberately excludes that callback from death analytics so replay-review activity cannot corrupt historical death data.

Reset, completion, and layer exit force Studio closed before attempt finalization.

## Owned replay invariant

Studio controls and recorded viewport reproduction operate on the v0.7 owned `ReplayClip`. They do not hold pointers into recorder retention and do not mutate the recorded clip.

## Safety boundaries

v0.8 does not intentionally:

- modify Geometry Dash save files
- modify account data
- inject gameplay input
- alter player physics
- alter collision authority
- alter completion authority
- modify unrelated Geode mods

## Source-level uncertainties deferred to v1.0

The following require real runtime evidence and are therefore not claimed as PASS:

1. Whether skipping `PlayLayer::postUpdate` fully freezes every independently scheduled GD/Cocos gameplay subsystem.
2. Whether additional input isolation is needed while Replay Studio is open.
3. Whether `m_objectLayer` alone reproduces every visual camera-layer effect used by all level/camera triggers.
4. Exact camera-continuity thresholds under unusual instant camera triggers.
5. Exact visual placement/scaling of the bottom panel across supported window/aspect configurations.
6. Native Slider drag behavior and touch priority alongside Geometry Dash UI in a live level.
7. Whether any third-party mod's hook ordering requires additional Studio-mode compatibility guards.
8. Replay ghost and recorded viewport correctness under aggressive backward scrubbing.

## v0.8 source acceptance checklist

- one authoritative cursor: implemented
- five canonical replay speeds: implemented
- pause/resume/toggle: implemented
- normalized + absolute seek: implemented
- distinct-frame stepping: implemented
- seek/step pause behavior: implemented
- UI has no parallel clock: implemented
- slider commands timeline and refreshes from timeline: implemented
- active recorder clock does not advance in Studio hook path: implemented
- recorded object-layer viewport captured per replay frame: implemented
- replay viewport derives from the same replay cursor: implemented
- active-attempt viewport captured/restored around Studio mode: implemented
- fleet hidden during Studio and re-synchronized on close: implemented
- Studio death callbacks excluded from DASH ECHO analytics: implemented
- lifecycle transitions close Studio first: implemented
- runtime/build verification: intentionally deferred to v1.0
