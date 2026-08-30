# DASH ECHO v0.1 — Recorder Architecture

Status: **IMPLEMENTED IN SOURCE / UNTESTED BY PROJECT DECISION**

Gameplay/build verification is intentionally deferred until the integrated v1.0 milestone.

## Mission

v0.1 establishes the authoritative attempt-state recording layer used by every later DASH ECHO subsystem.

The recorder is deliberately independent from rendering, UI, persistence, analytics, and cinematic camera logic.

## Capture lifecycle

1. `PlayLayer::postUpdate(float dt)` runs normal Geometry Dash behavior first.
2. DASH ECHO captures one `FrameRecord` after that update.
3. Each frame contains:
   - monotonic frame sequence
   - attempt-relative time
   - current level progress percentage
   - player 1 transform/visibility snapshot
   - player 2 transform/visibility snapshot
4. `PlayLayer::resetLevel()` finalizes the current attempt with `Reset`, performs the normal reset, then begins the next attempt.
5. `PlayLayer::levelComplete()` finalizes the current attempt with `Completed` before normal completion behavior.
6. `PlayLayer::onExit()` finalizes any remaining active attempt with `LayerExit` before leaving the layer.

## Data model

### PlayerSnapshot

Renderer-neutral visual transform state:

- present
- visible
- x / y
- rotation
- scaleX / scaleY

### FrameRecord

One captured gameplay state sample:

- sequence
- timeSeconds
- progressPercent
- player1
- player2

### AttemptRecord

One contiguous attempt:

- attemptId
- ordered frame vector
- durationSeconds
- maxProgressPercent
- endReason
- finalized

### RecorderStats

Session diagnostics:

- attemptsStarted
- attemptsFinalized
- framesCaptured
- framesDropped
- retainedAttempts
- retainedFrames

## Retention and safety

v0.1 is memory-only. It does not write Geometry Dash saves or account data.

Hard bounds prevent runaway memory use:

- 250,000 frames maximum per attempt
- 64 retained attempts maximum
- 1,000,000 retained frames maximum per PlayLayer session

Oldest finalized attempts are evicted first when retention bounds are exceeded. The active attempt is never intentionally evicted.

## Stability rules

- Invalid/non-finite `dt` values become zero.
- Captured `dt` is clamped to 0.25 seconds to prevent a single anomalous update from corrupting replay time.
- Progress is constrained to 0–100 percent.
- Missing player pointers produce an explicit `present = false` snapshot instead of dereferencing null.
- Frame-cap overflow increments `framesDropped` instead of growing memory without limit.

## v0.1 public contract for later versions

Later systems should consume the recorder through:

- `latestFinalizedAttempt()` — primary input for the v0.2 previous-attempt ghost
- `attempts()` — future multi-ghost/history source
- `activeAttempt()` — future live comparison/debug source
- `stats()` — future diagnostics/HUD source

The renderer must not mutate recorded attempt data.

## Explicitly out of scope until later versions

- ghost sprites/rendering
- replay playback
- interpolation
- input recording
- death markers
- heatmaps
- disk persistence
- replay file format
- timeline UI
- slow motion/scrubbing
- cinematic camera
- Android/mobile support

These exclusions are intentional; they prevent v0.1 from becoming coupled to later presentation systems.
