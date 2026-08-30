#include "EchoDeathAnalytics.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace dash_echo {

namespace {

float spatialDistance(
    float leftX,
    float leftY,
    float rightX,
    float rightY
) {
    return std::hypot(rightX - leftX, rightY - leftY);
}

bool finiteDeathEvent(DeathEvent const& event) {
    return
        std::isfinite(event.timeSeconds) &&
        std::isfinite(event.progressPercent) &&
        std::isfinite(event.x) &&
        std::isfinite(event.y);
}

} // namespace

EchoDeathAnalytics::EchoDeathAnalytics() {
    initializeHeatmap();
    m_clusters.reserve(128);
}

bool EchoDeathAnalytics::recordDeath(DeathEvent event) {
    if (event.attemptId == 0 || !finiteDeathEvent(event)) {
        return false;
    }

    // A Geometry Dash attempt has one terminal death outcome. destroyPlayer may
    // be reached more than once by chained hooks or dual-player cleanup, so the
    // attempt ID is the authoritative deduplication key.
    if (event.attemptId == m_lastRecordedAttemptId) {
        return false;
    }

    event.eventId = m_nextEventId++;
    event.timeSeconds = std::max(0.0, event.timeSeconds);
    event.progressPercent = std::clamp(event.progressPercent, 0.0f, 100.0f);
    event.playerIndex = event.playerIndex == 2 ? 2 : 1;

    if (
        event.hazardPresent &&
        (!std::isfinite(event.hazardX) || !std::isfinite(event.hazardY))
    ) {
        event.hazardPresent = false;
        event.hazardObjectId = 0;
        event.hazardX = 0.0f;
        event.hazardY = 0.0f;
    }

    m_events.push_back(event);
    m_lastRecordedAttemptId = event.attemptId;
    ++m_totalDeathsRecorded;

    // Aggregates are lifetime session authority. Raw event retention is a
    // separate bounded detail cache, so crossing 4096 deaths never causes an
    // O(N^2) full analytics rebuild on every later death.
    accumulateCluster(event);
    accumulateHeatmap(event);
    trimRawEventRetention();

    ++m_revision;
    return true;
}

void EchoDeathAnalytics::clear() {
    m_events.clear();
    m_clusters.clear();
    m_heatmap = {};
    initializeHeatmap();
    m_nextEventId = 1;
    m_lastRecordedAttemptId = 0;
    m_totalDeathsRecorded = 0;
    m_unclusteredDeaths = 0;
    ++m_revision;
}

std::deque<DeathEvent> const& EchoDeathAnalytics::events() const {
    return m_events;
}

std::vector<DeathCluster> const& EchoDeathAnalytics::clusters() const {
    return m_clusters;
}

std::array<HeatmapBucket, EchoDeathAnalytics::kHeatmapBucketCount> const&
EchoDeathAnalytics::heatmap() const {
    return m_heatmap;
}

DeathAnalyticsStats EchoDeathAnalytics::stats() const {
    std::size_t hottestCluster = 0;
    for (auto const& cluster : m_clusters) {
        hottestCluster = std::max(hottestCluster, cluster.deathCount);
    }

    std::size_t hottestBucket = 0;
    for (auto const& bucket : m_heatmap) {
        hottestBucket = std::max(hottestBucket, bucket.deathCount);
    }

    return DeathAnalyticsStats {
        m_totalDeathsRecorded,
        m_events.size(),
        m_clusters.size(),
        hottestCluster,
        hottestBucket,
        m_unclusteredDeaths,
        m_revision
    };
}

std::uint64_t EchoDeathAnalytics::revision() const {
    return m_revision;
}

bool EchoDeathAnalytics::eventFitsCluster(
    DeathEvent const& event,
    DeathCluster const& cluster
) {
    float const distance = spatialDistance(
        event.x,
        event.y,
        cluster.centroidX,
        cluster.centroidY
    );

    float const progressDistance = std::abs(
        event.progressPercent - cluster.meanProgressPercent
    );

    return
        distance <= kClusterRadius &&
        progressDistance <= kClusterProgressWindow;
}

bool EchoDeathAnalytics::clustersCanMerge(
    DeathCluster const& left,
    DeathCluster const& right
) {
    float const distance = spatialDistance(
        left.centroidX,
        left.centroidY,
        right.centroidX,
        right.centroidY
    );

    float const progressDistance = std::abs(
        left.meanProgressPercent - right.meanProgressPercent
    );

    // The merge threshold is tighter than event admission. This heals clusters
    // split by centroid drift without collapsing nearby-but-distinct hazards.
    return
        distance <= kClusterRadius * 0.70f &&
        progressDistance <= kClusterProgressWindow * 0.70f;
}

void EchoDeathAnalytics::absorbEvent(
    DeathCluster& cluster,
    DeathEvent const& event
) {
    float const oldWeight = static_cast<float>(cluster.deathCount);
    float const newWeight = oldWeight + 1.0f;

    cluster.centroidX =
        (cluster.centroidX * oldWeight + event.x) / newWeight;
    cluster.centroidY =
        (cluster.centroidY * oldWeight + event.y) / newWeight;
    cluster.meanProgressPercent =
        (cluster.meanProgressPercent * oldWeight + event.progressPercent) / newWeight;

    cluster.minProgressPercent = std::min(
        cluster.minProgressPercent,
        event.progressPercent
    );
    cluster.maxProgressPercent = std::max(
        cluster.maxProgressPercent,
        event.progressPercent
    );
    cluster.lastAttemptId = std::max(cluster.lastAttemptId, event.attemptId);
    ++cluster.deathCount;
}

