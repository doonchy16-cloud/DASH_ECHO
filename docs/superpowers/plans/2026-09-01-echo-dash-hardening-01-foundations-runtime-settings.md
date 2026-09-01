# ECHO_DASH Hardening 01 — Foundations, Runtime, and Settings Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Establish the testable core, validated configuration authority, explicit runtime state machine, and coordinator boundaries needed for all later hardening while preserving v1.1.2 behavior.

**Architecture:** Add a dependency-free native C++ test target for pure ECHO_DASH logic, introduce narrowly scoped semantic/time/settings types, split attempt-summary preparation from mutation, then move lifecycle authority from `main.cpp` booleans into `EchoRuntimeStateMachine` and `EchoRuntimeCoordinator`. Geode hooks remain responsible only for collecting vanilla objects/facts and invoking the required base methods.

**Tech Stack:** C++23, CMake 3.21+, CTest, dependency-free custom C++ test harness, Geode 5.10.1, Geometry Dash Windows 2.2081, Python `unittest`, GitHub Actions `windows-latest`.

**Spec:** `docs/superpowers/specs/2026-09-01-echo-dash-quality-hardening-design.md`

## Global Constraints

- No user-facing feature additions or removals.
- `ECHO_DASH` and `doonchy.dash-echo` remain unchanged.
- Do not create branches or worktrees; commit task checkpoints directly to `main` only after their tests are GREEN.
- Exactly one ghost timing authority remains `EchoGhostPlaybackEngine`.
- Ghost role remains presentation-only.
- Recorder sampling remains 30-240 Hz; capture-policy changes apply to the next attempt, never silently mid-attempt.
- This plan does not replace persistence storage; it creates the commit boundary that Plan 02 will make transactional.
- This plan does not redesign Replay Studio layout or ghost rendering; those belong to Plan 03.
- No runtime PASS claims are allowed from these source/build tests.

---

## File Structure for This Plan

**Create:**
- `src/EchoCoreTypes.hpp` — selective semantic wrappers for high-risk numeric identities/positions.
- `src/EchoTimePolicy.hpp`
- `src/EchoTimePolicy.cpp` — one finite/negative/spike delta sanitation policy.
- `src/EchoSettings.hpp`
- `src/EchoSettings.cpp` — raw settings model, normalized immutable snapshot, enums, and typed diff.
- `src/EchoSettingsGeode.hpp`
- `src/EchoSettingsGeode.cpp` — only Geode `Mod::get()` settings decoding boundary.
- `src/EchoRuntimeState.hpp`
- `src/EchoRuntimeState.cpp` — pure legal state/event transition authority.
- `src/EchoRuntimeCoordinator.hpp`
- `src/EchoRuntimeCoordinator.cpp` — attempt/continuation/finalization lifecycle orchestration over existing subsystems.
- `tests/cpp/TestHarness.hpp`
- `tests/cpp/test_main.cpp`
- `tests/cpp/test_current_invariants.cpp`
- `tests/cpp/test_core_types.cpp`
- `tests/cpp/test_time_policy.cpp`
- `tests/cpp/test_settings.cpp`
- `tests/cpp/test_runtime_state.cpp`
- `tests/cpp/test_attempt_history.cpp`

**Modify:**
- `CMakeLists.txt` — add an opt-in pure-core CTest target without changing normal Geode build output.
- `.github/workflows/build-v1.yml` — configure/build/run pure-core tests before the Geode Release build.
- `src/EchoAttemptHistory.hpp`
- `src/EchoAttemptHistory.cpp` — split prepare from commit while retaining a compatibility wrapper during migration.
- `src/main.cpp` — consume normalized settings, use shared time policy, delegate lifecycle policy to the coordinator, remove obsolete behavior booleans/fingerprint logic.
- `tests/test_v1_1_contract.py` — remove implementation-shape assertions that intentionally become obsolete; replace them with stable architectural contracts.

---

### Task 1: Add a dependency-free native core test target

**Files:**
- Create: `tests/cpp/TestHarness.hpp`
- Create: `tests/cpp/test_main.cpp`
- Create: `tests/cpp/test_current_invariants.cpp`
- Modify: `CMakeLists.txt`
- Modify: `.github/workflows/build-v1.yml`

**Interfaces:**
- Consumes: existing pure headers such as `EchoRecorder.hpp`.
- Produces: CMake option `ECHO_DASH_BUILD_CORE_TESTS`, executable `EchoDashCoreTests`, and CTest name `EchoDashCoreTests` used by every later task.

