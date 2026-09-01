# ECHO_DASH Quality Hardening Design

**Status:** LOCKED design authority; implementation not yet authorized by this document  
**Date:** 2026-09-01  
**Repository:** `doonchy16-cloud/DASH_ECHO`  
**Baseline main SHA before this design:** `0339d698016ae5e9d6c67130bc6c910965d39a42`  
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
6. No subsystem may publish a partial summary/replay pair or otherwise half-committed replay authority.
7. User replay data is preserved whenever recovery is reasonably possible.
8. Correctness work is mandatory; presentation work may be budgeted or deferred.
9. Diagnostics observe authority; they never become a second authority.
10. Every exposed setting must be defined, decoded, validated, applied, and tested.
11. Build, package, install, runtime, and stress evidence are distinct gates; no weaker gate implies a stronger one.
12. Refactoring must preserve current product behavior at each checkpoint.
13. No ECHO_DASH state may indefinitely block vanilla reset, completion, pause, resume, or exit.

## 3. Runtime architecture and lifecycle authority

### 3.1 Runtime coordinator

Introduce an `EchoRuntimeCoordinator` as the single ECHO_DASH lifecycle authority. Geometry Dash/Geode hooks become thin adapters that capture minimal vanilla facts and dispatch them to the coordinator.

The coordinator owns legal lifecycle transitions and orchestrates, but does not absorb, subsystem responsibilities. The major boundaries are:

- `EchoRecorder`: current-attempt capture and finalization.
- persistence/archive store: validated durable replay authority, retention, recovery, journal, snapshots.
- `EchoGhostPlaybackEngine`: canonical ghost playback timing only.
- `EchoGhostFleet`: replay-selection references, render slots, pose application, trail presentation.
- `EchoReplaySession`: selected replay, cursor, play/pause, speed, replay camera state.
- `EchoReplayControls`: render a view state and emit commands only.
- analytics/overlays: derived data and presentation.
- `EchoRuntimeCoordinator`: lifecycle ordering and cross-subsystem transaction boundaries.

A developer must be able to understand/test persistence without reading PlayLayer code, ghost timing without rendering code, and Replay Studio UI without archive mutation logic.

### 3.2 Explicit runtime state

Behavioral state is no longer inferred from combinations of booleans such as capture enabled, confirmed death, deferred reset requested, or Replay Studio open.

The lifecycle model includes explicit states equivalent to:

`Initializing -> Playing -> DeathContinuation -> ResetPending -> Resetting -> Playing`

with legal side paths for:

- `Playing <-> ReplayStudio`
- `Playing -> Completing -> Exiting`
- `Playing -> Exiting`

`ResetPending` means vanilla has requested reset but ECHO_DASH has not yet legally invoked the real reset. If ghost continuation is still valid, continuation authority may remain active while in this pending state until it completes or is safely aborted by the liveness policy. `Resetting` means the coordinator has committed to invoking the vanilla reset exactly once.

Replay Studio may be represented internally as a modal/orthogonal substate if that yields cleaner code, but the legal-transition semantics above remain binding.

Illegal combinations are structurally prevented. Examples include recording while Replay Studio owns playback, archive mutation while unstable fleet references are held, resetting while ghost consumers still own invalidatable replay references, two active attempts, or two timing authorities.

### 3.3 Attempt finalization and commit terminology

The implementation must distinguish these stages explicitly:

1. **Active attempt** — mutable recorder-owned capture.
2. **Finalized attempt** — immutable complete attempt data.
3. **Prepared commit** — summary + replay pair validated as one complete logical unit.
4. **Durable commit** — the journal transaction has been written, flushed, framed, and checksum-validated.
5. **Published revision** — consumers can observe the complete logical unit together with its explicit durability state.

A transient persistence failure may leave a complete finalized/prepared attempt retained in memory as **PendingDurability**, but no consumer may observe only one half of the summary/replay authority. A complete in-memory logical revision may be visible if its durability state is also truthful; it must never be represented as safely persisted until the durable revision proves that fact.

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

Semantic replay validation remains record-scoped where possible: a bad replay is quarantined without invalidating unrelated valid replays. Structural corruption that makes a candidate unsafe causes fallback to the next recovery authority.

### 4.4 Compaction

Compaction writes a new snapshot to a temporary generation, validates it completely, then promotes it only after proof that it represents the intended committed state. If compaction fails, the old snapshot + journal remain authoritative.

