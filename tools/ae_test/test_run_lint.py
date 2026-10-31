#!/usr/bin/env python3
"""Lints TEST RUN lists (tools/ae_test/test_runs/*.txt): the rules of the game's parser
(port/linux/game/ae_test_run_list.c) plus the ones only a tool can check before shipping: a `set` key must be a
config key (the table of port/linux/src/port_config.c), the list id is the file name, a check is at most 60
characters, a test id appears once.

usage: test_run_lint.py <list files...>
Prints `file:line: message` for each problem; exit 1 if there is any."""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
CONFIG_TABLE = ROOT / "port" / "linux" / "src" / "port_config.c"
PLAYER_TABLE = ROOT / "port" / "linux" / "src" / "ae_config_players.c"
ID = re.compile(r"[a-z0-9-]+")
MAX_TESTS, MAX_CHECKS, MAX_SETS, CHECK_LENGTH, MAX_NUMBER = 64, 3, 16, 60, 32767


def number(value, low=0, high=MAX_NUMBER):
    """The plain-digit number in value (ASCII digits only, leading zeros fine, as the parser reads it) or None."""
    if not (value.isascii() and value.isdigit()):
        return None
    n = int(value)
    return n if low <= n <= high else None


def config_keys():
    """Every key of the port's config table ({ "section.key", ...)."""
    keys = set(re.findall(r'^\s*\{ "([^"]+)",', CONFIG_TABLE.read_text(errors="replace"), re.M))
    if not keys:
        raise SystemExit(f"cannot read config keys from {CONFIG_TABLE}")
    return keys


def player_keys():
    """The per-player settings of ae_config_players.c's names[] (the runtime refuses a `set` of one: each player has their own)."""
    text = PLAYER_TABLE.read_text(errors="replace")
    block = re.search(r"names\[\]\s*=\s*\{(.*?)\n\};", text, re.S)
    keys = set(re.findall(r'"([^"]+)"', re.sub(r"/\*.*?\*/", "", block.group(1), flags=re.S))) if block else set()
    if not keys:
        raise SystemExit(f"cannot read per-player keys from {PLAYER_TABLE}")
    return keys


def lint(path, keys=None, players=None):
    """The problems of one list file: a list of (line, message)."""
    path = Path(path)
    keys = config_keys() if keys is None else keys
    players = player_keys() if players is None else players
    problems = []
    test = None  # the open test: a dict
    tests = {}  # id -> line
    have_list = False

    def bad(line, message):
        problems.append((line, message))

    def finish():
        if not test:
            return
        if not test["checks"]:
            bad(test["line"], f"test {test['id']}: no check")
        if bool(test["map"]) + bool(test["screen"]) + bool(test["menus"]) > 1:
            bad(test["line"], f"test {test['id']}: both map and screen (or menus)")
        elif not test["map"] and not test["screen"] and not test["menus"]:
            bad(test["line"], f"test {test['id']}: neither map nor screen (nor menus)")

    try:
        data = path.read_bytes()
    except OSError:
        return [(1, "cannot read")]
    # (a UTF-8 byte order mark is not part of the first line, as in the parser)
    lines = data.removeprefix(b"\xef\xbb\xbf").decode("utf-8", errors="replace").split("\n")
    test_lines = 0
    for at_line, raw in enumerate(lines, 1):
        text = raw.strip(" \t\r")
        if not text or text.startswith("#"):
            continue
        parts = re.split(r"[ \t]+", text, maxsplit=1)
        key, value = parts[0], (parts[1].strip(" \t\r") if len(parts) > 1 else "")
        if key == "test":
            finish()
            if not ID.fullmatch(value) or len(value) > 31:
                bad(at_line, f"bad test id '{value}'")
            elif value in tests:
                bad(at_line, f"duplicate test id '{value}' (first at line {tests[value]})")
            tests.setdefault(value, at_line)
            test_lines += 1
            if test_lines == MAX_TESTS + 1:
                bad(at_line, f"more than {MAX_TESTS} tests")
            test = {"id": value, "line": at_line, "checks": 0, "sets": 0, "map": "", "screen": 0, "menus": False}
        elif key == "list" and not test:
            have_list = True
            if not ID.fullmatch(value) or len(value) > 31:
                bad(at_line, f"bad list id '{value}'")
            elif value != path.stem:
                bad(at_line, f"list id '{value}' is not the file name '{path.stem}'")
        elif key == "revision" and not test:
            if number(value) is None:
                bad(at_line, f"bad revision '{value}'")
        elif key == "title":
            pass
        elif test and key == "check":
            if not value:
                bad(at_line, "empty check")
            elif len(value) > CHECK_LENGTH:
                bad(at_line, f"check is {len(value)} characters, at most {CHECK_LENGTH}")
            test["checks"] += 1
            if test["checks"] == MAX_CHECKS + 1:
                bad(at_line, f"more than {MAX_CHECKS} checks")
        elif test and key == "map":
            if not value:
                bad(at_line, "map has no name")
            test["map"] = value
        elif test and key == "screen":
            if number(value, 1) is None:
                bad(at_line, f"bad screen number '{value}'")
            else:
                test["screen"] = int(value)
        elif test and key == "gametype":
            pass
        elif test and key == "set":
            pair = re.split(r"[ \t]+", value, maxsplit=1)
            if len(pair) < 2:
                bad(at_line, "set needs a key and a value")
            else:
                if pair[0] not in keys:
                    bad(at_line, f"set key '{pair[0]}' is not in the config table")
                elif pair[0] in players:
                    bad(at_line, f"set key '{pair[0]}' is a per-player setting (each player has their own)")
                test["sets"] += 1
                if test["sets"] == MAX_SETS + 1:
                    bad(at_line, f"more than {MAX_SETS} sets")
        elif test and key == "players":
            if number(value, 1, 4) is None:
                bad(at_line, f"players must be 1 to 4, not '{value}'")
        elif test and key == "menus":
            test["menus"] = True
        elif test and key == "video":
            pass
        elif test and key == "card":
            if value not in ("top", "bottom"):
                bad(at_line, f"card is top or bottom, not '{value}'")
        elif test and key == "retest":
            if number(value) is None:
                bad(at_line, f"bad retest revision '{value}'")
        else:
            bad(at_line, f"unknown line, key '{key}'")
    finish()
    if not have_list:
        bad(1, "no list line")
    return sorted(problems)


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    keys, players = config_keys(), player_keys()
    failed = False
    for name in argv:
        for line, message in lint(name, keys, players):
            print(f"{name}:{line}: {message}")
            failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
