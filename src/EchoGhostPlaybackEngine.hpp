#pragma once

#include "EchoRecorder.hpp"

#include <cstdint>

namespace dash_echo {

enum class GhostPlaybackPhase : std::uint8_t {
    Tracking,
    Continuing
};

class EchoGhostPlaybackEngine final {
public:
    void reset();

    void track(
        double liveElapsedSeconds,
        float liveProgressPercent,
        bool progressAuthority
    );
    void beginContinuation(
        double liveElapsedSeconds,
        float liveProgressPercent,
        bool progressAuthority
    );
    void advance(double dt);

    [[nodiscard]] double resolveTime(
        AttemptRecord const& attempt,
        bool progressAlignmentSafe
    ) const;
    [[nodiscard]] bool finished(
        AttemptRecord const& attempt,
        bool progressAlignmentSafe
    ) const;

    [[nodiscard]] bool isContinuing() const;
    [[nodiscard]] GhostPlaybackPhase phase() const;
    [[nodiscard]] double continuationElapsedSeconds() const;

    [[nodiscard]] static bool supportsProgressAlignment(
        AttemptRecord const& attempt
    );

private:
    [[nodiscard]] double trackingAnchorTime(
        AttemptRecord const& attempt,
        bool progressAlignmentSafe
    ) const;
    [[nodiscard]] static double timeForProgress(
        AttemptRecord const& attempt,
        float progressPercent,
        double fallbackTimeSeconds
    );

    GhostPlaybackPhase m_phase = GhostPlaybackPhase::Tracking;
    double m_anchorElapsedSeconds = 0.0;
    float m_anchorProgressPercent = 0.0f;
    bool m_progressAuthority = false;
    double m_continuationElapsedSeconds = 0.0;
};

} // namespace dash_echo
