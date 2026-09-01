#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>

#include "EchoAttemptHistory.hpp"
#include "EchoDeathAnalytics.hpp"
#include "EchoDeathOverlay.hpp"
#include "EchoGhostFleet.hpp"
#include "EchoHeatmapOverlay.hpp"
#include "EchoRecorder.hpp"
#include "EchoReplayArchive.hpp"
#include "EchoReplayControls.hpp"
#include "EchoReplaySession.hpp"
#include "EchoTimePolicy.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

#include <fmt/format.h>

using namespace geode::prelude;

namespace {

constexpr char const* kReleaseName = "ECHO_DASH";
constexpr char const* kReleaseVersion = "v1.1.3";

dash_echo::ColorRGB toEchoColor(cocos2d::ccColor3B const& color) {
    return dash_echo::ColorRGB {
        static_cast<std::uint8_t>(color.r),
        static_cast<std::uint8_t>(color.g),
        static_cast<std::uint8_t>(color.b)
    };
}

std::size_t profileGhostCap(std::string const& profile) {
    if (profile == "Clean") return 2;
    if (profile == "Competitive") return 8;
    if (profile == "Multiverse") return 64;
    if (profile == "Chaos") return dash_echo::EchoGhostFleet::kMaxGhosts;
    return dash_echo::EchoGhostFleet::kMaxGhosts;
}

float playbackRateFromSetting(std::string const& value) {
    if (value == "0.10") return 0.10f;
    if (value == "0.25") return 0.25f;
    if (value == "0.50") return 0.50f;
    if (value == "2.00") return 2.00f;
    return 1.00f;
}

dash_echo::CinematicCameraMode cameraModeFromSetting(std::string const& value) {
    if (value == "Follow") return dash_echo::CinematicCameraMode::Follow;
    if (value == "Smooth") return dash_echo::CinematicCameraMode::Smooth;
    if (value == "Drone") return dash_echo::CinematicCameraMode::Drone;
    if (value == "Dynamic Zoom") return dash_echo::CinematicCameraMode::DynamicZoom;
    if (value == "Death Cam") return dash_echo::CinematicCameraMode::DeathCam;
    return dash_echo::CinematicCameraMode::Recorded;
}

} // namespace

class $modify(EchoDashPlayLayer, PlayLayer) {
    struct Fields {
        struct ReplayViewportRestore {
            bool valid = false;
            float x = 0.0f;
            float y = 0.0f;
            float rotation = 0.0f;
            float scaleX = 1.0f;
            float scaleY = 1.0f;
        };

        dash_echo::EchoRecorder recorder;
        dash_echo::EchoReplayArchive archive;
        dash_echo::EchoGhostFleet fleet;
        dash_echo::EchoDeathAnalytics deaths;
        dash_echo::EchoDeathOverlay deathOverlay;
        dash_echo::EchoHeatmapOverlay heatmapOverlay;
        dash_echo::EchoAttemptHistory history;
        dash_echo::EchoReplaySession replay;
        dash_echo::EchoReplayControls* replayControls = nullptr;
        cocos2d::CCLabelBMFont* diagnosticsLabel = nullptr;
        ReplayViewportRestore replayViewportRestore;
        dash_echo::EchoLevelContext levelContext;
        bool captureEnabled = true;
        bool confirmedDeath = false;
        bool deferredResetRequested = false;
        bool archiveReady = false;
        bool replayStudioOpen = false;
        bool fleetNeedsRebuild = true;
        bool diagnosticsEnabled = false;
        float sessionBestProgress = 0.0f;
        float settingsPollSeconds = 0.0f;
        float diagnosticsPollSeconds = 0.0f;
        std::string settingsFingerprint;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        applyEchoDashSettings(true);
        initializeArchive(level);
        ensureReplayControls();
        ensureRenderSystems();

        // Attempt #1 must be created by the same explicit lifecycle path used
        // after every reset. This removes the v1.1 lazy-first-attempt asymmetry.
        startNewAttempt();
        return true;
    }

