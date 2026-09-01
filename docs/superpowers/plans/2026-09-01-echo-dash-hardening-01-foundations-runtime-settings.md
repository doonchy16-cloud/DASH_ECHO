# ECHO_DASH Hardening 01 — Foundations, Runtime, and Settings Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Preserve the known-good v1.1.2 baseline, move changed hardening builds to truthful v1.1.3 development identity, then establish a Geode-independent native test harness, validated configuration authority, explicit lifecycle state machine, and coordinator boundary before deeper persistence/UX hardening.

**Architecture:** First capture untouched v1.1.2 evidence. Immediately afterward, version every changed hardening build as v1.1.3 and label CI outputs as development artifacts. Then make pure ECHO_DASH policy executable under CTest without Geode, centralize semantic/time/settings policy, split attempt-summary preparation from mutation, and move lifecycle authority out of `main.cpp` boolean combinations into `EchoRuntimeStateMachine` + `EchoRuntimeCoordinator`. Geode hooks remain thin adapters that collect vanilla facts and execute required base calls.

**Tech Stack:** C++23, CMake 3.21+, CTest, dependency-free native test harness, Geode 5.10.1, Geometry Dash Windows 2.2081, Python `unittest`, GitHub Actions `windows-latest`.

**Spec:** `docs/superpowers/specs/2026-09-01-echo-dash-quality-hardening-design.md`

## Global Constraints

- No user-facing feature additions/removals.
- Product name remains `ECHO_DASH`; mod ID remains `doonchy.dash-echo`.
- Repository execution remains `main` only; do not create branches/worktrees. Work in place with small reversible task commits.
- Untouched v1.1.2 evidence must be recorded before source/package version identity changes.
- Once hardening source changes begin, all built packages identify as `v1.1.3`; pre-release CI artifacts are explicitly named `hardening-dev` and include the source SHA.
- Exactly one `EchoGhostPlaybackEngine` remains ghost timing authority; `GhostRole` stays presentation-only.
- Recorder sample-rate range remains 30-240 Hz. Capture-policy changes apply to the next attempt, never silently mid-attempt.
- This plan does not introduce the journal/snapshot store (Plan 02), ghost/Replay Studio redesign (Plan 03), diagnostic authority (Plan 04), or release installer (Plan 05).
- Each behavior task follows RED -> minimal implementation -> GREEN -> regression -> commit.
- Source/build evidence does not imply runtime PASS.

---

## File Structure

**Create:**
- `src/EchoCoreTypes.hpp` — selective semantic value wrappers.
- `src/EchoTimePolicy.hpp/.cpp` — one defensive delta-time policy.
- `src/EchoSettings.hpp/.cpp` — pure raw/normalized settings and typed diffs.
- `src/EchoSettingsGeode.hpp/.cpp` — the only direct Geode setting-read boundary.
- `src/EchoRuntimeState.hpp/.cpp` — pure lifecycle state machine.
- `src/EchoRuntimeCoordinator.hpp/.cpp` — ECHO_DASH lifecycle orchestration authority.
- `tests/cpp/TestHarness.hpp`
- `tests/cpp/test_main.cpp`
- `tests/cpp/test_current_invariants.cpp`
- `tests/cpp/test_core_types.cpp`
- `tests/cpp/test_time_policy.cpp`
- `tests/cpp/test_settings.cpp`
- `tests/cpp/test_runtime_state.cpp`
- `tests/cpp/test_attempt_history.cpp`

**Modify:**
- `CMakeLists.txt`
- `.github/workflows/build-v1.yml`
- `mod.json`
- `src/main.cpp`
- `src/EchoGhostPlaybackEngine.cpp`
- `src/EchoAttemptHistory.hpp/.cpp`
- `tests/test_v1_1_contract.py`
- `tests/test_release_assets.py` only if version-aware assertions require it.

---

### Task 1: Preserve untouched v1.1.2 evidence, then establish truthful v1.1.3 hardening-development identity

**Files:**
- Modify after baseline evidence: `mod.json`
- Modify after baseline evidence: `CMakeLists.txt`
- Modify after baseline evidence: `src/main.cpp`
- Modify after baseline evidence: `.github/workflows/build-v1.yml`
- Modify after baseline evidence: `tests/test_v1_1_contract.py`

