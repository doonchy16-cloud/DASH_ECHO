#pragma once

#include "EchoRecorder.hpp"

#include <cstddef>
#include <cstdint>

class SimplePlayer;

namespace cocos2d {
class CCNode;
}

namespace dash_echo {

class EchoGhost final {
public:
    static constexpr std::uint8_t kGhostOpacity = 96;

    bool attach(cocos2d::CCNode* parent, int zOrder);
    void play(AttemptRecord const* attempt);
    void update(float dt);
    void stop();
    void hide();

    [[nodiscard]] bool isAttached() const;
    [[nodiscard]] bool isPlaying() const;
    [[nodiscard]] std::uint64_t sourceAttemptId() const;

private:
    struct IconProfile {
        int cube = 1;
        int ship = 1;
        int ball = 1;
        int ufo = 1;
        int wave = 1;
        int robot = 1;
        int spider = 1;
        int swing = 1;
    };

    struct VisualCache {
        bool modeInitialized = false;
        PlayerMode mode = PlayerMode::Cube;
        bool colorsInitialized = false;
        ColorRGB color1;
        ColorRGB color2;
    };

    void applyFrame(FrameRecord const& frame);
    void applySnapshot(
        SimplePlayer* ghost,
        PlayerSnapshot const& snapshot,
        VisualCache& cache
    );
    void applyMode(SimplePlayer* ghost, PlayerMode mode, VisualCache& cache);
    void applyColors(
        SimplePlayer* ghost,
        ColorRGB const& color1,
        ColorRGB const& color2,
        VisualCache& cache
    );
    void loadIconProfile();
    void resetVisualCaches();

    SimplePlayer* m_player1Ghost = nullptr;
    SimplePlayer* m_player2Ghost = nullptr;

    AttemptRecord const* m_attempt = nullptr;
    std::size_t m_frameIndex = 0;
    double m_elapsedSeconds = 0.0;
    std::uint64_t m_sourceAttemptId = 0;

    IconProfile m_icons;
    VisualCache m_player1Cache;
    VisualCache m_player2Cache;
};

} // namespace dash_echo
