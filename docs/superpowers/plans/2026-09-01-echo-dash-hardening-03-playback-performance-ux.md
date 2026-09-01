# ECHO_DASH Hardening 03 — Playback, Performance, and UX Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make current ghost/replay presentation bounded and deterministic, wire the existing Rendering Quality setting truthfully, and harden Replay Studio layout/interaction without adding capabilities.

**Architecture:** Extend the one role-agnostic ghost engine so each selected ghost yields one reusable frame-resolution value per frame. Cursor caches remain subordinate to authoritative time. Make derived presentation revision-driven. Add pure Rendering Quality and Replay Studio view/layout models so UI is a responsive projection of session authority rather than a second state owner.

**Tech Stack:** C++23, Cocos2d/Geode 5.10.1 presentation APIs, dependency-free CTest core mode established in Plan 01, Python contracts, pinned Windows Release/runtime evidence.

**Spec:** `docs/superpowers/specs/2026-09-01-echo-dash-quality-hardening-design.md`

## Global Constraints

- Run only after Plan 02 is terminal GREEN.
- Exactly one `EchoGhostPlaybackEngine` remains timing authority.
- `GhostRole` affects presentation only; no role-dependent source time/frame/interpolation/completion logic.
- Configured ceiling remains 256 ghosts; this plan does not add a new LOD/selection feature.
- Rendering Quality changes presentation cost only; never recorder sample rate, replay bytes, attempt outcome, analytics truth, or synchronization authority.
- Last/Best priority semantics stay blue/gold and visually dominant; no aura returns.
- Replay Studio keeps the same controls, speeds, camera modes, and pause-menu entrypoint.
- No persistent live-gameplay launcher.
- Prepared steady-state ghost update code performs zero ECHO_DASH-owned heap allocation.
- Heavy archive maintenance stays outside latency-sensitive gameplay work.

---

## File Structure

**Create:**
- `src/EchoPlaybackResolution.hpp`
- `src/EchoRenderingQuality.hpp/.cpp`
- `src/EchoReplayViewState.hpp`
- `src/EchoReplayLayout.hpp/.cpp`
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
- `src/EchoSettings.hpp/.cpp`
- `src/main.cpp`
- `CMakeLists.txt`
- `tests/test_v1_1_contract.py`

---

### Task 1: Resolve each ghost timeline exactly once per frame

**Files:**
- Create: `src/EchoPlaybackResolution.hpp`
- Create: `tests/cpp/test_playback_resolution.cpp`
- Create: `tests/cpp/test_role_neutrality.cpp`
- Modify: `src/EchoGhostPlaybackEngine.hpp/.cpp`
- Modify: `src/EchoGhost.hpp/.cpp`
- Modify: `src/EchoGhostFleet.hpp/.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

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

[[nodiscard]] GhostFrameResolution EchoGhostPlaybackEngine::resolve(
    AttemptRecord const& attempt,
    bool progressAlignmentSafe,
    GhostPlaybackCursor& cursor
) const;
```

`EchoGhost` changes from `synchronize(double)` to:

```cpp
void synchronize(GhostFrameResolution const& resolution);
```

Its already-bound `AttemptRecord` remains the frame source.

- [ ] **Step 1: Write failing interpolation/completion tests**

Fixture frames at t=0,1,2. At authoritative source t=0.5 expect indices 0/1, alpha 0.5, visible true, finished false. At source beyond final timestamp+epsilon expect finished true/visible false.

- [ ] **Step 2: Write cursor-authority tests**

Resolve monotonically forward, then force a backward source-time discontinuity. Result must still be canonical: invalidate/reseek cache rather than trusting stale index. Attempt change and replay restart also reset cursor.

- [ ] **Step 3: Write GhostRole metamorphic tests**

For the same replay/live state, repeat resolution under test labels Older/Last/Best/Last+Best without passing role to the engine. Source time, indices, alpha, visible, and finished must match exactly.

- [ ] **Step 4: Prove RED and implement engine `resolve`**

Compute authoritative source time using existing Tracking/Continuing logic, then frame indices. Use cursor advance only while source time is finite/nondecreasing and cursor belongs to the current attempt; otherwise `lower_bound` canonical search. Clamp finite alpha to [0,1].

- [ ] **Step 5: Remove duplicate timeline-seek authority from `EchoGhost`**

Retire `seekFrameCursor`, `m_frameIndex`, and `m_lastSynchronizedTime` as independent lookup state. `synchronize(resolution)` validates indices against the bound replay and applies exactly those frames.

- [ ] **Step 6: Store cursor/result per fleet slot**

```cpp
struct Slot {
    EchoGhost ghost;
    AttemptRecord const* attempt = nullptr;
    GhostRole role = GhostRole::Older;
    bool progressAlignmentSafe = false;
    GhostPlaybackCursor playbackCursor;
    GhostFrameResolution resolution;
};
```

