#!/usr/bin/env python3
"""Plays a TEST RUN list headless and judges its expect lines before the tester's round.

    python3 tools/ae_test/run_list.py <list.txt> --game build/linux64/halo --maps <maps folder>
        --root <run folder> [--after 8] [--timeout 900]

One game plays the whole list with DEV MODE and TEST RUN on (debug.test_run), each test passed by debug.test_run_auto
`--after` seconds after it is up (time for its setup lines to run and the game to show them). The game draws
offscreen. When the run ends, tools/ae_test/expect.py judges every expect line from the run's event stream (and, for
`log` rules, the game's own log of each test), and the
report goes to <root>/report.json and the terminal: PASS / FAIL (with the event lines it rests on) for the game's
checks, LOOK for the tester's, then each test's staging (tools/ae_test/preflight.py): the game it was really played in
(the gametype and rule lines of the game's log), each setup line with the game's answer, the seconds to its scene and
the picture the game took of the scene. A setup line still refused when its retries ended, or with no answer, is a
SETUP ERROR (a weapon the map has not, a point with no room, a player who never came). Exit 1 when a test never came
up, a setup failed or a test has no picture of its scene; exit 3 when every scene was staged and an expect line
failed.

The pictures are copied to --frames (default <root>/frames) as <list>-<test>.png.

Retail maps are local only; the run folder holds a copy of the game, the list, the game's
log (game.log) and its config folder (results, shots, the event stream under dev/). The weapon packs folder beside
the maps folder and the playlists file beside the program are linked in, as an install has them."""
import argparse
import json
import os
import random
import re
import shutil
import signal
import socket
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

sys.path.insert(0, str(Path(__file__).resolve().parent / "spots"))

import expect  # noqa: E402
import frame_lint  # noqa: E402
import preflight  # noqa: E402
import report_words  # noqa: E402
import spots  # noqa: E402 (`setup spot <name>` becomes `setup goto`)

# the shipped files of a build folder that a headless run reads beside its copy of the program (nothing that names a
# server on the internet)
SHIPPED = ("social.txt", "credits.txt", "announcers", "voices")
# the exit code of a run whose scenes were all staged and that has a failed expect line
EXIT_EXPECT = 3


def read_list(path):
    """A list's text with each `setup spot <name>` written as the `setup goto` line of that spot (the game knows only
    goto)."""
    try:
        return spots.expand_list(Path(path).read_text())
    except ValueError as error:
        raise SystemExit(f"{path}: {error}")


def list_id(text):
    for line in text.splitlines():
        words = line.split()
        if len(words) > 1 and words[0] == "list":
            return words[1]
    raise SystemExit("no list line")


EXIT_REQUIRES = 4


def requirements(text):
    """A list's `requires` lines: [(what, name)], what "mcc", "workshop" or "map"."""
    found = []
    for line in text.splitlines():
        words = line.split()
        if words[:1] == ["requires"] and len(words) > 1:
            found.append((words[1], words[2] if len(words) > 2 else ""))
    return found


def unmet(needs, maps, mcc, links):
    """The needs this machine does not meet: [words]. maps: the maps folder; mcc: --mcc's folder ("" none); links: the
    --link folders (name=path)."""
    missing = []
    mcc_path = Path(mcc).expanduser() if mcc else None
    for what, name in needs:
        if what == "mcc" and not (mcc_path and mcc_path.exists()):
            missing.append("MCC (--mcc <its folder>)")
        elif what == "workshop":
            # (a Workshop item's folder: <steamapps>/workshop/content/976730/<id>, steamapps at or above --mcc's folder)
            bases = [mcc_path, *mcc_path.parents][:6] if mcc_path else []
            if not any((base / sub / "workshop" / "content" / "976730" / name).is_dir()
                       for base in bases for sub in ("steamapps", ".")):
                missing.append(f"the Workshop item {name} (in MCC's Steam library)")
        elif what == "map":
            bare = name.split("@")[0]
            folders = [Path(maps).expanduser()] + [Path(link.partition("=")[2]).expanduser() for link in links]
            if not any((folder / f"{bare}.map").exists() or (folder / name).exists() for folder in folders):
                missing.append(f"the map {name}")
    return missing


