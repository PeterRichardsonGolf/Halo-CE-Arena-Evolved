"""SILENT MOVEMENT / SILENT WEAPONS (port/linux/game/ae_silent.c, ae_silent_rules.c; the rules' own unit test is
port/linux/tests/ae_silent_rules_test.c, run by tools/test_ae_units.py).

- the generator: ARENA OPTIONS has SOUND PRESET, SILENT MOVEMENT and SILENT WEAPONS, then CONTROLLER CROUCH, before
  BALL MELEE (the Oddball-only row stays last), thirteen rows clear of the help, and every other generated screen is
  byte-identical to its file on disk;
- the test setting's config row exists;
- (games, only when $AE_SILENT_BUILD names a linux64 build, through the harness on the machine that plays it):
  * NORMAL / JUST ME / SILENT with three moving, firing bots gathered near the host (HALO_NETWORK_TEST_SHOOT) and
    the host's own weapon swaps (Y every three seconds, an AR + pistol loadout): the counts the game logs per role
    (made, started, muted): NORMAL mutes nothing; JUST ME mutes the bots' movement and none of the host's own
    swaps; SILENT mutes every movement and weapon-swap sound, its own included; the other sounds (firing,
    reloading, impacts...) are never muted and keep playing;
  * joins: a SILENT host refuses stock-like machines (the bots without --ae-silent) with AE's reason (64), takes
    marked ones, and a NORMAL host takes the unmarked ones (the control);
  * the ARENA OPTIONS screen (PC menus) opened directly: the rows drawn (screenshots), SOUND PRESET's SPLIT
    SCREEN sets both rows to SILENT and CUSTOM puts them back.
"""
import os
import re
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import port_settings  # noqa: E402
import test_ae_menus  # noqa: E402  (its harness loader)

CE = ROOT / "port" / "assets" / "menus" / "ce"
ARENA_SCREEN = "main_menu.settings_select.multiplayer_setup.arena_options_edit.xml"
COUNTS = re.compile(r"silent sounds \((?:so far|end of map)\): movement made (\d+), started (\d+) \(own (\d+)\), "
                    r"muted (\d+) \(own (\d+)\); weapons made (\d+), started (\d+) \(own (\d+)\), muted (\d+) "
                    r"\(own (\d+)\); other started (\d+), muted (\d+)")


def test_untouched_screens_are_byte_identical():
    for name, lines in {**port_settings.settings_files(), **port_settings.multiplayer_files()}.items():
        assert "\n".join(lines) == (CE / name).read_text(), name


def test_arena_rows():
    keys = [row[0] for row in port_settings.ARENA_ROWS]
    assert keys[-5:] == ["sound_preset", "silent_movement", "silent_weapons", "controller_crouch", "ball_melee"]
    rows = {row[0]: row for row in port_settings.ARENA_ROWS}
    assert rows["silent_movement"][2] == rows["silent_weapons"][2] == ["NORMAL", "JUST ME", "SILENT"]
    assert rows["sound_preset"][2] == ["CUSTOM", "SPLIT SCREEN"]
    # (a help per value, at most three lines each)
    for key, _, values, helps in port_settings.ARENA_ROWS:
        assert len(helps) == len(values), key
        assert all(text.count("\\n") <= 2 for text in helps), key
    # (the last row's bottom above the help, the help above the buttons' bar at 414)
    last = port_settings.ARENA_ROW_TOP + port_settings.ARENA_ROW_PITCH * (len(keys) - 1)
    assert last + 26 <= port_settings.ARENA_HELP_TOP + 4
    assert port_settings.ARENA_HELP_TOP + port_settings.ARENA_HELP_HEIGHT <= 414
    text = (CE / ARENA_SCREEN).read_text()
    for key in ("sound_preset", "silent_movement", "silent_weapons"):
        assert f'{key}_spinner' in text
    # (the spinners' names are ae_silent.c's)
    glue = (ROOT / "port" / "linux" / "game" / "ae_silent.c").read_text()
    for key in ("sound_preset", "silent_movement", "silent_weapons"):
        assert f'"{key}_spinner"' in glue


