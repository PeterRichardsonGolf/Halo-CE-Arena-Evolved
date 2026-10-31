"""CREATE GAME, one screen with its lobby (0.3.9: the game on the left, the lobby panel on the right), in real games,
headless, through the ae_test harness (tools/ae_test), in tools/test_ae_gallery.py's pattern (tools/test_ae_menus.py's
helpers).

- CREATE GAME (debug.ae_test_screen 32) at 1080p 100 % and 130 %: it hosts at once (LAN: internet play is off here),
  player 1 in, the map chosen, the copy applied, the lobby live;
- 130 %: the rows scroll with the focus, down past START GAME into the panel;
- BOT SETUP (the party's BOT SETUP...): the popover at 100 % and 130 %; NETWORK's list to HOST ONLINE: the lobby opens
  again online, PUBLIC on
  (internet play off: its INVITE row says so);
- a quick option stepped and applied to the server;
- START GAME: the game plays (no AE screen over it), the gametype's PC options in it, the game first in RECENT
  (recent-games.txt);
- BOTS and BOT SKILL at the panel's foot: the lobby's bots join at once, PLAYERS counts the person alone;
- a maps folder without beavercreek.map: CREATE GAME starts on the first map that has its file;
- (0.4.3) START GAME's countdown: it counts (a short one, debug.ae_lobby_countdown_ms) and the game starts, START
  during it does nothing; the host's B cancels it (the server's countdown held, START GAME back); a guest pad's B is
  refused and the game starts.

Runs only when asked ($AE_MENUS_BUILD and the harness's config); each game is 30-50 s:

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_create_game.py
    AE_MENUS_BUILD=$PWD/build/linux64 python3 tools/test_ae_create_game.py      (the frames alone)
"""
import importlib.util
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_create_game_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

