#pragma once

#include "EchoGhost.hpp"
#include "EchoRecorder.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace cocos2d {
class CCNode;
}

namespace dash_echo {

struct GhostFleetStats {
    std::size_t assignedGhosts = 0;
    std::size_t configuredGhostLimit = 0;
    std::uint64_t newestAttemptId = 0;
    std::uint64_t personalBestAttemptId = 0;
};

class EchoGhostFleet final {
public:
    // Hard rendering cap: each historical attempt may render two SimplePlayers
    // in dual mode, so six slots means at most twelve ghost player nodes.
    static constexpr std::size_t kMaxGhosts = 6;
    static constexpr std::size_t kDefaultGhostLimit = 4;
    static constexpr std::uint8_t kOldestOpacity = 42;
    static constexpr std::uint8_t kNewestOpacity = 108;
    static constexpr std::uint8_t kPersonalBestOpacity = 138;

    bool attach(cocos2d::CCNode* parent, int topZOrder);
    void detach();

    void setGhostLimit(std::size_t ghostLimit);

    // Rebuilds selection only at attempt boundaries. Pointers remain stable for
    // the active attempt because recorder retention trimming occurs before this
    // call and not during frame capture.
    void rebuild(EchoRecorder const& recorder);
    void synchronize(double timeSeconds);
    void stop();
    void hide();

    [[nodiscard]] bool isAttached() const;
    [[nodiscard]] std::size_t ghostLimit() const;
    [[nodiscard]] std::size_t activeGhostCount() const;
    [[nodiscard]] GhostFleetStats stats() const;

private:
    struct Slot {
        EchoGhost ghost;
        AttemptRecord const* attempt = nullptr;
        bool personalBest = false;
    };

    static std::uint8_t opacityForRank(
        std::size_t rank,
        std::size_t count,
        bool personalBest
    );

    std::array<Slot, kMaxGhosts> m_slots;
    std::size_t m_ghostLimit = kDefaultGhostLimit;
    std::size_t m_activeGhosts = 0;
    std::uint64_t m_newestAttemptId = 0;
    std::uint64_t m_personalBestAttemptId = 0;
    bool m_attached = false;
};

} // namespace dash_echo
