#include "EchoReplayArchive.hpp"

#include <Geode/Geode.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <system_error>
#include <unordered_set>

namespace dash_echo {

namespace {

constexpr std::array<char, 8> kMagic {'E', 'C', 'H', 'O', 'D', 'A', 'S', 'H'};
constexpr std::uint32_t kMaxStoredStringBytes = 4096;
constexpr std::size_t kApproxSummaryBytes = 160;
constexpr std::size_t kApproxFrameBytes = 104;
constexpr double kMaxStoredDurationSeconds = 10'000'000.0;
constexpr float kMaxAbsCoordinate = 1'000'000'000.0f;
constexpr float kMaxAbsRotation = 1'000'000'000.0f;
constexpr float kMaxAbsScale = 1'000'000.0f;
constexpr double kDurationSlackSeconds = 2.0;

bool finiteBounded(float value, float absoluteLimit) {
    return std::isfinite(value) && std::abs(value) <= absoluteLimit;
}

template <class T>
bool writePod(std::ostream& out, T const& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    out.write(reinterpret_cast<char const*>(&value), sizeof(T));
    return static_cast<bool>(out);
}

template <class T>
bool readPod(std::istream& in, T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
    return static_cast<bool>(in);
}

bool writeBool(std::ostream& out, bool value) {
    std::uint8_t const raw = value ? 1 : 0;
    return writePod(out, raw);
}

bool readBool(std::istream& in, bool& value) {
    std::uint8_t raw = 0;
    if (!readPod(in, raw) || raw > 1) return false;
    value = raw != 0;
    return true;
}

bool writeString(std::ostream& out, std::string const& value) {
    if (value.size() > kMaxStoredStringBytes) return false;
    auto const size = static_cast<std::uint32_t>(value.size());
    if (!writePod(out, size)) return false;
    if (size > 0) out.write(value.data(), size);
    return static_cast<bool>(out);
}

bool readString(std::istream& in, std::string& value) {
    std::uint32_t size = 0;
    if (!readPod(in, size) || size > kMaxStoredStringBytes) return false;
    value.assign(size, '\0');
    if (size > 0) in.read(value.data(), size);
    return static_cast<bool>(in);
}

bool writeColor(std::ostream& out, ColorRGB const& color) {
    return
        writePod(out, color.r) &&
        writePod(out, color.g) &&
        writePod(out, color.b);
}

bool readColor(std::istream& in, ColorRGB& color) {
    return
        readPod(in, color.r) &&
        readPod(in, color.g) &&
        readPod(in, color.b);
}

bool writePlayer(std::ostream& out, PlayerSnapshot const& player) {
    auto const mode = static_cast<std::uint8_t>(player.mode);
    return
        writeBool(out, player.present) &&
        writeBool(out, player.visible) &&
        writePod(out, mode) &&
        writeColor(out, player.color1) &&
        writeColor(out, player.color2) &&
        writePod(out, player.x) &&
        writePod(out, player.y) &&
        writePod(out, player.rotation) &&
        writePod(out, player.scaleX) &&
        writePod(out, player.scaleY);
}

bool readPlayer(std::istream& in, PlayerSnapshot& player) {
    std::uint8_t mode = 0;
    if (
        !readBool(in, player.present) ||
        !readBool(in, player.visible) ||
        !readPod(in, mode) ||
        mode > static_cast<std::uint8_t>(PlayerMode::Swing) ||
        !readColor(in, player.color1) ||
        !readColor(in, player.color2) ||
        !readPod(in, player.x) ||
        !readPod(in, player.y) ||
        !readPod(in, player.rotation) ||
        !readPod(in, player.scaleX) ||
        !readPod(in, player.scaleY)
    ) return false;

    player.mode = static_cast<PlayerMode>(mode);
    return true;
}

bool writeCamera(std::ostream& out, CameraSnapshot const& camera) {
    return
        writeBool(out, camera.present) &&
        writePod(out, camera.x) &&
        writePod(out, camera.y) &&
        writePod(out, camera.rotation) &&
        writePod(out, camera.scaleX) &&
        writePod(out, camera.scaleY);
}

bool readCamera(std::istream& in, CameraSnapshot& camera) {
    return
        readBool(in, camera.present) &&
        readPod(in, camera.x) &&
        readPod(in, camera.y) &&
        readPod(in, camera.rotation) &&
        readPod(in, camera.scaleX) &&
        readPod(in, camera.scaleY);
}

bool writeFrame(std::ostream& out, FrameRecord const& frame) {
    return
        writePod(out, frame.sequence) &&
        writePod(out, frame.timeSeconds) &&
        writePod(out, frame.progressPercent) &&
        writePlayer(out, frame.player1) &&
        writePlayer(out, frame.player2) &&
        writeCamera(out, frame.camera) &&
        writeBool(out, frame.player1ContinuousFromPrevious) &&
        writeBool(out, frame.player2ContinuousFromPrevious) &&
        writeBool(out, frame.cameraContinuousFromPrevious);
}

bool readFrame(std::istream& in, FrameRecord& frame) {
    return
        readPod(in, frame.sequence) &&
        readPod(in, frame.timeSeconds) &&
        readPod(in, frame.progressPercent) &&
        readPlayer(in, frame.player1) &&
        readPlayer(in, frame.player2) &&
        readCamera(in, frame.camera) &&
        readBool(in, frame.player1ContinuousFromPrevious) &&
        readBool(in, frame.player2ContinuousFromPrevious) &&
        readBool(in, frame.cameraContinuousFromPrevious);
}

bool writeDeathSummary(std::ostream& out, AttemptDeathSummary const& death) {
    return
        writeBool(out, death.present) &&
        writePod(out, death.eventId) &&
        writePod(out, death.playerIndex) &&
        writePod(out, death.timeSeconds) &&
        writePod(out, death.progressPercent) &&
        writePod(out, death.x) &&
        writePod(out, death.y) &&
        writeBool(out, death.hazardPresent) &&
        writePod(out, death.hazardObjectId) &&
        writePod(out, death.hazardX) &&
        writePod(out, death.hazardY);
}

bool readDeathSummary(std::istream& in, AttemptDeathSummary& death) {
    return
        readBool(in, death.present) &&
        readPod(in, death.eventId) &&
        readPod(in, death.playerIndex) &&
        readPod(in, death.timeSeconds) &&
        readPod(in, death.progressPercent) &&
        readPod(in, death.x) &&
        readPod(in, death.y) &&
        readBool(in, death.hazardPresent) &&
        readPod(in, death.hazardObjectId) &&
        readPod(in, death.hazardX) &&
        readPod(in, death.hazardY);
}

bool writeSummary(std::ostream& out, AttemptHistoryEntry const& summary) {
    auto const outcome = static_cast<std::uint8_t>(summary.outcome);
    auto const reason = static_cast<std::uint8_t>(summary.sourceEndReason);
    auto const captured = static_cast<std::uint64_t>(summary.capturedFrameCount);
    return
        writePod(out, summary.attemptId) &&
        writePod(out, outcome) &&
        writePod(out, reason) &&
        writePod(out, summary.maxProgressPercent) &&
        writePod(out, summary.durationSeconds) &&
        writePod(out, captured) &&
        writePod(out, summary.droppedFrameCount) &&
        writePod(out, summary.firstCapturedTimeSeconds) &&
        writePod(out, summary.lastCapturedTimeSeconds) &&
        writeBool(out, summary.completed) &&
        writeBool(out, summary.personalBestAtFinalization) &&
        writePod(out, summary.priorBestProgressPercent) &&
        writePod(out, summary.improvementPercent) &&
        writeDeathSummary(out, summary.death);
}

bool readSummary(std::istream& in, AttemptHistoryEntry& summary) {
    std::uint8_t outcome = 0;
    std::uint8_t reason = 0;
    std::uint64_t captured = 0;
    if (
        !readPod(in, summary.attemptId) ||
        !readPod(in, outcome) ||
        outcome > static_cast<std::uint8_t>(AttemptOutcome::LayerExit) ||
        !readPod(in, reason) ||
        reason > static_cast<std::uint8_t>(AttemptEndReason::LayerExit) ||
        !readPod(in, summary.maxProgressPercent) ||
        !readPod(in, summary.durationSeconds) ||
        !readPod(in, captured) ||
        captured > EchoReplayArchive::kHardMaxFramesOnDisk ||
        !readPod(in, summary.droppedFrameCount) ||
        !readPod(in, summary.firstCapturedTimeSeconds) ||
        !readPod(in, summary.lastCapturedTimeSeconds) ||
        !readBool(in, summary.completed) ||
        !readBool(in, summary.personalBestAtFinalization) ||
        !readPod(in, summary.priorBestProgressPercent) ||
        !readPod(in, summary.improvementPercent) ||
        !readDeathSummary(in, summary.death)
    ) return false;

    summary.outcome = static_cast<AttemptOutcome>(outcome);
    summary.sourceEndReason = static_cast<AttemptEndReason>(reason);
    summary.capturedFrameCount = static_cast<std::size_t>(captured);
    return true;
}

bool writeAttempt(std::ostream& out, AttemptRecord const& attempt) {
    auto const reason = static_cast<std::uint8_t>(attempt.endReason);
    auto const frameCount = static_cast<std::uint64_t>(attempt.frames.size());
    if (
        !writePod(out, attempt.attemptId) ||
        !writePod(out, attempt.framesDropped) ||
        !writePod(out, attempt.durationSeconds) ||
        !writePod(out, attempt.maxProgressPercent) ||
        !writePod(out, reason) ||
        !writeBool(out, attempt.finalized) ||
        !writePod(out, frameCount)
    ) return false;

    for (auto const& frame : attempt.frames) {
        if (!writeFrame(out, frame)) return false;
    }
    return true;
}

bool readAttempt(std::istream& in, AttemptRecord& attempt, std::uint64_t& totalFrames) {
    std::uint8_t reason = 0;
    std::uint64_t frameCount = 0;
    if (
        !readPod(in, attempt.attemptId) ||
        !readPod(in, attempt.framesDropped) ||
        !readPod(in, attempt.durationSeconds) ||
        !readPod(in, attempt.maxProgressPercent) ||
        !readPod(in, reason) ||
        reason > static_cast<std::uint8_t>(AttemptEndReason::LayerExit) ||
        !readBool(in, attempt.finalized) ||
        !readPod(in, frameCount) ||
        frameCount > EchoReplayArchive::kHardMaxFramesOnDisk ||
        totalFrames + frameCount > EchoReplayArchive::kHardMaxFramesOnDisk
    ) return false;

    attempt.endReason = static_cast<AttemptEndReason>(reason);
    attempt.frames.resize(static_cast<std::size_t>(frameCount));
    for (auto& frame : attempt.frames) {
        if (!readFrame(in, frame)) return false;
    }
    totalFrames += frameCount;
    return true;
}

bool betterProgress(float candidateProgress, std::uint64_t candidateId,
                    float incumbentProgress, std::uint64_t incumbentId) {
    if (candidateProgress > incumbentProgress) return true;
    if (candidateProgress < incumbentProgress) return false;
    return candidateId > incumbentId;
}

} // namespace

std::string EchoLevelContext::storageKey() const {
    auto const identity = levelId != 0
        ? std::string("id_") + std::to_string(levelId)
        : std::string("local_") + std::to_string(fallbackHash);
    return identity + (platformer ? "_platformer" : "_classic") +
        (practice ? "_practice" : "_normal");
}

bool EchoLevelContext::matches(EchoLevelContext const& other) const {
    return
        levelId == other.levelId &&
        fallbackHash == other.fallbackHash &&
        platformer == other.platformer &&
        practice == other.practice;
}

void EchoReplayArchive::configure(
    std::size_t replayLimit,
    std::size_t diskBudgetMb,
    double archiveSampleRate
) {
    auto const oldReplayCount = m_replays.size();
    m_replayLimit = std::clamp<std::size_t>(replayLimit, 1, kHardMaxReplays);
    m_diskBudgetMb = std::clamp<std::size_t>(
        diskBudgetMb,
        kMinDiskBudgetMb,
        kMaxDiskBudgetMb
    );
    if (!std::isfinite(archiveSampleRate)) archiveSampleRate = 120.0;
    m_archiveSampleRate = std::clamp(archiveSampleRate, 30.0, 240.0);
    trimReplays();
    if (m_replays.size() != oldReplayCount) markDirty();
}

bool EchoReplayArchive::validatePlayerSnapshot(PlayerSnapshot const& player) {
    return
        finiteBounded(player.x, kMaxAbsCoordinate) &&
        finiteBounded(player.y, kMaxAbsCoordinate) &&
        finiteBounded(player.rotation, kMaxAbsRotation) &&
        finiteBounded(player.scaleX, kMaxAbsScale) &&
        finiteBounded(player.scaleY, kMaxAbsScale);
}

bool EchoReplayArchive::validateCameraSnapshot(CameraSnapshot const& camera) {
    return
        finiteBounded(camera.x, kMaxAbsCoordinate) &&
        finiteBounded(camera.y, kMaxAbsCoordinate) &&
        finiteBounded(camera.rotation, kMaxAbsRotation) &&
        finiteBounded(camera.scaleX, kMaxAbsScale) &&
        finiteBounded(camera.scaleY, kMaxAbsScale);
}

bool EchoReplayArchive::validateFrame(
    FrameRecord const& frame,
    std::uint64_t previousSequence,
    double previousTime,
    bool hasPrevious
) {
    if (frame.sequence == 0) return false;
    if (!std::isfinite(frame.timeSeconds) || frame.timeSeconds < 0.0) return false;
    if (
        !std::isfinite(frame.progressPercent) ||
        frame.progressPercent < 0.0f ||
        frame.progressPercent > 100.0f
    ) return false;
    if (hasPrevious && frame.timeSeconds < previousTime) return false;
    if (hasPrevious && frame.sequence <= previousSequence) return false;
    if (!validatePlayerSnapshot(frame.player1)) return false;
    if (!validatePlayerSnapshot(frame.player2)) return false;
    if (!validateCameraSnapshot(frame.camera)) return false;
    return true;
}

bool EchoReplayArchive::validateReplay(AttemptRecord const& attempt) {
    if (
        attempt.attemptId == 0 ||
        !attempt.finalized ||
        attempt.endReason == AttemptEndReason::Active ||
        attempt.frames.empty() ||
        attempt.frames.size() > kHardMaxFramesOnDisk ||
        !std::isfinite(attempt.durationSeconds) ||
        attempt.durationSeconds < 0.0 ||
        attempt.durationSeconds > kMaxStoredDurationSeconds ||
        !std::isfinite(attempt.maxProgressPercent) ||
        attempt.maxProgressPercent < 0.0f ||
        attempt.maxProgressPercent > 100.0f
    ) return false;

    bool hasPrevious = false;
    std::uint64_t previousSequence = 0;
    double previousTime = 0.0;
    float observedMaxProgress = 0.0f;

    for (auto const& frame : attempt.frames) {
        if (!validateFrame(frame, previousSequence, previousTime, hasPrevious)) {
            return false;
        }
        hasPrevious = true;
        previousSequence = frame.sequence;
        previousTime = frame.timeSeconds;
        observedMaxProgress = std::max(observedMaxProgress, frame.progressPercent);
    }

    if (previousTime > attempt.durationSeconds + kDurationSlackSeconds) return false;
    if (observedMaxProgress > attempt.maxProgressPercent + 0.01f) return false;
    return true;
}

bool EchoReplayArchive::validateSummary(AttemptHistoryEntry const& summary) {
    if (summary.attemptId == 0) return false;
    if (
        !std::isfinite(summary.maxProgressPercent) ||
        summary.maxProgressPercent < 0.0f ||
        summary.maxProgressPercent > 100.0f ||
        !std::isfinite(summary.durationSeconds) ||
        summary.durationSeconds < 0.0 ||
        summary.durationSeconds > kMaxStoredDurationSeconds ||
        !std::isfinite(summary.firstCapturedTimeSeconds) ||
        !std::isfinite(summary.lastCapturedTimeSeconds) ||
        summary.firstCapturedTimeSeconds < 0.0 ||
        summary.lastCapturedTimeSeconds < summary.firstCapturedTimeSeconds ||
        summary.lastCapturedTimeSeconds > summary.durationSeconds + kDurationSlackSeconds ||
        !std::isfinite(summary.priorBestProgressPercent) ||
        summary.priorBestProgressPercent < 0.0f ||
        summary.priorBestProgressPercent > 100.0f ||
        !std::isfinite(summary.improvementPercent) ||
        summary.improvementPercent < -100.0f ||
        summary.improvementPercent > 100.0f
    ) return false;

    if (summary.death.present) {
        auto const& death = summary.death;
        if (
            (death.playerIndex != 1 && death.playerIndex != 2) ||
            !std::isfinite(death.timeSeconds) ||
            death.timeSeconds < 0.0 ||
            death.timeSeconds > summary.durationSeconds + kDurationSlackSeconds ||
            !std::isfinite(death.progressPercent) ||
            death.progressPercent < 0.0f ||
            death.progressPercent > 100.0f ||
            !finiteBounded(death.x, kMaxAbsCoordinate) ||
            !finiteBounded(death.y, kMaxAbsCoordinate) ||
            !finiteBounded(death.hazardX, kMaxAbsCoordinate) ||
            !finiteBounded(death.hazardY, kMaxAbsCoordinate)
        ) return false;
    }

    return true;
}

bool EchoReplayArchive::loadCandidate(
    std::filesystem::path const& path,
    EchoLevelContext const& context,
    LoadCandidate& candidate
) const {
    candidate = LoadCandidate {};

    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) return false;

