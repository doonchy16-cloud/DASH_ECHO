#include "EchoSettings.hpp"

#include <algorithm>
#include <cmath>

namespace dash_echo {

namespace {

constexpr std::size_t kMaxGhosts = 256;
constexpr std::size_t kMinReplayRetention = 256;
constexpr std::size_t kMaxReplayRetention = 100'000;
constexpr std::size_t kMinDiskBudgetMb = 128;
constexpr std::size_t kMaxDiskBudgetMb = 8'192;

VisualProfile parseVisualProfile(std::string const& value) {
    if (value == "Clean") return VisualProfile::Clean;
    if (value == "Competitive") return VisualProfile::Competitive;
    if (value == "Chaos") return VisualProfile::Chaos;
    if (value == "Custom") return VisualProfile::Custom;
    return VisualProfile::Multiverse;
}

std::size_t profileCap(VisualProfile profile) {
    switch (profile) {
        case VisualProfile::Clean: return 2;
        case VisualProfile::Competitive: return 8;
        case VisualProfile::Multiverse: return 64;
        case VisualProfile::Chaos:
        case VisualProfile::Custom: return kMaxGhosts;
    }
    return 64;
}

RenderingQuality parseRenderingQuality(std::string const& value) {
    if (value == "Full") return RenderingQuality::Full;
    if (value == "Balanced") return RenderingQuality::Balanced;
    if (value == "Performance") return RenderingQuality::Performance;
    return RenderingQuality::Auto;
}

ReplayCameraSetting parseReplayCamera(std::string const& value) {
    if (value == "Follow") return ReplayCameraSetting::Follow;
    if (value == "Smooth") return ReplayCameraSetting::Smooth;
    if (value == "Drone") return ReplayCameraSetting::Drone;
    if (value == "Dynamic Zoom") return ReplayCameraSetting::DynamicZoom;
    if (value == "Death Cam") return ReplayCameraSetting::DeathCam;
    return ReplayCameraSetting::Recorded;
}

float parsePlaybackRate(std::string const& value) {
    if (value == "0.10") return 0.10f;
    if (value == "0.25") return 0.25f;
    if (value == "0.50") return 0.50f;
    if (value == "2.00") return 2.00f;
    return 1.00f;
}

std::uint8_t clampByte(std::int64_t value) {
    return static_cast<std::uint8_t>(std::clamp<std::int64_t>(value, 0, 255));
}

float finiteFloat(double value, double fallback, double minimum, double maximum) {
    if (!std::isfinite(value)) value = fallback;
    return static_cast<float>(std::clamp(value, minimum, maximum));
}

bool ghostPresentationChanged(
    GhostSettingsSnapshot const& oldValue,
    GhostSettingsSnapshot const& newValue
) {
    return
        oldValue.olderOpacityMin != newValue.olderOpacityMin ||
        oldValue.olderOpacityMax != newValue.olderOpacityMax ||
        oldValue.ageFadeStrength != newValue.ageFadeStrength ||
        oldValue.priorityXray != newValue.priorityXray ||
        oldValue.lastEnabled != newValue.lastEnabled ||
        oldValue.lastColor != newValue.lastColor ||
        oldValue.lastOpacity != newValue.lastOpacity ||
        oldValue.lastTrail != newValue.lastTrail ||
        oldValue.bestEnabled != newValue.bestEnabled ||
        oldValue.bestColor != newValue.bestColor ||
        oldValue.bestOpacity != newValue.bestOpacity ||
        oldValue.bestTrail != newValue.bestTrail ||
        oldValue.trailSeconds != newValue.trailSeconds ||
        oldValue.trailWidth != newValue.trailWidth ||
        oldValue.trailOpacity != newValue.trailOpacity;
}

} // namespace

EchoSettingsSnapshot normalizeEchoSettings(RawEchoSettings const& raw) {
    EchoSettingsSnapshot value;

    auto const clampedGhostCount = std::clamp<std::int64_t>(
        raw.ghostCount,
        0,
        static_cast<std::int64_t>(kMaxGhosts)
    );
    value.ghosts.requestedCount = static_cast<std::size_t>(clampedGhostCount);
    value.ghosts.profile = parseVisualProfile(raw.visualProfile);
    value.ghosts.effectiveCount = std::min(
        value.ghosts.requestedCount,
        profileCap(value.ghosts.profile)
    );
    value.ghosts.olderOpacityMin = clampByte(raw.olderOpacityMin);
    value.ghosts.olderOpacityMax = clampByte(raw.olderOpacityMax);
    value.ghosts.ageFadeStrength = finiteFloat(raw.ageFadeStrength, 1.0, 0.0, 2.0);
    value.ghosts.priorityXray = raw.priorityXray;
    value.ghosts.lastEnabled = raw.lastEnabled;
    value.ghosts.lastColor = raw.lastColor;
    value.ghosts.lastOpacity = clampByte(raw.lastOpacity);
    value.ghosts.lastTrail = raw.lastTrail;
    value.ghosts.bestEnabled = raw.bestEnabled;
    value.ghosts.bestColor = raw.bestColor;
    value.ghosts.bestOpacity = clampByte(raw.bestOpacity);
    value.ghosts.bestTrail = raw.bestTrail;
    value.ghosts.trailSeconds = finiteFloat(raw.trailSeconds, 0.55, 0.05, 2.0);
    value.ghosts.trailWidth = finiteFloat(raw.trailWidth, 1.8, 0.5, 6.0);
    value.ghosts.trailOpacity = clampByte(raw.trailOpacity);

    value.deaths.markers = raw.deathMarkers;
    value.deaths.markerScale = finiteFloat(raw.deathMarkerScale, 1.0, 0.5, 3.0);
    value.deaths.labels = raw.deathLabels;
    value.deaths.xray = raw.deathXray;
    value.deaths.heatStrip = raw.heatStrip;
    value.deaths.heatStripOpacity = clampByte(raw.heatStripOpacity);

    value.replay.defaultPlaybackRate = parsePlaybackRate(raw.defaultPlaybackRate);
    value.replay.defaultCamera = parseReplayCamera(raw.defaultCameraMode);

    value.storage.replayRetention = static_cast<std::size_t>(std::clamp<std::int64_t>(
        raw.replayRetention,
        static_cast<std::int64_t>(kMinReplayRetention),
        static_cast<std::int64_t>(kMaxReplayRetention)
    ));
    value.storage.diskBudgetMb = static_cast<std::size_t>(std::clamp<std::int64_t>(
        raw.diskBudgetMb,
        static_cast<std::int64_t>(kMinDiskBudgetMb),
        static_cast<std::int64_t>(kMaxDiskBudgetMb)
    ));

    value.recorderSampleRateHz = static_cast<double>(std::clamp<std::int64_t>(
        raw.recorderSampleRate,
        30,
        240
    ));
    value.renderingQuality = parseRenderingQuality(raw.renderingQuality);
    value.diagnostics = raw.diagnostics;

    return value;
}

EchoSettingsDiff diffEchoSettings(
    EchoSettingsSnapshot const& oldValue,
    EchoSettingsSnapshot const& newValue
) {
    EchoSettingsDiff diff;

    diff.fleetStructure =
        oldValue.ghosts.requestedCount != newValue.ghosts.requestedCount ||
        oldValue.ghosts.effectiveCount != newValue.ghosts.effectiveCount ||
        oldValue.ghosts.profile != newValue.ghosts.profile;

    diff.presentation =
        ghostPresentationChanged(oldValue.ghosts, newValue.ghosts) ||
        oldValue.deaths != newValue.deaths ||
        oldValue.renderingQuality != newValue.renderingQuality;

    diff.recorderPolicy =
        oldValue.recorderSampleRateHz != newValue.recorderSampleRateHz;
    diff.persistencePolicy = oldValue.storage != newValue.storage;
    diff.replayDefaults = oldValue.replay != newValue.replay;
    diff.diagnostics = oldValue.diagnostics != newValue.diagnostics;

    return diff;
}

} // namespace dash_echo
