#!/usr/bin/env python3
"""shader_load_ab: a map loaded as a fresh game start loads it (init.txt's map_name, through the loading screen), played
for its first minute with bots, and what the first minute cost: frames over 33 ms and the longest frame (the game's own
debug.frame_hitches line), the time from the start of the load to the first frame, and the game's resident memory
(sampled each second: peak, and at the end). One game at a time, each in a save root of its own (a fresh one, so no
cache/shader_keys of an earlier run is read, unless --save-root keeps one), with no visible window (SDL's offscreen
driver on the GPU, as the benchmark runs).

    python3 tools/ae_test/shader_load_ab.py --build build/linux64 --data ~/halo-data --map bloodgulch --runs 3 \\
        --label off --env HALO_PREPARE_SHADERS=false --env HALO_SHADER_CACHE=false --env __GL_SHADER_DISK_CACHE=0

prints a line a run and the median of each number, and writes them to --out (default shader-load-ab/<label>.json). The
Run it on a machine with a GPU, never while playing.
"""
import argparse
import json
import os
import re
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent

NVIDIA_EGL = "/usr/share/glvnd/egl_vendor.d/10_nvidia.json"
HITCHES = re.compile(r"frame hitches: (\S+)'s first minute had (\d+) over (\d+) ms \(worst ([\d.]+) ms, (\d+) ms in all\)")
LOAD = re.compile(r"load: (\S+): ([\d.]+) s from the start of its load to its first frame")
SHADERS = re.compile(r"shaders: (\S+): (\d+) keys, (\d+) programs made")
LOADING_SCREEN = re.compile(r"shaders: \S+: making \d+ keys .*at the loading screen")


def rss_kb(pid):
    try:
        text = Path(f"/proc/{pid}/status").read_text()
    except OSError:
        return None
    match = re.search(r"VmRSS:\s+(\d+) kB", text)
    return int(match.group(1)) if match else None


def one_run(args, number, save_root):
    scratch = Path(tempfile.mkdtemp(prefix="shader-load-", dir=args.scratch))
    xvfb = None
    try:
        data = scratch / "data"
        data.mkdir()
        source = Path(args.data).expanduser()
        for entry in source.iterdir():
            if entry.name not in ("init.txt", "debug.txt", "logs"):
                (data / entry.name).symlink_to(entry)
        (data / "init.txt").write_text(f"map_name {args.map}\n")
        binary = scratch / "bin"
        binary.mkdir()
        shutil.copy2(Path(args.build) / "halo", binary / "halo")
        for name in ("brokers.txt", "matchmaking.txt", "CHANGELOG.md"):
            if (ROOT / name).exists():
                shutil.copy2(ROOT / name, binary / name)
        save = Path(save_root) if save_root else scratch / "save"
        save.mkdir(parents=True, exist_ok=True)
        env = dict(os.environ, HALO_SAVE_ROOT=str(save), HALO_DATA_ROOT=str(data), HALO_UPDATE_AUTO="false",
                   HALO_UPDATE_ANSWER="no", HALO_HIDDEN_WINDOW="1", HALO_DISPLAY_MODE="windowed",
                   HALO_WINDOW_SIZE=args.window, HALO_NET_ONLINE="false", HALO_NET_ADDRESS="127.0.0.200",
                   HALO_ARENA_MENUS="0", HALO_EXIT_AFTER=str(args.seconds), SDL_AUDIO_DRIVER="dummy",
                   SDL_GAMECONTROLLER_IGNORE_DEVICES="all", HALO_NO_VSYNC="1", HALO_MAX_FPS="-1",
                   HALO_FRAME_HITCHES="33", SDL_VIDEO_DRIVER="offscreen", HALO_BOTS=str(args.bots), HALO_SKIP_INTRO="true")
        for item in args.env:
            key, _, value = item.partition("=")
            env[key] = value
        if os.path.exists(NVIDIA_EGL):
            env.setdefault("__EGL_VENDOR_LIBRARY_FILENAMES", NVIDIA_EGL)
        log = scratch / "run.log"
        peak = last = 0
        with open(log, "w") as output:
            game = subprocess.Popen([str(binary / "halo")], cwd=str(binary), env=env, stdout=output,
                                    stderr=subprocess.STDOUT)
            deadline = time.time() + args.seconds + 90
            while game.poll() is None and time.time() < deadline:
                size = rss_kb(game.pid)
                if size:
                    peak, last = max(peak, size), size
                time.sleep(1.0)
            if game.poll() is None:
                game.kill()
                game.wait()
        text = log.read_text(errors="replace")
        for path in (binary / "debug.txt", save / "debug.txt"):
            if path.exists():
                text += path.read_text(errors="replace")
        result = {"run": number, "peak_rss_mb": round(peak / 1024), "end_rss_mb": round(last / 1024)}
        hitch = HITCHES.search(text)
        if hitch:
            result.update(over_33=int(hitch.group(2)), longest_ms=float(hitch.group(4)), stalled_ms=int(hitch.group(5)))
        load = LOAD.search(text)
        if load:
            result["load_s"] = float(load.group(2))
        result["loading_screen"] = bool(LOADING_SCREEN.search(text))
        prep = [m for m in SHADERS.finditer(text) if m.group(1) == args.map]
        if prep:
            result["keys"], result["programs"] = int(prep[-1].group(2)), int(prep[-1].group(3))
        if args.keep:
            if (binary / "debug.txt").exists():
                shutil.copy(binary / "debug.txt", Path(args.keep) / f"{args.label}-{number}.txt")
            shutil.copy(log, Path(args.keep) / f"{args.label}-{number}.out")
        return result
    finally:
        if xvfb:
            xvfb.terminate()
            xvfb.wait()
        shutil.rmtree(scratch, ignore_errors=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--build", required=True, help="the build folder (build/linux64)")
    parser.add_argument("--data", required=True, help="a game data root")
    parser.add_argument("--map", default="bloodgulch")
    parser.add_argument("--runs", type=int, default=3)
    parser.add_argument("--label", default="run")
    parser.add_argument("--bots", type=int, default=15)
    parser.add_argument("--seconds", type=int, default=80, help="the game's lifetime: the load and a minute of play")
    parser.add_argument("--window", default="1920x1080")
    parser.add_argument("--env", action="append", default=[], help="KEY=VALUE for the game (repeatable)")
    parser.add_argument("--save-root", help="one save root for every run (its cache/shader_keys kept), else a fresh one each")
    parser.add_argument("--scratch", help="where each run's folder goes (never /tmp: map caches are big)")
    parser.add_argument("--keep", help="copy each run's debug.txt here")
    parser.add_argument("--out", default="shader-load-ab")
    args = parser.parse_args()
    if args.keep:
        Path(args.keep).mkdir(parents=True, exist_ok=True)
    results = []
    for number in range(1, args.runs + 1):
        result = one_run(args, number, args.save_root)
        results.append(result)
        print(args.label, json.dumps(result), flush=True)
    summary = {}
    for key in ("over_33", "longest_ms", "stalled_ms", "load_s", "peak_rss_mb", "end_rss_mb"):
        values = [r[key] for r in results if key in r]
        if values:
            summary[key] = statistics.median(values)
    print(args.label, "median", json.dumps(summary), flush=True)
    Path(args.out).mkdir(parents=True, exist_ok=True)
    Path(args.out, f"{args.label}.json").write_text(json.dumps({"runs": results, "median": summary}, indent=1))


if __name__ == "__main__":
    main()
