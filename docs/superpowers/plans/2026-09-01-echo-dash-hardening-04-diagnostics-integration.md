# ECHO_DASH Hardening 04 — Diagnostics and Geometry Dash Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make failures reconstructable and contained, identify the exact running build, and harden every Geometry Dash/Geode lifecycle boundary so ECHO_DASH cannot double-finalize, reenter a vanilla action, or permanently block vanilla gameplay lifecycle.

**Architecture:** Add one bounded structured diagnostic authority owned by the runtime coordinator and expose the exact source/build identity through it. Formalize vanilla lifecycle actions as Pending -> Executing -> Acknowledged so reentrant callbacks cannot execute a base action twice. Add an independent scheduler heartbeat watchdog that detects actual stalled continuation rather than imposing a maximum continuation duration. Centralize mode/context policy and adversarially model callback order, dual/practice/platformer behavior, teardown, and runtime liveness.

**Tech Stack:** C++23, fixed-capacity native data structures, Cocos2d scheduler/Geode hooks, CTest dependency-free policy tests, Python source contracts, pinned Windows runtime certification.

**Spec:** `docs/superpowers/specs/2026-09-01-echo-dash-quality-hardening-design.md`

## Global Constraints

- Run only after Plan 03 is terminal GREEN.
- Diagnostics remain local; no network telemetry, automatic upload, credentials, or gameplay-data reporting.
- Diagnostics are read-only observers; toggling them cannot alter recorder, playback, persistence, lifecycle, or attempt outcomes.
- Geometry Dash lifecycle authority outranks optional ECHO_DASH presentation.
- No ECHO_DASH state may indefinitely block reset, completion, pause, resume, or exit while surrounding scheduler/lifecycle is making progress.
- `Exiting` is terminal.
- Base lifecycle actions are executed at most once per accepted request; action phase is marked Executing **before** invoking the vanilla base method.
- `DeathAwaitingReset` from Plan 01 remains a legal state for no-ghost and continuation-completed-before-reset ordering.
- Platformer never uses classic-percent synchronization authority.
- Practice and dual-player behavior are separately tested; no new mode feature.
- A healthy advancing continuation is never cut short merely because it lasts a long time or one scheduler tick is delayed.

---

## File Structure

**Create:**
- `src/EchoBuildIdentity.hpp/.cpp` — exact version/source/archive/journal identity.
- `src/EchoDiagnostics.hpp/.cpp` — bounded structured events, health, deduplication, performance metrics, read-only snapshot/self-check.
- `src/EchoIntegrationPolicy.hpp/.cpp` — pure mode/context/lifecycle permission helpers.
- `tests/cpp/test_build_identity.cpp`
- `tests/cpp/test_diagnostics.cpp`
- `tests/cpp/test_integration_policy.cpp`
- `tests/cpp/test_runtime_callback_model.cpp`
- `docs/runtime/ECHO_DASH_RUNTIME_CERTIFICATION_CHECKLIST_v1.1.3.md`

**Modify:**
- `CMakeLists.txt`
- `.github/workflows/build-v1.yml`
- `src/EchoRuntimeCoordinator.hpp/.cpp`
- `src/EchoRuntimeState.hpp/.cpp`
- `src/EchoGhostFleet.hpp/.cpp`
- `src/EchoReplayArchive.hpp/.cpp`
- `src/EchoReplayControls.hpp/.cpp`
- `src/EchoRenderingQuality.hpp/.cpp`
- `src/main.cpp`
- `tests/test_v1_1_contract.py`

---

### Task 1: Add exact build identity plus bounded structured diagnostics

**Files:**
- Create: `src/EchoBuildIdentity.hpp/.cpp`
- Create: `src/EchoDiagnostics.hpp/.cpp`
- Create: `tests/cpp/test_build_identity.cpp`
- Create: `tests/cpp/test_diagnostics.cpp`
- Modify: `CMakeLists.txt`
- Modify: `.github/workflows/build-v1.yml`

**Interfaces:**

