"""AE's menus in a real game, headless: the test screen (debug.ae_test_screen) through the ae_test harness
(tools/ae_test, docs/testing.md), each game with its own data root, save root and debug.txt.

- the screen opens at the main menu, draws, and takes input (12 downs move the focus to item 12);
- its translucent swatch is one blend over the game's picture, not built up over frames in the back buffer;
- buttons pressed on it, and the B that closes it, never reach the game's menus behind it (the main menu keeps its
  first item focused);
- a contact sheet of the screen at 16:9, 4:3, in 2 and 4 synthetic split-screen views, with supersampling and at
  the original 480 lines (also: python3 tools/test_ae_menus.py sheet).

Runs only when asked, as its games take minutes: $AE_MENUS_BUILD names the game to play (a folder with halo, a
commit or a named build of the harness's config), and the harness's config must have a data_dir with the maps.
$AE_TEST_HARNESS points at another checkout's tools/ae_test to run with (a newer harness). Skips otherwise, as on
CI and in a plain pytest run of tools/. Each game is 30 s or more; the three tests play ten.

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_menus.py
"""
import importlib.util
import os
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
HARNESS = Path(os.environ.get("AE_TEST_HARNESS") or ROOT / "tools" / "ae_test").expanduser()
# tools/ae_test/harness.py (or $AE_TEST_HARNESS's), loaded by path when a test needs it (load_harness). The harness
# imports its sibling modules (record.py, ...) when it runs, so its folder goes on sys.path then, but last: it can't
# shadow another test's imports of the same names
harness = None


