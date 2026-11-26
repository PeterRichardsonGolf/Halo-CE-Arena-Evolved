#!/usr/bin/env python3
"""The preflight of TEST RUN lists: what each test's scene really was in a headless run, before the tester sees it.

    python3 tools/ae_test/preflight.py <list.txt>... --game build/linux64/halo --root <folder>
        [--frames <folder>] [--after 6] [--timeout 600]

Each list is played headless by tools/ae_test/run_list.py, one game at a time. While another game named `halo` runs
on the machine it waits up to --wait seconds, then gives the list up (PREFLIGHT NOT RUN); with --beside it plays the
list beside that game instead and says so (each run has its own loopback address and folders). For every test it prints what the game really
loaded and did: the map and gametype, the rule lines of the game's log (the gametype the game took, "ae seed rules:
...", "ae presets: ..."), each setup line with the game's answer and how often it was sent, the seconds from the load
to the scene, and the picture of the scene (taken by the game a second after the last setup line was accepted). A
picture of every test goes to --frames as <list>-<test>.png. The "live:" line is the rules the game itself read when the
scene was about to be set up ("test run rules: ..."); the "game:" lines are what was chosen before the game started.

A list fails (exit 1) when a setup line of any test was refused or had no answer, a test never came up, a test has no
picture, bots joined a game the list set to no bots, or an expect line failed. The last lines are one row a list:
PREFLIGHT PASS, PREFLIGHT STAGED, EXPECT FAILED (every scene is as the list asks; an expect line failed, often one that
waits for the bots or for the tester's move) or PREFLIGHT FAIL (a scene is not as the list asks).

The run-by-hand option plays each list as a person does: the NOTE box and screens, verdicts pressed on a pad as
BACK + A, and the tester's menus. Nobody moves in it, so an expect line it fails
is one the setup alone does not meet: the row is FAIL, EXPECT NOT MET BY THE SETUP ALONE. A test whose pressed verdict
never reached TEST RUN (a screen or dialog held the pad) fails the list. Each list's outcome goes to
<root>/<list>.preflight.json (the list's digest, the build, the way, the result): tools/ae_test/testenv.py check reads
it before the tester's launch and wants the run-by-hand path.

The reading of a run (staging, problems, lines) is plain functions over the game's log and its event stream:
tools/test_ae_test_run.py has their tests."""
import argparse
import datetime
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import expect  # noqa: E402
import report_words  # noqa: E402

# the lines of the game's log that say which game a test was really played in
RULE_LINES = ("ae seed rules:", "gametype '", "ae presets:", "ae social: start ")
SETUP_START = re.compile(r"test run setup(?::| kept) (?P<id>[a-z0-9-]+) \((?P<what>[^)]*)\)$")
# the rules the game really played, read by the game when the test's scene was about to be set up
LIVE_RULES = re.compile(r"test run rules: (?P<list>[a-z0-9-]+) (?P<id>[a-z0-9-]+): (?P<rules>.*)$")
BOT_JOINED = re.compile(r"bots: bot \d+ joined")
SCENE_PICTURE = re.compile(r"test run: (?P<list>[a-z0-9-]+) (?P<id>[a-z0-9-]+) scene picture ")
# (the AI the setup placed at the picture's time, and how many were ahead of the player: a build before this says none)
ACTORS_IN_VIEW = re.compile(r", actors placed (?P<placed>\d+), in view (?P<view>\d+)")
# (the floor below the player and the deepest one a few steps ahead at the picture; a build before this says none)
DROPS = re.compile(r", drop below (?P<below>-?[\d.]+), ahead (?P<ahead>-?[\d.]+)")
# a check about a fall's landing: its drop must be one that locks for long enough to judge (stock: 6 wu locks 0.4 s,
# 10 wu about a second; a 5.9 wu fall can kill with fall damage on)
FALL_CHECK = re.compile(r"\b(fall|falls|drop|drops|landing|lands|land|stun|stunned|locked)\b", re.I)
FALL_HEIGHT = 10.0


def list_sets(list_text):
    """{test id: {key: value}}: each test's `set` lines, its group's first (a group's hold for the tests after it, up to
    the next group)."""
    found, group, current = {}, {}, None
    for raw in list_text.splitlines():
        words = raw.split(None, 2)
        if not words or words[0].startswith("#"):
            continue
        if words[0] == "group":
            group, current = {}, None
        elif words[0] == "test" and len(words) > 1:
            current = found[words[1]] = dict(group)
        elif words[0] == "set" and len(words) > 2:
            (group if current is None else current)[words[1]] = words[2].strip()
    return found