    auto const bytes = std::filesystem::file_size(path, ec);
    if (ec || bytes > kHardMaxArchiveBytes) return false;

    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    std::array<char, 8> magic {};
    in.read(magic.data(), magic.size());
    std::uint32_t schema = 0;
    EchoLevelContext stored;
    std::uint8_t platformer = 0;
    std::uint8_t practice = 0;
    std::uint64_t summaryCount = 0;
    std::uint64_t replayCount = 0;

    if (
        !in || magic != kMagic ||
        !readPod(in, schema) || schema != kSchemaVersion ||
        !readPod(in, stored.levelId) ||
        !readPod(in, stored.fallbackHash) ||
        !readPod(in, platformer) || platformer > 1 ||
        !readPod(in, practice) || practice > 1 ||
        !readPod(in, stored.gdNormalPercent) ||
        !readString(in, stored.levelName) ||
        !readPod(in, summaryCount) || summaryCount > kMaxSummaries ||
        !readPod(in, replayCount) || replayCount > kHardMaxReplays
    ) return false;

    stored.platformer = platformer != 0;
    stored.practice = practice != 0;
    if (!stored.matches(context)) return false;

    std::unordered_set<std::uint64_t> summaryIds;
    for (std::uint64_t i = 0; i < summaryCount; ++i) {
        AttemptHistoryEntry entry;
        if (!readSummary(in, entry)) return false;

        bool const unique = entry.attemptId != 0 && summaryIds.insert(entry.attemptId).second;
        if (!unique || !validateSummary(entry)) {
            ++candidate.semanticIssueCount;
            continue;
        }
        candidate.summaries.push_back(std::move(entry));
    }