Heavy compaction, backup rotation, full validation, and retention maintenance must stay out of latency-sensitive gameplay frames. They run only at safe lifecycle/maintenance opportunities such as pause, level/context transition, Replay Studio idle time, menu/exit boundaries, or another explicitly budgeted safe window.

Background threading is not introduced merely for convenience. It is considered only if profiling later proves deterministic single-threaded maintenance insufficient and a separate approved design addresses concurrency, ownership, cancellation, and shutdown risk.

### 4.5 Durability states

Persistence exposes structured states equivalent to:

- `Durable`
- `PendingDurability`
- `PersistenceDegraded`

Normal transient failures are retried/deferred without user spam. Repeated dangerous failures may produce one concise actionable warning. Diagnostics must distinguish memory revision, durable revision, and snapshot revision.

A save/journal/compaction failure must have a defined disposition: retry, defer, degrade, recover, quarantine, or abort a transition. Logging and continuing without a defined safe policy is not acceptable.

### 4.6 Retention consistency

Retention/eviction is an explicit maintenance transaction. Trimming may not leave history, replay storage, or persistence generations disagreeing about what exists. Protected/current records must obey existing product semantics.

Deletion is never an incidental side effect of a failed write. If maintenance fails, the previous durable generation remains the authority.

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

The layout must be checked across unusual aspect ratios and UI scales so labels, transport controls, timeline, and close/settings actions do not overlap or leave the panel.

### 5.3 Refresh policy

Split continuously changing replay presentation (cursor/time/progress) from structural state (attempt identity, PB labels, speed, camera mode, navigation availability). Structural UI updates on revision changes; only the fast subset updates continuously while playback advances.

### 5.4 Slider and modal behavior

Slider seeking is clamped and finite-validated. Automatic playback does not fight an active user drag. A seek updates replay pose and viewport atomically.

Opening Replay Studio is transactional: validate a replay, capture gameplay viewport, enter the legal runtime state, suspend recording authority, hide normal ghosts, initialize replay, render Studio, then dismiss the vanilla PauseLayer. If Studio initialization fails, remain paused.

Closing Studio stops replay, restores the captured viewport when still valid, exits the Studio state, resumes the appropriate ghost/lifecycle behavior, and tears down Studio presentation. Scene exit dominates viewport restoration if underlying nodes are already gone.

Replay Studio must be input-modal with respect to gameplay actions it replaces while open; runtime certification must verify that non-control input does not leak into live gameplay authority.

### 5.5 Truthful controls and wording

Every exposed setting/control must have a demonstrable effect matching its name. User-facing language describes outcomes; diagnostics may use engineering terminology. Existing labels may be clarified without altering semantics or persisted identity.

Examples of acceptable wording cleanup include outcome-oriented names such as `Saved Replay Limit` instead of implementation-heavy wording, provided compatibility/storage keys remain stable.

Live gameplay remains visually sacred: no persistent ECHO_DASH launcher or normal diagnostic clutter is added.

## 6. Performance and determinism

### 6.1 Work tiers and frame budget

Classify per-frame work into:

- **Tier 0 correctness-critical:** lifecycle, capture of required replay events, canonical ghost clock, confirmed-death handling, attempt commit boundaries.
- **Tier 1 interactive presentation:** ghost transforms, priority trails, Replay Studio cursor/viewport.
- **Tier 2 derived presentation:** heat/death overlay rebuilds, diagnostics formatting, nonessential label/derived-data refresh.

Each frame executes mandatory correctness work first, then required interactive presentation, then only as much derived presentation/maintenance as the current bounded budget permits. Frame pressure may defer Tier 2 presentation, never Tier 0 correctness.

### 6.2 Bounded ghost hot path

The 0-256 ghost ceiling must be a proven runtime bound, not merely a container limit. After a fleet is prepared, the steady-state ghost resolution/render loop must perform **zero heap allocation per frame** from ECHO_DASH-owned hot-path code. Slots, player objects, trail buffers, scratch memory, and lookup state are reused.

Each active ghost gets at most one canonical playback-resolution result per frame. Pose and trail consumers reuse that result. Monotonic playback cursors/caches may accelerate lookups, but they are never authority and are invalidated on backward seek, attempt changes, reset, discontinuity, progress regression, or any condition that breaks monotonic assumptions.

