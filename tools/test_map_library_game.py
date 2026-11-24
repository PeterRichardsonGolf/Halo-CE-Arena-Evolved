"""The installed maps library in the game (port/linux/game/ae_map_library.c), driven by DEV MODE's "maps" command at the
main menu through the telnet console, over a data folder of its own: the stock maps (a link to a folder of them), fake
Custom Edition maps in maps_ce and two of its folders (one of 500 maps, whose scan time is printed), a downloaded Xbox map in maps_shared, a map pack, a file that is
not a map, a map file that is a link out of the roots and a folder linked out of them. Each command's answer line is
checked, and the files on disk after each delete and move.

Needs a built game (build/linux64/halo or build/linux/halo) and a maps folder with ui.map and bloodgulch.map
(AE_TEST_MAPS, else assets/maps): the build box runs it; elsewhere it skips."""
import os
import random
import re
import shutil
import socket
import subprocess
import time
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
MAPS = Path(os.environ.get("AE_TEST_MAPS", ROOT / "assets" / "maps"))


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


def _send(connection, line, wait=1.5):
    """sends a line, then reads what the console wrote back for wait seconds (a console never read stops taking)"""
    connection.sendall(line.encode() + b"\r\n")
    end = time.time() + wait
    connection.settimeout(0.2)
    while time.time() < end:
        try:
            if not connection.recv(65536):
                return
        except socket.timeout:
            continue


def _fake_map(path, name, version, kind=1, extra=0):
    """a map file's header (2048 bytes: head, version, name, scenario type, foot), padded by extra bytes"""
    header = bytearray(2048)
    header[0:4] = b"daeh"
    header[4:8] = version.to_bytes(4, "little")
    header[0x20:0x20 + len(name)] = name.encode()
    header[0x60:0x62] = kind.to_bytes(2, "little")
    header[0x7FC:0x800] = b"toof"
    path.write_bytes(bytes(header) + bytes(extra))


def _answers(log_text):
    """{dev line: "ok ..." or "error ..."} from the log's "dev cmd" lines"""
    found = {}
    for row in log_text.splitlines():
        match = re.search(r"dev cmd [a-z ]+: (.*?) -> (.*)$", row)
        if match:
            found[match.group(1).strip()] = match.group(2)
    return found


