#!/usr/bin/env python3
"""strip_checks: the score strip's headless checks (port/linux/game/ae_strip_engine.c, ae_score_strip.c).

    python3 tools/ae_test/strip_checks.py modes --build <rev|path> [--cases ctf,ffa_slay,...]
    python3 tools/ae_test/strip_checks.py pair --build <rev|path>
    python3 tools/ae_test/strip_checks.py digits --build <rev|path> [--maps prisoner,homobox@ce]

modes: one headless host per case, playing a seeded Arena Evolved gametype (HALO_NETWORK_TEST_GAMETYPE) with
system-link bots and debug.score_strip_log (HALO_DEBUG_SCORE_STRIP_LOG=1). Every 5 seconds of game time the game
logs what its strip says for each local player:

    score strip: tick 450 view 0 mode=ctf teams=1 left=2(blue) right=1(red) clock=4:12 up=0 goal="TO 3" ...

and each case's rules are checked on those lines (mode, sides' colours, the goal, the clock's direction, what the
context and sub-lines must read). A case PASSes when the game ran clean (the harness's own rules) and every rule
held on at least one line (the rules that must hold on every line say so).

pair: a host and a joined client (a second game instance, tools/ae_test/handshake.py's pair) playing AE TEAM SLY
(--gametype) with a kill every 6 seconds; each client line's perspective-free signature (mode, goal, the two sides' scores and colours as a set)
must equal one of the two host lines nearest its tick (Review Focus 2: a joiner's strip reads as the host's).

digits: CE's counter digits (ae_hud_digits.c) drawn by debug.digits (HALO_DEBUG_DIGITS) in magenta at 24 px cap;
in the screenshots, the magenta pixels' box must be 24 +- 1 px tall (cap top to baseline) and as wide as the ink the
game logs ("hud digits: ... ink L..R, emboldened E": R - L + E) +- 2 px, with its left at L +- 2.
"""
import argparse
import re
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import harness  # noqa: E402
import handshake  # noqa: E402

LINE = re.compile(r"score strip: tick (?P<tick>-?\d+) view (?P<view>\d+) mode=(?P<mode>\w+) teams=(?P<teams>\d) "
                  r"left=(?P<left>.*?)\((?P<lc>\w+)\) right=(?P<right>.*?)\((?P<rc>\w+)\) clock=(?P<clock>\S+) "
                  r"up=(?P<up>\d) goal=\"(?P<goal>[^\"]*)\" lead=\"(?P<lead>[^\"]*)\" context=\"(?P<context>[^\"]*)\" "
                  r"left_sub=\"(?P<left_sub>[^\"]*)\" right_sub=\"(?P<right_sub>[^\"]*)\" bars=(?P<bars>\d) "
                  r"fill=(?P<lf>[\d.]+)/(?P<rf>[\d.]+) place=(?P<place>-?\d+) lead_me=(?P<lead_me>\d)")

