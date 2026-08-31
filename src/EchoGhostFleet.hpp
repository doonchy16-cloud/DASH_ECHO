#pragma once

#include "EchoGhost.hpp"
#include "EchoReplayArchive.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace cocos2d {
class CCNode;
class CCDrawNode;
}

namespace dash_echo {

enum class GhostRole : std::uint8_t {
    Older,
    LastAttempt,
    BestRecorded,
    LastAndBest
};

struct GhostFleetVisualSettings {
    std::uint8_t oldestOpacity = 36;
    std::uint8_t newestOlderOpacity = 104;
    float ageFadeStrength = 1.0f;

    bool lastEnabled = true;
    ColorRGB lastColor {74, 163, 255};
    std::uint8_t lastOpacity = 190;
    bool lastTrail = true;

    bool bestEnabled = true;
    ColorRGB bestColor {255, 213, 74};
    std::uint8_t bestOpacity = 220;
    bool bestTrail = true;

    float trailSeconds = 0.55f;
    float trailWidth = 1.8f;
    std::uint8_t trailOpacity = 170;
    bool priorityXray = true;
};

struct GhostFleetStats {
    std::size_t assignedGhosts = 0;
    std::size_t configuredGhostLimit = 0;
    std::size_t allocatedGhostSlots = 0;
    std::uint64_t newestAttemptId = 0;
    std::uint64_t bestRecordedAttemptId = 0;
};

class EchoGhostFleet final {
public:
    static constexpr std::size_t kMaxGhosts = 256;
    static constexpr std::size_t kDefaultGhostLimit = 16;

    bool attach(cocos2d::CCNode* parent, int topZOrder);
    void detach();

    void setGhostLimit(std::size_t ghostLimit);
    void setVisualSettings(GhostFleetVisualSettings const& settings);

    // Selection references archive-owned AttemptRecords. Archive mutation occurs
    // only at attempt/lifecycle boundaries after fleet.stop(), then rebuild()
    // reacquires stable pointers.
    void rebuild(EchoReplayArchive const& archive);
    void synchronize(double timeSeconds);
    void synchronize(
        double timeSeconds,
        float progressPercent,
        bool progressAlignmentEnabled
    );
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
        GhostRole role = GhostRole::Older;
        bool progressAlignmentSafe = false;
        double synchronizedTimeSeconds = 0.0;
    };

    bool ensurePool(std::size_t count);
    void configureSlot(
        Slot& slot,
        std::size_t rank,
        std::size_t count,
        std::uint64_t latestAttemptId,
        std::uint64_t bestAttemptId
    );
    void rebuildPriorityTrails();
    void drawTrailForSlot(Slot const& slot, double timeSeconds);

    [[nodiscard]] static bool progressIsMonotonic(AttemptRecord const& attempt);
    [[nodiscard]] static double timeForProgress(
        AttemptRecord const& attempt,
        float progressPercent,
        double fallbackTimeSeconds
    );

    [[nodiscard]] std::uint8_t opacityForRank(
        std::size_t rank,
        std::size_t count,
        GhostRole role
    ) const;

    std::vector<std::unique_ptr<Slot>> m_slots;
    std::size_t m_ghostLimit = kDefaultGhostLimit;
    std::size_t m_activeGhosts = 0;
    std::uint64_t m_newestAttemptId = 0;
    std::uint64_t m_bestRecordedAttemptId = 0;
    GhostFleetVisualSettings m_visual;

    cocos2d::CCNode* m_parent = nullptr;
    cocos2d::CCDrawNode* m_priorityTrailNode = nullptr;
    int m_topZOrder = 0;
    bool m_attached = false;
};

} // namespace dash_echo
