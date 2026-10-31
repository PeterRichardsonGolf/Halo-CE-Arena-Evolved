"""Map folders (queue 142; port/linux/game/ae_map_folders.c): the maps folder's folders hold maps as the maps folder
does, a map still known by its name alone.

With $AE_MAP_FOLDERS_BUILD (a linux64 build's folder) and $AE_MAP_FOLDERS_DATA (a data root with the stock maps,
cex_nitra, cex_gothra, cex_abyss, octagon and decidia in maps/, and Custom Edition's resource maps, homobox,
h2_blood_gulch and h2_relic in maps/ce, with maps/ce/layouts/h2_blood_gulch.txt: the player's own files, never the
repository's), each test makes data roots of links under $AE_MAP_FOLDERS_WORK (default ~/halo-test/mapfolders-work;
not /tmp, which is memory on the local machine):

- maps/Comp Variants/ (bloodgulch, a stock map, and octagon), maps/CE+ X/ (cex_nitra, cex_gothra, and cex_abyss, which
  is loose too), maps/_hidden/ (octagon_b: not a page), loose decidia and cex_abyss; maps/ce/H2CE/ (h2_blood_gulch
  with its layout beside it in H2CE/layouts, h2_relic), loose homobox;
- the scan logs the pages (STOCK, CE+ X, COMP VARIANTS, H2CE, MULTIPLAYER) and the duplicate once; hosting a folder map,
  the stock map in a folder and the Custom Edition folder map loads each from its folder; the layout beside the
  Custom Edition folder map applies;
- a guest that has cex_nitra loose joins a host that has it in its folder (one map, by its name)."""
import json
import os
import re
import shutil
import subprocess
import sys
import textwrap
from pathlib import Path

import pytest

BUILD = os.environ.get("AE_MAP_FOLDERS_BUILD", "")
DATA = os.environ.get("AE_MAP_FOLDERS_DATA", "")
WORK = os.environ.get("AE_MAP_FOLDERS_WORK", "~/halo-test/mapfolders-work")
STOCK = ("beavercreek", "sidewinder", "damnation", "ratrace", "prisoner", "hangemhigh", "chillout", "carousel",
         "boardingaction", "bloodgulch", "wizard", "putput", "longest")
HOST, GUEST = "127.0.0.191", "127.0.0.192"


def needs():
    if not BUILD or not DATA or not (Path(BUILD).expanduser() / "halo").is_file():
        pytest.skip("$AE_MAP_FOLDERS_BUILD and $AE_MAP_FOLDERS_DATA name no build and data root")
    data = Path(DATA).expanduser()
    wanted = [data / "maps" / f"{name}.map" for name in (*STOCK, "ui", "cex_nitra", "cex_gothra", "cex_abyss",
                                                         "octagon", "octagon_b", "decidia")]
    wanted += [data / "maps/ce" / f"{name}.map" for name in ("bitmaps", "sounds", "loc", "homobox", "h2_blood_gulch",
                                                             "h2_relic")]
    wanted.append(data / "maps/ce/layouts/h2_blood_gulch.txt")
    missing = [str(path) for path in wanted if not path.exists()]
    if missing:
        pytest.skip(f"$AE_MAP_FOLDERS_DATA lacks {missing[0]}")
    return data


