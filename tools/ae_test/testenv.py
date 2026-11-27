#!/usr/bin/env python3
"""The gate before the tester plays a TEST RUN list: what is checked by hand before each launch, checked every time.

    python3 tools/ae_test/testenv.py check <install> <list id>... [--preflight [host:]FOLDER] [--waive-preflight WHY]
    python3 tools/ae_test/testenv.py left <install> <list id>...

<install> is the test install the tester launches (or $AE_TEST_INSTALL when omitted). A list id is a file
of the install's test-runs/ folder; the repo's copy is tools/ae_test/test_runs/<id>.txt of the tree this script is in.

`check` prints one row a check (OK, WARN or FAIL, what it found, and for a FAIL what to do), then GATE PASS or GATE
FAIL; exit 1 on a FAIL. The checks:

  build      the install's program was put there by tools/ae_test/deploy_latest.py (its record,
             halo-latest.deploy.json, names the same file) and says which commit it is: a build with no commit
             writes "build unknown" in every result, and nobody can say which build a FAIL was played on
  files      the files deployed with it (the playlists, the changelog, the weapon cache) are the ones deployed:
             a binary swapped alone left the playlists of an older build beside it
  lint       the repo's list passes tools/ae_test/test_run_lint.py (its errors fail the gate; its warnings show)
  deployed   the install's copy of the list is the repo's, as list_index.py --sync writes it (spots as gotos)
  left       the list has tests the runner will open: it opens only those that have not passed, and a list whose
             tests all passed starts the game with nothing to play
  preflight  the list was played by hand (tools/ae_test/preflight.py: the run by hand's path, the
             verdicts pressed on a pad, one game for the whole list) and passed, on the same list text and the same
             build; --preflight names the folder of the *.preflight.json records (preflight.py's --root), on another
             machine as host:folder. --waive-preflight says why one is not needed (a list with no game in it, checked
             on another machine recently...): the row says WAIVED and the reason

`left` prints, a list a line, the tests the runner will open.

    python3 tools/ae_test/testenv.py snapshot <install> --out FILE

`snapshot` writes the tester's settings from the install's config.toml (beside its program): the sections a player
sets (display, input, controls, game, menus, bots, streamer, chat), less this machine's own (the window, resolution,
frame limits, graphics quality, paths). preflight.py --config FILE plays with them (your HUD STYLE, pad layout and
game options) and records the file's digest; `check` warns when the preflight's settings are not the install's now.

    python3 tools/ae_test/testenv.py join <install> <list id>... --preflight [host:]FOLDER [--json FILE]

`join` puts the tester's verdicts beside what the automated run said of the same build: for each test of the list, the
preflight's check rows (<FOLDER>/<list>/report.json: MEASURED PASS or FAIL, LOOK) and your last PASS or FAIL on that
build (results.jsonl's "build"; your rows on other builds are counted apart, never scored). A test is predicted PASS when
every measured check passed, FAIL when one failed, and not measured when it has none. The table ends with the hit rates
(automated PASS -> your PASS, automated FAIL -> your FAIL), your FAILs that the automation passed, and the list's
retired expect lines. Run it after each round.
"""
import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE / "spots"))

import report_words  # noqa: E402
import spots  # noqa: E402
import test_run_lint  # noqa: E402

LISTS = HERE / "test_runs"
PROGRAM = "halo-latest"
RECORD = f"{PROGRAM}.deploy.json"


# ---------- files

def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def file_facts(path):
    """{size, mtime_ns, sha256} of a file: what a deploy record keeps of each file it put in place."""
    status = os.stat(path)
    return {"size": status.st_size, "mtime_ns": status.st_mtime_ns, "sha256": sha256_file(path)}


def same_file(path, facts):
    """Whether the file is the one the facts describe: its size and time first (no read), its digest when they moved."""
    try:
        status = os.stat(path)
    except OSError:
        return False
    if status.st_size != facts.get("size"):
        return False
    if status.st_mtime_ns == facts.get("mtime_ns"):
        return True
    return sha256_file(path) == facts.get("sha256")