- [ ] **Step 1: Write the tiny test harness and one baseline test file**

Create `tests/cpp/TestHarness.hpp` with a registry, registration helper, and two macros:

```cpp
#pragma once

#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace echo_test {

using TestFunction = void (*)();

struct TestCase {
    std::string name;
    TestFunction function = nullptr;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Register {
    Register(char const* name, TestFunction function) {
        registry().push_back({name, function});
    }
};

inline void check(bool condition, char const* expression, char const* file, int line) {
    if (!condition) {
        throw std::runtime_error(
            std::string(file) + ":" + std::to_string(line) + " CHECK failed: " + expression
        );
    }
}

} // namespace echo_test

#define ECHO_TEST(name) \
    static void name(); \
    static echo_test::Register name##_registration(#name, &name); \
    static void name()

#define ECHO_CHECK(expression) \
    echo_test::check(static_cast<bool>(expression), #expression, __FILE__, __LINE__)
```

Create `tests/cpp/test_main.cpp`:

```cpp
#include "TestHarness.hpp"

#include <exception>
#include <iostream>

int main() {
    std::size_t failures = 0;
    for (auto const& test : echo_test::registry()) {
        try {
            test.function();
            std::cout << "PASS " << test.name << '\n';
        } catch (std::exception const& error) {
            ++failures;
            std::cerr << "FAIL " << test.name << ": " << error.what() << '\n';
        }
    }
    std::cout << (echo_test::registry().size() - failures) << "/"
              << echo_test::registry().size() << " passed\n";
    return failures == 0 ? 0 : 1;
}
```

Create `tests/cpp/test_current_invariants.cpp`:

```cpp
#include "TestHarness.hpp"
#include "EchoRecorder.hpp"

ECHO_TEST(current_recorder_limits_are_preserved) {
    ECHO_CHECK(dash_echo::EchoRecorder::kMinCaptureSampleRate == 30.0);
    ECHO_CHECK(dash_echo::EchoRecorder::kDefaultCaptureSampleRate == 120.0);
    ECHO_CHECK(dash_echo::EchoRecorder::kMaxCaptureSampleRate == 240.0);
}
```

- [ ] **Step 2: Run CMake before adding the test target and prove RED**

Run:

```powershell
cmake -S . -B build-core-tests -DECHO_DASH_BUILD_CORE_TESTS=ON
```

Expected: configuration does not create `EchoDashCoreTests`; `ctest --test-dir build-core-tests -C Release -N` lists zero ECHO_DASH core tests.

- [ ] **Step 3: Add the CMake test target**

Append the following before `add_subdirectory($ENV{GEODE_SDK} ...)`:

```cmake
option(ECHO_DASH_BUILD_CORE_TESTS "Build dependency-free ECHO_DASH core tests" OFF)

if (ECHO_DASH_BUILD_CORE_TESTS)
    enable_testing()
    add_executable(EchoDashCoreTests
        tests/cpp/test_main.cpp
        tests/cpp/test_current_invariants.cpp
    )
    target_include_directories(EchoDashCoreTests PRIVATE src tests/cpp)
    target_compile_features(EchoDashCoreTests PRIVATE cxx_std_23)
    add_test(NAME EchoDashCoreTests COMMAND EchoDashCoreTests)
endif()
```

Keep the normal `EchoDash` shared-library target unchanged.

- [ ] **Step 4: Build and prove GREEN**

Run:

```powershell
cmake -S . -B build-core-tests -DECHO_DASH_BUILD_CORE_TESTS=ON
cmake --build build-core-tests --config Release --target EchoDashCoreTests
ctest --test-dir build-core-tests -C Release --output-on-failure
```

Expected: `EchoDashCoreTests` PASS with `1/1 passed`.

- [ ] **Step 5: Add the native test commands to CI before Geode package compilation**

Add a workflow step before `Install pinned Geode CLI 3.7.4`:

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

Run:

```powershell
python -m unittest tests.test_v1_1_contract tests.test_release_assets -v
```