    dash_echo::EchoLevelContext makeLevelContext(GJGameLevel* level) const {
        dash_echo::EchoLevelContext context;
        if (!level) return context;

        context.levelId = static_cast<std::int64_t>(level->m_levelID);
        context.levelName = std::string(level->m_levelName.c_str());
        context.platformer = level->isPlatformer();
        context.practice = this->m_isPracticeMode;
        context.gdNormalPercent = context.platformer ? 0 : level->getNormalPercent();

        if (context.levelId == 0) {
            std::string const fallbackMaterial =
                context.levelName + "\n" + std::string(level->m_levelString.c_str());
            context.fallbackHash = dash_echo::EchoReplayArchive::stableNameHash(
                fallbackMaterial
            );
        }
        return context;
    }

    void restoreDeathsFromArchive() {
        auto& deaths = m_fields->deaths;
        deaths.clear();

        for (auto const& summary : m_fields->archive.summaries()) {
            if (!summary.death.present) continue;

            dash_echo::DeathEvent event;
            event.attemptId = summary.attemptId;
            event.timeSeconds = summary.death.timeSeconds;
            event.progressPercent = summary.death.progressPercent;
            event.playerIndex = summary.death.playerIndex;
            event.x = summary.death.x;
            event.y = summary.death.y;
            event.hazardPresent = summary.death.hazardPresent;
            event.hazardObjectId = summary.death.hazardObjectId;
            event.hazardX = summary.death.hazardX;
            event.hazardY = summary.death.hazardY;
            deaths.recordDeath(event);
        }
    }

    void initializeArchive(GJGameLevel* level) {
        auto& archive = m_fields->archive;
        auto& recorder = m_fields->recorder;
        auto& history = m_fields->history;
        auto& replay = m_fields->replay;

        m_fields->levelContext = makeLevelContext(level);
        history.clear();
        recorder.clear();

        bool const loaded = archive.load(m_fields->levelContext);
        if (!loaded) {
            log::warn(
                "{} {} rejected or could not read archive for {}. Starting with an empty safe archive.",
                kReleaseName,
                kReleaseVersion,
                m_fields->levelContext.storageKey()
            );
        }

        auto const archiveLoadStats = archive.stats();
        if (archiveLoadStats.recoveredFromBackup) {
            log::warn(
                "{} {} recovered {} from the previous known-good archive generation",
                kReleaseName,
                kReleaseVersion,
                m_fields->levelContext.storageKey()
            );
        }
        if (archiveLoadStats.quarantinedReplayCount > 0) {
            log::warn(
                "{} {} quarantined {} invalid replay(s) while loading {}",
                kReleaseName,
                kReleaseVersion,
                archiveLoadStats.quarantinedReplayCount,
                m_fields->levelContext.storageKey()
            );
        }

        recorder.setNextAttemptIdFloor(archive.maxAttemptId() + 1);
        restoreDeathsFromArchive();

        replay.setArchive(&archive);
        replay.loadLatestFromArchive();

        m_fields->sessionBestProgress = 0.0f;
        m_fields->archiveReady = true;
        m_fields->fleetNeedsRebuild = true;
        updateReplayTruthContext();
    }

    void ensureCurrentArchiveContext() {
        if (!this->m_level) return;

        auto const current = makeLevelContext(this->m_level);
        if (
            m_fields->archiveReady &&
            m_fields->archive.context().matches(current)
        ) {
            m_fields->levelContext = current;
            updateReplayTruthContext();
            return;
        }

        if (m_fields->archiveReady && m_fields->archive.isDirty()) {
            if (!m_fields->archive.save()) {
                log::warn("{} {} could not save prior level archive before context switch", kReleaseName, kReleaseVersion);
            }
        }

        m_fields->fleet.stop();
        m_fields->replay.clear();
        initializeArchive(this->m_level);
    }