def log_rules(log_text, list_id):
    """{test id: {"loaded": the setup line's words ("map beavercreek AE CTF, 1 players here", "no load"),
    "rules": [the rule lines], "live": the rules the game said it really played ("" when it said none), "bots": the bots
    that joined while the test's game loaded}}: a test that went on in the game of the test before it has that game's
    rule lines."""
    found, current, last_rules, loading = {}, None, [], False
    for raw in log_text.splitlines():
        line = raw.split("arena-evolved: ", 1)[-1].strip()
        if line.startswith(f"test run: {list_id} ended"):
            current = None
            continue
        match = SETUP_START.match(line)
        if match:
            kept = line.startswith("test run setup kept")
            current = {"loaded": match.group("what") or "", "rules": list(last_rules) if kept else [], "kept": kept,
                       "live": "", "bots": 0}
            found[match.group("id")] = current
            loading = not kept
            if not kept:
                last_rules = current["rules"]
            continue
        # (the game took the scene's picture: every setup line was accepted, whatever the event stream holds)
        match = SCENE_PICTURE.match(line)
        if match and match.group("list") == list_id and match.group("id") in found:
            found[match.group("id")]["pictured"] = True
            seen = ACTORS_IN_VIEW.search(line)
            if seen:
                found[match.group("id")]["actors"] = (int(seen.group("placed")), int(seen.group("view")))
            drops = DROPS.search(line)
            if drops:
                found[match.group("id")]["drops"] = (float(drops.group("below")), float(drops.group("ahead")))
            continue
        match = LIVE_RULES.match(line)
        if match:
            if match.group("list") == list_id and match.group("id") in found:
                found[match.group("id")]["live"] = match.group("rules")
            continue
        if current is not None and line.startswith("test run setup ready "):
            loading = False
        # (the bots of the game as it came up; a dummy made by a setup line joins later)
        if current is not None and loading and BOT_JOINED.search(line):
            current["bots"] += 1
        if current is not None and any(marker in line for marker in RULE_LINES):
            # (a kept test's own lines add to the game's; a loaded test's are the game's)
            words = re.sub(r"^\d\d\.\d\d\.\d\d \d\d:\d\d:\d\d\s+", "", line)
            if words not in current["rules"]:
                current["rules"].append(words)
    return found


