#!/usr/bin/env python3
"""bot_gate: play the bot scorecard scenes headless for one build and score them against a baseline (Phase 0, the gate).

    python3 tools/ae_test/bot_gate.py --game <build dir>/build/linux64/halo --out ~/ae-gate-runs/new \
        [--baseline ~/ae-gate-runs/b0] [--save] [--scenes btb,ctf,arena,drive,stuck] [--runs 2] [--after 90]

Each scene is a TEST RUN list (tools/ae_test/test_runs/bots-score-<scene>.txt) played by run_list.py on the box's data
root, <runs> times at once (each on a loopback address of its own).  The game logs of one scene are scored together
(bot_scorecard: per-game summary counters, the last value of each stretch from the map's start).  --save writes <out>/<scene>.json (a baseline);
--baseline <dir> compares every scene with <dir>/<scene>.json and prints the tables.  Exit 1 on a FAIL (a behaviour
stopped); WARN and WORSE are for the owner.  The game seeds its random numbers from the clock's second, so the runs of
a scene start START_GAP seconds apart (started in one second they played the same game); a scene whose runs still log
the same bot events is reported as one sample.  The runs are added up and only a halved or doubled rate is called out.  Headless runs belong on the box, never the laptop."""
import argparse
import hashlib
import json
import random
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import bot_scorecard  # noqa: E402

SCENES = ["btb", "ctf", "arena", "drive", "stuck", "canyon"]
# (owner 2026-11-13) the map scenes play four games a round: two games of a map's stuck spots swing too much to tell a fix
# from luck (Danger Canyon's centre pocket: 60 stuck reports in one round, 1 in another)
MAP_SCENE_RUNS = {"canyon": 4, "stuck": 4}


def scene_runs(scene):
    """How many games a scene plays when --runs does not say: four for the map scenes, two for the rest."""
    return MAP_SCENE_RUNS.get(scene, 2)


# the data root's map pack folders a scene may play from (linked into each run's data root when the --maps folder has them beside it)
PACK_FOLDERS = ["maps_smp", "maps_pe", "maps_pro", "custom_maps"]
# (seconds between a scene's runs: the game's seed changes with the clock's second)
START_GAP = 1.5


def flatten(source):
    """A list in the first format (0.7.0): no groups, no setup or expect lines; each test carries its own map, gametype
    and set lines.  The scene's tests then only play; the ride calls of the big-team scene are left out."""
    head, tests, group, current = [], [], [], None
    for line in source.splitlines():
        word = line.split(None, 1)[0] if line.strip() else ""
        if word == "group":
            group, current = [], None
        elif word == "test":
            current = [line]
            tests.append((current, list(group)))
        elif current is None:
            if word in ("map", "gametype", "set"):
                group.append(line)
            elif word in ("list", "revision", "title") or line.startswith("#"):
                head.append(line)
        elif word not in ("setup", "expect"):
            current.append(line)
            tests[-1] = (current, tests[-1][1])
    lines = list(head)
    for body, settings in tests:
        lines += [""] + body + settings
    return "\n".join(lines) + "\n"


