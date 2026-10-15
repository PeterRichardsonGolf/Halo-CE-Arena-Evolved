"""Arena Evolved and the bots of local games (OpenCE PR #367, port/linux/game/bots.c; AE's side ae_bots.c):

- the generator (tools/port_settings.py): Game Options' BOTS opens the BOTS screen, whose rows set bots.count,
  bots.skill and bots.teams with the values bots.c reads, and the generated files agree;
- the settings' rows in port_config.c and the values the screen writes;
- with $AE_BOTS_BUILD (a linux64 build's folder) and $AE_BOTS_DATA (a data root with the stock maps): local games
  with bots in AE's gametypes (tools/test_bots.py plays upstream's own), read from halo.log's lines."""
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
GAME_OPTIONS = "main_menu.settings_select.player_setup.player_profile_edit.mods_setup.xml"
BOTS = "main_menu.settings_select.player_setup.player_profile_edit.mods_setup.bots.xml"
BOTS_FIRST_MACHINE = 113


def test_bots_screen():
    spec = port_settings.SCREENS["mods_setup/bots"]
    assert spec["title"] == "BOTS" and spec["screen"] == "bots_settings_screen"
    rows = {row[1]: row for row in spec["rows"]}
    assert list(rows) == ["bots.count", "bots.skill", "bots.teams"]
    assert [value for _, value in rows["bots.count"][2]] == [str(n) for n in range(16)]
    assert rows["bots.count"][2][0] == ("OFF", "0")
    # (the names bots.c's bots_skills and bots_teams read)
    bots = (ROOT / "port" / "linux" / "game" / "bots.c").read_text()
    for _, skill in rows["bots.skill"][2]:
        assert f'{{ "{skill}", ' in bots, skill
    assert [value for _, value in rows["bots.teams"][2]] == ["even", "against"]
    assert '"against"' in bots
    text = (CE / BOTS).read_text()
    for setting, row in rows.items():
        assert f'setting="{setting}" values="{"|".join(value for _, value in row[2])}"' in text, setting
    for row in spec["rows"]:
        helps = row[3] if isinstance(row[3], list) else [row[3]]
        assert all(len(line) <= 52 for text in helps for line in text.split("\n")), row[0]
    assert 73 + len(spec["rows"]) * spec["spacing"] <= spec.get("help_top", 350)
    assert f'"ce/{BOTS}"' in (ROOT / "port" / "assets" / "menus" / "menus.json").read_text()


def test_game_options_opens_bots():
    text = (CE / GAME_OPTIONS).read_text()
    assert 'open="main_menu/settings_select/player_setup/player_profile_edit/mods_setup/bots/bots_settings_screen"' in text
    assert '<string text="BOTS:"/>' in text


def test_settings_rows():
    config = (ROOT / "port" / "linux" / "src" / "port_config.c").read_text()
    assert re.search(r'\{ "bots\.count", _config_integer, "0",', config)
    assert re.search(r'\{ "bots\.skill", _config_string, "\\"normal\\"",', config)
    assert re.search(r'\{ "bots\.teams", _config_string, "\\"even\\"",', config)


# ---------- games (the box)

BUILD = Path(os.environ.get("AE_BOTS_BUILD", ""))
DATA = Path(os.environ.get("AE_BOTS_DATA", ""))
PLAYER = re.compile(r"player (\d+): (?:\(-?[\d.]+ -?[\d.]+ -?[\d.]+\)|dead).*? k(-?\d+) d(-?\d+) .*?m(\d+)")


def _play(tmp_path, test, seconds, **settings):
    if not os.environ.get("AE_BOTS_BUILD") or not (BUILD / "halo").is_file() or not (DATA / "maps").is_dir():
        pytest.skip("needs $AE_BOTS_BUILD (a linux64 build) and $AE_BOTS_DATA (a data root with the stock maps)")
    data, save = tmp_path / "data", tmp_path / "save"
    data.mkdir()
    save.mkdir()
    for name in ("maps", "packs"):
        if (DATA / name).exists():
            (data / name).symlink_to((DATA / name).resolve())
    game = tmp_path / "halo"
    shutil.copy(BUILD / "halo", game)
    env = {**os.environ, "SDL_VIDEODRIVER": "offscreen", "SDL_AUDIO_DRIVER": "dummy", "SDL_AUDIODRIVER": "dummy",
           "HALO_DATA_ROOT": str(data), "HALO_SAVE_ROOT": str(save), "HALO_HIDDEN_WINDOW": "1",
           "HALO_FULLSCREEN": "0", "HALO_NO_VSYNC": "1", "HALO_NO_AUDIO": "1", "HALO_UPDATE_AUTO": "false",
           "HALO_NET_ONLINE": "false", "HALO_NET_ADDRESS": "127.0.0.1", "HALO_NET_BROADCAST": "127.255.255.255",
           "HALO_NETWORK_TEST": test, "HALO_NETWORK_TEST_START": "5", "HALO_NETWORK_TEST_SCORE": "500",
           "HALO_TEST_INPUT": "bot:1", "HALO_EXIT_AFTER": str(seconds + 20)}
    env.update({name: str(value) for name, value in settings.items()})
    result = subprocess.run([str(game)], cwd=tmp_path, env=env, capture_output=True, text=True, errors="replace",
                            timeout=seconds + 180)
    debug = data / "debug.txt"
    return result.returncode, result.stdout + result.stderr + (debug.read_text(errors="replace") if debug.is_file() else "")


def _bots(output):
    bots = {}
    for line in output.splitlines():
        if "network test: tick" in line:
            for match in PLAYER.finditer(line):
                if int(match.group(4)) >= BOTS_FIRST_MACHINE:
                    bots[int(match.group(1))] = (int(match.group(2)), int(match.group(3)))
    return bots


@pytest.mark.parametrize("gametype", ["AE FFA SLAY", "AE TEAM SLY"])
def test_bots_in_ae_slayer(tmp_path, gametype):
    status, output = _play(tmp_path, "local:bloodgulch", 70, HALO_NETWORK_TEST_GAMETYPE=gametype, HALO_BOTS=5,
                           HALO_BOT_SKILL="heroic")
    assert status == 0, output[-3000:]
    bots = _bots(output)
    assert len(bots) == 5, f"{len(bots)} bots"
    assert sum(kills for kills, _ in bots.values()) > 0 and sum(deaths for _, deaths in bots.values()) > 0


def test_no_bots_in_ae_juggernaut(tmp_path):
    status, output = _play(tmp_path, "local:bloodgulch", 25, HALO_NETWORK_TEST_GAMETYPE="AE JUGGER",
                           HALO_BOTS=5)
    assert status == 0, output[-3000:]
    assert not _bots(output)
    assert re.search(r"bots: none in this game: .* is a game of Oddball, not Slayer", output)
