#!/usr/bin/env python3
"""strip_checks: the score strip's headless checks (port/linux/game/ae_strip_engine.c, ae_score_strip.c).

    python3 tools/ae_test/strip_checks.py modes --build <rev|path> [--cases ctf,ffa_slay,...]
    python3 tools/ae_test/strip_checks.py pair --build <rev|path>
    python3 tools/ae_test/strip_checks.py digits --build <rev|path> [--maps prisoner,homobox@ce]
    python3 tools/ae_test/strip_checks.py states --build <rev|path>
    python3 tools/ae_test/strip_checks.py clock --build <rev|path> --base <the pre-strip build's path>
    python3 tools/ae_test/strip_checks.py style --build <rev|path>

modes: one headless host per case, playing a seeded Arena Evolved gametype (HALO_NETWORK_TEST_GAMETYPE) with
system-link bots and debug.score_strip_log (HALO_DEBUG_SCORE_STRIP_LOG=1). Every 5 seconds of game time the game
logs what its strip says for each local player:

    score strip: tick 450 view 0 mode=ctf teams=1 left=2(blue) right=1(red) clock=4:12 up=0 goal="TO 3" ...

and each case's rules are checked on those lines (mode, sides' colours, the goal, the clock's direction, what the
context and sub-lines must read). A case PASSes when the game ran clean (the harness's own rules) and every rule
held on at least one line (the rules that must hold on every line say so).

pair: a host and a joined client (a second game instance, tools/ae_test/handshake.py's pair) playing AE TEAM SLY
(--gametype) with a kill every 6 seconds; each client line's perspective-free signature (mode, goal, the two sides'
scores and colours as a set) must equal one of the two host lines nearest its tick (Review Focus 2: a joiner's
strip reads as the host's).

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
                  r"fill=(?P<lf>[\d.]+)/(?P<rf>[\d.]+) place=(?P<place>-?\d+) lead_me=(?P<lead_me>\d)"
                  r"(?: drawn=(?P<drawn>\d) alpha=(?P<alpha>[\d.]+) clock_box=(?P<clock_box>[-\d,]+) "
                  r"score_box=(?P<score_box>[-\d,]+))?"
                  r"(?: list=(?P<list>\d) list_foot=(?P<list_foot>[-\d.]+) list_left=(?P<list_left>[-\d.]+) "
                  r"line=(?P<line>[-\d.]+) corner_clock=(?P<corner_clock>\d))?")

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


SHOT = re.compile(r"capture: screenshot (?P<path>\S+)")
TEAM_RGB = {"blue": (89, 140, 255), "red": (255, 77, 77)}


def shots_with_lines(text):
    """(screenshot file name, the strip line of view 0 logged before it or None) in the order taken"""
    last, out = None, []
    for raw in text.splitlines():
        m = LINE.search(raw)
        if m and m["view"] == "0":
            last = m.groupdict()
            continue
        s = SHOT.search(raw)
        if s:
            out.append((Path(s["path"]).name, last))
    return out


def box_pixels(w, h, rgb, box, pad=2):
    x, y, bw, bh = [float(v) for v in box.split(",")]
    for yy in range(max(0, int(y) - pad), min(h, int(y + bh) + pad)):
        for xx in range(max(0, int(x) - pad), min(w, int(x + bw) + pad)):
            i = (yy * w + xx) * 3
            yield rgb[i], rgb[i + 1], rgb[i + 2]


def strip_shown_in(png, line):
    """why the strip does not show as its line says in this screenshot (empty: it does)"""
    w, h, rgb = harness.read_png(png)
    why = []
    bright = sum(1 for r, g, b in box_pixels(w, h, rgb, line["clock_box"]) if r > 200 and g > 200 and b > 200)
    if bright < 15:
        why.append(f"{png.name}: {bright} bright pixels in the clock's box")
    target = TEAM_RGB.get(line["lc"])
    if target:
        near = sum(1 for p in box_pixels(w, h, rgb, line["score_box"]) if all(abs(p[i] - target[i]) <= 40
                                                                                for i in range(3)))
        if near < 8:
            why.append(f"{png.name}: {near} pixels of {line['lc']} in the left score's box")
    return why


def judge_states(case, text, out_dir):
    lines = [l for l in strip_lines(text) if l["view"] == "0"]
    if not lines or lines[0].get("drawn") is None:
        return ["no score strip lines with the drawn state"]
    why = []
    if case == "classic":
        if any(l["drawn"] == "1" for l in lines):
            why.append("drawn with HUD STYLE CLASSIC")
        return why
    if not (int(lines[0]["tick"]) < 90 and lines[0]["drawn"] == "0"):
        why.append(f"the first line (tick {lines[0]['tick']}) is not a hidden one in the countdown")
    shown = [(name, l) for name, l in shots_with_lines(text) if l and l["drawn"] == "1"]
    # (the short game's screenshots may all fall in its countdown and end: the shown case looks at pictures)
    if not shown and case == "shown":
        why.append("no screenshot while the strip was drawn")
    for name, line in shown[-2:]:
        why += strip_shown_in(out_dir / "shots" / name, line)
    if case == "postgame":
        drawn = [i for i, l in enumerate(lines) if l["drawn"] == "1"]
        hidden_after = [i for i, l in enumerate(lines) if l["drawn"] == "0" and drawn and i > drawn[0]]
        if not hidden_after:
            why.append("never hidden after the game ended")
        elif any(i > hidden_after[0] for i in drawn):
            why.append("drawn again after the game ended")
    return why


def run_states(cfg, a, out):
    cases = {
        "shown": {},
        "postgame": {"HALO_NETWORK_TEST_SCORE": "2", "HALO_NETWORK_TEST_KILL": "4"},
        "classic": {"HALO_HUD_STYLE": "classic"},
    }
    results = {}
    for name, extra in cases.items():
        env = {"HALO_DEBUG_SCORE_STRIP_LOG": "1", "HALO_NETWORK_TEST_AUTO_BALANCE": "1", "HALO_NETWORK_TEST_KILL": "6",
               **extra}
        spec = harness.parse_spec({"name": "states-" + name, "build": a.build, "map": "prisoner",
                                   "saved_gametype": "AE TEAM SLY", "start": 5, "bots": 3, "bot_args": ["--move"],
                                   "window": "1920x1080", "exit_after": 45, "screenshots": 150, "env": env,
                                   "expect": {"ticks": 300}})
        r = harness.play(cfg, spec, out / spec["name"], None, a.keep_work, run_id=f"{out.name}-{name}")
        debug = out / spec["name"] / "debug.txt"
        why = judge_states(name, debug.read_text(errors="replace") if debug.exists() else "", out / spec["name"])
        status = "PASS" if r.get("status") == "PASS" and not why else "FAIL"
        why = (r.get("why") or []) + why
        harness.log(f"  states {name}: {status}" + (f" ({'; '.join(why)})" if why else ""))
        results[name] = {"status": status, "why": why}
    harness.write_json(out / "states.json", results)
    return results


# 1920x1080 single view: the HUD's corners (x0, y0, x1, y1)
HUD_REGIONS = {"ammo": (140, 70, 430, 200), "shield": (1470, 70, 1780, 170), "sensor": (120, 800, 350, 1015),
               "clock": (1590, 955, 1775, 1008)}


def old_clock_blue(p):
    """the old corner clock's colour: the HUD's blue at 0.7 over the scene"""
    r, g, b = p
    return 60 < r < 125 and 110 < g < 170 and 150 < b < 215 and 25 <= b - g <= 70 and r < g


def hud_colour(p):
    """the HUD's own blues (its panels' lines and numbers)"""
    r, g, b = p
    return b > 150 and b - r > 50 and g > 100


def count_in(png, region, test):
    w, h, rgb = harness.read_png(png)
    x0, y0, x1, y1 = region
    n = 0
    for y in range(y0, min(y1, h)):
        for x in range(x0, min(x1, w)):
            i = (y * w + x) * 3
            if test((rgb[i], rgb[i + 1], rgb[i + 2])):
                n += 1
    return n


def judge_clock(results):
    """results: name -> (debug text, [pngs taken while playing]); why list"""
    why = []
    ae_text, ae_pngs = results["ae"]
    lines = [l for l in strip_lines(ae_text) if l["view"] == "0" and l.get("drawn") == "1"]
    if not lines:
        why.append("AE: no strip drawn")
    for l in lines:
        if l.get("corner_clock") != "0":
            why.append(f"AE: the corner clock is on at tick {l['tick']}")
            break
        if l.get("list") != "1":
            why.append(f"AE: no timers list at tick {l['tick']}")
            break
        if abs(float(l["list_foot"]) - float(l["line"])) > 3:
            why.append(f"AE: the list's foot {l['list_foot']} is not on the line {l['line']}")
            break
    for png in ae_pngs[-2:]:
        n = count_in(png, HUD_REGIONS["clock"], old_clock_blue)
        if n > 15:
            why.append(f"AE: {n} pixels of the old clock's blue in {png.name}")
    classic_text, classic_pngs = results["classic"]
    if any(l.get("drawn") == "1" for l in strip_lines(classic_text)):
        why.append("CLASSIC: the strip was drawn")
    base_text, base_pngs = results["base"]
    if not classic_pngs or not base_pngs:
        why.append("no pictures of play from CLASSIC or the pre-strip build")
        return why
    for name, region in HUD_REGIONS.items():
        if name == "sensor":  # (its disc is see-through: its colour is the scene's as much as the HUD's)
            continue
        classic = count_in(classic_pngs[-1], region, hud_colour)
        base = count_in(base_pngs[-1], region, hud_colour)
        if abs(classic - base) > 0.25 * max(base, 40):
            why.append(f"CLASSIC vs the pre-strip build: {name} has {classic} HUD pixels, not {base} (25 %)")
    if count_in(classic_pngs[-1], HUD_REGIONS["clock"], old_clock_blue) < 60:
        why.append("CLASSIC: the old corner clock is missing")
    return why


def run_clock(cfg, a, out):
    if not a.base:
        raise SystemExit("strip_checks clock: --base <the pre-strip build> is needed")
    runs = {"ae": (a.build, "ae"), "classic": (a.build, "classic"), "base": (a.base, "classic")}
    results = {}
    statuses = []
    for name, (build, style) in runs.items():
        spec = harness.parse_spec({"name": "clock-" + name, "build": build, "map": "prisoner",
                                   "saved_gametype": "AE TEAM SLY", "start": 3, "window": "1920x1080",
                                   "exit_after": 40, "screenshots": 100,
                                   "env": {"HALO_DEBUG_SCORE_STRIP_LOG": "1", "HALO_HUD_STYLE": style},
                                   "expect": {"ticks": 300}})
        r = harness.play(cfg, spec, out / spec["name"], None, a.keep_work, run_id=f"{out.name}-{name}")
        statuses.append(r.get("status"))
        debug = out / spec["name"] / "debug.txt"
        text = debug.read_text(errors="replace") if debug.exists() else ""
        pngs = [out / spec["name"] / s for s in r.get("shots", [])]
        # (pictures of play: after the countdown, the HUD up)
        playing = [p for p in pngs if count_in(p, HUD_REGIONS["shield"], hud_colour) > 200]
        results[name] = (text, playing)
    why = judge_clock(results) + [f"a game: {s}" for s in statuses if s != "PASS"]
    status = "FAIL" if why else "PASS"
    harness.log(f"  clock: {status}" + (f" ({'; '.join(why)})" if why else ""))
    harness.write_json(out / "clock.json", {"status": status, "why": why})
    return {"clock": {"status": status, "why": why}}


WHITE = (229, 240, 255)


def bright_mean(png, region, threshold=170):
    """the mean colour of a region's bright pixels (any channel over threshold) and their count"""
    w, h, rgb = harness.read_png(png)
    x0, y0, x1, y1 = region
    total, n = [0, 0, 0], 0
    for y in range(max(0, y0), min(y1, h)):
        for x in range(max(0, x0), min(x1, w)):
            i = (y * w + x) * 3
            p = rgb[i], rgb[i + 1], rgb[i + 2]
            if max(p) > threshold:
                for k in range(3):
                    total[k] += p[k]
                n += 1
    return (tuple(v / n for v in total) if n else None), n


