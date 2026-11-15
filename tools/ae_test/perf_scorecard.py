#!/usr/bin/env python3
"""perf_scorecard: frame times and memory of a build, measured the same way each time, and compared with a baseline.

    python3 tools/ae_test/perf_scorecard.py run --build build/linux64 --save new.json [--repeats 3] [--stages standard]
    python3 tools/ae_test/perf_scorecard.py soak --build build/linux64 --map bloodgulch --seconds 600 --save soak.json
    python3 tools/ae_test/perf_scorecard.py compare new.json --baseline base.json

run plays debug.benchmark (port/linux/game/benchmark.c: Blood Gulch, Damnation, Sidewinder and Chill Out, a local
Slayer game with bots, the view turning) through harness.py's play(), --repeats times, one game at a time, and reads
each map's line from debug.txt. Vsync is off and the frame rate unlimited, so the numbers are what the machine can do.
While a game runs, its memory is sampled every 2 s: the process's resident memory (/proc) and its graphics memory
(the DRM fdinfo counters, else nvidia-smi). soak plays one map for a long time and gives the resident memory's growth
in MB a minute over the second half.

compare fails (exit 1) when a map's median or 99th percentile frame time rises, or its 1 % low frame rate falls, by
more than the larger of 5 % and twice the repeats' spread (the median frame time's: the average frame rate is shown,
not judged, as it swings too much between repeats); when the peak resident or graphics memory grows by more than
10 %; or when a soak's growth is over 2 MB a minute. Bad input exits 2. Run it on a dedicated test machine, not
the one you play on.
"""
import argparse
import json
import os
import re
import statistics
import subprocess
import sys
import threading
import time
from pathlib import Path

BENCH_LINE = re.compile(r"benchmark: (\S+): ([\d.]+) frames a second on average, ([\d.]+) the 1% low, ([\d.]+) ms the "
                        r"median frame, ([\d.]+) the 99th percentile, ([\d.]+) the longest, (\d+) over 33 ms "
                        r"\((\d+) frames in ([\d.]+) s\)")
FIELDS = ("avg_fps", "low_fps", "median_ms", "p99_ms", "longest_ms", "over_33", "frames", "seconds")
# (higher is better for the frame rates, lower for the frame times). Not the average frame rate: unlimited at ~300 a
# second, a few very short frames swing it 20-40 % between repeats of the same build (measured), while the median
# frame time held within a few per cent
JUDGED = {"median_ms": -1, "p99_ms": -1, "low_fps": +1}
FRAME_TOLERANCE = 0.05
MEMORY_TOLERANCE = 0.10
SOAK_GROWTH_MB_MINUTE = 2.0
SAMPLE_SECONDS = 2.0
NVIDIA_EGL = "/usr/share/glvnd/egl_vendor.d/10_nvidia.json"


# ---------- reading

def parse_benchmark(text):
    """{map: {field: value}} from a debug.txt's benchmark lines (the last line of a map wins)"""
    stages = {}
    for match in BENCH_LINE.finditer(text):
        values = [float(v) for v in match.groups()[1:]]
        stages[match.group(1)] = dict(zip(FIELDS, values))
    return stages


def combine(runs):
    """the repeats of each map: the median of each field and the median frame time's spread (max - min over the
    median)"""
    maps = sorted({name for run in runs for name in run})
    out = {}
    for name in maps:
        seen = [run[name] for run in runs if name in run]
        entry = {field: statistics.median(s[field] for s in seen) for field in FIELDS}
        times = [s["median_ms"] for s in seen]
        entry["spread"] = ((max(times) - min(times)) / entry["median_ms"] if entry["median_ms"] > 0 and len(times) > 1
                           else 0.0)
        entry["repeats"] = len(seen)
        out[name] = entry
    return out


