# ECHO_DASH Hardening 02 — Persistence and Durability Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace per-attempt whole-archive rewriting with context-bound journaled transactional durability, deterministic snapshot recovery, explicit durability revisions, and bounded cross-context pending preservation while retaining supported v1.1.2 replay/history data.

**Architecture:** Extract level/archive context and wire encoding into Geode-independent modules. Represent each new finalized attempt as one immutable shared archive value containing summary + replay. Use a checksum-framed append-only journal and injectable filesystem store. A bounded pending-durability ledger retains accepted immutable attempts across level/mode context switches without duplicating replay payloads. Snapshot compaction and retention run only at safe maintenance boundaries; recovery preserves corrupt evidence before repair.

**Tech Stack:** C++23 standard library, Windows durable file operations for the certified target, CTest dependency-free native tests, Geode 5.10.1 save-directory adapter, Python source contracts, GitHub Actions Windows Release build.

**Spec:** `docs/superpowers/specs/2026-09-01-echo-dash-quality-hardening-design.md`

## Global Constraints

- Run only after Plan 01 is terminal GREEN.
- No archive browser, cloud sync, network storage, or new replay capability.
- Product data/settings identity stays `doonchy.dash-echo`.
- Current v1.1.2 schema-1 files under `Mod::get()->getSaveDir()/echo_dash/<storageKey>.edar` and `.edar.bak` remain readable. Rejected/corrupt artifacts are preserved before repair/promotion.
- Every **new** finalized attempt is one complete summary+replay logical unit. A legacy or intentionally retention-evicted summary without replay is explicit compatibility state, never a half-commit.
- A failed journal write may produce `PendingDurability`; it may not expose half of a new attempt.
- Later revisions for the same context are never durably appended ahead of an earlier failed revision.
- Accepted pending attempts are not silently discarded by context switch. Pending memory is bounded; once the bound is exhausted during storage failure, new archive publication is rejected/degraded rather than deleting earlier accepted pending data.
- Heavy compaction, backup rotation, full validation, and retention maintenance never run in latency-sensitive live `postUpdate` work.
- Parsers validate hard bounds before allocating declared payloads.
- No PASS claim without fresh fault tests plus terminal Windows Release build evidence.

---

## File Structure

**Create:**
- `src/EchoArchiveContext.hpp/.cpp` — pure `EchoLevelContext`, storage key, matching, stable FNV-1a hash/fingerprint.
- `src/EchoArchiveData.hpp` — immutable archive entry/image types.
- `src/EchoArchiveCodec.hpp/.cpp` — schema-1 reader plus schema-2 snapshot reader/writer; no Geode path ownership.
- `src/EchoChecksum.hpp/.cpp` — deterministic CRC32.
- `src/EchoReplayJournal.hpp/.cpp` — bounded journal record encode/scan.
- `src/EchoDurableFileOps.hpp/.cpp` — durable append/flush/copy/replace primitives.
- `src/EchoArchiveStore.hpp/.cpp` — `IArchiveStore`, `IArchiveStoreFactory`, production filesystem store/factory.
- `src/EchoArchiveStoreGeode.hpp/.cpp` — only Geode save-directory adapter for production store factory.
- `src/EchoPendingDurability.hpp/.cpp` — fixed-capacity reservation + detached pending ledger.
- `tests/cpp/test_archive_context.cpp`
- `tests/cpp/test_archive_codec.cpp`
- `tests/cpp/test_replay_journal.cpp`
- `tests/cpp/test_archive_store.cpp`
- `tests/cpp/test_pending_durability.cpp`
- `tests/cpp/test_replay_archive_durability.cpp`
- `tests/cpp/test_archive_corruption.cpp`
- `tests/cpp/archive_fixture_writer.cpp`
- `tests/fixtures/v1_1_2_schema1_minimal.bin` — committed compatibility fixture generated from extracted unchanged schema-1 writer.

**Modify:**
- `src/EchoReplayArchive.hpp/.cpp`
- `src/EchoAttemptHistory.hpp/.cpp`
- `src/EchoRuntimeCoordinator.hpp/.cpp`
- `src/main.cpp`
- `CMakeLists.txt`
- `.github/workflows/build-v1.yml`
- `tests/test_v1_1_contract.py`

