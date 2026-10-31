"""The weapon pack's checks against the map a game loads (port/linux/game/ae_weapon_pack.c, Task 7) and the
fallback: a map the pack cannot be added to refuses it with one line, the players get the stock weapons (the
assault rifle for a pack primary), the console says "BATTLE RIFLE unavailable on <map>: <reason>", and the game
plays on. A Custom Edition map takes the pack as an Xbox map does, checked the same way (a Gearbox map has every
stock tag the pack borrows; a custom map that lacks one refuses it with "missing <group> <name>").

Needs a game: AE_TEST_PACK_BUILD (a build for tools/ae_test/run.py) and a config for the machine that plays
(docs/testing.md). The real-pack cases also need AE_TEST_PACK_REAL (a real battle rifle pack: game art, never in
this repository); its forced-failure variants are made here, in the test's own folder, by changing a few bytes
of a copy and signing it again (each still passes the pack reader). The stock-map rotation (all 13 Xbox
multiplayer maps, a minute each), the Halo 1: NHE map and the CE+ X map need AE_TEST_PACK_ROTATION=1 and, for the
last two, the mods NHE and CE+ X in the data root (each case skips without its map)."""
import os
import re
import struct
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS / "ae_test"))
sys.path.insert(0, str(TOOLS / "weapon_pack"))
sys.path.insert(0, str(TOOLS))
import aepack  # noqa: E402
import test_weapon_pack_add as add  # noqa: E402

# (tools/ae_test's harness, as the add test imports it: tools/harness is another package of that name)
harness = add.harness

BUILD = add.BUILD
REAL = add.REAL
ROTATION = add.ROTATION

STOCK_MAPS = ("beavercreek", "bloodgulch", "boardingaction", "carousel", "chillout", "damnation", "hangemhigh",
              "longest", "prisoner", "putput", "ratrace", "sidewinder", "wizard")

ADDED = r"weapon pack: added to {map}: \d+ tags new"
HELD = r"weapon pack: battle_rifle is held by the label '(h_br|ar)'"
UNAVAILABLE = r"weapon pack: BATTLE RIFLE unavailable on {map}: {reason}\r?$"
# (a Custom Edition map's tags checked by the validator before the pack's: its state is the map's, not ui.map's)
CE_CHECKED = r"the map 'ae_battle_rifle \(weapon pack\) on {map}' "
NOISE = r"its tags failed the tag check \(the line before this one says which\)"
# (the fallback: the stock loadout's assault rifle first, as game_engine_log_first_spawn names it)
STOCK_FIRST_SPAWN = r"first spawn: assault rifle \+ pistol"
PACK_FIRST_SPAWN = r"first spawn: battle_rifle \+ pistol"

# (a game of a mod empties the map cache and fills it again before it starts)
MOD_SECONDS = 120

# WEAPONS record fields ("<14I")
WEAPON_LABEL, WEAPON_FALLBACKS = 4, (5, 6)
# animation_graph's nodes block (first_person_weapons.c: verify_animation_graph_nodes_offset): its pointer
NODES_POINTER = 0x68 + 4


def string_at(image, offset):
    start = image.sections[aepack.STRINGS] + offset
    return start, bytes(image.data[start:image.data.index(b"\0", start)])


def labels_zz(real):
    """the weapon's label and fallback labels all "zz" (as ae_pack_build --labels zz makes it): a label no map's
    player has"""
    image = add.Image(real)
    at = image.sections[aepack.WEAPONS]
    record = list(aepack.WEAPON_RECORD.unpack_from(image.data, at))
    start, text = string_at(image, record[WEAPON_LABEL])
    assert len(text) >= 2, text
    image.data[start:start + len(text)] = b"zz" + b"\0" * (len(text) - 2)
    for field in WEAPON_FALLBACKS:
        record[field] = record[WEAPON_LABEL]
    aepack.WEAPON_RECORD.pack_into(image.data, at, *record)
    return image.signed()


def external_renamed(real, required):
    """an external tag (required or optional) renamed so that no map has it: (its group text, new name, pack)"""
    image = add.Image(real)
    for index in range(image.tag_count()):
        tag = image.tag(index)
        if tag[add.TAG_KIND] != aepack.EXTERNAL or bool(tag[add.TAG_REQUIRED]) != required:
            continue
        start, text = string_at(image, tag[add.TAG_NAME])
        image.data[start] = ord("#")
        group = struct.pack(">I", tag[add.TAG_GROUP]).decode("latin-1")
        return group, "#" + text[1:].decode(), image.signed()
    raise AssertionError(f"the pack has no {'required' if required else 'optional'} external tag")