**Interfaces:**
- Stable rollback baseline remains the previously certified v1.1.2 package/source evidence.
- All subsequent hardening source uses visible/runtime version `v1.1.3`.
- Development CI artifact names use:

```text
ECHO-DASH-v1.1.3-hardening-dev-${GITHUB_SHA}-compiler-evidence
ECHO-DASH-v1.1.3-hardening-dev-${GITHUB_SHA}-windows
```

No development artifact is called a release candidate before Plan 05.

- [ ] **Step 1: Capture fresh untouched baseline source tests before any version edit**

```powershell
python -m unittest tests.test_v1_1_contract tests.test_release_assets -v
```

Expected: current v1.1.2 contracts terminal GREEN. Record source SHA and the terminal Windows workflow run for that exact source. If the baseline itself fails, stop this plan and repair/investigate the baseline rather than mixing that defect into hardening.

- [ ] **Step 2: Change version contract first and prove RED**

Update version assertions to require:

```python
self.assertEqual(metadata["version"], "v1.1.3")
self.assertIn("VERSION 1.1.3", self.read("CMakeLists.txt"))
self.assertRegex(main, r'kReleaseVersion\s*=\s*"v1\.1\.3"')
```

Also require workflow development artifact names to contain `v1.1.3-hardening-dev` and `${{ github.sha }}`. Run the Python suite; expected FAIL while production surfaces still report v1.1.2.

- [ ] **Step 3: Update all changed-build identity surfaces together**

Set:

```text
mod.json version = v1.1.3
CMake project VERSION = 1.1.3
src/main.cpp kReleaseVersion = v1.1.3
workflow display name = ECHO_DASH v1.1.3 Hardening Development Build
workflow dev artifact names = v1.1.3-hardening-dev-${{ github.sha }}-...
```

Do not change mod ID, Geode target, GD target, icon, or product name.

- [ ] **Step 4: Prove GREEN and commit**

```powershell
python -m unittest tests.test_v1_1_contract tests.test_release_assets -v
```

```bash
git add mod.json CMakeLists.txt src/main.cpp .github/workflows/build-v1.yml tests/test_v1_1_contract.py tests/test_release_assets.py
git commit -m "chore: start ECHO_DASH v1.1.3 hardening development"
```

---

### Task 2: Add a genuinely Geode-independent native core test mode

**Files:**
- Create: `tests/cpp/TestHarness.hpp`
- Create: `tests/cpp/test_main.cpp`
- Create: `tests/cpp/test_current_invariants.cpp`
- Modify: `CMakeLists.txt`
- Modify: `.github/workflows/build-v1.yml`

**Interface:** `ECHO_DASH_BUILD_CORE_TESTS=ON` means core-tests-only. Root CMake returns before any Geode SDK lookup/add_subdirectory. Normal Geode builds leave it OFF and remain unchanged.

- [ ] **Step 1: Create the dependency-free harness**

`tests/cpp/TestHarness.hpp`:

```cpp
#pragma once
#include <stdexcept>
#include <string>
#include <vector>

namespace echo_test {
using TestFunction = void (*)();
struct TestCase { std::string name; TestFunction function = nullptr; };
inline std::vector<TestCase>& registry() { static std::vector<TestCase> value; return value; }
struct Register {
    Register(char const* name, TestFunction fn) { registry().push_back({name, fn}); }
};
inline void check(bool ok, char const* expr, char const* file, int line) {
    if (!ok) throw std::runtime_error(
        std::string(file) + ":" + std::to_string(line) + " CHECK failed: " + expr
    );
}
}
#define ECHO_TEST(name) static void name(); static echo_test::Register name##_reg(#name, &name); static void name()
#define ECHO_CHECK(expr) echo_test::check(static_cast<bool>(expr), #expr, __FILE__, __LINE__)
```

`test_main.cpp` iterates the registry, prints `PASS/FAIL`, catches `std::exception`, and returns nonzero when any test fails.

`test_current_invariants.cpp` initially pins:

```cpp
#include "TestHarness.hpp"
#include "EchoRecorder.hpp"
ECHO_TEST(current_recorder_limits_are_preserved) {
    ECHO_CHECK(dash_echo::EchoRecorder::kMinCaptureSampleRate == 30.0);
    ECHO_CHECK(dash_echo::EchoRecorder::kDefaultCaptureSampleRate == 120.0);
    ECHO_CHECK(dash_echo::EchoRecorder::kMaxCaptureSampleRate == 240.0);
}
```

- [ ] **Step 2: Prove target is absent before CMake change**

```powershell
cmake -S . -B build-core-tests -DECHO_DASH_BUILD_CORE_TESTS=ON
```

Expected before implementation: configure fails or does not create `EchoDashCoreTests`.

- [ ] **Step 3: Add core-only CMake path before Geode lookup**

```cmake
option(ECHO_DASH_BUILD_CORE_TESTS "Build dependency-free ECHO_DASH core tests only" OFF)

if (ECHO_DASH_BUILD_CORE_TESTS)
    project(EchoDashCoreTests LANGUAGES CXX)
    enable_testing()
    add_executable(EchoDashCoreTests
        tests/cpp/test_main.cpp
        tests/cpp/test_current_invariants.cpp
    )
    target_include_directories(EchoDashCoreTests PRIVATE src tests/cpp)
    target_compile_features(EchoDashCoreTests PRIVATE cxx_std_23)
    add_test(NAME EchoDashCoreTests COMMAND EchoDashCoreTests)
    return()
endif()
```

The normal project statement after this path remains `project(EchoDash VERSION 1.1.3)`.

- [ ] **Step 4: Prove GREEN with `GEODE_SDK` unset**

```powershell
Remove-Item Env:GEODE_SDK -ErrorAction SilentlyContinue
cmake -S . -B build-core-tests -DECHO_DASH_BUILD_CORE_TESTS=ON
cmake --build build-core-tests --config Release --target EchoDashCoreTests
ctest --test-dir build-core-tests -C Release --output-on-failure
```

Expected: 1/1 PASS, no Geode SDK lookup.

- [ ] **Step 5: Put native tests before SDK installation in CI**

Add a workflow step that runs the same configure/build/CTest commands before pinned Geode SDK installation.

- [ ] **Step 6: Run Python regressions and commit**

```powershell
python -m unittest discover -s tests -p "test_*.py" -v
```

```bash
git add CMakeLists.txt .github/workflows/build-v1.yml tests/cpp
git commit -m "test: add native ECHO_DASH core harness"
```

---

### Task 3: Add selective semantic types and one delta-time policy

**Files:**
- Create: `src/EchoCoreTypes.hpp`
- Create: `src/EchoTimePolicy.hpp/.cpp`
- Create: `tests/cpp/test_core_types.cpp`
- Create: `tests/cpp/test_time_policy.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/main.cpp`
- Modify: `src/EchoGhostPlaybackEngine.cpp`

**Interfaces:**

```cpp
struct AttemptId { std::uint64_t value = 0; bool operator==(AttemptId const&) const = default; };
struct FrameSequence { std::uint64_t value = 0; bool operator==(FrameSequence const&) const = default; };
struct ReplayTime { double seconds = 0.0; bool operator==(ReplayTime const&) const = default; };
struct ProgressPercent { float value = 0.0f; bool operator==(ProgressPercent const&) const = default; };
struct NormalizedCursor { float value = 0.0f; bool operator==(NormalizedCursor const&) const = default; };
[[nodiscard]] double sanitizeDeltaSeconds(double dt, double maximum = 0.25);
```

- [ ] **Step 1: Add failing type/time tests**

Assert `AttemptId` is not implicitly convertible to `FrameSequence`; finite positive values pass; negative/NaN/infinity become zero; 1.0 seconds clamps to 0.25; invalid/nonpositive maximum yields zero.

- [ ] **Step 2: Add new pure sources/tests to core target and prove RED**

- [ ] **Step 3: Implement wrappers and `sanitizeDeltaSeconds`**

Use `std::isfinite` and bounded comparison; avoid duplicated policy.

- [ ] **Step 4: Replace duplicated defensive dt sanitation in `main.cpp` and `EchoGhostPlaybackEngine.cpp`**

