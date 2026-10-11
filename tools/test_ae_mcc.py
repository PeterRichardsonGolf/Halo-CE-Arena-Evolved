"""AE's MCC Custom Edition files, read in place (port/linux/src/ae_mcc.c, ae_mcc_platform.c; port/linux/game/
ae_glue_mcc.c). The detection's own cases are ae_mcc_test.c's (tools/test_ae_units.py).

- the generator: Game Options' MAP FILES row opens the MAP FILES screen (MCC FILES ON/OFF, BROWSE, the status line),
  the question has its title, text and three buttons, and every other generated screen is byte-identical to its file;
- the config rows and the function names exist;
- (games, only when $AE_MCC_BUILD names a build folder and $AE_MCC_PATH an MCC folder with the real Custom Edition
  files, and $AE_MCC_DATA a data root with a Custom Edition map, homobox, in maps_ce and no resource maps) a host of
  homobox@ce reads MCC's files with game.mcc_use "yes", is refused with "ask" (logged once: no question outside the
  menus), and the player's own files win; the question's USE MCC FILES writes "yes" and the folder, NEVER ASK AGAIN
  "no", which a restart keeps. The build's config.toml (beside it) is removed before each game.
"""
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import port_settings  # noqa: E402

CE = ROOT / "port" / "assets" / "menus" / "ce"
PE = "main_menu/settings_select/player_setup/player_profile_edit"
GAME_OPTIONS = "main_menu.settings_select.player_setup.player_profile_edit.mods_setup.xml"
MAP_FILES = "main_menu.settings_select.player_setup.player_profile_edit.mods_setup.map_files.xml"
QUESTION = "main_menu.multiplayer_type_select.mp_map_select.port.xml"


def test_generated_screens_match_their_files():
    for name, lines in {**port_settings.settings_files(), **port_settings.multiplayer_files()}.items():
        assert "\n".join(lines) == (CE / name).read_text(), name


def test_map_files_screen():
    spec = port_settings.SCREENS["mods_setup/map_files"]
    (row,) = spec["rows"]
    assert row[1] == "game.mcc_use" and [v for _, v in row[2]] == ["yes", "no"]
    assert [s for s, _ in row[2]] == ["ON", "OFF"]
    text = (CE / MAP_FILES).read_text()
    assert 'setting="game.mcc_use" values="yes|no"' in text
    assert '<on event="a" run="ae mcc browse"/>' in text
    assert '<data input="ae mcc status"/>' in text
    game_options = (CE / GAME_OPTIONS).read_text()
    assert f'open="{PE}/mods_setup/map_files/map_files_screen"' in game_options
    # (the fourteen rows and MAP FILES fit above the help line)
    assert len(port_settings.SCREENS["mods_setup"]["rows"]) == 14
    assert 73 + 14 * port_settings.SCREENS["mods_setup"]["spacing"] < port_settings.SCREENS["mods_setup"]["help_top"]


def test_question():
    text = (CE / QUESTION).read_text()
    assert 'text="HALO MASTER CHIEF COLLECTION FOUND"' in text
    assert "It only reads them and never changes" in text
    for caption, run in (("USE MCC FILES", "ae mcc use"), ("NOT NOW", "ae mcc not now"),
                         ("NEVER ASK AGAIN", "ae mcc never ask")):
        assert f'text="{caption}"' in text and f'run="{run}"' in text
    # (USE MCC FILES opens the gametypes as the Map screen's OK does; the question keeps no history)
    assert 'run="ae mcc use" open="main_menu/multiplayer_type_select/connected/gametype_select_screen_wrapper"' in text
    assert "no_history" in text
    listed = (ROOT / "port" / "assets" / "menus" / "menus.json").read_text()
    assert f"ce/{QUESTION}" in listed and f"ce/{MAP_FILES}" in listed


def test_config_rows_and_functions():
    text = (ROOT / "port" / "linux" / "src" / "port_config.c").read_text()
    assert '{ "game.mcc_path", _config_string, "\\"\\"", NULL, _environment_value, /* AE hook */' in text
    assert '{ "game.mcc_use", _config_string, "\\"ask\\"", "HALO_MCC_USE", _environment_value, /* AE hook */' in text
    tags = (ROOT / "port" / "linux" / "game" / "menu_tags.c").read_text()
    for name in ("ae mcc use", "ae mcc not now", "ae mcc never ask", "ae mcc browse", "ae mcc status"):
        assert f'"{name}"' in tags


# ---------- games

def _games():
    names = ("AE_MCC_BUILD", "AE_MCC_PATH", "AE_MCC_DATA")
    if not all(os.environ.get(name) for name in names):
        pytest.skip("$AE_MCC_BUILD, $AE_MCC_PATH and $AE_MCC_DATA name no build, MCC folder and data root")
    return Path(os.environ["AE_MCC_BUILD"]), os.environ["AE_MCC_PATH"], Path(os.environ["AE_MCC_DATA"])


