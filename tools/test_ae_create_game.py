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
    ("create-130-foot", 32, 130, ("pad:down",) * 16, 38),
    # (START GAME, the primary action, focused: down to the last row of the left column)
    ("create-start", 32, 100, ("pad:down",) * 13, 36),
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
    ("split", 32, 100, ("wait",) + ("type:Down",) * 15 + ("type:Return", "wait", "wait", "pad:start", "wait",
                                                         "pad:a", "wait", "wait", "pad:b", "wait"), 62),
    # (0.4.0 the guest's pad cannot change the bot rows: the joined pad starts on the host's row, down past the guest's,
    # ADD PLAYER and SHUFFLE TEAMS to BOTS, right)
    ("split-bots", 32, 100, ("wait",) + ("type:Down",) * 15 + ("type:Return", "wait", "wait", "pad:start", "wait",
                                                              "pad:a", "wait", "wait", "pad:down", "pad:down",
                                                              "pad:down", "pad:down", "pad:right", "wait", "wait"), 64),
    # (NETWORK's list: HOST ONLINE: the lobby opens again online; then PUBLIC on)
    # (down past MAP, GAMETYPE, GAME OPTIONS, the 6 quick options and MORE OPTIONS... to NETWORK)
    ("online", 32, 100, ("wait",) + ("pad:down",) * 10 + ("pad:a", "pad:down", "pad:a", "wait", "wait",
                                                          "pad:down", "pad:down", "pad:right", "wait"), 42),
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
    ("split-more", 32, 100, ("wait",) + ("type:Down",) * 15 + ("type:Return", "wait", "wait", "pad:start", "wait",
                                                              "pad:a", "wait", "type:Escape", "wait") +
     ("type:Up",) * 5 + ("pad:a", "wait"), 64),
    # (0.7.6 a guest's pad refuses every host row of the left column: the keyboard walks up from START GAME one row at a
    # time, the joined pad's A on each: HOST RULES, MAX PLAYERS, GAME NAME, NETWORK, MORE OPTIONS, the six quick options,
    # GAME OPTIONS, GAMETYPE, MAP (its right too))
    ("split-guard", 32, 100, ("wait",) + ("type:Down",) * 15 + ("type:Return", "wait", "wait", "pad:start", "wait",
                                                              "pad:a", "wait", "type:Escape", "wait") +
     ("type:Up", "pad:a") * 14 + ("pad:right", "wait"), 90),
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
    ("countdown-guest", 32, 100, ("wait",) + ("type:Down",) * 15 + ("type:Return", "wait", "wait", "pad:start", "wait",
                                                                   "pad:a", "wait", "wait", "type:Escape",
                                                                   "type:Return", "pad:b", "wait", "wait"), 72),
    # (0.7.4 GAME NAME, LAN: down past MAP .. NETWORK to it (the 11th row); on a keyboard Enter types in place (a typed
    # name over the selected one, Enter keeps); on a pad A opens AE's keyboard beside the column; OFFLINE has no row)
    ("name-row", 32, 100, ("wait",) + ("pad:down",) * 11 + ("wait",), 36),
    ("name-row-130", 32, 130, ("wait",) + ("pad:down",) * 11 + ("wait",), 36),
    ("name-typing", 32, 100, ("wait",) + ("type:Down",) * 11 + ("type:Return", "wait") +
     tuple(f"type:{c}" for c in "Friday") + ("type:Space",) + tuple(f"type:{c}" for c in "4v4s") + ("wait",), 44),
    ("name-typed", 32, 100, ("wait",) + ("type:Down",) * 11 + ("type:Return", "wait") +
     tuple(f"type:{c}" for c in "Friday") + ("type:Space",) + tuple(f"type:{c}" for c in "4v4s") +
     ("type:Return", "wait", "wait"), 46),
    ("name-escape", 32, 100, ("wait",) + ("type:Down",) * 11 + ("type:Return", "wait", "type:X", "type:Escape", "wait"), 40),
    ("name-long", 32, 100, ("wait",) + ("type:Down",) * 11 + ("type:Return", "wait") +
     tuple(f"type:{c}" for c in "ABCDEFGHIJKLMNOPQRST") + ("type:Return", "wait", "wait"), 70),
    ("name-pad", 32, 100, ("wait",) + ("pad:down",) * 11 + ("pad:a", "wait"), 40),
    ("name-offline", 32, 100, ("wait",) + ("pad:down",) * 11 + ("wait",), 36),
    # (0.7.5 HOST RULES..., the row after MAX PLAYERS on a LAN lobby: A opens the panel in MY PARTY's place; at 130 % it
    # scrolls with the focus; a PUBLIC online game holds TEXT CHAT; Y restores the defaults)
    ("host-rules", 32, 100, ("wait",) + ("pad:down",) * 13 + ("pad:a", "wait", "pad:down", "pad:down", "pad:right",
                                                                "wait"), 40),
    ("host-rules-130", 32, 130, ("wait",) + ("pad:down",) * 13 + ("pad:a", "wait") + ("pad:down",) * 8 + ("wait",), 44),
    ("host-rules-public", 32, 100, ("wait",) + ("pad:down",) * 12 + ("pad:a",) + ("pad:down",) * 2 + ("pad:a", "wait",
                                                                                                 "pad:right", "wait"), 44),
    # (0.7.5 HOST ONLINE with a VPN up: the dialog, then STAY ON LAN; and HOST ANYWAY)
    ("vpn-confirm", 32, 100, ("wait",) + ("pad:down",) * 10 + ("pad:a", "pad:down", "pad:a", "wait"), 40),
    # (0.7.5 the join helper's dialog, over the lobby here: a Workshop map, packs older than the host's, no answer)
    ("join-helper-workshop", 32, 100, ("wait", "wait"), 30),
    ("join-helper-update", 32, 100, ("wait", "wait"), 30),
    ("join-helper-retry", 32, 130, ("wait", "wait"), 30),
    # (0.7.6 INVITE pinned over START GAME, HOST ONLINE (the test link: no internet game here): the list scrolls at 130 %
    # and the row is on screen at every place it stops; A on it copies; MY PARTY's INVITE FRIEND beside ADD PLAYER (down
    # to it, or right from ADD PLAYER) copies too)
    ("invite-scroll", 32, 130, ("wait",) + ("pad:down",) * 16 + ("wait",), 44),
    ("invite-row", 32, 100, ("wait",) + ("pad:down",) * 15 + ("pad:a", "wait", "wait"), 40),
    ("invite-friend", 32, 100, ("wait", "pad:x", "pad:down", "pad:down", "pad:a", "wait", "wait"), 40),
    ("invite-pair", 32, 100, ("wait", "pad:x", "pad:down", "pad:right", "pad:a", "wait", "wait"), 40),
    # (0.7.6 PLAYLIST..., the row above MAP (the host starts on MAP: up, A): the panel over MY PARTY, from a seeded
    # playlist of three games. Weights (right on the first game, left on the second), the switches under BETWEEN GAMES:
    # MAP VOTE off (its value says 3 CHOICES + SAME AGAIN while on), NO REPEATS off)
    ("playlist-edit", 32, 100, ("wait", "pad:up", "pad:a", "wait", "pad:down", "pad:right", "pad:down", "pad:left",
                                "pad:down", "pad:down", "pad:down", "pad:right", "pad:down", "pad:right", "wait"), 46),
    # (the first game grabbed with a held A and moved down twice: "hold:a" keeps A down through the tokens that follow
    # (a token is pressed for 250 ms only), each "pad:down" moves the game a place, "hold:none" lets A go: it drops third;
    # a press of A alone changes the game instead (its CHANGE MAP picker opens))
    ("playlist-reorder", 32, 100, ("wait", "pad:up", "pad:a", "wait", "pad:down", "hold:a", "pad:down", "pad:down",
                                   "hold:none", "wait", "wait"), 46),
    # (the grab mid-way, for the picture: A still held on the first game, moved one place down)
    ("playlist-grab", 32, 100, ("wait", "pad:up", "pad:a", "wait", "pad:down", "hold:a", "pad:down", "wait"), 24),
    # (a short press of A on a game changes it: its CHANGE MAP picker opens, the gametype's after it)
    ("playlist-change", 32, 100, ("wait", "pad:up", "pad:a", "wait", "pad:down", "pad:a", "wait", "wait"), 32),
    # (PLAYLIST turned on (right on its first row): it picks the first game by weight, and MAP and GAMETYPE become UP NEXT)
    ("playlist-on", 32, 100, ("wait", "pad:up", "pad:a", "wait", "pad:right", "wait", "pad:b", "wait"), 40),
    # (a guest's pad is refused on PLAYLIST... too: up from START GAME to it, A)
    ("split-guard-playlist", 32, 100, ("wait",) + ("type:Down",) * 15 + ("type:Return", "wait", "wait", "pad:start", "wait",
                                                                      "pad:a", "wait", "type:Escape", "wait") +
     ("type:Up",) * 15 + ("pad:a", "wait"), 70),
    # (the grab mid-way, for the picture: A still held on the first game, moved one place down)
    ("playlist-grab", 32, 100, ("wait", "pad:up", "pad:a", "wait", "pad:down", "pad:a", "pad:a+down", "pad:a"), 24),
    # (a game's X takes it out; the playlist turned on picks a game of its own: PLAYLIST's right)
    ("playlist-remove", 32, 100, ("wait", "pad:up", "pad:a", "wait", "pad:down", "pad:down", "pad:x", "wait"), 40),
    ("playlist-130", 32, 130, ("wait", "pad:up", "pad:a", "wait", "pad:down", "pad:down", "wait"), 40),
    ("vpn-anyway", 32, 100, ("wait",) + ("pad:down",) * 10 + ("pad:a", "pad:down", "pad:a", "wait", "pad:right", "pad:a",
                                                             "wait", "wait"), 50),
)
# (the cases with their own count: a short one, so the suite is not slowed)
COUNTDOWN_ENV = {"countdown": {"HALO_AE_LOBBY_COUNTDOWN_MS": "2999"}}