# the host's player kills the last other player every 6 seconds (scores move); a ball in the host's player's hand at
# 8 seconds (oddball's holder, juggernaut)
KILLS = {"HALO_NETWORK_TEST_KILL": "6"}
BALL = {"HALO_NETWORK_TEST_BALL": "8"}
# each case: the seeded gametype, the map, bots, and its rules: field -> regex (fullmatch) that must hold on some
# line; "all:<field>" must hold on every line
CASES = {
    "team_slayer": {"gametype": "AE TEAM SLY", "map": "prisoner", "bots": 3, "teams": True, "env": KILLS,
                    "rules": {"left": r"[1-9]\d*", "lf": r"0\.0[1-9]|0\.[1-9]\d", "all:mode": "slayer",
                              "all:teams": "1", "all:lc": "red|blue", "all:rc": "red|blue",
                              "all:goal": "TO 50", "all:bars": "1", "lead": r"\+\d+|TIED", "all:up": "0"}},
    "ffa_slay": {"gametype": "AE FFA SLAY", "map": "prisoner", "bots": 3, "env": KILLS,
                 "rules": {"all:mode": "slayer", "all:teams": "0", "all:lc": "me", "all:rc": "white",
                           "all:goal": "TO 25", "left": r"[1-9]\d*", "lead_me": "1",
                           "all:left_sub": r"YOU \d+(ST|ND|RD|TH)", "all:right_sub": r"(LEADER|2ND) \S+",
                           "right_sub": r"2ND \S+"}},
    "ctf": {"gametype": "AE CTF", "map": "bloodgulch", "bots": 3, "teams": True,
            "rules": {"all:mode": "ctf", "all:teams": "1", "all:goal": "TO 3", "all:lc": "red|blue",
                      "left_sub": r"HOME|DROPPED \d+:\d\d|\S+", "right_sub": r"HOME|DROPPED \d+:\d\d|\S+"}},
    # (the test host's player takes a flag at 8 seconds: a carried flag names its carrier)
    "ctf_carry": {"gametype": "AE CTF", "map": "bloodgulch", "bots": 3, "teams": True, "env": BALL,
                  "rules": {"all:mode": "ctf", "carried": "Player"}},
    "ffa_ball": {"gametype": "AE FFA BALL", "map": "prisoner", "bots": 3, "env": BALL,
                 "rules": {"all:mode": "oddball", "all:teams": "0", "all:goal": r"TO \d+:00",
                           "all:left": r"\d+:\d\d", "all:lead": "", "context": r"BALL \S+",
                           "left": r"0:(0[1-9]|[1-5]\d)|[1-9]:\d\d"}},
    "team_ball": {"gametype": "AE TEAM OB", "map": "prisoner", "bots": 3, "teams": True, "env": BALL,
                  "rules": {"all:mode": "oddball", "all:teams": "1", "all:goal": r"TO \d+:00",
                            "all:left_sub": r"YOU \d+:\d\d", "context": r"BALL \S+",
                            "left_sub": r"YOU 0:(0[1-9]|[1-5]\d)"}},
    "king": {"gametype": "AE KING", "map": "prisoner", "bots": 3, "teams": True,
             "rules": {"all:mode": "king", "all:teams": "1", "all:goal": r"TO \d+:00",
                       "all:context": r"HILL (EMPTY|YOURS|CONTESTED|THEIRS)(  MOVES IN \d+:\d\d)?"}},
    "crazy_king": {"gametype": "AE CRAZYKNG", "map": "prisoner", "bots": 3,
                   "rules": {"all:mode": "king", "all:teams": "0",
                             "all:context": r"HILL (EMPTY|YOURS|CONTESTED|THEIRS)  MOVES IN \d+:\d\d"}},
    "jugger": {"gametype": "AE JUGGER", "map": "prisoner", "bots": 3, "env": KILLS,
               "rules": {"all:mode": "juggernaut", "all:teams": "0", "all:goal": r"TO \d+",
                         "context": r"JUGGERNAUT \S+|YOU ARE THE JUGGERNAUT"}},
    "accum": {"gametype": "AE ACCUM", "map": "prisoner", "bots": 3,
              "rules": {"all:mode": "accumulation", "all:teams": "0", "all:context": r"YOU HOLD \d+ OF \d+"}},
    "stalker": {"gametype": "AE STALKER", "map": "prisoner", "bots": 3,
                "rules": {"all:mode": "juggernaut", "all:teams": "0"}},
    "rev_tag": {"gametype": "AE REV TAG", "map": "prisoner", "bots": 3,
                "rules": {"all:mode": "accumulation", "all:teams": "0", "context": r"IT \S+|"}},
    "elim": {"gametype": "AE ELIM", "map": "prisoner", "bots": 3, "env": KILLS,
             "rules": {"all:mode": "elimination", "all:teams": "0",
                       "all:context": r"(LAST LIFE|LIVES \d+)?( *\d+ OF \d+ LEFT)?",
                       "context": r"LAST LIFE  [0-3] OF 4 LEFT"}},
    "phantoms": {"gametype": "AE PHANTOMS", "map": "prisoner", "bots": 3,
                 "rules": {"all:mode": "phantoms", "all:teams": "0", "context": r"TARGET \S+"}},
    "race": {"gametype": "AE RACE", "map": "bloodgulch", "bots": 3,
             "rules": {"all:mode": "race", "all:bars": "0", "all:goal": "", "all:left": r"\d+(ST|ND|RD|TH)",
                       "all:left_sub": r"LAP \d+( OF \d+)?", "all:context": r"CHECKPOINT \d+( OF \d+)?"}},
    "rally": {"gametype": "AE RALLY", "map": "bloodgulch", "bots": 3,
              "rules": {"all:mode": "rally", "all:bars": "0", "all:left_sub": r"POINTS \d+( OF \d+)?",
                        "context": r"NEXT FLAG \d+ M"}},
    # (no seeded slayer has no score limit: AE PRACTICE plays to 500; the clock counting up, Review Focus 3)
    "count_up": {"gametype": "AE PRACTICE", "map": "prisoner", "bots": 2, "env": {"HALO_MATCH_CLOCK": "up"},
                 "rules": {"all:mode": "slayer", "up": "1", "all:goal": "TO 500", "clock": r"0:[1-5]\d"}},
}
DEFAULT_CASES = ",".join(CASES)


