#pragma once

#include "EchoCinematicCamera.hpp"
#include "EchoGhost.hpp"
#include "EchoReplayTimeline.hpp"

#include <cstdint>

namespace cocos2d {
class CCNode;
}

namespace dash_echo {

class EchoReplaySession final {
public:
    static constexpr std::uint8_t kReplayOpacity = 176;

    bool attach(cocos2d::CCNode* parent, int zOrder);
    void detach();

    bool load(AttemptRecord const& attempt, AttemptHistoryEntry const& history);
    void start();
    void pause();
    void resume();
    void togglePlayback();
    void advance(float dt);
    void restart();
    void stop();
    void clear();

    bool setPlaybackRate(float rate);
    void cyclePlaybackRate();
    bool seekSeconds(double timeSeconds);
    bool seekNormalized(float normalizedPosition);
    bool stepPreviousFrame();
    bool stepNextFrame();

    void cycleCameraMode();
    void resetCameraMode();
    [[nodiscard]] CinematicCameraMode cameraMode() const;
    [[nodiscard]] char const* cameraModeName() const;
    [[nodiscard]] CameraPose cameraPose(
        float viewportWidth,
        float viewportHeight
    );

    [[nodiscard]] bool isAttached() const;
    [[nodiscard]] bool isLoaded() const;
    [[nodiscard]] bool isPlaying() const;
    [[nodiscard]] EchoReplayTimeline const& timeline() const;
    [[nodiscard]] EchoReplayTimeline& timeline();

private:
    void bindGhostToOwnedClip();
    void synchronizeGhost();
    void resetCinematicContinuity();

    EchoReplayTimeline m_timeline;
    EchoGhost m_ghost;
    EchoCinematicCamera m_camera;
};

} // namespace dash_echo
