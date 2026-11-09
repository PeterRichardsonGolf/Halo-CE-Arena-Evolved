"""The PlayStation button icons' decisions (port/linux/src/ae_buttons_rules.c, DamnationCE v0.3.41's feature in AE's
files): which texture a whole icon or a sheet's sprite draws, by the controller last used."""

import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run  # noqa: E402

CASES = ["scheme", "family", "whole", "placeholder", "sprites", "ce_sequence"]

# a fault in ae_buttons_rules.c, the function it is in, and the case that must catch it
NEGATIVE_CONTROLS = {
    # (the PlayStation texture for every controller: the Xbox icons gone)
    "whole-always": (("return playstation_now != 0;", "return 1;"), "ae_buttons_draw_playstation", "whole"),
    # (the PlayStation gate on sprites dropped)
    "sprites-ungated": (("if (!playstation_now || sequence_index < 0", "if (sequence_index < 0"),
                        "ae_buttons_sprite_wanted", "sprites"),
    # (the pixels' check dropped: another map's sheet drawn over)
    "pixels-unchecked": (("return pixels_match != 0;", "return 1;"), "ae_buttons_sprite_wanted", "sprites"),
    # (a Nintendo pad taken for a PlayStation one)
    "scheme-loose": (("return scheme == 2;", "return scheme >= 2;"), "ae_buttons_scheme_playstation", "scheme"),
    # (a Nintendo pad taken for a PlayStation one's family, or the reverse)
    "family-loose": (("return AE_BUTTON_FAMILY_NINTENDO;", "return AE_BUTTON_FAMILY_PLAYSTATION;"),
                     "ae_buttons_scheme_family", "family"),
    # (a glyph of another family drawn)
    "glyph-any-family": (("glyph_family == family_now", "1"), "ae_buttons_glyph_is_drawn", "family"),
    # (a Custom Edition map's sequences not shifted: the map's own A, B, X, Y slots drawn as PlayStation's)
    "ce-unshifted": (("return sequence_index - first_sequence;", "return sequence_index;"), "ae_buttons_ce_sequence",
                     "ce_sequence"),
}

FUNCTIONS = ("ae_buttons_scheme_playstation", "ae_buttons_scheme_family", "ae_buttons_glyph_is_drawn", "ae_buttons_draw_playstation", "ae_buttons_sprite_wanted",
             "ae_buttons_ce_sequence")


def generated(fault=None, faulty_function=None):
    header = read("port/linux/src/ae_buttons.h")
    source = read("port/linux/src/ae_buttons_rules.c")
    enum = re.search(r"enum\s*\{[^}]*AE_BUTTONS_NONE[^}]*\};", header).group(0)
    enum += "\n" + re.search(r"enum\s*\{[^}]*AE_BUTTON_FAMILY_OWN[^}]*\};", header).group(0)
    text = ""
    for name in FUNCTIONS:
        code = function(source, name)
        if fault and name == faulty_function:
            code = mutated(code, *fault)
        text += code + "\n"
    return (("config.inc", enum + "\n"), ("under_test.inc", text))


@pytest.mark.parametrize("case", CASES)
def test_case(case):
    status, output = run(build("ae_buttons_choice", generated()), case)
    assert status == 0, output


@pytest.mark.parametrize("control", NEGATIVE_CONTROLS)
def test_negative_control(control):
    fault, faulty_function, case = NEGATIVE_CONTROLS[control]
    status, output = run(build("ae_buttons_choice", generated(fault, faulty_function)), case)
    assert status == CHECK_FAILED, f"ae_buttons_rules.c with a fault ({control}) passed '{case}': the test cannot see it"
