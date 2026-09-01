# ECHO_DASH Quality Hardening Master Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Harden the existing ECHO_DASH feature set into a production-grade v1.1.3 patch release without adding new user-facing capabilities.

**Architecture:** Execute the approved hardening design as five dependency-ordered subplans. Each subplan leaves `main` in a buildable, behavior-preserving state and has its own TDD, MSVC, and review gates; the final subplan promotes one immutable candidate through package, install, runtime, stress, and soak evidence.

**Tech Stack:** C++23, Geode SDK/Loader 5.10.1, Geometry Dash Windows 2.2081, CMake 3.21+, Python 3 `unittest` for source/package contracts, CTest plus a dependency-free C++ test harness for pure-core behavioral tests, GitHub Actions on `windows-latest`, PowerShell 5.1-compatible installer scripts.

**Spec:** `docs/superpowers/specs/2026-09-01-echo-dash-quality-hardening-design.md`

## Global Constraints

- User-facing feature scope is frozen: no new ghost roles/types, analytics, cameras, replay controls, gameplay overlays, archive browsing UI, spectator camera, network telemetry, or training features.
- Product name remains `ECHO_DASH`; compatibility mod ID remains exactly `doonchy.dash-echo`.
- Target release for this no-feature hardening program is `v1.1.3`.
- Supported integration target remains Geode 5.10.1 / Geometry Dash Windows 2.2081 / x86-64 / C++23 until a separate approved compatibility design changes it.
- One `EchoGhostPlaybackEngine` remains the only ghost timing authority; `GhostRole` remains presentation-only.
- Supported configured ghost ceiling remains 256; recorder sample-rate range remains 30-240 Hz.
- GD Level PB, Best Recorded Echo, Session Best, and Latest Attempt remain separate concepts.
- Replay Studio remains pause-menu-only; live gameplay gets no persistent ECHO_DASH launcher.
- Repository workflow for this project is `main` only. Do not create branches or worktrees for this program.
- Every implementation task follows TDD where executable behavior can be isolated: write failing test, prove RED, implement minimal change, prove GREEN, run affected regression suite, then commit.
- No task may claim runtime PASS from source tests, compilation, package inspection, or installation. Evidence gates remain distinct.
- A task that discovers a required user-facing behavior change stops and returns to design review rather than expanding scope silently.
- Refactoring is incremental. No all-at-once rewrite of `main.cpp`, archive storage, Replay Studio, or ghost rendering is permitted.

---

## Program Decomposition

This spec spans multiple independently reviewable subsystems, so implementation is intentionally split. Execute these plans in order:

1. `docs/superpowers/plans/2026-09-01-echo-dash-hardening-01-foundations-runtime-settings.md`
   - native test harness and invariant registry
   - semantic core types and time policy
   - immutable validated settings authority and typed diffs
   - explicit runtime state machine
   - prepared attempt/history commit boundary
   - `EchoRuntimeCoordinator` extraction and thin Geode hooks

2. `docs/superpowers/plans/2026-09-01-echo-dash-hardening-02-persistence-durability.md`
   - legacy schema-1 compatibility extraction
   - snapshot/journal wire formats
   - framed checksum-validated append-only commit journal
   - deterministic primary/backup/journal recovery
   - pending/durable/degraded revision semantics
   - retention/compaction maintenance boundaries
   - corruption, truncation, and write-failure fault tests

3. `docs/superpowers/plans/2026-09-01-echo-dash-hardening-03-playback-performance-ux.md`
   - one playback resolution per ghost per frame
   - cursor caches that never become authority
   - steady-state allocation and trail bounds
   - revision-driven overlays/structural UI
   - truthful `Rendering Quality`
   - Replay Studio immutable view state, responsive layout, disabled states, scrub ownership, and transactional open/close behavior

4. `docs/superpowers/plans/2026-09-01-echo-dash-hardening-04-diagnostics-integration.md`
   - structured local diagnostics and bounded breadcrumbs
   - subsystem health/recovery semantics
   - exactly-once lifecycle trace
   - vanilla-call contracts, reentrancy, teardown dominance, continuation liveness containment
   - classic/platformer/practice/dual callback-model tests and runtime trace hooks

5. `docs/superpowers/plans/2026-09-01-echo-dash-hardening-05-release-certification.md`
   - v1.1.3 release surfaces
   - immutable build/package manifest and hash chain
   - transactional ASCII-safe Windows installer with verified rollback
   - package inspection and PowerShell parse tests
   - immutable candidate promotion
   - install/runtime/stress/soak/upgrade certification evidence