def read_record(install):
    try:
        return json.loads((Path(install) / RECORD).read_text())
    except (OSError, ValueError):
        return None


# ---------- lists

def parse_list(text):
    """{id, revision, tests: [{id, retest}]} as the game's parser reads them (ae_test_run_list.c: revision and retest
    default to 0)."""
    info = {"id": "", "revision": 0, "tests": []}
    for raw in text.splitlines():
        words = raw.strip().split(None, 1)
        if not words or words[0].startswith("#"):
            continue
        key, value = words[0], words[1].strip() if len(words) > 1 else ""
        if key == "list":
            info["id"] = value
        elif key == "revision" and not info["tests"] and value.isdigit():
            info["revision"] = int(value)
        elif key == "test":
            info["tests"].append({"id": value, "retest": 0})
        elif key == "retest" and info["tests"] and value.isdigit():
            info["tests"][-1]["retest"] = int(value)
    return info


def tests_left(text, results_text):
    """The ids of the tests the runner opens: those whose state is not PASSED, the state being the last record of the
    test that counts (ae_test_run_results.c states_of): not a NOTE, from a revision at or after the test's retest, and a
    FAIL or SKIP only from the list's own revision or a later one (an older one's is stale: a new revision is a fix)."""
    info = parse_list(text)
    states = {test["id"]: "TODO" for test in info["tests"]}
    retest = {test["id"]: test["retest"] for test in info["tests"]}
    for line in (results_text or "").splitlines():
        try:
            record = json.loads(line)
        except ValueError:
            continue
        test, verdict = record.get("test"), record.get("verdict")
        if record.get("list") != info["id"] or verdict == "NOTE" or test not in states:
            continue
        try:
            revision = int(record.get("revision", 0))
        except (TypeError, ValueError):
            continue
        if revision >= retest[test] and (verdict == "PASS" or revision >= info["revision"]):
            states[test] = verdict
    return [test["id"] for test in info["tests"] if states[test["id"]] != "PASS"]


def deployed_text(text):
    """A repo list as list_index.py --sync writes it into an install."""
    return spots.expand_list(text)


# ---------- the tester's settings

SNAPSHOT_SECTIONS = ("display", "input", "controls", "game", "menus", "bots", "streamer", "chat")
# this machine's own: the window and the display's, frame limits, graphics quality, paths
SNAPSHOT_LEAVE = re.compile(r"^(mode|window_\w*|resolution\w*|vsync|max_fps\w*|low_latency|hdr\w*|fps_counter|"
                            r"performance\w*|anti_aliasing|shadow_resolution|mcc_\w*|target_framerate|anisotropic|"
                            r"model_detail|particle_limit|dynamic_light_limit|far_clip_distance|per_pixel_lighting|"
                            r"\w*_path|\w*_folder)$")


def snapshot(config_text):
    """The tester's settings of a config.toml's text, as a config.toml: the set lines of SNAPSHOT_SECTIONS, less
    SNAPSHOT_LEAVE's keys; no comments."""
    out, section, kept = [], "", []
    for raw in config_text.splitlines():
        line = raw.strip()
        if line.startswith("[") and line.endswith("]"):
            section = line[1:-1].strip()
            continue
        if not line or line.startswith("#") or "=" not in line or section not in SNAPSHOT_SECTIONS:
            continue
        key = line.split("=", 1)[0].strip()
        if SNAPSHOT_LEAVE.match(key):
            continue
        kept.append((section, line))
    for name in SNAPSHOT_SECTIONS:
        lines = [line for section, line in kept if section == name]
        if lines:
            out += [f"[{name}]", *lines, ""]
    return "\n".join(out)


def snapshot_digest(install):
    path = Path(install) / "config.toml"
    if not path.exists():
        return ""
    return hashlib.sha256(snapshot(path.read_text(errors="replace")).encode("utf-8")).hexdigest()