    std::string settingsFingerprint() const {
        auto* mod = Mod::get();
        auto const lastColor = mod->getSettingValue<cocos2d::ccColor3B>("last-ghost-color");
        auto const bestColor = mod->getSettingValue<cocos2d::ccColor3B>("best-ghost-color");

        return fmt::format(
            "{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}",
            mod->getSettingValue<std::int64_t>("ghost-count"),
            mod->getSettingValue<std::string>("visual-profile"),
            mod->getSettingValue<std::int64_t>("older-opacity-min"),
            mod->getSettingValue<std::int64_t>("older-opacity-max"),
            mod->getSettingValue<double>("age-fade-strength"),
            mod->getSettingValue<bool>("priority-xray"),
            mod->getSettingValue<bool>("last-ghost-enabled"),
            lastColor.r, lastColor.g, lastColor.b,
            mod->getSettingValue<std::int64_t>("last-ghost-opacity"),
            mod->getSettingValue<bool>("last-ghost-trail"),
            mod->getSettingValue<bool>("best-ghost-enabled"),
            bestColor.r, bestColor.g, bestColor.b,
            mod->getSettingValue<std::int64_t>("best-ghost-opacity"),
            mod->getSettingValue<bool>("best-ghost-trail"),
            mod->getSettingValue<double>("trail-seconds"),
            mod->getSettingValue<double>("trail-width"),
            mod->getSettingValue<std::int64_t>("trail-opacity"),
            mod->getSettingValue<bool>("death-markers"),
            mod->getSettingValue<double>("death-marker-scale"),
            mod->getSettingValue<bool>("death-labels"),
            mod->getSettingValue<bool>("death-xray"),
            mod->getSettingValue<bool>("heat-strip"),
            mod->getSettingValue<std::int64_t>("heat-strip-opacity"),
            mod->getSettingValue<std::string>("default-playback-rate"),
            mod->getSettingValue<std::string>("default-camera-mode"),
            mod->getSettingValue<std::int64_t>("recorder-sample-rate"),
            mod->getSettingValue<std::int64_t>("replay-retention"),
            mod->getSettingValue<std::int64_t>("disk-budget-mb"),
            mod->getSettingValue<bool>("diagnostics")
        );
    }

