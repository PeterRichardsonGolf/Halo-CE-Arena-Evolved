"""The dedicated server (halo --server: docs/dedicated_server.md) with the game: its refusals and exit codes, a
rotation played by a joined client (lobby, game, postgame and the next entry), the HTTP API (status, players, kick,
ban and unban, say, the rotation), the events, standard input's commands, the owner's sign-in from loopback, a ban of
an address, drains (sv_stop empty, sv_restart game), and its shutdown by quit, SIGTERM and SIGINT. Needs the retail
maps in assets/maps and ninja linux's build/linux/halo; every machine runs from a folder of its own under pytest's
tmp_path (its config.toml, data root, saves and home), on loopback addresses of its own, offline (system link), with
no window, sound, update check or Discord."""

import base64
import codecs
import hashlib
import json
import os
import re
import shutil
import signal
import socket
import struct
import subprocess
import threading
import time
import urllib.error
import urllib.request
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
RETAIL_MAPS = ROOT / "assets/maps"
GAME = ROOT / "build/linux/halo"
SDL = ROOT / "build/linux/libSDL3.so.0"
# (AE_TEST_NET_BASE: another run on the same machine can take the loopback addresses 127.0.0.20x; 127.0.<n>.<k>)
_BASE = os.environ.get("AE_TEST_NET_BASE", "127.0.0")
SERVER_ADDRESS = f"{_BASE}.201"
CLIENT_ADDRESSES = (f"{_BASE}.202", f"{_BASE}.203")
PASSWORD = "test-password-1"


def _needs_game(*maps):
    if not GAME.is_file():
        pytest.skip("needs ninja linux's build/linux/halo")
    for name in maps:
        if not (RETAIL_MAPS / f"{name}.map").is_file():
            pytest.skip(f"needs {name}.map in assets/maps")


def _free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.bind(("127.0.0.1", 0))
        return probe.getsockname()[1]


class Machine:
    """a machine's folder (bin/ with the game and its config.toml, data/, saves/, home/) and its process"""

    def __init__(self, base, name, maps=True):
        self.folder = base / name
        self.name = name
        for part in ("bin", "data", "saves", "home", "xdg"):
            (self.folder / part).mkdir(parents=True, exist_ok=True)
        # (AE_TEST_SERVER_BINARY: another build as the server, to see a test fail on the build it is about)
        server_binary = os.environ.get("AE_TEST_SERVER_BINARY")
        shutil.copy(server_binary if name == "server" and server_binary else GAME, self.folder / "bin" / "halo")
        if SDL.is_file():
            shutil.copy(SDL, self.folder / "bin" / SDL.name)
        if maps:
            (self.folder / "data" / "maps").symlink_to(RETAIL_MAPS)
        self.process = None
        self.output = self.folder / "out.txt"

    def environment(self, **settings):
        environment = {key: value for key, value in os.environ.items() if not key.startswith("HALO_")}
        environment.update({
            # (what marks the processes this test started, should one need stopping by hand)
            "HALO_TEST_MARKER": f"test_dedicated_server-{self.name}",
            "HALO_DATA_ROOT": str(self.folder / "data"), "HALO_SAVE_ROOT": str(self.folder / "saves"),
            "HOME": str(self.folder / "home"), "XDG_DATA_HOME": str(self.folder / "xdg"),
            "SDL_VIDEO_DRIVER": "dummy", "SDL_AUDIO_DRIVER": "dummy", "HALO_UPDATE_AUTO": "false",
            "HALO_NET_ONLINE": "false", "HALO_DISCORD_APPLICATION": "", "HALO_NET_JOIN_FROM_CLIPBOARD": "0",
        })
        environment.update({key: str(value) for key, value in settings.items()})
        return environment

    def start(self, arguments=(), **settings):
        self.log = open(self.output, "w")
        self.process = subprocess.Popen([str(self.folder / "bin" / "halo"), *arguments], cwd=self.folder,
                                        env=self.environment(**settings), stdin=subprocess.PIPE,
                                        stdout=self.log, stderr=subprocess.STDOUT, text=True)
        return self

    def run(self, arguments=(), timeout=30, **settings):
        """the game run to its end: (exit status, its output)"""
        result = subprocess.run([str(self.folder / "bin" / "halo"), *arguments], cwd=self.folder,
                                env=self.environment(**settings), stdin=subprocess.DEVNULL, capture_output=True,
                                text=True, errors="replace", timeout=timeout)
        return result.returncode, result.stdout + result.stderr

    def text(self):
        return self.output.read_text(errors="replace") if self.output.is_file() else ""

    def debug(self):
        path = self.folder / "data" / "debug.txt"
        return path.read_text(errors="replace") if path.is_file() else ""

    def send(self, line):
        self.process.stdin.write(line + "\n")
        self.process.stdin.flush()

    def wait(self, timeout):
        try:
            return self.process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            return None

    def stop(self):
        if self.process and self.process.poll() is None:
            self.process.kill()
            self.process.wait()
        if self.process:
            self.log.close()


@pytest.fixture
def machines(tmp_path):
    made = []

    def make(name, maps=True):
        machine = Machine(tmp_path, name, maps)
        made.append(machine)
        return machine

    yield make
    for machine in made:
        machine.stop()


def _server_settings(web_port, **settings):
    values = {
        "HALO_NET_ADDRESS": SERVER_ADDRESS, "HALO_NET_BROADCAST": ",".join(CLIENT_ADDRESSES),
        "HALO_SERVER_WEB_PORT": web_port, "HALO_SERVER_WEB_PASSWORD": PASSWORD,
        "HALO_SERVER_LOBBY_SECONDS": 3, "HALO_SERVER_POSTGAME_SECONDS": 3, "HALO_SERVER_NAME": "Test Server",
        "HALO_SERVER_ROTATION": "bloodgulch:slayer, prisoner:slayer",
    }
    values.update(settings)
    return values


def _client(machine, address, name, server=SERVER_ADDRESS, join=None, **more):
    extra = {"HALO_NETWORK_TEST_JOIN_ADDRESS": join} if join else {}
    extra.update(more)
    return machine.start(HALO_NULL_RENDERER=1, HALO_NETWORK_TEST="join", HALO_NETWORK_TEST_NAME=name,
                         HALO_NET_ADDRESS=address, HALO_NET_BROADCAST=server, HALO_NO_AUDIO=1,
                         **extra)


def _until(condition, timeout, what, machine=None):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        value = condition()
        if value:
            return value
        if machine and machine.process.poll() is not None:
            break
        time.sleep(0.5)
    detail = f"\n--- {machine.name}'s output:\n{machine.text()[-3000:]}" if machine else ""
    pytest.fail(f"{what} not within {timeout} s{detail}")


class Api:
    def __init__(self, port):
        self.base = f"http://127.0.0.1:{port}"
        self.token = None

    def call(self, method, path, body=None, token=True):
        request = urllib.request.Request(self.base + path, method=method,
                                         data=json.dumps(body).encode() if body is not None else None)
        if body is not None:
            request.add_header("Content-Type", "application/json")
        if token and self.token:
            request.add_header("Authorization", f"Bearer {self.token}")
        try:
            with urllib.request.urlopen(request, timeout=10) as answer:
                return answer.status, json.loads(answer.read() or b"null")
        except urllib.error.HTTPError as failure:
            text = failure.read()
            try:
                return failure.code, json.loads(text)
            except ValueError:
                return failure.code, text.decode(errors="replace")
        except (urllib.error.URLError, ConnectionError, TimeoutError):
            return None, None

    def login(self):
        status, answer = self.call("POST", "/api/login", {"password": PASSWORD}, token=False)
        assert status == 200 and answer["ok"], answer
        self.token = answer["token"]

    def status(self):
        status, answer = self.call("GET", "/api/status")
        return answer if status == 200 else None

    def events(self):
        status, answer = self.call("GET", "/api/events?after=0")
        assert status == 200, answer
        return answer["events"]


def _event_names(api):
    return [event.get("event") for event in api.events()]


# ---------- refusals

def test_version_and_help(machines):
    _needs_game()
    machine = machines("version", maps=False)
    status, output = machine.run(["--version"], HALO_CONFIG_FILE=machine.folder / "config.toml")
    assert status == 0, output
    assert re.search(r"network version \d+", output), output
    status, output = machine.run(["--help"], HALO_CONFIG_FILE=machine.folder / "config.toml")
    assert status == 0 and "--server" in output, output


def test_no_maps_exits_2(machines):
    _needs_game()
    machine = machines("nomaps", maps=False)
    started = time.monotonic()
    status, output = machine.run(["--server"], HALO_SERVER_WEB_PORT=0)
    assert status == 2, output
    assert time.monotonic() - started < 10, "the refusal took more than 10 s"
    assert "no usable maps" in output, output


def test_rotation_of_missing_maps_exits_2(machines):
    _needs_game("bloodgulch")
    machine = machines("missingmaps")
    status, output = machine.run(["--server"], HALO_SERVER_WEB_PORT=0, HALO_SERVER_ROTATION="nosuchmap:slayer")
    assert status == 2, output


def test_rotation_of_unknown_gametypes_exits_3(machines):
    _needs_game("bloodgulch")
    machine = machines("badrotation")
    status, output = machine.run(["--server"], HALO_SERVER_WEB_PORT=0,
                                 HALO_SERVER_ROTATION="bloodgulch:nosuchgametype, bloodgulch:slayer+score=-5")
    assert status == 3, output
    assert "nothing playable" in output, output


def test_public_web_port_needs_a_long_password(machines):
    _needs_game("bloodgulch")
    machine = machines("shortpassword")
    status, output = machine.run(["--server"], HALO_SERVER_WEB_PORT=_free_port(), HALO_SERVER_WEB_BIND="0.0.0.0",
                                 HALO_SERVER_WEB_PASSWORD="short")
    assert status == 3, output


def test_public_web_port_without_proxy_needs_12_characters(machines):
    # (security wave 2, M3: a password that crosses the network in clear text) 8 to 11 characters pass on loopback or
    # behind a trusted proxy, not beyond loopback with neither
    _needs_game("bloodgulch")
    machine = machines("shortpassword2")
    status, output = machine.run(["--server"], HALO_SERVER_WEB_PORT=_free_port(), HALO_SERVER_WEB_BIND="0.0.0.0",
                                 HALO_SERVER_WEB_PASSWORD="elevenchars")
    assert status == 3, output
    assert "shorter than 12 characters" in output, output


def test_web_port_in_use_exits_4(machines):
    _needs_game("bloodgulch")
    machine = machines("portinuse")
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as taken:
        taken.bind(("127.0.0.1", 0))
        taken.listen(1)
        status, output = machine.run(["--server"], HALO_SERVER_WEB_PORT=taken.getsockname()[1],
                                     HALO_SERVER_WEB_PASSWORD=PASSWORD, HALO_NET_ADDRESS=SERVER_ADDRESS,
                                     HALO_NET_BROADCAST=CLIENT_ADDRESSES[0])
    assert status == 4, output


# ---------- standard input, a read-only data root, signals

def test_quit_on_standard_input_with_a_read_only_data_root(machines, tmp_path):
    _needs_game("bloodgulch")
    machine = machines("readonly")
    logs = tmp_path / "logs"
    logs.mkdir()
    data = machine.folder / "data"
    port = _free_port()
    status_file = tmp_path / "status.json"
    # (read-only from the start: the server must not need to write there)
    data.chmod(0o555)
    try:
        before = sorted(path.name for path in data.iterdir())
        machine.start(["--server"], **_server_settings(port, HALO_LOG_ROOT=logs, HALO_SERVER_STATUS_FILE=status_file))
        _until(lambda: "server: ready" in machine.text(), 90, "server: ready", machine)
        machine.send("sv_status")
        _until(lambda: "lobby" in machine.text().split("server: ready", 1)[1], 15, "sv_status's answer", machine)
        # (the status file, by its host path, as /api/status gives it)
        _until(lambda: status_file.is_file(), 15, "the status file", machine)
        assert json.loads(status_file.read_text())["state"] == "lobby"
        machine.send("quit")
        assert machine.wait(15) == 0, machine.text()[-3000:]
        assert sorted(path.name for path in data.iterdir()) == before, "the server wrote to its read-only data root"
        assert (logs / "debug.txt").is_file(), "no debug.txt in paths.logs"
    finally:
        data.chmod(0o755)


def test_second_sigint_exits_130(machines):
    _needs_game("bloodgulch")
    machine = machines("sigint")
    machine.start(["--server"], **_server_settings(0))
    _until(lambda: "server: ready" in machine.text(), 90, "server: ready", machine)
    machine.process.send_signal(signal.SIGINT)
    # (the second once the first is taken: two at once are one pending signal)
    _until(lambda: "stopping (signal 2)" in machine.text(), 10, "the first SIGINT taken", machine)
    machine.process.send_signal(signal.SIGINT)
    assert machine.wait(15) == 130, machine.text()[-3000:]


