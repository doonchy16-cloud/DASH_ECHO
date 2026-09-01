# ECHO_DASH Hardening 03 — Playback, Performance, and UX Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the existing ghost/replay presentation bounded and deterministic, wire the existing Rendering Quality setting truthfully, and harden Replay Studio interaction/layout without adding capabilities.

**Architecture:** Extend the single role-agnostic ghost playback engine to return one reusable frame-resolution value per selected ghost, keep cursor caches subordinate to authority, and make derived overlays/UI revision-driven. Add a pure rendering-quality controller and Replay Studio view/layout models so controls become a responsive projection of session authority instead of refreshing/building state ad hoc every frame.

**Tech Stack:** C++23, existing Cocos2d/Geode presentation APIs, dependency-free CTest core tests, Python source contracts, Windows Release runtime/performance evidence.

**Spec:** `docs/superpowers/specs/2026-09-01-echo-dash-quality-hardening-design.md`

## Global Constraints

- Run only after Plan 02 is terminal GREEN.
- One `EchoGhostPlaybackEngine` remains the sole timing authority.
- `GhostRole` may affect only color/opacity/trail/z-presentation, never source time, frame indices, interpolation alpha, or completion.
- Existing configured ceiling stays 256 ghosts; no new LOD/selection feature is introduced.
- Rendering Quality affects presentation cost only; it may not alter recorder sample rate, stored replay bytes, death analytics, attempt results, or timing authority.
- Live gameplay remains free of a persistent launcher.
- Replay Studio keeps the same controls/cameras/speeds/navigation capabilities.
- No heavy archive maintenance is introduced into gameplay hot paths.
- Prepared steady-state ghost update code must perform no ECHO_DASH-owned heap allocation.

---

## File Structure for This Plan

**Create:**
- `src/EchoPlaybackResolution.hpp` — reusable cursor/result types shared by playback engine and ghost presentation.
- `src/EchoRenderingQuality.hpp`
- `src/EchoRenderingQuality.cpp` — pure requested/effective quality policy and Auto hysteresis.
- `src/EchoReplayViewState.hpp` — immutable Replay Studio projection type/truth context.
- `src/EchoReplayLayout.hpp`
- `src/EchoReplayLayout.cpp` — pure responsive panel/control geometry model.
- `tests/cpp/test_playback_resolution.cpp`
- `tests/cpp/test_role_neutrality.cpp`
- `tests/cpp/test_rendering_quality.cpp`
- `tests/cpp/test_replay_view_state.cpp`
- `tests/cpp/test_replay_layout.cpp`

**Modify:**
- `src/EchoGhostPlaybackEngine.hpp/.cpp`
- `src/EchoGhost.hpp/.cpp`
- `src/EchoGhostFleet.hpp/.cpp`
- `src/EchoDeathOverlay.hpp/.cpp`
- `src/EchoHeatmapOverlay.hpp/.cpp`
- `src/EchoReplaySession.hpp/.cpp`
- `src/EchoReplayControls.hpp/.cpp`
- `src/EchoRuntimeCoordinator.hpp/.cpp`
- `src/main.cpp`
- `src/EchoSettings.hpp/.cpp`
- `CMakeLists.txt`
- `tests/test_v1_1_contract.py`

---

### Task 1: Return one canonical playback resolution per ghost per frame

**Files:**
- Create: `src/EchoPlaybackResolution.hpp`
- Create: `tests/cpp/test_playback_resolution.cpp`
- Create: `tests/cpp/test_role_neutrality.cpp`
- Modify: `src/EchoGhostPlaybackEngine.hpp/.cpp`
- Modify: `src/EchoGhost.hpp/.cpp`
- Modify: `src/EchoGhostFleet.hpp/.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces:

```cpp
struct GhostPlaybackCursor {
    std::size_t lowerFrameIndex = 0;
    double lastSourceTimeSeconds = 0.0;
    bool valid = false;
};

