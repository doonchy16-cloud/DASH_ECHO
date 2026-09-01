# ECHO_DASH Hardening 04 — Diagnostics and Geometry Dash Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make failures reconstructable and contained, and harden every Geometry Dash/Geode lifecycle boundary so ECHO_DASH cannot double-finalize, reenter unsafe transitions, or permanently block vanilla gameplay lifecycle.

**Architecture:** Add one bounded structured diagnostic authority owned by the runtime coordinator, publish state/health/performance transitions into it without hot-loop noise, and make the existing diagnostics overlay a read-only projection. Then formalize exactly-once vanilla-call contracts, add a continuation-progress watchdog independent of the PlayLayer `postUpdate` override, and adversarially test callback order/mode/context behavior.

**Tech Stack:** C++23, fixed-capacity native data structures, Cocos2d scheduler/Geode hooks, CTest dependency-free policy tests, Python source contracts, pinned Windows runtime certification.

**Spec:** `docs/superpowers/specs/2026-09-01-echo-dash-quality-hardening-design.md`

## Global Constraints

- Run only after Plan 03 is terminal GREEN.
- Diagnostics remain local; no network telemetry, automatic upload, credentials, or gameplay-data reporting is introduced.
- Diagnostics are read-only observers; toggling them cannot alter recorder, playback, persistence, lifecycle, or attempt outcomes.
- Geometry Dash lifecycle authority outranks optional ECHO_DASH presentation.
- No ECHO_DASH state may indefinitely block reset, completion, pause, resume, or exit while the surrounding Cocos/Geometry Dash scheduler is still making progress.
- `Exiting` is terminal.
- Base Geometry Dash lifecycle methods are invoked at most once for each corresponding vanilla callback/accepted deferred action.
- Platformer never uses classic-percent synchronization authority.
- Practice and dual-player behavior are separately tested; no new practice/dual feature is added.

---

## File Structure for This Plan

**Create:**
- `src/EchoDiagnostics.hpp`
- `src/EchoDiagnostics.cpp` — bounded structured events, health, deduplication, performance rolling metrics, read-only snapshot.
- `src/EchoIntegrationPolicy.hpp`
- `src/EchoIntegrationPolicy.cpp` — pure mode/context/progress-authority and callback-order helpers that do not depend on Cocos nodes.
- `tests/cpp/test_diagnostics.cpp`
- `tests/cpp/test_integration_policy.cpp`
- `tests/cpp/test_runtime_callback_model.cpp`
- `docs/runtime/ECHO_DASH_RUNTIME_CERTIFICATION_CHECKLIST_v1.1.3.md` — exact manual/runtime matrix used again by Plan 05.

**Modify:**
- `src/EchoRuntimeCoordinator.hpp/.cpp`
- `src/EchoRuntimeState.hpp/.cpp`
- `src/EchoGhostFleet.hpp/.cpp`
- `src/EchoReplayArchive.hpp/.cpp`
- `src/EchoReplayControls.hpp/.cpp`
- `src/main.cpp`
- `CMakeLists.txt`
- `tests/test_v1_1_contract.py`

---

### Task 1: Add bounded structured diagnostics and subsystem health

**Files:**
- Create: `src/EchoDiagnostics.hpp`
- Create: `src/EchoDiagnostics.cpp`
- Create: `tests/cpp/test_diagnostics.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces:

```cpp
enum class DiagnosticCategory : std::uint8_t {
    Lifecycle,
    Recorder,
    Playback,
    Ghost,
    Archive,
    Recovery,
    UI,
    Settings,
    Performance,
    Integration
};

enum class DiagnosticSeverity : std::uint8_t { Debug, Info, Warning, Error };
enum class SubsystemHealth : std::uint8_t { Healthy, Degraded, Unavailable };
enum class EchoSubsystem : std::uint8_t {
    Runtime,
    Recorder,
    Archive,
    GhostRenderer,
    ReplayStudio,
    DeathPresentation,
    Diagnostics
};

