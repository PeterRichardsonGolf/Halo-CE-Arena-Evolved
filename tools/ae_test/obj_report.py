#!/usr/bin/env python3
"""obj_report: how the objective modes went in a game, from the "ae obj:" lines of debug.txt
(debug.objective_log = events | snap | full; port/linux/game/ae_objective_log.c), as Markdown tables.

    python3 tools/ae_test/obj_report.py debug.txt [more debug.txt ...]
    python3 tools/ae_test/obj_report.py --check debug.txt     # every line has exactly its event's keys, in order

One file is one game. Tables, as far as the log level has the lines for them:
  * the game (map, mode, players, bots, the end and the score);
  * CTF: the flags' takes by where from, the captures, take -> capture seconds (median, p90), the flags' drops by cause, the
    returns by how, the time each flag spent carried / dropped / home (from the one-a-second snap);
  * oddball / tag / juggernaut: takes, drops by cause, seconds held per player (median, p90);
  * hill: seconds by state (neutral, held by red, held by blue, contested) and the hill's moves; territories: mean territories
    held by each team, the captures and the contested time;
  * bomb and search and destroy: rounds by winner and reason, plant and defuse times (seconds from the round's start / from the
    defuse's start), blast kills;
  * jobs: seconds per team, job and role (the ps lines: a bot's behaviour and the part it plays, people as "human"), the median
    and 90th percentile of the distance to the objective of each;
  * why not: the objective-blocked reasons (obj_blocked) and what forced a job change (job_change).

The format and every event's keys are ae_objective_log_rules.c's table (the one source; --check reads it from there). 30 ticks a
second.
"""
import argparse
import re
import statistics
import sys
from collections import Counter, defaultdict
from pathlib import Path

TICKS = 30.0
ROOT = Path(__file__).resolve().parent.parent.parent
RULES = ROOT / "port" / "linux" / "game" / "ae_objective_log_rules.c"
LINE = re.compile(r"ae obj: t=(-?\d+) host=(\d) mode=(\w+) ev=(\w+)(.*)$")
WORD = re.compile(r"(\w+)=(\S*)")
WHO = "p b team x y z"


def event_table(path=RULES):
    """{event: [keys]} out of the rules' C table."""
    table = {}
    for match in re.finditer(r'\{\s*"(\w+)",\s*((?:"[^"]*"|WHO|\s)+)\}', path.read_text()):
        keys = []
        for piece in re.findall(r'"([^"]*)"|(WHO)', match.group(2)):
            keys.append(WHO if piece[1] else piece[0])
        table[match.group(1)] = " ".join(keys).split()
    return table


def parse(path):
    """The events of one debug.txt: dicts with t, host, mode, ev and the keys (numbers where they are), in file order."""
    events = []
    for line in Path(path).read_text(errors="replace").splitlines():
        m = LINE.search(line)
        if not m:
            continue
        event = {"t": int(m.group(1)), "host": int(m.group(2)), "mode": m.group(3), "ev": m.group(4), "_keys": []}
        for key, value in WORD.findall(m.group(5)):
            event["_keys"].append(key)
            try:
                event[key] = float(value) if "." in value else int(value)
            except ValueError:
                event[key] = value
        events.append(event)
    return events


def check(path, table):
    """Lines that do not conform: [(line number, text, why)]."""
    bad = []
    for number, line in enumerate(Path(path).read_text(errors="replace").splitlines(), 1):
        m = LINE.search(line)
        if not m:
            continue
        keys = [key for key, _ in WORD.findall(m.group(5))]
        want = table.get(m.group(4))
        if want is None:
            bad.append((number, line, "unknown event"))
        elif keys != want:
            bad.append((number, line, f"keys {keys} want {want}"))
    return bad


def percentile(values, fraction):
    if not values:
        return None
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int(fraction * len(ordered)))]


def stat(values):
    if not values:
        return "-"
    return f"{statistics.median(values):.1f} / {percentile(values, 0.9):.1f}"


def table(headers, rows):
    out = ["| " + " | ".join(headers) + " |", "|" + "|".join("---" for _ in headers) + "|"]
    out += ["| " + " | ".join(str(c) for c in row) + " |" for row in rows]
    return "\n".join(out)


def first(events, name):
    return next((e for e in events if e["ev"] == name), None)