# ---------- a rotation played

def test_rotation_moderation_and_shutdown(machines):
    """a client joins, plays the first entry (ended by sv_end_game), the postgame, then the second entry; the API
    kicks it, bans and unbans it, says a notice and refuses a bad rotation; SIGTERM shows the client the postgame
    and exits 0"""
    _needs_game("bloodgulch", "prisoner")
    port = _free_port()
    server = machines("server").start(["--server"], **_server_settings(port))
    api = Api(port)
    _until(lambda: "server: ready" in server.text(), 90, "server: ready", server)
    api.login()
    status = api.status()
    assert status["name"] == "Test Server" and status["state"] == "lobby", status
    assert status["players"] == 0, "the host has a player of its own"

    # (with a mod on, which the client names to the host with its build)
    client_machine = machines("client")
    (client_machine.folder / "data" / "mods" / "testmod").mkdir(parents=True)
    mod_toml = client_machine.folder / "data" / "mods" / "testmod" / "mod.toml"
    mod_toml.write_text('name = "Test mod"\nversion = "1.2"\n')
    client = _client(client_machine, CLIENT_ADDRESSES[0], "Tester")
    _until(lambda: (api.status() or {}).get("state") == "playing", 120, "the first game", server)
    assert api.status()["map"] == "bloodgulch"
    players = api.call("GET", "/api/players")[1]["players"]
    assert [player["name"] for player in players] == ["Tester"], players
    # its machine's build, as the client told it (after its ready, reliably), and said once in the server's log
    _until(lambda: api.call("GET", "/api/players")[1]["players"][0].get("client"), 30, "the client's build", server)
    told = api.call("GET", "/api/players")[1]["players"][0]["client"]
    assert told["version"].startswith("Halo CE: Arena Evolved ") and isinstance(told["build"], int), told
    assert told["flavor"] and re.fullmatch(r"([\w.-]+/[\w.-]+)?", told["repository"]), told
    assert told["mods"] == ["testmod 1.2"], told
    assert server.text().count("plays " + told["version"]) == 1, "the client's build not logged once"

    # the first game ended by a command, its postgame, the second entry's lobby and game
    server.send("sv_end_game")
    _until(lambda: "postgame" in _event_names(api), 30, "the postgame", server)
    _until(lambda: (api.status() or {}).get("map") == "prisoner" and api.status()["state"] == "playing", 120,
           "the second game, on prisoner", server)
    names = _event_names(api)
    for event in ("server_started", "lobby_opened", "player_joined", "game_started", "game_ended", "postgame"):
        assert event in names, f"no {event} event: {names}"
    assert names.count("game_started") >= 2, names
    _until(lambda: "prisoner" in client.debug(), 30, "the client on prisoner", client)

    # a notice, and a rotation entry the API refuses with nothing changed
    status, answer = api.call("POST", "/api/say", {"text": "hello from the test"})
    assert status == 200 and answer["ok"], answer
    rotation = api.call("GET", "/api/rotation")[1]
    status, answer = api.call("PUT", "/api/rotation", {"entries": ["bloodgulch:slayer", "nosuchmap:ctf"]})
    assert status == 400 and answer["error"]["code"] == "rejected", answer
    assert answer["error"]["rejected"][0]["index"] == 1, answer
    assert api.call("GET", "/api/rotation")[1]["entries"] == rotation["entries"], "a refused rotation changed it"

    # a kick by slot (the name checked), then a ban of an hour, which bans.txt keeps with its end
    slot = api.call("GET", "/api/players")[1]["players"][0]["slot"]
    status, answer = api.call("POST", f"/api/players/{slot}/kick", {"name": "Someone Else"})
    assert status != 200 or not answer.get("ok"), "a kick of another name than the slot's went through"
    status, answer = api.call("POST", f"/api/players/{slot}/kick", {"name": "Tester"})
    assert status == 200 and answer["ok"], answer
    _until(lambda: not api.call("GET", "/api/players")[1]["players"], 30, "the kicked player gone", server)
    assert "kick" in _event_names(api)
    # (the join test joins once: a new start of it joins again)
    client.stop()
    _client(client, CLIENT_ADDRESSES[0], "Tester")
    _until(lambda: api.call("GET", "/api/players")[1]["players"], 90, "the client back after its kick", server)
    slot = api.call("GET", "/api/players")[1]["players"][0]["slot"]
    status, answer = api.call("POST", f"/api/players/{slot}/ban",
                              {"name": "Tester", "duration": "1h", "reason": "a test"})
    assert status == 200 and answer["ok"], answer
    _until(lambda: not api.call("GET", "/api/players")[1]["players"], 30, "the banned player gone", server)
    bans = api.call("GET", "/api/bans")[1]["bans"]
    assert len(bans) == 1 and bans[0]["until"], bans
    assert "until=" in (server.folder / "data" / "bans.txt").read_text(errors="replace")
    client.stop()
    _client(client, CLIENT_ADDRESSES[0], "Tester")
    time.sleep(25)
    assert not api.call("GET", "/api/players")[1]["players"], "the banned client joined again"
    status, answer = api.call("DELETE", f"/api/bans/{bans[0]['n']}")
    assert status == 200 and answer["ok"], answer
    client.stop()
    _client(client, CLIENT_ADDRESSES[0], "Tester")
    _until(lambda: api.call("GET", "/api/players")[1]["players"], 120, "the client back after its unban", server)

    # SIGTERM in a game: the client shown the postgame, the server gone with exit code 0
    _until(lambda: (api.status() or {}).get("state") == "playing", 120, "a game to stop in", server)
    server.process.send_signal(signal.SIGTERM)
    assert server.wait(15) == 0, server.text()[-3000:]
    assert "stopped (exit code 0)" in server.text()
    assert not re.search(r"EXCEPTION|assert", server.debug()), "an exception or assert in the server's debug.txt"


def test_word_filter_keeps_a_name_out(machines):
    """with server.chat_filter "block", a player whose name has a word of the filter's list (streamer mode's) is
    refused at the door, and the server says why; the word is ROT13'd here, as in the filter's own tests"""
    _needs_game("bloodgulch")
    port = _free_port()
    server = machines("server").start(["--server"], **_server_settings(
        port, HALO_SERVER_CHAT_FILTER="block", HALO_SERVER_CHAT_FILTER_SWEARING="mild",
        HALO_SERVER_ROTATION="bloodgulch:slayer"))
    api = Api(port)
    _until(lambda: "server: ready" in server.text(), 90, "server: ready", server)
    api.login()
    _client(machines("client"), CLIENT_ADDRESSES[0], codecs.decode("Shpxre", "rot13"))
    _until(lambda: "its name has words the server's filter keeps out" in server.debug(), 90, "the name refused",
           server)
    assert not api.call("GET", "/api/players")[1]["players"], "a player of a refused name joined"
    # (a name without them joins)
    _client(machines("other"), CLIENT_ADDRESSES[1], "Tester")
    _until(lambda: [player["name"] for player in api.call("GET", "/api/players")[1]["players"]] == ["Tester"], 90,
           "a player of a fine name in", server)


def test_empty_game_ends_and_join_in_progress(machines):
    """a game whose players leave ends after server.empty_seconds and the lobby opens again; a second client joins a
    game in progress"""
    _needs_game("bloodgulch")
    port = _free_port()
    server = machines("server").start(["--server"], **_server_settings(
        port, HALO_SERVER_ROTATION="bloodgulch:slayer", HALO_SERVER_EMPTY_SECONDS=5))
    api = Api(port)
    _until(lambda: "server: ready" in server.text(), 90, "server: ready", server)
    api.login()
    first = _client(machines("first"), CLIENT_ADDRESSES[0], "First")
    _until(lambda: (api.status() or {}).get("state") == "playing", 120, "the game", server)
    machines_by_name = {"second": _client(machines("second"), CLIENT_ADDRESSES[1], "Second")}
    _until(lambda: len(api.call("GET", "/api/players")[1]["players"]) == 2, 60, "the second client in the game",
           server)
    # (loaded, and playing: a game whose players all leave ends, a joiner still loading with it)
    _until(lambda: "local machine is loaded" in machines_by_name["second"].debug(), 60, "the second client loaded",
           machines_by_name["second"])
    time.sleep(5)
    first.stop()
    _until(lambda: len(api.call("GET", "/api/players")[1]["players"]) == 1, 60, "the first client gone", server)
    assert api.status()["state"] == "playing", "the game ended with a player in it"
    # the last client gone: the game ends as empty, and the lobby opens again
    machines_by_name["second"].stop()
    _until(lambda: any(event.get("event") == "game_ended" and event.get("reason") == "empty"
                       for event in api.events()), 90, "the empty game's end", server)
    _until(lambda: (api.status() or {}).get("state") == "lobby", 60, "the lobby again", server)
    # the next join starts a new game
    _client(machines("third"), CLIENT_ADDRESSES[0], "Third")
    _until(lambda: (api.status() or {}).get("state") == "playing", 120, "a new game for the next player", server)
    assert api.status()["games_hosted"] == 2, api.status()


def test_owner_from_loopback_and_a_drain_until_empty(machines, tmp_path):
    """the owner's password (from its file) signs in from loopback and sees addresses in the events; POST /api/bans
    bans an address with nobody of it present; sv_stop empty withdraws the server and refuses a new client, then stops
    with exit code 0 once the last player leaves"""
    _needs_game("bloodgulch")
    port = _free_port()
    owner_file = tmp_path / "owner-password"
    owner_file.write_text("owner-password-1\r\n")
    server = machines("server").start(["--server"], **_server_settings(
        port, HALO_SERVER_ROTATION="bloodgulch:slayer", HALO_SERVER_WEB_OWNER_PASSWORD_FILE=owner_file,
        HALO_SERVER_INSTANCE="drain-test"))
    api, owner = Api(port), Api(port)
    _until(lambda: "server: ready" in server.text(), 90, "server: ready", server)
    api.login()
    status, answer = owner.call("POST", "/api/login", {"password": "owner-password-1"}, token=False)
    assert status == 200 and answer["role"] == "owner", answer
    owner.token = answer["token"]
    status = owner.status()
    assert status["instance"] == "drain-test" and status["pid"] == server.process.pid and status["drain"] is None, status
    assert {"owner_web", "ban_address", "stop", "event_addresses", "identity"} <= set(status["capabilities"]), status
    # (the owner's role alone stops the server; a ban needs no player present)
    status, answer = api.call("POST", "/api/stop", {"after": "empty"})
    assert status == 403 and answer["error"]["code"] == "forbidden", answer
    status, answer = api.call("POST", "/api/bans", {"ip": "198.51.100.77", "duration": "1h", "reason": "a test"})
    assert status == 200 and answer["ok"], answer
    assert api.call("GET", "/api/bans")[1]["bans"][answer["n"]]["ip"] == "198.51.100.77"

    first = _client(machines("first"), CLIENT_ADDRESSES[0], "First")
    _until(lambda: (api.status() or {}).get("state") == "playing", 120, "the game", server)
    joined = [event for event in owner.events() if event.get("event") == "player_joined"]
    assert joined and joined[0]["address"] == CLIENT_ADDRESSES[0] and "hwid" in joined[0], joined
    assert all("address" not in event for event in api.events() if event.get("event") == "player_joined")

    status, answer = owner.call("POST", "/api/stop", {"after": "empty", "notice": "maintenance"})
    assert status == 200 and answer["ok"], answer
    drain = owner.status()["drain"]
    assert drain["after"] == "empty" and drain["restart"] is False and drain["since"] > 0, drain
    assert any(event.get("event") == "drain" and event.get("after") == "empty" for event in owner.events())
    # a new machine is refused while the server drains
    second = _client(machines("second"), CLIENT_ADDRESSES[1], "Second")
    time.sleep(20)
    assert [player["name"] for player in api.call("GET", "/api/players")[1]["players"]] == ["First"]
    assert server.process.poll() is None, "the server stopped with a player on it"
    second.stop()
    # the last player gone: the server stops, exit code 0, its shutdown said to be the drain's
    first.stop()
    assert server.wait(60) == 0, server.text()[-3000:]
    assert "stopping (drained)" in server.text()


