"""A weapon pack added to the map a game loads (port/linux/game/ae_weapon_pack.c): the pack's tags added when the
gametype asks for the pack, players starting with its weapon, the pack let go again as the map changes, and a
pack whose tags cannot be trusted refused for the map while the game plays on with the stock weapons.

Needs a game: AE_TEST_PACK_BUILD (a build for tools/ae_test/run.py) and a config for the machine that plays
(docs/testing.md). The cases marked "real pack" also need AE_TEST_PACK_REAL, the path of a real battle rifle pack
(tools/ae_pack_build's output: game art, never in this repository); its HOSTILE VARIANTS are made here in the
test's own folder by changing a few bytes of a copy and signing it again, so each one passes the pack reader
(the containers are sound) and must be caught when the pack is added to a map. Skipped otherwise."""
import json
import os
import re
import shutil
import struct
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS / "ae_test"))
sys.path.insert(0, str(TOOLS / "weapon_pack"))
sys.path.insert(0, str(TOOLS))
import aepack  # noqa: E402
import handshake  # noqa: E402
import harness  # noqa: E402
import run  # noqa: E402
import test_weapon_pack_load as load  # noqa: E402

BUILD = os.environ.get("AE_TEST_PACK_BUILD")
REAL = os.environ.get("AE_TEST_PACK_REAL")
# the long cases (the rotation's four one-minute games; a host and a client playing two games): left out unless
# asked for (AE_TEST_PACK_ROTATION=1)
ROTATION = os.environ.get("AE_TEST_PACK_ROTATION")

ADDED = r"weapon pack: added to {map}: \d+ tags new \(\d+ bytes\), \d+ the map's own, \d+ optional ones the map " \
        r"lacks, {corrections} corrections; its tag table \d+ to \d+; fingerprint (?P<fingerprint>[0-9a-f]{{16}})\r?$"
# (the real pack's tags need no correction: one would silently cut a reference or a value of the rifle)
NO_CORRECTIONS = "0"
LET_GO = r"weapon pack: let go from {map}: \d+ tags; [1-9]\d* bitmap and \d+ sound reads \(\d+ bytes\) were " \
         r"served from the pack, 0 could not be\r?$"
FIRST_SPAWN = r"weapon pack: first spawn: the {slot} weapon is the pack's battle_rifle \("
REFUSED = r"weapon pack: refused for bloodgulch: {reason}; the game plays without it\r?$"
ANY_ADDED = r"weapon pack: added to "
ANY_FIRST_SPAWN = r"weapon pack: first spawn"

# TAGS record fields ("<4I4B9I")
TAG_GROUP, TAG_NAME, TAG_KIND, TAG_REQUIRED = 0, 3, 4, 5
TAG_BYTES_OFFSET, TAG_BYTES_SIZE, TAG_FIXUPS_FIRST, TAG_FIXUPS_COUNT = 9, 10, 11, 12
TAG_PARTS_FIRST, TAG_PARTS_COUNT, TAG_RESOURCES_FIRST, TAG_RESOURCES_COUNT = 13, 14, 15, 16


class Image:
    """a pack's bytes, to change a few of (every section of a pack the builder writes is stored)"""

    def __init__(self, data):
        self.data = bytearray(data)
        self.sections = {entry[0]: entry[1] for entry in aepack.section_entries(self.data)}
        self.counts = {entry[0]: entry[3] for entry in aepack.section_entries(self.data)}
        assert all(entry[4] == aepack.STORED for entry in aepack.section_entries(self.data))

    def tag_count(self):
        return self.counts[aepack.TAGS] // aepack.TAG_RECORD.size

    def tag(self, index):
        return aepack.TAG_RECORD.unpack_from(self.data, self.sections[aepack.TAGS] + index * aepack.TAG_RECORD.size)

    def tag_set(self, index, field, value):
        record = list(self.tag(index))
        record[field] = value
        aepack.TAG_RECORD.pack_into(self.data, self.sections[aepack.TAGS] + index * aepack.TAG_RECORD.size, *record)

    def name(self, index):
        start = self.sections[aepack.STRINGS] + self.tag(index)[TAG_NAME]
        return bytes(self.data[start:self.data.index(b"\0", start)]).decode()

    def fixup_offset(self, index):
        return self.sections[aepack.FIXUPS] + index * aepack.FIXUP_RECORD.size

    def fixup(self, index):
        return aepack.FIXUP_RECORD.unpack_from(self.data, self.fixup_offset(index))

    def weapon_root(self):
        return aepack.WEAPON_RECORD.unpack_from(self.data, self.sections[aepack.WEAPONS])[1]

    def signed(self):
        aepack.rehash(self.data)
        # (what the engine's reader takes: the change must be one only the add can catch)
        aepack.read(bytes(self.data))
        return bytes(self.data)


