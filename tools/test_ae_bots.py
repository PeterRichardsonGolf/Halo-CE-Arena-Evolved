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
    # (AE: SHOOTING SKILL and MOVEMENT SKILL after BOT SKILL, ae_bots_combat.c; PLAYFIGHT after them,
    # ae_bots.c's ae_bots_playfight)
    assert list(rows) == ["bots.count", "bots.skill", "bots.shooting_skill", "bots.movement_skill", "bots.playfight",
                          "bots.teams"]
    assert [value for _, value in rows["bots.count"][2]] == [str(n) for n in range(16)]
    assert rows["bots.count"][2][0] == ("OFF", "0")
    # (the names bots.c's bots_skills and bots_teams read; RECRUIT's in AE's ae_bots_skill.inc, read by name first)
    bots = (ROOT / "port" / "linux" / "game" / "bots.c").read_text()
    recruit = (ROOT / "port" / "linux" / "game" / "ae_bots_skill.inc").read_text()
    assert [value for _, value in rows["bots.skill"][2]][0] == "recruit"
    assert 'if (!strcmp(name, "recruit")) return AE_BOTS_SKILL_RECRUIT;' in bots
    for _, skill in rows["bots.skill"][2]:
        assert f'{{ "{skill}", ' in bots + recruit, skill
    # (the two parts' levels: SAME AS BOT SKILL first, then BOT SKILL's own, the names ae_bots_combat.c reads)
    combat = (ROOT / "port" / "linux" / "game" / "ae_bots_combat.c").read_text()
    for part in ("bots.shooting_skill", "bots.movement_skill"):
        values = [value for _, value in rows[part][2]]
        assert values == ["same"] + [value for _, value in rows["bots.skill"][2]], part
        assert rows[part][2][0][0] == "SAME AS BOT SKILL"
        assert f'config_string("{part}")' in combat
    assert '{ "recruit", "easy", "normal", "heroic", "legendary" }' in combat
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
    assert re.search(r'\{ "bots\.playfight", _config_boolean, "false",', config)
    assert re.search(r'\{ "bots\.shooting_skill", _config_string, "\\"same\\"", "HALO_BOT_SHOOTING_SKILL",', config)
    assert re.search(r'\{ "bots\.movement_skill", _config_string, "\\"same\\"", "HALO_BOT_MOVEMENT_SKILL",', config)
    assert [value for _, value in {row[1]: row for row in port_settings.SCREENS["mods_setup/bots"]["rows"]}["bots.playfight"][2]] == ["false", "true"]


# ---------- games (the box)

BUILD = Path(os.environ.get("AE_BOTS_BUILD", ""))
DATA = Path(os.environ.get("AE_BOTS_DATA", ""))
PLAYER = re.compile(r"player (\d+): (?:\(-?[\d.]+ -?[\d.]+ -?[\d.]+\)|dead).*? k(-?\d+) d(-?\d+) .*?m(\d+)")


def _play(tmp_path, test, seconds, layouts=None, **settings):
    if not os.environ.get("AE_BOTS_BUILD") or not (BUILD / "halo").is_file() or not (DATA / "maps").is_dir():
        pytest.skip("needs $AE_BOTS_BUILD (a linux64 build) and $AE_BOTS_DATA (a data root with the stock maps)")
    data, save = tmp_path / "data", tmp_path / "save"
    data.mkdir()
    save.mkdir()
    for name in ("maps", "packs"):
        if (DATA / name).exists() and not (name == "maps" and layouts):
            (data / name).symlink_to((DATA / name).resolve())
    if layouts:
        # (maps/ of links to the maps, and maps/layouts/<map>.txt: the map layout files, ae_layout.c)
        (data / "maps" / "layouts").mkdir(parents=True)
        for entry in (DATA / "maps").iterdir():
            (data / "maps" / entry.name).symlink_to(entry.resolve())
        for name, text in layouts.items():
            (data / "maps" / "layouts" / f"{name}.txt").write_text(text)
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


def test_playfight_never_shoots(tmp_path):
    """PLAYFIGHT (bots.playfight): LEGENDARY bots with the fight by level (ae_bots_combat.c) still never fire,
    throw or strike: no round leaves their magazines, no grenade their hands, no kill is theirs"""
    status, output = _play(tmp_path, "local:bloodgulch", 60, HALO_BOTS=7, HALO_BOT_SKILL="legendary",
                           HALO_BOT_PLAYFIGHT="true", HALO_BOT_EVENTS="1")
    assert status == 0, output[-3000:]
    bots = _bots(output)
    assert len(bots) == 7, f"{len(bots)} bots"
    assert sum(kills for kills, _ in bots.values()) == 0
    assert re.search(r"bot event: .* ev=pos ", output)
    assert not re.search(r"bot event: .* ev=(shot|first_shot|melee|throw|hit) ", output)


