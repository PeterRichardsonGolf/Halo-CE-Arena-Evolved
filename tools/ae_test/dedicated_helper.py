#!/usr/bin/env python3
"""A dedicated server (halo --server) as a harness game's helper (harness.py's "helper": started in the game's own
network namespace just before it, stopped after it), with an optional second client that joins it headless (a person
the player card can act on). Its folders under --work: server/ (its data root: maps linked, admins.txt) and goat/.

    dedicated_helper.py --game build/linux64/halo --maps assets/maps --work <folder> \\
        [--admins 'cidr:127.0.0.0/8<TAB>admin<TAB>0<TAB>0<TAB><TAB>'] [--rotation 'bloodgulch:ctf, prisoner:slayer'] \\
        [--goat] [--address 127.0.0.201] [--client 127.0.0.200]

Every process stops with this one (SIGTERM, or the harness's stop of the session)."""

import argparse
import os
import signal
import subprocess
import sys
import time
from pathlib import Path


def folder(work, name, maps):
    base = Path(work) / name
    for part in ("data", "saves", "home", "xdg"):
        (base / part).mkdir(parents=True, exist_ok=True)
    link = base / "data" / "maps"
    if not link.exists():
        link.symlink_to(Path(maps).resolve())
    return base


def environment(base, game, **settings):
    env = {key: value for key, value in os.environ.items() if not key.startswith("HALO_")}
    env.update({
        "HALO_DATA_ROOT": str(base / "data"), "HALO_SAVE_ROOT": str(base / "saves"), "HOME": str(base / "home"),
        "XDG_DATA_HOME": str(base / "xdg"), "SDL_VIDEO_DRIVER": "dummy", "SDL_AUDIO_DRIVER": "dummy",
        "HALO_UPDATE_AUTO": "false", "HALO_NET_ONLINE": "false", "HALO_DISCORD_APPLICATION": "",
        "HALO_NET_JOIN_FROM_CLIPBOARD": "0", "LD_LIBRARY_PATH": str(Path(game).resolve().parent),
    })
    env.update({key: str(value) for key, value in settings.items()})
    return env


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--game", required=True)
    parser.add_argument("--maps", required=True)
    parser.add_argument("--work", required=True)
    parser.add_argument("--admins", default="")
    parser.add_argument("--rotation", default="bloodgulch:ctf, prisoner:slayer, beavercreek:ctf, hangemhigh:slayer")
    parser.add_argument("--goat", action="store_true")
    parser.add_argument("--address", default="127.0.0.201")
    parser.add_argument("--client", default="127.0.0.200")
    parser.add_argument("--goat-address", default="127.0.0.202")
    parser.add_argument("--extra", action="append", default=[], help="NAME=VALUE for the server")
    parser.add_argument("--pack", action="append", default=[], help="a map pack's folder (maps_<pack>) linked beside maps")
    parser.add_argument("--weapon-packs", default="", help="a packs folder linked as the server's and Goat's packs/")
    parser.add_argument("--after", action="append", default=[],
                        help="SECONDS:COMMAND: a line to the server's standard input that long after its start")
    arguments = parser.parse_args()
    game = str(Path(arguments.game).resolve())
    processes = []

    def stop(*_):
        for process in processes:
            if process.poll() is None:
                process.terminate()
        time.sleep(1)
        for process in processes:
            if process.poll() is None:
                process.kill()
        sys.exit(0)

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    server = folder(arguments.work, "server", arguments.maps)
    if arguments.weapon_packs and not (server / "data" / "packs").exists():
        (server / "data" / "packs").symlink_to(Path(arguments.weapon_packs).resolve())
    for pack in arguments.pack:
        link = server / "data" / Path(pack).name
        if not link.exists():
            link.symlink_to(Path(pack).resolve())
    if arguments.admins:
        (server / "data" / "admins.txt").write_text(arguments.admins.replace("<TAB>", "\t") + "\n")
    settings = {"HALO_NET_ADDRESS": arguments.address,
                "HALO_NET_BROADCAST": ",".join([arguments.client, arguments.goat_address]),
                "HALO_SERVER_WEB_PORT": 0, "HALO_SERVER_LOBBY_SECONDS": 3, "HALO_SERVER_POSTGAME_SECONDS": 5,
                "HALO_SERVER_NAME": "Friday Night 4v4", "HALO_SERVER_ROTATION": arguments.rotation}
    for item in arguments.extra:
        name, _, value = item.partition("=")
        settings[name] = value
    log = open(server / "out.txt", "w")
    processes.append(subprocess.Popen([game, "--server"], cwd=server, env=environment(server, game, **settings),
                                      stdin=subprocess.PIPE, stdout=log, stderr=subprocess.STDOUT))
    if arguments.goat:
        time.sleep(4)
        goat = folder(arguments.work, "goat", arguments.maps)
        if arguments.weapon_packs and not (goat / "data" / "packs").exists():
            (goat / "data" / "packs").symlink_to(Path(arguments.weapon_packs).resolve())
        for pack in arguments.pack:
            link = goat / "data" / Path(pack).name
            if not link.exists():
                link.symlink_to(Path(pack).resolve())
        goat_log = open(goat / "out.txt", "w")
        processes.append(subprocess.Popen([game], cwd=goat, env=environment(
            goat, game, HALO_NULL_RENDERER=1, HALO_NETWORK_TEST="join", HALO_NETWORK_TEST_NAME="Goat",
            HALO_NET_ADDRESS=arguments.goat_address, HALO_NET_BROADCAST=arguments.address, HALO_NO_AUDIO=1),
            stdout=goat_log, stderr=subprocess.STDOUT))
    started = time.time()
    pending = []
    for item in arguments.after:
        seconds, _, command = item.partition(":")
        pending.append((float(seconds), command))
    pending.sort()
    while all(process.poll() is None for process in processes[:1]):
        while pending and time.time() - started >= pending[0][0]:
            _, command = pending.pop(0)
            try:
                processes[0].stdin.write((command + "\n").encode())
                processes[0].stdin.flush()
            except OSError:
                pass
        time.sleep(0.5)
    stop()


if __name__ == "__main__":
    main()