def oversized_blocks(real):
    """every block and data of the weapon tag claims 16 million elements or bytes: its count is the long before
    each pointer the pack's fixups set (a block's count; a data's file offset, which nothing reads)"""
    image = Image(real)
    tag = image.tag(image.weapon_root())
    tag_bytes = image.sections[aepack.TAG_BYTES] + tag[TAG_BYTES_OFFSET]
    changed = 0
    for index in range(tag[TAG_FIXUPS_FIRST], tag[TAG_FIXUPS_FIRST] + tag[TAG_FIXUPS_COUNT]):
        offset, kind, _pad, _value = image.fixup(index)
        if kind == aepack.FIXUP_POINTER and offset >= 4:
            struct.pack_into("<I", image.data, tag_bytes + offset - 4, 0x00FFFFFF)
            changed += 1
    assert changed
    return image.signed()


def pointers_at_the_root(real):
    """every pointer fixup of the weapon tag points at the tag's own first byte: its blocks would lie over its
    root and over each other"""
    image = Image(real)
    tag = image.tag(image.weapon_root())
    changed = 0
    for index in range(tag[TAG_FIXUPS_FIRST], tag[TAG_FIXUPS_FIRST] + tag[TAG_FIXUPS_COUNT]):
        offset, kind, pad, _value = image.fixup(index)
        if kind == aepack.FIXUP_POINTER:
            aepack.FIXUP_RECORD.pack_into(image.data, image.fixup_offset(index), offset, kind, pad, 0)
            changed += 1
    assert changed
    return image.signed()


def external_of_another_group(real):
    """a required external tag is said to be of another group than the map's tag of that name is: (name, pack)"""
    image = Image(real)
    for index in range(image.tag_count()):
        tag = image.tag(index)
        if tag[TAG_KIND] == aepack.EXTERNAL and tag[TAG_REQUIRED] and tag[TAG_GROUP] != aepack.group("snd!"):
            image.tag_set(index, TAG_GROUP, aepack.group("snd!"))
            return image.name(index), image.signed()
    raise AssertionError("the pack has no required external tag")


def pixels_outside_the_pack(real):
    """an own bitmap's first resource is said to hold another part of the "map file" than the bitmap reads"""
    image = Image(real)
    for index in range(image.tag_count()):
        tag = image.tag(index)
        if tag[TAG_KIND] == aepack.OWN and tag[TAG_GROUP] == aepack.GROUP_BITMAP and tag[TAG_RESOURCES_COUNT]:
            for resource in range(tag[TAG_RESOURCES_FIRST], tag[TAG_RESOURCES_FIRST] + tag[TAG_RESOURCES_COUNT]):
                at = image.sections[aepack.RESOURCES] + resource * aepack.RESOURCE_RECORD.size
                record = list(aepack.RESOURCE_RECORD.unpack_from(image.data, at))
                record[1] += 0x40000000
                aepack.RESOURCE_RECORD.pack_into(image.data, at, *record)
            return image.signed()
    raise AssertionError("the pack has no own bitmap with pixels")


