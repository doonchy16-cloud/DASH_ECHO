#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include "EchoGhostFleet.hpp"
#include "EchoRecorder.hpp"

#include <algorithm>
#include <cstdint>

using namespace geode::prelude;

class $modify(DashEchoPlayLayer, PlayLayer) {
    struct Fields {
        dash_echo::EchoRecorder recorder;
        dash_echo::EchoGhostFleet fleet;
        bool captureEnabled = true;
    };

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        auto& recorder = m_fields->recorder;
        auto& fleet = m_fields->fleet;

        if (!fleet.isAttached()) {
            cocos2d::CCNode* parent = this->m_objectLayer;
            int topGhostZOrder = 0;

            if (this->m_player1 && this->m_player1->getParent()) {
                parent = this->m_player1->getParent();
                topGhostZOrder = this->m_player1->getZOrder() - 1;
            }

            fleet.attach(parent, topGhostZOrder);
        }

        if (!m_fields->captureEnabled) return;

        if (!recorder.hasActiveAttempt()) {
            recorder.beginAttempt();
        }

        recorder.captureFrame(
            dt,
            this->getCurrentPercent(),
            this->m_player1,
            this->m_player2
        );

        // Every historical ghost consumes the exact same authoritative current-
        // attempt clock. No fleet member accumulates an independent timeline.
        fleet.synchronize(recorder.activeElapsedSeconds());
    }

    void resetLevel() {
        auto& recorder = m_fields->recorder;
        auto& fleet = m_fields->fleet;

        // Release all historical AttemptRecord references before recorder
        // finalization/retention is allowed to evict old attempts.
        fleet.stop();

        if (recorder.hasActiveAttempt()) {
            recorder.finalizeAttempt(dash_echo::AttemptEndReason::Reset);
        }

        PlayLayer::resetLevel();

        m_fields->captureEnabled = true;
        recorder.beginAttempt();

        // Settings are applied only at an attempt boundary, never while the fleet
        // is actively holding historical AttemptRecord references.
        auto const requestedGhostCount = Mod::get()->getSettingValue<std::int64_t>("ghost-count");
        auto const boundedGhostCount = std::clamp<std::int64_t>(
            requestedGhostCount,
            0,
            static_cast<std::int64_t>(dash_echo::EchoGhostFleet::kMaxGhosts)
        );
        fleet.setGhostLimit(static_cast<std::size_t>(boundedGhostCount));

        // Retention trimming has already completed. Selection now remains stable
        // for the entire active attempt and is rebuilt only at the next boundary.
        fleet.rebuild(recorder);
    }

    void levelComplete() {
        auto& recorder = m_fields->recorder;
        auto& fleet = m_fields->fleet;

        m_fields->captureEnabled = false;
        fleet.stop();

        if (recorder.hasActiveAttempt()) {
            recorder.finalizeAttempt(dash_echo::AttemptEndReason::Completed);
        }

        PlayLayer::levelComplete();
    }

    void onExit() {
        auto& recorder = m_fields->recorder;
        auto& fleet = m_fields->fleet;

        m_fields->captureEnabled = false;
        fleet.stop();

        if (recorder.hasActiveAttempt()) {
            recorder.finalizeAttempt(dash_echo::AttemptEndReason::LayerExit);
        }

        auto const recorderStats = recorder.stats();
        auto const fleetStats = fleet.stats();
        log::debug(
            "DASH ECHO v0.4 session closed: {} attempts started, {} finalized, {} frames retained, {} frames dropped, ghost limit {}, PB attempt {}",
            recorderStats.attemptsStarted,
            recorderStats.attemptsFinalized,
            recorderStats.retainedFrames,
            recorderStats.framesDropped,
            fleetStats.configuredGhostLimit,
            fleetStats.personalBestAttemptId
        );

        PlayLayer::onExit();
    }
};