# ---------- the checks

class Rows:
    def __init__(self):
        self.rows = []

    def add(self, state, name, found, fix=""):
        self.rows.append((state, name, found, fix))

    def failed(self):
        return sum(1 for row in self.rows if row[0] == "FAIL")

    def print(self):
        for state, name, found, fix in self.rows:
            print(f"{state:8} {name:10} {found}")
            if fix and state == "FAIL":
                print(f"{'':8} {'':10} fix: {fix}")


def check_build(install, rows):
    """The program and its deploy record. Returns the record's commit ("" when unknown)."""
    program = Path(install) / PROGRAM
    record = read_record(install)
    if not program.exists():
        rows.add("FAIL", "build", f"no {PROGRAM} in {install}", "deploy one: tools/ae_test/deploy_latest.py")
        return ""
    if not record:
        rows.add("FAIL", "build", f"no {RECORD}: {PROGRAM} was not put there by deploy_latest.py, so its build is not "
                 "known", "deploy it with tools/ae_test/deploy_latest.py --go")
        return ""
    facts = record.get("files", {}).get(PROGRAM, {})
    if not same_file(program, facts):
        rows.add("FAIL", "build", f"{PROGRAM} is not the file its record names (swapped by hand after "
                 f"{record.get('time', '?')}?)", "deploy it with tools/ae_test/deploy_latest.py --go")
        return ""
    commit = record.get("commit", "")
    if not commit or commit.startswith("unknown"):
        rows.add("FAIL", "build", f"{PROGRAM} says no commit (results would say \"build unknown\")",
                 "build from a tree synced by tools/ae_test/box_sync.py (it writes BUILD_COMMIT), then deploy")
        return ""
    state = "WARN" if commit.endswith("-dirty") else "OK"
    rows.add(state, "build", f"{commit} from {record.get('source', '?')}, deployed {record.get('time', '?')}"
             + (" (built with changes not committed)" if state == "WARN" else ""))
    return commit


def check_files(install, rows):
    record = read_record(install)
    if not record:
        return
    changed = [name for name, facts in sorted(record.get("files", {}).items())
               if name != PROGRAM and not same_file(Path(install) / name, facts)]
    if changed:
        rows.add("FAIL", "files", f"changed since the deploy: {', '.join(changed)}",
                 "deploy again with tools/ae_test/deploy_latest.py --go (it backs up what it replaces)")
    else:
        rows.add("OK", "files", f"{len(record.get('files', {})) - 1} files beside it as deployed "
                 f"({', '.join(sorted(n for n in record.get('files', {}) if n != PROGRAM))})")


# read_preflight's answer when the other machine did not answer (ssh's own failure, 255): not "no record"
UNREACHABLE = {"unreachable": True}


def read_preflight(source, list_id):
    """The list's preflight record from a folder, here or on another machine (host:folder); None when there is none;
    UNREACHABLE when that machine did not answer."""
    name = f"{list_id}.preflight.json"
    if re.match(r"^[A-Za-z0-9_.-]+:", source) and not Path(source).exists():
        host, folder = source.split(":", 1)
        try:
            done = subprocess.run(["ssh", "-n", "-o", "ConnectTimeout=8", "-o", "BatchMode=yes", host,
                                   f"cat {folder.rstrip('/')}/{name}"], capture_output=True, text=True, timeout=30)
        except subprocess.TimeoutExpired:
            return UNREACHABLE
        if done.returncode == 255:
            return UNREACHABLE
        text = done.stdout if done.returncode == 0 else ""
    else:
        path = Path(source).expanduser() / name
        text = path.read_text() if path.exists() else ""
    try:
        return json.loads(text) if text else None
    except ValueError:
        return None