---

### Task 1: Extract pure archive context and legacy schema-1 codec; capture a real fixture

**Files:**
- Create: `src/EchoArchiveContext.hpp/.cpp`
- Create: `src/EchoArchiveData.hpp`
- Create: `src/EchoArchiveCodec.hpp/.cpp`
- Create: `tests/cpp/test_archive_context.cpp`
- Create: `tests/cpp/test_archive_codec.cpp`
- Create: `tests/cpp/archive_fixture_writer.cpp`
- Create during execution: `tests/fixtures/v1_1_2_schema1_minimal.bin`
- Modify: `src/EchoReplayArchive.hpp/.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

```cpp
struct EchoLevelContext {
    std::int64_t levelId = 0;
    std::uint64_t fallbackHash = 0;
    bool platformer = false;
    bool practice = false;
    int gdNormalPercent = 0;
    std::string levelName;

    [[nodiscard]] std::string storageKey() const;
    [[nodiscard]] bool matches(EchoLevelContext const& other) const;
};

[[nodiscard]] std::uint64_t stableNameHash(std::string_view text);
[[nodiscard]] std::uint64_t archiveContextFingerprint(EchoLevelContext const& context);

enum class ArchivedReplayState : std::uint8_t {
    Present,
    EvictedByRetention,
    LegacySummaryOnly
};

struct ArchivedAttempt {
    std::uint64_t revision = 0;
    AttemptHistoryEntry summary;
    std::optional<AttemptRecord> replay;
    ArchivedReplayState replayState = ArchivedReplayState::Present;
};

using ArchivedAttemptPtr = std::shared_ptr<ArchivedAttempt const>;

struct EchoArchiveImage {
    EchoLevelContext context;
    std::deque<ArchivedAttemptPtr> attempts;
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

`stableNameHash` is the current 64-bit FNV-1a implementation moved out of `EchoReplayArchive`; existing callers migrate to the pure function. `archiveContextFingerprint` hashes `context.storageKey()`.

- [ ] **Step 1: Add preservation/context tests before extraction**

Pin storage keys for ID/local, classic/platformer, normal/practice contexts. Identical contexts must match; practice/platformer differences must not. Preserve current v1.1.2 Python recovery/quarantine contracts.

- [ ] **Step 2: Move `EchoLevelContext` + hash implementation without behavior change**

`EchoReplayArchive.hpp` includes `EchoArchiveContext.hpp`; remove its duplicate context/hash ownership. Main fallback-name hashing uses `stableNameHash`.

- [ ] **Step 3: Move schema-1 wire helpers and semantic validators into codec without changing bytes**

Move POD/string/color/player/camera/frame/summary/replay/context serialization and validation from `EchoReplayArchive.cpp`. Codec remains Geode-independent.

- [ ] **Step 4: Join schema-1 summary/replay records into explicit immutable entries**

For each decoded summary, match at most one replay by attempt ID. Valid pair -> `Present`; valid summary without replay -> `LegacySummaryOnly`; orphan replay, duplicate summary/replay, identity mismatch -> quarantine/reject according to semantic validity. Never guess a pair. Legacy entries use revision 0 because schema 1 has no durability revision.

- [ ] **Step 5: Write round-trip tests and prove RED/GREEN**

One deterministic attempt must preserve attempt ID, frame sequence/time/progress, player/camera state, end reason, summary outcome, context, and `Present`. A summary-only schema-1 case must decode as `LegacySummaryOnly`.

- [ ] **Step 6: Build and commit a deterministic real schema-1 fixture**

Core-only CMake target includes `EchoArchiveContext.cpp`, `EchoArchiveCodec.cpp`, and fixture writer. Run:

```powershell
cmake --build build-core-tests --config Release --target EchoDashArchiveFixtureWriter
.\build-core-tests\Release\EchoDashArchiveFixtureWriter.exe tests\fixtures\v1_1_2_schema1_minimal.bin
Get-FileHash tests\fixtures\v1_1_2_schema1_minimal.bin -Algorithm SHA256
```

Record the exact SHA literal in `test_archive_codec.cpp`; every later test loads the committed fixture directly.

- [ ] **Step 7: Prove GREEN and commit**

```bash
git add src/EchoArchiveContext.* src/EchoArchiveData.hpp src/EchoArchiveCodec.* src/EchoReplayArchive.* tests/cpp tests/fixtures CMakeLists.txt tests/test_v1_1_contract.py
git commit -m "refactor: extract ECHO_DASH archive context and codec"
```

---

### Task 2: Add CRC32 and the context-bound append-only journal

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
    ArchivedAttemptPtr attempt;
};