def test_a_drain_until_the_game_is_over_restarts(machines):
    """sv_restart game during a game: the game goes on to its end and its postgame, then the server stops with exit
    code 5"""
    _needs_game("bloodgulch")
    port = _free_port()
    server = machines("server").start(["--server"], **_server_settings(
        port, HALO_SERVER_ROTATION="bloodgulch:slayer", HALO_SERVER_WEB_OWNER_PASSWORD="owner-password-1"))
    owner = Api(port)
    _until(lambda: "server: ready" in server.text(), 90, "server: ready", server)
    status, answer = owner.call("POST", "/api/login", {"password": "owner-password-1"}, token=False)
    assert status == 200 and answer["role"] == "owner", answer
    owner.token = answer["token"]
    _client(machines("client"), CLIENT_ADDRESSES[0], "Player")
    _until(lambda: (owner.status() or {}).get("state") == "playing", 120, "the game", server)
    status, answer = owner.call("POST", "/api/stop", {"after": "game", "restart": True})
    assert status == 200 and answer["ok"], answer
    time.sleep(3)
    assert server.process.poll() is None and owner.status()["state"] == "playing", "the drain cut the game short"
    status, answer = owner.call("POST", "/api/command", {"line": "sv_end_game"})
    assert status == 200 and answer["ok"], answer
    assert server.wait(60) == 5, server.text()[-3000:]
    text = server.text()
    assert "stopping (drained)" in text and text.index("game: ended") < text.index("stopping (drained)"), text[-3000:]


def _cpu_seconds(pid):
    """a process's user and system time (/proc/<pid>/stat), in seconds"""
    fields = Path(f"/proc/{pid}/stat").read_text().rsplit(")", 1)[1].split()
    return (int(fields[11]) + int(fields[12])) / os.sysconf("SC_CLK_TCK")


def test_idle_server_spends_little_cpu(machines):
    """a server with nobody on it uses under 3 % of a core (it runs 10 frames a second)"""
    _needs_game("bloodgulch")
    if not Path("/proc/self/stat").is_file():
        pytest.skip("needs /proc")
    server = machines("idle").start(["--server"], **_server_settings(0))
    _until(lambda: "server: ready" in server.text(), 90, "server: ready", server)
    time.sleep(5)
    started, used = time.monotonic(), _cpu_seconds(server.process.pid)
    time.sleep(30)
    share = (_cpu_seconds(server.process.pid) - used) / (time.monotonic() - started)
    assert share < 0.03, f"the idle server used {share:.1%} of a core"


def test_web_port_survives_junk(machines):
    """random bytes, heads cut short and left open, oversized heads and bodies, and bad WebSocket frames at the web
    port: the server keeps answering, and keeps running"""
    import random

    _needs_game("bloodgulch")
    port = _free_port()
    server = machines("junk").start(["--server"], **_server_settings(port))
    api = Api(port)
    _until(lambda: "server: ready" in server.text(), 90, "server: ready", server)
    rng = random.Random(1)
    stalled = []
    for index in range(300):
        with socket.create_connection(("127.0.0.1", port), timeout=5) as connection:
            kind = index % 5
            if kind == 0:
                connection.sendall(bytes(rng.randrange(256) for _ in range(rng.randrange(1, 4096))))
            elif kind == 1:
                connection.sendall(b"GET / HTTP/1.1\r\nHost: x\r\n" + b"X-Long: " + b"a" * 20000 + b"\r\n\r\n")
            elif kind == 2:
                connection.sendall(b"POST /api/login HTTP/1.1\r\nContent-Length: 999999\r\n\r\n" + b"{" * 70000)
            elif kind == 3:
                connection.sendall(b"GET /ws HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                                   b"Sec-WebSocket-Version: 13\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                                   b"Sec-WebSocket-Protocol: opence-rcon-1\r\n\r\n")
                connection.sendall(b"\x81\x7f" + b"\xff" * 8 + bytes(rng.randrange(256) for _ in range(64)))
            else:
                connection.sendall(b"GET /api/status?" + bytes(rng.choice(b"%/.\x00\xff?&=") for _ in range(200)) +
                                   b" HTTP/1.1\r\n\r\n")
            try:
                connection.recv(4096)
            except (socket.timeout, ConnectionError):
                pass
    for _ in range(4):
        stalled.append(socket.create_connection(("127.0.0.1", port), timeout=5))
        stalled[-1].sendall(b"GET /healthz HTTP/1.1\r\n")
    status, answer = api.call("GET", "/healthz", token=False)
    assert status == 200, (status, answer)
    time.sleep(12)
    for connection in stalled:
        connection.close()
    status, answer = api.call("GET", "/healthz", token=False)
    assert status == 200, (status, answer)
    assert server.process.poll() is None, server.text()[-3000:]
    server.send("quit")
    assert server.wait(15) == 0, server.text()[-3000:]


def test_settings_refuse_a_value_they_cannot_read(machines):
    """PUT /api/settings with a string it cannot read whole (300 characters and more, or one holding \\u0000): refused,
    the setting left as it was (it had been applied as the empty string, which took a game password away), as a
    value of 257 to 299 characters is; a key it cannot read refused with a message of its own"""
    _needs_game("bloodgulch")
    port = _free_port()
    server = machines("settings").start(["--server"], **_server_settings(port))
    api = Api(port)
    _until(lambda: "server: ready" in server.text(), 90, "server: ready", server)
    api.login()

    def setting(name):
        status, answer = api.call("GET", "/api/settings")
        assert status == 200, answer
        return next(entry for entry in answer["settings"] if entry["name"] == name)

    status, answer = api.call("PUT", "/api/settings", {"server.motd": "Welcome", "server.password": "hunter2"})
    assert status == 200 and answer["applied"] == ["server.motd", "server.password"], answer
    assert setting("server.password")["set"]
    for value in ("a" * 300, "a" * 1000, "hi\u0000"):
        for name in ("server.motd", "server.password"):
            status, answer = api.call("PUT", "/api/settings", {name: value})
            assert status == 200 and answer["applied"] == [], (name, len(value), answer)
            assert answer["errors"][0]["name"] == name and "longer than 256 characters" in answer["errors"][0]["message"], \
                answer
    assert setting("server.motd")["value"] == "Welcome"
    assert setting("server.password")["set"], "the game password was taken away"
    status, answer = api.call("PUT", "/api/settings", {"server.motd": "a" * 260})
    assert answer["applied"] == [] and "longer than 256 characters" in answer["errors"][0]["message"], answer
    status, answer = api.call("PUT", "/api/settings", {"": 1})
    assert answer["applied"] == [] and answer["errors"][0]["code"] == "not_found", answer
    assert answer["errors"][0]["message"] == "a setting's name is a string of at most 63 characters", answer
    status, answer = api.call("PUT", "/api/settings", {"server.motd": "Hello"})
    assert answer["applied"] == ["server.motd"], answer
    assert setting("server.motd")["value"] == "Hello"
    server.send("quit")
    assert server.wait(15) == 0, server.text()[-3000:]


def test_probe_prints_the_server(machines):
    """halo --probe, on the LAN: the server's game as one line of JSON, nothing joined"""
    _needs_game("prisoner")
    server = machines("probed").start(["--server"], **_server_settings(
        0, HALO_SERVER_NAME="Probe Me", HALO_SERVER_ROTATION="prisoner:ctf"))
    _until(lambda: "server: ready" in server.text(), 90, "server: ready", server)
    prober = machines("prober")
    result = subprocess.run([str(prober.folder / "bin" / "halo"), "--probe"], cwd=prober.folder,
                            env=prober.environment(HALO_NET_ADDRESS=CLIENT_ADDRESSES[0],
                                                   HALO_NET_BROADCAST=SERVER_ADDRESS, HALO_NET_ONLINE="false"),
                            stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=90)
    assert result.returncode == 0, result.stdout + result.stderr[-2000:]
    game = json.loads(result.stdout.strip().splitlines()[-1])
    assert game["ok"] and game["name"] == "Probe Me" and game["map"] == "prisoner", game
    # (an AE host sets the Delta bit, 0x04, which is also the dedicated bit: an AE client reads a Delta host as not
    # dedicated, network_game_client_advertised_game_dedicated; the master's listing says "dedicated")
    assert game["gametype"] == "ctf" and game["players"] == 0, game
    assert not (prober.folder / "bin" / "config.toml").exists(), "the probe wrote its settings"


# ---------- VPNs and linked Discord accounts (docs/dedicated_server.md; docs/master_protocol.md, "The netlist",
# "Player keys, links and passes")

MASTER_SEED = bytes(range(32))
PUBLISHER_SEED = bytes(range(32, 64))


class FakeMaster:
    """a master server on loopback: it serves a signed netlist and the passes of the player keys linked to it, and
    takes a server's publishes (ed25519_ref signs: tools/ed25519_ref.py)"""

    def __init__(self, ranges, seed=MASTER_SEED):
        import http.server
        import sys
        import threading

        sys.path.insert(0, str(ROOT / "tools"))
        import ed25519_ref

        self.ed25519 = ed25519_ref
        self.seed = seed
        self.public_key = ed25519_ref.public_key(seed)
        self.key_id = hashlib.sha256(self.public_key).digest()[:8]
        self.links = {}
        self.asked = []
        self.netlist = netlist_body(ranges, serial=1)
        master = self

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, *arguments):
                pass

            def answer(self, status, body, content_type="application/json", headers=()):
                self.send_response(status)
                self.send_header("Content-Type", content_type)
                self.send_header("Content-Length", str(len(body)))
                for name, value in headers:
                    self.send_header(name, value)
                self.end_headers()
                self.wfile.write(body)

            def do_PUT(self):
                self.rfile.read(int(self.headers.get("Content-Length", 0)))
                self.answer(200, json.dumps({"ok": True, "listed": True, "interval": 30, "ttl": 90,
                                             "probe": "skipped", "reachable": True,
                                             "master_time": int(time.time())}).encode())

            do_DELETE = do_PUT

            def do_GET(self):
                from urllib.parse import parse_qs, urlparse

                url = urlparse(self.path)
                master.asked.append(url.path)
                if url.path == "/v1/netlist":
                    signature = master.ed25519.sign(master.seed, master.netlist)
                    self.answer(200, master.netlist, "application/octet-stream", [
                        ("Signature", "ed25519=" + base64.b64encode(signature).decode()),
                        ("Signature-Key", master.key_id.hex())])
                elif url.path == "/v1/pass":
                    query = parse_qs(url.query)
                    key = bytes.fromhex(query["key"][0])
                    signed = f"opence-pass-get-1\n{master.key_id.hex()}\n{key.hex()}\n{query['ts'][0]}".encode()
                    if not master.ed25519.verify(key, signed, bytes.fromhex(query["sig"][0])):
                        self.answer(401, b'{"ok":false,"error":{"code":"bad_signature"}}')
                    elif key not in master.links:
                        self.answer(404, b'{"ok":false,"error":{"code":"not_linked"}}')
                    else:
                        self.answer(200, master.make_pass(key, *master.links[key]), "application/octet-stream")
                else:
                    self.answer(404, b'{"ok":false,"error":{"code":"not_found"}}')

        self.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        self.url = f"http://127.0.0.1:{self.server.server_address[1]}"
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()

    def masters_line(self):
        return f"{self.url} ed25519:{self.public_key.hex()}\n"

    def make_pass(self, player_key, discord_id, name, issued=None, flags=1):
        issued = int(time.time()) if issued is None else issued
        body = (b"OPCEPASS" + bytes([1, flags]) + self.key_id + player_key + struct.pack(">Q", discord_id) +
                name.encode().ljust(32, b"\0") + struct.pack(">QQ", issued, issued + 72 * 3600))
        return body + self.ed25519.sign(self.seed, b"opence-pass-1" + body)

    def close(self):
        self.server.shutdown()


def netlist_body(ranges, serial):
    """a netlist (docs/master_protocol.md) of (first, last) dotted IPv4 ranges"""
    def number(text):
        return struct.unpack(">I", socket.inet_aton(text))[0]

    pairs = sorted((number(first), number(last)) for first, last in ranges)
    return (b"OPCENET1" + bytes([1, 0, 0, 0]) + struct.pack(">QQI", int(time.time()), serial, len(pairs)) +
            b"".join(struct.pack(">II", first, last) for first, last in pairs))


def snowflake(created):
    """a Discord id made at the Unix time"""
    return (created * 1000 - 1420070400000) << 22


def linked_player(machine, master, seed, discord_id, name, masters=None):
    """a player machine with a player key linked at the master, and its pass already held (its masters.txt the
    master's, or those given, in their order)"""
    public = master.ed25519.public_key(seed)
    (machine.folder / "bin" / "masters.txt").write_text("".join(each.masters_line() for each in masters or [master]))
    (machine.folder / "bin" / "player.key").write_text(f"# test\n{seed.hex()}\n")
    (machine.folder / "bin" / f"pass-{master.key_id.hex()}.bin").write_bytes(
        master.make_pass(public, discord_id, name))
    master.links[public] = (discord_id, name)
    return machine