Expected: all pre-existing tests PASS.

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt .github/workflows/build-v1.yml tests/cpp
git commit -m "test: add native ECHO_DASH core harness"
```

---

### Task 2: Add semantic core values and one time-sanitization policy

**Files:**
- Create: `src/EchoCoreTypes.hpp`
- Create: `src/EchoTimePolicy.hpp`
- Create: `src/EchoTimePolicy.cpp`
- Create: `tests/cpp/test_core_types.cpp`
- Create: `tests/cpp/test_time_policy.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/main.cpp`
- Modify: `src/EchoGhostPlaybackEngine.cpp`

**Interfaces:**
- Produces: `AttemptId`, `FrameSequence`, `ReplayTime`, `ProgressPercent`, `NormalizedCursor`; `sanitizeDeltaSeconds(double, double = 0.25)`.
- Consumes: no Geode types.

- [ ] **Step 1: Write failing semantic/time tests**

Create `tests/cpp/test_core_types.cpp`:

```cpp
#include "TestHarness.hpp"
#include "EchoCoreTypes.hpp"

ECHO_TEST(semantic_values_do_not_cross_convert) {
    dash_echo::AttemptId attempt {42};
    dash_echo::FrameSequence frame {42};
    ECHO_CHECK(attempt.value == 42);
    ECHO_CHECK(frame.value == 42);
    static_assert(!std::is_convertible_v<dash_echo::AttemptId, dash_echo::FrameSequence>);
}
```

Create `tests/cpp/test_time_policy.cpp`:

```cpp
#include "TestHarness.hpp"
#include "EchoTimePolicy.hpp"

#include <limits>

ECHO_TEST(delta_policy_rejects_invalid_and_bounds_spikes) {
    ECHO_CHECK(dash_echo::sanitizeDeltaSeconds(-1.0) == 0.0);
    ECHO_CHECK(dash_echo::sanitizeDeltaSeconds(std::numeric_limits<double>::quiet_NaN()) == 0.0);
    ECHO_CHECK(dash_echo::sanitizeDeltaSeconds(std::numeric_limits<double>::infinity()) == 0.0);
    ECHO_CHECK(dash_echo::sanitizeDeltaSeconds(1.0) == 0.25);
    ECHO_CHECK(dash_echo::sanitizeDeltaSeconds(0.125) == 0.125);
}
```

- [ ] **Step 2: Add the new test files to `EchoDashCoreTests` and prove RED**

Run the CMake build/CTest commands from Task 1.

Expected: compile FAIL because `EchoCoreTypes.hpp` and `EchoTimePolicy.hpp` do not exist.

- [ ] **Step 3: Implement the minimal semantic wrappers**

Create `src/EchoCoreTypes.hpp`:

```cpp
#pragma once

#include <cstdint>

namespace dash_echo {

struct AttemptId {
    std::uint64_t value = 0;
    bool operator==(AttemptId const&) const = default;
};

struct FrameSequence {
    std::uint64_t value = 0;
    bool operator==(FrameSequence const&) const = default;
};

struct ReplayTime {
    double seconds = 0.0;
    bool operator==(ReplayTime const&) const = default;
};

struct ProgressPercent {
    float value = 0.0f;
    bool operator==(ProgressPercent const&) const = default;
};

struct NormalizedCursor {
    float value = 0.0f;
    bool operator==(NormalizedCursor const&) const = default;
};

} // namespace dash_echo
```

Create `src/EchoTimePolicy.hpp`:

```cpp
#pragma once

namespace dash_echo {

[[nodiscard]] double sanitizeDeltaSeconds(double dt, double maximum = 0.25);

} // namespace dash_echo
```

Create `src/EchoTimePolicy.cpp`:

```cpp
#include "EchoTimePolicy.hpp"

#include <algorithm>
#include <cmath>

