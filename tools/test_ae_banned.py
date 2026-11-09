"""0.7.6 HOST MODERATION: BANNED PLAYERS (SETTINGS > ONLINE > HOSTING), headless through the ae_test harness, PC menus: the
bans of a bans.txt (as the engine writes them) listed with their names, newest first; A asks to UNBAN with CANCEL first;
UNBAN takes the line out of bans.txt; an empty file says "No banned players." (test screen 38).

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_banned.py
"""
import importlib.util
import time
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_banned_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

NOW = int(time.time())
BANS = (
    "2026-11-01 18:03:11\tip=203.0.113.7\thwid=00112233445566778899aabbccddeeff\tdiscord_username=none\tdiscord_id=none\t"
    "players=Goat\treason=banned by the host\n"
    "2026-11-03 20:41:02\tip=203.0.113.9\thwid=ffeeddccbbaa99887766554433221100\tdiscord_username=none\tdiscord_id=none\t"
    "players=Butcher\treason=banned by the host\n"
    f"2026-11-04 09:00:00\tip=203.0.113.11\thwid=none\tdiscord_username=none\tdiscord_id=none\tplayers=Over\treason=x\t"
    f"until={NOW - 60}\n")

CASES = (
    # (name, bans.txt, presses): the list; A opens the ask on the newest (Butcher), CANCEL first; down to UNBAN, A
    ("list", BANS, ()),
    ("ask", BANS, ("a", "wait")),
    ("unban", BANS, ("a", "wait", "down", "a", "wait", "wait")),
    ("cancel", BANS, ("a", "wait", "a", "wait", "wait")),
    ("empty", "", ()),
)


@pytest.fixture(scope="module")
def cfg():
    c = menus.config()
    if not c:
        pytest.skip("needs the ae_test harness config (data_dir with maps) and a built game ($AE_MENUS_BUILD)")
    return c


def run_cases(cfg, out, only=()):
    runs = {}
    for name, bans, presses in CASES:
        if only and name not in only:
            continue
        runs[name] = menus.play(cfg, out, name, views=38, window="1920x1080", shots=6, exit_after=menus.LEAD_IN + len(presses) + 6,
                                test_input=menus.menu_input(*presses) if presses else menus.menu_input("wait"),
                                root_files={"bans.txt": bans})
        frames = runs[name][2]
        if frames:
            (out / f"{name}.png").write_bytes(frames[-1].read_bytes())
    return runs


@pytest.fixture(scope="module")
def runs(cfg):
    return run_cases(cfg, menus.out_dir(cfg, "banned"))


def test_lists_the_live_bans_newest_first(runs):
    _, text, _ = runs["list"]
    assert "ae banned: open (2 bans)" in text


def test_unban_asks_then_takes_the_line_out(runs):
    _, text, _ = runs["ask"]
    assert "ae banned: unban asked Butcher" in text and "unbanned" not in text
    _, text, _ = runs["cancel"]
    assert "ae banned: unban asked Butcher" in text and "unbanned" not in text
    _, text, _ = runs["unban"]
    assert "ae banned: unbanned Butcher" in text and "moderation: unban of ban 1: taken out of bans.txt" in text


def test_nothing_banned_is_one_quiet_line(runs):
    _, text, _ = runs["empty"]
    assert "ae banned: open (0 bans)" in text


if __name__ == "__main__":
    import sys

    c = menus.config()
    if not c:
        sys.exit("needs the ae_test harness config and $AE_MENUS_BUILD")
    out = menus.out_dir(c, "banned")
    for case, (result, _, frames) in run_cases(c, out, sys.argv[1:]).items():
        print(case, result.get("status"), out / f"{case}.png")
