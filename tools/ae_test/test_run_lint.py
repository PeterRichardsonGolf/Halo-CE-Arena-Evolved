#!/usr/bin/env python3
"""Lints TEST RUN lists (tools/ae_test/test_runs/*.txt): the rules of the game's parser
(port/linux/game/ae_test_run_list.c) plus the ones only a tool can check before shipping: a `set` key must be a
config key (the table of port/linux/src/port_config.c), the list id is the file name, a check is at most 60
characters, a test id appears once, a setup line starts with a dev command (port/linux/game/ae_dev_cmd_rules.c), a
screen or menus test has no setup line that changes a game (no game runs there: the game answers "no game running")
and an expect line's rule is one tools/ae_test/expect.py judges.

usage: test_run_lint.py <list files...>
Prints `file:line: message` for each problem; exit 1 if there is any. Warnings (`file:line: warning: ...`) do not fail
a list: a check that names a place or an object (a flag, a ramp, a tank) in a test with no goto setup line, which
leaves the owner to walk there (owner 2026-11-02: "this test should have spawned me by the blue flag")."""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import expect  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent.parent
CONFIG_TABLE = ROOT / "port" / "linux" / "src" / "port_config.c"
PLAYER_TABLE = ROOT / "port" / "linux" / "src" / "ae_config_players.c"
ID = re.compile(r"[a-z0-9-]+")
DEV_RULES = ROOT / "port" / "linux" / "game" / "ae_dev_cmd_rules.c"
MAX_TESTS, MAX_CHECKS, MAX_SETS, CHECK_LENGTH, MAX_NUMBER = 64, 3, 16, 60, 32767
MAX_SETUPS, SETUP_LENGTH, EXPECT_LENGTH = 10, 119, 95
# the dev commands that change a game, so need one running (ae_dev_cmd.c's in_tick)
GAME_COMMANDS = {"give", "spawn", "powerup", "respawn", "kill", "end", "score", "goto", "time", "reset", "option", "dummy", "tune"}
EXPECT_RULES = ("weapon", "ammo", "spawn", "death", "kill", "score", "object", "gone", "respawn", "hud", "cmd",
                "game_over", "no", "log", "option", "round_start", "round_end", "down", "out", "revived", "zone",
                "round_wait", "match_over")
# places and objects a check can be about: the test should put the owner there (setup goto <spot> [facing <spot>])
PLACES = re.compile(r"\b(flags?|altar|ramps?|tanks?|hills?|ball|bomb|bases?|teleporters?|ledges?|bridges?|caves?|"
                    r"towers?|warthogs?|ghosts?|banshees?|scorpions?|turrets?|plates?|sites?|lifts?|cliffs?)\b", re.I)
# a check that sends the owner to set something up by hand: a test sets itself up (setup lines), the owner only looks
# the stock multiplayer maps' items (equipment tags' paths): setup powerup <item> finds a map's item by its file's name or
# a part of its path, "_" for a space (ae_dev_cmd.c's find_tag). "overshield" matched nothing ("over shield"), so the
# rc1 round's overshield test gave none: a name no stock item has is a warning (a map's own items may still match)
STOCK_ITEMS = ("powerups/over shield", "powerups/active camouflage", "powerups/health pack", "powerups/double speed",
               "powerups/full-spectrum vision", "powerups/assault rifle ammo/assault rifle ammo",
               "powerups/pistol ammo/pistol ammo", "powerups/shotgun ammo/shotgun ammo",
               "powerups/sniper rifle ammo/sniper rifle ammo", "powerups/rocket launcher ammo/rocket launcher ammo",
               "powerups/flamethrower ammo/flamethrower ammo", "powerups/needler ammo/needler ammo",
               "weapons/frag grenade/frag grenade", "weapons/plasma grenade/plasma grenade")
# the words the game reads after a dev command's name (ae_dev_cmd_rules.h's AE_DEV_CMD_WORDS); the commands that take the
# rest of their line as one text are not counted
COMMAND_WORDS = 10
REST_COMMANDS = {"gametype", "mark", "config", "agent", "menu", "say"}
HAND_SETUP = re.compile(r"\b(set|switch|turn|change|pick|choose)\b.*\b(game options|settings|options)\b|"
                        r"\bby hand\b|\bpause\b.*\bgame options\b", re.I)


def dev_commands():
    """The dev commands' names ({ "give", "usage", bare },)."""
    names = set(re.findall(r'\{ "([a-z_]+)", "[^"]*", [01] \}', DEV_RULES.read_text(errors="replace")))
    if not names:
        raise SystemExit(f"cannot read the dev commands from {DEV_RULES}")
    return names


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


# a check that has the owner walk to an item or shoot it where it lies: the item must lie away from them
LIES_AWAY = re.compile(r"\b(lies|lying|on the ground|in front of you|ahead of you|walk (over|onto|to)|shoot it)\b", re.I)


