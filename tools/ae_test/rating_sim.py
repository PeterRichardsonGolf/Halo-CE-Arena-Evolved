#!/usr/bin/env python3
"""Offline ladder simulation for the rating core (port/linux/game/ae_rating_rules.c).

A Python reimplementation of the rules the simulation needs (level table, hysteresis, a game's change, the default window, the
bot anchors and the roster draw); `selftest()` pins it to the same golden numbers as port/linux/tests/ae_rating_rules_test.c
so the two cannot drift unseen. A simulated player of fixed true skill plays N games against bots of the offline roster and the
script reports how fast his level climbs and where it stops.

Strength model (a free parameter, not a measurement): levels are strength units; the chance that A beats B is logistic in the
difference, `scale` levels per e-fold. A bot plays at its nearest anchor's strength (`anchors`: five tiers) or exactly at the
level it is rated (`smooth`: the PLUS of many tiers). Games: 1v1, 4v4 (team strength = mean, the difference widened by
sqrt(4)), ffa8 (Plackett-Luce placement).

Usage: rating_sim.py [--games 200] [--trials 400] [--scale 3] [--threshold-scale 1.0] [--format 4v4|1v1|ffa8] [--bots anchors|smooth]
"""
import argparse
import math
import random
import statistics

LEVELS = 50
POINTS_MAX = 0x3FFFFFFF
LATE = [18100, 18900, 20000, 21500, 23500, 26100, 29400, 33500, 38500, 44500]
ANCHORS = [5, 17, 30, 42, 48]  # internal levels; shown 6, 18, 31, 43, 49


def level_points(level, scale=1.0):
    if level < 8:
        v = [0, 100, 200, 400, 600, 900, 1200, 1600][level]
    elif level < 40:
        v = 2000 + 500 * (level - 8)
    else:
        v = LATE[level - 40]
    return int(v * scale)


def level_from_points(points, previous, scale=1.0):
    level = 0
    for i in range(1, LEVELS):
        if points < level_points(i, scale):
            if previous > 0 and level == previous - 1 and points > (level_points(level, scale) + level_points(level + 1, scale)) // 2:
                level = previous
            return level
        level += 1
    return level


def win_base(d):
    return 180 if d <= 119 else 20 if d >= 135 else 180 - 10 * (d - 119)


def loss_base(d):
    return -20 if d <= 119 else -180 if d >= 135 else -20 - 10 * (d - 119)


def win_weight(level):
    return 100 if level <= 40 else [95, 90, 85, 80, 70, 65, 60, 55, 50, 50][level - 41]


def loss_weight(level):
    return [0, 5, 10, 20, 35, 45, 55, 60, 65, 70, 75, 80, 85, 90, 95][level] if level < 15 else 100


def tdiv(a, b):
    """C division: toward zero."""
    q = abs(a) // abs(b)
    return q if (a >= 0) == (b > 0) else -q


def pair_change(own, other, own_place, other_place):
    d = max(0, min(254, own - other + 127))
    if own_place >= 0 and (other_place < 0 or own_place < other_place):
        return tdiv(win_base(d) * win_weight(own), 100)
    if other_place >= 0 and (own_place < 0 or other_place < own_place):
        return tdiv(loss_base(d) * loss_weight(own), 100)
    return 0


def game_change(own_level, own_place, opponents, points):
    """opponents: [(level, place)] on other teams."""
    if not opponents:
        return 0
    total = sum(pair_change(own_level, lv, own_place, pl) for lv, pl in opponents)
    total = tdiv(total, len(opponents))
    total = max(-points, min(POINTS_MAX - points, total))
    return max(-200, min(200, total))


def window_upper(level):
    step = 5 if level <= 5 else 6 if level <= 15 else 7 if level <= 25 else 8 if level <= 30 else 9 if level <= 35 else 10
    return min(level + step, 127)


def window_low(level):
    i = level
    while i > 0 and window_upper(i - 1) >= level:
        i -= 1
    return i


def bot_difficulty(level):
    best, dist = 0, 1000
    for d, a in enumerate(ANCHORS):
        if abs(level - a) < dist:
            best, dist = d, abs(level - a)
    return best


def draw_level(rng, player_level):
    low, high = window_low(player_level), min(window_upper(player_level), LEVELS - 1)
    far = max(player_level - low, high - player_level)
    levels = list(range(low, high + 1))
    weights = [far - abs(l - player_level) + 1 for l in levels]
    return rng.choices(levels, weights)[0]


