#include "EchoGhostFleet.hpp"

#include <Geode/Geode.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace dash_echo {

namespace {

cocos2d::ccColor4F trailColor(ColorRGB const& color, float alpha) {
    return cocos2d::ccColor4F {
        static_cast<float>(color.r) / 255.0f,
        static_cast<float>(color.g) / 255.0f,
        static_cast<float>(color.b) / 255.0f,
        std::clamp(alpha, 0.0f, 1.0f)
    };
}

} // namespace

bool EchoGhostFleet::attach(cocos2d::CCNode* parent, int topZOrder) {
    if (m_attached) return true;
    if (!parent) return false;

    m_parent = parent;
    m_topZOrder = topZOrder;

    auto* trails = cocos2d::CCDrawNode::create();
    if (!trails) {
        m_parent = nullptr;
        return false;
    }
    parent->addChild(
        trails,
        m_visual.priorityXray ? topZOrder + 2 : topZOrder - 1
    );
    m_priorityTrailNode = trails;

    for (auto& slot : m_slots) {
        if (!slot->ghost.attach(parent, topZOrder)) {
            detach();
            return false;
        }
    }

    m_attached = true;
    return true;
}

void EchoGhostFleet::detach() {
    stop();
    for (auto& slot : m_slots) {
        slot->ghost.detach();
    }
    if (m_priorityTrailNode) {
        m_priorityTrailNode->removeFromParentAndCleanup(true);
        m_priorityTrailNode = nullptr;
    }
    m_parent = nullptr;
    m_topZOrder = 0;
    m_attached = false;
}

void EchoGhostFleet::setGhostLimit(std::size_t ghostLimit) {
    m_ghostLimit = std::min(ghostLimit, kMaxGhosts);
}

void EchoGhostFleet::setVisualSettings(GhostFleetVisualSettings const& settings) {
    m_visual = settings;
    m_visual.ageFadeStrength = std::clamp(m_visual.ageFadeStrength, 0.0f, 2.0f);
    m_visual.trailSeconds = std::clamp(m_visual.trailSeconds, 0.05f, 2.0f);
    m_visual.trailWidth = std::clamp(m_visual.trailWidth, 0.5f, 6.0f);

    if (m_priorityTrailNode) {
        m_priorityTrailNode->setZOrder(
            m_visual.priorityXray ? m_topZOrder + 2 : m_topZOrder - 1
        );
    }

    for (std::size_t i = 0; i < m_activeGhosts && i < m_slots.size(); ++i) {
        configureSlot(
            *m_slots[i],
            i,
            m_activeGhosts,
            m_newestAttemptId,
            m_bestRecordedAttemptId
        );
    }
}

void EchoGhostFleet::rebuild(EchoReplayArchive const& archive) {
    stop();

    auto const* latest = archive.latestReplay();
    auto const* best = archive.bestRecordedReplay();
    m_newestAttemptId = latest ? latest->attemptId : 0;
    m_bestRecordedAttemptId = best ? best->attemptId : 0;

    if (m_ghostLimit == 0 || archive.replays().empty()) return;

    std::vector<AttemptRecord const*> selectedNewestFirst;
    selectedNewestFirst.reserve(m_ghostLimit);

    auto const& replays = archive.replays();
    for (auto it = replays.rbegin(); it != replays.rend(); ++it) {
        if (!it->finalized || it->frames.empty()) continue;
        selectedNewestFirst.push_back(&*it);
        if (selectedNewestFirst.size() == m_ghostLimit) break;
    }

    if (best) {
        bool const alreadySelected = std::any_of(
            selectedNewestFirst.begin(),
            selectedNewestFirst.end(),
            [best](AttemptRecord const* attempt) {
                return attempt && attempt->attemptId == best->attemptId;
            }
        );
        if (!alreadySelected) {
            if (selectedNewestFirst.size() < m_ghostLimit) {
                selectedNewestFirst.push_back(best);
            } else if (!selectedNewestFirst.empty()) {
                selectedNewestFirst.back() = best;
            }
        }
    }

    std::sort(
        selectedNewestFirst.begin(),
        selectedNewestFirst.end(),
        [](AttemptRecord const* left, AttemptRecord const* right) {
            return left->attemptId < right->attemptId;
        }
    );
    selectedNewestFirst.erase(
        std::unique(
            selectedNewestFirst.begin(),
            selectedNewestFirst.end(),
            [](AttemptRecord const* left, AttemptRecord const* right) {
                return left->attemptId == right->attemptId;
            }
        ),
        selectedNewestFirst.end()
    );

    m_activeGhosts = std::min(selectedNewestFirst.size(), m_ghostLimit);
    if (!ensurePool(m_activeGhosts)) {
        stop();
        return;
    }

    for (std::size_t i = 0; i < m_activeGhosts; ++i) {
        auto& slot = *m_slots[i];
        slot.attempt = selectedNewestFirst[i];
        configureSlot(
            slot,
            i,
            m_activeGhosts,
            m_newestAttemptId,
            m_bestRecordedAttemptId
        );
        slot.ghost.play(slot.attempt);
    }

    for (std::size_t i = m_activeGhosts; i < m_slots.size(); ++i) {
        m_slots[i]->ghost.stop();
        m_slots[i]->attempt = nullptr;
        m_slots[i]->role = GhostRole::Older;
    }
}

