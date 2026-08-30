#include "EchoReplayTimeline.hpp"

#include <algorithm>
#include <cmath>

namespace dash_echo {

bool EchoReplayTimeline::load(
    AttemptRecord const& attempt,
    AttemptHistoryEntry const& history
) {
    clear();

    if (
        attempt.attemptId == 0 ||
        attempt.attemptId != history.attemptId ||
        !validateAttempt(attempt)
    ) {
        return false;
    }

    m_clip.attempt = attempt;
    m_clip.history = history;
    m_clip.startTimeSeconds = m_clip.attempt.frames.front().timeSeconds;
    m_clip.endTimeSeconds = m_clip.attempt.frames.back().timeSeconds;
    m_clip.durationSeconds = m_clip.endTimeSeconds - m_clip.startTimeSeconds;

    buildMarkers();
    m_cursorSeconds = m_clip.startTimeSeconds;
    m_playbackRate = 1.0f;
    m_state = ReplayTimelineState::Ready;
    return true;
}

void EchoReplayTimeline::clear() {
    m_clip = {};
    m_state = ReplayTimelineState::Empty;
    m_cursorSeconds = 0.0;
    m_playbackRate = 1.0f;
}

void EchoReplayTimeline::start() {
    if (!isLoaded()) return;

    if (m_state == ReplayTimelineState::Finished) {
        m_cursorSeconds = m_clip.startTimeSeconds;
    }
    m_state = ReplayTimelineState::Playing;
}

void EchoReplayTimeline::pause() {
    if (m_state == ReplayTimelineState::Playing) {
        m_state = ReplayTimelineState::Paused;
    }
}

void EchoReplayTimeline::resume() {
    if (!isLoaded()) return;
    if (m_state == ReplayTimelineState::Playing) return;
    start();
}

void EchoReplayTimeline::togglePlayback() {
    if (m_state == ReplayTimelineState::Playing) {
        pause();
    } else {
        resume();
    }
}

void EchoReplayTimeline::restart() {
    if (!isLoaded()) return;
    m_cursorSeconds = m_clip.startTimeSeconds;
    m_state = ReplayTimelineState::Ready;
}

void EchoReplayTimeline::advance(float dt) {
    if (m_state != ReplayTimelineState::Playing) return;

    float safeDt = 0.0f;
    if (std::isfinite(dt)) {
        safeDt = std::clamp(dt, 0.0f, 0.25f);
    }

    double const scaledDelta =
        static_cast<double>(safeDt) * static_cast<double>(m_playbackRate);
    setCursorClamped(m_cursorSeconds + scaledDelta);

    if (m_cursorSeconds >= m_clip.endTimeSeconds) {
        m_state = ReplayTimelineState::Finished;
    }
}

bool EchoReplayTimeline::setPlaybackRate(float rate) {
    if (!std::isfinite(rate)) return false;

    for (float const supported : kPlaybackRates) {
        if (samePlaybackRate(rate, supported)) {
            m_playbackRate = supported;
            return true;
        }
    }
    return false;
}

void EchoReplayTimeline::cyclePlaybackRate() {
    std::size_t const current = playbackRateIndex();
    std::size_t const next = (current + 1) % kPlaybackRates.size();
    m_playbackRate = kPlaybackRates[next];
}

bool EchoReplayTimeline::seekSeconds(double timeSeconds) {
    if (!isLoaded() || !std::isfinite(timeSeconds)) return false;

    setCursorClamped(timeSeconds);
    m_state = ReplayTimelineState::Paused;
    return true;
}

bool EchoReplayTimeline::seekNormalized(float normalizedPosition) {
    if (!isLoaded() || !std::isfinite(normalizedPosition)) return false;

    float const bounded = std::clamp(normalizedPosition, 0.0f, 1.0f);
    double const target =
        m_clip.startTimeSeconds +
        m_clip.durationSeconds * static_cast<double>(bounded);
    return seekSeconds(target);
}

bool EchoReplayTimeline::stepPreviousFrame() {
    if (!isLoaded() || m_clip.attempt.frames.empty()) return false;

    auto const& frames = m_clip.attempt.frames;
    auto lower = std::lower_bound(
        frames.begin(),
        frames.end(),
        m_cursorSeconds,
        [](FrameRecord const& frame, double value) {
            return frame.timeSeconds < value;
        }
    );

    double target = m_clip.startTimeSeconds;
    if (lower != frames.begin()) {
        --lower;
        target = lower->timeSeconds;
    }

    setCursorClamped(target);
    m_state = ReplayTimelineState::Paused;
    return true;
}

bool EchoReplayTimeline::stepNextFrame() {
    if (!isLoaded() || m_clip.attempt.frames.empty()) return false;

    auto const& frames = m_clip.attempt.frames;
    auto upper = std::upper_bound(
        frames.begin(),
        frames.end(),
        m_cursorSeconds,
        [](double value, FrameRecord const& frame) {
            return value < frame.timeSeconds;
        }
    );

    double target = m_clip.endTimeSeconds;
    if (upper != frames.end()) {
        target = upper->timeSeconds;
    }

    setCursorClamped(target);
    m_state = ReplayTimelineState::Paused;
    return true;
}

bool EchoReplayTimeline::isLoaded() const {
    return m_state != ReplayTimelineState::Empty;
}

bool EchoReplayTimeline::isPlaying() const {
    return m_state == ReplayTimelineState::Playing;
}

bool EchoReplayTimeline::isPaused() const {
    return m_state == ReplayTimelineState::Paused;
}

ReplayTimelineState EchoReplayTimeline::state() const { return m_state; }
ReplayClip const* EchoReplayTimeline::clip() const { return isLoaded() ? &m_clip : nullptr; }
AttemptRecord const* EchoReplayTimeline::replayAttempt() const { return isLoaded() ? &m_clip.attempt : nullptr; }
AttemptHistoryEntry const* EchoReplayTimeline::historyEntry() const { return isLoaded() ? &m_clip.history : nullptr; }
std::vector<ReplayTimelineMarker> const& EchoReplayTimeline::markers() const { return m_clip.markers; }
std::uint64_t EchoReplayTimeline::sourceAttemptId() const { return isLoaded() ? m_clip.attempt.attemptId : 0; }
double EchoReplayTimeline::cursorSeconds() const { return isLoaded() ? m_cursorSeconds : 0.0; }
double EchoReplayTimeline::durationSeconds() const { return isLoaded() ? m_clip.durationSeconds : 0.0; }
float EchoReplayTimeline::playbackRate() const { return m_playbackRate; }

float EchoReplayTimeline::normalizedCursor() const {
    if (!isLoaded() || m_clip.durationSeconds <= 0.0) return 0.0f;
    double const normalized =
        (m_cursorSeconds - m_clip.startTimeSeconds) / m_clip.durationSeconds;
    return static_cast<float>(std::clamp(normalized, 0.0, 1.0));
}

float EchoReplayTimeline::progressPercentAtCursor() const {
    if (!isLoaded() || m_clip.attempt.frames.empty()) return 0.0f;

    auto const& frames = m_clip.attempt.frames;
    if (m_cursorSeconds <= frames.front().timeSeconds) {
        return std::clamp(frames.front().progressPercent, 0.0f, 100.0f);
    }
    if (m_cursorSeconds >= frames.back().timeSeconds) {
        return std::clamp(frames.back().progressPercent, 0.0f, 100.0f);
    }

    auto upper = std::upper_bound(
        frames.begin(),
        frames.end(),
        m_cursorSeconds,
        [](double value, FrameRecord const& frame) {
            return value < frame.timeSeconds;
        }
    );

    if (upper == frames.begin()) {
        return std::clamp(frames.front().progressPercent, 0.0f, 100.0f);
    }

    auto const& to = *upper;
    auto const& from = *(upper - 1);
    double const span = to.timeSeconds - from.timeSeconds;
    if (!std::isfinite(span) || span <= 0.0) {
        return std::clamp(from.progressPercent, 0.0f, 100.0f);
    }

    float const alpha = static_cast<float>(std::clamp(
        (m_cursorSeconds - from.timeSeconds) / span,
        0.0,
        1.0
    ));
    return std::clamp(
        std::lerp(from.progressPercent, to.progressPercent, alpha),
        0.0f,
        100.0f
    );
}

CameraSnapshot EchoReplayTimeline::cameraAtCursor() const {
    CameraSnapshot empty;
    if (!isLoaded() || m_clip.attempt.frames.empty()) return empty;

    auto const& frames = m_clip.attempt.frames;
    if (m_cursorSeconds <= frames.front().timeSeconds) return frames.front().camera;
    if (m_cursorSeconds >= frames.back().timeSeconds) return frames.back().camera;

    auto upper = std::upper_bound(
        frames.begin(),
        frames.end(),
        m_cursorSeconds,
        [](double value, FrameRecord const& frame) {
            return value < frame.timeSeconds;
        }
    );

    if (upper == frames.begin()) return frames.front().camera;

    auto const& to = *upper;
    auto const& from = *(upper - 1);
    if (!from.camera.present || !to.camera.present) return from.camera;

    double const span = to.timeSeconds - from.timeSeconds;
    if (!std::isfinite(span) || span <= 0.0) return from.camera;

    float const alpha = static_cast<float>(std::clamp(
        (m_cursorSeconds - from.timeSeconds) / span,
        0.0,
        1.0
    ));

    if (!to.cameraContinuousFromPrevious) return from.camera;

    CameraSnapshot result;
    result.present = true;
    result.x = std::lerp(from.camera.x, to.camera.x, alpha);
    result.y = std::lerp(from.camera.y, to.camera.y, alpha);
    result.rotation = interpolateRotation(from.camera.rotation, to.camera.rotation, alpha);
    result.scaleX = std::lerp(from.camera.scaleX, to.camera.scaleX, alpha);
    result.scaleY = std::lerp(from.camera.scaleY, to.camera.scaleY, alpha);
    return result;
}

PlayerSnapshot EchoReplayTimeline::playerAtCursor(std::uint8_t playerIndex) const {
    return playerAtTime(playerIndex, m_cursorSeconds);
}

PlayerSnapshot EchoReplayTimeline::playerAtTime(
    std::uint8_t playerIndex,
    double timeSeconds
) const {
    PlayerSnapshot empty;
    if (
        !isLoaded() ||
        m_clip.attempt.frames.empty() ||
        !std::isfinite(timeSeconds)
    ) {
        return empty;
    }

    bool const secondPlayer = playerIndex == 2;
    auto const selectPlayer = [secondPlayer](FrameRecord const& frame) -> PlayerSnapshot const& {
        return secondPlayer ? frame.player2 : frame.player1;
    };
    auto const continuousFromPrevious = [secondPlayer](FrameRecord const& frame) {
        return secondPlayer ?
            frame.player2ContinuousFromPrevious :
            frame.player1ContinuousFromPrevious;
    };

    auto const& frames = m_clip.attempt.frames;
    if (timeSeconds <= frames.front().timeSeconds) return selectPlayer(frames.front());
    if (timeSeconds >= frames.back().timeSeconds) return selectPlayer(frames.back());

    auto upper = std::upper_bound(
        frames.begin(),
        frames.end(),
        timeSeconds,
        [](double value, FrameRecord const& frame) {
            return value < frame.timeSeconds;
        }
    );
    if (upper == frames.begin()) return selectPlayer(frames.front());

    auto const& toFrame = *upper;
    auto const& fromFrame = *(upper - 1);
    auto const& from = selectPlayer(fromFrame);
    auto const& to = selectPlayer(toFrame);

    if (!from.present || !to.present || !continuousFromPrevious(toFrame)) {
        return from;
    }

    double const span = toFrame.timeSeconds - fromFrame.timeSeconds;
    if (!std::isfinite(span) || span <= 0.0) return from;

    float const alpha = static_cast<float>(std::clamp(
        (timeSeconds - fromFrame.timeSeconds) / span,
        0.0,
        1.0
    ));

    PlayerSnapshot result = from;
    result.present = true;
    result.visible = from.visible && to.visible;
    result.mode = from.mode;
    result.color1 = interpolateColor(from.color1, to.color1, alpha);
    result.color2 = interpolateColor(from.color2, to.color2, alpha);
    result.x = std::lerp(from.x, to.x, alpha);
    result.y = std::lerp(from.y, to.y, alpha);
    result.rotation = interpolateRotation(from.rotation, to.rotation, alpha);
    result.scaleX = std::lerp(from.scaleX, to.scaleX, alpha);
    result.scaleY = std::lerp(from.scaleY, to.scaleY, alpha);
    return result;
}

bool EchoReplayTimeline::validateAttempt(AttemptRecord const& attempt) {
    if (!attempt.finalized || attempt.frames.size() < 2) return false;

    double previous = -1.0;
    for (auto const& frame : attempt.frames) {
        if (!std::isfinite(frame.timeSeconds) || frame.timeSeconds < 0.0) return false;
        if (previous > frame.timeSeconds) return false;
        previous = frame.timeSeconds;
    }

    double const duration =
        attempt.frames.back().timeSeconds - attempt.frames.front().timeSeconds;
    return std::isfinite(duration) && duration > 0.0;
}

ReplayTimelineMarker EchoReplayTimeline::makeMarker(
    ReplayTimelineMarkerType type,
    double timeSeconds,
    float progressPercent,
    double startTimeSeconds,
    double durationSeconds
) {
    ReplayTimelineMarker marker;
    marker.type = type;
    marker.timeSeconds = timeSeconds;
    marker.progressPercent = std::clamp(progressPercent, 0.0f, 100.0f);

    if (durationSeconds > 0.0 && std::isfinite(durationSeconds)) {
        marker.normalizedPosition = static_cast<float>(std::clamp(
            (timeSeconds - startTimeSeconds) / durationSeconds,
            0.0,
            1.0
        ));
    }
    return marker;
}

bool EchoReplayTimeline::samePlaybackRate(float left, float right) {
    return std::abs(left - right) <= 0.0001f;
}

float EchoReplayTimeline::interpolateRotation(float from, float to, float alpha) {
    float delta = std::fmod(to - from, 360.0f);
    if (delta > 180.0f) delta -= 360.0f;
    if (delta < -180.0f) delta += 360.0f;
    return from + delta * std::clamp(alpha, 0.0f, 1.0f);
}

ColorRGB EchoReplayTimeline::interpolateColor(
    ColorRGB const& from,
    ColorRGB const& to,
    float alpha
) {
    float const bounded = std::clamp(alpha, 0.0f, 1.0f);
    auto const channel = [bounded](std::uint8_t left, std::uint8_t right) {
        float const value = std::lerp(
            static_cast<float>(left),
            static_cast<float>(right),
            bounded
        );
        return static_cast<std::uint8_t>(std::clamp(value, 0.0f, 255.0f));
    };

    return ColorRGB {
        channel(from.r, to.r),
        channel(from.g, to.g),
        channel(from.b, to.b)
    };
}

void EchoReplayTimeline::buildMarkers() {
    m_clip.markers.clear();
    m_clip.markers.reserve(5);

    auto const& history = m_clip.history;
    auto const& frames = m_clip.attempt.frames;

    m_clip.markers.push_back(makeMarker(
        ReplayTimelineMarkerType::Start,
        m_clip.startTimeSeconds,
        frames.front().progressPercent,
        m_clip.startTimeSeconds,
        m_clip.durationSeconds
    ));

    if (history.death.present) {
        double const deathTime = std::clamp(
            history.death.timeSeconds,
            m_clip.startTimeSeconds,
            m_clip.endTimeSeconds
        );
        m_clip.markers.push_back(makeMarker(
            ReplayTimelineMarkerType::Death,
            deathTime,
            history.death.progressPercent,
            m_clip.startTimeSeconds,
            m_clip.durationSeconds
        ));
    }

    if (history.completed) {
        m_clip.markers.push_back(makeMarker(
            ReplayTimelineMarkerType::Completion,
            m_clip.endTimeSeconds,
            history.maxProgressPercent,
            m_clip.startTimeSeconds,
            m_clip.durationSeconds
        ));
    }

    if (history.personalBestAtFinalization) {
        m_clip.markers.push_back(makeMarker(
            ReplayTimelineMarkerType::PersonalBest,
            m_clip.endTimeSeconds,
            history.maxProgressPercent,
            m_clip.startTimeSeconds,
            m_clip.durationSeconds
        ));
    }

    m_clip.markers.push_back(makeMarker(
        ReplayTimelineMarkerType::End,
        m_clip.endTimeSeconds,
        frames.back().progressPercent,
        m_clip.startTimeSeconds,
        m_clip.durationSeconds
    ));
}

void EchoReplayTimeline::setCursorClamped(double timeSeconds) {
    if (!isLoaded() || !std::isfinite(timeSeconds)) return;
    m_cursorSeconds = std::clamp(
        timeSeconds,
        m_clip.startTimeSeconds,
        m_clip.endTimeSeconds
    );
}

std::size_t EchoReplayTimeline::playbackRateIndex() const {
    for (std::size_t i = 0; i < kPlaybackRates.size(); ++i) {
        if (samePlaybackRate(m_playbackRate, kPlaybackRates[i])) return i;
    }

    for (std::size_t i = 0; i < kPlaybackRates.size(); ++i) {
        if (samePlaybackRate(1.0f, kPlaybackRates[i])) return i;
    }
    return 0;
}

} // namespace dash_echo
