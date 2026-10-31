#!/usr/bin/env python3
"""panel_checks: HUD STYLE AE's panels' headless checks (port/linux/game/ae_panel_*.c, ae_panels.c, ae_sensor.c).

    python3 tools/ae_test/panel_checks.py primitives --build <rev|path> [--maps prisoner,homobox@ce,bloodgulch]
    python3 tools/ae_test/panel_checks.py engine --build <rev|path> [--cases ar,plasma,hurt,powerups,seat]
    python3 tools/ae_test/panel_checks.py pair --build <rev|path>
    python3 tools/ae_test/panel_checks.py groups --build <rev|path> [--base <the pre-panels build>]
    python3 tools/ae_test/panel_checks.py sensor --build <rev|path>
    python3 tools/ae_test/panel_checks.py split --build <rev|path>

primitives: debug.ae_panel_primitives (HALO_DEBUG_AE_PANEL_PRIMITIVES=1) draws one of each primitive on a flat
grey plate in the first view (ae_panel_draw.c) and logs their places; on the screenshot: the box's outline is
1.5 +- 0.5 px with a DARK keyline on both sides and its corners cut round, the lit AR round is 23-24 px tall,
the plasma disc 20 +- 1 px with its dark ring, the dimmed frag keeps its keyline (G2), and every edge measured
goes from 10 % to 90 % in under 1.8 px (no blur). The HUD art must be found in the map (the log line).

engine: debug.ae_panel_log (HALO_DEBUG_AE_PANEL_LOG=1) logs every 2 s what the panels read for each local player
("ae panel: tick 600 view 0 name=... weapon=assault rifle kind=rounds loaded=42/60 reserve=180 ..."); one headless
host on Blood Gulch per case: ar (the scripted player fires, reloads and throws), plasma (heat rises, the battery
drains), hurt (the shield drops and recharges; health 0.40), powerups (debug.ae_panel_powerups: the overshield's
projection starts near 92 s and counts down, camo's ticks from 37 s; neither ever rises), seat (a vehicle seat).

pair: a host and a joined second game instance; the host logs its joiner's panels too ("player N"), and the
joiner's own lines must match the host's for it (weapon, rounds, reserve, grenades) within 2 lines (Review Focus 8).
"""
import argparse
import math
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import harness  # noqa: E402

PANEL_LINE = re.compile(r"ae panel: tick (?P<tick>-?\d+) (?P<who>view \d+|player \d+) name=(?P<name>\S*) "
                        r"weapon=(?P<weapon>.*?) kind=(?P<kind>\w+) loaded=(?P<loaded>-?\d+)/(?P<magazine>-?\d+) "
                        r"reserve=(?P<reserve>-?\d+) heat=(?P<heat>[\d.]+) battery=(?P<battery>-?[\d.]+) "
                        r"overheated=(?P<overheated>\d) low=(?P<low>\d) grenades=(?P<frags>\d+)/(?P<plasmas>\d+) "
                        r"armed=(?P<armed>\w+) shield=(?P<shield>-?[\d.]+) health=(?P<health>-?[\d.]+) "
                        r"camo=(?P<camo>-?\d+) overshield=(?P<overshield>[\d.]+) seat=(?P<seat>\S+)"
                        r"(?: origin=(?P<ox>[-\d.]+),(?P<oy>[-\d.]+) right=(?P<rx>[-\d.]+),(?P<ry>[-\d.]+) "
                        r"scale=(?P<scale>[\d.]+) placed=(?P<placed>\d))?")
PRIMITIVES_LOG = re.compile(r"ae panel primitives: .* art (?P<art>found|missing)")
PLATE = 102          # the plate's grey (0.40)


def lum(rgb, w, x, y):
    i = (y * w + x) * 3
    return (rgb[i] * 299 + rgb[i + 1] * 587 + rgb[i + 2] * 114) / 1000


def row_profile(rgb, w, y, x0, x1):
    return [lum(rgb, w, x, y) for x in range(x0, x1)]


def col_profile(rgb, w, x, y0, y1):
    return [lum(rgb, w, x, y) for y in range(y0, y1)]


def runs(profile, test):
    """[(start, length)] of the runs where test(value) holds"""
    out, start = [], None
    for i, v in enumerate(profile + [None]):
        if v is not None and test(v):
            start = i if start is None else start
        elif start is not None:
            out.append((start, i - start))
            start = None
    return out


def softness(profile):
    """the most pixels an edge takes from 10 % to 90 % of its step, over every rise from a DARK keyline (< 70) to
    a bright fill (> 150) within 5 px and every fall back"""
    worst = 0
    for seq in (profile, profile[::-1]):
        for i, v in enumerate(seq):
            if v >= 70:
                continue
            for j in range(i + 1, min(len(seq), i + 6)):
                if seq[j] > 150:
                    top = max(seq[j:j + 2])
                    a, b = v + 0.1 * (top - v), v + 0.9 * (top - v)
                    worst = max(worst, sum(1 for k in range(i + 1, j) if a < seq[k] < b))
                    break
    return worst