def staging_warnings(lines):
    """Staging that a headless run accepts and the owner cannot judge: [(line, message)].
    - a test that sets a bots.* value after a test that starts a tournament game (setup social start): that game borrows
      the bot settings and puts its own saved ones back when it ends, which is after the next test set its own;
    - an item or a flag put at the owner's feet (powerup <item> [now], flag ours|theirs here) in a test whose check has
      them walk to it or shoot it where it lies: it is picked up or touched as the test opens;
    - an expect line on "ae seed rules:", the line of a gametype's choice: the game says its rules as "live rule: ..."."""
    found = []
    social, group_bots, in_test, test_id = None, None, False, ""
    feet, checks = [], []

    def finish():
        if feet and any(LIES_AWAY.search(text) for text in checks):
            for at_line, value in feet:
                word = value.split()[0]
                tail = "powerup <item> ahead" if word == "powerup" else f"flag {value.split()[1]} ahead"
                found.append((at_line, f"test {test_id}: setup {value} puts it at your feet, where it is taken as the test "
                                       f"opens, and the check has you go to it or shoot it: use {tail}"))

    for at_line, raw in enumerate(lines, 1):
        text = raw.strip()
        if not text or text.startswith("#"):
            continue
        parts = text.split(None, 1)
        key, value = parts[0], parts[1].strip() if len(parts) > 1 else ""
        words = value.split()
        if key == "group":
            finish()
            in_test, group_bots, feet, checks = False, None, [], []
        elif key == "test":
            finish()
            in_test, test_id, feet, checks = True, value, [], []
            # (said once a group, at its first test)
            if social and group_bots:
                found.append((group_bots, f"test {value}: its group sets a bots.* value after test {social} started a "
                                          f"tournament game, which puts the bot settings back when its game ends: after "
                                          f"this test set its own. Move test {social} to the end of the list"))
                group_bots = None
        elif key == "set" and words[:1] and words[0].startswith("bots."):
            if not in_test:
                group_bots = at_line
            elif social:
                found.append((at_line, f"test {test_id}: sets {words[0]} after test {social} started a tournament game, "
                                       f"which puts the bot settings back when its game ends: after this test set its "
                                       f"own. Move test {social} to the end of the list"))
        elif key == "check" and in_test:
            checks.append(value)
        elif key == "expect" and "log ae seed rules" in value.lower():
            found.append((at_line, f"test {test_id}: 'ae seed rules:' is written when the gametype is chosen, before the "
                                   f"game starts, and says nothing of the game that runs: expect a 'log live rule: ...' "
                                   f"line (live rule: flag pickup hold, live rule: shootable grenades on)"))
        elif key == "setup" and in_test:
            if words[:2] == ["social", "start"]:
                social = test_id
            elif words[:1] == ["powerup"] and (len(words) == 2 or (len(words) == 3 and words[2].lower() == "now")):
                feet.append((at_line, value))
            elif words[:1] == ["flag"] and len(words) == 3 and words[2].lower() == "here":
                feet.append((at_line, value))
    finish()
    return found


def warnings(path):
    """The warnings of one list file (a list that also passes lint): a list of (line, message)."""
    found = []
    test, group_goto = None, False

    def finish():
        if test and not test["goto"]:
            for line, text in test["checks"]:
                match = PLACES.search(text)
                if match:
                    found.append((line, f"test {test['id']}: the check names '{match.group(0)}' but no setup goto puts "
                                        f"you there (setup goto <spot> [facing <spot>])"))
                    break

    try:
        lines = Path(path).read_text(errors="replace").split("\n")
    except OSError:
        return found
    for at_line, raw in enumerate(lines, 1):
        text = raw.strip()
        if not text or text.startswith("#"):
            continue
        parts = text.split(None, 1)
        key, value = parts[0], parts[1].strip() if len(parts) > 1 else ""
        is_goto = key == "setup" and value.split()[:1] in (["goto"], ["dev"]) and "goto" in value.split()[:2]
        if key == "group":
            finish()
            test, group_goto = None, False
        elif key == "test":
            finish()
            test = {"id": value, "checks": [], "goto": group_goto}
        elif is_goto:
            if test:
                test["goto"] = True
            else:
                group_goto = True
        elif test and key == "check":
            test["checks"].append((at_line, value))
        if key == "setup" and value.split()[:1] == ["powerup"] and len(value.split()) > 1:
            item = value.split()[1].replace("_", " ").lower()
            if not any(item in path for path in STOCK_ITEMS):
                stock = ", ".join(path.rsplit("/", 1)[1].replace(" ", "_") for path in STOCK_ITEMS[:5])
                found.append((at_line, f"setup powerup {value.split()[1]}: no stock map's item is named so ({stock}, "
                                       f"...; '_' for a space): only a map's own item can match it"))
    finish()
    found += staging_warnings(lines)
    return sorted(found)


