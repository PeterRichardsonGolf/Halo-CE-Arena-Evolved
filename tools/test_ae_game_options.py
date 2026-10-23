"""The game setup spike's GAME OPTIONS screen (debug.ae_test_screen 30) in real games, headless, through the ae_test
harness (tools/ae_test), in tools/test_ae_gallery.py's pattern (tools/test_ae_menus.py's helpers).

- AE TEAM SLY at 1080p 100 % and 130 %, AE CTF and AE FFA BALL (their mode groups), 4:3;
- a value stepped (SHIELD TYPE): the changed row, the rail's dot, the crumb's count;
- the picker open (SENSOR RANGE) and the DEFAULTS dialog;
- the table agrees with the model (no "the model has" line) and the log's focus / group / set lines.

Runs only when asked ($AE_MENUS_BUILD and the harness's config); each game is about 30 s:

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_game_options.py
    AE_MENUS_BUILD=$PWD/build/linux64 python3 tools/test_ae_game_options.py      (the frames and a sheet alone)
"""
import importlib.util
import shutil
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_options_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

EXIT_AFTER = 30
SCREEN = 30
# EQUIPMENT (RB three times), down to WEAPON PACKS (after WEAPON SET, MAP WEAPONS, GRENADES ON MAP), A
PACKS_OPEN = ("pad:rb",) * 3 + ("pad:down",) * 3 + ("pad:a",)
# name, gametype, window, UI SCALE, the presses (after the lead-in: one a second)
CASES = (
    ("slay-match", "AE TEAM SLY", "1920x1080", 100, ()),
    ("slay-match-130", "AE TEAM SLY", "1920x1080", 130, ()),
    # (PLAYERS, down to SHIELD TYPE, one step right: NONE, changed)
    ("slay-changed", "AE TEAM SLY", "1920x1080", 100, ("pad:rb", "pad:down", "pad:down", "pad:down", "pad:down",
                                                       "pad:right", "pad:down")),
    # (PLAYERS, down to SENSOR RANGE, A: its picker)
    ("slay-picker", "AE TEAM SLY", "1920x1080", 100, ("pad:rb",) + ("pad:down",) * 8 + ("pad:a",)),
    # (a change, then Y: the DEFAULTS dialog)
    ("slay-defaults", "AE TEAM SLY", "1920x1080", 100, ("pad:right", "pad:y")),
    # (a change, then B: the leave dialog; an AE gametype: no SAVE over it, only SAVE AS COPY)
    ("slay-leave", "AE TEAM SLY", "1920x1080", 100, ("pad:right", "pad:b")),
    # (four groups on: VEHICLES, then the mode's)
    ("ctf-mode", "AE CTF", "1920x1080", 100, ("pad:rb",) * 5),
    ("ball-mode", "AE FFA BALL", "1920x1080", 100, ("pad:rb",) * 5),
    ("slay-4x3", "AE TEAM SLY", "1440x1080", 100, ("pad:rb",)),
    # (VEHICLES, down to VEHICLES DESTRUCTIBLE, the row after VEHICLE RESPAWN TIME: its help)
    ("vehicles", "AE TEAM SLY", "1920x1080", 100, ("pad:rb",) * 4 + ("pad:down",)),
    ("vehicles-130", "AE TEAM SLY", "1920x1080", 130, ("pad:rb",) * 4 + ("pad:down",)),
    # (EQUIPMENT of a team game, three groups on: the top of the group, then its foot with every new row)
    ("equip-top", "AE TEAM SLY", "1920x1080", 100, ("pad:rb",) * 3),
    ("equip-foot", "AE TEAM SLY", "1920x1080", 100, ("pad:rb",) * 3 + ("pad:down",) * 17),
    ("equip-top-130", "AE TEAM SLY", "1920x1080", 130, ("pad:rb",) * 3),
    ("equip-foot-130", "AE TEAM SLY", "1920x1080", 130, ("pad:rb",) * 3 + ("pad:down",) * 17),
    # (MATCH, down to NAME, A: AE's keyboard on a pad)
    ("name-keyboard", "AE TEAM SLY", "1920x1080", 100, ("pad:down",) * 3 + ("pad:a",)),
    # (EQUIPMENT > WEAPON PACKS: the row is the fourth of EQUIPMENT; A opens the sub-screen)
    ("packs-open", "AE TEAM SLY", "1920x1080", 100, PACKS_OPEN),
    ("packs-open-130", "AE TEAM SLY", "1920x1080", 130, PACKS_OPEN),
    # (the SMG and the carbine on (the test hook: the harness's data has the BR's pack only), then A: the BR on too;
    # down to the carbine, now the third pack on: MAP ONLY, its two-line help)
    ("packs-third", "AE TEAM SLY", "1920x1080", 100, PACKS_OPEN + ("pad:a",) + ("pad:down",) * 3),
    ("packs-third-130", "AE TEAM SLY", "1920x1080", 130, PACKS_OPEN + ("pad:a",) + ("pad:down",) * 3),
    # (a pack not installed here: focused, A does nothing)
    ("packs-not-installed", "AE TEAM SLY", "1920x1080", 100, PACKS_OPEN + ("pad:down", "pad:a")),
    # (the SMG on (hook), A: the BR on; down to PRIMARY WEAPON, A: its values, the stock ten, the BR and the SMG; then
    # B, B: back to EQUIPMENT, the sub-screen closed with both on)
    ("packs-primary", "AE TEAM SLY", "1920x1080", 100, PACKS_OPEN + ("pad:a",) + ("pad:down",) * 6 + ("pad:a",)),
    ("packs-back", "AE TEAM SLY", "1920x1080", 100, PACKS_OPEN + ("pad:a",) + ("pad:down",) * 6 +
     ("pad:a", "pad:b", "pad:b")),
    # (the BR on, then Y: DEFAULTS of this screen only, confirmed)
    ("packs-defaults", "AE TEAM SLY", "1920x1080", 100, PACKS_OPEN + ("pad:a", "pad:y", "pad:down", "pad:a")),
)
# (the environment a case adds: the loadout CUSTOM for the start weapons' list)
CASE_ENV = {"packs-third": {"HALO_AE_TEST_SET": "SMG PACK=1;CARBINE PACK=1"},
            "packs-third-130": {"HALO_AE_TEST_SET": "SMG PACK=1;CARBINE PACK=1"},
            "packs-primary": {"HALO_AE_TEST_SET": "LOADOUT=1;SMG PACK=1"},
            "packs-back": {"HALO_AE_TEST_SET": "LOADOUT=1;SMG PACK=1"}}

