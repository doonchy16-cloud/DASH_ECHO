#pragma once

#include "EchoReplayTimeline.hpp"

#include <cstdint>

namespace dash_echo {

enum class CinematicCameraMode : std::uint8_t {
    Recorded,
    Follow,
    Smooth,
    Drone,
    DynamicZoom,
    DeathCam
};

struct CameraPose {
    bool valid = false;
    float x = 0.0f;
    float y = 0.0f;
    float rotation = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
};

class EchoCinematicCamera final {
public:
    CameraPose evaluate(
        EchoReplayTimeline const& timeline,
        float viewportWidth,
        float viewportHeight
    );

    void setMode(CinematicCameraMode mode);
    void cycleMode(bool deathAvailable);
    void reset();
    void resetSmoothing();

    [[nodiscard]] CinematicCameraMode mode() const;
    [[nodiscard]] static char const* modeName(CinematicCameraMode mode);

private:
    struct SubjectPoint {
        bool valid = false;
        float x = 0.0f;
        float y = 0.0f;
        float dualSeparation = 0.0f;
    };

    static bool finitePose(CameraPose const& pose);
    static CameraPose recordedPose(EchoReplayTimeline const& timeline);
    static SubjectPoint subjectAt(
        EchoReplayTimeline const& timeline,
        double timeSeconds
    );
    static CameraPose centerOnSubject(
        CameraPose const& baseline,
        SubjectPoint const& subject,
        float viewportWidth,
        float viewportHeight,
        float scaleMultiplier = 1.0f
    );
    static float subjectSpeed(
        EchoReplayTimeline const& timeline,
        SubjectPoint const& subject
    );

    CameraPose evaluateFollow(
        EchoReplayTimeline const& timeline,
        CameraPose const& baseline,
        float viewportWidth,
        float viewportHeight
    ) const;
    CameraPose evaluateSmooth(
        EchoReplayTimeline const& timeline,
        CameraPose const& target
    );
    CameraPose evaluateDrone(
        EchoReplayTimeline const& timeline,
        CameraPose const& baseline,
        float viewportWidth,
        float viewportHeight
    ) const;
    CameraPose evaluateDynamicZoom(
        EchoReplayTimeline const& timeline,
        CameraPose const& baseline,
        float viewportWidth,
        float viewportHeight
    ) const;
    CameraPose evaluateDeathCam(
        EchoReplayTimeline const& timeline,
        CameraPose const& baseline,
        float viewportWidth,
        float viewportHeight
    ) const;

    CinematicCameraMode m_mode = CinematicCameraMode::Recorded;
    CameraPose m_smoothedPose;
    bool m_hasSmoothedPose = false;
    std::uint64_t m_lastSourceAttemptId = 0;
    double m_lastCursorSeconds = 0.0;
};

} // namespace dash_echo
