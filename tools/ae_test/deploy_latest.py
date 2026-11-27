#!/usr/bin/env python3
"""Put a build into a test install (AE Latest), with everything that goes with it, and say what it would do first.

    python3 tools/ae_test/deploy_latest.py --from <host>:<folder> [--to <install>]
        [--label rc8t5] [--cache host:path/ae_content.aecache] [--network-files] [--lists] [--force-lists]
        [--allow-dirty] [--go]

Without --go it changes nothing: it fetches the build into a staging folder and prints the plan (each file the same,
new or replaced, and the backup it would keep). With --go, and only while no game runs from the install, it:

  - keeps each file it replaces as <name>.prev-<label>-<12 digits of the old file's SHA-256> (as the hand deploys did);
  - puts each new file in place as <name>.new, then renames it over the old (a reader never sees half a file);
  - writes halo-latest.deploy.json: the commit, where it came from, the time, and each file's size, time and SHA-256
    (tools/ae_test/testenv.py check reads it: a binary swapped by hand, or a playlist file left old, fails the gate);
  - appends the same to deploy-log.jsonl in the install;
  - with --lists, copies the source tree's test tools into <install>/testenv-tree/ (tools/ae_test and the three tables
    its lint reads, at their paths: the launcher's gate runs from there, never from a worktree that may go), then that
    copy's list_index.py --sync puts the same tree's TEST RUN lists into the install's test-runs/ (a list that differs
    there is left and named, unless --force-lists; a results file is never touched).

What it takes from the source (<host>:<tree>, a tree synced by tools/ae_test/box_sync.py and built with ninja linux64):
build/linux64/halo as halo-latest; social.txt, matchmaking.txt and credits.txt from build/linux64 (the files the game
reads beside its program: a deploy of the binary alone left the playlists of an older build); CHANGELOG.md from the
tree; the tree's BUILD_COMMIT, which must name a commit that the program holds (a tree with no BUILD_COMMIT builds
"build unknown"). brokers.txt and masters.txt (the network's addresses) only with --network-files. The weapon cache
(packs/ae_content.aecache) only with --cache: the build and its cache go together (a test build refuses a cache that is
not its lock's).
"""
import argparse
import datetime
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import testenv  # noqa: E402

STAGING = Path(os.environ.get("TMPDIR", "/tmp")) / "arena-evolved-deploy-staging"
# (installed name, path in the source tree)
BUILD_FILES = [(testenv.PROGRAM, "build/linux64/halo"), ("social.txt", "build/linux64/social.txt"),
               ("matchmaking.txt", "build/linux64/matchmaking.txt"), ("credits.txt", "build/linux64/credits.txt"),
               ("forge_markers.bin", "build/linux64/forge_markers.bin"), ("tag_props.bin", "build/linux64/tag_props.bin"),
               ("CHANGELOG.md", "CHANGELOG.md")]
NETWORK_FILES = [("brokers.txt", "build/linux64/brokers.txt"), ("masters.txt", "build/linux64/masters.txt")]
CACHE = "packs/ae_content.aecache"
# the gate's own files: tools/ae_test and the tables test_run_lint.py reads, kept at their paths in <install>/testenv-tree
TREE_FILES = ["port/linux/src/port_config.c", "port/linux/src/ae_config_players.c", "port/linux/game/ae_dev_cmd_rules.c"]
TREE_FOLDER = "testenv-tree"


def fetch(source, relative, target):
    """Copy <source>/<relative> (host:folder, or a folder here) to target; False when it is not there."""
    target.parent.mkdir(parents=True, exist_ok=True)
    if ":" in source and not Path(source).exists():
        host, folder = source.split(":", 1)
        done = subprocess.run(["scp", "-q", "-p", f"{host}:{folder.rstrip('/')}/{relative}", str(target)],
                              capture_output=True, text=True)
        return done.returncode == 0
    path = Path(source).expanduser() / relative
    if not path.exists():
        return False
    shutil.copy2(path, target)
    return True


def fetch_path(spec, target):
    """--cache's host:path or path."""
    if ":" in spec and not Path(spec).exists():
        host, path = spec.split(":", 1)
        return subprocess.run(["scp", "-q", "-p", f"{host}:{path}", str(target)], capture_output=True).returncode == 0
    if not Path(spec).expanduser().exists():
        return False
    shutil.copy2(Path(spec).expanduser(), target)
    return True