    void applyEchoDashSettings(bool force = false) {
        auto* mod = Mod::get();
        auto const fingerprint = settingsFingerprint();
        if (!force && fingerprint == m_fields->settingsFingerprint) return;
        m_fields->settingsFingerprint = fingerprint;

        auto const requestedGhostCount = std::clamp<std::int64_t>(
            mod->getSettingValue<std::int64_t>("ghost-count"),
            0,
            static_cast<std::int64_t>(dash_echo::EchoGhostFleet::kMaxGhosts)
        );
        auto const profile = mod->getSettingValue<std::string>("visual-profile");
        auto const effectiveGhostCount = std::min<std::size_t>(
            static_cast<std::size_t>(requestedGhostCount),
            profileGhostCap(profile)
        );
        m_fields->fleet.setGhostLimit(effectiveGhostCount);

        dash_echo::GhostFleetVisualSettings visual;
        visual.oldestOpacity = static_cast<std::uint8_t>(std::clamp<std::int64_t>(
            mod->getSettingValue<std::int64_t>("older-opacity-min"), 0, 255
        ));
        visual.newestOlderOpacity = static_cast<std::uint8_t>(std::clamp<std::int64_t>(
            mod->getSettingValue<std::int64_t>("older-opacity-max"), 0, 255
        ));
        visual.ageFadeStrength = static_cast<float>(mod->getSettingValue<double>("age-fade-strength"));
        visual.priorityXray = mod->getSettingValue<bool>("priority-xray");

        visual.lastEnabled = mod->getSettingValue<bool>("last-ghost-enabled");
        visual.lastColor = toEchoColor(mod->getSettingValue<cocos2d::ccColor3B>("last-ghost-color"));
        visual.lastOpacity = static_cast<std::uint8_t>(std::clamp<std::int64_t>(
            mod->getSettingValue<std::int64_t>("last-ghost-opacity"), 0, 255
        ));
        visual.lastTrail = mod->getSettingValue<bool>("last-ghost-trail");

        visual.bestEnabled = mod->getSettingValue<bool>("best-ghost-enabled");
        visual.bestColor = toEchoColor(mod->getSettingValue<cocos2d::ccColor3B>("best-ghost-color"));
        visual.bestOpacity = static_cast<std::uint8_t>(std::clamp<std::int64_t>(
            mod->getSettingValue<std::int64_t>("best-ghost-opacity"), 0, 255
        ));
        visual.bestTrail = mod->getSettingValue<bool>("best-ghost-trail");

        visual.trailSeconds = static_cast<float>(mod->getSettingValue<double>("trail-seconds"));
        visual.trailWidth = static_cast<float>(mod->getSettingValue<double>("trail-width"));
        visual.trailOpacity = static_cast<std::uint8_t>(std::clamp<std::int64_t>(
            mod->getSettingValue<std::int64_t>("trail-opacity"), 0, 255
        ));
        m_fields->fleet.setVisualSettings(visual);

        auto const sampleRate = std::clamp<std::int64_t>(
            mod->getSettingValue<std::int64_t>("recorder-sample-rate"), 30, 240
        );
        m_fields->recorder.setCaptureSampleRate(static_cast<double>(sampleRate));

        auto const replayRetention = std::clamp<std::int64_t>(
            mod->getSettingValue<std::int64_t>("replay-retention"),
            256,
            static_cast<std::int64_t>(dash_echo::EchoReplayArchive::kHardMaxReplays)
        );
        auto const diskBudget = std::clamp<std::int64_t>(
            mod->getSettingValue<std::int64_t>("disk-budget-mb"),
            static_cast<std::int64_t>(dash_echo::EchoReplayArchive::kMinDiskBudgetMb),
            static_cast<std::int64_t>(dash_echo::EchoReplayArchive::kMaxDiskBudgetMb)
        );
        m_fields->archive.configure(
            static_cast<std::size_t>(replayRetention),
            static_cast<std::size_t>(diskBudget),
            static_cast<double>(sampleRate)
        );

        m_fields->deathOverlay.setEnabled(mod->getSettingValue<bool>("death-markers"));
        m_fields->deathOverlay.setDisplay(
            static_cast<float>(mod->getSettingValue<double>("death-marker-scale")),
            mod->getSettingValue<bool>("death-labels")
        );

        m_fields->heatmapOverlay.setEnabled(mod->getSettingValue<bool>("heat-strip"));
        m_fields->heatmapOverlay.setOpacity(static_cast<std::uint8_t>(std::clamp<std::int64_t>(
            mod->getSettingValue<std::int64_t>("heat-strip-opacity"), 0, 255
        )));

        m_fields->replay.setDefaultPlaybackRate(
            playbackRateFromSetting(mod->getSettingValue<std::string>("default-playback-rate"))
        );
        m_fields->replay.setDefaultCameraMode(
            cameraModeFromSetting(mod->getSettingValue<std::string>("default-camera-mode"))
        );

        m_fields->diagnosticsEnabled = mod->getSettingValue<bool>("diagnostics");
        if (m_fields->diagnosticsLabel) {
            m_fields->diagnosticsLabel->setVisible(m_fields->diagnosticsEnabled);
        }

        m_fields->fleetNeedsRebuild = true;
        updateReplayTruthContext();
    }

    void updateReplayTruthContext() {
        if (!m_fields->replayControls) return;

        auto const stats = m_fields->archive.stats();
        float gdBest = 0.0f;
        bool platformer = m_fields->levelContext.platformer;
        if (this->m_level && !platformer) {
            gdBest = static_cast<float>(this->m_level->getNormalPercent());
        }

        m_fields->replayControls->setProgressContext(
            gdBest,
            stats.bestRecordedProgress,
            m_fields->sessionBestProgress,
            platformer
        );
    }

    void ensureDiagnosticsLabel() {
        if (m_fields->diagnosticsLabel) return;
        auto* label = CCLabelBMFont::create("", "bigFont.fnt");
        if (!label) return;
        auto const winSize = CCDirector::sharedDirector()->getWinSize();
        label->setAnchorPoint({0.0f, 1.0f});
        label->setScale(0.28f);
        label->setPosition({8.0f, winSize.height - 8.0f});
        label->setVisible(m_fields->diagnosticsEnabled);
        this->addChild(label, 9998);
        m_fields->diagnosticsLabel = label;
    }

