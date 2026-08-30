#include "EchoReplayArchive.hpp"

#include <Geode/Geode.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <system_error>

namespace dash_echo {

namespace {

constexpr std::array<char, 8> kMagic {'E', 'C', 'H', 'O', 'D', 'A', 'S', 'H'};
constexpr std::uint32_t kMaxStoredStringBytes = 4096;
constexpr std::size_t kApproxSummaryBytes = 160;
constexpr std::size_t kApproxFrameBytes = 104;

float safePercent(float value) {
    if (!std::isfinite(value)) return 0.0f;
    return std::clamp(value, 0.0f, 100.0f);
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
    summary.maxProgressPercent = safePercent(summary.maxProgressPercent);
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
    attempt.maxProgressPercent = safePercent(attempt.maxProgressPercent);
    attempt.frames.resize(static_cast<std::size_t>(frameCount));
    for (auto& frame : attempt.frames) {
        if (!readFrame(in, frame)) return false;
    }
    totalFrames += frameCount;
    return attempt.attemptId != 0 && attempt.finalized && !attempt.frames.empty();
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

bool EchoReplayArchive::load(EchoLevelContext const& context) {
    m_context = context;
    m_summaries.clear();
    m_replays.clear();
    m_loaded = true;
    m_dirty = false;
    ++m_revision;

    auto const path = archivePath();
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return true;
    if (ec) return false;

    auto const bytes = std::filesystem::file_size(path, ec);
    if (ec || bytes > kHardMaxArchiveBytes) {
        geode::log::warn("ECHO_DASH ignored oversized/unreadable archive {}", path.string());
        return false;
    }

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
    ) {
        geode::log::warn("ECHO_DASH rejected invalid archive header {}", path.string());
        return false;
    }

    stored.platformer = platformer != 0;
    stored.practice = practice != 0;
    if (!stored.matches(context)) {
        geode::log::warn("ECHO_DASH rejected archive context mismatch {}", path.string());
        return false;
    }

    std::deque<AttemptHistoryEntry> summaries;
    for (std::uint64_t i = 0; i < summaryCount; ++i) {
        AttemptHistoryEntry entry;
        if (!readSummary(in, entry) || entry.attemptId == 0) return false;
        summaries.push_back(std::move(entry));
    }

    std::deque<AttemptRecord> replays;
    std::uint64_t totalFrames = 0;
    for (std::uint64_t i = 0; i < replayCount; ++i) {
        AttemptRecord attempt;
        if (!readAttempt(in, attempt, totalFrames)) return false;
        replays.push_back(std::move(attempt));
    }

    if (!in) return false;

    m_summaries = std::move(summaries);
    m_replays = std::move(replays);
    trimSummaries();
    trimReplays();
    m_dirty = false;
    ++m_revision;
    return true;
}

bool EchoReplayArchive::save() {
    if (!m_loaded) return false;
    if (!m_dirty) return true;

    auto const dir = archiveDirectory();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) return false;

    auto const path = archivePath();
    auto const temp = path.string() + ".tmp";
    auto const backup = path.string() + ".bak";

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
        ) return false;

        for (auto const& summary : m_summaries) {
            if (!writeSummary(out, summary)) return false;
        }
        for (auto const& replay : m_replays) {
            if (!writeAttempt(out, replay)) return false;
        }
        out.flush();
        if (!out) return false;
    }

    std::filesystem::remove(backup, ec);
    ec.clear();
    bool const hadOriginal = std::filesystem::exists(path, ec) && !ec;
    if (hadOriginal) {
        std::filesystem::rename(path, backup, ec);
        if (ec) {
            std::filesystem::remove(temp, ec);
            return false;
        }
    }

    ec.clear();
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        if (hadOriginal) {
            std::error_code restoreError;
            std::filesystem::rename(backup, path, restoreError);
        }
        std::filesystem::remove(temp, ec);
        return false;
    }

    if (hadOriginal) {
        std::filesystem::remove(backup, ec);
    }
    m_dirty = false;
    return true;
}

void EchoReplayArchive::clear() {
    m_summaries.clear();
    m_replays.clear();
    markDirty();
}

bool EchoReplayArchive::ingest(
    AttemptRecord const& attempt,
    AttemptHistoryEntry const& summary
) {
    if (!m_loaded || attempt.attemptId == 0 || !attempt.finalized) return false;
    if (attempt.attemptId != summary.attemptId) return false;
    if (summaryById(attempt.attemptId)) return false;

    m_summaries.push_back(summary);
    if (!attempt.frames.empty()) {
        m_replays.push_back(compressReplay(attempt));
    }

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
        m_dirty
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