def bitmap_signature_broken(real):
    """the first own bitmap's first bitmap no longer says 'bitm' (the tag validator leaves that to the game,
    whose bitmap functions assert it): found by a byte-flip run of the add path"""
    image = Image(real)
    for index in range(image.tag_count()):
        tag = image.tag(index)
        if tag[TAG_KIND] != aepack.OWN or tag[TAG_GROUP] != aepack.GROUP_BITMAP:
            continue
        tag_bytes = image.sections[aepack.TAG_BYTES] + tag[TAG_BYTES_OFFSET]
        for fixup in range(tag[TAG_FIXUPS_FIRST], tag[TAG_FIXUPS_FIRST] + tag[TAG_FIXUPS_COUNT]):
            offset, kind, _pad, value = image.fixup(fixup)
            # (the bitmaps block: its pointer at 0x64 of a bitmap group, its elements start with the signature)
            if kind == aepack.FIXUP_POINTER and offset == 0x64 and \
                    bytes(image.data[tag_bytes + value:tag_bytes + value + 4]) == b"mtib":
                image.data[tag_bytes + value + 1] ^= 0x01
                return image.signed()
    raise AssertionError("the pack has no own bitmap with bitmaps")


def geometry_for_no_part(real):
    """a model's first PARTS record names the tag's first byte (its root) as a model part"""
    image = Image(real)
    for index in range(image.tag_count()):
        tag = image.tag(index)
        if tag[TAG_PARTS_COUNT]:
            at = image.sections[aepack.PARTS] + tag[TAG_PARTS_FIRST] * aepack.PART_RECORD.size
            record = list(aepack.PART_RECORD.unpack_from(image.data, at))
            if record[0] < aepack.MODEL_PART_SIZE:
                continue
            record[0] = 0
            aepack.PART_RECORD.pack_into(image.data, at, *record)
            return image.signed()
    raise AssertionError("the pack has no model part")


def respelled(real, pack_id, word):
    """the real pack as another slot's (a test stand-in, ae_weapon_slots.h): its pack id the slot's and every
    string's "battle_rifle" respelled as word (same length), so its own tags do not collide with the real pack's
    on the map"""
    assert len(word) == len("battle_rifle")
    data = bytearray(real)
    entry = next(e for e in aepack.section_entries(data) if e[0] == aepack.STRINGS)
    start, size = entry[1], entry[2]
    data[start:start + size] = bytes(data[start:start + size]).replace(b"battle_rifle", word.encode())
    data[0x14:0x34] = pack_id.encode().ljust(32, b"\0")
    aepack.rehash(data)
    aepack.read(bytes(data))
    return bytes(data)


class Game(unittest.TestCase):
    def setUp(self):
        self.folder = Path(tempfile.mkdtemp(dir=Path.home(), prefix="packadd-"))
        self.addCleanup(shutil.rmtree, self.folder, True)

    def packs(self, name, image):
        folder = self.folder / name
        folder.mkdir()
        (folder / "ae_battle_rifle.aepack").write_bytes(image)
        return folder

    def play(self, name, *arguments, refused=False, seconds=30, map_name="bloodgulch", overflow_ok=False):
        """overflow_ok: a map whose own bitmap overflows the texture cache with or without the pack (Timberland's
        jackal shield noise: the Custom Edition map's own, logged without any pack too)"""
        spec = self.folder / f"{name}.json"
        expect = {**({"refusals_ok": True} if refused else {}), **({"cache_overflow_ok": True} if overflow_ok else {})}
        spec.write_text(json.dumps({"expect": expect} if expect else {}))
        rc = run.main(["--spec", str(spec), "--build", BUILD, "--exit-after", str(seconds), "--name", name,
                       "--out-name", f"packadd-{name}", *(["--map", map_name] if map_name else []), *arguments])
        self.assertEqual(rc, 0, name)

    def debug_lines(self, name):
        """the debug.txt lines of a case played by play() (run.py's out folder for it)"""
        cfg = harness.load_config(None)
        path = harness.expand(cfg["out_dir"]) / "run" / f"packadd-{name}" / name / "debug.txt"
        return path.read_text(errors="replace").splitlines()

    def refused(self, name, image, reason):
        """a pack the reader takes is refused for the map with its reason; nothing is added, nobody is given the
        pack's weapon (the stock fallback), and the game plays to its end (exit 0, ticks, no assert)"""
        self.play(name, "--packs-from", str(self.packs(name, image)), "--pack-loadout", "primary",
                  "--expect-line", r"weapon pack: 'ae_battle_rifle' revision ",
                  "--expect-line", REFUSED.format(reason=reason),
                  "--expect-no-line", ANY_ADDED, "--expect-no-line", ANY_FIRST_SPAWN, refused=True)


