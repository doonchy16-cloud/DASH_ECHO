import hashlib
import struct
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APPROVED_LOGO_SHA256 = "ecf4014d5fa210759c40fc4ae85389beaf7bd667e0afe169f7e1eb924c9a02c2"


class EchoDashReleaseAssetContract(unittest.TestCase):
    def test_approved_logo_is_source_authority(self):
        logo = ROOT / "logo.png"
        self.assertTrue(logo.is_file(), "approved ECHO_DASH logo.png must live at repository root")
        data = logo.read_bytes()
        self.assertTrue(data.startswith(b"\x89PNG\r\n\x1a\n"), "logo.png must be a PNG")
        self.assertEqual(hashlib.sha256(data).hexdigest(), APPROVED_LOGO_SHA256)
        width, height = struct.unpack(">II", data[16:24])
        self.assertEqual((width, height), (128, 128))


if __name__ == "__main__":
    unittest.main()