# name, debug.ae_test_screen, UI SCALE, the presses (after the lead-in: one a second), seconds in all
CASES = (
    ("create", 32, 100, (), 30),
    ("create-130", 32, 130, (), 30),
    # (130 %: the rows scroll with the focus, the help kept: down past START GAME into the party)
    ("create-130-foot", 32, 130, ("pad:down",) * 15, 38),
    # (START GAME, the primary action, focused: down to the last row of the left column)
    ("create-start", 32, 100, ("pad:down",) * 12, 36),
    # (0.4.2 the party view: X, down to BOTS (the host, ADD PLAYER, SHUFFLE TEAMS), three bots, then BOT SKILL)
    ("panel-lobby", 32, 100, ("wait", "pad:x", "pad:down", "pad:down", "pad:down", "pad:right", "pad:right",
                              "pad:right", "wait", "pad:down"), 40),
    ("panel-lobby-130", 32, 130, ("wait", "pad:x", "pad:down", "pad:down", "pad:down", "pad:right", "pad:right",
                                  "pad:right", "wait", "pad:down"), 40),
    # (BOT SETUP: one bot, then BOT SETUP... (under BOT SKILL), A opens it over the party)
    ("setup", 32, 100, ("wait", "pad:x", "pad:down", "pad:down", "pad:down", "pad:right", "pad:down", "pad:down",
                        "pad:a"), 36),
    ("setup-130", 32, 130, ("wait", "pad:x", "pad:down", "pad:down", "pad:down", "pad:right", "pad:down", "pad:down",
                            "pad:a"), 36),
    # (0.3.9: BOT SETUP's rows: SHOOTING SKILL and MOVEMENT SKILL to RECRUIT, PLAYFIGHT ON (A), BOT TEAMS AGAINST YOU;
    # and the popover at 130 %)
    ("setup-bots", 32, 100, ("wait", "pad:x", "pad:down", "pad:down", "pad:down", "pad:right", "pad:down", "pad:down",
                             "pad:a", "pad:right", "pad:down", "pad:right", "pad:down", "pad:a", "pad:down",
                             "pad:right", "wait"), 50),
    ("setup-bots-130", 32, 130, ("wait", "pad:x", "pad:down", "pad:down", "pad:down", "pad:right", "pad:down",
                                 "pad:down", "pad:a") + ("pad:down",) * 3 + ("wait",), 44),
    # (0.3.9: RB jumps to the party, down past ADD PLAYER and SHUFFLE TEAMS to BOTS, one right there; LB back to the
    # left column's row (MAP), RB again comes back to BOTS; the footer's "LB RB Column" at 100 and 130 %)
    ("columns", 32, 100, ("wait", "pad:rb", "pad:down", "pad:down", "pad:down", "pad:right", "pad:lb", "pad:rb",
                          "wait"), 42),
    ("columns-130", 32, 130, ("wait", "pad:rb", "wait"), 32),
    # (0.3.9 split screen: the keyboard down past START GAME into the party (the host), ADD PLAYER, Enter; the one pad
    # leaves the keyboard's controller for its own, its START opens its profile card, A joins it as a guest (focused),
    # its B takes that player out again)
    ("split", 32, 100, ("wait",) + ("type:Down",) * 14 + ("type:Return", "wait", "wait", "pad:start", "wait",
                                                         "pad:a", "wait", "wait", "pad:b", "wait"), 62),
    # (0.4.0 the guest's pad cannot change the bot rows: the joined pad down to BOTS, right)
    ("split-bots", 32, 100, ("wait",) + ("type:Down",) * 14 + ("type:Return", "wait", "wait", "pad:start", "wait",
                                                              "pad:a", "wait", "wait", "pad:down", "pad:down",
                                                              "pad:right", "wait", "wait"), 64),
    # (NETWORK's list: HOST ONLINE: the lobby opens again online; then PUBLIC on)
    # (down past MAP, GAMETYPE, GAME OPTIONS, the 6 quick options and MORE OPTIONS... to NETWORK)
    ("online", 32, 100, ("wait",) + ("pad:down",) * 10 + ("pad:a", "pad:down", "pad:a", "wait", "wait",
                                                          "pad:down", "pad:right", "wait"), 42),
    # (the lobby: the party's BOTS, 3 bots, then BOT SKILL focused)
    ("lobby-bots", 32, 100, ("wait", "pad:x", "pad:down", "pad:down", "pad:down", "pad:right", "pad:right",
                             "pad:right", "wait", "wait", "pad:down"), 44),
    # (a quick option: down past MAP, GAMETYPE, GAME OPTIONS to the first, stepped)
    ("lobby", 32, 100, ("wait",) + ("pad:down",) * 3 + ("pad:right",), 34),
    # (0.4.2 A on a quick option: its list, the row's name over it)
    ("dropdown", 32, 100, ("wait",) + ("pad:down",) * 3 + ("pad:a", "pad:down", "wait"), 36),
    # (0.4.2 the whole lobby: six bots, X twice: LOBBY's team columns, RB to the blue one)
    ("see-all", 32, 100, ("wait", "pad:x", "pad:down", "pad:down", "pad:down") + ("pad:right",) * 6 +
     ("wait", "pad:x", "wait", "pad:rb", "wait"), 46),
    # (0.4.2 MORE OPTIONS...: down past MAP, GAMETYPE, GAME OPTIONS and the 6 quick options to it, A opens the popover;
    # LOADOUT to CATEGORY, down to PRIMARY WEAPON, right: the next weapon, LOADOUT CUSTOM again)
    ("more", 32, 100, ("wait",) + ("pad:down",) * 9 + ("pad:a", "pad:right", "pad:down", "pad:right", "wait"), 40),
    ("more-130", 32, 130, ("wait",) + ("pad:down",) * 9 + ("pad:a", "pad:down", "wait"), 36),
    # (A on PRIMARY WEAPON: its list, the row's name over it)
    ("more-list", 32, 100, ("wait",) + ("pad:down",) * 9 + ("pad:a", "pad:down", "pad:a", "pad:down", "wait"), 40),
    # (a guest's pad: player 2 joined, its A on MORE OPTIONS... is refused)
    ("split-more", 32, 100, ("wait",) + ("type:Down",) * 14 + ("type:Return", "wait", "wait", "pad:start", "wait",
                                                              "pad:a", "wait", "type:Escape", "wait") +
     ("type:Up",) * 3 + ("pad:a", "wait"), 64),
    # (0.4.2: START brings the focus to START GAME; (0.4.3, owner) only the row itself starts the game: A there. One
    # START starts nothing, and neither does a second START on the row)
    ("play", 32, 100, ("wait", "wait", "pad:start", "pad:a"), 50),
    ("start-once", 32, 100, ("wait", "wait", "pad:start", "wait", "wait"), 36),
    ("start-twice", 32, 100, ("wait", "wait", "pad:start", "pad:start", "wait", "wait"), 36),
    # (the keyboard plays player 1: in MY PARTY (X), the one pad's START joins it as player 2 with no ADD PLAYER
    # first (its profile card on controller 2, A: GUEST); the game does not start. (0.4.3, owner) Outside MY PARTY
    # its START is refused, the help saying so)
    ("lonepad", 32, 100, ("wait", "pad:x", "type:Down", "type:Up", "pad:start", "wait", "pad:a", "wait", "wait"), 46),
    ("lonepad-outside", 32, 100, ("wait", "type:Down", "type:Up", "pad:start", "wait", "pad:a", "wait", "wait"), 44),
    # (0.4.3 the countdown, 3 s here (COUNTDOWN_ENV): START then A on START GAME counts, START during it does nothing,
    # the game starts)
    ("countdown", 32, 100, ("wait", "wait", "pad:start", "pad:a", "pad:start", "wait"), 50),
    # (the host's B one second into the default 5 s count: cancelled, START GAME back, no game)
    ("countdown-cancel", 32, 100, ("wait", "wait", "pad:start", "pad:a", "pad:b", "wait", "wait", "wait", "wait",
                                   "wait", "wait", "wait"), 40),
    # (split screen: player 2 joined on the pad (as "split"), the keyboard back to START GAME (Esc) starts the count
    # (Enter), the guest pad's B is refused, the game starts)
    ("countdown-guest", 32, 100, ("wait",) + ("type:Down",) * 14 + ("type:Return", "wait", "wait", "pad:start", "wait",
                                                                   "pad:a", "wait", "wait", "type:Escape",
                                                                   "type:Return", "pad:b", "wait", "wait"), 72),
)
# (the cases with their own count: a short one, so the suite is not slowed)
COUNTDOWN_ENV = {"countdown": {"HALO_AE_LOBBY_COUNTDOWN_MS": "2999"}}