    std::unordered_set<std::uint64_t> replayIds;
    std::uint64_t totalFrames = 0;
    for (std::uint64_t i = 0; i < replayCount; ++i) {
        AttemptRecord attempt;
        if (!readAttempt(in, attempt, totalFrames)) return false;

        bool const unique = attempt.attemptId != 0 && replayIds.insert(attempt.attemptId).second;
        if (unique && validateReplay(attempt)) {
            candidate.replays.push_back(std::move(attempt));
        } else {
            ++candidate.quarantinedReplayCount;
            ++candidate.semanticIssueCount;
        }
    }

    if (!in) return false;
    if (in.peek() != std::char_traits<char>::eof()) return false;
    return true;
}

bool EchoReplayArchive::validateCandidateFile(
    std::filesystem::path const& path,
    EchoLevelContext const& context
) const {
    LoadCandidate candidate;
    return
        loadCandidate(path, context, candidate) &&
        candidate.semanticIssueCount == 0;
}

bool EchoReplayArchive::load(EchoLevelContext const& context) {
    m_context = context;
    m_summaries.clear();
    m_replays.clear();
    m_loaded = true;
    m_dirty = false;
    m_recoveredFromBackup = false;
    m_quarantinedReplayCount = 0;
    ++m_revision;

    auto const path = archivePath();
    auto const backup = backupPath();
    std::error_code ec;
    bool const primaryExists = std::filesystem::exists(path, ec) && !ec;
    ec.clear();
    bool const backupExists = std::filesystem::exists(backup, ec) && !ec;

    if (!primaryExists && !backupExists) return true;

    LoadCandidate candidate;
    if (primaryExists && loadCandidate(path, context, candidate)) {
        m_summaries = std::move(candidate.summaries);
        m_replays = std::move(candidate.replays);
        m_quarantinedReplayCount = candidate.quarantinedReplayCount;
        trimSummaries();
        trimReplays();
        m_dirty = candidate.semanticIssueCount != 0;
        ++m_revision;

        if (m_quarantinedReplayCount > 0) {
            geode::log::warn(
                "ECHO_DASH quarantined {} semantically invalid replay(s) from {}",
                m_quarantinedReplayCount,
                path.string()
            );
        }
        return true;
    }

    if (backupExists && loadCandidate(backup, context, candidate)) {
        m_summaries = std::move(candidate.summaries);
        m_replays = std::move(candidate.replays);
        m_quarantinedReplayCount = candidate.quarantinedReplayCount;
        m_recoveredFromBackup = true;
        trimSummaries();
        trimReplays();
        m_dirty = candidate.semanticIssueCount != 0;
        ++m_revision;

        std::error_code restoreError;
        std::filesystem::copy_file(
            backup,
            path,
            std::filesystem::copy_options::overwrite_existing,
            restoreError
        );
        if (restoreError) {
            geode::log::warn(
                "ECHO_DASH loaded known-good backup but could not restore primary {}: {}",
                path.string(),
                restoreError.message()
            );
        } else {
            geode::log::warn(
                "ECHO_DASH recovered archive {} from retained backup",
                path.string()
            );
        }

        if (m_quarantinedReplayCount > 0) {
            geode::log::warn(
                "ECHO_DASH backup recovery quarantined {} semantically invalid replay(s)",
                m_quarantinedReplayCount
            );
        }
        return true;
    }

    geode::log::warn(
        "ECHO_DASH rejected both primary and backup archive candidates for {}",
        context.storageKey()
    );
    return false;
}

