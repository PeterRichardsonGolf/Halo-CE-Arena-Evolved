"""DEV MODE's commands (port/linux/game/ae_dev_cmd*.c): checks that need no game.

The grammar itself is a C unit (port/linux/tests/ae_dev_cmd_rules_test.c, run by tools/test_ae_units.py)."""
import json
import os
import random
import re
import shutil
import socket
import subprocess
import sys
import time
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
RULES = ROOT / "port" / "linux" / "game" / "ae_dev_cmd_rules.c"
HS = ROOT / "source" / "hs"


def dev_commands():
    """The command table: (name, bare) for each entry but the empty first one."""
    table = re.findall(r'\{ "([a-z_]*)", "[^"]*", ([01]) \},', RULES.read_text())
    return [(name, bare == "1") for name, bare in table if name]


def script_names():
    """Every script function's and global's name the console's evaluator knows."""
    functions = re.findall(r'^\s+"([a-z_0-9]+)",\s*$', (HS / "hs.c").read_text(), re.M)
    globals_ = re.findall(r'\{ "([a-z_0-9]+)", _hs_type', (HS / "hs_globals_external.c").read_text())
    return set(functions) | set(globals_)


def test_table_read():
    names = [name for name, _ in dev_commands()]
    assert "give" in names and "reset" in names and len(names) == len(set(names))


def test_bare_names_never_clash_with_script_names():
    """A bare dev name would hide a script function of that name while DEV MODE is on: none may clash ("help" is the
    script console's, so the dev one is "dev help" only)."""
    names = script_names()
    assert len(names) > 400
    assert "help" in names
    assert "dev" not in names
    clashes = [name for name, bare in dev_commands() if bare and name in names]
    assert clashes == []


# ---------- in a game (needs the retail maps in assets/maps and a built game; the box runs it)

RETAIL_MAPS = ROOT / "assets" / "maps"


def _game_binary():
    for build in ("linux64", "linux"):
        game = ROOT / "build" / build / "halo"
        if game.is_file():
            return game
    return None


def _free_port():
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        return probe.getsockname()[1]


def _telnet(port, deadline):
    while time.time() < deadline:
        try:
            return socket.create_connection(("127.0.0.1", port), timeout=2)
        except OSError:
            time.sleep(0.5)
    raise AssertionError("the telnet console never opened")


def _send(connection, line, replies, wait=1.5):
    """sends a line, then reads what the console wrote back for wait seconds (a console never read stops taking)"""
    connection.sendall(line.encode() + b"\r\n")
    end = time.time() + wait
    connection.settimeout(0.2)
    while time.time() < end:
        try:
            data = connection.recv(65536)
        except socket.timeout:
            continue
        if not data:
            replies.append("<closed>")
            return
        replies.append(data.decode(errors="replace"))