## Program Gates

### Gate 0 — Baseline preservation

Before Plan 01 changes source behavior, record fresh baseline evidence on `main`:

```powershell
python -m unittest tests.test_v1_1_contract tests.test_release_assets -v
```

Expected: all current v1.1.2 source/package contract tests PASS. Record the terminal GitHub Actions run for the same source baseline; a local-only result is not a Windows build gate.

### Gate 1 — Foundations/runtime/settings

Plan 01 is complete only when:

- pure-core C++ tests run through CTest on Windows,
- the runtime state machine rejects illegal transitions without mutation,
- settings normalization/diff tests cover every exposed non-title setting,
- recorder-policy changes are proven next-attempt-only,
- attempt/history preparation is non-mutating until commit,
- `main.cpp` hooks dispatch to the coordinator rather than owning lifecycle policy,
- current behavior contracts and Windows Release build are terminal GREEN.

Do not start Plan 02 if the coordinator extraction changed user-visible behavior or produced unresolved runtime-lifecycle uncertainty that can be exercised before persistence work.

### Gate 2 — Persistence durability

Plan 02 is complete only when:

- schema-1 fixture loading remains GREEN,
- complete journal commits survive restart independently of snapshot compaction,
- partial journal tails are rejected without losing earlier commits,
- primary/backup/journal recovery follows the deterministic hierarchy,
- failed compaction leaves old authority intact,
- a complete attempt can be `PendingDurability` without publishing a partial summary/replay pair,
- retention and compaction are absent from latency-sensitive gameplay paths by contract,
- fault-injection tests and Windows Release build are terminal GREEN.

### Gate 3 — Playback/performance/UX

Plan 03 is complete only when:

- role metamorphic tests prove identical timing across Older/Last/Best/Last+Best,
- each selected ghost has at most one canonical playback-resolution operation per frame,
- prepared steady-state ghost update paths perform no ECHO_DASH-owned heap allocation,
- overlay and structural UI refreshes are revision-driven,
- all four existing Rendering Quality values are wired and affect presentation only,
- Replay Studio controls are a projection of immutable session view state,
- disabled actions cannot mutate session state,
- Studio open/close/viewport/scrub contracts pass,
- Windows Release build is terminal GREEN.

### Gate 4 — Diagnostics/integration

Plan 04 is complete only when:

- diagnostic events are bounded, sequenced, deduplicated, local, and read-only,
- health transitions close from degraded to recovered deterministically,
- duplicate death/reset/exit callbacks do not double-finalize or double-call vanilla operations,
- `Exiting` is terminal,
- continuation liveness containment releases optional presentation when vanilla lifecycle must proceed,
- platformer never receives classic progress synchronization authority,
- practice/dual callback-model tests pass,
- Windows Release build is terminal GREEN.

### Gate 5 — Release/certification

Plan 05 is complete only when the exact same immutable v1.1.3 candidate bytes have terminal evidence for:

`SOURCE -> CONTRACT -> BUILD -> PACKAGE -> INSTALL -> RUNTIME -> STRESS`

The release candidate is rejected if any byte changes after runtime testing. A rebuild becomes a new candidate and re-enters the appropriate earlier gate.

## Cross-Plan Review Rules

After every task-level commit:

```powershell
python -m unittest discover -s tests -p "test_*.py" -v
cmake --build build-core-tests --config Release
ctest --test-dir build-core-tests -C Release --output-on-failure
```

Use the CMake/CTest commands only after Plan 01 has created the native test target. Before that, run the Python regression suite only. At the end of every subplan, trigger the full pinned Windows GitHub Actions workflow and wait for terminal completion.

Reviewer checklist after each subplan:

- no feature-scope expansion,
- no second ghost timing authority,
- no role-specific timing behavior,
- no new archive mutation path outside persistence authority,
- no heavy maintenance newly introduced into `postUpdate`,
- no ignored important result that should be `[[nodiscard]]`,
- no user-data deletion during code upgrade paths,
- no PASS claim based on queued/running evidence.

## Final Definition of Done

The quality-hardening program is complete only when all five subplans are implemented and reviewed, the v1.1.3 immutable candidate passes the final certification matrix, and runtime evidence confirms the current feature set still behaves correctly on the pinned Geometry Dash/Geode target. The old v1.1.2 package remains the rollback baseline until v1.1.3 reaches terminal RUNTIME and STRESS PASS.