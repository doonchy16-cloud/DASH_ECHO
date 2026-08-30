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
    void synchronize(double timeSeconds);
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

    void seekFrameCursor(double timeSeconds);
    void applyInterpolatedFrame(
        FrameRecord const& from,
        FrameRecord const& to,
        float alpha
    );
    void applyInterpolatedSnapshot(
        SimplePlayer* ghost,
        PlayerSnapshot const& from,
        PlayerSnapshot const& to,
        bool continuous,
        float alpha,
        VisualCache& cache
    );
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

    static float interpolateRotation(float from, float to, float alpha);
    static ColorRGB interpolateColor(ColorRGB const& from, ColorRGB const& to, float alpha);

    SimplePlayer* m_player1Ghost = nullptr;
    SimplePlayer* m_player2Ghost = nullptr;

    AttemptRecord const* m_attempt = nullptr;
    std::size_t m_frameIndex = 0;
    double m_lastSynchronizedTime = 0.0;
    std::uint64_t m_sourceAttemptId = 0;

    IconProfile m_icons;
    VisualCache m_player1Cache;
    VisualCache m_player2Cache;
};

} // namespace dash_echo
