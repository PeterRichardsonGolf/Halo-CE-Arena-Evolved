#!/usr/bin/env python3
"""viewmodel_fit.py: the DEV MODE `viewmodel place` numbers that put one weapon's first-person view where another's sits.

DEV MODE's `viewmodel probe` writes, for the weapon in hand, its pivot (the gun's root node) and every marker of its
first-person model in the camera's frame (forward, left, up; world units) to the log:

    viewmodel probe: ar_modern pivot 0.31200 -0.08000 -0.11000
    viewmodel probe: ar_modern marker "primary trigger" 0.71000 -0.07500 -0.06000

Probe the weapon to match (the target, e.g. the stock assault rifle) and the weapon to move (the source, e.g. the
combat assault rifle), each standing still with nothing placed on the source (`viewmodel clear` first). This finds the
turn, scale and move (the best fit over the markers both models have, by name) that carry the source's markers onto
the target's, and prints the source's `viewmodel place` line: right, up, forward (model units, a hundredth of a world
unit), yaw, pitch, roll (degrees), scale; the same numbers ae_view_model_place_frame turns back into the move.

usage: viewmodel_fit.py <log> --target <key> --source <key> [--no-scale] [--markers "a,b,c"]
       (the log holds both probes; the last probe of each key is used)
"""
import argparse
import math
import re
import sys

PROBE = re.compile(r'viewmodel probe: (.+?) (?:pivot|marker "([^"]*)") (-?[\d.]+) (-?[\d.]+) (-?[\d.]+)$')
UNIT = 0.01
TURN_MAXIMUM = 45.0
PLACE_MAXIMUM = 60.0


class Refused(Exception):
    """no fit: the reason"""


def read_probes(text):
    """{key: {"pivot": (f, l, u), "markers": {name: (f, l, u)}}}: each key's last probe (a probe starts at its pivot
    line)"""
    probes = {}
    for line in text.splitlines():
        found = PROBE.search(line)
        if not found:
            continue
        key, name, *numbers = found.groups()
        point = tuple(float(n) for n in numbers)
        if name is None:
            probes[key] = {"pivot": point, "markers": {}}
        elif key in probes:
            probes[key]["markers"][name] = point
    return probes


# ---------- small linear algebra (3x3, lists of rows)

def mat_mul(a, b):
    return [[sum(a[r][k] * b[k][c] for k in range(3)) for c in range(3)] for r in range(3)]


def mat_vec(a, v):
    return [sum(a[r][k] * v[k] for k in range(3)) for r in range(3)]


def transpose(a):
    return [[a[c][r] for c in range(3)] for r in range(3)]


def det(a):
    return (a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0]) +
            a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]))


def jacobi_eigen(sym):
    """eigenvalues and eigenvectors (columns) of a symmetric 3x3 matrix"""
    a = [row[:] for row in sym]
    v = [[1.0 if r == c else 0.0 for c in range(3)] for r in range(3)]
    for _ in range(100):
        p, q = max(((r, c) for r in range(3) for c in range(r + 1, 3)), key=lambda rc: abs(a[rc[0]][rc[1]]))
        if abs(a[p][q]) < 1e-15:
            break
        theta = (a[q][q] - a[p][p]) / (2.0 * a[p][q])
        t = (1.0 if theta >= 0 else -1.0) / (abs(theta) + math.sqrt(theta * theta + 1.0))
        c = 1.0 / math.sqrt(t * t + 1.0)
        s = t * c
        rot = [[1.0 if r == k else 0.0 for k in range(3)] for r in range(3)]
        rot[p][p] = rot[q][q] = c
        rot[p][q] = s
        rot[q][p] = -s
        a = mat_mul(transpose(rot), mat_mul(a, rot))
        v = mat_mul(v, rot)
    return [a[i][i] for i in range(3)], v