struct JournalEviction {
    std::uint64_t revision = 0;
    std::uint64_t contextFingerprint = 0;
    std::vector<std::uint64_t> attemptIds;
};

enum class JournalScanStatus : std::uint8_t { Clean, TruncatedTail, RejectedHeader };

struct JournalScanResult {
    JournalScanStatus status = JournalScanStatus::Clean;
    std::vector<JournalAttemptCommit> commits;
    std::vector<JournalEviction> evictions;
    std::uint64_t lastValidOffset = 0;
    std::size_t quarantinedTransactions = 0;
};

[[nodiscard]] std::uint32_t echoCrc32(std::span<std::byte const> bytes);
[[nodiscard]] std::vector<std::byte> encodeAttemptCommitRecord(JournalAttemptCommit const& tx);
[[nodiscard]] std::vector<std::byte> encodeEvictionRecord(JournalEviction const& tx);
[[nodiscard]] JournalScanResult scanJournal(std::span<std::byte const> bytes, std::uint64_t expectedContextFingerprint);
```

Framing:

```text
8 bytes magic = "ECHOJNL2"
4 bytes journal schema = 1
2 bytes transaction type
2 bytes reserved = 0
8 bytes payload length
8 bytes transaction revision
8 bytes context fingerprint
8 bytes attempt id (0 for eviction)
4 bytes payload CRC32
payload
8 bytes footer = "ECHOCMIT"
```

- [ ] **Step 1: Write CRC/context tests**

`"123456789" -> 0xCBF43926`, empty -> 0. Context fingerprint equality/difference follows storage-key identity.

- [ ] **Step 2: Write round-trip/truncation/corruption tests before implementation**

Cover one/two commits, eviction, wrong context, checksum corruption, partial header/payload/footer, trailing garbage, absurd length, duplicate ID list.

Rules: partial final record preserves earlier full records; bad checksum with trustworthy bounded length quarantines that record and continues; invalid header/absurd length stops at valid prefix without allocating the declared payload; mismatched context is never applied.

- [ ] **Step 3: Prove RED then implement bounded parser**

Hard caps before allocation:

```cpp
static constexpr std::uint64_t kMaxJournalPayloadBytes = 512ull * 1024ull * 1024ull;
static constexpr std::size_t kMaxEvictionIdsPerTransaction = 100'000;
```

- [ ] **Step 4: Enforce complete immutable commit values**

`encodeAttemptCommitRecord` requires non-null `attempt`, `replayState == Present`, present replay, matching nonzero summary/replay attempt IDs, `attempt->revision == tx.revision`, and context fingerprint nonzero. Eviction requires nonempty bounded unique IDs.

- [ ] **Step 5: Prove GREEN and commit**

```bash
git add src/EchoChecksum.* src/EchoReplayJournal.* tests/cpp/test_replay_journal.cpp CMakeLists.txt
git commit -m "feat: add context-bound ECHO_DASH replay journal"
```

---

### Task 3: Add durable file operations and injectable context store factory

**Files:**
- Create: `src/EchoDurableFileOps.hpp/.cpp`
- Create: `src/EchoArchiveStore.hpp/.cpp`
- Create: `src/EchoArchiveStoreGeode.hpp/.cpp`
- Create: `tests/cpp/test_archive_store.cpp`
- Modify: `src/EchoArchiveCodec.hpp/.cpp`
- Modify: `src/main.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

