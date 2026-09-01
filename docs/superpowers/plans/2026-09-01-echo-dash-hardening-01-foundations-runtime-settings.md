# ECHO_DASH Hardening 01 — Foundations, Runtime, and Settings Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Establish a Geode-independent native test harness, validated configuration authority, explicit runtime state machine, and coordinator boundary before deeper persistence/UX hardening.

**Architecture:** First make pure ECHO_DASH policy code executable under CTest without requiring the Geode SDK. Then centralize semantic/time/settings policy, split attempt-summary preparation from mutation, and move lifecycle authority out of `main.cpp` boolean combinations into `EchoRuntimeStateMachine` + `EchoRuntimeCoordinator`. Geode hooks remain thin adapters that collect vanilla facts and execute required base calls.

**Tech Stack:** C++23, CMake 3.21+, CTest, dependency-free native test harness, Geode 5.10.1, Geometry Dash Windows 2.2081, Python `unittest`, GitHub Actions `windows-latest`.

**Spec:** `docs/superpowers/specs/2026-09-01-echo-dash-quality-hardening-design.md`

## Global Constraints

- No user-facing feature additions/removals.
- Product name remains `ECHO_DASH`; mod ID remains `doonchy.dash-echo`.
- Repository execution remains `main` only; do not create branches/worktrees.
- Exactly one `EchoGhostPlaybackEngine` remains ghost timing authority; `GhostRole` stays presentation-only.
- Recorder sample-rate range remains 30-240 Hz. Capture-policy changes apply to the next attempt, never silently mid-attempt.
- This plan does not introduce the journal/snapshot store (Plan 02), ghost/Replay Studio redesign (Plan 03), diagnostic authority (Plan 04), or release installer (Plan 05).
- Each behavior task follows RED -> minimal implementation -> GREEN -> regression -> commit.
- Source/build evidence does not imply runtime PASS.

---

## File Structure

**Create:**
- `src/EchoCoreTypes.hpp`
- `src/EchoTimePolicy.hpp/.cpp`
- `src/EchoSettings.hpp/.cpp`
- `src/EchoSettingsGeode.hpp/.cpp`
- `src/EchoRuntimeState.hpp/.cpp`
- `src/EchoRuntimeCoordinator.hpp/.cpp`
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
- `src/EchoAttemptHistory.hpp/.cpp`
- `src/main.cpp`
- `mod.json`
- `tests/test_v1_1_contract.py`

---

### Task 1: Add a genuinely Geode-independent native core test mode

**Files:**
- Create: `tests/cpp/TestHarness.hpp`
- Create: `tests/cpp/test_main.cpp`
- Create: `tests/cpp/test_current_invariants.cpp`
- Modify: `CMakeLists.txt`
- Modify: `.github/workflows/build-v1.yml`

**Interface:** CMake option `ECHO_DASH_BUILD_CORE_TESTS=ON` means **core-tests-only** and returns from the root CMake file before any Geode SDK check/add_subdirectory. Normal Geode builds leave it OFF and remain unchanged.

- [ ] **Step 1: Create the tiny dependency-free harness**

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

`tests/cpp/test_main.cpp` iterates the registry, prints `PASS/FAIL`, catches `std::exception`, and exits nonzero when any test fails.

`tests/cpp/test_current_invariants.cpp` initially pins:

```cpp
#include "TestHarness.hpp"
#include "EchoRecorder.hpp"
ECHO_TEST(current_recorder_limits_are_preserved) {
    ECHO_CHECK(dash_echo::EchoRecorder::kMinCaptureSampleRate == 30.0);
    ECHO_CHECK(dash_echo::EchoRecorder::kDefaultCaptureSampleRate == 120.0);
    ECHO_CHECK(dash_echo::EchoRecorder::kMaxCaptureSampleRate == 240.0);
}
```

- [ ] **Step 2: Prove the target is absent before the CMake change**

```powershell
cmake -S . -B build-core-tests -DECHO_DASH_BUILD_CORE_TESTS=ON
```

