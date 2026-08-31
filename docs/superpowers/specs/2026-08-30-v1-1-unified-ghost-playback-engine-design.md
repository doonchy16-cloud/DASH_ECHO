# ECHO_DASH v1.1 Unified Ghost Playback Engine Design

## Status

Approved by Doonchy on 2026-08-30: **Approach A — one shared hybrid playback engine for every ghost, with full post-death continuation until each historical attempt ends.**

## Problem

The current v1.1.1 alignment hotfix split ghost synchronization by role. Best Recorded can be progress-aligned while other ghosts remain elapsed-time aligned. This causes two architectural failures:

1. Best Recorded freezes when the live player dies because live progress stops advancing.
2. Different ghost roles effectively run on different playback rules.

Role-specific playback is rejected. Ghost identity may control presentation only.

## Locked invariant

**Every historical ghost runs through one canonical `EchoGhostPlaybackEngine`. `GhostRole` may affect color, trail, opacity, and labels, but must never affect synchronization, timeline advancement, continuation, completion, or interpolation authority.**

## Engine phases

The shared engine has exactly two runtime phases.

### Tracking

While the live attempt is active, the engine receives one canonical live input tuple:

- live elapsed seconds
- live progress percent
- whether progress is authoritative for this level mode

For classic levels with monotonic recorded progress, every historical attempt resolves its source timestamp from the same progress-mapping algorithm. For modes where progress is not authoritative, including platformer, every historical attempt uses the same elapsed-time fallback. There is no Best-specific or Last-specific clock.

### Continuing

When a real player death is confirmed, the engine freezes the Tracking anchor once:

- death elapsed seconds
- death progress percent
- progress-authority mode

From that anchor, one continuation clock advances by real `dt`. For every historical attempt:

`source time = source time at death anchor + shared continuation elapsed`

Each ghost therefore continues naturally from the exact point where it was displayed at death. A ghost hides only after its own recorded timeline ends. The continuation phase ends only when all selected historical attempts have ended.

## Death/reset lifecycle

A confirmed death does **not** immediately finalize/mutate the archive because the fleet holds pointers into archive-owned replay records. Instead:

1. Capture the authoritative death event frame.
2. Record death analytics.
3. Stop live recorder sampling for the dead attempt.
4. Start the fleet's shared Continuing phase using the death anchor.
5. If Geometry Dash requests `resetLevel()` while continuation is active, defer the reset instead of mutating archive/fleet state.
6. Continue advancing all ghosts until the shared engine reports all selected attempts finished.
7. Stop the fleet, finalize/persist the dead attempt, call the original Geometry Dash reset, rebuild archive-backed ghost selection, reset the playback engine to Tracking, and start the next attempt.

This preserves archive pointer safety and the existing immediate persistence behavior at the actual reset boundary.

If there are no historical ghosts with remaining playback after the death anchor, normal reset behavior is allowed immediately.

## Manual reset and completion

- Manual reset while the player is alive remains immediate and does not enter Continuing.
- Level completion remains immediate and does not enter Continuing.
- Layer exit cancels continuation, safely stops fleet references, then finalizes/saves through the existing lifecycle.

## Component boundaries

### `EchoGhostPlaybackEngine.hpp/.cpp`

New role-agnostic timing authority. It owns:

- phase (`Tracking` / `Continuing`)
- live/death anchor input
- continuation elapsed time
- progress monotonicity eligibility
- progress-to-recorded-time mapping
- source-time resolution for any `AttemptRecord`
- finished-state calculation for any `AttemptRecord`

It must not include or reference `GhostRole`.

### `EchoGhostFleet.hpp/.cpp`

Fleet remains responsible for:

- selecting archive-owned attempts
- allocating/reusing visual ghost slots
- assigning visual roles
- applying opacity/trail styling
- asking the single playback engine for each slot's source time
- determining whether all active slots are finished under that engine

Fleet must remove role-specific timing predicates and its own progress-mapping implementation.

### `main.cpp`

PlayLayer integration owns lifecycle orchestration only:

- feed Tracking input each live frame
- arm Continuing once on confirmed death
- defer Geometry Dash reset while continuation is active
- advance continuation each frame
- perform the real reset only after continuation completes

## Performance and safety

- Progress monotonicity is evaluated once per selected attempt at fleet rebuild and cached per slot.
- Source-time lookup may use binary search over recorded progress; no per-frame linear scan of entire attempts.
- `dt` is finite-checked and clamped before continuation advancement.
- No archive mutation occurs while fleet slots reference archive-owned replay records.
- Existing 0–256 ghost limit remains unchanged.
- No forced chunk loading, gameplay mutation, physics replay, or player-control injection is introduced.

## User-visible behavior

- All ghosts stay aligned by the same engine while the player is alive.
- When the player dies, ghosts do **not** freeze.
- All historical ghosts continue until each recorded attempt itself dies/ends.
- Blue Last, gold Best, and older ghosts differ only visually.
- Reset/new attempt happens after the historical continuation finishes.

## Non-goals for this repair

- No new spectator-camera system is added in this pass.
- No new skip-continuation button is added in this pass.
- No replay archive schema change is required.
- The embedded mod version remains v1.1.1 while runtime certification is still open; release candidates are distinguished by package hash.

## Acceptance gates

1. Contract test proves `EchoGhostPlaybackEngine` exists and contains no `GhostRole` reference.
2. Contract test proves fleet owns exactly one playback engine and role is absent from timing-resolution predicates.
3. Contract test proves main has a confirmed-death continuation path and deferred reset path.
4. Contract test proves continuation finishes only after all active selected attempts report finished.
5. Existing v1.1.1 regression contract remains green.
6. Geode CLI 3.7.4 and SDK 5.10.1 Windows Release build is terminal green.
7. Runtime remains HOLD until Doonchy confirms in Geometry Dash that ghosts continue after death and remain aligned.