def section_game(events):
    start, end = first(events, "game_start"), first(events, "game_end")
    last = max((e["t"] for e in events), default=0)
    rows = [("mode", events[0]["mode"] if events else "-"), ("map", start["map"] if start else "-"),
            ("players / bots", f"{start['players']} / {start['bots']}" if start else "-"),
            ("length (s)", f"{last / TICKS:.0f}"),
            ("end", f"winner {end['winner']}, score {end['score_r']}-{end['score_b']}" if end else "not over")]
    snaps = [e for e in events if e["ev"] == "snap"]
    if snaps and not end:
        rows.append(("last score", f"{snaps[-1]['score_r']}-{snaps[-1]['score_b']}"))
    return "## Game\n\n" + table(["", ""], rows)


def section_ctf(events):
    takes = [e for e in events if e["ev"] == "flag_take"]
    caps = [e for e in events if e["ev"] == "flag_capture"]
    drops = [e for e in events if e["ev"] == "flag_drop"]
    returns = [e for e in events if e["ev"] == "flag_return"]
    if not (takes or caps or drops or returns):
        return ""
    out = ["## Flags", ""]
    out.append(table(["event", "flag 0", "flag 1"], [
        ("takes from the stand", *(sum(1 for e in takes if e["flag"] == f and e["from"] == "stand") for f in (0, 1))),
        ("takes from the ground", *(sum(1 for e in takes if e["flag"] == f and e["from"] == "dropped") for f in (0, 1))),
        ("captures", *(sum(1 for e in caps if e["flag"] == f) for f in (0, 1))),
        ("returns by touch", *(sum(1 for e in returns if e["flag"] == f and e["how"] == "touch") for f in (0, 1))),
        ("returns by time", *(sum(1 for e in returns if e["flag"] == f and e["how"] == "timeout") for f in (0, 1))),
    ]))
    out += ["", "take -> capture seconds (median / p90): " + stat([e["carry_s"] for e in caps if e["carry_s"] >= 0]),
            "carry distance, wu (median / p90): " + stat([e["dist"] for e in caps if e["dist"] >= 0]),
            "held seconds per drop (median / p90): " + stat([e["held_s"] for e in drops if e["held_s"] >= 0]),
            "drops by cause: " + ", ".join(f"{k} {v}" for k, v in sorted(Counter(e["why"] for e in drops).items()))]
    state = defaultdict(lambda: Counter())
    for e in events:
        if e["ev"] == "snap":
            for flag, word in ((0, e["o1"]), (1, e["o2"])):
                state[flag][str(word).split(":")[0]] += 1
    if state:
        out += ["", "seconds each flag spent (from the snap): " + "; ".join(
            f"flag {f}: " + ", ".join(f"{k} {v}" for k, v in sorted(c.items())) for f, c in sorted(state.items()))]
    return "\n".join(out)


def section_ball(events):
    takes = [e for e in events if e["ev"] == "ball_take"]
    drops = [e for e in events if e["ev"] == "ball_drop"]
    if not (takes or drops):
        return ""
    per = defaultdict(float)
    for e in drops:
        per[e["p"]] += e["held_s"]
    return "\n".join(["## Ball", "", f"takes {len(takes)} ({sum(1 for e in takes if e['prev'] >= 0)} from a holder's hands), "
        f"drops by cause: " + ", ".join(f"{k} {v}" for k, v in sorted(Counter(e['why'] for e in drops).items())),
        "held seconds per hold (median / p90): " + stat([e["held_s"] for e in drops]),
        "", table(["player", "seconds held"], [(p, f"{s:.0f}") for p, s in sorted(per.items(), key=lambda x: -x[1])])])


def section_hill(events):
    snaps = [e for e in events if e["ev"] == "snap" and e["mode"] == "koth"]
    moves = sum(1 for e in events if e["ev"] in ("hill_new", "hill_move"))
    terr = [e for e in events if e["ev"] == "snap" and e["mode"] == "terr"]
    out = []
    if snaps or moves:
        share = Counter(str(e["o2"]) for e in snaps)
        total = sum(share.values()) or 1
        out += ["## Hill", "", f"hill positions {moves}; seconds by state: " + ", ".join(
            f"{k} {v} ({100 * v // total}%)" for k, v in sorted(share.items()))]
    if terr or any(e["ev"].startswith("terr_") for e in events):
        held = [tuple(int(x) for x in str(e["o1"]).split("/")) for e in terr if str(e["o1"]).count("/") == 2]
        out += ["", "## Territories", ""]
        if held:
            out.append("mean held (red / blue / neutral): " + " / ".join(
                f"{statistics.mean(h[i] for h in held):.1f}" for i in range(3)))
        caps = [e for e in events if e["ev"] == "terr_capture"]
        out.append(f"captures {len(caps)} (by team: " + ", ".join(
            f"{k} {v}" for k, v in sorted(Counter(e['team'] for e in caps).items())) + ")")
        out.append("contested state changes: " + str(sum(1 for e in events if e["ev"] == "terr_state" and e["state"] == "contested")))
    return "\n".join(out)