def _game(tmp_path, case, seconds, env, data=None, keep_config=False):
    """one headless game; (its debug.txt, its config.toml's text)"""
    build, _, default_data = _games()
    out = tmp_path / case
    shutil.copytree(data or default_data, out / "data", symlinks=True)
    (out / "save").mkdir()
    config = build / "config.toml"
    if not keep_config and config.exists():
        config.unlink()
    run_env = dict(os.environ, HALO_DATA_ROOT=str(out / "data"), HALO_SAVE_ROOT=str(out / "save"),
                   HALO_NET_ONLINE="false", HALO_NET_ADDRESS="127.0.0.178", SDL_AUDIO_DRIVER="dummy",
                   HALO_HIDDEN_WINDOW="1", HALO_EXIT_AFTER=str(seconds), HOME=str(out), **env)
    run_env.pop("XDG_DATA_HOME", None)
    run_env.pop("HALO_MCC_PATH", None)
    run_env.update(env)
    subprocess.run(["timeout", str(seconds + 60), "xvfb-run", "-a", "./halo"], cwd=build, env=run_env,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    debug = (out / "data" / "debug.txt").read_text(errors="replace")
    return debug, config.read_text() if config.exists() else ""


def test_game_reads_mcc_files(tmp_path):
    _, mcc, _ = _games()
    debug, _ = _game(tmp_path, "yes", 20, {"HALO_MCC_PATH": mcc, "HALO_MCC_USE": "yes",
                                            "HALO_NETWORK_TEST": "host:homobox@ce"})
    for name in ("bitmaps", "sounds", "loc"):
        assert f"mcc: {name}.map read from MCC's folder" in debug
    assert "Custom Edition map homobox@ce: checked" in debug
    assert "refused" not in debug


def test_game_ask_is_no_outside_the_menus(tmp_path):
    _, mcc, _ = _games()
    debug, _ = _game(tmp_path, "ask", 15, {"HALO_MCC_PATH": mcc, "HALO_NETWORK_TEST": "host:homobox@ce"})
    assert debug.count('game.mcc_use is "ask"') == 1
    assert re.search(r"homobox@ce refused: there is no d:\\maps_ce\\bitmaps\.map\r?$", debug, re.M)


def test_game_own_files_win(tmp_path):
    _, mcc, data = _games()
    own = tmp_path / "own-data"
    shutil.copytree(data, own, symlinks=True)
    for name in ("bitmaps", "sounds", "loc"):
        (own / "maps_ce" / f"{name}.map").symlink_to(Path(mcc) / "halo1" / "maps" / "custom_edition" / f"{name}.map")
    debug, _ = _game(tmp_path, "own", 15, {"HALO_MCC_PATH": mcc, "HALO_MCC_USE": "yes",
                                            "HALO_NETWORK_TEST": "host:homobox@ce"}, data=own)
    assert "read from MCC's folder" not in debug
    assert "Custom Edition map homobox@ce: checked" in debug


MAP_SCREEN = "main_menu/multiplayer_type_select/mp_map_select/mp_map_select_screen"


def _pick_ce_map(answer):
    """the menus' input: the Map screen's first Custom Edition map (after the stock maps), A, then the answer"""
    return "menu:" + " ".join(["wait"] * 4 + ["down"] * 13 + ["a", "wait", "wait"] + answer + ["wait"] * 4)


def test_game_question(tmp_path):
    _, mcc, _ = _games()
    env = {"HALO_MCC_PATH": mcc, "HALO_MENUS": "pc", "HALO_MENU_OPEN": MAP_SCREEN}
    debug, config = _game(tmp_path, "use", 30, dict(env, HALO_TEST_INPUT=_pick_ce_map(["a"])))
    assert "asking to read MCC's" in debug and "mcc: USE MCC FILES" in debug
    assert 'mcc_use = "yes"' in config and f'mcc_path = "{mcc}"' in config
    assert "mcc: bitmaps.map read from MCC's folder" in debug
    debug, config = _game(tmp_path, "never", 28, dict(env, HALO_TEST_INPUT=_pick_ce_map(["right", "right", "a"])))
    assert "mcc: NEVER ASK AGAIN" in debug and 'mcc_use = "no"' in config
    # (kept across a restart: no question)
    debug, _ = _game(tmp_path, "never-restart", 26, dict(env, HALO_TEST_INPUT=_pick_ce_map([])), keep_config=True)
    assert "asking to read MCC's" not in debug and "game.mcc_use no" in debug
