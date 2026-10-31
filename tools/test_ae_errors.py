"""The game's errors with AE's menus on, headless, through the ae_test harness (tools/test_ae_lobby.py's pattern):

- (0.4.0) a joiner at AE's main menu without the host's map (map downloads off) joins an AE host (92: Blood Gulch,
  played once a second player is in) from LAN GAMES: the game queues its "You don't have the map" error for the main
  menu (display_error_text_when_main_menu_loaded). It shows as AE's error dialog over AE's screen, not the game's
  blue ATTENTION dialog (which AE's input never reached: START did nothing, A and B went to AE's screen under it);
  the joiner's LOBBY (its game gone) goes back to LAN GAMES first; START closes the error, and B then leaves LAN GAMES
  (the input reaches AE's screens again).

Runs only when asked ($AE_MENUS_BUILD and the harness's config, as test_ae_menus.py); about 100 s:

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_errors.py
"""
import copy
import importlib.util
import shutil
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_errors_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

HOST = "127.0.0.51"
CLIENT = "127.0.0.52"
MISSING = "bloodgulch"


@pytest.fixture(scope="module")
def cfg():
    c = menus.config()
    if not c:
        pytest.skip("needs the ae_test harness config (data_dir with maps) and a built game ($AE_MENUS_BUILD)")
    return c


def in_order(text, lines):
    position = 0
    for line in lines:
        found = text.find(line, position)
        assert found >= 0, f"missing or out of order: {line!r} (after {text[max(0, position - 400):position]!r})"
        position = found + len(line)


def data_without(cfg, folder, missing):
    """a data folder like the harness's data_dir with every map but missing.map (the maps linked one by one)"""
    harness = menus.harness
    data = harness.expand(cfg["data_dir"])
    if folder.exists():
        shutil.rmtree(folder)
    (folder / "maps").mkdir(parents=True)
    for entry in (data / "maps").iterdir():
        if entry.name != f"{missing}.map":
            (folder / "maps" / entry.name).symlink_to(entry.resolve())
    for name in ("mods", "discs", "packs", "voices"):
        if (data / name).exists():
            (folder / name).symlink_to((data / name).resolve())
    return folder


def play_pair(cfg, out, name, host_env, client_env, client_input, seconds=100, join_delay=2, missing=MISSING,
              host_seconds=None, host_input=None):
    """an AE host with every map and a joiner (without missing.map when named), in one network namespace: (host
    result, client result, host text, client text)"""
    harness = menus.harness
    host = harness.parse_spec({"name": "host", "build": menus.build(), "address": HOST, "broadcast": CLIENT,
                               "exit_after": host_seconds or seconds, "window": "1280x720", "screenshots": 300,
                               "env": host_env, "expect": {"ticks": 0}})
    if host_input:
        host["test_input"] = host_input
    client = harness.parse_spec({"name": "client", "build": menus.build(), "address": CLIENT, "broadcast": HOST,
                                 "exit_after": seconds - join_delay, "delay": join_delay, "window": "1280x720",
                                 "screenshots": 60, "env": client_env, "expect": {"ticks": 0},
                                 "test_input": client_input})
    pdir = out / name
    work = harness.expand(cfg["work_dir"]) / f"{out.name}-{name}"
    harness.OWN_WORK.add(work)
    build = harness.resolve_build(cfg, menus.build())
    client_cfg = copy.deepcopy(cfg)
    if missing:
        client_cfg["data_dir"] = str(data_without(cfg, work / "client-data", missing))
    prepared = [harness.prepare_game(c, sp, build, work / sp["name"], pdir / sp["name"])
                for c, sp in ((cfg, host), (client_cfg, client))]
    inner = harness.run_group(cfg, prepared, None, work)
    results = [harness.collect_game(sp, pr, i, pdir / sp["name"]) for sp, pr, i in zip((host, client), prepared, inner)]
    shutil.rmtree(work, ignore_errors=True)
    texts = [(pdir / sp["name"] / "debug.txt").read_text(errors="replace") if (pdir / sp["name"] / "debug.txt").exists()
             else "" for sp in (host, client)]
    return results[0], results[1], texts[0], texts[1]


def test_missing_map_error_is_ae(cfg):
    """the joiner without the host's map: its LOBBY of a game gone leaves for LAN GAMES, AE's error dialog (the game's
    text) shows over it, START closes it, B closes LAN GAMES; the game's own error dialog never loads"""
    out = menus.out_dir(cfg, "errors-missing-map")
    # (LAN GAMES joins at ~20 s, its profile picked with a second A; the host plays once the joiner is in, the joiner
    # leaves for its main menu without the map; START ~45 s, B ~50 s)
    keys = ("pad:a", "pad:down", "pad:down", "pad:down", "pad:a", "wait", "wait", "pad:a", "wait", "pad:a") + \
        ("wait",) * 23 + \
        ("pad:start",) + ("wait",) * 4 + ("pad:b",)
    host_r, client_r, host, client = play_pair(
        cfg, out, "pair", {"HALO_ARENA_MENUS": "1", "HALO_AE_TEST_SCREEN": "92"},
        {"HALO_ARENA_MENUS": "1", "HALO_AE_TEST_SCREEN": "0", "HALO_NET_MAP_DOWNLOAD": "false"},
        menus.menu_input(*keys))
    assert host_r.get("status") == "PASS", host_r.get("why")
    # (display_error returned before the game's error widget loaded: "taken"; AE's dialog logs the text it shows)
    # (the joiner's LOBBY leaves as the client goes; the error then shows over LAN GAMES)
    in_order(client, ["ae browser: joined", f"map missing: You don't have the map {MISSING}",
                      "ae create game: lobby: the game is gone", "ae errors: error 29 taken",
                      "ae errors: shown: Attention",
                      f"ae menus: error: Attention: You don't have the map {MISSING}", "ae errors: closed",
                      "ae browser: closed"])
    shots = sorted((out / "pair" / "client").rglob("frame*.png"))
    assert shots, "no joiner screenshots"


def test_host_leaves_mid_game(cfg):
    """(0.4.0) the host (CREATE GAME, 32) quits while its joiner plays: the joiner lands back at AE's main menu with the
    game's error as AE's dialog (never the game's widget), START closes it, and the main menu takes the next A"""
    out = menus.out_dir(cfg, "errors-host-leaves")
    # (the joiner in by ~20 s; the host starts at ~30 s and quits at 55 s; the joiner's START at ~75 s, A at ~80 s)
    keys = ("pad:a", "pad:down", "pad:down", "pad:down", "pad:a", "wait", "wait", "pad:a", "wait", "pad:a") + \
        ("wait",) * 53 + \
        ("pad:start",) + ("wait",) * 4 + ("pad:a",)
    host_r, client_r, host, client = play_pair(
        cfg, out, "pair", {"HALO_ARENA_MENUS": "1", "HALO_AE_TEST_SCREEN": "32", "HALO_AE_NETWORK": "lan", "HALO_NET_ONLINE": "false"},
        {"HALO_ARENA_MENUS": "1", "HALO_AE_TEST_SCREEN": "0"}, menus.menu_input(*keys), seconds=110, missing=None,
        host_seconds=55, host_input=menus.menu_input(*(("wait",) * 18 + ("pad:start", "pad:start"))))
    # (the game's "closed down" error, taken while the main menu loads, before AE's is there: shown once it opens)
    in_order(client, ["ae browser: joined", "ae create game: lobby: the host started",
                      "ae errors: error 4 taken", "ae main menu: open", "ae errors: shown: Disconnected",
                      "ae menus: error: Disconnected: ", "ae errors: closed", "ae main menu: MULTIPLAYER"])
