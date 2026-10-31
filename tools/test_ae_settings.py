"""The 0.3.7 menus' SETTINGS (debug.ae_test_screen 34: the top level; 35: a group) in real games, headless, through
the ae_test harness (tools/ae_test), in tools/test_ae_game_options.py's pattern (tools/test_ae_menus.py's helpers).

- the top level and CONTROLS / VIEW / HUD at 1080p 100 % and 130 %;
- a value stepped (BUTTON LAYOUT, LOOK SENSITIVITY): changed, in use at once, saved to the player's ae.toml;
- B with changes: KEEP / UNDO (KEEP keeps, UNDO puts the opened values back), B without changes just goes back;
- Y RESTORE DEFAULTS: CANCEL first, then RESTORE DEFAULTS changes the shown rows back;
- the picker (A), LB / RB sections;
- two players on this PC: X switches player, a change is the second player's own;
- every profile gets its ae.toml seeded on first use;
- (phase B) VIDEO, AUDIO, ONLINE, ACCESSIBILITY and CHAT AND STREAMING (this PC's: config.toml, beside each game's own
  binary in the harness): each opened at 100 % and 130 %, a row changed, KEEP / UNDO, RESTORE DEFAULTS; a display row's
  timed KEEP / UNDO; ACCESSIBILITY's LANGUAGE (a RESTART row) asking to restart as SETTINGS closes.

Runs only when asked ($AE_MENUS_BUILD and the harness's config); each game is about 30 s:

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_settings.py
    AE_MENUS_BUILD=$PWD/build/linux64 python3 tools/test_ae_settings.py [case ...]   (the frames and a sheet alone)
"""
import importlib.util
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_settings_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