def fetch_tree(source, target):
    """The source tree's tools/ae_test and TREE_FILES into target, at their paths; False when one is not there."""
    (target / "tools" / "ae_test").mkdir(parents=True, exist_ok=True)
    if ":" in source and not Path(source).exists():
        host, folder = source.split(":", 1)
        done = subprocess.run(["rsync", "-rq", "--no-times", "--checksum", "-e", "ssh -o ConnectTimeout=8",
                               f"{host}:{folder.rstrip('/')}/tools/ae_test/", str(target / "tools" / "ae_test") + "/"],
                              capture_output=True)
        if done.returncode:
            return False
    else:
        shutil.copytree(Path(source).expanduser() / "tools" / "ae_test", target / "tools" / "ae_test", dirs_exist_ok=True)
    return all(fetch(source, name, target / name) for name in TREE_FILES)


def game_running(install):
    """The games running from the install (not a HOST A SERVER process: halo-latest --server)."""
    found = []
    for proc in Path("/proc").iterdir():
        if not proc.name.isdigit():
            continue
        try:
            exe = os.readlink(proc / "exe")
            args = (proc / "cmdline").read_bytes().split(b"\0")
        except OSError:
            continue
        if exe.startswith(str(install)) and b"--server" not in args:
            found.append(f"{proc.name} {exe}")
    return found


def plan(install, staged):
    """[(name, staged file, state)]: state "same", "new" or "replace"."""
    rows = []
    for name, path in staged:
        there = install / name
        if not there.exists():
            rows.append((name, path, "new"))
        elif testenv.sha256_file(there) == testenv.sha256_file(path):
            rows.append((name, path, "same"))
        else:
            rows.append((name, path, "replace"))
    return rows


def backup_name(install, name, label):
    old = install / name
    return old.with_name(f"{old.name}.prev-{label}-{testenv.sha256_file(old)[:12]}")


def main(argv=None):
    """Runs deploy() and removes the staging folder it made (its copies are in the install, or were only looked at)."""
    stage = []
    try:
        return deploy(argv, stage)
    finally:
        for folder in stage:
            shutil.rmtree(folder, ignore_errors=True)