`renderFromPlaybackEngine` invokes one `resolve` per active slot and all pose/trail consumers reuse `slot.resolution`.

- [ ] **Step 7: Run CTest/Python regressions and commit**

```bash
git add src/EchoPlaybackResolution.hpp src/EchoGhostPlaybackEngine.* src/EchoGhost.* src/EchoGhostFleet.* tests/cpp CMakeLists.txt
git commit -m "perf: resolve each ECHO_DASH ghost once per frame"
```

---

### Task 2: Bound steady-state fleet/trail work and allocation

**Files:**
- Modify: `src/EchoGhostFleet.hpp/.cpp`
- Modify: `src/EchoGhost.hpp/.cpp`
- Modify: `tests/test_v1_1_contract.py`

**Interfaces:**

```cpp
struct GhostFleetFrameStats {
    std::size_t resolutions = 0;
    std::size_t synchronizedGhosts = 0;
    std::size_t trailSegments = 0;
};
[[nodiscard]] GhostFleetFrameStats lastFrameStats() const;
```

- [ ] **Step 1: Write source/behavior contracts first**

Inside the steady render body forbid ECHO-owned `new`, `make_unique`, `push_back`, `emplace_back`, `resize`, or `reserve`. Assert `resolutions <= activeGhostCount` and Full-quality priority trail segments <= 128 total (64 each for at most two priority identities).

- [ ] **Step 2: Prove RED before stats/bounds exist**

- [ ] **Step 3: Preallocate only at attach/rebuild/safe settings boundaries**

`ensurePool` may allocate outside the frame loop. Slot cursor/result reset in place.

- [ ] **Step 4: Replace transient trail containers with fixed-capacity/direct bounded iteration**

Use `std::array` scratch or direct draw iteration. Never retain unbounded history for a visible trail.

- [ ] **Step 5: Add numeric frame stats only**

Reset/increment counters without log formatting or dynamic strings in the hot loop.

- [ ] **Step 6: GREEN + commit**

```bash
git add src/EchoGhostFleet.* src/EchoGhost.* tests/test_v1_1_contract.py
git commit -m "perf: bound ECHO_DASH ghost presentation work"
```

---

### Task 3: Make the existing Rendering Quality setting deterministic and presentation-only

**Files:**
- Create: `src/EchoRenderingQuality.hpp/.cpp`
- Create: `tests/cpp/test_rendering_quality.cpp`
- Modify: `src/EchoSettings.hpp/.cpp`
- Modify: `src/EchoGhostFleet.hpp/.cpp`
- Modify: `src/EchoRuntimeCoordinator.hpp/.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

```cpp
enum class EffectiveRenderingQuality : std::uint8_t { Full, Balanced, Performance };

struct RenderingBudget {
    std::size_t priorityTrailSegments = 64;
    std::uint8_t olderGhostPresentationInterval = 1;
};

class EchoRenderingQualityController final {
public:
    void setRequested(RenderingQuality requested);
    void observeFrame(double echoPresentationMs, double frameBudgetMs);
    [[nodiscard]] EffectiveRenderingQuality effective() const;
    [[nodiscard]] RenderingBudget budget() const;
};
```

Fixed policy:

```text
Full        64 priority-trail segments, Older presentation every 1 frame
Balanced    40 priority-trail segments, Older presentation every 1 frame
Performance 24 priority-trail segments, Older presentation every 2 frames
```

Auto:

```text
budget = max(0.25 ms, 12% of finite current frame budget)
degrade one level after 30 consecutive over-budget observations
recover one level after 180 consecutive observations below 60% of budget
60-observation cooldown after any transition
```

Last/Best/LastAndBest presentation remains every frame in all modes. Older cadence changes visual application frequency only; timing is still resolved canonically.

- [ ] **Step 1: Write explicit-mode tests**

Each requested mode yields its exact budget. Cycle all quality values and assert capture sample rate/value object remains unchanged.

- [ ] **Step 2: Write Auto hysteresis tests**

29 over-budget samples -> no degradation; 30th -> one level. Recovery requires cooldown plus 180 low samples. Alternating load cannot oscillate.

- [ ] **Step 3: Prove RED, implement fixed-state controller, prove GREEN**

No allocation/I/O/logging in `observeFrame`.

- [ ] **Step 4: Measure only ECHO presentation time**

Use `steady_clock` around fleet/overlay presentation, not vanilla `PlayLayer::postUpdate`. Pass measured ECHO milliseconds plus sanitized frame budget to controller.

- [ ] **Step 5: Apply budget**

Trail cap follows budget. Performance may skip applying the newly resolved pose to Older-only ghost sprites on alternate frames; resolution still occurs from the canonical engine and priority identities remain every-frame.

- [ ] **Step 6: GREEN + commit**

```bash
git add src/EchoRenderingQuality.* src/EchoSettings.* src/EchoGhostFleet.* src/EchoRuntimeCoordinator.* tests/cpp/test_rendering_quality.cpp CMakeLists.txt
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

