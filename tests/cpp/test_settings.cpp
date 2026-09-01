#include "TestHarness.hpp"
#include "EchoSettings.hpp"

#include <limits>

namespace {

bool onlyRecorderPolicy(dash_echo::EchoSettingsDiff const& diff) {
    return !diff.presentation && !diff.fleetStructure && diff.recorderPolicy &&
        !diff.persistencePolicy && !diff.replayDefaults && !diff.diagnostics;
}

bool onlyFleetStructure(dash_echo::EchoSettingsDiff const& diff) {
    return !diff.presentation && diff.fleetStructure && !diff.recorderPolicy &&
        !diff.persistencePolicy && !diff.replayDefaults && !diff.diagnostics;
}

bool onlyPresentation(dash_echo::EchoSettingsDiff const& diff) {
    return diff.presentation && !diff.fleetStructure && !diff.recorderPolicy &&
        !diff.persistencePolicy && !diff.replayDefaults && !diff.diagnostics;
}

bool onlyPersistencePolicy(dash_echo::EchoSettingsDiff const& diff) {
    return !diff.presentation && !diff.fleetStructure && !diff.recorderPolicy &&
        diff.persistencePolicy && !diff.replayDefaults && !diff.diagnostics;
}

bool onlyReplayDefaults(dash_echo::EchoSettingsDiff const& diff) {
    return !diff.presentation && !diff.fleetStructure && !diff.recorderPolicy &&
        !diff.persistencePolicy && diff.replayDefaults && !diff.diagnostics;
}

bool onlyDiagnostics(dash_echo::EchoSettingsDiff const& diff) {
    return !diff.presentation && !diff.fleetStructure && !diff.recorderPolicy &&
        !diff.persistencePolicy && !diff.replayDefaults && diff.diagnostics;
}

} // namespace

ECHO_TEST(settings_normalization_clamps_and_restores_documented_defaults) {
    dash_echo::RawEchoSettings raw;
    raw.ghostCount = 999;
    raw.visualProfile = "Competitive";
    raw.olderOpacityMin = -10;
    raw.olderOpacityMax = 900;
    raw.ageFadeStrength = std::numeric_limits<double>::quiet_NaN();
    raw.defaultPlaybackRate = "invalid";
    raw.defaultCameraMode = "invalid";
    raw.recorderSampleRate = 999;
    raw.replayRetention = 1;
    raw.diskBudgetMb = 99'999;
    raw.renderingQuality = "invalid";

    auto const value = dash_echo::normalizeEchoSettings(raw);

    ECHO_CHECK(value.ghosts.requestedCount == 256);
    ECHO_CHECK(value.ghosts.effectiveCount == 8);
    ECHO_CHECK(value.ghosts.profile == dash_echo::VisualProfile::Competitive);
    ECHO_CHECK(value.ghosts.olderOpacityMin == 0);
    ECHO_CHECK(value.ghosts.olderOpacityMax == 255);
    ECHO_CHECK(value.ghosts.ageFadeStrength == 1.0f);
    ECHO_CHECK(value.replay.defaultPlaybackRate == 1.0f);
    ECHO_CHECK(value.replay.defaultCamera == dash_echo::ReplayCameraSetting::Recorded);
    ECHO_CHECK(value.recorderSampleRateHz == 240.0);
    ECHO_CHECK(value.storage.replayRetention == 256);
    ECHO_CHECK(value.storage.diskBudgetMb == 8192);
    ECHO_CHECK(value.renderingQuality == dash_echo::RenderingQuality::Auto);
}

