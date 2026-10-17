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


def test_bots_in_ae_juggernaut(tmp_path):
    """bots play every mode (ae_bots_objective.c): AE JUGGER (the oddball engine with Halo 2's rules) has them, and one
    of them becomes the juggernaut"""
    status, output = _play(tmp_path, "local:chillout", 70, HALO_NETWORK_TEST_GAMETYPE="AE JUGGER", HALO_BOTS=5)
    assert status == 0, output[-3000:]
    assert len(_bots(output)) == 5
    assert not re.search(r"bots: none in this game", output)
    assert re.search(r"bots: objective tick \d+ \(juggernaut\)", output), "no objective line a minute in"


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


@pytest.mark.parametrize("test,seconds,expected", [
    # (CTF on a small map: a flag taken within two minutes and a half)
    ("local:hangemhigh:ctf", 150, r"bots: bot \d+ took the flag"),
    # (King on Chill Out: a bot in the hill within a minute)
    ("local:chillout:king", 70, r"bots: bot \d+ in the hill"),
])
def test_bots_play_the_objective(tmp_path, test, seconds, expected):
    status, output = _play(tmp_path, test, seconds, HALO_BOTS=5)
    assert status == 0, output[-3000:]
    assert len(_bots(output)) == 5
    assert re.search(expected, output), f"no line {expected!r}"
    assert not re.search(r"assert|exception", output, re.I), "an assert or exception in the game's log"


# ---------- vehicles (ae_bots_vehicle.c; notes/specs/2026-10-16-bots-infinite.md area 4)

VEHICLE_LINE = re.compile(r"bots: vehicle tick \d+: seats taken gunner (\d+) passenger (\d+) driver (\d+) .*?"
                          r"kills gunner (\d+) passenger (\d+) driver (\d+); riding s gunner (\d+) passenger (\d+) driver (\d+), "
                          r"parked (\d+), fighting (\d+), longest ride (\d+) s")


def test_bots_take_vehicle_seats(tmp_path):
    """Team Slayer on Blood Gulch (Warthogs) with the driving spike: bots take seats, ride, fight from them and get
    out; none stays for good in a parked vehicle"""
    status, output = _play(tmp_path, "local:bloodgulch:team_slayer", 130, HALO_BOTS=7, HALO_BOT_VEHICLES=2)
    assert status == 0, output[-3000:]
    assert len(_bots(output)) == 7
    taken = re.findall(r"bots: bot \d+ took the (\w+) seat of a (\w+)", output)
    assert len(taken) >= 2, f"seats taken: {taken}"
    lines = VEHICLE_LINE.findall(output)
    assert lines, "no vehicle minute line"
    # (Infinite's inactive vehicle: out after 15 s parked; a gun turret 8 s after its last fight)
    for line in re.findall(r"left the [\w ]+ \(parked\) after ([\d.]+) s", output):
        assert float(line) < 70.0
    assert not re.search(r"assert|exception", output, re.I), "an assert or exception in the game's log"


def test_no_vehicles_no_seats(tmp_path):
    """A gametype without vehicles (AE FFA BALL: vehicle set none): no seat is looked for or taken"""
    status, output = _play(tmp_path, "local:chillout", 70, HALO_NETWORK_TEST_GAMETYPE="AE FFA BALL", HALO_BOTS=5)
    assert status == 0, output[-3000:]
    assert len(_bots(output)) == 5
    assert not re.search(r"bots: bot \d+ took the", output)
    lines = VEHICLE_LINE.findall(output)
    assert lines and all(int(value) == 0 for value in lines[-1][:9])


def test_playfight_gunner_never_fires(tmp_path):
    """PLAYFIGHT with vehicles: a bot in a Warthog's gun never fires it (the trigger is cleared after everything), no
    bot drives, and no one is killed by a bot"""
    status, output = _play(tmp_path, "local:bloodgulch:team_slayer", 90, HALO_BOTS=7, HALO_BOT_SKILL="legendary",
                           HALO_BOT_PLAYFIGHT="true", HALO_BOT_VEHICLES=2, HALO_BOT_EVENTS="1")
    assert status == 0, output[-3000:]
    bots = _bots(output)
    assert len(bots) == 7
    assert sum(kills for kills, _ in bots.values()) == 0
    assert not re.search(r"bot event: .* ev=(shot|first_shot|melee|throw|hit) ", output)
    assert not re.search(r"took the driver seat", output)