bool EchoReplayArchive::save() {
    if (!m_loaded) return false;
    if (!m_dirty) return true;

    auto const dir = archiveDirectory();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) return false;

    auto const path = archivePath();
    auto const temp = std::filesystem::path(path.string() + ".tmp");
    auto const backup = backupPath();

    std::filesystem::remove(temp, ec);
    ec.clear();

    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) return false;

        out.write(kMagic.data(), kMagic.size());
        std::uint8_t const platformer = m_context.platformer ? 1 : 0;
        std::uint8_t const practice = m_context.practice ? 1 : 0;
        auto const summaryCount = static_cast<std::uint64_t>(m_summaries.size());
        auto const replayCount = static_cast<std::uint64_t>(m_replays.size());

        if (
            !writePod(out, kSchemaVersion) ||
            !writePod(out, m_context.levelId) ||
            !writePod(out, m_context.fallbackHash) ||
            !writePod(out, platformer) ||
            !writePod(out, practice) ||
            !writePod(out, m_context.gdNormalPercent) ||
            !writeString(out, m_context.levelName) ||
            !writePod(out, summaryCount) ||
            !writePod(out, replayCount)
        ) {
            out.close();
            std::filesystem::remove(temp, ec);
            return false;
        }

        for (auto const& summary : m_summaries) {
            if (!validateSummary(summary) || !writeSummary(out, summary)) {
                out.close();
                std::filesystem::remove(temp, ec);
                return false;
            }
        }
        for (auto const& replay : m_replays) {
            if (!validateReplay(replay) || !writeAttempt(out, replay)) {
                out.close();
                std::filesystem::remove(temp, ec);
                return false;
            }
        }
        out.flush();
        if (!out) {
            out.close();
            std::filesystem::remove(temp, ec);
            return false;
        }
    }

    if (!validateCandidateFile(temp, m_context)) {
        geode::log::warn("ECHO_DASH refused to rotate an invalid temp archive {}", temp.string());
        std::filesystem::remove(temp, ec);
        return false;
    }

    ec.clear();
    bool const hadPrimary = std::filesystem::exists(path, ec) && !ec;
    bool const primaryKnownGood = hadPrimary && validateCandidateFile(path, m_context);

    if (primaryKnownGood) {
        ec.clear();
        std::filesystem::copy_file(
            path,
            backup,
            std::filesystem::copy_options::overwrite_existing,
            ec
        );
        if (ec) {
            std::filesystem::remove(temp, ec);
            return false;
        }
    }

    if (hadPrimary) {
        ec.clear();
        std::filesystem::remove(path, ec);
        if (ec) {
            std::filesystem::remove(temp, ec);
            return false;
        }
    }

    ec.clear();
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        if (std::filesystem::exists(backup)) {
            std::error_code restoreError;
            std::filesystem::copy_file(
                backup,
                path,
                std::filesystem::copy_options::overwrite_existing,
                restoreError
            );
        }
        std::filesystem::remove(temp, ec);
        return false;
    }

    if (!validateCandidateFile(path, m_context)) {
        geode::log::warn("ECHO_DASH post-rotation validation failed for {}", path.string());
        if (std::filesystem::exists(backup)) {
            std::error_code restoreError;
            std::filesystem::copy_file(
                backup,
                path,
                std::filesystem::copy_options::overwrite_existing,
                restoreError
            );
        }
        return false;
    }

    // Intentionally retain backup as the previous known-good generation.
    m_dirty = false;
    m_recoveredFromBackup = false;
    m_quarantinedReplayCount = 0;
    return true;
}

