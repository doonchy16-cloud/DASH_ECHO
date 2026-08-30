#include "EchoReplayControls.hpp"

#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/Slider.hpp>
#include <Geode/binding/SliderThumb.hpp>

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
    this->setAnchorPoint({ 0.0f, 0.0f });
    this->setPosition({ 0.0f, 0.0f });

    auto* launcherMenu = CCMenu::create();
    launcherMenu->setPosition({ 0.0f, 0.0f });
    this->addChild(launcherMenu, 2);

    auto* launcherLabel = CCLabelBMFont::create("ECHO", "goldFont.fnt");
    launcherLabel->setScale(0.65f);
    m_launcher = CCMenuItemSpriteExtra::create(
        launcherLabel,
        this,
        menu_selector(EchoReplayControls::onOpen)
    );
    m_launcher->setPosition({ winSize.width - 42.0f, 33.0f });
    launcherMenu->addChild(m_launcher);

    auto* panel = CCLayerColor::create(
        cocos2d::ccColor4B { 0, 0, 0, 170 },
        winSize.width,
        86.0f
    );
    panel->setPosition({ 0.0f, 0.0f });
    panel->setVisible(false);
    this->addChild(panel, 1);
    m_panel = panel;

    m_attemptLabel = CCLabelBMFont::create("Attempt", "goldFont.fnt");
    m_attemptLabel->setScale(0.43f);
    m_attemptLabel->setPosition({ 76.0f, 69.0f });
    panel->addChild(m_attemptLabel);

    m_timeLabel = CCLabelBMFont::create("0.00 / 0.00s", "bigFont.fnt");
    m_timeLabel->setScale(0.34f);
    m_timeLabel->setPosition({ winSize.width * 0.50f, 69.0f });
    panel->addChild(m_timeLabel);

    m_progressLabel = CCLabelBMFont::create("0.00%", "bigFont.fnt");
    m_progressLabel->setScale(0.34f);
    m_progressLabel->setPosition({ winSize.width - 70.0f, 69.0f });
    panel->addChild(m_progressLabel);

    m_slider = Slider::create(
        this,
        menu_selector(EchoReplayControls::onScrub),
        0.80f
    );
    m_slider->setValue(0.0f);
    m_slider->setPosition({ winSize.width * 0.50f, 45.0f });
    panel->addChild(m_slider);

    auto* controlsMenu = CCMenu::create();
    controlsMenu->setPosition({ 0.0f, 0.0f });
    panel->addChild(controlsMenu);

    auto makeButton = [this, controlsMenu](
        char const* text,
        cocos2d::CCPoint position,
        cocos2d::SEL_MenuHandler selector,
        cocos2d::CCLabelBMFont** labelOut
    ) {
        auto* label = CCLabelBMFont::create(text, "goldFont.fnt");
        label->setScale(0.48f);
        auto* item = CCMenuItemSpriteExtra::create(label, this, selector);
        item->setPosition(position);
        controlsMenu->addChild(item);
        if (labelOut) *labelOut = label;
    };

    float const centerX = winSize.width * 0.50f;
    makeButton(
        "PLAY",
        { centerX - 125.0f, 17.0f },
        menu_selector(EchoReplayControls::onPlayPause),
        &m_playLabel
    );
    makeButton(
        "RESTART",
        { centerX - 64.0f, 17.0f },
        menu_selector(EchoReplayControls::onRestart),
        nullptr
    );
    makeButton(
        "<",
        { centerX + 2.0f, 17.0f },
        menu_selector(EchoReplayControls::onPreviousFrame),
        nullptr
    );
    makeButton(
        ">",
        { centerX + 38.0f, 17.0f },
        menu_selector(EchoReplayControls::onNextFrame),
        nullptr
    );
    makeButton(
        "1.00x",
        { centerX + 94.0f, 17.0f },
        menu_selector(EchoReplayControls::onSpeed),
        &m_speedLabel
    );
    makeButton(
        "X",
        { winSize.width - 24.0f, 17.0f },
        menu_selector(EchoReplayControls::onClose),
        nullptr
    );

    refresh();
    return true;
}

void EchoReplayControls::refresh() {
    bool const loaded = m_session && m_session->isLoaded();

    if (m_launcher) {
        m_launcher->setVisible(loaded && !m_studioOpen);
    }
    if (m_panel) {
        m_panel->setVisible(loaded && m_studioOpen);
    }

    if (!loaded || !m_studioOpen) return;

    auto const& timeline = m_session->timeline();
    auto const* clip = timeline.clip();
    if (!clip) return;

    double const elapsed = std::clamp(
        timeline.cursorSeconds() - clip->startTimeSeconds,
        0.0,
        clip->durationSeconds
    );

    if (m_attemptLabel) {
        m_attemptLabel->setString(
            fmt::format("Attempt #{}", timeline.sourceAttemptId()).c_str()
        );
    }
    if (m_timeLabel) {
        m_timeLabel->setString(
            fmt::format("{:.2f} / {:.2f}s", elapsed, clip->durationSeconds).c_str()
        );
    }
    if (m_progressLabel) {
        m_progressLabel->setString(
            fmt::format("{:.2f}%", timeline.progressPercentAtCursor()).c_str()
        );
    }
    if (m_playLabel) {
        m_playLabel->setString(timeline.isPlaying() ? "PAUSE" : "PLAY");
    }
    if (m_speedLabel) {
        m_speedLabel->setString(
            fmt::format("{:.2f}x", timeline.playbackRate()).c_str()
        );
    }
    if (m_slider) {
        m_slider->setValue(timeline.normalizedCursor());
    }
}

void EchoReplayControls::closeStudio() {
    setStudioOpen(false);
}

bool EchoReplayControls::isStudioOpen() const {
    return m_studioOpen;
}

void EchoReplayControls::onOpen(CCObject*) {
    setStudioOpen(true);
}

void EchoReplayControls::onClose(CCObject*) {
    setStudioOpen(false);
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

void EchoReplayControls::onScrub(CCObject* sender) {
    if (!m_session || !m_session->isLoaded() || !sender) return;

    auto* thumb = static_cast<SliderThumb*>(sender);
    m_session->seekNormalized(thumb->getValue());
    refresh();
}

void EchoReplayControls::setStudioOpen(bool open) {
    if (!m_session) return;

    if (open) {
        if (!m_session->isLoaded()) return;
        if (m_studioOpen) return;

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

    if (m_onStudioModeChanged) {
        m_onStudioModeChanged(m_studioOpen);
    }
    refresh();
}

} // namespace dash_echo
