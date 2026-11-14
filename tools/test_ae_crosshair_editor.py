"""(0.7.7) the CROSSHAIR screen: every value row of its tabs (and CUSTOM COLOR's RED, GREEN, BLUE), stepped in the real screen
(headless, the ae_test harness), changes its own field of the crosshair and nothing else. The log's share codes (ae player
settings: ... set display.crosshair_N <code>) are decoded here and each step's change is held to the bytes its key owns (the
screen logs "ae crosshair screen: set <key> <value>"); the CRC alone may differ besides.

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_crosshair_editor.py
"""
import importlib.util
import re
import shutil
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
_spec = importlib.util.spec_from_file_location("ae_settings_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

CROCKFORD = "0123456789ABCDEFGHJKMNPQRSTVWXYZ"
# the code's 23 bytes (ae_crosshair_rules.c code_bytes): 0 version, 1 shape, 2 flags (inner 1, outer 2, frame 4, outline 8,
# movement 16, firing 32, centre 64-192), 3-5 colour, 6 opacity, 7-10 inner, 11-14 outer, 15-17 frame, 18 centre size, 19-20
# outline, 21-22 the CRC
OWNED = {
    "xh.color": {3, 4, 5}, "xh.opacity": {6}, "xh.centre": {2}, "xh.centre_size": {18},
    "xh.inner_on": {2}, "xh.inner_length": {7}, "xh.inner_thickness": {8}, "xh.inner_offset": {9}, "xh.inner_opacity": {10},
    "xh.outer_on": {2}, "xh.outer_length": {11}, "xh.outer_thickness": {12}, "xh.outer_offset": {13}, "xh.outer_opacity": {14},
    "xh.frame_on": {2}, "xh.frame_size": {15}, "xh.frame_thickness": {16}, "xh.frame_opacity": {17},
    "xh.outline_on": {2}, "xh.outline_thickness": {19}, "xh.outline_opacity": {20},
    "xh.movement": {2}, "xh.firing": {2},
    # (CUSTOM COLOR's RED / GREEN / BLUE: the colour's bytes)
    "xh.color_hex": {3, 4, 5},
}


def decode(code):
    body = code.replace("-", "")[3:]
    bits = "".join(f"{CROCKFORD.index(c):05b}" for c in body)
    return [int(bits[i * 8:i * 8 + 8], 2) for i in range(23)]


def presses():
    """every value row of every tab, from the SHAPE tab's first row: right on each (the first twice: its first step has no code
    before it to hold a change against), down to the next; RB to the next tab (its first item focused)"""
    right, down, rb = "pad:right", "pad:down", "pad:rb"
    out = [right, right, down, right]                       # SHAPE: CENTER, CENTER SIZE
    out += [rb, down, down, right, down, right, down, right, down, right]   # COLOR: (the palette's one row, CUSTOM COLOR) RED GREEN
    #                                                                     BLUE OPACITY
    out += [rb, right] + [down, right] * 9                  # LINES: inner, then outer
    out += [rb, right] + [down, right] * 6                  # FRAME: frame, then outline
    out += [rb, right, down, right]                         # MOTION
    return out


def steps(text):
    """[(key, bytes before, bytes after)] for every 'set <key>' whose share code changed"""
    out, previous, pending = [], None, None
    for line in text.splitlines():
        match = re.search(r"set display\.crosshair_\d (?:[^|]*\|)?(AE1-[0-9A-Z-]+)", line)
        if match:
            pending = decode(match.group(1))
            continue
        match = re.search(r"ae crosshair screen: set (xh\.[a-z_]+) ", line)
        if match and pending is not None:
            out.append((match.group(1), previous, pending))
            previous, pending = pending, None
    return out


@pytest.fixture(scope="module")
def run():
    cfg = menus.config()
    if not cfg:
        pytest.skip("needs the ae_test harness config and a built game ($AE_MENUS_BUILD)")
    pressed = presses()
    out = menus.out_dir(cfg, "crosshair-editor")
    try:
        result, text, _ = menus.play(cfg, out, "xh-every-row", views=35, window="1920x1080", shots=1,
                                     exit_after=menus.LEAD_IN + len(pressed) + 8,
                                     env={"HALO_ARENA_MENUS_SCALE": "100", "HALO_AE_TEST_SETTINGS_GROUP": "10",
                                          "HALO_AE_TEST_SETTINGS_PLAYERS": "1"},
                                     test_input=menus.menu_input(*pressed))
        yield text
    finally:
        shutil.rmtree(out, ignore_errors=True)


def test_every_row_changes_only_its_own_field(run):
    text = run
    seen = set()
    for key, before, after in steps(text):
        assert key in OWNED, f"a step on {key} is not an editor row"
        if before is None:
            continue
        changed = {i for i in range(21) if before[i] != after[i]}
        assert changed and changed <= OWNED[key], f"stepping {key} changed bytes {sorted(changed)}, its own are {sorted(OWNED[key])}"
        seen.add(key)
    assert set(OWNED) - {"xh.color"} <= seen, sorted(set(OWNED) - seen)