ECHO_TEST(settings_profile_caps_preserve_existing_ghost_limits) {
    dash_echo::RawEchoSettings raw;
    raw.ghostCount = 256;

    raw.visualProfile = "Clean";
    ECHO_CHECK(dash_echo::normalizeEchoSettings(raw).ghosts.effectiveCount == 2);

    raw.visualProfile = "Competitive";
    ECHO_CHECK(dash_echo::normalizeEchoSettings(raw).ghosts.effectiveCount == 8);

    raw.visualProfile = "Multiverse";
    ECHO_CHECK(dash_echo::normalizeEchoSettings(raw).ghosts.effectiveCount == 64);

    raw.visualProfile = "Chaos";
    ECHO_CHECK(dash_echo::normalizeEchoSettings(raw).ghosts.effectiveCount == 256);

    raw.visualProfile = "Custom";
    ECHO_CHECK(dash_echo::normalizeEchoSettings(raw).ghosts.effectiveCount == 256);

    raw.visualProfile = "unknown";
    auto const fallback = dash_echo::normalizeEchoSettings(raw);
    ECHO_CHECK(fallback.ghosts.profile == dash_echo::VisualProfile::Multiverse);
    ECHO_CHECK(fallback.ghosts.effectiveCount == 64);
}

ECHO_TEST(settings_performance_quality_decodes_exactly) {
    dash_echo::RawEchoSettings raw;
    raw.renderingQuality = "Performance";
    ECHO_CHECK(
        dash_echo::normalizeEchoSettings(raw).renderingQuality ==
        dash_echo::RenderingQuality::Performance
    );
}

ECHO_TEST(settings_diff_classifies_each_authority_boundary) {
    dash_echo::RawEchoSettings raw;
    auto const baseline = dash_echo::normalizeEchoSettings(raw);

    auto changed = raw;
    changed.recorderSampleRate = 60;
    ECHO_CHECK(onlyRecorderPolicy(
        dash_echo::diffEchoSettings(baseline, dash_echo::normalizeEchoSettings(changed))
    ));

    changed = raw;
    changed.ghostCount = 32;
    ECHO_CHECK(onlyFleetStructure(
        dash_echo::diffEchoSettings(baseline, dash_echo::normalizeEchoSettings(changed))
    ));

    changed = raw;
    changed.visualProfile = "Competitive";
    ECHO_CHECK(onlyFleetStructure(
        dash_echo::diffEchoSettings(baseline, dash_echo::normalizeEchoSettings(changed))
    ));

    changed = raw;
    changed.lastOpacity = 101;
    ECHO_CHECK(onlyPresentation(
        dash_echo::diffEchoSettings(baseline, dash_echo::normalizeEchoSettings(changed))
    ));

    changed = raw;
    changed.deathMarkerScale = 1.5;
    ECHO_CHECK(onlyPresentation(
        dash_echo::diffEchoSettings(baseline, dash_echo::normalizeEchoSettings(changed))
    ));

    changed = raw;
    changed.renderingQuality = "Balanced";
    ECHO_CHECK(onlyPresentation(
        dash_echo::diffEchoSettings(baseline, dash_echo::normalizeEchoSettings(changed))
    ));

    changed = raw;
    changed.replayRetention = 2'000;
    ECHO_CHECK(onlyPersistencePolicy(
        dash_echo::diffEchoSettings(baseline, dash_echo::normalizeEchoSettings(changed))
    ));

    changed = raw;
    changed.defaultPlaybackRate = "0.50";
    ECHO_CHECK(onlyReplayDefaults(
        dash_echo::diffEchoSettings(baseline, dash_echo::normalizeEchoSettings(changed))
    ));

    changed = raw;
    changed.diagnostics = true;
    ECHO_CHECK(onlyDiagnostics(
        dash_echo::diffEchoSettings(baseline, dash_echo::normalizeEchoSettings(changed))
    ));
}

ECHO_TEST(settings_diff_is_empty_for_equal_normalized_snapshots) {
    dash_echo::RawEchoSettings raw;
    auto const value = dash_echo::normalizeEchoSettings(raw);
    auto const diff = dash_echo::diffEchoSettings(value, value);

    ECHO_CHECK(!diff.presentation);
    ECHO_CHECK(!diff.fleetStructure);
    ECHO_CHECK(!diff.recorderPolicy);
    ECHO_CHECK(!diff.persistencePolicy);
    ECHO_CHECK(!diff.replayDefaults);
    ECHO_CHECK(!diff.diagnostics);
}
