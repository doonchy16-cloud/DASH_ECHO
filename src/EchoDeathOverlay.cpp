#include "EchoDeathOverlay.hpp"

#include <Geode/Geode.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace geode::prelude;

namespace dash_echo {

bool EchoDeathOverlay::attach(cocos2d::CCNode* parent, int zOrder) {
    if (isAttached()) return true;
    if (!parent) return false;

    auto* root = cocos2d::CCNode::create();
    auto* drawNode = cocos2d::CCDrawNode::create();
    auto* labelLayer = cocos2d::CCNode::create();
    if (!root || !drawNode || !labelLayer) {
        return false;
    }

    root->addChild(drawNode);
    root->addChild(labelLayer);
    root->setVisible(m_enabled);
    parent->addChild(root, zOrder);

    m_root = root;
    m_drawNode = drawNode;
    m_labelLayer = labelLayer;
    m_renderedRevision = 0;
    return true;
}

void EchoDeathOverlay::detach() {
    if (m_root) {
        m_root->removeFromParentAndCleanup(true);
    }

    m_root = nullptr;
    m_drawNode = nullptr;
    m_labelLayer = nullptr;
    m_renderedRevision = 0;
}

void EchoDeathOverlay::setEnabled(bool enabled) {
    m_enabled = enabled;
    if (m_root) {
        m_root->setVisible(enabled);
    }
}

void EchoDeathOverlay::refresh(EchoDeathAnalytics const& analytics) {
    if (!isAttached() || !m_enabled) return;
    if (m_renderedRevision == analytics.revision()) return;

    rebuild(analytics);
    m_renderedRevision = analytics.revision();
}

void EchoDeathOverlay::clearVisuals() {
    if (m_drawNode) {
        m_drawNode->clear();
    }
    if (m_labelLayer) {
        m_labelLayer->removeAllChildrenWithCleanup(true);
    }
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
    for (auto const& cluster : clusters) {
        selected.push_back(&cluster);
    }

    std::sort(
        selected.begin(),
        selected.end(),
        [](DeathCluster const* left, DeathCluster const* right) {
            if (left->deathCount != right->deathCount) {
                return left->deathCount > right->deathCount;
            }
            if (left->lastAttemptId != right->lastAttemptId) {
                return left->lastAttemptId > right->lastAttemptId;
            }
            return left->clusterId < right->clusterId;
        }
    );

    if (selected.size() > kMaxRenderedClusters) {
        selected.resize(kMaxRenderedClusters);
    }

    // Stable draw order keeps less important markers underneath stronger zones.
    std::sort(
        selected.begin(),
        selected.end(),
        [](DeathCluster const* left, DeathCluster const* right) {
            if (left->deathCount != right->deathCount) {
                return left->deathCount < right->deathCount;
            }
            return left->clusterId < right->clusterId;
        }
    );

    auto const stats = analytics.stats();
    float const hottest = static_cast<float>(
        std::max<std::size_t>(stats.hottestClusterDeaths, 1)
    );

    for (auto const* cluster : selected) {
        if (!cluster || cluster->deathCount == 0) continue;

        float const intensity = std::clamp(
            static_cast<float>(cluster->deathCount) / hottest,
            0.0f,
            1.0f
        );

        float const radius = std::clamp(
            4.0f + std::sqrt(static_cast<float>(cluster->deathCount)) * 2.0f,
            4.0f,
            14.0f
        );

        cocos2d::CCPoint const position {
            cluster->centroidX,
            cluster->centroidY
        };

        cocos2d::ccColor4F const outerColor {
            1.0f,
            0.45f - intensity * 0.25f,
            0.08f,
            0.18f + intensity * 0.48f
        };
        cocos2d::ccColor4F const innerColor {
            1.0f,
            0.78f - intensity * 0.35f,
            0.18f,
            0.38f + intensity * 0.48f
        };

        m_drawNode->drawDot(position, radius, outerColor);
        m_drawNode->drawDot(position, std::max(2.0f, radius * 0.42f), innerColor);

        if (cluster->deathCount < 2) continue;

        std::string const text = "x" + std::to_string(cluster->deathCount);
        auto* label = cocos2d::CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
        if (!label) continue;

        label->setScale(0.28f);
        label->setOpacity(220);
        label->setColor(cocos2d::ccColor3B {255, 230, 190});
        label->setPosition({
            cluster->centroidX,
            cluster->centroidY + radius + 7.0f
        });
        m_labelLayer->addChild(label);
    }
}

} // namespace dash_echo
