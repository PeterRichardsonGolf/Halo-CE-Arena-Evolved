"""Settings' HUD screen, the Settings list with it, ZOOM RESET on Game Options, and the item waypoints' pictures
(display.waypoint_style):

- the generator (tools/port_settings.py): the HUD screen's rows (moved from Game Options, with WAYPOINTS), Game
  Options' rows (ZOOM RESET under ZOOM), the profile menu's twelve rows above the buttons with HUD after VIDEO
  SETUP, and the hand-edited files (tools/ce_menus.py's) agreeing with its data;
- the Settings list's picture goes by the entry's name (ae_settings.c, unit tested in ae_settings_test.c);
- Arena Evolved's glyphs are what tools/ae_waypoint_glyphs.py draws;
- with $AE_MCC_BUILD (a linux64 build's folder) and $AE_MCC_DATA (a data root with the stock maps), as
  tools/test_ae_mcc.py's: TRAINING games on Prisoner in each style, whose debug.waypoint_log lines say which items
  drew a picture and which fell back to text; and with $AE_MCC_PATH a Custom Edition map (homobox@ce).
The pictures' rules are ae_waypoint_icons_test.c's (tools/test_ae_units.py)."""
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import ae_waypoint_glyphs  # noqa: E402
import port_settings  # noqa: E402

CE = ROOT / "port" / "assets" / "menus" / "ce"
PE = "main_menu/settings_select/player_setup/player_profile_edit"
GAME_OPTIONS = "main_menu.settings_select.player_setup.player_profile_edit.mods_setup.xml"
HUD = "main_menu.settings_select.player_setup.player_profile_edit.hud_setup.xml"
PROFILE_MENU = "main_menu.settings_select.player_setup.player_profile_edit.xml"
HUD_SETTINGS = ["display.hud_style", "display.hud_area", "display.compact_hud", "display.scoreboard_fade",
                "display.campaign_timer", "display.match_clock", "display.short_messages", "display.waypoint_style",
                "display.waypoint_size"]
GAME_SETTINGS = ["game.disc", "game.weapon_packs", "game.fall_damage", "game.health", "game.callouts",
                 "game.callout_detail", "game.callout_voice", "display.spawn_heat", "input.zoom_mode", "input.zoom_reset"]


def _settings(folder):
    return [row[1] for row in port_settings.SCREENS[folder]["rows"]]


def test_generated_screens_match_their_files():
    for name, lines in {**port_settings.settings_files(), **port_settings.multiplayer_files()}.items():
        assert "\n".join(lines) == (CE / name).read_text(), name


def test_hud_screen():
    assert _settings("hud_setup") == HUD_SETTINGS
    spec = port_settings.SCREENS["hud_setup"]
    assert spec["title"] == "HUD" and spec["screen"] == "hud_settings_screen"
    row = spec["rows"][-2]
    assert row[0] == "WAYPOINTS:" and row[2] == [("TEXT", "text"), ("ICONS", "icons"), ("BOTH", "both")]
    assert len(row[3]) == 3 and all(len(text.split("\n")) <= 2 for text in row[3])
    text = (CE / HUD).read_text()
    assert 'setting="display.waypoint_style" values="text|icons|both"' in text
    # (WAYPOINT SIZE: 50 to 100 percent in steps of 5, after WAYPOINTS)
    size = spec["rows"][-1]
    assert size[0] == "WAYPOINT SIZE:" and size[1] == "display.waypoint_size"
    assert [value for _, value in size[2]] == [str(n) for n in range(50, 101, 5)] and size[2][4] == ("70%", "70")
    assert 'setting="display.waypoint_size" values="50|55|60|65|70|75|80|85|90|95|100"' in text
    for setting in HUD_SETTINGS:
        assert f'setting="{setting}"' in text, setting
    # (HUD STYLE first: AE (the default) or CLASSIC, a help for each)
    style = spec["rows"][0]
    assert style[0] == "HUD STYLE:" and style[1] == "display.hud_style"
    assert style[2] == [("AE", "ae"), ("CLASSIC", "classic")] and len(style[3]) == 2
    assert all(len(help_text.split("\n")) <= 2 for help_text in style[3])
    assert 'setting="display.hud_style" values="ae|classic"' in text
    # (its rows and its help line stay above the buttons)
    assert 73 + len(HUD_SETTINGS) * spec["spacing"] <= spec.get("help_top", 350)
    assert f'"ce/{HUD}"' in (ROOT / "port" / "assets" / "menus" / "menus.json").read_text()