def name_collision(real, candidates=("weapons\\pistol\\pistol", "weapons\\sniper rifle\\sniper rifle",
                                     "weapons\\plasma rifle\\plasma rifle")):
    """the pack's own weapon tag renamed to a stock weapon's name every stock map has: (the name, pack)"""
    image = add.Image(real)
    taken = {(image.tag(index)[add.TAG_GROUP], image.name(index)) for index in range(image.tag_count())}
    root = image.weapon_root()
    tag = image.tag(root)
    start, text = string_at(image, tag[add.TAG_NAME])
    for name in candidates:
        if (tag[add.TAG_GROUP], name) not in taken and len(name) <= len(text):
            image.data[start:start + len(text)] = name.encode() + b"\0" * (len(text) - len(name))
            return name, image.signed()
    raise AssertionError("no stock weapon name fits")


def arms_mismatch(real):
    """every own animation graph's first node renamed: the map's first-person hands (or the weapon's model) have
    a node the weapon's first-person animations lack"""
    image = add.Image(real)
    changed = 0
    for index in range(image.tag_count()):
        tag = image.tag(index)
        if tag[add.TAG_KIND] != aepack.OWN or tag[add.TAG_GROUP] != aepack.group("antr"):
            continue
        tag_bytes = image.sections[aepack.TAG_BYTES] + tag[add.TAG_BYTES_OFFSET]
        count = struct.unpack_from("<i", image.data, tag_bytes + NODES_POINTER - 4)[0]
        for fixup in range(tag[add.TAG_FIXUPS_FIRST], tag[add.TAG_FIXUPS_FIRST] + tag[add.TAG_FIXUPS_COUNT]):
            offset, kind, _pad, value = image.fixup(fixup)
            if kind == aepack.FIXUP_POINTER and offset == NODES_POINTER and count > 0:
                image.data[tag_bytes + value] = ord("#")
                changed += 1
    assert changed, "the pack has no own animation graph with nodes"
    return image.signed()


class Checks(add.Game):
    def lines(self, name):
        return self.debug_lines(name)

    def ce_map(self, file):
        """a Custom Edition map's case skips without its file in the data root's maps/ce"""
        cfg = harness.load_config(None)
        if not (harness.expand(cfg["data_dir"]) / "maps" / "ce" / f"{file}.map").exists():
            self.skipTest(f"no maps/ce/{file}.map in the data root")

    def fallback(self, name, image, reason, *arguments, map_name="bloodgulch"):
        """refused for the map with its reason, the fallback given (the stock assault rifle), the console's line,
        and the game plays to its end (exit 0, no assert). The lines name the map by its file (a Custom Edition
        map's without its @ce)"""
        logged = map_name.split("@")[0]
        self.play(name, "--packs-from", str(self.packs(name, image)), "--pack-loadout", "primary",
                  "--expect-line", add.REFUSED.format(reason=reason).replace("bloodgulch", logged),
                  "--expect-line", UNAVAILABLE.format(map=logged, reason=reason),
                  "--expect-line", STOCK_FIRST_SPAWN,
                  "--expect-no-line", add.ANY_ADDED, "--expect-no-line", add.ANY_FIRST_SPAWN,
                  *arguments, refused=True, map_name=map_name)


@unittest.skipUnless(BUILD, "AE_TEST_PACK_BUILD not set (needs a game)")
class NoRealPack(Checks):
    def test_no_pack_here_falls_back(self):
        self.play("nopack", "--no-packs", "--pack-loadout", "primary",
                  "--expect-line", UNAVAILABLE.format(map="bloodgulch", reason="this machine has no battle_rifle pack"),
                  "--expect-line", STOCK_FIRST_SPAWN, "--expect-no-line", add.ANY_ADDED)

    def test_custom_edition_map_checks_the_pack(self):
        """a Custom Edition map takes the pack as an Xbox map does: the synthetic pack's noise tags go through the
        tag validator beside the map's (its line names the pack on the map) and are refused, the stock weapons
        given, and the console's line names the map by its file"""
        self.ce_map("homobox")
        self.play("ce", "--packs-from", str(self.packs("ce", add.load.make_pack())), "--pack-loadout", "primary",
                  "--expect-line", CE_CHECKED.format(map="homobox"),
                  "--expect-line", add.REFUSED.format(reason=NOISE).replace("bloodgulch", "homobox"),
                  "--expect-line", UNAVAILABLE.format(map="homobox", reason=NOISE),
                  "--expect-line", STOCK_FIRST_SPAWN,
                  "--expect-no-line", add.ANY_ADDED, "--expect-no-line", add.ANY_FIRST_SPAWN,
                  "--expect-no-line", "not on Custom Edition maps", refused=True, map_name="homobox@ce")

    def test_custom_edition_map_unasked_says_nothing(self):
        self.ce_map("homobox")
        self.play("ce-unasked", "--packs-from", str(self.packs("ce-unasked", add.load.make_pack())),
                  "--expect-no-line", "weapon pack: refused", "--expect-no-line", "BATTLE RIFLE",
                  map_name="homobox@ce")


