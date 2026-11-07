"""tools/weapon_cache/registry.py and registry.json: the id rules (design 4.3), the columns, the generated table."""
import copy
import json
import os
import subprocess
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "weapon_cache"))
import registry as rg  # noqa: E402

REPO = os.path.abspath(os.path.join(HERE, ".."))


class RegistryTest(unittest.TestCase):
    def setUp(self):
        self.data = rg.load()

    def weapons(self):
        return {r["id"]: r for r in self.data["rows"] if r["kind"] == "weapon"}

    def test_file_is_sound(self):
        self.assertEqual(rg.check_rows(self.data), [])

    def test_today_slots_are_ids(self):
        w = self.weapons()
        order = ["battle_rifle", "smg", "smg_silenced", "carbine", "ar_silenced", "dmr", "evo_rifle", "needler",
                 "sniper_bolt", "magnum_scoped", "brute_shot", "ar_modern", "sidearm", "energy_sword", "smg_tactical",
                 "pdw", "revolver", "focus_beam", "spiker", "ravager", "auto_shotgun", "grenade_launcher",
                 "concussion_rifle", "gravity_hammer"]
        self.assertEqual([w[i]["key"] for i in range(1, 25)], order)

    def test_id_ranges(self):
        by_id = {r["id"]: r for r in self.data["rows"]}
        self.assertEqual(by_id[0x02000001]["key"], "vehicle_damage")
        self.assertEqual(by_id[0x02000002]["key"], "bomb")
        self.assertEqual(by_id[0x03000000]["key"], "emp")
        self.assertEqual(by_id[0x03000001]["status"], "held")
        self.assertEqual(by_id[0x04000001]["kind"], "base")

    def test_every_active_weapon_has_every_column(self):
        for row in self.weapons().values():
            self.assertEqual(rg.check_weapon_columns(row), [], row["key"])

    def test_missing_column_and_zero_bot_value_are_refused(self):
        bad = copy.deepcopy(self.data)
        row = next(r for r in bad["rows"] if r["key"] == "smg_silenced")
        del row["feed"]
        next(r for r in bad["rows"] if r["key"] == "carbine")["bot"]["value"] = 0
        problems = rg.check_rows(bad)
        self.assertTrue(any("missing column feed" in p for p in problems))
        self.assertTrue(any("bot value" in p for p in problems))

    def test_duplicates_and_legacy_rules(self):
        bad = copy.deepcopy(self.data)
        bad["rows"].append(copy.deepcopy(bad["rows"][0]))
        self.assertTrue(any("duplicate id" in p for p in rg.check_rows(bad)))
        bad = copy.deepcopy(self.data)
        next(r for r in bad["rows"] if r["id"] == 3)["legacy_slot"] = 4
        self.assertTrue(any("legacy_slot" in p for p in rg.check_rows(bad)))

    def test_append_only(self):
        old = self.data
        new = copy.deepcopy(old)
        self.assertEqual(rg.check_append_only(old, new), [])
        new["rows"] = [r for r in new["rows"] if r["id"] != 5]
        self.assertTrue(any("removed" in p for p in rg.check_append_only(old, new)))
        new = copy.deepcopy(old)
        next(r for r in new["rows"] if r["id"] == 5)["key"] = "other"
        self.assertTrue(any("changed" in p for p in rg.check_append_only(old, new)))

    def test_next_id_is_above_all_used(self):
        self.assertEqual(rg.next_id(self.data, "weapon"), 25)
        self.assertEqual(rg.next_id(self.data, "config"), 0x01000001)

    def test_add_assigns_and_bumps_revision(self):
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "r.json")
            rg.save(self.data, path)
            self.assertEqual(rg.main(["--registry", path, "add", "--kind", "weapon", "--key", "zzz"]), 0)
            new = rg.load(path)
            self.assertEqual(new["revision"], self.data["revision"] + 1)
            row = next(r for r in new["rows"] if r["key"] == "zzz")
            self.assertEqual((row["id"], row["status"]), (25, "held"))

    def test_generated_table_matches_checked_in(self):
        text = rg.generate(self.data)
        self.assertIn('AE_WEAPON_REGISTRY_ROW(16, "pdw"', text)
        self.assertIn('AE_WEAPON_REGISTRY_ALIAS(7, "evo")', text)
        if os.path.exists(rg.DEFAULT_INC):
            with open(rg.DEFAULT_INC) as handle:
                self.assertEqual(handle.read(), text, "run: python3 tools/weapon_cache/registry.py gen")

    def test_slots_facade_reads_the_registry(self):
        """T2-4: ae_weapon_slots.c has no table of its own any more (names, displays, layout words and fallbacks are the
        registry's rows); display names are unique, the pin of what the facade says is port/linux/tests/ae_weapon_slots_test.c"""
        path = os.path.join(REPO, "port", "linux", "game", "ae_weapon_slots.c")
        text = open(path).read()
        self.assertIn('#include "ae_weapon_registry.h"', text)
        self.assertNotIn('"SUPPRESSED SMG"', text)
        displays = [row["display"] for row in self.weapons().values()]
        self.assertEqual(len(displays), len(set(displays)))


if __name__ == "__main__":
    unittest.main()
