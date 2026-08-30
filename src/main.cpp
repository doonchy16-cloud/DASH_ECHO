#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include "EchoAttemptHistory.hpp"
#include "EchoDeathAnalytics.hpp"
#include "EchoDeathOverlay.hpp"
#include "EchoGhostFleet.hpp"
#include "EchoRecorder.hpp"
#include "EchoReplaySession.hpp"

#include <algorithm>
#include <cstdint>

using namespace geode::prelude;

class $modify(DashEchoPlayLayer, PlayLayer) {
    struct Fields {
        dash_echo::EchoRecorder recorder;
        dash_echo::EchoGhostFleet fleet;
        dash_echo::EchoDeathAnalytics deaths;
        dash_echo::EchoDeathOverlay deathOverlay;
        dash_echo::EchoAttemptHistory history;
        dash_echo::EchoReplaySession replay;
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

    bool finalizeActiveAttempt(dash_echo::AttemptEndReason reason) {
        auto& recorder = m_fields->recorder;
        auto& deaths = m_fields->deaths;
        auto& history = m_fields->history;
        auto& replay = m_fields->replay;

        auto const* active = recorder.activeAttempt();
        if (!active) return false;

        std::uint64_t const attemptId = active->attemptId;
        auto const* priorBest = recorder.personalBestAttempt();
        float const priorBestProgress =
            priorBest ? priorBest->maxProgressPercent : 0.0f;

        recorder.finalizeAttempt(reason);

        auto const* finalized = recorder.attemptById(attemptId);
        if (!finalized || !finalized->finalized) {
            log::warn(
                "DASH ECHO v0.7 could not resolve finalized attempt {} for history",
                attemptId
            );
            return false;
        }

        auto const* currentBest = recorder.personalBestAttempt();
        std::uint64_t const currentBestAttemptId =
            currentBest ? currentBest->attemptId : 0;
        auto const* death = deaths.deathForAttempt(attemptId);

        bool const committed = history.commitFinalizedAttempt(
            *finalized,
            death,
            priorBestProgress,
            currentBestAttemptId
        );
        if (!committed) return false;

        // v0.7 prepares, but does not auto-start, a fully owned replay clip.
        // Once copied, this replay candidate no longer depends on recorder retention.
        if (auto const* entry = history.entryForAttempt(attemptId)) {
            replay.load(*finalized, *entry);
        }
        return true;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        auto& recorder = m_fields->recorder;
        auto& fleet = m_fields->fleet;
        auto& deaths = m_fields->deaths;
        auto& deathOverlay = m_fields->deathOverlay;
        auto& replay = m_fields->replay;

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
        if (!replay.isAttached()) {
            replay.attach(renderParent, topGhostZOrder);
        }
        if (!deathOverlay.isAttached()) {
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

        fleet.stop();
        finalizeActiveAttempt(dash_echo::AttemptEndReason::Reset);

        PlayLayer::resetLevel();

        m_fields->captureEnabled = true;
        recorder.beginAttempt();

        applyDashEchoSettings();
        fleet.rebuild(recorder);
        deathOverlay.refresh(deaths);
    }

    void levelComplete() {
        auto& fleet = m_fields->fleet;

        m_fields->captureEnabled = false;
        fleet.stop();
        finalizeActiveAttempt(dash_echo::AttemptEndReason::Completed);

        PlayLayer::levelComplete();
    }

    void onExit() {
        auto& recorder = m_fields->recorder;
        auto& fleet = m_fields->fleet;
        auto& deaths = m_fields->deaths;
        auto& deathOverlay = m_fields->deathOverlay;
        auto& history = m_fields->history;
        auto& replay = m_fields->replay;

        m_fields->captureEnabled = false;
        fleet.stop();
        finalizeActiveAttempt(dash_echo::AttemptEndReason::LayerExit);

        auto const recorderStats = recorder.stats();
        auto const fleetStats = fleet.stats();
        auto const deathStats = deaths.stats();
        auto const historyStats = history.stats();
        log::debug(
            "DASH ECHO v0.7 session closed: {} attempts started, {} finalized, {} history retained / {} committed, replay candidate {}, {} deaths, {} completions, PB {}, best {:.2f}%, longest {:.3f}s, {} frames retained, {} dropped, ghost limit {}, {} clusters",
            recorderStats.attemptsStarted,
            recorderStats.attemptsFinalized,
            historyStats.retainedEntries,
            historyStats.totalCommittedAttempts,
            replay.timeline().sourceAttemptId(),
            historyStats.totalDeaths,
            historyStats.totalCompletions,
            historyStats.currentPersonalBestAttemptId,
            historyStats.currentBestProgressPercent,
            historyStats.longestAttemptSeconds,
            recorderStats.retainedFrames,
            recorderStats.framesDropped,
            fleetStats.configuredGhostLimit,
            deathStats.clusterCount
        );

        replay.detach();
        deathOverlay.detach();
        PlayLayer::onExit();
    }
};