struct DiagnosticEvent {
    std::uint64_t sequence = 0;
    double sessionSeconds = 0.0;
    DiagnosticCategory category = DiagnosticCategory::Lifecycle;
    DiagnosticSeverity severity = DiagnosticSeverity::Info;
    RuntimeState runtimeState = RuntimeState::Initializing;
    std::uint64_t attemptId = 0;
    std::uint32_t operation = 0;
    std::uint32_t result = 0;
    std::uint32_t repeatCount = 1;
};

struct PerformanceMetricSnapshot {
    double currentMs = 0.0;
    double averageMs = 0.0;
    double p95Ms = 0.0;
    double worstMs = 0.0;
};

struct EchoDiagnosticsSnapshot {
    std::uint64_t newestSequence = 0;
    std::size_t eventCount = 0;
    std::array<SubsystemHealth, 7> health {};
    PerformanceMetricSnapshot presentation;
};

class EchoDiagnostics final {
public:
    static constexpr std::size_t kEventCapacity = 256;
    static constexpr std::size_t kPerformanceWindow = 240;

    void setSessionSeconds(double value);
    void record(DiagnosticEvent event);
    void setHealth(EchoSubsystem subsystem, SubsystemHealth health, std::uint32_t reason);
    void observePresentationMs(double value);
    [[nodiscard]] EchoDiagnosticsSnapshot snapshot() const;
    [[nodiscard]] std::vector<DiagnosticEvent> recentEvents() const;
};
```

The bounded vector returned by `recentEvents()` is created only on explicit diagnostics inspection, not in hot paths.

- [ ] **Step 1: Write sequence/capacity tests**

Record 300 distinct events and assert newest sequence is 300 while retained event count is exactly 256. Sequences must strictly increase from the first retained event to the last.

- [ ] **Step 2: Write duplicate-suppression tests**

Record the same failure signature `(category, runtimeState, attemptId, operation, result)` repeatedly without an intervening state/result change. It occupies one retained event whose `repeatCount` increments. A later recovery result creates a new event and closes the degraded health state.

- [ ] **Step 3: Write performance-window tests**

Feed 240 deterministic samples and verify current, arithmetic average, worst, and p95 (nearest-rank 95th percentile over the bounded window) match expected values. Feed invalid/negative values and assert they are ignored.

- [ ] **Step 4: Prove RED, implement fixed-capacity ring/metrics, prove GREEN**

No per-frame formatted strings are stored. Diagnostic event creation uses numeric enums/IDs; human-readable formatting happens only in the overlay/log projection.

- [ ] **Step 5: Commit**

```bash
git add src/EchoDiagnostics.* tests/cpp/test_diagnostics.cpp CMakeLists.txt
git commit -m "feat: add bounded ECHO_DASH diagnostics authority"
```

---

### Task 2: Wire diagnostics to authoritative subsystem transitions

**Files:**
- Modify: `src/EchoRuntimeCoordinator.hpp/.cpp`
- Modify: `src/EchoReplayArchive.hpp/.cpp`
- Modify: `src/EchoGhostFleet.hpp/.cpp`
- Modify: `src/EchoRenderingQuality.hpp/.cpp`
- Modify: `src/main.cpp`
- Modify: `tests/cpp/test_diagnostics.cpp`

**Interfaces:**
- Consumes: structured commit/maintenance states, runtime transitions, fleet frame stats, Rendering Quality effective state.
- Produces: authoritative lifecycle/archive/recovery/settings/performance events and health transitions.

- [ ] **Step 1: Inject `EchoDiagnostics&` into the runtime coordinator**

The coordinator records state transitions, attempt start/finalize outcomes, persistence durability transitions, Studio open/close outcomes, and illegal transition attempts. It does not record per-frame ghost updates.

- [ ] **Step 2: Map archive state to health deterministically**

`Durable` with no pending entries -> Archive Healthy. `PendingDurability` -> Archive Degraded. Repeated/permanent store failure -> Archive Degraded with a distinct reason. Successful retry/restore -> emit recovery event and return Healthy. Unrecoverable load with gameplay continuing on an empty safe authority -> Archive Unavailable for the rejected context until a later successful durable commit proves recovery.

- [ ] **Step 3: Map presentation failures independently**

Fleet attach/allocation failure changes GhostRenderer health only. Replay Controls creation failure changes ReplayStudio health only. Death/heat attach failure changes DeathPresentation health only. Recorder/archive health must remain unchanged.

- [ ] **Step 4: Feed performance aggregates without formatting**

The Plan-03 presentation timer calls `diagnostics.observePresentationMs()`. Diagnostics display reads rolling snapshot every 0.5 seconds; no archive or replay scan is performed to compute it.

- [ ] **Step 5: Record requested/effective settings transitions**

When Rendering Quality Auto changes effective mode, record one Settings/Performance event. Do not log every Auto observation.

- [ ] **Step 6: Prove GREEN and commit**

```bash
git add src/EchoRuntimeCoordinator.* src/EchoReplayArchive.* src/EchoGhostFleet.* src/EchoRenderingQuality.* src/main.cpp tests/cpp/test_diagnostics.cpp
git commit -m "feat: connect ECHO_DASH diagnostics to authority"
```

---

### Task 3: Formalize exactly-once vanilla lifecycle actions and reentrancy containment

**Files:**
- Create: `tests/cpp/test_runtime_callback_model.cpp`
- Modify: `src/EchoRuntimeCoordinator.hpp/.cpp`
- Modify: `src/EchoRuntimeState.hpp/.cpp`
- Modify: `src/main.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces:

