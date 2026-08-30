#pragma once

#include "EchoReplaySession.hpp"

#include <Geode/Geode.hpp>

#include <functional>

class CCMenuItemSpriteExtra;
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

    void refresh();
    void closeStudio();

    [[nodiscard]] bool isStudioOpen() const;

private:
    void onOpen(cocos2d::CCObject* sender);
    void onClose(cocos2d::CCObject* sender);
    void onPlayPause(cocos2d::CCObject* sender);
    void onRestart(cocos2d::CCObject* sender);
    void onPreviousFrame(cocos2d::CCObject* sender);
    void onNextFrame(cocos2d::CCObject* sender);
    void onSpeed(cocos2d::CCObject* sender);
    void onScrub(cocos2d::CCObject* sender);

    void setStudioOpen(bool open);

    EchoReplaySession* m_session = nullptr;
    StudioModeCallback m_onStudioModeChanged;
    bool m_studioOpen = false;

    CCMenuItemSpriteExtra* m_launcher = nullptr;
    cocos2d::CCNode* m_panel = nullptr;
    Slider* m_slider = nullptr;

    cocos2d::CCLabelBMFont* m_attemptLabel = nullptr;
    cocos2d::CCLabelBMFont* m_timeLabel = nullptr;
    cocos2d::CCLabelBMFont* m_progressLabel = nullptr;
    cocos2d::CCLabelBMFont* m_playLabel = nullptr;
    cocos2d::CCLabelBMFont* m_speedLabel = nullptr;
};

} // namespace dash_echo