EXIT_AFTER = 30
CONTROLS, VIDEO, VIEW, AUDIO, HUD, CHAT, ONLINE, ACCESSIBILITY, SYSTEM = 0, 1, 2, 3, 4, 5, 6, 7, 8
GROUP_NAMES = ("CONTROLS", "VIDEO", "VIEW", "AUDIO", "HUD", "CHAT AND STREAMING", "ONLINE", "ACCESSIBILITY", "SYSTEM")
# name, screen (34 top, 35 a group), group, players, window, UI SCALE, the presses (after the lead-in: one a second)
CASES = (
    ("top", 34, 0, 1, "1920x1080", 100, ()),
    ("top-130", 34, 0, 1, "1920x1080", 130, ()),
    ("controls", 35, CONTROLS, 1, "1920x1080", 100, ()),
    ("controls-130", 35, CONTROLS, 1, "1920x1080", 130, ()),
    ("view", 35, VIEW, 1, "1920x1080", 100, ()),
    ("hud", 35, HUD, 1, "1920x1080", 100, ()),
    ("hud-130", 35, HUD, 1, "1920x1080", 130, ()),
    # (BUTTON LAYOUT one right: SWAP TRIGGERS; down twice to LOOK SENSITIVITY, one right: 4)
    ("changed", 35, CONTROLS, 1, "1920x1080", 100, ("pad:right", "pad:down", "pad:down", "pad:right")),
    # (the same with no profile chosen yet, as AE's menus right after a launch: player 1 takes the one used last, else
    # the first; 0.3.8 refused every CONTROLS profile row there, "BUTTON LAYOUT 1 not set")
    ("changed-fresh", 35, CONTROLS, 1, "1920x1080", 100, ("pad:right", "pad:down", "pad:down", "pad:right")),
    # (a player's own row (VIEW's FOV, one right), B, KEEP: written to the profile's ae.toml at once; and the same with
    # no profile chosen yet: player 1 takes the one used last first. 0.3.7-0.3.9 kept every such change in memory only:
    # a profile's index is negative, which read as none)
    ("view-saved", 35, VIEW, 1, "1920x1080", 100, ("pad:right", "pad:b", "pad:a")),
    ("view-saved-fresh", 35, VIEW, 1, "1920x1080", 100, ("pad:right", "pad:b", "pad:a")),
    # (a change, then B: KEEP / UNDO, shown)
    ("keep-dialog", 35, CONTROLS, 1, "1920x1080", 100, ("pad:right", "pad:b")),
    # (KEEP: back to nothing; UNDO: down once, A)
    ("keep", 35, CONTROLS, 1, "1920x1080", 100, ("pad:right", "pad:b", "pad:a")),
    ("undo", 35, CONTROLS, 1, "1920x1080", 100, ("pad:right", "pad:b", "pad:down", "pad:a")),
    # (no change: B goes back with no dialog)
    ("back", 35, VIEW, 1, "1920x1080", 100, ("pad:b",)),
    # (two changes, then Y: RESTORE DEFAULTS asked (CANCEL first): shown, then down, A restores)
    ("defaults-dialog", 35, HUD, 1, "1920x1080", 100, ("pad:right", "pad:down", "pad:right", "pad:y")),
    ("defaults", 35, HUD, 1, "1920x1080", 100, ("pad:right", "pad:down", "pad:right", "pad:y", "pad:down", "pad:a")),
    # (A: the values' picker; RB three times: the fourth section, SCOREBOARD after HUD, RETICLE and NAMES)
    ("picker", 35, VIEW, 1, "1920x1080", 100, ("pad:a",)),
    ("sections", 35, HUD, 1, "1920x1080", 100, ("pad:rb", "pad:rb", "pad:rb")),
    # (two players: X switches to player 2, a change there is theirs: LEFT STICK DEADZONE, 5 %)
    ("seat", 35, CONTROLS, 2, "1920x1080", 100, ("pad:x",) + ("pad:down",) * 7 + ("pad:right",)),
    ("top-seat", 34, 0, 2, "1920x1080", 100, ("pad:x",)),
    # (a change for player 1, then X: KEEP / UNDO first; KEEP, then player 2)
    ("seat-keep", 35, CONTROLS, 2, "1920x1080", 100, ("pad:right", "pad:x", "pad:a")),
    # (SYSTEM from the top level (the ninth group); MENUS, a RESTART row, stepped as the owner did (CLASSIC, back, again,
    # then picked from its values); B, KEEP: back on the top level, still open; its B asks RESTART NOW / LATER)
    ("restart", 34, 0, 1, "1920x1080", 100, ("pad:down",) * 8 + ("pad:a", "pad:right", "pad:left", "pad:right",
                                                                 "pad:a", "pad:a", "pad:b", "pad:a", "wait", "wait",
                                                                 "pad:b", "wait")),
    # (the same by the mouse: the prompts' Back, KEEP CHANGES, the top level's Back)
    ("restart-mouse", 34, 0, 1, "1920x1080", 100, ("down",) * 8 + ("a", "right", "click:53:452", "wait",
                                                                    "click:218:234", "wait", "wait", "click:40:452",
                                                                    "wait", "wait")),
    # (phase B) VIDEO: RB twice to PERFORMANCE, one right (FPS: POSITION shows), B: KEEP / UNDO, UNDO (down, A)
    ("video", 35, VIDEO, 1, "1920x1080", 100, ()),
    ("video-130", 35, VIDEO, 1, "1920x1080", 130, ()),
    ("video-changed", 35, VIDEO, 1, "1920x1080", 100, ("pad:rb", "pad:rb", "pad:right")),
    ("video-undo", 35, VIDEO, 1, "1920x1080", 100, ("pad:rb", "pad:rb", "pad:right", "pad:b", "pad:down", "pad:a")),
    # (DISPLAY MODE one left (the hidden window is WINDOWED, the last value): the timed KEEP / UNDO, shown; B undoes it)
    ("video-display", 35, VIDEO, 1, "1920x1080", 100, ("pad:left", "wait")),
    ("video-display-undo", 35, VIDEO, 1, "1920x1080", 100, ("pad:left", "wait", "pad:b", "wait")),
    # (0.7.0) FRAME RATE LIMIT typed like the mouse rows (four down from DISPLAY MODE: WINDOW SIZE, RESOLUTION SCALING,
    # V-SYNC, then it): 237 kept (a value no list entry has: it shows as itself, 30 to 1000), left and right step to the
    # list's neighbours (200, 240), 5 is held to 30, 5000 to 1000, Esc drops; a pad's A lists ALL VALUES with the typed
    # value in its place and TYPE A NUMBER... last (the keyboard opens from it)
    ("fps", 35, VIDEO, 1, "1920x1080", 100, ("pad:down",) * 4),
    ("fps-typing", 35, VIDEO, 1, "1920x1080", 100, ("pad:down",) * 4 + ("key:Return", "key:2", "key:3", "wait")),
    ("fps-typing-130", 35, VIDEO, 1, "1920x1080", 130, ("pad:down",) * 4 + ("key:Return", "key:2", "key:3", "wait")),
    ("fps-typed", 35, VIDEO, 1, "1920x1080", 100, ("pad:down",) * 4 + ("key:Return", "key:2", "key:3", "key:7",
                                                                       "key:Return", "wait")),
    ("fps-step", 35, VIDEO, 1, "1920x1080", 100, ("pad:down",) * 4 + ("key:Return", "key:2", "key:3", "key:7",
                                                                      "key:Return", "pad:left", "pad:right")),
    ("fps-low", 35, VIDEO, 1, "1920x1080", 100, ("pad:down",) * 4 + ("key:Return", "key:5", "key:Return")),
    ("fps-high", 35, VIDEO, 1, "1920x1080", 100, ("pad:down",) * 4 + ("key:Return", "key:5", "key:0", "key:0", "key:0",
                                                                      "key:Return")),
    ("fps-cancel", 35, VIDEO, 1, "1920x1080", 100, ("pad:down",) * 4 + ("key:Return", "key:2", "key:Escape", "wait")),
    ("fps-picker", 35, VIDEO, 1, "1920x1080", 100, ("pad:down",) * 4 + ("key:Return", "key:2", "key:3", "key:7",
                                                                        "key:Return", "pad:a", "wait")),
    ("fps-keyboard", 35, VIDEO, 1, "1920x1080", 100, ("pad:down",) * 4 + ("pad:a",) + ("pad:down",) * 20 +
     ("pad:a", "wait", "wait")),
    # AUDIO: SOUND is the first row; down once, MASTER VOLUME one left (9), B: KEEP; RESTORE DEFAULTS puts it back
    # (down, A); SOUND itself one right (OFF, a RESTART row)
    ("audio", 35, AUDIO, 1, "1920x1080", 100, ()),
    ("audio-130", 35, AUDIO, 1, "1920x1080", 130, ()),
    ("audio-keep", 35, AUDIO, 1, "1920x1080", 100, ("pad:down", "pad:left", "pad:b", "pad:a")),
    ("audio-defaults", 35, AUDIO, 1, "1920x1080", 100, ("pad:down", "pad:left", "pad:y", "pad:down", "pad:a")),
    ("audio-sound", 35, AUDIO, 1, "1920x1080", 100, ("pad:right",)),
    # ONLINE: RB to MAPS, DOWNLOAD MAPS one right (OFF: INTERNET DOWNLOADS goes), B, UNDO (down, A)
    ("online", 35, ONLINE, 1, "1920x1080", 100, ()),
    ("online-130", 35, ONLINE, 1, "1920x1080", 130, ()),
    ("online-changed", 35, ONLINE, 1, "1920x1080", 100, ("pad:rb", "pad:right")),
    ("online-undo", 35, ONLINE, 1, "1920x1080", 100, ("pad:rb", "pad:right", "pad:b", "pad:down", "pad:a")),
    # (INTERNET SHARING: RB to MAPS, down 2, one right)
    ("online-sharing", 35, ONLINE, 1, "1920x1080", 100, ("pad:rb", "pad:down", "pad:down", "pad:right")),
    # SYSTEM: the FILES and CAMPAIGN rows (down to WEAPON PACKS: right; MODDED DISC, MCC FILES: right; the campaign's
    # two by RB, past UPDATES); shots only for the rest
    ("system", 35, SYSTEM, 1, "1920x1080", 100, ()),
    ("system-130", 35, SYSTEM, 1, "1920x1080", 130, ()),
    ("system-files", 35, SYSTEM, 1, "1920x1080", 100, ("pad:down", "pad:down", "pad:right", "pad:down", "pad:down",
                                                       "pad:right")),
    # (UPDATES: AUTO UPDATE by two RB, one right)
    ("system-updates", 35, SYSTEM, 1, "1920x1080", 100, ("pad:rb", "pad:rb", "pad:right")),
    ("system-campaign", 35, SYSTEM, 1, "1920x1080", 100, ("pad:rb", "pad:rb", "pad:rb", "pad:right", "pad:down",
                                                          "pad:right")),
    # ACCESSIBILITY: MENU SIZE one right (115 %, the screen laid out again at once); the link PLAYER NAME SIZE (down 4,
    # A: HUD's row); LANGUAGE (a RESTART row) from the top level: DEUTSCH, B, KEEP, B asks RESTART NOW / LATER
    ("accessibility", 35, ACCESSIBILITY, 1, "1920x1080", 100, ()),
    ("accessibility-130", 35, ACCESSIBILITY, 1, "1920x1080", 130, ()),
    ("accessibility-size", 35, ACCESSIBILITY, 1, "1920x1080", 100, ("pad:right",)),
    ("accessibility-link", 35, ACCESSIBILITY, 1, "1920x1080", 100, ("pad:down",) * 4 + ("pad:a", "wait")),
    ("language-restart", 34, 0, 1, "1920x1080", 100, ("pad:down",) * 7 + ("pad:a", "pad:down", "pad:down", "pad:right",
                                                                         "pad:b", "pad:a", "wait", "pad:b", "wait")),
    # CHAT AND STREAMING: STREAMER MODE (down 2, left: ON, the CENSOR rows show), then RESTORE DEFAULTS (Y, down, A)
    ("chat", 35, CHAT, 1, "1920x1080", 100, ()),
    ("chat-130", 35, CHAT, 1, "1920x1080", 130, ()),
    ("chat-streamer", 35, CHAT, 1, "1920x1080", 100, ("pad:down", "pad:down", "pad:left")),
    ("chat-defaults", 35, CHAT, 1, "1920x1080", 100, ("pad:down", "pad:down", "pad:left", "pad:y", "pad:down",
                                                      "pad:a")),
    # (0.3.9) MOUSE's sensitivities to 0.01: RB twice to MOUSE, down to HORIZONTAL SENSITIVITY (1.0). Shift with
    # right: 1.01, right alone: the next step, 1.25 ("+shift", "+ctrl": the harness holds that modifier for AE)
    ("mouse", 35, CONTROLS, 1, "1920x1080", 100, ("pad:rb", "pad:rb", "pad:down")),
    ("mouse-130", 35, CONTROLS, 1, "1920x1080", 130, ("pad:rb", "pad:rb", "pad:down")),
    ("mouse-fine", 35, CONTROLS, 1, "1920x1080", 100, ("pad:rb", "pad:rb", "pad:down", "key:Right+shift",
                                                       "key:Right")),
    # (Enter types: 1.37 kept, then B, KEEP: written to the profile's ae.toml; the field shown mid-typing at 100 and
    # 130 %; Esc drops what was typed; 9 is held to the range's 4.0; a pad's A still lists ALL VALUES)
    ("mouse-typed", 35, CONTROLS, 1, "1920x1080", 100, ("pad:rb", "pad:rb", "pad:down", "key:Return", "key:1",
                                                        "key:.", "key:3", "key:7", "key:Return", "pad:b",
                                                        "pad:a")),
    ("mouse-typing", 35, CONTROLS, 1, "1920x1080", 100, ("pad:rb", "pad:rb", "pad:down", "key:Return", "key:1",
                                                         "key:.", "key:3", "wait")),
    ("mouse-typing-130", 35, CONTROLS, 1, "1920x1080", 130, ("pad:rb", "pad:rb", "pad:down", "key:Return", "key:1",
                                                             "key:.", "key:3", "wait")),
    ("mouse-cancel", 35, CONTROLS, 1, "1920x1080", 100, ("pad:rb", "pad:rb", "pad:down", "key:Return", "key:2",
                                                         "key:Escape", "wait")),
    ("mouse-clamp", 35, CONTROLS, 1, "1920x1080", 100, ("pad:rb", "pad:rb", "pad:down", "key:Return", "key:9",
                                                        "key:Return")),
    ("mouse-pad-a", 35, CONTROLS, 1, "1920x1080", 100, ("pad:rb", "pad:rb", "pad:down", "pad:a")),
    # (Ctrl with the wheel over HORIZONTAL SENSITIVITY: 1.01 (the pointer first: the row's middle, in the menus' 640 x
    # 480); MCC's ZOOMED SENSITIVITY SCALE (MOUSE STYLE right, down twice) with Shift: 1.01 too)
    ("mouse-wheel", 35, CONTROLS, 1, "1920x1080", 100, ("pad:rb", "pad:rb", "pad:down", "move:338:158",
                                                        "wheel:1+ctrl", "wait")),
    ("mouse-mcc", 35, CONTROLS, 1, "1920x1080", 100, ("pad:rb", "pad:rb", "pad:right", "pad:down", "pad:down",
                                                      "key:Right+shift")),
)