def test_dev_commands_in_a_local_game(tmp_path):
    """A local Slayer game on Blood Gulch with DEV MODE and the telnet console: each command answers, and the log
    has its "dev cmd" line, ok or error as expected; a bare "help" is the script console's, not a dev command."""
    game = _game_binary()
    if not game or not (RETAIL_MAPS / "bloodgulch.map").is_file():
        pytest.skip("needs bloodgulch.map in assets/maps and a built game (build/linux64/halo)")
    (tmp_path / "data").mkdir()
    (tmp_path / "save").mkdir()
    (tmp_path / "data" / "maps").symlink_to(RETAIL_MAPS.resolve())
    copy = tmp_path / "halo"
    shutil.copy(game, copy)
    port = _free_port()
    # (an address of its own on the loopback: other games on the machine hold 127.0.0.1's ports)
    env = {**os.environ, "SDL_VIDEODRIVER": "offscreen", "SDL_AUDIODRIVER": "dummy",
           "HALO_DATA_ROOT": str(tmp_path / "data"), "HALO_SAVE_ROOT": str(tmp_path / "save"),
           "HALO_HIDDEN_WINDOW": "1", "HALO_FULLSCREEN": "0", "HALO_NO_VSYNC": "1", "HALO_NO_AUDIO": "1",
           "HALO_UPDATE_AUTO": "false", "HALO_NET_ONLINE": "false", "HALO_NET_ADDRESS": f"127.0.0.{random.randint(40, 250)}",
           "HALO_NET_BROADCAST": "127.255.255.255", "HALO_NETWORK_TEST": "local:bloodgulch",
           "HALO_NETWORK_TEST_START": "3", "HALO_NETWORK_TEST_TIME_LIMIT": "5", "HALO_EXIT_AFTER": "300", "HALO_DEV_MODE": "1",
           "HALO_TELNET_CONSOLE": "1", "HALO_TELNET_CONSOLE_PORT": str(port)}
    # (to a file: a pipe nobody reads till the end fills, and the game stops in its next log line)
    log = open(tmp_path / "game.log", "w")
    process = subprocess.Popen([str(copy)], cwd=tmp_path, env=env, stdout=log, stderr=subprocess.STDOUT)
    lines = [
        "dev list weapons", "give sniper_rifle", "give no_such_gun", "spawn warthog", "powerup camouflage",
        "goto start 0", "score me 5", "score red 3", "kill", "respawn", "mark smoke check", "time 30", "reset",
        "dev help give",
        "help", "dev end round", "bots 2", "end game",
    ]
    replies = []
    try:
        connection = _telnet(port, time.time() + 60)
        # (the game up and its players spawned: the network test's first tick line, then a few seconds; a busy machine
        # takes longer to load)
        deadline = time.time() + 180
        while time.time() < deadline and "network test: tick" not in (tmp_path / "game.log").read_text(errors="replace"):
            _send(connection, "", replies, wait=1)
        time.sleep(5)
        for line in lines:
            # (the last a while longer: the bots' first seconds on a map are slow frames)
            _send(connection, line, replies, wait=4 if line == lines[-1] else 1.5)
        connection.close()
    finally:
        # (done: the game need not run to debug.exit_after)
        if process.poll() is None:
            process.terminate()
        try:
            process.wait(timeout=30)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        log.close()
    output = (tmp_path / "game.log").read_text(errors="replace")
    (tmp_path / "telnet.log").write_text("".join(replies))
    results = {}
    for row in output.splitlines():
        found = re.search(r"dev cmd [a-z ]+: (.*?) -> (.*)$", row)
        if found:
            results[found.group(1).strip()] = found.group(2)
    expect = {
        "dev list weapons": "ok", "give sniper_rifle": "ok gave", "give no_such_gun": "error no weapon",
        "spawn warthog": "ok made", "powerup camouflage": "ok made", "goto start 0": "ok at",
        "score me 5": "ok me now 5", "kill": "ok 1 killed", "respawn": "ok", "bots 2": "ok bots 2",
        "mark smoke check": "ok marked", "time 30": "ok 30 s left", "score red 3": "ok red now 3",
        "reset": "ok reset: everyone respawns", "dev help give": "ok give",
        "dev end round": "error this gametype has no rounds", "end game": "ok game ended",
    }
    assert "help" not in results, "a bare help is the script console's"
    problems = [f"{line}: {results.get(line)!r}, wanted {want!r}" for line, want in expect.items()
                if not (results.get(line) or "").startswith(want)]
    assert not problems, "\n".join(problems) + "\n--- dev and console lines ---\n" + "\n".join(
        row for row in output.splitlines() if "dev" in row or "telnet" in row or "console" in row) + \
        "\n--- telnet ---\n" + "".join(replies)[-3000:]


# ---------- tools/ae_test/expect.py on a made-up stream

def test_expect_judges_rules_in_each_tests_window(tmp_path):
    sys.path.insert(0, str(ROOT / "tools" / "ae_test"))
    import expect
    listing = tmp_path / "l.txt"
    listing.write_text("list l\ngroup g\nmap m\n"
                       "test a\ncheck c1\nexpect 1 weapon sniper_rifle loaded 4\ncheck c2\nexpect 2 no death player 0\n"
                       "check c3\n"
                       "test b\ncheck d1\nexpect 1 respawn vehicle ghost 30 +-1\ncheck d2\nexpect 2 score player 0 team 3\n"
                       "check d3\nexpect 3 cmd give ok\n"
                       "test c\ncheck e1\nexpect 1 game_over\n")
    events = [
        {"tick": 0, "ev": "test", "list": "l", "test": "a", "state": "start"},
        {"tick": 5, "ev": "weapon", "player": 0, "weapon": "weapons\\sniper rifle\\sniper rifle", "loaded": 4,
         "total": 8},
        {"tick": 6, "ev": "test", "list": "l", "test": "a", "state": "verdict", "verdict": "PASS"},
        {"tick": 7, "ev": "death", "player": 0},
        {"tick": 8, "ev": "test", "list": "l", "test": "b", "state": "start"},
        {"tick": 10, "ev": "object_gone", "kind": "vehicle", "tag": "vehicles\\ghost\\ghost_mp", "id": 1},
        {"tick": 940, "ev": "object_new", "kind": "vehicle", "tag": "vehicles\\ghost\\ghost_mp", "id": 2, "x": 0,
         "y": 0, "z": 0},
        {"tick": 950, "ev": "score", "player": 0, "individual": 1, "team": 2},
        {"tick": 951, "ev": "cmd", "source": "test run", "line": "give dmr", "ok": False, "text": "no weapon"},
    ]
    stream = tmp_path / "e.jsonl"
    stream.write_text("\n".join(json.dumps(event) for event in events) + "\nnot json\n")
    rows = {(row["test"], row["check"]): row for row in expect.report(listing, stream)}
    assert rows[("a", 1)]["verdict"] == "PASS" and rows[("a", 1)]["evidence"] == [2]
    # (the death after a's verdict is outside a's window)
    assert rows[("a", 2)]["verdict"] == "PASS"
    assert rows[("a", 3)]["verdict"] == "LOOK"
    # (930 ticks: 31 s, within 30 +-1)
    assert rows[("b", 1)]["verdict"] == "PASS", rows[("b", 1)]
    assert rows[("b", 2)]["verdict"] == "FAIL" and "1/2" in rows[("b", 2)]["reason"]
    assert rows[("b", 3)]["verdict"] == "FAIL"
    assert rows[("c", 1)]["verdict"] == "NOT RUN"