```cpp
enum class VanillaAction : std::uint8_t { None, Reset, LevelComplete, Exit };

struct VanillaActionRequest {
    VanillaAction action = VanillaAction::None;
    std::uint64_t sequence = 0;
};

[[nodiscard]] VanillaActionRequest pendingVanillaAction() const;
[[nodiscard]] bool acknowledgeVanillaAction(std::uint64_t sequence);
```

Only the coordinator may create an action request; `main.cpp` consumes and acknowledges it exactly once around the required base call.

- [ ] **Step 1: Write callback-model tests for duplicate requests**

Model these sequences and assert one finalization/action only:

```text
Reset, Reset
Death, Reset, Reset, ContinuationComplete
Complete, Complete
Exit, Exit
Death, Exit, Reset
```

- [ ] **Step 2: Prove RED**

Expected: compile FAIL before the action-request interface exists.

- [ ] **Step 3: Implement monotonic vanilla-action sequence IDs**

A second request for the same action while one is pending returns the existing request. Acknowledged action sequences cannot be acknowledged twice. `Exiting` rejects all later reset/Studio/start-attempt requests.

- [ ] **Step 4: Refactor base calls in `main.cpp` through one helper per action**

Use helpers with visible base invocation:

```cpp
void executeVanillaReset(VanillaActionRequest request);
void executeVanillaLevelComplete(VanillaActionRequest request);
void executeVanillaExit(VanillaActionRequest request);
```

Each checks the expected action, calls the base method exactly once, then acknowledges the sequence. No other source path directly calls that base lifecycle method except initialization and the corresponding helper.

- [ ] **Step 5: Add source contracts for base-call count/location**

Python tests inspect `main.cpp` and require one explicit helper call site for each lifecycle base method; obsolete direct calls in other ECHO_DASH helpers are rejected.

- [ ] **Step 6: Prove GREEN and commit**

```bash
git add src/EchoRuntimeCoordinator.* src/EchoRuntimeState.* src/main.cpp tests/cpp/test_runtime_callback_model.cpp tests/test_v1_1_contract.py CMakeLists.txt
git commit -m "fix: make vanilla lifecycle actions exactly-once"
```

---

### Task 4: Add continuation liveness containment with an independent heartbeat watchdog