def countdown_frames(pngs):
    """the countdown's pictures: black round a big digit in the middle (1920x1080)"""
    out = []
    for png in pngs:
        w, h, rgb = harness.read_png(png)
        sampled = [(x, y) for y in range(0, h, 10) for x in range(0, w, 10)]
        lit = sum(1 for x, y in sampled if max(rgb[(y * w + x) * 3:(y * w + x) * 3 + 3]) > 40)
        mean, n = bright_mean(png, (860, 380, 1060, 620))
        # (nearly all black but for the digit and its lines: not the menus, not play)
        if sampled and lit < 0.08 * len(sampled) and n > 400:
            out.append((png, mean))
    return out


def judge_countdown(frames, expected):
    if not frames:
        return ["no picture of the countdown"]
    png, mean = frames[-1]
    if any(abs(mean[k] - expected[k]) > 25 for k in range(3)):
        return [f"the countdown's digit is {tuple(round(v) for v in mean)} in {png.name}, not {expected}"]
    return []


def run_style(cfg, a, out):
    cases = {
        "countdown-team": {"saved_gametype": "AE TEAM SLY", "env": {"HALO_NETWORK_TEST_AUTO_BALANCE": "1"}},
        "countdown-ffa": {"saved_gametype": "AE FFA SLAY", "env": {}},
        "overlay": {"saved_gametype": "AE FFA SLAY", "env": {"HALO_PERFORMANCE": "fps", "HALO_MAX_FPS": "25",
                                                             "HALO_PERFORMANCE_POSITION": "top_left"}},
    }
    results = {}
    for name, case in cases.items():
        env = {"HALO_DEBUG_SCORE_STRIP_LOG": "1", **case["env"]}
        spec = harness.parse_spec({"name": "style-" + name, "build": a.build, "map": "prisoner",
                                   "saved_gametype": case["saved_gametype"], "start": 3, "window": "1920x1080",
                                   "exit_after": 30, "screenshots": 8, "env": env, "expect": {"ticks": 200}})
        r = harness.play(cfg, spec, out / spec["name"], None, a.keep_work, run_id=f"{out.name}-{name}")
        debug = out / spec["name"] / "debug.txt"
        text = debug.read_text(errors="replace") if debug.exists() else ""
        pngs = [out / spec["name"] / s for s in r.get("shots", [])]
        why = list(r.get("why") or [])
        if name.startswith("countdown"):
            lines = [l for l in strip_lines(text) if l["view"] == "0" and l.get("drawn") == "1"]
            expected = WHITE if name == "countdown-ffa" else TEAM_RGB.get(lines[0]["lc"] if lines else "", None)
            if expected is None:
                why.append("no team from the log")
            else:
                why += judge_countdown(countdown_frames(pngs), expected)
        else:
            reds = whites = 0
            for png in pngs[-3:]:
                w, h, rgb = harness.read_png(png)
                for y in range(0, 40):
                    for x in range(0, 260):
                        i = (y * w + x) * 3
                        p = rgb[i], rgb[i + 1], rgb[i + 2]
                        if p[0] > 130 and p[1] < 90 and p[2] < 90:
                            reds += 1
                        elif min(p) > 180:
                            whites += 1
            if reds < 30 or whites > reds // 4:
                why.append(f"the performance line is not red as a whole ({reds} red, {whites} white pixels)")
        status = "PASS" if r.get("status") == "PASS" and not why else "FAIL"
        harness.log(f"  style {name}: {status}" + (f" ({'; '.join(why)})" if why else ""))
        results[name] = {"status": status, "why": why}
    harness.write_json(out / "style.json", results)
    return results


def main(argv):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    harness.add_common_args(p)
    p.add_argument("what", choices=("modes", "pair", "digits", "states", "clock", "style"))
    p.add_argument("--base", help="clock: the pre-strip build to compare CLASSIC with")
    p.add_argument("--build", required=True)
    p.add_argument("--cases", default=DEFAULT_CASES)
    p.add_argument("--seconds", type=int, default=45)
    p.add_argument("--gametype", default="AE TEAM SLY", help="pair: the host's seeded gametype")
    p.add_argument("--maps", default="prisoner,homobox@ce", help="digits: the maps")
    p.add_argument("--parallel", default=None)
    a = p.parse_args(argv)
    if a.box:
        raise SystemExit("strip_checks: run it on the build machine itself, with --build a path or commit there")
    cfg = harness.load_config(a.config)
    out = harness.new_out_dir(cfg, "strip_checks", a.what, a.out_name)
    results = {"modes": run_modes, "pair": run_pair, "digits": run_digits, "states": run_states,
               "clock": run_clock, "style": run_style}[a.what](cfg, a, out)
    failed = [n for n, r in results.items() if r["status"] != "PASS"]
    harness.log(f"strip_checks {a.what}: {len(results) - len(failed)} passed, {len(failed)} failed; results in {out}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
