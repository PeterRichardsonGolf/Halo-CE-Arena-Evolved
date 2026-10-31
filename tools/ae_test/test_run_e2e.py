"""TEST RUN's runner in a real game, headless (port/linux/game/ae_test_run.h): one game plays a whole list with
verdicts fired by debug.test_run_auto, and the files it leaves are checked:

- one record per test in <id>.results.jsonl, a PNG for each FAIL and NOTE, a kept clip for a FAILed video test and none
  for a PASSed one, and the one game process the whole time;
- the game killed (SIGKILL) mid-run and started again resumes at the first test not passed, and a torn last line is
  not a record.

Box-only: it runs only when $AE_TEST_RUN_E2E_BUILD names the game's binary (build/linux64/halo) and the maps are at
$AE_TEST_RUN_E2E_MAPS (default ~/halo-data/maps); it skips on CI (CI is set) and without them, ffmpeg or xvfb aside
(the game draws offscreen). Each game takes a minute or two. The run folders go under $AE_TEST_RUN_E2E_ROOT (default
~/halo-test/testrun) and are deleted when the test passes.

    AE_TEST_RUN_E2E_BUILD=$PWD/build/linux64/halo python3 -m pytest -q tools/ae_test/test_run_e2e.py
"""
import json
import os
import shutil
import signal
import subprocess
import time
from pathlib import Path

import pytest

LIST = """list e2e
revision 1
title E2E LIST

test vpass
check The screen draws
screen 1
video

test vfail
check The screen draws
screen 1
video

test plain
check The map loads
map bloodgulch
"""
# (a run that ends on a screen test: AE's main menu has to come back and the TEST RUN screen open over it)
LIST_SCREENS = """list e2e
revision 1
title E2E LIST

test one
check The screen draws
screen 1

test two
check The screen draws
screen 1
"""
VERDICTS = "PASS:5,FAIL:5,NOTE:5,SKIP:5"


def binary():
    path = os.environ.get("AE_TEST_RUN_E2E_BUILD")
    if os.environ.get("CI") or not path or not Path(path).expanduser().is_file():
        pytest.skip("box-only: $AE_TEST_RUN_E2E_BUILD names no game binary (or this is CI)")
    return Path(path).expanduser()


def maps():
    path = Path(os.environ.get("AE_TEST_RUN_E2E_MAPS", "~/halo-data/maps")).expanduser()
    if not any(path.glob("*.map")):
        pytest.skip("no maps at " + str(path))
    return path


def make_root(tmp_root, name, text=None):
    root = tmp_root / name
    shutil.rmtree(root, ignore_errors=True)
    (root / "data" / "maps").mkdir(parents=True)
    (root / "data" / "test-runs").mkdir()
    (root / "save").mkdir()
    (root / "data" / "test-runs" / "e2e.txt").write_text(text or LIST)
    for entry in maps().iterdir():
        (root / "data" / "maps" / entry.name).symlink_to(entry)
    return root


def start(game, root, seconds, verdicts=VERDICTS, keys=None):
    env = dict(os.environ, SDL_VIDEODRIVER="offscreen", SDL_AUDIO_DRIVER="dummy", HALO_DATA_ROOT=str(root / "data"),
               HALO_SAVE_ROOT=str(root / "save"), HALO_HIDDEN_WINDOW="1", HALO_UPDATE_AUTO="false",
               HALO_NET_ONLINE="false", HALO_NET_ADDRESS=os.environ.get("AE_TEST_RUN_E2E_ADDRESS", "127.0.0.31"), HALO_ARENA_MENUS="1", HALO_MCC_USE="never",
               HALO_DEV_MODE="1", HALO_TEST_RUN="e2e", HALO_TEST_RUN_AUTO=verdicts, HALO_EXIT_AFTER=str(seconds))
    if keys:
        env["HALO_TEST_INPUT"] = keys
        env.pop("HALO_TEST_RUN_AUTO")
    # (the config folder is the binary's: a copy of it in the run's folder keeps this run's records and shots there)
    (root / "bin").mkdir(exist_ok=True)
    if not (root / "bin" / "halo").exists():
        shutil.copy(game, root / "bin" / "halo")
    log = open(root / "out.log", "a")
    return subprocess.Popen([str(root / "bin" / "halo")], cwd=root, env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)


def results(root):
    found = sorted(root.rglob("e2e.results.jsonl"))
    assert len(found) == 1, found
    return found[0]


def records(path, torn=0):
    """the lines that parse; a line that does not is an error unless it is one of the `torn` the test made itself
    (a SIGKILL can tear the last line it wrote, and the test appends one on purpose)"""
    out, bad = [], []
    for line in path.read_text(errors="replace").splitlines():
        try:
            out.append(json.loads(line))
        except ValueError:
            bad.append(line)
    assert len(bad) <= torn, f"{len(bad)} unparseable lines in {path} (at most {torn} expected): {bad}"
    return out


def wait(process, until, timeout):
    end = time.time() + timeout
    while time.time() < end and process.poll() is None and not until():
        time.sleep(0.5)


@pytest.fixture
def tmp_root():
    root = Path(os.environ.get("AE_TEST_RUN_E2E_ROOT", "~/halo-test/testrun")).expanduser()
    root.mkdir(parents=True, exist_ok=True)
    return root