def strip_lines(text):
    return [m.groupdict() for m in map(LINE.search, text.splitlines()) if m]


def check_rules(lines, rules):
    """the rules that failed (a list of strings; empty: all held)"""
    failed = []
    if not lines:
        return ["no score strip lines"]
    for key, pattern in rules.items():
        every = key.startswith("all:")
        field = key[4:] if every else key
        if field == "carried":  # (either flag's line names the carrier)
            held = [pattern in (line["left_sub"], line["right_sub"]) for line in lines]
        else:
            held = [bool(re.fullmatch(pattern, line[field])) for line in lines]
        if every and not all(held):
            bad = next(line for line, ok in zip(lines, held) if not ok)
            failed.append(f"{field} = {bad[field]!r} (tick {bad['tick']}) is not /{pattern}/ on every line")
        elif not every and not any(held):
            failed.append(f"{field} never /{pattern}/ (seen {sorted({line[field] for line in lines})[:4]})")
    return failed


def case_spec(name, build, exit_after=45):
    case = CASES[name]
    env = {"HALO_DEBUG_SCORE_STRIP_LOG": "1", **case.get("env", {})}
    if case.get("teams"):
        env["HALO_NETWORK_TEST_AUTO_BALANCE"] = "1"
    return harness.parse_spec({"name": name, "build": build, "map": case["map"], "saved_gametype": case["gametype"],
                               "start": 5, "bots": case["bots"], "bot_args": ["--move"], "exit_after": exit_after,
                               "env": env, "expect": {"ticks": 300}})


def run_modes(cfg, a, out):
    names = [n for n in a.cases.split(",") if n]
    for n in names:
        if n not in CASES:
            raise SystemExit(f"strip_checks: unknown case {n} (known: {', '.join(CASES)})")
    parallel = harness.resolve_parallel(cfg, a.parallel)
    slots = harness.GameSlots(cfg, parallel)

    def one(name):
        r = harness.play(cfg, case_spec(name, a.build, a.seconds), out / name, slots, a.keep_work,
                         run_id=f"{out.name}-{name}")
        debug = out / name / "debug.txt"
        lines = strip_lines(debug.read_text(errors="replace")) if debug.exists() else []
        failed = check_rules(lines, CASES[name]["rules"])
        status = "PASS" if r.get("status") == "PASS" and not failed else "FAIL"
        why = (r.get("why") or []) + failed
        harness.log(f"  {name}: {status}, {len(lines)} lines" + (f" ({'; '.join(why)})" if why else ""))
        return name, {"status": status, "why": why, "lines": len(lines), "last": lines[-1] if lines else None}

    with ThreadPoolExecutor(max_workers=parallel) as ex:
        results = dict(ex.map(one, names))
    harness.write_json(out / "strip_checks.json", results)
    return results


