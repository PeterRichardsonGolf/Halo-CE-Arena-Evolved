"""AE's lobby glue in real games, headless, through the ae_test harness (tools/test_ae_menus.py's pattern):

- the lobby drive (debug.ae_test_screen 90) hosts a LAN game with no upstream widget, 3 system-link bots join, it sets
  Blood Gulch and slayer, starts, ends the game, goes back to the lobby, and AE takes the pregame UI (the game's own
  SELECT MAP screen is never loaded);
- with display.arena_menus on but a session AE didn't make (network_test's host), the game's own pregame UI runs
  as before and the hook says it left it to the game;
- (Task 13) an AE joiner (91) joins an AE host (92) on loopback, plays its game, takes the client's pregame UI after
  it, and leaves; a LOCAL host (93) refuses a bot; an ONLINE host (95) with internet play off lists nothing; a join
  with no host gives its reason;
- (Task 14) the profile drive (94) makes, switches, saves and reads back a profile on a fresh save root, and a guest's
  settings are never written.

Runs only when asked ($AE_MENUS_BUILD and the harness's config, as test_ae_menus.py); about 90 s a game:

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_lobby.py
"""
import importlib.util
import re
import shutil
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_lobby_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

SELECT_MAP_FAILED = "failed to load map select postgame screen"


@pytest.fixture(scope="module")
def cfg():
    c = menus.config()
    if not c:
        pytest.skip("needs the ae_test harness config (data_dir with maps) and a built game ($AE_MENUS_BUILD)")
    return c


def play(cfg, out, name, spec):
    base = {"name": name, "build": menus.build(), "window": "1280x720", "screenshots": 120}
    base.update(spec)
    result = menus.harness.play(cfg, base, out / name)
    debug = out / name / "debug.txt"
    text = debug.read_text(errors="replace") if debug.exists() else ""
    pngs = sorted((out / name).rglob("frame*.png"))
    return result, text, pngs


def in_order(text, lines):
    """the index in text of each line, in order; a missing (or out of order) one fails with what came before"""
    position = 0
    for line in lines:
        found = text.find(line, position)
        assert found >= 0, f"missing or out of order: {line!r} (after {text[max(0, position - 400):position]!r})"
        position = found + len(line)


def test_host_bots_round_trip(cfg):
    """host + 3 bots: join, start, end, back to the lobby, with no upstream widget; the take-over shows AE's screen"""
    out = menus.out_dir(cfg, "lobby")
    result, text, pngs = play(cfg, out, "round-trip", {
        "exit_after": 90, "bots": 3,
        "env": {"HALO_ARENA_MENUS": "1", "HALO_AE_TEST_SCREEN": "90"},
        "expect": {"ticks": 0}})
    assert result.get("status") == "PASS", result.get("why")
    in_order(text, ["ae lobby: hosting (LAN)", "ae lobby: local player added (controller 1)", "ae lobby: roster 4",
                    "ae lobby: map bloodgulch", "ae lobby: gametype slayer", "ae lobby: starting",
                    "ae lobby: game ended", "ae lobby: back to pregame", "ae lobby: took the pregame UI",
                    "ae lobby: back in the lobby, roster 4"])
    assert SELECT_MAP_FAILED not in text
    assert "pregame UI left to the game" not in text
    # (task 17: the lobby keeps what was set across the round: the map and a gametype, not the playlist's defaults)
    back = re.search(r"ae lobby: back in the lobby, roster 4 map '([^']*)' gametype '([^']*)'", text)
    assert back and back.group(1) == "bloodgulch", back
    # (the built-in slayer carries no description text: it reads the same before the game and after the round)
    before = re.search(r"ae lobby: set, roster map '([^']*)' gametype '([^']*)'", text)
    assert before and before.groups() == back.groups(), (before, back)
    assert pngs, "no screenshots"
    (out / "back-in-the-lobby.png").write_bytes(pngs[-1].read_bytes())