```cpp
void refreshIfNeeded(EchoDeathAnalytics const& analytics, std::uint64_t presentationRevision);
[[nodiscard]] std::uint64_t renderedAnalyticsRevision() const;
[[nodiscard]] std::uint64_t renderedPresentationRevision() const;
```

- [ ] **Step 1: Add failing source contract**

`postUpdate` must not unconditionally call full `deathOverlay.refresh(...)`/`heatmapOverlay.refresh(...)` every frame.

- [ ] **Step 2: Track analytics + relevant presentation revision in each overlay**

Return immediately when both match. Rebuild exactly once when death data or the overlay's own appearance settings change.

- [ ] **Step 3: Remove duplicate rebuild from death callback plus frame update**

Death recording increments analytics revision; presentation refresh consumes the new revision once.

- [ ] **Step 4: GREEN + commit**

```bash
git add src/EchoDeathOverlay.* src/EchoHeatmapOverlay.* src/main.cpp tests/test_v1_1_contract.py
git commit -m "perf: refresh death presentation by revision"
```

---

### Task 5: Publish Replay Studio as immutable session view state

**Files:**
- Create: `src/EchoReplayViewState.hpp`
- Create: `tests/cpp/test_replay_view_state.cpp`
- Modify: `src/EchoReplaySession.hpp/.cpp`
- Modify: `src/EchoReplayControls.hpp/.cpp`
- Modify: `src/main.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

```cpp
struct ReplayTruthContext {
    float gdLevelBestPercent = 0.0f;
    float bestRecordedPercent = 0.0f;
    float sessionBestPercent = 0.0f;
    bool platformer = false;
    bool operator==(ReplayTruthContext const&) const = default;
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

Session adds `setTruthContext`, `viewState`, and `structuralRevision`.

- [ ] **Step 1: Write unloaded/loaded view tests**

Unloaded disables navigation/step. A loaded known replay reports attempt/duration/cursor/progress/rate/camera/truth and correct previous/next availability.

- [ ] **Step 2: Write structural-revision tests**

Loaded attempt, play state, rate, camera, navigation availability, or truth changes structural revision. Ordinary elapsed cursor movement does not force structural labels to rebuild.

- [ ] **Step 3: Prove RED then implement session projection**

- [ ] **Step 4: Remove semantic truth fields from controls**

Delete controls-owned GD PB/Best/Session/platformer truth. Main/coordinator sends truth to session; controls consume `viewState()` only.

- [ ] **Step 5: Split structural/fast refresh**

`refreshStructural(view)` updates attempt/truth/button enabled states/speed/camera/play state only on revision change. `refreshFast(view)` updates timeline cursor/time/progress while Studio is active.

- [ ] **Step 6: GREEN + commit**

```bash
git add src/EchoReplayViewState.hpp src/EchoReplaySession.* src/EchoReplayControls.* src/main.cpp tests/cpp/test_replay_view_state.cpp CMakeLists.txt
git commit -m "refactor: project Replay Studio from session view state"
```

---

### Task 6: Make Replay Studio responsive and truthful on narrow/wide viewports

**Files:**
- Create: `src/EchoReplayLayout.hpp/.cpp`
- Create: `tests/cpp/test_replay_layout.cpp`
- Modify: `src/EchoReplayControls.hpp/.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

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
    bool compact = false;
};

[[nodiscard]] ReplayStudioLayout computeReplayStudioLayout(
    float viewportWidth,
    float viewportHeight
);
```

Layout law:

```text
safe margin = min(12, max(4, viewportWidth/40, viewportHeight/40))
available width = max(0, viewportWidth - 2*margin)
normal panel width = min(900, available width)
compact=true when available width < 520
compact panel width = max(0, available width)
panel height = min(210, max(132, viewportHeight - 2*margin))
```

There is deliberately **no invalid `clamp(min > max)` case**. Compact mode does not remove controls; it reduces spacing/font scale and may wrap utility controls to an additional row inside the same existing Studio panel.

- [ ] **Step 1: Write layout tests**

Test logical sizes `480x320`, `640x480`, `854x480`, `1280x720`, `3440x1440`. Every positive viewport keeps panel inside margins; row ordering is strict; no control anchor lies outside panel bounds. `480x320` must select compact mode.

- [ ] **Step 2: Prove RED and implement pure layout**

No Cocos node creation in the pure function.

- [ ] **Step 3: Group the same controls by hierarchy**

Identity/truth -> timeline -> transport -> utility. Do not add/remove a replay command.

- [ ] **Step 4: Add stable IDs and larger hit targets**

IDs for previous/next attempt, previous/next frame, play/pause, restart, speed, camera, settings, close, slider. Primary hit dimension targets 44 logical pixels in normal layouts and as large as safely possible in compact mode.

- [ ] **Step 5: Drive disabled state from `ReplayStudioViewState`**

Unavailable actions look disabled and callbacks cannot mutate the session. Labels come only from view state.

- [ ] **Step 6: GREEN + commit**

```bash
git add src/EchoReplayLayout.* src/EchoReplayControls.* tests/cpp/test_replay_layout.cpp CMakeLists.txt
git commit -m "fix: harden Replay Studio layout and control states"
```

---

### Task 7: Make scrub/open/close transactional and input-isolated

**Files:**
- Modify: `src/EchoReplaySession.hpp/.cpp`
- Modify: `src/EchoReplayControls.hpp/.cpp`
- Modify: `src/EchoRuntimeCoordinator.hpp/.cpp`
- Modify: `src/main.cpp`
- Modify: `tests/test_v1_1_contract.py`

- [ ] **Step 1: Add scrub ownership**

UI fact `m_userScrubbing` prevents fast refresh from overwriting slider thumb while drag is active. End drag invokes one clamped seek and resumes ordinary cursor projection.

- [ ] **Step 2: Harden seek inputs**

`seekNormalized`: reject NaN/infinity, clamp finite [0,1]. `seekSeconds`: reject nonfinite, clamp [0,duration]. Successful seek synchronizes replay pose/camera continuity before publishing cursor state.

- [ ] **Step 3: Two-phase Studio open**

```text
validate loaded replay
-> coordinator accepts OpenStudio
-> capture viewport
-> construct/show panel + input blocker
-> hide normal fleet presentation
-> report open success
-> PauseLayer may then onResume/remove itself
```

If any required UI construction fails after coordinator acceptance, close/rollback Studio state, restore viewport/fleet, and return false so vanilla PauseLayer remains.

- [ ] **Step 4: Add full-screen input blocker while Studio owns input**

Use a Cocos touch/mouse interception layer behind Studio controls but above gameplay input. It exists only during Studio and is detached idempotently. This is modal plumbing, not a new control/feature.

- [ ] **Step 5: Transactional close**

Stop replay -> remove blocker/panel -> restore captured viewport if node still valid -> coordinator CloseStudio -> resume appropriate Playing fleet tracking. Scene exit invalidates restore state and wins.

- [ ] **Step 6: Contracts**

Exactly one ordinary PauseLayer ECHO entry; no live launcher; Studio cannot coexist with DeathContinuation/Resetting/Exiting; failed Studio initialization cannot remove PauseLayer.

- [ ] **Step 7: GREEN + commit**

```bash
git add src/EchoReplaySession.* src/EchoReplayControls.* src/EchoRuntimeCoordinator.* src/main.cpp tests/test_v1_1_contract.py
git commit -m "fix: make Replay Studio transitions transactional"
```

---

### Task 8: Plan-03 performance/UX evidence gate

**Files:** modify only if verification finds a defect.

- [ ] **Step 1: Structural audit**

Prove no archive maintenance in ordinary live `postUpdate`, no unconditional full overlay rebuild, no `GhostRole` in playback engine, one fleet engine resolution per active slot, no hot-loop dynamic growth, and no Rendering Quality dependency on recorder setters.

- [ ] **Step 2: Full local suite**

```powershell
python -m unittest discover -s tests -p "test_*.py" -v
cmake -S . -B build-core-tests -DECHO_DASH_BUILD_CORE_TESTS=ON
cmake --build build-core-tests --config Release --target EchoDashCoreTests
ctest --test-dir build-core-tests -C Release --output-on-failure
```

Expected zero failures.

- [ ] **Step 3: Full pinned Windows workflow**

Wait for terminal success; build/package success does not certify runtime smoothness.

- [ ] **Step 4: Runtime performance matrix on the exact artifact**

Configured ghost counts `1,8,16,64,128,256`, recorder 120 and 240 Hz where practical, priority trails/death/heat existing presentation. Record incremental ECHO cost and observed visual correctness. Plan 04 will expose richer aggregate diagnostics; external frame logs/video are acceptable at this interim gate.

- [ ] **Step 5: Runtime Replay Studio matrix on the same bytes**

Pause->ECHO, repeated open/close, prev/next attempt edge states, prev/next frame edge states, play/pause/restart, all existing speeds/cameras, scrub, close/viewport restore, narrow and wide viewport if available, and gameplay-input isolation. Any leakage/stuck state is FAIL.

- [ ] **Step 6: Record source SHA, workflow run, package SHA and evidence. Proceed to Plan 04 only with no unresolved source/build/UX defect.**
