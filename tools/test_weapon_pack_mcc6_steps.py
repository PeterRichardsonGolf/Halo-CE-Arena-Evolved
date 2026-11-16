"""tools/weapon_pack/mcc6_steps.py: the display shader twins step, the HUD subtree swap, the bitmap format refusal."""
import hashlib
import os
import struct
import sys
import tempfile
import unittest
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "weapon_pack"))
import aepack as ap  # noqa: E402
import mcc6_steps as st  # noqa: E402

BULLET = "cmt\\weapons\\_shared\\battle_rifle\\shaders\\battle_rifle_display_bullet.shader_transparent_chicago"


def bitmap_tag(formats, flags=0, count_fixup=True):
    body = bytearray(96 + 12 + 48 * len(formats))
    struct.pack_into("<I", body, 96, len(formats))
    for i, fmt in enumerate(formats):
        struct.pack_into("<h", body, 108 + i * 48 + 12, fmt)
    fixups = [ap.Fixup(100, ap.FIXUP_POINTER, 108)] if count_fixup else []
    return ap.Tag(ap.GROUP_BITMAP, "b", bytes=bytes(body), fixups=fixups, flags=flags)


class StepsTest(unittest.TestCase):
    def recipe(self):
        return {"steps": [{"op": "copy", "tag": "x"}, {"op": st.OP_TWINS, "layer": "fix2", "tag": "m.gbxmodel",
                                                        "from": ["cmt", "m.gbxmodel"], "bullet": BULLET}]}

    def test_twins_step_expands_to_the_pilots_edit(self):
        out = st.expand(self.recipe())
        step = out["steps"][1]
        self.assertEqual(step["op"], "edit")
        self.assertEqual(step["edits"], [["set", "shaders[2].shader", BULLET],
                                         ["set", "shaders[3].shader", st.CLASSIC_COMPASS],
                                         ["set", "shaders[4].shader", st.CLASSIC_NUMBERS],
                                         ["set", "shaders[5].shader", st.CLASSIC_NUMBERS]])
        self.assertEqual(self.recipe()["steps"][1]["op"], st.OP_TWINS, "the input is not changed")

    def test_twins_step_refusals(self):
        r = self.recipe()
        del r["steps"][1]["bullet"]
        with self.assertRaises(st.StepError):
            st.expand(r)
        r = self.recipe()
        r["steps"][1]["twins"] = {"9": BULLET}
        with self.assertRaises(st.StepError):
            st.expand(r)

    def test_model_shader_list_check(self):
        names = ["a", "b", "x_mcc", "y_mcc", "z_mcc", "w_mcc"]
        st.check_model_shaders(names, ["x_mcc", "y_mcc", "z_mcc", "w_mcc"])
        with self.assertRaises(st.StepError):
            st.check_model_shaders(names, ["x_mcc", "other", "z_mcc", "w_mcc"])
        with self.assertRaises(st.StepError):
            st.check_model_shaders(names[:4], [])

    def test_bitmap_formats(self):
        self.assertEqual(st.bitmap_format_problems([bitmap_tag([14, 11, 16])]), [])
        bad = st.bitmap_format_problems([bitmap_tag([14, 18])])
        self.assertEqual(len(bad), 1)
        self.assertIn("format 18", bad[0][1])
        self.assertEqual(st.bitmap_format_problems([bitmap_tag([st.A8Y8])]), [])
        meter = st.bitmap_format_problems([bitmap_tag([st.A8Y8], flags=ap.FLAG_METER_COVERAGE)])
        self.assertIn("a8y8", meter[0][1])
        self.assertIn("cannot be read", st.bitmap_format_problems([bitmap_tag([14], count_fixup=False)])[0][1])

    def test_hud_subtree_swap(self):
        with tempfile.TemporaryDirectory() as d:
            mcc, classic, out = (os.path.join(d, n) for n in ("mcc.zip", "classic.zip", "out.zip"))
            with zipfile.ZipFile(mcc, "w") as z:
                z.writestr("m/tags/cmt/a.bitmap", b"A")
                z.writestr("m/tags/cmt/ui/hudv4.1/h.bitmap", b"BC7")
            with zipfile.ZipFile(classic, "w") as z:
                z.writestr("c/tags/cmt/ui/hudv4.1/h.bitmap", b"OLD")
                z.writestr("c/tags/cmt/other.bitmap", b"no")
            sha = lambda p: hashlib.sha256(open(p, "rb").read()).hexdigest()
            kw = dict(mcc_root="m/tags/", classic_root="c/tags/", subtree="cmt/ui/hudv4.1/")
            with self.assertRaises(st.StepError):
                st.hud_subtree_swap(mcc, classic, out, expect={"mcc": "0", "classic": "0", "out": "0"}, **kw)
            # first run with a wrong output pin to learn the sha, then pin it
            try:
                st.hud_subtree_swap(mcc, classic, out, expect={"mcc": sha(mcc), "classic": sha(classic), "out": "0"}, **kw)
            except st.StepError:
                pass
            want = sha(out)
            st.hud_subtree_swap(mcc, classic, out, expect={"mcc": sha(mcc), "classic": sha(classic), "out": want}, **kw)
            with zipfile.ZipFile(out) as z:
                self.assertEqual(z.read("m/tags/cmt/ui/hudv4.1/h.bitmap"), b"OLD")
                self.assertEqual(z.read("m/tags/cmt/a.bitmap"), b"A")
                self.assertNotIn("m/tags/cmt/other.bitmap", z.namelist())

    def test_shared_mode_swaps_only_members_both_archives_have(self):
        with tempfile.TemporaryDirectory() as d:
            mcc, classic, out = (os.path.join(d, n) for n in ("mcc.zip", "classic.zip", "out.zip"))
            with zipfile.ZipFile(mcc, "w") as z:
                z.writestr("m/tags/cmt/effects/impacts/both.effect", b"MCC-FLAG")
                z.writestr("m/tags/cmt/effects/impacts/mcc_only.effect", b"MCC-ONLY")
            with zipfile.ZipFile(classic, "w") as z:
                z.writestr("c/tags/cmt/effects/impacts/both.effect", b"CLASSIC")
                z.writestr("c/tags/cmt/effects/impacts/classic_only.effect", b"NO")
            sha = lambda p: hashlib.sha256(open(p, "rb").read()).hexdigest()
            kw = dict(mcc_root="m/tags/", classic_root="c/tags/", subtrees=[("cmt/effects/impacts/", "shared")])
            try:
                st.hud_subtree_swap(mcc, classic, out, expect={"mcc": sha(mcc), "classic": sha(classic), "out": "0"}, **kw)
            except st.StepError:
                pass
            st.hud_subtree_swap(mcc, classic, out, expect={"mcc": sha(mcc), "classic": sha(classic), "out": sha(out)}, **kw)
            with zipfile.ZipFile(out) as z:
                self.assertEqual(z.read("m/tags/cmt/effects/impacts/both.effect"), b"CLASSIC")
                self.assertEqual(z.read("m/tags/cmt/effects/impacts/mcc_only.effect"), b"MCC-ONLY")
                self.assertNotIn("m/tags/cmt/effects/impacts/classic_only.effect", z.namelist())


if __name__ == "__main__":
    unittest.main()