Replay/archive navigation also gains internal indexes/caches where needed so latest/best/previous/next lookup does not become an avoidable full-history scan in latency-sensitive paths.

### 6.3 Trails and derived presentation

Priority trails remain bounded by existing visual semantics. Prefer incremental window maintenance if it is simpler and demonstrably correct; if Cocos draw-node behavior makes rebuilding safer, retain rebuilding with a strict segment cap and proven bounded cost. Predictability outranks clever optimization.

Death overlay, heat strip, structural Replay Studio UI, settings application, and other derived presentation rebuild only when their relevant revision changes. Diagnostics use bounded cadence and aggregates rather than scanning history every frame.

### 6.4 Timing policy

Centralize finite/negative/spike delta-time sanitation. Different clocks may have explicitly different semantics, but defensive time policy is documented and consistent. Use recorded absolute timestamps as authoritative replay positions where possible; continuation uses double precision, bounded delta time, and explicit anchors.

Visuals remain read-only consumers: replay/ghost/trail/heat/diagnostic rendering may never alter GD physics, collision, timers, recorded progress, attempt result, or durable replay authority.

### 6.5 Rendering quality semantics

The existing `Rendering Quality` setting becomes truthful and affects **presentation cost only**. It never changes recorder sampling, replay bytes, attempt outcome, death analytics authority, or synchronization truth.

- `Full`: prioritize the existing maximum visual fidelity path.
- `Balanced`: reduce unnecessary presentation cost while preserving current semantics.
- `Performance`: aggressively minimize presentation cost within the existing visual feature set and bounded quality policies.
- `Auto`: select an effective presentation policy only after sustained measured pressure; use hysteresis/cooldown so it cannot oscillate rapidly.

Auto/quality policy may tune existing presentation budgets/refresh costs but may not change replay selection semantics, introduce a new visible ghost-LOD feature, or invent role-specific timing. Exact pressure/recovery thresholds are determined from profiling and then locked in implementation tests/certification rather than guessed in this design.

### 6.6 Fault containment

Presentation failure cannot cascade into recorder or persistence failure. A failed trail node, heatmap node, ghost sprite allocation, or Replay Studio control surface degrades that presentation subsystem while preserving the rest of ECHO_DASH and Geometry Dash.

Examples:

- failed trail node -> ghosts/recording/archive remain operational,
- one ghost sprite allocation failure -> reduce safely rendered presentation slots without corrupting selection/timing authority,
- failed heatmap -> death analytics still records,
- failed Replay Studio controls -> recording/gameplay remain operational and the pause entry fails safely.

### 6.7 Performance evidence

Runtime performance certification measures incremental ECHO_DASH cost rather than pretending the mod controls total Geometry Dash performance. Representative matrices include 1, 8, 16, 64, 128, and 256 ghosts and record CPU/frame cost, ghost-resolution cost, presentation cost, allocations/frame, memory, persistence-maintenance duration, worst frame, and rolling p95/p99-like cost.

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

Tests assert outcomes and legal transitions rather than only searching for function names or source strings. Structural tests remain useful for release/package constraints, but refactoring is protected primarily by behavioral contracts.

### 7.3 Lifecycle and event-order testing

Test all legal transitions and reject illegal transitions without authority mutation. Exercise duplicated and adversarial callback orders: duplicate deaths, duplicate reset requests, duplicate exit/finalization signals, exit during continuation, level completion during deferred reset, Replay Studio during teardown, persistence failure followed by new attempts, and other valid-but-hostile sequences.

Attempt identity is immutable across recorder, death analytics, history, archive, ghost fleet, and Replay Studio. A mismatched summary/replay/attempt identity is rejected rather than silently reconciled by guessing.

### 7.4 Replay and ghost determinism

Known replay fixtures resolve to identical state for identical source times across repeated runs, seeking, restart, play/pause, and frame stepping. Metamorphic tests render the same replay under Older/Last/Best/Last+Best roles and prove timing/frame/interpolation/completion values remain identical; only presentation properties may differ.

Timing abuse covers zero, negative, NaN, infinity, tiny, and large delta values. Progress abuse covers negative, NaN, flat, duplicate, 0%, 100%, and slightly out-of-range values. Platformer may not accidentally engage classic-progress authority.

### 7.5 Persistence fault injection and fuzzing

