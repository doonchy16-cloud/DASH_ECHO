#pragma once

#include "EchoAttemptHistory.hpp"
#include "EchoRecorder.hpp"

#include <cstdint>
#include <vector>

namespace dash_echo {

enum class ReplayTimelineState : std::uint8_t {
    Empty,
    Ready,
    Playing,
    Finished
};

enum class ReplayTimelineMarkerType : std::uint8_t {
    Start,
    End,
    Death,
    Completion,
    PersonalBest
};

struct ReplayTimelineMarker {
    ReplayTimelineMarkerType type = ReplayTimelineMarkerType::Start;
    double timeSeconds = 0.0;
    float normalizedPosition = 0.0f;
    float progressPercent = 0.0f;
};

struct ReplayClip {
    AttemptRecord attempt;
    AttemptHistoryEntry history;
    std::vector<ReplayTimelineMarker> markers;
    double startTimeSeconds = 0.0;
    double endTimeSeconds = 0.0;
    double durationSeconds = 0.0;
};

class EchoReplayTimeline final {
public:
    bool load(AttemptRecord const& attempt, AttemptHistoryEntry const& history);
    void clear();
    void start();
    void restart();
    void advance(float dt);

    [[nodiscard]] bool isLoaded() const;
    [[nodiscard]] bool isPlaying() const;
    [[nodiscard]] ReplayTimelineState state() const;
    [[nodiscard]] ReplayClip const* clip() const;
    [[nodiscard]] AttemptRecord const* replayAttempt() const;
    [[nodiscard]] AttemptHistoryEntry const* historyEntry() const;
    [[nodiscard]] std::vector<ReplayTimelineMarker> const& markers() const;
    [[nodiscard]] std::uint64_t sourceAttemptId() const;
    [[nodiscard]] double cursorSeconds() const;
    [[nodiscard]] double durationSeconds() const;
    [[nodiscard]] float normalizedCursor() const;
    [[nodiscard]] float progressPercentAtCursor() const;

private:
    static bool validateAttempt(AttemptRecord const& attempt);
    static ReplayTimelineMarker makeMarker(
        ReplayTimelineMarkerType type,
        double timeSeconds,
        float progressPercent,
        double startTimeSeconds,
        double durationSeconds
    );

    void buildMarkers();
    void setCursorClamped(double timeSeconds);

    ReplayClip m_clip;
    ReplayTimelineState m_state = ReplayTimelineState::Empty;
    double m_cursorSeconds = 0.0;
};

} // namespace dash_echo