Do not change semantic clocks beyond using the shared defensive rule.

- [ ] **Step 5: Prove GREEN and commit**

```bash
git add src/EchoCoreTypes.hpp src/EchoTimePolicy.* src/main.cpp src/EchoGhostPlaybackEngine.cpp tests/cpp CMakeLists.txt
git commit -m "refactor: centralize ECHO_DASH core time policy"
```

---

### Task 4: Build one immutable validated settings authority

**Files:**
- Create: `src/EchoSettings.hpp/.cpp`
- Create: `src/EchoSettingsGeode.hpp/.cpp`
- Create: `tests/cpp/test_settings.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/main.cpp`

**Interfaces:**

```cpp
enum class VisualProfile : std::uint8_t { Clean, Competitive, Multiverse, Chaos, Custom };
enum class RenderingQuality : std::uint8_t { Auto, Full, Balanced, Performance };
enum class ReplayCameraSetting : std::uint8_t { Recorded, Follow, Smooth, Drone, DynamicZoom, DeathCam };

struct RawEchoSettings {
    std::int64_t ghostCount = 16;
    std::string visualProfile = "Multiverse";
    std::int64_t olderOpacityMin = 36;
    std::int64_t olderOpacityMax = 104;
    double ageFadeStrength = 1.0;
    bool priorityXray = true;
    bool lastEnabled = true;
    ColorRGB lastColor {74, 163, 255};
    std::int64_t lastOpacity = 190;
    bool lastTrail = true;
    bool bestEnabled = true;
    ColorRGB bestColor {255, 213, 74};
    std::int64_t bestOpacity = 220;
    bool bestTrail = true;
    double trailSeconds = 0.55;
    double trailWidth = 1.8;
    std::int64_t trailOpacity = 170;
    bool deathMarkers = true;
    double deathMarkerScale = 1.0;
    bool deathLabels = true;
    bool deathXray = true;
    bool heatStrip = true;
    std::int64_t heatStripOpacity = 170;
    std::string defaultPlaybackRate = "1.00";
    std::string defaultCameraMode = "Recorded";
    std::int64_t recorderSampleRate = 120;
    std::int64_t replayRetention = 10'000;
    std::int64_t diskBudgetMb = 2'048;
    std::string renderingQuality = "Auto";
    bool diagnostics = false;
};

struct GhostSettingsSnapshot {
    std::size_t requestedCount = 16;
    std::size_t effectiveCount = 16;
    VisualProfile profile = VisualProfile::Multiverse;
    std::uint8_t olderOpacityMin = 36;
    std::uint8_t olderOpacityMax = 104;
    float ageFadeStrength = 1.0f;
    bool priorityXray = true;
    bool lastEnabled = true;
    ColorRGB lastColor {74, 163, 255};
    std::uint8_t lastOpacity = 190;
    bool lastTrail = true;
    bool bestEnabled = true;
    ColorRGB bestColor {255, 213, 74};
    std::uint8_t bestOpacity = 220;
    bool bestTrail = true;
    float trailSeconds = 0.55f;
    float trailWidth = 1.8f;
    std::uint8_t trailOpacity = 170;
    bool operator==(GhostSettingsSnapshot const&) const = default;
};

struct DeathSettingsSnapshot {
    bool markers = true;
    float markerScale = 1.0f;
    bool labels = true;
    bool xray = true;
    bool heatStrip = true;
    std::uint8_t heatStripOpacity = 170;
    bool operator==(DeathSettingsSnapshot const&) const = default;
};

struct ReplaySettingsSnapshot {
    float defaultPlaybackRate = 1.0f;
    ReplayCameraSetting defaultCamera = ReplayCameraSetting::Recorded;
    bool operator==(ReplaySettingsSnapshot const&) const = default;
};

struct StorageSettingsSnapshot {
    std::size_t replayRetention = 10'000;
    std::size_t diskBudgetMb = 2'048;
    bool operator==(StorageSettingsSnapshot const&) const = default;
};

struct EchoSettingsSnapshot {
    GhostSettingsSnapshot ghosts;
    DeathSettingsSnapshot deaths;
    ReplaySettingsSnapshot replay;
    StorageSettingsSnapshot storage;
    double recorderSampleRateHz = 120.0;
    RenderingQuality renderingQuality = RenderingQuality::Auto;
    bool diagnostics = false;
    bool operator==(EchoSettingsSnapshot const&) const = default;
};

struct EchoSettingsDiff {
    bool presentation = false;
    bool fleetStructure = false;
    bool recorderPolicy = false;
    bool persistencePolicy = false;
    bool replayDefaults = false;
    bool diagnostics = false;
};

[[nodiscard]] EchoSettingsSnapshot normalizeEchoSettings(RawEchoSettings const& raw);
[[nodiscard]] EchoSettingsDiff diffEchoSettings(EchoSettingsSnapshot const& oldValue, EchoSettingsSnapshot const& newValue);
[[nodiscard]] RawEchoSettings readGeodeEchoSettings();
```

