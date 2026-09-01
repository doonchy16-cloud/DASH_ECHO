# ECHO_DASH Hardening 02 — Persistence and Durability Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace per-attempt whole-archive rewriting with journaled transactional durability, deterministic snapshot recovery, and explicit durability revisions while preserving all supported v1.1.2 replay data.

**Architecture:** Keep `EchoReplayArchive` as the public in-memory façade, extract serialization into a codec, add an explicit `EchoReplayJournal` and filesystem-backed `EchoArchiveStore`, then make attempt commits append one checksum-framed journal transaction before/with publication. Snapshot compaction and retention become safe-window maintenance; primary/backup/journal recovery is deterministic and corruption is quarantined without overwriting evidence.

**Tech Stack:** C++23 standard library, Windows durable file operations where required by the pinned target, CTest dependency-free native tests, Geode 5.10.1 storage path integration, Python source contracts, GitHub Actions Windows Release build.

**Spec:** `docs/superpowers/specs/2026-09-01-echo-dash-quality-hardening-design.md`

## Global Constraints

- Run only after Plan 01 is terminal GREEN.
- No user-facing archive browser, cloud storage, sync, or new replay feature is introduced.
- Existing `doonchy.dash-echo` data/settings identity is preserved.
- Existing schema-1 archives from v1.1.2 remain readable and are never destroyed merely because new persistence code rejects a candidate.
- Journal/snapshot format evolution is internal and must be backward compatible through tested fixtures.
- A complete summary/replay pair is the indivisible logical unit.
- A failed persistence operation may produce `PendingDurability`; it may not publish half a summary/replay pair.
- Heavy compaction, backup rotation, and full-retention maintenance may not execute in latency-sensitive gameplay `postUpdate` work.
- Primary evidence is preserved before repair; diagnostics/repair do not overwrite corrupt artifacts first.
- No PASS claim is made without fresh fault tests plus Windows Release build evidence.

---

## File Structure for This Plan

**Create:**
- `src/EchoArchiveData.hpp` — complete in-memory snapshot image/value types used by codec/store tests.
- `src/EchoArchiveCodec.hpp`
- `src/EchoArchiveCodec.cpp` — schema-1 reader plus schema-2 snapshot writer/reader; no Geode path ownership.
- `src/EchoChecksum.hpp`
- `src/EchoChecksum.cpp` — deterministic CRC32 for journal/snapshot payload integrity.
- `src/EchoReplayJournal.hpp`
- `src/EchoReplayJournal.cpp` — append/scan transaction framing.
- `src/EchoArchiveStore.hpp`
- `src/EchoArchiveStore.cpp` — primary/backup/journal paths, durable append, load hierarchy, compaction promotion.
- `src/EchoDurableFileOps.hpp`
- `src/EchoDurableFileOps.cpp` — flush/replace helpers with Windows write-through semantics on the certified platform.
- `tests/cpp/test_archive_codec.cpp`
- `tests/cpp/test_replay_journal.cpp`
- `tests/cpp/test_archive_store.cpp`
- `tests/cpp/test_replay_archive_durability.cpp`
- `tests/fixtures/v1_1_2_schema1_minimal.bin` — captured compatibility fixture from the extracted unmodified schema-1 writer before schema-2 changes.
- `tests/cpp/archive_fixture_writer.cpp` — one-time executable used to produce that fixture, retained so its provenance is auditable.

**Modify:**
- `src/EchoReplayArchive.hpp`
- `src/EchoReplayArchive.cpp` — delegate encoding/storage, expose structured commit/maintenance state, keep query APIs stable.
- `src/EchoAttemptHistory.hpp/.cpp` — commit the prepared history entry only after archive logical commit accepts the pair.
- `src/EchoRuntimeCoordinator.hpp/.cpp` — consume structured archive commit results and safe-window maintenance.
- `src/main.cpp` — remove direct `archive.save()` lifecycle policy.
- `CMakeLists.txt`
- `.github/workflows/build-v1.yml`
- `tests/test_v1_1_contract.py`

---

### Task 1: Extract the legacy archive codec and capture a real schema-1 fixture

