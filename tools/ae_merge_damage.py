#!/usr/bin/env python3
"""Syntax-only compile of the tree's C units for the warnings a bad merge leaves behind: a lost `break;`
(-Wimplicit-fallthrough), a label nothing jumps to (-Wunused-label), a brace-less initializer (-Wmissing-braces).

The build's own flags silence every warning, so this reuses each unit's compile command from build.ninja
(`ninja -t compdb <rule>`), drops the output options, adds -fsyntax-only and the three warnings, and compares what
comes out with tools/ae_merge_damage_baseline.txt. A baseline line is `file | warning | text of the source line`
(no line numbers, so an unrelated edit above does not shift it); a line is listed once however many units include it.
Exit 1 on a warning the baseline lacks; --update rewrites the baseline from the current tree.

Usage: ae_merge_damage.py [--build-dir build/linux64] [--rule linux64_cc] [--jobs N] [--update]
Needs a configured tree whose generated headers exist (a build, or at least `ninja linux64` once).
"""
import argparse
import json
import os
import re
import shlex
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASELINE = ROOT / "tools" / "ae_merge_damage_baseline.txt"
WARNINGS = ("-Wimplicit-fallthrough", "-Wunused-label", "-Wmissing-braces")
WARNING_LINE = re.compile(r"^(?P<file>[^\s:][^:]*):(?P<line>\d+):\d+: warning: .*\[(?P<kind>-W[\w-]+)\]\s*$")
HEADER = ("# Warnings tools/ae_merge_damage.py accepts today: file | warning | source line. Rewrite with --update.")


def syntax_command(command):
    """The unit's compile command as a syntax-only check with the three warnings on (after -Wno-everything)."""
    words = shlex.split(command)
    kept = []
    skip = 0
    for index, word in enumerate(words):
        if skip:
            skip -= 1
        elif word in ("-MMD", "-MP", "-flto", "-flto=full", "-flto=thin"):
            continue
        elif word in ("-MF", "-o", "-MT"):
            skip = 1
        elif word == "-c":
            continue
        else:
            kept.append(word)
    return kept + ["-fsyntax-only", "-Wno-error", *WARNINGS]


def check_unit(entry):
    """(file, warning, source line text) for each wanted warning of one unit."""
    run = subprocess.run(syntax_command(entry["command"]), cwd=entry["directory"], capture_output=True, text=True)
    found = []
    for line in run.stderr.splitlines():
        match = WARNING_LINE.match(line)
        if not match or match["kind"] not in WARNINGS:
            continue
        path = Path(match["file"])
        absolute = path if path.is_absolute() else Path(entry["directory"]) / path
        try:
            shown = str(absolute.resolve().relative_to(ROOT))
        except ValueError:
            shown = str(path)
        try:
            text = absolute.read_text(errors="replace").splitlines()[int(match["line"]) - 1].strip()
        except (OSError, IndexError):
            text = "?"
        found.append((shown, match["kind"], re.sub(r"\s+", " ", text)))
    return found, run.returncode


def collect(rule, jobs):
    compdb = subprocess.run(["ninja", "-t", "compdb", rule], cwd=ROOT, capture_output=True, text=True, check=True)
    entries = json.loads(compdb.stdout)
    found = []
    failed = 0
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        for part, code in pool.map(check_unit, entries):
            found.extend(part)
            failed += code != 0
    return entries, found, failed


def line_of(item):
    return " | ".join(item)


def read_baseline():
    if not BASELINE.exists():
        return set()
    return {line.rstrip("\n") for line in BASELINE.read_text().splitlines() if line.strip() and not line.startswith("#")}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rule", default="linux64_cc")
    parser.add_argument("--jobs", type=int, default=max(1, (os.cpu_count() or 2) // 2))
    parser.add_argument("--update", action="store_true")
    args = parser.parse_args()
    entries, found, failed = collect(args.rule, args.jobs)
    if failed:
        print(f"{failed} of {len(entries)} units did not compile (build the tree first: generated headers)", file=sys.stderr)
        return 2
    now = {line_of(item) for item in found}
    if args.update:
        BASELINE.write_text(HEADER + "\n" + "\n".join(sorted(now)) + "\n")
        print(f"baseline written: {len(now)} warnings over {len(entries)} units")
        return 0
    new = now - read_baseline()
    for line in sorted(new):
        print("new warning: " + line)
    print(f"{len(entries)} units, {len(now)} warnings, {len(new)} not in the baseline")
    return 1 if new else 0


if __name__ == "__main__":
    sys.exit(main())