struct GhostFrameResolution {
    double sourceTimeSeconds = 0.0;
    std::size_t fromIndex = 0;
    std::size_t toIndex = 0;
    float interpolationAlpha = 0.0f;
    bool visible = false;
    bool finished = false;
};
```

Extend the engine with:

```cpp
[[nodiscard]] GhostFrameResolution resolve(
    AttemptRecord const& attempt,
    bool progressAlignmentSafe,
    GhostPlaybackCursor& cursor
) const;
```

Change `EchoGhost` presentation to:

```cpp
void synchronize(GhostFrameResolution const& resolution);
```

while its already-bound `AttemptRecord const*` remains the frame source.

- [ ] **Step 1: Write failing resolution tests**

Build a replay with frames at `t=0`, `1`, `2`. Track live time at `0.5`; assert `fromIndex=0`, `toIndex=1`, alpha `0.5`, visible true, finished false. Resolve `2.5`; assert finished true and visible false.

- [ ] **Step 2: Write cursor invalidation tests**

Resolve forward through a replay, then force a source-time decrease by resetting/tracking earlier live time. The result must remain correct even if the cache was ahead; the implementation invalidates/reseeks rather than trusting stale cursor position.

- [ ] **Step 3: Write role metamorphic tests before changing fleet code**

Use one replay and four role labels in the test fixture. For each role, call the same engine with equivalent state and assert identical source time, indices, alpha, visible, and finished values. The test source must not pass `GhostRole` into any engine API.

- [ ] **Step 4: Prove RED and implement `resolve`**

`resolve` first obtains authoritative source time using the existing Tracking/Continuing logic, then resolves frame indices. When source time is monotonic and cursor valid, advance from `lowerFrameIndex`; otherwise use `std::lower_bound` and refresh the cache. Clamp alpha to `[0,1]` and reject non-finite derived values safely.

- [ ] **Step 5: Remove the second per-ghost timeline seek from `EchoGhost`**

Delete/retire `seekFrameCursor(double)` and internal `m_frameIndex`/`m_lastSynchronizedTime` as timing lookup authority. `synchronize(resolution)` validates indices against the bound attempt and applies exactly those frames.

- [ ] **Step 6: Store one cursor and one latest resolution in each fleet slot**

Extend `Slot`:

```cpp
GhostPlaybackCursor playbackCursor;
GhostFrameResolution resolution;
```

`renderFromPlaybackEngine()` calls `resolve()` once for each active slot, stores the result, and passes it to `ghost.synchronize`. `drawTrailForSlot` consumes `slot.resolution.sourceTimeSeconds` and `fromIndex`; it may not call the engine again.

- [ ] **Step 7: Prove GREEN and commit**

Run CTest/Python suite.

```bash
git add src/EchoPlaybackResolution.hpp src/EchoGhostPlaybackEngine.* src/EchoGhost.* src/EchoGhostFleet.* tests/cpp CMakeLists.txt
git commit -m "perf: resolve each ECHO_DASH ghost once per frame"
```

---

### Task 2: Bound steady-state ghost/trail work and eliminate ECHO-owned hot-loop allocation

**Files:**
- Modify: `src/EchoGhostFleet.hpp/.cpp`
- Modify: `src/EchoGhost.hpp/.cpp`
- Modify: `tests/cpp/test_playback_resolution.cpp`
- Modify: `tests/test_v1_1_contract.py`

**Interfaces:**
- Produces: fixed-capacity priority-trail scratch storage and explicit per-frame counters for test/diagnostic instrumentation.

Add:

```cpp
struct GhostFleetFrameStats {
    std::size_t resolutions = 0;
    std::size_t synchronizedGhosts = 0;
    std::size_t trailSegments = 0;
};