def test_not_ae_session_left_to_the_game(cfg):
    """preflight P20: display.arena_menus on, a session AE didn't make (network_test hosts, 1-minute games): back
    from the game the hook leaves the pregame UI to the game, whose screen loads"""
    out = menus.out_dir(cfg, "lobby-not-ae")
    result, text, pngs = play(cfg, out, "not-ae", {
        "map": "bloodgulch", "gametype": "slayer,slayer", "start": 10, "exit_after": 110,
        "env": {"HALO_ARENA_MENUS": "1", "HALO_NETWORK_TEST_TIME_LIMIT": "1"}})
    assert result.get("status") == "PASS", result.get("why")
    assert "ae lobby: pregame UI left to the game (not AE's session)" in text
    assert "ae lobby: took the pregame UI" not in text
    assert SELECT_MAP_FAILED not in text


HOST = "127.0.0.200"
CLIENT = "127.0.0.201"


def play_pair(cfg, out, name, host_env, client_env, seconds=100, join_delay=6):
    """two machines in one network namespace (tools/ae_test/handshake.py's way, with AE's drives instead of
    network_test): (host result, client result, host text, client text)"""
    harness = menus.harness
    host = harness.parse_spec({"name": "host", "build": menus.build(), "address": HOST, "broadcast": CLIENT,
                               "exit_after": seconds, "window": "1280x720", "screenshots": 300, "env": host_env,
                               "expect": {"ticks": 0}})
    client = harness.parse_spec({"name": "client", "build": menus.build(), "address": CLIENT, "broadcast": HOST,
                                 "exit_after": seconds - join_delay, "delay": join_delay, "window": "1280x720",
                                 "screenshots": 300, "env": client_env, "expect": {"ticks": 0}})
    pdir = out / name
    work = harness.expand(cfg["work_dir"]) / f"{out.name}-{name}"
    harness.OWN_WORK.add(work)
    build = harness.resolve_build(cfg, menus.build())
    prepared = [harness.prepare_game(cfg, sp, build, work / sp["name"], pdir / sp["name"]) for sp in (host, client)]
    inner = harness.run_group(cfg, prepared, None, work)
    results = [harness.collect_game(sp, pr, i, pdir / sp["name"]) for sp, pr, i in zip((host, client), prepared, inner)]
    shutil.rmtree(work, ignore_errors=True)
    texts = [(pdir / sp["name"] / "debug.txt").read_text(errors="replace") if (pdir / sp["name"] / "debug.txt").exists()
             else "" for sp in (host, client)]
    return results[0], results[1], texts[0], texts[1]


def roster_players(text):
    """the drive's player lines: (slot, name, local, host)"""
    import re
    return [(int(m[1]), m[2], int(m[3]), int(m[4]))
            for m in re.finditer(r"ae lobby: player (\d+) '([^']*)' local (\d) host (\d)", text)]


def test_join_and_leave(cfg):
    """an AE host (92) and an AE joiner (91): the joiner joins, both see 2 players (the joiner's own player local, the
    host's the host: from the real machine index), the host plays and ends a game, both come back to AE's lobby (the
    client's take-over), the joiner leaves and the host's roster goes back to 1"""
    out = menus.out_dir(cfg, "lobby-join")
    host_r, client_r, host, client = play_pair(cfg, out, "pair",
                                               {"HALO_ARENA_MENUS": "1", "HALO_AE_TEST_SCREEN": "92"},
                                               {"HALO_ARENA_MENUS": "1", "HALO_AE_TEST_SCREEN": "91"})
    for side, r in (("host", host_r), ("client", client_r)):
        assert r.get("status") == "PASS", (side, r.get("why"))
    in_order(client, ["ae lobby: joined", "ae lobby: roster 2", "ae lobby: took the pregame UI", "ae lobby: left"])
    in_order(host, ["ae lobby: hosting (LAN)", "ae lobby: roster 2", "ae lobby: took the pregame UI",
                    "ae lobby: back in the lobby, roster 2", "ae lobby: roster 1"])
    assert SELECT_MAP_FAILED not in host and SELECT_MAP_FAILED not in client
    # (from the client's side: one local player, its own, not the host; the host's player is the host, not local)
    players = roster_players(client)
    assert len(players) == 2, players
    assert len([p for p in players if p[2]]) == 1, players
    assert all(not (p[2] and p[3]) for p in players) and len([p for p in players if p[3]]) == 1, players
    host_players = roster_players(host)
    assert any(p[2] and p[3] for p in host_players), host_players