def svd3(m):
    """m = u * diag(s) * v^T for a 3x3 matrix, through the eigenvectors of m^T m"""
    values, v = jacobi_eigen(mat_mul(transpose(m), m))
    order = sorted(range(3), key=lambda i: -values[i])
    v = [[v[r][i] for i in order] for r in range(3)]
    s = [math.sqrt(max(values[i], 0.0)) for i in order]
    mv = mat_mul(m, v)
    u = [[0.0] * 3 for _ in range(3)]
    for i in range(3):
        if s[i] > 1e-12:
            for r in range(3):
                u[r][i] = mv[r][i] / s[i]
    # (a missing column: the cross of the others, so u is a rotation or a reflection)
    if s[2] <= 1e-12:
        a = [u[r][0] for r in range(3)]
        b = [u[r][1] for r in range(3)]
        c = [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
        for r in range(3):
            u[r][2] = c[r]
    return u, s, v


def similarity(source, target, allow_scale=True):
    """scale, rotation (rows), translation with target ~ scale * rotation * source + translation (least squares over the
    paired points: the classic SVD solution with the reflection taken out)"""
    n = len(source)
    mean_s = [sum(p[i] for p in source) / n for i in range(3)]
    mean_t = [sum(p[i] for p in target) / n for i in range(3)]
    cs = [[p[i] - mean_s[i] for i in range(3)] for p in source]
    ct = [[p[i] - mean_t[i] for i in range(3)] for p in target]
    var_s = sum(sum(x * x for x in p) for p in cs) / n
    cov = [[sum(ct[k][r] * cs[k][c] for k in range(n)) / n for c in range(3)] for r in range(3)]
    u, s, v = svd3(cov)
    d = [1.0, 1.0, -1.0 if det(u) * det(v) < 0 else 1.0]
    rotation = mat_mul(u, [[d[r] if r == c else 0.0 for c in range(3)] for r in range(3)])
    rotation = mat_mul(rotation, transpose(v))
    scale = sum(s[i] * d[i] for i in range(3)) / var_s if allow_scale and var_s > 0 else 1.0
    turned = mat_vec(rotation, mean_s)
    translation = [mean_t[i] - scale * turned[i] for i in range(3)]
    return scale, rotation, translation


def turn_angles(rotation):
    """yaw, pitch, roll (degrees) of a turn in the camera's frame, as ae_view_model_rules.c's local_turn builds it:
    its columns forward, left, up"""
    forward = [rotation[r][0] for r in range(3)]
    up = [rotation[r][2] for r in range(3)]
    yaw = math.atan2(forward[1], forward[0])
    pitch = math.asin(max(-1.0, min(1.0, forward[2])))
    cy, sy, cp, sp = math.cos(yaw), math.sin(yaw), math.cos(pitch), math.sin(pitch)
    plain_up = [-sp * cy, -sp * sy, cp]
    right = [forward[1] * plain_up[2] - forward[2] * plain_up[1], forward[2] * plain_up[0] - forward[0] * plain_up[2],
             forward[0] * plain_up[1] - forward[1] * plain_up[0]]
    roll = math.atan2(sum(up[i] * right[i] for i in range(3)), sum(up[i] * plain_up[i] for i in range(3)))
    return math.degrees(yaw), math.degrees(pitch), math.degrees(roll)


def local_turn(yaw, pitch, roll):
    """the turn (rows) for the angles: the same construction as ae_view_model_rules.c's local_turn"""
    yaw, pitch, roll = math.radians(yaw), math.radians(pitch), math.radians(roll)
    cy, sy, cp, sp, cr, sr = (math.cos(yaw), math.sin(yaw), math.cos(pitch), math.sin(pitch), math.cos(roll),
                              math.sin(roll))
    forward = [cp * cy, cp * sy, sp]
    up = [-sp * cy, -sp * sy, cp]
    right = [forward[1] * up[2] - forward[2] * up[1], forward[2] * up[0] - forward[0] * up[2],
             forward[0] * up[1] - forward[1] * up[0]]
    rolled = [up[i] * cr + right[i] * sr for i in range(3)]
    left = [rolled[1] * forward[2] - rolled[2] * forward[1], rolled[2] * forward[0] - rolled[0] * forward[2],
            rolled[0] * forward[1] - rolled[1] * forward[0]]
    return [[forward[r], left[r], rolled[r]] for r in range(3)]


def place_numbers(scale, rotation, translation, pivot):
    """the `viewmodel place` numbers: the engine turns and scales about the pivot, then moves; so the move is
    translation - pivot + scale * rotation * pivot, in the camera's frame (forward, left, up), as right, up, forward"""
    turned = mat_vec(rotation, pivot)
    move = [translation[i] - pivot[i] + scale * turned[i] for i in range(3)]
    yaw, pitch, roll = turn_angles(rotation)
    return [-move[1] / UNIT, move[2] / UNIT, move[0] / UNIT, yaw, pitch, roll, scale]


def fit(probes, target_key, source_key, allow_scale=True, only=None):
    """(the place numbers, the markers used, the largest miss in world units after the fit)"""
    for key in (target_key, source_key):
        if key not in probes:
            raise Refused("no probe of %s in the log" % key)
    target, source = probes[target_key], probes[source_key]
    names = sorted(set(target["markers"]) & set(source["markers"]))
    if only:
        names = [n for n in names if n in only]
    if len(names) < 3:
        raise Refused("%d markers both weapons have (%s): a fit needs 3, not in a line" % (len(names), ", ".join(names)))
    src = [source["markers"][n] for n in names]
    dst = [target["markers"][n] for n in names]
    scale, rotation, translation = similarity(src, dst, allow_scale)
    miss = max(math.dist([scale * x for x in mat_vec(rotation, s)], [d[i] - translation[i] for i in range(3)])
               for s, d in zip(src, dst))
    numbers = place_numbers(scale, rotation, translation, source["pivot"])
    for value in numbers[0:3]:
        if abs(value) > PLACE_MAXIMUM:
            raise Refused("the move needed (%.1f) is past what viewmodel place takes (%.0f)" % (value, PLACE_MAXIMUM))
    for value in numbers[3:6]:
        if abs(value) > TURN_MAXIMUM:
            raise Refused("the turn needed (%.1f degrees) is past what viewmodel place takes" % value)
    return numbers, names, miss


def main(arguments):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("log")
    parser.add_argument("--target", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--no-scale", action="store_true")
    parser.add_argument("--markers", help="only these markers, comma separated")
    options = parser.parse_args(arguments)
    with open(options.log, encoding="utf-8", errors="replace") as file:
        probes = read_probes(file.read())
    only = set(options.markers.split(",")) if options.markers else None
    try:
        numbers, names, miss = fit(probes, options.target, options.source, not options.no_scale, only)
    except Refused as refusal:
        print("viewmodel_fit: %s" % refusal, file=sys.stderr)
        return 1
    print("markers: %s" % ", ".join(names))
    print("largest miss after the fit: %.4f world units (%.2f model units)" % (miss, miss / UNIT))
    print("viewmodel place %s" % " ".join("%.3f" % n for n in numbers))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
