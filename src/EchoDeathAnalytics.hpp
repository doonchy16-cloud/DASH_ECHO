#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace dash_echo {

struct DeathEvent {
    std::uint64_t eventId = 0;
    std::uint64_t attemptId = 0;
    double timeSeconds = 0.0;
    float progressPercent = 0.0f;
    std::uint8_t playerIndex = 1;
    float x = 0.0f;
    float y = 0.0f;
    bool hazardPresent = false;
    int hazardObjectId = 0;
    float hazardX = 0.0f;
    float hazardY = 0.0f;
};

struct DeathCluster {
    std::uint64_t clusterId = 0;
    std::size_t deathCount = 0;
    float centroidX = 0.0f;
    float centroidY = 0.0f;
    float meanProgressPercent = 0.0f;
    float minProgressPercent = 0.0f;
    float maxProgressPercent = 0.0f;
    std::uint64_t firstAttemptId = 0;
    std::uint64_t lastAttemptId = 0;
};

struct HeatmapBucket {
    float beginPercent = 0.0f;
    float endPercent = 0.0f;
    std::size_t deathCount = 0;
    float normalizedIntensity = 0.0f;
};

struct DeathAnalyticsStats {
    std::size_t retainedDeathEvents = 0;
    std::size_t clusterCount = 0;
    std::size_t hottestClusterDeaths = 0;
    std::size_t hottestHeatmapBucketDeaths = 0;
    std::uint64_t revision = 0;
};

class EchoDeathAnalytics final {
public:
    static constexpr std::size_t kMaxDeathEvents = 4096;
    static constexpr std::size_t kHeatmapBucketCount = 100;
    static constexpr float kClusterRadius = 54.0f;
    static constexpr float kClusterProgressWindow = 1.50f;

    bool recordDeath(DeathEvent event);
    void clear();

    [[nodiscard]] std::deque<DeathEvent> const& events() const;
    [[nodiscard]] std::vector<DeathCluster> const& clusters() const;
    [[nodiscard]] std::array<HeatmapBucket, kHeatmapBucketCount> const& heatmap() const;
    [[nodiscard]] DeathAnalyticsStats stats() const;
    [[nodiscard]] std::uint64_t revision() const;

private:
    static bool eventFitsCluster(DeathEvent const& event, DeathCluster const& cluster);
    static bool clustersCanMerge(DeathCluster const& left, DeathCluster const& right);
    static void absorbEvent(DeathCluster& cluster, DeathEvent const& event);
    static void absorbCluster(DeathCluster& target, DeathCluster const& source);

    void trimRetention();
    void rebuildDerived();
    void rebuildClusters();
    void mergeCompatibleClusters();
    void rebuildHeatmap();

    std::deque<DeathEvent> m_events;
    std::vector<DeathCluster> m_clusters;
    std::array<HeatmapBucket, kHeatmapBucketCount> m_heatmap {};
    std::uint64_t m_nextEventId = 1;
    std::uint64_t m_lastRecordedAttemptId = 0;
    std::uint64_t m_revision = 0;
};

} // namespace dash_echo