def staging(list_text, log_text, events):
    """One dict a test of the list, in list order: id, loaded, rules, lines ([{line, ok, text, tries}], the list's setup
    lines the game answered, each with its last answer), error (the SETUP ERROR's words, "" for none), state ("set up",
    "setup failed", "not set up": it never came up), load_s and setup_s (seconds from the load to ready and from the load
    to the scene, None when unknown), verdict, live (the rules the game said it really played), bots (the bots that
    joined while its game loaded) and bots_set (the list's `set bots.count` for it, None when it sets none)."""
    list_id = ""
    for raw in list_text.splitlines():
        words = raw.split()
        if len(words) > 1 and words[0] == "list":
            list_id = words[1]
    rules = log_rules(log_text, list_id)
    sets = list_sets(list_text)
    # (each test's check lines)
    checks, current = {}, None
    for raw in list_text.splitlines():
        words = raw.strip().split(None, 1)
        if words[:1] == ["test"] and len(words) > 1:
            current = checks.setdefault(words[1].strip(), [])
        elif words[:1] == ["check"] and current is not None and len(words) > 1:
            current.append(words[1])
    tests = {}
    order = [test["id"] for test in expect.parse_list(list_text)]
    current = None
    for event in events:
        kind, state, name = event.get("ev"), event.get("state"), event.get("test")
        if kind == "test" and event.get("list") == list_id:
            if state == "load":
                # (a test loaded again, RELOAD or a resumed run: its last showing is the one reported)
                current = tests[name] = {"id": name, "lines": [], "error": "", "state": "not set up", "load_t": event.get("t"),
                                         "load_s": None, "setup_s": None, "verdict": ""}
            elif name in tests:
                row = tests[name]
                if state == "ready" and row["load_t"] is not None:
                    row["load_s"] = round((event.get("t", 0) - row["load_t"]) / 1000, 1)
                elif state == "setup":
                    row["state"] = "set up"
                    if row["load_t"] is not None:
                        row["setup_s"] = round((event.get("t", 0) - row["load_t"]) / 1000, 1)
                elif state == "setup_failed":
                    row["state"] = "setup failed"
                    row["error"] = event.get("error", "") or "the setup failed"
                elif state == "verdict":
                    row["verdict"] = event.get("verdict", "")
                    if current is row:
                        current = None
        elif kind == "cmd" and event.get("source") == "test run" and current is not None and event.get("line") != "reset":
            lines = current["lines"]
            if lines and lines[-1]["line"] == event.get("line"):
                lines[-1].update(ok=bool(event.get("ok")), text=event.get("text", ""), tries=lines[-1]["tries"] + 1)
            else:
                lines.append({"line": event.get("line", ""), "ok": bool(event.get("ok")), "text": event.get("text", ""),
                              "tries": 1})
    out = []
    for name in order:
        row = tests.get(name, {"id": name, "lines": [], "error": "", "state": "not set up", "load_s": None,
                               "setup_s": None, "verdict": ""})
        row.pop("load_t", None)
        row["loaded"] = rules.get(name, {}).get("loaded", "")
        row["rules"] = rules.get(name, {}).get("rules", [])
        # (a stream that stops before the test's last events, as on the LAN GAMES screen: the log's word for it)
        if row["state"] == "not set up" and rules.get(name, {}).get("pictured"):
            row["state"] = "set up"
        row["live"] = rules.get(name, {}).get("live", "")
        row["actors"] = rules.get(name, {}).get("actors")
        row["drops"] = rules.get(name, {}).get("drops")
        row["fall_check"] = any(FALL_CHECK.search(text) for text in checks.get(name, []))
        row["bots"] = rules.get(name, {}).get("bots", 0)
        wanted = sets.get(name, {}).get("bots.count")
        row["bots_set"] = int(wanted) if wanted is not None and wanted.isdigit() else None
        out.append(row)
    return out


def problems(rows, pictures=None):
    """What fails a list: [(test id, words)]. pictures: the test ids that have a scene picture (None: not checked)."""
    found = []
    for row in rows:
        if row["state"] == "setup failed":
            found.append((row["id"], f"SETUP ERROR {row['error']}"))
        elif row["state"] != "set up":
            found.append((row["id"], "the test never came up (no load, a list error, or the run ended before it)"))
        # (a line whose last answer is a refusal, from a game that goes on after one)
        for line in row["lines"]:
            if not line["ok"] and row["state"] != "setup failed":
                found.append((row["id"], f"SETUP ERROR {line['line']}: {line['text']}"))
        # (the list asked for a game without bots and bots joined it: another screen's settings won)
        if row.get("bots_set") == 0 and row.get("bots", 0) > 0:
            found.append((row["id"], f"{row['bots']} bots joined the game, the list set bots.count 0"))
        # (AI placed for the scene and none ahead of the player at its picture: the tester opens on an empty scene)
        if row.get("actors") and row["actors"][0] > 0 and row["actors"][1] == 0:
            found.append((row["id"], f"none of the {row['actors'][0]} placed AI is ahead of the player at the picture "
                                     f"(behind, too far, or not drawn): stage them in view"))
        # (a fall to judge, staged lower than a landing lock can be seen: "no fall damage at all at this height")
        if row.get("fall_check") and row.get("drops") and 0 <= max(row["drops"]) < FALL_HEIGHT:
            found.append((row["id"], f"a fall check, and the deepest drop here is {max(row['drops']):.1f} wu (below "
                                     f"{row['drops'][0]:.1f}, ahead {row['drops'][1]:.1f}): a landing to judge needs "
                                     f"{FALL_HEIGHT:.0f}+ wu"))
        if pictures is not None and row["state"] == "set up" and row["id"] not in pictures:
            found.append((row["id"], "no picture of the scene"))
    return found


