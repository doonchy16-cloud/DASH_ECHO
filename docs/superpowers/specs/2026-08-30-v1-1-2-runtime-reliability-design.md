# ECHO_DASH v1.1.2 Runtime Reliability Design

## Status
Approved for implementation by Doonchy via GO on 2026-08-30.

## Scope
v1.1.2 hardens the runtime foundation beneath the v1.1.1 Unified Ghost Engine without changing the user-visible ghost synchronization doctrine that is still awaiting in-game certification.

The v1.1.1 Unified Ghost Engine v4 installer remains a frozen runtime-test baseline. v1.1.2 source work proceeds independently and must not be packaged as the replacement runtime candidate until its own verification gates pass.

## Locked invariants

1. Every historical ghost continues to use the one canonical EchoGhostPlaybackEngine.
2. Ghost roles affect presentation only, never playback authority.
3. Archive mutation may not occur while the fleet owns archive-backed replay pointers.
4. Every finalized run remains archive-worthy subject only to explicit retention/storage safety policy.
5. A malformed replay must never be trusted as playback authority.
6. A recoverable archive failure must not silently erase usable history.
7. Runtime settings polling must continue while Replay Studio is open.
8. Build/package PASS never implies in-game runtime PASS.

## Reliability architecture

### 1. Persistent known-good backup

The archive save pipeline becomes:

1. Serialize new state to `<archive>.tmp`.
2. Re-open and validate the temp candidate before authority rotation.
3. If the current primary exists and validates, preserve it as `<archive>.bak`.
4. Atomically replace the primary with the validated temp candidate.
5. Keep `<archive>.bak` after success as the previous known-good generation.

A failed or invalid current primary must never overwrite a valid backup.

### 2. Recovery-on-load

Load authority order:

1. Primary archive.
2. Previous known-good `.bak` only when the primary exists but fails validation/read, or when primary is missing and backup exists.
3. Empty archive only when neither candidate can be accepted.

If backup recovery succeeds:

- archive data is loaded from backup;
- recovery is exposed through archive stats/diagnostics;
- the valid backup bytes are restored to the primary path when safely possible;
- the event is logged visibly enough for diagnostics but does not crash gameplay.

### 3. Semantic replay validation and quarantine

Structural binary parsing and semantic replay validity are distinct.

A structurally readable replay is accepted only if:

- attempt ID is non-zero;
- replay is finalized;
- replay contains frames;
- duration and max progress are finite and in legal ranges;
- every frame has finite non-negative time;
- frame timestamps are monotonic non-decreasing;
- frame sequence numbers strictly increase;
- frame progress is finite and within 0..100;
- present player snapshots contain finite transform values;
- present camera snapshots contain finite transform values;
- absurd transform magnitudes are rejected using intentionally broad safety bounds.

A semantically invalid but structurally readable replay is quarantined by omission from the trusted replay deque. Its failure must not poison unrelated valid replays from the same archive. The corresponding summary may remain available as historical metadata if it is itself valid.

A structurally unreadable replay cannot be safely skipped in schema v1 because records are not length-prefixed; that candidate archive is rejected and backup recovery is attempted.

### 4. Summary validation

Summaries must reject non-finite/illegal duration, progress, timing, improvement, and death snapshot values. Duplicate attempt IDs are not accepted as independent authoritative records.

### 5. Load telemetry

ReplayArchiveStats gains at least:

- `recoveredFromBackup`
- `quarantinedReplayCount`

These are session/load telemetry, not persisted authority.

### 6. Settings refresh invariant

The existing main-loop order is preserved and regression-locked:

`settings poll -> applyEchoDashSettings(false) -> Replay Studio early return`

This allows settings changed from Replay Studio to take effect without requiring Studio to close.

### 7. Continuation safety

The v1.1.1 continuation lifecycle remains:

- confirmed death -> shared engine Continuing;
- vanilla reset requests are deferred;
- the reset lifecycle executes only after vanilla has requested reset and all selected historical ghosts have finished;
- fleet releases replay pointers before archive mutation.

v1.1.2 adds regression coverage around these invariants but does not invent new continuation behavior before v1.1.1 v4 is runtime-tested.

## Non-goals

- No Ghost Multiverse selection intelligence yet.
- No adaptive LOD yet.
- No new spectator camera yet.
- No post-death speed controls yet.
- No schema-v2 record framing in this patch.
- No new gameplay-speed simulation; recorded level speed remains part of recorded game reality, Replay Studio speed remains viewer-only.

## Verification gates

1. RED contract proving the current source lacks retained-backup recovery and semantic quarantine.
2. GREEN contract after implementation.
3. Pinned Geode CLI 3.7.4.
4. Pinned Geode SDK 5.10.1.
5. Windows Release compile.
6. Compiler evidence artifact.
7. Exactly one `.geode` candidate artifact.
8. Independent package inspection before any installer package is produced.
9. In-game runtime certification remains separate.
