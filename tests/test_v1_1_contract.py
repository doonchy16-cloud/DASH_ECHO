import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class EchoDashV112Contract(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_branding_version_and_legacy_id(self):
        metadata = json.loads(self.read("mod.json"))
        self.assertEqual(metadata["name"], "ECHO_DASH")
        self.assertEqual(metadata["version"], "v1.1.2")
        self.assertEqual(metadata["id"], "doonchy.dash-echo")
        self.assertIn("VERSION 1.1.2", self.read("CMakeLists.txt"))

    def test_ghost_count_supports_256(self):
        metadata = json.loads(self.read("mod.json"))
        ghost_count = metadata["settings"]["ghost-count"]
        self.assertEqual(ghost_count["min"], 0)
        self.assertGreaterEqual(ghost_count["max"], 256)
        self.assertGreaterEqual(ghost_count["default"], 8)

    def test_priority_identity_is_trail_only(self):
        settings = json.loads(self.read("mod.json"))["settings"]
        self.assertEqual(settings["last-ghost-color"]["default"].lower(), "#4aa3ff")
        self.assertEqual(settings["best-ghost-color"]["default"].lower(), "#ffd54a")
        self.assertTrue(settings["last-ghost-trail"]["default"])
        self.assertTrue(settings["best-ghost-trail"]["default"])
        self.assertNotIn("last-ghost-aura", settings)
        self.assertNotIn("last-ghost-aura-size", settings)
        self.assertNotIn("best-ghost-aura", settings)
        self.assertNotIn("best-ghost-aura-size", settings)

        ghost_header = self.read("src/EchoGhost.hpp")
        fleet_header = self.read("src/EchoGhostFleet.hpp")
        self.assertNotIn("GhostAuraStyle", ghost_header)
        self.assertNotIn("setAuraStyle", ghost_header)
        self.assertNotIn("lastAura", fleet_header)
        self.assertNotIn("bestAura", fleet_header)

    def test_all_ghosts_use_one_role_agnostic_playback_engine(self):
        engine_h = self.read("src/EchoGhostPlaybackEngine.hpp")
        engine_cpp = self.read("src/EchoGhostPlaybackEngine.cpp")
        fleet_h = self.read("src/EchoGhostFleet.hpp")
        fleet_cpp = self.read("src/EchoGhostFleet.cpp")

        self.assertIn("class EchoGhostPlaybackEngine", engine_h)
        self.assertIn("GhostPlaybackPhase", engine_h)
        self.assertIn("Tracking", engine_h)
        self.assertIn("Continuing", engine_h)
        self.assertNotIn("GhostRole", engine_h + engine_cpp)
        self.assertIn("EchoGhostPlaybackEngine m_playbackEngine", fleet_h)
        self.assertIn("beginContinuation", fleet_h)
        self.assertIn("advanceContinuation", fleet_h)
        self.assertIn("continuationComplete", fleet_h)
        self.assertIn("renderFromPlaybackEngine", fleet_cpp)
        self.assertNotIn("alignBestIdentity", fleet_cpp)
        self.assertNotIn("carriesBestIdentity", fleet_cpp)
        self.assertNotIn("timeForProgress", fleet_h)
        self.assertNotIn("progressIsMonotonic", fleet_h)

        render_match = re.search(
            r"void EchoGhostFleet::renderFromPlaybackEngine\(\).*?\n}",
            fleet_cpp,
            re.S,
        )
        self.assertIsNotNone(render_match)
        self.assertNotIn("GhostRole", render_match.group(0))
        self.assertIn("m_playbackEngine.resolveTime", render_match.group(0))

    def test_confirmed_death_defers_reset_until_all_ghosts_finish(self):
        main = self.read("src/main.cpp")
        self.assertIn("confirmedDeath", main)
        self.assertIn("deferredResetRequested", main)
        self.assertIn("beginContinuation", main)
        self.assertIn("advanceContinuation", main)
        self.assertIn("continuationComplete", main)
        self.assertIn("performResetLifecycle", main)
        self.assertRegex(
            main,
            r"if \(m_fields->fleet\.isContinuing\(\)\)\s*\{\s*m_fields->deferredResetRequested = true;\s*return;\s*}",
        )
        self.assertRegex(
            main,
            r"if \(\s*m_fields->deferredResetRequested\s*&&\s*m_fields->fleet\.continuationComplete\(\)\s*\)\s*\{\s*performResetLifecycle\(\);",
        )

    def test_settings_refresh_remains_live_while_replay_studio_is_open(self):
        main = self.read("src/main.cpp")
        post = re.search(
            r"void postUpdate\(float dt\)\s*\{(?P<body>.*?)\n\s*}\n\n\s*void destroyPlayer",
            main,
            re.S,
        )
        self.assertIsNotNone(post)
        body = post.group("body")
        poll = body.find("applyEchoDashSettings(false)")
        studio_early_return = body.find("if (m_fields->replayStudioOpen)")
        self.assertGreaterEqual(poll, 0)
        self.assertGreaterEqual(studio_early_return, 0)
        self.assertLess(poll, studio_early_return)

    def test_archive_retains_known_good_backup_and_recovers_from_it(self):
        header = self.read("src/EchoReplayArchive.hpp")
        cpp = self.read("src/EchoReplayArchive.cpp")

        self.assertIn("recoveredFromBackup", header)
        self.assertIn("quarantinedReplayCount", header)
        self.assertIn("loadCandidate", header)
        self.assertIn("backupPath", header)
        self.assertIn("validateReplay", header)

        self.assertIn("EchoReplayArchive::loadCandidate", cpp)
        self.assertIn("EchoReplayArchive::backupPath", cpp)
        self.assertRegex(
            cpp,
            r"(?s)bool EchoReplayArchive::load\(.*?loadCandidate\(path.*?loadCandidate\(backup",
        )
        self.assertRegex(cpp, r"m_recoveredFromBackup\s*=\s*true")

        save = re.search(
            r"bool EchoReplayArchive::save\(\)\s*\{(?P<body>.*?)\n}\n\nvoid EchoReplayArchive::clear",
            cpp,
            re.S,
        )
        self.assertIsNotNone(save)
        save_body = save.group("body")
        self.assertIn("validateCandidateFile", save_body)
        self.assertNotRegex(save_body, r"remove\(backup")

    def test_archive_semantically_validates_and_quarantines_bad_replays(self):
        header = self.read("src/EchoReplayArchive.hpp")
        cpp = self.read("src/EchoReplayArchive.cpp")

        self.assertIn("validateReplay", header)
        self.assertIn("validateFrame", header)
        self.assertIn("validateSummary", header)
        self.assertIn("m_quarantinedReplayCount", header)

        self.assertIn("std::isfinite(frame.timeSeconds)", cpp)
        self.assertIn("std::isfinite(frame.progressPercent)", cpp)
        self.assertIn("frame.timeSeconds < previousTime", cpp)
        self.assertIn("frame.sequence <= previousSequence", cpp)
        self.assertIn("validatePlayerSnapshot", cpp)
        self.assertIn("validateCameraSnapshot", cpp)
        self.assertRegex(
            cpp,
            r"(?s)validateReplay\(attempt\).*?replays\.push_back.*?else.*?quarantinedReplayCount",
        )

    def test_release_surfaces_are_all_v112(self):
        main = self.read("src/main.cpp")
        workflow = self.read(".github/workflows/build-v1.yml")

        self.assertRegex(main, r'kReleaseVersion\s*=\s*"v1\.1\.2"')
        self.assertIn("ECHO_DASH 1.1.2 |", main)
        self.assertIn("name: ECHO_DASH v1.1.2 Build", workflow)
        self.assertIn("Run v1.1.2 contract regression tests", workflow)
        self.assertIn("ECHO-DASH-v1.1.2-compiler-evidence", workflow)
        self.assertIn("ECHO-DASH-v1.1.2-windows", workflow)
        self.assertIn("Upload v1.1.2 candidate", workflow)

    def test_fleet_is_dynamic_and_supports_256(self):
        header = self.read("src/EchoGhostFleet.hpp")
        self.assertNotIn("std::array<Slot, kMaxGhosts>", header)
        max_match = re.search(r"kMaxGhosts\s*=\s*(\d+)", header)
        self.assertIsNotNone(max_match)
        self.assertGreaterEqual(int(max_match.group(1)), 256)
        self.assertRegex(header, r"std::vector<.*Slot")

    def test_pause_menu_is_the_replay_entrypoint(self):
        main = self.read("src/main.cpp")
        controls_h = self.read("src/EchoReplayControls.hpp")
        controls_cpp = self.read("src/EchoReplayControls.cpp")
        self.assertIn("<Geode/modify/PauseLayer.hpp>", main)
        self.assertIn("left-button-menu", main)
        self.assertIn("openStudio", controls_h)
        self.assertNotIn("m_launcher", controls_h)
        self.assertNotIn("REPLAY READY", controls_cpp)

    def test_first_attempt_uses_explicit_same_lifecycle_as_later_attempts(self):
        main = self.read("src/main.cpp")
        self.assertIn("startNewAttempt", main)
        init_match = re.search(
            r"bool init\(GJGameLevel\* level, bool useReplay, bool dontCreateObjects\).*?\n\s*}",
            main,
            re.S,
        )
        self.assertIsNotNone(init_match)
        self.assertIn("startNewAttempt", init_match.group(0))
        self.assertIn("captureEventFrame", main)

    def test_archive_is_configured_to_keep_all_runs_until_storage_budget(self):
        settings = json.loads(self.read("mod.json"))["settings"]
        retention = settings["replay-retention"]
        self.assertGreaterEqual(retention["default"], 10000)
        self.assertGreaterEqual(retention["max"], 10000)
        archive = self.read("src/EchoReplayArchive.hpp")
        hard_match = re.search(r"kHardMaxReplays\s*=\s*([0-9'_,]+)", archive)
        self.assertIsNotNone(hard_match)
        hard = int(re.sub(r"[^0-9]", "", hard_match.group(1)))
        self.assertGreaterEqual(hard, 10000)

    def test_archive_and_heatmap_sources_exist(self):
        for path in (
            "src/EchoReplayArchive.hpp",
            "src/EchoReplayArchive.cpp",
            "src/EchoHeatmapOverlay.hpp",
            "src/EchoHeatmapOverlay.cpp",
        ):
            self.assertTrue((ROOT / path).is_file(), path)

    def test_recorder_has_configurable_sampling_and_event_capture(self):
        header = self.read("src/EchoRecorder.hpp")
        self.assertIn("setCaptureSampleRate", header)
        self.assertIn("captureSampleRate", header)
        self.assertIn("captureEventFrame", header)

    def test_release_strings_do_not_claim_old_runtime_version(self):
        stale = []
        for path in (ROOT / "src").glob("*.cpp"):
            text = path.read_text(encoding="utf-8")
            if any(value in text for value in (
                "DASH ECHO v0.9",
                "DASH ECHO v1.0",
                "ECHO_DASH v1.1.0",
                "ECHO_DASH v1.1.1",
            )):
                stale.append(path.name)
        self.assertEqual(stale, [])

    def test_about_and_readme_are_v112(self):
        combined = self.read("README.md") + "\n" + self.read("about.md")
        self.assertIn("ECHO_DASH", combined)
        self.assertIn("v1.1.2", combined)
        self.assertIn("pause", combined.lower())
        self.assertIn("trail", combined.lower())
        self.assertIn("backup", combined.lower())


if __name__ == "__main__":
    unittest.main()
