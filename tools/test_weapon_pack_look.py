"""The look of the pack's weapons (port/linux/game/ae_weapon_pack.c and ae_burst_rifle.c, Task 9): the pickup texts
and the swap / pickup icon come from the pack's weapon entry; an icon whose sequence has no sprites (round 6b's bug:
a sheet drawn whole over the view) is refused; the bitmaps the pack flags SMOOTH are sampled smooth; zoomed in with
the rifle, its holder sees none of its particles. The pictures against the parity reference (halo-pack5f + v6e) are
the task report's; these cases check what the log and the frames can tell alone.

Needs a game and the real pack: AE_TEST_PACK_BUILD and AE_TEST_PACK_REAL (tools/test_weapon_pack_add.py)."""
import re
import shutil
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))
import test_weapon_pack_add as add  # noqa: E402

aepack = add.aepack
harness = add.harness

BUILD = add.BUILD
REAL = add.REAL

LOOK = (r"weapon pack: battle_rifle's look: pickup texts \"Picked up a battle rifle\" / \"Picked up %d rounds for "
        r"battle rifle\", message icon ae\\packs\\battle_rifle\\hud\\br_meter sequence 0 \([1-9]\d* sprites\); its "
        r"HUD's icon sequence \d+")
SMOOTH = r"weapon pack: ae\\packs\\battle_rifle\\hud\\br_alphas_smooth drawn smooth \(512x128, linear with its mips\)"
SMOOTH_METER = (r"weapon pack: ae\\packs\\battle_rifle\\hud\\br_meter_smooth drawn smooth \(512x128, linear with its "
                r"mips, its green its coverage\)")
WEAPON_TEXT = r"weapon pack: item message \"Picked up a battle rifle\" shown \(its weapon entry's weapon pickup text\)"
AMMO_TEXT = (r"weapon pack: item message \"Picked up %d rounds for battle rifle\" shown \(its weapon entry's ammo "
             r"pickup text\)")
ICON_REFUSED = r"weapon pack: battle_rifle's message icon refused for bloodgulch: sequence 0 of [^ ]+br_meter_smooth " \
               r"has no sprites; no icon is drawn"
ICON_MISSING = r"weapon pack: battle_rifle's message icon refused for bloodgulch: its bitmap is not on the map; no icon " \
               r"is drawn"
NO_ICON = r"weapon pack: battle_rifle's prompt drawn with no icon \(its icon was refused\)"
ICON_SHOWN = r"weapon pack: battle_rifle's message icon shown in a prompt \(sequence 0 of its entry's bitmap\)"
HIDDEN = r"ae burst rifle: tick \d+, zoomed: local player 0 sees no particle of the rifle or its holder"
UNREAD = "could not be read from the pack"
# (in game from about 10 s; the rifle lies two steps ahead (debug.weapon_pack_ground): three steps onto it. With two
# stock weapons held: the swap prompt (nothing pressed: a held X takes the rifle only where the frames are quick
# enough); with one: it is picked up)
WALK_ONTO = "menu:" + " ".join(["wait"] * 14 + ["key:W"] * 3 + ["wait"] * 8)
# (WEAPONS record fields, "<14I": the message icon's bitmap tag and sequence)
WEAPON_ICON_TAG = 10
# (an external bitmap the icon may name in the missing-bitmap case: a lens flare's, which may be NONE)
OPTIONAL_ICON_DONOR = "effects\\particles\\lens flare\\bitmaps\\flares_generic2"