    void refreshDiagnostics() {
        if (!m_fields->diagnosticsEnabled || !m_fields->diagnosticsLabel) return;
        auto const fleet = m_fields->fleet.stats();
        auto const archive = m_fields->archive.stats();
        auto const recorder = m_fields->recorder.stats();
        m_fields->diagnosticsLabel->setString(fmt::format(
            "ECHO_DASH 1.1.3 | ghosts {}/{} pool {} | archive {} replays / {} summaries / {} frames | recovery {} quarantine {} | session {} frames @ {:.0f}Hz",
            fleet.assignedGhosts,
            fleet.configuredGhostLimit,
            fleet.allocatedGhostSlots,
            archive.replayCount,
            archive.summaryCount,
            archive.retainedFrames,
            archive.recoveredFromBackup ? "backup" : "primary",
            archive.quarantinedReplayCount,
            recorder.retainedFrames,
            recorder.captureSampleRate
        ).c_str());
    }

    void captureActiveViewportForStudio() {
        auto* layer = this->m_objectLayer;
        if (!layer) return;

        auto const position = layer->getPosition();
        auto& restore = m_fields->replayViewportRestore;
        restore.valid = true;
        restore.x = position.x;
        restore.y = position.y;
        restore.rotation = layer->getRotation();
        restore.scaleX = layer->getScaleX();
        restore.scaleY = layer->getScaleY();
    }

    void applyReplayViewport() {
        if (!m_fields->replayStudioOpen || !this->m_objectLayer) return;

        auto const winSize = CCDirector::sharedDirector()->getWinSize();
        auto const pose = m_fields->replay.cameraPose(winSize.width, winSize.height);
        if (!pose.valid) return;

        this->m_objectLayer->setPosition({pose.x, pose.y});
        this->m_objectLayer->setRotation(pose.rotation);
        this->m_objectLayer->setScaleX(pose.scaleX);
        this->m_objectLayer->setScaleY(pose.scaleY);
    }

    void restoreActiveViewportAfterStudio() {
        auto& restore = m_fields->replayViewportRestore;
        if (!restore.valid || !this->m_objectLayer) {
            restore.valid = false;
            return;
        }

        this->m_objectLayer->setPosition({restore.x, restore.y});
        this->m_objectLayer->setRotation(restore.rotation);
        this->m_objectLayer->setScaleX(restore.scaleX);
        this->m_objectLayer->setScaleY(restore.scaleY);
        restore.valid = false;
    }

    void ensureReplayControls() {
        if (m_fields->replayControls) return;

        auto* controls = dash_echo::EchoReplayControls::create(
            &m_fields->replay,
            [this](bool open) {
                m_fields->replayStudioOpen = open;
                if (open) {
                    captureActiveViewportForStudio();
                    m_fields->fleet.hide();
                    applyReplayViewport();
                    return;
                }

                restoreActiveViewportAfterStudio();
                if (
                    m_fields->recorder.hasActiveAttempt() &&
                    !m_fields->fleet.isContinuing()
                ) {
                    m_fields->fleet.track(
                        m_fields->recorder.activeElapsedSeconds(),
                        this->getCurrentPercent(),
                        !m_fields->levelContext.platformer
                    );
                }
            }
        );

        if (!controls) {
            log::warn("{} {} could not create Replay Studio controls", kReleaseName, kReleaseVersion);
            return;
        }

        controls->setID("echo-dash-replay-controls");
        this->addChild(controls, 10000);
        m_fields->replayControls = controls;
        updateReplayTruthContext();
        controls->refresh();
    }

    void closeReplayStudioForLifecycle() {
        if (m_fields->replayControls && m_fields->replayControls->isStudioOpen()) {
            m_fields->replayControls->closeStudio();
            return;
        }

        m_fields->replayStudioOpen = false;
        restoreActiveViewportAfterStudio();
        m_fields->replay.stop();
    }

    void startNewAttempt() {
        m_fields->confirmedDeath = false;
        m_fields->deferredResetRequested = false;

        if (!m_fields->captureEnabled) return;
        auto& recorder = m_fields->recorder;
        if (recorder.hasActiveAttempt()) return;

        recorder.beginAttempt();
        recorder.captureEventFrame(
            this->getCurrentPercent(),
            this->m_player1,
            this->m_player2,
            this->m_objectLayer
        );
    }

