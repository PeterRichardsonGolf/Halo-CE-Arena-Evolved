"""AE's widget gallery (debug.ae_test_screen 11-15, 21, 23, 41) in real games, headless, through the ae_test harness
(tools/ae_test), in tools/test_ae_menus.py's pattern (its helpers: play, menu_input, out_dir, tile).

- every page at 1080p 100 % and 130 %, at 4:3 and 21:9, and in 2 and 4 synthetic split views (also 720p quarters):
  each case's last frame (<case>.png) and contact sheets (sheet-full-100.png, sheet-full-130.png, sheet-aspect.png,
  sheet-2p.png, sheet-4p.png);
- the VIEW sizing rule measured in the running game (the gallery's "view" log lines);
- input: focus steps, a value picker, sounds one per press; typing on a keyboard and AE's keyboard on a pad;
- REDUCE MOTION keeps the caret steady.

Runs only when asked (as test_ae_menus.py: $AE_MENUS_BUILD and the harness's config); each game is about 30 s:

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_gallery.py
    python3 tools/test_ae_gallery.py sheets      (the sheets alone)
"""
import importlib.util
import re
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_gallery_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

EXIT_AFTER = 30
# name, debug.ae_test_screen, window, UI SCALE, the presses (after the lead-in), more environment
CASES = (
    ("full-100-p1", 11, "1920x1080", 100, (), {}),
    ("full-100-p2", 12, "1920x1080", 100, (), {}),
    ("full-100-p3", 13, "1920x1080", 100, ("2:a",), {}),
    ("full-100-p4", 14, "1920x1080", 100, (), {}),
    ("full-100-p5", 15, "1920x1080", 100, (), {}),
    ("full-130-p1", 11, "1920x1080", 130, (), {}),
    ("full-130-p2", 12, "1920x1080", 130, (), {}),
    ("full-130-p3", 13, "1920x1080", 130, ("2:a",), {}),
    ("full-130-p4", 14, "1920x1080", 130, (), {}),
    ("aspect-4x3", 15, "1440x1080", 100, (), {}),
    # (preflight P30: the tab strip's overflow is on page 1)
    ("aspect-4x3-p1", 11, "1440x1080", 100, (), {}),
    ("aspect-21x9", 15, "2560x1080", 100, (), {}),
    ("2p-100-a", 21, "1920x1080", 100, (), {}),
    ("2p-100-b", 23, "1920x1080", 100, (), {}),
    ("2p-130-a", 21, "1920x1080", 130, (), {}),
    ("2p-130-b", 23, "1920x1080", 130, (), {}),
    ("4p-100", 41, "1920x1080", 100, (), {}),
    ("4p-130", 41, "1920x1080", 130, (), {}),
    ("4p-720-100", 41, "1280x720", 100, (), {}),
    ("4p-720-130", 41, "1280x720", 130, (), {}),
)
SHEETS = {
    "sheet-full-100.png": ("full-100-p1", "full-100-p2", "full-100-p3", "full-100-p4", "full-100-p5"),
    "sheet-full-130.png": ("full-130-p1", "full-130-p2", "full-130-p3", "full-130-p4"),
    "sheet-aspect.png": ("aspect-4x3", "aspect-4x3-p1", "aspect-21x9"),
    "sheet-2p.png": ("2p-100-a", "2p-100-b", "2p-130-a", "2p-130-b"),
    "sheet-4p.png": ("4p-100", "4p-130", "4p-720-100", "4p-720-130"),
}
VIEW_LINE = re.compile(r"ae gallery: view (\d+) page (\d+) density (FULL|VIEW) s=([\d.]+) panel=(\d+)x(\d+) "
                       r"text=([\d.]+)px minor=([\d.]+)px row=([\d.]+)px")


def play(cfg, out, name, value, presses=(), window="1920x1080", scale=100, env=None, shots=60):
    e = {"HALO_ARENA_MENUS_SCALE": str(scale)}
    e.update(env or {})
    return menus.play(cfg, out, name, views=value, window=window, env=e, shots=shots, exit_after=EXIT_AFTER,
                      test_input=menus.menu_input(*presses) if presses else menus.menu_input("wait"))