# the rows set by value index, the copy renamed, saved as new and read back from the file (the tests' hooks in
# ae_screen_game_options.c; the harness types no text): name, SET, NAME, what the file must hold
SAVES = (
    ("save-packs", "BATTLE RIFLE PACK=1;SMG PACK=1;PACK WEAPONS ON MAP=1;STARTING GRENADES=3;BOTTOMLESS CLIP=1;"
     "LOADOUT=1;PRIMARY WEAPON=10;SECONDARY WEAPON=11", "GORS TEST",
     {"BATTLE RIFLE PACK": 1, "SMG PACK": 1, "SILENCED SMG PACK": 0, "CARBINE PACK": 0, "SILENCED AR PACK": 0,
      "PACK WEAPONS ON MAP": 1, "STARTING GRENADES": 3, "BOTTOMLESS CLIP": 1, "PRIMARY WEAPON": 10,
      "SECONDARY WEAPON": 11, "LOADOUT": 1}),
    # (the BR joins a game whose loadout names the SMG and the carbine: the carbine is the third pack, no source
    # reaches it, the secondary goes back to the pistol the carbine plays as; the SMG, still second, stays)
    ("save-remap", "SMG PACK=1;CARBINE PACK=1;LOADOUT=1;PRIMARY WEAPON=11;SECONDARY WEAPON=13;BATTLE RIFLE PACK=1",
     "GORS REMAP", {"PRIMARY WEAPON": 11, "SECONDARY WEAPON": 3, "CARBINE PACK": 1, "BATTLE RIFLE PACK": 1}),
    # (a pack turned off: the slot that took its weapon plays the stock one)
    ("save-pack-off", "SMG PACK=1;LOADOUT=1;PRIMARY WEAPON=11;SMG PACK=0", "GORS OFF",
     {"SMG PACK": 0, "PRIMARY WEAPON": 2}),
    # (NAME untouched: the old name and " 2", as before)
    ("save-default-name", "", None, {}),
    # (the old name typed in as NAME is no rename: the same number)
    ("save-stock-values", "", "AE TEAM SLY", {}),
)