def check_list(install, list_id, commit, rows, preflight, waive, frame_fails=False):
    install = Path(install)
    deployed = install / "test-runs" / f"{list_id}.txt"
    repo = LISTS / f"{list_id}.txt"
    if not deployed.exists():
        rows.add("FAIL", "deployed", f"{list_id}: not in {install / 'test-runs'}",
                 f"python3 tools/ae_test/list_index.py --sync {install / 'test-runs'}")
        return
    text = deployed.read_text(encoding="utf-8", errors="replace")
    if repo.exists():
        problems = test_run_lint.lint(repo)
        warned = test_run_lint.warnings(repo)
        if problems:
            rows.add("FAIL", "lint", f"{list_id}: {len(problems)} error(s); first: line {problems[0][0]}: "
                     f"{problems[0][1]}", f"python3 tools/ae_test/test_run_lint.py {repo}")
        else:
            rows.add("WARN" if warned else "OK", "lint", f"{list_id}: no errors"
                     + (f", {len(warned)} warning(s); first: line {warned[0][0]}: {warned[0][1]}" if warned else ""))
        wanted = deployed_text(repo.read_text(encoding="utf-8"))
        if wanted != text:
            rows.add("FAIL", "deployed", f"{list_id}: the install's copy is not the repo's",
                     f"python3 tools/ae_test/list_index.py --sync {install / 'test-runs'} --force")
        else:
            rows.add("OK", "deployed", f"{list_id}: the install's copy is the repo's")
    else:
        problems = test_run_lint.lint(deployed)
        if problems:
            rows.add("FAIL", "lint", f"{list_id} (local only): line {problems[0][0]}: {problems[0][1]}",
                     f"python3 tools/ae_test/test_run_lint.py {deployed}")
        else:
            rows.add("WARN", "deployed", f"{list_id}: a local list, not in the repo (lint passed)")
    # (what the list needs on the machine that plays it: a map file is looked for in the install; MCC and Workshop items
    # are the player's own, named so the tester can check them)
    for line in text.splitlines():
        words = line.split()
        if words[:1] != ["requires"] or len(words) < 2:
            continue
        if words[1] == "map" and len(words) > 2:
            bare = words[2].split("@")[0]
            if not any((install / folder / f"{bare}.map").exists() for folder in ("maps", "custom_maps", "maps_dev")):
                rows.add("FAIL", "requires", f"{list_id}: needs the map {words[2]}, not in {install}/maps",
                         "put the map in the install's maps folder (or drop the test)")
            else:
                rows.add("OK", "requires", f"{list_id}: the map {words[2]} is there")
        else:
            rows.add("WARN", "requires", f"{list_id}: needs {' '.join(words[1:])} on this PC (not checked here)")
    results = install / "test-runs" / f"{list_id}.results.jsonl"
    left = tests_left(text, results.read_text(errors="replace") if results.exists() else "")
    total = len(parse_list(text)["tests"])
    if not left:
        rows.add("FAIL", "left", f"{list_id}: 0 of {total} tests to play (every test has passed: the game would start "
                 "with nothing on screen)", "a re-check of passed tests is a new small list, or a revision bump with "
                 "retest lines on the tests to play again")
    else:
        rows.add("OK", "left", f"{list_id}: {len(left)} of {total} tests to play ({', '.join(left[:6])}"
                 + (", ..." if len(left) > 6 else "") + ")")
    if waive:
        rows.add("WAIVED", "preflight", f"{list_id}: {waive}")
        return
    if not preflight:
        rows.add("FAIL", "preflight", f"{list_id}: no preflight named (--preflight)",
                 "run the list by hand with preflight.py; then "
                 "--preflight <host>:<folder>")
        return
    record = read_preflight(preflight, list_id)
    digest = hashlib.sha256(text.encode("utf-8")).hexdigest()
    if record is UNREACHABLE:
        # (the build machine down or away: the launch is not held up for it, the row says the check was not made)
        rows.add("WARN", "preflight", f"{list_id}: {preflight.split(':', 1)[0]} did not answer: the preflight was not "
                 "checked")
        return
    if not record:
        rows.add("FAIL", "preflight", f"{list_id}: no record in {preflight}",
                 f"python3 tools/ae_test/preflight.py --owner tools/ae_test/test_runs/{list_id}.txt --game <the build>")
    elif record.get("mode") != "owner":
        rows.add("FAIL", "preflight", f"{list_id}: the record is an automatic run's, not a run by hand",
                 "run preflight.py again with --owner")
    elif not record.get("passed"):
        rows.add("FAIL", "preflight", f"{list_id}: {record.get('result', 'not passed')} ({record.get('time', '?')})",
                 "read the preflight's output, fix the list or the build, run it again")
    elif record.get("list_sha256") != digest:
        rows.add("FAIL", "preflight", f"{list_id}: passed on another text of the list ({record.get('time', '?')})",
                 "the list changed after its preflight: run preflight.py --owner again")
    elif commit and record.get("build", "")[:8] != commit[:8]:
        rows.add("FAIL", "preflight", f"{list_id}: passed on build {record.get('build') or 'unknown'}, the install "
                 f"has {commit}", "run preflight.py --owner on the deployed build")
    else:
        mine = snapshot_digest(install)
        if record.get("config_sha256") and record.get("config_sha256") == mine:
            rows.add("OK", "settings", f"{list_id}: the preflight played with this install's settings")
        else:
            rows.add("WARN", "settings", f"{list_id}: the preflight played with "
                     + ("other settings than" if record.get("config_sha256") else "the defaults, not")
                     + " this install's (testenv.py snapshot, then preflight.py --config)")
        counts = dict(record.get("counts") or {})
        # (counted again from the run's report with today's classifier, as the test page and the join count)
        try:
            report = json.loads(read_remote(preflight, f"{list_id}/report.json") or "[]")
        except ValueError:
            report = []
        if report:
            again = report_words.count(report, [test["id"] for test in parse_list(text)["tests"]], [])
            counts.update({key: again[key] for key in ("measured_passed", "measured", "proxy", "look")})
            counts.setdefault("staged", again["staged"])
            counts.setdefault("tests", again["tests"])
        rows.add("OK", "preflight", f"{list_id}: run by hand on {record.get('build')} ({record.get('time', '?')}): "
                 + (report_words.words(counts) if counts else "staged, no counts (an older preflight)"))
        if counts.get("frame"):
            rows.add("FAIL" if frame_fails else "WARN", "frame", f"{list_id}: {counts['frame']} frame lint finding(s) in "
                     "its preflight (cut, past its panel, overlapping, low contrast)",
                     "read the preflight's FRAME lines and fix the screen, or the list's staging")