def play(cfg, out, name, screen, group, players, presses, window, scale):
    env = {"HALO_ARENA_MENUS_SCALE": str(scale), "HALO_AE_TEST_SETTINGS_GROUP": str(group),
           "HALO_AE_TEST_SETTINGS_PLAYERS": str(players)}
    if name.endswith("-fresh"):
        env["HALO_AE_TEST_SETTINGS_NO_ACTIVE"] = "1"
    # (a long run of presses gets the time it needs: the lead-in, one a second, a few seconds after)
    exit_after = max(EXIT_AFTER, menus.LEAD_IN + len(presses) + 6)
    return menus.play(cfg, out, name, views=screen, window=window, env=env, shots=8, exit_after=exit_after,
                      test_input=menus.menu_input(*presses) if presses else menus.menu_input("wait"))


def lines(text, start):
    return [line.split(start, 1)[1] for line in text.splitlines() if start in line]


def run_cases(cfg, out, only=()):
    """the cases (only those named, when names are given)"""
    runs = {}
    for name, screen, group, players, window, scale, presses in CASES:
        if only and name not in only:
            continue
        result, text, pngs = play(cfg, out, name, screen, group, players, presses, window, scale)
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
    return run_cases(cfg, menus.out_dir(cfg, "settings"))


def test_screens_open(runs):
    for name, screen, group, *_ in CASES:
        _, text, frame = runs[name]
        assert frame, name
        if screen == 34:
            assert lines(text, "ae settings: open player "), name
        else:
            opened = lines(text, "ae settings: group ")
            assert opened and opened[0].startswith(GROUP_NAMES[group]), (name, opened)


