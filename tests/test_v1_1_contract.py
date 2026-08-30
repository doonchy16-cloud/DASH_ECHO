import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class EchoDashV110Contract(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_branding_version_and_legacy_id(self):
        metadata = json.loads(self.read("mod.json"))
        self.assertEqual(metadata["name"], "ECHO_DASH")
        self.assertEqual(metadata["version"], "v1.1.0")
        self.assertEqual(metadata["id"], "doonchy.dash-echo")

    def test_ghost_count_supports_256(self):
        metadata = json.loads(self.read("mod.json"))
        ghost_count = metadata["settings"]["ghost-count"]
        self.assertEqual(ghost_count["min"], 0)
        self.assertGreaterEqual(ghost_count["max"], 256)
        self.assertGreaterEqual(ghost_count["default"], 8)

    def test_priority_identity_defaults_exist(self):
        settings = json.loads(self.read("mod.json"))["settings"]
        self.assertEqual(settings["last-ghost-color"]["default"].lower(), "#4aa3ff")
        self.assertEqual(settings["best-ghost-color"]["default"].lower(), "#ffd54a")
        self.assertTrue(settings["last-ghost-aura"]["default"])
        self.assertTrue(settings["best-ghost-aura"]["default"])
        self.assertIn("recorder-sample-rate", settings)
        self.assertIn("heat-strip", settings)
        self.assertIn("replay-retention", settings)
        self.assertIn("disk-budget-mb", settings)

    def test_fleet_is_dynamic_and_supports_256(self):
        header = self.read("src/EchoGhostFleet.hpp")
        self.assertNotIn("std::array<Slot, kMaxGhosts>", header)
        max_match = re.search(r"kMaxGhosts\s*=\s*(\d+)", header)
        self.assertIsNotNone(max_match)
        self.assertGreaterEqual(int(max_match.group(1)), 256)
        self.assertRegex(header, r"std::vector<.*Slot")

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

    def test_release_strings_do_not_claim_v09(self):
        stale = []
        for path in (ROOT / "src").glob("*.cpp"):
            text = path.read_text(encoding="utf-8")
            if "DASH ECHO v0.9" in text or "DASH ECHO v1.0" in text:
                stale.append(path.name)
        self.assertEqual(stale, [])

    def test_about_and_readme_are_v11(self):
        combined = self.read("README.md") + "\n" + self.read("about.md")
        self.assertIn("ECHO_DASH", combined)
        self.assertIn("v1.1.0", combined)
        self.assertNotIn("Current mod version: **v0.9.0**", combined)


if __name__ == "__main__":
    unittest.main()
