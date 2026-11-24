"""The AE master (a Go master built from the ae-master repository) with the game: a dedicated server with a
publisher key publishes to it by a signed first publish carrying its public key (no application), is auto approved after
the master's UDP probe, and is listed with its signed AE block (`?ae=1`: join_now, region, verified); an AE client
asking the same master through masters.txt finds it (one row, "lists 1 server"). Needs OPENCE_MASTER_BIN (the master's
binary), AE_TEST_GAME (a linux64 halo) and AE_TEST_DATA (a data root with maps/bloodgulch.map); skipped otherwise.
The master and the machines run offline from loopback with throwaway keys (never the real AE master's)."""

import json
import os
import re
import shutil
import subprocess
import time
import urllib.request
from pathlib import Path

import pytest

MASTER = os.environ.get("OPENCE_MASTER_BIN", "")
GAME = os.environ.get("AE_TEST_GAME", "")
DATA = os.environ.get("AE_TEST_DATA", "")
PORT = int(os.environ.get("AE_TEST_MASTER_PORT", "8091"))


def _env(folder, **extra):
    env = {k: v for k, v in os.environ.items() if not k.startswith(("HALO_", "MASTER_"))}
    env.update({"HALO_DATA_ROOT": str(folder / "data"), "HALO_SAVE_ROOT": str(folder / "saves"),
                "HOME": str(folder / "home"), "XDG_DATA_HOME": str(folder / "xdg"), "SDL_VIDEO_DRIVER": "dummy",
                "SDL_AUDIO_DRIVER": "dummy", "HALO_UPDATE_AUTO": "false", "HALO_DISCORD_APPLICATION": ""})
    env.update({k: str(v) for k, v in extra.items()})
    return env


def _machine(base, name, masters_line):
    folder = base / name
    for part in ("bin", "data", "saves", "home", "xdg"):
        (folder / part).mkdir(parents=True)
    shutil.copy(GAME, folder / "bin" / "halo")
    (folder / "data" / "maps").symlink_to(Path(DATA) / "maps")
    (folder / "bin" / "masters.txt").write_text(masters_line + "\n")
    return folder


def _fetch(path):
    with urllib.request.urlopen(f"http://127.0.0.1:{PORT}{path}", timeout=5) as answer:
        return json.load(answer)


def _until(condition, timeout, what):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            value = condition()
        except OSError:
            value = None
        if value:
            return value
        time.sleep(1)
    pytest.fail(f"{what} not within {timeout} s")


@pytest.mark.skipif(not (MASTER and GAME and DATA), reason="needs OPENCE_MASTER_BIN, AE_TEST_GAME, AE_TEST_DATA")
def test_lists_on_local_ae_master(tmp_path):
    key_path = tmp_path / "master.key"
    out = subprocess.run([MASTER, "keygen", "--out", str(key_path)], capture_output=True, text=True, check=True).stdout
    public = re.search(r"public key (ed25519:[0-9a-f]{64})", out).group(1)
    line = f"http://127.0.0.1:{PORT} {public}"
    master_env = {k: v for k, v in os.environ.items() if not k.startswith("MASTER_")}
    master_env.update({
        "MASTER_NAME": "aetest", "MASTER_CONTACT": "t@example.org", "MASTER_DB": str(tmp_path / "m.db"),
        "MASTER_KEY": str(key_path), "MASTER_LISTEN": f"127.0.0.1:{PORT}", "MASTER_PUBLIC_URL": f"http://127.0.0.1:{PORT}",
        "MASTER_PROBE_LISTEN": "0.0.0.0:0", "MASTER_ADMIN_TOKEN": "0123456789abcdef0123456789abcdef0123",
        "MASTER_AUTO_APPROVE": "true", "MASTER_MANUAL_APPLY": "false", "MASTER_NETLIST": "off",
        "MASTER_PUBLISH_MIN_INTERVAL": "1s"})
    master = subprocess.Popen([MASTER, "serve"], env=master_env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    machines = []
    try:
        _until(lambda: _fetch("/v1/master"), 15, "the master")
        server_folder = _machine(tmp_path, "server", line)
        server_log = open(server_folder / "out.txt", "w")
        machines.append(subprocess.Popen(
            [str(server_folder / "bin" / "halo"), "--server"], cwd=server_folder, stdin=subprocess.DEVNULL,
            stdout=server_log, stderr=subprocess.STDOUT,
            env=_env(server_folder, HALO_SERVER_WEB_PORT=18096, HALO_SERVER_WEB_PASSWORD="test-password-1",
                     HALO_SERVER_NAME="AE List Test", HALO_SERVER_ROTATION="bloodgulch:slayer",
                     HALO_SERVER_PUBLISHER_KEY="07" * 32, HALO_AE_MASTER_KEYS=public, HALO_SERVER_REGION="US-West",
                     HALO_SERVER_LOBBY_SECONDS=300)))
        # (auto approved by the probe, no application)
        listed = _until(lambda: [s for s in _fetch("/v1/servers?ae=1")["servers"] if s["name"] == "AE List Test"],
                        120, "the server listed")
        view = listed[0]["ae"]
        assert view["verified"] is True and view["join_now"] == "all" and view["region"] == "us-west", view
        assert listed[0]["dedicated"] is True and isinstance(view["probe_ping_ms"], int), listed[0]
        # (the standard list is as always: no AE block)
        assert all("ae" not in s for s in _fetch("/v1/servers")["servers"])

        client_folder = _machine(tmp_path, "client", line)
        subprocess.run([str(client_folder / "bin" / "halo")], cwd=client_folder, stdin=subprocess.DEVNULL,
                       capture_output=True, timeout=150,
                       env=_env(client_folder, HALO_NETWORK_TEST="browse", HALO_EXIT_AFTER=40, HALO_AE_MASTER_KEYS=public))
        debug = (client_folder / "data" / "debug.txt").read_text(errors="replace")
        assert f"server browser: http://127.0.0.1:{PORT} lists 1 server" in debug, debug[-2000:]
        assert len(re.findall(r"network test: listed AE List Test's game", debug)) == 1
    finally:
        for process in machines:
            process.terminate()
        master.terminate()
