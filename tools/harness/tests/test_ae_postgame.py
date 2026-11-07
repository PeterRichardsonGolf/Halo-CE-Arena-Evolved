"""The carnage report's confirm (port/linux/game/ae_postgame_rules.c, owner 2026-10-15): B asks "LEAVE THE GAME?"
instead of leaving at once; A on it leaves, B stays; each press it takes is waited out (the stock code reads held
buttons, a press is a run of frames); A with it closed is the stock code's (the host's pick game)."""

import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run  # noqa: E402

CASES = ["b_asks", "a_leaves", "b_stays", "a_stock", "release_first"]

# a fault in ae_postgame_rules.c and the case that must catch it
NEGATIVE_CONTROLS = {
    # (B leaves at once, as before the confirm)
    "b-leaves": (("state->open = TRUE;\n\t\tstate->wait_release = TRUE;\n\t\treturn AE_POSTGAME_HELD;",
                  "return AE_POSTGAME_LEAVE;"), "b_asks"),
    # (no wait for the release: the opening B closes the confirm on its next frame)
    "no-release": (("\t\tif (accept_down || back_down)\n\t\t\treturn AE_POSTGAME_HELD;\n", ""), "b_asks"),
    # (B on the confirm leaves too)
    "b-on-confirm-leaves": (("\tif (back_down)\n\t{\n\t\tstate->open = FALSE;\n\t\tstate->wait_release = TRUE;\n\t}\n"
                             "\treturn AE_POSTGAME_HELD;", "\tif (back_down)\n\t\treturn AE_POSTGAME_LEAVE;\n"
                             "\treturn AE_POSTGAME_HELD;"), "b_stays"),
    # (B first: A with B held opens the confirm, where the stock code picks the game)
    "b-first": (("if (accept_down || !back_down)", "if (!back_down)"), "a_stock"),
}


def generated(fault=None):
    header = read("port/linux/game/ae_postgame.h")
    source = read("port/linux/game/ae_postgame_rules.c")
    types = re.search(r"enum ae_postgame_action\s*\{.*?\};", header, re.S).group(0) + "\n" + \
        re.search(r"struct ae_postgame_confirm\s*\{.*?\};", header, re.S).group(0) + "\n"
    code = function(source, "ae_postgame_confirm_step")
    if fault:
        code = mutated(code, *fault)
    return (("config.inc", types), ("under_test.inc", code + "\n"))


@pytest.mark.parametrize("case", CASES)
def test_case(case):
    status, output = run(build("ae_postgame_choice", generated()), case)
    assert status == 0, output


@pytest.mark.parametrize("control", NEGATIVE_CONTROLS)
def test_negative_control(control):
    fault, case = NEGATIVE_CONTROLS[control]
    status, output = run(build("ae_postgame_choice", generated(fault)), case)
    assert status == CHECK_FAILED, f"ae_postgame_rules.c with a fault ({control}) passed '{case}': the test cannot see it"
