"""MENUS on the main menu (item 81; port/linux/game/ae_menus_swap.c, port/assets/menus/ce/main_menu.menus_select.xml):

- the PC main menu's file: MENUS directly above QUIT, the column 36 apart from 229 (18 up from the stock 247), the
  build number beside QUIT (y 418), and tools/ce_menus.py's data saying the same (the file is that tool's, which
  cannot run here: CLAUDE.local.md's menu file rules);
- the styles' screen's file is tools/port_settings.py's output, byte for byte;
- every row picture (tools/menu_row_art.py) is embedded (menus.json), named in bitmaps.xml (the PC rows) or by
  ae_menus_swap.c (the Xbox rows), 1024 by 256;
- the menus' functions are named where the files and the C look for them;
- with $AE_MENUS_SWAP_BUILD (a linux64 build's folder) and $AE_MENUS_SWAP_DATA (a data root with maps/ui.map and
  the maps): real games, headless, at 1280x720 and 1920x1080. PC: MENUS above QUIT opens the styles in place with
  PC focused; B goes back with MENUS focused; XBOX switches live (display.menus written), the Xbox main menu then
  has MENUS in its fourth row, focused; it opens the styles with XBOX focused; PC switches back, MENUS focused.
  Started in the Xbox's menus: MENUS is there from the start. In a LAN lobby there is no MENUS (nothing of it
  runs), and after leaving it MENUS opens again. The pictures are kept (contact sheets: --sheet).

    AE_MENUS_SWAP_BUILD=$PWD/build/linux64 AE_MENUS_SWAP_DATA=~/halo-data python3 -m pytest -q tools/test_ae_menus_swap.py
"""
import ast
import importlib.util
import json
import os
import re
import shutil
import struct
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
MENUS = ROOT / "port" / "assets" / "menus"
CE = MENUS / "ce"
SWAP_C = ROOT / "port" / "linux" / "game" / "ae_menus_swap.c"
# the main menu's rows (menu units): x, the first row's y, the spacing
ROW_X, FIRST_ROW, SPACING = 192, 211, 35
XBOX_FIRST_ROW = 247
LEAD_IN = 12


def _module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _art_data():
    """menu_row_art.py's STYLES folders and ROWS (read, not imported: it needs Pillow, NumPy and SciPy)"""
    tree = ast.parse((ROOT / "tools" / "menu_row_art.py").read_text())
    values = {}
    for node in tree.body:
        if isinstance(node, ast.Assign) and isinstance(node.targets[0], ast.Name) and node.targets[0].id == "ROWS":
            values["rows"] = ast.literal_eval(node.value)
    source = (ROOT / "tools" / "menu_row_art.py").read_text()
    values["folders"] = dict(re.findall(r'"(pc|xbox)": \([^,]+, "([^"]+)"\)', source))
    return values


def _children(xml, widget):
    body = re.search(r'<widget name="%s"[^>]*>(.*?)</widget>' % re.escape(widget), xml, re.S).group(1)
    return [(name, int(x or 0), int(y or 0)) for name, x, y in
            re.findall(r'<child widget="([^"]+)"(?: x="(\d+)")?(?: y="(\d+)")?/>', body)]


def test_main_menu_has_menus_above_quit_and_the_column_moved_up():
    xml = (CE / "main_menu.xml").read_text()
    rows = _children(xml, "main_menu/main_menu_select_list")
    names = [name.rsplit("/", 1)[1] for name, _, _ in rows]
    assert names == ["main_menu_item_load_camp", "main_menu_item_multiplayer", "main_menu_item_profile",
                     "main_menu_item_settings", "main_menu_item_menus", "main_menu_item_mods",
                     "main_menu_item_quit_game"]
    # (OpenCE build-220's MODS between MENUS and QUIT: seven rows, 35 apart)
    assert [y for _, _, y in rows] == [FIRST_ROW + SPACING * index for index in range(7)]
    assert [x for _, x, _ in rows] == [194] + [ROW_X] * 6
    # (QUIT ends at 454, inside the 5% action-safe area: 456)
    assert rows[-1][2] + 33 <= 456
    assert ("main_menu/build_number", 450, 430) in _children(xml, "main_menu/main_menu")