def test_game_options():
    assert _settings("mods_setup") == GAME_SETTINGS
    spec = port_settings.SCREENS["mods_setup"]
    assert 73 + len(GAME_SETTINGS) * spec["spacing"] <= spec.get("help_top", 350)
    text = (CE / GAME_OPTIONS).read_text()
    # (the HUD's rows are the HUD screen's only)
    for setting in HUD_SETTINGS:
        assert f'setting="{setting}"' not in text, setting
    for setting in GAME_SETTINGS:
        assert f'setting="{setting}"' not in (CE / HUD).read_text(), setting
    rows = {row[1]: row for row in spec["rows"]}
    reset = rows["input.zoom_reset"]
    assert reset[0] == "ZOOM RESET:" and reset[2] == [("FIRST LEVEL", "first"), ("LAST USED", "last")]
    assert len(reset[3]) == 2 and all(len(help_text.split("\n")) <= 2 for help_text in reset[3])
    assert 'setting="input.zoom_reset" values="first|last"' in text
    # (ZOOM's help: the wheel and the flashlight step the level of a hold; HOLD's tap then hold)
    toggle, hold, both = rows["input.zoom_mode"][3]
    assert "wheel" in hold and "flashlight" in hold and "tap then a hold" in hold
    assert "wheel" in both and "flashlight" in both
    assert all(len(help_text.split("\n")) <= 2 for help_text in (toggle, hold, both))


def test_no_setting_is_on_two_screens():
    seen = {}
    for folder, spec in port_settings.SCREENS.items():
        if folder == "video_settings/graphics":  # (Video Setup's category: its own rows again)
            continue
        for row in spec["rows"]:
            key = (row[1], row[4])
            assert key not in seen, f"{row[1]} is on {seen[key]} and {folder}"
            seen[key] = folder


def test_profile_menu():
    """twelve rows from 78, evenly, ending above the buttons (414); HUD after VIDEO SETUP; the hand-edited files
    agree with the generator's data; the list has the picture's input after the game's own"""
    rows = port_settings.PROFILE_ROWS
    assert len(rows) == 12 and len(set(rows)) == 12
    assert rows.index("hud_profile_item") == rows.index("video_settings_profile_item") + 1
    tops = [port_settings._profile_row_top(item) for item in rows]
    assert tops[0] == 78 and tops[-1] + port_settings.PROFILE_ROW_HEIGHT <= 414
    assert port_settings.PROFILE_ROW_HEIGHT <= port_settings.PROFILE_ROW_STEP
    menu = (CE / PROFILE_MENU).read_text()
    listed = menu.split(f'name="{PE}/profile_edit_select_list"')[1].split("</widget>")[0]
    assert re.findall(rf'<child widget="{PE}/(\w+)"', listed)[:len(rows)] == rows
    assert re.findall(r'<data input="([^"]*)"/>', listed) == ["plyr prof edit select menu upd8", "ae profile picture"]
    assert port_settings.WIDGET_PATCHES[f"{PE}/profile_edit_select_list"]["inputs"] == ["ae profile picture"]
    own = {"hud_profile_item": HUD, "mods_profile_item": GAME_OPTIONS,
           "map_files_profile_item": "main_menu.settings_select.player_setup.player_profile_edit.map_files.xml"}
    assert set(own) == set(port_settings.PROFILE_OWN_ROWS)
    for item, top in zip(rows, tops):
        (widget,) = re.findall(rf'<widget name="{PE}/{item}"[^>]*>', (CE / own.get(item, PROFILE_MENU)).read_text())
        assert f' top="{top}"' in widget and f' height="{port_settings.PROFILE_ROW_HEIGHT}"' in widget, item
        assert f' text_y="{port_settings.PROFILE_ROW_TEXT_Y}"' in widget, item
    (hud,) = re.findall(rf'<widget name="{PE}/hud_profile_item"[^>]*>.*?</widget>', (CE / HUD).read_text(), re.S)
    assert f'<on event="a" open="{PE}/hud_setup/hud_settings_screen"/>' in hud
    strings = (CE / "strings.xml").read_text()
    for name in ("profile_edit_options", "profile_edit_descriptions"):
        listed = re.findall(r'<string text="([^"]*)"/>', strings.split(f'<strings name="{PE}/{name}">')[1]
                            .split("</strings>")[0])
        assert listed == port_settings.STRING_OVERRIDES[f"{PE}/{name}"]
    options = port_settings.STRING_OVERRIDES[f"{PE}/profile_edit_options"]
    assert options[int(re.search(r'string_index="(\d+)"', hud).group(1))] == "HUD"
    # (a row's description is the string at its place in the list)
    descriptions = port_settings.STRING_OVERRIDES[f"{PE}/profile_edit_descriptions"]
    assert len(descriptions) == len(rows)
    assert "HUD" in descriptions[rows.index("hud_profile_item")]
    assert "Mods" in descriptions[rows.index("mods_profile_item")]
    assert "color" in descriptions[rows.index("color_profile_item")]
    assert "Rename" in descriptions[rows.index("name_profile_item")]


