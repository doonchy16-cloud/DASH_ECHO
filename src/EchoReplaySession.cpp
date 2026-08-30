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
    m_camera.reset();

    if (!m_timeline.load(attempt, history)) {
        return false;
    }

    bindGhostToOwnedClip();
    m_ghost.hide();
    return true;
}

void EchoReplaySession::start() {
    if (!m_timeline.isLoaded()) return;

    if (!m_ghost.isPlaying()) bindGhostToOwnedClip();
    m_timeline.start();
    synchronizeGhost();
}

void EchoReplaySession::pause() {
    if (!m_timeline.isLoaded()) return;
    m_timeline.pause();
    synchronizeGhost();
}

void EchoReplaySession::resume() {
    if (!m_timeline.isLoaded()) return;

    if (!m_ghost.isPlaying()) bindGhostToOwnedClip();
    m_timeline.resume();
    synchronizeGhost();
}

void EchoReplaySession::togglePlayback() {
    if (!m_timeline.isLoaded()) return;

    if (!m_ghost.isPlaying()) bindGhostToOwnedClip();
    m_timeline.togglePlayback();
    synchronizeGhost();
}

void EchoReplaySession::advance(float dt) {
    if (!m_timeline.isPlaying()) return;

    m_timeline.advance(dt);
    synchronizeGhost();
}

void EchoReplaySession::restart() {
    if (!m_timeline.isLoaded()) return;

    m_timeline.restart();
    resetCinematicContinuity();
    bindGhostToOwnedClip();
    synchronizeGhost();
}

void EchoReplaySession::stop() {
    if (m_timeline.isLoaded()) {
        m_timeline.restart();
    }
    resetCinematicContinuity();
    m_ghost.stop();
}

void EchoReplaySession::clear() {
    m_ghost.stop();
    m_timeline.clear();
    m_camera.reset();
}

bool EchoReplaySession::setPlaybackRate(float rate) {
    if (!m_timeline.setPlaybackRate(rate)) return false;
    synchronizeGhost();
    return true;
}

void EchoReplaySession::cyclePlaybackRate() {
    if (!m_timeline.isLoaded()) return;
    m_timeline.cyclePlaybackRate();
    synchronizeGhost();
}

bool EchoReplaySession::seekSeconds(double timeSeconds) {
    if (!m_timeline.seekSeconds(timeSeconds)) return false;

    resetCinematicContinuity();
    if (!m_ghost.isPlaying()) bindGhostToOwnedClip();
    synchronizeGhost();
    return true;
}

bool EchoReplaySession::seekNormalized(float normalizedPosition) {
    if (!m_timeline.seekNormalized(normalizedPosition)) return false;

    resetCinematicContinuity();
    if (!m_ghost.isPlaying()) bindGhostToOwnedClip();
    synchronizeGhost();
    return true;
}

bool EchoReplaySession::stepPreviousFrame() {
    if (!m_timeline.stepPreviousFrame()) return false;

    resetCinematicContinuity();
    if (!m_ghost.isPlaying()) bindGhostToOwnedClip();
    synchronizeGhost();
    return true;
}

bool EchoReplaySession::stepNextFrame() {
    if (!m_timeline.stepNextFrame()) return false;

    resetCinematicContinuity();
    if (!m_ghost.isPlaying()) bindGhostToOwnedClip();
    synchronizeGhost();
    return true;
}

void EchoReplaySession::cycleCameraMode() {
    if (!m_timeline.isLoaded()) return;

    auto const* history = m_timeline.historyEntry();
    bool const deathAvailable = history && history->death.present;
    m_camera.cycleMode(deathAvailable);
}

void EchoReplaySession::resetCameraMode() {
    m_camera.reset();
}

CinematicCameraMode EchoReplaySession::cameraMode() const {
    return m_camera.mode();
}

char const* EchoReplaySession::cameraModeName() const {
    return EchoCinematicCamera::modeName(m_camera.mode());
}

CameraPose EchoReplaySession::cameraPose(
    float viewportWidth,
    float viewportHeight
) {
    return m_camera.evaluate(m_timeline, viewportWidth, viewportHeight);
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

void EchoReplaySession::synchronizeGhost() {
    if (!m_timeline.isLoaded() || !m_ghost.isPlaying()) return;
    m_ghost.synchronize(m_timeline.cursorSeconds());
}

void EchoReplaySession::resetCinematicContinuity() {
    m_camera.resetSmoothing();
}

} // namespace dash_echo