Test truncation at every transaction/snapshot stage, bad lengths, bad magic, bad schema, invalid sequence/timestamps, NaN coordinates, checksum mismatch, trailing garbage, duplicate IDs, summary/replay mismatch, oversized declarations, random bytes, permission/write failures, and crash points around snapshot promotion/backup rotation. Parsers must never crash, unbounded-allocate, or silently accept malformed authority.

Keep historical archive fixtures from supported previous releases so compatibility is executable evidence rather than memory.

### 7.6 Runtime certification and soak tests

Unit tests cannot certify Geode hooks. Runtime candidates receive repeatable tests for classic levels, dual player, platformer, practice/checkpoints, normal death/reset/completion/exit, Replay Studio open/close/scrub/frame/attempt navigation, restart persistence, and settings changes.

Stress/soak testing covers representative ghost counts through 256, 240 Hz recording, dual player, existing overlays/trails, large archives, repeated pause/Studio/death/reset loops, and long sessions up to thousands of attempts where practical. Track memory, file handles, attached nodes, object cleanup, active-attempt count, stale references, and performance drift over time.

### 7.7 Evidence levels

Evidence labels remain explicit and non-interchangeable:

- DESIGN PASS
- SOURCE PASS
- CONTRACT PASS
- BUILD PASS
- PACKAGE PASS
- INSTALL PASS
- RUNTIME PASS
- STRESS PASS

A queued/running workflow is never PASS. Compiler green is not runtime proof; one successful runtime attempt is not stress proof.

## 8. Maintainability and internal APIs

### 8.1 Thin integration surface

`main.cpp`/Geode hooks become adapters, not policy controllers. Large functions are decomposed by architectural operation, not into meaningless micro-functions.

### 8.2 Narrow mutation APIs

Persistence and other authorities expose narrow mutation operations and read-only access. Dangerous ordering requirements are structuralized through coordinator operations and state guards rather than relying on comments or call-site memory.

Where an operation requires a safe consumer-release boundary, the API/coordinator sequence must make that ordering explicit instead of requiring callers to remember a `stop -> mutate -> rebuild` convention.

### 8.3 Strong semantic values

Use lightweight semantic wrappers/value structures selectively where numeric confusion creates real risk, such as AttemptId, FrameSequence, ReplayTime, ProgressPercent, and NormalizedCursor. Avoid ceremony for low-risk values.

### 8.4 Immutable committed data and ownership

Finalized/committed replay data is immutable. Compression or transformation creates a new validated representation rather than editing committed frames in place. Every resource/node/reference has a documented owner and detach/invalidation rule.

### 8.5 Settings snapshots and diffs

All Geode setting reads are centralized into an immutable validated `EchoSettingsSnapshot`. Subsystems consume relevant sections rather than reading Geode globals directly. A typed settings diff determines exactly which subsystem reacts and at which lifecycle boundary.

### 8.6 Structured results/errors

Important operations return explicit result types rather than ambiguous booleans. Persistence load/commit, attachment, and lifecycle transitions expose meaningful outcomes. Important outcomes are `[[nodiscard]]` where appropriate.

Errors retain structured internal context: operation, attempt ID, runtime state, persistence stage, platform error, and recovery disposition. Every failure has a defined disposition: retry, defer, degrade, recover, quarantine, or abort transition.

### 8.7 Code hygiene and complexity

Audit dead fields, callbacks, enums, settings, helpers, and diagnostics. Wire or remove zombie surfaces. Normalize product vocabulary around GD Level PB, Best Recorded Echo, Session Best, Last Attempt, Historical Ghost, Replay Studio, and Death Continuation.

Move historical version archaeology out of live logic comments; comments explain current invariants and why ordering exists. ECHO_DASH-owned code targets zero meaningful compiler warnings, separate from third-party SDK noise.

Large files such as `main.cpp`, persistence, and fleet code are reviewed for responsibility boundaries. File size alone is not a failure, but a file owning multiple distinct architectural roles triggers decomposition review.

Refactor incrementally: contract current behavior, extract one responsibility, run tests/MSVC, then proceed to the next extraction.

### 8.8 First-class fixtures

Maintain canonical fixtures for short/long classic attempts, dual, platformer, death, completion, discontinuity, historical archives, and corrupted archive variants. Subsystems are independently testable without requiring a live Geometry Dash window where the behavior does not intrinsically depend on hooks/rendering.