def test_generator_data_agrees_with_the_hand_edited_main_menu():
    import sys
    sys.path.insert(0, str(ROOT / "tools"))
    port_settings = _module(ROOT / "tools" / "port_settings.py", "port_settings")
    patch = port_settings.WIDGET_PATCHES["main_menu/main_menu_select_list"]["insert_before"]
    assert patch["main_menu/main_menu_item_quit_game"] == ['<child widget="main_menu/main_menu_item_menus" x="192" '
                                                           'y="351"/>',
                                                           '<child widget="main_menu/main_menu_item_mods" x="192" '
                                                           'y="386"/>']
    source = (ROOT / "tools" / "ce_menus.py").read_text()
    for item, place in (("load_camp", "(194, 211)"), ("multiplayer", "(192, 246)"), ("profile", "(192, 281)"),
                        ("settings", "(192, 316)"), ("quit_game", "(192, 421)")):
        assert f'("{item}", {place})' in source, item
    assert '"ui\\\\shell\\\\main_menu\\\\build_number"): (450, 430)' in source
    # the styles' screen: the generator's output as it is in the tree
    files = port_settings.main_menu_files()
    assert list(files) == ["main_menu.menus_select.xml"]
    assert "\n".join(files["main_menu.menus_select.xml"]) == (CE / "main_menu.menus_select.xml").read_text()


def test_styles_screen_is_the_main_menu_with_the_styles_in_place():
    xml = (CE / "main_menu.menus_select.xml").read_text()
    screen = _children(xml, "main_menu/menus_select/menus_screen")
    assert ("main_menu/halo_logo", 0, 28) in screen and ("main_menu/build_number", 450, 430) in screen
    rows = _children(xml, "main_menu/menus_select/menus_select_list")
    assert [name.rsplit("/", 1)[1] for name, _, _ in rows] == ["menus_item_pc", "menus_item_xbox", "menus_item_arena",
                                                              "menus_item_back"]
    # (BACK straight under the last style, at the rows' pitch: no gap; the C takes ARENA EVOLVED out of a build
    # without AE menus and moves BACK up a row, as the Xbox's list has it)
    assert [y for _, _, y in rows] == [211, 246, 281, 316]
    assert "BACK_GAP" not in SWAP_C.read_text()
    assert '<on event="b" back="true"/>' in xml and '<on event="back" back="true"/>' in xml
    assert xml.count('run="ae menus pick"') == 6
    # (MENUS's A opens nothing itself: "ae menus open" opens the styles through the custom activation, or refuses
    # quietly; a handler that failed would log the engine's "event handler function ... failed")
    item = re.search(r'<widget name="main_menu/main_menu_item_menus".*?</widget>', xml, re.S).group(0)
    assert '<on event="a" run="ae menus open"/>' in item and '<on event="start" run="ae menus open"/>' in item
    assert '<on event="custom_activation" open="main_menu/menus_select/menus_screen"/>' in item
    # (MENUS only on the main menu: no other file names it)
    for path in CE.glob("*.xml"):
        if path.name not in ("main_menu.xml", "main_menu.menus_select.xml"):
            assert "main_menu_item_menus" not in path.read_text(), path.name


def test_row_pictures_are_embedded_and_named():
    art = _art_data()
    files = json.loads((MENUS / "menus.json").read_text())["files"]
    bitmaps = (CE / "bitmaps.xml").read_text()
    source = SWAP_C.read_text()
    assert "ce/main_menu.menus_select.xml" in files
    for style, folder in art["folders"].items():
        for row, _, _ in art["rows"]:
            for frame in (0, 1):
                png = f"{folder}/{row}__{frame}.png"
                assert png in files, png
                data = (MENUS / png).read_bytes()
                assert data[:8] == b"\x89PNG\r\n\x1a\n"
                assert struct.unpack(">II", data[16:24]) == (1024, 256), png
                if style == "pc":
                    assert f'<frame png="{png}" width="256" height="64"/>' in bitmaps
    assert f'#define XBOX_ART "{art["folders"]["xbox"]}/"' in source
    for name in re.search(r"art_names\[NUMBER_OF_ART\] =\s*\{([^}]*)\}", source).group(1).split(","):
        assert name.strip().strip('"') in [row for row, _, _ in art["rows"]], name


