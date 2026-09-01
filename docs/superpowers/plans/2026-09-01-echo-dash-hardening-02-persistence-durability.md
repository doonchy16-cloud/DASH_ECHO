# ECHO_DASH Hardening 02 — Persistence and Durability Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace per-attempt whole-archive rewriting with journaled transactional durability, deterministic snapshot recovery, and explicit durability revisions while preserving supported v1.1.2 replay/history data.

**Architecture:** Keep `EchoReplayArchive` as the public in-memory façade, extract wire encoding into a pure codec, add a checksum-framed append-only journal and an injectable filesystem store, and make each newly finalized attempt one structurally atomic archive entry containing its summary plus replay. Existing schema-1 summary-only history is preserved as an explicit compatibility state. Snapshot compaction and retention become safe-window maintenance; primary/backup/journal recovery is deterministic and corrupt evidence is preserved before repair.

**Tech Stack:** C++23 standard library, Windows durable file operations for the certified target, CTest dependency-free native tests, Geode 5.10.1 storage integration, Python source contracts, GitHub Actions Windows Release build.

**Spec:** `docs/superpowers/specs/2026-09-01-echo-dash-quality-hardening-design.md`

## Global Constraints

- Run only after Plan 01 is terminal GREEN.
- No archive browser, cloud sync, network storage, or other user-facing replay feature is introduced.
- Product data/settings identity stays `doonchy.dash-echo`.
- v1.1.2 schema-1 archives remain readable; rejected/corrupt artifacts are preserved before any repair/promotion.
- Every **new** finalized attempt is published as one complete summary+replay logical unit. A legacy or intentionally retention-evicted summary without replay is represented explicitly, never confused with a failed half-commit.
- A failed journal write may produce `PendingDurability`; it may not expose half of a new attempt.
- Heavy compaction, backup rotation, full validation, and retention maintenance never run in latency-sensitive live `postUpdate` work.
- Journal and snapshot parsers validate hard bounds before allocating declared payload sizes.
- No PASS claim is made without fresh fault tests plus terminal Windows Release build evidence.

---

## File Structure for This Plan

**Create:**
- `src/EchoArchiveData.hpp` — in-memory archive image/value types and explicit replay-presence state.
- `src/EchoArchiveCodec.hpp`
- `src/EchoArchiveCodec.cpp` — schema-1 reader plus schema-2 snapshot reader/writer; no Geode path ownership.
- `src/EchoChecksum.hpp`
- `src/EchoChecksum.cpp` — deterministic CRC32.
- `src/EchoReplayJournal.hpp`
- `src/EchoReplayJournal.cpp` — bounded journal record encoding/scan.
- `src/EchoDurableFileOps.hpp`
- `src/EchoDurableFileOps.cpp` — durable append/flush/copy/replace primitives.
- `src/EchoArchiveStore.hpp`
- `src/EchoArchiveStore.cpp` — `IArchiveStore` plus production primary/backup/journal implementation.
- `tests/cpp/test_archive_codec.cpp`
- `tests/cpp/test_replay_journal.cpp`
- `tests/cpp/test_archive_store.cpp`
- `tests/cpp/test_replay_archive_durability.cpp`
- `tests/cpp/test_archive_corruption.cpp`
- `tests/cpp/archive_fixture_writer.cpp`
- `tests/fixtures/v1_1_2_schema1_minimal.bin` — committed compatibility fixture generated from the extracted unmodified schema-1 writer before schema-2 writes replace it.

**Modify:**
- `src/EchoReplayArchive.hpp/.cpp` — delegate encoding/storage, use structurally complete entries, expose structured commit/maintenance state, preserve query semantics.
- `src/EchoAttemptHistory.hpp/.cpp` — commit prepared history only after archive logical acceptance.
- `src/EchoRuntimeCoordinator.hpp/.cpp` — consume structured durability/maintenance results.
- `src/main.cpp` — remove direct whole-archive save/ingest policy.
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
- Create during execution: `tests/fixtures/v1_1_2_schema1_minimal.bin`
- Modify: `src/EchoReplayArchive.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

```cpp
enum class ArchivedReplayState : std::uint8_t {
    Present,
    EvictedByRetention,
    LegacySummaryOnly
};

