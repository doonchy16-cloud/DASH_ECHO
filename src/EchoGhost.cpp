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

    player1Ghost->setOpacity(kGhostOpacity);
    player2Ghost->setOpacity(kGhostOpacity);
    player1Ghost->setVisible(false);
    player2Ghost->setVisible(false);

    parent->addChild(player1Ghost, zOrder);
    parent->addChild(player2Ghost, zOrder);

    m_player1Ghost = player1Ghost;
    m_player2Ghost = player2Ghost;
    resetVisualCaches();
    return true;
}

void EchoGhost::play(AttemptRecord const* attempt) {
    stop();

    if (!attempt || !attempt->finalized || attempt->frames.empty()) {
        return;
    }

    m_attempt = attempt;
    m_frameIndex = 0;
    m_elapsedSeconds = 0.0;
    m_sourceAttemptId = attempt->attemptId;
}

void EchoGhost::update(float dt) {
    if (!isPlaying()) {
        return;
    }

    float safeDt = 0.0f;
    if (std::isfinite(dt)) {
        safeDt = std::clamp(dt, 0.0f, 0.25f);
    }
    m_elapsedSeconds += static_cast<double>(safeDt);

    auto const& frames = m_attempt->frames;
    if (frames.empty() || m_elapsedSeconds > m_attempt->durationSeconds) {
        stop();
        return;
    }

    if (m_elapsedSeconds < frames.front().timeSeconds) {
        hide();
        return;
    }

    while (
        m_frameIndex + 1 < frames.size() &&
        frames[m_frameIndex + 1].timeSeconds <= m_elapsedSeconds
    ) {
        ++m_frameIndex;
    }

    applyFrame(frames[m_frameIndex]);
}

void EchoGhost::stop() {
    m_attempt = nullptr;
    m_frameIndex = 0;
    m_elapsedSeconds = 0.0;
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

bool EchoGhost::isAttached() const {
    return m_player1Ghost != nullptr && m_player2Ghost != nullptr;
}

bool EchoGhost::isPlaying() const {
    return m_attempt != nullptr && m_attempt->finalized && !m_attempt->frames.empty();
}

std::uint64_t EchoGhost::sourceAttemptId() const {
    return m_sourceAttemptId;
}

void EchoGhost::applyFrame(FrameRecord const& frame) {
    applySnapshot(m_player1Ghost, frame.player1, m_player1Cache);
    applySnapshot(m_player2Ghost, frame.player2, m_player2Cache);
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
    ghost->setOpacity(kGhostOpacity);
    ghost->setVisible(true);
}

void EchoGhost::applyMode(SimplePlayer* ghost, PlayerMode mode, VisualCache& cache) {
    if (!ghost || (cache.modeInitialized && cache.mode == mode)) {
        return;
    }

    int iconId = m_icons.cube;
    IconType iconType = IconType::Cube;

    switch (mode) {
        case PlayerMode::Cube:
            iconId = m_icons.cube;
            iconType = IconType::Cube;
            break;
        case PlayerMode::Ship:
            iconId = m_icons.ship;
            iconType = IconType::Ship;
            break;
        case PlayerMode::Ball:
            iconId = m_icons.ball;
            iconType = IconType::Ball;
            break;
        case PlayerMode::Ufo:
            iconId = m_icons.ufo;
            iconType = IconType::Ufo;
            break;
        case PlayerMode::Wave:
            iconId = m_icons.wave;
            iconType = IconType::Wave;
            break;
        case PlayerMode::Robot:
            iconId = m_icons.robot;
            iconType = IconType::Robot;
            break;
        case PlayerMode::Spider:
            iconId = m_icons.spider;
            iconType = IconType::Spider;
            break;
        case PlayerMode::Swing:
            iconId = m_icons.swing;
            iconType = IconType::Swing;
            break;
    }

    ghost->updatePlayerFrame(iconId, iconType);
    ghost->setOpacity(kGhostOpacity);

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
    ghost->setOpacity(kGhostOpacity);

    cache.color1 = color1;
    cache.color2 = color2;
    cache.colorsInitialized = true;
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

} // namespace dash_echo