Expected before implementation: configure fails or does not create `EchoDashCoreTests` because the current root project has no core-only path.

- [ ] **Step 3: Restructure the root CMake entry without changing normal Geode behavior**

Immediately after `cmake_minimum_required`/C++ standard setup:

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

project(EchoDash VERSION 1.1.2)
```

Only after that `return()` does normal source glob/library creation, `GEODE_SDK` validation, Geode subdirectory, and `setup_geode_mod` execute.

- [ ] **Step 4: Prove GREEN without `GEODE_SDK`**

Start a shell/session where `GEODE_SDK` is unset and run:

```powershell
cmake -S . -B build-core-tests -DECHO_DASH_BUILD_CORE_TESTS=ON
cmake --build build-core-tests --config Release --target EchoDashCoreTests
ctest --test-dir build-core-tests -C Release --output-on-failure
```

Expected: one native test executable, 1/1 PASS, no Geode SDK lookup.

- [ ] **Step 5: Add the same native-test stage before SDK installation in CI**

```yaml
      - name: Run ECHO_DASH native core tests
        shell: pwsh
        run: |
          $ErrorActionPreference = 'Stop'
          cmake -S . -B build-core-tests -DECHO_DASH_BUILD_CORE_TESTS=ON
          cmake --build build-core-tests --config Release --target EchoDashCoreTests
          ctest --test-dir build-core-tests -C Release --output-on-failure
```

- [ ] **Step 6: Run existing Python contracts**

```powershell
python -m unittest tests.test_v1_1_contract tests.test_release_assets -v
```

Expected: all current tests PASS.

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt .github/workflows/build-v1.yml tests/cpp
git commit -m "test: add native ECHO_DASH core harness"
```

---

### Task 2: Add selective semantic types and one delta-time policy

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

Assert `AttemptId` is not implicitly convertible to `FrameSequence`; finite positive values pass; negative/NaN/infinity become zero; a 1-second spike clamps to 0.25.

- [ ] **Step 2: Add the source files to the core-only target and prove RED**

Expected compile failure before the interfaces exist.

- [ ] **Step 3: Implement wrappers and `sanitizeDeltaSeconds`**

Use `std::isfinite` and `std::clamp`; invalid/nonpositive maximum returns zero.

- [ ] **Step 4: Replace duplicated defensive `dt` sanitation in `main.cpp` and `EchoGhostPlaybackEngine.cpp`**

Do not alter semantic clocks beyond using one defensive policy.

- [ ] **Step 5: Prove GREEN and commit**

```bash
git add src/EchoCoreTypes.hpp src/EchoTimePolicy.* src/main.cpp src/EchoGhostPlaybackEngine.cpp tests/cpp CMakeLists.txt
git commit -m "refactor: centralize ECHO_DASH core time policy"
```

---

### Task 3: Build one immutable validated settings authority

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

struct EchoSettingsDiff {
    bool presentation = false;
    bool fleetStructure = false;
    bool recorderPolicy = false;
    bool persistencePolicy = false;
    bool replayDefaults = false;
    bool diagnostics = false;
};

struct RawEchoSettings;       // one field for every non-title mod.json setting
struct EchoSettingsSnapshot; // normalized effective values grouped by subsystem

