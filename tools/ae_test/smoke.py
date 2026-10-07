#!/usr/bin/env python3
"""smoke: the standard per-merge smoke set, N games at a time, a pass/fail table, optionally against a baseline.

    python3 tools/ae_test/smoke.py --build <rev> [--baseline <rev>] [--parallel auto|1|2|3] [--only stock_] --box
    python3 tools/ae_test/smoke.py --list

The set (SMOKE below): stock Blood Gulch, Damnation, Beaver Creek; campaign a10; the NHE mod's badcreek and
Blood Gulch; Custom Edition beavercreek_v2; the CE+ X mod's Xbox menu and one CE+ X multiplayer game.
A case whose map or mod is missing from the data folder is SKIP, not FAIL.

Verdicts: PASS; FAIL (exit code, no clean exit, asserts, exceptions, refusals, lost scripts, too few
network-test ticks, no scenario scripts in the campaign); with --baseline also the same cases on the
baseline build, and each FAIL whose every reason the baseline shares is PRE-EXISTING; a case that
passes but differs from the baseline in a count (scripts, items, corrected tags, ticks down by a
quarter) is listed under "differences". --baseline-from <summary.json> compares with an earlier run
instead of playing the baseline again.
"""
import argparse
import json
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import harness  # noqa: E402

SMOKE = [
    {"name": "stock_bloodgulch", "map": "bloodgulch", "mod": "", "exit_after": 35},
    {"name": "stock_damnation", "map": "damnation", "mod": "", "exit_after": 35},
    {"name": "stock_beavercreek", "map": "beavercreek", "mod": "", "exit_after": 35},
    {"name": "campaign_a10", "map": "levels\\a10\\a10", "mod": "", "exit_after": 45, "screenshots": 900},
    {"name": "nhe_badcreek", "map": "badcreek", "mod": "NHE", "exit_after": 40, "screenshots": 900},
    {"name": "nhe_bloodgulch", "map": "bloodgulch", "mod": "NHE", "exit_after": 40},
    {"name": "ce_beavercreek_v2", "map": "beavercreek_v2@ce", "mod": "", "exit_after": 45, "screenshots": 900},
    {"name": "cex_menu", "mod": "CE+ X", "menus": "xbox", "window": "1920x1080", "exit_after": 45,
     "screenshots": 300},
    {"name": "cex_mp", "map": "bloodgulch", "mod": "CE+ X", "exit_after": 90, "screenshots": 1200},
]

COUNTS = ("asserts", "exceptions", "refusals", "lost_scripts", "scenario_scripts", "items", "corrected")


def cases(only=None):
    out = []
    for c in SMOKE:
        if only and not any(c["name"].startswith(o) for o in only.split(",")):
            continue
        out.append(dict(c))
    return out


def compare(result, base):
    """the differences between a case's result and the baseline's: (verdict, notes)"""
    if base is None:
        return result.get("status"), []
    notes = []
    d, b = result.get("debug") or {}, base.get("debug") or {}
    for k in COUNTS:
        if d.get(k) != b.get(k):
            notes.append(f"{k} {b.get(k)} -> {d.get(k)}")
    if (b.get("tick") or 0) and (d.get("tick") or 0) < 0.75 * b["tick"]:
        notes.append(f"tick {b['tick']} -> {d.get('tick')}")
    if bool(d.get("clean_exit")) != bool(b.get("clean_exit")):
        notes.append(f"clean exit {b.get('clean_exit')} -> {d.get('clean_exit')}")
    status = result.get("status")
    if status == "FAIL" and base.get("status") == "FAIL":
        mine = {w.split(" ", 1)[-1] if w[0].isdigit() else w for w in result.get("why", [])}
        theirs = {w.split(" ", 1)[-1] if w[0].isdigit() else w for w in base.get("why", [])}
        if mine <= theirs:
            status = "PRE-EXISTING"
    return status, notes


