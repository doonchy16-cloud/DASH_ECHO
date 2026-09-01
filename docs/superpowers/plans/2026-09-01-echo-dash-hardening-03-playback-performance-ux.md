# ECHO_DASH Hardening 03 — Playback, Performance, and UX Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make current ghost/replay presentation bounded and deterministic, wire the existing Rendering Quality setting truthfully, and harden Replay Studio layout/input/interaction without adding capabilities.

**Architecture:** Extend the one role-agnostic ghost engine so each selected ghost yields one reusable frame-resolution value per frame. Cursor caches stay subordinate to authoritative time. Make derived presentation revision-driven. Add pure Rendering Quality and Replay Studio view/layout models so UI is a responsive projection of session authority. Studio becomes a true modal boundary: pointer input is intercepted and Geometry Dash `UILayer` gameplay input is reset/disabled while Studio owns interaction, then restored deterministically.

**Tech Stack:** C++23, Cocos2d/Geode 5.10.1 presentation APIs, Geometry Dash 2.2081 `UILayer` integration, dependency-free CTest core mode, Python contracts, pinned Windows Release/runtime evidence.

**Spec:** `docs/superpowers/specs/2026-09-01-echo-dash-quality-hardening-design.md`

## Global Constraints

- Run only after Plan 02 is terminal GREEN.
- Exactly one `EchoGhostPlaybackEngine` remains timing authority.
- `GhostRole` affects presentation only; no role-dependent source time/frame/interpolation/completion.
- Configured ceiling remains 256 ghosts; no new LOD/selection feature.
- Rendering Quality affects presentation cost only; never recorder sample rate, replay bytes, attempt outcome, analytics truth, or synchronization authority.
- Last/Best remain blue/gold and visually dominant; no aura returns.
- Replay Studio keeps exactly the existing replay commands, speeds, camera modes, and pause-menu entrypoint.
- No persistent live-gameplay launcher.
- Prepared steady-state ghost update code performs zero ECHO_DASH-owned heap allocation.
- Heavy archive maintenance stays outside latency-sensitive gameplay work.
- Studio may not dismiss PauseLayer unless replay state, UI construction, viewport capture, pointer blocker, and gameplay-input isolation are all established successfully.

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
    std::uint64_t attemptId = 0;
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

Its already-bound immutable `AttemptRecord` remains the frame source.

- [ ] **Step 1: Write interpolation/completion tests first**

Frames at t=0,1,2. Source 0.5 -> indices 0/1, alpha .5, visible true, finished false. Source beyond final+epsilon -> finished true, visible false. Before first frame -> valid clamped/hidden behavior matching current semantics.

- [ ] **Step 2: Write cursor-authority tests**

Resolve forward then force backward source time; cache invalidates/reseeks. Attempt ID change resets cache. Nonfinite source state produces a safe hidden/finished-or-invalid result according to existing engine policy, never stale frame reuse.

- [ ] **Step 3: Write GhostRole metamorphic tests**

Run same replay/live input under test labels Older/Last/Best/Last+Best without passing role to engine. Source time, indices, alpha, visibility, completion identical.

- [ ] **Step 4: Prove RED and implement `resolve`**

Compute authoritative source time from existing Tracking/Continuing semantics, then resolve frame indices. Cursor advances only if same attempt, finite nondecreasing source time, valid bounds; otherwise canonical lower_bound search. Alpha finite/clamped [0,1].

- [ ] **Step 5: Remove duplicate timeline-seek authority from `EchoGhost`**

Retire `seekFrameCursor`, `m_frameIndex`, and `m_lastSynchronizedTime`; `synchronize(resolution)` applies exactly the resolved frames after bounds validation.

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

`renderFromPlaybackEngine` invokes one resolve per active slot; pose/trail consumers reuse `slot.resolution`.

- [ ] **Step 7: GREEN + commit**

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

- [ ] **Step 1: Write structural/behavior contracts first**