[[nodiscard]] EchoSettingsSnapshot normalizeEchoSettings(RawEchoSettings const& raw);
[[nodiscard]] EchoSettingsDiff diffEchoSettings(
    EchoSettingsSnapshot const& oldValue,
    EchoSettingsSnapshot const& newValue
);
[[nodiscard]] RawEchoSettings readGeodeEchoSettings();
```

The snapshot must represent every current non-title setting: ghost/profile/fades/xray, Last, Best, trails, death/heat, replay defaults, sample rate, replay retention, disk budget, Rendering Quality, diagnostics.

- [ ] **Step 1: Write normalization/property tests first**

Representative assertions:

```text
requested ghost 999 -> 256
Competitive effective ghost count -> min(requested,8)
opacity -10/900 -> 0/255
NaN age fade -> current default 1.0
sample rate 999 -> 240
retention 1 -> 256
disk 99999 -> 8192
unknown enum string -> current documented default
Rendering Quality Performance decodes exactly
```

- [ ] **Step 2: Write diff-classification tests**

Sample rate only -> `recorderPolicy`; ghost count/profile -> `fleetStructure`; colors/trails/death/quality -> `presentation`; retention/disk -> `persistencePolicy`; replay speed/camera -> `replayDefaults`; diagnostics toggle -> `diagnostics`.

- [ ] **Step 3: Prove RED, then implement pure normalization/diff**

Non-finite floating settings resolve to the existing default before range clamp. Profile caps remain Clean=2, Competitive=8, Multiverse=64, Chaos/Custom=256.

- [ ] **Step 4: Implement `EchoSettingsGeode.cpp` as the only direct Geode setting-read boundary**

It reads exact existing `mod.json` keys into `RawEchoSettings`; it does not normalize.

- [ ] **Step 5: Replace `settingsFingerprint()` and helper parsing in `main.cpp`**

Store requested/applied snapshots plus revision. Existing 0.5-second polling may remain in this plan, but it compares normalized snapshots rather than formatting a 33-field string. Delete `profileGhostCap`, `playbackRateFromSetting`, `cameraModeFromSetting`, and direct `getSettingValue` calls from `main.cpp` once migrated.

- [ ] **Step 6: Prove recorder-policy boundary**

Do not call `recorder.setCaptureSampleRate` when a sample-rate setting changes during an active attempt. Apply the requested capture value immediately before the next `beginAttempt()`.

- [ ] **Step 7: Source audit and GREEN**

```powershell
Select-String -Path src\*.cpp -Pattern 'getSettingValue<'
```

Expected: production setting reads only in `EchoSettingsGeode.cpp`.

- [ ] **Step 8: Commit**

```bash
git add src/EchoSettings* src/main.cpp tests/cpp/test_settings.cpp CMakeLists.txt
git commit -m "refactor: centralize validated ECHO_DASH settings"
```

---

### Task 4: Introduce the pure runtime state machine

**Files:**
- Create: `src/EchoRuntimeState.hpp/.cpp`
- Create: `tests/cpp/test_runtime_state.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

```cpp
enum class RuntimeState : std::uint8_t {
    Initializing, Playing, ReplayStudio, DeathContinuation,
    ResetPending, Resetting, Completing, Exiting
};

enum class RuntimeEvent : std::uint8_t {
    InitializationComplete, OpenStudio, CloseStudio, ConfirmDeath,
    RequestReset, ContinuationComplete, ResetExecuted,
    CompleteLevel, ExitLevel
};

struct TransitionResult { RuntimeState from; RuntimeState to; bool accepted = false; };

class EchoRuntimeStateMachine final {
public:
    [[nodiscard]] TransitionResult apply(RuntimeEvent event);
    [[nodiscard]] RuntimeState state() const;
};
```

- [ ] **Step 1: Write legal-transition tests**

Prove:

```text
Initializing -> Playing
Playing -> ReplayStudio -> Playing
Playing -> DeathContinuation -> ResetPending -> Resetting -> Playing
Playing -> Resetting -> Playing (manual alive reset)
Playing -> Completing -> Exiting
Playing -> Exiting
```

- [ ] **Step 2: Write rejection-without-mutation tests**

Reject ReplayStudio+ConfirmDeath, DeathContinuation+OpenStudio, Exiting+OpenStudio, Completing+InitializationComplete. State must remain unchanged.

- [ ] **Step 3: Prove RED, implement explicit transition table/switch, prove GREEN**

`ExitLevel` transitions any non-Exiting live state to Exiting; after Exiting all events reject.

- [ ] **Step 4: Commit**

```bash
git add src/EchoRuntimeState.* tests/cpp/test_runtime_state.cpp CMakeLists.txt
git commit -m "refactor: add explicit ECHO_DASH runtime state machine"
```

---

### Task 5: Split attempt-history preparation from mutation

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