def test_expect_refuses_bad_rules_and_reads_tabs(tmp_path):
    sys.path.insert(0, str(ROOT / "tools" / "ae_test"))
    import expect
    for rule in ("no frobnicate", "no", "spawn banana", "kill victim", "score individual nope", "respawn vehicle g 30 +-x",
                 "respawn vehicle g", "object car ghost", "game_over now"):
        assert expect.validate(rule), rule
    for rule in ("option needler", "log", "option needler mcc now"):
        assert expect.validate(rule), rule
    for rule in ("no death player 0", "weapon sniper rifle loaded 4", "respawn item active camo 120 +-2", "cmd give ok",
                 "log ae needler: MCC", "option needler mcc", "option fall_damage halo_2"):
        assert expect.validate(rule) is None, rule
    listing = tmp_path / "l.txt"
    listing.write_text("list\tl\ntest\ta\ncheck\tc1\nexpect\t1\tno frobnicate\ncheck c2\nexpect 2 respawn vehicle ghost 30\n")
    events = [
        {"tick": 0, "ev": "test", "list": "l", "test": "a", "state": "start"},
        {"tick": 10, "ev": "object_gone", "kind": "vehicle", "tag": "ghost", "id": 1},
        {"tick": 100, "ev": "object_new", "kind": "vehicle", "tag": "ghost", "id": 2},
        {"tick": 200, "ev": "object_gone", "kind": "vehicle", "tag": "ghost", "id": 2},
        {"tick": 1100, "ev": "object_new", "kind": "vehicle", "tag": "ghost", "id": 3},
    ]
    stream = tmp_path / "e.jsonl"
    stream.write_text("\n".join(json.dumps(event) for event in events) + "\n")
    rows = {(row["test"], row["check"]): row for row in expect.report(listing, stream)}
    assert rows[("a", 1)]["verdict"] == "BAD RULE"
    # (the first pair took 3 s; the second, 30 s, passes)
    assert rows[("a", 2)]["verdict"] == "PASS", rows[("a", 2)]


def test_expect_log_and_option_rules(tmp_path):
    sys.path.insert(0, str(ROOT / "tools" / "ae_test"))
    import expect
    listing = tmp_path / "l.txt"
    listing.write_text("list l\ntest a\ncheck c1\nexpect 1 option needler mcc\ncheck c2\nexpect 2 log ae needler: MCC\n"
                       "check c3\nexpect 3 option fall_damage halo_2\n")
    events = [
        {"tick": 0, "ev": "test", "list": "l", "test": "a", "state": "start"},
        {"tick": 1, "ev": "cmd", "source": "test run", "line": "option needler mcc", "ok": True,
         "text": "NEEDLER is MCC in this game"},
        {"tick": 2, "ev": "log", "text": "ae needler: MCC needler (reach 9.5, speed 0.0300-0.0500, homing 180)"},
        {"tick": 3, "ev": "cmd", "source": "test run", "line": "option fall_damage halo_2", "ok": False,
         "text": "no GAME OPTIONS row"},
    ]
    stream = tmp_path / "e.jsonl"
    stream.write_text("\n".join(json.dumps(event) for event in events) + "\n")
    rows = {(row["test"], row["check"]): row["verdict"] for row in expect.report(listing, stream)}
    assert rows == {("a", 1): "PASS", ("a", 2): "PASS", ("a", 3): "FAIL"}