def test_vpn_policy_and_linked_discord_accounts(machines):
    """a server whose masters' netlist lists its clients' addresses (server.vpn_check_lan, as on the LAN): a client
    with no Discord account is refused (vpn_link_required, a join_refused event); one with a pass of an account a
    year old, linked at the master the server publishes to, is refused, joins again at once with its pass and plays
    as that verified account; a young account is refused (server.link_min_age_days), and so is the account's second
    machine (server.link_one_seat); with server.vpn_policy block a pass does not help; and a ban of the account drops
    it, its line in bans.txt a verified account's"""
    _needs_game("bloodgulch")
    clients = ("127.0.0.202", "127.0.0.203", "127.0.0.204", "127.0.0.205", "127.0.0.206")
    master = FakeMaster([("127.0.0.202", "127.0.0.206")])
    try:
        port = _free_port()
        server_machine = machines("server")
        (server_machine.folder / "bin" / "masters.txt").write_text(master.masters_line())
        server = server_machine.start(["--server"], **_server_settings(
            port, HALO_NET_BROADCAST=",".join(clients), HALO_SERVER_ROTATION="bloodgulch:slayer",
            HALO_SERVER_PUBLISHER_KEY=PUBLISHER_SEED.hex(), HALO_SERVER_VPN_CHECK_LAN="true",
            HALO_SERVER_LOG_ADDRESSES="true", HALO_SERVER_LOBBY_SECONDS=300))
        api = Api(port)
        _until(lambda: "server: ready" in server.text(), 90, "server: ready", server)
        _until(lambda: "netlist taken" in server.text(), 30, "the master's netlist", server)
        api.login()
        status, vpn = api.call("GET", "/api/vpn")
        assert status == 200 and vpn["policy"] == "link" and vpn["held"], vpn
        assert vpn["lists"][0]["ranges"] == 1 and vpn["lists"][0]["key_id"] == master.key_id.hex(), vpn
        assert api.call("POST", "/api/vpn/check", {"ip": "127.0.0.203"})[1]["by"] == "netlist"
        assert api.call("POST", "/api/vpn/check", {"ip": "127.0.0.9"})[1]["listed"] is False

        # (no Discord account: refused)
        _client(machines("unlinked"), clients[0], "Nobody")
        _until(lambda: "refused at the door (vpn_link_required)" in server.text(), 90, "the unlinked refused", server)
        refused = [event for event in api.events() if event.get("event") == "join_refused"]
        assert refused and refused[0]["reason"] == "vpn_link_required" and refused[0]["name"], refused
        assert not api.call("GET", "/api/players")[1]["players"], "a machine with no account joined"

        # (an account a year and more old: in, verified)
        old_id = snowflake(int(time.time()) - 400 * 86400)
        linked = linked_player(machines("linked"), master, bytes([7] * 32), old_id, "nelly")
        _client(linked, clients[1], "Linked")
        _until(lambda: [player for player in api.call("GET", "/api/players")[1]["players"]
                        if player["name"] == "Linked"], 90, "the linked player in", server)
        player = [p for p in api.call("GET", "/api/players")[1]["players"] if p["name"] == "Linked"][0]
        assert player["discord_verified"] is True and player["discord_name"] == "nelly", player
        assert player["discord_id"] == str(old_id), player
        assert "shows the Discord account nelly" in server.text()
        joined = [event for event in api.events() if event.get("event") == "player_joined" and event["name"] == "Linked"]
        assert joined and joined[0]["discord_verified"] is True, joined

        # (a young account: refused)
        young = linked_player(machines("young"), master, bytes([9] * 32), snowflake(int(time.time()) - 10 * 86400),
                              "fresh")
        _client(young, clients[2], "Young")
        _until(lambda: "refused at the door (vpn_link_too_new)" in server.text(), 90, "the young account refused",
               server)

        # (the linked account on a second machine while it plays: refused, server.link_one_seat)
        twin = linked_player(machines("twin"), master, bytes([7] * 32), old_id, "nelly")
        _client(twin, clients[4], "Twin")
        _until(lambda: "refused at the door (vpn_link_in_use)" in server.text(), 90, "the second seat refused", server)

        # (server.vpn_policy block: a pass does not help)
        status, answer = api.call("PUT", "/api/settings", {"server.vpn_policy": "block"})
        assert answer["applied"] == ["server.vpn_policy"], answer
        blocked = linked_player(machines("blocked"), master, bytes([11] * 32),
                                snowflake(int(time.time()) - 800 * 86400), "elder")
        _client(blocked, clients[3], "Blocked")
        _until(lambda: "refused at the door (vpn_blocked)" in server.text(), 90, "the VPN blocked", server)
        assert [p["name"] for p in api.call("GET", "/api/players")[1]["players"]] == ["Linked"]

        # (the account banned: dropped, and its line in bans.txt a verified one; last, since every machine of
        # this test has the same hardware id, which the ban keeps out too)
        status, answer = api.call("POST", "/api/bans", {"discord_id": str(old_id), "reason": "test"})
        assert status == 200 and answer["ok"], answer
        _until(lambda: not [p for p in api.call("GET", "/api/players")[1]["players"] if p["name"] == "Linked"], 30,
               "the banned account dropped", server)
        bans = api.call("GET", "/api/bans")[1]["bans"]
        assert bans[-1]["discord_id"] == str(old_id) and bans[-1]["discord_verified"] is True, bans
        assert f"discord_id={old_id}\t" in (server_machine.folder / "data" / "bans.txt").read_text()
        counts = api.call("GET", "/api/vpn")[1]["refused"]
        assert counts["vpn_link_required"] >= 1 and counts["vpn_link_too_new"] >= 1 and counts["vpn_blocked"] >= 1 and \
            counts["vpn_link_in_use"] >= 1, counts
        server.send("quit")
        assert server.wait(15) == 0, server.text()[-3000:]
    finally:
        master.close()


def test_vpn_policy_off_and_no_list(machines):
    """server.vpn_fail closed refuses machines from the internet's (here the LAN's, server.vpn_check_lan) while no
    netlist is held; vpn_allow.txt lets one in; server.vpn_policy off lets everyone in"""
    _needs_game("bloodgulch")
    port = _free_port()
    server_machine = machines("server")
    (server_machine.folder / "data" / "vpn_allow.txt").write_text("# the second client\n127.0.0.203\n")
    server = server_machine.start(["--server"], **_server_settings(
        port, HALO_SERVER_ROTATION="bloodgulch:slayer", HALO_SERVER_VPN_CHECK_LAN="true",
        HALO_SERVER_VPN_FAIL="closed", HALO_SERVER_MASTER_PUBLISH="false", HALO_SERVER_LOBBY_SECONDS=300))
    api = Api(port)
    _until(lambda: "server: ready" in server.text(), 90, "server: ready", server)
    api.login()
    assert api.call("GET", "/api/vpn")[1]["held"] is False
    first = _client(machines("first"), CLIENT_ADDRESSES[0], "First")
    _until(lambda: "refused at the door (vpn_blocked)" in server.text(), 90, "the first refused", server)
    _client(machines("second"), CLIENT_ADDRESSES[1], "Second")
    _until(lambda: [p for p in api.call("GET", "/api/players")[1]["players"] if p["name"] == "Second"], 90,
           "the allowed machine in", server)
    status, answer = api.call("PUT", "/api/settings", {"server.vpn_policy": "off"})
    assert answer["applied"] == ["server.vpn_policy"], answer
    # (a refused test client gives up: another, of the same address, joins)
    first.stop()
    _client(machines("again"), CLIENT_ADDRESSES[0], "Again")
    _until(lambda: [p for p in api.call("GET", "/api/players")[1]["players"] if p["name"] == "Again"], 120,
           "a machine of that address in once the policy is off", server)
    server.send("quit")
    assert server.wait(15) == 0, server.text()[-3000:]


def test_vpn_link_with_one_of_a_servers_masters(machines):
    """a server that publishes to two masters takes a pass of either; a player linked with the second alone (its
    masters.txt naming both, the first before) joins with that one's pass: the first's, which it has none of, is
    not asked for"""
    _needs_game("bloodgulch")
    first, second = FakeMaster([("127.0.0.202", "127.0.0.203")]), \
        FakeMaster([("127.0.0.202", "127.0.0.203")], seed=bytes(range(64, 96)))
    try:
        port = _free_port()
        server_machine = machines("server")
        (server_machine.folder / "bin" / "masters.txt").write_text(first.masters_line() + second.masters_line())
        server = server_machine.start(["--server"], **_server_settings(
            port, HALO_SERVER_ROTATION="bloodgulch:slayer", HALO_SERVER_PUBLISHER_KEY=PUBLISHER_SEED.hex(),
            HALO_SERVER_VPN_CHECK_LAN="true", HALO_SERVER_LOG_ADDRESSES="true", HALO_SERVER_LOBBY_SECONDS=300))
        api = Api(port)
        _until(lambda: "server: ready" in server.text(), 90, "server: ready", server)
        _until(lambda: server.text().count("netlist taken") == 2, 30, "both masters' netlists", server)
        api.login()
        assert len(api.call("GET", "/api/vpn")[1]["lists"]) == 2
        linked = linked_player(machines("linked"), second, bytes([5] * 32), snowflake(int(time.time()) - 900 * 86400),
                               "second_only", masters=[first, second])
        _client(linked, CLIENT_ADDRESSES[0], "Linked")
        _until(lambda: [p for p in api.call("GET", "/api/players")[1]["players"] if p["name"] == "Linked"], 90,
               "the player linked with the second master in", server)
        player = [p for p in api.call("GET", "/api/players")[1]["players"] if p["name"] == "Linked"][0]
        assert player["discord_verified"] and player["discord_name"] == "second_only", player
        assert "joining again with " + second.url + "'s pass" in linked.text(), linked.text()[-2000:]
        server.send("quit")
        assert server.wait(15) == 0, server.text()[-3000:]
    finally:
        first.close()
        second.close()


# ---------- admin features: API keys, warned restarts, backups (0.7.7)