def read_remote(source, relative):
    """A file's text from a folder here or on another machine (host:folder); "" when it is not there."""
    if re.match(r"^[A-Za-z0-9_.-]+:", source) and not Path(source).exists():
        host, folder = source.split(":", 1)
        done = subprocess.run(["ssh", "-n", "-o", "ConnectTimeout=8", host, f"cat {folder.rstrip('/')}/{relative}"],
                              capture_output=True, text=True)
        return done.stdout if done.returncode == 0 else ""
    path = Path(source).expanduser() / relative
    return path.read_text(errors="replace") if path.exists() else ""


def retired_lines(text):
    """[(test id, check, why, rule)] of a list's retired lines."""
    found, test = [], ""
    for raw in text.splitlines():
        words = raw.strip().split(None, 2)
        if words[:1] == ["test"] and len(words) > 1:
            test = words[1]
        elif words[:1] == ["retired"] and len(words) > 2:
            why, _, rule = words[2].partition("|")
            found.append((test, words[1], why.strip(), rule.strip()))
    return found


def join(list_text, results_text, report, build):
    """The join of one list: {"tests": [{test, checks: [(n, verdict, rule)], predicted, tester_verdict}],
    "other_builds": n, "build": build}. predicted: PASS, FAIL or "not measured"; the tester verdict field is the last
    PASS or FAIL on build (first 8 digits), "" when none."""
    info = parse_list(list_text)
    checks = {}
    for row in report or []:
        checks.setdefault(row.get("test"), []).append((row.get("check"), row.get("verdict"), row.get("rule", "")))
    tester, other = {}, 0
    for line in (results_text or "").splitlines():
        try:
            record = json.loads(line)
        except ValueError:
            continue
        if record.get("list") != info["id"] or record.get("verdict") not in ("PASS", "FAIL"):
            continue
        if build and str(record.get("build", ""))[:8] == build[:8]:
            tester[record.get("test")] = record.get("verdict")
        else:
            other += 1
    texts = {(row.get("test"), row.get("check")): row.get("text", "") for row in report or []}
    tests = []
    for test in info["tests"]:
        rows = checks.get(test["id"], [])
        # (a proxy says the setup ran, not that the check holds: it predicts nothing; report_words.proxy_reason)
        measured = [verdict for n, verdict, rule in rows
                    if rule and not report_words.proxy_reason(rule, texts.get((test["id"], n), ""))]
        predicted = "not measured" if not measured else "PASS" if all(v == "PASS" for v in measured) else "FAIL"
        proxy = {n: bool(rule and report_words.proxy_reason(rule, texts.get((test["id"], n), ""))) for n, _, rule in rows}
        tests.append({"test": test["id"], "checks": rows, "proxy": proxy, "predicted": predicted,
                      "own" "er": tester.get(test["id"], "")})
    return {"build": build, "tests": tests, "other_builds": other}


