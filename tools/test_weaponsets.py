"""tools/weapon_cache/weaponsets.py and weaponsets/*.txt: the built-in weapon sets and their generated table."""
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "weapon_cache"))
import weaponsets as ws  # noqa: E402


class WeaponsetsTest(unittest.TestCase):
    def test_sets_in_wire_order(self):
        self.assertEqual(ws.SETS, ("casual", "comp", "btb"))

    def test_every_file_has_its_header_and_format(self):
        for name in ws.SETS:
            lines = [l.strip() for l in ws.text_of(name).split("\n") if l.strip() and not l.strip().startswith("#")]
            self.assertEqual(lines[0], "weaponset 1 %s" % name, name)
            self.assertEqual(lines[1], "format 1", name)
            self.assertTrue(any(l.startswith("row ") for l in lines), name)

    def test_shotgun_is_a_power_pad_everywhere(self):
        """The stock shotgun is CLOSE POWER, static, in every set; the auto shotgun a CLOSE BASE rack"""
        for name in ws.SETS:
            rows = [l for l in ws.text_of(name).split("\n") if l.startswith("row CLOSE POWER")]
            self.assertEqual(len(rows), 1, name)
            self.assertIn("logic static", rows[0])
            self.assertIn("stock:shotgun", rows[0])
            self.assertNotIn("stock:shotgun", "".join(l for l in ws.text_of(name).split("\n") if l.startswith("row CLOSE BASE")))

    def test_rack_ammo_trial(self):
        """Casual keeps the classic walk-over refill, comp swaps only"""
        self.assertIn("rack_ammo topup", ws.text_of("casual"))
        self.assertIn("rack_ammo fresh", ws.text_of("comp"))
        self.assertIn("extends casual", ws.text_of("btb"))

    def test_no_weapon_config_named(self):
        """A pool entry names a weapon alone: the spawn places no config, so a `<key>:<config>` would only log a line"""
        for name in ws.SETS:
            for line in ws.text_of(name).split("\n"):
                if line.startswith("row ") and " weapons " in line:
                    for ref in line.split(" weapons ", 1)[1].split():
                        self.assertFalse(":" in ref and not ref.startswith("stock:"), "%s: %s" % (name, ref))

    def test_literal_escapes(self):
        self.assertEqual(ws.c_literal('a "b"\n'), '\t"a \\"b\\"\\n"')
        self.assertEqual(ws.c_literal(""), '\t""')

    def test_generated_table_matches_checked_in(self):
        text = ws.generate()
        self.assertIn('AE_WEAPONSET_TEXT("casual",', text)
        self.assertIn('\t"row CLOSE POWER logic static respawn 120 weapons stock:shotgun\\n"', text)
        with open(ws.DEFAULT_INC) as handle:
            self.assertEqual(handle.read(), text, "run: python3 tools/weapon_cache/weaponsets.py gen")


if __name__ == "__main__":
    unittest.main()