# ---------- tools/ae_test/live.py with a made-up console

def test_live_agent_puts_verdicts_on_the_card(tmp_path):
    sys.path.insert(0, str(ROOT / "tools" / "ae_test"))
    import live

    class FakeConsole:
        def __init__(self):
            self.lines = []

        def send(self, line, confirm=None):
            self.lines.append(line)
            return "confirmed"

        def poll(self, confirm):
            return False

    listing = tmp_path / "l.txt"
    listing.write_text("list l\ntest a\ncheck It is given\nexpect 1 weapon sniper_rifle\ncheck Looks right\n"
                       "check Nobody died\nexpect 3 no death player 0\n")
    console = FakeConsole()
    agent = live.Agent(listing, console)
    events = [
        {"ev": "test", "list": "l", "test": "a", "state": "load"},
        {"ev": "test", "list": "l", "test": "a", "state": "start"},
        {"ev": "cmd", "line": "give sniper_rifle", "ok": True, "text": "gave"},
        {"ev": "weapon", "player": 0, "weapon": "weapons\\sniper rifle\\sniper rifle", "loaded": 4, "total": 8},
        {"ev": "death", "player": 0},
        {"ev": "test", "list": "l", "test": "a", "state": "verdict", "verdict": "PASS"},
        {"ev": "weapon", "player": 0, "weapon": "weapons\\pistol\\pistol", "loaded": 12, "total": 48},
    ]
    for number, event in enumerate(events, 1):
        event["_line"] = number
        agent.take(event)
    sent = console.lines
    assert sent[:3] == ["agent check 1 waiting not seen yet", "agent check 2 look", "agent check 3 waiting not seen yet"]
    assert any(line.startswith("agent check 1 pass sniper rifle: 4 loaded, 8 more") and "(events 4)" in line
               for line in sent), sent
    assert any(line.startswith("agent check 3 pass never") for line in sent), sent
    assert sent[-1].startswith("agent check 3 fail it happened: death of player 0"), sent
    # (nothing after the verdict: the test is over)
    assert not any("pistol" in line for line in sent)


def test_live_agent_lines_fit_the_telnet_console(tmp_path):
    """Every line the agent sends fits the telnet console's 127-character line (at most 120): a long reason or message
    is cut, the verdict word kept; a "no" rule's later FAIL line too."""
    sys.path.insert(0, str(ROOT / "tools" / "ae_test"))
    import live

    class FakeConsole:
        def __init__(self):
            self.lines = []

        def send(self, line, confirm=None):
            self.lines.append(line)
            return "confirmed"

        def poll(self, confirm):
            return False

    listing = tmp_path / "l.txt"
    listing.write_text("list l\ntest a\ncheck Nobody died\nexpect 1 no death player 0\n"
                       "check It is given\nexpect 2 weapon sniper_rifle\n")
    console = FakeConsole()
    agent = live.Agent(listing, console)
    agent.greet = "x" * 400
    events = [{"ev": "test", "list": "l", "test": "a", "state": "start"}]
    events += [{"ev": "mark", "text": "y" * 50} for _ in range(30)]
    events += [{"ev": "weapon", "player": 0, "weapon": "weapons\\sniper rifle\\" + "sniper rifle " * 20, "loaded": 4,
                "total": 8}, {"ev": "death", "player": 0}]
    for number, event in enumerate(events, 1):
        event["_line"] = number
        agent.take(event)
    sent = console.lines
    assert sent and all(len(line) <= live.LINE <= 120 for line in sent), [len(line) for line in sent]
    assert any(line.startswith("agent say xxx") and line.endswith("...") for line in sent), sent
    assert any(line.startswith("agent check 2 pass sniper rifle") for line in sent), sent
    assert any(line.startswith("agent check 1 fail it happened") for line in sent), sent
    assert live.card_line("agent check 3 waiting", "r" * 500)[:22] == "agent check 3 waiting "
    assert len(live.card_line("agent check 3 waiting", "r" * 500)) == live.LINE