**Files:**
- Modify: `src/EchoGhostFleet.hpp/.cpp`
- Modify: `src/EchoRuntimeCoordinator.hpp/.cpp`
- Modify: `src/main.cpp`
- Modify: `tests/cpp/test_runtime_callback_model.cpp`

**Interfaces:**
- Produces:

```cpp
enum class ContinuationAdvanceResult : std::uint8_t {
    Progressed,
    Completed,
    Invalid
};

[[nodiscard]] ContinuationAdvanceResult advanceContinuation(double dt);

struct ContinuationWatchdog {
    double schedulerElapsedSeconds = 0.0;
    double lastObservedContinuationSeconds = 0.0;
    double stalledSeconds = 0.0;
    bool armed = false;
};
```

Watchdog law after vanilla reset has been requested:

```text
if continuation elapsed advances -> stalledSeconds = 0
if scheduler ticks but continuation elapsed does not advance -> accumulate stalledSeconds
if invalid playback authority -> fail over immediately
if stalledSeconds >= 0.75 seconds -> fail over
```

The 0.75-second rule detects absence of continuation progress; it is not a maximum allowed ghost-continuation duration.

- [ ] **Step 1: Write watchdog policy tests**

A 10-second continuation that advances every watchdog tick must never fail over. A continuation with reset pending and no continuation progress for 0.74 seconds remains pending; at 0.75 seconds it emits exactly one reset action. `Invalid` emits the reset action immediately.

- [ ] **Step 2: Prove RED and implement `ContinuationAdvanceResult`**

`EchoGhostFleet` returns Completed only when all selected active ghost attempts are complete, Progressed when valid continuation time advanced, and Invalid when shared playback state/source data is non-finite or structurally unusable.

- [ ] **Step 3: Arm a Cocos scheduler watchdog only while ResetPending**

`main.cpp` schedules a small callback on the PlayLayer/Cocos scheduler when coordinator enters ResetPending and unschedules it on reset execution, completion, or exit. The callback calls a coordinator watchdog method using scheduler delta; it does not perform rendering.

- [ ] **Step 4: On liveness failover, preserve attempt data and release optional presentation before vanilla reset**

Required order:

```text
record Integration warning/recovery context
-> fleet.stop() / release replay references
-> preserve already-finalized/pending-durability attempt state
-> transition ResetPending -> Resetting through an explicit failover event
-> queue one VanillaAction::Reset
```

No arbitrary replay/attempt deletion occurs.

- [ ] **Step 5: Add source contract that no timeout skips a healthy advancing continuation**

The watchdog condition must include a stalled/non-progressing predicate; a raw `if elapsed >= 0.75 then reset` is forbidden.

- [ ] **Step 6: Prove GREEN and commit**

```bash
git add src/EchoGhostFleet.* src/EchoRuntimeCoordinator.* src/main.cpp tests/cpp/test_runtime_callback_model.cpp tests/test_v1_1_contract.py
git commit -m "fix: guarantee death-continuation liveness"
```

---

### Task 5: Centralize mode/context integration policy and harden teardown

**Files:**
- Create: `src/EchoIntegrationPolicy.hpp`
- Create: `src/EchoIntegrationPolicy.cpp`
- Create: `tests/cpp/test_integration_policy.cpp`
- Modify: `src/EchoRuntimeCoordinator.hpp/.cpp`
- Modify: `src/main.cpp`
- Modify: `src/EchoReplayControls.cpp`
- Modify: `src/EchoGhostFleet.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces:

```cpp
struct IntegrationMode {
    bool platformer = false;
    bool practice = false;
};

