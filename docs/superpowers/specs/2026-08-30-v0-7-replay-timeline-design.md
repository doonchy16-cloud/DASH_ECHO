# DASH ECHO v0.7 — Replay Timeline Design

Status: **APPROVED FOR IMPLEMENTATION / MAIN ONLY / RUNTIME TESTING DEFERRED TO v1.0**

## Goal

Introduce a replay-time authority that owns one immutable replay clip and exposes deterministic fixed-speed timeline playback independently from recorder retention.

## Core decision: owned replay clip

A selected replay is copied into an owned `ReplayClip`. This intentionally trades bounded one-clip memory duplication for pointer safety and deterministic replay lifetime.

The clip survives recorder eviction because it contains its own copied `AttemptRecord` and copied `AttemptHistoryEntry` metadata. The active recorder never becomes coupled to the replay studio's lifetime.

## Components

### `EchoReplayTimeline`

Owns:

- copied finalized `AttemptRecord`
- copied `AttemptHistoryEntry`
- derived timeline markers
- playback cursor
- fixed v0.7 playback state

States:

- Empty
- Ready
- Playing
- Finished

v0.7 playback rate is exactly 1.0x. Variable speed and user scrubbing are v0.8 responsibilities.

### `EchoReplaySession`

Owns:

- one `EchoReplayTimeline`
- one `EchoGhost` renderer dedicated to replay-studio playback

The session binds the ghost only to the timeline's owned clip, never directly to recorder storage.

## Loading

`EchoReplayTimeline::load(attempt, history)` requires:

- finalized attempt
- non-empty frames
- matching attempt IDs
- finite, non-negative, monotonic frame timestamps
- positive renderable duration based on the last actual frame timestamp

Invalid clips are rejected rather than silently sorted or repaired. Replay history must preserve recorded truth.

## Timeline markers

Derived markers are immutable clip metadata:

- Start at first captured timestamp
- End at final captured timestamp
- Death marker when history contains copied death context
- Completion marker for completed attempts
- Personal Best marker when the history entry was PB at finalization

Markers store absolute time, normalized 0–1 timeline position, and progress percentage where meaningful.

## Cursor and progress

The timeline cursor is clamped to the renderable frame range. `progressPercentAtCursor()` uses timestamp bracketing and linear progress interpolation for UI display only.

The replay ghost itself continues using the v0.3 interpolation/discontinuity engine, so timeline progress interpolation never controls player transforms.

## Session playback

`EchoReplaySession` can:

- attach its dedicated ghost renderer to the gameplay world layer
- load a finalized attempt/history pair
- start from the beginning
- advance at fixed 1x using sanitized `dt`
- restart
- stop/clear

`advance(dt)` moves the timeline first, then synchronizes the replay ghost to the authoritative timeline cursor.

## Integration boundary

v0.7 adds the replay session to PlayLayer fields and prepares the most recently finalized attempt as the replay candidate after history commit. It does not yet expose interactive controls or enter replay mode automatically.

This makes v0.8 purely an interaction/playback-control milestone rather than forcing it to redesign replay ownership.

## Safety boundaries

- replay clip is read-only after load
- replay ghost is visual-only
- no save/account mutation
- no collision/physics authority
- no automatic replay entry
- no build/runtime test before v1.0

## Source acceptance criteria

- replay remains valid after recorder eviction because clip is owned
- invalid/non-monotonic clips are rejected
- one timeline cursor is authoritative for replay session
- replay ghost consumes timeline time, not its own clock
- markers are deterministic and immutable after load
- v0.8 can add seek/speed without changing clip ownership
- main remains the only branch