**Files:**
- Create: `src/EchoArchiveData.hpp`
- Create: `src/EchoArchiveCodec.hpp`
- Create: `src/EchoArchiveCodec.cpp`
- Create: `tests/cpp/test_archive_codec.cpp`
- Create: `tests/cpp/archive_fixture_writer.cpp`
- Create during the task: `tests/fixtures/v1_1_2_schema1_minimal.bin`
- Modify: `src/EchoReplayArchive.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces:

```cpp
struct EchoArchiveImage {
    EchoLevelContext context;
    std::deque<AttemptHistoryEntry> summaries;
    std::deque<AttemptRecord> replays;
    std::uint64_t revision = 0;
    std::size_t quarantinedReplayCount = 0;
};

enum class ArchiveDecodeStatus : std::uint8_t {
    LoadedSchema1,
    LoadedSchema2,
    RejectedStructural,
    RejectedContext
};

struct ArchiveDecodeResult {
    ArchiveDecodeStatus status = ArchiveDecodeStatus::RejectedStructural;
    EchoArchiveImage image;
    std::size_t semanticIssueCount = 0;
};

[[nodiscard]] ArchiveDecodeResult decodeArchiveSnapshot(
    std::filesystem::path const& path,
    EchoLevelContext const& expected
);

[[nodiscard]] bool encodeSchema1Fixture(
    std::filesystem::path const& path,
    EchoArchiveImage const& image
);
```

`encodeSchema1Fixture` is test/provenance support only and must reproduce the current v1.1.2 wire format exactly; production writes move to schema 2 later in this plan.

- [ ] **Step 1: Add a source contract that the current archive behavior does not change during extraction**

Keep all current v1.1.2 Python archive tests GREEN and add a contract that `EchoReplayArchive` still reports `recoveredFromBackup` and `quarantinedReplayCount` after codec extraction.

- [ ] **Step 2: Move the existing POD/string/player/camera/frame/summary/replay/context serialization and semantic validators into `EchoArchiveCodec.cpp` without changing wire bytes**

`EchoReplayArchive.cpp` calls codec functions; it no longer owns low-level `readPod`, `writePod`, `readFrame`, `writeFrame`, or equivalent format helpers.

- [ ] **Step 3: Add native round-trip tests and prove RED before wiring the codec**

Create a minimal image containing one finalized attempt and one matching summary. Test schema-1 encode/decode preserves attempt ID, frame sequence, timestamps, progress, player/camera presence, end reason, summary outcome, and level context.

Run CTest; expected RED before codec implementation, GREEN after extraction.

- [ ] **Step 4: Build the one-time fixture writer**

Add a small CMake executable `EchoDashArchiveFixtureWriter` from `archive_fixture_writer.cpp`, `EchoArchiveCodec.cpp`, and pure dependencies. It writes exactly `tests/fixtures/v1_1_2_schema1_minimal.bin` from a deterministic one-attempt image.

Run:

```powershell
cmake --build build-core-tests --config Release --target EchoDashArchiveFixtureWriter
.\build-core-tests\Release\EchoDashArchiveFixtureWriter.exe tests\fixtures\v1_1_2_schema1_minimal.bin
Get-FileHash tests\fixtures\v1_1_2_schema1_minimal.bin -Algorithm SHA256
```

Record the produced SHA in `test_archive_codec.cpp` and assert the fixture exists before later schema changes.

- [ ] **Step 5: Add a fixture-load test that reads the committed binary, not a file generated at test runtime**

Expected: `LoadedSchema1`, one replay, one summary, matching attempt ID, zero semantic issues.

- [ ] **Step 6: Run all tests and commit**

```bash
git add src/EchoArchive* src/EchoReplayArchive.cpp tests/cpp tests/fixtures CMakeLists.txt tests/test_v1_1_contract.py
git commit -m "refactor: extract ECHO_DASH archive codec"
```

---

### Task 2: Add checksums and the append-only journal format

**Files:**
- Create: `src/EchoChecksum.hpp`
- Create: `src/EchoChecksum.cpp`
- Create: `src/EchoReplayJournal.hpp`
- Create: `src/EchoReplayJournal.cpp`
- Create: `tests/cpp/test_replay_journal.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces:

```cpp
enum class JournalTransactionType : std::uint16_t {
    CommitAttempt = 1,
    EvictAttempts = 2
};

struct JournalAttemptCommit {
    std::uint64_t revision = 0;
    AttemptHistoryEntry summary;
    AttemptRecord replay;
};

struct JournalEviction {
    std::uint64_t revision = 0;
    std::vector<std::uint64_t> attemptIds;
};

enum class JournalScanStatus : std::uint8_t {
    Clean,
    TruncatedTail,
    RejectedHeader
};

struct JournalScanResult {
    JournalScanStatus status = JournalScanStatus::Clean;
    std::vector<JournalAttemptCommit> commits;
    std::vector<JournalEviction> evictions;
    std::uint64_t lastValidOffset = 0;
    std::size_t quarantinedTransactions = 0;
};

[[nodiscard]] std::uint32_t echoCrc32(std::span<std::byte const> bytes);
```

