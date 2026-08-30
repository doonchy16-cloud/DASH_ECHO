# DASH ECHO v0.3 — Ghost Synchronization Architecture

Status: **SOURCE IMPLEMENTED / RUNTIME UNVERIFIED BY PROJECT DECISION**

Gameplay/build verification remains deferred until the integrated v1.0 milestone.

## Intended outcome

Make the previous-attempt ghost visually continuous and timeline-stable without inventing motion across real discontinuities such as teleports, mode transitions, visibility changes, large scale jumps, or long capture gaps.

## Root-cause correction

v0.2 maintained a second elapsed-time accumulator inside the ghost. Even when both clocks consumed the same `dt`, two independently accumulated timelines were an unnecessary drift risk and would complicate future pause, speed-control, and scrub behavior.

v0.3 removes the ghost-local playback clock.

**The active `EchoRecorder` timeline is now the sole playback-time authority.**

`DashEchoPlayLayer` captures the current frame, then calls:

`ghost.synchronize(recorder.activeElapsedSeconds())`

The ghost is therefore a view over authoritative recorded time rather than a second clock owner.

## Explicit lifecycle hardening

Adversarial source review found a second upstream weakness inherited from v0.1: `captureFrame()` could implicitly begin an attempt whenever no active attempt existed. That behavior risked generating phantom attempts if `postUpdate()` occurred after completion.

v0.3 fixes the generation point:

- `EchoRecorder::captureFrame()` now records only into an explicitly active attempt.
- `DashEchoPlayLayer` owns whether capture is enabled.
- initial/normal gameplay explicitly starts an attempt before capture.
- reset finalizes the old attempt, performs Geometry Dash reset, then explicitly begins the next attempt.
- completion and layer exit disable capture before finalization.

This prevents future history/analytics features from inheriting phantom attempt records.

## Frame synchronization

Normal forward playback uses an amortized O(1) frame cursor.

Backward or otherwise non-monotonic time seeks use binary search. This is intentionally included now so future replay scrubbing does not require replacing the synchronization primitive.

For a time between samples A and B:

1. Locate A <= time < B.
2. Compute normalized alpha from the recorded timestamps.
3. If B is marked continuous from A, interpolate.
4. If B is marked discontinuous from A, hold A until B's exact timestamp and then snap to B.

## Interpolated state

Continuous boundaries interpolate:

- x / y position
- scale X / Y
- primary and secondary RGB colors
- rotation using the shortest angular path

Mode is not interpolated. Continuous samples are required to have the same mode.

## Continuity authority

Continuity is derived once when the recorder captures the newer frame and stored in `FrameRecord` independently for player 1 and player 2.

This prevents every future consumer from inventing its own teleport/discontinuity rules.

A boundary is currently non-continuous if any material condition applies:

- either player sample is absent
- either player sample is invisible
- gameplay mode changes
- either snapshot contains non-finite transform data
- sample delta is <= 0 or greater than 100 ms
- displacement exceeds 120 world units AND implied speed exceeds 3000 world units/second
- scale X or Y changes by more than 0.35 in one sample boundary

The displacement thresholds are conservative source-level heuristics and are **not runtime-certified yet**. v1.0 testing must validate or tune them against real Geometry Dash portal, teleport, dash, speed, and high-refresh-rate behavior.

## End-of-data rule

The ghost renderable timeline ends at the final actual recorded frame timestamp rather than `AttemptRecord::durationSeconds`.

Reason: if an attempt reaches the hard per-attempt frame cap, duration can continue increasing while no new samples exist. Holding the final ghost pose until attempt duration would fabricate state. v0.3 stops rendering after the last authoritative sample instead.

## Dependency impact

### v0.4 multiple ghosts

Can reuse the same synchronization engine per ghost without duplicating timing logic.

### v0.7 replay timeline / v0.8 scrubbing

Backward/non-monotonic seek support already exists in the synchronization primitive.

### pause / playback-speed controls

Because the ghost accepts an authoritative time instead of owning `dt`, later systems can change the supplied timeline without rewriting interpolation.

## Adversarial review

Potential remaining weak links requiring v1.0 evidence:

- heuristic teleport thresholds may be too high or too low for unusual levels
- exact Geometry Dash visual animation state is still not recorded
- jetpack-specific visual representation remains outside the current PlayerMode set
- runtime `SimplePlayer` fidelity across every icon/mode transition is unverified
- gameplay pause semantics depend on the actual `PlayLayer::postUpdate` behavior and remain runtime-unverified

These are not declared resolved without evidence.

## Verification gate

At v1.0, test at minimum:

- normal cube movement at multiple refresh rates
- ship, ball, UFO, wave, robot, spider, and swing transitions
- dual mode
- mini/size portals
- teleport portals and other abrupt position changes
- speed portals and high-speed sections
- pause/resume
- completion followed by any residual `postUpdate` activity, checking that no phantom attempt is created
- long attempts
- an attempt reaching or simulating recorder frame limits
- backward seek behavior when replay controls are available

Until that evidence exists, v0.3 remains **SOURCE IMPLEMENTED / 🟡 HOLD FOR RUNTIME VERIFICATION**.