def play(cfg, out, name, value, scale, presses, seconds):
    env = {"HALO_ARENA_MENUS_SCALE": str(scale), "HALO_AE_TEST_GAMETYPE": "AE TEAM SLY", "HALO_NET_ONLINE": "false"}
    env.update(COUNTDOWN_ENV.get(name, {}))
    # (0.7.0: a new lobby starts OFFLINE, as the create cases check; the others drive a LAN lobby's rows, as before)
    if name not in ("create", "create-130", "no-beavercreek"):
        env.setdefault("HALO_AE_NETWORK", "lan")
    return menus.play(cfg, out, name, views=value, env=env, shots=8, exit_after=seconds,
                      test_input=menus.menu_input(*presses) if presses else menus.menu_input("wait"))


def lines(text, start):
    return [line.split(start, 1)[1] for line in text.splitlines() if start in line]


def run_cases(cfg, out, only=()):
    """the cases (only those named, when names are given)"""
    runs = {}
    for name, value, scale, presses, seconds in CASES:
        if only and name not in only:
            continue
        result, text, pngs = play(cfg, out, name, value, scale, presses, seconds)
        frame = None
        if pngs:
            frame = out / f"{name}.png"
            frame.write_bytes(pngs[-1].read_bytes())
        runs[name] = (result, text, frame)
    return runs