def test_profiles_seeded(runs):
    """the test's profile gets its ae.toml on first use"""
    _, text, _ = runs["controls"]
    assert lines(text, "ae player settings: player 1 ") and any("seeded" in line for line in
                                                                lines(text, "ae player settings: player 1 "))


def test_a_step_is_in_use_and_saved(runs):
    _, text, _ = runs["changed"]
    said = lines(text, "ae settings: set ")
    assert "BUTTON LAYOUT SWAP TRIGGERS" in said and "LOOK SENSITIVITY 4" in said, said
    assert "ae profiles: controls 1 " in text
    assert lines(text, "ae settings: closed CONTROLS ")[-1:] != ["(0 changed)"]


def test_a_step_without_a_chosen_profile(runs):
    """right after a launch (no profile chosen): player 1 takes one, and CONTROLS' profile rows change and save"""
    _, text, _ = runs["changed-fresh"]
    assert "ae profiles: player 1 had no profile: " in text
    said = lines(text, "ae settings: set ")
    assert "BUTTON LAYOUT SWAP TRIGGERS" in said and "LOOK SENSITIVITY 4" in said, said
    assert " not set" not in "\n".join(lines(text, "ae settings: "))
    assert "ae profiles: controls 1 " in text


def test_a_players_own_change_is_written(runs):
    """a VIEW row's change is the profile's: its ae.toml written with it (with no profile chosen too)"""
    for name in ("view-saved", "view-saved-fresh"):
        _, text, _ = runs[name]
        said = lines(text, "ae player settings: player 1 ")
        assert any(line.startswith("set display.fov ") for line in said), (name, said)
        written = [line for line in said if " written (" in line]
        assert written and not written[-1].endswith("(0 values)"), (name, said)
        assert "could not be written" not in text, name
    assert "ae profiles: player 1 had no profile: " in runs["view-saved-fresh"][1]