@unittest.skipUnless(BUILD, "AE_TEST_PACK_BUILD not set (needs a game)")
class SyntheticPack(Game):
    """no game art: the load test's synthetic pack, whose tags are noise"""

    def test_noise_tags_refused_for_the_map(self):
        self.refused("noise", load.make_pack(), r"its tags failed the tag check \(the line before this one says which\)")

    def test_not_asked_for_not_looked_at(self):
        """the same pack in a game whose gametype asks for none: read at start, never added, no refusal"""
        self.play("noise-unasked", "--packs-from", str(self.packs("noise", load.make_pack())),
                  "--expect-line", r"weapon pack: 'ae_battle_rifle' revision ",
                  "--expect-no-line", ANY_ADDED, "--expect-no-line", "weapon pack: refused for ")

    def test_asked_for_with_no_pack(self):
        self.play("asked-nopacks", "--no-packs", "--pack-loadout", "primary",
                  "--expect-line", r"weapon pack: not added to bloodgulch: this game has no pack for slot 1\r?$",
                  "--expect-no-line", ANY_ADDED, "--expect-no-line", ANY_FIRST_SPAWN)


@unittest.skipUnless(BUILD and REAL, "AE_TEST_PACK_BUILD and AE_TEST_PACK_REAL not set (needs a game and a real pack)")
class RealPack(Game):
    def real(self):
        return Path(REAL).expanduser().read_bytes()

    def good(self):
        return str(self.packs("good", self.real()))

    def test_added_and_primary(self):
        self.play("primary", "--packs-from", self.good(), "--pack-loadout", "primary",
                  "--expect-line", ADDED.format(map="bloodgulch", corrections=NO_CORRECTIONS),
                  "--expect-line", FIRST_SPAWN.format(slot="primary"),
                  "--expect-no-line", FIRST_SPAWN.format(slot="secondary"),
                  "--expect-no-line", "weapon pack: .*could not be read")

    def test_secondary(self):
        self.play("secondary", "--packs-from", self.good(), "--pack-loadout", "secondary",
                  "--expect-line", ADDED.format(map="bloodgulch", corrections=NO_CORRECTIONS),
                  "--expect-line", FIRST_SPAWN.format(slot="secondary"),
                  "--expect-no-line", FIRST_SPAWN.format(slot="primary"))

    def test_not_asked_for(self):
        """pack_loadout unset: the pack is read at start and never added"""
        self.play("unasked", "--packs-from", self.good(),
                  "--expect-line", r"weapon pack: 'ae_battle_rifle' revision ",
                  "--expect-no-line", ANY_ADDED, "--expect-no-line", ANY_FIRST_SPAWN)

    def test_campaign_map_without_a_game_variant_gets_none(self):
        """a smoke check only: a campaign map loaded through init.txt, with the test host's pack setting in the
        environment, logs no "added". It cannot fail on the guards themselves: the test host applies the pack bytes
        only when it hosts, so the pending game variant is all zero here and neither guard (no game variant; a
        scenario that is not a multiplayer one) is reached. No route loads a campaign map with a pack variant
        pending (network co-op and split-screen campaign games carry no pack bytes), so the scenario-type guard is
        belt and braces, untested. The menus' map between games, with a pack variant pending until the variant is
        cleared, is checked by the rotation (no "added to ui" / "refused for ui")."""
        self.play("campaign", "--packs-from", self.good(), "--map", "levels\\a10\\a10",
                  "--env", "HALO_NETWORK_TEST_PACK_LOADOUT=primary",
                  "--expect-no-line", ANY_ADDED, map_name=None)

    def test_oversized_block_counts_refused(self):
        self.refused("blocks", oversized_blocks(self.real()),
                     r"its tags failed the tag check \(the line before this one says which\)")

    def test_pointers_over_the_root_refused(self):
        self.refused("pointers", pointers_at_the_root(self.real()),
                     r"its tags failed the tag check \(the line before this one says which\)")

    def test_external_of_another_group_refused(self):
        name, image = external_of_another_group(self.real())
        self.refused("group", image, "missing snd! " + re.escape(name))

    def test_pixels_outside_the_pack_refused(self):
        self.refused("pixels", pixels_outside_the_pack(self.real()), r".* reads pixels the pack does not hold")

    def test_bitmap_signature_refused(self):
        self.refused("signature", bitmap_signature_broken(self.real()), r".* has a bitmap the game cannot read")

    def test_geometry_for_no_part_refused(self):
        self.refused("parts", geometry_for_no_part(self.real()), r".* has geometry for what is no model part")

    def test_several_packs_and_map_spawns(self):
        """three slots' packs on one map (the real one and two stand-ins for the SMGs' slots), each added past the
        one before; the loadout's second source the second named pack's weapon; MAP SPAWNS: the map's assault
        rifles spawn slot 2's weapon, its pistols the battle rifle"""
        folder = self.folder / "slots"
        folder.mkdir()
        (folder / "ae_battle_rifle.aepack").write_bytes(self.real())
        (folder / "ae_smg.aepack").write_bytes(respelled(self.real(), "ae_smg", "smgtest_____"))
        (folder / "ae_smg_silenced.aepack").write_bytes(respelled(self.real(), "ae_smg_silenced", "smgsiltest__"))
        added = r"weapon pack: added to bloodgulch: \d+ tags new \(\d+ bytes\), \d+ the map's own, \d+ optional ones " \
                r"the map lacks, 0 corrections; its tag table {before} to "
        self.play("slots", "--packs-from", str(folder), "--pack-map-spawns",
                  "--env", "HALO_NETWORK_TEST_WEAPON_PACKS=7", "--env", "HALO_NETWORK_TEST_PACK_LOADOUT=first_second",
                  "--expect-line", added.format(before="1806"),
                  "--expect-line", FIRST_SPAWN.format(slot="primary"),
                  "--expect-line", r"weapon pack: first spawn: the secondary weapon is the pack's smgtest_____ \(",
                  "--expect-line", r"weapon pack: map item \d+ spawns the smg \(slot 2\) in place of its assault rifle",
                  "--expect-line", r"weapon pack: map item \d+ spawns the battle_rifle \(slot 1\) in place of its pistol",
                  "--expect-line", r"weapon pack: smgsiltest__'s look: .*ae\\packs\\smgsiltest__\\",
                  "--expect-no-line", "weapon pack: refused", "--expect-no-line", "could not be read")
        tables = [m.group(1, 2) for m in map(re.compile(r"its tag table (\d+) to (\d+);").search,
                                              self.debug_lines("slots")) if m]
        self.assertEqual(len(tables), 3, tables)
        # (each pack's table starts where the one before ended)
        self.assertEqual([t[0] for t in tables[1:]], [t[1] for t in tables[:-1]])

    @unittest.skipUnless(ROTATION, "AE_TEST_PACK_ROTATION not set (four one-minute games)")
    def test_rotation(self):
        """one process, four games of a minute over three maps with three moving bots: the pack added to each
        map and let go from it, resources read from the pack in every game, the rifle the primary each time"""
        lines = []
        for map_name in ("bloodgulch", "hangemhigh", "prisoner"):
            lines += ["--expect-line", ADDED.format(map=map_name, corrections=NO_CORRECTIONS),
                      "--expect-line", LET_GO.format(map=map_name)]
        self.play("rotation", "--packs-from", self.good(), "--pack-loadout", "primary",
                  "--network-test", "host:bloodgulch+hangemhigh+prisoner:slayer,slayer,slayer,slayer",
                  "--env", "HALO_NETWORK_TEST_TIME_LIMIT=1", "--start", "10", "--bots", "3", "--bot-args=--move",
                  "--expect-line", r"network test: game 4 plays prisoner",
                  "--expect-no-line", "weapon pack: refused", "--expect-no-line", "could not be read",
                  # (the menus' map, loaded between the games while the pack's variant is still pending)
                  "--expect-no-line", "weapon pack: (added to|refused for) ui",
                  *lines, seconds=360, map_name=None)
        # each game's map: its "added", then its own first spawn with the rifle, before the next map's
        maps = ["bloodgulch", "hangemhigh", "prisoner", "prisoner"]
        games, current = [], None
        for line in self.debug_lines("rotation"):
            added = re.search(r"weapon pack: added to (\w+):", line)
            if added:
                current = {"map": added.group(1), "first_spawn": False}
                games.append(current)
            elif current and re.search(FIRST_SPAWN.format(slot="primary"), line):
                current["first_spawn"] = True
        self.assertEqual([game["map"] for game in games], maps)
        self.assertEqual([game["first_spawn"] for game in games], [True] * len(maps))

    @unittest.skipUnless(ROTATION, "AE_TEST_PACK_ROTATION not set (two machines, two games)")
    def test_host_and_client(self):
        """an AE host and an AE client, both with the pack, two games of Blood Gulch (the first ends at a score of
        1): each machine adds the pack to each game's map with the same fingerprint (its tags at the same tag
        indices), and the client lets it go between the games, as the host does"""
        cfg = harness.load_config(None)
        out = harness.expand(cfg["out_dir"]) / "handshake" / "packadd-pair"
        shutil.rmtree(out, ignore_errors=True)  # (no earlier run's pair is read)
        rc = handshake.main(["--ae", BUILD, "--other", BUILD, "--one-way", "--seconds", "120",
                             "--map", "bloodgulch:slayer,slayer", "--packs-from", self.good(),
                             "--env", "HALO_NETWORK_TEST_PACK_LOADOUT=primary", "--env", "HALO_NETWORK_TEST_SCORE=1",
                             "--out-name", "packadd-pair"])
        self.assertEqual(rc, 0)
        pair = next(out.glob("aehost-*"))
        added = re.compile(ADDED.format(map="bloodgulch", corrections=NO_CORRECTIONS))
        prints = {}
        for side in ("host", "client"):
            lines = (pair / side / "debug.txt").read_text(errors="replace").splitlines()
            prints[side] = [m.group("fingerprint") for m in map(added.search, lines) if m]
            self.assertEqual(len(prints[side]), 2, side)
            self.assertTrue(any(re.search(LET_GO.format(map="bloodgulch"), line) for line in lines), side)
        self.assertEqual(len(set(prints["host"] + prints["client"])), 1, prints)


class Variants(unittest.TestCase):
    """the hostile variants themselves (no game): each is still a pack the reader takes"""

    @unittest.skipUnless(REAL, "AE_TEST_PACK_REAL not set")
    def test_variants_pass_the_reader(self):
        real = Path(REAL).expanduser().read_bytes()
        for make in (oversized_blocks, pointers_at_the_root, pixels_outside_the_pack, geometry_for_no_part,
                     bitmap_signature_broken):
            self.assertNotEqual(make(real), real, make.__name__)
        name, image = external_of_another_group(real)
        self.assertTrue(name)
        self.assertNotEqual(image, real)

    def test_image_edits_a_synthetic_pack(self):
        image = Image(load.make_pack())
        self.assertEqual(image.tag_count(), 6)
        self.assertEqual(image.weapon_root(), 0)
        self.assertEqual(image.name(4), "ui\\hud\\bitmaps\\combined\\hud_msg_icons")
        image.tag_set(4, TAG_REQUIRED, 0)
        self.assertEqual(aepack.read(image.signed()).tags[4][TAG_REQUIRED], 0)


if __name__ == "__main__":
    unittest.main()
