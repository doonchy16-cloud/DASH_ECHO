# DASH ECHO v0.2 — Previous-Attempt Ghost

Status: **IMPLEMENTED IN SOURCE / RUNTIME VERIFICATION DEFERRED TO v1.0**

## Intended outcome

After an attempt is reset, the immediately preceding finalized attempt becomes a translucent, non-interactive visual ghost during the next attempt.

v0.2 is intentionally limited to one previous-attempt ghost. Multi-ghost selection belongs to v0.4.

## Architecture

```text
PlayLayer
  ├─ EchoRecorder
  │    └─ AttemptRecord / FrameRecord / PlayerSnapshot
  └─ EchoGhost
       └─ SimplePlayer visual nodes
```

The recorder owns historical state. The ghost owns presentation/playback. The ghost never mutates recorded frames.

## Recorder extension

v0.2 extends `PlayerSnapshot` with renderer-relevant state that would otherwise force a later rewrite:

- gameplay mode: cube / ship / ball / UFO / wave / robot / spider / swing
- primary player color
- secondary player color

Existing v0.1 transform state remains authoritative:

- presence
- visibility
- X/Y
- rotation
- scale X/Y

## Ghost attachment

`EchoGhost` attaches two hidden `SimplePlayer` nodes to the same parent coordinate space as the live player whenever possible.

This avoids inventing a second coordinate transform and prevents the renderer from depending on camera-space guesses.

The ghost is inserted one z-order below player 1 when player 1 and its parent are available.

## Playback lifecycle

### Normal attempt

`PlayLayer::postUpdate(dt)`:

1. Geometry Dash performs its normal update.
2. DASH ECHO ensures the ghost visual nodes are attached.
3. The current attempt frame is recorded.
4. The previous-attempt ghost advances by the same bounded delta time.

### Reset

`PlayLayer::resetLevel()`:

1. stop/hide the currently displayed ghost
2. finalize the current attempt as `Reset`
3. call Geometry Dash's normal reset
4. begin the new recorder attempt
5. select `latestFinalizedAttempt()` as the single v0.2 ghost source

### Completion

The active ghost is stopped and the current attempt is finalized as `Completed` before normal level-completion behavior continues.

### Layer exit

The active ghost is stopped and any active attempt is finalized as `LayerExit` before normal layer exit.

## Timing

v0.2 uses timestamp-gated discrete frame playback.

- Playback elapsed time is bounded with the same 0.25-second per-update maximum used by the recorder.
- The ghost remains hidden until the first recorded frame timestamp is reached.
- Playback advances to the latest recorded frame whose timestamp is less than or equal to elapsed playback time.
- When recorded duration is exceeded, playback stops and both ghost nodes hide.

No interpolation is performed in v0.2. Interpolation and tighter synchronization are explicitly reserved for v0.3.

## Visual state

The ghost:

- uses the player's selected icon IDs through `GameManager`
- updates icon type when the recorded gameplay mode changes
- reapplies primary/secondary colors when recorded colors change
- uses recorded position, rotation, scale, and visibility
- renders at opacity `96 / 255`
- has no collision, gameplay input, or authoritative game-state role

## Dual-player handling

Two visual nodes are allocated from the beginning:

- ghost player 1
- ghost player 2

Player 2 remains hidden whenever the recorded snapshot reports it absent or invisible. This avoids requiring a later renderer redesign when dual mode appears.

## Safety and authority

v0.2 remains local and presentation-only.

It does not intentionally:

- change Geometry Dash save files
- change account state
- change completion percentages
- inject player inputs
- affect collisions
- affect physics
- write replay files
- modify unrelated Geode mods

## Adversarial review notes

### Pointer lifetime

`EchoGhost` references the latest finalized `AttemptRecord` owned by `EchoRecorder`. During a live attempt, recorder retention trimming does not run on frame capture; trimming occurs at attempt boundaries. The ghost source is selected only after the new attempt begins, after boundary trimming has completed.

### Coordinate space

Ghost nodes use the live player's parent when available instead of assuming world/camera transforms. This is the smallest durable correction for coordinate consistency.

### Mode-change reconstruction

The official GD 2.2081 bindings expose player mode flags and `SimplePlayer::updatePlayerFrame`, so v0.2 records mode rather than institutionalizing a cube-only placeholder.

### Known deferred limitations

These are not declared resolved in v0.2:

- interpolation between frames
- exact sub-frame synchronization under unusual timing behavior
- gameplay animation reconstruction beyond `SimplePlayer` icon mode
- trail reconstruction
- death particles
- platformer jetpack-specific visual reconstruction
- persistent replay storage

Those limitations are explicit future milestones, not hidden PASS claims.

## Verification status

Per project instruction, no build or in-game verification is performed before v1.0.

Therefore the truthful state is:

**SOURCE IMPLEMENTED — RUNTIME HOLD**