def play(cfg, out, name, value, scale, presses, seconds):
    env = {"HALO_ARENA_MENUS_SCALE": str(scale), "HALO_AE_TEST_GAMETYPE": "AE TEAM SLY", "HALO_NET_ONLINE": "false"}
    env.update(COUNTDOWN_ENV.get(name, {}))
    if name.startswith("vpn"):
        env["HALO_AE_TEST_VPN"] = "ProtonVPN"
    if name.startswith("join-helper"):
        env["HALO_AE_TEST_JOIN_HELPER"] = {"workshop": "5", "update": "1", "retry": "7"}[name.split("-")[-1]]
        env["HALO_AE_TEST_JOIN_HELPER_MAP"] = "frigid keep@ws2899770689"
    if name == "host-rules-public" or name.startswith("invite"):
        env["HALO_AE_NETWORK"] = "online"
    if name.startswith("invite"):
        env["HALO_AE_TEST_INVITE_LINK"] = "halo://join/" + "0123456789abcdef" * 4
    # (0.7.0: a new lobby starts OFFLINE, as the create cases check; the others drive a LAN lobby's rows, as before)
    if name not in ("create", "create-130", "no-beavercreek", "name-offline"):
        env.setdefault("HALO_AE_NETWORK", "lan")
    save = seeded_playlist_save(out) if name.startswith("playlist") else None
    return menus.play(cfg, out, name, views=value, env=env, shots=8, exit_after=seconds,
                      test_input=menus.menu_input(*presses) if presses else menus.menu_input("wait"), save_from=save)


