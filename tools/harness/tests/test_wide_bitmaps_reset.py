"""Custom Edition maps' wide bitmaps, map after map: each map's are made narrower; the table is forgotten with the
map's tags (port/linux/game/ae_wide_bitmaps.c)."""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run

RESET = ("\tmemset(wide, 0, sizeof(wide));\n\twide_count = 0;\n", "")


def generated(fault=None):
    source = read("port/linux/game/ae_wide_bitmaps.c")
    code = re.search(r"^enum\n\{\n\tFORMAT_X8R8G8B8.*?^\};\n", source, re.M | re.S).group(0)
    code += re.search(r"^struct wide_bitmap\n\{.*?^\};\n", source, re.M | re.S).group(0)
    code += "static struct wide_bitmap wide[MAXIMUM_REMEMBERED];\nstatic int wide_count;\n"
    for name in ("remembered", "levels_left_out", "ae_wide_bitmaps_tags_unloaded", "ae_wide_bitmap_prepare"):
        code += function(source, name) + "\n"
    if fault:
        code = mutated(code, *fault)
    return (("under_test.inc", code),)


def test_maps_in_turn():
    status, output = run(build("wide_bitmaps_reset", generated()), "maps-in-turn")
    assert status == 0, output


def test_negative_control():
    status, output = run(build("wide_bitmaps_reset", generated(RESET)), "maps-in-turn")
    assert status == CHECK_FAILED, output
