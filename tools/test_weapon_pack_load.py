"""The weapon packs read at start (port/linux/game/ae_weapon_pack.c): slot 1's pack logged, a refused one logged
with its reason while the game plays on, and nothing at all from a game with no packs folder.

Needs a game: set AE_TEST_PACK_BUILD (a build for tools/ae_test/run.py: a revision, name or folder) and a config for
the machine that plays (docs/testing.md); skipped otherwise. The packs are synthetic, written here by
tools/weapon_pack/aepack.py (the format's Python writer): no game files. Each case plays 30 s of Blood Gulch."""
import json
import os
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS / "ae_test"))
sys.path.insert(0, str(TOOLS / "weapon_pack"))
import aepack  # noqa: E402
import run  # noqa: E402

BUILD = os.environ.get("AE_TEST_PACK_BUILD")

WEAPON = aepack.group("weap")
BITMAP = aepack.group("bitm")
SOUND = aepack.group("snd!")
ITEMS = aepack.group("itmc")
# a slot-1 line of any kind, accepted or refused (the rules line's "weapon pack: none" is neither)
ANY_PACK_LINE = r"weapon pack: ('|slot )"


def data(size, seed):
    return bytes((index * 13 + seed) & 0xFF for index in range(size))


def make_pack(pack_id="ae_battle_rifle", revision=3, resource_compression=aepack.STORED, resource_size=None):
    """a small pack in the shape of the battle rifle's: own tags (one with a 1.5 MiB resource), externals (one
    required), an item collection and the weapon entry battle_rifle"""
    tags = [
        aepack.Tag(WEAPON, "ae\\packs\\battle_rifle\\weapons\\battle_rifle\\battle_rifle", bytes=data(256, 1),
                   fixups=[aepack.Fixup(0x10, aepack.FIXUP_POINTER, 0x80),
                           aepack.Fixup(0x20, aepack.FIXUP_REFERENCE, 1),
                           aepack.Fixup(0x60, aepack.FIXUP_TAG_INDEX, 3)]),
        aepack.Tag(BITMAP, "ae\\packs\\battle_rifle\\hud\\counter", flags=aepack.FLAG_SMOOTH, bytes=data(0x60, 2),
                   resources=[aepack.Resource(0x1000, data(3 * 512 * 1024, 3))]),
        aepack.Tag(BITMAP, "ae\\packs\\battle_rifle\\hud\\scope", bytes=data(0x60, 4),
                   resources=[aepack.Resource(0x2000, data(5000, 5), resource_compression, size=resource_size)]),
        aepack.Tag(SOUND, "sound\\sfx\\weapons\\assault rifle\\fire", kind=aepack.EXTERNAL),
        aepack.Tag(BITMAP, "ui\\hud\\bitmaps\\combined\\hud_msg_icons", kind=aepack.EXTERNAL, required=1),
        aepack.Tag(ITEMS, "ae\\packs\\battle_rifle\\items\\battle_rifle", bytes=data(32, 6),
                   fixups=[aepack.Fixup(0, aepack.FIXUP_REFERENCE, 0)]),
    ]
    weapons = [aepack.Weapon("battle_rifle", 0, "h_br", "BATTLE RIFLE", item_collection_tag=5, behaviour=1,
                             fallback_labels=("ar", None), weapon_pickup_text="Picked up a battle rifle",
                             ammo_pickup_text="Picked up %d rounds for battle rifle", icon_bitmap_tag=4,
                             replaces=2)]
    return aepack.build(aepack.Pack(pack_id, revision, "BATTLE RIFLE", tags, weapons, credits="Test pack. No art.\n"))


def accepted_line(image):
    """the line slot 1's pack is logged with: 4 own tags, 2 external, 2 resources of 1,577,864 bytes (1.5 MB)"""
    sha = aepack.digest(image).hex()[:16]
    return (rf"weapon pack: 'ae_battle_rifle' revision 3 \(sha256 {sha}\.\.\.\), 4 own tags, 2 external, "
            r"2 resources \(1\.5 MB\), weapons: battle_rifle\r?$")


def refused_line(path, reason):
    return rf"weapon pack: slot 1 \({path}\) refused: {reason}; the game plays without it\r?$"