    bool finalizeActiveAttempt(dash_echo::AttemptEndReason reason) {
        auto& recorder = m_fields->recorder;
        auto& deaths = m_fields->deaths;
        auto& history = m_fields->history;
        auto& archive = m_fields->archive;
        auto& replay = m_fields->replay;

        auto const* active = recorder.activeAttempt();
        if (!active) return false;

        std::uint64_t const attemptId = active->attemptId;
        auto const priorStats = archive.stats();
        float const priorBestProgress = priorStats.bestRecordedProgress;
        std::uint64_t const priorBestId = priorStats.bestRecordedAttemptId;

        recorder.finalizeAttempt(reason);

        auto const* finalized = recorder.attemptById(attemptId);
        if (!finalized || !finalized->finalized) {
            log::warn("{} {} could not resolve finalized attempt {}", kReleaseName, kReleaseVersion, attemptId);
            return false;
        }

        bool const becomesBest =
            finalized->maxProgressPercent > priorBestProgress ||
            (finalized->maxProgressPercent == priorBestProgress && attemptId > priorBestId);
        std::uint64_t const bestIdAfter = becomesBest ? attemptId : priorBestId;
        auto const* death = deaths.deathForAttempt(attemptId);

        bool const committed = history.commitFinalizedAttempt(
            *finalized,
            death,
            priorBestProgress,
            bestIdAfter
        );
        if (!committed) {
            log::warn("{} {} rejected history commit for attempt {}", kReleaseName, kReleaseVersion, attemptId);
            return false;
        }

        auto const* entry = history.entryForAttempt(attemptId);
        if (!entry || !archive.ingest(*finalized, *entry)) {
            log::warn("{} {} could not ingest attempt {} into replay archive", kReleaseName, kReleaseVersion, attemptId);
            return false;
        }

        // Every successful finalization is persisted immediately. This is more
        // I/O than deferred batching, but it makes a completed run durable before
        // the next attempt begins and directly addresses the first-run loss.
        if (!archive.save()) {
            log::warn("{} {} could not persist archive after attempt {}", kReleaseName, kReleaseVersion, attemptId);
        }

        m_fields->sessionBestProgress = std::max(
            m_fields->sessionBestProgress,
            finalized->maxProgressPercent
        );
        recorder.setNextAttemptIdFloor(archive.maxAttemptId() + 1);
        replay.setArchive(&archive);
        replay.loadReplayFromArchive(attemptId);
        m_fields->fleetNeedsRebuild = true;
        updateReplayTruthContext();
        if (m_fields->replayControls) m_fields->replayControls->refresh();
        return true;
    }

    void ensureRenderSystems() {
        cocos2d::CCNode* renderParent = this->m_objectLayer;
        int topGhostZOrder = 0;
        if (this->m_player1 && this->m_player1->getParent()) {
            renderParent = this->m_player1->getParent();
            topGhostZOrder = this->m_player1->getZOrder() - 1;
        }

        if (!m_fields->fleet.isAttached()) {
            m_fields->fleet.attach(renderParent, topGhostZOrder);
        }
        if (!m_fields->replay.isAttached()) {
            m_fields->replay.attach(renderParent, topGhostZOrder);
        }
        if (!m_fields->deathOverlay.isAttached()) {
            m_fields->deathOverlay.attach(renderParent, topGhostZOrder);
        }
        if (!m_fields->heatmapOverlay.isAttached()) {
            m_fields->heatmapOverlay.attach(this, 9997);
        }

        bool const deathXray = Mod::get()->getSettingValue<bool>("death-xray");
        m_fields->deathOverlay.setRenderZOrder(
            deathXray ? topGhostZOrder + 3 : topGhostZOrder - 1
        );

        // Archive-backed fleet pointers must remain stable while continuation is
        // running. Settings that request a rebuild are therefore applied at the
        // next safe lifecycle boundary instead of resetting the shared engine.
        if (
            m_fields->fleetNeedsRebuild &&
            m_fields->archiveReady &&
            !m_fields->fleet.isContinuing()
        ) {
            m_fields->fleet.rebuild(m_fields->archive);
            m_fields->fleetNeedsRebuild = false;
            if (m_fields->recorder.hasActiveAttempt()) {
                m_fields->fleet.track(
                    m_fields->recorder.activeElapsedSeconds(),
                    this->getCurrentPercent(),
                    !m_fields->levelContext.platformer
                );
            }
        }
    }