def test_functions_are_named():
    tags = (ROOT / "port" / "linux" / "game" / "menu_tags.c").read_text()
    source = SWAP_C.read_text()
    for name in ("ae menus open", "ae menus pick", "ae menus focus", "ae menus item"):
        assert f'"{name}"' in tags, name
        assert f'"{name}"' in source, name


def test_refusals_never_fail_their_handler():
    """a refused MENUS or switch returns TRUE (the failure sound and our own log line), so the engine logs no error"""
    source = SWAP_C.read_text()
    for function in ("menus_open", "menus_pick"):
        body = re.search(r"static boolean %s\(.*?\n}\n" % function, source, re.S).group(0)
        refused = body[body.index("if (why)"):]
        refused = refused[:refused.index("}")]
        assert "return TRUE;" in refused and "return FALSE;" not in refused, function


def _process_ui_widgets():
    source = (ROOT / "source" / "interface" / "ui_widget.c").read_text()
    start = source.index("\nvoid process_ui_widgets(")
    return source, source[start:source.index("\n}\n", start)]


def test_switch_runs_after_the_pointer_lets_its_targets_go():
    """the pointer's targets point at the widgets drawn last frame; ui_widgets_process_mouse lets them all go at its
    end, whatever happened. The switch's rebuild deletes every widget, so it must run after that, in the same frame:
    else a pointer moved or clicked on the rebuild's frame would reach deleted widgets (review C1)"""
    source, body = _process_ui_widgets()
    mouse = body.index("\tui_widgets_process_mouse();")
    swap = body.index("\tae_menus_swap_update(); /* AE hook */")
    assert mouse < swap
    # (nothing between them reads the targets: only the port's queued press)
    between = body[mouse:swap]
    assert "ui_mouse_" not in between.replace("ui_widgets_process_mouse", "")
    start = source.index("\nstatic void ui_widgets_process_mouse(\n")
    mouse_body = source[start:source.index("\n}\n", start)]
    # (the targets forgotten unconditionally, at the function's end, at its outermost level)
    # (0.3.5's menu crash fix forgets the deleted widgets there too: ae_ui_deleted_clear, an AE hook)
    assert mouse_body.rstrip().endswith("\tui_mouse_target_count = 0;\n\tui_mouse_targets_settled = FALSE;\n"
                                        "\tae_ui_deleted_clear(); /* AE hook */\n\n\treturn;")
    assert mouse_body.count("return;") == 1


# ---------- the game


def _read_png(path):
    harness = _module(ROOT / "tools" / "ae_test" / "harness.py", "ae_menus_swap_harness")
    return harness.read_png(path)


def _lit(png, y, x0=300, x1=344):
    """the share of near-white pixels in the middle of the main menu's row at y (menu units): a focused row's text
    is white, an unfocused row's blue"""
    width, height, rgb = _read_png(png)
    scale = height / 480
    left = (width - 640 * scale) / 2
    xs = range(int(left + x0 * scale), int(left + x1 * scale))
    ys = range(int((y + 9) * scale), int((y + 24) * scale))
    white = sum(1 for py in ys for px in xs if min(rgb[(py * width + px) * 3:(py * width + px) * 3 + 3]) > 215)
    return white / (len(xs) * len(ys))


