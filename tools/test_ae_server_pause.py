"""Running a dedicated server from the pause menu (0.7.7, plan Task 12; the approved mockups E-2 to E-5, F-1, F-2, F-4;
port/linux/game/ae_screen_pause.c, ae_server_ingame.c), in real games, headless, through the ae_test harness: a dedicated
server (tools/ae_test/dedicated_helper.py, started in the game's own network namespace) and the player's game joining
it with AE's menus; a second client, Goat, joined too (the person the card acts on).

- a joiner whose address an admins.txt row gives a role: the pause has PEOPLE's PLAYERS and SERVER's RUN THE SERVER
  (with the role); without a role it has nothing new;
- RUN THE SERVER's page: UP NEXT (left / right picks the next game, sent to the server after a moment), END GAME NOW
  (asks, CANCEL first, not red), SAY TO EVERYONE... (AE's keyboard), RESTART THE SERVER (the owner's: muted for an
  admin); the right card is the server's;
- a moderator: UP NEXT open (the default), RESTART and the ban muted;
- the player card of a person (Goat): MOVE, MUTE IN CHAT, KICK FOR 2 MINUTES, BAN FROM THIS SERVER (an admin's); the ban
  asks how long (1 HOUR / 1 DAY / 1 WEEK / FOREVER, 1 DAY first), and the server bans for that long.

The frames: <case>.png in the run's folder (ae_menus/<stamp>-server-pause).

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_server_pause.py
    AE_MENUS_BUILD=$PWD/build/linux64 python3 tools/test_ae_server_pause.py [case ...]   (the frames alone)
"""
import importlib.util
import re
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
_spec = importlib.util.spec_from_file_location("ae_server_pause_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

# seconds before the first press: the server's lobby, the join, the map's load, the game's start
LEAD_IN = 40
ADMIN = "cidr:127.0.0.0/8<TAB>admin<TAB>0<TAB>0<TAB><TAB>"
MODERATOR = "cidr:127.0.0.0/8<TAB>moderator<TAB>0<TAB>0<TAB><TAB>"
TO_SERVER = ("start", "wait") + ("down",) * 5
TO_PLAYERS = ("start", "wait") + ("down",) * 4
# name, admins.txt row, the presses after the lead-in (one a second)
CASES = (
    ("admin-pause", ADMIN, ("start", "wait", "wait")),
    ("none-pause", "", ("start", "wait", "wait")),
    ("admin-row", ADMIN, TO_SERVER + ("wait",)),
    ("server-page", ADMIN, TO_SERVER + ("a", "wait", "wait")),
    ("server-next", ADMIN, TO_SERVER + ("a", "wait", "right", "wait", "wait", "wait")),
    ("server-end-asks", ADMIN, TO_SERVER + ("a", "wait", "down", "a", "wait", "wait")),
    ("server-end-yes", ADMIN, TO_SERVER + ("a", "wait", "down", "a", "wait", "down", "a", "wait", "wait", "wait")),
    ("server-restart-muted", ADMIN, TO_SERVER + ("a", "wait", "down", "down", "down", "wait")),
    ("moderator-page", MODERATOR, TO_SERVER + ("a", "wait", "down", "down", "down", "wait")),
    ("players-page", ADMIN, TO_PLAYERS + ("a", "wait", "wait")),
    # (Goat's card, the person under this player's own row: MOVE, MUTE IN CHAT, KICK, BAN; a moderator's has no BAN)
    ("card-admin", ADMIN, TO_PLAYERS + ("a", "wait", "down", "a", "wait", "down", "down", "down", "wait")),
    ("card-moderator", MODERATOR, TO_PLAYERS + ("a", "wait", "down", "a", "wait", "wait")),
    # (BAN asks how long: 1 DAY first; right: 1 WEEK; down to the ban, A: the server bans for a week)
    ("ban-asks", ADMIN, TO_PLAYERS + ("a", "wait", "down", "a", "wait", "down", "down", "down", "a", "wait", "wait")),
    ("ban-week", ADMIN, TO_PLAYERS + ("a", "wait", "down", "a", "wait", "down", "down", "down", "a", "wait", "right",
                                      "wait", "down", "a", "wait", "wait", "wait")),
    ("mute", ADMIN, TO_PLAYERS + ("a", "wait", "down", "a", "wait", "down", "a", "wait", "wait", "wait")),
    ("move", ADMIN, TO_PLAYERS + ("a", "wait", "down", "a", "wait", "a", "wait", "down", "a", "wait", "wait", "wait")),
)


def helper(work, admins, rotation=None):
    build = Path(menus.build())
    maps = ROOT / "assets" / "maps"
    command = [sys.executable, str(HERE / "ae_test" / "dedicated_helper.py"), "--game", str(build / "halo"),
               "--maps", str(maps), "--work", str(work), "--goat"]
    if admins:
        command += ["--admins", admins]
    if rotation:
        command += ["--rotation", rotation]
    return command


def play(cfg, out, name, admins, presses, extra=()):
    work = out / f"{name}-server"
    work.mkdir(parents=True, exist_ok=True)
    env = {"HALO_ARENA_MENUS": "1", "SDL_AUDIO_DRIVER": "dummy", "HALO_NETWORK_TEST_NAME": "Player"}
    spec = {"name": name, "build": menus.build(), "window": "1920x1080", "menus": "pc", "network_test": "join",
            "address": "127.0.0.200", "broadcast": "127.0.0.201", "exit_after": LEAD_IN + len(presses) + 4,
            "screenshots": 8, "test_input": "menu:" + " ".join(["wait"] * LEAD_IN + list(presses)),
            "expect": {"ticks": 0}, "env": env, "helper": helper(work, admins) + list(extra)}
    result = menus.harness.play(cfg, spec, out / name)
    debug = out / name / "debug.txt"
    text = debug.read_text(errors="replace") if debug.exists() else ""
    server_debug = work / "server" / "data" / "debug.txt"
    server_text = server_debug.read_text(errors="replace") if server_debug.exists() else ""
    server_text += (work / "server" / "out.txt").read_text(errors="replace") if (work / "server" / "out.txt").exists() \
        else ""
    pngs = sorted((out / name).rglob("frame*.png")) or sorted((out / name).rglob("*.png"))
    frame = None
    if pngs:
        frame = out / f"{name}.png"
        frame.write_bytes(pngs[-1].read_bytes())
    return result, text, server_text, frame


def run_cases(cfg, out, only=()):
    runs = {}
    for name, admins, presses in CASES:
        if only and name not in only:
            continue
        runs[name] = play(cfg, out, name, admins, presses)
    return runs


@pytest.fixture(scope="module")
def cfg():
    c = menus.config()
    if not c:
        pytest.skip("needs the ae_test harness config (data_dir with maps) and a built game ($AE_MENUS_BUILD)")
    return c


@pytest.fixture(scope="module")
def runs(cfg):
    return run_cases(cfg, menus.out_dir(cfg, "server-pause"))


def rows_of(text):
    found = re.findall(r"ae pause: player 1 rows: (.*)", text)
    return found[-1] if found else ""


def test_admin_has_the_server_rows(runs):
    _, text, _, frame = runs["admin-pause"]
    assert "the server says this player is ADMIN here" in text
    assert "PLAYERS" in rows_of(text) and "RUN THE SERVER" in rows_of(text), rows_of(text)
    assert frame


def test_no_role_sees_nothing_new(runs):
    _, text, _, _ = runs["none-pause"]
    assert "ae pause: open player 1" in text
    assert "RUN THE SERVER" not in rows_of(text) and "PLAYERS" not in rows_of(text), rows_of(text)


def test_server_page_opens(runs):
    _, text, _, frame = runs["server-page"]
    assert "ae pause: player 1 server page (ADMIN)" in text
    assert frame


def test_up_next_goes_to_the_server(runs):
    _, text, server, _ = runs["server-next"]
    assert re.search(r"ae pause: player 1 asks the server to play \S+ next: sent", text), text[-3000:]
    assert re.search(r"game [0-9a-f]{8} \(admin\): sv_rotation_begin \d+", server), server[-3000:]


def test_end_game_asks_then_ends(runs):
    _, text, _, _ = runs["server-end-asks"]
    assert "asks the server to end the game" not in text
    _, text, server, _ = runs["server-end-yes"]
    assert "ae pause: player 1 asks the server to end the game: sent" in text
    assert re.search(r"\(admin\): sv_end_game", server), server[-3000:]


def test_restart_is_the_owners(runs):
    _, text, _, frame = runs["server-restart-muted"]
    assert "the server says this player is ADMIN here" in text
    assert frame


def test_card_rows_by_role(runs):
    _, text, _, frame = runs["card-admin"]
    assert "ae pause: player 1 card of 'Goat'" in text, text[-3000:]
    assert frame
    _, text, _, _ = runs["card-moderator"]
    assert "ae pause: player 1 card of 'Goat'" in text


def test_ban_asks_how_long_then_bans(runs):
    _, text, _, frame = runs["ban-asks"]
    assert "asks the server to ban" not in text
    assert frame
    _, text, server, _ = runs["ban-week"]
    assert "ae pause: player 1 asks the server to ban 'Goat' for 1 WEEK: sent" in text, text[-3000:]
    assert re.search(r"\(admin\): ban \d+ 10080m", server), server[-3000:]


def test_mute_and_move_go_to_the_server(runs):
    _, text, server, _ = runs["mute"]
    assert re.search(r"\(admin\): mute \d+", server), server[-3000:]
    _, text, server, _ = runs["move"]
    assert re.search(r"\(admin\): sv_team \d+ (red|blue)", server), server[-3000:]


def test_moderator_page(runs):
    _, text, _, frame = runs["moderator-page"]
    assert "ae pause: player 1 server page (MODERATOR)" in text
    assert frame


if __name__ == "__main__":
    c = menus.config()
    if not c:
        raise SystemExit("needs $AE_MENUS_BUILD and the harness config")
    folder = menus.out_dir(c, "server-pause")
    for case, (result, _, _, frame) in run_cases(c, folder, sys.argv[1:]).items():
        print(case, frame)