```cpp
enum class ArchiveLoadDisposition : std::uint8_t {
    NoExistingArchive,
    LoadedPrimary,
    LoadedPrimaryWithQuarantine,
    LoadedLegacySchema1,
    RecoveredBackup,
    RecoveredJournalOnly,
    Unrecoverable
};

enum class DurableWriteResult : std::uint8_t { Durable, RetryableFailure, PermanentFailure };

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

class IArchiveStoreFactory {
public:
    virtual ~IArchiveStoreFactory() = default;
    [[nodiscard]] virtual std::unique_ptr<IArchiveStore> create(EchoLevelContext const&) const = 0;
};

class EchoArchiveStoreFactory final : public IArchiveStoreFactory {
public:
    explicit EchoArchiveStoreFactory(std::filesystem::path baseDirectory);
    [[nodiscard]] std::unique_ptr<IArchiveStore> create(EchoLevelContext const&) const override;
};

[[nodiscard]] std::unique_ptr<IArchiveStoreFactory> makeGeodeArchiveStoreFactory();
```

For context key `K`, new storage lives in `<save>/echo_dash/K/`:

```text
archive.bin
archive.bin.bak
archive.journal
archive.bin.tmp
```

Legacy lookup remains `<save>/echo_dash/K.edar` and `K.edar.bak`; these are read compatibility inputs and are not deleted automatically.

- [ ] **Step 1: Write temporary-directory load/recovery tests**

Cover no files, legacy schema1 primary, legacy invalid primary + valid legacy backup, valid schema2 primary, valid primary + newer journal, partial tail, snapshot failure + context-matching journal from revision1, mismatched-context journal, and rejected-artifact preservation.

- [ ] **Step 2: Prove RED**

- [ ] **Step 3: Implement durable append/flush**

On Windows use `CreateFileW`/append plus `FlushFileBuffers` or equivalently strong explicit durable operations. Return structured results; no exceptions escape into gameplay code. Non-Windows fallback is testable but not v1.1.3 runtime-certified.

- [ ] **Step 4: Add schema-2 snapshot framing**

Header: magic `ECHOSNP2`, schema 2, snapshot revision, context fingerprint + serialized context identity, bounded payload length, CRC32. Payload encodes immutable archive entries including replay state. Decoder still reads schema1.

- [ ] **Step 5: Implement deterministic recovery hierarchy**

1. valid new primary;
2. new backup if primary structurally invalid;
3. legacy schema1 primary/backup only when no usable new snapshot exists;
4. replay matching-context journal revisions strictly newer than selected snapshot/legacy base;
5. duplicate/out-of-order revisions quarantine/reject, never reorder;
6. journal-only recovery requires first applicable revision 1 and valid empty-base context;
7. unrecoverable leaves all files untouched.

A structurally valid primary with semantic-invalid units loads unrelated valid units and reports quarantine rather than silently preferring a different semantic state from backup.

- [ ] **Step 6: Implement compaction promotion**

Write/flush temp, decode and require exact intended context/revision, preserve/hash current valid primary as backup, promote temp durably, reopen/validate primary, then compact journal records <= snapshot revision. Any failure keeps prior recoverable snapshot+journal authority. Legacy `.edar` is retained until a future separately approved cleanup policy.

- [ ] **Step 7: Wire production factory only at Geode boundary**

`main.cpp`/integration initializes `EchoReplayArchive` with `makeGeodeArchiveStoreFactory()`. Core tests inject fake factories; pure store/codec code never calls `Mod::get()`.

- [ ] **Step 8: Prove GREEN and commit**

```bash
git add src/EchoDurableFileOps.* src/EchoArchiveStore* src/EchoArchiveCodec.* src/main.cpp tests/cpp/test_archive_store.cpp CMakeLists.txt
git commit -m "feat: add durable ECHO_DASH archive store"
```

---

### Task 4: Add bounded pending-durability reservation/ledger

**Files:**
- Create: `src/EchoPendingDurability.hpp/.cpp`
- Create: `tests/cpp/test_pending_durability.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

```cpp
struct PendingReservation {
    std::uint64_t token = 0;
    std::size_t bytes = 0;
    bool valid = false;
};

struct DetachedPendingCommit {
    std::uint64_t acceptedSequence = 0;
    PendingReservation reservation;
    EchoLevelContext context;
    JournalAttemptCommit transaction;
};

class EchoPendingDurabilityLedger final {
public:
    static constexpr std::size_t kMaxPendingAttempts = 16;
    static constexpr std::size_t kMaxPendingBytes = 128ull * 1024ull * 1024ull;