def _ws_open(port, bearer=None):
    """a WebSocket to /ws (the RCON's subprotocol, signed in at the upgrade by a Bearer key if given): (socket, buffer)"""
    connection = socket.create_connection(("127.0.0.1", port), timeout=10)
    extra = f"Authorization: Bearer {bearer}\r\n".encode() if bearer else b""
    connection.sendall(b"GET /ws HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n" + extra +
                       b"Sec-WebSocket-Version: 13\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                       b"Sec-WebSocket-Protocol: opence-rcon-1\r\n\r\n")
    data = b""
    while b"\r\n\r\n" not in data:
        data += connection.recv(4096)
    assert data.startswith(b"HTTP/1.1 101"), data
    return connection, data.split(b"\r\n\r\n", 1)[1]


def _ws_send(connection, message):
    payload = json.dumps(message).encode()
    mask = b"\x01\x02\x03\x04"
    assert len(payload) < 126
    connection.sendall(bytes([0x81, 0x80 | len(payload)]) + mask +
                       bytes(byte ^ mask[index % 4] for index, byte in enumerate(payload)))


def _ws_receive(connection, buffer):
    """the next text frame as JSON (or the close frame as {"closed": code}); (message, rest of buffer)"""
    def need(count):
        nonlocal buffer
        while len(buffer) < count:
            chunk = connection.recv(4096)
            if not chunk:
                raise ConnectionError("closed")
            buffer += chunk
    need(2)
    opcode, length = buffer[0] & 15, buffer[1] & 127
    offset = 2
    if length == 126:
        need(4)
        length, offset = struct.unpack(">H", buffer[2:4])[0], 4
    need(offset + length)
    payload, buffer = buffer[offset:offset + length], buffer[offset + length:]
    if opcode == 8:
        return {"closed": struct.unpack(">H", payload[:2])[0] if len(payload) >= 2 else 0}, buffer
    return json.loads(payload), buffer


def _key_server(machines, port, **settings):
    server = machines("server").start(["--server"], **_server_settings(port, HALO_SERVER_ROTATION="bloodgulch:slayer",
                                                                       **settings))
    _until(lambda: "server: ready" in server.text(), 90, "server: ready", server)
    return server


def test_api_keys(machines):
    """a key made by the API is shown once, kept as a hash, and works as a Bearer token on the API and the RCON with
    its role alone; a revoked key fails at once and closes its console; an allow-list refuses other addresses; a
    moderator key cannot make keys; the audit lines name the key; wrong keys lock the address out; the console
    commands make, list and revoke keys"""
    _needs_game("bloodgulch")
    port = _free_port()
    server = _key_server(machines, port)
    admin = Api(port)
    admin.login()

    # (made: the secret once; listed: without it)
    status, made = admin.call("POST", "/api/keys", {"name": "dash", "role": "viewer"})
    assert status == 200 and made["ok"] and re.fullmatch(r"aek_[0-9a-f]{8}_[0-9a-f]{48}", made["key"]), made
    status, listed = admin.call("GET", "/api/keys")
    assert status == 200 and [key["name"] for key in listed["keys"]] == ["dash"], listed
    assert made["key"] not in json.dumps(listed) and "key" not in listed["keys"][0], listed
    keys_file = server.folder / "data" / "api_keys.txt"
    assert keys_file.is_file() and made["key"].split("_")[2] not in keys_file.read_text(), "the secret is kept hashed"
    assert (keys_file.stat().st_mode & 0o077) == 0, "api_keys.txt is its user's alone"
    assert admin.call("POST", "/api/keys", {"name": "dash", "role": "viewer"})[0] == 409
    assert admin.call("POST", "/api/keys", {"name": "Bad Name", "role": "viewer"})[0] == 400
    assert admin.call("POST", "/api/keys", {"name": "boss", "role": "owner"})[0] == 400
    assert admin.call("POST", "/api/keys", {"name": "late", "role": "viewer", "days": 5000})[0] == 400
    assert admin.call("POST", "/api/keys", {"name": "fenced", "role": "viewer", "allow": "300.1.1.1/8"})[0] == 400

    viewer = Api(port)
    viewer.token = made["key"]
    status, answer = viewer.call("GET", "/api/status")
    assert status == 200 and answer["name"] == "Test Server", answer
    status, answer = viewer.call("GET", "/api/session")
    assert status == 200 and answer["role"] == "viewer", answer
    assert viewer.call("GET", "/api/keys")[0] == 403
    assert viewer.call("POST", "/api/say", {"text": "x"})[0] == 403
    assert "api_keys" in viewer.status()["capabilities"]

    # (a moderator key moderates, and its lines in the log name it)
    status, moderator_key = admin.call("POST", "/api/keys", {"name": "mod-bot", "role": "moderator", "days": 30,
                                                             "allow": "127.0.0.0/8,10.0.0.0/8"})
    assert status == 200, moderator_key
    mod = Api(port)
    mod.token = moderator_key["key"]
    status, answer = mod.call("POST", "/api/say", {"text": "hello from a key"})
    assert status == 200 and answer["ok"], answer
    assert mod.call("POST", "/api/keys", {"name": "x", "role": "viewer"})[0] == 403
    lines = admin.call("GET", "/api/log?limit=500")[1]["lines"]
    assert any("key:mod-bot" in line["text"] and "say" in line["text"] for line in lines), lines[-8:]
    assert all(moderator_key["key"] not in line["text"] and made["key"] not in line["text"] for line in lines), \
        "a secret is in the log"
    listing = {key["name"]: key for key in admin.call("GET", "/api/keys")[1]["keys"]}
    assert listing["mod-bot"]["expires"] > time.time() and listing["mod-bot"]["allow"].startswith("127."), listing
    # (an address the list does not allow)
    assert admin.call("POST", "/api/keys", {"name": "fenced", "role": "viewer", "allow": "10.0.0.0/8"})[0] == 200
    fenced = Api(port)
    fenced.token = admin.call("POST", "/api/keys", {"name": "fenced2", "role": "viewer", "allow": "10.1.2.3"})[1]["key"]
    assert fenced.call("GET", "/api/status")[0] == 401

    # (the RCON: the key signs in, runs a command, and is closed when the key is revoked)
    connection, buffer = _ws_open(port)
    hello, buffer = _ws_receive(connection, buffer)
    # (the hello holds no build or capabilities before the sign-in since the security fixes of preview 2.1)
    assert hello["type"] == "hello" and "capabilities" not in hello, hello
    _ws_send(connection, {"type": "auth", "key": made["key"]})
    message, buffer = _ws_receive(connection, buffer)
    assert message["type"] == "auth" and message["ok"] and message["role"] == "viewer", message
    _ws_send(connection, {"type": "command", "id": 1, "line": "sv_version"})
    message, buffer = _ws_receive(connection, buffer)
    assert message["type"] == "reply" and message["ok"], message
    status, answer = admin.call("DELETE", "/api/keys/dash")
    assert status == 200 and answer["ok"], answer
    assert viewer.call("GET", "/api/status")[0] == 401, "a revoked key still works"
    connection.settimeout(8)
    closed = {}
    for _ in range(5):
        message, buffer = _ws_receive(connection, buffer)
        if "closed" in message:
            closed = message
            break
    assert closed.get("closed") == 1008, closed
    connection.close()
    # (a socket signed in by a Bearer key at the upgrade is closed when that key is revoked too)
    bearer = admin.call("POST", "/api/keys", {"name": "upgraded", "role": "viewer"})[1]["key"]
    connection, buffer = _ws_open(port, bearer)
    message, buffer = _ws_receive(connection, buffer)
    assert message["type"] == "hello", message
    message, buffer = _ws_receive(connection, buffer)
    assert message["type"] == "auth" and message["ok"], message
    assert admin.call("DELETE", "/api/keys/upgraded")[0] == 200
    connection.settimeout(8)
    closed = {}
    for _ in range(5):
        message, buffer = _ws_receive(connection, buffer)
        if "closed" in message:
            closed = message
            break
    assert closed.get("closed") == 1008, closed
    connection.close()
    assert admin.call("DELETE", "/api/keys/dash")[0] == 404
    assert "dash" not in keys_file.read_text()

    # (the console: make, list, revoke; the secret is in the console's reply, not in the log)
    server.send("sv_key_add console-key admin 7")
    token = _until(lambda: re.search(r"aek_[0-9a-f]{8}_[0-9a-f]{48}", server.text()), 20, "the console's key", server)
    server.send("sv_keys")
    _until(lambda: "console-key" in server.text().split(token.group(0))[-1], 20, "sv_keys", server)
    console = Api(port)
    console.token = token.group(0)
    assert console.call("GET", "/api/keys")[1]["keys"], "an admin key lists the keys"
    log_text = json.dumps(admin.call("GET", "/api/log?limit=1000")[1])
    assert token.group(0) not in log_text and token.group(0).split("_")[2] not in log_text, "a secret is in the log"
    server.send("sv_key_remove console-key")
    _until(lambda: console.call("GET", "/api/status")[0] == 401, 20, "the console key revoked")

    # (wrong keys count toward the address's lockout, as wrong passwords do)
    wrong = Api(port)
    wrong.token = "aek_00000000_" + "0" * 48
    codes = [wrong.call("GET", "/api/status")[0] for _ in range(8)]
    assert set(codes) == {401}, codes
    status, answer = Api(port).call("POST", "/api/login", {"password": PASSWORD}, token=False)
    assert status == 429 and answer["error"]["code"] == "locked", (status, answer)
    # (locked out: even a right key is refused until the lock ends)
    assert mod.call("GET", "/api/status")[0] == 401


def test_restart_warning_and_cancel(machines):
    """sv_restart with a delay warns the players in chat (at the start, then at 10 and 5 seconds), exits with code 5,
    and sv_restart cancel stops a countdown before it ends; the API takes seconds; the daily restart begins its
    own countdown at the time of day"""
    _needs_game("bloodgulch")
    port = _free_port()
    server = _key_server(machines, port, HALO_SERVER_RESTART_WARNING_SECONDS=5)
    api = Api(port)
    # (the owner's console: standard input)
    client = _client(machines("client"), CLIENT_ADDRESSES[0], "Watcher")
    api.login()
    _until(lambda: (api.status() or {}).get("state") == "playing", 120, "the game", server)

    server.send("sv_restart 40 maintenance")
    _until(lambda: "Server restarting in 40 seconds: maintenance." in server.text(), 20, "the first notice", server)
    server.send("sv_restart cancel")
    _until(lambda: "restart is cancelled" in server.text(), 20, "the cancel", server)
    time.sleep(2)
    assert server.process.poll() is None, "a cancelled restart stopped the server"
    assert "Server restarting in 10 seconds" not in server.text()

    server.send("sv_restart 14 updating")
    _until(lambda: "Server restarting in 10 seconds: updating." in server.text(), 20, "the 10 s notice", server)
    _until(lambda: "Server restarting in 5 seconds: updating." in server.text(), 20, "the 5 s notice", server)
    assert server.wait(40) == 5, server.text()[-3000:]
    text = server.text()
    assert "Server restarting in 14 seconds: updating." in text and "stopping (sv_restart)" in text, text[-3000:]
    # (the client's own log shows the chat notice)
    client_log = (client.folder / "data" / "debug.txt").read_text(errors="replace") + client.text()
    assert "Server restarting in" in client_log, "the client saw no notice"


def test_restart_by_api_with_seconds_and_default_warning(machines):
    """POST /api/stop with seconds counts down and cancels; with the setting at 0, a bare sv_stop stops at once"""
    _needs_game("bloodgulch")
    port = _free_port()
    owner_server = machines("server").start(["--server"], **_server_settings(
        port, HALO_SERVER_ROTATION="bloodgulch:slayer", HALO_SERVER_WEB_OWNER_PASSWORD="owner-password-1",
        HALO_SERVER_RESTART_WARNING_SECONDS=0))
    _until(lambda: "server: ready" in owner_server.text(), 90, "server: ready", owner_server)
    owner = Api(port)
    status, answer = owner.call("POST", "/api/login", {"password": "owner-password-1"}, token=False)
    owner.token = answer["token"]
    assert owner.call("POST", "/api/stop", {"seconds": 5000})[0] == 400
    status, answer = owner.call("POST", "/api/stop", {"seconds": 30, "restart": True, "notice": "api test"})
    assert status == 200 and answer["ok"], answer
    _until(lambda: "Server restarting in 30 seconds: api test." in owner_server.text(), 20, "the countdown", owner_server)
    status, answer = owner.call("POST", "/api/stop", {"cancel": True})
    assert status == 200 and answer["ok"], answer
    assert "restart is cancelled" in owner_server.text()
    time.sleep(1)
    assert owner_server.process.poll() is None
    # (0 seconds: at once; the default with the setting at 0 is also at once)
    owner_server.send("sv_stop")
    assert owner_server.wait(40) == 0, owner_server.text()[-2000:]


def test_daily_restart(machines):
    """server.restart_daily begins the warned restart at the time of day (here the next minute but one)"""
    _needs_game("bloodgulch")
    port = _free_port()
    now = time.localtime()
    seconds = now.tm_hour * 3600 + now.tm_min * 60 + now.tm_sec
    target = ((seconds + 100) // 60 + 1) * 60 % 86400
    stamp = f"{target // 3600:02d}:{target % 3600 // 60:02d}"
    server = _key_server(machines, port, HALO_SERVER_RESTART_DAILY=stamp, HALO_SERVER_RESTART_WARNING_SECONDS=6)
    assert server.wait(260) == 5, server.text()[-3000:]
    text = server.text()
    assert "Server restarting in" in text and "daily restart" in text, text[-3000:]


def test_backups(machines):
    """the server copies config.toml, bans.txt and api_keys.txt to the data folder's backups folder before it writes
    them again, keeps the last server.backups_keep of each, and sv_backups lists them"""
    _needs_game("bloodgulch")
    port = _free_port()
    server = _key_server(machines, port, HALO_SERVER_BACKUPS_KEEP=2)
    backups = server.folder / "data" / "backups"
    # (a ban file: the first write has nothing to copy, the next copies the first's; one second apart: a stamp each)
    for index in range(4):
        server.send(f"sv_ban_address 198.51.100.{index + 1} 1h test")
        time.sleep(1.2)
    server.send("sv_key_add one viewer")
    time.sleep(1.2)
    server.send("sv_key_add two viewer")
    time.sleep(1.2)
    server.send("sv_motd backup test")
    time.sleep(1.2)
    server.send("sv_motd backup test two")
    time.sleep(1.2)
    server.send("sv_motd backup test three")
    time.sleep(1.2)
    found = sorted(path.name for path in backups.iterdir())
    assert any(name.startswith("bans.txt.") for name in found), found
    assert any(name.startswith("api_keys.txt.") for name in found), found
    assert any(name.startswith("config.toml.") for name in found), found
    for base in ("bans.txt", "config.toml"):
        mine = [name for name in found if name.startswith(base + ".")]
        assert 1 <= len(mine) <= 2, (base, found)
        assert all(re.fullmatch(re.escape(base) + r"\.\d{8}-\d{6}", name) for name in mine), mine
    # (a backup is the file as it was: the newest of bans.txt holds fewer bans than the file now)
    newest = sorted(name for name in found if name.startswith("bans.txt."))[-1]
    assert (backups / newest).read_text().count("\n") < (server.folder / "data" / "bans.txt").read_text().count("\n")
    server.send("sv_backups")
    _until(lambda: "backups/bans.txt." in server.text().split("sv_backups")[-1], 20, "sv_backups", server)
    # (the settings' lock is not held for ever by a backup config_write makes of a config.toml that changed: the server
    # still answers, over the API too, and a setting still changes)
    assert len([name for name in found if name.startswith("config.toml.")]) == 2, found
    api = Api(port)
    api.login()
    assert api.status() is not None, "the web page does not answer after the backups"
    status, answer = api.call("POST", "/api/command", {"line": "sv_motd after the backups"})
    assert status == 200 and answer["ok"], (status, answer)


# ---------- moderation depth and abuse limits (0.7.7: admins.txt, command_access.txt, the whitelist, bans by network and
# name, alerts, event hooks, the claim)

def _basic(port, user, secret, method="GET", path="/api/session", body=None):
    """a call with an admin row's name and secret as Basic auth: (status, JSON)"""
    request = urllib.request.Request(f"http://127.0.0.1:{port}{path}", method=method,
                                     data=json.dumps(body).encode() if body is not None else None)
    request.add_header("Authorization", "Basic " + base64.b64encode(f"{user}:{secret}".encode()).decode())
    if body is not None:
        request.add_header("Content-Type", "application/json")
    try:
        with urllib.request.urlopen(request, timeout=10) as answer:
            return answer.status, json.loads(answer.read() or b"null")
    except urllib.error.HTTPError as failure:
        text = failure.read()
        try:
            return failure.code, json.loads(text)
        except ValueError:
            return failure.code, text.decode(errors="replace")


def _secret_after(server, marker, pattern=r"\b[0-9a-f]{40}\b"):
    """the secret the console printed after a marker line"""
    found = _until(lambda: re.search(pattern, server.text().split(marker)[-1]) if marker in server.text() else None, 30,
                   "the secret", server)
    return found.group(0)


def _players(api):
    status, answer = api.call("GET", "/api/players")
    return answer["players"] if status == 200 else []


def test_admin_rows_immunity_and_command_access(machines):
    """an admin row's name and secret sign in on the API (Basic) and the RCON with the row's role alone; the secret
    is kept as a hash in a private file; a network row gives its players the role and immunity, so a moderator cannot
    kick, ban or mute them (the owner can); command_access.txt moves a command to another role; wrong logins lock the
    address out"""
    _needs_game("bloodgulch")
    port = _free_port()
    server = _key_server(machines, port, HALO_SERVER_BOTS_FILL="off")
    admin = Api(port)
    admin.login()

    server.send("sv_admin_add name:ops moderator 5")
    secret = _secret_after(server, "shown this once")
    admins = server.folder / "data" / "admins.txt"
    assert admins.is_file() and (admins.stat().st_mode & 0o077) == 0, "admins.txt is its user's alone"
    assert secret not in admins.read_text() and "name:ops\tmoderator\t5" in admins.read_text()
    assert _basic(port, "ops", secret) [0] == 200
    status, answer = _basic(port, "ops", secret)
    assert answer["role"] == "moderator", answer
    assert _basic(port, "ops", secret, "GET", "/api/keys")[0] == 403, "a moderator row reads the keys"
    assert _basic(port, "ops", "0" * 40)[0] == 401 and _basic(port, "nobody", secret)[0] == 401
    server.send("sv_admins")
    _until(lambda: "name:ops  moderator  immunity 5" in server.text(), 20, "sv_admins", server)
    assert secret not in "".join(line["text"] for line in admin.call("GET", "/api/log?limit=800")[1]["lines"])

    # (the RCON: name and secret)
    connection, buffer = _ws_open(port)
    _, buffer = _ws_receive(connection, buffer)
    _ws_send(connection, {"type": "auth", "name": "ops", "secret": secret})
    message, buffer = _ws_receive(connection, buffer)
    assert message["type"] == "auth" and message["ok"] and message["role"] == "moderator", message
    connection.close()

    # (a network row: its players are immune to a moderator; the owner's console may act)
    server.send(f"sv_admin_add cidr:{_BASE}.0/24 admin 50")
    _until(lambda: f"admin row cidr:{_BASE}.0/24 made" in server.text(), 20, "the network row", server)
    client = _client(machines("client"), CLIENT_ADDRESSES[0], "Tester")
    _until(lambda: _players(admin), 120, "the client", server)
    slot = _players(admin)[0]["slot"]
    status, answer = _basic(port, "ops", secret, "POST", f"/api/players/{slot}/kick", {"name": "Tester"})
    assert not (status == 200 and answer.get("ok")), (status, answer)
    status, answer = _basic(port, "ops", secret, "POST", f"/api/players/{slot}/ban", {"name": "Tester", "duration": "1h"})
    assert not (status == 200 and answer.get("ok")), (status, answer)
    time.sleep(2)
    assert _players(admin), "an immune player was dropped by a moderator"
    # (taken out, and command_access.txt lifting kick to the admin role: the moderator is refused for its role)
    server.send(f"sv_admin_remove cidr:{_BASE}.0/24")
    _until(lambda: "taken out" in server.text(), 20, "the row out", server)
    (server.folder / "data" / "command_access.txt").write_text("# test\nkick admin\n")
    server.send("sv_reload")
    server.send("sv_access")
    _until(lambda: "kick needs the admin role" in server.text(), 20, "sv_access", server)
    status, answer = _basic(port, "ops", secret, "POST", f"/api/players/{slot}/kick", {"name": "Tester"})
    assert not (status == 200 and answer.get("ok")), (status, answer)
    assert _players(admin), "a kick lifted to the admin role went through for a moderator"
    # (and back down: command_access.txt names no kick any more, the moderator may)
    (server.folder / "data" / "command_access.txt").write_text("sv_players moderator\n")
    server.send("sv_reload")
    time.sleep(1)
    status, answer = _basic(port, "ops", secret, "POST", f"/api/players/{slot}/kick", {"name": "Tester"})
    assert status == 200 and answer["ok"], (status, answer)
    _until(lambda: not _players(admin), 30, "the kicked player gone", server)

    # (wrong logins count toward the address's lockout)
    codes = [_basic(port, "ops", "1" * 40)[0] for _ in range(8)]
    assert set(codes) == {401}, codes
    status, answer = Api(port).call("POST", "/api/login", {"password": PASSWORD}, token=False)
    assert status == 429 and answer["error"]["code"] == "locked", (status, answer)
    assert _basic(port, "ops", secret)[0] == 401, "a locked address signed in"


def test_whitelist_and_reserved_slots(machines):
    """with the whitelist on, only a listed address joins (an admin row's network always does); reserved slots keep the
    last slots for admin rows"""
    _needs_game("bloodgulch")
    port = _free_port()
    server = _key_server(machines, port, HALO_SERVER_WHITELIST="true", HALO_SERVER_BOTS_FILL="off")
    api = Api(port)
    api.login()
    first = _client(machines("first"), CLIENT_ADDRESSES[0], "Unlisted")
    _until(lambda: "not on the whitelist" in server.text(), 90, "a refusal at the door", server)
    time.sleep(3)
    assert not _players(api), "an unlisted machine joined"
    server.send(f"sv_whitelist add ip:{CLIENT_ADDRESSES[0]}")
    _until(lambda: f"ip:{CLIENT_ADDRESSES[0]} listed" in server.text(), 20, "the entry", server)
    assert f"ip:{CLIENT_ADDRESSES[0]}" in (server.folder / "data" / "whitelist.txt").read_text()
    assert server.text().count("sv_whitelist: ") == 0
    first.stop()
    _client(first, CLIENT_ADDRESSES[0], "Listed")
    _until(lambda: _players(api), 120, "the listed client", server)
    server.send("sv_whitelist")
    _until(lambda: f"ip:{CLIENT_ADDRESSES[0]}" in server.text().split("the whitelist is ON")[-1], 20, "the list", server)
    # (a bad entry is refused; removing the entry takes it off)
    server.send("sv_whitelist add ip:1.2.3")
    _until(lambda: "sv_whitelist: an entry is ip:" in server.text(), 20, "a bad entry refused", server)
    server.send(f"sv_whitelist remove ip:{CLIENT_ADDRESSES[0]}")
    _until(lambda: "taken off" in server.text(), 20, "the entry off", server)
    # (an admin row's network joins a whitelist that lists no one)
    server.send(f"sv_admin_add cidr:{CLIENT_ADDRESSES[1]} moderator")
    _until(lambda: "made" in server.text().split("taken off")[-1], 20, "the row", server)
    second = _client(machines("second"), CLIENT_ADDRESSES[1], "Staff")
    _until(lambda: any(player["name"] == "Staff" for player in _players(api)), 120, "the admin's machine", server)


def test_reserved_slots_keep_room_for_admin_rows(machines):
    """with 2 slots and 1 reserved, a first machine joins and a second is told the game is full, unless an admin row
    holds its address (or its key signed in with a role on this server before)"""
    _needs_game("bloodgulch")
    port = _free_port()
    server = _key_server(machines, port, HALO_SERVER_BOTS_FILL="off", HALO_SERVER_MAX_PLAYERS=2,
                         HALO_SERVER_RESERVED_SLOTS=1)
    api = Api(port)
    api.login()
    _client(machines("first"), CLIENT_ADDRESSES[0], "First")
    _until(lambda: _players(api), 120, "the first client", server)
    second = _client(machines("second"), CLIENT_ADDRESSES[1], "Second")
    _until(lambda: "kept for admins" in server.text(), 90, "the second refused", server)
    assert [player["name"] for player in _players(api)] == ["First"], _players(api)
    server.send(f"sv_admin_add cidr:{CLIENT_ADDRESSES[1]} moderator")
    _until(lambda: "made" in server.text().split("kept for admins")[-1], 20, "the row", server)
    second.stop()
    _client(second, CLIENT_ADDRESSES[1], "Second")
    _until(lambda: len(_players(api)) == 2, 120, "the admin's machine in the reserved slot", server)


def test_bans_by_network_and_by_name_and_json(machines, tmp_path):
    """a banned network is refused at the door and its players dropped; a banned name is dropped within seconds; the
    bans export and import as JSON (the API and the console) with duplicates skipped and bad rows refused"""
    _needs_game("bloodgulch")
    port = _free_port()
    server = _key_server(machines, port, HALO_SERVER_BOTS_FILL="off")
    api = Api(port)
    api.login()
    server.send("sv_ban_range 203.0.113.0/24 1h a test range")
    _until(lambda: "ban added" in server.text(), 20, "the range ban", server)
    server.send("sv_ban_name Sk8*")
    _until(lambda: server.text().count("ban added") == 2, 20, "the name ban", server)
    server.send("sv_ban_range 0.0.0.0/0")
    _until(lambda: "sv_ban_range: " in server.text(), 20, "the whole internet refused", server)
    server.send("sv_ban_name *")
    _until(lambda: "sv_ban_name: a name pattern is" in server.text(), 20, "a bare * refused", server)
    status, exported = api.call("GET", "/api/bans/export")
    assert status == 200 and exported["ok"], exported
    entries = {(entry["ip"], entry["name_pattern"]) for entry in exported["bans"]}
    assert ("203.0.113.0/24", "") in entries and ("", "Sk8*") in entries, entries
    # (import: one new, one there already, one bad, one over)
    body = {"bans": [
        {"ip": "198.51.100.0/25", "reason": "imported", "until": None},
        {"ip": "203.0.113.0/24"},
        {"ip": "999.9.9.9"},
        {"name_pattern": "old*", "until": 5},
        {"hwid": "abc123def456", "players": "x"},
    ]}
    status, answer = api.call("POST", "/api/bans/import", body)
    assert status == 200 and (answer["added"], answer["skipped"], answer["refused"]) == (2, 2, 1), answer
    assert api.call("POST", "/api/bans/import", {"nothing": 1})[0] == 400
    assert Api(port).call("GET", "/api/bans/export")[0] == 401
    bans_text = (server.folder / "data" / "bans.txt").read_text()
    assert "\tip=198.51.100.0/25\t" in bans_text and "\tnamepat=Sk8*" in bans_text
    # (the console: export to a file in the data folder, import it back: nothing new; a bad file name refused)
    server.send("sv_bans_export mine.json")
    _until(lambda: "bans written to the data folder's mine.json" in server.text(), 20, "the export file", server)
    saved = json.loads((server.folder / "data" / "mine.json").read_text())
    assert len(saved["bans"]) == len(api.call("GET", "/api/bans/export")[1]["bans"])
    server.send("sv_bans_import mine.json")
    _until(lambda: "bans imported: 0 added" in server.text(), 20, "the import file", server)
    server.send("sv_bans_import ../mine.json")
    _until(lambda: "sv_bans_import: the file is a name ending in .json" in server.text(), 20, "a path refused", server)

    # (a network ban refuses the client's machine at the door; taking it out lets it in)
    server.send(f"sv_ban_range {_BASE}.202/31 1h clients")
    _until(lambda: server.text().count("ban added") >= 3, 20, "the loopback ban", server)
    client = _client(machines("client"), CLIENT_ADDRESSES[0], "Tester")
    _until(lambda: "banned network was refused" in server.text(), 90, "the door", server)
    time.sleep(3)
    assert not _players(api), "a machine of a banned network joined"
    number = [ban["n"] for ban in api.call("GET", "/api/bans")[1]["bans"] if ban["ip"] == f"{_BASE}.202/31"][0]
    status, answer = api.call("DELETE", f"/api/bans/{number}")
    assert status == 200 and answer["ok"], answer
    client.stop()
    _client(client, CLIENT_ADDRESSES[0], "Sk8er")
    # (the name ban drops the player within seconds of its joining)
    _until(lambda: "a banned name" in server.text(), 120, "the name ban at work", server)
    _until(lambda: not _players(api), 30, "the banned name gone", server)


def test_join_flood_from_one_address(machines):
    """more machines from one address than server.max_machines_per_address are refused (address_full) while the
    first one stays"""
    _needs_game("bloodgulch")
    port = _free_port()
    server = _key_server(machines, port, HALO_SERVER_BOTS_FILL="off", HALO_SERVER_MAX_MACHINES_PER_ADDRESS=1)
    api = Api(port)
    api.login()
    _client(machines("first"), CLIENT_ADDRESSES[0], "First")
    _until(lambda: _players(api), 120, "the first machine", server)
    for index in range(2):
        _client(machines(f"flood{index}"), CLIENT_ADDRESSES[0], f"Flood{index}")
    _until(lambda: "machines of its address play already" in server.text(), 120, "the refusal", server)
    time.sleep(5)
    assert [player["name"] for player in _players(api)] == ["First"], _players(api)


def test_event_hooks_run_commands_with_safe_fields(machines):
    """a hooks file maps events to server commands: player_joined runs a say with the player's name put in, with no way
    for a name to add a command; unknown events and fields are ignored"""
    _needs_game("bloodgulch")
    port = _free_port()
    machine = machines("server")
    (machine.folder / "data").mkdir(parents=True, exist_ok=True)
    (machine.folder / "data" / "on_event.txt").write_text(
        "# hooks\nplayer_joined -> say Welcome {name}; say second {unknown}!\nnothing_happens -> sv_stop now\n"
        "bad line\n")
    machine.start(["--server"], **_server_settings(port, HALO_SERVER_ROTATION="bloodgulch:slayer",
                                                   HALO_SERVER_BOTS_FILL="off", HALO_SERVER_ON_EVENT="on_event.txt"))
    _until(lambda: "server: ready" in machine.text(), 90, "server: ready", machine)
    api = Api(port)
    api.login()
    _client(machines("client"), CLIENT_ADDRESSES[0], "Hooky")
    _until(lambda: "hook: say Welcome Hooky" in machine.text(), 120, "the hook", machine)
    assert "hook: say second !" in machine.text()
    assert "nothing_happens" not in machine.text() and machine.process.poll() is None
    # (the setting is the owner's: the API's admin cannot point it at another file)
    status, answer = api.call("POST", "/api/command", {"line": "sv_set server.on_event other.txt"})
    assert not (status == 200 and answer.get("ok")), (status, answer)


class _Hook:
    """a webhook receiver on loopback: the JSON bodies posted to it"""

    def __init__(self):
        import http.server
        import threading
        self.bodies = []
        outer = self

        class Handler(http.server.BaseHTTPRequestHandler):
            def do_POST(self):
                outer.bodies.append(json.loads(self.rfile.read(int(self.headers["Content-Length"]))))
                self.send_response(200)
                self.end_headers()

            def log_message(self, *arguments):
                pass

        self.server = http.server.HTTPServer(("127.0.0.1", 0), Handler)
        self.port = self.server.server_address[1]
        threading.Thread(target=self.server.serve_forever, daemon=True).start()

    def close(self):
        self.server.shutdown()


def test_alert_webhook_restart_loop_and_low_disk(machines):
    """with a webhook set the server posts JSON alerts: a restart loop (three starts within ten minutes) at its start and
    a low disk (the threshold set above any disk); none without a webhook"""
    _needs_game("bloodgulch")
    hook = _Hook()
    try:
        port = _free_port()
        machine = machines("server")
        (machine.folder / "data").mkdir(parents=True, exist_ok=True)
        now = int(time.time())
        (machine.folder / "data" / "server-starts.log").write_text(f"{now - 120}\n{now - 60}\n")
        machine.start(["--server"], **_server_settings(
            port, HALO_SERVER_ROTATION="bloodgulch:slayer", HALO_SERVER_BOTS_FILL="off",
            HALO_SERVER_ALERT_WEBHOOK=f"http://127.0.0.1:{hook.port}/hook", HALO_SERVER_ALERT_DISK_MB=900000000,
            HALO_SERVER_NAME="Alert Server"))
        _until(lambda: "server: ready" in machine.text(), 90, "server: ready", machine)
        kinds = lambda: {body["event"] for body in hook.bodies}  # noqa: E731
        _until(lambda: {"restart_loop", "disk_low"} <= kinds(), 90, "the alerts", machine)
        loop = [body for body in hook.bodies if body["event"] == "restart_loop"][0]
        assert loop["server"] == "Alert Server" and loop["source"] == "arena-evolved-server", loop
        assert loop["time"] >= now and "started" in loop["text"] and loop["content"].startswith("Alert Server: "), loop
        # (a low disk is not said again within the gap)
        time.sleep(5)
        assert [body["event"] for body in hook.bodies].count("disk_low") == 1, hook.bodies
        # (the webhook is a secret setting: not shown to a role that lists the settings)
        api = Api(port)
        api.login()
        listed = json.dumps(api.call("GET", "/api/settings")[1])
        assert f"127.0.0.1:{hook.port}" not in listed
    finally:
        hook.close()


def test_first_run_claim(machines):
    """with server.claim on and no web password, no password is made: a claim token is written (mode 0600, not in the
    log) and the first visitor with it sets the password; a wrong token or a short password is refused; once claimed
    the token is gone and the claim route is closed"""
    _needs_game("bloodgulch")
    port = _free_port()
    machine = machines("server")
    settings = _server_settings(port, HALO_SERVER_ROTATION="bloodgulch:slayer", HALO_SERVER_CLAIM="true")
    # (no password in the environment: a setting the environment gives is pinned, and the claim writes it to config.toml)
    del settings["HALO_SERVER_WEB_PASSWORD"]
    machine.start(["--server"], **settings)
    _until(lambda: "server: ready" in machine.text(), 90, "server: ready", machine)
    token_file = machine.folder / "data" / "claim-token"
    assert token_file.is_file() and (token_file.stat().st_mode & 0o077) == 0, "claim-token is its user's alone"
    token = token_file.read_text().strip()
    assert re.fullmatch(r"[0-9a-f]{32}", token), token
    assert "not claimed yet" in machine.text() and token not in machine.text(), "the token is in the log"
    assert not (machine.folder / "data" / "first-admin-password.txt").exists(), "a password was made beside the claim"
    api = Api(port)
    assert api.call("POST", "/api/login", {"password": "anything-at-all"}, token=False)[0] == 401
    assert api.call("GET", "/api/claim", token=False)[0] == 405
    status, answer = api.call("POST", "/api/claim", {"token": "0" * 32, "password": "a-long-password-1"}, token=False)
    assert status == 403, (status, answer)
    status, answer = api.call("POST", "/api/claim", {"token": token, "password": "short"}, token=False)
    assert status == 400, (status, answer)
    status, answer = api.call("POST", "/api/claim", {"token": token, "password": "a-long-password-1"}, token=False)
    assert status == 200 and answer["claimed"], (status, answer)
    assert not token_file.exists(), "the claim token stays after the claim"
    status, answer = api.call("POST", "/api/login", {"password": "a-long-password-1"}, token=False)
    assert status == 200 and answer["role"] == "admin", (status, answer)
    assert api.call("POST", "/api/claim", {"token": token, "password": "another-long-one"}, token=False)[0] == 404


def test_rcon_bot_sample(machines):
    """docs/examples/server_rcon_bot.py signs in with an API key, subscribes and answers a join with a notice"""
    _needs_game("bloodgulch")
    port = _free_port()
    server = _key_server(machines, port, HALO_SERVER_BOTS_FILL="off")
    api = Api(port)
    api.login()
    status, made = api.call("POST", "/api/keys", {"name": "samplebot", "role": "moderator"})
    assert status == 200, made
    bot = subprocess.Popen(["python3", "-I", str(ROOT / "docs/examples/server_rcon_bot.py"), "--port", str(port), "--key",
                            made["key"], "--seconds", "150"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    try:
        _until(lambda: not bot.poll(), 20, "the bot running")
        _client(machines("client"), CLIENT_ADDRESSES[0], "Botted")
        _until(lambda: "key:samplebot: say Welcome, Botted!" in server.text() or
               any("key:samplebot" in line["text"] and "Welcome, Botted!" in line["text"]
                   for line in api.call("GET", "/api/log?limit=500")[1]["lines"]), 150, "the bot's notice", server)
    finally:
        bot.kill()
        output = bot.communicate()[0]
    assert "signed in as moderator" in output and "event player_joined" in output, output


def test_the_web_pages_admin_routes_on_a_fresh_server(machines):
    """(0.7.7, the web page's Access tab, backups and stop countdown) GET /api/access and GET /api/backups answer an
    admin; the bans' export and import work on a server with no bans.txt yet (the export freed a NULL text: the game's
    free stops on it, and the server died); a warned restart shows in the status and its cancel takes it out"""
    _needs_game("bloodgulch")
    port = _free_port()
    server = _key_server(machines, port, HALO_SERVER_BOTS_FILL="off")
    assert not (server.folder / "data" / "bans.txt").exists()
    api = Api(port)
    api.login()
    status, exported = api.call("GET", "/api/bans/export")
    assert status == 200 and exported == {"ok": True, "bans": []}, exported
    status, answer = api.call("POST", "/api/bans/import", {"bans": [{"ip": "198.51.100.7", "reason": "imported"}]})
    assert status == 200 and answer["added"] == 1, answer
    status, access = api.call("GET", "/api/access")
    assert status == 200 and access["admins"] == [] and access["whitelist"] == {"on": False, "entries": []}, access
    assert access["access"] == [] and access["reserved_slots"] == 0, access
    server.send("sv_whitelist add ip:192.0.2.0/24")
    _until(lambda: "ip:192.0.2.0/24 listed" in server.text(), 20, "the whitelist entry", server)
    assert api.call("GET", "/api/access")[1]["whitelist"]["entries"] == ["ip:192.0.2.0/24"]
    status, backups = api.call("GET", "/api/backups")
    assert status == 200 and backups["keep"] == 20 and isinstance(backups["backups"], list), backups
    for backup in backups["backups"]:
        assert re.fullmatch(re.escape(backup["file"]) + r"\.\d{8}-\d{6}", backup["name"]) and backup["time"] > 0
        assert backup["size"] >= 0
    assert api.call("GET", "/api/status")[1]["restart_warning"] is None
    server.send("sv_restart 600 a test")
    # (the status is the director's snapshot, made again every frame or two: wait for it)
    _until(lambda: api.call("GET", "/api/status")[1]["restart_warning"], 20, "the countdown in the status", server)
    warning = api.call("GET", "/api/status")[1]["restart_warning"]
    assert warning["restart"] is True and warning["total"] == 600 and warning["reason"] == "a test", warning
    assert 590 <= warning["seconds"] <= 600, warning
    server.send("sv_restart cancel")
    _until(lambda: api.call("GET", "/api/status")[1]["restart_warning"] is None, 20, "the cancel in the status", server)
    assert "the restart is cancelled" in server.text()
    assert server.process.poll() is None


def test_bots_alone_on_a_server_hurt_each_other(machines):
    """a server nobody has joined, with server.bots_alone: its bots' shots and blows damage (their damage was held for
    reports from the server's own machine, which sends none, so such games ended 0-0 with every bot at full health)"""
    _needs_game("bloodgulch")
    port = _free_port()
    server = machines("server").start(["--server"], **_server_settings(
        port, HALO_SERVER_ROTATION="bloodgulch:ae_team_sly", HALO_SERVER_BOTS_ALONE="true",
        HALO_SERVER_BOTS_FILL="minimum", HALO_SERVER_MIN_PLAYERS=4, HALO_SERVER_MAX_PLAYERS=8,
        HALO_SERVER_BOTS_SKILL="heroic", HALO_OBJECTIVE_LOG="full", HALO_SERVER_LOBBY_SECONDS=3))
    _until(lambda: "server: ready" in server.text(), 90, "server: ready", server)

    def hurt():
        # (an `ae obj` sample line: a bot below full health or shield, or a score)
        for line in (server.text() + server.debug()).splitlines():
            if "ev=ps" in line:
                values = dict(part.split("=", 1) for part in line.split() if "=" in part)
                try:
                    if float(values.get("hp", 100)) < 99 or float(values.get("shield", 100)) < 99:
                        return True
                except ValueError:
                    pass
            elif "ev=snap" in line and not re.search(r"score_r=0 score_b=0", line):
                return True
        return False

    _until(hurt, 150, "a bot hurt by another's shot or blow", server)


def test_the_moderation_pass_leaves_bots_alone(machines):
    """with the whitelist on and bots filling an otherwise empty server, no bot is kicked for being unlisted (the
    once-a-second pass counted bots as people: a refused kick line per bot each second)"""
    _needs_game("bloodgulch")
    port = _free_port()
    server = machines("server").start(["--server"], **_server_settings(
        port, HALO_SERVER_ROTATION="bloodgulch:ae_team_sly", HALO_SERVER_BOTS_ALONE="true",
        HALO_SERVER_BOTS_FILL="minimum", HALO_SERVER_MIN_PLAYERS=4, HALO_SERVER_MAX_PLAYERS=8,
        HALO_SERVER_WHITELIST="true", HALO_SERVER_LOBBY_SECONDS=3))
    _until(lambda: "server: ready" in server.text(), 90, "server: ready", server)
    time.sleep(25)
    assert "not on the whitelist" not in server.text(), server.text()[-2000:]
    assert "a banned name" not in server.text()


def test_a_person_joining_a_lobby_full_of_bots_at_min_players_starts_the_game(machines):
    """server.min_players equal to the most players, bots fill minimum and bots alone: the lobby holds eight bots; a
    person who joins takes a bot's seat, and the lobby still fills to eight (one slot was held free for a joiner, as a
    listen host does, which left the lobby at seven for ever: the countdown never came)"""
    _needs_game("bloodgulch")
    port = _free_port()
    server = machines("server").start(["--server"], **_server_settings(
        port, HALO_SERVER_ROTATION="bloodgulch:slayer", HALO_SERVER_BOTS_ALONE="true",
        HALO_SERVER_BOTS_FILL="minimum", HALO_SERVER_MIN_PLAYERS=8, HALO_SERVER_MAX_PLAYERS=8,
        HALO_SERVER_LOBBY_SECONDS=45))
    _until(lambda: "server: ready" in server.text(), 90, "server: ready", server)
    api = Api(port)
    api.login()

    def lobby_bots():
        status = api.status() or {}
        return status.get("state") == "lobby" and (status.get("bots") or {}).get("count") == 8

    _until(lobby_bots, 120, "eight bots in the lobby", server)
    person = _client(machines("first"), CLIENT_ADDRESSES[0], "Person")
    _until(lambda: "Person" in [player["name"] for player in _players(api)], 90, "the person in", server)
    assert (api.status() or {}).get("state") == "lobby", api.status()
    _until(lambda: (api.status() or {}).get("state") == "playing", 150, "the game started with the person in it", server)
    status = api.status()
    assert (status.get("bots") or {}).get("count") == 7, status
    names = [player["name"] for player in _players(api)]
    assert "Person" in names and len(names) == 8, names
    assert person.process.poll() is None
    person.stop()

TUNNEL_FRONT = f"{_BASE}.210"
TUNNEL_BACK = f"{_BASE}.230"


class TunnelAgent:
    """a tunnel agent's stand-in (playit.gg on the server's own computer): what the players connect to (TUNNEL_FRONT)
    is forwarded to the server from one address (TUNNEL_BACK), a new port for each stream and datagram flow, so that
    every player comes from the same address as far as the server can tell"""

    TCP_PORTS = (5150, 5153)
    UDP_PORTS = (5150, 5151, 5160)

    def __init__(self):
        self.stop = threading.Event()
        self.threads = []
        self.flows = {}
        self.lock = threading.Lock()
        self.fronts = {}
        self.listeners = []
        for port in self.UDP_PORTS:
            front = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            front.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            front.bind((TUNNEL_FRONT, port))
            front.settimeout(0.2)
            self.fronts[port] = front
            self._thread(self._datagrams, front, port)
        for port in self.TCP_PORTS:
            listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            listener.bind((TUNNEL_FRONT, port))
            listener.listen(8)
            listener.settimeout(0.2)
            self.listeners.append(listener)
            self._thread(self._accept, listener, port)

    def _thread(self, target, *arguments):
        thread = threading.Thread(target=target, args=arguments, daemon=True)
        thread.start()
        self.threads.append(thread)

    def _datagrams(self, front, port):
        while not self.stop.is_set():
            try:
                data, client = front.recvfrom(4096)
            except socket.timeout:
                continue
            except OSError:
                return
            with self.lock:
                upstream = self.flows.get((client, port))
                if upstream is None:
                    upstream = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                    upstream.bind((TUNNEL_BACK, 0))
                    upstream.settimeout(0.2)
                    self.flows[(client, port)] = upstream
                    self._thread(self._replies, upstream, front, client)
            upstream.sendto(data, (SERVER_ADDRESS, port))

    def _replies(self, upstream, front, client):
        while not self.stop.is_set():
            try:
                data, _ = upstream.recvfrom(4096)
            except socket.timeout:
                continue
            except OSError:
                return
            front.sendto(data, client)

    def _accept(self, listener, port):
        while not self.stop.is_set():
            try:
                client, _ = listener.accept()
            except socket.timeout:
                continue
            except OSError:
                return
            upstream = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            upstream.bind((TUNNEL_BACK, 0))
            try:
                upstream.connect((SERVER_ADDRESS, port))
            except OSError:
                client.close()
                continue
            self._thread(self._pipe, client, upstream)
            self._thread(self._pipe, upstream, client)

    def _pipe(self, source, destination):
        source.settimeout(0.2)
        while not self.stop.is_set():
            try:
                data = source.recv(65536)
            except socket.timeout:
                continue
            except OSError:
                break
            if not data:
                break
            try:
                destination.sendall(data)
            except OSError:
                break
        for end in (source, destination):
            try:
                end.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass

    def close(self):
        self.stop.set()
        for thread in self.threads:
            thread.join(timeout=2)
        for front in list(self.fronts.values()) + self.listeners + list(self.flows.values()):
            front.close()


@pytest.fixture
def tunnel():
    agent = TunnelAgent()
    yield agent
    agent.close()


def _tunnel_server(machines, port, **settings):
    server = machines("server").start(["--server"], **_server_settings(
        port, HALO_SERVER_ROTATION="bloodgulch:slayer", HALO_SERVER_BOTS_FILL="off", HALO_SERVER_EMPTY_SECONDS=600,
        # (no unsolicited adverts to the clients' own addresses: they find the server through the tunnel alone)
        HALO_NET_BROADCAST=f"{_BASE}.250", HALO_SERVER_TUNNEL_ADDRESSES=TUNNEL_BACK, **settings))
    _until(lambda: "server: ready" in server.text(), 90, "server: ready", server)
    return server


def test_two_players_from_one_address_both_stay_and_play(machines, tunnel):
    """every player of a server behind a tunnel agent comes from the agent's one address: a second machine from it is
    another machine (it dropped the first: "joined from the address of machine"), each machine's datagrams are told
    apart by the port they come from, and a machine that joins again (its hardware id the same) does not replace its old
    self either: through a tunnel no machine is dropped for another's join, the old connection is found dead on its own"""
    _needs_game("bloodgulch")
    port = _free_port()
    server = _tunnel_server(machines, port)
    api = Api(port)
    api.login()
    first = _client(machines("first"), CLIENT_ADDRESSES[0], "First", TUNNEL_FRONT, f"{TUNNEL_FRONT}:5150",
                    HALO_HARDWARE_ID="test-machine-first")
    _until(lambda: (api.status() or {}).get("state") == "playing", 120, "the game", server)
    second = _client(machines("second"), CLIENT_ADDRESSES[1], "Second", TUNNEL_FRONT, f"{TUNNEL_FRONT}:5150",
                     HALO_HARDWARE_ID="test-machine-second")
    _until(lambda: len(api.call("GET", "/api/players")[1]["players"]) == 2, 90, "both machines in the game", server)
    _until(lambda: "local machine is loaded" in second.debug(), 90, "the second machine loaded", second)
    # (each machine's datagrams are found by their port, the two ports not the same)
    def ports():
        text = server.debug() + server.text()
        return {number: set(re.findall(rf"machine #{number}'s datagrams come from port (\d+)", text)) for number in (1, 2)}

    _until(lambda: all(ports().values()), 60, "two machines' datagram ports", server)
    assert ports()[1].isdisjoint(ports()[2]), ports()
    # (AE_TEST_PLAY_SECONDS: a longer look, 120 for the two minutes of the release check)
    time.sleep(int(os.environ.get("AE_TEST_PLAY_SECONDS", "20")))
    names = sorted(player["name"] for player in api.call("GET", "/api/players")[1]["players"])
    assert names == ["First", "Second"], names
    assert "joined from the address of machine" not in server.debug() + server.text()
    assert first.process.poll() is None and second.process.poll() is None
    # the first machine's connection lost (its process stopped: the server finds no death), and it joins again, the
    # same machine (its hardware id the same): from a tunnel's address nobody is dropped for it (a hardware id is what
    # the client says; the old self stays until its connection is found dead, here when its process ends)
    first.process.send_signal(signal.SIGSTOP)
    again = machines("again").start(HALO_NULL_RENDERER=1, HALO_NETWORK_TEST="join", HALO_NETWORK_TEST_NAME="First",
                                    HALO_NET_ADDRESS=CLIENT_ADDRESSES[0], HALO_NET_BROADCAST=TUNNEL_FRONT,
                                    HALO_NO_AUDIO=1, HALO_HARDWARE_ID="test-machine-first",
                                    HALO_NETWORK_TEST_JOIN_ADDRESS=f"{TUNNEL_FRONT}:5150")
    _until(lambda: len(api.call("GET", "/api/players")[1]["players"]) == 3, 90, "the machine back beside its old self",
           server)
    assert "which is dropped" not in server.debug() + server.text()
    # (its name is taken by its old self, so it plays as "First 2" until that one is found dead)
    first.process.kill()

    def names():
        return [player["name"] for player in api.call("GET", "/api/players")[1]["players"]]

    _until(lambda: "First" not in names(), 90, "the old self gone", server)
    assert "Second" in names() and "First 2" in names(), names()
    assert second.process.poll() is None and again.process.poll() is None



def test_players_from_one_address_without_tokens_are_told_apart_by_elimination(machines, tunnel):
    """clients that send no token (0.7.6 and earlier): the port of a machine's datagrams is found by elimination when
    they load one after the other, and neither machine is dropped"""
    _needs_game("bloodgulch")
    port = _free_port()
    server = _tunnel_server(machines, port)
    api = Api(port)
    api.login()
    join = f"{TUNNEL_FRONT}:5150"
    first = _client(machines("first"), CLIENT_ADDRESSES[0], "First", TUNNEL_FRONT, join, HALO_NET_FLOW_BIND_OFF=1,
                    HALO_HARDWARE_ID="test-machine-first")
    _until(lambda: (api.status() or {}).get("state") == "playing", 120, "the game", server)
    second = _client(machines("second"), CLIENT_ADDRESSES[1], "Second", TUNNEL_FRONT, join, HALO_NET_FLOW_BIND_OFF=1,
                     HALO_HARDWARE_ID="test-machine-second")
    _until(lambda: len(api.call("GET", "/api/players")[1]["players"]) == 2, 90, "both machines in the game", server)
    _until(lambda: all(f"machine #{number}'s datagrams come from port" in server.debug() + server.text() and
                       re.search(rf"machine #{number}'s datagrams come from port \d+ of its address \(by elimination\)",
                                 server.debug() + server.text()) for number in (1, 2)), 90,
           "both ports found by elimination", server)
    time.sleep(20)
    assert sorted(player["name"] for player in api.call("GET", "/api/players")[1]["players"]) == ["First", "Second"]
    assert "which is dropped" not in server.debug() + server.text()
    assert first.process.poll() is None and second.process.poll() is None


def test_a_machine_without_a_hardware_id_drops_nobody(machines, tunnel):
    """a join request whose hardware id is blank (the client told none, or only characters the host leaves out) names
    no machine: from a tunnel's address it is refused, and in no case is another machine dropped for it"""
    _needs_game("bloodgulch")
    port = _free_port()
    server = _tunnel_server(machines, port)
    api = Api(port)
    api.login()
    join = f"{TUNNEL_FRONT}:5150"
    first = _client(machines("first"), CLIENT_ADDRESSES[0], "First", TUNNEL_FRONT, join,
                    HALO_HARDWARE_ID="test-machine-first")
    _until(lambda: (api.status() or {}).get("state") == "playing", 120, "the game", server)
    # (HALO_HARDWARE_ID=none: the client finds no machine id, as in a container without one, and sends a blank)
    blank = _client(machines("second"), CLIENT_ADDRESSES[1], "Nobody", TUNNEL_FRONT, join, HALO_HARDWARE_ID="none")
    _until(lambda: "no hardware id" in server.text() + server.debug(), 90, "the blank id refused at the door", server)
    time.sleep(10)
    text = server.debug() + server.text()
    assert "which is dropped" not in text
    assert [player["name"] for player in api.call("GET", "/api/players")[1]["players"]] == ["First"]
    assert first.process.poll() is None
    blank.stop()
