#include "EchoAttemptHistory.hpp"

#include <algorithm>
#include <cmath>

namespace dash_echo {

namespace {

float finitePercent(float value) {
    if (!std::isfinite(value)) return 0.0f;
    return std::clamp(value, 0.0f, 100.0f);
}

double finiteNonNegative(double value) {
    if (!std::isfinite(value)) return 0.0;
    return std::max(0.0, value);
}

} // namespace

bool EchoAttemptHistory::commitFinalizedAttempt(
    AttemptRecord const& attempt,
    DeathEvent const* death,
    float priorPersonalBestProgress,
    std::uint64_t currentPersonalBestAttemptId
) {
    if (attempt.attemptId == 0 || !attempt.finalized) {
        return false;
    }

    if (entryForAttempt(attempt.attemptId)) {
        ++m_duplicateCommitsRejected;
        ++m_revision;
        return false;
    }

    AttemptHistoryEntry entry;
    entry.attemptId = attempt.attemptId;
    entry.outcome = resolveOutcome(attempt, death);
    entry.sourceEndReason = attempt.endReason;
    entry.maxProgressPercent = finitePercent(attempt.maxProgressPercent);
    entry.durationSeconds = finiteNonNegative(attempt.durationSeconds);
    entry.capturedFrameCount = attempt.frames.size();
    entry.droppedFrameCount = attempt.framesDropped;

    if (!attempt.frames.empty()) {
        entry.firstCapturedTimeSeconds = finiteNonNegative(
            attempt.frames.front().timeSeconds
        );
        entry.lastCapturedTimeSeconds = finiteNonNegative(
            attempt.frames.back().timeSeconds
        );
    }

    entry.completed = attempt.endReason == AttemptEndReason::Completed;
    entry.personalBestAtFinalization =
        currentPersonalBestAttemptId != 0 &&
        currentPersonalBestAttemptId == attempt.attemptId;

    entry.priorBestProgressPercent = finitePercent(priorPersonalBestProgress);
    entry.improvementPercent = std::max(
        0.0f,
        entry.maxProgressPercent - entry.priorBestProgressPercent
    );
    entry.death = copyDeathSummary(attempt, death);

    m_entries.push_back(entry);
    ++m_totalCommittedAttempts;

    switch (entry.outcome) {
        case AttemptOutcome::Death:
            ++m_totalDeaths;
            break;
        case AttemptOutcome::Reset:
            ++m_totalManualResets;
            break;
        case AttemptOutcome::Completed:
            ++m_totalCompletions;
            break;
        case AttemptOutcome::LayerExit:
            ++m_totalLayerExits;
            break;
    }

    if (entry.personalBestAtFinalization) {
        ++m_totalPersonalBests;
        m_currentPersonalBestAttemptId = entry.attemptId;
        m_currentBestProgressPercent = entry.maxProgressPercent;
    } else if (currentPersonalBestAttemptId != 0) {
        m_currentPersonalBestAttemptId = currentPersonalBestAttemptId;
    }

    m_longestAttemptSeconds = std::max(
        m_longestAttemptSeconds,
        entry.durationSeconds
    );

    trimRetention();
    ++m_revision;
    return true;
}

void EchoAttemptHistory::clear() {
    m_entries.clear();
    m_totalCommittedAttempts = 0;
    m_totalDeaths = 0;
    m_totalManualResets = 0;
    m_totalCompletions = 0;
    m_totalLayerExits = 0;
    m_totalPersonalBests = 0;
    m_duplicateCommitsRejected = 0;
    m_currentPersonalBestAttemptId = 0;
    m_currentBestProgressPercent = 0.0f;
    m_longestAttemptSeconds = 0.0;
    ++m_revision;
}

AttemptHistoryEntry const* EchoAttemptHistory::entryForAttempt(
    std::uint64_t attemptId
) const {
    if (attemptId == 0) return nullptr;

    for (auto it = m_entries.rbegin(); it != m_entries.rend(); ++it) {
        if (it->attemptId == attemptId) {
            return &*it;
        }
    }
    return nullptr;
}

AttemptHistoryEntry const* EchoAttemptHistory::latestEntry() const {
    if (m_entries.empty()) return nullptr;
    return &m_entries.back();
}

std::deque<AttemptHistoryEntry> const& EchoAttemptHistory::entries() const {
    return m_entries;
}

AttemptHistoryStats EchoAttemptHistory::stats() const {
    return AttemptHistoryStats {
        m_totalCommittedAttempts,
        m_entries.size(),
        m_totalDeaths,
        m_totalManualResets,
        m_totalCompletions,
        m_totalLayerExits,
        m_totalPersonalBests,
        m_duplicateCommitsRejected,
        m_currentPersonalBestAttemptId,
        m_currentBestProgressPercent,
        m_longestAttemptSeconds,
        m_revision
    };
}

std::uint64_t EchoAttemptHistory::revision() const {
    return m_revision;
}

AttemptOutcome EchoAttemptHistory::resolveOutcome(
    AttemptRecord const& attempt,
    DeathEvent const* death
) {
    if (death && death->attemptId == attempt.attemptId) {
        return AttemptOutcome::Death;
    }

    switch (attempt.endReason) {
        case AttemptEndReason::Completed:
            return AttemptOutcome::Completed;
        case AttemptEndReason::LayerExit:
            return AttemptOutcome::LayerExit;
        case AttemptEndReason::Reset:
        case AttemptEndReason::Active:
            return AttemptOutcome::Reset;
    }

    return AttemptOutcome::Reset;
}

AttemptDeathSummary EchoAttemptHistory::copyDeathSummary(
    AttemptRecord const& attempt,
    DeathEvent const* death
) {
    AttemptDeathSummary result;
    if (!death || death->attemptId != attempt.attemptId) {
        return result;
    }

    result.present = true;
    result.eventId = death->eventId;
    result.playerIndex = death->playerIndex == 2 ? 2 : 1;
    result.progressPercent = finitePercent(death->progressPercent);
    result.x = std::isfinite(death->x) ? death->x : 0.0f;
    result.y = std::isfinite(death->y) ? death->y : 0.0f;
    result.hazardPresent = death->hazardPresent;
    result.hazardObjectId = death->hazardPresent ? death->hazardObjectId : 0;
    result.hazardX =
        death->hazardPresent && std::isfinite(death->hazardX) ?
        death->hazardX : 0.0f;
    result.hazardY =
        death->hazardPresent && std::isfinite(death->hazardY) ?
        death->hazardY : 0.0f;
    return result;
}

void EchoAttemptHistory::trimRetention() {
    while (m_entries.size() > kMaxHistoryEntries) {
        auto eviction = m_entries.end();

        for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
            if (
                m_currentPersonalBestAttemptId != 0 &&
                it->attemptId == m_currentPersonalBestAttemptId
            ) {
                continue;
            }
            eviction = it;
            break;
        }

        if (eviction == m_entries.end()) {
            break;
        }
        m_entries.erase(eviction);
    }
}

} // namespace dash_echo
