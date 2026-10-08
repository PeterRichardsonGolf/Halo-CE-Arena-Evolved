#!/usr/bin/env python3
"""handshake: a host and a joining client from two builds, in one network namespace, compared.

    python3 tools/ae_test/handshake.py --ae <rev> --other stock [--other opence] [--seconds 90] --box

For each other build, two pairs: AE hosts and the other joins, then the other hosts and AE joins
(--one-way: AE hosts only). Blood Gulch, the network test's scripted kills and hits
(HALO_NETWORK_TEST_START=20, _SHOOT=3, _KILL=20); the client starts 12 s after the host.

Checked per pair, from both debug.txt files: the client joined (its "joining a host of network version"
line), both builds' versions and network versions, the Delta Peer agreement lines, refusals, asserts;
the players' positions at the same tick on host and client (median and 90th percentile distance) and
both machines' final kills/deaths. PASS: joined, two players seen on both, every player's median
distance under --max-median metres (default 0.5) and the same final kills/deaths, no asserts/exceptions.

A named build can carry its own environment in the config, e.g. a 32-bit stock OpenCE build:
"opence": {"dir": ".../build/linux", "env": {"LIBGL_ALWAYS_SOFTWARE": "1"}}.
"""
import argparse
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import harness  # noqa: E402

HOST = "127.0.0.200"
CLIENT = "127.0.0.201"


def pair_specs(host_build, client_build, seconds=90, join_delay=12, env=None, map_name="bloodgulch", mod="",
               saved_gametype=None, save_from=None):
    """the host's and the client's specs: both on the map (and the mod's maps); the host's gametype a saved one of
    its save root (save_from copied as it) when given"""
    common = {"HALO_NETWORK_TEST_START": "20", "HALO_NETWORK_TEST_SHOOT": "3", "HALO_NETWORK_TEST_KILL": "20"}
    common.update(env or {})
    host = {"name": "host", "build": host_build, "network_test": f"host:{map_name}",
            "address": HOST, "broadcast": CLIENT, "exit_after": seconds, "env": common, "mod": mod}
    if saved_gametype:
        host["saved_gametype"] = saved_gametype
    if save_from:
        host["save_from"] = save_from
    client = harness.parse_spec({"name": "client", "build": client_build, "network_test": "join",
                                 "address": CLIENT, "broadcast": HOST, "exit_after": seconds - join_delay,
                                 "delay": join_delay, "env": common, "mod": mod})
    return harness.parse_spec(host), client


def judge(host_r, client_r, tracks, max_median=0.5):
    why = []
    hd, cd = host_r.get("debug") or {}, client_r.get("debug") or {}
    if not cd.get("joined"):
        why.append("client never joined")
    for side, d in (("host", hd), ("client", cd)):
        if (d.get("players") or 0) < 2:
            why.append(f"{side} saw {d.get('players') or 0} players")
        if d.get("asserts") or d.get("exceptions"):
            why.append(f"{side}: {d.get('asserts')} asserts, {d.get('exceptions')} exceptions")
        if d.get("refusals"):
            why.append(f"{side}: {d.get('refusals')} refusals")
    if not tracks:
        why.append("no common players to compare")
    for p, t in tracks.items():
        if t["median_m"] is None or t["median_m"] > max_median:
            why.append(f"player {p}: median {t['median_m']} m")
        if t["host_kd"] != t["client_kd"]:
            why.append(f"player {p}: k/d host {t['host_kd']} client {t['client_kd']}")
    return ("FAIL" if why else "PASS"), why