def signature(line):
    sides = sorted([(line["left"], line["lc"]), (line["right"], line["rc"])])
    return (line["mode"], line["goal"], tuple(sides))


def compare_pair(host_lines, client_lines):
    """the client lines whose signature is not one of the two host lines nearest their tick"""
    bad = []
    host = sorted((int(l["tick"]), signature(l)) for l in host_lines if l["view"] == "0")
    for line in client_lines:
        if line["view"] != "0":
            continue
        tick = int(line["tick"])
        near = sorted(host, key=lambda h: abs(h[0] - tick))[:2]
        if not any(sig == signature(line) for _, sig in near):
            bad.append(f"client tick {tick} {signature(line)} vs host {near}")
    return bad


def run_pair(cfg, a, out):
    host, client = handshake.pair_specs(a.build, a.build, seconds=a.seconds + 30, join_delay=12,
                                        env={"HALO_DEBUG_SCORE_STRIP_LOG": "1", "HALO_NETWORK_TEST_AUTO_BALANCE": "1",
                                             "HALO_NETWORK_TEST_KILL": "6"},
                                        map_name="bloodgulch", saved_gametype=a.gametype)
    work = harness.expand(cfg["work_dir"]) / f"{out.name}-pair"
    harness.OWN_WORK.add(work)
    build = harness.resolve_build(cfg, a.build)
    prepared = [harness.prepare_game(cfg, s, build, work / s["name"], out / "pair" / s["name"]) for s in (host, client)]
    slots = harness.GameSlots(cfg, 2)
    slots.acquire(2)
    try:
        inner = harness.run_group(cfg, prepared, None, work)
    finally:
        slots.release(2)
    results = [harness.collect_game(s, p, i, out / "pair" / s["name"], a.keep_work)
               for s, p, i in zip((host, client), prepared, inner)]
    texts = [(out / "pair" / s["name"] / "debug.txt") for s in (host, client)]
    host_lines, client_lines = [strip_lines(t.read_text(errors="replace")) if t.exists() else [] for t in texts]
    why = [] if (results[1].get("debug") or {}).get("joined") else ["client never joined"]
    if not any(l["left"] not in ("0", "0:00") or l["right"] not in ("0", "0:00") for l in client_lines):
        why.append("no score moved on the client (nothing to compare)")
    if len(client_lines) < 3:
        why.append(f"client logged {len(client_lines)} strip lines")
    why += compare_pair(host_lines, client_lines)
    status = "FAIL" if why else "PASS"
    harness.log(f"  pair: {status}, host {len(host_lines)} lines, client {len(client_lines)}" +
                (f" ({'; '.join(why[:5])})" if why else ""))
    result = {"status": status, "why": why, "host_lines": len(host_lines), "client_lines": len(client_lines)}
    harness.write_json(out / "pair.json", result)
    return {"pair": result}


DIGITS_TEXT = "1:42 17 10"
DIGITS_LOG = re.compile(r"hud digits: '[^']*' at (?P<cap>[\d.]+) px cap, x [\d.]+ baseline (?P<base>[\d.]+): "
                        r"advance [\d.]+, ink (?P<left>[\d.]+)\.\.(?P<right>[\d.]+), emboldened (?P<bold>[\d.]+), "
                        r"sheet (?P<sheet>\w+)")


def magenta_box(w, h, rgb, x0=0, y0=0, x1=None, y1=None):
    """the box (x0, y0, x1, y1, inclusive) of the debug digits' magenta pixels in a region, or None"""
    x1 = w if x1 is None else min(x1, w)
    y1 = h if y1 is None else min(y1, h)
    xs, ys = [], []
    for y in range(y0, y1):
        row = y * w * 3
        for x in range(x0, x1):
            r, g, b = rgb[row + 3 * x], rgb[row + 3 * x + 1], rgb[row + 3 * x + 2]
            if r > 150 and b > 150 and g < 90 and abs(r - b) < 70:
                xs.append(x)
                ys.append(y)
    return (min(xs), min(ys), max(xs), max(ys)) if xs else None