def test_keep_and_undo(runs):
    assert lines(runs["keep"][1], "ae settings: keep") and not lines(runs["keep"][1], "ae settings: undo")
    undo = lines(runs["undo"][1], "ae settings: undo ")
    assert undo == ["(1 rows)"], undo
    # (no change: B goes back, no dialog)
    back = runs["back"][1]
    assert lines(back, "ae settings: closed VIEW ") == ["(0 changed)"] and "Keep your changes?" not in back


def test_restart_asked_after_keep(runs):
    """a RESTART row (MENUS) changed and kept: SETTINGS stays open on its top level, and leaving it asks RESTART NOW /
    LATER (0.3.7.1: the owner saw SETTINGS close after KEEP CHANGES; not reproduced here, this pins the flow)"""
    for name in ("restart", "restart-mouse"):
        text = runs[name][1]
        rest = text.split("ae settings: keep", 1)
        assert len(rest) == 2, name
        said = lines(rest[1], "ae settings: ")
        assert said[:1] == ["closed SYSTEM (1 changed)"] and "restart asked" in said, (name, said)
        assert "closed SETTINGS" not in said and "restart later" not in said, (name, said)


def test_restore_defaults(runs):
    said = lines(runs["defaults"][1], "ae settings: defaults ")
    assert said == ["(2 rows)"], said
    assert not lines(runs["defaults-dialog"][1], "ae settings: defaults ")