## 9. Geometry Dash / Geode integration hardening

### 9.1 Vanilla-call contracts

Every hook documents whether the base Geometry Dash method runs before ECHO_DASH logic, after it, exactly once, or may be legally deferred. Hooks may not accidentally invoke vanilla lifecycle twice.

`destroyPlayer` remains observation-first: capture candidate facts, let vanilla determine actual death semantics, then commit terminal-death behavior only when validated. A destroy callback first becomes a player-death observation; the coordinator decides whether it represents the terminal attempt death, including dual-player cases.

### 9.2 Death continuation liveness

Death continuation may delay a vanilla reset only while the explicit continuation/reset-pending state owns that authority. It can never create a permanent deadlock.

Preserving Geometry Dash liveness outranks optional post-death ghost presentation. If continuation becomes provably unable to progress, playback state becomes non-finite/invalid, level teardown begins, or a contradictory terminal vanilla transition arrives, ECHO_DASH releases presentation continuation safely, releases replay references, preserves/commits the attempt according to durability policy, and allows vanilla lifecycle to proceed.

This is not an arbitrary time-based ghost skip feature; failover is triggered by an actual broken-progress or terminal-lifecycle condition.

### 9.3 Reentrancy and teardown

Coordinator transition ownership prevents reentrant reset/finalization/rebuild/persist loops. `Exiting` is terminal: no new attempt, fleet rebuild, Replay Studio opening, death mutation, continuation delay, or nonessential settings application is allowed after entry.

Attach/detach operations are idempotent and tolerate partial construction. Missing optional Geometry Dash/Cocos nodes disable the relevant presentation safely; missing correctness-critical nodes abort/defer the ECHO operation without crashing the game.

Scene teardown dominates all optional presentation. ECHO_DASH may not delay destruction because ghosts or Replay Studio have unfinished presentation work.

### 9.4 Mode/context separation

Level/context changes are explicit transitions that release old consumers, preserve/flush old authority, load the new archive context, validate it, and start the new attempt.

Practice mode receives dedicated runtime certification because checkpoint callbacks can differ from normal mode. Platformer must never use classic percent progress as ghost synchronization authority. Dual-player callbacks become observations that the coordinator resolves into one attempt lifecycle.

### 9.5 Pause/viewport integration

Pause -> Replay Studio is transactional. Studio must initialize successfully before vanilla PauseLayer is dismissed. Viewport ownership is scoped: capture exact object-layer transform, let Replay Studio temporarily own it, restore it on normal close, and abandon restoration safely if the scene/object layer has already been destroyed.

### 9.6 Compatibility and callback tracing

Support claims are pinned to the exact Geode/GD integration versions actually compiled and runtime-certified. Supporting a new Geometry Dash or Geode version requires new integration evidence; compilation alone is insufficient.

Diagnostics builds record a bounded coordinator callback/transition trace so runtime failures can be reconstructed, for example death -> continuation -> reset requested -> continuation complete -> resetting -> playing.

## 10. Installation, upgrade, and release reliability

### 10.1 Immutable release artifact

The CI-built `.geode` is the canonical release artifact. Installers transport and verify it; they never modify or repack it.

The required hash chain is:

`CI .geode SHA256 == installer payload SHA256 == installed .geode SHA256`.

### 10.2 Release manifest

A machine-readable manifest is generated/verified from release authority and contains at least source commit, `.geode` hash, DLL hash, logo hash, mod ID, visible version, Geode target, GD target, architecture, and installer version. Installer and diagnostics consume the same manifest authority.

### 10.3 Transactional installer

Preflight validates the outer package, manifest, `.geode`, `mod.json`, DLL architecture, Geometry Dash/Geode target, running-game state, and current installation before mutation. If preflight fails, zero installed files are changed.

Target discovery is deterministic: known existing Geode installation, Steam library information, standard Steam path, then explicit valid user-selected path. A candidate target must prove it is a real Geometry Dash + Geode installation before mutation.

Conflict detection scans embedded mod IDs, not filenames. Before replacement, the existing ECHO_DASH package is hashed, backed up, and the backup is verified. Installation prepares/verifies the new file, removes only matching mod-ID conflicts, places the exact canonical package, verifies installed bytes and exactly one matching mod ID, then commits. Any post-mutation failure restores the prior verified installation byte-for-byte when one existed.