[[nodiscard]] GhostFleetFrameStats lastFrameStats() const;
```

- [ ] **Step 1: Write source/behavior contracts for the hot path**

Assert `renderFromPlaybackEngine()` contains no `make_unique`, `new`, `push_back`, `emplace_back`, `resize`, or `reserve`. Assert frame stats resolutions never exceed active ghost count and trail segments never exceed `2 * 64` under Full quality.

- [ ] **Step 2: Prove RED for frame stats/bounds**

Expected: compile/source contract FAIL before instrumentation exists.

- [ ] **Step 3: Preallocate all fleet slots during `rebuild/ensurePool`, not render**

Keep the existing vector/unique_ptr pool, but allocation is legal only in attach/rebuild/safe-boundary configuration. Reset each slot cursor/resolution without reallocation.

- [ ] **Step 4: Replace per-frame temporary trail containers with fixed-capacity scratch**

Use `std::array` or direct bounded iteration. Full-quality priority trail remains capped at 64 segments per priority ghost; there are at most two priority identities.

- [ ] **Step 5: Increment frame stats without formatting/logging in the hot loop**

Reset the three counters at render start and increment them as work occurs. No dynamic diagnostic strings are produced here.

- [ ] **Step 6: Prove GREEN and commit**

```bash
git add src/EchoGhostFleet.* src/EchoGhost.* tests
git commit -m "perf: bound ECHO_DASH ghost presentation work"
```

---

### Task 3: Make existing Rendering Quality truthful with deterministic presentation-only policies

**Files:**
- Create: `src/EchoRenderingQuality.hpp`
- Create: `src/EchoRenderingQuality.cpp`
- Create: `tests/cpp/test_rendering_quality.cpp`
- Modify: `src/EchoSettings.hpp/.cpp`
- Modify: `src/EchoGhostFleet.hpp/.cpp`
- Modify: `src/EchoRuntimeCoordinator.hpp/.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces:

```cpp
struct RenderingBudget {
    std::size_t priorityTrailSegments = 64;
    std::uint8_t olderGhostPresentationInterval = 1;
};

enum class EffectiveRenderingQuality : std::uint8_t { Full, Balanced, Performance };

class EchoRenderingQualityController final {
public:
    void setRequested(RenderingQuality requested);
    void observeFrame(double echoPresentationMs, double frameBudgetMs);
    [[nodiscard]] EffectiveRenderingQuality effective() const;
    [[nodiscard]] RenderingBudget budget() const;
};
```

Fixed policies:

```text
Full        trail segments 64, older presentation interval 1 frame
Balanced    trail segments 40, older presentation interval 1 frame
Performance trail segments 24, older presentation interval 2 frames
```

Auto policy:

```text
frame cost budget = max(0.25 ms, 12% of current finite frame budget)
degrade after 30 consecutive observations above budget
recover one level after 180 consecutive observations below 60% of budget
60-observation cooldown after any quality transition
```

Priority Last/Best ghosts continue presentation every frame in all modes. Older-ghost interval changes presentation cadence only; the shared engine remains timing authority and replay data is untouched.

- [ ] **Step 1: Write deterministic mode tests**

Assert each explicit requested mode produces the exact budget above and never changes a `CaptureSettings` value.

- [ ] **Step 2: Write Auto hysteresis tests**

Feed 29 overloaded observations -> no change; 30th -> one-level degradation. Feed fewer than 180 recovery observations -> no recovery; 180 after cooldown -> one-level recovery. Rapid alternating pressure cannot oscillate.

- [ ] **Step 3: Prove RED and implement controller**

Use only fixed-size counters/state; no allocations or wall-clock I/O inside the controller.

- [ ] **Step 4: Measure only ECHO presentation work**

Wrap the fleet/overlay presentation portion with `std::chrono::steady_clock` and pass elapsed milliseconds plus sanitized frame budget to the controller. Do not include Geometry Dash base `postUpdate` time in the ECHO cost sample.

- [ ] **Step 5: Apply effective budget in fleet rendering**

Trail segment cap uses `priorityTrailSegments`. In Performance, Older-only slots apply their new resolved pose every second presentation frame; Last/Best/LastAndBest still synchronize visually every frame. Even when visual application is skipped, no role-specific time is invented.

- [ ] **Step 6: Prove Rendering Quality cannot alter recorder authority**