def judge_primitives(w, h, rgb, pc_art=False):
    why = []
    measures = {}
    # the box's left edge on its middle row (y 138): the line between two keylines
    p = row_profile(rgb, w, 138, 55, 90)
    bright = runs(p, lambda v: v > 150)
    if not bright:
        why.append("box: no line on its middle row")
    else:
        s, n = bright[0]
        measures["box_line"] = n
        if not 1 <= n <= 2:
            why.append(f"box: line {n} px wide, not 1.5 +- 0.5")
        left = p[max(0, s - 2):s]
        right = p[s + n:s + n + 2]
        if not left or min(left) > 70 or not right or min(right) > 70:
            why.append(f"box: no DARK keyline beside the line (left {left}, right {right})")
        measures["box_soft"] = softness(p)
    # the corners are round: the bottom-left corner is acute (63 degrees) at (60, 156); a 3 px radius cuts its tip by
    # 2.6 px along the bisector, so the pixels at its very tip are not line-bright (a sharp corner's are)
    corner = max(lum(rgb, w, x, 155) for x in (60, 61))
    edge = max(lum(rgb, w, x, 155) for x in range(90, 100))
    measures["corner"] = round(corner)
    if corner > 0.8 * edge:
        why.append(f"box: the corner is not round (corner {corner:.0f}, edge {edge:.0f})")
    # the lit AR round (260, 120, 23.6 tall): bright rows in its column band
    rows = [y for y in range(110, 152) if max(lum(rgb, w, x, y) for x in range(255, 285)) > 120]
    height = (max(rows) - min(rows) + 1) if rows else 0
    measures["round_height"] = height
    # (a Custom Edition map's rounds are Halo PC's own art, mapped onto the Xbox's by its ink box: a px or two
    # shorter)
    if not (21 if pc_art else 23) <= height <= 25:
        why.append(f"AR round {height} px tall, not 23-24 (its edge rows at half cover count)")
    # the plasma disc (340, 132, d 20): its width on the centre row and the dark ring inside
    p = row_profile(rgb, w, 132, 320, 361)
    lit = [i for i, v in enumerate(p) if v > 150]
    width = (lit[-1] - lit[0] + 1) if lit else 0
    measures["plasma_width"] = width
    if not 19 <= width <= 21:
        why.append(f"plasma disc {width} px wide, not 20 +- 1")
    ring = [p[i] for i in range(20 - 6, 20 - 3)] + [p[i] for i in range(20 + 3, 20 + 7)]
    if not ring or min(ring) > 0.75 * (max(p) if p else 255):
        why.append("plasma disc: no dark ring at 46 % of its radius")
    measures["plasma_soft"] = softness(p)
    # G2: the dimmed frag (450, 120, 24 tall, alpha 0.55) keeps its keyline: a DARK pixel beside its ink
    darks = sum(1 for y in range(118, 146) for x in range(446, 475) if lum(rgb, w, x, y) < 50)
    measures["frag_keyline_px"] = darks
    if darks < 20:
        why.append(f"dimmed frag: no keyline ({darks} dark px)")
    for key in ("box_soft", "plasma_soft"):
        if measures.get(key, 0) > 1:
            why.append(f"{key}: an edge takes {measures[key]} px from 10 % to 90 % (blur)")
    # the plate is there (the debug draw ran)
    if abs(lum(rgb, w, 40, 320) - PLATE) > 6:
        why.append(f"no grey plate (ground {lum(rgb, w, 40, 320):.0f})")
    return why, measures


def panel_lines(text, who="view 0"):
    """the debug.ae_panel_log lines (ae_panel_engine.c) of one view or player, in order, numbers as numbers"""
    out = []
    for m in PANEL_LINE.finditer(text):
        if who and m["who"] != who:
            continue
        line = m.groupdict()
        for key in ("tick", "loaded", "magazine", "reserve", "frags", "plasmas", "camo", "overheated", "low"):
            line[key] = int(line[key])
        for key in ("heat", "battery", "shield", "health", "overshield"):
            line[key] = float(line[key])
        out.append(line)
    return out


def never_rises(lines, key, jump=1.0):
    """the places a countdown rose (more than jump), other than its first appearance after zero"""
    bad = []
    for before, after in zip(lines, lines[1:]):
        if before[key] > 0 and after[key] > before[key] + jump:
            bad.append(f"{key} rose {before[key]} -> {after[key]} at tick {after['tick']}")
    return bad


# the engine cases: env for one headless host on Blood Gulch and the judge of its view 0 lines (why list)
def judge_ar(lines):
    ar = [l for l in lines if l["weapon"] == "assault rifle"]
    why = [] if ar else ["no line with the assault rifle"]
    if ar:
        if any(l["kind"] != "rounds" or l["magazine"] != 60 for l in ar):
            why.append("the assault rifle is not rounds of 60")
        if min(l["loaded"] for l in ar) >= 60:
            why.append("it never fired (loaded stayed 60)")
        falls = [i for i in range(1, len(ar)) if ar[i]["loaded"] < ar[i - 1]["loaded"]]
        rises = [i for i in range(1, len(ar)) if ar[i]["loaded"] > ar[i - 1]["loaded"] and
                 ar[i]["reserve"] < ar[i - 1]["reserve"]]
        if falls and not rises:
            why.append("no reload seen (loaded rising as the reserve drops)")
    if lines and lines[-1]["frags"] + lines[-1]["plasmas"] >= lines[0]["frags"] + lines[0]["plasmas"]:
        why.append("no grenade thrown (counts never dropped)")
    return why


def judge_plasma(lines):
    pr = [l for l in lines if l["weapon"] == "plasma rifle"]
    if not pr:
        return ["no line with the plasma rifle"]
    why = []
    if any(l["kind"] != "battery" for l in pr):
        why.append("the plasma rifle is not a battery weapon")
    if max(l["heat"] for l in pr) <= 0.0:
        why.append("its heat never rose")
    if min(l["battery"] for l in pr) >= 1.0:
        why.append("its battery never drained")
    return why