```cpp
struct EchoBuildIdentity {
    std::string_view product;
    std::string_view version;
    std::string_view sourceSha;
    std::uint32_t snapshotSchema = 2;
    std::uint32_t journalSchema = 1;
};

[[nodiscard]] EchoBuildIdentity currentEchoBuildIdentity();

enum class DiagnosticCategory : std::uint8_t {
    Lifecycle, Recorder, Playback, Ghost, Archive,
    Recovery, UI, Settings, Performance, Integration
};

enum class DiagnosticSeverity : std::uint8_t { Debug, Info, Warning, Error };
enum class SubsystemHealth : std::uint8_t { Healthy, Degraded, Unavailable };
enum class EchoSubsystem : std::uint8_t {
    Runtime, Recorder, Archive, GhostRenderer,
    ReplayStudio, DeathPresentation, Diagnostics
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
    EchoBuildIdentity build;
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

- [ ] **Step 1: Write build-identity tests and prove RED**

Local core test expects product `ECHO_DASH`, version `v1.1.3`, snapshot schema 2, journal schema 1, and source SHA either `local` or 40 lowercase/uppercase hex characters.

- [ ] **Step 2: Inject source SHA at build configuration**

For normal Geode builds, CMake reads `$ENV{GITHUB_SHA}`. If it matches 40 hex characters, add a private compile definition:

```cmake
target_compile_definitions(${PROJECT_NAME} PRIVATE ECHO_DASH_SOURCE_SHA="${ECHO_DASH_SOURCE_SHA}")
```

Otherwise define `ECHO_DASH_SOURCE_SHA="local"`. Core test target uses `local`. Do not use git shell commands as hidden build authority.

- [ ] **Step 3: Write sequence/capacity/dedup tests**

300 distinct events -> newest sequence 300, retained exactly 256, strict retained sequence ordering. Same failure signature `(category,state,attempt,operation,result)` repeats in one retained event with `repeatCount`; recovery result creates a new event/health transition.

- [ ] **Step 4: Write performance-window tests**

240 deterministic samples -> current/mean/worst/nearest-rank p95 exact. Negative/nonfinite observations ignored.

- [ ] **Step 5: Prove RED, implement fixed-capacity ring/metrics, prove GREEN**

No per-frame formatted strings. `recentEvents()` allocates only on explicit diagnostics inspection, not in hot path.

- [ ] **Step 6: Commit**

```bash
git add src/EchoBuildIdentity.* src/EchoDiagnostics.* tests/cpp/test_build_identity.cpp tests/cpp/test_diagnostics.cpp CMakeLists.txt .github/workflows/build-v1.yml
git commit -m "feat: add ECHO_DASH build identity and diagnostics authority"
```

---

### Task 2: Wire diagnostics to authoritative transitions and health

**Files:**
- Modify: `src/EchoRuntimeCoordinator.hpp/.cpp`
- Modify: `src/EchoReplayArchive.hpp/.cpp`
- Modify: `src/EchoGhostFleet.hpp/.cpp`
- Modify: `src/EchoRenderingQuality.hpp/.cpp`
- Modify: `src/main.cpp`
- Modify: `tests/cpp/test_diagnostics.cpp`

**Interfaces:** coordinator owns/receives `EchoDiagnostics&`; diagnostics consumes numeric snapshots/results only.

- [ ] **Step 1: Connect coordinator transitions**

Record accepted/rejected lifecycle transitions, attempt start/finalize outcomes, Studio open/close, durability transitions, and illegal transition attempts. No per-frame ghost event spam.

- [ ] **Step 2: Map archive health deterministically**

Durable + no pending -> Healthy. Pending durability -> Degraded. Persistent/repeated write failure -> Degraded distinct reason. Successful retry -> Recovery event + Healthy. Unrecoverable load with empty-safe context -> Unavailable until a later durable commit proves recovery.

- [ ] **Step 3: Map presentation failures independently**

Fleet allocation/attach failure affects GhostRenderer only; controls creation affects ReplayStudio only; death/heat attach affects DeathPresentation only. Recorder/archive health unchanged.

- [ ] **Step 4: Feed presentation metrics without formatting**

Plan03 timer calls `observePresentationMs`. Overlay reads aggregate at 0.5s cadence; no archive/replay scan.

- [ ] **Step 5: Record requested/effective Rendering Quality transition once per effective change**

Do not log every Auto observation.

- [ ] **Step 6: GREEN + commit**

```bash
git add src/EchoRuntimeCoordinator.* src/EchoReplayArchive.* src/EchoGhostFleet.* src/EchoRenderingQuality.* src/main.cpp tests/cpp/test_diagnostics.cpp
git commit -m "feat: connect ECHO_DASH diagnostics to authority"
```

---

### Task 3: Formalize Pending -> Executing -> Acknowledged vanilla lifecycle actions

**Files:**
- Create: `tests/cpp/test_runtime_callback_model.cpp`
- Modify: `src/EchoRuntimeCoordinator.hpp/.cpp`
- Modify: `src/EchoRuntimeState.hpp/.cpp`
- Modify: `src/main.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