void EchoGhostFleet::synchronize(double timeSeconds) {
    for (std::size_t i = 0; i < m_activeGhosts; ++i) {
        m_slots[i]->ghost.synchronize(timeSeconds);
    }
    rebuildPriorityTrails(timeSeconds);
}

void EchoGhostFleet::stop() {
    if (m_priorityTrailNode) m_priorityTrailNode->clear();
    for (auto& slot : m_slots) {
        slot->ghost.stop();
        slot->attempt = nullptr;
        slot->role = GhostRole::Older;
    }
    m_activeGhosts = 0;
    m_newestAttemptId = 0;
    m_bestRecordedAttemptId = 0;
}

void EchoGhostFleet::hide() {
    if (m_priorityTrailNode) m_priorityTrailNode->clear();
    for (auto& slot : m_slots) {
        slot->ghost.hide();
    }
}

bool EchoGhostFleet::isAttached() const {
    return m_attached;
}

std::size_t EchoGhostFleet::ghostLimit() const {
    return m_ghostLimit;
}

std::size_t EchoGhostFleet::activeGhostCount() const {
    return m_activeGhosts;
}

GhostFleetStats EchoGhostFleet::stats() const {
    return GhostFleetStats {
        m_activeGhosts,
        m_ghostLimit,
        m_slots.size(),
        m_newestAttemptId,
        m_bestRecordedAttemptId
    };
}

bool EchoGhostFleet::ensurePool(std::size_t count) {
    count = std::min(count, kMaxGhosts);
    while (m_slots.size() < count) {
        auto slot = std::make_unique<Slot>();
        if (m_attached && !slot->ghost.attach(m_parent, m_topZOrder)) {
            return false;
        }
        m_slots.push_back(std::move(slot));
    }
    return true;
}

void EchoGhostFleet::configureSlot(
    Slot& slot,
    std::size_t rank,
    std::size_t count,
    std::uint64_t latestAttemptId,
    std::uint64_t bestAttemptId
) {
    bool const last = slot.attempt && slot.attempt->attemptId == latestAttemptId;
    bool const best = slot.attempt && slot.attempt->attemptId == bestAttemptId;

    if (last && best) slot.role = GhostRole::LastAndBest;
    else if (last) slot.role = GhostRole::LastAttempt;
    else if (best) slot.role = GhostRole::BestRecorded;
    else slot.role = GhostRole::Older;

    slot.ghost.setOpacity(opacityForRank(rank, count, slot.role));
}

void EchoGhostFleet::rebuildPriorityTrails(double timeSeconds) {
    if (!m_priorityTrailNode) return;
    m_priorityTrailNode->clear();

    for (std::size_t i = 0; i < m_activeGhosts; ++i) {
        auto const& slot = *m_slots[i];
        bool draw = false;
        if (
            (slot.role == GhostRole::LastAttempt || slot.role == GhostRole::LastAndBest) &&
            m_visual.lastEnabled && m_visual.lastTrail
        ) draw = true;
        if (
            (slot.role == GhostRole::BestRecorded || slot.role == GhostRole::LastAndBest) &&
            m_visual.bestEnabled && m_visual.bestTrail
        ) draw = true;
        if (draw) drawTrailForSlot(slot, timeSeconds);
    }
}

