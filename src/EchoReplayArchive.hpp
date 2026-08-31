#pragma once

#include "EchoAttemptHistory.hpp"
#include "EchoRecorder.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <string>

namespace dash_echo {

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

struct ReplayArchiveStats {
    std::size_t summaryCount = 0;
    std::size_t replayCount = 0;
    std::size_t retainedFrames = 0;
    std::uint64_t latestAttemptId = 0;
    std::uint64_t bestRecordedAttemptId = 0;
    float bestRecordedProgress = 0.0f;
    std::uint64_t revision = 0;
    bool dirty = false;
    bool recoveredFromBackup = false;
    std::size_t quarantinedReplayCount = 0;
};

class EchoReplayArchive final {
public:
    static constexpr std::uint32_t kSchemaVersion = 1;

    // v1.1.2 treats every finalized run as archive-worthy. These are safety
    // ceilings, not a normal rolling-history policy; ordinary eviction is
    // driven by the explicit user retention / disk-budget settings.
    static constexpr std::size_t kMaxSummaries = 100'000;
    static constexpr std::size_t kHardMaxReplays = 100'000;
    static constexpr std::size_t kDefaultReplayLimit = 10'000;
    static constexpr std::size_t kMinDiskBudgetMb = 128;
    static constexpr std::size_t kMaxDiskBudgetMb = 8'192;
    static constexpr std::size_t kDefaultDiskBudgetMb = 2'048;
    static constexpr std::uint64_t kHardMaxFramesOnDisk = 50'000'000;
    static constexpr std::uint64_t kHardMaxArchiveBytes = 8ull * 1024ull * 1024ull * 1024ull;

    void configure(
        std::size_t replayLimit,
        std::size_t diskBudgetMb,
        double archiveSampleRate
    );

    bool load(EchoLevelContext const& context);
    bool save();
    void clear();

    bool ingest(
        AttemptRecord const& attempt,
        AttemptHistoryEntry const& summary
    );

    [[nodiscard]] AttemptRecord const* replayById(std::uint64_t attemptId) const;
    [[nodiscard]] AttemptHistoryEntry const* summaryById(std::uint64_t attemptId) const;
    [[nodiscard]] AttemptRecord const* latestReplay() const;
    [[nodiscard]] AttemptRecord const* bestRecordedReplay() const;
    [[nodiscard]] AttemptHistoryEntry const* latestSummary() const;
    [[nodiscard]] AttemptHistoryEntry const* bestRecordedSummary() const;

    [[nodiscard]] std::uint64_t previousReplayId(std::uint64_t attemptId) const;
    [[nodiscard]] std::uint64_t nextReplayId(std::uint64_t attemptId) const;
    [[nodiscard]] std::uint64_t maxAttemptId() const;

    [[nodiscard]] std::deque<AttemptHistoryEntry> const& summaries() const;
    [[nodiscard]] std::deque<AttemptRecord> const& replays() const;
    [[nodiscard]] EchoLevelContext const& context() const;
    [[nodiscard]] ReplayArchiveStats stats() const;
    [[nodiscard]] bool isLoaded() const;
    [[nodiscard]] bool isDirty() const;

    [[nodiscard]] static std::uint64_t stableNameHash(std::string const& text);

private:
    struct LoadCandidate {
        std::deque<AttemptHistoryEntry> summaries;
        std::deque<AttemptRecord> replays;
        std::size_t quarantinedReplayCount = 0;
        std::size_t semanticIssueCount = 0;
    };

    [[nodiscard]] AttemptRecord compressReplay(AttemptRecord const& source) const;
    [[nodiscard]] std::filesystem::path archiveDirectory() const;
    [[nodiscard]] std::filesystem::path archivePath() const;
    [[nodiscard]] std::filesystem::path backupPath() const;
    [[nodiscard]] std::size_t estimatedSerializedBytes() const;
    [[nodiscard]] std::uint64_t bestRecordedAttemptId() const;
    [[nodiscard]] float bestRecordedProgress() const;

    [[nodiscard]] bool loadCandidate(
        std::filesystem::path const& path,
        EchoLevelContext const& context,
        LoadCandidate& candidate
    ) const;
    [[nodiscard]] bool validateCandidateFile(
        std::filesystem::path const& path,
        EchoLevelContext const& context
    ) const;

    [[nodiscard]] static bool validatePlayerSnapshot(PlayerSnapshot const& player);
    [[nodiscard]] static bool validateCameraSnapshot(CameraSnapshot const& camera);
    [[nodiscard]] static bool validateFrame(
        FrameRecord const& frame,
        std::uint64_t previousSequence,
        double previousTime,
        bool hasPrevious
    );
    [[nodiscard]] static bool validateReplay(AttemptRecord const& attempt);
    [[nodiscard]] static bool validateSummary(AttemptHistoryEntry const& summary);

    void trimSummaries();
    void trimReplays();
    void markDirty();

    EchoLevelContext m_context;
    std::deque<AttemptHistoryEntry> m_summaries;
    std::deque<AttemptRecord> m_replays;
    std::size_t m_replayLimit = kDefaultReplayLimit;
    std::size_t m_diskBudgetMb = kDefaultDiskBudgetMb;
    double m_archiveSampleRate = 120.0;
    std::uint64_t m_revision = 0;
    bool m_loaded = false;
    bool m_dirty = false;
    bool m_recoveredFromBackup = false;
    std::size_t m_quarantinedReplayCount = 0;
};

} // namespace dash_echo
