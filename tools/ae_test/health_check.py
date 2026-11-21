#!/usr/bin/env python3
"""health_check: the HALO 2 health timers asserted from a run's log.

    python3 tools/ae_test/health_check.py <debug.txt> [--tolerance 4]

The run is a hosted game with the gametype HEALTH = HALO 2 and the test hook
HALO_NETWORK_TEST_DAMAGE="seconds:scale,..." (network_test.c: the host's first player is hit with its
weapon's bullet at those game times; every change of its health, shields and shield stun is a log line
with its tick). The stand-in bots of tools/system_link_bots.py cannot aim (--fire only holds their
trigger), so the "enemy fire" is this engine hook: the host applies real weapon damage through
object_cause_damage, the same path a bullet takes.

Rules (source/objects/damage.c, HALO2_*): a hit on the shield stuns it for 5 s (150 ticks); when the stun is
over the shield recharges in 2 s (1/60 per tick); a hit while stunned starts the 5 s again, so the shield
never recharges under fire. The body recovers after 10 s (300 ticks) without damage, then in 5 s
(1/150 per tick).

Log lines read:
    network test: tick T hit xS: health H shield S stun K          (a scripted hit, state right after)
    network test: tick T health H shield S stun K                   (a change of the state)
"""
import argparse
import re
import sys
from pathlib import Path

SHIELD_STUN = 150
SHIELD_RECHARGE_TICKS = 60
BODY_QUIET = 300
BODY_RECHARGE_TICKS = 150

HIT = re.compile(r"network test: tick (\d+) hit x([\d.]+): health ([\d.-]+) shield ([\d.-]+) stun (-?\d+)")
CHANGE = re.compile(r"network test: tick (\d+) health ([\d.-]+) shield ([\d.-]+) stun (-?\d+)")


def parse(lines):
    """-> (hits [(tick, health, shield, stun)], states [(tick, health, shield, stun)]) in file order"""
    hits, states = [], []
    for line in lines:
        m = HIT.search(line)
        if m:
            t, _, h, s, k = m.groups()
            hits.append((int(t), float(h), float(s), int(k)))
            continue
        m = CHANGE.search(line)
        if m:
            t, h, s, k = m.groups()
            states.append((int(t), float(h), float(s), int(k)))
    return hits, states


def first_rise(states, key, after, before=None, above=None):
    """the first tick in (after, before) at which the value (1 health, 2 shield) rises over `above`"""
    for st in states:
        if st[0] <= after or (before is not None and st[0] >= before):
            continue
        if st[key] > above + 1e-4:
            return st[0]
    return None


def value_at(states, key, tick, default):
    """the value of a state key at a tick: the last logged change at or before it"""
    value = default
    for st in states:
        if st[0] > tick:
            break
        value = st[key]
    return value


def check(lines, tol=4, need_body=False):
    """-> [(id, ok, message)]"""
    hits, states = parse(lines)
    out = []
    if not hits:
        return [("hits", False, "no scripted hit in the log (HALO_NETWORK_TEST_DAMAGE set? a hosted game?)")]
    out.append(("hits", True, f"{len(hits)} scripted hits at ticks {[h[0] for h in hits]}"))
    # the stun a hit leaves: 5 s
    # (every hit that left the shield standing or took it down; a hit with no stun is a failure, not skipped)
    shield_hits = [h for h in hits if h[2] < 1.0 - 1e-4]
    out.append(("stun_set", bool(shield_hits) and all(abs(h[3] - SHIELD_STUN) <= 2 for h in shield_hits),
                f"shield stun right after the hits: {[h[3] for h in hits]} (want {SHIELD_STUN})"))
    # never under fire: no rise of the shield between a hit and its stun's end, nor before the next hit + 5 s
    bad = []
    for i, h in enumerate(hits):
        rise = first_rise(states, 2, h[0], h[0] + SHIELD_STUN - tol, above=h[2])
        if rise is not None:
            bad.append((h[0], rise))
    out.append(("no_recharge_under_fire", not bad,
                "the shield stays down for 5 s after every hit" if not bad else f"shield rose early after hits {bad}"))
    last = hits[-1]
    # shield back: 5 s after the last hit, then 2 s
    if last[2] < 1.0 - 1e-4:
        start = first_rise(states, 2, last[0], None, above=last[2])
        want = last[0] + SHIELD_STUN
        out.append(("shield_delay", start is not None and abs(start - want) <= tol + 1,
                    f"last shield hit at {last[0]}: recharge begins at {start} (want about {want})"))
        if start is not None:
            full = next((st[0] for st in states if st[0] > start and st[2] >= 1.0 - 1e-4), None)
            want_full = start + round((1.0 - last[2]) * SHIELD_RECHARGE_TICKS)
            out.append(("shield_full", full is not None and abs(full - want_full) <= tol + 2,
                        f"shield full at {full} (want about {want_full}: {(1.0 - last[2]) * 2:.2f} s after it began)"))
    # body: 10 s without damage, then 5 s. The last body damage is the last tick the health FELL (a scripted hit
    # that got through, or the hurt hook, HALO_NETWORK_TEST_HURT, which leaves 40%)
    fall = None
    prev = 1.0
    for st in states:
        if st[1] < prev - 1e-4:
            fall = st
        prev = st[1]
    if fall is None:
        out.append(("body", not need_body, "no body damage in the log" + (" (needed: use HALO_NETWORK_TEST_HURT)" if need_body else "")))
    else:
        damage = fall[0]
        begin = first_rise(states, 1, damage, None, above=fall[1])
        want = damage + BODY_QUIET
        out.append(("body_delay", begin is not None and abs(begin - want) <= tol + 1,
                    f"last body damage at {damage} (health {fall[1]:.2f}): recovery begins at {begin} (want about {want})"))
        if begin is not None:
            full = next((st[0] for st in states if st[0] > begin and st[1] >= 1.0 - 1e-4), None)
            want_full = begin + round((1.0 - fall[1]) * BODY_RECHARGE_TICKS)
            out.append(("body_full", full is not None and abs(full - want_full) <= tol + 3,
                        f"health full at {full} (want about {want_full})"))
    return out


def main(argv):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("log")
    p.add_argument("--tolerance", type=int, default=4, help="ticks")
    p.add_argument("--need-body", action="store_true", help="fail when the log has no body damage")
    a = p.parse_args(argv)
    results = check(Path(a.log).read_text(errors="replace").splitlines(), a.tolerance, a.need_body)
    for ident, ok, msg in results:
        print(f"{'PASS' if ok else 'FAIL'}  {ident}: {msg}")
    return 0 if all(ok for _, ok, _ in results) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