@unittest.skipUnless(BUILD and REAL, "AE_TEST_PACK_BUILD and AE_TEST_PACK_REAL not set (needs a game and a real pack)")
class RealPackChecks(Checks):
    def real(self):
        return Path(REAL).expanduser().read_bytes()

    def test_added_on_bloodgulch_with_the_rifle(self):
        """the real pack passes every check: held by a label in every seat, its arms fit, no console line"""
        self.play("added", "--packs-from", str(self.packs("added", self.real())), "--pack-loadout", "primary",
                  "--expect-line", ADDED.format(map="bloodgulch"), "--expect-line", HELD,
                  "--expect-line", PACK_FIRST_SPAWN,
                  "--expect-no-line", "BATTLE RIFLE unavailable", "--expect-no-line", "weapon pack: refused")

    def test_labels_zz_refused(self):
        self.fallback("labels", labels_zz(self.real()), r"the player cannot hold it: .*\(zz(, zz)*\).*")

    def test_required_external_missing_refused(self):
        group, name, image = external_renamed(self.real(), True)
        self.fallback("required", image, "missing " + re.escape(f"{group} {name}"))

    def test_optional_external_missing_added(self):
        """an optional external the map lacks is no tag: one line, and the pack is added"""
        group, name, image = external_renamed(self.real(), False)
        self.play("optional", "--packs-from", str(self.packs("optional", image)), "--pack-loadout", "primary",
                  "--expect-line", r"weapon pack: bloodgulch lacks 1 optional tags of the pack, first " +
                  re.escape(f"{group} {name}") + ": they are no tag there",
                  "--expect-line", r"weapon pack: added to bloodgulch: \d+ tags new \(\d+ bytes\), \d+ the map's own, "
                  r"1 optional ones the map lacks",
                  "--expect-line", PACK_FIRST_SPAWN, "--expect-no-line", "weapon pack: refused")

    def test_name_collision_refused(self):
        name, image = name_collision(self.real())
        self.fallback("collision", image, "name collision: the map has weap " + re.escape(name))

    def test_first_person_arms_refused(self):
        self.fallback("arms", arms_mismatch(self.real()), r"first-person arms do not match: .*")

    def test_added_on_a_gearbox_custom_edition_map(self):
        """a Gearbox map (Timberland) has every stock tag the pack borrows: added, held, the first spawn the rifle,
        no read of the pack's tags outside them, no refusal and no console line"""
        self.ce_map("timberland")
        self.play("ce-timberland", "--packs-from", str(self.packs("ce-timberland", self.real())),
                  "--pack-loadout", "primary",
                  "--expect-line", add.ADDED.format(map="timberland", corrections=add.NO_CORRECTIONS),
                  "--expect-line", HELD, "--expect-line", PACK_FIRST_SPAWN,
                  "--expect-no-line", "outside them", "--expect-no-line", "BATTLE RIFLE unavailable",
                  "--expect-no-line", "weapon pack: refused", refused=True, overflow_ok=True,
                  map_name="timberland@ce")

    def test_custom_map_without_the_stock_tags_falls_back(self):
        """a custom Custom Edition map without the stock assault rifle's tags the pack borrows (H2 Blood Gulch):
        refused for "missing <group> <name>", the map's own weapons (named as the map names them: not the stock
        first spawn line's), the console's line, the game played"""
        self.ce_map("h2_blood_gulch")
        reason = r"missing \w+ weapons\\assault rifle\\.*"
        self.play("ce-missing", "--packs-from", str(self.packs("ce-missing", self.real())), "--pack-loadout", "primary",
                  "--expect-line", add.REFUSED.format(reason=reason).replace("bloodgulch", "h2_blood_gulch"),
                  "--expect-line", UNAVAILABLE.format(map="h2_blood_gulch", reason=reason),
                  "--expect-line", r"first spawn: ",
                  "--expect-no-line", add.ANY_ADDED, "--expect-no-line", add.ANY_FIRST_SPAWN,
                  refused=True, map_name="h2_blood_gulch@ce")

    def test_tag_table_full_refused(self):
        self.fallback("table", self.real(), r"tag table full: \d+ of the map's and \d+ of the pack's, at most 1000",
                      "--env", "HALO_WEAPON_PACK_TABLE_LIMIT=1000")

    @unittest.skipUnless(ROTATION, "AE_TEST_PACK_ROTATION not set (13 one-minute games)")
    def test_stock_maps_rotation(self):
        """one process, the 13 Xbox multiplayer maps a minute each, three moving bots: the pack added to every
        map (no correction), held by a label, its first spawn the rifle; no refusal, no console line"""
        maps = list(STOCK_MAPS)
        lines = []
        for map_name in maps:
            lines += ["--expect-line", add.ADDED.format(map=map_name, corrections=add.NO_CORRECTIONS)]
        self.play("stock13", "--packs-from", str(self.packs("stock13", self.real())), "--pack-loadout", "primary",
                  "--network-test", "host:" + "+".join(maps) + ":" + ",".join(["slayer"] * len(maps)),
                  "--env", "HALO_NETWORK_TEST_TIME_LIMIT=1", "--start", "10", "--bots", "3", "--bot-args=--move",
                  "--expect-line", rf"network test: game {len(maps)} plays {maps[-1]}",
                  "--expect-no-line", "weapon pack: refused", "--expect-no-line", "BATTLE RIFLE unavailable",
                  "--expect-no-line", "could not be read", *lines, seconds=90 * len(maps), map_name=None)
        # (each map's label line comes before its "added" line, its first spawn after it)
        games, current, held = [], None, False
        for line in self.lines("stock13"):
            found = re.search(r"weapon pack: added to (\w+):", line)
            if found:
                current = {"map": found.group(1), "held": held, "first_spawn": False}
                games.append(current)
                held = False
            elif re.search(HELD, line):
                held = True
            elif current and re.search(PACK_FIRST_SPAWN, line):
                current["first_spawn"] = True
        self.assertEqual([game["map"] for game in games], maps)
        self.assertTrue(all(game["held"] and game["first_spawn"] for game in games), games)

    def mod_map(self, mod):
        cfg = harness.load_config(None)
        if not (harness.expand(cfg["data_dir"]) / "mods" / mod / "maps").is_dir():
            self.skipTest(f"no mods/{mod} in the data root")

    @unittest.skipUnless(ROTATION, "AE_TEST_PACK_ROTATION not set (the mods' maps)")
    def test_nhe_badcreek_added(self):
        """Halo 1: NHE's map (build 01.10.12.2300): a build the engine plays, so the pack is added"""
        self.mod_map("NHE")
        self.play("nhe", "--packs-from", str(self.packs("nhe", self.real())), "--pack-loadout", "primary",
                  "--mod", "NHE", "--expect-line", ADDED.format(map="badcreek"), "--expect-line", PACK_FIRST_SPAWN,
                  "--expect-no-line", "weapon pack: refused", refused=True, map_name="badcreek", seconds=MOD_SECONDS)

    @unittest.skipUnless(ROTATION, "AE_TEST_PACK_ROTATION not set (the mods' maps)")
    def test_ceplusx_bloodgulch_beside_its_own_rifle(self):
        """CE+ X's Blood Gulch has its own battle rifle (dw2\\...): replaced by the pack's (owner 10-24), both
        names logged"""
        self.mod_map("CE+ X")
        self.play("cex", "--packs-from", str(self.packs("cex", self.real())), "--pack-loadout", "primary",
                  "--mod", "CE+ X", "--expect-line", ADDED.format(map="bloodgulch"),
                  "--expect-line", r"weapon pack: the map's own battle rifle \(dw2\\weapons\\battle_rifle\\battle_rifle\) "
                  r"replaced by the pack's \(ae\\packs\\battle_rifle\\",
                  "--expect-no-line", "weapon pack: refused", refused=True, seconds=MOD_SECONDS)


class Variants(unittest.TestCase):
    """the forced-failure variants themselves (no game): each still a pack the reader takes"""

    @unittest.skipUnless(REAL, "AE_TEST_PACK_REAL not set")
    def test_variants_pass_the_reader(self):
        real = Path(REAL).expanduser().read_bytes()
        self.assertNotEqual(labels_zz(real), real)
        self.assertNotEqual(arms_mismatch(real), real)
        for required in (True, False):
            group, name, image = external_renamed(real, required)
            self.assertTrue(name.startswith("#"))
            self.assertNotEqual(image, real)
        name, image = name_collision(real)
        self.assertIn(name, ("weapons\\pistol\\pistol", "weapons\\sniper rifle\\sniper rifle",
                             "weapons\\plasma rifle\\plasma rifle"))
        image = add.Image(labels_zz(real))
        record = aepack.WEAPON_RECORD.unpack_from(image.data, image.sections[aepack.WEAPONS])
        self.assertEqual(string_at(image, record[WEAPON_LABEL])[1], b"zz")
        self.assertEqual({record[field] for field in WEAPON_FALLBACKS}, {record[WEAPON_LABEL]})


if __name__ == "__main__":
    unittest.main()