def join_lines(list_id, joined, retired):
    lines = [f"join {list_id}: preflight build {joined['build'] or 'unknown'}; your rows on other builds: "
             f"{joined['other_builds']} (not scored)"]
    hits = {"PASS": [0, 0], "FAIL": [0, 0]}
    missed = []
    for row in joined["tests"]:
        marks = " ".join(f"#{n} {('PROXY ' if row.get('proxy', {}).get(n) else 'MEASURED ') + v if rule else 'LOOK'}"
                         for n, v, rule in row["checks"]) or "(no run)"
        verdict = row["own" "er"]
        lines.append(f"  {row['test']:24} {marks:44} predicted {row['predicted']:13} yours {verdict or '-'}")
        if verdict and row["predicted"] in hits:
            hits[row["predicted"]][1] += 1
            hits[row["predicted"]][0] += verdict == row["predicted"]
            if row["predicted"] == "PASS" and verdict == "FAIL":
                missed.append(row["test"])
    lines.append(f"  automated PASS -> your PASS: {hits['PASS'][0]}/{hits['PASS'][1]}; automated FAIL -> your FAIL: "
                 f"{hits['FAIL'][0]}/{hits['FAIL'][1]}; your FAILs the automation passed: "
                 f"{', '.join(missed) or 'none'}")
    lines.append(f"  retired expect lines: {len(retired)}"
                 + "".join(f"\n    {test} #{n}: {why} ({rule})" for test, n, why, rule in retired))
    return lines


