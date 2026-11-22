"""Tests of tools/ae_test/health_check.py and log_checks.py over recorded sample logs (tools/ae_test/sample_logs/)."""
import re
import sys
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "ae_test"))
import health_check  # noqa: E402
import log_checks  # noqa: E402

SAMPLES = HERE / "ae_test" / "sample_logs"


def lines(name):
    return (SAMPLES / name).read_text().splitlines()


def by_id(results):
    return {i: (ok, m) for i, ok, m in results}


class HealthCheck(unittest.TestCase):
    def test_recorded_halo2_run_passes(self):
        res = by_id(health_check.check(lines("health_halo2.log"), need_body=True))
        for ident in ("hits", "stun_set", "no_recharge_under_fire", "shield_delay", "shield_full", "body_delay", "body_full"):
            self.assertTrue(res[ident][0], (ident, res[ident]))

    def test_shield_recharging_early_fails(self):
        # the stun of the last hit cut to 2 s: the shield rises 3 s too early
        out = []
        for l in lines("health_halo2.log"):
            m = re.search(r"tick (\d+) health ([\d.]+) shield ([\d.]+)", l)
            if m and 150 < int(m.group(1)) < 290 and "hit" not in l:
                l = l.replace("tick %s " % m.group(1), "tick %d " % (int(m.group(1)) - 90))
            out.append(l)
        res = by_id(health_check.check(out, need_body=True))
        self.assertFalse(res["shield_delay"][0])

    def test_body_recovery_too_soon_fails(self):
        out = []
        for l in lines("health_halo2.log"):
            m = re.search(r"tick (\d+) health ([\d.]+)", l)
            if m and int(m.group(1)) > 306 and float(m.group(2)) < 1.0 and "shield 1.0000" in l and int(m.group(1)) > 500:
                l = l.replace("tick %s " % m.group(1), "tick %d " % (int(m.group(1)) - 200))
            out.append(l)
        res = by_id(health_check.check(out, need_body=True))
        self.assertFalse(res["body_delay"][0])

    def test_empty_log_fails(self):
        self.assertFalse(health_check.check(["nothing"])[0][1])


class LogChecks(unittest.TestCase):
    def run_check(self, name, source, **params):
        return by_id(log_checks.run_checks(source, [name], params))[name]

    def test_waypoint_goes_off(self):
        ok, msg = self.run_check("waypoint_off_after_pickup", lines("waypoints_hud.log"))
        self.assertTrue(ok, msg)

    def test_waypoint_that_never_goes_off_fails(self):
        on_only = [l for l in lines("waypoints_hud.log") if " on at" in l]
        self.assertFalse(self.run_check("waypoint_off_after_pickup", on_only)[0])
        # TRAINING keeps them: the same log is right the other way round
        self.assertTrue(self.run_check("waypoint_off_after_pickup", on_only, waypoint_stays="1")[0])

    def test_drop_except_power(self):
        self.assertTrue(self.run_check("drop_except_power", lines("drop_except_power.log"))[0])
        bad = [l.replace(", gone", ", now") for l in lines("drop_except_power.log")]
        self.assertFalse(self.run_check("drop_except_power", bad)[0])

    def test_all_weapons(self):
        self.assertTrue(self.run_check("all_weapons", lines("all_weapons.log"), min_weapons="14")[0])
        short = [l for l in lines("all_weapons.log") if "weapon 3:" not in l]
        self.assertFalse(self.run_check("all_weapons", short, min_weapons="14")[0])

    def test_spree_texts(self):
        short = ["arena-evolved: test capture: hud: Killing Spree! (5)"]
        long = ["arena-evolved: test capture: hud: Killing Spree!"]
        self.assertTrue(self.run_check("spree_text_short", short)[0])
        self.assertFalse(self.run_check("spree_text_short", long)[0])
        self.assertTrue(self.run_check("spree_text_long", long)[0])
        self.assertFalse(self.run_check("spree_text_long", short)[0])

    def test_start_text_and_postgame_and_ball(self):
        cap = lines("capture_events.log")
        self.assertTrue(self.run_check("start_text", cap, start_text="Welcome Player")[0])
        self.assertFalse(self.run_check("start_text", cap, start_text="TEAM AE SLAYER")[0])
        self.assertFalse(self.run_check("postgame_report", cap)[0])
        self.assertTrue(self.run_check("postgame_report", cap + ["arena-evolved: test capture: postgame"])[0])
        # (the engine's own format, game_engine.c's "AE rules:" error() line, as written to debug.txt)
        rules = ["10.09.26 03:35:14  AE rules: timers level line of sight, spawn heat on, objective normal, nhe extras off, "
                 "drop secondary always, nhe mode by vehicles, starting frags 2, ball melee lethal"]
        self.assertTrue(self.run_check("ball_melee_lethal", rules + cap)[0])
        self.assertFalse(self.run_check("ball_melee_lethal", [rules[0].replace("lethal", "stock")] + cap)[0])
        self.assertFalse(self.run_check("ball_melee_lethal", cap)[0])

    def test_unknown_check(self):
        self.assertFalse(self.run_check("nope", [])[0])


if __name__ == "__main__":
    unittest.main()