`EchoSettingsGeode.cpp` converts Geode `ccColor3B` values to `ColorRGB` and is the only file that directly reads `Mod::get()->getSettingValue`.

- [ ] **Step 1: Write normalization/property tests first**

Assert: requested ghost 999 -> 256; Competitive effective count -> min(requested,8); opacity -10/900 -> 0/255; NaN age fade -> 1.0; sample rate 999 -> 240; retention 1 -> 256; disk 99999 -> 8192; unknown profile/rate/camera/quality -> current documented default; Performance decodes exactly.

- [ ] **Step 2: Write diff-classification tests**

Sample rate only -> `recorderPolicy`; ghost count/profile -> `fleetStructure`; colors/trails/death/quality -> `presentation`; retention/disk -> `persistencePolicy`; replay speed/camera -> `replayDefaults`; diagnostics toggle -> `diagnostics`.

- [ ] **Step 3: Prove RED, implement pure normalization/diff, prove GREEN**

Non-finite floating settings resolve to existing defaults before range clamp. Profile caps remain Clean=2, Competitive=8, Multiverse=64, Chaos/Custom=256.

- [ ] **Step 4: Implement Geode adapter and remove direct setting reads from `main.cpp`**

The adapter reads exact existing `mod.json` keys; it does not normalize. Main stores requested/applied snapshots + revision and compares snapshots instead of building a 33-field formatted string.

- [ ] **Step 5: Prove recorder-policy boundary**

A sample-rate change during an active attempt updates requested settings but does not call `recorder.setCaptureSampleRate`. The next attempt applies the requested sample rate immediately before `beginAttempt()`.

- [ ] **Step 6: Source audit and commit**

```powershell
Select-String -Path src\*.cpp -Pattern 'getSettingValue<'
```

Expected: production setting reads only in `EchoSettingsGeode.cpp`.

```bash
git add src/EchoSettings* src/main.cpp tests/cpp/test_settings.cpp CMakeLists.txt
git commit -m "refactor: centralize validated ECHO_DASH settings"
```

---

### Task 5: Introduce the pure runtime state machine, including death-without-continuation ordering

**Files:**
- Create: `src/EchoRuntimeState.hpp/.cpp`
- Create: `tests/cpp/test_runtime_state.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

```cpp
enum class RuntimeState : std::uint8_t {
    Initializing,
    Playing,
    ReplayStudio,
    DeathContinuation,
    DeathAwaitingReset,
    ResetPending,
    Resetting,
    Completing,
    Exiting
};

enum class RuntimeEvent : std::uint8_t {
    InitializationComplete,
    OpenStudio,
    CloseStudio,
    ConfirmDeathWithContinuation,
    ConfirmDeathWithoutContinuation,
    RequestReset,
    ContinuationComplete,
    ResetExecuted,
    CompleteLevel,
    ExitLevel
};

struct TransitionResult {
    RuntimeState from = RuntimeState::Initializing;
    RuntimeState to = RuntimeState::Initializing;
    bool accepted = false;
};

