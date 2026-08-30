#include "EchoCinematicCamera.hpp"

#include <algorithm>
#include <cmath>

namespace dash_echo {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr double kSmoothingDiscontinuitySeconds = 0.35;
constexpr double kDroneLookAheadSeconds = 0.18;
constexpr float kMaxDroneLookAheadWorldUnits = 110.0f;
constexpr double kDeathCamWindowSeconds = 1.35;

float shortestRotation(float from, float to, float alpha) {
    float delta = std::fmod(to - from, 360.0f);
    if (delta > 180.0f) delta -= 360.0f;
    if (delta < -180.0f) delta += 360.0f;
    return from + delta * std::clamp(alpha, 0.0f, 1.0f);
}

float smoothstep01(float value) {
    float const x = std::clamp(value, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

bool usablePlayer(PlayerSnapshot const& player) {
    return
        player.present &&
        player.visible &&
        std::isfinite(player.x) &&
        std::isfinite(player.y);
}

} // namespace

CameraPose EchoCinematicCamera::evaluate(
    EchoReplayTimeline const& timeline,
    float viewportWidth,
    float viewportHeight
) {
    CameraPose const baseline = recordedPose(timeline);
    if (!baseline.valid) return baseline;

    if (
        !std::isfinite(viewportWidth) ||
        !std::isfinite(viewportHeight) ||
        viewportWidth <= 0.0f ||
        viewportHeight <= 0.0f
    ) {
        return baseline;
    }

    CameraPose result = baseline;
    switch (m_mode) {
        case CinematicCameraMode::Recorded:
            return baseline;
        case CinematicCameraMode::Follow:
            result = evaluateFollow(timeline, baseline, viewportWidth, viewportHeight);
            break;
        case CinematicCameraMode::Smooth: {
            CameraPose const target = evaluateFollow(
                timeline,
                baseline,
                viewportWidth,
                viewportHeight
            );
            result = evaluateSmooth(timeline, target);
            break;
        }
        case CinematicCameraMode::Drone:
            result = evaluateDrone(timeline, baseline, viewportWidth, viewportHeight);
            break;
        case CinematicCameraMode::DynamicZoom:
            result = evaluateDynamicZoom(
                timeline,
                baseline,
                viewportWidth,
                viewportHeight
            );
            break;
        case CinematicCameraMode::DeathCam:
            result = evaluateDeathCam(timeline, baseline, viewportWidth, viewportHeight);
            break;
    }

    return finitePose(result) ? result : baseline;
}

void EchoCinematicCamera::setMode(CinematicCameraMode mode) {
    if (m_mode == mode) return;
    m_mode = mode;
    resetSmoothing();
}

void EchoCinematicCamera::cycleMode(bool deathAvailable) {
    CinematicCameraMode next = CinematicCameraMode::Recorded;

    switch (m_mode) {
        case CinematicCameraMode::Recorded:
            next = CinematicCameraMode::Follow;
            break;
        case CinematicCameraMode::Follow:
            next = CinematicCameraMode::Smooth;
            break;
        case CinematicCameraMode::Smooth:
            next = CinematicCameraMode::Drone;
            break;
        case CinematicCameraMode::Drone:
            next = CinematicCameraMode::DynamicZoom;
            break;
        case CinematicCameraMode::DynamicZoom:
            next = deathAvailable ?
                CinematicCameraMode::DeathCam :
                CinematicCameraMode::Recorded;
            break;
        case CinematicCameraMode::DeathCam:
            next = CinematicCameraMode::Recorded;
            break;
    }

    setMode(next);
}

void EchoCinematicCamera::reset() {
    m_mode = CinematicCameraMode::Recorded;
    resetSmoothing();
}

void EchoCinematicCamera::resetSmoothing() {
    m_smoothedPose = {};
    m_hasSmoothedPose = false;
    m_lastSourceAttemptId = 0;
    m_lastCursorSeconds = 0.0;
}

CinematicCameraMode EchoCinematicCamera::mode() const {
    return m_mode;
}

char const* EchoCinematicCamera::modeName(CinematicCameraMode mode) {
    switch (mode) {
        case CinematicCameraMode::Recorded: return "RECORDED";
        case CinematicCameraMode::Follow: return "FOLLOW";
        case CinematicCameraMode::Smooth: return "SMOOTH";
        case CinematicCameraMode::Drone: return "DRONE";
        case CinematicCameraMode::DynamicZoom: return "DYN ZOOM";
        case CinematicCameraMode::DeathCam: return "DEATH CAM";
    }
    return "RECORDED";
}

bool EchoCinematicCamera::finitePose(CameraPose const& pose) {
    return
        pose.valid &&
        std::isfinite(pose.x) &&
        std::isfinite(pose.y) &&
        std::isfinite(pose.rotation) &&
        std::isfinite(pose.scaleX) &&
        std::isfinite(pose.scaleY) &&
        std::abs(pose.scaleX) > 0.0001f &&
        std::abs(pose.scaleY) > 0.0001f;
}

CameraPose EchoCinematicCamera::recordedPose(EchoReplayTimeline const& timeline) {
    CameraSnapshot const camera = timeline.cameraAtCursor();

    CameraPose pose;
    pose.valid = camera.present;
    pose.x = camera.x;
    pose.y = camera.y;
    pose.rotation = camera.rotation;
    pose.scaleX = camera.scaleX;
    pose.scaleY = camera.scaleY;

    if (!finitePose(pose)) return {};
    return pose;
}

EchoCinematicCamera::SubjectPoint EchoCinematicCamera::subjectAt(
    EchoReplayTimeline const& timeline,
    double timeSeconds
) {
    SubjectPoint subject;
    PlayerSnapshot const player1 = timeline.playerAtTime(1, timeSeconds);
    PlayerSnapshot const player2 = timeline.playerAtTime(2, timeSeconds);

    bool const p1 = usablePlayer(player1);
    bool const p2 = usablePlayer(player2);

    if (p1 && p2) {
        subject.valid = true;
        subject.x = (player1.x + player2.x) * 0.5f;
        subject.y = (player1.y + player2.y) * 0.5f;
        subject.dualSeparation = std::hypot(
            player2.x - player1.x,
            player2.y - player1.y
        );
        return subject;
    }

    if (p1) {
        subject.valid = true;
        subject.x = player1.x;
        subject.y = player1.y;
        return subject;
    }

    if (p2) {
        subject.valid = true;
        subject.x = player2.x;
        subject.y = player2.y;
        return subject;
    }

    return subject;
}

CameraPose EchoCinematicCamera::centerOnSubject(
    CameraPose const& baseline,
    SubjectPoint const& subject,
    float viewportWidth,
    float viewportHeight,
    float scaleMultiplier
) {
    if (!finitePose(baseline) || !subject.valid) return baseline;
    if (!std::isfinite(scaleMultiplier)) return baseline;

    float const multiplier = std::clamp(scaleMultiplier, 0.65f, 1.25f);

    CameraPose pose = baseline;
    pose.scaleX = baseline.scaleX * multiplier;
    pose.scaleY = baseline.scaleY * multiplier;

    float const radians = baseline.rotation * kPi / 180.0f;
    float const cosine = std::cos(radians);
    float const sine = std::sin(radians);

    float const scaledX = subject.x * pose.scaleX;
    float const scaledY = subject.y * pose.scaleY;
    float const rotatedX = scaledX * cosine - scaledY * sine;
    float const rotatedY = scaledX * sine + scaledY * cosine;

    float const subjectScreenX = baseline.x + rotatedX;
    float const subjectScreenY = baseline.y + rotatedY;
    float const targetScreenX = viewportWidth * 0.5f;
    float const targetScreenY = viewportHeight * 0.5f;

    pose.x = baseline.x + (targetScreenX - subjectScreenX);
    pose.y = baseline.y + (targetScreenY - subjectScreenY);

    return finitePose(pose) ? pose : baseline;
}

float EchoCinematicCamera::subjectSpeed(
    EchoReplayTimeline const& timeline,
    SubjectPoint const& subject
) {
    if (!subject.valid) return 0.0f;

    double const cursor = timeline.cursorSeconds();
    double const sampleDelta = 0.08;
    SubjectPoint const future = subjectAt(timeline, cursor + sampleDelta);
    if (!future.valid) return 0.0f;

    float const distance = std::hypot(
        future.x - subject.x,
        future.y - subject.y
    );
    if (!std::isfinite(distance)) return 0.0f;

    return std::clamp(
        distance / static_cast<float>(sampleDelta),
        0.0f,
        5000.0f
    );
}

CameraPose EchoCinematicCamera::evaluateFollow(
    EchoReplayTimeline const& timeline,
    CameraPose const& baseline,
    float viewportWidth,
    float viewportHeight
) const {
    SubjectPoint const subject = subjectAt(timeline, timeline.cursorSeconds());
    if (!subject.valid) return baseline;
    return centerOnSubject(baseline, subject, viewportWidth, viewportHeight);
}

CameraPose EchoCinematicCamera::evaluateSmooth(
    EchoReplayTimeline const& timeline,
    CameraPose const& target
) {
    if (!finitePose(target)) return target;

    std::uint64_t const source = timeline.sourceAttemptId();
    double const cursor = timeline.cursorSeconds();

    bool const sourceChanged = source == 0 || source != m_lastSourceAttemptId;
    double const delta = cursor - m_lastCursorSeconds;
    bool const discontinuity =
        sourceChanged ||
        !std::isfinite(delta) ||
        delta < 0.0 ||
        delta > kSmoothingDiscontinuitySeconds;

    if (!m_hasSmoothedPose || discontinuity) {
        m_smoothedPose = target;
        m_hasSmoothedPose = true;
    } else if (delta > 0.0) {
        float const alpha = static_cast<float>(
            1.0 - std::exp(-8.0 * delta)
        );
        float const bounded = std::clamp(alpha, 0.0f, 1.0f);
        m_smoothedPose.x = std::lerp(m_smoothedPose.x, target.x, bounded);
        m_smoothedPose.y = std::lerp(m_smoothedPose.y, target.y, bounded);
        m_smoothedPose.rotation = shortestRotation(
            m_smoothedPose.rotation,
            target.rotation,
            bounded
        );
        m_smoothedPose.scaleX = std::lerp(
            m_smoothedPose.scaleX,
            target.scaleX,
            bounded
        );
        m_smoothedPose.scaleY = std::lerp(
            m_smoothedPose.scaleY,
            target.scaleY,
            bounded
        );
        m_smoothedPose.valid = true;
    }

    m_lastSourceAttemptId = source;
    m_lastCursorSeconds = cursor;
    return finitePose(m_smoothedPose) ? m_smoothedPose : target;
}

CameraPose EchoCinematicCamera::evaluateDrone(
    EchoReplayTimeline const& timeline,
    CameraPose const& baseline,
    float viewportWidth,
    float viewportHeight
) const {
    double const cursor = timeline.cursorSeconds();
    SubjectPoint subject = subjectAt(timeline, cursor);
    SubjectPoint const future = subjectAt(timeline, cursor + kDroneLookAheadSeconds);
    if (!subject.valid || !future.valid) {
        return evaluateFollow(timeline, baseline, viewportWidth, viewportHeight);
    }

    float const dx = future.x - subject.x;
    float const dy = future.y - subject.y;
    float const distance = std::hypot(dx, dy);

    if (std::isfinite(distance) && distance > 0.001f) {
        float const lookAhead = std::min(
            distance * 1.4f,
            kMaxDroneLookAheadWorldUnits
        );
        subject.x += dx / distance * lookAhead;
        subject.y += dy / distance * lookAhead;
    }

    return centerOnSubject(baseline, subject, viewportWidth, viewportHeight, 0.94f);
}

CameraPose EchoCinematicCamera::evaluateDynamicZoom(
    EchoReplayTimeline const& timeline,
    CameraPose const& baseline,
    float viewportWidth,
    float viewportHeight
) const {
    SubjectPoint const subject = subjectAt(timeline, timeline.cursorSeconds());
    if (!subject.valid) return baseline;

    float const speed = subjectSpeed(timeline, subject);
    float const speedFactor = std::clamp(speed / 1600.0f, 0.0f, 1.0f);
    float const separationFactor = std::clamp(
        subject.dualSeparation / 400.0f,
        0.0f,
        1.0f
    );

    float const zoomMultiplier = std::clamp(
        1.05f - speedFactor * 0.18f - separationFactor * 0.20f,
        0.72f,
        1.08f
    );

    return centerOnSubject(
        baseline,
        subject,
        viewportWidth,
        viewportHeight,
        zoomMultiplier
    );
}

CameraPose EchoCinematicCamera::evaluateDeathCam(
    EchoReplayTimeline const& timeline,
    CameraPose const& baseline,
    float viewportWidth,
    float viewportHeight
) const {
    auto const* history = timeline.historyEntry();
    if (!history || !history->death.present) return baseline;

    double const deathTime = history->death.timeSeconds;
    if (!std::isfinite(deathTime)) return baseline;

    double const cursor = timeline.cursorSeconds();
    double const start = deathTime - kDeathCamWindowSeconds;
    if (cursor < start) return baseline;

    float const raw = static_cast<float>(
        (cursor - start) / kDeathCamWindowSeconds
    );
    float const ease = smoothstep01(raw);

    SubjectPoint subject = subjectAt(timeline, cursor);
    SubjectPoint deathTarget;
    deathTarget.valid =
        std::isfinite(history->death.x) &&
        std::isfinite(history->death.y);
    deathTarget.x = history->death.x;
    deathTarget.y = history->death.y;

    if (!deathTarget.valid) return baseline;

    if (subject.valid) {
        deathTarget.x = std::lerp(subject.x, deathTarget.x, ease);
        deathTarget.y = std::lerp(subject.y, deathTarget.y, ease);
    }

    float const zoomMultiplier = std::lerp(1.0f, 1.18f, ease);
    return centerOnSubject(
        baseline,
        deathTarget,
        viewportWidth,
        viewportHeight,
        zoomMultiplier
    );
}

} // namespace dash_echo
