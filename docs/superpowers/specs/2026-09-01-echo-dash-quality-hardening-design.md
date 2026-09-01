# ECHO_DASH Quality Hardening Design

**Status:** LOCKED design authority; implementation not yet authorized by this document  
**Date:** 2026-09-01  
**Repository:** `doonchy16-cloud/DASH_ECHO`  
**Baseline main SHA:** `0339d698016ae5e9d6c67130bc6c910965d39a42`  
**Product name:** ECHO_DASH  
**Compatibility mod ID:** `doonchy.dash-echo`  
**Current certified integration target:** Geode 5.10.1 / Geometry Dash Windows 2.2081 / x86-64 / C++23

## 1. Purpose and scope

This design hardens the current ECHO_DASH product without expanding its user-facing feature set. The goal is to make every existing capability more reliable, deterministic, performant, understandable, maintainable, diagnosable, and safe to install and upgrade.

The feature set is frozen for this pass. The design does **not** authorize new ghost types, new analytics, new cameras, new replay capabilities, new gameplay overlays, new storage UI, spectator camera, new training features, new launcher controls, or other product-scope expansion.

The existing behavioral identity remains intact:

- ECHO_DASH records attempts and replays.
- Last Attempt remains blue.
- Best Recorded Echo remains gold.
- Ghost role affects presentation only, never playback timing.
- One shared ghost playback authority drives all ghosts.
- Replay Studio remains pause-menu-only; live gameplay remains free of a persistent launcher.
- GD Level PB, Best Recorded Echo, Session Best, and Latest Attempt remain distinct concepts.
- Existing archive/history, death visualization, heat strip, trails, settings, Replay Studio controls, camera modes, and diagnostics remain current capabilities.
- The internal mod ID remains `doonchy.dash-echo` to preserve saved settings/data compatibility.

The approved strategy is a **layered deep refactor with explicit contracts**, implemented incrementally behind behavior-preservation gates rather than as a single rewrite.

## 2. Global quality laws

The following rules apply across every subsystem:

1. Geometry Dash remains authoritative for gameplay state and lifecycle.
2. ECHO_DASH presentation may degrade; Geometry Dash must remain usable.
3. Exactly one ghost playback engine is authoritative for ghost timing.
4. Ghost role is presentation metadata only.
5. A finalized attempt has one immutable attempt identity end-to-end.
6. No subsystem may publish half-committed replay authority.
7. User replay data is preserved whenever recovery is reasonably possible.
8. Correctness work is mandatory; presentation work may be budgeted or deferred.
9. Diagnostics observe authority; they never become a second authority.
10. Every exposed setting must be defined, decoded, validated, applied, and tested.
11. Build, package, install, runtime, and stress evidence are distinct gates; no weaker gate implies a stronger one.
12. Refactoring must preserve current product behavior at each checkpoint.

## 3. Runtime architecture and lifecycle authority

### 3.1 Runtime coordinator

Introduce an `EchoRuntimeCoordinator` as the single ECHO_DASH lifecycle authority. Geometry Dash/Geode hooks become thin adapters that capture minimal vanilla facts and dispatch them to the coordinator.

The coordinator owns legal lifecycle transitions and orchestrates, but does not absorb, subsystem responsibilities. The major boundaries are:

- `EchoRecorder`: current-attempt capture and finalization.
- persistence/archive store: validated durable replay authority, retention, recovery, journal, snapshots.
- `EchoGhostPlaybackEngine`: canonical ghost playback timing only.
- `EchoGhostFleet`: replay selection references, render slots, pose application, trail presentation.
- `EchoReplaySession`: selected replay, cursor, play/pause, speed, replay camera state.
- `EchoReplayControls`: render a view state and emit commands only.
- analytics/overlays: derived data and presentation.
- `EchoRuntimeCoordinator`: lifecycle ordering and cross-subsystem transaction boundaries.

### 3.2 Explicit runtime state

Behavioral state is no longer inferred from combinations of booleans such as capture enabled, confirmed death, deferred reset requested, or Replay Studio open.

The lifecycle model includes explicit states equivalent to:

`Initializing -> Playing -> DeathContinuation -> ResetPending -> Resetting -> Playing`

with legal side paths for:

- `Playing <-> ReplayStudio`
- `Playing -> Completing -> Exiting`
- `Playing -> Exiting`

Replay Studio may be modeled as a modal/orthogonal substate internally if that produces a cleaner implementation, but the legal-transition semantics above are binding.

Illegal combinations are structurally prevented. Examples include recording while Replay Studio owns playback, archive mutation while unstable fleet references are held, resetting while ghost consumers still own invalidatable replay references, two active attempts, or two timing authorities.

### 3.3 Attempt finalization and commit terminology

The implementation must distinguish these stages explicitly:

1. **Active attempt** — mutable recorder-owned capture.
2. **Finalized attempt** — immutable complete attempt data.
3. **Prepared commit** — summary + replay pair validated as a complete logical unit.
4. **Durable commit** — the journal transaction has been written, flushed, framed, and checksum-validated.
5. **Published revision** — consumers can observe the complete logical unit together with its durability state.

A transient persistence failure may leave a complete finalized attempt retained in memory as **pending durability**, but no consumer may observe a partial summary/replay pair. The system must explicitly distinguish in-memory logical completeness from on-disk durability.

### 3.4 Idempotence and exactly-once rules

Attempt finalization, reset, completion, and exit must be exactly-once or idempotent. Duplicate callbacks cannot create duplicate archive records, increment attempt IDs twice, free resources twice, or invoke vanilla lifecycle operations twice.

## 4. Persistence and durability architecture

### 4.1 Snapshot + append-only journal

Replace per-attempt full-archive rewrite as the primary durability path with:

- primary snapshot,
- known-good backup snapshot,
- append-only transaction journal.

A finalized attempt is durably committed by appending one framed journal transaction and flushing it. Full snapshot compaction is maintenance, not the per-attempt critical path.

### 4.2 Transaction envelope

Each journal record has explicit framing sufficient to reject truncation and corruption before trusting payload contents. The envelope includes, at minimum, a format identifier, schema/version, transaction type, payload length, attempt identity, payload, checksum, and commit footer/end marker.

A partial trailing transaction is discarded during recovery; earlier valid transactions remain authoritative.

### 4.3 Startup recovery hierarchy

Startup recovery follows deterministic candidate rules:

1. Validate and load the primary snapshot when valid.
2. Otherwise validate and load the known-good backup.
3. Replay valid journal transactions newer than the selected snapshot.
4. If snapshots are unusable but the journal contains sufficient valid committed authority, rebuild as far as safely possible.
5. Reject/quarantine malformed units without poisoning unrelated valid replay data.
6. Only start empty when no recoverable authority remains.

Rejected files are preserved for forensic/recovery purposes before any repair operation. ECHO_DASH must never destroy potentially recoverable replay data merely because the current loader rejected it.

### 4.4 Compaction

Compaction writes a new snapshot to a temporary generation, validates it completely, then promotes it only after proof that it represents the intended committed state. If compaction fails, the old snapshot + journal remain authoritative.

Heavy compaction, backup rotation, full validation, and retention maintenance must stay out of latency-sensitive gameplay frames. They run only at safe lifecycle/maintenance opportunities. Background threading is not introduced merely for convenience; it is considered only if profiling later proves deterministic single-threaded maintenance insufficient and a separate approved design addresses concurrency risk.

### 4.5 Durability states

Persistence exposes structured states equivalent to:

- `Durable`
- `PendingDurability`
- `PersistenceDegraded`

Normal transient failures are retried/deferred without user spam. Repeated dangerous failures may produce one concise actionable warning. Diagnostics must distinguish memory revision, durable revision, and snapshot revision.

### 4.6 Retention consistency

Retention/eviction is an explicit maintenance transaction. Trimming may not leave history, replay storage, or persistence generations disagreeing about what exists. Protected/current records must obey existing product semantics.

## 5. UX and interaction hardening

### 5.1 UI is a projection of authoritative state

Replay Studio never owns semantic replay truth. `EchoReplaySession` publishes an immutable view state containing the current attempt, navigation availability, play/pause, normalized position, elapsed/duration, progress, playback rate, camera mode, GD PB, Best Recorded Echo, and Session Best. Controls render that state and emit commands.