def play(cfg, out, name, gametype, presses, window, scale):
    env = dict({"HALO_ARENA_MENUS_SCALE": str(scale), "HALO_AE_TEST_GAMETYPE": gametype}, **CASE_ENV.get(name, {}))
    return menus.play(cfg, out, name, views=SCREEN, window=window, env=env, shots=8, exit_after=EXIT_AFTER,
                      test_input=menus.menu_input(*presses) if presses else menus.menu_input("wait"))


def lines(text, start):
    return [line.split(start, 1)[1] for line in text.splitlines() if start in line]


def play_save(cfg, out, name, sets, rename):
    result = _play_save(cfg, out, name, sets, rename)
    shutil.rmtree(out / name / "shots", ignore_errors=True)
    return result


def _play_save(cfg, out, name, sets, rename):
    env = {"HALO_AE_TEST_GAMETYPE": "AE TEAM SLY", "HALO_AE_TEST_SAVE": "1"}
    if sets:
        env["HALO_AE_TEST_SET"] = sets
    if rename:
        env["HALO_AE_TEST_NAME"] = rename
    return menus.play(cfg, out, name, views=SCREEN, window="1920x1080", env=env, shots=1, exit_after=EXIT_AFTER,
                      test_input=menus.menu_input("wait"))


def values(text):
    """the logged value lines as {label: index}"""
    found = {}
    for line in lines(text, "ae game options: value "):
        label, _, index = line.rpartition("=")
        found[label] = int(index)
    return found


def run_cases(cfg, out, only=()):
    """the cases (only those named, when names are given)"""
    runs = {}
    for name, gametype, window, scale, presses in CASES:
        if only and name not in only:
            continue
        result, text, pngs = play(cfg, out, name, gametype, presses, window, scale)
        frame = None
        if pngs:
            frame = out / f"{name}.png"
            frame.write_bytes(pngs[-1].read_bytes())
        # (the box's disk is shared: the frames' copies of a run go once the last one is kept)
        shutil.rmtree(out / name / "shots", ignore_errors=True)
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
    return run_cases(cfg, menus.out_dir(cfg, "game-options"))


@pytest.fixture(scope="module")
def saves(cfg):
    out = menus.out_dir(cfg, "game-options-saves")
    return {name: (play_save(cfg, out, name, sets, rename)[1], rename, expected)
            for name, sets, rename, expected in SAVES}


def test_opens_on_the_gametype(runs):
    for name, gametype, *_ in CASES:
        _, text, frame = runs[name]
        opened = lines(text, "ae game options: open ")
        assert opened and opened[0].startswith(f"'{gametype}'"), (name, opened)
        assert not lines(text, "the model has"), name
        assert frame, name


def test_a_step_changes_the_copy(runs):
    _, text, _ = runs["slay-changed"]
    assert "PLAYERS" in lines(text, "ae game options: group ")
    assert "SHIELD TYPE NONE" in lines(text, "ae game options: set ")
    assert lines(text, "ae game options: closed ")[-1:] != ["(0 changed)"]


def test_leaving_with_changes_asks(runs):
    # (B with a change asks first; an AE gametype is never saved over: SAVE is not offered)
    _, text, _ = runs["slay-leave"]
    assert lines(text, "ae game options: leave: asked ") == ["(save over: no)"], lines(text, "ae game options: leave")
    assert not lines(text, "ae game options: closed ")


def test_vehicles_destructible_row(runs):
    # (VEHICLES' second row, after VEHICLE RESPAWN TIME, at 100 % and 130 %)
    for name in ("vehicles", "vehicles-130"):
        _, text, _ = runs[name]
        assert "VEHICLES" in lines(text, "ae game options: group "), name
        assert "VEHICLES DESTRUCTIBLE" in lines(text, "ae game options: focus "), (name, lines(text, "ae game options: focus "))


def test_the_mode_groups(runs):
    assert "CAPTURE THE FLAG" in lines(runs["ctf-mode"][1], "ae game options: group ")
    assert "ODDBALL" in lines(runs["ball-mode"][1], "ae game options: group ")