    void performResetLifecycle() {
        closeReplayStudioForLifecycle();

        // The fleet owns pointers into archive replay records, so it must release
        // those pointers before finalization can ingest/mutate the archive.
        m_fields->fleet.stop();
        finalizeActiveAttempt(dash_echo::AttemptEndReason::Reset);

        PlayLayer::resetLevel();

        m_fields->captureEnabled = true;
        ensureCurrentArchiveContext();
        applyEchoDashSettings(true);
        startNewAttempt();
        m_fields->fleet.rebuild(m_fields->archive);
        m_fields->fleetNeedsRebuild = false;
        m_fields->fleet.track(
            m_fields->recorder.activeElapsedSeconds(),
            this->getCurrentPercent(),
            !m_fields->levelContext.platformer
        );
        m_fields->deathOverlay.refresh(m_fields->deaths);
        m_fields->heatmapOverlay.refresh(m_fields->deaths);
        updateReplayTruthContext();
        if (m_fields->replayControls) m_fields->replayControls->refresh();
    }

    void postUpdate(float dt) {
        float const safeDt = static_cast<float>(
            dash_echo::sanitizeDeltaSeconds(static_cast<double>(dt))
        );
        m_fields->settingsPollSeconds += safeDt;
        m_fields->diagnosticsPollSeconds += safeDt;

        if (m_fields->settingsPollSeconds >= 0.50f) {
            m_fields->settingsPollSeconds = 0.0f;
            applyEchoDashSettings(false);
        }

        if (m_fields->replayStudioOpen) {
            m_fields->replay.advance(dt);
            applyReplayViewport();
            if (m_fields->replayControls) m_fields->replayControls->refresh();
            return;
        }

        // Geometry Dash still advances its native death state. If it reaches its
        // reset point while ECHO_DASH is continuing ghosts, resetLevel() below
        // records that request and returns without mutating archive/fleet state.
        PlayLayer::postUpdate(dt);

        ensureReplayControls();
        ensureDiagnosticsLabel();
        ensureRenderSystems();

        m_fields->deathOverlay.refresh(m_fields->deaths);
        m_fields->heatmapOverlay.refresh(m_fields->deaths);
        if (m_fields->replayControls) m_fields->replayControls->refresh();

        if (m_fields->diagnosticsPollSeconds >= 0.50f) {
            m_fields->diagnosticsPollSeconds = 0.0f;
            refreshDiagnostics();
        }

        if (m_fields->fleet.isContinuing()) {
            m_fields->fleet.advanceContinuation(static_cast<double>(safeDt));
            if (
                m_fields->deferredResetRequested &&
                m_fields->fleet.continuationComplete()
            ) {
                performResetLifecycle();
            }
            return;
        }

        if (!m_fields->captureEnabled) return;

        if (!m_fields->recorder.hasActiveAttempt()) {
            log::warn("{} {} repaired missing active attempt during postUpdate", kReleaseName, kReleaseVersion);
            startNewAttempt();
        }

        m_fields->recorder.captureFrame(
            dt,
            this->getCurrentPercent(),
            this->m_player1,
            this->m_player2,
            this->m_objectLayer
        );
        m_fields->fleet.track(
            m_fields->recorder.activeElapsedSeconds(),
            this->getCurrentPercent(),
            !m_fields->levelContext.platformer
        );
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        auto& recorder = m_fields->recorder;
        auto& deaths = m_fields->deaths;

        dash_echo::DeathEvent candidate;
        bool candidateValid = false;
        bool const wasDeadBefore = player && player->m_isDead;

        if (
            !m_fields->replayStudioOpen &&
            m_fields->captureEnabled &&
            player && recorder.hasActiveAttempt()
        ) {
            if (auto const* attempt = recorder.activeAttempt()) {
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

        if (candidateValid && player && !wasDeadBefore && player->m_isDead) {
            recorder.captureEventFrame(
                candidate.progressPercent,
                this->m_player1,
                this->m_player2,
                this->m_objectLayer
            );
            if (deaths.recordDeath(candidate)) {
                m_fields->deathOverlay.refresh(deaths);
                m_fields->heatmapOverlay.refresh(deaths);
            }

            if (!m_fields->confirmedDeath) {
                m_fields->confirmedDeath = true;
                m_fields->captureEnabled = false;

                // All selected historical ghosts enter the same continuation
                // phase from the exact shared death anchor. Role never affects
                // timing; it only affects color/trail/opacity presentation.
                m_fields->fleet.beginContinuation(
                    recorder.activeElapsedSeconds(),
                    candidate.progressPercent,
                    !m_fields->levelContext.platformer
                );
            }
        }
    }

    void resetLevel() {
        if (m_fields->fleet.isContinuing()) {
            m_fields->deferredResetRequested = true;
            return;
        }
        performResetLifecycle();
    }

    void levelComplete() {
        closeReplayStudioForLifecycle();
        m_fields->captureEnabled = false;
        m_fields->fleet.stop();
        finalizeActiveAttempt(dash_echo::AttemptEndReason::Completed);
        PlayLayer::levelComplete();
    }

    void onExit() {
        closeReplayStudioForLifecycle();
        m_fields->captureEnabled = false;
        m_fields->fleet.stop();
        finalizeActiveAttempt(dash_echo::AttemptEndReason::LayerExit);

        if (m_fields->archive.isDirty() && !m_fields->archive.save()) {
            log::warn("{} {} could not persist dirty archive during PlayLayer exit", kReleaseName, kReleaseVersion);
        }

        auto const recorderStats = m_fields->recorder.stats();
        auto const fleetStats = m_fields->fleet.stats();
        auto const deathStats = m_fields->deaths.stats();
        auto const archiveStats = m_fields->archive.stats();
        log::debug(
            "{} {} session closed: {} attempts started, {} finalized, archive {} summaries / {} replays / {} frames, recovery {}, quarantine {}, best echo {} {:.2f}%, session {:.2f}%, recorder {} frames @ {:.0f}Hz, ghosts {}/{} pool {}, {} death clusters",
            kReleaseName,
            kReleaseVersion,
            recorderStats.attemptsStarted,
            recorderStats.attemptsFinalized,
            archiveStats.summaryCount,
            archiveStats.replayCount,
            archiveStats.retainedFrames,
            archiveStats.recoveredFromBackup ? "backup" : "primary",
            archiveStats.quarantinedReplayCount,
            archiveStats.bestRecordedAttemptId,
            archiveStats.bestRecordedProgress,
            m_fields->sessionBestProgress,
            recorderStats.retainedFrames,
            recorderStats.captureSampleRate,
            fleetStats.assignedGhosts,
            fleetStats.configuredGhostLimit,
            fleetStats.allocatedGhostSlots,
            deathStats.clusterCount
        );

        m_fields->replay.detach();
        m_fields->fleet.detach();
        m_fields->deathOverlay.detach();
        m_fields->heatmapOverlay.detach();
        PlayLayer::onExit();
    }
};

class $modify(EchoDashPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        // Pause menu remains the only ordinary entrypoint. Nothing from
        // ECHO_DASH floats over live gameplay.
        auto* menu = this->getChildByID("left-button-menu");
        if (!menu) return;

        auto* sprite = ButtonSprite::create("ECHO");
        if (!sprite) return;
        sprite->setScale(0.62f);

        auto* button = CCMenuItemSpriteExtra::create(
            sprite,
            this,
            menu_selector(EchoDashPauseLayer::onEchoDash)
        );
        if (!button) return;

        button->setID("echo-dash-pause-button");
        menu->addChild(button);
        menu->updateLayout();
    }

    void onEchoDash(CCObject* sender) {
        auto* play = PlayLayer::get();
        if (!play) return;

        auto* controls = typeinfo_cast<dash_echo::EchoReplayControls*>(
            play->getChildByID("echo-dash-replay-controls")
        );
        if (!controls || !controls->hasReplay()) {
            FLAlertLayer::create(
                "ECHO_DASH",
                "No saved replay yet. Finish, die, reset, or exit an attempt first.",
                "OK"
            )->show();
            return;
        }

        // Open Studio first so its PlayLayer callback freezes normal capture,
        // then resume/remove the vanilla pause layer underneath it.
        if (controls->openStudio()) {
            this->onResume(sender);
        }
    }
};