PLAYLIST_FILE = ("# playlist: 4V4 ARENA\n# map vote: 3\n# no repeats: 1\n"
                 "bloodgulch:file=AE TEAM SLY  # weight 3\nprisoner:file=AE FFA SLAY  # weight 2\n"
                 "hangemhigh:file=AE TEAM SLY  # weight 2\n")


def seeded_playlist_save(out):
    """a save root (in out: never /tmp) whose private folder holds a playlist of three games"""
    save = out / "playlist-save"
    (save / "private").mkdir(parents=True, exist_ok=True)
    (save / "private" / "playlist.txt").write_text(PLAYLIST_FILE)
    return save


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

def test_a_guest_pad_cannot_change_any_host_row(runs):
    """(0.7.6, owner) every host row of the left column refuses a guest pad: the setup rows, the game's rules (MORE
    OPTIONS, the quick options, GAME OPTIONS), the gametype, the map and the host rules; nothing opens, nothing changes"""
    text = runs["split-guard"][1]
    said = lines(text, "ae create game: ")
    assert "local player added (controller 2)" in lines(text, "ae lobby: "), lines(text, "ae lobby: ")
    refused = [line for line in said if "are the host's" in line or "is the host's" in line]
    for what, least in (("the lobby's setup rows", 3), ("the game's rules", 8), ("the gametype", 1), ("the map", 2),
                        ("the host rules", 1)):
        count = len([line for line in refused if line.startswith(f"lobby: {what} ")])
        assert count >= least, (what, count, refused)
    assert "lobby: more open" not in said and "lobby: setup open" not in said, said
    assert not [line for line in said if line.startswith("lobby: quick ")], said