@pytest.mark.parametrize("name", [save[0] for save in SAVES])
def test_new_rows_survive_save_as_new(saves, name):
    text, rename, expected = saves[name]
    assert not lines(text, "the model has"), name
    assert not lines(text, "test: save failed"), name
    saved = lines(text, "ae game options: saved as new ")
    assert saved, name
    # a typed name is kept as typed when free, else (and with none typed) the old name, cut to 11 characters with the number
    if rename and rename != "AE TEAM SLY":
        assert saved[0].startswith(f"'{rename}'"), saved
    else:
        assert saved[0].startswith("'AE TEAM S 2'"), saved
    assert any(line.startswith("'") for line in lines(text, "ae game options: test: reopened ")), name
    got = values(text)
    for label, index in expected.items():
        assert got.get(label) == index, (name, label, got.get(label), index)


def test_weapon_packs_opens(runs):
    # (EQUIPMENT's WEAPON PACKS row opens the sub-screen on its first pack; nothing on in AE TEAM SLY)
    for name in ("packs-open", "packs-open-130"):
        _, text, _ = runs[name]
        assert "WEAPON PACKS" in lines(text, "ae game options: focus "), name
        assert lines(text, "ae game options: weapon packs open ") == ["(10 rows, on: NONE)"], name
        assert lines(text, "ae game options: packs focus ")[:1] == ["BATTLE RIFLE PACK"], name


def test_weapon_packs_third_pack(runs):
    for name in ("packs-third", "packs-third-130"):
        _, text, _ = runs[name]
        sets = lines(text, "ae game options: set ")
        assert sets == ["SMG PACK ON", "CARBINE PACK ON", "BATTLE RIFLE PACK ON"], (name, sets)
        assert lines(text, "ae game options: packs focus ")[-1] == "CARBINE PACK", name


def test_weapon_packs_not_installed_does_not_turn_on(runs):
    _, text, _ = runs["packs-not-installed"]
    assert lines(text, "ae game options: packs focus ")[-1] == "SMG PACK"
    assert not lines(text, "ae game options: set ")


def test_weapon_packs_primary_lists_only_packs_that_can_start(runs):
    _, text, _ = runs["packs-primary"]
    assert "BATTLE RIFLE PACK ON" in lines(text, "ae game options: set ")
    offered = lines(text, "ae game options: offered PRIMARY WEAPON ")
    assert offered, lines(text, "ae game options: ")[-8:]
    names = offered[-1].split(",")
    assert names[:10] == ["NONE", "RANDOM", "ASSAULT RIFLE", "PISTOL", "SHOTGUN", "SNIPER RIFLE", "ROCKET LAUNCHER",
                          "PLASMA PISTOL", "PLASMA RIFLE", "NEEDLER"], names
    assert names[10:] == ["BATTLE RIFLE", "SMG"], names


def test_weapon_packs_back_to_equipment(runs):
    # (B closes the picker, B the sub-screen: back on EQUIPMENT, GAME OPTIONS still open)
    _, text, _ = runs["packs-back"]
    assert lines(text, "ae game options: weapon packs closed ") == ["(on: 2 ON)"]
    assert not lines(text, "ae game options: closed ")


def test_weapon_packs_defaults_this_screen_only(runs):
    _, text, _ = runs["packs-defaults"]
    assert "BATTLE RIFLE PACK ON" in lines(text, "ae game options: set ")
    assert lines(text, "ae game options: defaults ") == ["(weapon packs)"]
    assert not [line for line in text.splitlines() if line.rstrip().endswith("ae game options: defaults")]


def test_the_pack_rows_and_name_are_in_the_table(runs):
    # (every new row has a glue encoding of the model's value count)
    for name, *_ in CASES:
        assert not lines(runs[name][1], "the model has"), name


if __name__ == "__main__":
    c = menus.config()
    if not c:
        sys.exit("needs the ae_test harness config and a built game ($AE_MENUS_BUILD)")
    folder = menus.out_dir(c, "game-options")
    done = run_cases(c, folder, tuple(sys.argv[1:]))
    frames = [done[n][2] for n, *_ in CASES if n in done and done[n][2]]
    if frames:
        print(menus.tile(frames, folder / "sheet.png", width=960, columns=2))
    for n, (result, text, frame) in done.items():
        print(n, frame, lines(text, "ae game options: ")[-3:])
