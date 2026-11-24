#!/usr/bin/env python3
"""Plays a TEST RUN list headless and judges its expect lines: the agents' run before the owner's round.

    python3 tools/ae_test/run_list.py <list.txt> --game build/linux64/halo [--maps ~/halo-data/maps]
        [--root ~/ae-work/run-list] [--after 8] [--timeout 900]

One game plays the whole list with DEV MODE and TEST RUN on (debug.test_run), each test passed by debug.test_run_auto
`--after` seconds after it is up (time for its setup lines to run and the game to show them). The game draws
offscreen. When the run ends, tools/ae_test/expect.py judges every expect line from the run's event stream (and, for
`log` rules, the game's own log of each test), and the
report goes to <root>/report.json and the terminal: PASS / FAIL (with the event lines it rests on) for the game's
checks, LOOK for the owner's, then each test's staging (tools/ae_test/preflight.py): the game it was really played in
(the gametype and rule lines of the game's log), each setup line with the game's answer, the seconds to its scene and
the picture the game took of the scene. A setup line still refused when its retries ended, or with no answer, is a
SETUP ERROR (a weapon the map has not, a point with no room, a player who never came). Exit 1 when a test never came
up, a setup failed or a test has no picture of its scene; exit 3 when every scene was staged and an expect line
failed.

The pictures are copied to --frames (default <root>/frames) as <list>-<test>.png.

Retail maps: local only (the box's ~/halo-data/maps); the run folder holds a copy of the game, the list, the game's
log (game.log) and its config folder (results, shots, the event stream under dev/). The weapon packs folder beside
the maps folder and the playlists file beside the program are linked in, as an install has them."""
import argparse
import json
import os
import random
import shutil
import signal
import socket
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import expect  # noqa: E402
import preflight  # noqa: E402

# the shipped files of a build folder that a headless run reads beside its copy of the program (nothing that names a
# server on the internet)
SHIPPED = ("social.txt", "credits.txt", "announcers", "voices")
# the exit code of a run whose scenes were all staged and that has a failed expect line
EXIT_EXPECT = 3


def list_id(text):
    for line in text.splitlines():
        words = line.split()
        if len(words) > 1 and words[0] == "list":
            return words[1]
    raise SystemExit("no list line")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("list")
    parser.add_argument("--game", required=True)
    parser.add_argument("--maps", default="~/halo-data/maps")
    parser.add_argument("--root", default="~/ae-work/run-list")
    parser.add_argument("--after", type=int, default=8)
    # (debug.test_run_auto's own text, e.g. "PASS:10,FAIL:10": in place of PASS after --after seconds)
    parser.add_argument("--auto", default="")
    # (more folders in the data root, e.g. --link maps_dev=~/devbox-test/data/maps_dev for the Test Box)
    parser.add_argument("--link", action="append", default=[], metavar="NAME=PATH")
    # (a config.toml to play with, e.g. the preview's own: the owner's settings, bots and all)
    parser.add_argument("--config")
    # (the newer game's install folder, for lists on its maps: off unless given)
    parser.add_argument("--mcc")
    # (keep the run folder's save root between runs: the maps AE copied to its cache stay, as on the owner's PC)
    parser.add_argument("--keep", action="store_true")
    # (the live agent beside the game, as the owner's round has it: its verdicts on the card and in the records;
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

    text = Path(args.list).read_text()
    identity = list_id(text)
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
    shutil.copy(Path(args.game).expanduser(), root / "bin" / "halo")
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
               HALO_TEST_RUN=identity, HALO_TEST_RUN_AUTO=args.auto or f"PASS:{args.after}", HALO_EXIT_AFTER=str(args.timeout))
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
    rows = preflight.staging(Path(args.list).read_text(), (root / "game.log").read_text(errors="replace"), events)
    pictures = preflight.collect_pictures(root, identity, args.frames or root / "frames")
    print()
    for line in preflight.lines(rows, pictures):
        print(line)
    setup_errors = [f"{'SETUP ERROR' if words.startswith('SETUP ERROR') else 'PREFLIGHT'} {test}: "
                    f"{words.removeprefix('SETUP ERROR ')}" for test, words in preflight.problems(rows, pictures)]
    for line in setup_errors:
        print(line)
    # (how long the owner waits: launch to the first test up; each test's load to its window opening, the reset and
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
    # automatic run is often a line that waits for the bots or for the owner's own move)
    if setup_errors:
        return 1
    return EXIT_EXPECT if any(row["verdict"] in ("FAIL", "NOT RUN", "BAD RULE") for row in results) else 0


if __name__ == "__main__":
    sys.exit(main())