def judge_hurt(lines):
    """(every player's lines: the test host hurts its first player, a bot)"""
    why = []
    for name in sorted({l["name"] for l in lines}):
        own = [l for l in lines if l["name"] == name]
        low = [i for i, l in enumerate(own) if l["shield"] < 0.99]
        if low and any(l["shield"] >= 0.99 for l in own[low[0]:]) and any(abs(l["health"] - 0.40) < 0.02 for l in own):
            return []
    why.append("no player's shield dropped and recharged with health reading 0.40 (debug.network_test_damage, _hurt)")
    return why


def judge_powerups(lines):
    why = []
    over = [l for l in lines if l["overshield"] > 0]
    if not over:
        why.append("no overshield projection")
    else:
        if not 80.0 <= over[0]["overshield"] <= 93.0:
            why.append(f"the overshield started at {over[0]['overshield']} s, not about 92")
        if len(over) > 2 and over[-1]["overshield"] >= over[0]["overshield"]:
            why.append("the overshield did not count down")
        why += never_rises(over, "overshield", 0.5)
    camo = [l for l in lines if l["camo"] > 0]
    if not camo:
        why.append("no camo ticks")
    else:
        if not 1000 <= camo[0]["camo"] <= 1110:
            why.append(f"camo started at {camo[0]['camo']} ticks, not about 1110 (37 s)")
        why += never_rises(camo, "camo", 1)
    return why


def judge_seat(lines):
    return [] if any(l["seat"] != "none" for l in lines) else ["no line in a seat"]


ENGINE_CASES = {  # (env, judge, whose lines: "view 0" or None for every player's)
    "ar": ({"HALO_NETWORK_TEST_LOADOUT": "2,3", "HALO_TEST_INPUT": "bot:3"}, judge_ar, "view 0"),
    "plasma": ({"HALO_NETWORK_TEST_LOADOUT": "8,7", "HALO_TEST_INPUT": "bot:5"}, judge_plasma, "view 0"),
    "hurt": ({"HALO_NETWORK_TEST_DAMAGE": "10:1,11:1", "HALO_NETWORK_TEST_HURT": "24"}, judge_hurt, None),
    "powerups": ({"HALO_DEBUG_AE_PANEL_POWERUPS": "overshield:8,camo:20"}, judge_powerups, "view 0"),
    "seat": ({"HALO_NETWORK_TEST_VEHICLE": "10"}, judge_seat, None),
}


def run_engine(cfg, a, out):
    results = {}
    for case in a.cases.split(","):
        env, judge, whose = ENGINE_CASES[case]
        env = dict(env, HALO_DEBUG_AE_PANEL_LOG="1")
        name = "engine-" + case
        seconds = max(a.seconds, 80) if case == "ar" else a.seconds    # (the AR empties and reloads by 60 s)
        spec = harness.parse_spec({"name": name, "build": a.build, "map": "bloodgulch", "start": 3, "bots": 1,
                                   "window": "1920x1080", "exit_after": seconds, "env": env,
                                   "expect": {"ticks": 300}})
        r = harness.play(cfg, spec, out / name, None, a.keep_work, run_id=f"{out.name}-{name}")
        debug = out / name / "debug.txt"
        lines = panel_lines(debug.read_text(errors="replace") if debug.exists() else "", whose)
        why = list(r.get("why") or [])
        why += ["fewer than 5 'ae panel:' lines"] if len(lines) < 5 else judge(lines)
        status = "PASS" if r.get("status") == "PASS" and not why else "FAIL"
        harness.log(f"  {name}: {status}, {len(lines)} lines" + (f" ({'; '.join(why)})" if why else ""))
        results[name] = {"status": status, "why": why, "lines": len(lines)}
    harness.write_json(out / "engine.json", results)
    return results


def compare_players(host_text, client_text):
    """the joiner's view 0 lines against the host's lines for that player (by name) within 2 lines: weapon,
    rounds, reserve and grenades; camo and overshield never rising on either side"""
    client = panel_lines(client_text, "view 0")
    if not client:
        return ["the joiner logged no 'ae panel:' line"], 0
    name = client[0]["name"]
    host = [l for l in panel_lines(host_text, None) if l["who"].startswith("player") and l["name"] == name]
    if not host:
        return [f"the host logged no line for the joiner ({name})"], 0
    why, matched = [], 0
    keys = ("weapon", "loaded", "reserve", "frags", "plasmas")
    for line in client:
        near = sorted(host, key=lambda h: abs(h["tick"] - line["tick"]))[:3]
        if any(all(h[k] == line[k] for k in keys) for h in near):
            matched += 1
    if matched < 0.8 * len(client):
        why.append(f"only {matched} of {len(client)} joiner lines match the host's within 2 lines")
    for side in (client, host):
        why += never_rises([l for l in side if l["camo"] > 0], "camo", 1)
        why += never_rises([l for l in side if l["overshield"] > 0], "overshield", 0.5)
    return why, matched


def run_pair(cfg, a, out):
    import handshake  # noqa: E402 (the pair's specs)
    host, client = handshake.pair_specs(a.build, a.build, seconds=a.seconds + 30, join_delay=12,
                                        env={"HALO_DEBUG_AE_PANEL_LOG": "1", "HALO_NETWORK_TEST_AUTO_BALANCE": "1"},
                                        map_name="bloodgulch")
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
    host_text, client_text = [t.read_text(errors="replace") if t.exists() else "" for t in texts]
    why = [] if (results[1].get("debug") or {}).get("joined") else ["client never joined"]
    more, matched = compare_players(host_text, client_text)
    why += more
    status = "FAIL" if why else "PASS"
    harness.log(f"  pair: {status}, {matched} joiner lines matched" + (f" ({'; '.join(why[:5])})" if why else ""))
    result = {"status": status, "why": why, "matched": matched}
    harness.write_json(out / "pair.json", result)
    return {"pair": result}