@pytest.fixture(scope="module")
def cfg():
    c = menus.config()
    if not c:
        pytest.skip("needs the ae_test harness config (data_dir with maps) and a built game ($AE_MENUS_BUILD)")
    return c


@pytest.fixture(scope="module")
def runs(cfg):
    return run_cases(cfg, menus.out_dir(cfg, "create-game"))


def test_create_game_opens_a_live_lobby(runs):
    for name in ("create", "create-130"):
        said = lines(runs[name][1], "ae create game: ")
        assert "open" in said, (name, said)
        assert any(line.startswith("hosting (OFFLINE, ") for line in said), (name, said)
        assert said.index("hosted, player 1 in") < said.index("lobby: open"), (name, said)
        assert "lobby: players 1, bots 0" in said, (name, said)
        assert not [line for line in said if line.startswith("open failed")], (name, said)


def test_lobby_setup(runs):
    for name in ("setup", "setup-130"):
        assert "lobby: setup open" in lines(runs[name][1], "ae create game: "), name
    said = lines(runs["online"][1], "ae create game: ")
    assert "lobby: public on" in said, said
    reopen = said.index("lobby: network HOST ONLINE, reopening")
    hosted = [index for index, line in enumerate(said) if line == "hosted, player 1 in"]
    assert len(hosted) == 2 and hosted[0] < reopen < hosted[1], said
    assert any(line.startswith("hosting (HOST ONLINE, ") for line in said[reopen:]), said
    assert "ae lobby: hosting (ONLINE, invite only)" in runs["online"][1]


def test_party_view_and_lobby(runs):
    """(0.4.2) X opens the party view, X again the whole lobby (LOBBY); A on a quick option opens its list"""
    said = lines(runs["see-all"][1], "ae create game: ")
    assert "lobby: column panel" in said and "lobby: see all" in said, said
    assert "lobby: bots 6" in said, said
    assert "lobby: players 1, bots 0" in lines(runs["panel-lobby"][1], "ae create game: ")
    assert "lobby: bots 3" in lines(runs["panel-lobby"][1], "ae create game: ")


def test_quick_option_reaches_the_server(runs):
    said = lines(runs["lobby"][1], "ae create game: ")
    assert "hosted, player 1 in" in said and "lobby: open" in said, said
    assert any(line.startswith("lobby: quick ") for line in said), said
    assert any("to the server" in line for line in lines(runs["lobby"][1], "ae game options: applied ")), said


def test_bots_rows(runs):
    said = lines(runs["lobby-bots"][1], "ae create game: ")
    assert "lobby: bots 1" in said and "lobby: bots 3" in said, said
    # (the lobby's bots follow the setting at once; PLAYERS counts the person alone)
    assert lines(runs["lobby-bots"][1], "lan bots: bot 3 joined the lobby"), said
    assert "lobby: players 1, bots 3" in said, said


def test_lobby_setup_bot_rows(runs):
    """(0.3.9) with bots, LOBBY SETUP holds the classic menus' four bot settings, each written to config.toml at once"""
    text = runs["setup-bots"][1]
    said = lines(text, "ae create game: ")
    for line in ("lobby: shooting skill RECRUIT", "lobby: movement skill RECRUIT", "lobby: playfight ON",
                 "lobby: teams AGAINST YOU"):
        assert line in said, (line, said)
    assert "could not" not in "\n".join(said)
    assert "lobby: setup open" in lines(runs["setup-bots-130"][1], "ae create game: ")


def test_columns(runs):
    """(0.3.9) RB to the lobby panel, LB back: no scrolling down past START GAME to reach the lobby"""
    said = lines(runs["columns"][1], "ae create game: ")
    columns = [line for line in said if line.startswith("lobby: column ")]
    assert columns == ["lobby: column panel", "lobby: column left", "lobby: column panel"], said
    assert "lobby: bots 1" in said, said
    assert "lobby: column panel" in lines(runs["columns-130"][1], "ae create game: ")