void EchoReplayArchive::clear() {
    m_summaries.clear();
    m_replays.clear();
    m_recoveredFromBackup = false;
    m_quarantinedReplayCount = 0;
    markDirty();
}

bool EchoReplayArchive::ingest(
    AttemptRecord const& attempt,
    AttemptHistoryEntry const& summary
) {
    if (!m_loaded || attempt.attemptId == 0 || !attempt.finalized) return false;
    if (attempt.attemptId != summary.attemptId) return false;
    if (!validateSummary(summary) || !validateReplay(attempt)) return false;
    if (summaryById(attempt.attemptId)) return false;

    // Prepare and validate the complete replay payload before mutating either
    // archive container. This keeps summary + replay ingestion transactional.
    auto compressed = compressReplay(attempt);
    if (!validateReplay(compressed)) return false;

    m_summaries.push_back(summary);
    m_replays.push_back(std::move(compressed));

    trimSummaries();
    trimReplays();
    markDirty();
    return true;
}

AttemptRecord const* EchoReplayArchive::replayById(std::uint64_t attemptId) const {
    for (auto it = m_replays.rbegin(); it != m_replays.rend(); ++it) {
        if (it->attemptId == attemptId) return &*it;
    }
    return nullptr;
}

AttemptHistoryEntry const* EchoReplayArchive::summaryById(std::uint64_t attemptId) const {
    for (auto it = m_summaries.rbegin(); it != m_summaries.rend(); ++it) {
        if (it->attemptId == attemptId) return &*it;
    }
    return nullptr;
}

