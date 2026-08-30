#include "EchoHeatmapOverlay.hpp"

#include <Geode/Geode.hpp>

#include <algorithm>

using namespace geode::prelude;

namespace dash_echo {

bool EchoHeatmapOverlay::attach(cocos2d::CCNode* parent, int zOrder) {
    if (isAttached()) return true;
    if (!parent) return false;

    auto* root = cocos2d::CCNode::create();
    auto* draw = cocos2d::CCDrawNode::create();
    if (!root || !draw) return false;

    root->addChild(draw);
    root->setVisible(m_enabled);
    parent->addChild(root, zOrder);
    m_root = root;
    m_drawNode = draw;
    m_renderedRevision = 0;
    return true;
}

void EchoHeatmapOverlay::detach() {
    if (m_root) m_root->removeFromParentAndCleanup(true);
    m_root = nullptr;
    m_drawNode = nullptr;
    m_renderedRevision = 0;
}

void EchoHeatmapOverlay::setEnabled(bool enabled) {
    m_enabled = enabled;
    if (m_root) m_root->setVisible(enabled);
}

void EchoHeatmapOverlay::setOpacity(std::uint8_t opacity) {
    if (m_opacity == opacity) return;
    m_opacity = opacity;
    m_renderedRevision = 0;
}

void EchoHeatmapOverlay::refresh(EchoDeathAnalytics const& analytics) {
    if (!isAttached() || !m_enabled) return;
    if (m_renderedRevision == analytics.revision()) return;
    rebuild(analytics);
    m_renderedRevision = analytics.revision();
}

void EchoHeatmapOverlay::clearVisuals() {
    if (m_drawNode) m_drawNode->clear();
    m_renderedRevision = 0;
}

bool EchoHeatmapOverlay::isAttached() const {
    return m_root != nullptr && m_drawNode != nullptr;
}

bool EchoHeatmapOverlay::isEnabled() const {
    return m_enabled;
}

void EchoHeatmapOverlay::rebuild(EchoDeathAnalytics const& analytics) {
    if (!m_drawNode) return;
    m_drawNode->clear();

    auto const win = cocos2d::CCDirector::sharedDirector()->getWinSize();
    float const left = win.width * 0.15f;
    float const right = win.width * 0.85f;
    float const y = win.height - 15.0f;
    float const width = right - left;
    float const bucketWidth = width / static_cast<float>(EchoDeathAnalytics::kHeatmapBucketCount);
    float const opacity = static_cast<float>(m_opacity) / 255.0f;

    auto const& buckets = analytics.heatmap();
    for (std::size_t i = 0; i < buckets.size(); ++i) {
        float const intensity = std::clamp(buckets[i].normalizedIntensity, 0.0f, 1.0f);
        if (intensity <= 0.0f) continue;

        // Cool yellow at low density -> red at the hottest bucket.
        cocos2d::ccColor4F const color {
            1.0f,
            0.82f - intensity * 0.70f,
            0.08f,
            opacity * (0.28f + intensity * 0.72f)
        };

        float const x0 = left + static_cast<float>(i) * bucketWidth;
        float const x1 = x0 + bucketWidth + 0.5f;
        m_drawNode->drawSegment({x0, y}, {x1, y}, 2.2f, color);
    }

    cocos2d::ccColor4F const baseline {1.0f, 1.0f, 1.0f, opacity * 0.18f};
    m_drawNode->drawSegment({left, y}, {right, y}, 0.6f, baseline);
}

} // namespace dash_echo