@unittest.skipUnless(BUILD, "AE_TEST_PACK_BUILD not set (needs a game)")
class PacksAtStart(unittest.TestCase):
    def play(self, name, *arguments, refused=False):
        """30 s of Blood Gulch: PASS (ticks >= 150, its expected lines); refused: a refusal line is expected (the
        harness counts each "refused" line, and fails a game that has one unless the spec allows it)"""
        spec = self.folder / f"{name}.json"
        spec.write_text(json.dumps({"expect": {"refusals_ok": True}} if refused else {}))
        rc = run.main(["--spec", str(spec), "--build", BUILD, "--map", "bloodgulch", "--exit-after", "30",
                       "--name", name, "--out-name", f"packload-{name}", *arguments])
        self.assertEqual(rc, 0, name)

    def setUp(self):
        self.folder = Path(tempfile.mkdtemp(dir=Path.home(), prefix="packload-"))
        self.addCleanup(shutil.rmtree, self.folder, True)

    def packs(self, name, image):
        folder = self.folder / name
        folder.mkdir()
        if image is not None:
            (folder / "ae_battle_rifle.aepack").write_bytes(image)
        return folder

    def test_slot_1_logged(self):
        image = make_pack()
        self.play("accepted", "--packs-from", str(self.packs("good", image)), "--expect-line", accepted_line(image),
                  "--expect-no-line", "weapon pack: slot ")

    def test_byte_flipped_debug_file_refused(self):
        """debug.weapon_pack_file is read in place of packs/ae_battle_rifle.aepack: a byte changed in its resources
        fails the SHA-256, the slot stays empty (the good pack beside it is not read) and the game plays"""
        image = bytearray(make_pack())
        image[aepack.section_offset(image, aepack.RESOURCE_DATA) + 100] ^= 0x01
        flipped = self.folder / "flipped.aepack"
        flipped.write_bytes(bytes(image))
        self.play("flipped", "--packs-from", str(self.packs("good", make_pack())), "--pack-file", str(flipped),
                  "--expect-line", refused_line(r"d:\\pack_file\\flipped\.aepack", "SHA-256 mismatch"),
                  "--expect-no-line", "weapon pack: '", refused=True)

    def test_no_packs_folder_logs_nothing(self):
        self.play("nopacks", "--expect-no-line", ANY_PACK_LINE)

    def test_empty_packs_folder(self):
        self.play("empty", "--packs-from", str(self.packs("empty", None)),
                  "--expect-line", refused_line(r"d:\\packs\\ae_battle_rifle\.aepack", "there is no such file"), refused=True)

    def test_another_pack_in_the_slot(self):
        self.play("otherid", "--packs-from", str(self.packs("other", make_pack(pack_id="ae_other"))),
                  "--expect-line", refused_line(r"d:\\packs\\ae_battle_rifle\.aepack",
                                                "it is pack 'ae_other', the slot takes 'ae_battle_rifle'"),
                  "--expect-no-line", "weapon pack: '", refused=True)

    def test_lying_resource_refused(self):
        """a zlib resource whose record claims more than its stream inflates to passes the open (resources are not
        inflated there) and is refused by the one read of every resource at start"""
        image = make_pack(resource_compression=aepack.ZLIB, resource_size=5016)
        self.play("lie", "--packs-from", str(self.packs("lie", image)),
                  "--expect-line", refused_line(r"d:\\packs\\ae_battle_rifle\.aepack",
                                                r"resource 1 of tag 2: inflated size lie or bad zlib stream"),
                  "--expect-no-line", "weapon pack: '", refused=True)


class TestPack(unittest.TestCase):
    """the synthetic packs themselves (no game): the good one is accepted by the format's reader, as the game must"""

    def test_good_pack_reads(self):
        parsed = aepack.read(make_pack())
        self.assertEqual(len(parsed.weapons), 1)

    def test_lying_pack_is_refused_only_by_its_resources(self):
        image = make_pack(resource_compression=aepack.ZLIB, resource_size=5016)
        aepack.read(image, verify_resources=False)
        with self.assertRaises(aepack.Refused):
            aepack.read(image)

    def test_sizes(self):
        self.assertEqual(3 * 512 * 1024 + 5000, 1577864)
        self.assertEqual((1577864 * 10 + 524288) // 1048576, 15)


if __name__ == "__main__":
    unittest.main()