def test_sections_and_picker(runs):
    assert lines(runs["sections"][1], "ae settings: section ")[-1:] == ["SCOREBOARD"]
    assert lines(runs["picker"][1], "ae settings: focus ")[:1] == ["FOV"]


def test_the_second_players_own(runs):
    text = runs["seat"][1]
    assert "player 2" in lines(text, "ae settings: ")
    own = lines(text, "ae player settings: player 2 set ")
    assert own == ["input.left_stick_deadzone 5"], own
    assert not lines(text, "ae player settings: player 1 set ")
    assert "player 2" in lines(runs["top-seat"][1], "ae settings: ")
    # (X with a change asks first: KEEP, then the next player)
    said = lines(runs["seat-keep"][1], "ae settings: ")
    assert "keep" in said and "player 2" in said and said.index("keep") < said.index("player 2"), said


def test_video(runs):
    said = lines(runs["video-changed"][1], "ae settings: ")
    assert "set PERFORMANCE FPS" in said, said
    assert lines(runs["video-undo"][1], "ae settings: undo ") == ["(1 rows)"]
    # (a display row: asked with its countdown; B is UNDO, the old mode back)
    assert lines(runs["video-display"][1], "ae settings: display asked ")
    said = lines(runs["video-display-undo"][1], "ae settings: ")
    assert any(line.startswith("display undo (DISPLAY MODE ") for line in said), said


def test_audio(runs):
    said = lines(runs["audio-keep"][1], "ae settings: ")
    assert "set MASTER VOLUME 9" in said and "keep" in said and "closed AUDIO (1 changed)" in said, said
    assert lines(runs["audio-defaults"][1], "ae settings: defaults ") == ["(1 rows)"]
    # (SOUND, a RESTART row: written to config.toml, so the harness's config shows it)
    assert "set SOUND OFF" in lines(runs["audio-sound"][1], "ae settings: ")


def test_online_sharing(runs):
    assert "set INTERNET SHARING ON" in lines(runs["online-sharing"][1], "ae settings: ")


def test_system_rows(runs):
    said = lines(runs["system-files"][1], "ae settings: ")
    assert "set WEAPON PACKS OFF" in said and any(line.startswith("set MCC FILES") for line in said), said
    said = lines(runs["system-updates"][1], "ae settings: ")
    # (the harness's config starts it OFF: a press turns it the other way)
    assert any(line.startswith("set AUTO UPDATE ") for line in said), said
    said = lines(runs["system-campaign"][1], "ae settings: ")
    assert "set FALL DAMAGE OFF" in said and "set HEALTH REACH" in said, said


def test_online(runs):
    said = lines(runs["online-changed"][1], "ae settings: ")
    assert "set DOWNLOAD MAPS OFF" in said, said
    said = lines(runs["online-undo"][1], "ae settings: ")
    assert "undo (1 rows)" in said and "closed ONLINE (0 changed)" in said, said


def test_accessibility(runs):
    said = lines(runs["accessibility-size"][1], "ae settings: ")
    assert "set MENU SIZE 115 %" in said, said
    assert "ae menus: settings: scale 1.15" in runs["accessibility-size"][1]
    said = lines(runs["accessibility-link"][1], "ae settings: ")
    assert "link to HUD PLAYER NAME SIZE" in said and any(line.startswith("group HUD ") for line in said), said
    text = runs["language-restart"][1]
    rest = text.split("ae settings: keep", 1)
    assert len(rest) == 2
    said = lines(rest[1], "ae settings: ")
    assert said[:1] == ["closed ACCESSIBILITY (1 changed)"] and "restart asked" in said, said