Journal record framing is fixed as:

```text
8 bytes  magic = "ECHOJNL2"
4 bytes  journal schema = 1
2 bytes  transaction type
2 bytes  reserved = 0
8 bytes  payload length
8 bytes  transaction revision
8 bytes  attempt id (0 for eviction transaction)
4 bytes  payload CRC32
payload
8 bytes  footer = "ECHOCMIT"
```

All numeric fields use the same little-endian POD convention as the existing Windows format for this pinned release.

- [ ] **Step 1: Write CRC tests**

Use the standard CRC32 check vector `"123456789" -> 0xCBF43926` and empty input `-> 0`.

- [ ] **Step 2: Write journal round-trip and truncation tests before implementation**

Tests must cover one attempt commit, two sequential commits, eviction record, payload checksum corruption, truncation in header, truncation in payload, missing footer, and trailing garbage.

Expected behavior:

- a partial final record yields `TruncatedTail` and keeps every earlier valid transaction;
- a bad checksum with a trustworthy header/length increments quarantine and continues to the next framed record;
- an invalid header/absurd length stops scanning at the valid prefix without allocating the declared payload.

- [ ] **Step 3: Prove RED, implement CRC32 and bounded journal parser, then prove GREEN**

Hard bounds before allocation:

```cpp
static constexpr std::uint64_t kMaxJournalPayloadBytes = 512ull * 1024ull * 1024ull;
static constexpr std::size_t kMaxEvictionIdsPerTransaction = 100'000;
```

A declared payload above the hard bound is `RejectedHeader`; never allocate it.

- [ ] **Step 4: Add `appendAttemptCommit` and `appendEviction` functions that build the complete record in memory before one append write**

Do not mutate archive memory in this task; this layer only serializes/appends framed bytes.

- [ ] **Step 5: Commit**

```bash
git add src/EchoChecksum.* src/EchoReplayJournal.* tests/cpp/test_replay_journal.cpp CMakeLists.txt
git commit -m "feat: add ECHO_DASH replay commit journal"
```

---

### Task 3: Add durable file operations and `EchoArchiveStore`

**Files:**
- Create: `src/EchoDurableFileOps.hpp`
- Create: `src/EchoDurableFileOps.cpp`
- Create: `src/EchoArchiveStore.hpp`
- Create: `src/EchoArchiveStore.cpp`
- Create: `tests/cpp/test_archive_store.cpp`
- Modify: `src/EchoArchiveCodec.hpp/.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces:

```cpp
enum class ArchiveLoadDisposition : std::uint8_t {
    NoExistingArchive,
    LoadedPrimary,
    LoadedPrimaryWithQuarantine,
    RecoveredBackup,
    RecoveredJournalOnly,
    Unrecoverable
};

enum class DurableWriteResult : std::uint8_t {
    Durable,
    RetryableFailure,
    PermanentFailure
};

struct ArchiveStoreLoadResult {
    ArchiveLoadDisposition disposition = ArchiveLoadDisposition::NoExistingArchive;
    EchoArchiveImage image;
    std::uint64_t snapshotRevision = 0;
    std::uint64_t durableRevision = 0;
    std::size_t quarantinedReplayCount = 0;
    std::size_t quarantinedJournalTransactions = 0;
    bool primaryRepairRecommended = false;
};