def load_harness():
    global harness
    if harness is None:
        if str(HARNESS) not in sys.path:
            sys.path.append(str(HARNESS))
        spec = importlib.util.spec_from_file_location("ae_menus_test_harness", HARNESS / "harness.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        harness = module
    return harness

LEAD_IN = 12  # seconds of waits before the first press: the game starts and the main menu (and the screen) opens
EXIT_AFTER = 34
SWATCH = (1860, 20, 1900, 60)  # the screen's swatch at 1920x1080 (layout units = pixels): x0, y0, x1, y1
SHEET_CASES = (
    # name, window, views, more environment
    ("16x9", "1920x1080", 1, {}),
    ("4x3", "1280x960", 1, {}),
    ("2p", "1920x1080", 2, {}),
    ("4p", "1920x1080", 4, {}),
    ("ssaa2x-720", "1280x720", 1, {"HALO_ANTI_ALIASING": "ssaa2x"}),
    ("original-480", "1920x1080", 1, {"HALO_RESOLUTION_SCALING": "original"}),
)


def build():
    return os.environ.get("AE_MENUS_BUILD") or None


def config():
    if not build() or not (HARNESS / "harness.py").exists():
        return None
    cfg = load_harness().load_config()
    data = cfg.get("data_dir")
    if not data or not (harness.expand(data) / "maps").exists():
        return None
    return cfg


def out_dir(cfg, name):
    folder = harness.expand(cfg["out_dir"]) / "ae_menus" / f"{harness.stamp()}-{name}"
    folder.mkdir(parents=True, exist_ok=True)
    return folder


def menu_input(*buttons):
    """HALO_TEST_INPUT: the lead-in's waits, then one button a second"""
    return "menu:" + " ".join(["wait"] * LEAD_IN + list(buttons))


def play(cfg, out, name, views=1, test_input=None, window="1920x1080", env=None, shots=60, exit_after=EXIT_AFTER):
    """one game through the harness: (result, its debug.txt, its PNGs by frame)"""
    spec = {"name": name, "build": build(), "window": window, "exit_after": exit_after, "screenshots": shots,
            "env": dict({"HALO_ARENA_MENUS": "1", "HALO_AE_TEST_SCREEN": str(views)}, **(env or {}))}
    if test_input:
        spec["test_input"] = test_input
    result = harness.play(cfg, spec, out / name)
    debug = out / name / "debug.txt"
    text = debug.read_text(errors="replace") if debug.exists() else ""
    pngs = sorted((out / name).rglob("frame*.png"))
    return result, text, pngs


def focus_lines(text):
    return [int(line.rsplit(" ", 1)[1]) for line in text.splitlines() if "ae menus: focus " in line]


def brightness_median(image, inside, outside=None):
    """the median brightness ((r + g + b) / 3) of the pixels in the box inside (x0, y0, x1, y1), or, given
    outside, of those in outside but not in inside (a ring); medians, as the menu's stars move behind"""
    w, h, rgb = image
    box = outside or inside
    values = []
    for y in range(max(0, box[1]), min(h, box[3])):
        for x in range(max(0, box[0]), min(w, box[2])):
            if outside and inside[0] <= x < inside[2] and inside[1] <= y < inside[3]:
                continue
            o = (y * w + x) * 3
            values.append((rgb[o] + rgb[o + 1] + rgb[o + 2]) / 3)
    values.sort()
    return values[len(values) // 2]


def tile(pngs, out, width=960, columns=3, gap=8):
    """a contact sheet: each picture scaled to width (nearest pixel), columns wide, on dark grey"""
    tiles = []
    for p in pngs:
        w, h, rgb = harness.read_png(p)
        th = max(1, round(h * width / w))
        xs = [min(w - 1, int(x * w / width)) * 3 for x in range(width)]
        idx = [o + c for o in xs for c in range(3)]
        data = bytearray()
        for y in range(th):
            row = rgb[min(h - 1, int(y * h / th)) * w * 3:][:w * 3]
            data += bytes(map(row.__getitem__, idx))
        tiles.append((th, data))
    rows = [tiles[i:i + columns] for i in range(0, len(tiles), columns)]
    sheet_w = columns * width + (columns + 1) * gap
    heights = [max(t[0] for t in r) for r in rows]
    sheet_h = sum(heights) + (len(rows) + 1) * gap
    sheet = bytearray(b"\x20" * (sheet_w * sheet_h * 3))
    y0 = gap
    for r, rh in zip(rows, heights):
        for i, (th, data) in enumerate(r):
            x0 = gap + i * (width + gap)
            for y in range(th):
                o = ((y0 + y) * sheet_w + x0) * 3
                sheet[o:o + width * 3] = data[y * width * 3:(y + 1) * width * 3]
        y0 += rh + gap
    Path(out).write_bytes(harness.png_bytes(sheet_w, sheet_h, sheet))
    return out


@pytest.fixture(scope="module")
def cfg():
    c = config()
    if not c:
        pytest.skip("needs the ae_test harness config (data_dir with maps) and a built game ($AE_MENUS_BUILD)")
    return c


def test_test_screen_draws_and_takes_input(cfg):
    out = out_dir(cfg, "input")
    result, text, pngs = play(cfg, out, "input", test_input=menu_input(*["down"] * 12))
    assert result.get("status") == "PASS", result.get("why")
    assert "ae menus: test screen (views 1)" in text, "the test screen never opened"
    focus = focus_lines(text)
    assert focus and focus[-1] == 12, f"12 downs: focus {focus}"
    assert focus == list(range(1, 13)), f"one step a press: {focus}"
    assert len(pngs) >= 10, "too few screenshots"

    # the swatch (white, alpha 0.25) is one blend over the picture, early and late: no build-up in the back
    # buffer the game keeps from frame to frame (over a background b, one blend is 0.75 b + 64, two 0.56 b + 112)
    late = harness.read_png(pngs[-1])
    early = harness.read_png(pngs[-10])
    assert late[0] == 1920 and late[1] == 1080, "the picture is not 1920x1080"
    inner = (SWATCH[0] + 4, SWATCH[1] + 4, SWATCH[2] - 4, SWATCH[3] - 4)
    ring = (SWATCH[0] - 12, SWATCH[1] - 12, SWATCH[2] + 12, SWATCH[3] + 12)
    blends = []
    for image in (early, late):
        swatch = brightness_median(image, inner)
        background = brightness_median(image, SWATCH, ring)
        residual = swatch - (background * 0.75 + 64)
        blends.append((swatch, background, residual))
        assert abs(residual) <= 20, f"swatch {swatch} over {background}: not one blend"
    # (the menu's picture moves behind it, so early and late compare as blends: the same, not built up)
    assert abs(blends[0][2] - blends[1][2]) <= 16, f"the swatch's blend changed from {blends[0]} to {blends[1]}"


# the PC main menu's first and third items at 1920x1080: the focused one is drawn white, the others blue
MENU_ITEMS = {"CAMPAIGN": (790, 565, 1140, 620), "PROFILES": (800, 730, 1130, 785)}


def white_pixels(image, box):
    w, h, rgb = image
    n = 0
    for y in range(box[1], box[3]):
        for x in range(box[0], box[2]):
            o = (y * w + x) * 3
            n += rgb[o] > 200 and rgb[o + 1] > 200 and rgb[o + 2] > 200
    return n


def campaign_focused(image):
    """the main menu shows with its first item (CAMPAIGN) focused, as it starts"""
    whites = {name: white_pixels(image, box) for name, box in MENU_ITEMS.items()}
    return whites["CAMPAIGN"] > 1000 and whites["PROFILES"] * 5 < whites["CAMPAIGN"], whites


def test_input_does_not_reach_the_menus_behind(cfg):
    """buttons on the screen, then B to close it: the game's main menu behind still has its first item focused,
    as a control game shows; the same buttons with no screen do move it (the downs) or ask to quit (B). (The
    harness presses one button at a time, so a button held over the close, which ae_hooks.c holds back until let
    go of, isn't exercised here.)"""
    out = out_dir(cfg, "leak")
    presses = ["down", "down", "x", "y", "rb", "lb", "b"]
    result, text, pngs = play(cfg, out, "screen", test_input=menu_input(*presses), shots=120)
    assert result.get("status") == "PASS", result.get("why")
    assert focus_lines(text) == [1, 2]
    assert "ae menus: button X" in text and "ae menus: button Y" in text
    assert "ae menus: button TAB_NEXT" in text and "ae menus: button TAB_PREVIOUS" in text
    assert "ae menus: test screen closed" in text
    control = play(cfg, out, "control", views=0, shots=120)
    pressed = play(cfg, out, "pressed", views=0, test_input=menu_input(*presses), shots=120)
    for r in (control[0], pressed[0]):
        assert r.get("status") == "PASS", r.get("why")
    checks = {name: campaign_focused(harness.read_png(run_pngs[-1]))
              for name, run_pngs in (("screen", pngs), ("control", control[2]), ("pressed", pressed[2]))}
    (out / "menu-items.txt").write_text("".join(f"{name}: {checks[name]}\n" for name in checks))
    assert checks["control"][0], f"the control game's main menu is not as expected: {checks['control']}"
    assert not checks["pressed"][0], f"the presses with no screen left the menu as it was: {checks['pressed']}"
    assert checks["screen"][0], f"the menu behind took input meant for the screen: {checks['screen']}"


def test_menus_closed_keys_still_work(cfg):
    """debug.ae_test_screen 9: the test screen with upstream's widgets closed (ui_widgets_close_all, as AE's own
    screens that replace the menus do): the keyboard still drives AE (early check A: without the ui_widget.c pointer
    hook the game takes the keys back for play once no widget is up); the menu settings at their defaults"""
    out = out_dir(cfg, "closed")
    result, text, pngs = play(cfg, out, "closed", views=9,
                              test_input=menu_input("key:Down", "key:Down", "key:Return"))
    assert result.get("status") == "PASS", result.get("why")
    assert "ae menus: replaced the game's menus" in text
    assert focus_lines(text) == [1, 2]
    assert "ae menus: accept 2" in text
    assert "ae menus: settings: scale 1.00, reduce motion 0, volume 1.00" in text


def test_pad_y_after_keyboard(cfg):
    """a pad's Y on the first controller after keyboard use is a Y, not Tab's focus step (M1 review M2)"""
    out = out_dir(cfg, "pad-y")
    result, text, pngs = play(cfg, out, "pad-y", test_input=menu_input("key:Down", "y"))
    assert result.get("status") == "PASS", result.get("why")
    assert "ae menus: button Y" in text
    assert focus_lines(text) == [1]


def test_settings_keys(cfg):
    """display.arena_menus_scale (snapped to 90 / 100 / 115 / 130), display.arena_menus_reduce_motion and
    audio.arena_menus_volume reach the menus' settings cache (ae_settings_*)"""
    out = out_dir(cfg, "settings")
    result, text, pngs = play(cfg, out, "settings", env={"HALO_ARENA_MENUS_SCALE": "120",
                                                          "HALO_ARENA_MENUS_REDUCE_MOTION": "true",
                                                          "HALO_ARENA_MENUS_VOLUME": "0.5"}, exit_after=24)
    assert result.get("status") == "PASS", result.get("why")
    assert "ae menus: settings: scale 1.15, reduce motion 1, volume 0.50" in text


def contact_sheet(cfg, out):
    """the test screen in each SHEET_CASES case; full-size frames and sheet.png in out"""
    frames = []
    for name, window, views, env in SHEET_CASES:
        result, text, pngs = play(cfg, out, name, views=views, window=window, env=env, shots=120,
                                  test_input=menu_input(*["down"] * 13), exit_after=30)
        assert result.get("status") == "PASS", (name, result.get("why"))
        assert f"ae menus: test screen (views {views})" in text, name
        assert pngs, f"{name}: no screenshots"
        frame = out / f"{name}.png"
        frame.write_bytes(pngs[-1].read_bytes())
        frames.append(frame)
    return tile(frames, out / "sheet.png")


def test_contact_sheet(cfg):
    out = out_dir(cfg, "sheet")
    sheet = contact_sheet(cfg, out)
    assert sheet.exists()


if __name__ == "__main__":
    if sys.argv[1:2] != ["sheet"]:
        sys.exit("usage: tools/test_ae_menus.py sheet  (the contact sheet; or run it with pytest)")
    c = config()
    if not c:
        sys.exit("needs the ae_test harness config and a built game ($AE_MENUS_BUILD)")
    print(contact_sheet(c, out_dir(c, "sheet")))
