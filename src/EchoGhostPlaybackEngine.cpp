#include "EchoGhostPlaybackEngine.hpp"
#include "EchoTimePolicy.hpp"

#include <algorithm>
#include <cmath>

namespace dash_echo {

namespace {

constexpr double kFinishedEpsilonSeconds = 0.000001;

} // namespace

void EchoGhostPlaybackEngine::reset() {
    m_phase = GhostPlaybackPhase::Tracking;
    m_anchorElapsedSeconds = 0.0;
    m_anchorProgressPercent = 0.0f;
    m_progressAuthority = false;
    m_continuationElapsedSeconds = 0.0;
}

void EchoGhostPlaybackEngine::track(
    double liveElapsedSeconds,
    float liveProgressPercent,
    bool progressAuthority
) {
    m_phase = GhostPlaybackPhase::Tracking;
    m_anchorElapsedSeconds =
        std::isfinite(liveElapsedSeconds) ? std::max(0.0, liveElapsedSeconds) : 0.0;
    m_anchorProgressPercent =
        std::isfinite(liveProgressPercent) ? std::clamp(liveProgressPercent, 0.0f, 100.0f) : 0.0f;
    m_progressAuthority = progressAuthority;
    m_continuationElapsedSeconds = 0.0;
}

void EchoGhostPlaybackEngine::beginContinuation(
    double liveElapsedSeconds,
    float liveProgressPercent,
    bool progressAuthority
) {
    m_phase = GhostPlaybackPhase::Continuing;
    m_anchorElapsedSeconds =
        std::isfinite(liveElapsedSeconds) ? std::max(0.0, liveElapsedSeconds) : 0.0;
    m_anchorProgressPercent =
        std::isfinite(liveProgressPercent) ? std::clamp(liveProgressPercent, 0.0f, 100.0f) : 0.0f;
    m_progressAuthority = progressAuthority;
    m_continuationElapsedSeconds = 0.0;
}

void EchoGhostPlaybackEngine::advance(double dt) {
    if (m_phase != GhostPlaybackPhase::Continuing) return;

    m_continuationElapsedSeconds += sanitizeDeltaSeconds(dt);
}

double EchoGhostPlaybackEngine::resolveTime(
    AttemptRecord const& attempt,
    bool progressAlignmentSafe
) const {
    double const anchor = trackingAnchorTime(attempt, progressAlignmentSafe);
    if (m_phase != GhostPlaybackPhase::Continuing) return anchor;
    return anchor + m_continuationElapsedSeconds;
}

bool EchoGhostPlaybackEngine::finished(
    AttemptRecord const& attempt,
    bool progressAlignmentSafe
) const {
    if (!attempt.finalized || attempt.frames.empty()) return true;
    double const resolved = resolveTime(attempt, progressAlignmentSafe);
    if (!std::isfinite(resolved)) return true;
    return resolved > attempt.frames.back().timeSeconds + kFinishedEpsilonSeconds;
}

bool EchoGhostPlaybackEngine::isContinuing() const {
    return m_phase == GhostPlaybackPhase::Continuing;
}

GhostPlaybackPhase EchoGhostPlaybackEngine::phase() const {
    return m_phase;
}

double EchoGhostPlaybackEngine::continuationElapsedSeconds() const {
    return m_continuationElapsedSeconds;
}

bool EchoGhostPlaybackEngine::supportsProgressAlignment(
    AttemptRecord const& attempt
) {
    auto const& frames = attempt.frames;
    if (frames.size() < 2) return false;

    float previous = frames.front().progressPercent;
    if (!std::isfinite(previous)) return false;

    for (std::size_t i = 1; i < frames.size(); ++i) {
        float const current = frames[i].progressPercent;
        if (!std::isfinite(current) || current < previous) return false;
        previous = current;
    }

    return frames.back().progressPercent > frames.front().progressPercent + 0.001f;
}

double EchoGhostPlaybackEngine::trackingAnchorTime(
    AttemptRecord const& attempt,
    bool progressAlignmentSafe
) const {
    if (
        m_progressAuthority &&
        progressAlignmentSafe &&
        attempt.finalized &&
        !attempt.frames.empty()
    ) {
        return timeForProgress(
            attempt,
            m_anchorProgressPercent,
            m_anchorElapsedSeconds
        );
    }
    return m_anchorElapsedSeconds;
}

double EchoGhostPlaybackEngine::timeForProgress(
    AttemptRecord const& attempt,
    float progressPercent,
    double fallbackTimeSeconds
) {
    auto const& frames = attempt.frames;
    if (frames.empty() || !std::isfinite(progressPercent)) return fallbackTimeSeconds;

    if (progressPercent <= frames.front().progressPercent) {
        return frames.front().timeSeconds;
    }
    if (progressPercent > frames.back().progressPercent) {
        // The current run has already passed this historical run. Resolve beyond
        // its final timestamp so the shared renderer hides it instead of leaving
        // a stale ghost frozen behind.
        return frames.back().timeSeconds + 1.0;
    }

    auto upper = std::lower_bound(
        frames.begin(),
        frames.end(),
        progressPercent,
        [](FrameRecord const& frame, float value) {
            return frame.progressPercent < value;
        }
    );

    if (upper == frames.begin()) return upper->timeSeconds;
    if (upper == frames.end()) return frames.back().timeSeconds;

    auto const& to = *upper;
    auto const& from = *(upper - 1);
    float const deltaProgress = to.progressPercent - from.progressPercent;
    if (deltaProgress <= 0.000001f) return to.timeSeconds;

    double const alpha = std::clamp(
        static_cast<double>(progressPercent - from.progressPercent) /
            static_cast<double>(deltaProgress),
        0.0,
        1.0
    );
    return std::lerp(from.timeSeconds, to.timeSeconds, alpha);
}

} // namespace dash_echo