class EchoArchiveStore final {
public:
    explicit EchoArchiveStore(std::filesystem::path directory);
    [[nodiscard]] ArchiveStoreLoadResult load(EchoLevelContext const& context) const;
    [[nodiscard]] DurableWriteResult append(JournalAttemptCommit const& commit);
    [[nodiscard]] DurableWriteResult append(JournalEviction const& eviction);
    [[nodiscard]] DurableWriteResult compact(EchoArchiveImage const& image);
};
```

Path names are deterministic within the existing per-level archive directory:

```text
archive.bin
archive.bin.bak
archive.journal
archive.bin.tmp
```

- [ ] **Step 1: Write store recovery tests against a temporary directory**

Cover:

1. no files -> `NoExistingArchive`;
2. valid primary schema1 -> `LoadedPrimary`;
3. structurally invalid primary + valid backup -> `RecoveredBackup`;
4. valid primary + one journal commit -> resulting image includes both snapshot and journal authority;
5. partial journal tail -> valid prefix survives;
6. both snapshots invalid and journal starting from a valid empty base -> `RecoveredJournalOnly`;
7. rejected artifacts remain on disk after load.

- [ ] **Step 2: Prove RED**

Expected: compile FAIL because `EchoArchiveStore` does not exist.

- [ ] **Step 3: Implement durable append**

On Windows, `EchoDurableFileOps` opens/flushes the journal with Win32 `CreateFileW`/`FlushFileBuffers` or an equivalent write-through path. Return structured failure; do not throw across gameplay code. The non-Windows fallback may use standard filesystem streams but is not runtime-certified by this release.

- [ ] **Step 4: Add schema-2 snapshot framing**

Schema-2 snapshot header contains distinct magic `ECHOSNP2`, schema `2`, snapshot revision, context identity, payload length, and CRC32. Payload encodes complete summaries/replays using the extracted field codec. `decodeArchiveSnapshot` continues to detect/read schema1 and schema2.

- [ ] **Step 5: Implement deterministic load hierarchy**

Primary structurally valid -> use it, quarantining only semantically invalid records. Primary structurally invalid -> try backup. Replay journal transactions strictly newer than the selected snapshot revision in revision order. Duplicate/out-of-order revisions are rejected/quarantined rather than guessed.

If no snapshot is usable, accept journal-only recovery only when the first valid journal revision can legally reconstruct from an empty image for the requested context; otherwise return `Unrecoverable` and preserve files.

- [ ] **Step 6: Implement compaction promotion**

Write `archive.bin.tmp`, flush, decode/validate it, preserve/verify current primary as `.bak`, then promote temp to primary using the durable replacement helper. If any stage fails, old primary/backup/journal authority remains untouched. Do not clear journal entries represented by the new snapshot until primary promotion has been re-read successfully.

- [ ] **Step 7: Prove GREEN and commit**

```bash
git add src/EchoDurableFileOps.* src/EchoArchiveStore.* src/EchoArchiveCodec.* tests/cpp/test_archive_store.cpp CMakeLists.txt
git commit -m "feat: add durable ECHO_DASH archive store"
```

---

### Task 4: Convert `EchoReplayArchive` to structured commit and durability revisions

**Files:**
- Modify: `src/EchoReplayArchive.hpp`
- Modify: `src/EchoReplayArchive.cpp`
- Create: `tests/cpp/test_replay_archive_durability.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Replace `bool ingest(...)` as the authoritative mutation API with:

```cpp
enum class ReplayCommitDisposition : std::uint8_t {
    Durable,
    PendingDurability,
    Rejected
};

struct ReplayCommitResult {
    ReplayCommitDisposition disposition = ReplayCommitDisposition::Rejected;
    std::uint64_t attemptId = 0;
    std::uint64_t memoryRevision = 0;
    std::uint64_t durableRevision = 0;
};

struct ReplayArchiveStats {
    // existing counts/best fields remain
    std::uint64_t memoryRevision = 0;
    std::uint64_t durableRevision = 0;
    std::uint64_t snapshotRevision = 0;
    std::size_t pendingDurabilityCount = 0;
    bool persistenceDegraded = false;
    bool recoveredFromBackup = false;
    std::size_t quarantinedReplayCount = 0;
};

[[nodiscard]] ReplayCommitResult commit(
    AttemptRecord const& attempt,
    AttemptHistoryEntry const& summary
);

[[nodiscard]] bool retryPendingDurability();
```

Keep query APIs (`replayById`, `summaryById`, `latestReplay`, `bestRecordedReplay`, previous/next IDs) stable.

- [ ] **Step 1: Write atomic logical-pair tests**

Test invalid summary, invalid replay, attempt-ID mismatch, and duplicate attempt. All must return `Rejected` with both summary/replay containers unchanged.

- [ ] **Step 2: Write durable and pending revision tests**

Use a store seam that can inject one append failure. On successful append, memory and durable revisions advance together. On injected failure, complete summary/replay pair is published together in memory, `memoryRevision > durableRevision`, the attempt ID is tracked as pending, and `pendingDurabilityCount == 1`.