void EchoDeathAnalytics::absorbCluster(
    DeathCluster& target,
    DeathCluster const& source
) {
    if (source.deathCount == 0) return;
    if (target.deathCount == 0) {
        target = source;
        return;
    }

    float const targetWeight = static_cast<float>(target.deathCount);
    float const sourceWeight = static_cast<float>(source.deathCount);
    float const totalWeight = targetWeight + sourceWeight;

    target.centroidX =
        (target.centroidX * targetWeight + source.centroidX * sourceWeight) /
        totalWeight;
    target.centroidY =
        (target.centroidY * targetWeight + source.centroidY * sourceWeight) /
        totalWeight;
    target.meanProgressPercent =
        (target.meanProgressPercent * targetWeight +
         source.meanProgressPercent * sourceWeight) /
        totalWeight;

    target.minProgressPercent = std::min(
        target.minProgressPercent,
        source.minProgressPercent
    );
    target.maxProgressPercent = std::max(
        target.maxProgressPercent,
        source.maxProgressPercent
    );
    target.firstAttemptId = std::min(
        target.firstAttemptId,
        source.firstAttemptId
    );
    target.lastAttemptId = std::max(
        target.lastAttemptId,
        source.lastAttemptId
    );
    target.clusterId = std::min(target.clusterId, source.clusterId);
    target.deathCount += source.deathCount;
}

void EchoDeathAnalytics::initializeHeatmap() {
    for (std::size_t i = 0; i < m_heatmap.size(); ++i) {
        auto& bucket = m_heatmap[i];
        bucket.beginPercent = static_cast<float>(i);
        bucket.endPercent = static_cast<float>(i + 1);
        bucket.deathCount = 0;
        bucket.normalizedIntensity = 0.0f;
    }
}

void EchoDeathAnalytics::trimRawEventRetention() {
    while (m_events.size() > kMaxDeathEvents) {
        m_events.pop_front();
    }
}

void EchoDeathAnalytics::accumulateCluster(DeathEvent const& event) {
    std::size_t bestIndex = m_clusters.size();
    float bestScore = std::numeric_limits<float>::infinity();

    for (std::size_t i = 0; i < m_clusters.size(); ++i) {
        auto const& cluster = m_clusters[i];
        if (!eventFitsCluster(event, cluster)) continue;

        float const distance = spatialDistance(
            event.x,
            event.y,
            cluster.centroidX,
            cluster.centroidY
        );
        float const progressDistance = std::abs(
            event.progressPercent - cluster.meanProgressPercent
        );
        float const score =
            distance / kClusterRadius +
            progressDistance / kClusterProgressWindow;

        if (score < bestScore) {
            bestScore = score;
            bestIndex = i;
        }
    }

    if (bestIndex < m_clusters.size()) {
        absorbEvent(m_clusters[bestIndex], event);
        mergeClusterAt(bestIndex);
        return;
    }

    if (m_clusters.size() >= kMaxClusters) {
        // Do not corrupt cluster meaning merely to preserve a perfect count.
        // Heatmap and total-death authority still record this event truthfully.
        ++m_unclusteredDeaths;
        return;
    }

    DeathCluster cluster;
    cluster.clusterId = event.eventId;
    cluster.deathCount = 1;
    cluster.centroidX = event.x;
    cluster.centroidY = event.y;
    cluster.meanProgressPercent = event.progressPercent;
    cluster.minProgressPercent = event.progressPercent;
    cluster.maxProgressPercent = event.progressPercent;
    cluster.firstAttemptId = event.attemptId;
    cluster.lastAttemptId = event.attemptId;
    m_clusters.push_back(cluster);
    mergeClusterAt(m_clusters.size() - 1);
}

void EchoDeathAnalytics::mergeClusterAt(std::size_t index) {
    if (index >= m_clusters.size()) return;

    bool merged = true;
    while (merged) {
        merged = false;

        for (std::size_t other = 0; other < m_clusters.size(); ++other) {
            if (other == index) continue;
            if (!clustersCanMerge(m_clusters[index], m_clusters[other])) continue;

            absorbCluster(m_clusters[index], m_clusters[other]);
            m_clusters.erase(
                m_clusters.begin() + static_cast<std::ptrdiff_t>(other)
            );
            if (other < index) {
                --index;
            }
            merged = true;
            break;
        }
    }
}

void EchoDeathAnalytics::accumulateHeatmap(DeathEvent const& event) {
    float const bounded = std::clamp(event.progressPercent, 0.0f, 100.0f);
    std::size_t index = static_cast<std::size_t>(std::floor(bounded));
    if (index >= kHeatmapBucketCount) {
        index = kHeatmapBucketCount - 1;
    }

    ++m_heatmap[index].deathCount;
    normalizeHeatmap();
}

void EchoDeathAnalytics::normalizeHeatmap() {
    std::size_t peak = 0;
    for (auto const& bucket : m_heatmap) {
        peak = std::max(peak, bucket.deathCount);
    }

    if (peak == 0) {
        for (auto& bucket : m_heatmap) {
            bucket.normalizedIntensity = 0.0f;
        }
        return;
    }

    for (auto& bucket : m_heatmap) {
        bucket.normalizedIntensity =
            static_cast<float>(bucket.deathCount) / static_cast<float>(peak);
    }
}

} // namespace dash_echo
