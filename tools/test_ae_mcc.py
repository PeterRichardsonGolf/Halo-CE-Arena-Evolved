"""AE's MCC Custom Edition files, read in place (port/linux/src/ae_mcc.c, ae_mcc_platform.c; port/linux/game/
ae_glue_mcc.c). The detection's own cases are ae_mcc_test.c's (tools/test_ae_units.py).

- the generator: the profile menu's MAP FILES item (after GAME OPTIONS, the rows 30 px apart) opens the MAP FILES
  screen (MCC FILES ASK/ON/OFF, BROWSE, the status line), Game Options is as it was (14 rows at 20), the question has
  its title, text and three buttons, and every other generated screen is byte-identical to its file;
- the config rows and the function names exist;
- (games, only when $AE_MCC_BUILD names a build folder and $AE_MCC_PATH an MCC folder with the real Custom Edition
  files, and $AE_MCC_DATA a data root with a Custom Edition map, homobox, in maps_ce and no resource maps) a host of
  homobox@ce reads MCC's files with game.mcc_use "yes", is refused with "ask" (logged once, the refusal saying where
  to turn them on: no question outside the menus), and the player's own files win; a Steam that lists MCC without its
  files adds its line to the refusal; the question's USE MCC FILES writes "yes" (and the folder, when Steam found
  it: HALO_MCC_PATH's is one run's), NEVER ASK AGAIN "no", which a restart keeps. The games run a copy of the build
  (its program and the files beside it) in the test's folder: the build's own config.toml is never touched.
- (games, $AE_MCC_PATH a whole MCC install: halo1/maps/ui.map, bitmaps.map and custom_edition) booting with no Xbox
  data (ae_mcc_boot.c): a data root with nothing in it boots from MCC's ui.map (the maps folder made, the main menu
  loaded); with game.mcc_use "no" it quits saying the Xbox maps are missing; the Xbox ui.map still comes first.
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
MAP_FILES = "main_menu.settings_select.player_setup.player_profile_edit.map_files.xml"
PROFILE_MENU = "main_menu.settings_select.player_setup.player_profile_edit.xml"
QUESTION = "main_menu.multiplayer_type_select.mp_map_select.port.xml"


def test_generated_screens_match_their_files():
    for name, lines in {**port_settings.settings_files(), **port_settings.multiplayer_files()}.items():
        assert "\n".join(lines) == (CE / name).read_text(), name


def test_map_files_screen():
    spec = port_settings.SCREENS["map_files"]
    (row,) = spec["rows"]
    assert row[1] == "game.mcc_use" and row[2] == [("ASK", "ask"), ("ON", "yes"), ("OFF", "no")]
    text = (CE / MAP_FILES).read_text()
    assert 'setting="game.mcc_use" values="ask|yes|no"' in text
    assert '<on event="a" run="ae mcc browse"/>' in text
    assert '<data input="ae mcc status"/>' in text
    assert f'<on event="a" open="{PE}/map_files/map_files_screen"/>' in text


def test_profile_menu_item():
    """MAP FILES is the profile menu's own item, after GAME OPTIONS; the rows end above the
    buttons; the hand-edited files (tools/ce_menus.py's) agree with the generator's data"""
    rows = port_settings.PROFILE_ROWS
    assert rows.index("map_files_profile_item") == rows.index("mods_profile_item") + 1
    tops = [port_settings._profile_row_top(item) for item in rows]
    assert tops[0] == 78 and tops[-1] + port_settings.PROFILE_ROW_HEIGHT <= 414
    menu = (CE / PROFILE_MENU).read_text()
    order = re.findall(rf'<child widget="{PE}/(\w+)"', menu.split(f'name="{PE}/profile_edit_select_list"')[1])
    assert order[:len(rows)] == rows
    files = {MAP_FILES: "map_files_profile_item", GAME_OPTIONS: "mods_profile_item",
             GAME_OPTIONS.replace("mods_setup", "hud_setup"): "hud_profile_item"}
    for item, top in zip(rows, tops):
        text = (CE / next((name for name, own in files.items() if own == item), PROFILE_MENU)).read_text()
        (widget,) = re.findall(rf'<widget name="{PE}/{item}"[^>]*>', text)
        assert f' top="{top}"' in widget and f' height="{port_settings.PROFILE_ROW_HEIGHT}"' in widget, item
    strings = (CE / "strings.xml").read_text()
    for name in ("profile_edit_options", "profile_edit_descriptions"):
        # (the upstream menus' strings carry their tag's map numbers too: <strings name=".." map="..">, <string .. map="0"/>)
        listed = re.findall(r'<string text="([^"]*)"[^>]*/>', strings.split(f'<strings name="{PE}/{name}"')[1]
                            .split("</strings>")[0])
        assert listed == [text.replace("'", "'") for text in port_settings.STRING_OVERRIDES[f"{PE}/{name}"]]
    # (the descriptions go by the items' places)
    assert "Custom Edition maps' files" in port_settings.STRING_OVERRIDES[f"{PE}/profile_edit_descriptions"][
        rows.index("map_files_profile_item")]
    tags = (ROOT / "port" / "linux" / "game" / "menu_tags.c").read_text()
    assert f'"{PE}/map_files_profile_item", /* AE hook */' in tags


def test_game_options_has_no_map_files():
    spec = port_settings.SCREENS["mods_setup"]
    # (nine rows 30 px apart since the HUD's rows moved to the HUD screen, ten 27 px apart since 0.2.5's WEAPON PACKS,
    # 24 px apart since 0.2.6's BOTS category after them; the help line under them)
    assert len(spec["rows"]) == 10 and spec["spacing"] == 24 and "help_top" not in spec
    assert 73 + (len(spec["rows"]) + len(spec["categories"])) * spec["spacing"] <= 350
    assert [folder for _, folder, *_ in spec["categories"]] == ["mods_setup/bots"]
    assert "map_files" not in (CE / GAME_OPTIONS).read_text().replace("map_files_profile_item", "")


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
    assert '{ "game.mcc_use", _config_string, "\\"yes\\"", "HALO_MCC_USE", _environment_value, /* AE hook */' in text
    tags = (ROOT / "port" / "linux" / "game" / "menu_tags.c").read_text()
    for name in ("ae mcc use", "ae mcc not now", "ae mcc never ask", "ae mcc browse", "ae mcc status"):
        assert f'"{name}"' in tags


# ---------- games

def _games():
    names = ("AE_MCC_BUILD", "AE_MCC_PATH", "AE_MCC_DATA")
    if not all(os.environ.get(name) for name in names):
        pytest.skip("$AE_MCC_BUILD, $AE_MCC_PATH and $AE_MCC_DATA name no build, MCC folder and data root")
    return Path(os.environ["AE_MCC_BUILD"]), os.environ["AE_MCC_PATH"], Path(os.environ["AE_MCC_DATA"])


def _build_copy(tmp_path):
    """the build's program and the files beside it (not its folders), copied once into the test's folder: the
    games' config.toml is the copy's"""
    build = Path(os.environ["AE_MCC_BUILD"]) if os.environ.get("AE_MCC_BUILD") else _games()[0]
    copy = tmp_path / "build"
    if not copy.exists():
        copy.mkdir()
        for path in build.iterdir():
            if path.is_file() and path.name != "config.toml":
                shutil.copy2(path, copy / path.name)
    return copy


def _link_maps(source, target):
    """copytree's copy for a data root: map files hard-linked (a data root is ~6 GB of maps, a copy for each game
    filled the build box's disk), every other file copied (the game writes debug.txt and the like)"""
    if str(source).endswith(".map"):
        try:
            os.link(source, target)
            return target
        except OSError:
            pass
    return shutil.copy2(source, target)


def _game(tmp_path, case, seconds, env, data=None, keep_config=False, linked=None):
    """one headless game; (its debug.txt, its config.toml's text). linked: a data root whose maps folder is linked
    (not copied) into the game's own data folder"""
    build = _build_copy(tmp_path)
    out = tmp_path / case
    if linked:
        (out / "data").mkdir(parents=True)
        (out / "data" / "maps").symlink_to(Path(linked) / "maps")
    else:
        _, _, default_data = _games()
        shutil.copytree(data or default_data, out / "data", symlinks=True, copy_function=_link_maps)
        if not data:
            # (a data root without Custom Edition's resource maps, as a player's with no Custom Edition install: the
            # box's has them in maps\ce, which made MCC's never needed)
            for folder in ("maps_ce", "maps/ce"):
                for name in ("bitmaps", "sounds", "loc"):
                    path = out / "data" / folder / f"{name}.map"
                    if path.exists() or path.is_symlink():
                        path.unlink()
    (out / "save").mkdir()
    config = build / "config.toml"
    if not keep_config and config.exists():
        config.unlink()
    run_env = dict(os.environ, HALO_DATA_ROOT=str(out / "data"), HALO_SAVE_ROOT=str(out / "save"),
                   HALO_NET_ONLINE="false", HALO_NET_ADDRESS="127.0.0.178", SDL_AUDIO_DRIVER="dummy", HALO_ARENA_MENUS="0",
                   HALO_HIDDEN_WINDOW="1", HALO_EXIT_AFTER=str(seconds), HOME=str(out))
    for name in ("XDG_DATA_HOME", "HALO_MCC_PATH", "HALO_MCC_USE"):
        run_env.pop(name, None)
    run_env.update(env)
    subprocess.run(["timeout", str(seconds + 60), "xvfb-run", "-a", "./halo"], cwd=build, env=run_env,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    debug = (out / "data" / "debug.txt").read_text(errors="replace")
    return debug, config.read_text() if config.exists() else ""


def _steam_tree(root, mcc=None):
    """a fake Steam root listing MCC: with its files (links to the real ones) and a manifest, or an empty shell"""
    install = root / "steamapps" / "common" / "Halo The Master Chief Collection"
    (install / "MCC" / "Binaries" / "Win64").mkdir(parents=True)
    (root / "steamapps" / "libraryfolders.vdf").write_text(
        '"libraryfolders"\n{\n\t"0"\n\t{\n\t\t"path"\t\t"%s"\n\t\t"apps"\n\t\t{\n\t\t\t"976730"\t\t"1"\n\t\t}\n\t}\n}\n'
        % str(root).replace("\\", "\\\\").replace('"', '\\"'))
    if mcc:
        files = install / "halo1" / "maps" / "custom_edition"
        files.mkdir(parents=True)
        for name in ("bitmaps", "sounds", "loc"):
            (files / f"{name}.map").symlink_to(Path(mcc) / "halo1" / "maps" / "custom_edition" / f"{name}.map")
        (root / "steamapps" / "appmanifest_976730.acf").write_text(
            '"AppState"\n{\n\t"appid"\t\t"976730"\n\t"StateFlags"\t\t"6"\n'
            '\t"installdir"\t\t"Halo The Master Chief Collection"\n}\n')
    return install


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
    debug, _ = _game(tmp_path, "ask", 15, {"HALO_MCC_PATH": mcc, "HALO_MCC_USE": "ask", "HALO_NETWORK_TEST": "host:homobox@ce"})
    assert debug.count('game.mcc_use is "ask"') == 1
    assert re.search(r"homobox@ce refused: there is no d:\\maps_ce\\bitmaps\.map\. MCC found: turn on the main menu's "
                     r"Settings > MAP FILES to use its files\.\r?$", debug, re.M)


def test_game_no_reads_nothing_of_steam(tmp_path):
    _, mcc, _ = _games()
    debug, _ = _game(tmp_path, "no", 15, {"HALO_MCC_PATH": mcc, "HALO_MCC_USE": "no",
                                           "HALO_NETWORK_TEST": "host:homobox@ce"})
    assert "mcc:" not in debug
    assert re.search(r"homobox@ce refused: there is no d:\\maps_ce\\bitmaps\.map\r?$", debug, re.M)


def test_game_incomplete_steam(tmp_path):
    """Steam lists MCC, its folder is an empty shell: the refusal says so"""
    _games()
    _steam_tree(tmp_path / "xdg" / "Steam")
    debug, _ = _game(tmp_path, "incomplete", 15, {"XDG_DATA_HOME": str(tmp_path / "xdg"),
                                                  "HALO_NETWORK_TEST": "host:homobox@ce"})
    assert "mcc: detected: MCC found, but not its Custom Edition files" in debug
    assert re.search(r"bitmaps\.map\. MCC found, but its Custom Edition files are missing\. Check the install in "
                     r"Steam\.\r?$", debug, re.M)


def test_game_steam_detection(tmp_path):
    """a Steam root (an update queued, StateFlags 6) found through XDG_DATA_HOME; with "yes" the folder is kept"""
    _, mcc, _ = _games()
    install = _steam_tree(tmp_path / "xdg" / "Steam", mcc)
    debug, config = _game(tmp_path, "steam", 20, {"XDG_DATA_HOME": str(tmp_path / "xdg"), "HALO_MCC_USE": "yes",
                                                  "HALO_NETWORK_TEST": "host:homobox@ce"})
    assert f"found in {install} (from Steam" in debug and "Custom Edition map homobox@ce: checked" in debug
    assert f'mcc_path = "{install}"' in config


def test_game_own_files_win(tmp_path):
    _, mcc, data = _games()
    own = tmp_path / "own-data"
    shutil.copytree(data, own, symlinks=True, copy_function=_link_maps)
    (own / "maps_ce").mkdir(exist_ok=True)
    for name in ("bitmaps", "sounds", "loc"):
        if (own / "maps_ce" / f"{name}.map").exists():
            continue
        (own / "maps_ce" / f"{name}.map").symlink_to(Path(mcc) / "halo1" / "maps" / "custom_edition" / f"{name}.map")
    debug, _ = _game(tmp_path, "own", 15, {"HALO_MCC_PATH": mcc, "HALO_MCC_USE": "yes",
                                            "HALO_NETWORK_TEST": "host:homobox@ce"}, data=own)
    assert "read from MCC's folder" not in debug
    assert "Custom Edition map homobox@ce: checked" in debug


def _mcc_install():
    _, mcc, _ = _games()
    if not (Path(mcc) / "halo1" / "maps" / "ui.map").is_file():
        pytest.skip("$AE_MCC_PATH has no halo1/maps/ui.map (a whole MCC install)")
    return mcc


def test_boot_from_mcc_ui(tmp_path):
    mcc = _mcc_install()
    empty = tmp_path / "empty-data"
    empty.mkdir()
    debug, _ = _game(tmp_path, "boot-mcc", 25, {"HALO_MCC_PATH": mcc, "HALO_MCC_USE": "yes"}, data=empty)
    assert "maps folder: no Xbox ui.map; the main menu is read from MCC's halo1/maps/ui.map" in debug
    assert re.search(r"mcc boot: ui from .*/halo1/maps/ui\.map \(MCC v13\)", debug)
    assert "mcc: loaded levels\\ui\\ui" in debug
    assert "refused" not in debug and "EXCEPTION" not in debug
    assert (tmp_path / "boot-mcc" / "data" / "maps").is_dir()


def test_boot_mcc_no_quits(tmp_path):
    mcc = _mcc_install()
    empty = tmp_path / "empty-data"
    empty.mkdir()
    debug, _ = _game(tmp_path, "boot-no", 15, {"HALO_MCC_PATH": mcc, "HALO_MCC_USE": "no"}, data=empty)
    assert "is not the Xbox maps" in debug and "mcc boot:" not in debug


def test_boot_xbox_first(tmp_path):
    mcc = _mcc_install()
    debug, _ = _game(tmp_path, "boot-xbox", 15, {"HALO_MCC_PATH": mcc, "HALO_MCC_USE": "yes"})
    assert re.search(r"mcc boot: ui from d:\\maps\\ui\.map \(Xbox\)", debug)
    assert "mcc: loaded levels" not in debug


MAP_SCREEN = "main_menu/multiplayer_type_select/mp_map_select/mp_map_select_screen"


def _pick_ce_map(answer):
    """the menus' input: the Map screen's first Custom Edition map (the MULTIPLAYER page's first, the chooser's last
    page: map folders, ae_map_folders.c), A, then the answer"""
    return "menu:" + " ".join(["wait"] * 12 + ["up", "wait", "left", "wait", "down"] + ["a", "wait", "wait"] + answer +
                              ["wait"] * 4)


@pytest.mark.skip(reason="drives the classic PC menus' map picker, frozen since AE's menus became the default (0.7.0: "
                         "the MCC question there is no longer reached)")
def test_game_question(tmp_path):
    _, mcc, _ = _games()
    env = {"HALO_MCC_PATH": mcc, "HALO_MENUS": "pc", "HALO_MENU_OPEN": MAP_SCREEN}
    debug, config = _game(tmp_path, "use", 30, dict(env, HALO_TEST_INPUT=_pick_ce_map(["a"])))
    assert "asking to read MCC's" in debug and "mcc: USE MCC FILES" in debug
    # (HALO_MCC_PATH's folder is one run's: not kept)
    assert 'mcc_use = "yes"' in config and '\nmcc_path = "' not in config
    assert "mcc: bitmaps.map read from MCC's folder" in debug
    # (the question logs nothing of "ask" as the engine's reads do: its probe of the player's folders is quiet)
    assert 'game.mcc_use is "ask"' not in debug.split("asking to read MCC's")[1].split("mcc: USE MCC FILES")[0]
    debug, config = _game(tmp_path, "never", 28, dict(env, HALO_TEST_INPUT=_pick_ce_map(["right", "right", "a"])))
    assert "mcc: NEVER ASK AGAIN" in debug and 'mcc_use = "no"' in config
    # (kept across a restart: no question, and no map looks in Steam)
    debug, _ = _game(tmp_path, "never-restart", 26, dict(env, HALO_TEST_INPUT=_pick_ce_map([])), keep_config=True)
    assert "asking to read MCC's" not in debug and "mcc: detected" not in debug
    assert "beavercreek_v2@ce refused" in debug


# ---------- the Workshop catalog logged at start (ae_mcc_workshop.h)

def _workshop_games():
    """$AE_MCC_BUILD and a data root ($AE_MCC_DATA, else $HALO_DATA_ROOT) with the Xbox maps; no MCC files needed"""
    data = os.environ.get("AE_MCC_DATA") or os.environ.get("HALO_DATA_ROOT")
    if not os.environ.get("AE_MCC_BUILD") or not data:
        pytest.skip("$AE_MCC_BUILD and $AE_MCC_DATA (or $HALO_DATA_ROOT) name no build and data root")
    return Path(os.environ["AE_MCC_BUILD"]), data


def _synthetic_steam(root):
    """a Steam root with a library (itself), MCC's three tiny synthetic resource files and a manifest, and one
    Workshop item: two multiplayer maps, one whole synthetic v13 map and one cut short"""
    install = root / "steamapps" / "common" / "Halo The Master Chief Collection"
    files = install / "halo1" / "maps" / "custom_edition"
    files.mkdir(parents=True)
    for type_, name in enumerate(("bitmaps", "sounds", "loc")):
        # (type + 1, one path at 16, the table at 32, one resource)
        words = [type_ + 1, 16, 32, 1]
        (files / f"{name}.map").write_bytes(b"".join(w.to_bytes(4, "little") for w in words) + b"a\\b" + bytes(25))
    (root / "steamapps" / "libraryfolders.vdf").write_text(
        '"libraryfolders"\n{\n\t"0"\n\t{\n\t\t"path"\t\t"%s"\n\t\t"apps"\n\t\t{\n\t\t\t"976730"\t\t"1"\n\t\t}\n\t}\n}\n'
        % str(root).replace("\\", "\\\\").replace('"', '\\"'))
    (root / "steamapps" / "appmanifest_976730.acf").write_text(
        '"AppState"\n{\n\t"appid"\t\t"976730"\n\t"StateFlags"\t\t"4"\n'
        '\t"installdir"\t\t"Halo The Master Chief Collection"\n}\n')
    item = root / "steamapps" / "workshop" / "content" / "976730" / "424242"
    (item / "multiplayer").mkdir(parents=True)
    (item / "maps").mkdir()
    (item / "ModInfo.json").write_text(
        '{"Engine": "Halo1", "Title": {"Neutral": "Synthetic Pack"}, "ModIdentifier": {"HostedModIds": '
        '{"SteamWorkshopId": 424242}}, "GameModContents": {"MultiplayerMaps": '
        '["multiplayer/whole.json", "multiplayer/short.json"]}}')
    for name in ("whole", "short"):
        (item / "multiplayer" / f"{name}.json").write_text(
            '{"ScenarioFile": "levels/test/%s/%s", "Title": {"Neutral": "Synthetic %s"}}' % (name, name, name))
    header = bytearray(0x800)
    header[0:4] = b"daeh"
    header[4:8] = (13).to_bytes(4, "little")
    header[8:12] = (0x1000).to_bytes(4, "little")
    header[0x7FC:0x800] = b"toof"
    (item / "maps" / "whole.map").write_bytes(bytes(header) + bytes(0x800))
    (item / "maps" / "short.map").write_bytes(bytes(header) + bytes(0x100))
    return install


def test_game_logs_the_workshop_catalog(tmp_path):
    """MCC found through a synthetic Steam tree: the catalog is logged at start, one map playable, one being updated"""
    _, data = _workshop_games()
    _synthetic_steam(tmp_path / "xdg" / "Steam")
    debug, _ = _game(tmp_path, "workshop", 8, {"XDG_DATA_HOME": str(tmp_path / "xdg"), "HALO_MCC_USE": "yes"},
                     linked=data)
    assert re.search(r"mcc workshop: 1 items, 2 maps, 1 hidden, \d+ ms", debug)
    assert re.search(r"mcc workshop hidden: 424242 short: being updated by Steam", debug)
    assert debug.count("mcc workshop hidden:") == 1


def test_game_logs_no_workshop_without_mcc(tmp_path):
    """no MCC anywhere: no catalog line"""
    _, data = _workshop_games()
    (tmp_path / "xdg").mkdir()
    debug, _ = _game(tmp_path, "workshop-none", 8, {"XDG_DATA_HOME": str(tmp_path / "xdg")}, linked=data)
    assert "mcc workshop" not in debug


def test_game_logs_no_workshop_with_mcc_use_no(tmp_path):
    """the fake MCC present but game.mcc_use no: no detection line and no catalog line"""
    _, data = _workshop_games()
    _synthetic_steam(tmp_path / "xdg" / "Steam")
    debug, _ = _game(tmp_path, "workshop-no", 8, {"XDG_DATA_HOME": str(tmp_path / "xdg"), "HALO_MCC_USE": "no"},
                     linked=data)
    assert "mcc workshop" not in debug
    assert "mcc: detected" not in debug