AttemptRecord const* EchoReplayArchive::latestReplay() const {
    AttemptRecord const* latest = nullptr;
    for (auto const& replay : m_replays) {
        if (!latest || replay.attemptId > latest->attemptId) latest = &replay;
    }
    return latest;
}

AttemptRecord const* EchoReplayArchive::bestRecordedReplay() const {
    AttemptRecord const* best = nullptr;
    for (auto const& replay : m_replays) {
        if (
            !best ||
            betterProgress(
                replay.maxProgressPercent,
                replay.attemptId,
                best->maxProgressPercent,
                best->attemptId
            )
        ) best = &replay;
    }
    return best;
}

AttemptHistoryEntry const* EchoReplayArchive::latestSummary() const {
    AttemptHistoryEntry const* latest = nullptr;
    for (auto const& summary : m_summaries) {
        if (!latest || summary.attemptId > latest->attemptId) latest = &summary;
    }
    return latest;
}

AttemptHistoryEntry const* EchoReplayArchive::bestRecordedSummary() const {
    auto const* bestReplay = bestRecordedReplay();
    return bestReplay ? summaryById(bestReplay->attemptId) : nullptr;
}

std::uint64_t EchoReplayArchive::previousReplayId(std::uint64_t attemptId) const {
    std::uint64_t best = 0;
    for (auto const& replay : m_replays) {
        if (replay.attemptId < attemptId && replay.attemptId > best) {
            best = replay.attemptId;
        }
    }
    return best;
}