def view_lines(text):
    """the gallery's view lines: {view: (page, density, s, panel_w, panel_h, text, minor, row)} (the last of each)"""
    views = {}
    for m in VIEW_LINE.finditer(text):
        views[int(m[1])] = (int(m[2]), m[3], float(m[4]), int(m[5]), int(m[6]), float(m[7]), float(m[8]),
                            float(m[9]))
    return views


def sounds(text):
    return [line.rsplit(" ", 1)[1] for line in text.splitlines() if "ae sound: " in line]


def run_cases(cfg, out):
    """every case: {name: (result, debug text, last frame)}; each frame copied to out/<name>.png"""
    runs = {}
    for name, value, window, scale, presses, env in CASES:
        result, text, pngs = play(cfg, out, name, value, presses, window, scale, env)
        frame = None
        if pngs:
            frame = out / f"{name}.png"
            frame.write_bytes(pngs[-1].read_bytes())
        runs[name] = (result, text, frame)
    return runs


def write_sheets(out, runs):
    written = []
    for sheet, names in SHEETS.items():
        frames = [runs[n][2] for n in names if runs.get(n) and runs[n][2]]
        if frames:
            written.append(menus.tile(frames, out / sheet, width=960, columns=2 if len(frames) == 4 else 3))
    return written


@pytest.fixture(scope="module")
def cfg():
    c = menus.config()
    if not c:
        pytest.skip("needs the ae_test harness config (data_dir with maps) and a built game ($AE_MENUS_BUILD)")
    return c


@pytest.fixture(scope="module")
def gallery(cfg):
    out = menus.out_dir(cfg, "gallery")
    return out, run_cases(cfg, out)


def test_gallery_sheets(gallery):
    out, runs = gallery
    for name, value, *_ in CASES:
        result, text, frame = runs[name]
        assert result.get("status") == "PASS", (name, result.get("why"))
        assert f"ae gallery: open (value {value})" in text, name
        assert frame, f"{name}: no screenshot"
    for sheet in write_sheets(out, runs):
        assert Path(sheet).exists()


def test_view_density_numbers(gallery):
    out, runs = gallery
    two = view_lines(runs["2p-100-a"][1])
    assert sorted(two) == [1, 2], two
    assert two[1][1] == "VIEW" and two[1][2] == 1.00 and (two[1][3], two[1][4]) == (470, 464), two
    four = view_lines(runs["4p-100"][1])
    assert sorted(four) == [1, 2, 3, 4], four
    assert all((line[3], line[4]) == (442, 464) for line in four.values()), four
    small = view_lines(runs["4p-720-100"][1])
    assert sorted(small) == [1, 2, 3, 4], small
    for view, line in small.items():
        assert line[5] >= 16.0 and line[6] >= 14.0 and line[7] >= 26.6, (view, line)
    big = view_lines(runs["4p-130"][1])
    assert big[1][2] == 1.30, big
    full = view_lines(runs["full-100-p1"][1])
    assert full[1][1] == "FULL" and full[1][2] == 1.00, full


def focus_lines(text):
    return [int(m[1]) for m in re.finditer(r"ae gallery: focus (\d+)/\d+", text)]


def test_gallery_input(cfg):
    """page 2 (preflight P15): three downs step the focus by one to NETWORK, a closed picker row; A opens its list
    (forward), down and A pick (cursor, forward); B closes the gallery (back). One sound a press."""
    out = menus.out_dir(cfg, "gallery-input")
    result, text, pngs = play(cfg, out, "input", 12, ("down", "down", "down", "a", "down", "a", "b"))
    assert result.get("status") == "PASS", result.get("why")
    assert focus_lines(text) == [2, 3, 4], focus_lines(text)
    assert sounds(text) == ["cursor", "cursor", "cursor", "forward", "cursor", "forward", "back"], sounds(text)
    assert "ae gallery: picked ONLINE" in text
    assert "ae gallery: closed" in text


