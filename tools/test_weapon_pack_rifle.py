"""The AE battle rifle's behaviour (port/linux/game/ae_burst_rifle.c, Task 8): the pack's rifle fires three
rounds a new pull, two ticks apart, bursts 12 ticks apart at the fastest (the 0.20 s recovery); the zoom as seen, aim assist,
spread, bullet, magazine and the owner's balance; a map's own battle rifle untouched. The numbers are the weapon pack
plan's "Rifle numbers and look" table, which the parity reference (the prototype halo-pack5f with its v6e donor)
measured the same on the same runs (task-8-report.md).

Needs a game and the real pack: AE_TEST_PACK_BUILD and AE_TEST_PACK_REAL (tools/test_weapon_pack_add.py). The CE+ X
case also needs AE_TEST_PACK_ROTATION=1 and the mod CE+ X in the data root."""
import re
import sys
import unittest
from collections import Counter
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))
import test_weapon_pack_add as add  # noqa: E402

BUILD = add.BUILD
REAL = add.REAL
ROTATION = add.ROTATION

ROUND = re.compile(r"ae burst rifle: tick (\d+), round (\d) of 3 (fired|not fired), (\d+) loaded, (\d+) in reserve, "
                   r"recovery \d+, firing effect (-?\d+) of (\d+)")
# (a pack bitmap read as zeros: the scope's mask, smoke, impacts; reads into guest memory go through a host buffer)
UNREAD = "could not be read from the pack"
SET_UP = r"ae burst rifle: '[^']+' set up \(3 rounds a pull, 15 a second, recovery 0\.2 s, damage 6\.5\)"
SETUP_LINES = (
    r"ae burst rifle: zoom 1 level 2\.563x \(3\.00x as seen\)",
    r"ae burst rifle: spread 0\.3 to 0\.3 deg, inner cone 0, a tracer every 3 rounds",
    r"ae burst rifle: magazine 36 loaded, 108 at first, reserve at most 108",
    r"ae burst rifle: bullet now 400 wu a second, range 40 wu, gravity 0",
    r"ae burst rifle: damage 6\.5 a round, shield x1\.06, body x1\.50",
    r"ae burst rifle: aim assist now autoaim 3\.50 deg / 17\.0 wu, magnetism 7\.00 deg / 21\.0 wu, deviation 0",
    SET_UP,
)
ZOOMED_CAP = r"ae burst rifle: zoomed aim assist ranges [\d.]+ / [\d.]+ wu, capped at 40\.0"
HIT = re.compile(r"network test: tick \d+ hit x1\.0( head)?: health (-?[\d.]+) shield ([\d.]+) stun \d+ dead (\d)")


def bursts(lines):
    """the rounds of each burst: [(tick, round, fired, loaded, reserve), ...] a burst"""
    result = []
    for line in lines:
        match = ROUND.search(line)
        if not match:
            continue
        entry = (int(match[1]), int(match[2]), match[3] == "fired", int(match[4]), int(match[5]), int(match[6]),
                 int(match[7]))
        if entry[1] == 1 or not result:
            result.append([])
        result[-1].append(entry)
    return result


def gaps_in_magazines(burst_list):
    """the ticks between successive bursts' first rounds, a list for each magazine (a reload ends one)"""
    magazines = [[]]
    for a, b in zip(burst_list, burst_list[1:]):
        if b[0][3] == a[-1][3] - 1:
            magazines[-1].append(b[0][0] - a[0][0])
        elif magazines[-1]:
            magazines.append([])
    return [gaps for gaps in magazines if gaps]