```cpp
enum class VanillaAction : std::uint8_t { None, Reset, LevelComplete, Exit };
enum class VanillaActionPhase : std::uint8_t { None, Pending, Executing };

struct VanillaActionRequest {
    VanillaAction action = VanillaAction::None;
    VanillaActionPhase phase = VanillaActionPhase::None;
    std::uint64_t sequence = 0;
};

[[nodiscard]] VanillaActionRequest pendingVanillaAction() const;
[[nodiscard]] bool beginVanillaAction(std::uint64_t sequence);
[[nodiscard]] bool acknowledgeVanillaAction(std::uint64_t sequence);
```

Only coordinator creates requests. `beginVanillaAction` atomically changes matching Pending -> Executing before any base method call. Reentrant callback while Executing can observe the same request but cannot begin it again. `acknowledgeVanillaAction` clears the matching Executing request after base invocation.

- [ ] **Step 1: Write duplicate/reentrant callback model tests**

At minimum:

```text
Reset, Reset
Death(no continuation), Reset, Reset
Death, Reset, Reset, ContinuationComplete
Death, ContinuationComplete, Reset
Complete, Complete
Exit, Exit
Death, Exit, Reset
```

Assert one attempt finalization + at most one executable vanilla action.

- [ ] **Step 2: Write explicit reentrancy test**

Queue Reset, call `beginVanillaAction(seq)` -> true/Executing. Simulate reentrant reset callback -> no second begin/action. A second `beginVanillaAction(seq)` -> false. `acknowledge` once -> true; second acknowledge -> false.

- [ ] **Step 3: Prove RED and implement monotonic sequence/action state**

Second request for same action while Pending/Executing returns existing request. `Exiting` rejects later reset/Studio/start-attempt requests.

- [ ] **Step 4: Route base calls through one helper per action**

```cpp
void executeVanillaReset(VanillaActionRequest request);
void executeVanillaLevelComplete(VanillaActionRequest request);
void executeVanillaExit(VanillaActionRequest request);
```

Each helper verifies action, calls `beginVanillaAction(sequence)`, invokes exactly one corresponding `PlayLayer::...` base method, then acknowledges. If begin fails, it must not call base. No other ECHO_DASH lifecycle helper directly calls those base methods.

- [ ] **Step 5: Add source contracts for base-call locations**

Python contracts require one explicit helper-owned base invocation per lifecycle operation and reject obsolete direct calls elsewhere.

- [ ] **Step 6: GREEN + commit**

```bash
git add src/EchoRuntimeCoordinator.* src/EchoRuntimeState.* src/main.cpp tests/cpp/test_runtime_callback_model.cpp tests/test_v1_1_contract.py CMakeLists.txt
git commit -m "fix: make vanilla lifecycle actions exactly-once"
```

---

### Task 4: Add continuation liveness containment with independent heartbeat watchdog

**Files:**
- Modify: `src/EchoGhostFleet.hpp/.cpp`
- Modify: `src/EchoRuntimeCoordinator.hpp/.cpp`
- Modify: `src/main.cpp`
- Modify: `tests/cpp/test_runtime_callback_model.cpp`

**Interfaces:**

```cpp
enum class ContinuationAdvanceResult : std::uint8_t {
    Progressed,
    Completed,
    Invalid
};

[[nodiscard]] ContinuationAdvanceResult advanceContinuation(double dt);

struct ContinuationWatchdog {
    double lastObservedContinuationSeconds = 0.0;
    double stalledSeconds = 0.0;
    std::uint32_t stalledTicks = 0;
    bool armed = false;
};
```

Watchdog law only after vanilla reset is pending:

```text
valid continuation elapsed advances -> stalledSeconds=0, stalledTicks=0
scheduler ticks but continuation does not advance -> stalledSeconds += finite scheduler dt, stalledTicks++
Invalid playback -> immediate failover
stalledTicks >= 2 AND stalledSeconds >= 0.75s -> failover
```

The two-tick predicate prevents one delayed scheduler callback from being interpreted as a deadlock. 0.75 seconds measures accumulated observed stall, not total continuation duration.

- [ ] **Step 1: Write watchdog policy tests**

10-second advancing continuation never fails over. No progress for 0.74s remains pending. One single 1.0s stalled callback remains pending because stalledTicks=1. Second stalled callback crossing both conditions emits exactly one Reset action. Invalid emits immediate reset.

- [ ] **Step 2: Prove RED and implement `ContinuationAdvanceResult`**

Completed only when all active selected attempts complete. Progressed only when valid shared continuation time increased. Invalid when shared playback/source state is nonfinite/structurally unusable.

- [ ] **Step 3: Arm Cocos scheduler watchdog only while ResetPending**

Schedule a small PlayLayer callback entering ResetPending; unschedule on Resetting/completion/exit. It calls coordinator watchdog with scheduler dt and performs no rendering.

