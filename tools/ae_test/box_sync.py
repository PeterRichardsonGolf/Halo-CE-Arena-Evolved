#!/usr/bin/env python3
"""Copy a worktree to a build machine's folder the one way every test build is made from.

    python3 tools/ae_test/box_sync.py <host>:<folder> [--tree .] [--owner TEXT] [--dry-run]

What it copies: the files git knows in the tree (tracked, and new ones not ignored), with their changes, never the
build folders, the game data or a configured build.ninja. Nothing on the other side is removed (an OWNER.txt, a runs/
folder and the build stay).

How: rsync of that file list with --checksum and no kept times. Each copied file gets the time of the copy, so ninja
rebuilds it and linux64's rewritten copy of it (build/linux64/lp64): rsync -a kept the source's older times and ninja
built the old code.

Then it writes BUILD_COMMIT in the folder: the tree's commit (8 digits), with "-dirty" when the tree has changes not
committed. A tree with no .git takes its commit from that file (tools/version.py), so the game's build says which
commit it is (TEST RUN's records, the log's "build ..."): without it every record said "build unknown".

A maker note is written in a new folder when supplied, never over one that is there.
"""
import argparse
import subprocess
import sys
from pathlib import Path


def git(tree, *words):
    return subprocess.run(["git", "-C", str(tree), *words], check=True, capture_output=True, text=True).stdout


def commit_stamp(tree):
    """The tree's commit as BUILD_COMMIT holds it: 8 digits, "-dirty" when a tracked file has changes."""
    commit = git(tree, "rev-parse", "--short=8", "HEAD").strip()
    dirty = git(tree, "status", "--porcelain", "--untracked-files=no").strip()
    return commit + ("-dirty" if dirty else "")


def file_list(tree):
    """The files to copy: tracked and new ones git does not ignore, that are there (a deleted tracked file is not)."""
    names = git(tree, "ls-files", "-z", "--cached", "--others", "--exclude-standard").split("\0")
    return sorted({name for name in names if name and (tree / name).is_file()})


def rsync_command(tree, target, list_file, dry_run):
    command = ["rsync", "-rlp", "--no-times", "--checksum", "--files-from", str(list_file), "--from0", "--itemize-changes"]
    if dry_run:
        command.append("--dry-run")
    return command + [f"{tree}/", target.rstrip("/") + "/"]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("target", help="host:folder (the folder is made when it is not there)")
    parser.add_argument("--tree", type=Path, default=Path("."))
    parser.add_argument("--owner", default="", help="OWNER.txt's text for a new folder")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)
    if ":" not in args.target:
        parser.error("the target is host:folder")
    host, folder = args.target.split(":", 1)
    tree = Path(git(args.tree, "rev-parse", "--show-toplevel").strip())
    stamp = commit_stamp(tree)
    names = file_list(tree)
    # (the list goes in the git folder, never in the tree: a worktree's .git is a file naming it)
    list_file = Path(git(tree, "rev-parse", "--git-dir").strip())
    list_file = (list_file if list_file.is_absolute() else tree / list_file) / "box-sync-files"
    list_file.write_bytes(b"\0".join(name.encode() for name in names) + b"\0")
    print(f"box sync: {tree} at {stamp}, {len(names)} files, to {args.target}{' (dry run)' if args.dry_run else ''}")
    made = subprocess.run(["ssh", "-n", host, f"test -d {folder} && echo there || (mkdir -p {folder} && echo made)"],
                          check=True, capture_output=True, text=True).stdout.strip() if not args.dry_run else "?"
    result = subprocess.run(rsync_command(tree, args.target, list_file, args.dry_run), capture_output=True, text=True)
    changed = [line for line in result.stdout.splitlines() if line[:1] in "<>c" and not line.endswith("/")]
    print(f"box sync: {len(changed)} files copied (changed or new)")
    if result.returncode:
        print(result.stderr, file=sys.stderr)
        print(f"box sync: FAILED (rsync exit {result.returncode})")
        return 1
    if args.dry_run:
        for line in changed[:40]:
            print("  " + line)
        print(f"box sync: dry run, BUILD_COMMIT would be {stamp}")
        return 0
    owner = ""
    if made == "made" and args.owner:
        owner = f" && printf '%s\\n' {sh_quote(args.owner)} > {folder}/OWNER.txt"
    subprocess.run(["ssh", "-n", host, f"printf '%s\\n' {stamp} > {folder}/BUILD_COMMIT{owner}"], check=True)
    if made == "made" and not args.owner:
        print("box sync: warning: a new folder with no maker note (the maker option names the folder's maker)")
    print(f"box sync: OK, BUILD_COMMIT {stamp}")
    return 0


def sh_quote(text):
    return "'" + text.replace("'", "'\\''") + "'"


if __name__ == "__main__":
    sys.exit(main())
