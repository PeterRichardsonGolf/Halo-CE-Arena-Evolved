"""Arena Evolved's pause SETTINGS and high-res HUD on Custom Edition maps (item 79; port/linux/game/ae_glue_ce_maps.c,
port/linux/src/ae_ce_hud.c):

- the table of Halo PC HUD bitmaps the high-res textures stand for (port/linux/src/ae_ce_hud_table.c, written by
  tools/ae_ce_hud.py) names only the textures' own bitmaps, once each, and no meter's;
- with $AE_CE_BUILD (a linux64 build's folder) and $AE_CE_DATA (a data root whose maps/ce has homobox.map and Custom
  Edition's bitmaps.map, sounds.map and loc.map; the player's own files, never the repository's): a one-player game
  on homobox@ce, paused and SETTINGS opened, and a two-player split-screen one paused by player 2, whose debug.txt
  says the map's pause menu got SETTINGS (Halo PC's dead CHANGE SETTINGS row taken out), its tags were all read
  (none outside them as zeroes), the split-screen pause screens were laid out, and the high-res HUD stood for the
  map's bitmaps (none of the table's logged as other pixels)."""
import json
import os
import re
import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
TABLE = ROOT / "port" / "linux" / "src" / "ae_ce_hud_table.c"
LAYOUT = ROOT / "port" / "assets" / "hud" / "layout.json"
CE_MAP = "homobox@ce"
# a second a button: the game is under way by then (HALO_NETWORK_TEST_START=3 on a loaded box)
LEAD_IN = 24


def table_rows():
    return [(tag.replace("\\\\", "\\"), int(bitmap), int(crc, 16))
            for tag, bitmap, crc in re.findall(r'\{ "([^"]+)", (\d+), (0x[0-9a-f]+)u \}', TABLE.read_text())]


def test_table_names_only_the_textures_bitmaps_once_and_no_meter():
    assets = {(asset["tag"], asset["bitmap"]): asset for asset in json.loads(LAYOUT.read_text())["assets"]}
    rows = table_rows()
    assert rows, "the table is empty"
    assert len({(tag, bitmap) for tag, bitmap, _ in rows}) == len(rows), "a bitmap is listed twice"
    count = int(re.search(r"ae_ce_hud_bitmap_count = (\d+);", TABLE.read_text()).group(1))
    assert count == len(rows)
    for tag, bitmap, crc in rows:
        asset = assets.get((tag, bitmap))
        assert asset, f"{tag} {bitmap}: no texture stands for it"
        assert not any(cell.get("kind") == "meter" or cell.get("thresholds") for cell in asset.get("cells", [])), tag
        # (a PC bitmap's pixels, never the Xbox's: those the texture's own CRC already names)
        assert crc != asset["crc"], f"{tag} {bitmap}: the Xbox's CRC"


def test_no_table_row_is_a_sprite_texture():
    """hud_hires_sprites_drawable (the textures drawn for some of a bitmap's sprites, through a placeholder) has no
    Custom Edition hook: the table comes from layout.json only, whose textures stand for whole bitmaps (the sprite
    ones are port/assets/buttons')."""
    assets = json.loads(LAYOUT.read_text())["assets"]
    assert not any(asset.get("sprites") for asset in assets)
    assert 'json.loads(LAYOUT.read_text())["assets"]' in (ROOT / "tools" / "ae_ce_hud.py").read_text()


def test_engine_function_table_has_no_unnamed_entry():
    """ae_glue_ce_maps.c takes a pause row out only when its function index is past the engine's table, which it
    counts by name (ui_widget_event_handler_function_name: NULL past the last): the table must have a name for
    each function, as many as the dispatcher runs, so that no function the engine has reads as missing."""
    source = (ROOT / "source" / "interface" / "ui_widget_event_handler_functions.c").read_text()
    size = int(re.search(r"char const \*names\[(\d+)\];", source).group(1))
    assert re.search(r"ui_widget_event_handler_function functions\[%d\];" % size, source)
    table = source[source.index("event_handler_function_list =\n"):]
    names = table[table.index('\t{\n\t\t"NULL",'):]
    names = names[:names.index("\n\t}")]
    entries = [line.strip() for line in names.splitlines()[1:] if line.strip()]
    assert len(entries) == size
    assert all(re.fullmatch(r'"[^"]+",?', entry) for entry in entries)
    assert "function_index < %d)" % size in source
    assert re.search(r"struct event_handler_globals event_handler_functions =\n\{\n\t%d," % size, source)


