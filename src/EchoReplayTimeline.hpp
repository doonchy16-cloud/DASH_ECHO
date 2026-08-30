#pragma once

#include "EchoAttemptHistory.hpp"
#include "EchoRecorder.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace dash_echo {

enum class ReplayTimelineState : std::uint8_t {
    Empty,
    Ready,
    Playing,
    Paused,
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
    static constexpr std::array<float, 5> kPlaybackRates {
        0.10f,
        0.25f,
        0.50f,
        1.00f,
        2.00f
    };

    bool load(AttemptRecord const& attempt, AttemptHistoryEntry const& history);
    void clear();

    void start();
    void pause();
    void resume();
    void togglePlayback();
    void restart();
    void advance(float dt);

    bool setPlaybackRate(float rate);
    void cyclePlaybackRate();

    bool seekSeconds(double timeSeconds);
    bool seekNormalized(float normalizedPosition);
    bool stepPreviousFrame();
    bool stepNextFrame();

    [[nodiscard]] bool isLoaded() const;
    [[nodiscard]] bool isPlaying() const;
    [[nodiscard]] bool isPaused() const;
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
    [[nodiscard]] float playbackRate() const;

private:
    static bool validateAttempt(AttemptRecord const& attempt);
    static ReplayTimelineMarker makeMarker(
        ReplayTimelineMarkerType type,
        double timeSeconds,
        float progressPercent,
        double startTimeSeconds,
        double durationSeconds
    );
    static bool samePlaybackRate(float left, float right);

    void buildMarkers();
    void setCursorClamped(double timeSeconds);
    [[nodiscard]] std::size_t playbackRateIndex() const;

    ReplayClip m_clip;
    ReplayTimelineState m_state = ReplayTimelineState::Empty;
    double m_cursorSeconds = 0.0;
    float m_playbackRate = 1.0f;
};

} // namespace dash_echo