def play(scene, game, out, runs, after, maps, timeout, extra=(), flat=False):
    """Start <runs> run_list.py processes for a scene and wait; return their game.log paths."""
    processes, logs, outputs = [], [], []
    addresses = random.sample(range(40, 251), runs)
    for number in range(1, runs + 1):
        root = out / "runs" / f"{scene}-{number}"
        address = f"127.0.0.{addresses[number - 1]}"
        if number > 1:
            time.sleep(START_GAP)
        source = (HERE / "test_runs" / f"bots-score-{scene}.txt").read_text()
        if extra:
            # (a copy of the list with more set lines in every group, same list id)
            lines = []
            for line in source.splitlines():
                lines.append(line)
                if line.startswith("set debug.bot_events"):
                    lines += [f"set {item.replace('=', ' ', 1)}" for item in extra]
            source = "\n".join(lines) + "\n"
        if flat:
            source = flatten(source)
        copy = out / f"bots-score-{scene}.txt"
        copy.write_text(source)
        command = ["nice", "-n", "10", sys.executable, str(HERE / "run_list.py"), str(copy),
                   "--game", str(game), "--maps", str(maps), "--root", str(root), "--after", str(after),
                   "--timeout", str(timeout), "--address", address]
        # (the map packs' folders beside maps: the stuck scene's Blamite Mines is blamitemines@smp, in maps_smp)
        for folder in PACK_FOLDERS:
            if (maps.parent / folder).is_dir():
                command += ["--link", f"{folder}={maps.parent / folder}"]
        outputs.append(open(out / f"{scene}-{number}.run.txt", "w"))
        processes.append(subprocess.Popen(command, stdout=outputs[-1], stderr=subprocess.STDOUT))
        logs.append(root / f"bots-score-{scene}" / "game.log")
    for process, output in zip(processes, outputs):
        process.wait()
        output.close()
    return [log for log in logs if log.exists()]


def same_games(logs):
    """Groups (lists of indexes, 2 or more) of game logs whose bot event lines are the same: one game played twice."""
    groups = {}
    for index, log in enumerate(logs):
        digest = hashlib.sha256()
        with open(log, errors="replace") as handle:
            for line in handle:
                if "bot event: " in line:
                    digest.update(line.split("bot event: ", 1)[1].encode())
        groups.setdefault(digest.hexdigest(), []).append(index)
    return [group for group in groups.values() if len(group) > 1]


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--game", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--baseline")
    parser.add_argument("--save", action="store_true")
    parser.add_argument("--scenes", default=",".join(SCENES))
    parser.add_argument("--runs", type=int, default=None,
                        help="games a scene (default 2; the map scenes' MAP_SCENE_RUNS)")
    parser.add_argument("--after", type=int, default=90)
    parser.add_argument("--maps", default="~/halo-data/maps")
    parser.add_argument("--timeout", type=int, default=900)
    parser.add_argument("--extra-set", action="append", default=[], metavar="KEY=VALUE",
                        help="more `set` lines for every scene, e.g. debug.bots_drive_classic=true")
    parser.add_argument("--flat", action="store_true", help="lists in the 0.7.0 format (no groups, setup or expect lines)")
    parser.add_argument("--score-only", action="store_true", help="do not play: score the logs already under --out")
    args = parser.parse_args()
    out = Path(args.out).expanduser()
    out.mkdir(parents=True, exist_ok=True)
    maps = Path(args.maps).expanduser()
    failed = False
    for scene in args.scenes.split(","):
        if args.score_only:
            logs = sorted((out / "runs").glob(f"{scene}-*/bots-score-{scene}/game.log"))
        else:
            runs = args.runs if args.runs else scene_runs(scene)
            logs = play(scene, Path(args.game).expanduser(), out, runs, args.after, maps, args.timeout, args.extra_set, args.flat)
        print(f"## {scene}: {len(logs)} game logs")
        if not logs:
            print("no logs: the scene did not run")
            failed = True
            continue
        for group in same_games(logs):
            print("WARNING: the same game in " + ", ".join(str(logs[index]) for index in group) +
                  " (one sample, counted " + str(len(group)) + " times)")
        cards = [bot_scorecard.score(bot_scorecard.parse(log)) for log in logs]
        card = bot_scorecard.combine(cards)
        if args.save:
            saved = {"version": 1, "source": ", ".join(map(str, logs)), **card}
            (out / f"{scene}.json").write_text(json.dumps(saved, indent=2, sort_keys=True) + "\n")
        baseline = None
        if args.baseline:
            path = Path(args.baseline).expanduser() / f"{scene}.json"
            baseline = bot_scorecard.load_baseline(path) if path.exists() else None
            if baseline is None:
                print(f"no baseline {path}")
        text, this_failed = bot_scorecard.table(card, baseline)
        print(text + "\n")
        failed |= this_failed
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