The command flow is:

`UI command -> validate -> mutate replay session -> publish revision -> render new state`.

Individual handlers do not independently manufacture label state.

### 5.2 Replay Studio layout and hierarchy

Keep the same controls and capabilities, but reorganize them into stable conceptual layers:

1. identity/attempt,
2. truth metrics (GD PB / Best Recorded Echo / Session Best),
3. timeline,
4. transport/actions.

Use responsive panel bounds, margins, rows, and control groups instead of hard-coded magic coordinates. Click targets are larger than visual text labels where appropriate. Unavailable navigation/step actions are visibly disabled instead of accepting a click and doing nothing.

### 5.3 Refresh policy

Split continuously changing replay presentation (cursor/time/progress) from structural state (attempt identity, PB labels, speed, camera mode, navigation availability). Structural UI updates on revision changes; only the fast subset updates continuously while playback advances.

### 5.4 Slider and modal behavior

Slider seeking is clamped and finite-validated. Automatic playback does not fight an active user drag. A seek updates replay pose and viewport atomically.

Opening Replay Studio is transactional: validate a replay, capture gameplay viewport, enter the legal runtime state, suspend recording authority, hide normal ghosts, initialize replay, render Studio, then dismiss the vanilla PauseLayer. If Studio initialization fails, remain paused.

Closing Studio stops replay, restores the captured viewport when still valid, exits the Studio state, resumes the appropriate ghost/lifecycle behavior, and tears down Studio presentation. Scene exit dominates viewport restoration if underlying nodes are already gone.

### 5.5 Truthful controls and wording

Every exposed setting/control must have a demonstrable effect matching its name. User-facing language describes outcomes; diagnostics may use engineering terminology. Existing labels may be clarified without altering semantics or persisted identity.

Live gameplay remains visually sacred: no persistent ECHO_DASH launcher or normal diagnostic clutter is added.

## 6. Performance and determinism

### 6.1 Work tiers

Classify per-frame work into:

- **Tier 0 correctness-critical:** lifecycle, capture of required replay events, canonical ghost clock, confirmed-death handling, attempt commit boundaries.
- **Tier 1 interactive presentation:** ghost transforms, priority trails, Replay Studio cursor/viewport.
- **Tier 2 derived presentation:** heat/death overlay rebuilds, diagnostics formatting, nonessential label/derived-data refresh.

Frame pressure may defer Tier 2 presentation, never Tier 0 correctness.

### 6.2 Bounded hot path

The 0-256 ghost ceiling must be a proven runtime bound, not merely a container limit. Steady-state ghost rendering should avoid heap allocation where practical and must reuse prepared slots/buffers/scratch state.

Each active ghost gets at most one canonical playback-resolution result per frame. Pose and trail consumers reuse that result. Monotonic playback cursors/caches may accelerate lookups, but they are never authority and are invalidated on backward seek, attempt changes, reset, discontinuity, or any condition that breaks monotonic assumptions.

### 6.3 Revision-driven derived work

Death overlay, heat strip, structural Replay Studio UI, settings application, and other derived presentation rebuild only when their relevant revision changes. Diagnostics use bounded cadence and aggregates rather than scanning history every frame.

### 6.4 Timing policy

Centralize finite/negative/spike delta-time sanitation. Different clocks may have explicitly different semantics, but defensive time policy is documented and consistent. Use recorded absolute timestamps as authoritative replay positions where possible; continuation uses double precision, bounded delta time, and explicit anchors.

### 6.5 Rendering quality semantics

The existing `Rendering Quality` setting becomes truthful and affects **presentation cost only**. It never changes recorder sampling, replay bytes, attempt outcome, death analytics authority, or synchronization truth.

- `Full`: prioritize the existing maximum visual fidelity path.
- `Balanced`: reduce unnecessary presentation cost while preserving current semantics.
- `Performance`: minimize presentation cost within the existing visual feature set and bounded quality policies.
- `Auto`: select an effective presentation policy only after sustained measured pressure; use hysteresis/cooldown so it cannot oscillate rapidly.

Auto/quality policy may not change which replay is authoritative or invent role-specific timing. Exact thresholds are profiling-driven during implementation and certification, not guessed in this design.