Add a C++ property test that cycles all requested/effective modes and asserts the normalized settings capture sample rate remains identical.

- [ ] **Step 7: Prove GREEN and commit**

```bash
git add src/EchoRenderingQuality.* src/EchoSettings.* src/EchoGhostFleet.* src/EchoRuntimeCoordinator.* tests/cpp CMakeLists.txt
git commit -m "fix: make Rendering Quality presentation-only and truthful"
```

---

### Task 4: Make death/heat presentation revision-driven

**Files:**
- Modify: `src/EchoDeathOverlay.hpp/.cpp`
- Modify: `src/EchoHeatmapOverlay.hpp/.cpp`
- Modify: `src/main.cpp`
- Modify: `tests/test_v1_1_contract.py`

**Interfaces:**
- Produces on both overlays:

```cpp
void refreshIfNeeded(EchoDeathAnalytics const& analytics);
[[nodiscard]] std::uint64_t renderedRevision() const;
```

- [ ] **Step 1: Add failing source contracts**

Assert `main.cpp::postUpdate` no longer calls unconditional `deathOverlay.refresh(...)` or `heatmapOverlay.refresh(...)` every frame.

- [ ] **Step 2: Add `m_renderedRevision` to each overlay**

`refreshIfNeeded` compares `analytics.revision()` and presentation-settings revision. Rebuild once when data or relevant presentation settings change; otherwise return immediately.

- [ ] **Step 3: Replace death-event immediate full rebuild calls with revision publication**

A recorded death increments analytics revision; the next legal render update invokes `refreshIfNeeded`. No duplicate analytical reconstruction is done in `destroyPlayer` plus `postUpdate`.

- [ ] **Step 4: Prove GREEN and commit**

```bash
git add src/EchoDeathOverlay.* src/EchoHeatmapOverlay.* src/main.cpp tests/test_v1_1_contract.py
git commit -m "perf: refresh death presentation by revision"
```

---

### Task 5: Publish an immutable Replay Studio view state

**Files:**
- Create: `src/EchoReplayViewState.hpp`
- Create: `tests/cpp/test_replay_view_state.cpp`
- Modify: `src/EchoReplaySession.hpp/.cpp`
- Modify: `src/EchoReplayControls.hpp/.cpp`
- Modify: `src/main.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces:

```cpp
struct ReplayTruthContext {
    float gdLevelBestPercent = 0.0f;
    float bestRecordedPercent = 0.0f;
    float sessionBestPercent = 0.0f;
    bool platformer = false;
};

struct ReplayStudioViewState {
    std::uint64_t revision = 0;
    std::uint64_t attemptId = 0;
    bool loaded = false;
    bool playing = false;
    bool hasPreviousAttempt = false;
    bool hasNextAttempt = false;
    bool canStepPrevious = false;
    bool canStepNext = false;
    double elapsedSeconds = 0.0;
    double durationSeconds = 0.0;
    float normalizedPosition = 0.0f;
    float progressPercent = 0.0f;
    float playbackRate = 1.0f;
    CinematicCameraMode cameraMode = CinematicCameraMode::Recorded;
    ReplayTruthContext truth;
};
```

Add to `EchoReplaySession`:

```cpp
void setTruthContext(ReplayTruthContext const& truth);
[[nodiscard]] ReplayStudioViewState viewState() const;
[[nodiscard]] std::uint64_t structuralRevision() const;
```

- [ ] **Step 1: Write session-view tests**

With an unloaded session: loaded false and navigation/step controls disabled. With a loaded three-frame replay: attempt ID, duration, normalized position, previous/next step availability, speed, camera mode, and truth context reflect session/archive state.

- [ ] **Step 2: Prove RED**

Expected: compile FAIL before view-state interface exists.

- [ ] **Step 3: Implement structural revision rules**

Increment structural revision only when loaded attempt, play state, playback rate, camera mode, navigation availability, or truth context changes. Elapsed/progress cursor movement alone does not force structural label rebuild.

- [ ] **Step 4: Remove semantic truth storage from controls**

Delete `m_gdLevelBestPercent`, `m_bestRecordedPercent`, `m_sessionBestPercent`, and `m_platformer` from `EchoReplayControls`; replace `setProgressContext` with `session.setTruthContext` at the coordinator/main authority boundary.

- [ ] **Step 5: Split control refresh**

Expose private `refreshStructural(ReplayStudioViewState const&)` and `refreshFast(ReplayStudioViewState const&)`. Structural refresh runs only when view revision changes; fast refresh updates slider/time/progress while Studio playback advances.

- [ ] **Step 6: Prove GREEN and commit**

```bash
git add src/EchoReplayViewState.hpp src/EchoReplaySession.* src/EchoReplayControls.* src/main.cpp tests/cpp/test_replay_view_state.cpp CMakeLists.txt
git commit -m "refactor: project Replay Studio from session view state"
```

---

### Task 6: Make Replay Studio responsive, truthful, and edge-safe

**Files:**
- Create: `src/EchoReplayLayout.hpp`
- Create: `src/EchoReplayLayout.cpp`
- Create: `tests/cpp/test_replay_layout.cpp`
- Modify: `src/EchoReplayControls.hpp/.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces:

