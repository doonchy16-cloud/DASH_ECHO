#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

class PlayerObject;
namespace cocos2d {
class CCNode;
}

namespace dash_echo {

enum class AttemptEndReason {
    Active,
    Reset,
    Completed,
    LayerExit
};

enum class PlayerMode : std::uint8_t {
    Cube,
    Ship,
    Ball,
    Ufo,
    Wave,
    Robot,
    Spider,
    Swing
};

struct ColorRGB {
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;

    bool operator==(ColorRGB const&) const = default;
};

struct PlayerSnapshot {
    bool present = false;
    bool visible = false;
    PlayerMode mode = PlayerMode::Cube;
    ColorRGB color1;
    ColorRGB color2;
    float x = 0.0f;
    float y = 0.0f;
    float rotation = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
};

struct CameraSnapshot {
    bool present = false;
    float x = 0.0f;
    float y = 0.0f;
    float rotation = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
};

struct FrameRecord {
    std::uint64_t sequence = 0;
    double timeSeconds = 0.0;
    float progressPercent = 0.0f;
    PlayerSnapshot player1;
    PlayerSnapshot player2;
    CameraSnapshot camera;
    bool player1ContinuousFromPrevious = false;
    bool player2ContinuousFromPrevious = false;
    bool cameraContinuousFromPrevious = false;
};

struct AttemptRecord {
    std::uint64_t attemptId = 0;
    std::vector<FrameRecord> frames;
    std::uint64_t framesDropped = 0;
    double durationSeconds = 0.0;
    float maxProgressPercent = 0.0f;
    AttemptEndReason endReason = AttemptEndReason::Active;
    bool finalized = false;
};

struct RecorderStats {
    std::uint64_t attemptsStarted = 0;
    std::uint64_t attemptsFinalized = 0;
    std::uint64_t framesCaptured = 0;
    std::uint64_t framesDropped = 0;
    std::size_t retainedAttempts = 0;
    std::size_t retainedFrames = 0;
    double captureSampleRate = 120.0;
};

class EchoRecorder final {
public:
    // Recorder is now a current-session working set. Long-lived replay/history
    // ownership belongs to EchoReplayArchive in v1.1.
    static constexpr std::size_t kMaxFramesPerAttempt = 75'000;
    static constexpr std::size_t kMaxRetainedAttempts = 16;
    static constexpr std::size_t kMaxRetainedFrames = 250'000;
    static constexpr double kDefaultCaptureSampleRate = 120.0;
    static constexpr double kMinCaptureSampleRate = 30.0;
    static constexpr double kMaxCaptureSampleRate = 240.0;

    void beginAttempt();
    void captureFrame(
        float dt,
        float progressPercent,
        PlayerObject* player1,
        PlayerObject* player2,
        cocos2d::CCNode* viewportLayer
    );
    void captureEventFrame(
        float progressPercent,
        PlayerObject* player1,
        PlayerObject* player2,
        cocos2d::CCNode* viewportLayer
    );
    void finalizeAttempt(AttemptEndReason reason);
    void clear();

    void setCaptureSampleRate(double hz);
    [[nodiscard]] double captureSampleRate() const;
    void setNextAttemptIdFloor(std::uint64_t nextAttemptId);

    [[nodiscard]] bool hasActiveAttempt() const;
    [[nodiscard]] double activeElapsedSeconds() const;
    [[nodiscard]] AttemptRecord const* activeAttempt() const;
    [[nodiscard]] AttemptRecord const* attemptById(std::uint64_t attemptId) const;
    [[nodiscard]] AttemptRecord const* latestFinalizedAttempt() const;
    [[nodiscard]] AttemptRecord const* personalBestAttempt() const;
    [[nodiscard]] std::deque<AttemptRecord> const& attempts() const;
    [[nodiscard]] RecorderStats stats() const;

private:
    static bool canInterpolate(
        PlayerSnapshot const& previous,
        PlayerSnapshot const& current,
        double deltaSeconds
    );
    static bool canInterpolateCamera(
        CameraSnapshot const& previous,
        CameraSnapshot const& current,
        double deltaSeconds
    );
    static bool isBetterPersonalBest(
        AttemptRecord const& candidate,
        AttemptRecord const& incumbent
    );

    [[nodiscard]] AttemptRecord* mutableActiveAttempt();
    [[nodiscard]] PlayerSnapshot snapshotPlayer(PlayerObject* player) const;
    [[nodiscard]] CameraSnapshot snapshotCamera(cocos2d::CCNode* viewportLayer) const;
    void appendSnapshot(
        float progressPercent,
        PlayerObject* player1,
        PlayerObject* player2,
        cocos2d::CCNode* viewportLayer,
        bool replaceSameTimestamp
    );
    void updateAttemptProgress(float progressPercent);
    void trimRetention();

    std::deque<AttemptRecord> m_attempts;
    std::uint64_t m_nextAttemptId = 1;
    std::uint64_t m_nextFrameSequence = 1;
    std::uint64_t m_attemptsStarted = 0;
    std::uint64_t m_attemptsFinalized = 0;
    std::uint64_t m_framesCaptured = 0;
    std::uint64_t m_framesDropped = 0;
    std::size_t m_retainedFrames = 0;
    double m_activeElapsedSeconds = 0.0;
    double m_captureSampleRate = kDefaultCaptureSampleRate;
    double m_captureSampleInterval = 1.0 / kDefaultCaptureSampleRate;
    double m_nextRegularSampleTime = 0.0;
};

} // namespace dash_echo