### 6.6 Fault containment

Presentation failure cannot cascade into recorder or persistence failure. A failed trail node, heatmap node, ghost sprite allocation, or Replay Studio control surface degrades that presentation subsystem while preserving the rest of ECHO_DASH and Geometry Dash.

## 7. Correctness, tests, and adversarial verification

### 7.1 Invariant registry

Maintain one authoritative registry of project laws, including at minimum:

- exactly one active attempt while Playing,
- exactly one ghost timing authority,
- GhostRole cannot affect timing,
- Replay Studio cannot record gameplay,
- invalidatable archive storage cannot mutate while consumers hold unsafe references,
- complete durable transactions are never silently lost,
- UI is not semantic authority,
- rendering failure cannot corrupt recording,
- known-good persistence generation cannot be corrupted by a failed promotion,
- reset executes exactly once,
- attempt identity remains consistent across recorder/history/archive/analytics/replay,
- no ECHO_DASH state may permanently block vanilla reset/completion/pause/resume/exit.

Each invariant maps to a requirement and executable tests; high-risk invariants also receive runtime guards/assertions at ownership boundaries.

### 7.2 Behavioral contracts over implementation-shape tests

Tests should assert outcomes and legal transitions rather than only searching for function names or source strings. Structural tests remain useful for release/package constraints, but refactoring is protected primarily by behavioral contracts.

### 7.3 Lifecycle and event-order testing

Test all legal transitions and reject illegal transitions without authority mutation. Exercise duplicated and adversarial callback orders: duplicate deaths, duplicate reset requests, exit during continuation, level completion during deferred reset, Replay Studio during teardown, persistence failure followed by new attempts, and other valid-but-hostile sequences.

### 7.4 Replay and ghost determinism

Known replay fixtures must resolve to identical state for identical source times across repeated runs, seeking, restart, play/pause, and frame stepping. Metamorphic tests render the same replay under Older/Last/Best/Last+Best roles and prove timing/frame/interpolation/completion values remain identical; only presentation properties may differ.

### 7.5 Persistence fault injection and fuzzing

Test truncation at every transaction/snapshot stage, bad lengths, bad magic, bad schema, invalid sequence/timestamps, NaN coordinates, checksum mismatch, trailing garbage, duplicate IDs, summary/replay mismatch, oversized declarations, random bytes, and permission/write failures. Parsers must never crash, unbounded-allocate, or silently accept malformed authority.

Keep historical archive fixtures from supported previous releases so compatibility is executable evidence rather than memory.

### 7.6 Runtime certification and soak tests

Unit tests cannot certify Geode hooks. Runtime candidates receive repeatable tests for classic levels, dual player, platformer, practice/checkpoints, normal death/reset/completion/exit, Replay Studio open/close/scrub/frame/attempt navigation, restart persistence, and settings changes.

Stress/soak testing covers representative ghost counts through 256, high recorder sampling, dual player, existing overlays/trails, large archives, repeated pause/Studio/death/reset loops, and long sessions up to thousands of attempts where practical. Measure incremental ECHO_DASH overhead, memory, allocations, file handles, node counts, worst/p95/p99-like frame cost, and persistence maintenance cost.

## 8. Maintainability and internal APIs

### 8.1 Thin integration surface

`main.cpp`/Geode hooks become adapters, not policy controllers. Large functions are decomposed by architectural operation, not into meaningless micro-functions.

### 8.2 Narrow mutation APIs

Persistence and other authorities expose narrow mutation operations and read-only access. Dangerous ordering requirements are structuralized through coordinator operations and state guards rather than relying on comments or call-site memory.

### 8.3 Strong semantic values

Use lightweight semantic wrappers/value structures selectively where numeric confusion creates real risk, such as AttemptId, FrameSequence, ReplayTime, ProgressPercent, and NormalizedCursor. Avoid ceremony for low-risk values.

### 8.4 Immutable committed data and ownership

Finalized/committed replay data is immutable. Compression or transformation creates a new validated representation rather than editing committed frames in place. Every resource/node/reference has a documented owner and detach/invalidation rule.

### 8.5 Settings snapshots and diffs