def _game(tmp_path, case, seconds, test_input, env=None):
    names = ("AE_CE_BUILD", "AE_CE_DATA")
    if not all(os.environ.get(name) for name in names):
        pytest.skip("$AE_CE_BUILD and $AE_CE_DATA name no build and data root")
    build, data = Path(os.environ["AE_CE_BUILD"]), Path(os.environ["AE_CE_DATA"])
    if not all((data / "maps" / "ce" / name).exists() for name in ("homobox.map", "bitmaps.map", "sounds.map",
                                                                   "loc.map")):
        pytest.skip("$AE_CE_DATA has no maps/ce with homobox and the resource maps")
    copy = tmp_path / "build"
    if not copy.exists():
        copy.mkdir()
        for path in build.iterdir():
            if path.is_file() and path.name != "config.toml":
                shutil.copy2(path, copy / path.name)
    out = tmp_path / case
    (out / "data").mkdir(parents=True)
    (out / "data" / "maps").symlink_to(data / "maps")
    (out / "save").mkdir()
    run_env = dict(os.environ, HALO_DATA_ROOT=str(out / "data"), HALO_SAVE_ROOT=str(out / "save"),
                   HALO_NET_ONLINE="false", HALO_NET_ADDRESS="127.0.0.181", SDL_AUDIO_DRIVER="dummy", HALO_ARENA_MENUS="0",
                   HALO_HIDDEN_WINDOW="1", HALO_EXIT_AFTER=str(seconds), HOME=str(out),
                   HALO_NETWORK_TEST=f"host:{CE_MAP}", HALO_NETWORK_TEST_START="3",
                   HALO_TEST_INPUT="menu:" + " ".join(["wait"] * LEAD_IN + test_input))
    for name in ("XDG_DATA_HOME", "HALO_MCC_PATH", "HALO_MCC_USE", "HALO_MENUS", "HALO_HIGH_RES_HUD"):
        run_env.pop(name, None)
    run_env.update(env or {})
    done = subprocess.run(["timeout", str(seconds + 60), "xvfb-run", "-a", "./halo"], cwd=copy, env=run_env,
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    debug = (out / "data" / "debug.txt").read_text(errors="replace")
    assert "exiting after debug.exit_after" in debug, f"{case}: the game ended early ({done.returncode})"
    assert "EXCEPTION" not in debug, case
    # (the map's own lines, from its load on)
    start = debug.find(f"Custom Edition map {CE_MAP}: ")
    assert start >= 0, f"{case}: {CE_MAP} did not load"
    return debug[start:]


def _common(log):
    assert re.search(r"Custom Edition map: the pause list's row \S+\\change_settings_button runs a function this "
                     r"game has not: taken out", log)
    assert "menus: the pause menu has SETTINGS and END GAME\n" in log
    assert "Custom Edition map: 2 split-screen pause screens laid out as the one player's" in log
    assert "outside them (read as zeroes)" not in log
    assert "pause menu's widgets are not whole" not in log
    assert "event handler function 137" not in log
    assert not re.search(r"event handler function 1(?:5\d|6\d)", log)
    assert "invalid event_handler_function" not in log
    # (only the pause family's rows: handlers also lead to the settings screens and the main menu, whose rows stay)
    assert not re.search(r"the pause list's row ui\\shell\\main_menu\\\S+ (?:runs|taken)", log)
    loaded = re.search(r"high-res hud: (\d+) of \d+ bitmaps in this map", log)
    assert loaded and int(loaded.group(1)) > 0
    # (each bitmap the table names, drawn, is stood for: none logged as other pixels)
    for tag, bitmap, _ in table_rows():
        assert f"high-res hud: {tag} bitmap {bitmap} is not the one" not in log, (tag, bitmap)


def test_one_player_pause_settings_and_hud(tmp_path):
    log = _game(tmp_path, "one", 45, ["start", "wait", "down", "wait", "down", "wait", "a", "wait", "wait"])
    _common(log)
    # (SETTINGS opened the profile's settings: the throwaway save root has no profile, so one is made)
    assert "menus: made a player profile for Settings" in log


def test_xbox_menus_pause_settings_and_end_game(tmp_path):
    log = _game(tmp_path, "xbox", 40, ["start", "wait", "wait"], {"HALO_MENUS": "xbox"})
    _common(log)


def test_split_screen_pause(tmp_path):
    log = _game(tmp_path, "split", 40, ["2:start", "wait", "2:down", "wait", "2:a", "wait", "wait"],
                {"HALO_NETWORK_TEST_LOCAL_PLAYERS": "2"})
    _common(log)
    assert re.search(r"Custom Edition map: \d+ screens opened from a split-screen pause fitted to its player's view",
                     log)
    assert re.search(r"Custom Edition map: split-pause screen .*game.options.* opened for local player 2", log)


def test_ffa_restart_and_unsafe_rows(tmp_path):
    log = _game(tmp_path, "restart", 65,
                ["start", "wait", "down", "wait", "a", "wait", "a", "wait", "wait", "wait"])
    _common(log)
    assert "\\choose_team_button taken out: there are no teams in this game" in log
    assert "\\new_game_button taken out: its PC lobby widget is not safe in this engine" in log
    assert "Custom Edition map: current game restarted" in log
    assert log.count("game rules:") >= 2
    # (the same map again, not the menu background's)
    assert len(re.findall(r"Custom Edition map homobox@ce: \d+ tags", log)) >= 2
    assert "failed to spawn widget" not in log
    assert "the game host went down" not in log


def test_split_screen_choose_team(tmp_path):
    log = _game(tmp_path, "team", 45,
                ["2:start", "wait", "2:down", "wait", "2:a", "wait", "2:a", "wait", "2:a", "wait"],
                {"HALO_NETWORK_TEST": f"host:{CE_MAP}:ctf", "HALO_NETWORK_TEST_LOCAL_PLAYERS": "2"})
    _common(log)
    assert "\\new_game_button taken out: its PC lobby widget is not safe in this engine" in log
    assert "Custom Edition map: controller 2 changed team from CHOOSE TEAM" in log