@unittest.skipUnless(BUILD and REAL, "AE_TEST_PACK_BUILD and AE_TEST_PACK_REAL not set (needs a game and a real pack)")
class Rifle(add.Game):
    def good(self):
        return str(self.packs("good", Path(REAL).expanduser().read_bytes()))

    def rifle(self, name, *arguments, seconds=40, map_name="bloodgulch"):
        self.play(name, "--packs-from", self.good(), "--pack-loadout", "primary", "--start", "5",
                  "--expect-no-line", UNREAD, *arguments, seconds=seconds, map_name=map_name)
        return self.debug_lines(name)

    def test_setup_and_cadence(self):
        """the numbers set up once; held fire (debug.burst_autofire): rounds at t, t+2, t+4 and bursts 12 ticks
        apart (6 plus the 0.20 s recovery's 6); a fresh rifle 36 + 72; the zoomed aim assist cap never met unzoomed; no kick; each
        burst plays the trigger's firing effects from the first"""
        lines = self.rifle("cadence", "--env", "HALO_WEAPON_LOG=1", "--env", "HALO_BURST_AUTOFIRE=1",
                           *[argument for line in SETUP_LINES for argument in ("--expect-line", line)],
                           "--expect-line", r"ae burst rifle: debug\.burst_autofire 1 on",
                           "--expect-line", r"ae burst rifle: the firer's kick '[^']+' removed",
                           "--expect-no-line", ZOOMED_CAP)
        self.assertEqual(sum(bool(re.search(SET_UP, line)) for line in lines), 1)
        burst_list = bursts(lines)
        self.assertGreaterEqual(len(burst_list), 24)
        for burst in burst_list:
            self.assertEqual([entry[0] - burst[0][0] for entry in burst], [0, 2, 4])
            self.assertTrue(all(entry[2] for entry in burst))
        self.assertEqual(burst_list[0][0][3:5], (35, 72))
        magazines = gaps_in_magazines(burst_list)
        self.assertGreaterEqual(sum(map(len, magazines)), 20)
        for gaps in magazines:
            # (the 0.20 s recovery is a whole 6 ticks: no fraction carried, every gap the same)
            self.assertEqual(set(gaps), {12}, gaps)
        # (the firing effects: the same ones in the same order in every burst, the first effect first)
        effects = Counter(tuple(entry[5] for entry in burst) for burst in burst_list)
        self.assertEqual(len(effects), 1, effects)
        self.assertEqual(next(iter(effects))[0], 0)
        self.assertGreater(burst_list[0][0][6], 0)

    def test_pull_during_recovery_lost(self):
        """debug.burst_autofire 2: a pull in the middle of each recovery, then another half a second after it: the
        early pulls are lost (no burst starts when the recovery ends), so bursts come only from the late pulls"""
        lines = self.rifle("recovery", "--env", "HALO_WEAPON_LOG=1", "--env", "HALO_BURST_AUTOFIRE=2",
                           "--expect-line", r"ae burst rifle: debug\.burst_autofire 2 on")
        early = [line for line in lines if "a pull during the recovery" in line]
        burst_list = bursts(lines)
        self.assertGreaterEqual(len(burst_list), 10)
        self.assertGreaterEqual(len(early), len(burst_list) - 1)
        for burst in burst_list:
            self.assertEqual([entry[0] - burst[0][0] for entry in burst], [0, 2, 4])
        gaps = [gap for magazine in gaps_in_magazines(burst_list) for gap in magazine]
        self.assertGreaterEqual(len(gaps), 8)
        # (a kept pull would start the next burst 12 ticks after the last, as at the fastest cadence; the late
        # pull comes AUTOFIRE_LATE_TICKS, 15, later: 27)
        self.assertEqual(set(gaps), {27}, gaps)

    def test_zoom_and_fire_then_next_map(self):
        """zoomed and firing (the scope's mask, smoke and impact bitmaps are read from the pack while the renderer
        holds the texture cache), then the map ends: its "let go" line counts no failed read"""
        presses = " ".join(["wait"] * 12 + ["key:Z", "wait", "wait", "key:Z", "key:Z", "wait", "key:Z"] * 3)
        self.play("zoomfire", "--packs-from", self.good(), "--pack-loadout", "primary",
                  "--network-test", "host:bloodgulch+prisoner:slayer,slayer",
                  "--env", "HALO_NETWORK_TEST_TIME_LIMIT=1", "--start", "5", "--env", "HALO_BURST_AUTOFIRE=1",
                  "--test-input", "menu:" + presses,
                  "--expect-line", add.LET_GO.format(map="bloodgulch"),
                  "--expect-line", r"network test: game 2 plays prisoner",
                  "--expect-no-line", UNREAD, seconds=150, map_name=None)

    def test_presses_zoom_and_reserve(self):
        """a pull of 250 ms (debug.test_input) fires exactly three rounds; walking over a second rifle tops the
        reserve at 108; zoomed (display.fov 100), the aim assist ranges are capped at 40 wu and the view is still
        17.6 degrees high"""
        presses = " ".join(["wait"] * 14 + ["key:W"] * 3 + ["wait"] + ["key:K"] * 8 + ["key:Z", "wait", "wait"] +
                           ["key:K"] * 2 + ["wait"])
        lines = self.rifle("presses", "--env", "HALO_KEY_FIRE=K", "--env", "HALO_WEAPON_LOG=1",
                           "--env", "HALO_WEAPON_PACK_GROUND=1", "--env", "HALO_FOV=100",
                           "--test-input", "menu:" + presses,
                           "--expect-line", ZOOMED_CAP,
                           "--expect-line", r"ae burst rifle: zoomed view 17\.6\d deg vertical .*display\.fov 100\)")
        burst_list = bursts(lines)
        self.assertEqual(len(burst_list), 10)
        for burst in burst_list:
            self.assertEqual([entry[0] - burst[0][0] for entry in burst], [0, 2, 4])
        self.assertEqual(max(entry[4] for burst in burst_list for entry in burst), 108)
        self.assertEqual(sum(bool(re.search(ZOOMED_CAP, line)) for line in lines), 1)

    def test_damage(self):
        """network_test hits with the rifle's own round: 11 empty the shield; a head round after them kills (the
        11th already does: it breaks the shield and the rest reaches the head); body only, the 19th kills"""
        script = [f"10.{i}:1" for i in range(10)] + ["11.0:1h"]
        script += [f"25.{i}:1" for i in range(10)] + ["26.0:1", "26.1:1h"]
        script += [f"{s}.{i}:1" for s in (40, 41) for i in range(10)]
        lines = self.rifle("damage", "--env", "HALO_NETWORK_TEST_DAMAGE=" + ",".join(script), seconds=70)
        hits = [(bool(m[1]), float(m[2]), float(m[3]), m[4] == "1") for m in map(HIT.search, lines) if m]
        self.assertGreaterEqual(len(hits), 11 + 12 + 19)
        first, second, third = hits[:11], hits[11:23], hits[23:42]
        self.assertEqual([hit[3] for hit in first], [False] * 10 + [True])
        self.assertEqual([hit[3] for hit in second], [False] * 11 + [True])
        self.assertEqual((second[10][2], second[10][1]), (0.0, 0.9842))
        self.assertEqual([hit[3] for hit in third], [False] * 18 + [True])
        self.assertEqual(Counter(hit[2] == 0.0 for hit in third[:11])[True], 1)

    @unittest.skipUnless(ROTATION, "AE_TEST_PACK_ROTATION not set (the CE+ X mod's start-up)")
    def test_map_own_rifle_untouched(self):
        """CE+ X's Blood Gulch has a battle rifle of its own: replaced by the pack's (owner 10-24), only the pack's
        is set up"""
        lines = self.rifle("cex", "--mod", "CE+ X",
                           "--expect-line", r"weapon pack: the map's own battle rifle \(.*\) replaced by the pack's",
                           "--expect-line", SET_UP, seconds=150)
        set_up = [line for line in lines if re.search(SET_UP, line)]
        self.assertEqual(len(set_up), 1)
        self.assertIn("ae\\packs\\", set_up[0])


if __name__ == "__main__":
    unittest.main()
