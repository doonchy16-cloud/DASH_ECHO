#include "EchoGhostFleet.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace dash_echo {

bool EchoGhostFleet::attach(cocos2d::CCNode* parent, int topZOrder) {
    if (m_attached) {
        return true;
    }
    if (!parent) {
        return false;
    }

    for (std::size_t i = 0; i < m_slots.size(); ++i) {
        // Slot order is oldest -> newest. The newest selected attempt therefore
        // receives the highest ghost z-order while all remain below the live player.
        int const zOrder = topZOrder - static_cast<int>(kMaxGhosts - 1 - i);
        if (!m_slots[i].ghost.attach(parent, zOrder)) {
            for (std::size_t rollback = 0; rollback <= i; ++rollback) {
                m_slots[rollback].ghost.detach();
            }
            m_attached = false;
            return false;
        }
    }

    m_attached = true;
    return true;
}

void EchoGhostFleet::detach() {
    stop();
    for (auto& slot : m_slots) {
        slot.ghost.detach();
    }
    m_attached = false;
}

void EchoGhostFleet::rebuild(EchoRecorder const& recorder) {
    stop();

    auto const* personalBest = recorder.personalBestAttempt();
    m_personalBestAttemptId = personalBest ? personalBest->attemptId : 0;

    std::vector<AttemptRecord const*> selectedNewestFirst;
    selectedNewestFirst.reserve(kMaxGhosts);

    auto const& attempts = recorder.attempts();
    for (auto it = attempts.rbegin(); it != attempts.rend(); ++it) {
        if (!it->finalized || it->frames.empty()) {
            continue;
        }

        selectedNewestFirst.push_back(&*it);
        if (selectedNewestFirst.size() == kMaxGhosts) {
            break;
        }
    }

    if (personalBest) {
        bool const alreadySelected = std::any_of(
            selectedNewestFirst.begin(),
            selectedNewestFirst.end(),
            [personalBest](AttemptRecord const* attempt) {
                return attempt && attempt->attemptId == personalBest->attemptId;
            }
        );

        if (!alreadySelected) {
            if (selectedNewestFirst.size() < kMaxGhosts) {
                selectedNewestFirst.push_back(personalBest);
            } else if (!selectedNewestFirst.empty()) {
                // Preserve the strongest historical attempt by replacing the
                // oldest member of the newest-N window, not the newest attempt.
                selectedNewestFirst.back() = personalBest;
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

    m_activeGhosts = std::min(selectedNewestFirst.size(), kMaxGhosts);
    m_newestAttemptId = 0;

    for (std::size_t i = 0; i < m_activeGhosts; ++i) {
        auto const* attempt = selectedNewestFirst[i];
        bool const isPersonalBest =
            personalBest && attempt->attemptId == personalBest->attemptId;

        auto& slot = m_slots[i];
        slot.attempt = attempt;
        slot.personalBest = isPersonalBest;
        slot.ghost.setOpacity(opacityForRank(i, m_activeGhosts, isPersonalBest));
        slot.ghost.play(attempt);

        m_newestAttemptId = std::max(m_newestAttemptId, attempt->attemptId);
    }

    for (std::size_t i = m_activeGhosts; i < m_slots.size(); ++i) {
        m_slots[i].ghost.stop();
        m_slots[i].attempt = nullptr;
        m_slots[i].personalBest = false;
    }
}

void EchoGhostFleet::synchronize(double timeSeconds) {
    for (std::size_t i = 0; i < m_activeGhosts; ++i) {
        m_slots[i].ghost.synchronize(timeSeconds);
    }
}

void EchoGhostFleet::stop() {
    for (auto& slot : m_slots) {
        slot.ghost.stop();
        slot.attempt = nullptr;
        slot.personalBest = false;
    }
    m_activeGhosts = 0;
    m_newestAttemptId = 0;
}

void EchoGhostFleet::hide() {
    for (auto& slot : m_slots) {
        slot.ghost.hide();
    }
}

bool EchoGhostFleet::isAttached() const {
    return m_attached;
}

std::size_t EchoGhostFleet::activeGhostCount() const {
    return m_activeGhosts;
}

GhostFleetStats EchoGhostFleet::stats() const {
    return GhostFleetStats {
        m_activeGhosts,
        m_newestAttemptId,
        m_personalBestAttemptId
    };
}

std::uint8_t EchoGhostFleet::opacityForRank(
    std::size_t rank,
    std::size_t count,
    bool personalBest
) {
    float normalized = 1.0f;
    if (count > 1) {
        normalized = static_cast<float>(rank) / static_cast<float>(count - 1);
    }

    float const faded = std::lerp(
        static_cast<float>(kOldestOpacity),
        static_cast<float>(kNewestOpacity),
        normalized
    );

    auto opacity = static_cast<std::uint8_t>(std::clamp(std::lround(faded), 0L, 255L));
    if (personalBest) {
        opacity = std::max(opacity, kPersonalBestOpacity);
    }
    return opacity;
}

} // namespace dash_echo