def test_map_library_at_the_main_menu(tmp_path):
    game = _game_binary()
    if not game or not (MAPS / "ui.map").is_file() or not (MAPS / "bloodgulch.map").is_file():
        pytest.skip("needs a built game (build/linux64/halo) and ui.map + bloodgulch.map (AE_TEST_MAPS or assets/maps)")
    data = tmp_path / "data"
    (data / "maps_ce" / "Mine").mkdir(parents=True)
    (data / "maps_shared").mkdir()
    (data / "maps_testpack").mkdir()
    (tmp_path / "save").mkdir()
    (tmp_path / "outside").mkdir()
    # (a maps folder of its own, its stock files links to the real ones: a move into it stays in this test's folder)
    (data / "maps").mkdir()
    for stock in ("ui.map", "bloodgulch.map", "bitmaps.map", "sounds.map", "loc.map"):
        if (MAPS / stock).is_file():
            (data / "maps" / stock).symlink_to((MAPS / stock).resolve())
    _fake_map(data / "maps_ce" / "libtest_alpha.map", "libtest_alpha", 609, extra=4096)
    _fake_map(data / "maps_ce" / "libtest_beta.map", "libtest_beta", 609)
    (data / "maps_ce" / "libtest_beta.txt").write_text("a description beside it")
    _fake_map(data / "maps_ce" / "Mine" / "libtest_gamma.map", "libtest_gamma", 609)
    _fake_map(data / "maps_shared" / "libtest_shared.map", "libtest_shared", 5)
    _fake_map(data / "maps_testpack" / "libtest_packed.map", "libtest_packed", 5)
    (data / "maps_ce" / "libtest_junk.map").write_text("not a map")
    # (a folder of 500 maps: the scan's cost through the game's own file calls, printed)
    (data / "maps_ce" / "Bulk").mkdir()
    for index in range(500):
        _fake_map(data / "maps_ce" / "Bulk" / f"bulk_{index:03d}.map", f"bulk_{index:03d}", 609)
    _fake_map(tmp_path / "outside" / "libtest_secret.map", "libtest_secret", 609)
    (data / "maps_ce" / "libtest_linked.map").symlink_to(tmp_path / "outside" / "libtest_secret.map")
    (data / "maps_ce" / "Away").symlink_to(tmp_path / "outside")
    # (a Custom Edition map kept in the maps folder itself: the player's own, so it may be deleted)
    _fake_map(data / "maps" / "libtest_inmaps.map", "libtest_inmaps", 609)
    copy = tmp_path / "halo"
    shutil.copy(game, copy)
    port = _free_port()
    env = {**os.environ, "SDL_VIDEODRIVER": "offscreen", "SDL_AUDIODRIVER": "dummy",
           "HALO_DATA_ROOT": str(data), "HALO_SAVE_ROOT": str(tmp_path / "save"),
           "HALO_HIDDEN_WINDOW": "1", "HALO_FULLSCREEN": "0", "HALO_NO_VSYNC": "1", "HALO_NO_AUDIO": "1",
           "HALO_UPDATE_AUTO": "false", "HALO_NET_ONLINE": "false",
           "HALO_NET_ADDRESS": f"127.0.0.{random.randint(40, 250)}", "HALO_NET_BROADCAST": "127.255.255.255",
           "HALO_EXIT_AFTER": "240", "HALO_DEV_MODE": "1", "HALO_TELNET_CONSOLE": "1",
           "HALO_TELNET_CONSOLE_PORT": str(port)}
    log = open(tmp_path / "game.log", "w")
    process = subprocess.Popen([str(copy)], cwd=tmp_path, env=env, stdout=log, stderr=subprocess.STDOUT)
    lines = [
        "maps list libtest", "maps list ce: libtest", "maps list pack:", "maps list downloaded:", "maps missing",
        "maps delete bloodgulch", "maps delete libtest_packed", "maps delete libtest_linked", "maps delete libtest_secret",
        "maps delete nothing_here",
        "maps move libtest_alpha ce", "maps move libtest_gamma Mine", "maps move libtest_gamma Away",
        "maps delete libtest_inmaps", "maps move libtest_beta Sorted",
        "maps delete libtest_alpha", "maps move libtest_shared Comp", "maps frob",
        "maps list lib",
    ]
    try:
        connection = _telnet(port, time.time() + 90)
        # (the menus up: the first scan's answer in the log; a busy machine takes longer)
        deadline = time.time() + 150
        while time.time() < deadline and "maps scan" not in _answers((tmp_path / "game.log").read_text(errors="replace")):
            _send(connection, "maps scan", wait=2)
        for line in lines:
            _send(connection, line)
        connection.close()
    finally:
        if process.poll() is None:
            process.terminate()
        try:
            process.wait(timeout=30)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        log.close()
    output = (tmp_path / "game.log").read_text(errors="replace")
    answers = _answers(output)
    print("\n".join(f"{line} -> {answers.get(line)}" for line in ["maps scan"] + lines))
    print("\n".join(row for row in output.splitlines() if "[maps] library" in row))
    assert answers.get("maps scan", "").startswith("ok maps: scanned"), answers
    assert int(re.search(r"scanned (\d+) maps", answers["maps scan"]).group(1)) >= 509, answers["maps scan"]
    # (the fake maps found by source; the junk file never; a linked page folder's map is listed, as the game reads it)
    first = answers["maps list libtest"]
    assert "libtest_alpha" in first and "libtest_beta" in first and "libtest_gamma" in first, first
    assert "libtest_shared" in first and "libtest_packed@testpack" in first, first
    assert "libtest_junk" not in first, first
    # (alpha, beta, gamma, the maps folder's own, the linked file and the linked folder's map: as the game lists them)
    assert answers["maps list ce: libtest"].startswith("ok maps: 6 of"), answers["maps list ce: libtest"]
    assert "libtest_packed@testpack" in answers["maps list pack:"]
    assert "libtest_shared" in answers["maps list downloaded:"]
    assert "required ce:" in answers["maps missing"] and "required mcc:" in answers["maps missing"]
    # (every refusal with its reason)
    assert answers["maps delete bloodgulch"].startswith("error maps: bloodgulch refused: Stock maps")
    assert answers["maps delete libtest_packed"].startswith("error maps: libtest_packed refused: A map pack's")
    assert "is a link" in answers["maps delete libtest_linked"] or "not in the library" in answers["maps delete libtest_linked"]
    assert "outside the maps folders" in answers["maps delete libtest_secret"] or \
        "not in the library" in answers["maps delete libtest_secret"]
    assert answers["maps delete nothing_here"].startswith("error maps: That map is not in the library")
    assert "A folder's name" in answers["maps move libtest_alpha ce"]
    assert "in that folder already" in answers["maps move libtest_gamma Mine"]
    # (a folder that is a link out of the root: never moved into)
    assert "is a link" in answers["maps move libtest_gamma Away"], answers["maps move libtest_gamma Away"]
    assert (data / "maps_ce" / "Mine" / "libtest_gamma.map").is_file()
    assert answers["maps delete libtest_inmaps"] == "ok maps: deleted libtest_inmaps", answers["maps delete libtest_inmaps"]
    assert not (data / "maps" / "libtest_inmaps.map").exists()
    assert answers["maps frob"].startswith("error usage: maps")
    # (the changes made: a move with the file beside it, a delete, a downloaded Xbox map moved into the maps folder's)
    assert answers["maps move libtest_beta Sorted"] == "ok maps: moved libtest_beta to Sorted"
    assert (data / "maps_ce" / "Sorted" / "libtest_beta.map").is_file()
    assert (data / "maps_ce" / "Sorted" / "libtest_beta.txt").is_file()
    assert not (data / "maps_ce" / "libtest_beta.map").exists()
    assert answers["maps delete libtest_alpha"] == "ok maps: deleted libtest_alpha"
    assert not (data / "maps_ce" / "libtest_alpha.map").exists()
    assert answers["maps move libtest_shared Comp"] == "ok maps: moved libtest_shared to Comp"
    assert (data / "maps" / "Comp" / "libtest_shared.map").is_file()
    # (what is outside the roots untouched)
    assert (tmp_path / "outside" / "libtest_secret.map").is_file()
    assert (data / "maps_ce" / "libtest_linked.map").is_symlink()
    last = answers["maps list lib"]
    assert "libtest_alpha" not in last and "libtest_beta" in last, last