LAYOUT_LINE = re.compile(
    r"ae panels: tick (?P<tick>\d+) view (?P<view>\d+) state=(?P<state>\S+) "
    r"number_box=(?P<nb>[-\d.,]+) grenade_box=(?P<gb>[-\d.,]+) baseline=(?P<baseline>[-\d.]+) cap=(?P<cap>[\d.]+) "
    r"rounds=(?P<rounds>\d+) round0=(?P<round0>[-\d.,]+) lit=(?P<lit>\d+) red=(?P<red>\d) bar=(?P<bar>[-\d.,]+) "
    r"shield_bar=(?P<sb>[-\d.,]+) icon=(?P<icon>[-\d.,]+) cell0=(?P<cells>[-\d.,]+) cross=(?P<cross>[-\d.,]+) "
    r"buffs=(?P<buffs>\d+) buff0=(?P<buff0>[-\d.,]+) shield_red=(?P<shield_red>\d) held=(?P<held>\d+)"
    r"(?: faded=(?P<faded>\d+))?")


def layout_lines(text, view="0"):
    out = []
    for m in LAYOUT_LINE.finditer(text):
        if m["view"] != view:
            continue
        line = m.groupdict()
        for key in ("nb", "gb", "round0", "bar", "sb", "icon", "cells", "cross", "buff0"):
            line[key] = [float(v) for v in line[key].split(",")]
        line["faded"] = line["faded"] or "0"
        line["baseline"], line["cap"] = float(line["baseline"]), float(line["cap"])
        for key in ("tick", "rounds", "lit", "red", "buffs", "shield_red", "held", "faded"):
            line[key] = int(line[key])
        out.append(line)
    return out


def rgb_at(rgb, w, x, y):
    i = (int(y) * w + int(x)) * 3
    return rgb[i], rgb[i + 1], rgb[i + 2]


def is_blue(c):
    return c[2] > 190 and 120 < c[0] < 215 and c[1] > 170 and c[2] - c[0] > 25


def is_red(c):
    return c[0] > 190 and c[1] < 130 and c[2] < 130


def is_green(c):
    return c[1] > 190 and c[0] < 190 and c[1] - c[2] > 40


def any_near(rgb, w, h, x, y, test, r=2):
    return any(test(rgb_at(rgb, w, xx, yy)) for xx in range(int(x) - r, int(x) + r + 1)
               for yy in range(int(y) - r, int(y) + r + 1) if 0 <= xx < w and 0 <= yy < h)


def relative_luminance(c):
    def ch(v):
        v /= 255.0
        return v / 12.92 if v <= 0.03928 else ((v + 0.055) / 1.055) ** 2.4
    return 0.2126 * ch(c[0]) + 0.7152 * ch(c[1]) + 0.0722 * ch(c[2])


def contrast(a, b):
    la, lb = sorted((relative_luminance(a), relative_luminance(b)), reverse=True)
    return (la + 0.05) / (lb + 0.05)