def selftest():
    assert [level_points(i) for i in (0, 1, 3, 8, 39, 40, 49)] == [0, 100, 400, 2000, 17500, 18100, 44500]
    assert [level_from_points(p, 0) for p in (99, 100, 17499, 17500, 44499, 44500)] == [0, 1, 38, 39, 48, 49]
    assert level_from_points(2800, 10) == 10 and level_from_points(2750, 10) == 9 and level_from_points(2800, 0) == 9
    assert pair_change(20, 28, 0, 1) == 180 and pair_change(20, 20, 0, 1) == 100 and pair_change(20, 13, 0, 1) == 30
    assert pair_change(20, 20, 1, 0) == -100 and pair_change(5, 5, 1, 0) == -45 and pair_change(1, 8, 1, 0) == -1
    assert pair_change(49, 49, 0, 1) == 50 and pair_change(20, 20, -1, 3) == -100
    assert game_change(20, 1, [(20, 0), (12, 2), (28, 3)], 1000) == 33
    assert [window_upper(l) for l in (0, 5, 6, 15, 16, 26, 31, 36, 49)] == [5, 10, 12, 21, 23, 34, 40, 46, 59]
    assert [window_low(l) for l in (0, 6, 10, 20, 30, 49)] == [0, 1, 5, 14, 23, 39]
    assert [bot_difficulty(l) for l in (11, 12, 24, 37, 46)] == [0, 1, 2, 3, 4]


def play(rng, skill, fmt, bots, scale, tscale, games):
    points, level, curve = 0, 0, []
    for _ in range(games):
        rated = [draw_level(rng, level) for _ in range({"1v1": 1, "4v4": 7, "ffa8": 7}[fmt])]
        strength = [ANCHORS[bot_difficulty(r)] if bots == "anchors" else r for r in rated]
        if fmt == "1v1":
            p = 1 / (1 + math.exp(-(skill - strength[0]) / scale))
            win = rng.random() < p
            places = (0 if win else 1, 1 if win else 0)
            opp = [(rated[0], places[1])]
            own_place = places[0]
        elif fmt == "4v4":
            mine = [skill] + strength[:3]
            theirs = strength[3:]
            diff = statistics.mean(mine) - statistics.mean(theirs)
            win = rng.random() < 1 / (1 + math.exp(-diff * 2 / scale))
            own_place = 0 if win else 1
            opp = [(r, 1 if win else 0) for r in rated[3:]]
        else:
            perf = [s / scale - math.log(-math.log(rng.random())) for s in [skill] + strength]
            order = sorted(range(len(perf)), key=lambda i: -perf[i])
            place = {i: r for r, i in enumerate(order)}
            own_place = place[0]
            opp = [(rated[i - 1], place[i]) for i in range(1, len(perf))]
        points += game_change(level, own_place, opp, points)
        level = level_from_points(points, level, tscale)
        curve.append(level)
    return curve


def run(args):
    rng = random.Random(args.seed)
    marks = [25, 50, 100, 200]
    targets = [20, 30, 40, 45, 50]
    print(f"format {args.format}  bots {args.bots}  scale {args.scale}  threshold scale {args.threshold_scale}  games {args.games}  trials {args.trials}")
    print("true skill (shown) | mean level shown after " + ", ".join(str(m) for m in marks) + " games | median games to shown " +
          ", ".join(str(t) for t in targets) + " (reached %)")
    for skill_shown in args.skills:
        skill = skill_shown - 1
        curves = [play(rng, skill, args.format, args.bots, args.scale, args.threshold_scale, args.games) for _ in range(args.trials)]
        cells = []
        for m in marks:
            if m <= args.games:
                cells.append("%5.1f" % (statistics.mean(c[m - 1] for c in curves) + 1))
        reach = []
        for t in targets:
            hit = [next((i + 1 for i, lv in enumerate(c) if lv + 1 >= t), None) for c in curves]
            got = [h for h in hit if h is not None]
            reach.append(("%4d" % statistics.median(got) if got else "   -") + " (%3d%%)" % (100 * len(got) // len(curves)))
        final = statistics.mean(c[-1] for c in curves) + 1
        print(f"{skill_shown:>18} | {' '.join(cells)}  final {final:5.1f} | {'  '.join(reach)}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--games", type=int, default=200)
    ap.add_argument("--trials", type=int, default=300)
    ap.add_argument("--scale", type=float, default=3.0)
    ap.add_argument("--threshold-scale", type=float, default=1.0)
    ap.add_argument("--format", choices=["1v1", "4v4", "ffa8"], default="4v4")
    ap.add_argument("--bots", choices=["anchors", "smooth"], default="anchors")
    ap.add_argument("--skills", type=int, nargs="*", default=[10, 20, 30, 40, 45, 50])
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args()
    selftest()
    if args.selftest:
        print("selftest ok")
        return
    run(args)


if __name__ == "__main__":
    main()