def test_local_refuses(cfg):
    """a LOCAL host (93) with a bot: nobody joins (the roster stays 1, the bot tool joins nothing)"""
    out = menus.out_dir(cfg, "lobby-local")
    result, text, pngs = play(cfg, out, "local", {
        "exit_after": 45, "bots": 1,
        "env": {"HALO_ARENA_MENUS": "1", "HALO_AE_TEST_SCREEN": "93"}, "expect": {"ticks": 0}})
    assert result.get("status") == "PASS", result.get("why")
    assert "ae lobby: hosting (LOCAL)" in text
    rosters = [line.rsplit(" ", 1)[1] for line in text.splitlines() if "ae lobby: roster " in line]
    # (every roster line 1: nobody admitted, not even for a while)
    assert rosters and all(r == "1" for r in rosters), rosters
    bots = (out / "local" / "bots.log")
    bot_log = bots.read_text(errors="replace") if bots.exists() else ""
    # (system_link_bots.py logs "all N machines are in the lobby" once they joined)
    assert bot_log and "in the lobby" not in bot_log, bot_log[-800:]


def test_online_lists_only_when_allowed(cfg):
    """preflight P21: value 95 hosts ONLINE; with internet play off (HALO_NET_ONLINE=false) nothing is hosted for the
    internet and nothing listed: no invite, no game-list line"""
    out = menus.out_dir(cfg, "lobby-online")
    result, text, pngs = play(cfg, out, "online", {
        "exit_after": 35, "env": {"HALO_ARENA_MENUS": "1", "HALO_AE_TEST_SCREEN": "95", "HALO_NET_ONLINE": "false"},
        "expect": {"ticks": 0}})
    assert result.get("status") == "PASS", result.get("why")
    assert "ae lobby: hosting (ONLINE, invite only)" in text
    assert "Internet play: hosting" not in text and "Game list: the game is listed" not in text


def test_online_is_invite_only(cfg):
    """the M2 ruling: an ONLINE host is INVITE ONLY whatever network.host_public says: with internet play on (and
    host_public on), it hosts for invites but never lists the game in everyone's server browser"""
    out = menus.out_dir(cfg, "lobby-online-on")
    result, text, pngs = play(cfg, out, "online-on", {
        "exit_after": 45, "env": {"HALO_ARENA_MENUS": "1", "HALO_AE_TEST_SCREEN": "95", "HALO_NET_ONLINE": "true",
                                  "HALO_NET_HOST_PUBLIC": "true", "HALO_NET_PUBLIC_LOBBY": "true"},
        "expect": {"ticks": 0}})
    assert result.get("status") == "PASS", result.get("why")
    assert "ae lobby: hosting (ONLINE, invite only)" in text
    # (the positive control: internet play really hosted, so a public listing would have been logged here)
    assert "Internet play: hosting" in text
    assert "listed in everyone's server browser" not in text
    # (network.list_hosted_games is off by default: the OpenCE game list is that opt-in's, M4's PRIVACY row)
    assert "Game list: the game is listed" not in text


def test_join_reason_no_game(cfg):
    """a join with no host: "No game found on the LAN" within 15 s, no assert"""
    out = menus.out_dir(cfg, "lobby-no-game")
    result, text, pngs = play(cfg, out, "no-game", {
        "exit_after": 35, "env": {"HALO_ARENA_MENUS": "1", "HALO_AE_TEST_SCREEN": "91"}, "expect": {"ticks": 0}})
    assert result.get("status") == "PASS", result.get("why")
    import re
    in_order(text, ["ae lobby: searching the LAN", "ae lobby: join gave up after", "ae lobby: join: No game found on the LAN"])
    # (within 15 s of the search's start: the glue's 10 s)
    seconds = int(re.search(r"ae lobby: join gave up after (\d+) s", text)[1])
    assert 10 <= seconds <= 15, seconds