def ring_median(rgb, w, h, box, gap=6, width=6):
    """the median colour of a ring round a box (the local ground)"""
    x0, y0, x1, y1 = box
    px = []
    for y in range(int(y0) - gap - width, int(y1) + gap + width):
        for x in range(int(x0) - gap - width, int(x1) + gap + width):
            inside = x0 - gap <= x <= x1 + gap and y0 - gap <= y <= y1 + gap
            if not inside and 0 <= x < w and 0 <= y < h:
                px.append(rgb_at(rgb, w, x, y))
    px.sort(key=relative_luminance)
    return px[len(px) // 2] if px else (0, 0, 0)


def para_mid_left(box):
    """a leaning box's left edge x at its middle row (box = x0, y0, x1, y1 with the bottom edge x0..x1)"""
    x0, y0, x1, y1 = box
    return x0 + 0.46 * (y1 - y0) / 2


# the groups' cases: (map, env, judge(line, frames) -> why)
def g_base(line, frames):
    why = []
    if line["held"] <= 0:
        why.append("CE's own panels were not held back (held=0)")
    nb = line["nb"]
    if not any(any_near(rgb, w, h, para_mid_left(nb) + 0.7, (nb[1] + nb[3]) / 2, is_blue) for w, h, rgb in frames):
        why.append(f"no HUD-blue outline at the number box's left edge ({para_mid_left(nb):.0f}, {(nb[1] + nb[3]) / 2:.0f})")
    if line["rounds"]:
        x, y, hh = line["round0"]
        if not any(any_near(rgb, w, h, x + 0.3 * hh, y + hh / 2, lambda c: is_blue(c) or is_red(c), 2)
                   for w, h, rgb in frames):
            why.append("no round at the first round's place")
    sb = line["sb"]
    if not any(any_near(rgb, w, h, sb[0] + 30, (sb[1] + sb[3]) / 2, lambda c: is_blue(c) or is_green(c) or is_red(c), 2)
               for w, h, rgb in frames):
        why.append("no shield bar fill")
    if line["buffs"] and abs(line["buff0"][0] - sb[2]) > 1:
        why.append(f"the buff rows' right {line['buff0'][0]} is not the bar's {sb[2]}")
    return why


def g_low(line, frames):
    x, y, hh = line["round0"]
    reds = sum(1 for w, h, rgb in frames if any_near(rgb, w, h, x + 0.3 * hh, y + hh / 2, is_red, 2))
    blues = sum(1 for w, h, rgb in frames if any_near(rgb, w, h, x + 0.3 * hh, y + hh / 2, is_blue, 2))
    return [] if reds and blues else [f"low ammo: red on {reds} frames, blue on {blues} (want both)"]


def g_hot(line, frames):
    b = line["bar"]
    if not b[0]:
        return ["no heat bar"]
    return [] if any(any_near(rgb, w, h, b[1] + 40, (b[2] + b[4]) / 2, is_red, 2) for w, h, rgb in frames) else \
        ["the overheated bar is not red"]


def g_shield_down(line, frames):
    sb = line["sb"]
    return [] if any(any_near(rgb, w, h, sb[2] - 0.46 * (sb[3] - sb[1]) / 2 - 0.7, (sb[1] + sb[3]) / 2, is_red, 2)
                     for w, h, rgb in frames) else ["the empty shield's outline is never red"]


def g_overshield(line, frames):
    sb = line["sb"]
    return [] if any(any_near(rgb, w, h, sb[0] + 60, (sb[1] + sb[3]) / 2 + 3, is_green, 2) for w, h, rgb in frames) \
        else ["no green overshield fill"]


def g_camo(line, frames):
    if line["buffs"] < 1:
        return ["no own-buff row"]
    right, base, px = line["buff0"]
    white = lambda c: min(c) > 200
    return [] if any(any(white(rgb_at(rgb, w, x, y)) for x in range(int(right - 4 * px), int(right))
                         for y in range(int(base - 0.7 * px), int(base))) for w, h, rgb in frames) else \
        ["no white text in the own-buff row"]


def g_bright(line, frames):
    """Review Focus 9 on lit sand: the digits >= 4.5 and the lit rounds >= 3 against the local ground; the unarmed
    pair's keyline present (G2)"""
    why = []
    w, h, rgb = frames[-1]
    nb = line["nb"]
    ground = ring_median(rgb, w, h, (nb[0], nb[1], nb[2] + 0.46 * (nb[3] - nb[1]), nb[3]))
    digits = max((rgb_at(rgb, w, x, y) for x in range(int(nb[0]), int(nb[2])) for y in range(int(nb[1]), int(nb[3]))),
                 key=relative_luminance)
    c = contrast(digits, ground)
    if c < 4.5:
        why.append(f"digits {c:.2f}:1 against the ground {ground}")
    if line["rounds"]:
        x, y, hh = line["round0"]
        # (against the ground round the whole magazine block: next to a round is another round; the round's pixels
        # include its DARK keyline, which is what carries a HUD-blue round on pale ground: the fill alone measured
        # 2.6-2.7 against Blood Gulch's sky)
        block = (x - 0.46 * 2 * (hh + 4), y, nb[0] + 301 + 20, y + 3 * hh + 8)
        ground_rounds = ring_median(rgb, w, h, block, 6, 6)
        cr = max(contrast(rgb_at(rgb, w, xx, yy), ground_rounds) for xx in range(int(x) - 1, int(x + 0.6 * hh) + 1)
                 for yy in range(int(y) - 1, int(y + hh) + 1))
        if cr < 3.0:
            why.append(f"lit round {cr:.2f}:1")
    gb = line["gb"]
    darks = sum(1 for xx in range(int(gb[0] + 70), int(gb[2])) for yy in range(int(gb[1]), int(gb[3]))
                if relative_luminance(rgb_at(rgb, w, xx, yy)) < 0.01)
    if darks < 15:
        why.append(f"the unarmed pair has no keyline ({darks} dark px)")
    return why


GROUP_CASES = {
    "ar": ("prisoner", {"HALO_NETWORK_TEST_LOADOUT": "2,3"}, [g_base]),
    "low": ("prisoner", {"HALO_NETWORK_TEST_LOADOUT": "2,3", "HALO_DEBUG_AE_PANEL_STATE": "low"}, [g_base, g_low]),
    "hot": ("prisoner", {"HALO_NETWORK_TEST_LOADOUT": "8,7", "HALO_DEBUG_AE_PANEL_STATE": "overheated"},
            [g_base, g_hot]),
    "shield_down": ("prisoner", {"HALO_DEBUG_AE_PANEL_STATE": "shield_down"}, [g_shield_down]),
    "overshield": ("prisoner", {"HALO_DEBUG_AE_PANEL_STATE": "overshield1"}, [g_base, g_overshield]),
    "camo": ("prisoner", {"HALO_DEBUG_AE_PANEL_STATE": "camo"}, [g_base, g_camo]),
    "bright": ("bloodgulch", {"HALO_NETWORK_TEST_LOADOUT": "2,3"}, [g_base, g_bright]),
}


def run_groups(cfg, a, out):
    results = {}
    cases = a.cases if a.cases != ",".join(ENGINE_CASES) else ",".join(GROUP_CASES)
    for case in cases.split(","):
        map_name, env, judges = GROUP_CASES[case]
        name = "groups-" + case
        env = dict(env, HALO_DEBUG_AE_PANEL_LOG="1")
        spec = harness.parse_spec({"name": name, "build": a.build, "map": map_name, "start": 3, "bots": 1,
                                   "window": "1920x1080", "exit_after": 32, "screenshots": 25, "env": env,
                                   "expect": {"ticks": 300}})
        r = harness.play(cfg, spec, out / name, None, a.keep_work, run_id=f"{out.name}-{name}")
        debug = out / name / "debug.txt"
        lines = layout_lines(debug.read_text(errors="replace") if debug.exists() else "")
        why = list(r.get("why") or [])
        pngs = [out / name / s for s in r.get("shots", [])][2:]
        frames = [harness.read_png(p) for p in pngs]
        if not lines:
            why.append("no 'ae panels:' line")
        elif not frames:
            why.append("no screenshot")
        else:
            for judge in judges:
                why += judge(lines[-1], frames)
        status = "PASS" if r.get("status") == "PASS" and not why else "FAIL"
        harness.log(f"  {name}: {status}" + (f" ({'; '.join(why)})" if why else ""))
        results[name] = {"status": status, "why": why}
    harness.write_json(out / "groups.json", results)
    return results


SENSOR_LINE = re.compile(
    r"ae sensor: tick (?P<tick>\d+) view (?P<view>\d+) centre=(?P<cx>[-\d.]+),(?P<cy>[-\d.]+) r=(?P<r>[\d.]+) "
    r"inner=(?P<inner>[\d.]+) half=(?P<half>[\d.]+) vfov=(?P<vfov>[\d.]+) view=(?P<vw>\d+)x(?P<vh>\d+) "
    r"(?:blips=(?P<blips>\d+) )?label=(?P<label>\S+) baseline=(?P<baseline>[-\d.]+)")


def sensor_lines(text, view="0"):
    out = []
    for m in SENSOR_LINE.finditer(text):
        if m["view"] == view:
            line = {k: (v if k == "label" or v is None else float(v)) for k, v in m.groupdict().items()}
            out.append(line)
    return out


def wedge_formula(vfov, w, h):
    half = math.degrees(math.atan(math.tan(vfov / 2) * w / h))
    return max(5.0, min(85.0, half))


def s_base(line, frames):
    """the rim at r and the inner ring at 0.616 r (sampled at 45 degrees below right, clear of the dividers and the
    wedge), the wedge's edge at 0.8 r on the formula's angle, the label"""
    why = []
    cx, cy, r = line["cx"], line["cy"], line["r"]
    if abs(line["inner"] / r - 0.616) > 0.01:
        why.append(f"inner ring at {line['inner'] / r:.3f} r")
    want = wedge_formula(line["vfov"], line["vw"], line["vh"])
    if abs(line["half"] - want) > 1.0:
        why.append(f"wedge half-angle {line['half']} against the formula's {want:.1f}")
    c45 = math.cos(math.radians(45))
    bluish = lambda c: c[2] > 150 and c[2] >= c[0] + 15
    for name, rad in (("rim", r - 0.75), ("inner ring", line["inner"])):
        x, y = cx + rad * c45, cy + rad * c45
        if not any(any_near(rgb, w, h, x, y, bluish, 1) for w, h, rgb in frames):
            why.append(f"no {name} at ({x:.0f}, {y:.0f})")
    a = math.radians(-90 + line["half"])
    x, y = cx + 0.8 * r * math.cos(a), cy + 0.8 * r * math.sin(a)
    if not any(any_near(rgb, w, h, x, y, bluish, 2) for w, h, rgb in frames):
        why.append(f"no wedge edge at ({x:.0f}, {y:.0f})")
    if line["label"] != "15m":
        why.append(f"label {line['label']}")
    return why


def s_blips(lines, frames):
    """CE's colours (enemy red, friend yellow) inside the rim on some frame. Since 0.3.0 CE draws its own contacts
    over AE's disc and the line has no blips= count; an older build's line has it, and then CE must have handed some"""
    if all(l["blips"] is not None for l in lines) and not any(l["blips"] for l in lines):
        return ["CE handed over no blip"]
    line = lines[-1]
    cx, cy, r = line["cx"], line["cy"], line["r"]
    blip = lambda c: (c[0] > 180 and c[1] < 90 and c[2] < 90) or (c[0] > 180 and c[1] > 180 and c[2] < 90)
    for w, h, rgb in frames:
        for y in range(int(cy - r), int(cy + r), 2):
            for x in range(int(cx - r), int(cx + r), 2):
                if (x - cx) ** 2 + (y - cy) ** 2 < r * r and blip(rgb_at(rgb, w, x, y)):
                    return []
    return ["no blip in CE's colours inside the rim"]


def s_contrast(line, frames):
    """G3 on lit ground: across the inner ring and across the behind divider, the pixel that stands out most (its
    blue line or its DARK keyline) against the ground just beside, >= 1.6"""
    w, h, rgb = frames[-1]
    cx, cy, r, inner = line["cx"], line["cy"], line["r"], line["inner"]
    why = []
    c45 = math.cos(math.radians(45))
    ground = rgb_at(rgb, w, cx + (inner - 7) * c45, cy + (inner - 7) * c45)
    across = [rgb_at(rgb, w, cx + (inner + d) * c45, cy + (inner + d) * c45) for d in (-3, -2, -1, 0, 1, 2, 3)]
    best = max(contrast(c, ground) for c in across)
    if best < 1.6:
        why.append(f"inner ring {best:.2f}:1")
    mid = (inner + r) / 2
    beside = rgb_at(rgb, w, cx - 7, cy + mid)
    best = max(contrast(rgb_at(rgb, w, cx + d, cy + mid), beside) for d in (-2, -1, 0, 1, 2, 3))
    if best < 1.6:
        why.append(f"behind divider {best:.2f}:1")
    return why


ZOOM_INPUT = "menu:" + ",".join(["wait"] * 16 + ["key:Z"] + ["wait"] * 6 + ["key:Z"] + ["wait"] * 20)
SENSOR_CASES = {
    "fov70": ("prisoner", {"HALO_FOV": "70"}, [s_base]),
    "fov110": ("prisoner", {"HALO_FOV": "110"}, [s_base]),
    "scoped": ("prisoner", {"HALO_FOV": "70", "HALO_NETWORK_TEST_LOADOUT": "5,3", "HALO_TEST_INPUT": ZOOM_INPUT}, [s_base]),
    "bright": ("bloodgulch", {}, [s_base, s_contrast]),
}


def run_sensor(cfg, a, out):
    results = {}
    cases = a.cases if a.cases != ",".join(ENGINE_CASES) else ",".join(SENSOR_CASES)
    for case in cases.split(","):
        map_name, env, judges = SENSOR_CASES[case]
        name = "sensor-" + case
        env = dict(env, HALO_DEBUG_AE_PANEL_LOG="1")
        spec = harness.parse_spec({"name": name, "build": a.build, "map": map_name, "start": 3, "bots": 3,
                                   "bot_args": ["--move"], "window": "1920x1080", "exit_after": 36, "screenshots": 25,
                                   "env": env, "expect": {"ticks": 300}})
        r = harness.play(cfg, spec, out / name, None, a.keep_work, run_id=f"{out.name}-{name}")
        debug = out / name / "debug.txt"
        lines = sensor_lines(debug.read_text(errors="replace") if debug.exists() else "")
        why = list(r.get("why") or [])
        frames = [harness.read_png(p) for p in [out / name / s for s in r.get("shots", [])][2:]]
        info = {}
        if not lines:
            why.append("no 'ae sensor:' line")
        elif not frames:
            why.append("no screenshot")
        else:
            for judge in judges:
                why += judge(lines[-1], frames)
            if map_name == "prisoner":      # (Blood Gulch's bots are often out of range)
                why += s_blips(lines, frames)
            info = {"half": sorted({l["half"] for l in lines}), "vfov": sorted({l["vfov"] for l in lines}),
                    "r": lines[-1]["r"], "centre": (lines[-1]["cx"], lines[-1]["cy"])}
            if case == "scoped" and len(info["vfov"]) < 2:
                why.append("the camera's field of view never changed when zoomed")
        status = "PASS" if r.get("status") == "PASS" and not why else "FAIL"
        harness.log(f"  {name}: {status} {info}" + (f" ({'; '.join(why)})" if why else ""))
        results[name] = {"status": status, "why": why, "info": info}
    harness.write_json(out / "sensor.json", results)
    return results


def run_split(cfg, a, out):
    """2p and 4p (and a seat case per vehicle): every panel inside its view, the floors (cross >= 9 px, digits >= 12 px
    cap), the 4p AR block clear of the held scoreboard's moved header (its top at 137 of a 540 view) by >= 4 px"""
    results = {}
    cases = {
        "2p": ("prisoner", 2, {"HALO_NETWORK_TEST_LOADOUT": "2,3"}),
        "4p": ("prisoner", 4, {"HALO_NETWORK_TEST_LOADOUT": "2,3"}),
    }
    for case, (map_name, players, env) in cases.items():
        name = "split-" + case
        spec = harness.parse_spec({"name": name, "build": a.build, "map": map_name, "start": 10, "bots": 1,
                                   "local_players": players, "window": "1920x1080", "exit_after": 40,
                                   "screenshots": 30, "env": dict(env, HALO_DEBUG_AE_PANEL_LOG="1"),
                                   "expect": {"ticks": 300}})
        r = harness.play(cfg, spec, out / name, None, a.keep_work, run_id=f"{out.name}-{name}")
        debug = out / name / "debug.txt"
        text = debug.read_text(errors="replace") if debug.exists() else ""
        why = list(r.get("why") or [])
        views_seen = 0
        for view in range(players):
            lines = layout_lines(text, str(view))
            sensors = sensor_lines(text, str(view))
            if not lines or not sensors:
                why.append(f"view {view}: no 'ae panels:' / 'ae sensor:' line")
                continue
            views_seen += 1
            line, sensor = lines[-1], sensors[-1]
            vw, vh = sensor["vw"], sensor["vh"]
            boxes = [line["nb"], line["gb"], line["sb"], [line["cross"][0], line["cross"][1],
                     line["cross"][0] + line["cross"][2], line["cross"][1] + line["cross"][2]]]
            for box in boxes:
                if box[0] < -0.5 or box[1] < -0.5 or box[2] + 0.46 * (box[3] - box[1]) > vw + 0.5 or box[3] > vh + 0.5:
                    why.append(f"view {view}: {box} leaves the {vw:.0f}x{vh:.0f} view")
            if line["cross"][2] < 9:
                why.append(f"view {view}: cross {line['cross'][2]} px")
            if line["cap"] < 12:
                why.append(f"view {view}: digits {line['cap']} px cap")
            if sensor["cx"] - sensor["r"] < 0 or sensor["cy"] + sensor["r"] > vh:
                why.append(f"view {view}: the sensor leaves the view")
            if players == 4 and line["rounds"]:
                x, y, hh = line["round0"]
                bottom = y + 3 * hh + 2 * 4 * 0.75
                header_top = 137.0 * vh / 540.0
                if header_top - bottom < 4:
                    why.append(f"view {view}: the AR block ends at {bottom:.1f}, the header starts at {header_top:.1f}")
        status = "PASS" if r.get("status") == "PASS" and not why and views_seen == players else "FAIL"
        harness.log(f"  {name}: {status}, {views_seen} views" + (f" ({'; '.join(why[:6])})" if why else ""))
        results[name] = {"status": status, "why": why}
    # (the Ghost's and the Warthog chain gun's HUDs have no heat element and they never heat: no seat row, as classic;
    # HUD audit 10-26)
    seats = {"ghost": ("ghost:driver:6", "empty"), "warthog_gunner": ("warthog:gunner:6", "empty"),
             "scorpion": ("scorpion:driver:6", "empty"), "banshee": ("banshee:driver:6", "bolts")}
    for case, (setting, kind) in seats.items():
        name = "seat-" + case
        spec = harness.parse_spec({"name": name, "build": a.build, "map": "bloodgulch", "start": 3, "bots": 1,
                                   "saved_gametype": "AE PRACTICE", "window": "1920x1080", "exit_after": 26,
                                   "screenshots": 30,
                                   "env": {"HALO_DEBUG_AE_PANEL_LOG": "1", "HALO_DEBUG_AE_PANEL_SEAT": setting},
                                   "expect": {"ticks": 300}})
        r = harness.play(cfg, spec, out / name, None, a.keep_work, run_id=f"{out.name}-{name}")
        debug = out / name / "debug.txt"
        text = debug.read_text(errors="replace") if debug.exists() else ""
        why = list(r.get("why") or [])
        lines = panel_lines(text)
        vehicles = sorted(set(re.findall(r"ae panel: debug vehicle (.+)", text)))
        if "debug seated" not in text:
            status = "NOT SEEN"
            why.append(f"no such seat on the map (its vehicles: {', '.join(vehicles) or 'none'})")
        else:
            if not any(l["seat"].startswith(kind) for l in lines):
                why.append(f"no line with a {kind} seat row ({sorted(set(l['seat'] for l in lines))})")
            status = "PASS" if r.get("status") == "PASS" and not why else "FAIL"
        harness.log(f"  {name}: {status}" + (f" ({'; '.join(why)})" if why else ""))
        results[name] = {"status": status, "why": why}
    harness.write_json(out / "split.json", results)
    return results


def run_waypoints(cfg, a, out):
    """TRAINING's waypoint labels fade under the panels: the panels' log counts the labels faded (and the frames are
    kept to look at)"""
    name = "waypoints"
    spec = harness.parse_spec({"name": name, "build": a.build, "map": "prisoner", "start": 3, "bots": 2,
                               "bot_args": ["--move"], "saved_gametype": "AE TRAINING", "window": "1920x1080",
                               "exit_after": 45, "screenshots": 25, "test_input": "look:7",
                               "env": {"HALO_DEBUG_AE_PANEL_LOG": "1"}, "expect": {"ticks": 300}})
    r = harness.play(cfg, spec, out / name, None, a.keep_work, run_id=f"{out.name}-{name}")
    debug = out / name / "debug.txt"
    lines = layout_lines(debug.read_text(errors="replace") if debug.exists() else "")
    why = list(r.get("why") or [])
    faded = sum(l["faded"] for l in lines)
    if not faded:
        why.append("no waypoint label met a panel (nothing faded)")
    status = "PASS" if r.get("status") == "PASS" and not why else "FAIL"
    harness.log(f"  {name}: {status}, {faded} labels faded" + (f" ({'; '.join(why)})" if why else ""))
    result = {name: {"status": status, "why": why, "faded": faded}}
    harness.write_json(out / "waypoints.json", result)
    return result


def run_primitives(cfg, a, out):
    results = {}
    for map_name in a.maps.split(","):
        name = "primitives-" + harness.safe_name(map_name)
        spec = harness.parse_spec({"name": name, "build": a.build, "map": map_name, "start": 3, "bots": 0,
                                   "window": "1920x1080", "exit_after": 30, "screenshots": 120,
                                   "env": {"HALO_DEBUG_AE_PANEL_PRIMITIVES": "1"}, "expect": {"ticks": 200}})
        r = harness.play(cfg, spec, out / name, None, a.keep_work, run_id=f"{out.name}-{name}")
        debug = out / name / "debug.txt"
        text = debug.read_text(errors="replace") if debug.exists() else ""
        why = list(r.get("why") or [])
        found = list(PRIMITIVES_LOG.finditer(text))
        m = found[-1] if found else None       # (the last map's: the menus' map has the icons only)
        if not m:
            why.append("no 'ae panel primitives:' line")
        elif m["art"] != "found":
            why.append("the HUD art was not found in the map")
        pngs = [out / name / s for s in r.get("shots", [])][1:]
        measures = None
        if not pngs:
            why.append("no screenshot")
        else:
            w, h, rgb = harness.read_png(pngs[-1])
            more, measures = judge_primitives(w, h, rgb, pc_art=map_name.endswith("@ce"))
            why += more
        status = "PASS" if r.get("status") == "PASS" and not why else "FAIL"
        harness.log(f"  {name}: {status} {measures}" + (f" ({'; '.join(why)})" if why else ""))
        results[name] = {"status": status, "why": why, "measures": measures}
    harness.write_json(out / "primitives.json", results)
    return results


def main(argv):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    harness.add_common_args(p)
    p.add_argument("what", choices=("primitives", "engine", "pair", "groups", "sensor", "split", "waypoints"))
    p.add_argument("--cases", default=",".join(ENGINE_CASES), help="engine: the cases")
    p.add_argument("--seconds", type=int, default=50)
    p.add_argument("--build", required=True)
    p.add_argument("--maps", default="prisoner,homobox@ce,bloodgulch")
    a = p.parse_args(argv)
    if a.box:
        raise SystemExit("panel_checks: run it on the build machine itself, with --build a path or commit there")
    cfg = harness.load_config(a.config)
    out = harness.new_out_dir(cfg, "panel_checks", a.what, a.out_name)
    results = {"primitives": run_primitives, "engine": run_engine, "pair": run_pair,
               "groups": run_groups, "sensor": run_sensor, "split": run_split,
               "waypoints": run_waypoints}[a.what](cfg, a, out)
    failed = [n for n, r in results.items() if r["status"] not in ("PASS", "NOT SEEN")]
    harness.log(f"panel_checks {a.what}: {len(results) - len(failed)} passed, {len(failed)} failed; results in {out}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