User replay/history data is never part of code-package cleanup. The stable internal mod ID remains unchanged so compatible settings/data survive upgrades.

### 10.4 Predictable package structure

Release ZIPs use one stable structure and naming convention so installation is not relearned every release. The implementation plan should preserve the established layout pattern with:

- upgrade `.cmd` + `.ps1`,
- read-only diagnose `.cmd` + `.ps1`,
- `payload/` containing the exact `.geode` and release manifest,
- README/instructions,
- checksums.

No installer script may silently mutate the `.geode` payload.

### 10.5 Windows script contract and UX

Executable PowerShell/cmd assets remain compatible with the intended Windows runtime. Unless a newer PowerShell requirement is deliberately adopted in a future approved design, shipped scripts are ASCII-safe/encoding-validated, use CRLF where appropriate, and are parse-tested in a compatible Windows environment.

Installer UX remains animated/alive but professional and truthful. ASCII-safe spinner/progress characters such as `| / - \\`, `#`, and `.` may accompany real stages. Stage progress is preferred over fake numeric percentages when exact percentage completion is not measurable.

Errors state: what failed, whether the previous installation changed, and the next action. Diagnostic scripts are read-only and never auto-repair.

### 10.6 Automated package inspection and installer matrix

Before distribution, automation verifies:

- outer ZIP integrity and expected structure,
- script parse/encoding validity,
- manifest integrity,
- `.geode` ZIP integrity,
- correct mod ID, visible name, version, Geode/GD targets, DLL architecture, and icon,
- expected/no-unexpected binaries,
- manifest/hash chain consistency.

Installer certification covers at minimum: no prior ECHO_DASH, prior same version, prior older version, duplicate matching mod packages, Geometry Dash running, unwritable destination, bad payload hash, bad manifest, corrupt existing mod, injected copy/post-verify failure with rollback, filesystem paths with spaces, standard Steam install, and alternate Steam library.

### 10.7 Reproducibility and immutable candidate promotion

Track semantic reproducibility separately from byte reproducibility. At minimum, the same pinned source/toolchain must produce matching metadata/architecture/inputs with retained compiler evidence. Bit-for-bit reproducibility is claimed only if it is actually demonstrated.

Once a runtime candidate is designated, its bytes are frozen. Any byte change creates a new candidate. Promotion gates apply to the exact same candidate:

`SOURCE -> CONTRACT -> BUILD -> PACKAGE -> INSTALL -> RUNTIME -> STRESS`.

Rebuilding after runtime testing invalidates runtime certification for the new artifact.

## 11. Diagnostics, observability, and recoverability

### 11.1 Structured local diagnostics

Diagnostics remain local and bounded. No external telemetry, automatic upload, credential collection, network analytics, or unrelated personal/filesystem data collection is required.

Significant events use a structured model containing monotonic sequence, timestamp, category, severity, runtime state, attempt ID when relevant, operation, result, and context. Categories include lifecycle, recorder, playback, ghost, archive, recovery, UI, settings, performance, and integration.

Log transitions and exceptional events, not per-frame ghost noise. Keep a bounded ring buffer of recent breadcrumbs. Important breadcrumbs are recorded before dangerous transitions where possible so crash-adjacent evidence indicates the last known operation.

### 11.2 Authority and health reporting

Diagnostics expose authoritative state rather than symptoms: runtime state, attempt identity, recorder state, ghost engine phase, archive revisions/durability/recovery/quarantine, Replay Studio state, requested/effective settings, and bounded performance aggregates.

Subsystem health is one of `Healthy`, `Degraded`, or `Unavailable`. Recovery transitions are logged separately from failures so successful recovery does not look like permanent failure.

Duplicate failures are rate-limited/deduplicated and closed by a recovery event when the condition heals.

### 11.3 Persistence and ghost observability

Persistence diagnostics distinguish memory revision, durable revision, snapshot revision, recovery source, and quarantine count. This makes normal uncompacted journal state distinguishable from pending or degraded durability.

Ghost diagnostics separate timing authority from presentation role. Bounded detail may show engine phase, live elapsed/progress, progress-authority status, continuation elapsed, active/finished counts, and a selected ghost's attempt/source-time/frame-cursor resolution without dumping all ghosts every frame.

### 11.4 Effective settings and performance aggregates