[[nodiscard]] bool usesProgressAuthority(IntegrationMode mode);
[[nodiscard]] bool mayOpenReplayStudio(RuntimeState state);
[[nodiscard]] bool mayRebuildFleet(RuntimeState state);
```

Rules:

```text
usesProgressAuthority(platformer=true) = false
usesProgressAuthority(platformer=false) = true
mayOpenReplayStudio only in Playing
mayRebuildFleet only in Playing and when no unsafe archive consumer transition is active
```

- [ ] **Step 1: Write pure policy tests**

Cover classic normal, classic practice, platformer normal, platformer practice, and every runtime state for Studio/fleet permissions.

- [ ] **Step 2: Prove RED and implement the pure policy**

Replace repeated `!m_fields->levelContext.platformer` expressions with `usesProgressAuthority` at the integration boundary.

- [ ] **Step 3: Make context switches explicit coordinator operations**

On practice/mode/context change: close Studio if needed, release fleet/replay consumers, retry pending durability/maintenance at safe boundary, load new archive context, restore death analytics, set replay archive, apply settings, start the new attempt only after state returns to Playing.

- [ ] **Step 4: Make teardown dominance explicit**

On `beginExit`: unschedule watchdog, close/detach Studio idempotently, stop fleet, finalize at most once, best-effort persistence retry/maintenance, detach presentation, then queue one vanilla exit. After state Exiting, no new attempt/fleet rebuild/settings maintenance/Studio opening is accepted.

- [ ] **Step 5: Make attach/detach idempotence observable in tests/source contracts**

Repeated `detach()` calls on Fleet/Replay/Death/Heat/Controls do not dereference stale parents or double-remove nodes. Missing optional nodes mark only their subsystem unavailable/degraded.

- [ ] **Step 6: Prove GREEN and commit**

```bash
git add src/EchoIntegrationPolicy.* src/EchoRuntimeCoordinator.* src/main.cpp src/EchoReplayControls.cpp src/EchoGhostFleet.cpp tests/cpp/test_integration_policy.cpp CMakeLists.txt
git commit -m "refactor: harden Geometry Dash integration policy"
```

---

### Task 6: Expand adversarial callback-order and dual/practice/platformer model tests

**Files:**
- Modify: `tests/cpp/test_runtime_callback_model.cpp`
- Modify: `tests/cpp/test_integration_policy.cpp`
- Modify: `tests/test_v1_1_contract.py`

**Interfaces:**
- Produces: executable callback-order evidence independent of graphics rendering.

- [ ] **Step 1: Add event permutation cases**

At minimum:

```text
init -> updates -> death -> reset request -> continuation complete -> reset executed
init -> manual reset alive -> reset executed
init -> Studio open -> Studio close -> reset
init -> Studio open -> exit
init -> death -> reset pending -> exit
init -> death -> duplicate death -> duplicate reset -> continuation complete
init -> level complete -> exit callback
```

For each, assert one legal terminal/intermediate state, one finalization identity, and at most one pending vanilla action.

- [ ] **Step 2: Add dual-player observation cases**

Model P1 and P2 death observations with one attempt ID. Duplicate/non-terminal observations cannot create a second attempt finalization or second continuation anchor. Player index remains analytics metadata only.

- [ ] **Step 3: Add practice-mode cases**

Repeated checkpoint-like death/reset sequences remain within the practice archive context; switching practice flag causes an explicit context transition rather than merging histories.

- [ ] **Step 4: Add platformer cases**

Every platformer tracking/continuation fixture asserts `progressAuthority == false`; elapsed-time behavior remains deterministic.

- [ ] **Step 5: Run CTest and commit**

```bash
git add tests/cpp/test_runtime_callback_model.cpp tests/cpp/test_integration_policy.cpp tests/test_v1_1_contract.py
git commit -m "test: adversarially model ECHO_DASH lifecycle callbacks"
```

---

### Task 7: Replace the diagnostics overlay with a read-only authority snapshot

**Files:**
- Modify: `src/main.cpp`
- Modify: `src/EchoDiagnostics.hpp/.cpp`
- Modify: `tests/cpp/test_diagnostics.cpp`
- Modify: `tests/test_v1_1_contract.py`

**Interfaces:**
- Consumes: coordinator state, recorder stats, archive stats/revisions, fleet frame stats, Rendering Quality requested/effective, diagnostic health/performance snapshot.
- Produces: existing on-screen diagnostics toggle with clearer authority-oriented text; no new UI entrypoint.

- [ ] **Step 1: Add a read-only self-check result type**

```cpp
struct EchoSelfCheck {
    bool runtimeStateLegal = true;
    bool activeAttemptInvariant = true;
    bool archiveRevisionOrderValid = true;
    bool diagnosticsObserverOnly = true;
};