def test_split_screen_join(runs):
    """(0.3.9) ADD PLAYER, then a second controller's START joins it (a guest's profile: no nameless player); its B
    takes the player out, the lobby stays"""
    text = runs["split"][1]
    said = lines(text, "ae create game: ")
    assert "lobby: add player on" in said, said
    assert "local player added (controller 2)" in lines(text, "ae lobby: "), lines(text, "ae lobby: ")
    assert "player 2 guest" in lines(text, "ae profiles: ")
    assert "local player left (controller 2)" in lines(text, "ae lobby: ")
    assert "lobby: left" not in said, said


def test_a_guest_pad_cannot_change_the_bot_rows(runs):
    """(0.4.0) players 2-4 see BOTS and BOT SKILL, and LOBBY SETUP's bot rows, and the host's player 1 changes them"""
    text = runs["split-bots"][1]
    said = lines(text, "ae create game: ")
    assert "local player added (controller 2)" in lines(text, "ae lobby: "), lines(text, "ae lobby: ")
    assert any("the bot rows are the host's" in line for line in said), said
    assert not [line for line in lines(text, "ae bots: ") if "count" in line and "set" in line], text[-800:]


def test_more_options(runs):
    """(0.4.2) MORE OPTIONS... opens its popover; its rows are live as the quick rows: a start weapon picked sets
    LOADOUT to CUSTOM, the copy reaches the server, GAME OPTIONS counts the changes"""
    text = runs["more"][1]
    said = lines(text, "ae create game: ")
    assert "lobby: more open" in said, said
    assert "lobby: more LOADOUT CATEGORY" in said, said
    primary = [line for line in said if line.startswith("lobby: more PRIMARY WEAPON ")]
    assert primary, said
    assert said.index("lobby: more LOADOUT CATEGORY") < said.index(primary[0]) < said.index("lobby: more LOADOUT CUSTOM"), said
    assert len([line for line in lines(text, "ae game options: applied ") if "to the server" in line]) >= 2, said
    assert "lobby: more open" in lines(runs["more-130"][1], "ae create game: ")
    assert "lobby: more open" in lines(runs["more-list"][1], "ae create game: ")


def test_a_guest_pad_cannot_open_more_options(runs):
    text = runs["split-more"][1]
    said = lines(text, "ae create game: ")
    assert "local player added (controller 2)" in lines(text, "ae lobby: "), lines(text, "ae lobby: ")
    assert any("the game's rules are the host's" in line for line in said), said
    assert "lobby: more open" not in said, said


def test_one_start_only_focuses_start_game(runs):
    said = lines(runs["start-once"][1], "ae create game: ")
    assert "lobby: START to START GAME" in said, said
    assert "lobby: start" not in said, said


def test_start_never_starts_the_game(runs):
    """(0.4.3, owner 2026-10-28) only the START GAME row starts a game: START on it again does nothing"""
    said = lines(runs["start-twice"][1], "ae create game: ")
    assert said.count("lobby: START to START GAME") == 1, said
    assert "lobby: start" not in said, said


def test_the_lobby_starts_the_game(runs):
    text = runs["play"][1]
    assert "lobby: start" in lines(text, "ae create game: ")
    # ((0.4.3) the default count first: 5 s)
    assert "starting (countdown 5999 ms)" in lines(text, "ae lobby: ")
    assert "lobby: countdown 5 s" in " ".join(lines(text, "ae create game: "))
    # (player 1 plays with a profile, not as "Player": the lobby gives it the one used last before adding it)
    assert "player 1 has no active player profile assigned" not in text
    # (the gametype's PC options reach the game, not the defaults: AE TEAM SLY's motion sensor shows enemies only
    # and it has a time limit)
    rules = lines(text, "game rules: ")
    assert rules and "radar players enemies" in rules[-1] and "time limit none" not in rules[-1], rules[-1:]
    # (AE menus Phase 2: the game the host started first in RECENT, its map's list name and the gametype's id)
    recent = lines(text, "ae pickers: recent game ")
    assert recent and recent[-1].endswith("| 'AE TEAM SLY'"), recent


