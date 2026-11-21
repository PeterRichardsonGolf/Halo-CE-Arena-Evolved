"""A Custom Edition BSP's materials as ce_bsp_loaded converts them in place (port/linux/game/ae_ce_bsp_apart.h):
shared blocks and whole shared materials pass, blocks over another's materials at another offset and vertex data
over the structure, the lightmaps or a material are refused."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, mutated, read, run

FAULTS = {
    "grid": ("\t\t\tif ((blocks[index].start - regions[region_count - 1].start) % sizeof(struct structure_material))\n",
             "\t\t\tif (0)\n"),
    "data": ("\t\t\tif (ae_ce_bsp_span_hits(regions, region_count, (uintptr_t)data, (uintptr_t)data + size) ||\n",
             "\t\t\tif (0 ||\n"),
}


def generated(fault=None):
    code = read("port/linux/game/ae_ce_bsp_apart.h")
    if fault:
        code = mutated(code, *fault)
    return (("under_test.inc", code),)


@pytest.mark.parametrize("case", ["kept", "refused"])
def test_case(case):
    status, output = run(build("ce_bsp_apart", generated()), case)
    assert status == 0, output


@pytest.mark.parametrize("fault", FAULTS)
def test_negative_control(fault):
    status, output = run(build("ce_bsp_apart", generated(FAULTS[fault])), "refused")
    assert status == CHECK_FAILED, output