def section_bomb(events):
    ev = [e for e in events if e["mode"] in ("bomb", "snd")]
    if not ev:
        return ""
    out = ["## Bomb / search and destroy", ""]
    ends = [e for e in ev if e["ev"] == "round_end"]
    out.append(table(["round", "winner", "why", "score"], [(e["round"], e["winner"], e["why"], f"{e['score_r']}-{e['score_b']}") for e in ends]))
    starts = {e["round"]: e["t"] for e in ev if e["ev"] == "round_start"}
    plants = [e for e in ev if e["ev"] == "plant_done"]
    times = []
    for plant in plants:
        began = max((t for r, t in starts.items() if t <= plant["t"]), default=None)
        if began is not None:
            times.append((plant["t"] - began) / TICKS)
    out += ["", f"plants {len(plants)}; seconds from the round's start (median / p90): " + stat(times)]
    starts_d = [e for e in ev if e["ev"] == "defuse_start"]
    dones = [e for e in ev if e["ev"] == "defuse_done"]
    cancels = [e for e in ev if e["ev"] == "defuse_cancel"]
    secs = []
    for done in dones:
        began = max((s["t"] for s in starts_d if s["p"] == done["p"] and s["t"] <= done["t"]), default=None)
        if began is not None:
            secs.append((done["t"] - began) / TICKS)
    out.append(f"defuses started {len(starts_d)}, done {len(dones)}, let go {len(cancels)}; seconds start -> done (median / p90): " + stat(secs))
    out.append("let-go seconds: " + stat([e["held_s"] for e in cancels]))
    out.append(f"blast kills {sum(1 for e in ev if e['ev'] == 'blast_kill')}, bomb takes {sum(1 for e in ev if e['ev'] == 'bomb_take')}, "
               "drops " + ", ".join(f"{k} {v}" for k, v in sorted(Counter(e['why'] for e in ev if e['ev'] == 'bomb_drop').items())))
    return "\n".join(out)


def section_jobs(events):
    rows = defaultdict(list)
    for e in events:
        if e["ev"] == "ps":
            rows[(e["team"], e["job"], e["role"])].append(e["d_obj"])
    if not rows:
        return ""
    return "\n".join(["## Jobs (one ps line is a second)", "", table(
        ["team", "job", "role", "seconds", "d_obj median / p90 (wu)"],
        [(t, j, r, len(d), stat([x for x in d if x >= 0])) for (t, j, r), d in sorted(rows.items(), key=lambda kv: (kv[0][0], -len(kv[1])))])])


def section_why(events):
    blocked = [e for e in events if e["ev"] == "obj_blocked"]
    changes = [e for e in events if e["ev"] == "job_change"]
    if not (blocked or changes):
        return ""
    out = ["## Why not the objective", ""]
    if blocked:
        out.append(table(["reason", "events", "jobs"], [(r, n, ", ".join(sorted({e['job'] for e in blocked if e['reason'] == r})))
                                                       for r, n in Counter(e["reason"] for e in blocked).most_common()]))
    if changes:
        out += ["", f"job changes {len(changes)}; forced: " + ", ".join(f"{k} {v}" for k, v in Counter(e["forced"] for e in changes).most_common()),
                "to: " + ", ".join(f"{k} {v}" for k, v in Counter(e["to"] for e in changes).most_common())]
    return "\n".join(out)


def report(path):
    events = parse(path)
    if not events:
        return f"# {path}\n\nno ae obj: lines (debug.objective_log is off?)\n"
    parts = [f"# {path}", section_game(events), section_ctf(events), section_ball(events), section_hill(events),
             section_bomb(events), section_jobs(events), section_why(events)]
    return "\n\n".join(p for p in parts if p) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("files", nargs="+")
    parser.add_argument("--check", action="store_true", help="every line has exactly its event's keys, in order")
    args = parser.parse_args(argv)
    if args.check:
        tbl = event_table()
        status = 0
        for path in args.files:
            bad = check(path, tbl)
            lines = sum(1 for e in parse(path))
            print(f"{path}: {lines} lines, {len(bad)} not conforming")
            for number, text, why in bad[:10]:
                print(f"  {number}: {why}: {text[:160]}")
            status |= 1 if bad else 0
        return status
    for path in args.files:
        print(report(path))
    return 0


if __name__ == "__main__":
    sys.exit(main())
