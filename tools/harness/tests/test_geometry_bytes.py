"""The real geometry_bytes_add (custom_edition_geometry.c): a hostile Custom Edition map's parts, all naming one block
of vertices, are refused at MAXIMUM_GEOMETRY_BYTES instead of wrapping the geometry's size to a small allocation.
(Arena Evolved: its port, ce_geometry_bytes_add in ce_models.c, which ce_models_check and ce_bsp_check call; tested
under OpenCE's name)"""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, constant, function, mutated, read, run  # noqa: E402

CASES = ["counts-real-parts", "refuses-before-wrapping", "refuses-huge-counts", "takes-exactly-the-most"]

# a fault in geometry_bytes_add and the case that must catch it
NEGATIVE_CONTROLS = {
    "no-limit": (("if (added > MAXIMUM_GEOMETRY_BYTES - *bytes)", "if (0)"), "refuses-before-wrapping"),
    "no-count-check": (("if (vertex_count > MAXIMUM_GEOMETRY_BYTES / vertex_size ||", "if (0 &&"),
                       "refuses-huge-counts"),
}


def generated(fault=None):
    geometry = read("port/linux/game/ce_models.c")
    config = f"#define MAXIMUM_GEOMETRY_BYTES {constant(geometry, 'MAXIMUM_GEOMETRY_BYTES')}\n"
    code = function(geometry, "ce_geometry_bytes_add").replace("ce_geometry_bytes_add", "geometry_bytes_add")
    if fault:
        code = mutated(code, *fault)
    return (("config.inc", config), ("under_test.inc", code + "\n"))


@pytest.mark.parametrize("case", CASES)
def test_case(case):
    status, output = run(build("geometry_bytes", generated()), case)
    assert status == 0, output


@pytest.mark.parametrize("control", NEGATIVE_CONTROLS)
def test_negative_control(control):
    fault, case = NEGATIVE_CONTROLS[control]
    status, output = run(build("geometry_bytes", generated(fault)), case)
    assert status == CHECK_FAILED, f"geometry_bytes_add with a fault ({control}) passed '{case}': the test cannot see it"