std::uint64_t EchoReplayArchive::nextReplayId(std::uint64_t attemptId) const {
    std::uint64_t next = std::numeric_limits<std::uint64_t>::max();
    for (auto const& replay : m_replays) {
        if (replay.attemptId > attemptId && replay.attemptId < next) {
            next = replay.attemptId;
        }
    }
    return next == std::numeric_limits<std::uint64_t>::max() ? 0 : next;
}

std::uint64_t EchoReplayArchive::maxAttemptId() const {
    std::uint64_t result = 0;
    for (auto const& summary : m_summaries) result = std::max(result, summary.attemptId);
    for (auto const& replay : m_replays) result = std::max(result, replay.attemptId);
    return result;
}

std::deque<AttemptHistoryEntry> const& EchoReplayArchive::summaries() const {
    return m_summaries;
}

std::deque<AttemptRecord> const& EchoReplayArchive::replays() const {
    return m_replays;
}

EchoLevelContext const& EchoReplayArchive::context() const {
    return m_context;
}

ReplayArchiveStats EchoReplayArchive::stats() const {
    std::size_t frames = 0;
    for (auto const& replay : m_replays) frames += replay.frames.size();
    auto const* latest = latestReplay();
    auto const* best = bestRecordedReplay();
    return ReplayArchiveStats {
        m_summaries.size(),
        m_replays.size(),
        frames,
        latest ? latest->attemptId : 0,
        best ? best->attemptId : 0,
        best ? best->maxProgressPercent : 0.0f,
        m_revision,
        m_dirty,
        m_recoveredFromBackup,
        m_quarantinedReplayCount
    };
}