def test_live_agent_marks_a_verdict_shown_only_once_answered(tmp_path):
    """A verdict counts as shown only when the game's answer to it comes back. An overflowed line (not run) is sent
    again; one with no answer may still run, so it is never sent again: it stays the one outstanding line, the others
    wait, and its late answer (on the console, or as its cmd event in the stream) marks it shown."""
    sys.path.insert(0, str(ROOT / "tools" / "ae_test"))
    import live
    import threading

    got, late = [], threading.Event()
    replies = ["\r\noverflowed client buffer; resetting buffer\r\n", "<echo>"]
    server = socket.socket()
    server.bind(("127.0.0.1", 0))
    server.listen(1)
    port = server.getsockname()[1]

    def serve():
        connection, _ = server.accept()
        connection.settimeout(0.2)
        pending = b""
        try:
            while True:
                if late.is_set():
                    # (the outstanding line's answer, late; then the next line's at once)
                    connection.sendall(b"\r\ncheck 1: waiting\r\n")
                    late.clear()
                try:
                    data = connection.recv(4096)
                except socket.timeout:
                    continue
                if not data:
                    break
                pending += data
                while b"\r\n" in pending:
                    line, pending = pending.split(b"\r\n", 1)
                    text = line.decode()
                    got.append(text)
                    reply = replies.pop(0) if replies else None
                    if reply == "<echo>":
                        reply = text
                    elif reply is None:
                        words = text.split()
                        reply = f"{text}\r\ncheck {words[2]}: {words[3]}\r\n"
                    connection.sendall(reply.encode())
        except OSError:
            pass
        connection.close()

    thread = threading.Thread(target=serve, daemon=True)
    thread.start()
    console = live.Console(port, wait=0.4)
    listing = tmp_path / "l.txt"
    listing.write_text("list l\ntest a\ncheck It is given\nexpect 1 weapon sniper_rifle\ncheck Looks right\n")
    agent = live.Agent(listing, console)
    agent.start("a")
    # (overflowed: sent again; then only the echo: outstanding, check 2 waits behind it)
    assert got == ["agent check 1 waiting not seen yet"] * 2, got
    assert agent.outstanding and 1 not in agent.shown and 2 in agent.wanted
    # (more events: nothing sent again, nothing new sent)
    for _ in range(3):
        agent.take({"ev": "mark", "text": "x", "_line": 1})
    assert len(got) == 2, got
    # (the late answer: shown, and check 2 goes)
    late.set()
    end = time.time() + 5
    while time.time() < end and 2 not in agent.shown:
        time.sleep(0.3)
        agent.tick()
    assert agent.shown.get(1) == ("waiting", "not seen yet"), (got, agent.shown)
    assert agent.shown.get(2) == ("look", ""), (got, agent.shown)
    assert got.count("agent check 1 waiting not seen yet") == 2 and got[-1] == "agent check 2 look", got
    console.close()
    server.close()
    thread.join(timeout=5)


def test_live_agent_outstanding_line_answered_by_the_stream(tmp_path):
    """An outstanding line's cmd event in the stream is its answer too (the console was lost meanwhile)."""
    sys.path.insert(0, str(ROOT / "tools" / "ae_test"))
    import live

    class SilentConsole:
        def __init__(self):
            self.lines = []

        def send(self, line, confirm=None):
            self.lines.append(line)
            return "unanswered"

        def poll(self, confirm):
            return False

    listing = tmp_path / "l.txt"
    listing.write_text("list l\ntest a\ncheck One\ncheck Two\n")
    console = SilentConsole()
    agent = live.Agent(listing, console)
    agent.start("a")
    assert console.lines == ["agent check 1 look"] and agent.outstanding
    agent.take({"ev": "cmd", "line": "agent check 1 look", "ok": True, "text": "check 1: look", "_line": 2})
    assert agent.shown.get(1) == ("look", "") and console.lines == ["agent check 1 look", "agent check 2 look"]


def test_live_agent_exits_when_the_console_slot_is_taken(tmp_path, capsys):
    """The game's telnet console has one client slot: when it is taken, the agent says so and exits 3."""
    sys.path.insert(0, str(ROOT / "tools" / "ae_test"))
    import live
    import threading

    server = socket.socket()
    server.bind(("127.0.0.1", 0))
    server.listen(1)
    port = server.getsockname()[1]

    def serve():
        connection, _ = server.accept()
        connection.sendall(b"sorry - the maximum number of clients are already connected. goodbye!\r\n")
        connection.close()

    thread = threading.Thread(target=serve, daemon=True)
    thread.start()
    listing = tmp_path / "l.txt"
    listing.write_text("list l\ntest a\ncheck One\n")
    events = tmp_path / "events-20261103-000000.jsonl"
    events.write_text(json.dumps({"ev": "test", "list": "l", "test": "a", "state": "start"}) + "\n")
    assert live.main(["live.py", str(listing), "--events", str(events), "--port", str(port)]) == 3
    assert "maximum number of clients" in capsys.readouterr().err
    server.close()
    thread.join(timeout=5)