def test_shooting_and_movement_skills(tmp_path):
    """SHOOTING SKILL and MOVEMENT SKILL each at a level of their own (bots.shooting_skill, bots.movement_skill):
    halo.log's fight line names both; RECRUIT's shooting throws at most rarely and never aims at a head"""
    status, output = _play(tmp_path, "local:bloodgulch", 95, HALO_BOTS=7, HALO_BOT_SKILL="legendary",
                           HALO_BOT_SHOOTING_SKILL="recruit", HALO_BOT_MOVEMENT_SKILL="heroic")
    assert status == 0, output[-3000:]
    lines = re.findall(r"bots: combat tick \d+ \(shooting (\w+), movement (\w+)\): grenades (\d+), .*? head aims (\d+), "
                       r"retreats (\d+), charges (\d+), jumps (\d+)", output)
    assert lines, output[-3000:]
    shooting, movement, grenades, head_aims, retreats, charges, jumps = lines[-1]
    assert (shooting, movement) == ("recruit", "heroic")
    assert int(head_aims) == 0 and int(grenades) <= 3
    # (HEROIC's movement: jumps in a fight)
    assert int(jumps) > 0


TRAVEL = re.compile(r"bots: travel tick \d+: teleports (\d+) \((\d+) planned, \d+ pads missed\), jumps (\d+) \((\d+) missed\), "
                    r"links tried (\d+) taken (\d+) declined (\d+), packs taken (\d+) of (\d+) detours")


def test_bots_take_teleporters(tmp_path):
    """Getting around (ae_bots_travel.c): Blood Gulch's two teleporter pads (the bases' roofs to the field) are read as
    links, and bots take them on their way (halo.log's minute line); with debug.bot_links false none are taken"""
    status, output = _play(tmp_path, "local:bloodgulch", 70, HALO_BOTS=7)
    assert status == 0, output[-3000:]
    assert "bots: travel: 2 links (2 teleporter pads, 0 layout jumps), 0 avoid zones; links taken" in output
    lines = TRAVEL.findall(output)
    assert lines, output[-3000:]
    teleports, planned, jumps, missed, tried, taken, declined, packs, detours = map(int, lines[-1])
    assert taken > 0 and tried >= taken and planned <= teleports


def test_bots_without_links(tmp_path):
    status, output = _play(tmp_path, "local:bloodgulch", 70, HALO_BOTS=7, HALO_BOT_LINKS="false")
    assert status == 0, output[-3000:]
    assert "links off (debug.bot_links)" in output
    lines = TRAVEL.findall(output)
    assert lines, output[-3000:]
    assert int(lines[-1][4]) == 0 and int(lines[-1][5]) == 0


def test_layout_bot_lines(tmp_path):
    """The layout file's bot lines (ae_layout_rules.h): debug.bot_hints names the map's layout header; a file with a
    bot jump and a bot avoid is read for the bots (a jump link more, the avoid zone's surfaces closed)"""
    first = tmp_path / "hints"
    first.mkdir()
    status, output = _play(first, "local:bloodgulch", 15, HALO_BOTS=1, HALO_BOT_HINTS="true")
    assert status == 0, output[-3000:]
    header = re.search(r"bots: hint: this map's layout file .* begins: (layout 1 bloodgulch [0-9a-f]{8})", output)
    assert header, output[-3000:]
    second = tmp_path / "layout"
    second.mkdir()
    text = (header.group(1) + "\n# a test of the bot lines\nbot jump at 40.0 -79.0 0.5 to 42.0 -79.0 0.5\n"
            "bot avoid 65.0 -120.0 0.0 radius 6\n")
    status, output = _play(second, "local:bloodgulch", 15, layouts={"bloodgulch": text}, HALO_BOTS=1)
    assert status == 0, output[-3000:]
    assert re.search(r"layout: .*bloodgulch\.txt \(0 items, .* 1 bot jumps, 1 bot avoids\)", output), output[-3000:]
    assert "bots: travel: 3 links (2 teleporter pads, 1 layout jumps), 1 avoid zones" in output
    closed = re.search(r"bots: travel: layout line 4: avoid \(65\.0 -120\.0 0\.0\) radius 6\.0: (\d+) walkable surfaces closed",
                       output)
    assert closed and int(closed.group(1)) > 0, output[-3000:]
