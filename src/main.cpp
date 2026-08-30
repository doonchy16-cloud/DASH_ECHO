#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include "EchoRecorder.hpp"

using namespace geode::prelude;

class $modify(DashEchoPlayLayer, PlayLayer) {
    struct Fields {
        dash_echo::EchoRecorder recorder;
    };

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        m_fields->recorder.captureFrame(
            dt,
            this->getCurrentPercent(),
            this->m_player1,
            this->m_player2
        );
    }

    void resetLevel() {
        auto& recorder = m_fields->recorder;
        if (recorder.hasActiveAttempt()) {
            recorder.finalizeAttempt(dash_echo::AttemptEndReason::Reset);
        }

        PlayLayer::resetLevel();
        recorder.beginAttempt();
    }

    void levelComplete() {
        auto& recorder = m_fields->recorder;
        if (recorder.hasActiveAttempt()) {
            recorder.finalizeAttempt(dash_echo::AttemptEndReason::Completed);
        }

        PlayLayer::levelComplete();
    }

    void onExit() {
        auto& recorder = m_fields->recorder;
        if (recorder.hasActiveAttempt()) {
            recorder.finalizeAttempt(dash_echo::AttemptEndReason::LayerExit);
        }

        auto const stats = recorder.stats();
        log::debug(
            "DASH ECHO v0.1 session closed: {} attempts started, {} finalized, {} frames retained, {} frames dropped",
            stats.attemptsStarted,
            stats.attemptsFinalized,
            stats.retainedFrames,
            stats.framesDropped
        );

        PlayLayer::onExit();
    }
};