struct ArchivedAttempt {
    AttemptHistoryEntry summary;
    std::optional<AttemptRecord> replay;
    ArchivedReplayState replayState = ArchivedReplayState::Present;
};

struct EchoArchiveImage {
    EchoLevelContext context;
    std::deque<ArchivedAttempt> attempts;
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

`encodeSchema1Fixture` is provenance/test support only. Production writes move to schema 2 later in this plan.

- [ ] **Step 1: Add a preservation contract before extraction**

Keep all current v1.1.2 archive Python tests GREEN. Add source contracts requiring `recoveredFromBackup` and `quarantinedReplayCount` to survive the extraction.

- [ ] **Step 2: Move low-level schema-1 encoding/decoding without changing bytes**

Move POD/string/color/player/camera/frame/summary/replay/context serialization plus semantic validators from `EchoReplayArchive.cpp` into `EchoArchiveCodec.cpp`. `EchoReplayArchive` calls the codec; it no longer owns `readPod`, `writePod`, `readFrame`, `writeFrame`, or equivalent wire helpers.

- [ ] **Step 3: Join schema-1 summaries/replays into explicit `ArchivedAttempt` values**

For each decoded summary, match at most one replay by attempt ID. A valid pair becomes `Present`. A valid summary with no replay becomes `LegacySummaryOnly`. A replay with no summary, duplicate summary, duplicate replay, or mismatched identity is quarantined/rejected according to semantic validity; never guess a pairing.

- [ ] **Step 4: Write round-trip tests and prove RED/GREEN**

Build a deterministic one-attempt image and prove schema-1 encode/decode preserves attempt ID, frame sequence, timestamps, progress, player/camera state, end reason, summary outcome, context, and `Present` replay state. Add a summary-only fixture in memory and prove decode marks it `LegacySummaryOnly` rather than treating it as a half-commit.

- [ ] **Step 5: Build the one-time fixture writer**

Add CMake target:

```cmake
add_executable(EchoDashArchiveFixtureWriter
    tests/cpp/archive_fixture_writer.cpp
    src/EchoArchiveCodec.cpp
)
target_include_directories(EchoDashArchiveFixtureWriter PRIVATE src tests/cpp)
target_compile_features(EchoDashArchiveFixtureWriter PRIVATE cxx_std_23)
```

Run:

```powershell
cmake --build build-core-tests --config Release --target EchoDashArchiveFixtureWriter
.\build-core-tests\Release\EchoDashArchiveFixtureWriter.exe tests\fixtures\v1_1_2_schema1_minimal.bin
Get-FileHash tests\fixtures\v1_1_2_schema1_minimal.bin -Algorithm SHA256
```

Record the SHA256 literal in `test_archive_codec.cpp` and test the committed fixture directly on every later run.

- [ ] **Step 6: Run all tests and commit**

```bash
git add src/EchoArchiveData.hpp src/EchoArchiveCodec.* src/EchoReplayArchive.cpp tests/cpp tests/fixtures CMakeLists.txt tests/test_v1_1_contract.py
git commit -m "refactor: extract ECHO_DASH archive codec"
```

---

### Task 2: Add CRC32 and the context-bound append-only journal format

**Files:**
- Create: `src/EchoChecksum.hpp/.cpp`
- Create: `src/EchoReplayJournal.hpp/.cpp`
- Create: `tests/cpp/test_replay_journal.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

```cpp
enum class JournalTransactionType : std::uint16_t {
    CommitAttempt = 1,
    EvictReplayPayloads = 2
};

struct JournalAttemptCommit {
    std::uint64_t revision = 0;
    std::uint64_t contextFingerprint = 0;
    ArchivedAttempt attempt;
};

struct JournalEviction {
    std::uint64_t revision = 0;
    std::uint64_t contextFingerprint = 0;
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

[[nodiscard]] std::uint64_t archiveContextFingerprint(EchoLevelContext const& context);
[[nodiscard]] std::uint32_t echoCrc32(std::span<std::byte const> bytes);
```

`archiveContextFingerprint` is `EchoReplayArchive::stableNameHash(context.storageKey())`; tests pin identical/different-context behavior.

Journal record framing is fixed:

```text
8 bytes  magic = "ECHOJNL2"
4 bytes  journal schema = 1
2 bytes  transaction type
2 bytes  reserved = 0
8 bytes  payload length
8 bytes  transaction revision
8 bytes  context fingerprint
8 bytes  attempt id (0 for eviction transaction)
4 bytes  payload CRC32
payload
8 bytes  footer = "ECHOCMIT"
```

All numeric fields use the existing little-endian POD convention for the pinned Windows release.

- [ ] **Step 1: Write CRC/context tests**

Assert CRC32 `"123456789" == 0xCBF43926`, empty input is zero, identical contexts have identical nonzero fingerprints, and normal/practice or classic/platformer variants produce different fingerprints.

- [ ] **Step 2: Write journal round-trip/truncation tests before implementation**

Cover one commit, two commits, replay-eviction transaction, wrong context fingerprint, checksum corruption, truncated header, truncated payload, missing footer, trailing garbage, and out-of-bound payload length.

Expected rules:

- partial final record -> `TruncatedTail` while earlier complete records survive;
- bad checksum with trustworthy bounded length -> quarantine that record and continue from its known end;
- invalid magic/header/absurd length -> stop at valid prefix without allocating declared payload;
- requested-context mismatch -> transaction is not applied to that archive image.

- [ ] **Step 3: Prove RED, then implement bounded parser**

Hard caps checked before allocation:

```cpp
static constexpr std::uint64_t kMaxJournalPayloadBytes = 512ull * 1024ull * 1024ull;
static constexpr std::size_t kMaxEvictionIdsPerTransaction = 100'000;
```

- [ ] **Step 4: Implement record builders**

`encodeAttemptCommitRecord` accepts only `ArchivedReplayState::Present` with a valid summary+replay pair and matching nonzero attempt IDs. `encodeEvictionRecord` requires a nonempty bounded unique ID list. Both build one complete byte vector before filesystem append.

- [ ] **Step 5: Prove GREEN and commit**

```bash
git add src/EchoChecksum.* src/EchoReplayJournal.* tests/cpp/test_replay_journal.cpp CMakeLists.txt
git commit -m "feat: add context-bound ECHO_DASH replay journal"
```

---

### Task 3: Add durable file operations and an injectable `IArchiveStore`

**Files:**
- Create: `src/EchoDurableFileOps.hpp/.cpp`
- Create: `src/EchoArchiveStore.hpp/.cpp`
- Create: `tests/cpp/test_archive_store.cpp`
- Modify: `src/EchoArchiveCodec.hpp/.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

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

class IArchiveStore {
public:
    virtual ~IArchiveStore() = default;
    [[nodiscard]] virtual ArchiveStoreLoadResult load(EchoLevelContext const&) const = 0;
    [[nodiscard]] virtual DurableWriteResult append(JournalAttemptCommit const&) = 0;
    [[nodiscard]] virtual DurableWriteResult append(JournalEviction const&) = 0;
    [[nodiscard]] virtual DurableWriteResult compact(EchoArchiveImage const&) = 0;
    [[nodiscard]] virtual std::uint64_t journalBytes() const = 0;
    [[nodiscard]] virtual std::size_t journalTransactions() const = 0;
};

class EchoArchiveStore final : public IArchiveStore {
public:
    explicit EchoArchiveStore(std::filesystem::path directory);
    // implements IArchiveStore
};
```

Path names within the existing per-level directory:

```text
archive.bin
archive.bin.bak
archive.journal
archive.bin.tmp
```

- [ ] **Step 1: Write temporary-directory load/recovery tests**

Cover no files, valid schema1 primary, invalid primary + valid backup, valid primary + newer journal commit, partial journal tail, both snapshots invalid + context-matching journal from revision 1, journal for a different context, and preservation of rejected source artifacts.

- [ ] **Step 2: Prove RED**

Expected compile failure before `IArchiveStore`/`EchoArchiveStore` exist.

- [ ] **Step 3: Implement durable append/flush primitives**

On Windows use Win32 `CreateFileW` with append/write-through semantics plus `FlushFileBuffers` (or an equivalently strong explicit durable path). Return structured errors; do not throw through gameplay code. Non-Windows standard-library fallback is testable but not runtime-certified by v1.1.3.

- [ ] **Step 4: Add schema-2 snapshot framing**

Header contains 8-byte `ECHOSNP2`, schema `2`, snapshot revision, context fingerprint plus serialized context identity, bounded payload length, and CRC32. Payload encodes complete `ArchivedAttempt` entries, including explicit replay state. `decodeArchiveSnapshot` continues detecting schema1/schema2.

- [ ] **Step 5: Implement deterministic load hierarchy**

1. Structurally valid primary wins; semantic-invalid units are quarantined while unrelated valid units load.
2. Structurally invalid primary -> try backup.
3. Replay valid journal records with matching context fingerprint and revision strictly newer than selected snapshot.
4. Duplicate/out-of-order revisions are quarantined/rejected, never reordered by guesswork.
5. With no usable snapshot, journal-only recovery is allowed only when the first applicable transaction is revision `1`, context fingerprint matches, and its record can construct a valid empty-base archive image.
6. Otherwise return `Unrecoverable` and leave files untouched.

- [ ] **Step 6: Implement compaction promotion**

Write/flush `archive.bin.tmp`, decode it again and require exact intended context/revision, preserve and hash/verify current valid primary as `.bak`, promote temp with durable replacement, then reopen/validate primary. Do not discard represented journal records until the promoted primary has been reread successfully. Any failure leaves the prior snapshot+journal authority recoverable.

- [ ] **Step 7: Prove GREEN and commit**

```bash
git add src/EchoDurableFileOps.* src/EchoArchiveStore.* src/EchoArchiveCodec.* tests/cpp/test_archive_store.cpp CMakeLists.txt
git commit -m "feat: add durable ECHO_DASH archive store"
```

---

### Task 4: Make `EchoReplayArchive` structurally atomic and revision-aware

**Files:**
- Modify: `src/EchoReplayArchive.hpp/.cpp`
- Create: `tests/cpp/test_replay_archive_durability.cpp`
- Modify: callers that currently iterate `summaries()`/`replays()` directly.
- Modify: `CMakeLists.txt`

**Interfaces:**

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
    std::size_t summaryCount = 0;
    std::size_t replayCount = 0;
    std::size_t retainedFrames = 0;
    std::uint64_t latestAttemptId = 0;
    std::uint64_t bestRecordedAttemptId = 0;
    float bestRecordedProgress = 0.0f;
    std::uint64_t memoryRevision = 0;
    std::uint64_t durableRevision = 0;
    std::uint64_t snapshotRevision = 0;
    std::size_t pendingDurabilityCount = 0;
    bool persistenceDegraded = false;
    bool recoveredFromBackup = false;
    std::size_t quarantinedReplayCount = 0;
};

class EchoReplayArchive final {
public:
    explicit EchoReplayArchive(std::unique_ptr<IArchiveStore> store = {});
    [[nodiscard]] ReplayCommitResult commit(
        AttemptRecord const& attempt,
        AttemptHistoryEntry const& summary
    );
    [[nodiscard]] bool retryPendingDurability();
    [[nodiscard]] std::deque<ArchivedAttempt> const& attempts() const;
    // existing replayById/summaryById/latest/best/previous/next queries remain
};
```

Production `load(context)` creates `EchoArchiveStore` when no injected store exists. Native tests inject a fake implementing `IArchiveStore`; no test-only global hooks or `#ifdef TESTING` mutation APIs are introduced.

- [ ] **Step 1: Write atomic-entry rejection tests**

Invalid summary, invalid replay, ID mismatch, duplicate attempt, active/unfinalized replay -> `Rejected` with `m_attempts`, memory revision, and pending queue unchanged.

- [ ] **Step 2: Write durable/pending tests using a fake store**

On success: insert one `ArchivedAttempt{summary,replay,Present}`, append one journal transaction, memory/durable revisions advance to the same value. On injected append failure: the same complete `ArchivedAttempt` is present once in memory, memory revision advances, durable revision does not, and one pending revision/attempt ID is recorded. No summary/replay half-state is representable.

- [ ] **Step 3: Write ordered retry tests**

Two pending revisions retry oldest first. Durable revision increases monotonically and only successfully flushed transactions leave the pending queue.

- [ ] **Step 4: Replace separate authoritative summary/replay deques with `m_attempts`**

Queries return pointers into `ArchivedAttempt.summary` or optional replay. Replace direct caller iteration with `attempts()` and explicit replay-state handling. Legacy summary-only entries remain visible to death/history restoration while `replayById()` correctly returns null.

- [ ] **Step 5: Make commit exception-safe before publication**

Validate and construct/compress the full `ArchivedAttempt` local value before mutating `m_attempts`. Insertion of that one value is the only logical publication mutation. Record the pending descriptor only after successful insertion. A thrown allocation failure is caught at the archive boundary, returns `Rejected`, and leaves prior authority unchanged.

- [ ] **Step 6: Prove GREEN and commit**

```bash
git add src/EchoReplayArchive.* src/main.cpp src/EchoGhostFleet.* tests/cpp/test_replay_archive_durability.cpp CMakeLists.txt
git commit -m "refactor: make replay archive commits structurally atomic"
```

---

### Task 5: Make retention and compaction explicit maintenance transactions

**Files:**
- Modify: `src/EchoReplayArchive.hpp/.cpp`
- Modify: `src/EchoArchiveStore.hpp/.cpp`
- Modify: `tests/cpp/test_replay_archive_durability.cpp`
- Modify: `tests/cpp/test_archive_store.cpp`

**Interfaces:**

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

- [ ] **Step 1: Write deterministic retention tests**

With a small fake replay limit/budget, compute the eviction set from oldest eligible replay payloads. It must never include a pending-durability entry or violate protected existing Best/Latest semantics. Eviction transitions selected entries from `Present` to `EvictedByRetention`; summaries remain explicit historical records.

- [ ] **Step 2: Prove eviction durability ordering**

Write `JournalEviction` first. If append fails, memory replay states remain unchanged. Only after durable eviction acceptance may replay payloads be released and frame counts updated.

- [ ] **Step 3: Write compaction tests**

After several durable journal transactions, `performMaintenance()` builds a schema-2 snapshot with `snapshotRevision == durableRevision`. After successful promotion/reopen, only journal records at or below snapshot revision are compacted away; newer records survive.

- [ ] **Step 4: Implement deterministic maintenance triggers**

Recommend maintenance when either:

```text
journal transaction count >= 128
journal byte size >= 64 MiB
```

or current configured retention/disk policy requires eviction. These are triggers only; no maintenance runs automatically in live frame code.

- [ ] **Step 5: Prove GREEN and commit**

```bash
git add src/EchoReplayArchive.* src/EchoArchiveStore.* tests/cpp/test_replay_archive_durability.cpp tests/cpp/test_archive_store.cpp
git commit -m "feat: add explicit archive maintenance transactions"
```

---

### Task 6: Integrate transactional durability into the runtime coordinator

**Files:**
- Modify: `src/EchoRuntimeCoordinator.hpp/.cpp`
- Modify: `src/EchoAttemptHistory.cpp`
- Modify: `src/main.cpp`
- Modify: `tests/test_v1_1_contract.py`

**Interfaces:**
- Consumes: `ReplayCommitResult`, `ArchiveMaintenanceResult`.
- Produces: exactly one coordinator finalization path and zero direct `EchoReplayArchive::save/ingest` policy in `main.cpp`.

- [ ] **Step 1: Add failing source contracts**

Assert `main.cpp` contains no `archive.save()` or `archive.ingest(` lifecycle calls and that finalization routes through `EchoRuntimeCoordinator::finalize`.

- [ ] **Step 2: Implement finalization order**

```text
recorder.finalizeAttempt
-> resolve immutable finalized attempt
-> prepare non-mutating history entry
-> archive.commit(full summary+replay logical unit)
-> if archive logical commit accepted: history.commitPreparedEntry
-> publish session-best/replay/fleet revision consequences
```

Map `Durable -> AttemptCommitStatus::Committed`, `PendingDurability -> AttemptCommitStatus::PendingDurability`, `Rejected -> AttemptCommitStatus::Rejected`. A persistence failure never causes a duplicate new attempt or duplicate history commit.

- [ ] **Step 3: Retry/maintain only at legal safe windows**

Retry pending durability and optionally perform maintenance during pause/Replay Studio idle, after a successful vanilla reset boundary, context switch before old-store release, and level exit. Ordinary Playing `postUpdate` may update bounded persistence health counters only; it may not compact or rewrite historical data.

- [ ] **Step 4: Harden context switch/exit**

Release fleet/replay consumers before retention/compaction that can invalidate replay payload addresses. Persistence failure does not indefinitely block vanilla exit; preserve pending state/evidence and report structured health for diagnostics Plan 04.

- [ ] **Step 5: Prove GREEN and commit**

```bash
git add src/EchoRuntimeCoordinator.* src/EchoAttemptHistory.cpp src/main.cpp tests/test_v1_1_contract.py
git commit -m "fix: integrate transactional replay durability"
```

---

### Task 7: Add adversarial parser, corruption, and filesystem fault tests

**Files:**
- Create: `tests/cpp/test_archive_corruption.cpp`
- Modify: `tests/cpp/test_replay_journal.cpp`
- Modify: `tests/cpp/test_archive_store.cpp`
- Modify: `tests/cpp/test_replay_archive_durability.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: executable evidence for interrupted writes, malformed input, and rollback laws.

- [ ] **Step 1: Test record cuts**

Cut a known two-record journal at every boundary class: partial magic, partial header, partial payload, before checksum-verifiable payload completion, partial footer, exactly after record one, and exactly after record two. Earlier fully committed records always survive.

- [ ] **Step 2: Test semantic corruption**

Mutate sequence to zero, timestamp/progress to NaN, coordinates beyond validator bounds, summary/replay IDs, duplicate attempt IDs, duplicate/out-of-order revisions, invalid enums, context fingerprint, and oversized counts. Expect bounded reject/quarantine with no partial publication.

- [ ] **Step 3: Test filesystem failure stages through fake durable ops/store**

Simulate append failure, temp-write failure, flush failure, backup-copy failure, replacement failure, and post-promotion validation failure. Every failed compaction leaves the prior known-good snapshot+journal loadable.

- [ ] **Step 4: Add deterministic bounded randomized parser test**

Generate 1,000 pseudo-random byte buffers from a fixed seed. Feed snapshot/journal decode entrypoints. Each call returns a bounded status without crash/exception escape; declared lengths above hard caps are rejected before allocation.

- [ ] **Step 5: Run Release and Debug CTest where available**

```powershell
cmake --build build-core-tests --config Release --target EchoDashCoreTests
ctest --test-dir build-core-tests -C Release --output-on-failure
cmake --build build-core-tests --config Debug --target EchoDashCoreTests
ctest --test-dir build-core-tests -C Debug --output-on-failure
```

Expected: zero failures.

- [ ] **Step 6: Commit**

```bash
git add tests/cpp/test_archive_corruption.cpp tests/cpp/test_replay_journal.cpp tests/cpp/test_archive_store.cpp tests/cpp/test_replay_archive_durability.cpp CMakeLists.txt
git commit -m "test: adversarially verify replay persistence"
```

---

### Task 8: Plan-02 persistence evidence gate

**Files:**
- Modify only if verification reveals a defect.

**Interfaces:**
- Produces: terminal durability evidence before Plan 03.

- [ ] **Step 1: Run the complete local test suite**

```powershell
python -m unittest discover -s tests -p "test_*.py" -v
cmake -S . -B build-core-tests -DECHO_DASH_BUILD_CORE_TESTS=ON
cmake --build build-core-tests --config Release --target EchoDashCoreTests
ctest --test-dir build-core-tests -C Release --output-on-failure
```

Expected: zero failures.

- [ ] **Step 2: Re-load the committed v1.1.2 fixture**

Expected: committed fixture SHA still matches Task 1; decode returns `LoadedSchema1` with the expected present replay and summary. Add a migration test containing a schema-1 summary-only case and confirm it remains historical metadata with `LegacySummaryOnly` state.

- [ ] **Step 3: Audit source paths**

Confirm no whole-archive save after each attempt, no compaction/retention in ordinary live `postUpdate`, every new attempt is one `ArchivedAttempt` logical value, pending entries cannot be retention-evicted, and load does not delete rejected evidence.

- [ ] **Step 4: Trigger the pinned Windows Release workflow and wait for terminal completion**

Expected terminal GREEN: Python contracts, native persistence/fault suite, Geode CLI 3.7.4 setup, Geode SDK 5.10.1 setup, Windows Release compile, compiler evidence upload, package collection/upload.

- [ ] **Step 5: Record exact Plan-02 source SHA and workflow run ID. Proceed to Plan 03 only after all evidence is terminal.**
