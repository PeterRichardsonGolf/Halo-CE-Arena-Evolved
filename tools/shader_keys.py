#!/usr/bin/env python3
"""The stock maps' shader key list (port/assets/shaders/stock.keys): what
each map's draws' shaders are made from, which the game makes at the map's
loading screen (display.prepare_shaders; port/linux/src/d3d8_shader_prep.inl).

    python3 tools/shader_keys.py record --build build/linux64/halo --data ~/halo-data
    python3 tools/shader_keys.py merge OUT.keys FOLDER...
    python3 tools/shader_keys.py check port/assets/shaders/stock.keys

record runs the build on each stock map in turn (the Xbox maps in the data
root's maps/ folder, the menus' too), each in a save root of its own, with
debug.draw_every_shader: the map's first frame is drawn first through every
shader its tags hold (port/linux/game/render_enumerate.c), and each draw's key
goes into the save root's cache/shader_keys/<map>.keys, as in any game. Then
it merges the maps' lists into one (merge), each pixel shader key given once
for all the maps. The keys are the translators' input, so one run (OpenGL,
any machine) makes the list for every renderer and platform; compiled
shaders can't be shipped, each driver makes its own.

Maps come only from a data root given with --data, used through links in
a scratch data root; never copies. Saves and settings go to a scratch folder,
never the player's. On Linux without a display, an Xvfb is started (as the test harness does). The list's format is in
port/assets/shaders/README.md.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
STOCK = ROOT / "port" / "assets" / "shaders" / "stock.keys"
HEADER = "chupathingy shader keys 1"
CAMPAIGN = re.compile(r"[a-d]\d\d")
# the maps' files a stock install has that are not played
NOT_PLAYED = {"fallback_ui"}
# (after the map's first frame, drawn through every shader: a few seconds of
# play, its first effects)
SETTLE_SECONDS = 12


def read_keys(text, only_map=None):
    """{map or None: set of (vertex, flags, packed, pixel)} of a list: pixel
    is the pixel key's slots as a tuple of (slot, value) pairs."""
    lines = text.splitlines()
    if not lines or not lines[0].startswith(HEADER):
        raise ValueError("not a shader key list of this format")
    numbers, maps, current = {}, {}, None
    for number, line in enumerate(lines[1:], 2):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        fields = line.split()
        if fields[0] == "p":
            slots = []
            for field in fields[2:]:
                slot, value = field.split(":")
                if int(value, 16):
                    slots.append((int(slot), int(value, 16)))
            numbers[int(fields[1])] = tuple(sorted(slots))
        elif fields[0] == "map":
            current = " ".join(fields[1:]).lower()
        elif fields[0] == "k":
            if len(fields) != 5 or int(fields[4]) not in numbers:
                raise ValueError(f"line {number}: a key without its pixel key: {line}")
            maps.setdefault(current, set()).add((int(fields[1], 16), int(fields[2], 16), int(fields[3], 16),
                                                 numbers[int(fields[4])]))
        else:
            raise ValueError(f"line {number}: not a record: {line}")
    return maps


def write_keys(maps):
    """the list's text: every pixel key once, numbered, then each map's keys."""
    pixels = sorted({key[3] for keys in maps.values() for key in keys})
    number = {pixel: index for index, pixel in enumerate(pixels)}
    out = [HEADER,
           "# The stock maps' shader keys (tools/shader_keys.py; the format: port/assets/shaders/README.md).",
           "# Made by the game at each map's loading screen (port/linux/src/d3d8_shader_prep.inl)."]
    for pixel in pixels:
        out.append(f"p {number[pixel]} " + " ".join(f"{slot}:{value:x}" for slot, value in pixel))
    for name in sorted(maps):
        out.append(f"map {name}")
        for vertex, flags, packed, pixel in sorted(maps[name], key=lambda key: (key[0], key[1], key[2],
                                                                                 number[key[3]])):
            out.append(f"k {vertex:016x} {flags:x} {packed:x} {number[pixel]}")
    return "\n".join(out) + "\n"


def merge(output, folders):
    maps = {}
    for folder in folders:
        for path in sorted(Path(folder).glob("*.keys")):
            for name, keys in read_keys(path.read_text()).items():
                maps.setdefault(name or path.stem.lower(), set()).update(keys)
    text = write_keys(maps)
    Path(output).parent.mkdir(parents=True, exist_ok=True)
    Path(output).write_text(text)
    print(f"{output}: {len(maps)} maps, {sum(len(keys) for keys in maps.values())} keys, "
          f"{len({key[3] for keys in maps.values() for key in keys})} pixel keys, {len(text) // 1024} KB")


def check(path):
    maps = read_keys(Path(path).read_text())
    if None in maps:
        sys.exit(f"{path}: keys outside a map")
    print(f"{path}: {len(maps)} maps, {sum(len(keys) for keys in maps.values())} keys")


def stage(build, scratch):
    """the build in a folder of its own, as the test harness does (config.toml is written beside it): a copy of the
    executable and the text files the menus read, nothing linked."""
    folder = scratch / "build"
    folder.mkdir()
    shutil.copy2(build, folder / build.name)
    for name in ("brokers.txt", "matchmaking.txt", "CHANGELOG.md"):
        if (ROOT / name).exists():
            shutil.copy2(ROOT / name, folder / name)
    return folder / build.name


# the stock multiplayer maps (their files' names), and the menus' map: what the stock list is made over
STOCK_MAPS = ("ui", "beavercreek", "bloodgulch", "boardingaction", "carousel", "chillout", "damnation", "dangercanyon",
              "deathisland", "gephyrophobia", "hangemhigh", "icefields", "infinity", "longest", "prisoner", "putput",
              "ratrace", "sidewinder", "timberland", "wizard")