```cpp
struct ReplayStudioLayout {
    float panelWidth = 0.0f;
    float panelHeight = 0.0f;
    float left = 0.0f;
    float right = 0.0f;
    float titleY = 0.0f;
    float truthY = 0.0f;
    float timelineY = 0.0f;
    float transportY = 0.0f;
    float utilityY = 0.0f;
};

[[nodiscard]] ReplayStudioLayout computeReplayStudioLayout(
    float viewportWidth,
    float viewportHeight
);
```

- [ ] **Step 1: Write layout tests for representative aspect ratios**

Test `640x480`, `854x480`, `1280x720`, and `3440x1440` logical layouts. Assert panel remains inside viewport safe margins, row Y values are strictly ordered, and left/right controls remain inside panel bounds.

- [ ] **Step 2: Prove RED and implement deterministic layout**

Use safe margin `12`, panel width clamped to `[520, min(viewportWidth-24, 900)]`, and panel height clamped to `[150, min(viewportHeight-24, 210)]`. Compute rows from panel bounds rather than fixed world coordinates.

- [ ] **Step 3: Retain the same existing controls but group them by hierarchy**

Header/attempt/truth at top, timeline in the middle, transport below, restart/speed/camera/settings/close in utility/header groups. Do not add a control.

- [ ] **Step 4: Give controls explicit IDs and larger hit targets**

Create stable IDs for previous/next attempt, previous/next frame, play/pause, restart, speed, camera, settings, close, and slider. Visual font size may remain compact; button background/hit area is at least 44 logical pixels in its primary dimension where layout permits.

- [ ] **Step 5: Drive enabled states from `ReplayStudioViewState`**

Previous/next attempt and frame controls visibly disable when unavailable. Disabled callbacks return without session mutation. Play label, speed label, and camera label come only from view state.

- [ ] **Step 6: Prove GREEN and commit**

```bash
git add src/EchoReplayLayout.* src/EchoReplayControls.* tests/cpp/test_replay_layout.cpp CMakeLists.txt
git commit -m "fix: harden Replay Studio layout and control states"
```

---

### Task 7: Make scrub/open/close behavior transactional and input-safe

**Files:**
- Modify: `src/EchoReplayControls.hpp/.cpp`
- Modify: `src/EchoRuntimeCoordinator.hpp/.cpp`
- Modify: `src/main.cpp`
- Modify: `tests/test_v1_1_contract.py`

**Interfaces:**
- Produces: explicit `m_userScrubbing` UI fact; coordinator remains semantic Studio-state authority.

- [ ] **Step 1: Add scrub ownership tests/contracts**

While `m_userScrubbing` is true, `refreshFast` may update time/progress labels but may not overwrite slider thumb position. On scrub end, one clamped `seekNormalized` occurs and ordinary cursor following resumes.