bool EchoReplayArchive::isLoaded() const {
    return m_loaded;
}

bool EchoReplayArchive::isDirty() const {
    return m_dirty;
}

std::uint64_t EchoReplayArchive::stableNameHash(std::string const& text) {
    std::uint64_t hash = 1469598103934665603ull;
    for (unsigned char value : text) {
        hash ^= static_cast<std::uint64_t>(value);
        hash *= 1099511628211ull;
    }
    return hash;
}

AttemptRecord EchoReplayArchive::compressReplay(AttemptRecord const& source) const {
    AttemptRecord result = source;
    result.frames.clear();
    if (source.frames.empty()) return result;

    result.frames.reserve(source.frames.size());
    result.frames.push_back(source.frames.front());

    double const interval = 1.0 / std::clamp(m_archiveSampleRate, 30.0, 240.0);
    double nextSample = source.frames.front().timeSeconds + interval;

    for (std::size_t i = 1; i + 1 < source.frames.size(); ++i) {
        auto const& frame = source.frames[i];
        bool const discontinuity =
            !frame.player1ContinuousFromPrevious ||
            !frame.player2ContinuousFromPrevious ||
            !frame.cameraContinuousFromPrevious;
        if (!discontinuity && frame.timeSeconds + 0.000001 < nextSample) continue;

        result.frames.push_back(frame);
        nextSample = frame.timeSeconds + interval;
    }

    if (
        source.frames.size() > 1 &&
        result.frames.back().sequence != source.frames.back().sequence
    ) {
        result.frames.push_back(source.frames.back());
    }
    return result;
}

std::filesystem::path EchoReplayArchive::archiveDirectory() const {
    return geode::Mod::get()->getSaveDir() / "echo_dash";
}

std::filesystem::path EchoReplayArchive::archivePath() const {
    return archiveDirectory() / (m_context.storageKey() + ".edar");
}

std::filesystem::path EchoReplayArchive::backupPath() const {
    return std::filesystem::path(archivePath().string() + ".bak");
}

std::size_t EchoReplayArchive::estimatedSerializedBytes() const {
    std::size_t result = 256 + m_summaries.size() * kApproxSummaryBytes;
    for (auto const& replay : m_replays) {
        result += 64 + replay.frames.size() * kApproxFrameBytes;
    }
    return result;
}

std::uint64_t EchoReplayArchive::bestRecordedAttemptId() const {
    auto const* best = bestRecordedReplay();
    return best ? best->attemptId : 0;
}

float EchoReplayArchive::bestRecordedProgress() const {
    auto const* best = bestRecordedReplay();
    return best ? best->maxProgressPercent : 0.0f;
}

void EchoReplayArchive::trimSummaries() {
    while (m_summaries.size() > kMaxSummaries) {
        auto const bestId = bestRecordedAttemptId();
        auto eviction = m_summaries.end();
        for (auto it = m_summaries.begin(); it != m_summaries.end(); ++it) {
            if (bestId != 0 && it->attemptId == bestId) continue;
            eviction = it;
            break;
        }
        if (eviction == m_summaries.end()) break;
        m_summaries.erase(eviction);
    }
}

void EchoReplayArchive::trimReplays() {
    auto overBudget = [this]() {
        auto const bytes = estimatedSerializedBytes();
        auto const budget = m_diskBudgetMb * 1024ull * 1024ull;
        return m_replays.size() > m_replayLimit || bytes > budget;
    };

    while (overBudget() && m_replays.size() > 2) {
        auto const* latest = latestReplay();
        auto const* best = bestRecordedReplay();
        auto const latestId = latest ? latest->attemptId : 0;
        auto const bestId = best ? best->attemptId : 0;

        auto eviction = m_replays.end();
        for (auto it = m_replays.begin(); it != m_replays.end(); ++it) {
            if (it->attemptId == latestId || it->attemptId == bestId) continue;
            eviction = it;
            break;
        }
        if (eviction == m_replays.end()) break;
        m_replays.erase(eviction);
    }
}

void EchoReplayArchive::markDirty() {
    m_dirty = true;
    ++m_revision;
}

} // namespace dash_echo