def _game(tmp_path, case, size, seconds, presses, menus, env=None, menus_files=None, config_text=None, may_quit=False):
    names = ("AE_MENUS_SWAP_BUILD", "AE_MENUS_SWAP_DATA")
    if not all(os.environ.get(name) for name in names):
        pytest.skip("$AE_MENUS_SWAP_BUILD and $AE_MENUS_SWAP_DATA name no build and data root")
    build, data = Path(os.environ["AE_MENUS_SWAP_BUILD"]), Path(os.environ["AE_MENUS_SWAP_DATA"]).expanduser()
    if not (data / "maps" / "ui.map").exists():
        pytest.skip("$AE_MENUS_SWAP_DATA has no maps/ui.map")
    out = tmp_path / case
    copy = out / "run"
    copy.mkdir(parents=True)
    shutil.copy2(build / "halo", copy / "halo")
    (out / "data").mkdir()
    (out / "data" / "maps").symlink_to(data / "maps")
    (out / "save").mkdir()
    # (a menus folder beside config.toml: the player's own files)
    for name, text in (menus_files or {}).items():
        (copy / "menus").mkdir(exist_ok=True)
        (copy / "menus" / name).write_text(text)
    run_env = {"PATH": os.environ.get("PATH", "/usr/bin:/bin"), "HOME": str(out), "HALO_DATA_ROOT": str(out / "data"),
               "HALO_SAVE_ROOT": str(out / "save"), "HALO_NET_ONLINE": "false", "HALO_NET_ADDRESS": "127.0.0.1",
               "SDL_AUDIO_DRIVER": "dummy", "HALO_ARENA_MENUS": "0", "HALO_HIDDEN_WINDOW": "1", "HALO_UPDATE_AUTO": "false",
               "HALO_EXIT_AFTER": str(seconds), "HALO_DISPLAY_MODE": "windowed", "HALO_WINDOW_SIZE": size,
               "HALO_SCREENSHOT_FORMAT": "png", "HALO_MENUS": menus,
               "HALO_TEST_INPUT": "menu:" + " ".join(["wait"] * LEAD_IN + presses)}
    run_env.update(env or {})
    # (a None drops the harness's own setting: the game's config.toml decides it)
    run_env = {name: value for name, value in run_env.items() if value is not None}
    if config_text:
        (copy / "config.toml").write_text(config_text)
    command = ["timeout", str(seconds + 90), "xvfb-run", "-a", "-s", f"-screen 0 {size}x24", "./halo"]
    if shutil.which("unshare"):
        command = ["timeout", str(seconds + 90), "unshare", "-rn", "sh", "-c",
                   f'ip link set lo up 2>/dev/null; exec xvfb-run -a -s "-screen 0 {size}x24" ./halo']
    done = subprocess.run(command, cwd=copy, env=run_env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    debug = (out / "data" / "debug.txt").read_text(errors="replace")
    assert "exiting after debug.exit_after" in debug or (may_quit and "ae main menu: quit" in debug), \
        f"{case}: the game ended early ({done.returncode})"
    assert "EXCEPTION" not in debug and "### ASSERT" not in debug, case
    shots = sorted((copy / "screenshots").glob("*.png"))
    keep = os.environ.get("AE_MENUS_SWAP_SHOTS")
    if keep:
        target = Path(keep).expanduser() / case
        target.mkdir(parents=True, exist_ok=True)
        for index, shot in enumerate(shots):
            shutil.copy2(shot, target / f"{index:02d}.png")
    config = (copy / "config.toml").read_text() if (copy / "config.toml").exists() else ""
    return debug, shots, config


def _in_order(text, lines):
    rest = text
    for line in lines:
        assert line in rest, f"missing, or out of order: {line!r}"
        rest = rest.split(line, 1)[1]


SHOT = "key:F10"
# PC: the main menu; MENUS focused; A: the styles, PC focused; XBOX focused; B: back, MENUS focused; A, down, A:
# XBOX; the Xbox's main menu, MENUS focused; A: the styles, XBOX focused; up, A: PC; the PC main menu, MENUS focused
SWITCH = [SHOT, "down", "down", "down", "down", SHOT, "a", "wait", SHOT, "down", SHOT, "b", "wait", SHOT,
          "a", "wait", "down", "a", "wait", "wait", SHOT, "a", "wait", SHOT, "up", "a", "wait", "wait", SHOT]


@pytest.mark.parametrize("size", ["1280x720", "1920x1080"])
def test_switch_pc_xbox_pc(tmp_path, size):
    debug, shots, config = _game(tmp_path, f"switch-{size}", size, 50, SWITCH, "pc")
    _in_order(debug, ["menus: MENUS opened", "menus: the styles shown, pc focused",
                      "menus: MENUS opened", "menus: switching to xbox (display.menus), rebuilding",
                      "menus: the Xbox's main menu has MENUS", "menus: MENUS focused (row 4)",
                      "menus: the Xbox's menus are up", "menus: MENUS opened", "menus: the styles shown, xbox focused",
                      "menus: switching to pc (display.menus), rebuilding", "menus: MENUS focused (row 5)",
                      "menus: the PC version's menus are up"])
    assert "could not be written" not in debug and "MENUS refused" not in debug
    assert re.search(r'^menus = "pc"$', config, re.M), "display.menus not written"
    assert len(shots) == 8, [shot.name for shot in shots]
    menus_row, xbox_slot = FIRST_ROW + 4 * SPACING, XBOX_FIRST_ROW + 3 * SPACING
    checks = [
        (0, FIRST_ROW, True), (0, menus_row, False),             # CAMPAIGN focused
        (1, menus_row, True), (1, FIRST_ROW, False),             # MENUS focused
        (2, FIRST_ROW, True), (2, FIRST_ROW + SPACING, False),   # the styles: PC focused, in CAMPAIGN's place
        (3, FIRST_ROW + SPACING, True), (3, FIRST_ROW, False),   # XBOX focused
        (4, menus_row, True),                                    # B: back, MENUS focused
        (5, xbox_slot, True), (5, XBOX_FIRST_ROW, False),        # the Xbox's main menu: MENUS (4th row) focused
        (6, XBOX_FIRST_ROW + SPACING, True),                     # its styles: XBOX focused
        (7, menus_row, True), (7, FIRST_ROW, False),             # the PC main menu again: MENUS focused
    ]
    for shot, y, lit in checks:
        share = _lit(shots[shot], y)
        assert (share > 0.08) if lit else (share < 0.03), (shot, y, lit, round(share, 3))


def test_switch_pc_to_ae_menus(tmp_path):
    # (0.7.0: the styles' ARENA EVOLVED row was never shown: its check named a header that is ae_screen_main_menu.h)
    # PC (config.toml: AE's menus off): MENUS, A: the styles; down, down: ARENA EVOLVED; A: written, and the game
    # starts again (AE's menus are made at the start). The game started again replays the same presses, now in AE's
    # main menu (down x4, A: AE's QUIT, so it ends there), and its log (debug.txt starts over) has AE's menus up
    debug, _, config = _game(tmp_path, "switch-arena", "1280x720", 40,
                             ["down", "down", "down", "down", "a", "wait", "down", "down", "a", "wait", "wait", "wait"],
                             "pc", env={"HALO_ARENA_MENUS": None}, config_text="[display]\narena_menus = false\n",
                             may_quit=True)
    assert "arena menus: on (display.arena_menus)" in debug and "ae main menu: quit" in debug
    assert re.search(r'^arena_menus = true$', config, re.M), "display.arena_menus not written"


def test_started_in_the_xbox_menus(tmp_path):
    # up from CAMPAIGN: the last row, MENUS; A: the styles (XBOX focused); B: back, MENUS focused
    debug, shots, _ = _game(tmp_path, "xbox-start", "1280x720", 34, [SHOT, "up", SHOT, "a", "wait", SHOT, "b", "wait",
                                                                    SHOT], "xbox")
    assert "menus: the Xbox's main menu has MENUS (12 tags added" in debug
    assert "menus: could not draw" not in debug
    _in_order(debug, ["menus: MENUS opened", "menus: the styles shown, xbox focused"])
    assert len(shots) == 4
    assert _lit(shots[0], XBOX_FIRST_ROW) > 0.08 and _lit(shots[0], XBOX_FIRST_ROW + 3 * SPACING) < 0.03
    assert _lit(shots[1], XBOX_FIRST_ROW + 3 * SPACING) > 0.08
    assert _lit(shots[2], XBOX_FIRST_ROW + SPACING) > 0.08 and _lit(shots[2], XBOX_FIRST_ROW) < 0.03
    assert _lit(shots[3], XBOX_FIRST_ROW + 3 * SPACING) > 0.08


# the main menu's MULTIPLAYER (a fresh save root: a new profile's name screen, its Done), Create Game > LAN, the map,
# the gametype, Server Setup's START GAME: the lobby (tools/test_ae_lan_lobby.py's way there)
TO_LOBBY = ("down a a wait wait down down down down a wait wait a wait wait a wait wait up wait a wait wait wait "
            "wait").split()


def test_no_menus_in_a_lobby(tmp_path):
    # the lobby (its picture kept); B five times: player 1 leaves, then Server Setup, Select Gametype, Select Map and
    # Multiplayer are left (the host let go), back at the main menu (MULTIPLAYER focused); down to MENUS, A
    back_out = []
    for _ in range(5):
        back_out += ["b", "wait", "wait"]
    presses = TO_LOBBY + [SHOT] + back_out + [SHOT, "down", "down", "down", "a", "wait", SHOT]
    debug, shots, _ = _game(tmp_path, "lobby", "1280x720", 75, presses, "pc")
    lobby = debug.split("server added player from machine #0 at controller index #0 to the game", 1)
    assert len(lobby) == 2, "the lobby did not open"
    in_lobby = lobby[1].split("network server disposed", 1)
    assert len(in_lobby) == 2, "the host was not let go"
    # (nothing of MENUS runs in the lobby; after it, at the main menu, MENUS opens the styles)
    assert "menus: MENUS" not in in_lobby[0] and "menus: the styles" not in in_lobby[0]
    _in_order(in_lobby[1], ["menus: MENUS opened", "menus: the styles shown, pc focused"])
    assert len(shots) == 3
    assert _lit(shots[2], FIRST_ROW) > 0.08


def test_pc_menus_that_cannot_be_built_put_the_xbox_menus_back(tmp_path):
    """a menus folder whose file is broken: the PC version's menus cannot be built (the game's own Xbox menus, which
    still have MENUS); picking PC writes it, finds it cannot be built, and puts XBOX back, once, MENUS focused"""
    presses = ["up", "a", "wait", "up", "a", "wait", "wait", "wait", SHOT]
    debug, shots, config = _game(tmp_path, "pc-broken", "1280x720", 34, presses, "xbox",
                                 menus_files={"broken.xml": "<menus><widget name=\"broken\">\n"})
    _in_order(debug, ["menus: the Xbox's main menu has MENUS", "menus: MENUS opened",
                      "menus: the styles shown, xbox focused", "menus: switching to pc (display.menus), rebuilding",
                      "menus: the pc menus could not be built: back to xbox", "menus: MENUS focused (row 4)",
                      "menus: the Xbox's menus are up"])
    assert debug.count("could not be built") == 1
    assert re.search(r'^menus = "xbox"$', config, re.M), "display.menus not put back"
    assert "event handler function" not in debug
    assert _lit(shots[-1], XBOX_FIRST_ROW + 3 * SPACING) > 0.08


def _xtest(display):
    """the X server's test extension (libXtst, as tools' xkeys did): the pointer moved and clicked"""
    import ctypes
    x11, xtst = ctypes.CDLL("libX11.so.6"), ctypes.CDLL("libXtst.so.6")
    x11.XOpenDisplay.restype = ctypes.c_void_p
    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XFlush.argtypes = [ctypes.c_void_p]
    xtst.XTestFakeMotionEvent.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_ulong]
    xtst.XTestFakeButtonEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
    handle = x11.XOpenDisplay(display.encode())
    assert handle, f"no display {display}"

    def move(x, y):
        xtst.XTestFakeMotionEvent(handle, -1, int(x), int(y), 0)
        x11.XFlush(handle)

    def click():
        for down in (1, 0):
            xtst.XTestFakeButtonEvent(handle, 1, down, 0)
            x11.XFlush(handle)
    return move, click