namespace dash_echo {

double sanitizeDeltaSeconds(double dt, double maximum) {
    if (!std::isfinite(dt) || dt <= 0.0 || !std::isfinite(maximum) || maximum <= 0.0) {
        return 0.0;
    }
    return std::clamp(dt, 0.0, maximum);
}

} // namespace dash_echo
```

- [ ] **Step 4: Replace duplicated `dt` sanitation in `main.cpp` and `EchoGhostPlaybackEngine.cpp`**

Use:

```cpp
auto const safeDt = dash_echo::sanitizeDeltaSeconds(static_cast<double>(dt));
```

Do not change replay timing semantics beyond sharing the same defensive clamp policy.

- [ ] **Step 5: Prove GREEN**

Run native CTest and Python regression suite.

Expected: all tests PASS; no current timing contract regresses.

- [ ] **Step 6: Commit**

```bash
git add src/EchoCoreTypes.hpp src/EchoTimePolicy.* src/main.cpp src/EchoGhostPlaybackEngine.cpp tests/cpp CMakeLists.txt
git commit -m "refactor: centralize ECHO_DASH core time policy"
```

---

### Task 3: Build the immutable validated settings authority

**Files:**
- Create: `src/EchoSettings.hpp`
- Create: `src/EchoSettings.cpp`
- Create: `src/EchoSettingsGeode.hpp`
- Create: `src/EchoSettingsGeode.cpp`
- Create: `tests/cpp/test_settings.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/main.cpp`

**Interfaces:**
- Produces:
  - `enum class VisualProfile { Clean, Competitive, Multiverse, Chaos, Custom };`
  - `enum class RenderingQuality { Auto, Full, Balanced, Performance };`
  - `struct RawEchoSettings;`
  - `struct EchoSettingsSnapshot;`
  - `struct EchoSettingsDiff { bool presentation; bool fleetStructure; bool recorderPolicy; bool persistencePolicy; bool replayDefaults; bool diagnostics; };`
  - `normalizeEchoSettings(RawEchoSettings const&) -> EchoSettingsSnapshot`
  - `diffEchoSettings(EchoSettingsSnapshot const&, EchoSettingsSnapshot const&) -> EchoSettingsDiff`
  - `readGeodeEchoSettings() -> RawEchoSettings`
- Consumes: `ColorRGB`, current mod.json keys, existing playback-rate/camera enums.

- [ ] **Step 1: Define the complete raw/effective model in a failing test**

The model must contain every non-title setting currently exposed in `mod.json`: ghost count/profile, older opacity range/fade/xray, Last settings, Best settings, trail settings, death settings, replay defaults, recorder sample rate, replay retention, disk budget, rendering quality, and diagnostics.

Write `tests/cpp/test_settings.cpp` with these representative properties:

```cpp
#include "TestHarness.hpp"
#include "EchoSettings.hpp"

#include <limits>

ECHO_TEST(settings_normalization_clamps_and_applies_profile_cap) {
    dash_echo::RawEchoSettings raw;
    raw.ghostCount = 999;
    raw.visualProfile = "Competitive";
    raw.olderOpacityMin = -10;
    raw.olderOpacityMax = 900;
    raw.ageFadeStrength = std::numeric_limits<double>::quiet_NaN();
    raw.recorderSampleRate = 999;
    raw.replayRetention = 1;
    raw.diskBudgetMb = 99'999;
    raw.renderingQuality = "Performance";

    auto const settings = dash_echo::normalizeEchoSettings(raw);
    ECHO_CHECK(settings.ghost.requestedCount == 256);
    ECHO_CHECK(settings.ghost.effectiveCount == 8);
    ECHO_CHECK(settings.ghost.oldestOpacity == 0);
    ECHO_CHECK(settings.ghost.newestOlderOpacity == 255);
    ECHO_CHECK(settings.ghost.ageFadeStrength == 1.0f);
    ECHO_CHECK(settings.capture.sampleRateHz == 240.0);
    ECHO_CHECK(settings.storage.replayLimit == 256);
    ECHO_CHECK(settings.storage.diskBudgetMb == 8192);
    ECHO_CHECK(settings.rendering.requested == dash_echo::RenderingQuality::Performance);
}

ECHO_TEST(settings_diff_classifies_side_effects) {
    auto a = dash_echo::normalizeEchoSettings({});
    auto b = a;
    b.capture.sampleRateHz = 240.0;
    auto const diff = dash_echo::diffEchoSettings(a, b);
    ECHO_CHECK(diff.recorderPolicy);
    ECHO_CHECK(!diff.presentation);
    ECHO_CHECK(!diff.persistencePolicy);
}
```

- [ ] **Step 2: Add `EchoSettings.cpp` to the native test target and prove RED**

Expected: compile FAIL because the settings interfaces do not exist.

- [ ] **Step 3: Implement all raw and normalized settings fields**

Use nested immutable-by-convention value structs. The effective snapshot must expose, at minimum:

```cpp
struct GhostSettings {
    std::size_t requestedCount = 16;
    std::size_t effectiveCount = 16;
    VisualProfile profile = VisualProfile::Multiverse;
    std::uint8_t oldestOpacity = 36;
    std::uint8_t newestOlderOpacity = 104;
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
};