All Geode setting reads are centralized into an immutable validated `EchoSettingsSnapshot`. Subsystems consume relevant sections rather than reading Geode globals directly. A typed settings diff determines exactly which subsystem reacts and at which lifecycle boundary.

### 8.6 Structured results/errors

Important operations return explicit result types rather than ambiguous booleans. Persistence load/commit, attachment, and lifecycle transitions expose meaningful outcomes. Important outcomes are `[[nodiscard]]` where appropriate.

Errors retain structured internal context: operation, attempt ID, runtime state, persistence stage, platform error, and recovery disposition. Every failure has a defined disposition: retry, defer, degrade, recover, quarantine, or abort transition.

### 8.7 Code hygiene

Audit dead fields, callbacks, enums, settings, helpers, and diagnostics. Wire or remove zombie surfaces. Normalize product vocabulary. Move historical version archaeology out of live logic comments; comments explain current invariants and why ordering exists. ECHO_DASH-owned code targets zero meaningful compiler warnings, separate from third-party SDK noise.

Refactor incrementally: contract current behavior, extract one responsibility, run tests/MSVC, then proceed to the next extraction.

## 9. Geometry Dash / Geode integration hardening

### 9.1 Vanilla-call contracts

Every hook documents whether the base Geometry Dash method runs before ECHO_DASH logic, after it, exactly once, or may be legally deferred. Hooks may not accidentally invoke vanilla lifecycle twice.

`destroyPlayer` remains observation-first: capture candidate facts, let vanilla determine actual death semantics, then commit terminal-death behavior only when validated.

### 9.2 Death continuation liveness

Death continuation may delay a vanilla reset only while the explicit continuation state owns that authority. It can never create a permanent deadlock.

Preserving Geometry Dash liveness outranks optional post-death ghost presentation. If continuation becomes provably unable to progress, playback state becomes invalid, level teardown begins, or a contradictory terminal vanilla transition arrives, ECHO_DASH releases presentation continuation safely, releases replay references, preserves/commits the attempt according to durability policy, and allows vanilla lifecycle to proceed.

This is not an arbitrary time-based ghost skip feature; failover is triggered by an actual broken-progress or terminal-lifecycle condition.

### 9.3 Reentrancy and teardown

Coordinator transition ownership prevents reentrant reset/finalization/rebuild/persist loops. `Exiting` is terminal: no new attempt, fleet rebuild, Replay Studio opening, death mutation, continuation delay, or nonessential settings application is allowed after entry.

Attach/detach operations are idempotent and tolerate partial construction. Missing optional Geometry Dash/Cocos nodes disable the relevant presentation safely; missing correctness-critical nodes abort/defer the ECHO operation without crashing the game.

### 9.4 Mode/context separation

Level/context changes are explicit transitions that release old consumers, preserve/flush old authority, load the new archive context, validate it, and start the new attempt.

Practice mode receives dedicated runtime certification because checkpoint callbacks can differ from normal mode. Platformer must never use classic percent progress as ghost synchronization authority. Dual-player callbacks become observations that the coordinator resolves into one attempt lifecycle.

### 9.5 Compatibility claim

Support claims are pinned to the exact Geode/GD integration versions actually compiled and runtime-certified. Supporting a new Geometry Dash or Geode version requires new integration evidence; compilation alone is insufficient.

## 10. Installation, upgrade, and release reliability

### 10.1 Immutable release artifact

The CI-built `.geode` is the canonical release artifact. Installers transport and verify it; they never modify or repack it.

The required hash chain is:

`CI .geode SHA256 == installer payload SHA256 == installed .geode SHA256`.

### 10.2 Release manifest

A machine-readable manifest is generated/verified from release authority and contains at least source commit, `.geode` hash, DLL hash, logo hash, mod ID, visible version, Geode target, GD target, architecture, and installer version. Installer and diagnostics consume the same manifest authority.

### 10.3 Transactional installer

Preflight validates the outer package, manifest, `.geode`, `mod.json`, DLL architecture, Geometry Dash/Geode target, running-game state, and current installation before mutation. Target discovery is deterministic: known existing Geode installation, Steam library information, standard path, then explicit valid user-selected path.