def lint(path, keys=None, players=None):
    """The problems of one list file: a list of (line, message)."""
    path = Path(path)
    keys = config_keys() if keys is None else keys
    players = player_keys() if players is None else players
    problems = []
    commands = dev_commands()
    test = None  # the open test: a dict
    group = None  # the open group's lines: a dict, as a test's
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
        elif test["screen"] or test["menus"]:
            for line, name in test["game_setups"]:
                bad(line, f"test {test['id']}: setup '{name}' changes a game, and a "
                          f"{'screen' if test['screen'] else 'menus'} test has none running")
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
        if key == "group":
            finish()
            test = None
            if not ID.fullmatch(value) or len(value) > 31:
                bad(at_line, f"bad group id '{value}'")
            group = {"id": value, "line": at_line, "checks": 0, "sets": 0, "map": "", "screen": 0, "menus": False,
                     "setups": 0, "game_setups": [], "is_group": True}
            continue
        # (a group's own lines, before its first test, are written into the group's lines)
        target = test if test else group if group and key in ("map", "screen", "gametype", "set", "players", "setup",
                                                               "menus") else None
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
            test = {"id": value, "line": at_line, "checks": 0, "sets": 0, "map": "", "screen": 0, "menus": False,
                    "setups": 0, "game_setups": []}
            if group:
                for copied in ("sets", "map", "screen", "menus", "setups"):
                    test[copied] = group[copied]
                test["game_setups"] = list(group["game_setups"])
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
            elif HAND_SETUP.search(value):
                bad(at_line, "check asks for setup by hand: put it in setup lines (option, give, spawn ...)")
            test["checks"] += 1
            if test["checks"] == MAX_CHECKS + 1:
                bad(at_line, f"more than {MAX_CHECKS} checks")
        elif target and key == "map":
            if not value:
                bad(at_line, "map has no name")
            target["map"] = value
        elif target and key == "screen":
            if number(value, 1) is None:
                bad(at_line, f"bad screen number '{value}'")
            else:
                target["screen"] = int(value)
        elif target and key == "gametype":
            pass
        elif target and key == "set":
            pair = re.split(r"[ \t]+", value, maxsplit=1)
            if len(pair) < 2:
                bad(at_line, "set needs a key and a value")
            else:
                if pair[0] not in keys:
                    bad(at_line, f"set key '{pair[0]}' is not in the config table")
                elif pair[0] in players:
                    bad(at_line, f"set key '{pair[0]}' is a per-player setting (each player has their own)")
                target["sets"] += 1
                if target["sets"] == MAX_SETS + 1:
                    bad(at_line, f"more than {MAX_SETS} sets")
        elif target and key == "players":
            if number(value, 1, 4) is None:
                bad(at_line, f"players must be 1 to 4, not '{value}'")
        elif target and key == "menus":
            target["menus"] = True
        elif target and key == "setup":
            words = value.split()
            name = words[1] if words and words[0] == "dev" and len(words) > 1 else words[0] if words else ""
            if not value:
                bad(at_line, "empty setup")
            elif len(value) > SETUP_LENGTH:
                bad(at_line, f"setup is {len(value)} characters, at most {SETUP_LENGTH}")
            elif name.lower() not in commands:
                bad(at_line, f"setup '{value}' is not a dev command")
            elif name.lower() not in REST_COMMANDS and len(words) - (2 if words[0] == "dev" else 1) > COMMAND_WORDS:
                bad(at_line, f"setup {name} has {len(words) - (2 if words[0] == 'dev' else 1)} words after its name: the "
                             f"game reads {COMMAND_WORDS}, drops the rest and refuses the line")
            elif name.lower() in GAME_COMMANDS:
                target["game_setups"].append((at_line, name.lower()))
            target["setups"] += 1
            if target["setups"] == MAX_SETUPS + 1:
                bad(at_line, f"more than {MAX_SETUPS} setups with the group's")
        elif test and key == "expect":
            parts = value.split(None, 1)
            n, rule = (parts[0], parts[1].strip()) if len(parts) > 1 else (value, "")
            if number(n, 1, MAX_CHECKS) is None or int(n) > test["checks"]:
                bad(at_line, f"expect names no check given yet ('{n}')")
            elif not rule:
                bad(at_line, "expect has no rule")
            elif len(rule) > EXPECT_LENGTH:
                bad(at_line, f"expect's rule is {len(rule)} characters, at most {EXPECT_LENGTH}")
            elif rule.split()[0] not in EXPECT_RULES:
                bad(at_line, f"expect rule '{rule.split()[0]}' is not one expect.py judges")
            elif expect.validate(rule):
                bad(at_line, f"expect rule: {expect.validate(rule)}")
        elif test and key in ("video", "fresh"):
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
        for line, message in warnings(name):
            print(f"{name}:{line}: warning: {message}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
