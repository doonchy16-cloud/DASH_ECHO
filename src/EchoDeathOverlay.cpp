#include "EchoDeathOverlay.hpp"

#include <Geode/Geode.hpp>
#include <fmt/format.h>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace geode::prelude;

namespace dash_echo {

bool EchoDeathOverlay::attach(cocos2d::CCNode* parent, int zOrder) {
    if (isAttached()) return true;
    if (!parent) return false;

    auto* root = cocos2d::CCNode::create();
    auto* drawNode = cocos2d::CCDrawNode::create();
    auto* labelLayer = cocos2d::CCNode::create();
    if (!root || !drawNode || !labelLayer) return false;

    root->addChild(drawNode);
    root->addChild(labelLayer);
    root->setVisible(m_enabled);
    parent->addChild(root, zOrder);

    m_root = root;
    m_drawNode = drawNode;
    m_labelLayer = labelLayer;
    m_zOrder = zOrder;
    m_renderedRevision = 0;
    return true;
}

void EchoDeathOverlay::detach() {
    if (m_root) m_root->removeFromParentAndCleanup(true);
    m_root = nullptr;
    m_drawNode = nullptr;
    m_labelLayer = nullptr;
    m_renderedRevision = 0;
}

void EchoDeathOverlay::setEnabled(bool enabled) {
    m_enabled = enabled;
    if (m_root) m_root->setVisible(enabled);
}

void EchoDeathOverlay::setDisplay(float markerScale, bool labelsEnabled) {
    float const bounded = std::clamp(markerScale, 0.5f, 3.0f);
    if (m_markerScale != bounded || m_labelsEnabled != labelsEnabled) {
        m_markerScale = bounded;
        m_labelsEnabled = labelsEnabled;
        m_renderedRevision = 0;
    }
}

void EchoDeathOverlay::setRenderZOrder(int zOrder) {
    m_zOrder = zOrder;
    if (m_root) m_root->setZOrder(zOrder);
}

void EchoDeathOverlay::refresh(EchoDeathAnalytics const& analytics) {
    if (!isAttached() || !m_enabled) return;
    if (m_renderedRevision == analytics.revision()) return;
    rebuild(analytics);
    m_renderedRevision = analytics.revision();
}

void EchoDeathOverlay::clearVisuals() {
    if (m_drawNode) m_drawNode->clear();
    if (m_labelLayer) m_labelLayer->removeAllChildrenWithCleanup(true);
    m_renderedRevision = 0;
}

bool EchoDeathOverlay::isAttached() const {
    return m_root != nullptr && m_drawNode != nullptr && m_labelLayer != nullptr;
}

bool EchoDeathOverlay::isEnabled() const {
    return m_enabled;
}

std::uint64_t EchoDeathOverlay::renderedRevision() const {
    return m_renderedRevision;
}

void EchoDeathOverlay::rebuild(EchoDeathAnalytics const& analytics) {
    if (!m_drawNode || !m_labelLayer) return;

    m_drawNode->clear();
    m_labelLayer->removeAllChildrenWithCleanup(true);

    auto const& clusters = analytics.clusters();
    if (clusters.empty()) return;

    std::vector<DeathCluster const*> selected;
    selected.reserve(std::min(clusters.size(), kMaxRenderedClusters));
    for (auto const& cluster : clusters) selected.push_back(&cluster);

    std::sort(
        selected.begin(), selected.end(),
        [](DeathCluster const* left, DeathCluster const* right) {
            if (left->deathCount != right->deathCount) return left->deathCount > right->deathCount;
            if (left->lastAttemptId != right->lastAttemptId) return left->lastAttemptId > right->lastAttemptId;
            return left->clusterId < right->clusterId;
        }
    );
    if (selected.size() > kMaxRenderedClusters) selected.resize(kMaxRenderedClusters);

    std::sort(
        selected.begin(), selected.end(),
        [](DeathCluster const* left, DeathCluster const* right) {
            if (left->deathCount != right->deathCount) return left->deathCount < right->deathCount;
            return left->clusterId < right->clusterId;
        }
    );

    auto const stats = analytics.stats();
    float const hottest = static_cast<float>(std::max<std::size_t>(stats.hottestClusterDeaths, 1));

    for (auto const* cluster : selected) {
        if (!cluster || cluster->deathCount == 0) continue;

        float const intensity = std::clamp(
            static_cast<float>(cluster->deathCount) / hottest,
            0.0f, 1.0f
        );
        float const radius = std::clamp(
            (8.0f + std::sqrt(static_cast<float>(cluster->deathCount)) * 3.2f) * m_markerScale,
            7.0f, 32.0f
        );
        cocos2d::CCPoint const position {cluster->centroidX, cluster->centroidY};

        cocos2d::ccColor4F const halo {
            1.0f, 0.18f + (1.0f - intensity) * 0.15f, 0.02f,
            0.30f + intensity * 0.35f
        };
        cocos2d::ccColor4F const core {
            1.0f, 0.72f - intensity * 0.42f, 0.08f,
            0.62f + intensity * 0.30f
        };
        cocos2d::ccColor4F const cross {1.0f, 0.92f, 0.72f, 0.86f};

        m_drawNode->drawDot(position, radius, halo);
        m_drawNode->drawDot(position, radius * 0.48f, core);
        float const arm = radius * 1.30f;
        m_drawNode->drawSegment(
            {position.x - arm, position.y},
            {position.x + arm, position.y},
            std::max(0.9f, m_markerScale),
            cross
        );
        m_drawNode->drawSegment(
            {position.x, position.y - arm},
            {position.x, position.y + arm},
            std::max(0.9f, m_markerScale),
            cross
        );

        if (!m_labelsEnabled) continue;

        std::string const text = cluster->deathCount > 1
            ? fmt::format("x{}  {:.1f}%", cluster->deathCount, cluster->meanProgressPercent)
            : fmt::format("DEATH  {:.1f}%", cluster->meanProgressPercent);
        auto* label = cocos2d::CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
        if (!label) continue;

        label->setScale(std::clamp(0.30f * m_markerScale, 0.25f, 0.52f));
        label->setOpacity(245);
        label->setColor(cocos2d::ccColor3B {255, 225, 170});
        label->setPosition({position.x, position.y + radius + 9.0f * m_markerScale});
        m_labelLayer->addChild(label);
    }
}

} // namespace dash_echo
