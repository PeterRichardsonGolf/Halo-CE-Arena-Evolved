#!/usr/bin/env python3
"""log_checks: a feature's expectation read from a run's log, table driven.

    python3 tools/ae_test/log_checks.py <debug.txt|test.log> --check waypoint_off_after_pickup [--check ...]
        [--param start_text="TEAM AE SLAYER"] [--list]

Every check is a row of CHECKS: an id, what it proves, the log lines it needs (a regular expression, or a function
over all lines) and the expectation (a count range, an order, or a function). The lines are the port's own
("arena-evolved: ..." in debug.txt) plus the test hook lines HALO_CAPTURE_ON_EVENT adds ("test capture: hud: <text>",
"test capture: kill", ...), so a run that wants the HUD text checks sets HALO_CAPTURE_ON_EVENT=hud,start,kill,pickup,postgame.
tools/test_ae_test.py (TestLogChecks) runs the table over the recorded sample logs in tools/ae_test/sample_logs/.
Exit 0 when every asked check passes.
"""
import argparse
import re
import sys
from pathlib import Path

WAYPOINT = re.compile(r"item timers: waypoint (\d+) (.+?) (on|off) at tick (\d+)")
HUD = re.compile(r"test capture: hud: (.*)$")
SHORT_SPREE = re.compile(r"^Killing Spree! \(\d+\)$")


def hud_texts(lines):
    return [m.group(1).strip() for m in (HUD.search(l) for l in lines) if m]


def count(pattern, low=1, high=None):
    """a check function: the lines with `pattern` are between low and high in number"""
    rx = re.compile(pattern)

    def run(lines, params):
        n = sum(1 for l in lines if rx.search(l))
        ok = n >= low and (high is None or n <= high)
        return ok, f"{n} lines match /{pattern}/ (want {low}-{'*' if high is None else high})"
    return run


def waypoint_off_after_pickup(lines, params):
    """every item waypoint that went on goes off again (a pickup, in HUD and LINE OF SIGHT timers); TRAINING keeps it
    on until its window ends, so it is checked with --param waypoint_stays=1 the other way round"""
    on = {}
    offs = []
    for l in lines:
        m = WAYPOINT.search(l)
        if not m:
            continue
        index, label, state, tick = m.group(1), m.group(2), m.group(3), int(m.group(4))
        if state == "on":
            on.setdefault(index, tick)
        elif index in on and tick >= on[index]:
            offs.append((index, label, on[index], tick))
            del on[index]
    if params.get("waypoint_stays"):
        return not offs, f"{len(offs)} waypoints went off after a pickup (want none: TRAINING keeps them)"
    return bool(offs), f"waypoints that went off after being on: {[(o[1], o[2], o[3]) for o in offs]}"


def spree_text_short(lines, params):
    texts = hud_texts(lines)
    spree = [t for t in texts if "Spree" in t]
    ok = bool(spree) and all(SHORT_SPREE.match(t) for t in spree)
    return ok, f"spree messages: {spree} (want 'Killing Spree! (n)')"


def spree_text_long(lines, params):
    texts = hud_texts(lines)
    spree = [t for t in texts if "Spree" in t]
    ok = bool(spree) and not any(SHORT_SPREE.match(t) for t in spree)
    return ok, f"spree messages: {spree} (want the long form, no '(n)' count)"


def start_text(lines, params):
    want = params.get("start_text")
    texts = hud_texts(lines)
    if not want:
        return False, "needs --param start_text=<the gametype's name>"
    return want in texts, f"HUD messages {texts[:6]} (want {want!r})"


def postgame_shown(lines, params):
    n = sum(1 for l in lines if "test capture: postgame" in l)
    return n == 1, f"{n} post-game captures (want 1; the PNG is the report)"


def ball_melee_lethal(lines, params):
    """the "AE rules:" line (game_engine.c, ends "ball melee lethal|stock") says the ball's melee is lethal and the run
    had a kill"""
    rules = [l for l in lines if "AE rules:" in l]
    lethal = any(re.search(r"ball melee lethal\s*$", l) for l in rules)
    kills = sum(1 for l in lines if "test capture: kill" in l)
    return lethal and kills > 0, f"AE rules lethal: {lethal}, kills seen: {kills}"


def drop_except_power(lines, params):
    """a power weapon (sniper) is 'gone' at death under DROP SECONDARY EXCEPT POWER, a non-power one stays 'now'"""
    gone = [l for l in lines if "items: dropped" in l and "except power" in l and re.search(r"sniper|rocket", l) and ", gone" in l]
    bad = [l for l in lines if "items: dropped" in l and "except power" in l and re.search(r"sniper|rocket", l) and ", now" in l]
    return bool(gone) and not bad, f"power weapons removed: {len(gone)}, left lying: {len(bad)}"


def all_weapons_created(lines, params):
    m = next((re.search(r"the map's list has (\d+) weapons", l) for l in lines if "cheat_all_weapons at tick" in l), None)
    listed = sum(1 for l in lines if re.search(r"network test:   weapon \d+: ", l))
    want = int(params.get("min_weapons", 1))
    n = int(m.group(1)) if m else 0
    made = [int(x.group(1)) for x in (re.search(r"created (\d+) weapon and equipment objects", l) for l in lines) if x]
    ok = bool(m) and n >= want and listed == n and bool(made) and all(c == n for c in made)
    return ok, f"list of {n} weapons, {listed} named in the log, objects created per spawn {made} (want {n} each)"


CHECKS = {
    "waypoint_off_after_pickup": ("an item waypoint goes off once the item is picked up", waypoint_off_after_pickup),
    "ball_melee_lethal": ("oddball: BALL MELEE LETHAL in the rules and a melee kill happened", ball_melee_lethal),
    "spree_text_short": ("SHORT MESSAGES: 'Killing Spree! (5)'", spree_text_short),
    "spree_text_long": ("long spree text (SHORT MESSAGES off)", spree_text_long),
    "start_text": ("the gametype's name shows at the start (--param start_text=)", start_text),
    "postgame_report": ("the post-game report was shown (one capture)", postgame_shown),
    "drop_except_power": ("DROP SECONDARY EXCEPT POWER removes a holstered power weapon", drop_except_power),
    "all_weapons": ("cheat_all_weapons ran and the map's list is named in the log (--param min_weapons=)", all_weapons_created),
}


def run_checks(lines, names, params=None):
    params = params or {}
    results = []
    for name in names:
        if name not in CHECKS:
            results.append((name, False, "unknown check"))
            continue
        ok, message = CHECKS[name][1](lines, params)
        results.append((name, bool(ok), message))
    return results


def main(argv):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("log", nargs="?")
    p.add_argument("--check", action="append", default=[])
    p.add_argument("--param", action="append", default=[], help="name=value")
    p.add_argument("--list", action="store_true")
    a = p.parse_args(argv)
    if a.list:
        for k, (d, _) in CHECKS.items():
            print(f"{k:28} {d}")
        return 0
    if not a.log or not a.check:
        p.error("a log and at least one --check")
    params = dict(kv.partition("=")[::2] for kv in a.param)
    lines = Path(a.log).read_text(errors="replace").splitlines()
    results = run_checks(lines, a.check, params)
    for name, ok, message in results:
        print(f"{'PASS' if ok else 'FAIL'}  {name}: {message}")
    return 0 if all(ok for _, ok, _ in results) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