def deploy(argv, made):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--from", dest="source", required=True, help="host:tree or a tree here")
    parser.add_argument("--to", type=Path, default=os.environ.get("AE_TEST_INSTALL"),
                        help="the test install (default: $AE_TEST_INSTALL)")
    parser.add_argument("--label", default="", help="the backups' and the log's name for this deploy (rc8t5)")
    parser.add_argument("--cache", default="", help="the weapon cache to put in packs/ (host:path or path)")
    parser.add_argument("--network-files", action="store_true")
    parser.add_argument("--lists", action="store_true")
    parser.add_argument("--force-lists", action="store_true")
    parser.add_argument("--allow-dirty", action="store_true", help="a build from a tree with changes not committed")
    parser.add_argument("--go", action="store_true")
    args = parser.parse_args(argv)
    if args.to is None:
        parser.error("--to is required when AE_TEST_INSTALL is unset")
    install = args.to.expanduser()
    label = args.label or datetime.datetime.now().strftime("%m%d-%H%M")
    stage = STAGING / f"{label}-{datetime.datetime.now().strftime('%H%M%S')}"
    stage.mkdir(parents=True)
    made.append(stage)
    print(f"deploy: {args.source} -> {install} ({'GO' if args.go else 'dry run: nothing in the install changes'})")
    print(f"deploy: staging in {stage}")

    if not fetch(args.source, "BUILD_COMMIT", stage / "BUILD_COMMIT"):
        print("deploy: REFUSED: the source has no BUILD_COMMIT (sync it with tools/ae_test/box_sync.py, which writes "
              "it, and build again: the build would say \"build unknown\")")
        return 1
    commit = (stage / "BUILD_COMMIT").read_text().split()[0] if (stage / "BUILD_COMMIT").read_text().split() else ""
    if not commit or commit.startswith("unknown"):
        print("deploy: REFUSED: BUILD_COMMIT names no commit")
        return 1
    if commit.endswith("-dirty") and not args.allow_dirty:
        print(f"deploy: REFUSED: {commit}: built with changes not committed (commit them, or --allow-dirty for a "
              "work-in-progress build)")
        return 1
    wanted = list(BUILD_FILES) + (list(NETWORK_FILES) if args.network_files else [])
    staged, missing = [], []
    for name, relative in wanted:
        target = stage / name
        if fetch(args.source, relative, target):
            staged.append((name, target))
        else:
            missing.append(relative)
    if testenv.PROGRAM not in [name for name, _ in staged]:
        print(f"deploy: REFUSED: no build/linux64/halo in {args.source} (ninja linux64 there first)")
        return 1
    if missing:
        print(f"deploy: REFUSED: not in the source: {', '.join(missing)} (build with plain `ninja linux64`: the "
              "binary's target alone leaves its text files old or missing)")
        return 1
    program = (stage / testenv.PROGRAM).read_bytes()
    if commit.split("-")[0][:8].encode() not in program:
        print(f"deploy: REFUSED: the program does not hold the commit {commit} (built before its BUILD_COMMIT was "
              "written? build again)")
        return 1
    if args.cache:
        target = stage / CACHE
        target.parent.mkdir(parents=True, exist_ok=True)
        if not fetch_path(args.cache, target):
            print(f"deploy: REFUSED: no cache at {args.cache}")
            return 1
        staged.append((CACHE, target))

    rows = plan(install, staged)
    for name, _, state in rows:
        backup = f" (the old one kept as {backup_name(install, name, label).name})" if state == "replace" else ""
        print(f"  {state:8} {name}{backup}")
    if not args.cache:
        print(f"  {'kept':8} {CACHE} (no --cache: the build must be one that takes this cache)")
    if args.lists:
        print(f"  {'lists':8} the repo's lists into {install / 'test-runs'}"
              + (" (those that differ replaced)" if args.force_lists else " (those that differ left and named)"))
    if not args.go:
        print(f"deploy: dry run done; commit {commit}. Again with --go to put it in place.")
        return 0

    running = game_running(install)
    if running:
        print(f"deploy: REFUSED: a game runs from the install: {'; '.join(running)}. Try again when it has ended.")
        return 1
    for name, path, state in rows:
        if state == "same":
            continue
        there = install / name
        there.parent.mkdir(parents=True, exist_ok=True)
        if state == "replace":
            shutil.copy2(there, backup_name(install, name, label))
        fresh = there.with_name(there.name + ".new")
        shutil.copy2(path, fresh)
        os.replace(fresh, there)
    record = {"commit": commit, "source": args.source, "label": label,
              "time": datetime.datetime.now().isoformat(timespec="seconds"),
              "files": {name: testenv.file_facts(install / name) for name, _, _ in rows}}
    old = testenv.read_record(install) or {}
    # (a cache not deployed this time is still the one the last deploy put there, when it is: its facts stay)
    if CACHE not in record["files"] and CACHE in old.get("files", {}) and testenv.same_file(install / CACHE,
                                                                                         old["files"][CACHE]):
        record["files"][CACHE] = old["files"][CACHE]
    (install / testenv.RECORD).write_text(json.dumps(record, indent=1) + "\n")
    with open(install / "deploy-log.jsonl", "a") as log:
        log.write(json.dumps(record) + "\n")
    print(f"deploy: OK, {testenv.PROGRAM} is {commit} ({sum(1 for r in rows if r[2] != 'same')} files changed)")
    if args.lists:
        tree = install / TREE_FOLDER
        if not fetch_tree(args.source, tree):
            print(f"deploy: the test tools could not be copied from {args.source}: lists not synced")
            return 1
        command = [sys.executable, str(tree / "tools" / "ae_test" / "list_index.py"), "--sync", str(install / "test-runs")]
        done = subprocess.run(command + (["--force"] if args.force_lists else []), capture_output=True, text=True)
        summary = [line for line in done.stdout.splitlines() if " copied, " in line] or ["?"]
        print(f"deploy: test tools in {tree}; lists: {summary[-1]}")
        for line in done.stdout.splitlines():
            if "differ" in line and " copied, " not in line:
                print(f"deploy:   {line}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