    [[nodiscard]] std::optional<PendingReservation> reserve(std::size_t estimatedBytes);
    void release(PendingReservation reservation);
    [[nodiscard]] bool detach(
        PendingReservation reservation,
        EchoLevelContext const& context,
        JournalAttemptCommit transaction
    );
    [[nodiscard]] std::size_t pendingAttempts() const;
    [[nodiscard]] std::size_t pendingBytes() const;
    [[nodiscard]] std::vector<std::size_t> pendingIndicesForContext(EchoLevelContext const&) const;
    [[nodiscard]] DetachedPendingCommit const* at(std::size_t index) const;
    void erase(std::size_t index);
};
```

Implementation uses a fixed-capacity `std::array<std::optional<DetachedPendingCommit>, 16>` for detached entries. Reservations are counter/token operations and allocate no replay storage. `JournalAttemptCommit` shares the immutable `ArchivedAttemptPtr`; detaching a pending attempt does not duplicate replay frames.

- [ ] **Step 1: Write reservation-bound tests**

16 accepted reservations fit if under 128MiB; 17th rejects. Byte limit rejects even before count limit. Release restores capacity. Invalid/double release does not underflow counters.

- [ ] **Step 2: Write detach/move tests**

A reserved transaction detaches into one fixed slot, preserving context/revision/attempt pointer identity. Detach with unknown reservation or mismatched context fingerprint rejects without consuming a slot.

- [ ] **Step 3: Prove RED, implement fixed ledger, prove GREEN**

- [ ] **Step 4: Commit**

```bash
git add src/EchoPendingDurability.* tests/cpp/test_pending_durability.cpp CMakeLists.txt
git commit -m "feat: bound ECHO_DASH pending durability"
```

---

### Task 5: Make `EchoReplayArchive` structurally atomic, ordered, and pending-safe

**Files:**
- Modify: `src/EchoReplayArchive.hpp/.cpp`
- Create: `tests/cpp/test_replay_archive_durability.cpp`
- Modify: callers that iterate old `summaries()`/`replays()` directly.
- Modify: `CMakeLists.txt`

**Interfaces:**

```cpp
enum class ReplayCommitDisposition : std::uint8_t { Durable, PendingDurability, Rejected };

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
    std::size_t currentContextPendingCount = 0;
    std::size_t detachedPendingCount = 0;
    bool persistenceDegraded = false;
    bool recoveredFromBackup = false;
    std::size_t quarantinedReplayCount = 0;
};

