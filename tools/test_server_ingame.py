"""Running a dedicated server from inside the game (0.7.7, plan Task 12; port/linux/game/ae_server_ingame.c), with the
game: a joined client signs in to the server's moderation by itself; a player with a role (admins.txt's "cidr:" row of
the address they join from, a "key:" row of their moderator key, or HOST A SERVER's owner key) is sent the server's
admin message (113: their role, what it may do, the server and its games to come); a player without one is sent
nothing. The actions themselves: tools/test_ae_pause.py's server cases (the pause menu's RUN THE SERVER page and the
player card, from the menus).

Needs a build's halo (AE_SERVER_GAME, default build/linux64/halo, else build/linux/halo) and the retail maps in
assets/maps, as tools/test_dedicated_server.py; every machine in a folder of its own under pytest's tmp_path, on
loopback addresses of its own, offline.

    python3 -m pytest -q tools/test_server_ingame.py
"""

import importlib.util
import os
import re
import time
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
_spec = importlib.util.spec_from_file_location("dedicated_server_machines", HERE / "test_dedicated_server.py")
ds = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(ds)

_game = os.environ.get("AE_SERVER_GAME")
if _game:
    ds.GAME = Path(_game)
elif (ROOT / "build/linux64/halo").is_file():
    ds.GAME = ROOT / "build/linux64/halo"
    ds.SDL = ROOT / "build/linux64/libSDL3.so.0"

machines = ds.machines
KEY = re.compile(r"this PC's moderator key: ([0-9a-f]{64})")


def _server(machines, **settings):
    ds._needs_game("bloodgulch")
    server = machines("server")
    return server, settings


def _start_server(server, settings, rows=""):
    if rows:
        (server.folder / "data" / "admins.txt").write_text(rows)
    values = {"HALO_SERVER_ROTATION": "bloodgulch:slayer, prisoner:ctf, nosuchmap:slayer",
              "HALO_SERVER_LOBBY_SECONDS": 2}
    values.update(settings)
    server.start(["--server"], **ds._server_settings(0, **values))
    ds._until(lambda: "players with a role run the server from the game" in server.debug() + server.text(), 60,
              "the server's moderation host", server)


def _join(machines, name="Goat", address=None):
    client = machines(name.lower())
    ds._client(client, address or ds.CLIENT_ADDRESSES[0], name)
    return client


def _client_log(client):
    return client.debug() + client.text()


def test_admin_by_address_is_sent_the_admin_message(machines):
    server, settings = _server(machines)
    _start_server(server, settings, "cidr:127.0.0.0/8\tadmin\t0\t0\t\t\n")
    client = _join(machines)
    ds._until(lambda: "the server says this player is ADMIN here" in _client_log(client), 90,
              "the client's admin message", client)
    log = _client_log(client)
    assert "signed in to the server's moderation" in log
    assert KEY.search(log), log[-2000:]
    # (an admin's actions: kick, ban, move, mute, next, end, say; the restart is the owner's on a stock command table)
    actions = int(re.search(r"is ADMIN here \(actions 0x([0-9a-f]+), (\d+) games listed\)", log).group(1), 16)
    assert actions == 0x7F, hex(actions)
    assert re.search(r"is ADMIN here \(actions 0x[0-9a-f]+, 3 games listed\)", log)
    server_log = server.debug() + server.text()
    assert re.search(r"machine \d+ signed in from the game, key [0-9a-f]{8}: admin", server_log), server_log[-3000:]


def test_no_role_is_sent_nothing(machines):
    server, settings = _server(machines)
    _start_server(server, settings)
    client = _join(machines)
    # (0.7.7: no key row, no role for this address: the game does not sign in, and the server never sees its key)
    ds._until(lambda: "the server's sign-in offer: 0 keys listed" in _client_log(client), 90, "the sign-in offer", client)
    time.sleep(8)
    assert "the server says this player is" not in _client_log(client)
    assert "signed in to the server's moderation" not in _client_log(client)
    assert "signed in from the game" not in server.debug() + server.text()


def test_key_row_added_while_playing_gives_the_role(machines):
    server, settings = _server(machines)
    _start_server(server, settings)
    client = _join(machines)
    ds._until(lambda: KEY.search(_client_log(client)), 90, "the client's key", client)
    key = KEY.search(_client_log(client)).group(1)
    server.send(f"sv_admin_add key:{key.upper()} moderator")
    ds._until(lambda: "the server says this player is MODERATOR here" in _client_log(client), 60,
              "the moderator's admin message", client)
    actions = int(re.search(r"is MODERATOR here \(actions 0x([0-9a-f]+)", _client_log(client)).group(1), 16)
    # (a moderator: UP NEXT now (0.7.7 default; command_access.txt can lock it), no ban, no restart)
    assert actions == 0x77, hex(actions)
    assert f"key:{key}\tmoderator" in (server.folder / "data" / "admins.txt").read_text()


def test_owner_key_from_the_starter(machines):
    """HOST A SERVER hands the server its starting PC's key (HALO_SERVER_OWNER_KEY): that player is its owner"""
    probe_server, settings = _server(machines)
    _start_server(probe_server, settings)
    probe = _join(machines, "Probe")
    ds._until(lambda: KEY.search(_client_log(probe)), 90, "the client's key", probe)
    key = KEY.search(_client_log(probe)).group(1)
    probe.stop()
    probe_server.stop()
    server = machines("owned")
    _start_server(server, {"HALO_SERVER_OWNER_KEY": key})
    # (the same PC again: its save root, so its key)
    probe.start(HALO_NULL_RENDERER=1, HALO_NETWORK_TEST="join", HALO_NETWORK_TEST_NAME="Probe",
                HALO_NET_ADDRESS=ds.CLIENT_ADDRESSES[0], HALO_NET_BROADCAST=ds.SERVER_ADDRESS, HALO_NO_AUDIO=1)
    ds._until(lambda: "the server says this player is OWNER here" in _client_log(probe), 90, "the owner's message",
              probe)
    assert "(the owner's key set by its starter)" in server.debug() + server.text()


def test_sv_team_moves_a_player(machines):
    server, settings = _server(machines)
    _start_server(server, dict(settings, HALO_SERVER_ROTATION="bloodgulch:ctf"))
    client = _join(machines)
    ds._until(lambda: "joined" in server.debug() + server.text() or "Goat" in server.debug(), 90, "the join", server)
    time.sleep(15)
    server.send("sv_team 0 blue")
    ds._until(lambda: "moved to the blue team" in server.debug() + server.text() or
              "sv_team:" in server.text(), 30, "sv_team's answer", server)
    assert "moved to the blue team" in server.debug() + server.text(), server.text()[-2000:]