class EchoRuntimeStateMachine final {
public:
    [[nodiscard]] TransitionResult apply(RuntimeEvent event);
    [[nodiscard]] RuntimeState state() const;
};
```

Binding transition table:

```text
Initializing + InitializationComplete -> Playing
Playing + OpenStudio -> ReplayStudio
ReplayStudio + CloseStudio -> Playing
Playing + ConfirmDeathWithContinuation -> DeathContinuation
Playing + ConfirmDeathWithoutContinuation -> DeathAwaitingReset
DeathContinuation + ContinuationComplete -> DeathAwaitingReset
DeathContinuation + RequestReset -> ResetPending
DeathAwaitingReset + RequestReset -> Resetting
ResetPending + ContinuationComplete -> Resetting
Playing + RequestReset -> Resetting
Resetting + ResetExecuted -> Playing
Playing + CompleteLevel -> Completing
Completing + ExitLevel -> Exiting
any non-Exiting live state + ExitLevel -> Exiting
Exiting + any event -> reject
```

- [ ] **Step 1: Write legal transition tests including no-ghost and completion-before-reset cases**

Explicitly test death with no selected ghosts, and a continuation that completes before vanilla `resetLevel()` is requested. Both must reach `DeathAwaitingReset`, then `RequestReset -> Resetting` without deadlock.

- [ ] **Step 2: Write rejection-without-mutation tests**

Reject ReplayStudio+ConfirmDeath, DeathContinuation+OpenStudio, DeathAwaitingReset+OpenStudio, Exiting+OpenStudio, Completing+InitializationComplete. State remains unchanged.

- [ ] **Step 3: Prove RED, implement explicit switch/table, prove GREEN**

- [ ] **Step 4: Commit**

```bash
git add src/EchoRuntimeState.* tests/cpp/test_runtime_state.cpp CMakeLists.txt
git commit -m "refactor: add explicit ECHO_DASH runtime state machine"
```

---

### Task 6: Split attempt-history preparation from mutation

**Files:**
- Modify: `src/EchoAttemptHistory.hpp/.cpp`
- Create: `tests/cpp/test_attempt_history.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

```cpp
[[nodiscard]] std::optional<AttemptHistoryEntry> prepareFinalizedAttempt(
    AttemptRecord const& attempt,
    DeathEvent const* death,
    float priorBest,
    std::uint64_t bestAttemptIdAfter
) const;
[[nodiscard]] bool commitPreparedEntry(AttemptHistoryEntry const& entry);
```

Keep `commitFinalizedAttempt(...)` temporarily as a compatibility wrapper until coordinator migration is complete.

- [ ] **Step 1: Write failing non-mutation/idempotence tests**

Prepare finalized attempt #7 -> history remains empty. Commit once -> one entry. Commit duplicate -> false, still one logical entry. Preparation of active/zero-ID/nonfinite invalid input returns nullopt without mutation.

- [ ] **Step 2: Add history source to core test target and prove RED**

- [ ] **Step 3: Move construction/validation into preparation and all mutation into commit**

Duplicate-ID rejection may increment the existing duplicate-rejection statistic but may not alter outcome/PB counters or entries.

- [ ] **Step 4: Prove GREEN and commit**

```bash
git add src/EchoAttemptHistory.* tests/cpp/test_attempt_history.cpp CMakeLists.txt
git commit -m "refactor: separate attempt history preparation from commit"
```

---

### Task 7: Extract `EchoRuntimeCoordinator` and remove lifecycle boolean authority

**Files:**
- Create: `src/EchoRuntimeCoordinator.hpp/.cpp`
- Modify: `src/main.cpp`
- Modify: `tests/cpp/test_runtime_state.cpp`
- Modify: `tests/test_v1_1_contract.py`

**Interfaces:**

