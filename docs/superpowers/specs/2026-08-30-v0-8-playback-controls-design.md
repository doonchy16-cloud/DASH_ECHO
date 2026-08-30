# DASH ECHO v0.8 — Playback Speed & Scrubbing Design

Status: **APPROVED FOR IMPLEMENTATION / MAIN ONLY / RUNTIME TESTING DEFERRED TO v1.0**

## Goal

Turn the v0.7 owned replay timeline into an interactive Replay Studio with pause/resume, speed presets, frame stepping, restart, normalized slider scrubbing, and a compact in-game control surface.

## Authority rule

The timeline remains the only replay-time authority. UI controls issue commands; they never maintain a parallel cursor or playback clock.

## Timeline extensions

`EchoReplayTimeline` gains:

- `Paused` state
- pause / resume / toggle semantics
- playback-rate authority
- supported presets: 0.10x, 0.25x, 0.50x, 1.00x, 2.00x
- absolute seek
- normalized 0–1 seek
- frame-step forward/backward
- cursor-to-frame bracketing that supports backward/non-monotonic movement

Seeking or stepping pauses playback intentionally. This prevents an active clock from fighting a user drag/step operation.

## Replay session extensions

`EchoReplaySession` delegates all control operations to the timeline and immediately re-synchronizes its dedicated replay ghost after every seek, step, restart, or state transition.

## Replay Studio UI

A dedicated `EchoReplayControls` node owns presentation only:

- ECHO launcher button when a replay candidate exists
- Replay Studio bottom panel
- attempt ID / progress / time labels
- play/pause button
- restart button
- previous-frame button
- next-frame button
- speed-cycle button + current speed label
- close button
- Geometry Dash `Slider` for normalized scrubbing

Slider callback reads `SliderThumb::getValue()` and sends that normalized value to the replay session. The slider never owns cursor truth.

## Studio-mode gameplay isolation

When Replay Studio is open, the PlayLayer hook does not call normal `PlayLayer::postUpdate` and does not capture new recorder frames. Instead it advances only the replay session and refreshes controls.

This freezes the active attempt's DASH ECHO timeline and avoids contaminating the current attempt with replay-watching time. Normal gameplay resumes from the same active attempt when Studio closes.

The historical multighost fleet is hidden while Studio is open to prevent replay ambiguity. It resumes synchronization from the active-attempt recorder clock after Studio closes.

## Important uncertainty

Skipping normal `PlayLayer::postUpdate` is source-level isolation, not yet runtime-proven full-engine pause semantics. Other independently scheduled Cocos/GD components may still animate. v1.0 must explicitly verify whether additional pause/input isolation is needed.

## Scrubbing semantics

- normalized 0 maps to first recorded sample time
- normalized 1 maps to final recorded sample time
- slider seek pauses replay
- step backward selects the preceding distinct frame timestamp
- step forward selects the following distinct frame timestamp
- seek clamps to replay bounds
- ghost synchronization uses existing v0.3 binary-search/backward-seek support

## Playback-rate semantics

Rates are restricted to the five preset values. `cyclePlaybackRate()` advances in this order:

0.10 → 0.25 → 0.50 → 1.00 → 2.00 → 0.10

Playback rate multiplies sanitized replay `dt`; it never modifies GD physics or the active recorder clock.

## Safety

- no save/account mutation
- replay visuals remain non-colliding
- controls cannot mutate recorded replay clip data
- no automatic replay entry
- close always leaves a prepared replay candidate available for reopening
- no runtime/build testing before v1.0

## Source acceptance criteria

- one authoritative cursor under play, pause, seek, and step
- no independent UI clock
- backward seek works without rebinding source clip
- slider value derives from timeline when not user-driving a seek
- active attempt recorder time does not advance during Studio mode
- Studio can close and normal capture can continue
- main remains only branch