- [ ] **Step 2: Clamp/finite-check every seek entry**

`EchoReplaySession::seekNormalized` rejects NaN and clamps finite values to `[0,1]`; `seekSeconds` clamps to `[0,duration]`. Successful seek synchronizes replay pose and camera continuity before publishing the new view revision/cursor.

- [ ] **Step 3: Make Studio open a two-phase UI transaction**

Required order:

```text
validate replay/session loaded
-> coordinator accepts OpenStudio
-> capture viewport
-> initialize/show Studio panel and input blocker
-> hide normal fleet presentation
-> only then allow PauseLayer::onResume to remove vanilla pause
```

If panel/input-blocker creation fails after coordinator acceptance, call `closeReplayStudio`, restore viewport/fleet state, and return false so PauseLayer remains.

- [ ] **Step 4: Add a full-screen Cocos touch blocker behind Studio controls**

Use a `CCLayer`/touch-dispatcher node attached only while Studio is open; its touch begin returns true to swallow pointer/touch interaction outside Studio. It is presentation plumbing, not a new launcher/control. Remove it idempotently on close/teardown.

- [ ] **Step 5: Make close restore ownership atomically**

Stop replay -> destroy/hide Studio input/panel -> restore viewport if still valid -> coordinator accepts CloseStudio -> resume fleet tracking if state is Playing. Scene teardown invalidates restore state and wins over restoration.

- [ ] **Step 6: Add source contracts**

PauseLayer still contains exactly one ordinary ECHO entry. No live gameplay launcher field/node is added. Studio cannot be open while coordinator state is DeathContinuation/Resetting/Exiting.

- [ ] **Step 7: Prove GREEN and commit**

```bash
git add src/EchoReplayControls.* src/EchoRuntimeCoordinator.* src/main.cpp tests/test_v1_1_contract.py
git commit -m "fix: make Replay Studio transitions transactional"
```

---

### Task 8: Add performance regression instrumentation and Plan-03 evidence gate

**Files:**
- Modify: `src/EchoGhostFleet.hpp/.cpp`
- Modify: `src/main.cpp`
- Modify: `tests/cpp/test_rendering_quality.cpp`
- Modify: `tests/test_v1_1_contract.py`

**Interfaces:**
- Produces: bounded counters needed later by diagnostics; runtime benchmark checklist evidence, not a new visible feature.

- [ ] **Step 1: Add structural regression checks**

Prove:

- no archive compaction/maintenance in ordinary live `postUpdate`;
- no unconditional overlay full rebuild each frame;
- no `GhostRole` dependency in engine;
- one `resolve(` call site inside the fleet per active slot path;
- hot loop contains no ECHO-owned dynamic-container growth calls;
- Rendering Quality code never references recorder setters.

- [ ] **Step 2: Run complete native/Python suite**

Expected: zero failures.

- [ ] **Step 3: Trigger full pinned Windows Release workflow and wait for terminal success**

No runtime smoothness claim is made yet.

- [ ] **Step 4: Run the runtime performance matrix on the exact candidate produced by this gate**

Record incremental ECHO_DASH frame cost and observed behavior for configured ghost counts `1, 8, 16, 64, 128, 256`, with priority trails and existing death/heat presentation. Repeat at recorder 120 Hz and 240 Hz. Capture current/worst and a representative p95/p99-like sample from diagnostics instrumentation once Plan 04 exposes it; until then retain external frame-time logs/video.

- [ ] **Step 5: Runtime UX checklist on the same candidate**

Verify Pause -> ECHO -> Studio, previous/next attempt edges, previous/next frame edges, play/pause, restart, all existing speeds, all existing camera modes, slider drag, close/viewport restore, odd aspect ratio if available, and repeated open/close. Any gameplay-input leakage while Studio is open is a FAIL requiring a fix before Plan 04.

- [ ] **Step 6: Record exact commit SHA, workflow run, package hash, and runtime evidence; proceed to Plan 04 only after source/build tests are terminal and no known UX regression remains.**