- [ ] **Step 4: Fail over preserving data**

```text
record Integration warning
-> fleet.stop/release replay references
-> keep already finalized/pending-durability attempt authority
-> ResetPending -> Resetting via explicit failover event
-> queue exactly one VanillaAction::Reset
```

No arbitrary data deletion.

- [ ] **Step 5: Add source contract against raw elapsed timeout**

A condition that resets solely because total continuation elapsed >= 0.75 is forbidden. Required stalled-progress/tick predicates must be present.

- [ ] **Step 6: GREEN + commit**

```bash
git add src/EchoGhostFleet.* src/EchoRuntimeCoordinator.* src/main.cpp tests/cpp/test_runtime_callback_model.cpp tests/test_v1_1_contract.py
git commit -m "fix: guarantee death-continuation liveness"
```

---

### Task 5: Centralize mode/context integration policy and teardown dominance

**Files:**
- Create: `src/EchoIntegrationPolicy.hpp/.cpp`
- Create: `tests/cpp/test_integration_policy.cpp`
- Modify: `src/EchoRuntimeCoordinator.hpp/.cpp`
- Modify: `src/main.cpp`
- Modify: `src/EchoReplayControls.cpp`
- Modify: `src/EchoGhostFleet.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

```cpp
struct IntegrationMode {
    bool platformer = false;
    bool practice = false;
};

[[nodiscard]] bool usesProgressAuthority(IntegrationMode mode);
[[nodiscard]] bool mayOpenReplayStudio(RuntimeState state);
[[nodiscard]] bool mayRebuildFleet(RuntimeState state, bool archiveMutationInFlight);
```

Rules:

```text
platformer -> progress authority false
classic -> progress authority true
Studio may open only in Playing
fleet may rebuild only in Playing AND archiveMutationInFlight=false
```

- [ ] **Step 1: Write pure policy tests for every state/mode**

Classic normal/practice, platformer normal/practice, every RuntimeState including DeathAwaitingReset.

- [ ] **Step 2: Prove RED and implement pure policy**

Replace repeated `!context.platformer` integration expressions with `usesProgressAuthority`.

- [ ] **Step 3: Make context switches explicit and preserve triggering attempt semantics**

Context transition occurs only at a legal lifecycle boundary. Existing active attempt is finalized with the actual triggering existing reason (Reset, Completed, LayerExit); do not invent a new end reason for context change. Release Studio/fleet/replay consumers, retry/detach pending durability per Plan02, load new archive context, restore analytics/session archive, apply settings, then start next attempt only after Playing.

- [ ] **Step 4: Make teardown dominance explicit**

`beginExit`: unschedule watchdog, close/detach Studio idempotently, stop fleet, finalize at most once, best-effort pending retry/maintenance, detach presentation, queue one vanilla Exit. After Exiting, reject new attempt/fleet rebuild/settings maintenance/Studio open.

- [ ] **Step 5: Prove attach/detach idempotence**

Repeated detach on Fleet/Replay/Death/Heat/Controls is safe. Missing optional nodes affect only relevant presentation health.

- [ ] **Step 6: GREEN + commit**

```bash
git add src/EchoIntegrationPolicy.* src/EchoRuntimeCoordinator.* src/main.cpp src/EchoReplayControls.cpp src/EchoGhostFleet.cpp tests/cpp/test_integration_policy.cpp CMakeLists.txt
git commit -m "refactor: harden Geometry Dash integration policy"
```

---

### Task 6: Expand adversarial callback-order and mode model tests

**Files:**
- Modify: `tests/cpp/test_runtime_callback_model.cpp`
- Modify: `tests/cpp/test_integration_policy.cpp`
- Modify: `tests/test_v1_1_contract.py`

- [ ] **Step 1: Add event permutations**

```text
init -> updates -> death -> reset request -> continuation complete -> reset
init -> updates -> death -> continuation complete -> reset request -> reset
init -> death with no ghosts -> reset request -> reset
init -> manual reset alive -> reset
init -> Studio open -> Studio close -> reset
init -> Studio open -> exit
init -> death -> reset pending -> exit
init -> duplicate death/reset -> completion -> one reset
init -> level complete -> exit callback
```

Each has one legal state, one finalization identity, at most one vanilla action.

- [ ] **Step 2: Add dual-player observation cases**

P1/P2 observations share one attempt ID; duplicate/non-terminal observation cannot create second finalization/continuation anchor. Player index remains analytics metadata only.

- [ ] **Step 3: Add practice cases**

Checkpoint-like death/reset stays practice context. Practice flag switch creates explicit context transition rather than merged history.

- [ ] **Step 4: Add platformer cases**

Every platformer tracking/continuation fixture asserts progressAuthority=false.

- [ ] **Step 5: Run CTest and commit**

```bash
git add tests/cpp/test_runtime_callback_model.cpp tests/cpp/test_integration_policy.cpp tests/test_v1_1_contract.py
git commit -m "test: adversarially model ECHO_DASH lifecycle callbacks"
```

---

### Task 7: Replace diagnostics overlay with read-only authority snapshot/self-check

**Files:**
- Modify: `src/main.cpp`
- Modify: `src/EchoDiagnostics.hpp/.cpp`
- Modify: `tests/cpp/test_diagnostics.cpp`
- Modify: `tests/test_v1_1_contract.py`

**Interfaces:**

```cpp
struct EchoSelfCheckInput {
    RuntimeState runtimeState = RuntimeState::Initializing;
    bool recorderHasActiveAttempt = false;
    std::uint64_t memoryRevision = 0;
    std::uint64_t durableRevision = 0;
    std::uint64_t snapshotRevision = 0;
    bool diagnosticsMutationAttempted = false;
};