struct CaptureSettings { double sampleRateHz = 120.0; };
struct StorageSettings { std::size_t replayLimit = 10'000; std::size_t diskBudgetMb = 2'048; };
struct RenderingSettings { RenderingQuality requested = RenderingQuality::Auto; };
```

Also define trail, death, replay-default, and diagnostics fields matching the existing defaults exactly. Unknown enum strings use the current defaults. Non-finite floats use the current defaults before range clamp.

- [ ] **Step 4: Implement `EchoSettingsDiff` by semantic category**

`presentation` covers colors/opacities/trails/death/heat/rendering quality; `fleetStructure` covers ghost count/profile; `recorderPolicy` covers recorder sample rate; `persistencePolicy` covers replay limit/disk budget; `replayDefaults` covers default replay speed/camera; `diagnostics` covers diagnostics visibility.

- [ ] **Step 5: Implement the Geode decoder in one file**

`readGeodeEchoSettings()` is the only production function in this plan allowed to call `Mod::get()->getSettingValue`. It fills `RawEchoSettings` using the exact existing keys from `mod.json`, then returns without clamping; normalization remains pure and testable.

- [ ] **Step 6: Replace `settingsFingerprint()` and direct setting reads in `main.cpp`**

Add fields:

```cpp
dash_echo::EchoSettingsSnapshot appliedSettings;
dash_echo::EchoSettingsSnapshot requestedSettings;
std::uint64_t settingsRevision = 0;
```

On the existing 0.5-second settings poll, call `readGeodeEchoSettings()`, normalize it, compare to `requestedSettings`, and derive a typed diff. Apply only the categories that are legal immediately. Store recorder-policy changes as requested configuration for `startNewAttempt()` rather than changing the active recorder sampling regime mid-attempt.

Remove `settingsFingerprint()`, `profileGhostCap()`, `playbackRateFromSetting()`, and `cameraModeFromSetting()` from `main.cpp` once their behavior is provided by the settings model.

- [ ] **Step 7: Prove recorder-policy boundary behavior with a testable helper**

Add to the settings model:

```cpp
[[nodiscard]] bool capturePolicyChanged(EchoSettingsDiff const& diff);
```

and test that presentation-only changes return false while sample-rate changes return true. In `startNewAttempt()`, apply `requestedSettings.capture.sampleRateHz` immediately before `recorder.beginAttempt()`.

- [ ] **Step 8: Prove GREEN**

Run native CTest and Python suite. Search production source:

```powershell
Select-String -Path src\*.cpp -Pattern 'getSettingValue<'
```

Expected: setting reads exist only in `src/EchoSettingsGeode.cpp` after this task.

- [ ] **Step 9: Commit**

```bash
git add src/EchoSettings* src/main.cpp tests/cpp/test_settings.cpp CMakeLists.txt
git commit -m "refactor: centralize validated ECHO_DASH settings"
```

---

### Task 4: Introduce the explicit runtime state machine

**Files:**
- Create: `src/EchoRuntimeState.hpp`
- Create: `src/EchoRuntimeState.cpp`
- Create: `tests/cpp/test_runtime_state.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces:
  - `RuntimeState { Initializing, Playing, ReplayStudio, DeathContinuation, ResetPending, Resetting, Completing, Exiting }`
  - `RuntimeEvent { InitializationComplete, OpenStudio, CloseStudio, ConfirmDeath, RequestReset, ContinuationComplete, ResetExecuted, CompleteLevel, ExitLevel }`
  - `TransitionResult { RuntimeState from; RuntimeState to; bool accepted; }`
  - `EchoRuntimeStateMachine::apply(RuntimeEvent)` and `state()`.

- [ ] **Step 1: Write the legal-transition tests**

```cpp
ECHO_TEST(runtime_state_accepts_normal_reset_cycle) {
    dash_echo::EchoRuntimeStateMachine state;
    ECHO_CHECK(state.apply(dash_echo::RuntimeEvent::InitializationComplete).accepted);
    ECHO_CHECK(state.state() == dash_echo::RuntimeState::Playing);
    ECHO_CHECK(state.apply(dash_echo::RuntimeEvent::ConfirmDeath).accepted);
    ECHO_CHECK(state.state() == dash_echo::RuntimeState::DeathContinuation);
    ECHO_CHECK(state.apply(dash_echo::RuntimeEvent::RequestReset).accepted);
    ECHO_CHECK(state.state() == dash_echo::RuntimeState::ResetPending);
    ECHO_CHECK(state.apply(dash_echo::RuntimeEvent::ContinuationComplete).accepted);
    ECHO_CHECK(state.state() == dash_echo::RuntimeState::Resetting);
    ECHO_CHECK(state.apply(dash_echo::RuntimeEvent::ResetExecuted).accepted);
    ECHO_CHECK(state.state() == dash_echo::RuntimeState::Playing);
}
```