def judge_digits(text, pngs):
    """(why list, the measured box): the digits' box against the game's logged ink"""
    m = DIGITS_LOG.search(text)
    if not m:
        return ["no 'hud digits:' line"], None
    if m["sheet"] != "found":
        return ["the map's HUD digits sheet was not found"], None
    cap, base = float(m["cap"]), float(m["base"])
    left, right, bold = float(m["left"]), float(m["right"]), float(m["bold"])
    boxes = []
    for png in pngs:
        w, h, rgb = harness.read_png(png)
        box = magenta_box(w, h, rgb, 0, int(base - cap - 20), int(right + 40), int(base + 10))
        if box:
            boxes.append(box)
    if not boxes:
        return ["no magenta digits in any screenshot"], None
    box = boxes[-1]
    why = []
    height = box[3] - box[1] + 1
    width = box[2] - box[0] + 1
    if abs(height - cap) > 1:
        why.append(f"height {height} px, not {cap:g} +- 1")
    if abs(width - (right - left + bold)) > 2:
        why.append(f"width {width} px, not {right - left + bold:.1f} +- 2")
    if abs(box[0] - left) > 2:
        why.append(f"left {box[0]}, not {left:.1f} +- 2")
    if abs(box[3] + 1 - base) > 1:
        why.append(f"bottom {box[3] + 1}, not the baseline {base:g} +- 1")
    return why, box


def run_digits(cfg, a, out):
    results = {}
    for map_name in a.maps.split(","):
        name = "digits-" + harness.safe_name(map_name)
        spec = harness.parse_spec({"name": name, "build": a.build, "map": map_name, "start": 3, "bots": 1,
                                   "window": "1920x1080", "exit_after": 40, "screenshots": 100,
                                   "env": {"HALO_DEBUG_DIGITS": DIGITS_TEXT}, "expect": {"ticks": 200}})
        r = harness.play(cfg, spec, out / name, None, a.keep_work, run_id=f"{out.name}-{name}")
        debug = out / name / "debug.txt"
        text = debug.read_text(errors="replace") if debug.exists() else ""
        pngs = [out / name / s for s in r.get("shots", [])][1:]
        why, box = judge_digits(text, pngs)
        why = (r.get("why") or []) + why
        status = "PASS" if r.get("status") == "PASS" and not why else "FAIL"
        harness.log(f"  {name}: {status}, box {box}" + (f" ({'; '.join(why)})" if why else ""))
        results[name] = {"status": status, "why": why, "box": box}
    harness.write_json(out / "digits.json", results)
    return results


def main(argv):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    harness.add_common_args(p)
    p.add_argument("what", choices=("modes", "pair", "digits"))
    p.add_argument("--build", required=True)
    p.add_argument("--cases", default=DEFAULT_CASES)
    p.add_argument("--seconds", type=int, default=45)
    p.add_argument("--gametype", default="AE TEAM SLY", help="pair: the host's seeded gametype")
    p.add_argument("--maps", default="prisoner,homobox@ce", help="digits: the maps")
    p.add_argument("--parallel", default=None)
    a = p.parse_args(argv)
    if a.box:
        raise SystemExit("strip_checks: run it on the box itself (ssh halo-build), with --build a path or commit there")
    cfg = harness.load_config(a.config)
    out = harness.new_out_dir(cfg, "strip_checks", a.what, a.out_name)
    results = {"modes": run_modes, "pair": run_pair, "digits": run_digits}[a.what](cfg, a, out)
    failed = [n for n, r in results.items() if r["status"] != "PASS"]
    harness.log(f"strip_checks {a.what}: {len(results) - len(failed)} passed, {len(failed)} failed; results in {out}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