- [ ] **Step 3: Write ordered retry tests**

With two pending attempts, retry must append oldest pending revision first. A successful retry advances durable revision monotonically and clears only transactions confirmed durable.

- [ ] **Step 4: Implement store injection without exposing test-only global state**

`EchoReplayArchive` receives an `std::unique_ptr<EchoArchiveStore>` after `load(context)` resolves its production directory. Add a package-private/test constructor or constructor overload taking `std::unique_ptr<IArchiveStore>` where `IArchiveStore` is a narrow interface implemented by `EchoArchiveStore` and a native-test fake. Avoid `#ifdef TESTING` mutation APIs.

- [ ] **Step 5: Replace `m_revision`/`m_dirty` with explicit revision/durability state**

A complete pending attempt remains in the existing in-memory replay/summary containers; pending IDs reference those existing values rather than duplicating entire replay payloads. Retention may not evict a pending-durability attempt.

- [ ] **Step 6: Prove GREEN and commit**

```bash
git add src/EchoReplayArchive.* tests/cpp/test_replay_archive_durability.cpp CMakeLists.txt
git commit -m "refactor: make replay commits transaction-aware"
```

---

### Task 5: Make retention and compaction explicit maintenance operations

**Files:**
- Modify: `src/EchoReplayArchive.hpp`
- Modify: `src/EchoReplayArchive.cpp`
- Modify: `src/EchoArchiveStore.hpp/.cpp`
- Modify: `tests/cpp/test_replay_archive_durability.cpp`
- Modify: `tests/cpp/test_archive_store.cpp`

**Interfaces:**
- Produces:

```cpp
enum class ArchiveMaintenanceResult : std::uint8_t {
    NoWork,
    Completed,
    DeferredPendingDurability,
    RetryableFailure,
    PermanentFailure
};

[[nodiscard]] ArchiveMaintenanceResult performMaintenance();
[[nodiscard]] bool maintenanceRecommended() const;
```

- [ ] **Step 1: Write retention tests before implementation**

Create more replays than a small test replay limit and verify the computed eviction set is deterministic, never includes pending-durability attempts, never leaves a summary/replay mismatch, and respects current protected Best/Latest semantics.

- [ ] **Step 2: Write compaction tests**

After several journal commits, `performMaintenance()` creates a schema-2 snapshot whose revision equals durable revision. Once the promoted snapshot re-loads successfully, journal entries at or below snapshot revision are removed/truncated safely; newer transactions remain.

- [ ] **Step 3: Prove RED, implement explicit eviction transaction, then mutate memory only after durable eviction acceptance**

If the eviction journal write fails, memory remains unchanged and maintenance returns a failure/deferred status.

- [ ] **Step 4: Add compaction thresholds that are deterministic and bounded**

Use maintenance recommendation when either condition is met:

```cpp
journal transaction count >= 128
journal byte size >= 64 MiB
```

These are maintenance triggers only, not retention/user-visible limits. They do not run work by themselves.

- [ ] **Step 5: Prove GREEN and commit**

```bash
git add src/EchoReplayArchive.* src/EchoArchiveStore.* tests/cpp
git commit -m "feat: add explicit archive maintenance transactions"
```

---

### Task 6: Integrate transactional persistence into the runtime coordinator

**Files:**
- Modify: `src/EchoRuntimeCoordinator.hpp`
- Modify: `src/EchoRuntimeCoordinator.cpp`
- Modify: `src/main.cpp`
- Modify: `src/EchoAttemptHistory.cpp`
- Modify: `tests/test_v1_1_contract.py`

**Interfaces:**
- Consumes: `ReplayCommitResult`, `ArchiveMaintenanceResult`.
- Produces: no direct `archive.save()`/`archive.ingest()` calls in `main.cpp`; exactly one coordinator commit path.

- [ ] **Step 1: Add failing source contracts**

Assert production `main.cpp` no longer contains `.save()` or `.ingest(` calls for `EchoReplayArchive`, and finalization routes through `EchoRuntimeCoordinator::finalize`.

- [ ] **Step 2: Change coordinator finalization order**

Required order:

```text
recorder finalize
-> resolve immutable finalized attempt
-> prepare non-mutating history entry
-> archive.commit(summary + replay)
-> commit prepared history entry only if archive logical commit accepted
-> publish session best/replay/fleet revision effects
```

