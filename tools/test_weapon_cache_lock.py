"""The committed cache lock: header PRESENT 1 and in step with tools/weapon_cache/cache.lock (release-kit check)."""
import json
import sys
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "weapon_cache"))
import check_release_lock  # noqa: E402
import lock_header  # noqa: E402

HEADER = HERE.parent / "port" / "linux" / "game" / "ae_weapon_cache_lock.h"


class CacheLock(unittest.TestCase):
    def test_header_is_present_and_matches_the_lock(self):
        text = HEADER.read_text()
        self.assertIn("#define AE_WEAPON_CACHE_LOCK_PRESENT 1", text)
        lock = json.loads((HERE / "weapon_cache" / "cache.lock").read_text())
        self.assertEqual(text, lock_header.render(lock))
        self.assertEqual(len(bytes.fromhex(lock["cache_sha256"])), 32)

    def test_a_missing_or_wrong_cache_is_refused(self):
        import tempfile
        with tempfile.TemporaryDirectory() as folder:
            wrong = Path(folder) / "x.aecache"
            wrong.write_bytes(b"not a cache" * 20)
            self.assertIsNotNone(check_release_lock.check(wrong))


if __name__ == "__main__":
    unittest.main()