def test_config_row():
    text = (ROOT / "port" / "linux" / "src" / "port_config.c").read_text()
    assert re.search(r'\{ "debug\.network_test_silent", _config_string, "\\"\\"", "HALO_NETWORK_TEST_SILENT", '
                     r'_environment_value, /\* AE hook \*/', text)


# ---------- games

def counts(text):
    """the last counts line: {role: (made, started, muted, own started, own muted)} for movement and weapons, and
    other's (None, started, muted, None, None)"""
    found = COUNTS.findall(text)
    assert found, "no 'silent sounds' counts line in debug.txt"
    n = [int(v) for v in found[-1]]
    return {"movement": (n[0], n[1], n[3], n[2], n[4]), "weapons": (n[5], n[6], n[8], n[7], n[9]),
            "other": (None, n[10], n[11], None, None)}


def game(case, env, bots=3, ae_bots=True, test_input=None, exit_after=75, extra=None):
    """one game through the harness: (result, debug.txt, bots.log)"""
    os.environ.setdefault("AE_MENUS_BUILD", os.environ["AE_SILENT_BUILD"])
    cfg = test_ae_menus.config()
    if cfg is None:
        pytest.skip("no build or data ($AE_SILENT_BUILD, the harness's data_dir)")
    harness = test_ae_menus.load_harness()
    out = harness.expand(cfg["out_dir"]) / "ae_silent" / f"{harness.stamp()}-{case}"
    spec = {"name": case, "build": os.environ["AE_SILENT_BUILD"], "map": "bloodgulch", "start": 5,
            "exit_after": exit_after, "env": env}
    if bots:
        spec["bots"] = bots
        spec["bot_args"] = ["--move", "--fire"] + (["--ae-silent"] if ae_bots else [])
    if test_input:
        spec["test_input"] = test_input
    spec.update(extra or {})
    result = harness.play(cfg, spec, out / case)
    text = (out / case / "debug.txt").read_text(errors="replace")
    bots_log = out / case / "bots.log"
    return result, text, bots_log.read_text(errors="replace") if bots_log.exists() else ""


# (the host's own swaps: Y every three seconds from 20 s, with an AR and a pistol)
SWAPS = "menu:" + " ".join(["wait"] * 20 + ["y", "wait", "wait"] * 16)
NEAR = {"HALO_NETWORK_TEST_SHOOT": "4", "HALO_NETWORK_TEST_LOADOUT": "2,3"}


@pytest.mark.skipif(not os.environ.get("AE_SILENT_BUILD"), reason="$AE_SILENT_BUILD names no game")
@pytest.mark.parametrize("mode", ["normal", "just_me", "silent"])
def test_game_modes(mode):
    result, text, _ = game(mode, dict(NEAR, HALO_NETWORK_TEST_SILENT=mode), test_input=SWAPS)
    assert result["status"] == "PASS", result.get("why")
    assert f"network test: silent movement {mode.replace('_', ' ')}, silent weapons {mode.replace('_', ' ')}" in text
    c = counts(text)
    movement, weapons, other = c["movement"], c["weapons"], c["other"]
    # (every mode heard footsteps, the host's swaps and the other sounds: firing, impacts...)
    assert movement[1] > 0 and weapons[1] > 0 and other[1] > 0, c
    # (the other sounds are never muted)
    assert other[2] == 0, c
    # (the host's own swaps were heard: its own weapon sounds)
    assert weapons[3] > 0, c
    if mode == "normal":
        assert movement[2] == 0 and weapons[2] == 0, c
    elif mode == "just_me":
        # (every sound of another player muted, none of the listener's own: the bots' steps, a bot's ready at its
        # respawn; the host's swaps and its own steps heard)
        for role in (movement, weapons):
            assert role[4] == 0, c
            assert role[2] == role[1] - role[3], c
        assert movement[2] > 0, c
    else:
        assert movement[2] == movement[1] and weapons[2] == weapons[1], c