def test_whole_list_in_one_game(tmp_root):
    game = binary()
    root = make_root(tmp_root, "e2e-whole")
    process = start(game, root, 400)
    try:
        wait(process, lambda: "test run: e2e ended" in (root / "out.log").read_text(errors="replace"), 380)
        assert process.poll() is None, "the game quit before the run ended"
    finally:
        os.killpg(process.pid, signal.SIGKILL)
        process.wait()
    log = (root / "out.log").read_text(errors="replace")
    done = records(results(root))
    assert [(r["test"], r["verdict"]) for r in done] == [("vpass", "PASS"), ("vfail", "FAIL"), ("plain", "SKIP")], log[-3000:]
    # (the auto NOTE asks PASS, FAIL or CONTINUE and picks CONTINUE: ONE SKIP record with the note and the shot)
    assert all(r["note"] == "auto" for r in done if r["verdict"] in ("FAIL", "SKIP"))
    assert done[2]["shot"]
    config = results(root).parent.parent
    for r in done:
        if r["verdict"] in ("FAIL", "SKIP"):
            assert (config / r["shot"]).stat().st_size > 1000, r
    clips = sorted(p.name for p in (config / "test-runs" / "clips").glob("*.mp4")) if shutil.which("ffmpeg") else None
    if clips is not None:
        assert len(clips) == 1 and clips[0].startswith("e2e-vfail-"), clips
        assert done[1]["video"].endswith(clips[0])
    for line in ("test run: e2e vpass PASS", "test run: e2e vfail FAIL", "test run: e2e plain SKIP"):
        assert line in log
    shutil.rmtree(root)  # (a failed run's folder stays for a look; the next run replaces it)


@pytest.mark.parametrize("name, verdicts, first", [("e2e-resume", VERDICTS, "PASS"), ("e2e-resume-fail", "FAIL:5", "FAIL")])
def test_kill_and_resume(tmp_root, name, verdicts, first):
    """killed after the first record, the game starts again at the first test it did not record (after a FAIL too: a
    FAIL is pending, but the crashed run's FAIL is not played first), and a torn last line is ended, not glued"""
    game = binary()
    root = make_root(tmp_root, name)
    process = start(game, root, 400, verdicts)
    try:
        wait(process, lambda: list(root.rglob("e2e.results.jsonl")) and len(records(results(root), 1)) >= 1, 300)
        assert process.poll() is None
        os.kill(process.pid, signal.SIGKILL)  # only the game this test started
        process.wait()
    finally:
        if process.poll() is None:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()
    path = results(root)
    before = records(path, 1)
    assert before and before[0]["verdict"] == first
    with open(path, "a") as file:
        file.write('{"run":"2026-10-25T00:00:00","list":"e2e","revision":1,"test":"vfail","verdict":"PA')  # a torn line
    passed = {r["test"] for r in before if r["verdict"] == "PASS"}
    recorded = {r["test"] for r in before}
    process = start(game, root, 400, verdicts)
    try:
        wait(process, lambda: "test run: e2e ended" in (root / "out.log").read_text(errors="replace"), 380)
    finally:
        os.killpg(process.pid, signal.SIGKILL)
        process.wait()
    log = (root / "out.log").read_text(errors="replace")
    assert "%d of 3 tests" % (3 - len(passed)) in log, log[-3000:]
    after = records(path, 2)  # (the torn line stays a line of its own; a SIGKILL may have torn another)
    assert after[:len(before)] == before
    assert not any(r["test"] in passed and r not in before for r in after)
    # (the first record of the new run is a test the crashed run did not record: the resume point, not a FAIL)
    new = after[len(before):]
    assert new and new[0]["test"] == next(t for t in ("vpass", "vfail", "plain") if t not in recorded), new
    shutil.rmtree(root)


def test_run_ending_on_a_screen_test_brings_the_menus_back(tmp_root):
    game = binary()
    root = make_root(tmp_root, "e2e-screen-end", LIST_SCREENS)
    process = start(game, root, 400, "PASS:5,SKIP:5")
    try:
        wait(process, lambda: "ae test run screen: open" in (root / "out.log").read_text(errors="replace"), 300)
    finally:
        os.killpg(process.pid, signal.SIGKILL)
        process.wait()
    log = (root / "out.log").read_text(errors="replace")
    assert "test run: e2e ended" in log, log[-3000:]
    assert log.index("ae test run screen: open") > log.index("test run: e2e ended"), log[-3000:]
    shutil.rmtree(root)


def test_keyboard_verdicts_on_a_screen_test(tmp_root):
    """F7 and F5 give verdicts on screen tests (a menu has the focus there), and F1 sets the test up again without
    leaving it (the real keys, pressed by debug.test_input: no auto verdicts)"""
    game = binary()
    root = make_root(tmp_root, "e2e-keys", LIST_SCREENS)
    keys = "menu:" + " wait" * 30 + " key:F1" + " wait" * 10 + " key:F7" + " wait" * 12 + " key:F5" + " wait" * 4
    process = start(game, root, 400, keys=keys)
    try:
        wait(process, lambda: list(root.rglob("e2e.results.jsonl")) and len(records(results(root), 1)) >= 2, 300)
    finally:
        os.killpg(process.pid, signal.SIGKILL)
        process.wait()
    log = (root / "out.log").read_text(errors="replace")
    done = records(results(root), 1)
    assert [(r["test"], r["verdict"]) for r in done] == [("one", "PASS"), ("two", "SKIP")], log[-3000:]
    # (F1 set "one" up again, and the screen stayed: the PASS came after, on the same test)
    assert log.count("test run setup: one (screen") >= 2, log[-3000:]
    shutil.rmtree(root)
