"""Blood drops' kinds start over with another map's tags, not only when the game clock goes back."""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run

GENERATION = ((" || generation != ae_tag_table_generation())", ")"),
              ("\t\tgeneration = ae_tag_table_generation();\n", "\t\t(void)generation;\n"))


def generated(fault=None):
    source = read("port/linux/game/ae_blood_drops.c")
    code = re.search(r"^enum\n\{\n\tAE_BLOOD_DROPS_MAXIMUM.*?^\};\n", source, re.M | re.S).group(0)
    start = source.index("static struct\n{\n\tlong decal;")
    code += source[start:source.index("static long ae_blood_drops_time = NONE;\n") + len("static long ae_blood_drops_time = NONE;\n")]
    code += function(source, "ae_blood_drops_kind") + "\n"
    for before, after in fault or ():
        code = mutated(code, before, after)
    return (("under_test.inc", code),)


def test_new_map():
    status, output = run(build("blood_drop_kinds", generated()), "new-map")
    assert status == 0, output


def test_negative_control():
    status, output = run(build("blood_drop_kinds", generated(GENERATION)), "new-map")
    assert status == CHECK_FAILED, output