struct EchoSelfCheck {
    bool runtimeStateLegal = true;
    bool activeAttemptInvariant = true;
    bool archiveRevisionOrderValid = true;
    bool diagnosticsObserverOnly = true;
};

[[nodiscard]] EchoSelfCheck selfCheck(EchoSelfCheckInput const& input) const;
```

- [ ] **Step 1: Write self-check tests**

`durableRevision <= memoryRevision`, `snapshotRevision <= durableRevision`; Playing requires active attempt unless explicit transition is in progress; diagnosticsMutationAttempted=false required. Self-check accepts values only and cannot repair.

- [ ] **Step 2: Prove diagnostics ON/OFF observer equivalence**

Equivalent coordinator/settings/event sequences with display on/off yield identical state/commit decisions.

- [ ] **Step 3: Format authority-oriented overlay at 0.5s cadence**

Show version + short source SHA, runtime state/attempt, recorder rate, ghosts/effective quality, archive memory/durable/snapshot/pending/quarantine/recovery, presentation aggregate, subsystem health. No archive scan to format.

- [ ] **Step 4: Add concise exit summary**

Attempts started/finalized, revisions, pending, recovery/quarantine, ghost state, illegal transition count, build identity. Label/format failure affects Diagnostics health only.

- [ ] **Step 5: GREEN + commit**

```bash
git add src/EchoDiagnostics.* src/main.cpp tests/cpp/test_diagnostics.cpp tests/test_v1_1_contract.py
git commit -m "fix: make ECHO_DASH diagnostics read-only and actionable"
```

---

### Task 8: Create and execute integration runtime certification checklist

**Files:**
- Create: `docs/runtime/ECHO_DASH_RUNTIME_CERTIFICATION_CHECKLIST_v1.1.3.md`
- Modify source/tests only if runtime evidence finds defects.

- [ ] **Step 1: Write exact checklist**

Include PASS/FAIL/evidence fields for:

- exact Geode-visible v1.1.3 + diagnostic short source SHA matching tested source;
- classic alive reset, confirmed death continuation, no-ghost death, completion-before-reset-request, complete, exit;
- dual identities without duplicate lifecycle;
- platformer elapsed-time authority;
- practice checkpoint/reset and context separation;
- Studio repeated open/close/scrub/frame/attempt nav/camera/speed/exit + keyboard/controller/touch input isolation;
- long healthy continuation not cut short; injected invalid/stalled continuation releases reset;
- restart persistence/recovery health;
- optional presentation failure containment where injectable;
- no stuck reset/completion/pause/resume/exit.

- [ ] **Step 2: Run complete source tests**

```powershell
python -m unittest discover -s tests -p "test_*.py" -v
cmake -S . -B build-core-tests -DECHO_DASH_BUILD_CORE_TESTS=ON
cmake --build build-core-tests --config Release --target EchoDashCoreTests
ctest --test-dir build-core-tests -C Release --output-on-failure
```

Expected zero failures.

- [ ] **Step 3: Trigger pinned Windows hardening-dev workflow and wait for terminal success**

Record source SHA, run/job/artifact IDs, package SHA.

- [ ] **Step 4: Execute checklist against exact package**

Any stuck lifecycle, duplicate finalization/base action, Studio input leakage, wrong platformer authority, source identity mismatch, or crash = FAIL. A source/binary fix creates new test bytes and reruns affected evidence.

- [ ] **Step 5: Record Plan-04 terminal evidence; proceed to Plan05 only with no unresolved FAIL.**
