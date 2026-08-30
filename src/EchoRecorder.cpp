#include "EchoRecorder.hpp"

#include <Geode/binding/PlayerObject.hpp>

#include <algorithm>
#include <cmath>

namespace dash_echo {

namespace {

constexpr double kMaxContinuousSampleGapSeconds = 0.100;
constexpr float kTeleportDistanceFloor = 120.0f;
constexpr float kTeleportSpeedFloor = 3000.0f;
constexpr float kScaleDiscontinuity = 0.35f;

PlayerMode detectPlayerMode(PlayerObject const* player) {
    if (!player) return PlayerMode::Cube;
    if (player->m_isShip) return PlayerMode::Ship;
    if (player->m_isBall) return PlayerMode::Ball;
    if (player->m_isBird) return PlayerMode::Ufo;
    if (player->m_isDart) return PlayerMode::Wave;
    if (player->m_isRobot) return PlayerMode::Robot;
    if (player->m_isSpider) return PlayerMode::Spider;
    if (player->m_isSwing) return PlayerMode::Swing;
    return PlayerMode::Cube;
}

ColorRGB toColorRGB(cocos2d::ccColor3B const& color) {
    return ColorRGB {
        static_cast<std::uint8_t>(color.r),
        static_cast<std::uint8_t>(color.g),
        static_cast<std::uint8_t>(color.b)
    };
}

bool finiteSnapshot(PlayerSnapshot const& snapshot) {
    return
        std::isfinite(snapshot.x) &&
        std::isfinite(snapshot.y) &&
        std::isfinite(snapshot.rotation) &&
        std::isfinite(snapshot.scaleX) &&
        std::isfinite(snapshot.scaleY);
}

} // namespace

void EchoRecorder::beginAttempt() {
    if (hasActiveAttempt()) return;

    AttemptRecord attempt;
    attempt.attemptId = m_nextAttemptId++;
    attempt.frames.reserve(4096);
    m_attempts.push_back(std::move(attempt));

    m_activeElapsedSeconds = 0.0;
    ++m_attemptsStarted;
    trimRetention();
}

void EchoRecorder::captureFrame(
    float dt,
    float progressPercent,
    PlayerObject* player1,
    PlayerObject* player2
) {
    auto* attempt = mutableActiveAttempt();
    if (!attempt) return;

    float safeDt = 0.0f;
    if (std::isfinite(dt)) safeDt = std::clamp(dt, 0.0f, 0.25f);
    m_activeElapsedSeconds += static_cast<double>(safeDt);

    float safeProgress = 0.0f;
    if (std::isfinite(progressPercent)) {
        safeProgress = std::clamp(progressPercent, 0.0f, 100.0f);
    }

    attempt->durationSeconds = m_activeElapsedSeconds;
    attempt->maxProgressPercent = std::max(attempt->maxProgressPercent, safeProgress);

    if (attempt->frames.size() >= kMaxFramesPerAttempt) {
        ++m_framesDropped;
        return;
    }

    FrameRecord frame;
    frame.sequence = m_nextFrameSequence++;
    frame.timeSeconds = m_activeElapsedSeconds;
    frame.progressPercent = safeProgress;
    frame.player1 = snapshotPlayer(player1);
    frame.player2 = snapshotPlayer(player2);

    if (!attempt->frames.empty()) {
        auto const& previous = attempt->frames.back();
        double const deltaSeconds = frame.timeSeconds - previous.timeSeconds;
        frame.player1ContinuousFromPrevious = canInterpolate(
            previous.player1,
            frame.player1,
            deltaSeconds
        );
        frame.player2ContinuousFromPrevious = canInterpolate(
            previous.player2,
            frame.player2,
            deltaSeconds
        );
    }

    attempt->frames.push_back(std::move(frame));
    ++m_framesCaptured;
    ++m_retainedFrames;
}

void EchoRecorder::finalizeAttempt(AttemptEndReason reason) {
    auto* attempt = mutableActiveAttempt();
    if (!attempt) return;

    attempt->durationSeconds = m_activeElapsedSeconds;
    attempt->endReason = reason;
    attempt->finalized = true;
    ++m_attemptsFinalized;

    m_activeElapsedSeconds = 0.0;
    trimRetention();
}

void EchoRecorder::clear() {
    m_attempts.clear();
    m_nextAttemptId = 1;
    m_nextFrameSequence = 1;
    m_attemptsStarted = 0;
    m_attemptsFinalized = 0;
    m_framesCaptured = 0;
    m_framesDropped = 0;
    m_retainedFrames = 0;
    m_activeElapsedSeconds = 0.0;
}

bool EchoRecorder::hasActiveAttempt() const {
    return !m_attempts.empty() && !m_attempts.back().finalized;
}

double EchoRecorder::activeElapsedSeconds() const {
    return hasActiveAttempt() ? m_activeElapsedSeconds : 0.0;
}

AttemptRecord const* EchoRecorder::activeAttempt() const {
    if (!hasActiveAttempt()) return nullptr;
    return &m_attempts.back();
}

AttemptRecord const* EchoRecorder::latestFinalizedAttempt() const {
    for (auto it = m_attempts.rbegin(); it != m_attempts.rend(); ++it) {
        if (it->finalized) return &*it;
    }
    return nullptr;
}

AttemptRecord const* EchoRecorder::personalBestAttempt() const {
    AttemptRecord const* best = nullptr;

    for (auto const& attempt : m_attempts) {
        if (!attempt.finalized || attempt.frames.empty()) continue;

        if (!best || isBetterPersonalBest(attempt, *best)) {
            best = &attempt;
        }
    }

    return best;
}

std::deque<AttemptRecord> const& EchoRecorder::attempts() const {
    return m_attempts;
}

RecorderStats EchoRecorder::stats() const {
    RecorderStats result;
    result.attemptsStarted = m_attemptsStarted;
    result.attemptsFinalized = m_attemptsFinalized;
    result.framesCaptured = m_framesCaptured;
    result.framesDropped = m_framesDropped;
    result.retainedAttempts = m_attempts.size();
    result.retainedFrames = m_retainedFrames;
    return result;
}

bool EchoRecorder::canInterpolate(
    PlayerSnapshot const& previous,
    PlayerSnapshot const& current,
    double deltaSeconds
) {
    if (!previous.present || !current.present) return false;
    if (!previous.visible || !current.visible) return false;
    if (previous.mode != current.mode) return false;
    if (!finiteSnapshot(previous) || !finiteSnapshot(current)) return false;
    if (
        !std::isfinite(deltaSeconds) ||
        deltaSeconds <= 0.0 ||
        deltaSeconds > kMaxContinuousSampleGapSeconds
    ) return false;

    float const dx = current.x - previous.x;
    float const dy = current.y - previous.y;
    float const distance = std::hypot(dx, dy);
    float const speed = distance / static_cast<float>(deltaSeconds);

    if (distance > kTeleportDistanceFloor && speed > kTeleportSpeedFloor) return false;

    if (
        std::abs(current.scaleX - previous.scaleX) > kScaleDiscontinuity ||
        std::abs(current.scaleY - previous.scaleY) > kScaleDiscontinuity
    ) return false;

    return true;
}

bool EchoRecorder::isBetterPersonalBest(
    AttemptRecord const& candidate,
    AttemptRecord const& incumbent
) {
    if (candidate.maxProgressPercent > incumbent.maxProgressPercent) return true;
    if (candidate.maxProgressPercent < incumbent.maxProgressPercent) return false;

    // Equal-progress ties deliberately prefer the newer attempt. This keeps the
    // pinned PB fresh and lets an older tied attempt age out normally.
    return candidate.attemptId > incumbent.attemptId;
}

AttemptRecord* EchoRecorder::mutableActiveAttempt() {
    if (!hasActiveAttempt()) return nullptr;
    return &m_attempts.back();
}

PlayerSnapshot EchoRecorder::snapshotPlayer(PlayerObject* player) const {
    PlayerSnapshot snapshot;
    if (!player) return snapshot;

    auto const position = player->getPosition();
    snapshot.present = true;
    snapshot.visible = player->isVisible();
    snapshot.mode = detectPlayerMode(player);
    snapshot.color1 = toColorRGB(player->m_playerColor1);
    snapshot.color2 = toColorRGB(player->m_playerColor2);
    snapshot.x = position.x;
    snapshot.y = position.y;
    snapshot.rotation = player->getRotation();
    snapshot.scaleX = player->getScaleX();
    snapshot.scaleY = player->getScaleY();
    return snapshot;
}

void EchoRecorder::trimRetention() {
    while (
        (m_attempts.size() > kMaxRetainedAttempts || m_retainedFrames > kMaxRetainedFrames) &&
        !m_attempts.empty()
    ) {
        auto const* personalBest = personalBestAttempt();
        auto eviction = m_attempts.end();

        for (auto it = m_attempts.begin(); it != m_attempts.end(); ++it) {
            if (!it->finalized) continue;
            if (personalBest && &*it == personalBest) continue;
            eviction = it;
            break;
        }

        // With one pinned PB plus an active attempt, per-attempt frame caps make
        // this normally unreachable. Breaking is safer than evicting authority.
        if (eviction == m_attempts.end()) break;

        if (eviction->frames.size() <= m_retainedFrames) {
            m_retainedFrames -= eviction->frames.size();
        } else {
            m_retainedFrames = 0;
        }
        m_attempts.erase(eviction);
    }
}

} // namespace dash_echo