Also test `Playing -> ReplayStudio -> Playing`, `Playing -> Completing -> Exiting`, and `Playing -> Exiting`.

- [ ] **Step 2: Write illegal-transition tests**

Prove these reject without state mutation:

```cpp
ReplayStudio + ConfirmDeath
Exiting + OpenStudio
DeathContinuation + OpenStudio
Completing + InitializationComplete
```

- [ ] **Step 3: Add source to test target and prove RED**

Expected: compile FAIL because state-machine interfaces do not exist.

- [ ] **Step 4: Implement a table/switch with no side effects on rejected transitions**

`RequestReset` from `Playing` transitions directly to `Resetting`; `RequestReset` from `DeathContinuation` transitions to `ResetPending`; `ContinuationComplete` is accepted only from `ResetPending` and transitions to `Resetting`. `ExitLevel` is accepted from any non-`Exiting` live state and transitions to `Exiting`; after `Exiting`, every event is rejected.

- [ ] **Step 5: Prove GREEN and commit**

```bash
git add src/EchoRuntimeState.* tests/cpp/test_runtime_state.cpp CMakeLists.txt
git commit -m "refactor: add explicit ECHO_DASH runtime state machine"
```

---

### Task 5: Split attempt-history preparation from mutation

**Files:**
- Modify: `src/EchoAttemptHistory.hpp`
- Modify: `src/EchoAttemptHistory.cpp`
- Create: `tests/cpp/test_attempt_history.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces:
  - `prepareFinalizedAttempt(...) -> std::optional<AttemptHistoryEntry>`
  - `commitPreparedEntry(AttemptHistoryEntry const&) -> bool`
- Preserves temporarily: `commitFinalizedAttempt(...)` as a wrapper that calls prepare then commit so current callers remain behavior-compatible until Task 6.

- [ ] **Step 1: Write failing tests for non-mutating preparation**

Create a finalized `AttemptRecord` fixture with `attemptId=7`, one valid frame, `maxProgressPercent=42`, and `endReason=Reset`. Assert:

```cpp
auto prepared = history.prepareFinalizedAttempt(attempt, nullptr, 30.0f, 3);
ECHO_CHECK(prepared.has_value());
ECHO_CHECK(history.entries().empty());
ECHO_CHECK(prepared->attemptId == 7);
ECHO_CHECK(history.commitPreparedEntry(*prepared));
ECHO_CHECK(history.entries().size() == 1);
ECHO_CHECK(!history.commitPreparedEntry(*prepared));
ECHO_CHECK(history.entries().size() == 1);
```

- [ ] **Step 2: Add `EchoAttemptHistory.cpp` and test file to the native test target and prove RED**

Expected: compile FAIL because `prepareFinalizedAttempt` and `commitPreparedEntry` do not exist.

- [ ] **Step 3: Extract the existing entry-building logic into `prepareFinalizedAttempt`**

Preparation validates `attempt.finalized`, nonzero attempt identity, supported end reason, and builds the complete `AttemptHistoryEntry` without mutating counters/deque/revision.

- [ ] **Step 4: Move all deque/counter/revision mutation into `commitPreparedEntry`**

Duplicate attempt IDs return false and increment only `duplicateCommitsRejected`; they may not mutate committed entries or other outcome counters.

- [ ] **Step 5: Keep compatibility wrapper and prove GREEN**

Run CTest and Python regression suite.

- [ ] **Step 6: Commit**

```bash
git add src/EchoAttemptHistory.* tests/cpp/test_attempt_history.cpp CMakeLists.txt
git commit -m "refactor: separate attempt history preparation from commit"
```

---

### Task 6: Extract `EchoRuntimeCoordinator` and centralize lifecycle policy

**Files:**
- Create: `src/EchoRuntimeCoordinator.hpp`
- Create: `src/EchoRuntimeCoordinator.cpp`
- Modify: `src/main.cpp`
- Modify: `tests/test_v1_1_contract.py`

**Interfaces:**
- Consumes: `EchoRuntimeStateMachine`, `EchoRecorder`, `EchoReplayArchive`, `EchoAttemptHistory`, `EchoDeathAnalytics`, `EchoGhostFleet`, `EchoReplaySession`, normalized settings.
- Produces:

```cpp
struct LiveFrameContext {
    float progressPercent = 0.0f;
    bool progressAuthority = false;
    PlayerObject* player1 = nullptr;
    PlayerObject* player2 = nullptr;
    cocos2d::CCNode* viewportLayer = nullptr;
};

