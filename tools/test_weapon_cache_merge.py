"""tools/weapon_cache/merge_donors.py: dedup, collisions and externals on small built packs, and on three real
shipped packs when the set is on this machine (AE_PACK_SET, default: none, the test skips)."""
import os
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "weapon_pack"))
sys.path.insert(0, os.path.join(HERE, "weapon_cache"))
import aepack as ap  # noqa: E402
import merge_donors as md  # noqa: E402

BIT = ap.group("bitm")
WEAP = ap.group("weap")


def make(pack_id, prefix, shared, own_bytes):
    tags = [ap.Tag(WEAP, prefix + "gun", bytes=own_bytes),
            ap.Tag(BIT, prefix + "shared", bytes=shared),
            ap.Tag(BIT, "weapons\\stock\\ext", kind=ap.EXTERNAL, bytes=b"")]
    weapon = ap.Weapon("k_" + pack_id, 0, "ar", "GUN", weapon_pickup_text="gun")
    return ap.build(ap.Pack(pack_id, 1, pack_id.upper(), tags, [weapon]))


class MergeTest(unittest.TestCase):
    def test_synthetic(self):
        with tempfile.TemporaryDirectory() as d:
            paths = []
            for n, own in (("a", b"A" * 64), ("b", b"B" * 64)):
                path = os.path.join(d, f"ae_{n}.aepack")
                with open(path, "wb") as f:
                    f.write(make(f"ae_{n}", f"ae\\packs\\{n}\\", b"S" * 256, own))
                paths.append(path)
            m = md.merge(paths)
        s = m["summary"]
        self.assertEqual(s["packs"], 2)
        self.assertEqual(s["content_dup_groups"], 1)
        self.assertEqual(s["content_dup_saving_bytes"], 256)
        self.assertEqual(s["external_names"], 1)
        self.assertEqual(s["unique_payload_bytes"], s["sum_payload_bytes"] - 256)

    def test_real_three(self):
        root = os.environ.get("AE_PACK_SET", "pack-set-not-given")
        paths = [os.path.join(root, f"ae_{n}.aepack") for n in ("smg", "smg_silenced", "carbine")]
        if not all(os.path.exists(p) for p in paths):
            self.skipTest("pack set not on this machine")
        m = md.merge(paths)
        s = m["summary"]
        self.assertEqual(s["packs"], 3)
        self.assertLess(s["unique_payload_bytes"], s["sum_payload_bytes"])
        self.assertEqual(s["content_dup_saving_bytes"], s["sum_payload_bytes"] - s["unique_payload_bytes"])


if __name__ == "__main__":
    unittest.main()