def auto_text(args):
    """debug.test_run_auto's text: --auto's, else PASS after --after seconds (by hand when requested)."""
    if args.auto:
        return ("OWNER," if args.owner else "") + args.auto
    return f"OWNER,PASS:{args.after}" if args.owner else f"PASS:{args.after}"


def owner_problems(text, results_path, log_text):
    """A run by hand: each test of the list must have its record, a PASS (the press reached the runner) or a SKIP
    of a test that could not be set up (a SETUP ERROR, said apart); [lines] for the tests that did not."""
    ids = [line.split(None, 1)[1].strip() for line in text.splitlines()
           if line.startswith("test ") and len(line.split()) > 1]
    verdicts = {}
    try:
        for line in results_path.read_text(errors="replace").splitlines():
            try:
                record = json.loads(line)
            except ValueError:
                continue
            verdicts[record.get("test")] = record.get("verdict")
    except OSError:
        pass
    found = []
    if "verdicts pressed as in a run by hand" not in log_text:
        found.append("BY HAND the game did not take the run-by-hand path")
    for test in ids:
        if test not in verdicts:
            found.append(f"BY HAND {test}: no verdict: the pressed BACK + A never reached TEST RUN (a screen or dialog "
                         f"held the pad, the run stopped, or the test never came up)")
        elif verdicts[test] not in ("PASS", "SKIP"):
            found.append(f"BY HAND {test}: recorded {verdicts[test]}, not the pressed PASS")
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("list")
    parser.add_argument("--game", required=True)
    parser.add_argument("--maps", default=os.environ.get("AE_TEST_MAPS"))
    parser.add_argument("--root", default=os.environ.get("AE_TEST_RUN_ROOT"))
    parser.add_argument("--after", type=int, default=8)
    # (debug.test_run_auto's own text, e.g. "PASS:10,FAIL:10": in place of PASS after --after seconds)
    parser.add_argument("--auto", default="")
    # (the run by hand: verdicts pressed on a pad, the NOTE box and every screen as in a person's run;
    # each test must end in the pressed verdict)
    parser.add_argument("--owner", action="store_true")
    # (the window's size, WxH: a run by hand plays 1280x720; use 1280x960 to see a 4:3 screen's fit)
    parser.add_argument("--window", default="")
    # (the frame lint's findings fail the list: off until a full round has no false alarm)
    parser.add_argument("--frame-fails", action="store_true")
    # (more folders in the data root, e.g. --link maps_dev=<folder>)
    parser.add_argument("--link", action="append", default=[], metavar="NAME=PATH")
    # (a config.toml to play with, e.g. the tester's settings, bots and all)
    parser.add_argument("--config")
    # (the newer game's install folder, for lists on its maps: off unless given)
    parser.add_argument("--mcc")
    # (keep the run folder's save root between runs: maps AE copied to its cache stay, as on the tester's PC)
    parser.add_argument("--keep", action="store_true")
    # (the live judge beside the game, as the tester's round has it: its verdicts on the card and in the records;
    # --live-shots: a screenshot every so many seconds, --live-greet: a message at each test)
    parser.add_argument("--live", action="store_true")
    parser.add_argument("--live-shots", type=float, default=0.0)
    parser.add_argument("--live-greet", default="")
    parser.add_argument("--timeout", type=int, default=900)
    # (where the scene pictures are copied to, <list>-<test>.png each)
    parser.add_argument("--frames", default="")
    # (an address of its own on the loopback: other games on the machine hold 127.0.0.1's ports)
    parser.add_argument("--address", default=f"127.0.0.{random.randint(40, 250)}")
    # (where its LAN search goes: a list that joins a game, a server's own loopback address)
    parser.add_argument("--broadcast", default="127.255.255.255")
    args = parser.parse_args()
    if not args.maps:
        parser.error("--maps is required when AE_TEST_MAPS is unset")
    if not args.root:
        parser.error("--root is required when AE_TEST_RUN_ROOT is unset")

    text = read_list(args.list)
    identity = list_id(text)
    # (what the list needs here, before a game is started for nothing: a missing Workshop item or map otherwise shows only
    # as an expect line that never comes)
    missing = unmet(requirements(text), args.maps, args.mcc or "", args.link)
    if missing:
        print(f"REQUIRES NOT MET {identity}: missing on this machine: {'; '.join(missing)}")
        return EXIT_REQUIRES
    root = Path(args.root).expanduser() / identity
    if root.exists():
        for part in ("data", "bin") if args.keep else ("",):
            shutil.rmtree(root / part, ignore_errors=True)
    (root / "data" / "test-runs").mkdir(parents=True)
    (root / "save").mkdir(exist_ok=True)
    (root / "bin").mkdir()
    (root / "data" / "maps").symlink_to(Path(args.maps).expanduser().resolve())
    (root / "data" / "test-runs" / f"{identity}.txt").write_text(text)
    for link in args.link:
        name, _, target = link.partition("=")
        (root / "data" / name).symlink_to(Path(target).expanduser().resolve())
    # (the weapon packs beside the maps folder: a seeded gametype's game needs them, and its lobby refuses a machine
    # without them, so the test never loads)
    beside_maps = Path(args.maps).expanduser().resolve().parent
    if (beside_maps / "packs").is_dir() and not (root / "data" / "packs").exists():
        (root / "data" / "packs").symlink_to(beside_maps / "packs")
    # (and the map packs' folders there, maps_<pack>: a test on <map>@<pack> loads from one)
    for folder in sorted(beside_maps.glob("maps_*")):
        if folder.is_dir() and not (root / "data" / folder.name).exists():
            (root / "data" / folder.name).symlink_to(folder)
    # (the config folder is the binary's: a copy in the run's folder keeps the run's files there)
    shutil.copy2(Path(args.game).expanduser(), root / "bin" / "halo")  # (its time kept: a campaign start point is read only by the program that took it)
    # (the files a build ships beside its program and reads from there: without social.txt the game knows the two
    # built-in playlists only, and a test's `social start <playlist>` finds no playlist)
    for name in SHIPPED:
        shipped = Path(args.game).expanduser().resolve().parent / name
        if shipped.exists():
            (root / "bin" / name).symlink_to(shipped)
    if args.config:
        shutil.copy(Path(args.config).expanduser(), root / "bin" / "config.toml")
    env = dict(os.environ, SDL_VIDEODRIVER="offscreen", SDL_AUDIODRIVER="dummy", HALO_DATA_ROOT=str(root / "data"),
               HALO_SAVE_ROOT=str(root / "save"), HALO_HIDDEN_WINDOW="1", HALO_FULLSCREEN="0", HALO_NO_VSYNC="1",
               HALO_NO_AUDIO="1", HALO_UPDATE_AUTO="false", HALO_NET_ONLINE="false", HALO_NET_ADDRESS=args.address,
               HALO_NET_BROADCAST=args.broadcast, HALO_ARENA_MENUS="1", HALO_MCC_USE="never", HALO_DEV_MODE="1",
               HALO_TEST_RUN=identity, HALO_TEST_RUN_AUTO=auto_text(args), HALO_EXIT_AFTER=str(args.timeout))
    # (a tester's launch: the PC menus and the bots' events)
    if args.owner:
        # (16:9 at the size the saved frames use: a 4:3 window hides 16:9-only faults)
        env.update(HALO_MENUS="pc", HALO_BOT_EVENTS="1", HALO_DISPLAY_MODE="windowed", HALO_WINDOW_SIZE="1280x720")
    if args.window:
        env.update(HALO_DISPLAY_MODE="windowed", HALO_WINDOW_SIZE=args.window)
    # (a list on <map>@mcc or a workshop map, <map>@ws<id>: the machine's copy of the newer game's files, in a folder
    # laid out as its store lays it out; without it such a test never loads)
    if args.mcc:
        env.update(HALO_MCC_USE="yes", HALO_MCC_PATH=str(Path(args.mcc).expanduser()))
    agent = None
    if args.live:
        with socket.socket() as probe:
            probe.bind(("127.0.0.1", 0))
            port = probe.getsockname()[1]
        env.update(HALO_TELNET_CONSOLE="1", HALO_TELNET_CONSOLE_PORT=str(port))
    # (the log to a file: a pipe nobody reads fills, and the game stops at its next line)
    with open(root / "game.log", "w") as log:
        process = subprocess.Popen([str(root / "bin" / "halo")], cwd=root, env=env, stdout=log,
                                   stderr=subprocess.STDOUT, start_new_session=True)
        if args.live:
            agent = subprocess.Popen([sys.executable, str(Path(__file__).resolve().parent / "live.py"), args.list,
                                      "--events", str(root / "bin" / "dev"), "--port", str(port),
                                      "--greet", args.live_greet, "--shots", str(args.live_shots)],
                                     stdout=open(root / "live.log", "w"), stderr=subprocess.STDOUT)
        ended = f"test run: {identity} ended"
        launched, first_ready = time.time(), None
        deadline = time.time() + args.timeout + 30
        while time.time() < deadline and process.poll() is None:
            text = (root / "game.log").read_text(errors="replace")
            if first_ready is None and "test run setup ready" in text:
                first_ready = time.time() - launched
            if ended in text:
                time.sleep(2)
                break
            time.sleep(0.25)
        if agent:
            agent.terminate()
        if process.poll() is None:
            os.killpg(process.pid, signal.SIGTERM)
            try:
                process.wait(timeout=20)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
    streams = sorted(root.rglob("events-*.jsonl"))
    if not streams:
        print(f"no event stream under {root} (see {root / 'game.log'})")
        return 1
    results = expect.report(args.list, streams[-1], root / "game.log")
    (root / "report.json").write_text(json.dumps(results, indent=1))
    for row in results:
        evidence = f" (events {', '.join(map(str, row['evidence']))})" if row["evidence"] else ""
        print(f"{row['verdict']:7} {row['test']} #{row['check']}: {row['text']}"
              + (f"\n        {row['rule']}: {row['reason']}{evidence}" if row["rule"] else ""))
    # (each test's staging: the game it was really played in, its setup lines with the game's answers, its picture)
    events = expect.read_events(streams[-1])
    rows = preflight.staging(read_list(args.list), (root / "game.log").read_text(errors="replace"), events)
    pictures = preflight.collect_pictures(root, identity, args.frames or root / "frames")
    print()
    for line in preflight.lines(rows, pictures):
        print(line)
    setup_errors = [f"{'SETUP ERROR' if words.startswith('SETUP ERROR') else 'PREFLIGHT'} {test}: "
                    f"{words.removeprefix('SETUP ERROR ')}" for test, words in preflight.problems(rows, pictures)]
    for line in setup_errors:
        print(line)
    # (how long the tester waits: launch to the first test up; each test's load to its window opening, the reset and
    # setup lines included)
    if first_ready is not None:
        print(f"TIME launch to first test ready: {first_ready:.1f} s")
    loads = {}
    for event in expect.read_events(streams[-1]):
        if event.get("ev") == "test" and event.get("state") == "load":
            loads[event.get("test")] = event.get("t", 0)
        if event.get("ev") == "test" and event.get("state") == "setup" and event.get("test") in loads:
            print(f"TIME {event.get('test')}: load to set up {(event.get('t', 0) - loads.pop(event.get('test'))) / 1000:.1f} s")
    print(f"\nevents: {streams[-1]}\nreport: {root / 'report.json'}")
    # (1: a scene was not staged as the list asks; 3: every scene was, and an expect line failed, which in a short
    # automatic run is often a line that waits for the bots or for the tester's move)
    # (the results file is in the config folder, beside the program's copy)
    results_files = sorted(root.rglob(f"{identity}.results.jsonl"))
    owner_errors = owner_problems(read_list(args.list), results_files[-1] if results_files else root / "none",
                                  (root / "game.log").read_text(errors="replace")) if args.owner else []
    for line in owner_errors:
        print(line)
    # (a `hurt ... taken max 0` that passed with no placed AI in view at the scene's picture proves nothing: no hit
    # because nothing was there; a FAIL of the list, said as HOLLOW)
    in_view = {row["id"]: row.get("actors") for row in rows}
    hollow = [f"HOLLOW  {row['test']} #{row['check']}: {row['rule']} passed with no placed AI in view at the picture "
              f"({'none placed' if not in_view.get(row['test']) else 'placed %d, in view %d' % in_view[row['test']]})"
              for row in results if row.get("verdict") == "PASS" and row.get("rule", "").split()[:1] == ["hurt"]
              and re.search(r"\btaken max 0\b", row["rule"])
              and not (in_view.get(row["test"]) and in_view[row["test"]][1] >= 1)]
    for line in hollow:
        print(line)
    owner_errors += hollow
    # (what was measured, never a bare PASS: report_words.py; summary.json for preflight.py's record)
    list_text = read_list(args.list)
    played = [line.split(None, 1)[1].strip() for line in list_text.splitlines()
              if line.startswith("test ") and len(line.split()) > 1]
    problem_tests = [test for test, _ in preflight.problems(rows, pictures)]
    problem_tests += [line.split()[1].rstrip(":") for line in owner_errors if len(line.split()) > 1]
    retired = sum(1 for line in list_text.splitlines() if line.strip().startswith("retired "))
    counts = report_words.count(results, played, problem_tests, retired)
    # (the frame lint over each scene's boxes: reported, not failing a list yet)
    frame_findings = 0
    found_lines = [f"{test}: {words}" for test, words in preflight.problems(rows, pictures)] + list(owner_errors)
    for boxes in sorted(root.rglob(f"test-runs/preflight/{identity}-*.boxes.json")):
        test = boxes.name[len(identity) + 1:-len(".boxes.json")]
        try:
            found = frame_lint.lint_file(boxes)
        except (OSError, ValueError) as error:
            print(f"FRAME   {test}: the boxes could not be read ({error})")
            continue
        for kind, text, words in found:
            print(f"FRAME   {test}: {kind} \"{text}\" {words}")
            found_lines.append(f"{test}: FRAME {kind} \"{text}\" {words}")
        frame_findings += len(found)
    counts["frame"] = frame_findings
    if args.frame_fails and frame_findings:
        owner_errors.append(f"FRAME   {identity}: {frame_findings} frame finding(s), and --frame-fails is on")
    # (what the run found before the tester's round: the test page's "found by preflight before you")
    counts["found"] = found_lines
    counts["proxies"] = [{"test": row["test"], "check": row["check"],
                          "why": report_words.proxy_reason(row.get("rule", ""), row.get("text", ""))}
                         for row in results if report_words.proxy_reason(row.get("rule", ""), row.get("text", ""))]
    (root / "summary.json").write_text(json.dumps(counts, indent=1) + "\n")
    print(f"MEASURE {identity}: {report_words.words(counts)}")
    for row in results:
        reason = report_words.proxy_reason(row.get("rule", ""), row.get("text", ""))
        if reason:
            print(f"PROXY   {row['test']} #{row['check']}: {row['rule']} ({reason})")
    if setup_errors or owner_errors:
        return 1
    return EXIT_EXPECT if any(row["verdict"] in ("FAIL", "NOT RUN", "BAD RULE") for row in results) else 0


if __name__ == "__main__":
    sys.exit(main())
