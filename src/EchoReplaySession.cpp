#include "EchoReplaySession.hpp"

namespace dash_echo {

bool EchoReplaySession::attach(cocos2d::CCNode* parent, int zOrder) {
    if (m_ghost.isAttached()) return true;
    if (!m_ghost.attach(parent, zOrder)) return false;

    m_ghost.setOpacity(kReplayOpacity);
    m_ghost.hide();
    return true;
}

void EchoReplaySession::detach() {
    m_ghost.detach();
}

bool EchoReplaySession::load(
    AttemptRecord const& attempt,
    AttemptHistoryEntry const& history
) {
    m_ghost.stop();
    if (!m_timeline.load(attempt, history)) {
        return false;
    }

    bindGhostToOwnedClip();
    m_ghost.hide();
    return true;
}

void EchoReplaySession::start() {
    if (!m_timeline.isLoaded()) return;

    if (!m_ghost.isPlaying()) {
        bindGhostToOwnedClip();
    }
    m_timeline.start();
    m_ghost.synchronize(m_timeline.cursorSeconds());
}

void EchoReplaySession::advance(float dt) {
    if (!m_timeline.isPlaying()) return;

    m_timeline.advance(dt);
    m_ghost.synchronize(m_timeline.cursorSeconds());
}

void EchoReplaySession::restart() {
    if (!m_timeline.isLoaded()) return;

    m_timeline.restart();
    bindGhostToOwnedClip();
    m_ghost.synchronize(m_timeline.cursorSeconds());
}

void EchoReplaySession::stop() {
    if (m_timeline.isLoaded()) {
        m_timeline.restart();
    }
    m_ghost.stop();
}

void EchoReplaySession::clear() {
    m_ghost.stop();
    m_timeline.clear();
}

bool EchoReplaySession::isAttached() const {
    return m_ghost.isAttached();
}

bool EchoReplaySession::isLoaded() const {
    return m_timeline.isLoaded();
}

bool EchoReplaySession::isPlaying() const {
    return m_timeline.isPlaying();
}

EchoReplayTimeline const& EchoReplaySession::timeline() const {
    return m_timeline;
}

EchoReplayTimeline& EchoReplaySession::timeline() {
    return m_timeline;
}

void EchoReplaySession::bindGhostToOwnedClip() {
    auto const* attempt = m_timeline.replayAttempt();
    if (!attempt) {
        m_ghost.stop();
        return;
    }

    m_ghost.setOpacity(kReplayOpacity);
    m_ghost.play(attempt);
}

} // namespace dash_echo