def growth_mb_minute(samples):
    """the resident memory's least-squares slope in MB a minute over the second half of [(seconds, MB), ...]"""
    if len(samples) < 4:
        return None
    half = samples[len(samples) // 2:]
    xs = [t for t, _ in half]
    ys = [m for _, m in half]
    mean_x, mean_y = statistics.fmean(xs), statistics.fmean(ys)
    den = sum((x - mean_x) ** 2 for x in xs)
    if den <= 0:
        return None
    return sum((x - mean_x) * (y - mean_y) for x, y in zip(xs, ys)) / den * 60.0


# ---------- judging

def compare(new, base):
    """[(level, text)]: FAIL lines for regressions past the tolerances, INFO lines for the rest"""
    lines = []
    for name, b in sorted(base.get("maps", {}).items()):
        n = new.get("maps", {}).get(name)
        if not n:
            lines.append(("FAIL", f"{name}: not measured (the baseline has it)"))
            continue
        tolerance = max(FRAME_TOLERANCE, 2 * max(n.get("spread", 0.0), b.get("spread", 0.0)))
        for field, sign in JUDGED.items():
            if b[field] <= 0:
                continue
            change = (n[field] - b[field]) / b[field]
            worse = -change * sign
            level = "FAIL" if worse > tolerance else "INFO"
            lines.append((level, f"{name}: {field} {b[field]:.2f} -> {n[field]:.2f} ({change:+.1%}, allowed "
                                 f"{tolerance:.0%} worse)"))
    for key in ("peak_rss_mb", "peak_gpu_mb"):
        b, n = base.get("memory", {}).get(key), new.get("memory", {}).get(key)
        if b and n:
            change = (n - b) / b
            level = "FAIL" if change > MEMORY_TOLERANCE else "INFO"
            lines.append((level, f"memory: {key} {b:.0f} -> {n:.0f} MB ({change:+.1%}, allowed "
                                 f"{MEMORY_TOLERANCE:.0%} more)"))
    growth = new.get("soak", {}).get("growth_mb_minute")
    if growth is not None:
        level = "FAIL" if growth > SOAK_GROWTH_MB_MINUTE else "INFO"
        lines.append((level, f"soak: resident memory grows {growth:.2f} MB a minute over the second half "
                             f"(allowed {SOAK_GROWTH_MB_MINUTE})"))
    return lines


# ---------- measuring (on the test machine)

def gpu_mb(pid):
    """the process's graphics memory in MB: DRM fdinfo's (drm-total-* or drm-resident-*, the first of each client),
    else nvidia-smi pmon's frame buffer figure; None when neither knows it"""
    total, seen, clients = 0.0, False, set()
    try:
        entries = list(Path(f"/proc/{pid}/fdinfo").iterdir())
    except OSError:
        entries = []
    for entry in entries:
        try:
            text = entry.read_text()
        except OSError:
            continue
        client = re.search(r"drm-client-id:\s*(\d+)", text)
        if not client or client.group(1) in clients:
            continue
        clients.add(client.group(1))
        found = re.search(r"drm-(?:total|resident)-\S+:\s*(\d+)\s*(\w*)", text)
        if found:
            total += int(found.group(1)) * {"KiB": 1 / 1024, "MiB": 1, "GiB": 1024}.get(found.group(2), 1 / 1048576)
            seen = True
    if seen:
        return total
    try:
        out = subprocess.run(["nvidia-smi", "pmon", "-c", "1", "-s", "m"], capture_output=True, text=True,
                             timeout=15).stdout
    except (OSError, subprocess.SubprocessError):
        return None
    for line in out.splitlines():
        parts = line.split()
        # (gpu, pid, type, fb MB, ccpm MB, name)
        if len(parts) >= 4 and parts[1] == str(pid) and parts[3].isdigit():
            return float(parts[3])
    return None


class Sampler(threading.Thread):
    """samples the memory of the processes whose program lies under a folder (the harness's work folder)"""

    def __init__(self, folder):
        super().__init__(daemon=True)
        self.folder = str(folder)
        self.samples = []
        self.gpu = []
        self.stop = threading.Event()
        self.start_time = time.monotonic()

    def pids(self):
        found = []
        for proc in Path("/proc").iterdir():
            if not proc.name.isdigit():
                continue
            try:
                exe = os.readlink(proc / "exe")
            except OSError:
                continue
            if exe.startswith(self.folder):
                found.append(int(proc.name))
        return found

    def run(self):
        while not self.stop.wait(SAMPLE_SECONDS):
            for pid in self.pids():
                try:
                    status = Path(f"/proc/{pid}/status").read_text()
                except OSError:
                    continue
                rss = re.search(r"VmRSS:\s*(\d+) kB", status)
                if rss:
                    self.samples.append((time.monotonic() - self.start_time, int(rss.group(1)) / 1024))
                graphics = gpu_mb(pid)
                if graphics is not None:
                    self.gpu.append(graphics)


def harness_module(root):
    import importlib.util
    sys.path.append(str(root / "tools/ae_test"))
    spec = importlib.util.spec_from_file_location("harness", root / "tools/ae_test/harness.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def play_one(h, cfg, build, name, out, stages, bots, window, extra_env, seconds):
    run_id = f"perf-{os.getpid()}-{int(time.time())}"
    env = {"HALO_BENCHMARK": stages, "HALO_BENCHMARK_BOTS": str(bots), "HALO_NO_VSYNC": "1", "HALO_MAX_FPS": "-1",
           "SDL_VIDEO_DRIVER": "offscreen"}
    if Path(NVIDIA_EGL).exists():
        env["__EGL_VENDOR_LIBRARY_FILENAMES"] = NVIDIA_EGL
    env.update(extra_env)
    spec = {"name": name, "build": str(build), "window": window, "exit_after": seconds, "env": env}
    sampler = Sampler(h.expand(cfg["work_dir"]) / run_id)
    sampler.start()
    try:
        # (the benchmark quits without the harness's clean exit line: its status is not the verdict, the lines are)
        h.play(cfg, spec, out, run_id=run_id)
    finally:
        sampler.stop.set()
        sampler.join()
    text = (out / "debug.txt").read_text(errors="replace") if (out / "debug.txt").exists() else ""
    return parse_benchmark(text), sampler


def stage_seconds(stages):
    if stages == "standard":
        return 4 * 15
    return sum(float(item.split(":")[1]) if ":" in item else 15 for item in stages.split(","))


def release_build(root):
    """whether the tree's last configure.py was a release one (a debug build's frame times mean nothing here)"""
    try:
        text = (root / "build.ninja").read_text(errors="replace")
    except OSError:
        return None
    match = re.search(r"^configure_args\s*=(.*)$", text, re.M)
    return match is not None and "--release" in match.group(1)


def measure(args, stages, repeats):
    root = Path(__file__).resolve().parents[2]
    # (the measured build's own tree: <tree>/build/linux64)
    if release_build(Path(args.build).expanduser().resolve().parents[1]) is False:
        print("perf_scorecard: WARNING: the build's tree is configured without --release: its frame times are a "
              "debug build's", file=sys.stderr)
    h = harness_module(root)
    cfg = h.load_config(args.config)
    out_root = Path(args.out).expanduser()
    runs, rss, gpu, samples = [], [], [], []
    extra = dict(item.split("=", 1) for item in args.env)
    for index in range(repeats):
        result, sampler = play_one(h, cfg, Path(args.build).expanduser().resolve(), f"perf-{index + 1}",
                                   out_root / f"run-{index + 1}", stages, args.bots, args.window, extra,
                                   int(stage_seconds(stages) * 3 + 120))
        if not result:
            print(f"run {index + 1}: no benchmark lines (see {out_root / f'run-{index + 1}'})", file=sys.stderr)
        runs.append(result)
        rss += [m for _, m in sampler.samples]
        gpu += sampler.gpu
        samples = sampler.samples
    scores = {"build": str(args.build), "stages": stages, "repeats": repeats, "maps": combine([r for r in runs if r]),
              "memory": {"peak_rss_mb": max(rss) if rss else None, "end_rss_mb": samples[-1][1] if samples else None,
                         "peak_gpu_mb": max(gpu) if gpu else None},
              "when": time.strftime("%Y-%m-%d %H:%M")}
    return scores, samples


# ---------- command line

def print_scores(scores):
    print(f"{'map':<12} {'avg fps':>8} {'1% low':>8} {'median':>8} {'p99 ms':>8} {'max ms':>8} {'>33':>5} {'spread':>7}")
    for name, s in sorted(scores.get("maps", {}).items()):
        print(f"{name:<12} {s['avg_fps']:>8.1f} {s['low_fps']:>8.1f} {s['median_ms']:>8.2f} {s['p99_ms']:>8.2f} "
              f"{s['longest_ms']:>8.2f} {int(s['over_33']):>5} {s['spread']:>7.1%}")
    memory = scores.get("memory", {})
    print("memory: " + ", ".join(f"{k} {v:.0f}" for k, v in memory.items() if v is not None))
    if "soak" in scores:
        print(f"soak: {scores['soak']}")


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="command", required=True)
    for name in ("run", "soak"):
        q = sub.add_parser(name)
        q.add_argument("--build", required=True, help="the build folder (build/linux64)")
        q.add_argument("--config", help="the harness's config (else AE_TEST_CONFIG or its default)")
        q.add_argument("--out", default="perf-out", help="where each game's debug.txt and results go")
        q.add_argument("--save", help="write the scores here (JSON)")
        q.add_argument("--bots", type=int, default=15)
        q.add_argument("--window", default="1920x1080")
        q.add_argument("--env", action="append", default=[], help="KEY=VALUE for the game (repeatable)")
        q.add_argument("--baseline", help="compare with these scores at the end")
        if name == "run":
            q.add_argument("--repeats", type=int, default=5)
            q.add_argument("--stages", default="standard", help="standard, or map:seconds,...")
        else:
            q.add_argument("--map", default="bloodgulch")
            q.add_argument("--seconds", type=int, default=600)
    q = sub.add_parser("compare")
    q.add_argument("scores")
    q.add_argument("--baseline", required=True)
    args = p.parse_args(argv)

    if args.command == "compare":
        try:
            new = json.loads(Path(args.scores).read_text())
            base = json.loads(Path(args.baseline).read_text())
        except (OSError, ValueError) as error:
            print(f"perf_scorecard: {error}", file=sys.stderr)
            return 2
    else:
        if any("=" not in item for item in args.env):
            print("perf_scorecard: --env takes KEY=VALUE", file=sys.stderr)
            return 2
        if args.command == "run":
            new, _ = measure(args, args.stages, args.repeats)
        else:
            new, samples = measure(args, f"{args.map}:{args.seconds}", 1)
            new["soak"] = {"map": args.map, "seconds": args.seconds, "growth_mb_minute": growth_mb_minute(samples),
                           "samples": len(samples)}
        if args.save:
            Path(args.save).write_text(json.dumps(new, indent=2) + "\n")
        print_scores(new)
        if not new["maps"]:
            return 2
        if not args.baseline:
            return 0
        base = json.loads(Path(args.baseline).read_text())
    lines = compare(new, base)
    for level, text in lines:
        print(f"{level} {text}")
    return 1 if any(level == "FAIL" for level, _ in lines) else 0


if __name__ == "__main__":
    sys.exit(main())