def link(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    target.symlink_to(source.resolve())


def host_root(root, data):
    maps = root / "maps"
    for name in (*STOCK, "ui", "decidia", "cex_abyss"):
        if name != "bloodgulch":
            link(data / "maps" / f"{name}.map", maps / f"{name}.map")
    link(data / "maps/bloodgulch.map", maps / "Comp Variants/bloodgulch.map")
    link(data / "maps/octagon.map", maps / "Comp Variants/octagon.map")
    for name in ("cex_nitra", "cex_gothra", "cex_abyss"):
        link(data / "maps" / f"{name}.map", maps / "CE+ X" / f"{name}.map")
    link(data / "maps/octagon_b.map", maps / "_hidden/octagon_b.map")
    (maps / "previews").mkdir()
    for name in ("bitmaps", "sounds", "loc", "homobox"):
        link(data / "maps/ce" / f"{name}.map", maps / "ce" / f"{name}.map")
    for name in ("h2_blood_gulch", "h2_relic"):
        link(data / "maps/ce" / f"{name}.map", maps / "ce/H2CE" / f"{name}.map")
    (maps / "ce/H2CE/layouts").mkdir()
    shutil.copy(data / "maps/ce/layouts/h2_blood_gulch.txt", maps / "ce/H2CE/layouts/h2_blood_gulch.txt")
    (root / "save").mkdir()
    return root


def guest_root(root, data):
    for name in (*STOCK, "ui", "cex_nitra"):
        link(data / "maps" / f"{name}.map", root / "maps" / f"{name}.map")
    (root / "save").mkdir()
    return root


def game_env(root, address, peer, seconds, extra):
    env = {"SDL_AUDIO_DRIVER": "dummy", "HALO_ARENA_MENUS": "0", "SDL_GAMECONTROLLER_IGNORE_DEVICES": "all", "HALO_HIDDEN_WINDOW": "1",
           "HALO_NET_ONLINE": "false", "HALO_NET_ADDRESS": address, "HALO_NET_BROADCAST": peer,
           "HALO_UPDATE_AUTO": "false", "HALO_UPDATE_ANSWER": "no", "HALO_DATA_ROOT": str(root),
           "HALO_SAVE_ROOT": str(root / "save"), "HALO_EXIT_AFTER": str(seconds), "HALO_MCC_USE": "never",
           "HOME": str(root)}
    env.update(extra)
    return env


INNER = textwrap.dedent("""
    import json, os, subprocess, sys, time
    plan = json.load(open(sys.argv[1]))
    subprocess.run(["ip", "link", "set", "lo", "up"], capture_output=True)
    for address in plan["addresses"]:
        subprocess.run(["ip", "addr", "add", address + "/8", "dev", "lo"], capture_output=True)
    running = []
    for game in plan["games"]:
        time.sleep(game["delay"])
        env = dict(os.environ)
        for name in ("XDG_DATA_HOME", "HALO_MCC_PATH", "HALO_MENUS", "HALO_MOD"):
            env.pop(name, None)
        env.update(game["env"])
        log = open(game["log"], "wb")
        running.append(subprocess.Popen(game["command"], cwd=game["cwd"], env=env, stdout=log, stderr=log))
    deadline = time.time() + plan["seconds"] + 40
    for process in running:
        try:
            process.wait(timeout=max(1, deadline - time.time()))
        except subprocess.TimeoutExpired:
            process.kill()
""")


def play(work, games, seconds):
    """the games (data root, address, peer, delay, extra environment) on their own network; each root's debug.txt"""
    binary = str(Path(BUILD).expanduser() / "halo")
    xvfb = ["xvfb-run", "-a", "-s", "-screen 0 1280x720x24"] if shutil.which("xvfb-run") else []
    plan = {"addresses": [HOST, GUEST], "seconds": seconds, "games": []}
    for root, address, peer, delay, extra in games:
        plan["games"].append({"delay": delay, "cwd": str(root), "log": str(root / "halo.log"), "command": xvfb + [binary],
                              "env": game_env(root, address, peer, seconds - delay, extra)})
    (work / "plan.json").write_text(json.dumps(plan))
    (work / "inner.py").write_text(INNER)
    subprocess.run(["unshare", "-rnm", "--pid", "--fork", "--mount-proc", sys.executable, str(work / "inner.py"),
                    str(work / "plan.json")], check=False, timeout=seconds + 120)
    return [(root / "debug.txt").read_text(errors="replace") if (root / "debug.txt").exists() else ""
            for root, *_ in games]


@pytest.fixture
def work():
    path = Path(WORK).expanduser()
    assert "/tmp" not in str(path.resolve()).split(os.sep)[1:2], "AE_MAP_FOLDERS_WORK must not be under /tmp"
    shutil.rmtree(path, ignore_errors=True)
    path.mkdir(parents=True)
    yield path
    if not os.environ.get("AE_MAP_FOLDERS_KEEP"):
        shutil.rmtree(path, ignore_errors=True)


def host(work, data, level, seconds=30):
    root = host_root(work / "host", data)
    (log,) = play(work, [(root, HOST, HOST, 0, {"HALO_NETWORK_TEST": f"host:{level}",
                                                 "HALO_NETWORK_TEST_START": "3"})], seconds)
    assert "exiting after debug.exit_after" in log, log[-3000:]
    assert "EXCEPTION" not in log
    return log


def pages(log):
    """the pages of the last full scan logged, in order: [(name, count)]"""
    found = re.findall(r"\[maps\] page (.+?): (\d+) maps?$", log, re.M)
    last = max(index for index, (name, _) in enumerate(found) if name == "STOCK")
    return [(name, int(count)) for name, count in found[last:]]


def test_scan_pages_duplicate_and_a_folder_map_hosted(work):
    data = needs()
    log = host(work, data, "cex_nitra")
    assert pages(log) == [("STOCK", 13), ("CE+ X", 2), ("COMP VARIANTS", 1), ("H2CE", 2), ("MULTIPLAYER", 3)], \
        "\n".join(line for line in log.splitlines() if "[maps]" in line)
    assert log.count("CE+ X\\cex_abyss.map is another map named cex_abyss: the maps folder's is played") == 1
    assert "octagon_b" not in log
    assert "precaching map 'levels\\test\\cex_nitra\\cex_nitra'" in log
    assert re.search(r"network test: tick \d+ player 0:", log)


def test_stock_map_in_a_folder_hosted(work):
    data = needs()
    log = host(work, data, "bloodgulch")
    assert "precaching map 'levels\\test\\bloodgulch\\bloodgulch'" in log
    assert re.search(r"network test: tick \d+ player 0:", log)


def test_custom_edition_folder_map_and_its_layout(work):
    data = needs()
    log = host(work, data, "h2_blood_gulch@ce")
    assert "Custom Edition map h2_blood_gulch@ce: d:\\maps\\ce\\H2CE\\h2_blood_gulch.map" in log
    assert re.search(r"layout: d:\\maps\\ce\\H2CE\\layouts\\h2_blood_gulch\.txt \(", log), \
        "\n".join(line for line in log.splitlines() if "layout:" in line)
    assert re.search(r"network test: tick \d+ player 0:", log)


def test_guest_with_the_map_loose_joins_a_host_with_it_in_a_folder(work):
    data = needs()
    host_data = host_root(work / "host", data)
    guest_data = guest_root(work / "guest", data)
    host_log, guest_log = play(work, [
        (host_data, HOST, GUEST, 0, {"HALO_NETWORK_TEST": "host:cex_nitra", "HALO_NETWORK_TEST_START": "20"}),
        (guest_data, GUEST, HOST, 10, {"HALO_NETWORK_TEST": "join"})], 70)
    assert "precaching map 'levels\\test\\cex_nitra\\cex_nitra'" in guest_log, guest_log[-3000:]
    assert "map sharing: downloaded" not in guest_log
    assert not (guest_data / "maps_shared").exists() or not list((guest_data / "maps_shared").iterdir())
    # (the guest plays: its own player is player 1 on both machines)
    assert "| local 1 camera" in guest_log, guest_log[-3000:]
    assert re.search(r"network test: tick \d+ player 0: .* player 1: ", host_log), host_log[-3000:]