def countdown_numbers(text):
    """the numbers the countdown showed on this machine, in order ("ae lobby: countdown N")"""
    return [int(line) for line in lines(text, "ae lobby: countdown ") if line.isdigit()]


def test_the_countdown_starts_the_game(runs):
    """(0.4.3) START GAME counts down (3 s here) on the server's own countdown, a beep a number on the UI voice (soft
    with the gametype's PRE-GAME COUNTDOWN on: AE TEAM SLY's own 3-2-1 beeps in the game; else a tick and GO), START
    during it does nothing, then the game starts"""
    text = runs["countdown"][1]
    said = lines(text, "ae create game: ")
    assert said.count("lobby: start") == 1, said
    counted = [line for line in said if line.startswith("lobby: countdown ")]
    assert counted and counted[0].startswith("lobby: countdown 2 s ("), said
    assert "starting (countdown 2999 ms)" in lines(text, "ae lobby: ")
    assert countdown_numbers(text) == [2, 1, 0], lines(text, "ae lobby: countdown")
    beeps = lines(text, "ae sound: countdown ")
    if "quiet: PRE-GAME COUNTDOWN on" in counted[0]:
        assert beeps == ["tick 2 (soft)", "tick 1 (soft)"], beeps
    else:
        assert beeps == ["tick 2", "tick 1", "go"], beeps
    # (START during the count: not a second start, no "START to START GAME" either)
    assert "lobby: START to START GAME" not in said[said.index("lobby: start"):], said
    assert "game rules:" in text and "lobby: the game runs" in said, said


def test_the_host_cancels_the_countdown(runs):
    """(0.4.3) the host's B during the count: the server's countdown held and NONE sent (every machine stops
    showing it), START GAME back, the lobby not left, no game"""
    text = runs["countdown-cancel"][1]
    said = lines(text, "ae create game: ")
    assert "lobby: start" in said and "lobby: start cancelled (player 1)" in said, said
    assert said.index("lobby: start") < said.index("lobby: start cancelled (player 1)"), said
    assert "start cancelled (the countdown held, NONE sent)" in lines(text, "ae lobby: ")
    assert "lobby: leave" not in said and "lobby: left" not in said, said
    assert countdown_numbers(text) and 0 not in countdown_numbers(text), lines(text, "ae lobby: countdown")
    assert "game rules:" not in text
    assert "lobby: the game runs" not in said, said


def test_a_guest_pad_cannot_cancel_the_countdown(runs):
    """(0.4.3) player 2's pad (split screen) presses B during the count: refused as its START is, the game starts"""
    text = runs["countdown-guest"][1]
    said = lines(text, "ae create game: ")
    assert "local player added (controller 2)" in lines(text, "ae lobby: "), lines(text, "ae lobby: ")
    assert "lobby: start" in said, said
    assert "lobby: cancel refused: a guest pad" in said, said
    assert "lobby: start cancelled (player 1)" not in said, said
    assert "local player left (controller 2)" not in lines(text, "ae lobby: ")
    assert "game rules:" in text, said


def without_battle_creek(cfg, out):
    """the harness's config on a data root with every map of its data_dir but beavercreek.map (links)"""
    data = menus.harness.expand(cfg["data_dir"])
    root = out / "data-no-beavercreek"
    (root / "maps").mkdir(parents=True, exist_ok=True)
    for entry in (data / "maps").iterdir():
        link = root / "maps" / entry.name
        if entry.name.lower() != "beavercreek.map" and not link.exists():
            link.symlink_to(entry.resolve())
    for folder in ("mods", "discs", "packs", "voices"):
        if (data / folder).exists() and not (root / folder).exists():
            (root / folder).symlink_to((data / folder).resolve())
    return dict(cfg, data_dir=str(root))