def test_settings_stays_in_the_pause_menu():
    """HUD is one of the entries a game's pause SETTINGS keeps (its settings apply in a game)"""
    tags = (ROOT / "port" / "linux" / "game" / "menu_tags.c").read_text()
    hidden = tags.split("pause_settings_hidden_items[] =")[1].split("};")[0]
    assert "hud_profile_item" not in hidden and "map_files_profile_item" in hidden
    assert '"ae profile picture", /* AE hook */' in tags
    # (the list holds every row: pause_settings_patch's places)
    places = int(re.search(r"short places\[(\d+)\];", tags).group(1))
    assert places >= len(port_settings.PROFILE_ROWS)


def test_config_rows():
    text = (ROOT / "port" / "linux" / "src" / "port_config.c").read_text()
    assert ('{ "display.waypoint_style", _config_string, "\\"both\\"", "HALO_WAYPOINT_STYLE", _environment_value, '
            "/* AE hook */") in text
    assert ('{ "display.hud_style", _config_string, "\\"ae\\"", "HALO_HUD_STYLE", _environment_value, '
            "/* AE hook */") in text
    assert ('{ "debug.waypoint_icons", _config_string, "\\"\\"", "HALO_WAYPOINT_ICONS", _environment_value, '
            "/* AE hook */") in text


def test_mcc_hint_names_the_main_menu():
    text = (ROOT / "port" / "linux" / "src" / "ae_mcc_platform.c").read_text()
    assert "turn on the main menu's Settings > MAP FILES to use its files." in text


def test_glyphs_are_what_the_tool_draws():
    assert ae_waypoint_glyphs.main(["--check"]) == 0
    for name in ("overshield", "camo"):
        assert (ROOT / "port" / "assets" / "waypoints" / f"{name}.png").read_bytes()[:8] == b"\x89PNG\r\n\x1a\n"
    assert (ROOT / "port" / "assets" / "waypoints" / "LICENSE.md").is_file()


# ---------- games

# (TIMERS and TRAINING: the waypoints over every power entry, through walls; no countdown)
TRAINING_FLAGS = "20906017"


def _games(mcc=False):
    names = ("AE_MCC_BUILD", "AE_MCC_DATA") + (("AE_MCC_PATH",) if mcc else ())
    if not all(os.environ.get(name) for name in names):
        pytest.skip("$" + ", $".join(names) + " name no build, data root" + (" and MCC folder" if mcc else ""))
    return Path(os.environ["AE_MCC_BUILD"]), Path(os.environ["AE_MCC_DATA"])


