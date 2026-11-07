"""A dedicated server's lobby page (0.7.7; port/linux/game/ae_screen_server_lobby.c, ae_server_lobby.c; the approved
mockups-077-server/server-lobby option A), in real games, headless, through the ae_test harness: a dedicated server
(tools/ae_test/dedicated_helper.py, in the game's own network namespace, its playlist on map packs) and the player's game
joining it with AE's menus. Each state of the page is captured (debug.capture_on_event's server_lobby_* kinds, and the
last frame):

- waiting: no bots, two people wanted: WAITING FOR PLAYERS, AFTER THAT's games, the message of the day, the chat hint;
- vote: bots fill the game, the server ends its first game; back in its lobby the vote begins (VOTE FOR THE NEXT GAME,
  A votes: YOUR VOTE), the winner shows (WON THE VOTE, framed green), then the count (STARTS IN);
- admin: a role from admins.txt: RUN THE SERVER, and its page;
- split: two players on this PC: THIS PC'S VOTE, CHANGE TEAM names the controller;
- locked: network.team_change off on the server: THE SERVER BALANCES TEAMS (a team change is refused);
- download: a pack map this PC lacks: GETTING THE MAP with its progress (the server's upload slowed);
- card / leave: the player card (X) and the leave question (B);
- kicked: the server kicks the player in its lobby;
- 4x3 / 21x9: the page at 1440 x 1080 and 2560 x 1080.

Its data folder (the client's and the server's): $AE_SERVER_LOBBY_DATA (default: test-data beside this tree), with maps
linked, and maps_pro / maps_smp each with its maplist.txt and previews/ (the main install's).

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_server_lobby.py
    AE_MENUS_BUILD=$PWD/build/linux64 python3 tools/test_ae_server_lobby.py [case ...]   (the frames alone)
"""
import importlib.util
import os
import re
import shutil
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
_spec = importlib.util.spec_from_file_location("ae_server_lobby_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

DATA = Path(os.environ.get("AE_SERVER_LOBBY_DATA", ROOT / "test-data"))
ADMIN = "cidr:127.0.0.0/8<TAB>admin<TAB>0<TAB>0<TAB><TAB>"
ROTATION = "pat_prisoner@pro:ae_br_ts, hektik@smp:ev_team_sly, frigid_keep@smp:ae_snd, pat_chillout@pro:ae_team_sly"
MOTD = "AE West test servers are up! Be kind, report problems on GitHub. Map packs download from the server."
BASE = ["HALO_SERVER_NAME=AE Official #1", "HALO_SERVER_REGION=NA West", f"HALO_SERVER_MOTD={MOTD}",
        "HALO_SERVER_MAX_PLAYERS=8", "HALO_SERVER_MAP_VOTE=true", "HALO_SERVER_MAP_VOTE_SECONDS=20",
        "HALO_SERVER_MAP_VOTE_CHOICES=4", "HALO_SERVER_LOBBY_SECONDS=10"]
WAITING = ["HALO_SERVER_BOTS_FILL=off", "HALO_SERVER_MIN_PLAYERS=2"]
BOTS = ["HALO_SERVER_BOTS_FILL=minimum", "HALO_SERVER_MIN_PLAYERS=6", "HALO_SERVER_BOTS_DELAY=0"]
KINDS = ("server_lobby_waiting,server_lobby_vote,server_lobby_result,server_lobby_countdown,server_lobby_download,"
         "server_lobby_voted,server_lobby_dialog,server_lobby_column,server_lobby_closed,server_lobby_transfer,server_lobby_team,server_lobby_removed,server_lobby_chat")
# (the join takes ~6 s; the lobby is up by ~10 s. With bots the first game starts at once; the server ends it at 45 s and
# its lobby's vote begins about 10 s later. A second person (Goat, who never votes) keeps the vote open its 20 s)
END_AT = 45
WAIT_TO_VOTE = ("wait",) * 46

# name, server settings, admins.txt row, rotation, presses (one a second after the lead-in), window, players, packs the
# client has, seconds after the presses
CASES = (
    ("waiting", WAITING + ["after:20:say Welcome! Say !nominate and a map's name to put it in the next vote."], "", ROTATION,
     ("wait",) * 8 + ("rb", "wait", "lb", "wait"), "1920x1080", 1, ("maps_pro", "maps_smp")),
    ("vote", BOTS + [f"after:{END_AT}:sv_end_game"], "", ROTATION,
     WAIT_TO_VOTE + ("down", "a", "wait") + ("wait",) * 30, "1920x1080", 1, ("maps_pro", "maps_smp"), True),
    ("admin", WAITING, ADMIN, ROTATION, ("wait",) * 4 + ("down", "wait", "wait", "a", "wait", "wait"), "1920x1080", 1,
     ("maps_pro", "maps_smp")),
    ("split", BOTS + [f"after:{END_AT}:sv_end_game"], "", ROTATION, WAIT_TO_VOTE + ("down", "down", "a", "wait", "wait"),
     "1920x1080", 2, ("maps_pro", "maps_smp"), True),
    ("locked", WAITING + ["HALO_NET_TEAM_CHANGE=off"], "", ROTATION, ("wait",) * 4 + ("y", "wait", "wait", "wait", "wait"),
     "1920x1080", 1, ("maps_pro", "maps_smp")),
    ("teamchange", WAITING + ["HALO_NET_TEAM_CHANGE=allow"], "", ROTATION, ("wait",) * 4 + ("right", "wait", "wait"),
     "1920x1080", 1, ("maps_pro", "maps_smp")),
    ("download", WAITING + ["HALO_NET_MAP_SHARE_UPLOAD=600", "HALO_NET_MAP_SHARE_LAN_UPLOAD=600"], "",
     "hektik@smp:ev_team_sly, pat_prisoner@pro:ae_br_ts, pat_chillout@pro:ae_team_sly, pat_ratrace@pro:ae_king",
     ("wait",) * 6, "1920x1080", 1, ("maps_pro",)),
    ("card", ["HALO_SERVER_BOTS_FILL=off", "HALO_SERVER_MIN_PLAYERS=3"], "", ROTATION, ("wait",) * 6 + ("rb", "down", "wait", "x", "wait", "wait", "b", "b", "wait", "wait"),
     "1920x1080", 1, ("maps_pro", "maps_smp"), True),
    ("kicked", WAITING + ["after:28:kick Player"], "", ROTATION, ("wait",) * 18, "1920x1080", 1, ("maps_pro", "maps_smp")),
    ("4x3", WAITING, "", ROTATION, ("wait",) * 4, "1440x1080", 1, ("maps_pro", "maps_smp")),
    ("21x9", WAITING, "", ROTATION, ("wait",) * 4, "2560x1080", 1, ("maps_pro", "maps_smp")),
)
LEAD_IN = 16


def helper(work, settings, admins, rotation, goat=False):
    build = Path(menus.build())
    command = [sys.executable, str(HERE / "ae_test" / "dedicated_helper.py"), "--game", str(build / "halo"),
               "--maps", str(DATA / "maps"), "--work", str(work), "--rotation", rotation,
               "--pack", str(DATA / "maps_pro"), "--pack", str(DATA / "maps_smp"), "--weapon-packs", str(DATA / "packs")]
    if admins:
        command += ["--admins", admins]
    if goat:
        command += ["--goat"]
    for item in BASE + list(settings):
        if item.startswith("after:"):
            command += ["--after", item[len("after:"):]]
        else:
            command += ["--extra", item]
    return command


def play(cfg, out, name, settings, admins, rotation, presses, window, players, packs, goat=False):
    work = out / f"{name}-server"
    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True)
    local = dict(cfg, data_dir=str(DATA))
    env = {"HALO_ARENA_MENUS": "1", "SDL_AUDIO_DRIVER": "dummy", "HALO_NETWORK_TEST_NAME": "Player",
           "HALO_CAPTURE_ON_EVENT": KINDS}
    if players > 1:
        # (the harness gives a join one player: the network test's own setting joins this PC's others)
        env["HALO_NETWORK_TEST_LOCAL_PLAYERS"] = str(players)
    spec = {"name": name, "build": menus.build(), "window": window, "menus": "pc", "network_test": "join",
            "address": "127.0.0.200", "broadcast": "127.0.0.201", "exit_after": LEAD_IN + len(presses) + 4,
            "screenshots": 600, "test_input": "menu:" + " ".join(["wait"] * LEAD_IN + list(presses)),
            "expect": {"ticks": 0}, "env": env, "local_players": players, "pack_folders": list(packs),
            "helper": helper(work, settings, admins, rotation, goat)}
    result = menus.harness.play(local, spec, out / name)
    debug = out / name / "debug.txt"
    text = debug.read_text(errors="replace") if debug.exists() else ""
    server_debug = work / "server" / "data" / "debug.txt"
    server_text = server_debug.read_text(errors="replace") if server_debug.exists() else ""
    if (work / "server" / "out.txt").exists():
        server_text += (work / "server" / "out.txt").read_text(errors="replace")
    captures = sorted((out / name / "captures").glob("*.png")) if (out / name / "captures").is_dir() else []
    pngs = sorted((out / name).rglob("frame*.png")) or sorted((out / name).rglob("*.png"))
    frame = None
    if pngs:
        frame = out / f"{name}.png"
        frame.write_bytes(pngs[-1].read_bytes())
    # (each capture named by the event that took it, in the log's order)
    kinds = re.findall(r"test capture: (server_lobby_\w+)", text)
    for index, (kind, png) in enumerate(zip(kinds, captures)):
        (out / f"{name}-{index:02d}-{kind}.png").write_bytes(png.read_bytes())
    return result, text, server_text, frame