def run_pair(cfg, name, host_build, client_build, out, a, slots):
    host, client = pair_specs(host_build, client_build, a.seconds, a.join_delay, map_name=a.map, mod=a.mod,
                              saved_gametype=a.saved_gametype, save_from=a.save_from)
    pdir = out / name
    work = harness.expand(cfg["work_dir"]) / f"{out.name}-{name}"
    harness.OWN_WORK.add(work)
    builds = [harness.resolve_build(cfg, host_build), harness.resolve_build(cfg, client_build)]
    prepared = [harness.prepare_game(cfg, s, b, work / s["name"], pdir / s["name"]) for s, b in zip((host, client), builds)]
    try:
        slots.acquire(2)
        try:
            inner = harness.run_group(cfg, prepared, None, work)
        finally:
            slots.release(2)
    except BaseException:  # (interrupted: leave no save roots behind)
        if not a.keep_work:
            import shutil
            shutil.rmtree(work, ignore_errors=True)
        raise
    results = [harness.collect_game(s, pr, i, pdir / s["name"], a.keep_work)
               for s, pr, i in zip((host, client), prepared, inner)]
    texts = [(pdir / s["name"] / "debug.txt").read_text(errors="replace") if (pdir / s["name"] / "debug.txt").exists()
             else "" for s in (host, client)]
    tracks = harness.compare_tracks(texts[0], texts[1])
    verdict, why = judge(results[0], results[1], tracks, a.max_median)
    if not a.keep_work:
        import shutil
        shutil.rmtree(work, ignore_errors=True)
    hd, cd = results[0]["debug"], results[1]["debug"]
    lines = [f"== {name}: {verdict}" + (f" ({'; '.join(why)})" if why else ""),
             f"   host   {builds[0]['label']}: {hd.get('version')}",
             f"   client {builds[1]['label']}: {cd.get('version')}",
             f"   network version host {hd.get('network_version')} client {cd.get('network_version')}; "
             f"joined: {cd.get('joined')}"]
    for side, d in (("host", hd), ("client", cd)):
        for l in d.get("delta", [])[:4]:
            lines.append(f"   {side} {l}")
    for p, t in tracks.items():
        lines.append(f"   player {p}: {t['samples']} samples, median {t['median_m']} m, 90th {t['p90_m']} m; "
                     f"k/d host {t['host_kd']} client {t['client_kd']}")
    data = {"name": name, "verdict": verdict, "why": why, "host": results[0], "client": results[1], "tracks": tracks,
            "builds": [b["label"] for b in builds]}
    harness.write_json(pdir / "pair.json", data)
    return data, "\n".join(lines)


def main(argv):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    harness.add_common_args(p)
    p.add_argument("--ae", help="our build (commit, named build or path)")
    p.add_argument("--other", action="append", help="a build to pair with (repeat; named builds like stock, opence)")
    p.add_argument("--one-way", action="store_true", help="only AE hosting")
    p.add_argument("--seconds", type=int, default=90)
    p.add_argument("--join-delay", type=int, default=12)
    p.add_argument("--max-median", type=float, default=0.5)
    p.add_argument("--map", default="bloodgulch", help="the map both play (default bloodgulch)")
    p.add_argument("--mod", default="", help="HALO_MOD for both (e.g. NHE: its maps)")
    p.add_argument("--saved-gametype", help="the host's gametype: a saved one of its save root, by stored name")
    p.add_argument("--save-from", help="a folder copied as the host's save root (never under /tmp)")
    a = p.parse_args(argv)
    cfg = harness.load_config(a.config)
    if a.box:
        rc, _ = harness.remote(cfg, "handshake", argv)
        return rc
    if not a.ae or not a.other:
        raise SystemExit("ae_test handshake: --ae and --other are needed")
    harness.apply_load_policy(cfg)
    out = harness.new_out_dir(cfg, "handshake", None, a.out_name)
    slots = harness.GameSlots(cfg, 2)
    texts, pairs = [], []
    t0 = time.time()
    others = [o for v in a.other for o in v.split(",")]
    for o in others:
        ol = harness.safe_name(o)[:12]
        plan = [(f"aehost-{ol}", a.ae, o)] + ([] if a.one_way else [(f"{ol}host-ae", o, a.ae)])
        for name, hb, cb in plan:
            data, text = run_pair(cfg, name, hb, cb, out, a, slots)
            pairs.append(data)
            texts.append(text)
            print(text, flush=True)
    ok = all(d["verdict"] == "PASS" for d in pairs)
    summary = "\n".join(texts) + f"\n\nHANDSHAKE {'PASS' if ok else 'FAIL'} ({time.time() - t0:.0f} s)"
    (out / "summary.txt").write_text(summary + "\n")
    harness.write_json(out / "summary.json", {"tool": "handshake", "pairs": pairs, "pass": ok})
    print(f"\nHANDSHAKE {'PASS' if ok else 'FAIL'}\nae_test: results in {out}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
