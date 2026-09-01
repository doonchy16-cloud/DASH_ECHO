#pragma once

#include "EchoRecorder.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace dash_echo {

enum class VisualProfile : std::uint8_t {
    Clean,
    Competitive,
    Multiverse,
    Chaos,
    Custom
};

enum class RenderingQuality : std::uint8_t {
    Auto,
    Full,
    Balanced,
    Performance
};

enum class ReplayCameraSetting : std::uint8_t {
    Recorded,
    Follow,
    Smooth,
    Drone,
    DynamicZoom,
    DeathCam
};

struct RawEchoSettings {
    std::int64_t ghostCount = 16;
    std::string visualProfile = "Multiverse";
    std::int64_t olderOpacityMin = 36;
    std::int64_t olderOpacityMax = 104;
    double ageFadeStrength = 1.0;
    bool priorityXray = true;
    bool lastEnabled = true;
    ColorRGB lastColor {74, 163, 255};
    std::int64_t lastOpacity = 190;
    bool lastTrail = true;
    bool bestEnabled = true;
    ColorRGB bestColor {255, 213, 74};
    std::int64_t bestOpacity = 220;
    bool bestTrail = true;
    double trailSeconds = 0.55;
    double trailWidth = 1.8;
    std::int64_t trailOpacity = 170;
    bool deathMarkers = true;
    double deathMarkerScale = 1.0;
    bool deathLabels = true;
    bool deathXray = true;
    bool heatStrip = true;
    std::int64_t heatStripOpacity = 170;
    std::string defaultPlaybackRate = "1.00";
    std::string defaultCameraMode = "Recorded";
    std::int64_t recorderSampleRate = 120;
    std::int64_t replayRetention = 10'000;
    std::int64_t diskBudgetMb = 2'048;
    std::string renderingQuality = "Auto";
    bool diagnostics = false;
};

struct GhostSettingsSnapshot {
    std::size_t requestedCount = 16;
    std::size_t effectiveCount = 16;
    VisualProfile profile = VisualProfile::Multiverse;
    std::uint8_t olderOpacityMin = 36;
    std::uint8_t olderOpacityMax = 104;
    float ageFadeStrength = 1.0f;
    bool priorityXray = true;
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
    bool operator==(GhostSettingsSnapshot const&) const = default;
};

struct DeathSettingsSnapshot {
    bool markers = true;
    float markerScale = 1.0f;
    bool labels = true;
    bool xray = true;
    bool heatStrip = true;
    std::uint8_t heatStripOpacity = 170;
    bool operator==(DeathSettingsSnapshot const&) const = default;
};

struct ReplaySettingsSnapshot {
    float defaultPlaybackRate = 1.0f;
    ReplayCameraSetting defaultCamera = ReplayCameraSetting::Recorded;
    bool operator==(ReplaySettingsSnapshot const&) const = default;
};

struct StorageSettingsSnapshot {
    std::size_t replayRetention = 10'000;
    std::size_t diskBudgetMb = 2'048;
    bool operator==(StorageSettingsSnapshot const&) const = default;
};

struct EchoSettingsSnapshot {
    GhostSettingsSnapshot ghosts;
    DeathSettingsSnapshot deaths;
    ReplaySettingsSnapshot replay;
    StorageSettingsSnapshot storage;
    double recorderSampleRateHz = 120.0;
    RenderingQuality renderingQuality = RenderingQuality::Auto;
    bool diagnostics = false;
    bool operator==(EchoSettingsSnapshot const&) const = default;
};

struct EchoSettingsDiff {
    bool presentation = false;
    bool fleetStructure = false;
    bool recorderPolicy = false;
    bool persistencePolicy = false;
    bool replayDefaults = false;
    bool diagnostics = false;
};

[[nodiscard]] EchoSettingsSnapshot normalizeEchoSettings(RawEchoSettings const& raw);
[[nodiscard]] EchoSettingsDiff diffEchoSettings(
    EchoSettingsSnapshot const& oldValue,
    EchoSettingsSnapshot const& newValue
);

} // namespace dash_echo