def test_one_start_only_focuses_start_game(runs):
    said = lines(runs["start-once"][1], "ae create game: ")
    assert "lobby: START to START GAME" in said, said
    assert "lobby: start" not in said, said


def test_start_never_starts_the_game(runs):
    """(0.4.3, owner 2026-10-28) only the START GAME row starts a game: START on it again does nothing"""
    said = lines(runs["start-twice"][1], "ae create game: ")
    assert said.count("lobby: START to START GAME") == 1, said
    assert "lobby: start" not in said, said


def test_invite_is_pinned_over_start_game(runs):
    """(0.7.6, owner) HOST ONLINE: the INVITE row is pinned over START GAME, on screen at every place the list scrolls
    to (130 %: the rows scroll with the focus), and the focus reaches it right after the list, then START GAME"""
    said = lines(runs["invite-scroll"][1], "ae create game: ")
    places = [line for line in said if line.startswith("lobby: list at row ")]
    assert places, said
    assert all(line.endswith("INVITE pinned shown") for line in places), places
    assert len({line.split(",")[0] for line in places}) >= 3, places


def test_invite_row_and_invite_friend_copy_the_link(runs):
    """A on the pinned INVITE row, on MY PARTY's INVITE FRIEND (down to it, or right from ADD PLAYER) copy the link"""
    for name in ("invite-row", "invite-friend", "invite-pair"):
        text = runs[name][1]
        assert "lobby: invite copied" in lines(text, "ae create game: "), (name, lines(text, "ae create game: "))
        assert "ae invite: copied" in text, name


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


def test_game_name_is_typed_in_place_and_reaches_the_server(runs):
    said = lines(runs["name-typed"][1], "ae create game: lobby: game name ")
    assert any(line.startswith("typing from ''") for line in said), said
    kept = [line for line in said if line.startswith("'") and "typing" not in line]
    assert len(kept) == 1 and len(kept[0].split("'")[1]) == 11, said
    name = kept[0].split("'")[1]
    assert f"name '{name}', most players" in runs["name-typed"][1]
    assert name.lower() == "friday 4v4s", name


def test_game_name_stops_at_15_characters(runs):
    said = lines(runs["name-long"][1], "ae create game: lobby: game name '")
    assert said and len(said[-1].split("'")[0]) == 15, said


def test_game_name_escape_keeps_the_old_name(runs):
    text = runs["name-escape"][1]
    assert "game name typing dropped" in text
    assert "ae create game: lobby: game name '" not in text


def test_host_rules_opens_in_the_partys_place(runs):
    for name in ("host-rules", "host-rules-130", "host-rules-public"):
        assert "ae create game: lobby: host rules open" in runs[name][1], name
    assert "ae create game: lobby: host rule network.team_change" in runs["host-rules"][1] or \
        "ae create game: lobby: host rule network.voice_mode" in runs["host-rules"][1]


