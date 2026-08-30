# DASH ECHO v0.7 — Replay Timeline Architecture

Status: **SOURCE IMPLEMENTED / RUNTIME VERIFICATION DEFERRED TO v1.0**

## Authority

`EchoReplayTimeline` is replay-time authority. It owns one immutable copied replay clip and one cursor. `EchoReplaySession` owns the timeline plus one dedicated visual-only `EchoGhost`.

## Owned clip

Loading requires a finalized attempt, matching history ID, at least two frames, finite non-negative monotonic timestamps, and positive renderable duration. Accepted data is copied so recorder retention cannot invalidate the replay.

## Markers

The clip derives deterministic Start, End, Death, Completion, and PersonalBest markers. v0.7 strengthened v0.6 history by preserving the exact terminal death timestamp instead of estimating it from duration/progress.

## Time semantics

The cursor remains in original recorded attempt-time coordinates for exact compatibility with `EchoGhost::synchronize`. UI normalization is derived separately from `(cursor - firstFrameTime) / duration`.

v0.7 advances only at fixed 1x. v0.8 will expose pause, speed, seek, stepping, and interactive controls without changing replay ownership.

## PlayLayer integration

After recorder finalization and immutable history commit, the exact finalized attempt/history pair is copied into the replay session. The replay is prepared but never auto-started in v0.7.

## Runtime obligations for v1.0

- verify copied-clip memory cost on long attempts
- verify timestamp monotonicity assumptions in real GD sessions
- verify exact death-marker timing
- verify replay ghost coordinate/z-order behavior
- verify lifecycle behavior around reset/completion/exit

No runtime PASS is claimed before those tests.
