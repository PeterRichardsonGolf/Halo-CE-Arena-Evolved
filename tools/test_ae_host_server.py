"""HOST A SERVER (0.7.7, plan Task 9; the approved mockups A-1 to A-6, B-2, C-1, C-2; port/linux/game/
ae_screen_host_server.c, ae_host_server.c, port/linux/src/ae_host_dedicated.c), headless through the ae_test harness:
the page from MULTIPLAYER, its start (the server as a process of its own, this same game with --server, its settings
file and its status file in the save root's server/ folder), this PC joining it when it is ready, MY SERVER once it
runs, the main menu's MY SERVER card, and QUIT asking what becomes of it (KEEP RUNNING focused).

The server's process runs in the game's own network namespace (the harness's) and ends with it.

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_host_server.py
    AE_MENUS_BUILD=$PWD/build/linux64 python3 tools/test_ae_host_server.py [case ...]   (the frames alone)
"""
import importlib.util
import re
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_host_server_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

LEAD_IN = 12
TO_PAGE = ("pad:a", "pad:down", "pad:a", "wait")
# (the page opens on START SERVER, pinned last; up: JOIN IT WHEN READY, right: NO; down back to START)
NO_JOIN = ("pad:up", "pad:right", "pad:down")
# name, the presses after the lead-in, seconds to run past them
CASES = (
    ("page", TO_PAGE + ("wait",), 3),
    ("page-public", TO_PAGE + ("pad:up", "pad:up", "pad:up", "pad:up", "pad:right", "wait"), 3),
    ("start-join", TO_PAGE + ("pad:a",) + ("wait",) * 26, 4),
    ("running", TO_PAGE + NO_JOIN + ("pad:a",) + ("wait",) * 22, 3),
    ("running-card", TO_PAGE + NO_JOIN + ("pad:a",) + ("wait",) * 20 + ("pad:b", "wait", "pad:b", "wait"), 3),
    ("quit-asks", TO_PAGE + NO_JOIN + ("pad:a",) + ("wait",) * 20 + ("pad:b", "wait", "pad:b", "wait") +
     ("pad:down",) * 4 + ("pad:a", "wait"), 3),
    ("stop-asks", TO_PAGE + NO_JOIN + ("pad:a",) + ("wait",) * 20 + ("pad:down",) * 8 + ("pad:a", "wait"), 3),
    ("stop-yes", TO_PAGE + NO_JOIN + ("pad:a",) + ("wait",) * 20 + ("pad:down",) * 8 + ("pad:a", "wait", "pad:down",
                                                                                         "pad:a", "wait", "wait", "wait"), 3),
)


def play(cfg, out, name, presses, tail):
    result, text, pngs = menus.play(cfg, out, name, views=0, test_input="menu:" + " ".join(["wait"] * LEAD_IN + list(presses)),
                                    env={"HALO_AE_NETWORK": "lan"}, shots=8, exit_after=LEAD_IN + len(presses) + tail)
    frame = None
    if pngs:
        frame = out / f"{name}.png"
        frame.write_bytes(pngs[-1].read_bytes())
    saves = sorted((out / name).rglob("server/out.txt"))
    server_out = saves[0].read_text(errors="replace") if saves else ""
    server_debug = sorted((out / name).rglob("server/data/debug.txt"))
    server_text = server_out + (server_debug[0].read_text(errors="replace") if server_debug else "")
    return result, text, server_text, frame


def run_cases(cfg, out, only=()):
    runs = {}
    for name, presses, tail in CASES:
        if only and name not in only:
            continue
        runs[name] = play(cfg, out, name, presses, tail)
    return runs


@pytest.fixture(scope="module")
def cfg():
    c = menus.config()
    if not c:
        pytest.skip("needs the ae_test harness config (data_dir with maps) and a built game ($AE_MENUS_BUILD)")
    return c


@pytest.fixture(scope="module")
def runs(cfg):
    return run_cases(cfg, menus.out_dir(cfg, "host-server"))


def test_page_opens_private_by_default(runs):
    _, text, _, frame = runs["page"]
    assert "ae main menu: HOST A SERVER" in text
    assert "ae host server: page open (not running)" in text
    assert frame


def test_start_and_join(runs):
    _, text, server, frame = runs["start-join"]
    assert "ae host server: START SERVER" in text
    assert re.search(r"ae host server: '.+' starting \(game port 230\d, web page 80\d\d, private\)", text), text[-3000:]
    assert "ae host server: starting -> ready" in text, text[-3000:]
    assert "ae host server: joining it at 127.0.0.1" in text
    assert "ae host server: joined it" in text, text[-3000:]
    # (this PC is its owner in the game: tools/test_server_ingame.py's test_owner_key_from_the_starter)
    assert "the server says this player is OWNER here" in text or "ae create game" in text, text[-3000:]


def test_running_page(runs):
    _, text, _, frame = runs["running"]
    assert "ae host server: starting -> ready" in text
    assert "joining it" not in text
    assert frame


def test_quit_asks_keep_running_first(runs):
    _, text, _, frame = runs["quit-asks"]
    assert "ae main menu: QUIT" in text
    assert "ae main menu: quit" not in text
    assert frame


def test_stop_asks_then_stops(runs):
    _, text, _, _ = runs["stop-asks"]
    assert "ae host server: STOP SERVER" in text and "asked to stop" not in text
    _, text, server, _ = runs["stop-yes"]
    assert "ae host server: asked to stop" in text
    assert "ae host server: stopping -> idle" in text or "ae host server: ready -> idle" in text, text[-3000:]


if __name__ == "__main__":
    c = menus.config()
    if not c:
        raise SystemExit("needs $AE_MENUS_BUILD and the harness config")
    folder = menus.out_dir(c, "host-server")
    for case, (_, _, _, frame) in run_cases(c, folder, sys.argv[1:]).items():
        print(case, frame)