def lines(rows, pictures=None):
    """The report's text lines for a list's rows."""
    out = []
    for row in rows:
        out.append(f"TEST {row['id']}: {row['state'].upper()}" + (f" ({row['loaded']})" if row["loaded"] else ""))
        for rule in row["rules"]:
            out.append(f"    game: {rule}")
        # (the rules as the game had them at the scene; the lines above are what was chosen before the game started)
        out.append(f"    live: {row['live']}" if row.get("live") else "    live: NOT SAID (a build without the line, or no map test)")
        if row.get("bots"):
            out.append(f"    bots: {row['bots']} joined as the game came up"
                       + (f" (the list set bots.count {row['bots_set']})" if row.get("bots_set") is not None else ""))
        for number, line in enumerate(row["lines"], 1):
            tries = f" (sent {line['tries']} times)" if line["tries"] > 1 else ""
            out.append(f"    setup {number}: {line['line']} -> {'ok' if line['ok'] else 'REFUSED'} {line['text']}{tries}")
        if row["error"]:
            out.append(f"    SETUP ERROR: {row['error']}")
        times = []
        if row["load_s"] is not None:
            times.append(f"load to ready {row['load_s']:.1f} s")
        if row["setup_s"] is not None:
            times.append(f"load to scene {row['setup_s']:.1f} s")
        if times:
            out.append("    time: " + ", ".join(times))
        if pictures is not None:
            out.append(f"    picture: {pictures[row['id']]}" if row["id"] in pictures else "    picture: NONE")
    return out


def collect_pictures(run_root, list_id, frames):
    """Copies the run's scene pictures (the game's config folder, test-runs/preflight/<list>-<test>.png) into frames:
    {test id: path}."""
    found = {}
    frames = Path(frames)
    frames.mkdir(parents=True, exist_ok=True)
    for picture in sorted(Path(run_root).rglob(f"test-runs/preflight/{list_id}-*.png")):
        target = frames / picture.name
        shutil.copyfile(picture, target)
        found[picture.stem[len(list_id) + 1:]] = str(target)
    return found


def other_game():
    """The other games running on this machine (`pgrep -a -x halo`), as text lines."""
    try:
        done = subprocess.run(["pgrep", "-a", "-x", "halo"], capture_output=True, text=True, timeout=10)
    except (OSError, subprocess.SubprocessError):
        return []
    return [line for line in done.stdout.splitlines() if line.strip()]


def summary_word(code, owner=False):
    """A list's row of the summary from run_list.py's exit code: every scene staged and every expect line passed; staged
    with a failed expect line (often one that waits for the bots or the tester: read it); or not staged. The run-by-hand
    way presses PASS and never moves: an expect line it fails is one the setup alone does not meet, a FAIL."""
    if owner and code == 3:
        return "FAIL, EXPECT NOT MET BY THE SETUP ALONE"
    if code == 4:
        return "NOT RUN, REQUIRES NOT MET"
    return {0: "PASS", 3: "STAGED, EXPECT FAILED"}.get(code, "FAIL")


def list_digest(text):
    """The SHA-256 of a list as the game reads it (its `setup spot` lines written as their gotos, as list_index.py
    --sync copies it into an install): the gate (testenv.py) compares an install's copy with it."""
    sys.path.insert(0, str(Path(__file__).resolve().parent / "spots"))
    import spots  # noqa: E402
    return hashlib.sha256(spots.expand_list(text).encode("utf-8")).hexdigest()


def game_build(log_text):
    """The build the game said it is ("test run: <list> started, ..., build <commit>[, program <12 digits>]"), "" when it
    said none."""
    found = re.findall(r"test run: [a-z0-9-]+ started.*, build ([^\s,]+)", log_text)
    return found[-1] if found else ""


def game_program(log_text):
    """The first 12 digits of the program's SHA-256 the game said (", program <12 digits>"), "" when it said none."""
    found = re.findall(r"test run: [a-z0-9-]+ started.*, program ([0-9a-f]{12})", log_text)
    return found[-1] if found else ""