enum class ResetRequestResult : std::uint8_t { Deferred, ExecuteNow, Rejected };
enum class AttemptCommitStatus : std::uint8_t { NoActiveAttempt, Committed, PendingDurability, Rejected };

class EchoRuntimeCoordinator final {
public:
    EchoRuntimeCoordinator(
        EchoRecorder& recorder,
        EchoReplayArchive& archive,
        EchoAttemptHistory& history,
        EchoDeathAnalytics& deaths,
        EchoGhostFleet& fleet,
        EchoReplaySession& replay
    );

    void initializationComplete();
    [[nodiscard]] RuntimeState state() const;
    [[nodiscard]] bool startAttempt(LiveFrameContext const& live, double sampleRateHz);
    void captureFrame(double dt, LiveFrameContext const& live);
    [[nodiscard]] bool confirmDeath(DeathEvent const& death, LiveFrameContext const& live);
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

Plan 02 may expand `AttemptCommitStatus` persistence detail, but these names remain stable for consumers.

- [ ] **Step 1: Add a new Python architecture contract that fails before extraction**

Add assertions that `src/EchoRuntimeCoordinator.hpp/.cpp` exist, `main.cpp` contains `EchoRuntimeCoordinator`, and direct calls to `recorder.finalizeAttempt(` plus `history.commitFinalizedAttempt(` no longer occur in `main.cpp`.

Run:

```powershell
python -m unittest tests.test_v1_1_contract -v
```

Expected: FAIL because coordinator files do not exist.

- [ ] **Step 2: Implement coordinator construction and attempt start/capture**

`startAttempt` applies the passed sample rate immediately before `recorder.beginAttempt()`, captures the initial event frame, and is accepted only in `Playing` with no active attempt. `captureFrame` is accepted only in `Playing` and delegates one frame capture plus one fleet tracking update using the same live context.

- [ ] **Step 3: Move confirmed-death continuation policy into coordinator**

`confirmDeath` is accepted only from `Playing`, records the death once, captures the event frame, transitions with `ConfirmDeath`, and starts the one shared fleet continuation. Duplicate confirmed-death requests outside `Playing` return false without re-recording or re-anchoring.

- [ ] **Step 4: Move reset/continuation policy into coordinator**

`requestReset()` applies `RequestReset`. From `DeathContinuation`, return `Deferred`; from `Playing`, return `ExecuteNow`; rejected transitions return `Rejected`. `advanceContinuation()` advances only in `DeathContinuation` or `ResetPending`; when state is `ResetPending` and `fleet.continuationComplete()`, apply `ContinuationComplete` and return true exactly once.

- [ ] **Step 5: Move attempt finalization orchestration into coordinator**

Use the Task 5 prepare/commit split. In this plan only, preserve current persistence behavior behind the coordinator: finalized recorder attempt -> prepare summary -> archive ingest -> archive save -> commit prepared history entry -> replay/fleet consumer refresh. If `archive.ingest` rejects, do not commit history. If `archive.save` fails, return `PendingDurability` and preserve the complete logical attempt state; Plan 02 replaces this temporary behavior with journal durability.

- [ ] **Step 6: Replace lifecycle booleans in `main.cpp`**

Remove `captureEnabled`, `confirmedDeath`, `deferredResetRequested`, and `replayStudioOpen` as behavioral authorities. `archiveReady`, `fleetNeedsRebuild`, diagnostics visibility, and viewport-restore validity may remain as orthogonal facts until later plans.

`postUpdate`, `destroyPlayer`, `resetLevel`, `levelComplete`, and `onExit` query/dispatch through the coordinator. Base Geometry Dash methods remain in `main.cpp` so their exact invocation order is visible at the integration boundary.

- [ ] **Step 7: Keep Replay Studio state coordinated**

The current controls callback calls `coordinator.openReplayStudio()` before viewport capture/hide/replay presentation and `coordinator.closeReplayStudio()` before viewport restore/resume. Failed open leaves the PauseLayer flow unchanged; Plan 03 makes the full UI transaction explicit.

- [ ] **Step 8: Update old implementation-shaped Python tests**

Remove regex requirements for `confirmedDeath` and `deferredResetRequested`. Replace them with stable contracts: state-machine/coordinator files exist, main routes death/reset through coordinator, `GhostRole` still absent from playback engine, and PauseLayer remains the only ordinary Studio entrypoint.

- [ ] **Step 9: Prove GREEN**

Run native CTest, full Python suite, then the pinned Windows Release workflow. The task is not complete until the workflow reaches terminal success.

- [ ] **Step 10: Commit**

```bash
git add src/EchoRuntimeCoordinator.* src/main.cpp tests/test_v1_1_contract.py
git commit -m "refactor: centralize ECHO_DASH runtime lifecycle"
```

---

### Task 7: Harden safe-boundary settings application and remove stale setting paths

**Files:**
- Modify: `src/EchoRuntimeCoordinator.hpp`
- Modify: `src/EchoRuntimeCoordinator.cpp`
- Modify: `src/main.cpp`
- Modify: `mod.json`
- Modify: `tests/cpp/test_settings.cpp`
- Modify: `tests/test_v1_1_contract.py`

**Interfaces:**
- Consumes: `EchoSettingsSnapshot`, `EchoSettingsDiff`.
- Produces: explicit pending recorder/persistence/fleet changes held until their legal boundary.

- [ ] **Step 1: Add tests for hostile lifecycle changes**

Test pure settings classification for sample-rate, fleet structure, persistence, presentation, replay defaults, rendering quality, and diagnostics. Add a source contract that `rendering-quality` is decoded by `EchoSettingsGeode.cpp` and no longer exists as a zombie setting.

- [ ] **Step 2: Prove RED**

Expected: at least the rendering-quality application contract FAILS until wired into the effective settings path.

- [ ] **Step 3: Apply each settings category at its legal boundary**

- presentation: immediate renderer configuration;
- fleet structure: set `fleetNeedsRebuild`, rebuild only when not Continuing/Resetting/ReplayStudio and archive references are safe;
- recorder policy: apply in `startAttempt` only;
- persistence policy: update archive configuration only; do not invoke full `save()` from the settings callback;
- replay defaults: update session defaults;
- diagnostics: visibility/state only;
- rendering quality: store requested/effective policy now, but Plan 03 supplies the presentation-cost behavior. It must not modify recorder configuration.

- [ ] **Step 4: Improve existing settings descriptions without changing keys/storage scales**

Update `recorder-sample-rate` description to include “Changes apply to the next attempt.” Rename display text `Replay Archive Run Limit` to `Saved Replay Limit` and clarify retention semantics. Keep JSON keys unchanged.

- [ ] **Step 5: Prove GREEN and commit**

Run all core/Python tests and Windows Release build.

```bash
git add src/EchoRuntimeCoordinator.* src/main.cpp mod.json tests
git commit -m "fix: make ECHO_DASH settings boundaries explicit"
```

---

### Task 8: Plan-01 regression and evidence gate

**Files:**
- Modify only if verification finds a defect: files from Tasks 1-7.

**Interfaces:**
- Produces: terminal evidence required before Plan 02.

- [ ] **Step 1: Run the complete local source test set**

```powershell
python -m unittest discover -s tests -p "test_*.py" -v
cmake -S . -B build-core-tests -DECHO_DASH_BUILD_CORE_TESTS=ON
cmake --build build-core-tests --config Release --target EchoDashCoreTests
ctest --test-dir build-core-tests -C Release --output-on-failure
```

Expected: zero failures.

- [ ] **Step 2: Perform the stale-path scan**

Search for direct production settings reads outside `EchoSettingsGeode.cpp`, obsolete lifecycle booleans, duplicate ghost timing engines, and `GhostRole` references in `EchoGhostPlaybackEngine.*`.

Expected: no direct settings reads outside the adapter; no removed behavioral booleans; exactly one fleet playback-engine member; no role dependency in playback engine.

- [ ] **Step 3: Trigger and wait for the pinned Windows Release workflow**

Expected terminal evidence: Python contracts GREEN, native core CTest GREEN, Geode CLI 3.7.4 installation GREEN, Geode SDK 5.10.1 installation GREEN, Windows Release package compilation GREEN, compiler evidence upload GREEN, package collection/upload GREEN.

- [ ] **Step 4: Review the diff against the approved spec**

Confirm no user-facing feature was added/removed and no Plan-02 persistence or Plan-03 UX redesign was pulled forward unnecessarily.

- [ ] **Step 5: Record the terminal Plan-01 commit SHA/run ID in the execution log and proceed to Plan 02 only after all above evidence is terminal.**