def test_vpn_confirm_asks_before_hosting_online(runs):
    assert "attention: A VPN IS ON" in runs["vpn-confirm"][1]
    assert "ae create game: stayed on LAN: a VPN is up" not in runs["vpn-confirm"][1]
    assert "ae create game: HOST ANYWAY with a VPN up" in runs["vpn-anyway"][1]


def test_join_helper_shows_its_cause(runs):
    assert "join helper: cause 5: Needs maps_pe/badcreek.map to join" in runs["join-helper-workshop"][1]
    assert "attention: GET THE MAP FIRST" in runs["join-helper-workshop"][1]
    assert "attention: UPDATE TO JOIN" in runs["join-helper-update"][1]
    assert "attention: THE HOST DIDN'T ANSWER" in runs["join-helper-retry"][1]


def test_game_name_on_a_pad_opens_the_keyboard(runs):
    assert "ae create game: lobby: game name typing from ''" in runs["name-pad"][1]


def test_playlist_panel_edits_in_place(runs):
    """(0.7.6, owner) PLAYLIST... opens in MY PARTY's place; a game's weight steps with left and right (1-5), the vote
    says SAME AGAIN and its switches turn, every change saved in the rotation's file"""
    text = runs["playlist-edit"][1]
    said = lines(text, "ae create game: ")
    assert "lobby: playlist '4V4 ARENA' read: 3 games" in said, said
    assert "lobby: playlist open" in said, said
    assert "lobby: playlist: game 1 weight 4 (50 %)" in said, said
    assert "lobby: playlist: game 2 weight 1 (14 %)" in said, said
    assert "lobby: playlist: map vote off" in said and "lobby: playlist: no repeats off" in said, said
    assert said.count("lobby: playlist saved (3 games)") >= 4, said
    # (the panel is not the host rules)
    assert "lobby: host rules open" not in said, said


def test_playlist_vote_text_says_same_again():
    """the panel's MAP VOTE value, while on, names SAME AGAIN (no row of its own: always there with the vote)"""
    source = (HERE / ".." / "port" / "linux" / "game" / "ae_screen_create_game.c").read_text()
    assert '"3 CHOICES + SAME AGAIN"' in source


def test_playlist_reorder_with_a_held_a(runs):
    """a pad's held A grabs a game, down moves it a place a press, letting go drops it and saves the order"""
    text = runs["playlist-reorder"][1]
    said = lines(text, "ae create game: ")
    assert "lobby: playlist: grabbed game 1" in said, said
    assert "lobby: playlist: game 1 moved to 2" in said and "lobby: playlist: game 2 moved to 3" in said, said
    assert "lobby: playlist: dropped at 3" in said, said


def test_playlist_remove_and_wide(runs):
    said = lines(runs["playlist-remove"][1], "ae create game: ")
    assert "lobby: playlist: removed game 2" in said, said
    assert "lobby: playlist saved (2 games)" in said, said
    assert "lobby: playlist open" in lines(runs["playlist-130"][1], "ae create game: ")


def test_a_guest_pad_cannot_open_the_playlist(runs):
    text = runs["split-guard-playlist"][1]
    said = lines(text, "ae create game: ")
    assert "local player added (controller 2)" in lines(text, "ae lobby: "), lines(text, "ae lobby: ")
    assert any(line.startswith("lobby: the playlist is the host's") or line.startswith("lobby: the playlist are the host's")
               for line in said), said
    assert "lobby: playlist open" not in said, said


def test_playlist_on_picks_the_next_game(runs):
    said = lines(runs["playlist-on"][1], "ae create game: ")
    assert "lobby: playlist on" in said, said
    up_next = [line for line in said if line.startswith("lobby: playlist: up next '")]
    assert up_next and not any("not here" in line for line in up_next), said


def test_a_short_press_changes_a_game(runs):
    text = runs["playlist-change"][1]
    said = lines(text, "ae create game: ")
    assert "lobby: playlist: pressed on game 1 (controller 0)" in said, said
    assert "lobby: playlist: let go of game 1 (changed)" in said, said
    assert "ae change map: open" in text