Inside steady render body forbid ECHO-owned `new`, `make_unique`, `push_back`, `emplace_back`, `resize`, `reserve`. Assert resolutions == number of active non-null selected slots processed and <= activeGhostCount. Full-quality priority trail segments are bounded to at most 128 total logical segments across at most two priority identities.

- [ ] **Step 2: Prove RED**

- [ ] **Step 3: Preallocate only at attach/rebuild/safe settings boundaries**

`ensurePool` may allocate outside frame loop. Slot cursor/result reset in place.

- [ ] **Step 4: Replace transient trail containers with fixed-capacity/direct bounded iteration**

Use stack/fixed-capacity scratch or direct draw iteration; no unbounded visible trail history.

- [ ] **Step 5: Add numeric frame stats only**

Reset/increment numeric counters; no log/string formatting in hot loop.

- [ ] **Step 6: GREEN + commit**

```bash
git add src/EchoGhostFleet.* src/EchoGhost.* tests/test_v1_1_contract.py
git commit -m "perf: bound ECHO_DASH ghost presentation work"
```

---

### Task 3: Make existing Rendering Quality deterministic and presentation-only

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
    [[nodiscard]] RenderingQuality requested() const;
    [[nodiscard]] EffectiveRenderingQuality effective() const;
    [[nodiscard]] RenderingBudget budget() const;
};
```

Explicit policies:

```text
Full        64 priority-trail segments; Older pose applied every frame
Balanced    40 priority-trail segments; Older pose applied every frame
Performance 24 priority-trail segments; Older pose applied every 2nd frame
```

Auto:

```text
presentation budget = max(0.25 ms, 12% of finite positive frameBudgetMs)
degrade one level after 30 consecutive over-budget observations
recover one level after 180 consecutive observations below 60% of budget
60-observation cooldown after every effective-level change
invalid performance observations are ignored
```

Last/Best/LastAndBest pose remains every frame in all modes. Every Older ghost is still timing-resolved every frame; Performance only changes how often that already-resolved presentation is applied.

- [ ] **Step 1: Write explicit-mode and authority-separation tests**

Each requested mode yields exact budget. Changing Rendering Quality leaves recorder sample-rate snapshot unchanged and never changes playback resolution.

- [ ] **Step 2: Write Auto hysteresis tests**

29 over-budget -> unchanged; 30th -> one-level degradation. Recovery waits through cooldown then requires 180 low observations. Alternating pressure cannot oscillate.

- [ ] **Step 3: Prove RED, implement allocation-free controller, prove GREEN**

- [ ] **Step 4: Measure ECHO presentation only**

Use `steady_clock` around ECHO fleet/overlay presentation work, not vanilla `PlayLayer::postUpdate`. Feed numeric milliseconds + finite positive current frame budget. Diagnostics integration comes in Plan04.

- [ ] **Step 5: Apply rendering budget without changing authority**

Trail cap follows effective budget. Older presentation cadence uses a fixed frame counter; canonical resolve still runs each frame. Priority identities always apply each frame.

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

`postUpdate` must not unconditionally call full death/heat `refresh(...)` each frame.

- [ ] **Step 2: Track analytics + presentation revisions**

Return immediately when both match. Rebuild once when death data or relevant appearance revision changes.

- [ ] **Step 3: Remove duplicate callback+frame rebuild**

Death recording increments analytics revision; presentation consumes it once.

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

void EchoReplaySession::setTruthContext(ReplayTruthContext const& context);
[[nodiscard]] ReplayStudioViewState EchoReplaySession::viewState() const;
[[nodiscard]] std::uint64_t EchoReplaySession::structuralRevision() const;
```

- [ ] **Step 1: Write unloaded/loaded view tests**

Unloaded disables navigation/step. Loaded fixture reports attempt/duration/cursor/progress/rate/camera/truth and previous/next availability.

- [ ] **Step 2: Write structural-revision tests**

Attempt, play state, rate, camera, navigation availability, or truth change structural revision. Ordinary elapsed cursor movement does not require structural-label rebuild.

- [ ] **Step 3: Prove RED then implement projection**