def test_gallery_typing(cfg):
    """page 3 (preflight P17): Enter starts typing into the first field (uppercase, its text selected), A E
    Backspace B types exactly 'AB', Enter keeps it; on the second field a pad's A (the second controller's: the
    harness's pads count as the keyboard's on the first) opens AE's keyboard, right and A type a key, B closes it"""
    out = menus.out_dir(cfg, "gallery-typing")
    presses = ("key:Return", "key:A", "key:E", "key:Backspace", "key:B", "key:Return", "down", "2:a", "2:right", "2:a",
               "2:start", "2:a", "2:b")
    result, text, pngs = play(cfg, out, "typing", 13, presses)
    assert result.get("status") == "PASS", result.get("why")
    lines = [line for line in text.splitlines() if "ae gallery" in line]
    assert "ae gallery: field 'AB'" in text, lines
    # (AE's keyboard: right to W, A types it (the mixed-case field: w) at the caret, the end; START keeps it; opened
    # again, B closes it unchanged)
    assert text.count("ae gallery: keyboard open") == 2 and text.count("ae gallery: keyboard closed") == 2, lines
    assert "ae gallery: field 'Team AE Pro Slayerw'" in text, lines
    after = text.split("ae gallery: field 'Team AE Pro Slayerw'", 1)[1]
    assert "ae gallery: field 'Team AE Pro Slayerw'" in after, lines
    # (the keyboard opening (forward), right (cursor), a key typed (forward), START (forward); opening (forward), B
    # (back): one a press)
    after_open = text.split("ae gallery: keyboard open", 1)[1]
    assert sounds(after_open)[:6] == ["forward", "cursor", "forward", "forward", "forward", "back"], sounds(after_open)


def accent_pixels(image, box):
    w, h, rgb = image
    n = 0
    for y in range(max(0, box[1]), min(h, box[3])):
        for x in range(max(0, box[0]), min(w, box[2])):
            o = (y * w + x) * 3
            n += rgb[o] < 140 and rgb[o + 1] > 160 and rgb[o + 2] > 190
    return n


def caret_counts(cfg, out, name, env, shots=7, last=10):
    """the accent pixels inside the first field's well (its accent ring left out) in the last screenshots"""
    result, text, pngs = play(cfg, out, name, 13, env=env, shots=shots)
    assert result.get("status") == "PASS", (name, result.get("why"))
    m = re.search(r"ae gallery: field 1 at (\d+),(\d+) (\d+)x(\d+)", text)
    assert m, "no field position logged"
    x, y, w, h = (int(v) for v in m.groups())
    box = (x + 6, y + 6, x + w - 6, y + h - 6)
    assert len(pngs) >= last, (name, len(pngs))
    return [accent_pixels(menus.harness.read_png(p), box) for p in pngs[-last:]]


def test_reduce_motion(cfg):
    """REDUCE MOTION: the caret never blinks: screenshots every 7 frames (about 115 ms, against the blink's 530 ms
    on and off) all show it; the control game without REDUCE MOTION catches it off in at least one"""
    out = menus.out_dir(cfg, "gallery-reduce")
    steady = caret_counts(cfg, out, "reduce", {"HALO_ARENA_MENUS_REDUCE_MOTION": "1"})
    assert all(c >= 20 for c in steady), steady
    blinking = caret_counts(cfg, out, "control", {})
    assert any(c < 20 for c in blinking) and any(c >= 20 for c in blinking), blinking


if __name__ == "__main__":
    if sys.argv[1:2] != ["sheets"]:
        sys.exit("usage: tools/test_ae_gallery.py sheets  (the gallery's frames and sheets; or run it with pytest)")
    c = menus.config()
    if not c:
        sys.exit("needs the ae_test harness config and a built game ($AE_MENUS_BUILD)")
    o = menus.out_dir(c, "gallery")
    for s in write_sheets(o, run_cases(c, o)):
        print(s)
