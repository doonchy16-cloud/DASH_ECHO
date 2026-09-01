# ECHO_DASH Quality Hardening Master Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Harden the existing ECHO_DASH feature set into a production-grade v1.1.3 patch release without adding new user-facing capabilities.

**Architecture:** Execute the approved hardening design as five dependency-ordered subplans. Each subplan leaves `main` in a buildable, behavior-preserving state and has its own TDD, MSVC, review, and evidence gates. The final subplan promotes one immutable candidate through package, install, runtime, stress, and soak certification.

**Tech Stack:** C++23, Geode SDK/Loader 5.10.1, Geometry Dash Windows 2.2081, CMake 3.21+, Python 3 `unittest`, CTest plus a dependency-free C++ core harness, GitHub Actions `windows-latest`, and Windows PowerShell 5.1-compatible release scripts.

**Spec:** `docs/superpowers/specs/2026-09-01-echo-dash-quality-hardening-design.md`

## Global Constraints

- User-facing feature scope is frozen: no new ghost roles/types, analytics, cameras, replay controls, gameplay overlays, archive browsing UI, spectator camera, network telemetry, or training features.
- Product name remains `ECHO_DASH`; compatibility mod ID remains exactly `doonchy.dash-echo`.
- Target release is `v1.1.3`; stable v1.1.2 remains the rollback baseline until final promotion.
- After Gate 0 captures the untouched v1.1.2 baseline, Plan 01 changes source/package version identity to `v1.1.3` immediately so changed hardening binaries are never mislabeled as v1.1.2. Until Plan 05 freezes a release candidate, CI artifact names must include `hardening-dev` and the source SHA; those artifacts are test builds, not promoted releases.
- Supported integration target remains Geode 5.10.1 / Geometry Dash Windows 2.2081 / x86-64 / C++23 until a separate approved compatibility design changes it.
- One `EchoGhostPlaybackEngine` remains the only ghost timing authority; `GhostRole` remains presentation-only.
- Supported configured ghost ceiling remains 256; recorder sample-rate range remains 30-240 Hz.
- GD Level PB, Best Recorded Echo, Session Best, and Latest Attempt remain distinct concepts.
- Replay Studio remains pause-menu-only; live gameplay gets no persistent ECHO_DASH launcher.
- Repository workflow is `main` only. Do not create branches or worktrees. This is explicit consent to implement in place on `main`; task commits must remain small, reviewable, and reversible.
- Every behavior task uses RED -> minimal implementation -> GREEN -> affected regression -> commit. If a task cannot produce executable RED/GREEN evidence, it must say why and use the strongest available structural/build/runtime evidence instead.
- No task may claim runtime PASS from source tests, compilation, package inspection, or installation. Evidence gates remain distinct.
- A task that discovers a required user-facing behavior change stops and returns to design review rather than expanding scope silently.
- Refactoring is incremental. No all-at-once rewrite of `main.cpp`, archive storage, Replay Studio, or ghost rendering is permitted.
- Evidence-only documentation commits made after a candidate is frozen do **not** change that candidate. Any change to release-affecting C++ source, metadata, assets, build scripts, installer/diagnostic scripts, or package tooling creates a new candidate and invalidates stronger evidence gathered on the prior bytes.

---

## Program Decomposition

Execute in this order:

1. `docs/superpowers/plans/2026-09-01-echo-dash-hardening-01-foundations-runtime-settings.md`
   - preserve baseline, then move development identity to v1.1.3
   - Geode-independent native test harness and invariant registry
   - semantic/time types and validated immutable settings authority
   - explicit runtime state machine including post-death/no-continuation ordering
   - prepared attempt/history commit boundary
   - `EchoRuntimeCoordinator` extraction and thin Geode hooks

2. `docs/superpowers/plans/2026-09-01-echo-dash-hardening-02-persistence-durability.md`
   - extract pure level/archive context and legacy schema-1 compatibility
   - snapshot/journal wire formats and CRC32
   - context-bound append-only commit journal
   - deterministic primary/backup/journal recovery
   - structurally atomic attempt entries, ordered pending durability, bounded cross-context pending preservation
   - explicit retention/compaction maintenance boundaries
   - corruption, truncation, and filesystem-failure tests

3. `docs/superpowers/plans/2026-09-01-echo-dash-hardening-03-playback-performance-ux.md`
   - one canonical playback resolution per ghost per frame
   - cursor caches that never become authority
   - steady-state allocation and trail bounds
   - revision-driven overlays/structural UI
   - truthful presentation-only `Rendering Quality`
   - Replay Studio immutable view state, responsive layout, disabled states, scrub ownership, complete gameplay-input isolation, and transactional open/close behavior

4. `docs/superpowers/plans/2026-09-01-echo-dash-hardening-04-diagnostics-integration.md`
   - structured local diagnostics and bounded breadcrumbs
   - subsystem health/recovery semantics and exact source/build identity
   - exactly-once vanilla lifecycle action execution with pending/executing/acknowledged state
   - reentrancy/teardown dominance and continuation liveness watchdog
   - classic/platformer/practice/dual callback-model tests and runtime certification checklist

5. `docs/superpowers/plans/2026-09-01-echo-dash-hardening-05-release-certification.md`
   - freeze v1.1.3 release surfaces from the already-versioned hardening source
   - immutable build/package manifest and hash chain
   - transactional ASCII-safe Windows installer with verified rollback and safe elevation
   - target discovery: explicit path -> current Geode profile -> Steam library metadata -> standard Steam path
   - package inspection and PowerShell parse/matrix tests
   - immutable candidate promotion
   - install/runtime/stress/soak/upgrade certification evidence

