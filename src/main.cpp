#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include "EchoDeathAnalytics.hpp"
#include "EchoDeathOverlay.hpp"
#include "EchoGhostFleet.hpp"
#include "EchoRecorder.hpp"

#include <algorithm>
#include <cstdint>

using namespace geode::prelude;

class $modify(DashEchoPlayLayer, PlayLayer) {
    struct Fields {
        dash_echo::EchoRecorder recorder;
        dash_echo::EchoGhostFleet fleet;
        dash_echo::EchoDeathAnalytics deaths;
        dash_echo::EchoDeathOverlay deathOverlay;
        bool captureEnabled = true;
        bool settingsLoaded = false;
    };

    void applyDashEchoSettings() {
        auto const requestedGhostCount = Mod::get()->getSettingValue<std::int64_t>("ghost-count");
        auto const boundedGhostCount = std::clamp<std::int64_t>(
            requestedGhostCount,
            0,
            static_cast<std::int64_t>(dash_echo::EchoGhostFleet::kMaxGhosts)
        );
        m_fields->fleet.setGhostLimit(static_cast<std::size_t>(boundedGhostCount));

        bool const deathMarkersEnabled =
            Mod::get()->getSettingValue<bool>("death-markers");
        m_fields->deathOverlay.setEnabled(deathMarkersEnabled);
        m_fields->settingsLoaded = true;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        auto& recorder = m_fields->recorder;
        auto& fleet = m_fields->fleet;
        auto& deaths = m_fields->deaths;
        auto& deathOverlay = m_fields->deathOverlay;

        if (!m_fields->settingsLoaded) {
            applyDashEchoSettings();
        }

        cocos2d::CCNode* renderParent = this->m_objectLayer;
        int topGhostZOrder = 0;
        if (this->m_player1 && this->m_player1->getParent()) {
            renderParent = this->m_player1->getParent();
            topGhostZOrder = this->m_player1->getZOrder() - 1;
        }

        if (!fleet.isAttached()) {
            fleet.attach(renderParent, topGhostZOrder);
        }
        if (!deathOverlay.isAttached()) {
            // The death overlay shares the live player's coordinate space and is
            // inserted below the live player. Cluster visuals never affect physics.
            deathOverlay.attach(renderParent, topGhostZOrder);
        }
        deathOverlay.refresh(deaths);

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

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        auto& recorder = m_fields->recorder;
        auto& deaths = m_fields->deaths;
        auto& deathOverlay = m_fields->deathOverlay;

        dash_echo::DeathEvent candidate;
        bool candidateValid = false;
        bool const wasDeadBefore = player && player->m_isDead;

        if (
            m_fields->captureEnabled &&
            player &&
            recorder.hasActiveAttempt()
        ) {
            auto const* attempt = recorder.activeAttempt();
            if (attempt) {
                auto const playerPosition = player->getPosition();
                candidate.attemptId = attempt->attemptId;
                candidate.timeSeconds = recorder.activeElapsedSeconds();
                candidate.progressPercent = this->getCurrentPercent();
                candidate.playerIndex = player == this->m_player2 ? 2 : 1;
                candidate.x = playerPosition.x;
                candidate.y = playerPosition.y;

                if (object) {
                    auto const hazardPosition = object->getPosition();
                    candidate.hazardPresent = true;
                    candidate.hazardObjectId = object->m_objectID;
                    candidate.hazardX = hazardPosition.x;
                    candidate.hazardY = hazardPosition.y;
                }

                candidateValid = true;
            }
        }

        // Let Geometry Dash and the rest of the hook chain decide whether this
        // destroy request actually produces a death. Analytics observe outcome;
        // they never become death/physics authority.
        PlayLayer::destroyPlayer(player, object);

        if (
            candidateValid &&
            player &&
            !wasDeadBefore &&
            player->m_isDead &&
            deaths.recordDeath(candidate)
        ) {
            deathOverlay.refresh(deaths);
        }
    }

    void resetLevel() {
        auto& recorder = m_fields->recorder;
        auto& fleet = m_fields->fleet;
        auto& deaths = m_fields->deaths;
        auto& deathOverlay = m_fields->deathOverlay;

        // Release all historical AttemptRecord references before recorder
        // finalization/retention is allowed to evict old attempts.
        fleet.stop();

        if (recorder.hasActiveAttempt()) {
            recorder.finalizeAttempt(dash_echo::AttemptEndReason::Reset);
        }

        PlayLayer::resetLevel();

        m_fields->captureEnabled = true;
        recorder.beginAttempt();

        // Settings are applied only at attempt boundaries after the initial load.
        // This avoids mutating fleet selection while it holds history references.
        applyDashEchoSettings();

        // Retention trimming has already completed. Selection now remains stable
        // for the entire active attempt and is rebuilt only at the next boundary.
        fleet.rebuild(recorder);
        deathOverlay.refresh(deaths);
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
        auto& deaths = m_fields->deaths;
        auto& deathOverlay = m_fields->deathOverlay;

        m_fields->captureEnabled = false;
        fleet.stop();

        if (recorder.hasActiveAttempt()) {
            recorder.finalizeAttempt(dash_echo::AttemptEndReason::LayerExit);
        }

        auto const recorderStats = recorder.stats();
        auto const fleetStats = fleet.stats();
        auto const deathStats = deaths.stats();
        log::debug(
            "DASH ECHO v0.5 session closed: {} attempts started, {} finalized, {} frames retained, {} frames dropped, ghost limit {}, PB attempt {}, {} deaths, {} clusters, hottest cluster {}",
            recorderStats.attemptsStarted,
            recorderStats.attemptsFinalized,
            recorderStats.retainedFrames,
            recorderStats.framesDropped,
            fleetStats.configuredGhostLimit,
            fleetStats.personalBestAttemptId,
            deathStats.retainedDeathEvents,
            deathStats.clusterCount,
            deathStats.hottestClusterDeaths
        );

        deathOverlay.detach();
        PlayLayer::onExit();
    }
};