Keep `commitFinalizedAttempt(...)` temporarily as a compatibility wrapper calling prepare then commit until coordinator migration is complete.

- [ ] **Step 1: Write a failing non-mutation test**

Prepare finalized attempt #7 and assert history remains empty; commit once -> one entry; commit same entry again -> false, still one entry.

- [ ] **Step 2: Add `EchoAttemptHistory.cpp` to core-test sources and prove RED**

- [ ] **Step 3: Extract all entry construction/validation into preparation**

Preparation requires finalized nonzero attempt, supported end reason, finite valid summary fields, and returns without touching deque/counters/revision.

- [ ] **Step 4: Put all history mutation into `commitPreparedEntry`**

Duplicate ID rejects without changing outcome/PB counters other than the existing duplicate-rejection statistic.

- [ ] **Step 5: Prove GREEN and commit**

```bash
git add src/EchoAttemptHistory.* tests/cpp/test_attempt_history.cpp CMakeLists.txt
git commit -m "refactor: separate attempt history preparation from commit"
```

---

### Task 6: Extract `EchoRuntimeCoordinator` and remove lifecycle boolean authority

**Files:**
- Create: `src/EchoRuntimeCoordinator.hpp/.cpp`
- Modify: `src/main.cpp`
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

enum class ResetRequestResult : std::uint8_t { Deferred, ExecuteNow, Rejected };
enum class AttemptCommitStatus : std::uint8_t {
    NoActiveAttempt, Committed, PendingDurability, Rejected
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
    [[nodiscard]] bool confirmDeath(DeathEvent const&, LiveFrameContext const&);
    [[nodiscard]] ResetRequestResult requestReset();
    [[nodiscard]] bool advanceContinuation(double dt);
    [[nodiscard]] AttemptCommitStatus finalize(AttemptEndReason reason);
    [[nodiscard]] bool openReplayStudio();
    [[nodiscard]] bool closeReplayStudio();
    void beginCompleting();
    void beginExit();
    void resetExecuted();
};
```

- [ ] **Step 1: Add failing architecture contracts**

Require coordinator files, require `main.cpp` to own/use a coordinator, and forbid direct `recorder.finalizeAttempt(` plus `history.commitFinalizedAttempt(` lifecycle policy in `main.cpp` after migration.

- [ ] **Step 2: Implement start/capture policy**

Start only from Playing with no active attempt; apply sample rate before `beginAttempt`; capture initial event frame. Capture only in Playing and update recorder + fleet tracking from the same `LiveFrameContext`.

- [ ] **Step 3: Move terminal-death policy**

`main.cpp::destroyPlayer` still captures candidate facts, calls vanilla `PlayLayer::destroyPlayer`, then dispatches a confirmed terminal observation only when vanilla changed the player to dead. Coordinator records the death/event frame once, transitions to DeathContinuation, and calls the one fleet continuation engine. Duplicates outside Playing reject.

- [ ] **Step 4: Move reset/continuation policy**

RequestReset from Playing -> Resetting/ExecuteNow. From DeathContinuation -> ResetPending/Deferred. `advanceContinuation` can move ResetPending -> Resetting exactly once when fleet reports complete.

- [ ] **Step 5: Move finalization orchestration with a temporary Plan-01 persistence adapter**

Until Plan 02 replaces persistence, use:

```text
recorder finalize
-> resolve finalized attempt
-> prepare history entry (non-mutating)
-> archive ingest/save through one coordinator helper
-> if logical archive acceptance succeeded: commit prepared history
-> update session best/replay/fleet revision requests
```

If current `archive.save()` fails after logical ingest, return `PendingDurability` and do not pretend disk durability succeeded. Plan 02 replaces this adapter with the journal store.

- [ ] **Step 6: Remove behavioral booleans from `main.cpp`**

Remove `captureEnabled`, `confirmedDeath`, `deferredResetRequested`, and `replayStudioOpen` as lifecycle authorities. Orthogonal facts such as archive loaded, fleet rebuild requested, diagnostics display, and viewport-restore validity may remain.

- [ ] **Step 7: Route Studio state through coordinator**

Controls callback asks coordinator to open/close Studio; `main.cpp` still owns Cocos viewport/node plumbing. Failed coordinator open does not dismiss PauseLayer.

- [ ] **Step 8: Replace obsolete regex tests with stable contracts**

Delete tests requiring old boolean names. Keep/strengthen: state machine/coordinator existence, one role-agnostic playback engine, PauseLayer-only Studio entry, no live launcher.

- [ ] **Step 9: Prove GREEN including terminal Windows workflow**

Run native/Python tests, push task commit, then wait for the pinned Windows workflow to finish. Build success is required; runtime behavior is not yet certified by this task.

- [ ] **Step 10: Commit**

```bash
git add src/EchoRuntimeCoordinator.* src/main.cpp tests/test_v1_1_contract.py
git commit -m "refactor: centralize ECHO_DASH runtime lifecycle"
```

---

### Task 7: Enforce safe settings application boundaries and wire every setting

**Files:**
- Modify: `src/EchoRuntimeCoordinator.hpp/.cpp`
- Modify: `src/main.cpp`
- Modify: `mod.json`
- Modify: `tests/cpp/test_settings.cpp`
- Modify: `tests/test_v1_1_contract.py`

**Interfaces:** typed `EchoSettingsDiff` determines immediate vs deferred consequences.

- [ ] **Step 1: Add hostile-change/property tests**

Presentation-only changes never set recorder/persistence flags. Sample-rate changes only set recorder policy. Rendering Quality is decoded/applied as presentation state and never changes capture settings.

- [ ] **Step 2: Prove the existing Rendering Quality hole RED**

Add a source contract requiring `rendering-quality` to be read in `EchoSettingsGeode.cpp` and represented in `EchoSettingsSnapshot`. It fails before wiring.

- [ ] **Step 3: Apply categories at legal boundaries**

```text
presentation -> relevant renderer immediately when attached
fleet structure -> mark rebuild; execute only in legal safe state
recorder policy -> next startAttempt only
persistence policy -> configure policy only; no heavy save from callback
replay defaults -> session defaults
diagnostics -> display/observer state only
rendering quality -> requested presentation policy; Plan 03 implements cost behavior
```

- [ ] **Step 4: Improve wording without changing storage keys/scales**

`Recorder Sample Rate` description explicitly says “Changes apply to the next attempt.” Display label `Replay Archive Run Limit` becomes `Saved Replay Limit`; JSON key remains `replay-retention`.

- [ ] **Step 5: Run full tests + terminal Windows build and commit**

```bash
git add src/EchoRuntimeCoordinator.* src/main.cpp mod.json tests
git commit -m "fix: make ECHO_DASH settings boundaries explicit"
```

---

### Task 8: Plan-01 evidence gate

**Files:** modify only if verification finds a defect.

- [ ] **Step 1: Fresh local suite**

```powershell
python -m unittest discover -s tests -p "test_*.py" -v
cmake -S . -B build-core-tests -DECHO_DASH_BUILD_CORE_TESTS=ON
cmake --build build-core-tests --config Release --target EchoDashCoreTests
ctest --test-dir build-core-tests -C Release --output-on-failure
```

Expected: zero failures and no Geode SDK required by the core-only configure.

- [ ] **Step 2: Source audit**

Check direct `getSettingValue` reads exist only in `EchoSettingsGeode.cpp`; old behavioral booleans are gone; one fleet playback-engine member exists; `EchoGhostPlaybackEngine.*` contains no `GhostRole` dependency.

- [ ] **Step 3: Trigger full pinned Windows Release workflow and wait for terminal completion**

Expected terminal GREEN for Python contracts, native CTest, pinned CLI/SDK setup, MSVC Geode Release build, compiler evidence, package collection/upload.

- [ ] **Step 4: Review diff against approved scope**

No Plan-02 persistence format, Plan-03 UX redesign, or new feature scope may have leaked into this foundation pass.

- [ ] **Step 5: Record exact source SHA/workflow run ID and proceed to Plan 02 only with terminal evidence.**