void EchoGhostFleet::drawTrailForSlot(Slot const& slot, double timeSeconds) {
    if (!m_priorityTrailNode || !slot.attempt || slot.attempt->frames.size() < 2) return;

    auto const& frames = slot.attempt->frames;
    if (timeSeconds < frames.front().timeSeconds || timeSeconds > frames.back().timeSeconds) return;

    auto upper = std::upper_bound(
        frames.begin(),
        frames.end(),
        timeSeconds,
        [](double value, FrameRecord const& frame) {
            return value < frame.timeSeconds;
        }
    );
    if (upper == frames.begin()) return;

    std::size_t end = static_cast<std::size_t>((upper - 1) - frames.begin());
    double const startTime = std::max(
        frames.front().timeSeconds,
        timeSeconds - static_cast<double>(m_visual.trailSeconds)
    );

    std::size_t begin = end;
    while (begin > 0 && frames[begin - 1].timeSeconds >= startTime) --begin;

    constexpr std::size_t kMaxTrailSegments = 64;
    if (end > begin + kMaxTrailSegments) begin = end - kMaxTrailSegments;

    bool const drawLast =
        slot.role == GhostRole::LastAttempt || slot.role == GhostRole::LastAndBest;
    bool const drawBest =
        slot.role == GhostRole::BestRecorded || slot.role == GhostRole::LastAndBest;

    auto drawPlayer = [&](bool playerTwo) {
        for (std::size_t i = begin + 1; i <= end; ++i) {
            auto const& previousFrame = frames[i - 1];
            auto const& currentFrame = frames[i];
            auto const& previous = playerTwo ? previousFrame.player2 : previousFrame.player1;
            auto const& current = playerTwo ? currentFrame.player2 : currentFrame.player1;
            bool const continuous = playerTwo
                ? currentFrame.player2ContinuousFromPrevious
                : currentFrame.player1ContinuousFromPrevious;

            if (
                !continuous ||
                !previous.present || !previous.visible ||
                !current.present || !current.visible
            ) continue;

            float const normalized = end == begin
                ? 1.0f
                : static_cast<float>(i - begin) / static_cast<float>(end - begin);
            float const alpha =
                (static_cast<float>(m_visual.trailOpacity) / 255.0f) *
                std::clamp(normalized, 0.10f, 1.0f);

            cocos2d::CCPoint const a {previous.x, previous.y};
            cocos2d::CCPoint const b {current.x, current.y};

            if (slot.role == GhostRole::LastAndBest && drawLast && drawBest) {
                // Dual-role run: broad blue last-attempt ribbon under a narrower
                // gold best-run ribbon. No halo/aura is rendered anywhere.
                m_priorityTrailNode->drawSegment(
                    a, b, m_visual.trailWidth * 1.55f,
                    trailColor(m_visual.lastColor, alpha * 0.58f)
                );
                m_priorityTrailNode->drawSegment(
                    a, b, m_visual.trailWidth,
                    trailColor(m_visual.bestColor, alpha)
                );
            } else if (drawBest && m_visual.bestTrail) {
                m_priorityTrailNode->drawSegment(
                    a, b, m_visual.trailWidth,
                    trailColor(m_visual.bestColor, alpha)
                );
            } else if (drawLast && m_visual.lastTrail) {
                m_priorityTrailNode->drawSegment(
                    a, b, m_visual.trailWidth,
                    trailColor(m_visual.lastColor, alpha)
                );
            }
        }
    };

    drawPlayer(false);
    drawPlayer(true);
}

std::uint8_t EchoGhostFleet::opacityForRank(
    std::size_t rank,
    std::size_t count,
    GhostRole role
) const {
    if (role == GhostRole::LastAttempt && m_visual.lastEnabled) {
        return m_visual.lastOpacity;
    }
    if (role == GhostRole::BestRecorded && m_visual.bestEnabled) {
        return m_visual.bestOpacity;
    }
    if (role == GhostRole::LastAndBest) {
        if (m_visual.lastEnabled && m_visual.bestEnabled) {
            return std::max(m_visual.lastOpacity, m_visual.bestOpacity);
        }
        if (m_visual.bestEnabled) return m_visual.bestOpacity;
        if (m_visual.lastEnabled) return m_visual.lastOpacity;
    }

    float normalized = 1.0f;
    if (count > 1) {
        normalized = static_cast<float>(rank) / static_cast<float>(count - 1);
    }
    float const shaped = std::pow(
        std::clamp(normalized, 0.0f, 1.0f),
        std::max(0.01f, m_visual.ageFadeStrength)
    );
    float const faded = std::lerp(
        static_cast<float>(m_visual.oldestOpacity),
        static_cast<float>(m_visual.newestOlderOpacity),
        shaped
    );
    return static_cast<std::uint8_t>(std::clamp(std::lround(faded), 0L, 255L));
}

} // namespace dash_echo
