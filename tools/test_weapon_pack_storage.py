"""The weapon pack's gametype bytes (universal_variant.weapon_pack / pack_loadout, offsets 0x29 / 0x2b; pack_loadout's
bit 4 is BATTLE RIFLE ON MAP, PACK_MAP_SPAWNS_BIT; 0x2a between them is pad5, not the pack's): how the game decodes
the edge values, read from a played game's rules line.

Needs a game: set AE_TEST_PACK_BUILD (a build for tools/ae_test/run.py: a revision, name or folder) and
AE_TEST_PACK_SAVE (a seeded save root, e.g. one a game left after --saved-gametype), and a config for the
machine that plays (docs/testing.md); skipped otherwise. Each case copies the save root, patches "AE TRAINING"
and plays 25 s of Blood Gulch."""
import os
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "ae_test"))
import gametype_file  # noqa: E402
import run  # noqa: E402

BUILD = os.environ.get("AE_TEST_PACK_BUILD")
SAVE = os.environ.get("AE_TEST_PACK_SAVE")

LINE = "weapon pack: %s, pack loadout: primary %s, secondary %s, battle rifle on map: %s"

# (offset values, the rules line's pack fields): weapon_pack 2-127 is reserved, 128 and over none; with the pack
# none the loadout and on-map fields read stock / off whatever their bits; a loadout source of 2 or 3 is stock;
# only bit 4 of pack_loadout is BATTLE RIFLE ON MAP (bits 5-7 are not)
MAP = gametype_file.PACK_MAP_SPAWNS_BIT
CASES = [
    ({"weapon_pack": 1, "pack_loadout": 1 | MAP}, ("battle rifle \\(slot 1\\)", "pack", "stock", "on")),
    ({"weapon_pack": 1, "pack_loadout": 4 | 0x20}, ("battle rifle \\(slot 1\\)", "stock", "pack", "off")),
    ({"weapon_pack": 1, "pack_loadout": 10}, ("battle rifle \\(slot 1\\)", "stock", "stock", "off")),
    ({"weapon_pack": 1, "pack_loadout": 15}, ("battle rifle \\(slot 1\\)", "stock", "stock", "off")),
    ({"weapon_pack": 2, "pack_loadout": 5 | MAP}, ("reserved", "pack", "pack", "on")),
    ({"weapon_pack": 128, "pack_loadout": 5 | MAP}, ("none", "stock", "stock", "off")),
    ({"weapon_pack": 200, "pack_loadout": 10}, ("none", "stock", "stock", "off")),
]


@unittest.skipUnless(BUILD and SAVE, "AE_TEST_PACK_BUILD / AE_TEST_PACK_SAVE not set (needs a game)")
class PackEdges(unittest.TestCase):
    def test_edge_values(self):
        for values, want in CASES:
            with self.subTest(values=values):
                with tempfile.TemporaryDirectory(dir=Path.home()) as d:
                    root = Path(d) / "save"
                    named = {gametype_file.NAMED[k]: v for k, v in values.items()}
                    gametype_file.copy_and_patch(SAVE, root, "AE TRAINING", named)
                    rc = run.main(["--build", BUILD, "--map", "bloodgulch", "--exit-after", "25", "--saved-gametype",
                                   "AE TRAINING", "--save-from", str(root), "--expect-line", LINE % want,
                                   "--name", "packedge", "--out-name", "packedge"])
                    shutil.rmtree(Path(d), ignore_errors=True)
                self.assertEqual(rc, 0)


if __name__ == "__main__":
    unittest.main()
