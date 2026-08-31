#pragma once

#include "EchoReplaySession.hpp"

#include <Geode/Geode.hpp>

#include <functional>

class Slider;

namespace dash_echo {

class EchoReplayControls final : public cocos2d::CCNode {
public:
    using StudioModeCallback = std::function<void(bool)>;

    static EchoReplayControls* create(
        EchoReplaySession* session,
        StudioModeCallback onStudioModeChanged
    );

    bool init(
        EchoReplaySession* session,
        StudioModeCallback onStudioModeChanged
    );

    [[nodiscard]] bool openStudio();
    void closeStudio();
    void refresh();
    void setProgressContext(
        float gdLevelBestPercent,
        float bestRecordedPercent,
        float sessionBestPercent,
        bool platformer
    );

    [[nodiscard]] bool isStudioOpen() const;
    [[nodiscard]] bool hasReplay() const;

private:
    void onClose(cocos2d::CCObject* sender);
    void onPreviousAttempt(cocos2d::CCObject* sender);
    void onNextAttempt(cocos2d::CCObject* sender);
    void onPlayPause(cocos2d::CCObject* sender);
    void onRestart(cocos2d::CCObject* sender);
    void onPreviousFrame(cocos2d::CCObject* sender);
    void onNextFrame(cocos2d::CCObject* sender);
    void onSpeed(cocos2d::CCObject* sender);
    void onCamera(cocos2d::CCObject* sender);
    void onSettings(cocos2d::CCObject* sender);
    void onScrub(cocos2d::CCObject* sender);

    void setStudioOpen(bool open);

    EchoReplaySession* m_session = nullptr;
    StudioModeCallback m_onStudioModeChanged;
    bool m_studioOpen = false;
    bool m_platformer = false;
    float m_gdLevelBestPercent = 0.0f;
    float m_bestRecordedPercent = 0.0f;
    float m_sessionBestPercent = 0.0f;

    cocos2d::CCNode* m_panel = nullptr;
    Slider* m_slider = nullptr;

    cocos2d::CCLabelBMFont* m_truthLabel = nullptr;
    cocos2d::CCLabelBMFont* m_attemptLabel = nullptr;
    cocos2d::CCLabelBMFont* m_timeLabel = nullptr;
    cocos2d::CCLabelBMFont* m_progressLabel = nullptr;
    cocos2d::CCLabelBMFont* m_playLabel = nullptr;
    cocos2d::CCLabelBMFont* m_speedLabel = nullptr;
    cocos2d::CCLabelBMFont* m_cameraLabel = nullptr;
};

} // namespace dash_echo
