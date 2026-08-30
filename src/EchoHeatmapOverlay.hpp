#pragma once

#include "EchoDeathAnalytics.hpp"

#include <cstdint>

namespace cocos2d {
class CCNode;
class CCDrawNode;
}

namespace dash_echo {

class EchoHeatmapOverlay final {
public:
    bool attach(cocos2d::CCNode* parent, int zOrder);
    void detach();

    void setEnabled(bool enabled);
    void setOpacity(std::uint8_t opacity);
    void refresh(EchoDeathAnalytics const& analytics);
    void clearVisuals();

    [[nodiscard]] bool isAttached() const;
    [[nodiscard]] bool isEnabled() const;

private:
    void rebuild(EchoDeathAnalytics const& analytics);

    cocos2d::CCNode* m_root = nullptr;
    cocos2d::CCDrawNode* m_drawNode = nullptr;
    bool m_enabled = true;
    std::uint8_t m_opacity = 170;
    std::uint64_t m_renderedRevision = 0;
};

} // namespace dash_echo
