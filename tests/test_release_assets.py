import hashlib
import struct
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APPROVED_LOGO_SHA256 = "f70c08eeeb9b76fe6672ad00aaea351073b44a5d5ecd0485b33a9ced3cb51625"


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
