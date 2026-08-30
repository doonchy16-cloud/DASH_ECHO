# DASH ECHO v0.4 — Ghost Fleet / Multiverse Architecture

Status: **SOURCE IMPLEMENTED / RUNTIME UNVERIFIED BY PROJECT DECISION**

Gameplay/build verification remains deferred until the integrated v1.0 milestone.

## Intended outcome

Render multiple historical attempts simultaneously without duplicating replay logic, allowing unbounded render growth, losing personal-best history, or introducing independent playback clocks.

## Architecture

`EchoRecorder` remains historical truth and the sole current-attempt time authority.

`EchoGhost` remains one renderer for one immutable `AttemptRecord`.

`EchoGhostFleet` owns a fixed array of reusable `EchoGhost` slots and is responsible only for:

- historical-attempt selection
- personal-best inclusion
- opacity ranking
- deterministic z-order
- synchronization fan-out
- hard rendering capacity
- releasing historical references at attempt boundaries

The fleet does not record gameplay and does not mutate `AttemptRecord` data.

## Hard capacity

`EchoGhostFleet::kMaxGhosts = 6`.

Each selected attempt can contain player 1 and player 2 visuals, so the absolute fleet visual bound is twelve `SimplePlayer` ghost nodes.

The six slots are allocated once per `PlayLayer`; active playback is limited to the configured historical-attempt count.

This prevents attempt count from becoming rendering cost.

## User-configurable count

`mod.json` exposes `ghost-count` as an integer setting:

- minimum: 0
- maximum: 6
- default: 4

The value is also clamped in C++ before it reaches `EchoGhostFleet`.

A value of 0 disables historical ghost playback while leaving recording active.

The setting is read only at reset/attempt boundaries. Mid-attempt setting edits therefore cannot mutate fleet selection while historical pointers are active.

## Selection policy

At each reset, after recorder finalization and retention trimming:

1. Stop the old fleet and release historical references.
2. Finalize the previous active attempt.
3. Reset Geometry Dash.
4. Begin the new authoritative attempt.
5. Read and clamp `ghost-count`.
6. Select newest finalized, non-empty attempts up to the configured limit.
7. Obtain the personal-best attempt from `EchoRecorder` authority.
8. If PB is not already selected and the fleet is full, replace the oldest member of the newest-N window with PB.
9. Sort selected attempts oldest -> newest for deterministic layering.
10. Assign reusable fleet slots and begin synchronized playback.

The newest attempt is never displaced to make room for PB.

## Personal-best authority

PB identification is centralized in `EchoRecorder::personalBestAttempt()`.

Definition:

- highest `maxProgressPercent` wins
- equal-progress ties prefer the newer `attemptId`

The fleet does not implement a second PB algorithm.

### PB retention

Normal recorder retention may evict old finalized attempts, but the current personal-best attempt is pinned.

When retention limits are exceeded, the recorder evicts the oldest finalized **non-PB** attempt first.

This prevents the PB ghost from silently degrading into "best among remaining history" after enough attempts.

## Opacity hierarchy

Selected attempts receive deterministic age fading:

- oldest normal selected attempt: approximately 42/255 opacity
- newest normal selected attempt: approximately 108/255 opacity
- selected PB: at least 138/255 opacity

PB keeps its recorded player colors; it is distinguished through stronger opacity rather than falsifying its recorded visual state.

## Layering

Fleet slots are attached in fixed oldest -> newest z-order beneath the live player.

This keeps newer historical attempts visually above older ghosts while ensuring the current player remains the gameplay focus.

## Synchronization

Every active fleet slot receives the exact same:

`recorder.activeElapsedSeconds()`

No fleet member owns an independent clock.

Each `EchoGhost` reuses v0.3 interpolation, discontinuity detection, mode reconstruction, color reconstruction, and frame seeking.

## Out-of-range seek correction

v0.4 corrects a v0.3 future-scrubbing weak link.

Previously, synchronizing beyond the final recorded frame called `stop()`, which discarded the source attempt. That prevented a later backward seek from restoring the ghost.

Now out-of-range time hides the ghost while preserving the source binding. If time later returns to the recorded range, binary-search cursor recovery can render it again.

Explicit `stop()` remains the authority boundary that releases the source attempt.

## Pointer / retention safety

Fleet slots hold pointers into the recorder deque only during one active attempt.

Before finalization or any retention operation is allowed to evict historical attempts, `DashEchoPlayLayer::resetLevel`, `levelComplete`, and `onExit` call `fleet.stop()`.

`fleet.stop()` clears both the internal `EchoGhost` source and slot metadata pointers.

The new fleet is rebuilt only after retention work has completed.

## Failure recovery

Fleet attachment is transactional at the source level:

- attach slots sequentially
- if any slot fails to attach, detach all slots already attached
- leave fleet state unattached

No partially initialized fleet is intentionally treated as ready.

## Performance model

Per update:

- recorder captures one current frame
- fleet synchronizes at most six historical attempts
- each ghost uses amortized O(1) forward frame-cursor movement during normal gameplay
- only active slots synchronize

Mode/color updates remain cached inside each `EchoGhost`.

## Adversarial review / residual risk

Runtime evidence is still required for:

- actual performance cost of up to twelve `SimplePlayer` nodes on target hardware
- visual readability of the 42–138 opacity range
- z-order behavior across unusual custom levels/layers
- dual-mode rendering fidelity with several simultaneous attempts
- exact interaction between Geode settings changes and an already-open PlayLayer
- existing v0.3 teleport/discontinuity thresholds
- exact mode/animation fidelity not represented by current snapshots

These remain **UNVERIFIED**, not PASS.

## v1.0 verification additions

In addition to prior tests, verify:

- ghost-count values 0, 1, 4, and 6
- 6-ghost normal level
- 6-ghost dual mode
- an old PB surviving beyond 64 total attempts
- PB outside the newest-N window
- a new tied PB replacing an older tied PB
- opacity/layering readability
- repeated reset/rebuild cycles
- fleet attach/cleanup across leaving and reopening levels
- backward seek from beyond a ghost's final frame once replay controls exist

Until runtime evidence exists, v0.4 remains **SOURCE IMPLEMENTED / 🟡 HOLD FOR RUNTIME VERIFICATION**.