def _game(tmp_path, case, game_map, seconds, env):
    """one headless TRAINING game with the waypoint log; its debug.txt's waypoint lines"""
    build, data = _games()
    copy = tmp_path / "build"
    if not copy.exists():
        copy.mkdir()
        for path in build.iterdir():
            if path.is_file() and path.name != "config.toml":
                shutil.copy2(path, copy / path.name)
    out = tmp_path / case
    shutil.copytree(data, out / "data", symlinks=True)
    (out / "save").mkdir()
    config = copy / "config.toml"
    if config.exists():
        config.unlink()
    run_env = dict(os.environ, HALO_DATA_ROOT=str(out / "data"), HALO_SAVE_ROOT=str(out / "save"),
                   HALO_NET_ONLINE="false", HALO_NET_ADDRESS="127.0.0.179", SDL_AUDIO_DRIVER="dummy", HALO_ARENA_MENUS="0",
                   HALO_HIDDEN_WINDOW="1", HALO_EXIT_AFTER=str(seconds), HOME=str(out),
                   HALO_NETWORK_TEST=f"host:{game_map}:slayer", HALO_NETWORK_TEST_FLAGS=TRAINING_FLAGS,
                   HALO_NETWORK_TEST_START="3", HALO_WAYPOINT_LOG="1")
    for name in ("XDG_DATA_HOME", "HALO_MCC_PATH", "HALO_MCC_USE", "HALO_WAYPOINT_STYLE"):
        run_env.pop(name, None)
    run_env.update(env)
    done = subprocess.run(["timeout", str(seconds + 60), "xvfb-run", "-a", "./halo"], cwd=copy, env=run_env,
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    assert done.returncode == 0, f"{case}: the game ended with {done.returncode}"
    debug = (out / "data" / "debug.txt").read_text(errors="replace")
    assert "EXCEPTION" not in debug and "assert" not in debug.lower().replace("asserts", ""), case
    return [line.split("waypoint: ", 1)[1].rstrip() for line in debug.splitlines() if "waypoint: view" in line]


def _lines(log):
    return [line for line in log if re.match(r'view \d+ line "', line)]


def _pictures(log):
    return [line for line in log if re.match(r"view \d+ entry \d+ picture ", line)]


# (Prisoner's power entries: a sniper rifle at each base (every 30 s: its waypoint from 20 s), a camo and an
# overshield (every 60 s: from 50 s) and the rockets (every 120 s: in none of these games). The seconds a game
# runs for the powerups' waypoints to be seen)
POWERUPS = 72


def test_game_text_style_draws_no_picture(tmp_path):
    log = _game(tmp_path, "text", "prisoner", POWERUPS, {"HALO_WAYPOINT_STYLE": "text"})
    lines = _lines(log)
    assert lines and not _pictures(log)
    assert all("style text: 0 pictures" in line for line in lines)
    assert any(re.match(r'view 0 line "(RED|BLUE) SNIPER 0:\d\d" ', line) for line in lines)
    assert any(line.startswith('view 0 line "OVERSHIELD') for line in lines)


def test_game_default_style_is_both(tmp_path):
    log = _game(tmp_path, "both", "prisoner", POWERUPS, {})
    lines, pictures = _lines(log), _pictures(log)
    assert lines and all("style both" in line for line in lines)
    # (the snipers' picture is the map's sprite, the powerups' are ours; none fell back)
    assert any(re.match(r"view 0 entry \d+ picture the map's sequence \d+, drawn \d+ by \d+$", line)
               for line in pictures)
    assert any("picture Arena Evolved's glyph 0," in line for line in pictures)
    assert any("picture Arena Evolved's glyph 1," in line for line in pictures)
    assert not any("glyph 2," in line for line in pictures)
    assert not [line for line in pictures if "picture none" in line and "not loaded" not in line]
    # (a label is the time, the name, then the picture: the logged rows are bar-separated)
    assert any(re.match(r'view 0 line "\d+\.\dm\|(RED|BLUE) SNIPER" style both: 1 picture,', line) for line in lines)
    assert any(re.match(r'view 0 line "\d+\.\dm\|(RED|BLUE) SNIPER\|0:\d\d" style both: 1 picture,', line) for line in lines)
    assert any(re.match(r'view 0 line "\d+\.\dm\|OVERSHIELD(\|0:\d\d)?" style both: 1 picture,', line) for line in lines)
    # (a line's height is its text's and, with pictures, their row over it: the logged "W by H")
    heights = {}
    for line in lines:
        match = re.search(r'line "[^"]*" style both: (\d+) picture.*?, \d+ by (\d+) at', line)
        if match:
            heights.setdefault(int(match.group(1)) > 0, set()).add(int(match.group(2)))
    assert heights.get(True) and heights.get(False, {0}) and min(heights[True]) > max(heights.get(False, {0}))


def test_game_icons_style(tmp_path):
    log = _game(tmp_path, "icons", "prisoner", 50, {"HALO_WAYPOINT_STYLE": "icons"})
    lines = _lines(log)
    assert lines and all("style icons" in line for line in lines)
    # (the distance over the picture, the timer under it while respawning: no name)
    assert any(re.match(r'view 0 line "\d+\.\dm" style icons: 1 picture,', line) for line in lines)
    assert any(re.match(r'view 0 line "\d+\.\dm\|0:\d\d" style icons: 1 picture,', line) for line in lines)
    assert not any("SNIPER" in line or "OVERSHIELD" in line or "CAMO" in line
                   for line in lines if "0 pictures" not in line and " . " not in line and " + " not in line)


@pytest.mark.parametrize("knob", ["no_sheet", "bad_sequence"])
def test_game_weapon_without_a_picture_shows_our_burst(tmp_path, knob):
    """a map without the sheet, and one whose weapon names a picture its sheet has not: the weapons show our burst
    in an outlined badge (never an empty row), and the powerups still their glyphs"""
    log = _game(tmp_path, knob, "prisoner", POWERUPS, {"HALO_WAYPOINT_STYLE": "icons", "HALO_WAYPOINT_ICONS": knob})
    lines, pictures = _lines(log), _pictures(log)
    assert not any("picture the map's sequence" in line for line in pictures)
    assert any("picture Arena Evolved's glyph 2," in line for line in pictures)
    assert any("picture Arena Evolved's glyph 0," in line or "picture Arena Evolved's glyph 1," in line for line in pictures)
    assert lines and all("1 picture," in line for line in lines if "SNIPER" not in line)
    # (every line has its picture: none is a bare text line)
    assert not any(re.match(r'view \d+ line "[^"]*" style icons: 0 pictures', line) for line in lines)


def test_game_split_screen(tmp_path):
    log = _game(tmp_path, "split", "prisoner", 50, {"HALO_NETWORK_TEST_LOCAL_PLAYERS": "2"})
    for view in (0, 1):
        assert any(re.match(rf"view {view} entry \d+ picture ", line) for line in log), view
        assert any(re.match(rf'view {view} line "[^"]+" style both: 1 picture,', line) for line in log), view


def test_game_custom_edition_map(tmp_path):
    """a Custom Edition map's HUD (the PC version's sheet): played to the end, each weapon's picture checked
    before it is drawn, or our burst"""
    _games(mcc=True)
    log = _game(tmp_path, "ce", "homobox@ce", 50, {"HALO_WAYPOINT_STYLE": "icons", "HALO_MCC_USE": "yes",
                                                     "HALO_MCC_PATH": os.environ["AE_MCC_PATH"]})
    lines, pictures = _lines(log), _pictures(log)
    assert lines and pictures
    for line in pictures:
        assert re.search(r"picture (the map's sequence \d+, drawn \d+ by \d+|Arena Evolved's glyph [012], drawn \d+ by "
                         r"\d+)$", line), line
    assert not any(re.match(r'view \d+ line " *" style icons: 0 pictures', line) for line in lines)
