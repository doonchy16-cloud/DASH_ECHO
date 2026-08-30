#pragma once

#include "EchoDeathAnalytics.hpp"
#include "EchoRecorder.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>

namespace dash_echo {

enum class AttemptOutcome : std::uint8_t {
    Death,
    Reset,
    Completed,
    LayerExit
};

struct AttemptDeathSummary {
    bool present = false;
    std::uint64_t eventId = 0;
    std::uint8_t playerIndex = 1;
    double timeSeconds = 0.0;
    float progressPercent = 0.0f;
    float x = 0.0f;
    float y = 0.0f;
    bool hazardPresent = false;
    int hazardObjectId = 0;
    float hazardX = 0.0f;
    float hazardY = 0.0f;
};

struct AttemptHistoryEntry {
    std::uint64_t attemptId = 0;
    AttemptOutcome outcome = AttemptOutcome::Reset;
    AttemptEndReason sourceEndReason = AttemptEndReason::Reset;

    float maxProgressPercent = 0.0f;
    double durationSeconds = 0.0;
    std::size_t capturedFrameCount = 0;
    std::uint64_t droppedFrameCount = 0;
    double firstCapturedTimeSeconds = 0.0;
    double lastCapturedTimeSeconds = 0.0;

    bool completed = false;
    bool personalBestAtFinalization = false;
    float priorBestProgressPercent = 0.0f;
    float improvementPercent = 0.0f;

    AttemptDeathSummary death;
};

struct AttemptHistoryStats {
    std::uint64_t totalCommittedAttempts = 0;
    std::size_t retainedEntries = 0;
    std::uint64_t totalDeaths = 0;
    std::uint64_t totalManualResets = 0;
    std::uint64_t totalCompletions = 0;
    std::uint64_t totalLayerExits = 0;
    std::uint64_t totalPersonalBests = 0;
    std::uint64_t duplicateCommitsRejected = 0;
    std::uint64_t currentPersonalBestAttemptId = 0;
    float currentBestProgressPercent = 0.0f;
    double longestAttemptSeconds = 0.0;
    std::uint64_t revision = 0;
};

class EchoAttemptHistory final {
public:
    static constexpr std::size_t kMaxHistoryEntries = 4096;

    bool commitFinalizedAttempt(
        AttemptRecord const& attempt,
        DeathEvent const* death,
        float priorPersonalBestProgress,
        std::uint64_t currentPersonalBestAttemptId
    );
    void clear();

    [[nodiscard]] AttemptHistoryEntry const* entryForAttempt(
        std::uint64_t attemptId
    ) const;
    [[nodiscard]] AttemptHistoryEntry const* latestEntry() const;
    [[nodiscard]] std::deque<AttemptHistoryEntry> const& entries() const;
    [[nodiscard]] AttemptHistoryStats stats() const;
    [[nodiscard]] std::uint64_t revision() const;

private:
    static AttemptOutcome resolveOutcome(
        AttemptRecord const& attempt,
        DeathEvent const* death
    );
    static AttemptDeathSummary copyDeathSummary(
        AttemptRecord const& attempt,
        DeathEvent const* death
    );

    void trimRetention();

    std::deque<AttemptHistoryEntry> m_entries;
    std::uint64_t m_totalCommittedAttempts = 0;
    std::uint64_t m_totalDeaths = 0;
    std::uint64_t m_totalManualResets = 0;
    std::uint64_t m_totalCompletions = 0;
    std::uint64_t m_totalLayerExits = 0;
    std::uint64_t m_totalPersonalBests = 0;
    std::uint64_t m_duplicateCommitsRejected = 0;
    std::uint64_t m_currentPersonalBestAttemptId = 0;
    float m_currentBestProgressPercent = 0.0f;
    double m_longestAttemptSeconds = 0.0;
    std::uint64_t m_revision = 0;
};

} // namespace dash_echo
