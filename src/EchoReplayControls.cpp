#include "EchoReplayControls.hpp"

#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/Slider.hpp>
#include <Geode/binding/SliderThumb.hpp>
#include <Geode/ui/GeodeUI.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <utility>

using namespace geode::prelude;

namespace dash_echo {

EchoReplayControls* EchoReplayControls::create(
    EchoReplaySession* session,
    StudioModeCallback onStudioModeChanged
) {
    auto* result = new EchoReplayControls();
    if (result && result->init(session, std::move(onStudioModeChanged))) {
        result->autorelease();
        return result;
    }
    delete result;
    return nullptr;
}

bool EchoReplayControls::init(
    EchoReplaySession* session,
    StudioModeCallback onStudioModeChanged
) {
    if (!CCNode::init() || !session) return false;

    m_session = session;
    m_onStudioModeChanged = std::move(onStudioModeChanged);

    auto const winSize = CCDirector::sharedDirector()->getWinSize();
    this->setContentSize(winSize);
    this->setAnchorPoint({0.0f, 0.0f});
    this->setPosition({0.0f, 0.0f});

    // v1.1.1 intentionally renders nothing during ordinary gameplay. Replay
    // Studio is opened from PauseLayer, so the live level stays distraction-free.
    auto* panel = CCLayerColor::create(
        cocos2d::ccColor4B {8, 10, 18, 226},
        winSize.width,
        142.0f
    );
    panel->setPosition({0.0f, 0.0f});
    panel->setVisible(false);
    this->addChild(panel, 2);
    m_panel = panel;

    auto* title = CCLabelBMFont::create("ECHO_DASH  |  REPLAY STUDIO", "goldFont.fnt");
    if (title) {
        title->setScale(0.43f);
        title->setAnchorPoint({0.0f, 0.5f});
        title->setPosition({14.0f, 126.0f});
        panel->addChild(title);
    }

    m_truthLabel = CCLabelBMFont::create("GD PB -- | BEST ECHO -- | SESSION --", "bigFont.fnt");
    if (m_truthLabel) {
        m_truthLabel->setScale(0.29f);
        m_truthLabel->setPosition({winSize.width * 0.50f, 124.0f});
        panel->addChild(m_truthLabel);
    }

    m_attemptLabel = CCLabelBMFont::create("Attempt", "goldFont.fnt");
    if (m_attemptLabel) {
        m_attemptLabel->setScale(0.38f);
        m_attemptLabel->setAnchorPoint({0.0f, 0.5f});
        m_attemptLabel->setPosition({14.0f, 100.0f});
        panel->addChild(m_attemptLabel);
    }

    m_timeLabel = CCLabelBMFont::create("0.00 / 0.00s", "bigFont.fnt");
    if (m_timeLabel) {
        m_timeLabel->setScale(0.31f);
        m_timeLabel->setPosition({winSize.width * 0.50f, 100.0f});
        panel->addChild(m_timeLabel);
    }

    m_progressLabel = CCLabelBMFont::create("0.00%", "bigFont.fnt");
    if (m_progressLabel) {
        m_progressLabel->setScale(0.31f);
        m_progressLabel->setAnchorPoint({1.0f, 0.5f});
        m_progressLabel->setPosition({winSize.width - 14.0f, 100.0f});
        panel->addChild(m_progressLabel);
    }

    m_slider = Slider::create(this, menu_selector(EchoReplayControls::onScrub), 0.86f);
    if (m_slider) {
        m_slider->setValue(0.0f);
        m_slider->setPosition({winSize.width * 0.50f, 76.0f});
        panel->addChild(m_slider);
    }

    auto* controlsMenu = CCMenu::create();
    if (!controlsMenu) return false;
    controlsMenu->setPosition({0.0f, 0.0f});
    panel->addChild(controlsMenu);

    auto makeButton = [this, controlsMenu](
        char const* text,
        cocos2d::CCPoint position,
        float scale,
        cocos2d::SEL_MenuHandler selector,
        cocos2d::CCLabelBMFont** labelOut
    ) {
        auto* label = CCLabelBMFont::create(text, "goldFont.fnt");
        if (!label) return;
        label->setScale(scale);
        auto* item = CCMenuItemSpriteExtra::create(label, this, selector);
        if (!item) return;
        item->setPosition(position);
        controlsMenu->addChild(item);
        if (labelOut) *labelOut = label;
    };

    float const centerX = winSize.width * 0.50f;

    // Row 1: attempt navigation + core transport.
    makeButton("ATT <", {centerX - 190.0f, 49.0f}, 0.40f,
        menu_selector(EchoReplayControls::onPreviousAttempt), nullptr);
    makeButton("ATT >", {centerX - 145.0f, 49.0f}, 0.40f,
        menu_selector(EchoReplayControls::onNextAttempt), nullptr);
    makeButton("< FRAME", {centerX - 92.0f, 49.0f}, 0.36f,
        menu_selector(EchoReplayControls::onPreviousFrame), nullptr);
    makeButton("PLAY", {centerX - 30.0f, 49.0f}, 0.43f,
        menu_selector(EchoReplayControls::onPlayPause), &m_playLabel);
    makeButton("FRAME >", {centerX + 31.0f, 49.0f}, 0.36f,
        menu_selector(EchoReplayControls::onNextFrame), nullptr);
    makeButton("RESTART", {centerX + 96.0f, 49.0f}, 0.37f,
        menu_selector(EchoReplayControls::onRestart), nullptr);

    // Row 2: mode controls and exit actions.
    makeButton("SPEED 1.00x", {centerX - 126.0f, 20.0f}, 0.35f,
        menu_selector(EchoReplayControls::onSpeed), &m_speedLabel);
    makeButton("CAM RECORDED", {centerX - 24.0f, 20.0f}, 0.35f,
        menu_selector(EchoReplayControls::onCamera), &m_cameraLabel);
    makeButton("SETTINGS", {centerX + 81.0f, 20.0f}, 0.37f,
        menu_selector(EchoReplayControls::onSettings), nullptr);
    makeButton("CLOSE", {centerX + 157.0f, 20.0f}, 0.40f,
        menu_selector(EchoReplayControls::onClose), nullptr);

    refresh();
    return true;
}

bool EchoReplayControls::openStudio() {
    if (!m_session || !m_session->isLoaded()) return false;
    setStudioOpen(true);
    return m_studioOpen;
}

void EchoReplayControls::closeStudio() {
    setStudioOpen(false);
}

void EchoReplayControls::refresh() {
    bool const loaded = m_session && m_session->isLoaded();
    if (m_panel) m_panel->setVisible(loaded && m_studioOpen);
    if (!loaded || !m_studioOpen) return;

    auto const& timeline = m_session->timeline();
    auto const* clip = timeline.clip();
    if (!clip) return;

    double const elapsed = std::clamp(
        timeline.cursorSeconds() - clip->startTimeSeconds,
        0.0,
        clip->durationSeconds
    );

    if (m_truthLabel) {
        if (m_platformer) {
            m_truthLabel->setString(
                fmt::format("GD PB -- | BEST ECHO {:.2f}% | SESSION {:.2f}%",
                    m_bestRecordedPercent, m_sessionBestPercent).c_str()
            );
        } else {
            m_truthLabel->setString(
                fmt::format("GD PB {:.2f}% | BEST ECHO {:.2f}% | SESSION {:.2f}%",
                    m_gdLevelBestPercent, m_bestRecordedPercent, m_sessionBestPercent).c_str()
            );
        }
        m_truthLabel->limitLabelWidth(260.0f, 0.29f, 0.22f);
    }
    if (m_attemptLabel) {
        m_attemptLabel->setString(fmt::format("ATTEMPT #{}", timeline.sourceAttemptId()).c_str());
    }
    if (m_timeLabel) {
        m_timeLabel->setString(fmt::format("TIME {:.2f} / {:.2f}s", elapsed, clip->durationSeconds).c_str());
    }
    if (m_progressLabel) {
        m_progressLabel->setString(fmt::format("PROGRESS {:.2f}%", timeline.progressPercentAtCursor()).c_str());
    }
    if (m_playLabel) m_playLabel->setString(timeline.isPlaying() ? "PAUSE" : "PLAY");
    if (m_speedLabel) {
        m_speedLabel->setString(fmt::format("SPEED {:.2f}x", timeline.playbackRate()).c_str());
        m_speedLabel->limitLabelWidth(82.0f, 0.35f, 0.27f);
    }
    if (m_cameraLabel) {
        m_cameraLabel->setString(fmt::format("CAM {}", m_session->cameraModeName()).c_str());
        m_cameraLabel->limitLabelWidth(92.0f, 0.35f, 0.25f);
    }
    if (m_slider) m_slider->setValue(timeline.normalizedCursor());
}

void EchoReplayControls::setProgressContext(
    float gdLevelBestPercent,
    float bestRecordedPercent,
    float sessionBestPercent,
    bool platformer
) {
    m_gdLevelBestPercent = std::clamp(gdLevelBestPercent, 0.0f, 100.0f);
    m_bestRecordedPercent = std::clamp(bestRecordedPercent, 0.0f, 100.0f);
    m_sessionBestPercent = std::clamp(sessionBestPercent, 0.0f, 100.0f);
    m_platformer = platformer;
    refresh();
}

bool EchoReplayControls::isStudioOpen() const {
    return m_studioOpen;
}

bool EchoReplayControls::hasReplay() const {
    return m_session && m_session->isLoaded();
}

void EchoReplayControls::onClose(CCObject*) {
    setStudioOpen(false);
}

void EchoReplayControls::onPreviousAttempt(CCObject*) {
    if (!m_session || !m_session->selectPreviousArchivedReplay()) return;
    m_session->restart();
    refresh();
}

void EchoReplayControls::onNextAttempt(CCObject*) {
    if (!m_session || !m_session->selectNextArchivedReplay()) return;
    m_session->restart();
    refresh();
}

void EchoReplayControls::onPlayPause(CCObject*) {
    if (!m_session || !m_session->isLoaded()) return;
    m_session->togglePlayback();
    refresh();
}

void EchoReplayControls::onRestart(CCObject*) {
    if (!m_session || !m_session->isLoaded()) return;
    m_session->restart();
    refresh();
}

void EchoReplayControls::onPreviousFrame(CCObject*) {
    if (!m_session || !m_session->isLoaded()) return;
    m_session->stepPreviousFrame();
    refresh();
}

void EchoReplayControls::onNextFrame(CCObject*) {
    if (!m_session || !m_session->isLoaded()) return;
    m_session->stepNextFrame();
    refresh();
}

void EchoReplayControls::onSpeed(CCObject*) {
    if (!m_session || !m_session->isLoaded()) return;
    m_session->cyclePlaybackRate();
    refresh();
}

void EchoReplayControls::onCamera(CCObject*) {
    if (!m_session || !m_session->isLoaded()) return;
    m_session->cycleCameraMode();
    refresh();
}

void EchoReplayControls::onSettings(CCObject*) {
    geode::openSettingsPopup(Mod::get(), false);
}

void EchoReplayControls::onScrub(CCObject* sender) {
    if (!m_session || !m_session->isLoaded() || !sender) return;
    auto* thumb = static_cast<SliderThumb*>(sender);
    m_session->seekNormalized(thumb->getValue());
    refresh();
}

void EchoReplayControls::setStudioOpen(bool open) {
    if (!m_session) return;

    if (open) {
        if (!m_session->isLoaded() || m_studioOpen) return;
        m_session->restart();
        m_studioOpen = true;
    } else {
        if (!m_studioOpen) {
            refresh();
            return;
        }
        m_session->stop();
        m_studioOpen = false;
    }

    if (m_onStudioModeChanged) m_onStudioModeChanged(m_studioOpen);
    refresh();
}

} // namespace dash_echo