def profile_files(save_root):
    """the profiles' files in a save root (the hard drive's saved games, u:\\UDATA\\<id>\\blam.sav: a game
    variant's is blam.lst, saved_game_files.c; the game's default profiles under z:\\saved are not players'):
    {path: (name, colour, button preset, look sensitivity, invert, vibration disabled)}, each read as the game
    writes it (player_profile.c: the 0x30-byte struct player_profile first, the name 12 UCS-2 characters)"""
    import struct
    files = {}
    for path in sorted(Path(save_root).rglob("blam.sav")):
        if "default_profile" in [p.lower() for p in path.parts]:
            continue
        data = path.read_bytes()
        if len(data) < 0x30:
            continue
        name = data[:24].decode("utf-16-le").split("\0")[0]
        colour, = struct.unpack_from("<h", data, 24)
        button, joystick, sensitivity, invert, vibration_disabled = struct.unpack_from("<5B", data, 40)
        files[str(path)] = (name, colour, button, sensitivity, invert, vibration_disabled)
    return files


def test_profile_drive(cfg):
    """the profiles' glue (94) on a fresh throwaway save root: a profile made, player 1's, controls 2 5 1 0 and colour
    3 saved, read back from the file by the game and here from the save root; player 2 a guest: their settings apply
    to the session and nothing is written (the profile files the same before and after)"""
    import re
    harness = menus.harness
    out = menus.out_dir(cfg, "lobby-profiles")
    run_id = "ae-profiles-" + harness.stamp()
    spec = {"name": "profiles", "build": menus.build(), "window": "1280x720", "exit_after": 25,
            "env": {"HALO_ARENA_MENUS": "1", "HALO_AE_TEST_SCREEN": "94"}, "expect": {"ticks": 0}}
    work = harness.expand(cfg["work_dir"]) / run_id
    try:
        result = harness.play(cfg, spec, out / "profiles", keep_work=True, run_id=run_id)
        debug = out / "profiles" / "debug.txt"
        text = debug.read_text(errors="replace") if debug.exists() else ""
        save_root = Path(result.get("env", {}).get("HALO_SAVE_ROOT", work / "missing"))
        files = profile_files(save_root)
        listing = sorted(str(p.relative_to(save_root)) for p in save_root.rglob("*") if p.is_file()) \
            if save_root.exists() else []
    finally:
        shutil.rmtree(work, ignore_errors=True)
    assert result.get("status") == "PASS", result.get("why")
    before = int(re.search(r"ae profiles: count (\d+)\s*$", text, re.M)[1])
    made = re.search(r"ae profiles: new 'AE TEST' -> index ([0-9A-F]{8})", text)
    assert made, text[-1500:]
    index = made[1]
    in_order(text, [f"ae profiles: new 'AE TEST' -> index {index}", "ae profiles: new: That name is already used",
                    "ae profiles: new: A name can't use that character",
                    "ae profiles: new: A name can't use that character",
                    "ae profiles: new: Names are up to 11 characters",
                    f"ae profiles: player 1 uses {index}", "ae profiles: controls 2 5 1 0 saved",
                    "ae profiles: colour 3 saved", "ae profiles: read back controls 2 5 1 0 colour 3",
                    "ae profiles: player 1 controls 2 5 1 0 in the game",
                    "ae profiles: colour 4 saved", "ae profiles: saved past an idle upstream edit buffer",
                    f"ae profiles: count {before + 1} before the guest", "ae profiles: 'AE TEST' colour ",
                    "ae profiles: player 2 guest", "ae profiles: player 2 is guest 1, player 1 is guest 0",
                    "ae profiles: guest controls not saved",
                    "ae profiles: guest controls ok 1 (A guest's settings are not saved)",
                    "ae profiles: guest controls 3 7 0 1 this session", "ae profiles: guest colour not saved",
                    f"ae profiles: count {before + 1} after the guest", "ae profiles: done"])
    # (the save root: the one profile made, as the drive left it: colour 4 last, controls 2 5 1 0; the guest's
    # controls 3 7 0 1 and colour 5 nowhere, and no other file)
    assert len(files) == before + 1, (files, listing)
    assert ("AE TEST", 4, 2, 5, 1, 1) in files.values(), files
    assert not [f for f in files.values() if f[0].startswith("Guest")], files