def table(results, baseline=None):
    def v(x):
        return "-" if x is None else str(x)
    rows = []
    head = f"{'case':20} {'verdict':12} {'exit':>4} {'secs':>5} {'tick':>5} {'items':>5} {'scr':>4} " \
           f"{'ass':>3} {'exc':>3} {'ref':>3} {'lost':>4}  notes"
    rows.append(head)
    rows.append("-" * len(head))
    for name, r in results.items():
        d = r.get("debug") or {}
        verdict, notes = compare(r, (baseline or {}).get(name)) if baseline else (r.get("status"), [])
        why = r.get("why") or []
        n = "; ".join(why + (["vs baseline: " + ", ".join(notes)] if notes else []))
        rows.append(f"{name:20} {verdict or '?':12} {v(r.get('exit_code')):>4} {r.get('seconds') or 0:5.0f} "
                    f"{v(d.get('tick')):>5} {v(d.get('items')):>5} "
                    f"{v(d.get('scenario_scripts')):>4} {v(d.get('asserts')):>3} "
                    f"{v(d.get('exceptions')):>3} {v(d.get('refusals')):>3} "
                    f"{v(d.get('lost_scripts')):>4}  {n}")
    return "\n".join(rows)


def run_set(cfg, build, label, specs, out, parallel, keep_work):
    slots = harness.GameSlots(cfg, parallel)
    results = {}

    def one(c):
        c = dict(c, build=build)
        spec = harness.parse_spec(c)
        try:
            r = harness.play(cfg, spec, out / label / spec["name"], slots, keep_work, run_id=f"{out.name}-{label}")
        except Exception as e:  # (one broken case must not lose the table)
            r = {"name": spec["name"], "status": "FAIL", "why": [f"harness error: {type(e).__name__}: {e}"]}
        harness.log("  " + harness.one_line(f"{label}/{spec['name']}", r))
        return spec["name"], r

    with ThreadPoolExecutor(max_workers=parallel) as ex:
        for name, r in ex.map(one, specs):
            results[name] = r
    return results


def main(argv):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    harness.add_common_args(p)
    p.add_argument("--build", help="the build under test (commit, named build or path)")
    p.add_argument("--baseline", help="a build to compare with (played with the same cases)")
    p.add_argument("--baseline-from", help="compare with an earlier smoke run's summary.json instead")
    p.add_argument("--parallel", default=None, help="games at once: auto (3, or 1 when other games run here), or N")
    p.add_argument("--only", help="comma-separated case-name prefixes")
    p.add_argument("--list", action="store_true", help="print the smoke set")
    a = p.parse_args(argv)
    if a.list:
        for c in SMOKE:
            print(json.dumps(c))
        return 0
    cfg = harness.load_config(a.config)
    if a.box:
        rc, _local = harness.remote(cfg, "smoke", argv)
        return rc
    if not a.build:
        raise SystemExit("ae_test smoke: --build is needed")
    harness.apply_load_policy(cfg)
    parallel = harness.resolve_parallel(cfg, a.parallel)
    out = harness.new_out_dir(cfg, "smoke", None, a.out_name)
    specs = cases(a.only)
    t0 = time.time()
    builds = {"build": a.build}
    for which, rev in (("build", a.build), ("baseline", a.baseline)):
        if rev:
            builds[which] = harness.resolve_build(cfg, rev)["label"]  # (builds a commit on first use)
    harness.log(f"ae_test smoke: {len(specs)} cases, {parallel} at a time -> {out}")
    results = run_set(cfg, a.build, "build", specs, out, parallel, a.keep_work)
    baseline = None
    if a.baseline:
        baseline = run_set(cfg, a.baseline, "baseline", specs, out, parallel, a.keep_work)
    elif a.baseline_from:
        baseline = json.loads(Path(a.baseline_from).read_text())["results"]
    text = f"smoke {builds['build']}" + (f" vs baseline {builds.get('baseline') or a.baseline_from}" if baseline else "") + \
        f" ({parallel} at a time, {time.time() - t0:.0f} s)\n" + table(results, baseline)
    if a.baseline:
        text += "\n\nbaseline " + builds["baseline"] + "\n" + table(baseline)
    verdicts = [compare(r, (baseline or {}).get(n))[0] if baseline else r.get("status") for n, r in results.items()]
    ok = all(v in ("PASS", "SKIP", "PRE-EXISTING") for v in verdicts)
    text += f"\n\nSMOKE {'PASS' if ok else 'FAIL'}: " + ", ".join(f"{v} {verdicts.count(v)}" for v in sorted(set(verdicts)))
    (out / "summary.txt").write_text(text + "\n")
    harness.write_json(out / "summary.json", {"tool": "smoke", "build": builds["build"], "baseline": builds.get("baseline"),
                                              "parallel": parallel, "results": results, "baseline_results": baseline,
                                              "pass": ok})
    print(text)
    print(f"ae_test: results in {out}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