def stray_texels(path):
    """round 6b's matrix measure: magenta and bright green texels in the upper half of a frame (the meter sheet
    drawn whole)"""
    width, height, rgb = harness.read_png(path)
    count = 0
    for offset in range(0, width * (height // 2) * 3, 3):
        r, g, b = rgb[offset], rgb[offset + 1], rgb[offset + 2]
        if (r > 140 and b > 140 and g < r - 50 and g < b - 50) or (g > 200 and r < 140 and b < 160):
            count += 1
    return count


def icon_on_the_counter_sheet(real):
    """the weapon entry's icon is the HUD counter's sheet (br_meter_smooth), whose sequence 0 has no sprites: the
    picture round 6b drew whole over the view"""
    image = add.Image(real)
    for index in range(image.tag_count()):
        if image.name(index).endswith("\\br_meter_smooth"):
            offset = image.sections[aepack.WEAPONS]
            record = list(aepack.WEAPON_RECORD.unpack_from(image.data, offset))
            record[WEAPON_ICON_TAG] = index
            aepack.WEAPON_RECORD.pack_into(image.data, offset, *record)
            return image.signed()
    raise AssertionError("the pack has no br_meter_smooth")


def icon_bitmap_missing(real):
    """the weapon entry's icon is an OPTIONAL external bitmap that no map has (a lens flare's bitmap made optional and
    renamed): a valid pack whose icon the map cannot give"""
    image = add.Image(real)
    for index in range(image.tag_count()):
        tag = image.tag(index)
        if tag[add.TAG_KIND] == aepack.EXTERNAL and image.name(index) == OPTIONAL_ICON_DONOR:
            image.tag_set(index, add.TAG_REQUIRED, 0)
            start = image.sections[aepack.STRINGS] + tag[add.TAG_NAME]
            image.data[start] = ord("#")
            offset = image.sections[aepack.WEAPONS]
            record = list(aepack.WEAPON_RECORD.unpack_from(image.data, offset))
            record[WEAPON_ICON_TAG] = index
            aepack.WEAPON_RECORD.pack_into(image.data, offset, *record)
            return image.signed()
    raise AssertionError("the pack has no external " + OPTIONAL_ICON_DONOR)


@unittest.skipUnless(BUILD and REAL, "AE_TEST_PACK_BUILD and AE_TEST_PACK_REAL not set (needs a game and a real pack)")
class Look(add.Game):
    def good(self):
        return str(self.packs("good", Path(REAL).expanduser().read_bytes()))

    def shots_folder(self, name):
        cfg = harness.load_config(None)
        return harness.expand(cfg["out_dir"]) / "run" / f"packadd-{name}" / name / "shots"

    def shots(self, name):
        return sorted(self.shots_folder(name).glob("*.png"))

    def play_with_shots(self, name, *arguments, **keywords):
        """play() with the case's earlier frames gone first (the harness writes over the folder, but keeps a longer
        earlier run's later frames)"""
        shutil.rmtree(self.shots_folder(name), ignore_errors=True)
        self.play(name, *arguments, **keywords)

    def test_look_and_ammo_text(self):
        """the look line (texts and icon from the weapon entry), the two SMOOTH bitmaps sampled smooth as they are
        drawn, and walking over a second rifle (debug.weapon_pack_ground) shows the entry's ammo text"""
        presses = " ".join(["wait"] * 14 + ["key:W"] * 5 + ["wait"] * 3)
        self.play("look", "--packs-from", self.good(), "--pack-loadout", "primary", "--start", "5",
                  "--env", "HALO_WEAPON_PACK_GROUND=1", "--test-input", "menu:" + presses,
                  "--expect-line", LOOK, "--expect-line", SMOOTH, "--expect-line", SMOOTH_METER,
                  "--expect-line", AMMO_TEXT, "--expect-no-line", UNREAD, seconds=30)

    def test_swap_prompt(self):
        """two stock weapons held (the pack asked for by BATTLE RIFLE ON MAP only), walked onto the rifle: the swap
        prompt draws the entry's icon; no frame shows the counter's sheet"""
        name = "swap"
        self.play_with_shots(name, "--packs-from", self.good(), "--pack-map-spawns", "--start", "5",
                  "--env", "HALO_NETWORK_TEST_LOADOUT=2,3", "--env", "HALO_WEAPON_PACK_GROUND=1",
                  "--window", "800x600", "--shots", "16", "--test-input", WALK_ONTO,
                  "--expect-line", LOOK, "--expect-line", ICON_SHOWN, "--expect-no-line", UNREAD, seconds=30)
        frames = self.shots(name)
        self.assertGreaterEqual(len(frames), 10)
        self.assertEqual(max(stray_texels(frame) for frame in frames), 0)

    def test_weapon_text(self):
        """one stock weapon held, the rifle two steps ahead: walked onto, it is picked up, and the pickup message is
        the entry's text"""
        self.play("pickup", "--packs-from", self.good(), "--pack-map-spawns", "--start", "5",
                  "--env", "HALO_NETWORK_TEST_LOADOUT=2,0", "--env", "HALO_WEAPON_PACK_GROUND=1",
                  "--test-input", WALK_ONTO, "--expect-line", WEAPON_TEXT, "--expect-no-line", UNREAD, seconds=25)

    def refused_icon(self, name, image, line):
        """the icon refused for the map with its line; the swap prompt is drawn with no icon (not the HUD's own, a
        sprite of the map's sheet that means something else), never the sheet, and the game plays on"""
        self.play_with_shots(name, "--packs-from", str(self.packs(name, image)), "--pack-map-spawns", "--start", "5",
                  "--env", "HALO_NETWORK_TEST_LOADOUT=2,3", "--env", "HALO_WEAPON_PACK_GROUND=1",
                  "--window", "800x600", "--shots", "16", "--test-input", WALK_ONTO,
                  "--expect-line", line, "--expect-line", add.ADDED.format(map="bloodgulch", corrections="0"),
                  "--expect-line", NO_ICON, "--expect-no-line", ICON_SHOWN, refused=True, seconds=30)
        self.assertEqual(max(stray_texels(frame) for frame in self.shots(name)), 0)

    def test_icon_without_sprites_refused(self):
        """the entry's icon is the counter's sheet, whose sequence has no sprites"""
        self.refused_icon("icon", icon_on_the_counter_sheet(Path(REAL).expanduser().read_bytes()), ICON_REFUSED)

    def test_icon_bitmap_missing_refused(self):
        """the entry's icon is an optional external bitmap the map lacks (a valid pack)"""
        self.refused_icon("iconmissing", icon_bitmap_missing(Path(REAL).expanduser().read_bytes()), ICON_MISSING)

    def test_zoomed_particles_hidden(self):
        """firing zoomed in (debug.burst_autofire; Z pressed every three seconds: a reload ends a zoom, and a press in
        a slow frame can go unseen), the holder sees none of the rifle's particles"""
        presses = " ".join(["wait"] * 14 + ["key:Z", "wait", "wait"] * 5)
        self.play("zoomed", "--packs-from", self.good(), "--pack-loadout", "primary", "--start", "5",
                  "--env", "HALO_WEAPON_LOG=1", "--env", "HALO_BURST_AUTOFIRE=1", "--test-input", "menu:" + presses,
                  "--expect-line", HIDDEN, "--expect-no-line", UNREAD, seconds=34)
        lines = self.debug_lines("zoomed")
        self.assertLessEqual(sum(bool(re.search(HIDDEN, line)) for line in lines), 16)


if __name__ == "__main__":
    unittest.main()