```cpp
struct LiveFrameContext {
    float progressPercent = 0.0f;
    bool progressAuthority = false;
    PlayerObject* player1 = nullptr;
    PlayerObject* player2 = nullptr;
    cocos2d::CCNode* viewportLayer = nullptr;
};

enum class DeathConfirmationResult : std::uint8_t {
    Rejected,
    AwaitingReset,
    Continuing
};

enum class ResetRequestResult : std::uint8_t {
    Deferred,
    ExecuteNow,
    Rejected
};

enum class ContinuationUpdate : std::uint8_t {
    NoChange,
    AwaitingReset,
    ExecuteReset,
    Invalid
};

enum class AttemptCommitStatus : std::uint8_t {
    NoActiveAttempt,
    Committed,
    PendingDurability,
    Rejected
};

class EchoRuntimeCoordinator final {
public:
    EchoRuntimeCoordinator(
        EchoRecorder&, EchoReplayArchive&, EchoAttemptHistory&,
        EchoDeathAnalytics&, EchoGhostFleet&, EchoReplaySession&
    );
    void initializationComplete();
    [[nodiscard]] RuntimeState state() const;
    [[nodiscard]] bool startAttempt(LiveFrameContext const&, double sampleRateHz);
    void captureFrame(double dt, LiveFrameContext const&);
    [[nodiscard]] DeathConfirmationResult confirmDeath(DeathEvent const&, LiveFrameContext const&);
    [[nodiscard]] ResetRequestResult requestReset();
    [[nodiscard]] ContinuationUpdate advanceContinuation(double dt);
    [[nodiscard]] AttemptCommitStatus finalize(AttemptEndReason reason);
    [[nodiscard]] bool openReplayStudio();
    [[nodiscard]] bool closeReplayStudio();
    void beginCompleting();
    void beginExit();
    void resetExecuted();
};
```

- [ ] **Step 1: Add failing architecture contracts**

Require coordinator files and state machine ownership. After migration, forbid direct `recorder.finalizeAttempt(` and `history.commitFinalizedAttempt(` lifecycle policy in `main.cpp`.

- [ ] **Step 2: Move start/capture policy**

Start only from Playing with no active attempt; apply sample rate before `beginAttempt`; capture initial event frame. Capture only in Playing and update recorder + fleet tracking from the same `LiveFrameContext`.

- [ ] **Step 3: Move terminal-death policy and distinguish no-continuation explicitly**

`main.cpp::destroyPlayer` captures candidate facts, calls vanilla `PlayLayer::destroyPlayer`, then dispatches only a confirmed terminal observation. Coordinator records the event frame/death once. If `fleet.beginContinuation(...)` returns true, transition via `ConfirmDeathWithContinuation` and return `Continuing`; otherwise transition via `ConfirmDeathWithoutContinuation` and return `AwaitingReset`. Duplicate death outside Playing rejects.

- [ ] **Step 4: Move reset/continuation policy**

`requestReset()`:

```text
Playing -> Resetting / ExecuteNow
DeathContinuation -> ResetPending / Deferred
DeathAwaitingReset -> Resetting / ExecuteNow
ResetPending -> Deferred (same pending reset)
Exiting/Completing/ReplayStudio illegal path -> Rejected unless caller first closes Studio by legal lifecycle policy
```

`advanceContinuation()`:

```text
DeathContinuation + completion before reset request -> DeathAwaitingReset / AwaitingReset
ResetPending + completion -> Resetting / ExecuteReset
valid unfinished continuation -> NoChange
invalid continuation -> Invalid (Plan 04 adds liveness containment; Plan 01 must not invent a timer)
```

Add native tests for zero ghosts and completion-before-reset.

- [ ] **Step 5: Move finalization orchestration with a temporary Plan-01 persistence adapter**

Until Plan 02 replaces storage:

```text
recorder.finalizeAttempt
-> resolve finalized attempt
-> prepare history entry without mutation
-> archive.ingest(summary+compressed replay) as the logical archive acceptance boundary
-> if logical archive acceptance succeeds, commitPreparedEntry into in-memory history projection
-> archive.save() for current temporary durability path
-> publish session-best/replay/fleet consequences
```

If `archive.save()` fails after logical acceptance, return `PendingDurability`; never claim disk durability. If the derived in-memory history projection rejects after archive logical acceptance, do **not** roll back/delete accepted archive data; rebuild/restore that projection from the archive summary at the next safe reconciliation point and record a warning. Plan 02 replaces this temporary adapter with journal durability.

- [ ] **Step 6: Remove behavioral boolean authority from `main.cpp`**

Remove `captureEnabled`, `confirmedDeath`, `deferredResetRequested`, and `replayStudioOpen` as lifecycle truth. Orthogonal facts such as archive readiness, fleet rebuild request, diagnostics display, and viewport-restore validity may remain.

