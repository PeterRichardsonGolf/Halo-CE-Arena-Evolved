#!/usr/bin/env python3
"""Plays a TEST RUN list headless and judges its expect lines: the agents' run before the owner's round.

    python3 tools/ae_test/run_list.py <list.txt> --game build/linux64/halo [--maps ~/halo-data/maps]
        [--root ~/ae-work/run-list] [--after 8] [--timeout 900]

One game plays the whole list with DEV MODE and TEST RUN on (debug.test_run), each test passed by debug.test_run_auto
`--after` seconds after it is up (time for its setup lines to run and the game to show them). The game draws
offscreen. When the run ends, tools/ae_test/expect.py judges every expect line from the run's event stream, and the
report goes to <root>/report.json and the terminal: PASS / FAIL (with the event lines it rests on) for the game's
checks, LOOK for the owner's, and SETUP ERROR for a setup line the game refused (a weapon the map has not, an option
row the gametype has not). Exit 1 when any expect line fails, a test never ran or a setup line was refused.

Retail maps: local only (the box's ~/halo-data/maps); the run folder holds a copy of the game, the list, the game's
log (game.log) and its config folder (results, shots, the event stream under dev/)."""
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
    # (keep the run folder's save root between runs: the maps AE copied to its cache stay, as on the owner's PC)
    parser.add_argument("--keep", action="store_true")
    # (the live agent beside the game, as the owner's round has it: its verdicts on the card and in the records;
    # --live-shots: a screenshot every so many seconds, --live-greet: a message at each test)
    parser.add_argument("--live", action="store_true")
    parser.add_argument("--live-shots", type=float, default=0.0)
    parser.add_argument("--live-greet", default="")
    parser.add_argument("--timeout", type=int, default=900)
    # (an address of its own on the loopback: other games on the machine hold 127.0.0.1's ports)
    parser.add_argument("--address", default=f"127.0.0.{random.randint(40, 250)}")
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
    # (the config folder is the binary's: a copy in the run's folder keeps the run's files there)
    shutil.copy(Path(args.game).expanduser(), root / "bin" / "halo")
    if args.config:
        shutil.copy(Path(args.config).expanduser(), root / "bin" / "config.toml")
    env = dict(os.environ, SDL_VIDEODRIVER="offscreen", SDL_AUDIODRIVER="dummy", HALO_DATA_ROOT=str(root / "data"),
               HALO_SAVE_ROOT=str(root / "save"), HALO_HIDDEN_WINDOW="1", HALO_FULLSCREEN="0", HALO_NO_VSYNC="1",
               HALO_NO_AUDIO="1", HALO_UPDATE_AUTO="false", HALO_NET_ONLINE="false", HALO_NET_ADDRESS=args.address,
               HALO_NET_BROADCAST="127.255.255.255", HALO_ARENA_MENUS="1", HALO_MCC_USE="never", HALO_DEV_MODE="1",
               HALO_TEST_RUN=identity, HALO_TEST_RUN_AUTO=args.auto or f"PASS:{args.after}", HALO_EXIT_AFTER=str(args.timeout))
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
    results = expect.report(args.list, streams[-1])
    (root / "report.json").write_text(json.dumps(results, indent=1))
    for row in results:
        evidence = f" (events {', '.join(map(str, row['evidence']))})" if row["evidence"] else ""
        print(f"{row['verdict']:7} {row['test']} #{row['check']}: {row['text']}"
              + (f"\n        {row['rule']}: {row['reason']}{evidence}" if row["rule"] else ""))
    # (a setup line the game refused: a weapon the map has not, a GAME OPTIONS row this gametype has not; the owner
    # would have met a test not set up)
    setup_errors = []
    test = None
    for event in expect.read_events(streams[-1]):
        if event.get("ev") == "test" and event.get("state") == "start":
            test = event.get("test")
        if event.get("ev") == "cmd" and event.get("source") == "test run" and not event.get("ok") and \
                event.get("line") != "reset":
            setup_errors.append(f"SETUP ERROR {test}: {event.get('line')} -> {event.get('text')}")
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
    return 1 if setup_errors or any(row["verdict"] in ("FAIL", "NOT RUN", "BAD RULE") for row in results) else 0


if __name__ == "__main__":
    sys.exit(main())