def write_record(root, list_path, word, owner, beside, config=""):
    """<root>/<list>.preflight.json: what was played (the list's digest, the build, the way) and how it went, for the
    gate before the tester's launch (tools/ae_test/testenv.py check --preflight)."""
    identity = Path(list_path).stem
    text = Path(list_path).read_text(encoding="utf-8")
    log = Path(root).expanduser() / identity / "game.log"
    try:
        counts = json.loads((Path(root).expanduser() / identity / "summary.json").read_text())
    except (OSError, ValueError):
        counts = {}
    record = {"list": identity, "list_sha256": list_digest(text), "mode": "owner" if owner else "automatic",
              "counts": counts,
              # (the settings it played with: testenv.py snapshot's file, compared with the install's by the gate)
              "config_sha256": hashlib.sha256(Path(config).expanduser().read_bytes()).hexdigest() if config else "",
              "result": word, "passed": word == "PASS", "beside_another_game": bool(beside),
              "build": game_build(log.read_text(errors="replace")) if log.exists() else "",
              "program": game_program(log.read_text(errors="replace")) if log.exists() else "",
              "time": datetime.datetime.now().isoformat(timespec="seconds")}
    target = Path(root).expanduser() / f"{identity}.preflight.json"
    target.write_text(json.dumps(record, indent=1) + "\n")
    return target


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("lists", nargs="+")
    parser.add_argument("--game", required=True)
    parser.add_argument("--maps", default=os.environ.get("AE_TEST_MAPS"))
    parser.add_argument("--root", default=os.environ.get("AE_TEST_PREFLIGHT_ROOT"))
    parser.add_argument("--frames", default="")
    parser.add_argument("--after", type=int, default=6)
    parser.add_argument("--timeout", type=int, default=600)
    # (seconds to wait for another game on the machine to end before a list is given up)
    parser.add_argument("--wait", type=int, default=600)
    # (after the wait: play beside the other game, on a machine other tests use too)
    parser.add_argument("--beside", action="store_true")
    # (passed on to run_list.py: more folders in the data root, a config.toml to play with, the newer game's install
    # folder for lists on its maps)
    parser.add_argument("--link", action="append", default=[], metavar="NAME=PATH")
    parser.add_argument("--config")
    parser.add_argument("--mcc")
    # (the run by hand: the gate wants this one before a tester's launch)
    parser.add_argument("--owner", action="store_true")
    # (run_list.py's --window: WxH, e.g. 1280x960 for a 4:3 screen's fit)
    parser.add_argument("--window", default="")
    # (run_list.py's --frame-fails: the frame lint's findings fail a list)
    parser.add_argument("--frame-fails", action="store_true")
    args = parser.parse_args(argv)
    if not args.maps:
        parser.error("--maps is required when AE_TEST_MAPS is unset")
    if not args.root:
        parser.error("--root is required when AE_TEST_PREFLIGHT_ROOT is unset")
    by_hand = getattr(args, "own" "er")
    frames = args.frames or str(Path(args.root).expanduser() / "frames")
    summary, failed = [], False
    for name in args.lists:
        waited = time.time()
        while other_game() and time.time() - waited < args.wait:
            time.sleep(5)
        beside = other_game()
        if beside and not args.beside:
            summary.append(f"PREFLIGHT NOT RUN {Path(name).stem}: another game runs on this machine")
            failed = True
            continue
        command = [sys.executable, str(Path(__file__).resolve().parent / "run_list.py"), name, "--game", args.game,
                   "--maps", args.maps, "--root", args.root, "--after", str(args.after), "--timeout", str(args.timeout),
                   "--frames", frames]
        for link in args.link:
            command += ["--link", link]
        if args.config:
            command += ["--config", args.config]
        if args.mcc:
            command += ["--mcc", args.mcc]
        if args.owner:
            command.append("--owner")
        if args.window:
            command += ["--window", args.window]
        if args.frame_fails:
            command.append("--frame-fails")
        print(f"===== {Path(name).stem}{' (run by hand)' if by_hand else ''}", flush=True)
        if beside:
            print(f"(played beside {len(beside)} other game(s) on this machine)", flush=True)
        done = subprocess.run(command, timeout=args.timeout + 120)
        word = summary_word(done.returncode, args.owner)
        record = json.loads(write_record(args.root, name, word, args.owner, beside, args.config).read_text())
        # (never a bare PASS: what was staged, measured and left to the tester's eyes; a FAIL says so first)
        measured = report_words.words(record["counts"]) if record.get("counts") else "no summary"
        summary.append(f"PREFLIGHT {'' if word == 'PASS' else word + ' '}{Path(name).stem}"
                       + (" (run by hand)" if by_hand else "") + (" (beside another game)" if beside else "")
                       + f": {measured}")
        failed = failed or done.returncode != 0
    print("\n".join(["", *summary]))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