def run_cases(cfg, out, only=()):
    runs = {}
    for case in CASES:
        if only and case[0] not in only:
            continue
        runs[case[0]] = play(cfg, out, *case)
    return runs


@pytest.fixture(scope="module")
def cfg():
    c = menus.config()
    if not c or not (DATA / "maps").exists():
        pytest.skip("needs the ae_test harness config, a built game ($AE_MENUS_BUILD) and the test data folder")
    return c


@pytest.fixture(scope="module")
def runs(cfg):
    return run_cases(cfg, menus.out_dir(cfg, "server-lobby"))


def test_waiting_page(runs):
    _, text, server, frame = runs["waiting"]
    assert re.search(r"ae server lobby: open \(", text), text[-3000:]
    assert "ae server lobby: state WAITING" in text
    assert re.search(r"server lobby: the server's lobby info \(NA WEST, joins AE, waits for 2, \d+ games", text), text[-3000:]
    assert "server lobby: info sent to machine" in server
    assert "ae vote: opened" not in text
    assert frame


def test_vote_on_the_page(runs):
    _, text, server, _ = runs["vote"]
    assert "ae server lobby: state VOTE" in text, text[-3000:]
    assert re.search(r"ae server lobby: voted for \S+:\S+", text), text[-3000:]
    assert "ae server lobby: state RESULT" in text
    assert "ae server lobby: state COUNTDOWN" in text
    assert "ae vote: opened" not in text


