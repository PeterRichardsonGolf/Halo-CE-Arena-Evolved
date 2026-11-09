"""A dedicated server's map vote in AE's menus (0.7.7; port/linux/game/ae_screen_vote.c), headless through the ae_test
harness: a server with server.map_vote on (tools/ae_test/dedicated_helper.py, in the game's own network namespace), the
player's game joining it with AE's menus; the server ends its first game (sv_end_game on its standard input), and in its
lobby after the post-game the vote begins: the popover opens over the player's screen with the choices, A votes (and
votes again for another), and once the vote is over the winner shows a moment and the popover closes.

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_vote.py
    AE_MENUS_BUILD=$PWD/build/linux64 python3 tools/test_ae_vote.py [case ...]   (the frames alone)
"""
import importlib.util
import re
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_vote_server_pause", HERE / "test_ae_server_pause.py")
pause = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(pause)
menus = pause.menus

# (the client joins at ~5 s and plays; the server ends its game at 50 s; the post-game and the lobby: the vote begins at
# ~66 s, for 20 s, with the 3 games and SAME AGAIN)
END_AT = 50
VOTE = ["--extra", "HALO_SERVER_MAP_VOTE=true", "--extra", "HALO_SERVER_MAP_VOTE_SECONDS=20", "--extra",
        "HALO_SERVER_MAP_VOTE_CHOICES=3", "--after", f"{END_AT}:sv_end_game"]
WAIT_TO_VOTE = ("wait",) * 30
CASES = (
    ("vote-open", WAIT_TO_VOTE + ("wait",)),
    ("vote-cast", WAIT_TO_VOTE + ("down", "a", "wait", "wait")),
    ("vote-change", WAIT_TO_VOTE + ("down", "a", "wait", "up", "a", "wait")),
    ("vote-over", WAIT_TO_VOTE + ("down", "a") + ("wait",) * 20),
)


def run_cases(cfg, out, only=()):
    runs = {}
    for name, presses in CASES:
        if only and name not in only:
            continue
        runs[name] = pause.play(cfg, out, name, "", presses, extra=VOTE)
    return runs


@pytest.fixture(scope="module")
def cfg():
    c = menus.config()
    if not c:
        pytest.skip("needs the ae_test harness config (data_dir with maps) and a built game ($AE_MENUS_BUILD)")
    return c


@pytest.fixture(scope="module")
def runs(cfg):
    return run_cases(cfg, menus.out_dir(cfg, "vote"))


def test_vote_opens(runs):
    _, text, server, frame = runs["vote-open"]
    assert re.search(r"lobby: map vote: \d choices for 20 s", server), server[-3000:]
    assert re.search(r"ae vote: opened, \d choices for \d+ s", text), text[-3000:]
    assert frame


def test_a_votes_and_changes(runs):
    _, text, _, _ = runs["vote-cast"]
    assert re.search(r"ae vote: voted for \S+:\S+", text), text[-3000:]
    _, text, _, _ = runs["vote-change"]
    assert len(re.findall(r"ae vote: voted for", text)) >= 2, text[-3000:]


def test_winner_shown_then_closed(runs):
    _, text, _, _ = runs["vote-over"]
    assert re.search(r"ae vote: over, the next game is \S+", text), text[-3000:]
    assert "ae vote: closed (the winner shown)" in text or "ae vote: closed (no vote)" in text


if __name__ == "__main__":
    c = menus.config()
    if not c:
        raise SystemExit("needs $AE_MENUS_BUILD and the harness config")
    folder = menus.out_dir(c, "vote")
    for case, (_, _, _, frame) in run_cases(c, folder, sys.argv[1:]).items():
        print(case, frame)
