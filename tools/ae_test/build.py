#!/usr/bin/env python3
"""build: one commit built on the build box (or here), in its own worktree; prints the binaries' paths.

    python3 tools/ae_test/build.py --commit <rev> [--targets linux64|linux|server|server-x64|servers|all] --box
    python3 tools/ae_test/build.py --commit <rev>          (on the box itself, or any machine with the toolchain)

--box pushes the commit to the box (refs/ae-test/<sha12>), copies the harness there and builds there.
Each commit gets <builds_dir>/<sha12> (a detached worktree of the config's repo): configure.py with the
config's configure_args (--release, ccache), then ninja per target (`linux`, the 32-bit build, with the
32-bit PKG_CONFIG_PATH). A commit already built is reused (--rebuild rebuilds). The other tools build a
commit they are given on first use the same way.
"""
import argparse
import json
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import harness  # noqa: E402


def main(argv):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    harness.add_common_args(p)
    p.add_argument("--commit", help="the commit (any git revision)")
    p.add_argument("--targets", default="linux64", help="comma-separated: linux64 (default), linux, server, "
                                                        "server-x64, servers (both servers), all")
    p.add_argument("--jobs", type=int)
    p.add_argument("--rebuild", action="store_true")
    p.add_argument("--list", action="store_true", help="the commits built under builds_dir")
    p.add_argument("--remove", help="delete this built commit's worktree (sha12)")
    a = p.parse_args(argv)
    cfg = harness.load_config(a.config)
    if a.box:
        rc, _ = harness.remote(cfg, "build", argv, fetch=False)
        return rc
    builds = harness.expand(cfg["builds_dir"])
    if a.list:
        for d in sorted(builds.glob("*")) if builds.exists() else []:
            if d.is_dir():
                have = [t for t, rel in harness.TARGETS.items() if (d / rel).exists()]
                print(f"{d.name}  {' '.join(have) or '(not built)'}")
        return 0
    if a.remove:
        d = harness.removable_build(cfg, a.remove)
        if d.exists():
            harness.git(cfg["repo"], "worktree", "remove", "--force", str(d), check=False)
            shutil.rmtree(d, ignore_errors=True)
        harness.git(cfg["repo"], "worktree", "prune", check=False)
        print(f"removed {d}")
        return 0
    if not a.commit:
        raise SystemExit("ae_test build: --commit is needed")
    harness.apply_load_policy(cfg)
    targets = a.targets.split(",")
    out = harness.new_out_dir(cfg, "build", a.commit[:12], a.out_name)
    if not a.rebuild:
        sha = harness.resolve_commit(harness.expand(cfg["repo"]), a.commit)
        tree = builds / (sha or "-")[:12]
        expanded = harness.expand_targets(targets)
        if sha and all((tree / harness.TARGETS[t]).exists() and (tree / ".ae_test_configure").exists()
                       for t in expanded):
            result = {"commit": sha, "tree": str(tree), "reused": True,
                      "targets": {t: {"path": str(tree / harness.TARGETS[t])} for t in expanded}}
            harness.write_json(out / "build.json", result)
            print(json.dumps(result, indent=2))
            return 0
    result = harness.build_commit(cfg, a.commit, targets, rebuild=a.rebuild, jobs=a.jobs, log_dir=out)
    harness.write_json(out / "build.json", result)
    print(json.dumps(result, indent=2))
    for t, v in result["targets"].items():
        print(f"ae_test build {result['commit'][:12]} {t}: {v['path']} ({v['seconds']} s)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
