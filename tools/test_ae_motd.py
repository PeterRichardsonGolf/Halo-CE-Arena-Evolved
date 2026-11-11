"""AE's MESSAGE OF THE DAY and WHAT'S NEW (menus phase 1, spec §7) in real games, headless, through the ae_test harness,
in tools/test_ae_matchmaking.py's pattern. A fresh save root is a first launch: with no master answering
(HALO_AE_MOTD_OFFLINE, as DEV MODE's motd_offline) and no saved message, the installed release's notes open first; B
records it, so the next launch the same day goes to the main menu; the main menu's rows open both by hand; MOTD's Y opens
WHAT'S NEW, whose badge clears once the release is opened.

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_motd.py
"""
import importlib.util
import shutil
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_motd_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

ON = {"HALO_AE_TEST_MOTD_SKIP": "0", "HALO_AE_MOTD_OFFLINE": "1"}
# name, the presses, the case's own environment, whether its save root says everything was seen today
CASES = (
    ("motd-first", ("wait", "wait", "wait", "pad:b", "wait"), ON, False),
    ("motd-again", ("wait", "wait", "wait", "wait"), ON, True),
    ("motd-by-hand", ("pad:down", "pad:down", "pad:down", "pad:a", "wait", "pad:b"), {}, False),
    ("whats-new-from-y", ("wait", "wait", "wait", "pad:y", "wait", "pad:down", "wait", "pad:b"), ON, False),
    ("whats-new-row", ("pad:down", "pad:down", "pad:down", "pad:down", "pad:a", "wait", "pad:b", "wait"), {}, False),
)


def lines(text, start):
    return [line.split(start, 1)[1] for line in text.splitlines() if start in line]


def seen_today_seed(cfg, name):
    """a save root whose MESSAGE OF THE DAY state says: this version (and a newer one) seen today, nothing to show"""
    import time
    root = menus.harness.expand(cfg["work_dir"]) / "seeds" / f"motd-{name}"
    shutil.rmtree(root, ignore_errors=True)
    (root / "private").mkdir(parents=True)
    day = time.strftime("%Y-%m-%d", time.localtime())
    (root / "private" / "ae_motd.txt").write_text(
        f"last_version 99.0.0\nclosed_rev -1\nshown_day {day}\nwhats_new_opened \n")
    return root


def run_cases(cfg, out, only=()):
    menus.load_harness()
    runs = {}
    for name, presses, env, seeded in CASES:
        if only and name not in only:
            continue
        env = {"HALO_AE_TEST_GAMETYPE": "AE TEAM SLY", "HALO_NET_ONLINE": "false", **env}
        seed = seen_today_seed(cfg, name) if seeded else None
        result, text, pngs = menus.play(cfg, out, name, views=0, window="1920x1080", env=env, shots=8, exit_after=30,
                                        test_input=menus.menu_input(*presses), save_from=seed)
        frame = None
        if pngs:
            frame = out / f"{name}.png"
            frame.write_bytes(pngs[-1].read_bytes())
        runs[name] = (result, text, frame, seed)
    return runs


@pytest.fixture(scope="module")
def cfg():
    c = menus.config()
    if not c:
        pytest.skip("needs the ae_test harness config (data_dir with maps) and a built game ($AE_MENUS_BUILD)")
    return c


@pytest.fixture(scope="module")
def runs(cfg):
    return run_cases(cfg, menus.out_dir(cfg, "motd"))


def test_motd_first_launch_release_notes(runs):
    said = lines(runs["motd-first"][1], "ae motd: ")
    assert "open (update, notes)" in said, said
    assert "closed (close)" in said, said


def test_motd_launch_under_4s(runs):
    """offline: the MOTD (or the main menu) is up at once, never after a 4 s wait"""
    said = lines(runs["motd-first"][1], "ae motd: ")
    assert "offline" in said and "no answer in 4 s" not in said, said


def test_motd_close_records(runs):
    """everything seen today (as B left it): the next launch goes to the main menu"""
    said = lines(runs["motd-again"][1], "ae motd: ")
    assert not any(line.startswith("open (") for line in said), said


def test_motd_row_by_hand(runs):
    said = lines(runs["motd-by-hand"][1], "ae motd: ")
    assert any(line.startswith("open (by hand, ") for line in said), said


def test_whats_new_from_motd_y(runs):
    said = lines(runs["whats-new-from-y"][1], "ae ")
    assert "motd: closed (whats new)" in said, said
    assert any(line.startswith("whats new: open (") for line in said), said
    assert len([line for line in said if line.startswith("whats new: release ")]) >= 2, said


def test_whats_new_row_opens(runs):
    # (the badge's own line follows the close: test_whats_new_badge_clears)
    said = [line for line in lines(runs["whats-new-row"][1], "ae whats new: ") if not line.startswith("badge ")]
    assert any(line.startswith("open (") for line in said) and said[-1] == "closed (close)", said


def test_whats_new_badge_clears(runs):
    """the row's badge: 1 NEW at the first launch (the installed release alone), none once its notes were opened"""
    text = runs["whats-new-row"][1]
    assert "whats new badge 1 NEW" in lines(text, "ae main menu: "), lines(text, "ae main menu: ")
    assert lines(text, "ae whats new: badge ")[-1:] == ["none"], lines(text, "ae whats new: ")


if __name__ == "__main__":
    c = menus.config()
    if not c:
        sys.exit("needs the ae_test harness config and a built game ($AE_MENUS_BUILD)")
    done = run_cases(c, menus.out_dir(c, "motd"), tuple(sys.argv[1:]))
    for n, (result, text, frame, kept) in done.items():
        print(n, frame, (lines(text, "ae motd: ") + lines(text, "ae whats new: "))[-6:])