`ReplayCommitDisposition::Durable` maps to `AttemptCommitStatus::Committed`; `PendingDurability` maps to `AttemptCommitStatus::PendingDurability`; `Rejected` maps to `Rejected` and does not commit history.

- [ ] **Step 3: Retry pending durability only at safe lifecycle windows**

Call `retryPendingDurability()` and, when recommended, `performMaintenance()` from explicit safe operations: pause/Replay Studio idle, successful reset boundary after vanilla reset, context switch before old store release, and level exit. Do not invoke compaction from ordinary live `postUpdate`.

- [ ] **Step 4: Update context switching and exit behavior**

Context switch releases fleet/replay consumers, retries pending durability, attempts maintenance without destroying failed evidence, then loads the new context. `onExit` does the same best-effort safe maintenance before resource detach; gameplay exit is never blocked indefinitely by persistence failure.

- [ ] **Step 5: Prove GREEN and commit**

```bash
git add src/EchoRuntimeCoordinator.* src/main.cpp src/EchoAttemptHistory.cpp tests/test_v1_1_contract.py
git commit -m "fix: integrate transactional replay durability"
```

---

### Task 7: Add adversarial persistence fault and parser tests

**Files:**
- Modify: `tests/cpp/test_replay_journal.cpp`
- Modify: `tests/cpp/test_archive_store.cpp`
- Modify: `tests/cpp/test_replay_archive_durability.cpp`
- Create: `tests/cpp/test_archive_corruption.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: executable evidence for corruption and interrupted-write laws.

- [ ] **Step 1: Add transaction-cut tests**

For a known valid journal record, test every cut class: before magic complete, header half-written, payload half-written, before CRC-verifiable payload complete, before footer complete, and immediately after a complete record. Earlier committed records must survive every cut.

- [ ] **Step 2: Add semantic corruption tests**

Mutate frame sequence to zero, timestamp to NaN, progress to NaN, coordinates beyond bounds, summary/replay attempt IDs to mismatch, duplicate revisions, duplicate attempt IDs, invalid enum values, and oversized counts. Expect reject/quarantine with no crash and no partial publication.

- [ ] **Step 3: Add filesystem failure injection**

Fake store operations must simulate append failure, temp-write failure, flush failure, backup-copy failure, replacement failure, and post-promotion validation failure. In every compaction failure, prior known-good snapshot+journal authority must remain loadable.

- [ ] **Step 4: Add bounded randomized parser test**

Generate 1,000 deterministic pseudo-random byte buffers with a fixed seed and feed journal/snapshot decode entrypoints. Assert each returns a bounded status without exception/crash and no allocation request can exceed the parser hard caps.

- [ ] **Step 5: Run CTest under Release and Debug where available**

Expected: zero failures in both configurations.

- [ ] **Step 6: Commit**

```bash
git add tests/cpp/test_archive_corruption.cpp tests/cpp/test_replay_journal.cpp tests/cpp/test_archive_store.cpp tests/cpp/test_replay_archive_durability.cpp CMakeLists.txt
git commit -m "test: adversarially verify replay persistence"
```

---

### Task 8: Plan-02 persistence evidence gate

**Files:**
- Modify only if verification reveals defects.

**Interfaces:**
- Produces: terminal durability evidence before Plan 03.

- [ ] **Step 1: Run complete Python/native tests**

```powershell
python -m unittest discover -s tests -p "test_*.py" -v
cmake -S . -B build-core-tests -DECHO_DASH_BUILD_CORE_TESTS=ON
cmake --build build-core-tests --config Release --target EchoDashCoreTests
ctest --test-dir build-core-tests -C Release --output-on-failure
```

Expected: zero failures.

- [ ] **Step 2: Re-load the committed v1.1.2 schema-1 fixture**

Expected: fixture hash still matches the value captured in Task 1 and decode returns `LoadedSchema1` with the expected attempt.

- [ ] **Step 3: Source-path audit**

Confirm no whole-archive save is invoked after every finalized attempt, no full compaction call occurs in ordinary live `postUpdate`, pending replay pairs remain complete, and corrupted evidence is not removed during load.

- [ ] **Step 4: Trigger full pinned Windows Release workflow and wait for terminal result**

Expected: Python contracts GREEN, native fault suite GREEN, Geode Windows Release build GREEN, package collection GREEN.

- [ ] **Step 5: Record the exact Plan-02 commit SHA and workflow run ID; proceed only with terminal evidence.**
