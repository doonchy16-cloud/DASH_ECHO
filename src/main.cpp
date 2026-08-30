#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include "EchoGhost.hpp"
#include "EchoRecorder.hpp"

using namespace geode::prelude;

class $modify(DashEchoPlayLayer, PlayLayer) {
    struct Fields {
        dash_echo::EchoRecorder recorder;
        dash_echo::EchoGhost ghost;
    };

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        auto& recorder = m_fields->recorder;
        auto& ghost = m_fields->ghost;

        if (!ghost.isAttached()) {
            cocos2d::CCNode* parent = this->m_objectLayer;
            int zOrder = 0;

            if (this->m_player1 && this->m_player1->getParent()) {
                parent = this->m_player1->getParent();
                zOrder = this->m_player1->getZOrder() - 1;
            }

            ghost.attach(parent, zOrder);
        }

        recorder.captureFrame(
            dt,
            this->getCurrentPercent(),
            this->m_player1,
            this->m_player2
        );

        // One authoritative clock: the current attempt recorder timeline.
        // The ghost never accumulates its own dt, eliminating clock drift.
        ghost.synchronize(recorder.activeElapsedSeconds());
    }

    void resetLevel() {
        auto& recorder = m_fields->recorder;
        auto& ghost = m_fields->ghost;

        ghost.stop();

        if (recorder.hasActiveAttempt()) {
            recorder.finalizeAttempt(dash_echo::AttemptEndReason::Reset);
        }

        PlayLayer::resetLevel();

        recorder.beginAttempt();
        ghost.play(recorder.latestFinalizedAttempt());
    }

    void levelComplete() {
        auto& recorder = m_fields->recorder;
        auto& ghost = m_fields->ghost;

        ghost.stop();

        if (recorder.hasActiveAttempt()) {
            recorder.finalizeAttempt(dash_echo::AttemptEndReason::Completed);
        }

        PlayLayer::levelComplete();
    }

    void onExit() {
        auto& recorder = m_fields->recorder;
        auto& ghost = m_fields->ghost;

        ghost.stop();

        if (recorder.hasActiveAttempt()) {
            recorder.finalizeAttempt(dash_echo::AttemptEndReason::LayerExit);
        }

        auto const stats = recorder.stats();
        log::debug(
            "DASH ECHO v0.3 session closed: {} attempts started, {} finalized, {} frames retained, {} frames dropped",
            stats.attemptsStarted,
            stats.attemptsFinalized,
            stats.retainedFrames,
            stats.framesDropped
        );

        PlayLayer::onExit();
    }
};