def test_role_holder_runs_the_server(runs):
    _, text, _, _ = runs["admin"]
    assert "the server says this player is ADMIN here" in text, text[-3000:]
    assert "ae server lobby: server page (ADMIN)" in text, text[-3000:]


def test_split_screen_vote(runs):
    _, text, _, _ = runs["split"]
    assert "ae server lobby: state VOTE" in text, text[-3000:]
    assert re.search(r"ae server lobby: voted for", text)


def test_locked_teams(runs):
    _, text, _, _ = runs["locked"]
    assert "ae server lobby: team change refused: the server balances teams" in text, text[-3000:]


def test_team_change(runs):
    _, text, _, _ = runs["teamchange"]
    assert "ae server lobby: team change asked (controller 1)" in text, text[-3000:]


def test_download(runs):
    _, text, _, _ = runs["download"]
    assert "ae server lobby: state DOWNLOAD" in text, text[-3000:]


def test_card_and_leave(runs):
    _, text, _, _ = runs["card"]
    assert re.search(r"ae server lobby: card of '\S+'", text), text[-3000:]


def test_kicked(runs):
    _, text, server, _ = runs["kicked"]
    assert re.search(r"kicked", server), server[-3000:]
    assert "ae server lobby: closed" in text, text[-3000:]
    assert "ae server lobby: removed from the server (code 6): told" in text, text[-3000:]


if __name__ == "__main__":
    c = menus.config()
    if not c:
        raise SystemExit("needs $AE_MENUS_BUILD and the harness config")
    folder = menus.out_dir(c, "server-lobby")
    for case, (result, _, _, frame) in run_cases(c, folder, sys.argv[1:]).items():
        print(case, frame)