[[nodiscard]] EchoSelfCheck selfCheck(...) const;
```

The function accepts const snapshots/values only. It cannot call repair/mutation methods.

- [ ] **Step 2: Write tests proving diagnostics ON/OFF cannot mutate authority**

Run equivalent pure coordinator/settings/diagnostic event sequences with diagnostics display enabled/disabled; state transitions and commit decisions must match.

- [ ] **Step 3: Format diagnostics at the existing bounded cadence**

Every 0.5 seconds while enabled, show concise lines for runtime state/attempt, recorder sample rate, ghost assigned/configured/effective quality, archive memory/durable/snapshot revisions and pending/quarantine/recovery, presentation performance aggregate, and subsystem health. Do not scan archive contents to build this string.

- [ ] **Step 4: Add concise exit summary**

Log attempts started/finalized, memory/durable/snapshot revisions, pending count, recovery/quarantine, peak/last ghost state, and illegal-transition count. Diagnostic label creation/formatting failure changes Diagnostics health only.

- [ ] **Step 5: Prove GREEN and commit**

```bash
git add src/EchoDiagnostics.* src/main.cpp tests/cpp/test_diagnostics.cpp tests/test_v1_1_contract.py
git commit -m "fix: make ECHO_DASH diagnostics read-only and actionable"
```

---

### Task 8: Create and execute the integration runtime certification checklist

**Files:**
- Create: `docs/runtime/ECHO_DASH_RUNTIME_CERTIFICATION_CHECKLIST_v1.1.3.md`
- Modify only if runtime evidence finds defects: relevant source/tests from this plan.

**Interfaces:**
- Produces: repeatable human/runtime evidence checklist reused by final release Plan 05.

- [ ] **Step 1: Write the checklist with exact cases**

Include sections and PASS fields for:

- classic normal: alive manual reset, confirmed death continuation, complete, exit;
- dual: both player identities observed, no duplicate attempt lifecycle;
- platformer: death/reset/replay with elapsed-time authority;
- practice: checkpoint deaths/resets and normal/practice context separation;
- Replay Studio: repeated open/close, scrub, frame step, attempt navigation, camera/speed, exit during Studio;
- continuation liveness: normal long advancing continuation is not cut short; forced/diagnostic invalid-continuation case releases reset;
- restart: history/replays persist and recovery health is truthful;
- missing/failed presentation containment where practically injectable;
- no stuck reset/completion/pause/resume/exit.

- [ ] **Step 2: Run complete source tests**

```powershell
python -m unittest discover -s tests -p "test_*.py" -v
cmake -S . -B build-core-tests -DECHO_DASH_BUILD_CORE_TESTS=ON
cmake --build build-core-tests --config Release --target EchoDashCoreTests
ctest --test-dir build-core-tests -C Release --output-on-failure
```

Expected: zero failures.

- [ ] **Step 3: Trigger full pinned Windows Release workflow and wait for terminal success**

Record exact source SHA, run ID, artifact ID, and package SHA256.

- [ ] **Step 4: Execute the runtime checklist against that exact package**

Any observed stuck lifecycle, duplicate finalization, Studio input leakage, platformer percent-authority use, or crash is FAIL. Fixes create a new candidate and require re-running affected source/build/runtime evidence.

- [ ] **Step 5: Record Plan-04 terminal evidence and proceed to Plan 05 only after the checklist has no unresolved FAIL.**
