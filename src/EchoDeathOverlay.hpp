#pragma once

#include "EchoDeathAnalytics.hpp"

#include <cstddef>
#include <cstdint>

namespace cocos2d {
class CCNode;
class CCDrawNode;
}

namespace dash_echo {

class EchoDeathOverlay final {
public:
    static constexpr std::size_t kMaxRenderedClusters = 24;

    bool attach(cocos2d::CCNode* parent, int zOrder);
    void detach();
    void setEnabled(bool enabled);
    void refresh(EchoDeathAnalytics const& analytics);
    void clearVisuals();

    [[nodiscard]] bool isAttached() const;
    [[nodiscard]] bool isEnabled() const;
    [[nodiscard]] std::uint64_t renderedRevision() const;

private:
    void rebuild(EchoDeathAnalytics const& analytics);

    cocos2d::CCNode* m_root = nullptr;
    cocos2d::CCDrawNode* m_drawNode = nullptr;
    cocos2d::CCNode* m_labelLayer = nullptr;
    bool m_enabled = true;
    std::uint64_t m_renderedRevision = 0;
};

} // namespace dash_echo
