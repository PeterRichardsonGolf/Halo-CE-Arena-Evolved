"""AE's PARTY manager (menus phase 1, spec §8) in real games, headless, through the ae_test harness, in
tools/test_ae_matchmaking.py's pattern: X opens it; at 4/4 ADD A PLAYER is refused; a held B on a guest's card removes
the guest; a second pad's START while MULTIPLAYER is open is refused with a line, no crash. HALO_AE_TEST_PARTY stages the
party's size as DEV MODE's lobby_fill does.

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_party.py
"""
import importlib.util
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_party_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

CASES = (
    ("party-open", ("pad:x", "wait"), {}),
    # (four seats, then ADD A PLAYER (after the four cards): refused, full)
    ("party-full", ("pad:x", "pad:down", "pad:down", "pad:down", "pad:down", "pad:a", "wait"), {"HALO_AE_TEST_PARTY": "4"}),
    # (owner 11-11: guest 2's card focused, Y asks, down to REMOVE, A: removed; B then closes it)
    ("party-remove", ("pad:x", "pad:down", "pad:y", "wait", "pad:down", "pad:a", "wait", "pad:b", "wait"),
     {"HALO_AE_TEST_PARTY": "2"}),
    # (owner 11-11: a guest's own B asks; a stray A keeps STAY: the seat stays)
    ("party-leave-stay", ("2:b", "wait", "2:a", "wait"), {"HALO_AE_TEST_PARTY": "2"}),
    # (MULTIPLAYER open, the second pad's START: its profile card inline in MY PARTY, A seats it (owner Q11))
    ("party-start-submenu", ("pad:a", "wait", "2:start", "2:start", "wait", "2:a", "wait"), {}),
)


def lines(text, start):
    return [line.split(start, 1)[1] for line in text.splitlines() if start in line]


def run_cases(cfg, out, only=()):
    runs = {}
    for name, presses, env in CASES:
        if only and name not in only:
            continue
        env = {"HALO_AE_TEST_GAMETYPE": "AE TEAM SLY", **env}
        result, text, pngs = menus.play(cfg, out, name, views=0, window="1920x1080", env=env, shots=8, exit_after=30,
                                        test_input=menus.menu_input(*presses))
        frame = None
        if pngs:
            frame = out / f"{name}.png"
            frame.write_bytes(pngs[-1].read_bytes())
        runs[name] = (result, text, frame)
    return runs


@pytest.fixture(scope="module")
def cfg():
    c = menus.config()
    if not c:
        pytest.skip("needs the ae_test harness config (data_dir with maps) and a built game ($AE_MENUS_BUILD)")
    return c


@pytest.fixture(scope="module")
def runs(cfg):
    return run_cases(cfg, menus.out_dir(cfg, "party"))


def test_party_opens_on_x(runs):
    assert "open (1/4)" in lines(runs["party-open"][1], "ae party: ")


def test_party_lobby_fill_4_full(runs):
    said = lines(runs["party-full"][1], "ae party: ")
    assert "filled to 4 (test guests on controllers 2-4)" in said, said
    assert "open (4/4)" in said and "full (4/4)" in said, said
    assert "add a player: waiting" not in said, said


def test_party_remove_guest_confirm(runs):
    said = lines(runs["party-remove"][1], "ae party: ")
    assert "open (2/4)" in said, said
    assert any(line.endswith("left (controller 2)") for line in said), said
    assert said[-1] == "closed", said


def test_party_start_on_submenu(runs):
    """(review focus 1) a second pad's START on MULTIPLAYER seats it inline in MY PARTY; no popover, no crash"""
    result, text, _ = runs["party-start-submenu"]
    assert "player 2 signed in (its own controller)" in lines(text, "ae main menu: "), lines(text, "ae main menu: ")
    assert "fault at" not in text
    assert not lines(text, "ae party: open")


if __name__ == "__main__":
    c = menus.config()
    if not c:
        sys.exit("needs the ae_test harness config and a built game ($AE_MENUS_BUILD)")
    done = run_cases(c, menus.out_dir(c, "party"), tuple(sys.argv[1:]))
    for n, (result, text, frame) in done.items():
        print(n, frame, (lines(text, "ae party: ") + lines(text, "ae main menu: a pad"))[-6:])


def test_party_leave_asks_first(runs):
    said = lines(runs["party-leave-stay"][1], "ae party: ")
    assert "leave? (controller 2)" in said, said
    assert "controller 2 stays" in said, said
    assert not any(line.endswith("left (controller 2)") for line in said), said
