import hashlib
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APPROVED_LOGO_SHA256 = "15a5310c7a22f0b0c1d20899add0a09265eaa98eb94e9759258d06d801dde14f"


class EchoDashReleaseAssetContract(unittest.TestCase):
    def test_approved_logo_is_source_authority(self):
        logo = ROOT / "logo.png"
        self.assertTrue(logo.is_file(), "approved ECHO_DASH logo.png must live at repository root")
        data = logo.read_bytes()
        self.assertTrue(data.startswith(b"\x89PNG\r\n\x1a\n"), "logo.png must be a PNG")
        self.assertEqual(hashlib.sha256(data).hexdigest(), APPROVED_LOGO_SHA256)


if __name__ == "__main__":
    unittest.main()
