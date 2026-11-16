#!/usr/bin/env python3
"""The index of the repo's TEST RUN lists, and their copy into a test install.

Every list lives in tools/ae_test/test_runs/*.txt. INDEX.md beside them names each list, its title, its tests and the
first line of its header comment, so that the list for a feature is found before a new one is written ("is there a
vehicle list already?": grep INDEX.md). A test install (the owner's preview, a box run folder) keeps its own
test-runs/ folder; --sync copies the repo's lists into it.

usage:
  list_index.py --write            rewrite tools/ae_test/test_runs/INDEX.md
  list_index.py --check            exit 1 when INDEX.md is not what --write would write (the unit test runs it)
  list_index.py --sync DIR [--force]
                                   copy every repo list into DIR: a new list is copied; one the same is left; one that
                                   differs is left and named, unless --force (a results file is never touched)
  list_index.py --find WORD...     the lists whose title, header or check lines hold every word (any case)
"""
import argparse
import shutil
import sys
from pathlib import Path

LISTS = Path(__file__).resolve().parent / "test_runs"
INDEX = LISTS / "INDEX.md"


def read_list(path):
    """The list's id, title, header comment lines, tests (id, title) and check lines."""
    info = {"file": path.name, "id": path.stem, "title": "", "header": [], "tests": [], "checks": []}
    in_header = True
    test = None
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if line.startswith("#"):
            if in_header:
                info["header"].append(line.lstrip("#").strip())
            continue
        if line:
            in_header = False
        word, _, rest = line.partition(" ")
        if word == "list":
            info["id"] = rest.strip()
        elif word == "title":
            if test is None:
                info["title"] = rest.strip()
            else:
                test["title"] = rest.strip()
        elif word == "test":
            test = {"id": rest.strip(), "title": ""}
            info["tests"].append(test)
        elif word == "check":
            info["checks"].append(rest.strip())
    return info


def all_lists():
    return [read_list(path) for path in sorted(LISTS.glob("*.txt"))]


def render():
    out = [
        "# TEST RUN lists",
        "",
        "Written by `tools/ae_test/list_index.py --write`; do not edit by hand. Look here (or run",
        "`list_index.py --find WORD...`) for a feature's list before writing a new one, and copy the lists into a test",
        "install with `list_index.py --sync <install>/test-runs`.",
        "",
    ]
    for info in all_lists():
        title = info["title"] or "(no title)"
        out.append(f"## {info['id']}: {title}")
        out.append("")
        out.append(f"File `{info['file']}`, {len(info['tests'])} tests.")
        if info["header"]:
            out.append("")
            out.append(info["header"][0])
        if info["tests"]:
            out.append("")
            for test in info["tests"]:
                out.append(f"- `{test['id']}`" + (f": {test['title']}" if test["title"] else ""))
        out.append("")
    return "\n".join(out)


def find(words):
    words = [word.lower() for word in words]
    hits = []
    for info in all_lists():
        text = " ".join([info["id"], info["title"], *info["header"], *info["checks"],
                         *(test["title"] for test in info["tests"])]).lower()
        if all(word in text for word in words):
            hits.append(info)
    return hits


def sync(target, force=False):
    """Copy the repo's lists into target; returns (copied, same, differing)."""
    target.mkdir(parents=True, exist_ok=True)
    copied, same, differing = [], [], []
    for path in sorted(LISTS.glob("*.txt")):
        there = target / path.name
        if there.exists() and there.read_bytes() == path.read_bytes():
            same.append(path.name)
        elif there.exists() and not force:
            differing.append(path.name)
        else:
            shutil.copyfile(path, there)
            copied.append(path.name)
    return copied, same, differing


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--write", action="store_true")
    group.add_argument("--check", action="store_true")
    group.add_argument("--sync", type=Path, metavar="DIR")
    group.add_argument("--find", nargs="+", metavar="WORD")
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args(argv)
    if args.write:
        INDEX.write_text(render(), encoding="utf-8")
        print(f"wrote {INDEX}")
        return 0
    if args.check:
        if not INDEX.exists() or INDEX.read_text(encoding="utf-8") != render():
            print(f"{INDEX} is out of date: run tools/ae_test/list_index.py --write", file=sys.stderr)
            return 1
        return 0
    if args.find:
        for info in find(args.find):
            print(f"{info['file']}: {info['title']}")
        return 0
    copied, same, differing = sync(args.sync, args.force)
    for name in copied:
        print(f"copied {name}")
    print(f"{len(copied)} copied, {len(same)} already the same, {len(differing)} differ (left)")
    for name in differing:
        print(f"differs, left as it is (--force replaces it): {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