NVIDIA_EGL = "/usr/share/glvnd/egl_vendor.d/10_nvidia.json"


def record(args):
    """each map in a game of its own (debug.network_test hosts it without the menus, with no visible window:
    the build box's way, tools/ae_test/harness.py), in a save root of its own."""
    build = Path(args.build).resolve()
    source = Path(args.data).expanduser().resolve()
    if not (source / "maps").is_dir():
        sys.exit(f"{source} has no maps folder: --data is a game data root")
    scratch = Path(tempfile.mkdtemp(prefix="shader-keys-", dir=args.scratch))
    xvfb = None
    try:
        data = scratch / "data"
        data.mkdir()
        for entry in source.iterdir():
            if entry.name not in ("init.txt", "debug.txt", "logs"):
                (data / entry.name).symlink_to(entry)
        names = [name for name in (args.maps or STOCK_MAPS) if (data / "maps" / f"{name}.map").exists()]
        if not names:
            sys.exit(f"none of the stock maps are in {data / 'maps'}")
        missing = [name for name in (args.maps or STOCK_MAPS) if name not in names]
        if missing:
            print("  not in the data root: " + " ".join(missing))
        if not os.environ.get("DISPLAY") and not os.environ.get("WAYLAND_DISPLAY"):
            # (a game with no window of its own draws nothing: an X server of its own, as the test harness does)
            sys.path.insert(0, str(ROOT / "tools" / "ae_test"))
            import harness
            with open(scratch / "xvfb.log", "w") as xlog:
                xvfb, display = harness.start_xvfb("1280x720", xlog)
        staged = stage(build, scratch)
        lists = scratch / "lists"
        lists.mkdir()
        for name in names:
            save = scratch / f"save-{name}"
            save.mkdir()
            env = dict(os.environ, HALO_SAVE_ROOT=str(save), HALO_DATA_ROOT=str(data), HALO_UPDATE_AUTO="false",
                       HALO_UPDATE_ANSWER="no", HALO_HIDDEN_WINDOW="1", HALO_DISPLAY_MODE="windowed",
                       HALO_WINDOW_SIZE="1280x720", HALO_DRAW_EVERY_SHADER="1", HALO_PREPARE_SHADERS="true",
                       HALO_NET_ONLINE="false", HALO_NET_ADDRESS="127.0.0.200", HALO_ARENA_MENUS="0",
                       HALO_EXIT_AFTER=str(args.seconds), SDL_AUDIO_DRIVER="dummy",
                       SDL_GAMECONTROLLER_IGNORE_DEVICES="all", HALO_SHADER_CACHE="false")
            if name != "ui":
                env["HALO_NETWORK_TEST"] = f"host:{name}"
            if xvfb:
                env["DISPLAY"] = display
            if os.path.exists(NVIDIA_EGL):
                env.setdefault("__EGL_VENDOR_LIBRARY_FILENAMES", NVIDIA_EGL)
            log = save / "run.log"
            with open(log, "w") as output:
                game = subprocess.Popen([str(staged)], cwd=str(staged.parent), env=env, stdout=output,
                                        stderr=subprocess.STDOUT)
                try:
                    game.wait(args.seconds + 60)
                except subprocess.TimeoutExpired:
                    game.kill()
                    game.wait()
            if (staged.parent / "debug.txt").exists():
                shutil.copy(staged.parent / "debug.txt", save / "bin-debug.txt")
                (staged.parent / "debug.txt").unlink()
            if args.keep:
                shutil.copytree(save, Path(args.keep) / name, dirs_exist_ok=True)
            keys = save / "cache" / "shader_keys" / f"{name}.keys"
            debug_logs = (log, save / "debug.txt", save / "bin-debug.txt")
            drawn = any("drawn through every shader" in path.read_text(errors="replace")
                        for path in debug_logs if path.exists())
            if not keys.exists():
                print(f"  {name}: no keys (log: {log})")
                continue
            shutil.copy(keys, lists / keys.name)
            count = sum(1 for line in keys.read_text().splitlines() if line.startswith("k "))
            print(f"  {name}: {count} keys" + ("" if drawn or name == "ui" else " (its first frame was not drawn "
                                                                              "through every shader)"))
        merge(args.output, [lists])
    finally:
        if xvfb:
            xvfb.terminate()
            xvfb.wait()
        shutil.rmtree(scratch, ignore_errors=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    commands = parser.add_subparsers(dest="command", required=True)
    recording = commands.add_parser("record", help="run the build on each stock map and make the list")
    recording.add_argument("--build", required=True, help="the build's executable (build/linux64/halo)")
    recording.add_argument("--data", required=True, help="a game data root (its maps folder is linked, never copied)")
    recording.add_argument("--output", default=str(STOCK), help="the list to write (default: the stock list)")
    recording.add_argument("--maps", nargs="*", help="only these maps (their files' names)")
    recording.add_argument("--seconds", type=int, default=60, help="each map's run, from the start (default 60)")
    recording.add_argument("--scratch", help="where the scratch folder goes (default: the system's)")
    recording.add_argument("--keep", help="copy each map's save root (its logs) into this folder")
    merging = commands.add_parser("merge", help="merge maps' own lists (cache/shader_keys folders) into one")
    merging.add_argument("output")
    merging.add_argument("folders", nargs="+")
    checking = commands.add_parser("check", help="read a list, as the game would")
    checking.add_argument("path")
    args = parser.parse_args()
    if args.command == "record":
        record(args)
    elif args.command == "merge":
        merge(args.output, args.folders)
    else:
        check(args.path)


if __name__ == "__main__":
    main()