- [ ] **Step 4: Remove semantic truth fields from controls**

Delete controls-owned GD PB/Best/Session/platformer truth. Main/coordinator sends truth to session; controls consume view state only.

- [ ] **Step 5: Split structural and fast refresh**

`refreshStructural(ReplayStudioViewState const&)` updates attempt/truth/enable/speed/camera/play labels only when revision changed. `refreshFast(...)` updates timeline/time/progress while open.

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

[[nodiscard]] ReplayStudioLayout computeReplayStudioLayout(float viewportWidth, float viewportHeight);
```

Layout law:

```text
safe margin = min(12, max(4, viewportWidth/40, viewportHeight/40))
available width = max(0, viewportWidth - 2*margin)
normal panel width = min(900, available width)
compact = available width < 520
compact panel width = available width
panel height = min(210, max(132, viewportHeight - 2*margin)) for sufficiently positive viewport
nonpositive/too-small viewport returns zero-sized invalid layout and Studio open must fail safely
```

No `clamp(min > max)`. Compact mode preserves all existing controls, reducing spacing/font scale and wrapping utility controls within the same panel.

- [ ] **Step 1: Write layout tests**

Test 480x320, 640x480, 854x480, 1280x720, 3440x1440, plus nonpositive/tiny invalid inputs. Positive supported viewport keeps panel in bounds and row order strict; 480x320 compact.

- [ ] **Step 2: Prove RED and implement pure layout**

- [ ] **Step 3: Group same controls by hierarchy**

Identity/truth -> timeline -> transport -> utility. No command added/removed.

- [ ] **Step 4: Add stable IDs and larger hit targets**

Stable IDs for prev/next attempt/frame, play/pause, restart, speed, camera, settings, close, slider. Target ~44 logical-pixel primary hit dimension in normal layout, maximum safe in compact.

- [ ] **Step 5: Drive disabled state from view state**

Unavailable actions visually disabled and callbacks are no-op before session mutation.

- [ ] **Step 6: GREEN + commit**

```bash
git add src/EchoReplayLayout.* src/EchoReplayControls.* tests/cpp/test_replay_layout.cpp CMakeLists.txt
git commit -m "fix: harden Replay Studio layout and control states"
```

---

### Task 7: Make scrub/open/close transactional and fully input-isolated

**Files:**
- Modify: `src/EchoReplaySession.hpp/.cpp`
- Modify: `src/EchoReplayControls.hpp/.cpp`
- Modify: `src/EchoRuntimeCoordinator.hpp/.cpp`
- Modify: `src/main.cpp`
- Modify: `tests/test_v1_1_contract.py`

**Interfaces:**

```cpp
struct ReplayInputIsolationState {
    bool uiLayerDisabledByStudio = false;
    bool pointerBlockerAttached = false;
};
```

`EchoReplayControls` owns the full-screen Cocos pointer/touch blocker node; `main.cpp` owns Geometry Dash `UILayer` gating because it is integration authority.

- [ ] **Step 1: Add scrub ownership and finite seek tests**

`m_userScrubbing` prevents fast refresh from moving slider thumb while drag is active. End drag performs one clamped seek. `seekNormalized` rejects NaN/inf then clamps finite [0,1]; `seekSeconds` rejects nonfinite then clamps [0,duration]. Successful seek synchronizes pose/camera continuity before publishing cursor state.

- [ ] **Step 2: Add explicit gameplay-input isolation helpers in `main.cpp`**

Use the existing Geometry Dash `UILayer` methods:

```text
resetAllButtons()
disableMenu()
enableMenu()
```

Open isolation requires a valid `m_uiLayer`. Before PauseLayer dismissal: `resetAllButtons()` then `disableMenu()`, record `uiLayerDisabledByStudio=true`. This prevents held keyboard/controller gameplay state from surviving into Studio.

- [ ] **Step 3: Add full-screen pointer/touch blocker**

Create a Cocos input interception layer behind Studio controls but above gameplay interaction. It exists only while Studio is open and is idempotently detached. Studio controls remain clickable; clicks/touches outside controls never reach live gameplay.

- [ ] **Step 4: Implement two-phase Studio open**

```text
validate loaded replay + valid layout + valid UILayer
-> coordinator accepts OpenStudio
-> capture viewport
-> construct/show panel and pointer blocker
-> resetAllButtons + disableMenu
-> hide normal fleet
-> report open success
-> only then PauseLayer calls onResume/removes itself
-> immediately after onResume, reacquire PlayLayer UILayer and reassert disableMenu because vanilla resume may alter input state
```

If any required step fails after coordinator acceptance: detach partial UI/blocker, restore viewport/fleet, reset buttons, enable menu only if Studio disabled it, close coordinator Studio state, return false so PauseLayer remains.

- [ ] **Step 5: Keep Studio input isolated every open frame without toggling authority**

Studio branch may verify the captured UILayer pointer is still the active one and menu remains disabled; it must not repeatedly reset held input every frame. If UILayer becomes invalid because scene teardown starts, scene exit wins and close cleanup becomes idempotent.

- [ ] **Step 6: Transactional close and held-input release**

```text
stop replay
-> detach pointer blocker/panel
-> restore captured viewport if node still valid
-> resetAllButtons on still-valid UILayer
-> enableMenu only when uiLayerDisabledByStudio was true and runtime is returning to Playing
-> coordinator CloseStudio
-> resume appropriate fleet tracking
```

On scene exit, do not re-enable gameplay input into a dying layer; clear isolation state and let vanilla teardown dominate.

- [ ] **Step 7: Add contracts/runtime cases**

Exactly one ordinary PauseLayer ECHO entry; no live launcher; Studio cannot coexist with DeathContinuation/DeathAwaitingReset/ResetPending/Resetting/Exiting; failed Studio init cannot remove PauseLayer; keyboard/controller button held before opening is reset and does not leak on close; pointer clicks outside panel do not reach gameplay.

- [ ] **Step 8: GREEN + commit**

```bash
git add src/EchoReplaySession.* src/EchoReplayControls.* src/EchoRuntimeCoordinator.* src/main.cpp tests/test_v1_1_contract.py
git commit -m "fix: make Replay Studio transitions transactional"
```

---

### Task 8: Plan-03 performance/UX evidence gate

**Files:** modify only if verification finds a defect.

- [ ] **Step 1: Structural audit**

No archive maintenance in live postUpdate; no unconditional overlay rebuild; no `GhostRole` in playback engine; one resolve per active slot; no hot-loop dynamic growth; Rendering Quality never touches recorder; Studio uses pointer blocker + UILayer gating.

- [ ] **Step 2: Full local suite**

```powershell
python -m unittest discover -s tests -p "test_*.py" -v
cmake -S . -B build-core-tests -DECHO_DASH_BUILD_CORE_TESTS=ON
cmake --build build-core-tests --config Release --target EchoDashCoreTests
ctest --test-dir build-core-tests -C Release --output-on-failure
```

Expected zero failures.

- [ ] **Step 3: Full pinned Windows hardening-dev workflow**

Wait for terminal success; build/package success does not certify runtime smoothness.

- [ ] **Step 4: Runtime performance matrix on exact artifact**

Ghost counts 1,8,16,64,128,256; recorder 120/240Hz where practical; existing trails/death/heat. Record incremental ECHO cost, frame stats, allocation behavior, visual correctness, requested/effective quality transitions.

- [ ] **Step 5: Runtime Replay Studio matrix on same bytes**

Pause->ECHO, repeated open/close, nav/step edges, play/pause/restart, all existing speeds/cameras, scrub, viewport restore, narrow/wide viewport if practical, held keyboard/controller input, pointer outside panel, and exit during Studio. Leakage/stuck state is FAIL.

- [ ] **Step 6: Record source SHA, workflow run, package SHA, evidence; proceed to Plan 04 only with no unresolved source/build/UX defect.**