- [ ] **Step 7: Route Studio state through coordinator**

Controls request coordinator open/close; `main.cpp` retains Cocos viewport/node plumbing. Failed coordinator open returns false and may not dismiss PauseLayer.

- [ ] **Step 8: Replace old regex tests with behavior/architecture contracts**

Remove tests that require old boolean names. Keep one role-agnostic playback engine, pause-menu-only Studio entry, no live launcher, state-machine/coordinator existence, and no-ghost death ordering contracts.

- [ ] **Step 9: Prove GREEN, run terminal Windows dev workflow, and commit**

```bash
git add src/EchoRuntimeCoordinator.* src/EchoRuntimeState.* src/main.cpp tests/cpp/test_runtime_state.cpp tests/test_v1_1_contract.py
git commit -m "refactor: centralize ECHO_DASH runtime lifecycle"
```

Wait for the pinned Windows workflow to finish. Build success is required; runtime PASS is not claimed here.

---

### Task 8: Enforce safe settings application boundaries and wire every setting

**Files:**
- Modify: `src/EchoRuntimeCoordinator.hpp/.cpp`
- Modify: `src/main.cpp`
- Modify: `mod.json`
- Modify: `tests/cpp/test_settings.cpp`
- Modify: `tests/test_v1_1_contract.py`

**Interfaces:** `EchoSettingsDiff` controls consequence timing.

- [ ] **Step 1: Add hostile-change/property tests**

Presentation changes never set recorder/persistence flags. Sample rate only sets recorder policy. Rendering Quality is represented as presentation policy and never changes capture settings.

- [ ] **Step 2: Prove the existing Rendering Quality hole RED**

Require `rendering-quality` to be read by `EchoSettingsGeode.cpp` and represented in `EchoSettingsSnapshot`. It fails before wiring.

- [ ] **Step 3: Apply categories at legal boundaries**

```text
presentation -> relevant renderer immediately when attached
fleet structure -> mark rebuild; execute only in a legal safe state
recorder policy -> next startAttempt only
persistence policy -> configure policy only; no heavy save from callback
replay defaults -> session defaults
diagnostics -> display/observer state only
rendering quality -> requested presentation policy; Plan 03 implements its runtime cost policy
```

- [ ] **Step 4: Improve wording without changing storage keys/scales**

`Recorder Sample Rate` description states “Changes apply to the next attempt.” Display label `Replay Archive Run Limit` becomes `Saved Replay Limit`; JSON key remains `replay-retention`.

- [ ] **Step 5: Run full local suite + terminal Windows build and commit**

```bash
git add src/EchoRuntimeCoordinator.* src/main.cpp mod.json tests
git commit -m "fix: make ECHO_DASH settings boundaries explicit"
```

---

### Task 9: Plan-01 evidence gate

**Files:** modify only if verification finds a defect.

- [ ] **Step 1: Fresh local suite**

```powershell
python -m unittest discover -s tests -p "test_*.py" -v
cmake -S . -B build-core-tests -DECHO_DASH_BUILD_CORE_TESTS=ON
cmake --build build-core-tests --config Release --target EchoDashCoreTests
ctest --test-dir build-core-tests -C Release --output-on-failure
```

Expected: zero failures and no Geode SDK lookup for core-only configure.

- [ ] **Step 2: Source audit**

Require direct `getSettingValue` reads only in `EchoSettingsGeode.cpp`; old lifecycle booleans gone; one fleet playback-engine member; `EchoGhostPlaybackEngine.*` contains no `GhostRole`; changed hardening surfaces all report v1.1.3.

- [ ] **Step 3: Trigger full pinned Windows hardening-dev workflow and wait for terminal completion**

Expected terminal GREEN for Python contracts, native CTest, pinned CLI/SDK setup, MSVC Geode Release build, compiler evidence, and package collection/upload. Artifact names must remain development-labeled.

- [ ] **Step 4: Review diff against approved scope**

No Plan-02 persistence format, Plan-03 UX redesign, or new feature scope may have leaked into foundations.

- [ ] **Step 5: Record exact source SHA/workflow run ID and proceed to Plan 02 only with terminal evidence.**
