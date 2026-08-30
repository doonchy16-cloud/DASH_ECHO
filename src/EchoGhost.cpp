#include "EchoGhost.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/SimplePlayer.hpp>

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace dash_echo {

bool EchoGhost::attach(cocos2d::CCNode* parent, int zOrder) {
    if (isAttached()) {
        return true;
    }
    if (!parent) {
        return false;
    }

    loadIconProfile();

    auto* player1Ghost = SimplePlayer::create(m_icons.cube);
    auto* player2Ghost = SimplePlayer::create(m_icons.cube);
    if (!player1Ghost || !player2Ghost) {
        return false;
    }

    applyOpacity(player1Ghost);
    applyOpacity(player2Ghost);
    player1Ghost->setVisible(false);
    player2Ghost->setVisible(false);

    parent->addChild(player1Ghost, zOrder);
    parent->addChild(player2Ghost, zOrder);

    m_player1Ghost = player1Ghost;
    m_player2Ghost = player2Ghost;
    resetVisualCaches();
    return true;
}

void EchoGhost::detach() {
    stop();

    if (m_player1Ghost) {
        m_player1Ghost->removeFromParentAndCleanup(true);
        m_player1Ghost = nullptr;
    }
    if (m_player2Ghost) {
        m_player2Ghost->removeFromParentAndCleanup(true);
        m_player2Ghost = nullptr;
    }

    resetVisualCaches();
}

void EchoGhost::play(AttemptRecord const* attempt) {
    stop();

    if (!attempt || !attempt->finalized || attempt->frames.empty()) {
        return;
    }

    m_attempt = attempt;
    m_frameIndex = 0;
    m_lastSynchronizedTime = 0.0;
    m_sourceAttemptId = attempt->attemptId;
}

void EchoGhost::synchronize(double timeSeconds) {
    if (!isPlaying()) {
        return;
    }

    if (!std::isfinite(timeSeconds) || timeSeconds < 0.0) {
        hide();
        return;
    }

    auto const& frames = m_attempt->frames;
    if (frames.empty()) {
        stop();
        return;
    }

    // Recorded samples, not attempt duration, define the renderable timeline.
    // This avoids freezing the last ghost frame if capture hit its hard frame cap.
    if (timeSeconds > frames.back().timeSeconds) {
        stop();
        return;
    }

    if (timeSeconds < frames.front().timeSeconds) {
        m_lastSynchronizedTime = timeSeconds;
        hide();
        return;
    }

    seekFrameCursor(timeSeconds);

    auto const& from = frames[m_frameIndex];
    if (m_frameIndex + 1 >= frames.size()) {
        applySnapshot(m_player1Ghost, from.player1, m_player1Cache);
        applySnapshot(m_player2Ghost, from.player2, m_player2Cache);
        m_lastSynchronizedTime = timeSeconds;
        return;
    }

    auto const& to = frames[m_frameIndex + 1];
    double const span = to.timeSeconds - from.timeSeconds;
    float alpha = 0.0f;
    if (std::isfinite(span) && span > 0.0) {
        alpha = static_cast<float>((timeSeconds - from.timeSeconds) / span);
        alpha = std::clamp(alpha, 0.0f, 1.0f);
    }

    applyInterpolatedFrame(from, to, alpha);
    m_lastSynchronizedTime = timeSeconds;
}

void EchoGhost::stop() {
    m_attempt = nullptr;
    m_frameIndex = 0;
    m_lastSynchronizedTime = 0.0;
    m_sourceAttemptId = 0;
    resetVisualCaches();
    hide();
}

void EchoGhost::hide() {
    if (m_player1Ghost) {
        m_player1Ghost->setVisible(false);
    }
    if (m_player2Ghost) {
        m_player2Ghost->setVisible(false);
    }
}

void EchoGhost::setOpacity(std::uint8_t opacity) {
    m_opacity = opacity;
    applyOpacity(m_player1Ghost);
    applyOpacity(m_player2Ghost);
}

bool EchoGhost::isAttached() const {
    return m_player1Ghost != nullptr && m_player2Ghost != nullptr;
}

bool EchoGhost::isPlaying() const {
    return m_attempt != nullptr && m_attempt->finalized && !m_attempt->frames.empty();
}

std::uint64_t EchoGhost::sourceAttemptId() const {
    return m_sourceAttemptId;
}

std::uint8_t EchoGhost::opacity() const {
    return m_opacity;
}

void EchoGhost::seekFrameCursor(double timeSeconds) {
    auto const& frames = m_attempt->frames;
    if (frames.empty()) {
        m_frameIndex = 0;
        return;
    }

    // Normal gameplay is monotonic, so this path is O(1) amortized.
    if (timeSeconds >= m_lastSynchronizedTime && timeSeconds >= frames[m_frameIndex].timeSeconds) {
        while (
            m_frameIndex + 1 < frames.size() &&
            frames[m_frameIndex + 1].timeSeconds <= timeSeconds
        ) {
            ++m_frameIndex;
        }
        return;
    }

    // Backward/non-monotonic seeks are already supported for later replay scrubbing.
    auto upper = std::upper_bound(
        frames.begin(),
        frames.end(),
        timeSeconds,
        [](double value, FrameRecord const& frame) {
            return value < frame.timeSeconds;
        }
    );

    if (upper == frames.begin()) {
        m_frameIndex = 0;
    } else {
        m_frameIndex = static_cast<std::size_t>(std::distance(frames.begin(), upper - 1));
    }
}

void EchoGhost::applyInterpolatedFrame(
    FrameRecord const& from,
    FrameRecord const& to,
    float alpha
) {
    applyInterpolatedSnapshot(
        m_player1Ghost,
        from.player1,
        to.player1,
        to.player1ContinuousFromPrevious,
        alpha,
        m_player1Cache
    );
    applyInterpolatedSnapshot(
        m_player2Ghost,
        from.player2,
        to.player2,
        to.player2ContinuousFromPrevious,
        alpha,
        m_player2Cache
    );
}

void EchoGhost::applyInterpolatedSnapshot(
    SimplePlayer* ghost,
    PlayerSnapshot const& from,
    PlayerSnapshot const& to,
    bool continuous,
    float alpha,
    VisualCache& cache
) {
    if (!continuous) {
        // A discontinuity belongs to the newer sample. Hold the old state until
        // its exact timestamp, then seekFrameCursor snaps to the new sample.
        applySnapshot(ghost, from, cache);
        return;
    }

    PlayerSnapshot blended = from;
    blended.x = std::lerp(from.x, to.x, alpha);
    blended.y = std::lerp(from.y, to.y, alpha);
    blended.rotation = interpolateRotation(from.rotation, to.rotation, alpha);
    blended.scaleX = std::lerp(from.scaleX, to.scaleX, alpha);
    blended.scaleY = std::lerp(from.scaleY, to.scaleY, alpha);
    blended.color1 = interpolateColor(from.color1, to.color1, alpha);
    blended.color2 = interpolateColor(from.color2, to.color2, alpha);

    applySnapshot(ghost, blended, cache);
}

void EchoGhost::applySnapshot(
    SimplePlayer* ghost,
    PlayerSnapshot const& snapshot,
    VisualCache& cache
) {
    if (!ghost) {
        return;
    }

    bool const shouldShow = snapshot.present && snapshot.visible;
    if (!shouldShow) {
        ghost->setVisible(false);
        return;
    }

    applyMode(ghost, snapshot.mode, cache);
    applyColors(ghost, snapshot.color1, snapshot.color2, cache);

    ghost->setPosition({snapshot.x, snapshot.y});
    ghost->setRotation(snapshot.rotation);
    ghost->setScaleX(snapshot.scaleX);
    ghost->setScaleY(snapshot.scaleY);
    applyOpacity(ghost);
    ghost->setVisible(true);
}

void EchoGhost::applyMode(SimplePlayer* ghost, PlayerMode mode, VisualCache& cache) {
    if (!ghost || (cache.modeInitialized && cache.mode == mode)) {
        return;
    }

    int iconId = m_icons.cube;
    IconType iconType = IconType::Cube;

    switch (mode) {
        case PlayerMode::Cube:   iconId = m_icons.cube;   iconType = IconType::Cube; break;
        case PlayerMode::Ship:   iconId = m_icons.ship;   iconType = IconType::Ship; break;
        case PlayerMode::Ball:   iconId = m_icons.ball;   iconType = IconType::Ball; break;
        case PlayerMode::Ufo:    iconId = m_icons.ufo;    iconType = IconType::Ufo; break;
        case PlayerMode::Wave:   iconId = m_icons.wave;   iconType = IconType::Wave; break;
        case PlayerMode::Robot:  iconId = m_icons.robot;  iconType = IconType::Robot; break;
        case PlayerMode::Spider: iconId = m_icons.spider; iconType = IconType::Spider; break;
        case PlayerMode::Swing:  iconId = m_icons.swing;  iconType = IconType::Swing; break;
    }

    ghost->updatePlayerFrame(iconId, iconType);
    applyOpacity(ghost);

    cache.mode = mode;
    cache.modeInitialized = true;
    cache.colorsInitialized = false;
}

void EchoGhost::applyColors(
    SimplePlayer* ghost,
    ColorRGB const& color1,
    ColorRGB const& color2,
    VisualCache& cache
) {
    if (!ghost) {
        return;
    }

    if (
        cache.colorsInitialized &&
        cache.color1 == color1 &&
        cache.color2 == color2
    ) {
        return;
    }

    cocos2d::ccColor3B const primary {color1.r, color1.g, color1.b};
    cocos2d::ccColor3B const secondary {color2.r, color2.g, color2.b};
    ghost->setColors(primary, secondary);
    applyOpacity(ghost);

    cache.color1 = color1;
    cache.color2 = color2;
    cache.colorsInitialized = true;
}

void EchoGhost::applyOpacity(SimplePlayer* ghost) {
    if (ghost) {
        ghost->setOpacity(m_opacity);
    }
}

void EchoGhost::loadIconProfile() {
    auto* gameManager = GameManager::sharedState();
    if (!gameManager) {
        m_icons = {};
        return;
    }

    m_icons.cube = gameManager->getPlayerFrame();
    m_icons.ship = gameManager->getPlayerShip();
    m_icons.ball = gameManager->getPlayerBall();
    m_icons.ufo = gameManager->getPlayerBird();
    m_icons.wave = gameManager->getPlayerDart();
    m_icons.robot = gameManager->getPlayerRobot();
    m_icons.spider = gameManager->getPlayerSpider();
    m_icons.swing = gameManager->getPlayerSwing();
}

void EchoGhost::resetVisualCaches() {
    m_player1Cache = {};
    m_player2Cache = {};
}

float EchoGhost::interpolateRotation(float from, float to, float alpha) {
    float const shortestDelta = std::remainder(to - from, 360.0f);
    return from + shortestDelta * alpha;
}

ColorRGB EchoGhost::interpolateColor(
    ColorRGB const& from,
    ColorRGB const& to,
    float alpha
) {
    auto channel = [alpha](std::uint8_t a, std::uint8_t b) -> std::uint8_t {
        float const value = std::lerp(static_cast<float>(a), static_cast<float>(b), alpha);
        return static_cast<std::uint8_t>(std::clamp(std::lround(value), 0L, 255L));
    };

    return ColorRGB {
        channel(from.r, to.r),
        channel(from.g, to.g),
        channel(from.b, to.b)
    };
}

} // namespace dash_echo