Diagnostics show requested versus effective values where they differ, such as requested ghost count versus profile-capped count or requested Auto quality versus the effective quality policy and reason. Performance uses bounded rolling aggregates/current/worst values rather than an event per frame.

### 11.5 Read-only doctrine

Diagnostics and self-checks observe; they do not repair authority. Turning diagnostics on/off cannot change replay results, ghost timing, persistence decisions, attempt outcomes, or lifecycle transitions. Diagnostic-node failure must have zero effect on mod authority.

Release/build identity is included so runtime evidence can be tied to exact source/package identity. A concise end-of-session summary reports attempts started/finalized, persistence revisions/pending state/recovery/quarantine, peak ghost state where useful, and illegal-transition count.

## 12. Configuration, defaults, and behavioral consistency

### 12.1 One effective configuration authority

All stored settings flow through:

`stored values -> decode -> validate -> normalize -> derive effective configuration -> immutable revision`.

Consumers receive validated effective values, not raw Geode values. Stored/requested values and effective values are distinct, especially where profiles cap ghost counts or Auto rendering quality chooses an effective policy.

Malformed/manual/legacy values are clamped or replaced by deterministic safe defaults. Normalization happens once at the settings boundary. Consumers may rely on validated invariants such as finite values, legal opacity/rate/count ranges, and known enum values.

### 12.2 Application boundaries

Typed settings diffs classify consequences:

- presentation-only changes apply to the relevant renderer,
- fleet-structure changes rebuild at a legal boundary,
- recorder-policy changes take effect at the next attempt boundary,
- persistence-policy changes update maintenance policy and run heavy work only in maintenance windows,
- diagnostics changes affect diagnostics only.

An active attempt receives an immutable capture configuration. A recorder sample-rate change during an attempt applies to the next attempt so replay capture semantics remain internally coherent.

### 12.3 Defaults and dependencies

Defaults represent the recommended balanced experience, validated through runtime profiling rather than by maximizing every visual effect. The current default system — including ghost count/profile, priority Last/Best presentation, trails, death visualization, 120 Hz recording, archive limits, and Auto rendering quality — is evaluated as one experience rather than setting-by-setting in isolation.

Best Recorded Echo and Last Attempt remain visually dominant over older historical context. Older ghosts must not collectively overpower the priority traces merely because more are visible.

Setting dependencies are explicit. Valid setting combinations must remain bounded and cannot create unbounded gameplay-loop complexity.

### 12.4 Migration and truthfulness

Existing recognized values preserve meaning through internal refactors. Unknown/malformed values fall back safely. Internal configuration schema/versioning makes migrations deterministic.

Every exposed setting must complete the chain:

`DEFINED -> DECODED -> VALIDATED -> APPLIED -> TESTED`.

Descriptions explain consequences and application timing. The UI must not imply immediate application when a setting intentionally takes effect at the next attempt or maintenance boundary. Reset-to-defaults travels through the same decode/validate/diff/application path rather than a hidden special path.

### 12.5 Configuration verification

Test representative matrices for ghost counts 0/1/16/256, all existing profiles, sample rates 30/120/240, all existing rendering-quality modes, Last/Best/trails/death/diagnostics combinations, and changes made while Playing, DeathContinuation, ResetPending, ReplayStudio, and Exiting.

Property/invariant tests include:

- effective ghost count never exceeds 256,
- profile caps are respected,
- presentation-only settings never alter finalized replay bytes,
- diagnostics settings never alter runtime outcomes,
- Rendering Quality never changes recorder configuration,
- hostile-timing setting changes cannot trigger duplicate finalization, pointer invalidation, replay corruption, stuck reset, active-attempt replacement, or invalid viewport state.

## 13. Data flow summary

### Live attempt

`Geometry Dash callbacks -> thin hooks -> EchoRuntimeCoordinator -> EchoRecorder -> FinalizedAttempt -> PreparedCommit -> journal durable commit -> published complete archive/history revision -> ghost/replay consumers`

If durable commit is temporarily unavailable, the complete prepared attempt may remain in memory with explicit PendingDurability state; it is never represented as durably saved until the durable revision advances.

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
- a visible LOD feature that changes ghost-selection semantics,
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

The implementation plan may subdivide these phases further but may not skip behavior-preservation gates or expand product scope. Because this hardening crosses several subsystems, the plan should use explicit phase checkpoints rather than a monolithic edit; each checkpoint must leave `main` in a coherent, verifiable state.

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