def test_chat_and_streaming(runs):
    said = lines(runs["chat-streamer"][1], "ae settings: ")
    assert "set STREAMER MODE ON" in said, said
    assert lines(runs["chat-defaults"][1], "ae settings: defaults ") == ["(1 rows)"]


def test_frame_rate_typed(runs):
    """(0.7.0) FRAME RATE LIMIT: typed like the mouse rows, 30 to 1000; the list's neighbours by left and right; ALL
    VALUES with the typed value and TYPE A NUMBER..."""
    said = lines(runs["fps-typed"][1], "ae settings: ")
    assert "typed FRAME RATE LIMIT '237'" in said and "set FRAME RATE LIMIT 237" in said, said
    for name in ("fps-typing", "fps-typing-130"):
        assert any(line.startswith("typing FRAME RATE LIMIT from ") for line in lines(runs[name][1], "ae settings: ")), name
    said = lines(runs["fps-step"][1], "ae settings: ")
    assert [line for line in said if line.startswith("set ")] == ["set FRAME RATE LIMIT 237", "set FRAME RATE LIMIT 200",
                                                                 "set FRAME RATE LIMIT 240"], said
    assert "set FRAME RATE LIMIT 30" in lines(runs["fps-low"][1], "ae settings: ")
    assert "set FRAME RATE LIMIT 1000" in lines(runs["fps-high"][1], "ae settings: ")
    said = lines(runs["fps-cancel"][1], "ae settings: ")
    assert "typing dropped" in said and not [line for line in said if line.startswith("set ")], said
    said = lines(runs["fps-keyboard"][1], "ae settings: ")
    assert "typing FRAME RATE LIMIT with the keyboard" in said, said


def test_mouse_number_rows(runs):
    """(0.3.9) MOUSE's sensitivities to 0.01: Shift and Ctrl + wheel step 0.01, the arrows alone the next step; Enter
    types a value (kept to 0.01 and written to the profile's ae.toml, held to the range, Esc drops it); a pad's A still
    lists ALL VALUES"""
    said = lines(runs["mouse-fine"][1], "ae settings: ")
    assert said[-2:] == ["set HORIZONTAL SENSITIVITY 1.01", "set HORIZONTAL SENSITIVITY 1.25"], said
    text = runs["mouse-typed"][1]
    said = lines(text, "ae settings: ")
    assert "typed HORIZONTAL SENSITIVITY '1.37'" in said and "set HORIZONTAL SENSITIVITY 1.37" in said, said
    own = lines(text, "ae player settings: player 1 ")
    assert "set input.mouse_sensitivity 1.37" in own and any(" written (" in line for line in own), own
    for name in ("mouse-typing", "mouse-typing-130"):
        assert "typing HORIZONTAL SENSITIVITY from '1.0'" in lines(runs[name][1], "ae settings: "), name
    said = lines(runs["mouse-cancel"][1], "ae settings: ")
    assert "typing dropped" in said and not [line for line in said if line.startswith("set ")], said
    assert "set HORIZONTAL SENSITIVITY 4.0" in lines(runs["mouse-clamp"][1], "ae settings: ")
    said = lines(runs["mouse-pad-a"][1], "ae settings: ")
    assert not [line for line in said if line.startswith("typing")], said
    assert "set HORIZONTAL SENSITIVITY 1.01" in lines(runs["mouse-wheel"][1], "ae settings: ")
    assert "set ZOOMED SENSITIVITY SCALE 1.01" in lines(runs["mouse-mcc"][1], "ae settings: ")


if __name__ == "__main__":
    c = menus.config()
    if not c:
        sys.exit("needs the ae_test harness config and a built game ($AE_MENUS_BUILD)")
    folder = menus.out_dir(c, "settings")
    done = run_cases(c, folder, tuple(sys.argv[1:]))
    frames = [done[n][2] for n, *_ in CASES if n in done and done[n][2]]
    if frames:
        print(menus.tile(frames, folder / "sheet.png", width=960, columns=2))
    for n, (result, text, frame) in done.items():
        print(n, frame, lines(text, "ae settings: ")[-4:])