class EchoReplayArchive final {
public:
    EchoReplayArchive();
    void setStoreFactory(std::unique_ptr<IArchiveStoreFactory> factory);
    [[nodiscard]] bool load(EchoLevelContext const& context);
    [[nodiscard]] ReplayCommitResult commit(AttemptRecord const&, AttemptHistoryEntry const&);
    [[nodiscard]] bool retryPendingDurability();
    [[nodiscard]] bool prepareContextSwitch();
    [[nodiscard]] std::deque<ArchivedAttemptPtr> const& attempts() const;
    // replayById/summaryById/latest/best/previous/next query semantics remain
};
```

Internal rule: an `ArchivedAttemptPtr` is immutable after publication. Retention later replaces a pointer with a newly constructed immutable summary-only value after eviction durability succeeds.

- [ ] **Step 1: Write atomic rejection tests**

Invalid summary/replay, ID mismatch, duplicate attempt, active/unfinalized replay -> Rejected with attempts/revisions/pending unchanged.

- [ ] **Step 2: Write durable/pending tests using fake store/factory**

Before publishing a new attempt, estimate its transaction memory and reserve pending capacity. Construct/compress/validate complete immutable value locally. Insert the pointer into active `m_attempts`; if insertion throws, release reservation and do not append to disk. Then:

```text
no earlier pending revision + append succeeds -> Durable; release reservation; durableRevision = memoryRevision
append fails -> keep complete in-memory value; ledger reservation remains associated with that revision; PendingDurability
already has earlier pending for same context -> do not append newer revision ahead of gap; publish complete value as later PendingDurability
```

No summary/replay half-state is representable.

- [ ] **Step 3: Prove strict retry ordering**

Pending revisions retry oldest-first per context. Revision N+1 is never appended while N remains failed. Successful contiguous retries advance durable revision; failed revision and all later same-context revisions remain pending.

- [ ] **Step 4: Replace separate summary/replay deques with immutable entry pointers**

Queries access `entry->summary` and optional replay. Legacy summary-only entries remain available for history/death restoration; `replayById` returns null when replay state is not Present.

- [ ] **Step 5: Preserve pending entries across context switch**

`prepareContextSwitch()` requires fleet/replay consumers already released by coordinator. For each current-context pending entry, move its shared transaction/reference into a detached ledger slot using its existing reservation, then clear the active view/store. Because the same immutable pointer is shared, replay bytes are not copied. If an invariant violation prevents detachment, return false and leave old active authority intact; do not silently clear it.

- [ ] **Step 6: Retry detached contexts at safe windows**

Create a store from the factory for each pending context. Within a context, retry revisions in order. Success removes its ledger entry/reservation. Failure leaves it intact and may continue retrying independent contexts; never reorder revisions within one context.

- [ ] **Step 7: Handle pending-budget exhaustion honestly**

When no reservation is available while storage is unhealthy, `commit()` returns Rejected before archive publication; existing accepted pending entries are preserved. Coordinator/diagnostics later surface persistence degraded. Gameplay lifecycle remains live, but the system does not falsely promise replay durability for the rejected run.

- [ ] **Step 8: Prove GREEN and commit**

```bash
git add src/EchoReplayArchive.* src/EchoPendingDurability.* src/main.cpp src/EchoGhostFleet.* tests/cpp/test_replay_archive_durability.cpp CMakeLists.txt
git commit -m "refactor: make replay archive commits structurally atomic"
```

---

### Task 6: Make retention and compaction explicit maintenance transactions

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

With small limits/budgets, compute oldest eligible replay payloads. Never evict pending durability or protected existing Best/Latest semantics. Eviction creates replacement immutable entries with same summary, null replay, `EvictedByRetention`.

- [ ] **Step 2: Prove eviction durability ordering**

Append `JournalEviction` first. On failure, active immutable entries remain unchanged. Only after durable eviction transaction acceptance may replay payload pointers be replaced/released.

- [ ] **Step 3: Write compaction tests**

After durable journal transactions, snapshot revision equals durable revision. After promotion/reopen, only journal records <= snapshot revision are compacted away; newer records survive.

- [ ] **Step 4: Implement triggers only, not frame execution**

Recommend maintenance when:

```text
journal transaction count >= 128
OR journal byte size >= 64 MiB
OR retention/disk policy requires eviction
```

No maintenance automatically runs in ordinary live frame update.

- [ ] **Step 5: Prove GREEN and commit**

```bash
git add src/EchoReplayArchive.* src/EchoArchiveStore.* tests/cpp/test_replay_archive_durability.cpp tests/cpp/test_archive_store.cpp
git commit -m "feat: add explicit archive maintenance transactions"
```

---

### Task 7: Integrate transactional durability into coordinator and context lifecycle

**Files:**
- Modify: `src/EchoRuntimeCoordinator.hpp/.cpp`
- Modify: `src/EchoAttemptHistory.cpp`
- Modify: `src/main.cpp`
- Modify: `tests/test_v1_1_contract.py`

**Interfaces:** consumes `ReplayCommitResult`, `ArchiveMaintenanceResult`, `prepareContextSwitch()`.

- [ ] **Step 1: Add failing source contracts**

`main.cpp` contains no `archive.save()` or `archive.ingest(` lifecycle policy. Finalization routes only through coordinator/archive `commit`.

- [ ] **Step 2: Implement finalization order**

```text
recorder.finalizeAttempt
-> resolve immutable finalized attempt
-> prepare history entry without mutation
-> archive.commit(complete summary+replay)
-> if logical archive commit accepted: history.commitPreparedEntry as derived in-memory projection
-> publish session-best/replay/fleet consequences
```

Map Durable -> Committed, PendingDurability -> PendingDurability, Rejected -> Rejected. If derived history projection fails after archive acceptance, reconstruct it from archive summary rather than roll back/delete archive authority.

- [ ] **Step 3: Retry/maintain only at safe windows**

Retry pending durability and optionally maintain during pause/Replay Studio idle, after successful reset boundary, context switch before old active view release, and level exit. Ordinary Playing `postUpdate` does not compact/rewrite history.

- [ ] **Step 4: Harden context switch**

Release fleet/replay consumers, call `prepareContextSwitch()` so pending accepted attempts remain in bounded detached ledger, then load new context. Failure to retry old pending does not destroy it and does not indefinitely block vanilla context transition.

- [ ] **Step 5: Harden exit**

Perform one best-effort retry of active/detached pending + safe maintenance within the existing exit boundary. If storage remains unavailable, preserve in-memory pending until process teardown and emit explicit degraded diagnostics later; do not claim durability or block vanilla exit indefinitely.

- [ ] **Step 6: Prove GREEN and commit**

```bash
git add src/EchoRuntimeCoordinator.* src/EchoAttemptHistory.cpp src/main.cpp tests/test_v1_1_contract.py
git commit -m "fix: integrate transactional replay durability"
```

---

### Task 8: Add adversarial parser, corruption, ordering, and filesystem fault tests

**Files:**
- Create: `tests/cpp/test_archive_corruption.cpp`
- Modify: `tests/cpp/test_replay_journal.cpp`
- Modify: `tests/cpp/test_archive_store.cpp`
- Modify: `tests/cpp/test_pending_durability.cpp`
- Modify: `tests/cpp/test_replay_archive_durability.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Cut journal records at every boundary class**

Partial magic/header/payload/footer, exactly after record1/record2. Earlier full commits always survive.

- [ ] **Step 2: Mutate semantic fields**

Sequence zero, NaN time/progress, out-of-bound coordinates, ID mismatch, duplicate attempts/revisions, invalid enums, context fingerprint, oversized counts. Expect bounded reject/quarantine with no partial publication.

- [ ] **Step 3: Simulate filesystem failure stages**

Append, temp write, flush, backup copy, replacement, post-promotion validation. Failed compaction leaves prior known-good snapshot+journal loadable.

- [ ] **Step 4: Test pending gap and context switch**

Force revision 5 append failure, accept 6 as pending, then make store healthy: retry must append 5 then 6. Switch contexts while 5/6 pending: old immutable data remains in detached ledger and is later durable. Exhaust 16/128MiB budget: older pending stays intact; additional publication rejects/degrades.

- [ ] **Step 5: Add deterministic randomized parser test**

1,000 fixed-seed random buffers through snapshot/journal decode; no exception escape/crash/unbounded allocation.

- [ ] **Step 6: Run Release + Debug CTest and commit**

```powershell
cmake --build build-core-tests --config Release --target EchoDashCoreTests
ctest --test-dir build-core-tests -C Release --output-on-failure
cmake --build build-core-tests --config Debug --target EchoDashCoreTests
ctest --test-dir build-core-tests -C Debug --output-on-failure
```

```bash
git add tests/cpp CMakeLists.txt
git commit -m "test: adversarially verify replay persistence"
```

---

### Task 9: Plan-02 persistence evidence gate

**Files:** modify only if verification reveals a defect.

- [ ] **Step 1: Run complete local suite**

```powershell
python -m unittest discover -s tests -p "test_*.py" -v
cmake -S . -B build-core-tests -DECHO_DASH_BUILD_CORE_TESTS=ON
cmake --build build-core-tests --config Release --target EchoDashCoreTests
ctest --test-dir build-core-tests -C Release --output-on-failure
```

Expected zero failures.

- [ ] **Step 2: Reload committed v1.1.2 fixtures**

Fixture SHA must match Task1. Minimal pair -> LoadedSchema1/Present. Summary-only case -> LegacySummaryOnly. Legacy files are not deleted after migration load.

- [ ] **Step 3: Audit source paths**

No whole archive save per attempt; no compaction/retention in live `postUpdate`; one immutable entry per new attempt; no out-of-order same-context pending append; pending cannot be retention-evicted; context switch cannot silently discard accepted pending; load does not delete rejected evidence.

- [ ] **Step 4: Trigger pinned Windows hardening-dev workflow and wait for terminal completion**

Expected terminal GREEN for Python/native persistence/fault tests, pinned CLI 3.7.4/SDK 5.10.1, Windows Release compile, compiler evidence, and package upload.

- [ ] **Step 5: Record exact Plan-02 source SHA/workflow run ID; proceed to Plan 03 only with terminal evidence.**