Conflict detection scans embedded mod IDs, not filenames. Before replacement, the existing ECHO_DASH package is hashed, backed up, and the backup is verified. Installation places the exact canonical package, verifies installed bytes and exactly one matching mod ID, then commits. Any post-mutation failure restores the prior verified installation byte-for-byte when one existed.

User replay/history data is never part of code-package cleanup. The stable internal mod ID remains unchanged so compatible settings/data survive upgrades.

### 10.4 Windows script contract and UX

Executable PowerShell/cmd assets remain compatible with the intended Windows runtime. Unless a newer PowerShell requirement is deliberately adopted in a future approved design, shipped scripts are ASCII-safe/encoding-validated and parse-tested in a compatible Windows environment. Progress displays real installation stages, not fake percentages.

Errors state what failed, whether the previous installation changed, and the next action. Diagnostic scripts are read-only and never auto-repair.

### 10.5 Immutable candidate promotion

Once a runtime candidate is designated, its bytes are frozen. Any byte change creates a new candidate. Promotion gates apply to the exact same candidate:

`SOURCE -> CONTRACT -> BUILD -> PACKAGE -> INSTALL -> RUNTIME -> STRESS`.

Rebuilding after runtime testing invalidates runtime certification for the new artifact.

## 11. Diagnostics, observability, and recoverability

### 11.1 Structured local diagnostics

Diagnostics remain local and bounded. No external telemetry, automatic upload, credential collection, or user-data analytics is required.

Significant events use a structured model containing monotonic sequence, timestamp, category, severity, runtime state, attempt ID when relevant, operation, result, and context. Categories include lifecycle, recorder, playback, ghost, archive, recovery, UI, settings, performance, and integration.

Log transitions and exceptional events, not per-frame ghost noise. Keep a bounded ring buffer of recent breadcrumbs.

### 11.2 Authority and health reporting

Diagnostics expose authoritative state rather than symptoms: runtime state, attempt identity, recorder state, ghost engine phase, archive revisions/durability/recovery/quarantine, Replay Studio state, requested/effective settings, and bounded performance aggregates.

Subsystem health is one of `Healthy`, `Degraded`, or `Unavailable`. Recovery transitions are logged separately from failures so successful recovery does not look like permanent failure.

Duplicate failures are rate-limited/deduplicated and closed by a recovery event when the condition heals.

### 11.3 Read-only doctrine

Diagnostics and self-checks observe; they do not repair authority. Turning diagnostics on/off cannot change replay results, ghost timing, persistence decisions, attempt outcomes, or lifecycle transitions. Diagnostic-node failure must have zero effect on mod authority.

Release/build identity is included so runtime evidence can be tied to exact source/package identity.

## 12. Configuration, defaults, and behavioral consistency

### 12.1 One effective configuration authority

All stored settings flow through:

`stored values -> decode -> validate -> normalize -> derive effective configuration -> immutable revision`.

Consumers receive validated effective values, not raw Geode values. Stored/requested values and effective values are distinct, especially where profiles cap ghost counts or Auto rendering quality chooses an effective policy.

Malformed/manual/legacy values are clamped or replaced by deterministic safe defaults. Normalization happens once at the settings boundary.

### 12.2 Application boundaries

Typed settings diffs classify consequences:

- presentation-only changes apply to the relevant renderer,
- fleet-structure changes rebuild at a legal boundary,
- recorder-policy changes take effect at the next attempt boundary,
- persistence-policy changes update maintenance policy and run heavy work only in maintenance windows,
- diagnostics changes affect diagnostics only.

An active attempt receives an immutable capture configuration. A recorder sample-rate change during an attempt applies to the next attempt so replay capture semantics remain internally coherent.

### 12.3 Defaults and dependencies

Defaults represent the recommended balanced experience, validated through runtime profiling rather than by maximizing every visual effect. Best Recorded Echo and Last Attempt remain visually dominant over older historical context.

Setting dependencies are explicit. Valid setting combinations must remain bounded and cannot create unbounded gameplay-loop complexity.

### 12.4 Migration and truthfulness

Existing recognized values preserve meaning through internal refactors. Unknown/malformed values fall back safely. Every exposed setting must complete the chain:

`DEFINED -> DECODED -> VALIDATED -> APPLIED -> TESTED`.