## Program Gates

### Gate 0 — Untouched v1.1.2 baseline preservation

Before Plan 01 changes version/source behavior, run:

```powershell
python -m unittest tests.test_v1_1_contract tests.test_release_assets -v
```

Record a terminal Windows workflow for the same baseline source SHA. This is the stable v1.1.2 rollback identity. Only after this evidence is recorded may Plan 01 move development identity to v1.1.3.

### Gate 1 — Foundations/runtime/settings

Plan 01 is complete only when:

- all changed hardening packages identify as v1.1.3 and development CI artifacts are clearly `hardening-dev`, not v1.1.2;
- pure-core C++ tests run through CTest without requiring `GEODE_SDK`;
- the runtime state machine rejects illegal transitions without mutation and explicitly handles death with no continuation/continuation completion before vanilla reset request;
- settings normalization/diff tests cover every exposed non-title setting;
- recorder-policy changes are next-attempt-only;
- attempt/history preparation is non-mutating until commit;
- `main.cpp` hooks dispatch lifecycle policy to the coordinator;
- Python/native tests and Windows Release build are terminal GREEN.

### Gate 2 — Persistence durability

Plan 02 is complete only when:

- schema-1 fixture loading remains GREEN;
- complete journal commits survive restart independently of snapshot compaction;
- partial journal tails preserve all earlier valid commits;
- primary/backup/journal recovery follows deterministic context-bound rules;
- failed compaction leaves old authority intact;
- a complete attempt can be pending durability without a partial summary/replay state;
- pending journal revisions remain ordered and a later revision is never durably appended ahead of an earlier failed revision;
- context switches do not silently discard already-accepted pending attempts; bounded pending preservation degrades explicitly if storage remains unavailable;
- retention and compaction are absent from latency-sensitive gameplay paths;
- fault-injection tests and Windows Release build are terminal GREEN.

### Gate 3 — Playback/performance/UX

Plan 03 is complete only when:

- role metamorphic tests prove identical timing across Older/Last/Best/Last+Best;
- each selected ghost has at most one canonical playback-resolution operation per frame;
- prepared steady-state ghost update paths perform no ECHO_DASH-owned heap allocation;
- overlay and structural UI refreshes are revision-driven;
- all four existing Rendering Quality values are wired and affect presentation only;
- Replay Studio controls are a projection of immutable session view state;
- disabled actions cannot mutate session state;
- Studio open/close/viewport/scrub and keyboard/controller/touch input-isolation contracts pass;
- Windows Release build is terminal GREEN.

### Gate 4 — Diagnostics/integration

Plan 04 is complete only when:

- diagnostic events are bounded, sequenced, deduplicated, local, read-only, and identify the exact running source/build where available;
- health transitions close from degraded to recovered deterministically;
- duplicate death/reset/complete/exit callbacks do not double-finalize or execute vanilla lifecycle twice;
- a vanilla action is marked executing before the base call so reentrancy cannot re-execute it;
- `Exiting` is terminal;
- continuation liveness containment releases optional presentation when vanilla lifecycle must proceed, without cutting a healthy advancing continuation because of one long scheduler tick;
- platformer never receives classic progress synchronization authority;
- practice/dual callback-model tests pass;
- Windows Release build and Plan-04 runtime checklist are terminal GREEN.

### Gate 5 — Release/certification

Plan 05 is complete only when the exact same immutable v1.1.3 candidate bytes have terminal evidence for:

`SOURCE -> CONTRACT -> BUILD -> PACKAGE -> INSTALL -> RUNTIME -> STRESS`

A rebuild or release-affecting byte/source change after runtime testing creates a new candidate. Evidence-ledger-only documentation commits do not alter or reidentify the frozen candidate.

## Cross-Plan Review Rules

After every task-level commit, run the affected targeted tests first, then the broad suite appropriate to the current stage. Once Plan 01 has created native tests, the broad local suite is:

```powershell
python -m unittest discover -s tests -p "test_*.py" -v
cmake -S . -B build-core-tests -DECHO_DASH_BUILD_CORE_TESTS=ON
cmake --build build-core-tests --config Release --target EchoDashCoreTests
ctest --test-dir build-core-tests -C Release --output-on-failure
```

At the end of every subplan, trigger the pinned Windows workflow and wait for terminal completion.

Reviewer checklist after each subplan:

- no feature-scope expansion;
- no second ghost timing authority;
- no role-specific timing behavior;
- no new archive mutation path outside persistence authority;
- no heavy maintenance newly introduced into live `postUpdate`;
- no ignored important result that should be `[[nodiscard]]`;
- no user-data deletion during code upgrade paths;
- no pending attempt silently discarded at a context boundary;
- no release artifact mislabeled as the stable v1.1.2 baseline;
- no PASS claim based on queued/running evidence.

## Final Definition of Done

The quality-hardening program is complete only when all five subplans are implemented and reviewed, the v1.1.3 immutable candidate passes the final certification matrix, and runtime evidence confirms the current feature set still behaves correctly on the pinned Geometry Dash/Geode target. The old v1.1.2 package remains the rollback baseline until v1.1.3 reaches terminal RUNTIME and STRESS PASS.
