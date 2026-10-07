#!/usr/bin/env python3
"""run: one headless game from a declarative spec, its debug.txt counted, a JSON result.

    python3 tools/ae_test/run.py --build <rev|name|path> --map bloodgulch [--players 2] [--bots 3 --bot-args=--move]
        [--mod NHE] [--window 1920x1080] [--exit-after 40] [--shots 300] [--env HALO_MATCH_CLOCK=both ...]
        [--init 'map_name levels\\a10\\a10'] [--record 20] [--box]
    python3 tools/ae_test/run.py --spec game.json [--box]

A spec file is a JSON object with the keys in harness.SPEC_KEYS (--list-keys prints them). The result
(<out>/result.json, also printed) has the exit code, the debug.txt counts (asserts, exceptions,
refusals, lost scripts, last network-test tick, items), PNG screenshots and PASS/FAIL with reasons.
"""
import argparse
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import harness  # noqa: E402


def spec_from_args(a):
    data = {}
    if a.spec:
        data = json.loads(Path(a.spec).read_text())
    pairs = {"build": a.build, "map": a.map, "gametype": a.gametype, "flags": a.flags, "start": a.start,
             "local_players": a.players, "bots": a.bots, "mod": a.mod, "menus": a.menus, "window": a.window,
             "exit_after": a.exit_after, "screenshots": a.shots, "test_input": a.test_input, "record": a.record,
             "target": a.target, "name": a.name, "network_test": a.network_test}
    for k, v in pairs.items():
        if v is not None:
            data[k] = v
    if a.bot_args:
        data["bot_args"] = a.bot_args.split()
    if a.init:
        data["init"] = a.init
    if a.env:
        env = dict(data.get("env", {}))
        for kv in a.env:
            k, _, v = kv.partition("=")
            env[k] = v
        data["env"] = env
    if "name" not in data:
        data["name"] = harness.safe_name(data.get("map", "menu").replace("\\", "_").split("_")[-1]
                                         if "\\" in data.get("map", "") else data.get("map", "menu"))
    return harness.parse_spec(data)


def parser():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    harness.add_common_args(p)
    p.add_argument("--spec", help="a JSON spec file")
    p.add_argument("--build", help="named build, path or commit")
    p.add_argument("--target", help="linux64 (default) or linux")
    p.add_argument("--name")
    p.add_argument("--map")
    p.add_argument("--gametype")
    p.add_argument("--network-test", help="raw HALO_NETWORK_TEST")
    p.add_argument("--flags", type=int)
    p.add_argument("--start", type=float)
    p.add_argument("--players", type=int)
    p.add_argument("--bots", type=int)
    p.add_argument("--bot-args")
    p.add_argument("--mod")
    p.add_argument("--menus")
    p.add_argument("--window")
    p.add_argument("--exit-after", type=float)
    p.add_argument("--shots", type=int, help="a screenshot every N frames")
    p.add_argument("--init", action="append", help="an init.txt line (repeat)")
    p.add_argument("--test-input")
    p.add_argument("--record", type=int, help="seconds of MP4 (no-op until the game and ffmpeg support it)")
    p.add_argument("--env", action="append", help="NAME=value (repeat)")
    p.add_argument("--list-keys", action="store_true", help="print the spec keys")
    p.add_argument("--dry-run", action="store_true", help="print the spec and the game's environment")
    return p


def main(argv, tool="run"):
    a = parser().parse_args(argv)
    if a.list_keys:
        for k, v in harness.SPEC_KEYS.items():
            print(f"{k:14} {v}")
        return 0
    cfg = harness.load_config(a.config)
    if a.box:
        rc, _local = harness.remote(cfg, tool, argv)
        return rc
    spec = spec_from_args(a)
    if not spec.get("build"):
        raise SystemExit("ae_test run: --build (or 'build' in the spec) is needed")
    if a.dry_run:
        print(json.dumps({"spec": spec, "env": harness.spec_env(spec, "<root>", "<save>", "<shots>"),
                          "init": harness.spec_init(spec)}, indent=2))
        return 0
    harness.apply_load_policy(cfg)
    out = harness.new_out_dir(cfg, tool, spec["name"], a.out_name)
    slots = harness.GameSlots(cfg, 1)
    result = harness.play(cfg, spec, out / spec["name"], slots, a.keep_work, run_id=out.name)
    line = harness.one_line(spec["name"], result)
    (out / "summary.txt").write_text(line + "\n")
    harness.write_json(out / "summary.json", {"tool": tool, "results": {spec["name"]: result}})
    print(line)
    print(f"ae_test: results in {out}")
    return 0 if result.get("status") in ("PASS", "SKIP") else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