Descriptions explain consequences and application timing. The UI must not imply immediate application when a setting intentionally takes effect at the next attempt or maintenance boundary.

## 13. Data flow summary

### Live attempt

`Geometry Dash callbacks -> thin hooks -> EchoRuntimeCoordinator -> EchoRecorder -> FinalizedAttempt -> PreparedCommit -> journal durable commit -> published archive/history revision -> ghost/replay consumers`

### Live ghost playback

`Geometry Dash live time/progress -> EchoRuntimeCoordinator -> one EchoGhostPlaybackEngine -> one resolution per ghost -> EchoGhostFleet presentation`

Role metadata is applied only after timing resolution.

### Replay Studio

`Pause entry -> coordinator transition -> EchoReplaySession -> immutable view state -> EchoReplayControls`

Control commands return through the session/coordinator; UI does not directly own gameplay/replay authority.

### Settings

`Geode settings -> settings decoder/validator -> immutable EchoSettingsSnapshot -> typed diff -> legal subsystem application boundary`.

### Recovery

`primary snapshot / backup snapshot + journal -> validators -> deterministic candidate/replay recovery -> published complete authority + explicit recovery/quarantine health state`.

## 14. Explicit non-goals

This hardening project does not authorize:

- spectator camera,
- new camera modes,
- new replay controls,
- new ghost roles/types,
- new analytics or training intelligence,
- new UI launchers or gameplay overlays,
- new archive browsing/storage features,
- new network/cloud telemetry,
- automatic external reporting,
- a visible LOD feature that changes ghost selection semantics,
- role-specific gameplay speed or timing,
- changing the stable internal mod ID,
- changing authoritative recorder/replay data merely to improve rendering performance.

Internal persistence format evolution is allowed only as necessary to implement the locked transactional durability design and must preserve supported prior data through tested migration/recovery fixtures.

## 15. Implementation order and containment strategy

This document is design authority, not an implementation plan. The later implementation plan must decompose work into small behavior-preserving phases with verification between them. The preferred dependency order is:

1. freeze/expand behavioral invariants and fixtures around current behavior,
2. settings snapshot/diff and explicit runtime-state foundations,
3. coordinator extraction and thin hooks,
4. attempt finalization/commit boundary,
5. journal + snapshot persistence with migration/fault tests,
6. replay/ghost hot-path and revision-driven performance work,
7. Replay Studio view-state/layout/interaction hardening,
8. diagnostics/health integration,
9. transactional installer/release manifest hardening,
10. full Windows build, package, installer, runtime, stress, upgrade, and soak certification.

The implementation plan may subdivide these phases further but may not skip behavior-preservation gates or expand product scope.

## 16. Acceptance doctrine

The hardening pass is complete only when evidence demonstrates that the existing feature set remains behaviorally correct while the new quality contracts are satisfied.

Minimum evidence categories:

- design/spec authority approved,
- contract/unit/property/fuzz tests pass,
- Windows MSVC build passes with ECHO_DASH-owned warnings addressed,
- release package inspection/hash chain passes,
- installer preflight/upgrade/rollback matrix passes,
- runtime lifecycle matrix passes on the pinned GD/Geode target,
- restart persistence and supported-upgrade fixtures pass,
- ghost role-neutrality and continuation behavior pass at runtime,
- performance/stress/soak evidence is recorded for representative ghost counts through the supported 256 ceiling,
- no known illegal lifecycle transition, duplicate finalization, unrecovered data-corruption path, or vanilla-liveness deadlock remains.

A queued/running workflow, successful compile, successful package, or successful install does not imply runtime or stress PASS. Each evidence level must be terminal and explicit.

## 17. Locked design summary

ECHO_DASH will be refactored toward an explicit runtime coordinator, legal state machine, immutable attempt/commit boundaries, transactional journaled persistence, deterministic recovery, responsive authoritative-state UX, bounded revision-driven rendering, one canonical ghost timing engine, narrow independently testable APIs, thin Geometry Dash hooks, local structured diagnostics, validated typed settings, and immutable hash-verified release artifacts.

The player receives the same current ECHO_DASH capabilities, but those capabilities become substantially harder to corrupt, stall, misconfigure, misdiagnose, regress, or install incorrectly.