def test_create_game_without_battle_creek(cfg):
    """(0.3.9) a maps folder without beavercreek.map: CREATE GAME, with no map used last, starts on the first map of
    the list that has its file (it chose Battle Creek, the list's first, listed with or without its file: the game's
    damaged-disc loop then ran the AE screens inside itself, which chose it again until the stack ran out)"""
    out = menus.out_dir(cfg, "create-no-beavercreek")
    result, text, pngs = play(without_battle_creek(cfg, out), out, "no-beavercreek", 32, 100, (), 30)
    said = lines(text, "ae create game: ")
    assert "couldn't find map 'beavercreek'" not in text
    assert any(line.startswith("hosting (OFFLINE, ") for line in said), said
    assert not any(line.startswith("hosting (OFFLINE, 'Battle Creek'") for line in said), said
    assert "hosted, player 1 in" in said and "lobby: open" in said, said
    assert not [line for line in said if line.startswith("open failed")], said


def test_a_host_that_fails_shows_its_dialog(cfg):
    """(0.4.0) hosting fails ("Could not host the game": another program holds the game's port, here a stand-in started
    in the game's own namespace by a wrapper in place of the binary): the error dialog opens before the screen's first
    draw (it read the density's metrics, still empty: a crash), the game goes on"""
    out = menus.out_dir(cfg, "create-host-fails")
    out.mkdir(parents=True, exist_ok=True)
    real = Path(menus.build()).resolve() / "halo"
    holder = out / "halo-with-port-held"
    holder.write_text("#!/bin/sh\n"
                      "python3 -c 'import socket,time;"
                      "u=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);u.bind((\"0.0.0.0\",5150));"
                      "t=socket.socket();t.bind((\"0.0.0.0\",5150));time.sleep(90)' &\n"
                      "sleep 1\n"
                      f"exec {real} \"$@\"\n")
    holder.chmod(0o755)
    env = {"HALO_ARENA_MENUS_SCALE": "100", "HALO_AE_TEST_GAMETYPE": "AE TEAM SLY", "HALO_NET_ONLINE": "false"}
    spec_build = menus.build
    menus.build = lambda: str(holder)
    try:
        result, text, pngs = menus.play(cfg, out, "host-fails", views=32, env=env, shots=8, exit_after=20,
                                        test_input=menus.menu_input("wait"))
    finally:
        menus.build = spec_build
    said = lines(text, "ae create game: ")
    assert any(line.startswith("open failed") for line in said), said
    assert "ae menus: error: The lobby did not open: Could not host the game" in text, text[-600:]
    assert "exiting after debug.exit_after" in text, text[-600:]
    assert "lobby: open" not in said, said
    assert result.get("status") != "FAIL", result

def test_lobby_lone_pad_start_joins(runs):
    """the keyboard plays player 1: a lone pad's START signs it in as player 2, the game not started"""
    text = runs["lonepad"][1]
    said = lines(text, "ae create game: ")
    assert "lobby: the keyboard's lone pad joins (controller 2)" in said, said
    assert "lobby: add player on" in said, said
    assert "ae lobby: starting" not in text
    said = lines(text, "ae lobby: ")
    assert said.index("local player added (controller 1)") < said.index("local player added (controller 2)"), said


def test_a_pad_joins_only_in_my_party(runs):
    """(0.4.3, owner 2026-10-28) a pad's START outside MY PARTY's party view joins nobody: refused, the game not
    started"""
    text = runs["lonepad-outside"][1]
    said = lines(text, "ae create game: ")
    assert "lobby: a pad's START outside MY PARTY: no join" in said, said
    assert "lobby: the keyboard's lone pad joins (controller 2)" not in said, said
    assert "local player added (controller 2)" not in lines(text, "ae lobby: ")
    assert "ae lobby: starting" not in text


if __name__ == "__main__":
    c = menus.config()
    if not c:
        sys.exit("needs the ae_test harness config and a built game ($AE_MENUS_BUILD)")
    folder = menus.out_dir(c, "create-game")
    done = run_cases(c, folder, tuple(sys.argv[1:]))
    for n, (result, text, frame) in done.items():
        print(n, frame, (lines(text, "ae create game: ") + lines(text, "ae lobby: "))[-6:])
