import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class EchoDashV111Contract(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_branding_version_and_legacy_id(self):
        metadata = json.loads(self.read("mod.json"))
        self.assertEqual(metadata["name"], "ECHO_DASH")
        self.assertEqual(metadata["version"], "v1.1.1")
        self.assertEqual(metadata["id"], "doonchy.dash-echo")

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

    def test_best_recorded_echo_has_progress_alignment_path(self):
        fleet_header = self.read("src/EchoGhostFleet.hpp")
        fleet_cpp = self.read("src/EchoGhostFleet.cpp")
        main = self.read("src/main.cpp")

        self.assertRegex(
            fleet_header,
            r"void synchronize\(\s*double timeSeconds,\s*float progressPercent,\s*bool progressAlignmentEnabled\s*\)",
        )
        self.assertIn("progressAlignmentSafe", fleet_header)
        self.assertIn("timeForProgress", fleet_header)
        self.assertIn("GhostRole::BestRecorded", fleet_cpp)
        self.assertIn("progressAlignmentEnabled", fleet_cpp)
        self.assertIn("timeForProgress", fleet_cpp)
        self.assertRegex(
            fleet_cpp,
            r"bool const carriesBestIdentity\s*=\s*slot\.role == GhostRole::BestRecorded\s*\|\|\s*slot\.role == GhostRole::LastAndBest\s*;",
        )
        self.assertRegex(
            fleet_cpp,
            r"bool const alignBestIdentity\s*=\s*progressAlignmentEnabled.*?carriesBestIdentity.*?slot\.progressAlignmentSafe.*?slot\.attempt\s*;",
        )
        self.assertRegex(
            main,
            r"fleet\.synchronize\(\s*m_fields->recorder\.activeElapsedSeconds\(\),\s*this->getCurrentPercent\(\)",
        )

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
            )):
                stale.append(path.name)
        self.assertEqual(stale, [])

    def test_about_and_readme_are_v111(self):
        combined = self.read("README.md") + "\n" + self.read("about.md")
        self.assertIn("ECHO_DASH", combined)
        self.assertIn("v1.1.1", combined)
        self.assertIn("pause", combined.lower())
        self.assertIn("trail", combined.lower())


if __name__ == "__main__":
    unittest.main()