def test_pointer_moving_and_clicking_through_the_switch(tmp_path):
    """the pointer starts the switch (a click on MENUS, a click on XBOX) and keeps moving and clicking, every 10 ms,
    through the fade, the rebuild's frame and the fade in: the switch completes, MENUS focused, nothing breaks. A
    software renderer's frame is long, so many pointer events land in the rebuild's frame (review C1)"""
    import random
    import time
    names = ("AE_MENUS_SWAP_BUILD", "AE_MENUS_SWAP_DATA")
    if not all(os.environ.get(name) for name in names) or not shutil.which("Xvfb"):
        pytest.skip("$AE_MENUS_SWAP_BUILD and $AE_MENUS_SWAP_DATA (and Xvfb) needed")
    build, data = Path(os.environ["AE_MENUS_SWAP_BUILD"]), Path(os.environ["AE_MENUS_SWAP_DATA"]).expanduser()
    out = tmp_path / "pointer"
    copy = out / "run"
    copy.mkdir(parents=True)
    shutil.copy2(build / "halo", copy / "halo")
    (out / "data").mkdir()
    (out / "data" / "maps").symlink_to(data / "maps")
    (out / "save").mkdir()
    number = next(n for n in range(80 + random.randrange(10), 140) if not Path(f"/tmp/.X11-unix/X{n}").exists())
    display = f":{number}"
    xvfb = subprocess.Popen(["Xvfb", display, "-screen", "0", "1280x720x24", "-nolisten", "tcp"],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    game = None
    try:
        time.sleep(1.5)
        env = {"PATH": os.environ.get("PATH", "/usr/bin:/bin"), "HOME": str(out), "DISPLAY": display,
               "HALO_DATA_ROOT": str(out / "data"), "HALO_SAVE_ROOT": str(out / "save"), "HALO_NET_ONLINE": "false",
               "HALO_NET_ADDRESS": "127.0.0.1", "SDL_AUDIO_DRIVER": "dummy", "HALO_ARENA_MENUS": "0", "HALO_UPDATE_AUTO": "false",
               "SDL_GAMECONTROLLER_IGNORE_DEVICES": "all", "HALO_EXIT_AFTER": "40", "HALO_DISPLAY_MODE": "windowed",
               "HALO_WINDOW_SIZE": "1280x720", "HALO_MENUS": "pc"}
        game = subprocess.Popen(["timeout", "130", "./halo"], cwd=copy, env=env, stdout=subprocess.DEVNULL,
                                stderr=subprocess.DEVNULL)
        debug_path = out / "data" / "debug.txt"

        def log():
            return debug_path.read_text(errors="replace") if debug_path.exists() else ""

        def wait_for(line, seconds):
            end = time.time() + seconds
            while time.time() < end:
                if line in log():
                    return True
                time.sleep(0.1)
            return False

        assert wait_for("starting main menu music", 60), "the main menu did not come up"
        time.sleep(2)
        move, click = _xtest(display)
        scale, left = 720 / 480, (1280 - 640 * 720 / 480) / 2

        def row(y):
            return left + 320 * scale, (y + 16) * scale

        move(*row(FIRST_ROW + 4 * SPACING))
        time.sleep(0.5)
        click()
        assert wait_for("menus: MENUS opened", 10), "a click on MENUS did not open the styles"
        time.sleep(1)
        move(*row(FIRST_ROW + SPACING))
        time.sleep(0.5)
        click()
        assert wait_for("menus: switching to xbox", 10), "a click on XBOX did not switch"
        # (moving over the rows and clicking empty space, through the fade, the rebuild and the fade in)
        end = time.time() + 3
        step = 0
        while time.time() < end:
            x, y = row(FIRST_ROW + (step % 6) * SPACING)
            move(x + (step % 7) * 9, y)
            if step % 15 == 0:
                move(40, 40)
                click()
            step += 1
            time.sleep(0.01)
        assert wait_for("menus: the Xbox's menus are up", 10)
        assert game.wait(timeout=120) == 0
    finally:
        if game and game.poll() is None:
            game.kill()
        xvfb.terminate()
        xvfb.wait()
    debug = log()
    assert "exiting after debug.exit_after" in debug
    assert "EXCEPTION" not in debug and "### ASSERT" not in debug
    _in_order(debug, ["menus: switching to xbox (display.menus), rebuilding", "menus: MENUS focused (row 4)",
                      "menus: the Xbox's menus are up"])