def brief_line(rows, lists):
    """The gate in one plain line: "TEST RUN <lists> not started: <the first problem>. <what to do>" or "TEST RUN <lists>
    ready", with the warnings counted."""
    names = ", ".join(lists)
    fails = [row for row in rows.rows if row[0] == "FAIL"]
    # (the preflight's warning first: a launch without its check is the one to know of)
    warns = sorted((row for row in rows.rows if row[0] == "WARN"), key=lambda row: row[1] != "preflight")
    if fails:
        _, name, found, fix = fails[0]
        more = f" (and {len(fails) - 1} more)" if len(fails) > 1 else ""
        return f"TEST RUN {names} not started: {found}{more}. To fix: {fix}"
    return f"TEST RUN {names} ready" + (f" ({len(warns)} warning(s): {warns[0][2]})" if warns else "")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    check = sub.add_parser("check")
    check.add_argument("install", type=Path)
    check.add_argument("lists", nargs="+")
    check.add_argument("--preflight", default="")
    check.add_argument("--waive-preflight", default="", metavar="WHY")
    # (one plain line for a person, the launcher's: what is wrong and what to do, or that the list is ready)
    check.add_argument("--brief", action="store_true")
    # (a preflight with frame lint findings fails the gate: off until it is turned on)
    check.add_argument("--frame-fails", action="store_true")
    joiner = sub.add_parser("join")
    joiner.add_argument("install", type=Path)
    joiner.add_argument("lists", nargs="+")
    joiner.add_argument("--preflight", required=True)
    joiner.add_argument("--json", type=Path)
    snap = sub.add_parser("snapshot")
    snap.add_argument("install", type=Path)
    snap.add_argument("--out", type=Path, required=True)
    left = sub.add_parser("left")
    left.add_argument("install", type=Path)
    left.add_argument("lists", nargs="+")
    words = list(argv) if argv is not None else sys.argv[1:]
    install_from_env = os.environ.get("AE_TEST_INSTALL")
    if install_from_env and words and words[0] in ("check", "join", "left"):
        options_with_values = {"--preflight", "--waive-preflight", "--json"}
        positional = []
        skip = False
        for index, word in enumerate(words[1:], 1):
            if skip:
                skip = False
            elif word in options_with_values:
                skip = True
            elif not word.startswith("-"):
                positional.append((index, word))
        if positional and (len(positional) == 1 or not (Path(positional[0][1]).expanduser().exists()
                                                        or "/" in positional[0][1])):
            words.insert(1, install_from_env)
    elif install_from_env and words[:1] == ["snapshot"] and len(words) > 1 and words[1].startswith("-"):
        words.insert(1, install_from_env)
    args = parser.parse_args(words)
    install = args.install.expanduser()
    if args.command == "snapshot":
        text = snapshot((install / "config.toml").read_text(errors="replace"))
        args.out.write_text(text)
        print(f"snapshot: {len([l for l in text.splitlines() if '=' in l])} settings to {args.out}")
        return 0
    if args.command == "join":
        everything = {}
        for list_id in args.lists:
            text = (install / "test-runs" / f"{list_id}.txt")
            results = install / "test-runs" / f"{list_id}.results.jsonl"
            if not text.exists():
                print(f"{list_id}: not in the install")
                continue
            record = read_preflight(args.preflight, list_id) or {}
            try:
                report = json.loads(read_remote(args.preflight, f"{list_id}/report.json") or "[]")
            except ValueError:
                report = []
            list_text = text.read_text(encoding="utf-8", errors="replace")
            joined = join(list_text, results.read_text(errors="replace") if results.exists() else "", report,
                          record.get("build", ""))
            joined["retired"] = retired_lines(list_text)
            everything[list_id] = joined
            print("\n".join(join_lines(list_id, joined, joined["retired"])))
        if args.json:
            args.json.write_text(json.dumps(everything, indent=1) + "\n")
        return 0
    if args.command == "left":
        for list_id in args.lists:
            path = install / "test-runs" / f"{list_id}.txt"
            results = install / "test-runs" / f"{list_id}.results.jsonl"
            if not path.exists():
                print(f"{list_id}: not in the install")
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            ids = tests_left(text, results.read_text(errors="replace") if results.exists() else "")
            print(f"{list_id}: {len(ids)} of {len(parse_list(text)['tests'])}: {' '.join(ids) or '(none)'}")
        return 0
    rows = Rows()
    commit = check_build(install, rows)
    check_files(install, rows)
    for list_id in args.lists:
        check_list(install, list_id, commit, rows, args.preflight, args.waive_preflight, args.frame_fails)
    failed = rows.failed()
    if args.brief:
        print(brief_line(rows, args.lists))
    else:
        rows.print()
        print(f"GATE {'FAIL (' + str(failed) + ')' if failed else 'PASS'}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
