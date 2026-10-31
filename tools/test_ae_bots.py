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
        assert rows[part][2][0][0] == "BOT SKILL"
        assert f'config_string("{part}")' in combat
    assert '{ "recruit", "easy", "normal", "heroic", "legendary", "demon" }' in combat
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
    env = {**os.environ, "SDL_VIDEODRIVER": "offscreen", "SDL_AUDIO_DRIVER": "dummy", "HALO_ARENA_MENUS": "0", "SDL_AUDIODRIVER": "dummy",
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
    """bots play every mode (ae_bots_objective.c): AE JUGGER (the oddball engine with its own rules) has them, and one
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


def test_bots_play_the_objective_in_the_next_game(tmp_path):
    """Two King games in a row from one lobby (1 minute in the hill wins): a player's index repeats from game to game, and the bots' objective
    states once carried the last game's pauses and timers into the next (final review I1)."""
    status, output = _play(tmp_path, "local:chillout:king,king", 300, HALO_BOTS=5, HALO_NETWORK_TEST_SCORE=1)
    assert status == 0, output[-3000:]
    assert "network test: game 2, king" in output, "the second game never started"
    second = output.split("network test: game 2, king", 1)[1]
    assert re.search(r"bots: bot \d+ in the hill", second), "no bot in the hill in the second game"
    assert not re.search(r"assert|exception", output, re.I), "an assert or exception in the game's log"


# ---------- vehicles (ae_bots_vehicle.c; notes/specs/2026-10-16-bots-infinite.md area 4)

VEHICLE_LINE = re.compile(r"bots: vehicle tick \d+: seats taken gunner (\d+) passenger (\d+) driver (\d+) .*?"
                          r"kills gunner (\d+) passenger (\d+) driver (\d+) \| riding s gunner (\d+) passenger (\d+) driver (\d+), "
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
    # (an inactive vehicle: out after 15 s parked; a gun turret 8 s after its last fight)
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


# ---------- getting around (ae_bots_travel.c; notes/specs/2026-10-16-bots-infinite.md area 2)

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


def test_layout_spawns_and_flags(tmp_path):
    """Layout spawn and flag lines (queue 141) with the mode guard (bug 139): CTF on stock Blood Gulch with its flags
    cleared plays Slayer; with the flags put back by flag lines and the spawns replaced by spawn lines, it plays CTF and
    the players spawn at the new spawns"""
    first = tmp_path / "header"
    first.mkdir()
    status, output = _play(first, "local:bloodgulch", 15, HALO_BOTS=1, HALO_BOT_HINTS="true")
    assert status == 0, output[-3000:]
    header = re.search(r"bots: hint: this map's layout file .* begins: (layout 1 bloodgulch [0-9a-f]{8})", output)
    assert header, output[-3000:]
    cleared = tmp_path / "cleared"
    cleared.mkdir()
    status, output = _play(cleared, "local:bloodgulch:ctf", 25, layouts={"bloodgulch": header.group(1) + "\nclear flags\n"},
                           HALO_BOTS=3)
    assert status == 0, output[-3000:]
    assert "mode guard: the map has no red CTF flag for CTF: played as Slayer" in output, output[-3000:]
    assert "practice off; slayer" in output
    placed = tmp_path / "placed"
    placed.mkdir()
    text = (header.group(1) + "\nclear flags\nflag ctf at 95.69 -159.45 -0.10 team red\n"
            "flag ctf at 40.24 -79.12 -0.10 team blue\nclear spawns\n"
            "spawn at 98.49 -157.64 1.70 team red games ctf\nspawn at 96.82 -156.99 1.70 team red games ctf\n"
            "spawn at 43.91 -80.68 1.70 team blue games ctf\nspawn at 41.54 -82.79 1.70 team blue games ctf\n")
    status, output = _play(placed, "local:bloodgulch:ctf", 25, layouts={"bloodgulch": text}, HALO_BOTS=3)
    assert status == 0, output[-3000:]
    assert re.search(r"layout: flags 0 of the map's \d+ kept, 2 added: 2 in all", output), output[-3000:]
    assert re.search(r"layout: spawns 0 of the map's \d+ kept, 4 added: 4 in all", output), output[-3000:]
    assert "mode guard" not in output
    assert "practice off; ctf" in output
    near = [m for m in re.finditer(r"player 0: \((-?[\d.]+) (-?[\d.]+) (-?[\d.]+)\)", output)]
    assert near, "player 0 never spawned"
    x, y = float(near[0].group(1)), float(near[0].group(2))
    assert min((x - 97.6) ** 2 + (y + 157.3) ** 2, (x - 42.7) ** 2 + (y + 81.7) ** 2) < 25, (x, y)
    assert not re.search(r"assert|exception", output, re.I), "an assert or exception in the game's log"


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
    assert re.search(r"layout: .*bloodgulch\.txt \(0 items, .* 1 bot jumps, 1 bot avoids, 0 spawns, 0 flags, 0 object lines, 0 vehicles\)", output), output[-3000:]
    assert "bots: travel: 3 links (2 teleporter pads, 1 layout jumps), 1 avoid zones" in output
    closed = re.search(r"bots: travel: layout line 4: avoid \(65\.0 -120\.0 0\.0\) radius 6\.0: (\d+) walkable surfaces closed",
                       output)
    assert closed and int(closed.group(1)) > 0, output[-3000:]


HOST_VIEWS = re.compile(r"net bots: the host has (\d+) local players \(views\) with (\d+) bots in the game")


@pytest.mark.parametrize("settings", [{"HALO_BOTS_LIVE_TEST": "8:7"}, {"HALO_BOTS": "7"}], ids=["in-progress", "lobby"])
def test_network_bots_take_no_host_view(tmp_path, settings):
    """the network bots on the host's own free controllers (ae_lan_bots.c) are no local players of the host: one view.
    Bots that joined the game in progress (bots.count raised in game, as BOTS IN GAME does) were the host's local
    players in 0.3.9's builds, a split-screen view each (network_client_manager.c's player added in game set the local
    player of any player of the host's machine); the lobby's bots (bots.count before the start) never were"""
    status, output = _play(tmp_path, "host:bloodgulch", 40, **settings)
    assert status == 0, output[-3000:]
    # (bots on the host's own controllers 2-4, in the game in progress or the lobby)
    assert re.search(r"(?:net|lan) bots: bot \d+ joined the (?:game in progress|lobby) \(machine #0(?: the host's)?, controller [234],",
                     output), output[-3000:]
    views = [(int(count), int(bots)) for count, bots in HOST_VIEWS.findall(output)]
    assert views, output[-3000:]
    # (0 at the game's first ticks, before its player is the host's)
    assert max(count for count, _ in views) == 1, views


@pytest.mark.parametrize("gametype", ["AE TEAM SLY", "AE FFA SLAY"])
def test_network_bots_off_mid_game_keeps_playing(tmp_path, gametype):
    """BOTS lowered to OFF in a network game in progress (owner 10-23, LAN TEAM AE BR SLAYER, the host alone with 17
    bots): every bot leaves, and the host plays on alone as in a game started alone. Before, the bots' left players
    left one team (or one player) in the game, and the game ended at once (game_engine_should_end_game's one team left
    rule, which still ends a game a person leaves)"""
    status, output = _play(tmp_path, "host:bloodgulch", 50, HALO_NETWORK_TEST_GAMETYPE=gametype,
                           HALO_BOTS_LIVE_TEST="5:5,15:0")
    assert status == 0, output[-3000:]
    left = re.findall(r"net bots: bot \d+ left the game in progress \(fewer wanted, tick (\d+)\)", output)
    assert len(set(left)) >= 1 and len(re.findall(r"net bots: bot \d+ joined the game in progress", output)) >= 5, \
        output[-3000:]
    last_left = max(int(tick) for tick in left)
    ticks = [int(tick) for tick in re.findall(r"network test: tick (\d+) .*?\| playing to", output)]
    # (still playing 20 s after the last bot left; no postgame)
    assert ticks and max(ticks) >= last_left + 20 * 30, (last_left, ticks[-3:])
    assert "switching to postgame" not in output, output[-3000:]


def test_the_ladder():
    """The six levels (owner 10-20): NORMAL and HEROIC are OpenCE's rows, LEGENDARY is AE's halfway from HEROIC to
    DEMON, DEMON is OpenCE's LEGENDARY row; every aim and sight number steps the right way level to level"""
    game = ROOT / "port" / "linux" / "game"
    table = (game / "bots.c").read_text()
    skill = (game / "ae_bots_skill.inc").read_text()

    def numbers(text, name):
        return [float(value.strip().rstrip("f")) for value in re.search(r'\{ "%s", ([^}]*) \}' % name, text)[1].split(",")]

    rows = [numbers(skill, "recruit"), numbers(table, "easy"), numbers(table, "normal"), numbers(table, "heroic"),
            numbers(skill, "legendary"), numbers(skill, "demon")]
    assert numbers(skill, "demon") == numbers(table, "legendary")
    heroic, legendary, demon = rows[3], rows[4], rows[5]
    for index in range(12):
        assert abs(legendary[index] - (heroic[index] + demon[index]) / 2) <= 0.05 * max(1.0, demon[index]) + 0.5, index
    for lower, higher in zip(rows, rows[1:]):
        assert higher[0] < lower[0] and higher[3] < lower[3] and higher[4] < lower[4]  # reaction, aim error, settling
        assert higher[1] > lower[1] and higher[2] > lower[2] and higher[7] >= lower[7]  # sight, view, lead
    assert "skill == AE_BOTS_SKILL_DEMON ? &ae_bots_demon" in skill and "&ae_bots_normal" not in skill
    assert 'if (!strcmp(name, "demon")) return AE_BOTS_SKILL_DEMON;' in table
    assert table.count("bots_skills[") == 2  # the table and the name lookup: nothing reads a row past the AE hook