@pytest.mark.skipif(not os.environ.get("AE_SILENT_BUILD"), reason="$AE_SILENT_BUILD names no game")
def test_game_join_refusal():
    refused = {"expect": {"refusals_ok": True}}
    # (stock-like machines in the lobby when the test host's gametype turns SILENT on: dropped, told AE's reason)
    result, text, bots = game("lobby", {"HALO_NETWORK_TEST_SILENT": "silent"}, bots=2, ae_bots=False,
                              exit_after=45, extra=refused)
    assert len(re.findall(r"silent sounds: dropping machine #\d+", text)) == 2, text[-3000:]
    assert len(re.findall(r"rejected, reason 64", bots)) == 2, bots[-2000:]
    assert result["debug"]["players"] == 1
    # (stock-like machines joining the SILENT game in progress: refused at their join)
    late = ["--join-delay", "25"]
    result, text, bots = game("late", {"HALO_NETWORK_TEST_SILENT": "silent"}, bots=2, ae_bots=False,
                              exit_after=50, extra=dict(refused, bot_args=["--move"] + late))
    assert len(re.findall(r"silent sounds: refusing machine #\d+", text)) == 2, text[-3000:]
    assert len(re.findall(r"rejected, reason 64", bots)) == 2, bots[-2000:]
    assert result["debug"]["players"] == 1
    # (the same host takes marked machines, in the lobby and in progress)
    for case, args in (("marked", ["--move", "--ae-silent"]), ("marked-late", ["--move", "--ae-silent"] + late)):
        result, text, bots = game(case, {"HALO_NETWORK_TEST_SILENT": "silent"}, bots=2, exit_after=50,
                                  extra={"bot_args": args})
        assert result["status"] == "PASS", (case, result.get("why"))
        assert "refusing machine" not in text and "dropping machine" not in text and "rejected, reason" not in bots
        assert result["debug"]["players"] >= 3, case
    # (a NORMAL host takes the unmarked ones: the control)
    result, text, bots = game("control", {}, bots=2, ae_bots=False, exit_after=45)
    assert result["status"] == "PASS", result.get("why")
    assert "refusing machine" not in text and "dropping machine" not in text and "rejected, reason" not in bots
    assert result["debug"]["players"] >= 3


@pytest.mark.skipif(not os.environ.get("AE_SILENT_BUILD"), reason="$AE_SILENT_BUILD names no game")
def test_menu_rows():
    """the screen opened directly (PC menus, no gametype edited: every row at its first value): SOUND PRESET (the
    10th row) to SPLIT SCREEN sets both rows SILENT; SILENT MOVEMENT moved to JUST ME shows CUSTOM; SPLIT SCREEN
    again, then CUSTOM, puts back JUST ME / SILENT"""
    presses = ["wait"] * 12 + ["down"] * 9 + ["wait", "right", "wait", "wait", "down", "left", "wait", "wait", "up",
                                             "right", "wait", "wait", "left", "wait", "wait"]
    result, text, _ = game("menu", {"HALO_MENUS": "pc", "HALO_MENU_OPEN":
                                    "main_menu/settings_select/multiplayer_setup/arena_options_edit/"
                                    "arena_options_screen"},
                           bots=0, test_input="menu:" + " ".join(presses), exit_after=len(presses) + 4,
                           extra={"window": "1280x960", "screenshots": 20, "map": "",
                                  "expect": {"ticks": 0, "clean_exit": True}})
    assert result["status"] == "PASS", result.get("why")
    assert "menus: debug.menu_open: there is no widget" not in text
    lines = re.findall(r"menus: (?:SOUND PRESET|silent movement) [^\n]*", text)
    assert lines == [
        "menus: SOUND PRESET split screen: silent movement silent, silent weapons silent",
        "menus: silent movement just me, silent weapons silent: SOUND PRESET custom",
        "menus: SOUND PRESET split screen: silent movement silent, silent weapons silent",
        "menus: SOUND PRESET custom: silent movement just me, silent weapons silent",
    ], lines
    assert result.get("shots"), "no screenshots"
