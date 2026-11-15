"""The game setup spike's CHANGE MAP screen (debug.ae_test_screen 31) in real games, headless, through the ae_test
harness (tools/ae_test), in tools/test_ae_gallery.py's pattern (tools/test_ae_menus.py's helpers).

- the folders at 1080p 100 % and 130 %, the folder of the map used last open, the groups by source (STOCK OG ...);
- the AE CTF filter (set at the start); LB / RB from filter to filter (LT / RT, the folders, checked live); left
  closing the open one;
- (AE menus Phase 2) X's folder menu: A puts the map in FAVOURITES (saved: map-folders.txt); NEW FOLDER... with AE's
  keyboard, START makes it; the search (Y; $HALO_AE_TEST_MAP_SEARCH: results, none, B clearing it; owner 2026-10-28: Y
  sets the filter to ALL first); a save root with RECENT's games and two folders of the player's: RECENT (the picker's
  own, owner 2026-10-28: the maps played, each once; A sets only the map), the folder menu's ticks, RENAME / DELETE and
  its question, an empty folder's line (the mockups' cases A, C, E, E2, F, H);
- the NHE disc's folder and the NHE filter;
- the facts read (the log's "facts N read" line) and no picture drawn where the data has none (the dashed place);
- (0.6.0, only when $AE_MCC_STEAM names a Steam folder whose steamapps holds MCC, with halo1/maps/bloodgulch.map,
  and the Workshop item 2899342285, Better Creek Pro) MCC's maps read in place: STOCK MCC, the WORKSHOP folder
  opening into PRO EDITION (the item's map with its thumbnail and the WORKSHOP badge), and a Pro Edition map Steam is
  still writing (a second item, its map cut short) listed dimmed, "STEAM IS UPDATING IT", A refused.

Runs only when asked ($AE_MENUS_BUILD and the harness's config); each game is about 30 s:

    AE_MENUS_BUILD=$PWD/build/linux64 python3 -m pytest -q tools/test_ae_change_map.py
    AE_MENUS_BUILD=$PWD/build/linux64 python3 tools/test_ae_change_map.py      (the frames alone)
"""
import importlib.util
import re
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("ae_change_map_menus", HERE / "test_ae_menus.py")
menus = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(menus)

EXIT_AFTER = 30
SCREEN = 31
# name, window, UI SCALE, the presses (after the lead-in: one a second), more environment
CASES = (
    ("open", "1920x1080", 100, (), {}),
    ("open-130", "1920x1080", 130, (), {}),
    # (the filter set at the start: a hidden window has no focus, so AE takes no keys, and the test pad has no
    # triggers; LT / RT, the folders, are checked live)
    ("filter-ctf", "1920x1080", 100, ("pad:down",), {"HALO_AE_TEST_MAP_FILTER": "5"}),
    ("filters", "1920x1080", 100, ("pad:rb",) * 3, {}),
    ("closed", "1920x1080", 100, ("pad:left",), {}),
    # (X on a map: the folder menu, A puts it in FAVOURITES, B; then up to FAVOURITES' row)
    ("favourite", "1920x1080", 100, ("pad:down", "pad:x", "pad:a", "pad:b", "pad:up", "pad:up"), {}),
    # (NEW FOLDER... on a map: down to it, A (AE's keyboard), START makes "New Folder" with the map in it)
    ("new-folder", "1920x1080", 100, ("pad:down", "pad:x", "pad:down", "pad:a", "wait", "pad:start", "wait"), {}),
    ("search", "1920x1080", 100, ("wait",), {"HALO_AE_TEST_MAP_SEARCH": "cr"}),
    ("search-empty", "1920x1080", 100, ("wait",), {"HALO_AE_TEST_MAP_SEARCH": "qzx"}),
    ("search-clear", "1920x1080", 100, ("pad:b",), {"HALO_AE_TEST_MAP_SEARCH": "cr"}),
    ("keyboard", "1920x1080", 100, ("pad:y",), {}),
    # (Y on the AE CTF filter: the search covers every map, the filter ALL)
    ("search-filter", "1920x1080", 100, ("pad:y",), {"HALO_AE_TEST_MAP_FILTER": "5"}),
    ("nhe", "1920x1080", 100, (), {"HALO_MOD": "NHE", "HALO_AE_TEST_MAP_FILTER": "6"}),
    # (0.7.6: from the top, STOCK OG open: A closes it; down to AE MAPS, A opens it: its sections; then down twice to
    # COMMUNITY (AE MAPS closed again by A), A: its sections, the first header's fold with A and RB-less LT / RT's steps)
    ("ae-maps", "1920x1080", 100, ("pad:up",) * 3 + ("pad:down", "pad:a", "pad:down", "pad:a"), {}),
    ("community", "1920x1080", 100, ("pad:up",) * 3 + ("pad:down", "pad:a") + ("pad:down",) * 2 + ("pad:a",), {}),
    ("community-fold", "1920x1080", 100, ("pad:up",) * 3 + ("pad:down", "pad:a") + ("pad:down",) * 2 + ("pad:a", "pad:down",
                                                                                                         "pad:a"), {}),
)
# (a save root with RECENT's games and the player's folders: up to the list's top, then down to the row each case
# shows. Rows: RECENT, its 3 maps (4 games: Decidia twice, listed once), FAVOURITES, Comp practice, Infection maps,
# STOCK OG ...)
TOP = ("pad:up",) * 10
# (their presses take longer than the others')
SEEDED_EXIT_AFTER = 45
SEEDED = (
    # (A: RECENT's first map focused; A on it sets only the map)
    ("seeded-top", "1920x1080", 100, TOP + ("pad:down",), {}),
    ("seeded-top-130", "1920x1080", 130, TOP + ("pad:down",), {}),
    ("seeded-pick", "1920x1080", 100, TOP + ("pad:down", "pad:a"), {}),
    # (C: the folder menu on RECENT's Blood Gulch: FAVOURITES and Infection maps ticked)
    ("seeded-menu", "1920x1080", 100, TOP + ("pad:down", "pad:down", "pad:x", "pad:down", "pad:down"), {}),
    # (F: its NEW FOLDER..., A: the name field and AE's keyboard beside it)
    ("seeded-new", "1920x1080", 100, TOP + ("pad:down", "pad:down", "pad:x") + ("pad:down",) * 3 + ("pad:a", "wait"),
     {}),
    # (E: X on Infection maps: RENAME / DELETE; E2: DELETE's question)
    ("seeded-rename", "1920x1080", 100, TOP + ("pad:down",) * 6 + ("pad:x",), {}),
    ("seeded-delete", "1920x1080", 100, TOP + ("pad:down",) * 6 + ("pad:x", "pad:down", "pad:a"), {}),
    # (H: Comp practice, empty, opened)
    ("seeded-empty", "1920x1080", 100, TOP + ("pad:down",) * 5 + ("pad:a",), {}),
)


def seeded_save(out):
    """a save root (a folder in out: never /tmp) whose private folder has RECENT's games and two folders of the
    player's (Comp practice empty)"""
    save = out / "seeded-save"
    (save / "private").mkdir(parents=True, exist_ok=True)
    (save / "private" / "recent-games.txt").write_text(
        "# AE recent games\ngame = decidia | AE 2V2 SLY\ngame = bloodgulch | AE BTB CTF\n"
        "game = prisoner | AE FFA SLAY\ngame = decidia | AE FFA SLAY\n")
    (save / "private" / "map-folders.txt").write_text(
        "# AE folders (maps)\nfolder = Favourites\nmap = bloodgulch\nmap = decidia\nfolder = Infection maps\n"
        "map = ratrace\nmap = prisoner\nmap = bloodgulch\nmap = hangemhigh\nfolder = Comp practice\n")
    return save


def _png(path, width, height, rgb):
    """a plain RGBA PNG of one colour (the test's own picture: no game file; the menus draw 8-bit RGBA, as the
    Workshop's own thumbnails are)"""
    import struct
    import zlib

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    rows = b"".join(b"\0" + bytes(rgb + (255,)) * width for _ in range(height))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) +
                     chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


MCC_DIR = "Halo The Master Chief Collection"
CREEK = "2899342285"
UPDATING = "2899666869"


def mcc_steam(out):
    """a Steam folder for the MCC cases (in out: never /tmp): $AE_MCC_STEAM's MCC linked; Better Creek Pro's item
    with a thumbnail of the test's; a second Pro Edition item whose map is cut short (Steam still writing it).
    Returns the MCC folder, or None without $AE_MCC_STEAM"""
    import json
    import os

    source = os.environ.get("AE_MCC_STEAM")
    if not source:
        return None
    source = Path(source) / "steamapps"
    steam = out / "mcc-steam" / "steamapps"
    common = steam / "common"
    common.mkdir(parents=True, exist_ok=True)
    if not (common / MCC_DIR).exists():
        (common / MCC_DIR).symlink_to(source / "common" / MCC_DIR)
    content = steam / "workshop" / "content" / "976730"
    creek = content / CREEK
    (creek / "maps").mkdir(parents=True, exist_ok=True)
    (creek / "multiplayer").mkdir(parents=True, exist_ok=True)
    real = source / "workshop" / "content" / "976730" / CREEK
    (creek / "ModInfo.json").write_bytes((real / "ModInfo.json").read_bytes())
    (creek / "multiplayer" / "pat_beavercreek.json").write_bytes((real / "multiplayer" / "pat_beavercreek.json").read_bytes())
    if not (creek / "maps" / "pat_beavercreek.map").exists():
        (creek / "maps" / "pat_beavercreek.map").symlink_to(real / "maps" / "pat_beavercreek.map")
    _png(creek / "images" / "beavercreek_pro_small.png", 668, 292, (64, 140, 120))
    updating = content / UPDATING
    for folder in ("maps", "multiplayer"):
        (updating / folder).mkdir(parents=True, exist_ok=True)
    info = json.loads((real / "ModInfo.json").read_text())
    info["ModIdentifier"]["HostedModIds"]["SteamWorkshopId"] = int(UPDATING)
    info["Title"]["Neutral"] = "Temple Pro"
    info["GameModContents"]["MultiplayerMaps"] = ["multiplayer/pat_wizard.json"]
    (updating / "ModInfo.json").write_text(json.dumps(info))
    (updating / "multiplayer" / "pat_wizard.json").write_text(json.dumps(
        {"ScenarioFile": "levels/test/pat_wizard/pat_wizard", "Title": {"Neutral": "Temple Pro"}}))
    with open(real / "maps" / "pat_beavercreek.map", "rb") as whole:
        (updating / "maps" / "pat_wizard.map").write_bytes(whole.read(1 << 20))
    return common / MCC_DIR


PRO_COLLECTION = "3001370390"


# (a game runs in a network namespace of its own: Steam's web API is a stand-in server there, tools/ae_test/fake_steam_api.py,
# started beside the game on its loopback)
STAND_IN_PORT = 18080
STAND_IN = [sys.executable, str(HERE / "ae_test" / "fake_steam_api.py"), str(STAND_IN_PORT), PRO_COLLECTION,
            "Halo CE - Pro Edition", CREEK, UPDATING]


def mcc_env(out, name="mcc", api=None):
    """the MCC cases' environment; its own save root a case (the collections' cache is kept there)"""
    mcc = mcc_steam(out)
    if not mcc:
        return None
    env = {"HALO_MCC_PATH": str(mcc), "HALO_MCC_USE": "yes", "XDG_DATA_HOME": str(out / f"{name}-xdg")}
    if api:
        env["HALO_AE_WORKSHOP_API"] = api
    return env


# (0.6.0, 0.7.6) the MCC cases: $AE_MCC_STEAM's. From the top (FAVOURITES), STOCK OG open: down to it, A closes it; rows
# FAVOURITES, STOCK OG, STOCK MCC, AE MAPS, COMMUNITY, WORKSHOP: down 4, A opens WORKSHOP, which lists its sections at
# once (PRO EDITION, Steam's collection; its maps under it); down: its header (A folds it), down: Better Creek Pro,
# down: Temple Pro (Steam updating it), A refused
TO_WORKSHOP = ("pad:up",) * 3 + ("pad:down", "pad:a") + ("pad:down",) * 4 + ("pad:a",)
DEAD_API = "http://127.0.0.1:9"
# name, presses, which answer Steam's web API gets: "live" (this test's stand-in), "none" (nothing listens), "cache" (a
# cache a day young, nothing listens), "kept" (the cache and a fold the player kept)
MCC_CASES = (
    ("mcc-workshop", TO_WORKSHOP, "live"),
    ("mcc-section", TO_WORKSHOP + ("pad:down",), "live"),
    ("mcc-fold", TO_WORKSHOP + ("pad:down", "pad:a"), "live"),
    ("mcc-pro", TO_WORKSHOP + ("pad:down", "pad:down"), "live"),
    ("mcc-updating", TO_WORKSHOP + ("pad:down", "pad:down", "pad:down", "pad:a"), "live"),
    ("mcc-kept", TO_WORKSHOP + ("pad:down",), "kept"),
    ("mcc-offline", TO_WORKSHOP + ("pad:down",), "none"),
    ("mcc-cached", TO_WORKSHOP + ("pad:down",), "cache"),
)


def cache_save(out, folds=False):
    """a save root whose private folder holds the collections' cache, a day young (the test's own text: Pro Edition, the two
    items of mcc_steam)"""
    import time
    save = out / ("kept-save" if folds else "cache-save")
    (save / "private").mkdir(parents=True, exist_ok=True)
    if folds:
        (save / "private" / "section-folds.txt").write_text("# AE section folds\nmaps\tWORKSHOP/PRO EDITION\n")
    (save / "private" / "workshop-collections.cache").write_text(
        f"# AE Workshop collections, as Steam last answered\nfetched\t{int(time.time())}\n"
        f"collection\t{PRO_COLLECTION}\tPRO EDITION\t{CREEK},{UPDATING}\n")
    return save


def play(cfg, out, name, window, scale, presses, env, save_from=None, helper=None):
    e = {"HALO_ARENA_MENUS_SCALE": str(scale)}
    e.update(env)
    return menus.play(cfg, out, name, views=SCREEN, window=window, env=e, shots=8,
                      exit_after=SEEDED_EXIT_AFTER if save_from else EXIT_AFTER,
                      test_input=menus.menu_input(*presses) if presses else menus.menu_input("wait"),
                      save_from=save_from, helper=helper)


def lines(text, start):
    return [line.split(start, 1)[1] for line in text.splitlines() if start in line]


def run_cases(cfg, out, only=()):
    """the cases (only those named, when names are given)"""
    runs = {}
    for name, window, scale, presses, env in CASES + SEEDED:
        if only and name not in only:
            continue
        seeded = seeded_save(out) if any(name == case[0] for case in SEEDED) else None
        result, text, pngs = play(cfg, out, name, window, scale, presses, env, save_from=seeded)
        frame = None
        if pngs:
            frame = out / f"{name}.png"
            frame.write_bytes(pngs[-1].read_bytes())
        runs[name] = (result, text, frame)
    if mcc_steam(out):
        for name, presses, answer in MCC_CASES:
            if only and name not in only:
                continue
            env = mcc_env(out, name, f"http://127.0.0.1:{STAND_IN_PORT}" if answer == "live" else DEAD_API)
            result, text, pngs = play(cfg, out, name, "1920x1080", 100, presses, env,
                                      save_from=cache_save(out, answer == "kept") if answer in ("cache", "kept") else None,
                                      helper=STAND_IN if answer == "live" else None)
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
    import os
    # ($AE_CHANGE_MAP_ONLY: case names, comma apart, to run only those; the tests of the others are skipped)
    only = tuple(name for name in os.environ.get("AE_CHANGE_MAP_ONLY", "").split(",") if name)
    return run_cases(cfg, menus.out_dir(cfg, "change-map"), only)


def need(runs, *names):
    missing = [name for name in names if name not in runs]
    if missing:
        pytest.skip(f"case(s) not run: {', '.join(missing)}")


def test_opens_with_folders_and_facts(runs):
    need(runs, *(name for name, *_ in CASES))
    for name, *_ in CASES:
        _, text, frame = runs[name]
        assert lines(text, "ae change map: open "), name
        assert lines(text, "ae change map: maps "), name
        assert frame, name
    text = runs["open"][1]
    groups = lines(text, "ae change map: maps ")[0]
    # (no old COMP group: its name only; COMMUNITY COMPETITIVE and a player's "Comp practice" are fine)
    assert "STOCK OG 13" in groups and not re.search(r"\bCOMP \d", groups) and "HALO CE" not in groups
    assert "FAVOURITES 0" in groups and "COMMUNITY COMPETITIVE" in groups and groups.index("STOCK OG") < groups.index("WARM UP")
    assert lines(text, "ae change map: facts "), "the facts are read within the run"


def test_the_filter_and_folders(runs):
    need(runs, "filter-ctf", "filters", "closed")
    assert lines(runs["filter-ctf"][1], "ae change map: open ")[0] == "(filter AE CTF)"
    assert len(lines(runs["filters"][1], "ae change map: filter ")) >= 3
    assert lines(runs["closed"][1], "ae change map: folder ")[-1].endswith(" closed")


def test_the_folder_menu(runs):
    need(runs, "favourite", "new-folder")
    text = runs["favourite"][1]
    assert lines(text, "ae change map: folder menu ")
    assert lines(text, "ae change map: ")  # (the map put in)
    assert [line for line in lines(text, "ae change map: '") if line.endswith(" in Favourites")]
    assert lines(text, "ae pickers: map-folders.txt written ")
    made = runs["new-folder"][1]
    assert lines(made, "ae change map: keyboard open")
    assert lines(made, "ae change map: folder 'New Folder' made")
    assert [line for line in lines(made, "ae change map: '") if line.endswith(" in New Folder")]


def test_search(runs):
    need(runs, "search", "search-clear", "search-empty", "keyboard", "search-filter")
    assert lines(runs["search"][1], "ae change map: open ")[0] == "(filter ALL): search 'cr'"
    cleared = lines(runs["search-clear"][1], "ae change map: search ")
    assert cleared and cleared[-1].startswith("'': ")
    assert lines(runs["search-empty"][1], "ae change map: open ")[0] == "(filter ALL): search 'qzx'"
    assert lines(runs["keyboard"][1], "ae change map: keyboard open")
    # (owner 2026-10-28: Y sets the filter to ALL first, whatever it was)
    assert lines(runs["keyboard"][1], "ae change map: (filter ALL): search opens")
    text = runs["search-filter"][1]
    assert lines(text, "ae change map: open ")[0] == "(filter AE CTF)"
    assert lines(text, "ae change map: (filter ALL): search opens") and lines(text, "ae change map: keyboard open")


def test_seeded_folders_and_recent(runs):
    need(runs, *(case[0] for case in SEEDED))
    text = runs["seeded-top"][1]
    assert lines(text, "ae pickers: recent-games.txt: ")[0] == "4 games"
    assert lines(text, "ae pickers: map-folders.txt: ")[0].startswith("3 folders, 6 entries")
    groups = lines(text, "ae change map: maps ")[0].split(", groups ", 1)[1]
    assert groups.startswith("FAVOURITES 2, Comp practice 0, Infection maps 4, STOCK OG 13")
    assert "'decidia' (recent)" in lines(text, "ae change map: focus ")[-1]
    assert lines(text, "ae change map: recent ")[0] == "3: 'decidia', 'bloodgulch', 'prisoner'"
    # (RECENT per picker: A on a RECENT map sets the map alone, no gametype)
    pick = runs["seeded-pick"][1]
    assert lines(pick, "ae change map: pick recent ")[0] == "'decidia'"
    assert not lines(pick, "ae game options: begin "), "a RECENT map sets no gametype"
    assert lines(runs["seeded-menu"][1], "ae change map: folder menu ")[0] == "'Blood Gulch'"
    assert lines(runs["seeded-rename"][1], "ae change map: folder menu ")[0] == "'Infection maps'"
    assert "folder Comp practice open" in " ".join(lines(runs["seeded-empty"][1], "ae change map: "))


def test_mcc_maps(runs):
    """(0.6.0, 0.7.6) MCC's maps read in place: STOCK MCC, WORKSHOP opening into its sections (Steam's collections), a map
    Steam is updating"""
    need(runs, "mcc-workshop", "mcc-section", "mcc-pro", "mcc-updating")
    text = runs["mcc-workshop"][1]
    # (the stock maps: as many as the Steam copy has)
    assert re.fullmatch(r"\d+ stock and 1 Workshop maps listed, 1 Workshop maps being updated by Steam",
                        lines(text, "mcc maps: ")[0])
    groups = lines(text, "ae change map: maps ")[0]
    assert re.search(r"STOCK MCC \d+", groups) and "WORKSHOP 2" in groups and "PRO EDITION 2" in groups
    assert "folder WORKSHOP open" in " ".join(lines(text, "ae change map: "))
    section = runs["mcc-section"][1]
    assert "focus section PRO EDITION" in " ".join(lines(section, "ae change map: "))
    pro = runs["mcc-pro"][1]
    assert any(line.startswith("'pat_beavercreek@ws2899342285' 668x292") for line in lines(pro, "ae change map: picture "))
    updating = runs["mcc-updating"][1]
    assert "'pat_wizard@ws2899666869' can't be picked: Steam is updating it" in " ".join(lines(updating,
                                                                                              "ae change map: "))
    assert not lines(updating, "ae change map: pick ")


def test_workshop_sections_come_from_steam(runs):
    """(0.7.6) the sections are Steam's own answer, asked on a thread of its own and kept a day: live, from a cache, and with
    nothing at all (every map under OTHER, no error)"""
    need(runs, "mcc-workshop", "mcc-cached", "mcc-offline")
    live = runs["mcc-workshop"][1]
    assert lines(live, "mcc workshop: collections from Steam: ")[0].startswith("1 of the ")
    assert "PRO EDITION 2" in lines(live, "ae change map: maps ")[0]
    cached = runs["mcc-cached"][1]
    assert lines(cached, "mcc workshop: collections: ")[0].startswith("1 in the cache")
    assert "PRO EDITION 2" in lines(cached, "ae change map: maps ")[0]
    assert not lines(cached, "mcc workshop: collections from Steam: ")
    offline = runs["mcc-offline"][1]
    assert "PRO EDITION" not in lines(offline, "ae change map: maps ")[0]
    assert "OTHER 2" in lines(offline, "ae change map: maps ")[0]
    assert lines(offline, "mcc workshop: collections not asked: ")
    assert "focus section OTHER" in " ".join(lines(offline, "ae change map: "))


def test_sections_fold_and_are_kept(runs):
    """(0.7.6) A on a section's header folds it; the choice is written (section-folds.txt) for the next time"""
    need(runs, "mcc-fold", "mcc-kept", "mcc-workshop")
    fold = runs["mcc-fold"][1]
    assert "section PRO EDITION folded" in " ".join(lines(fold, "ae change map: "))
    # (kept: a section-folds.txt of the player's starts it folded, and A on it opens it)
    kept = runs["mcc-kept"][1]
    assert "section WORKSHOP/PRO EDITION starts folded (kept)" in " ".join(lines(kept, "ae change map: "))
    assert not lines(runs["mcc-workshop"][1], "ae change map: section WORKSHOP/PRO EDITION starts folded")


def test_community_and_ae_maps_sections(runs):
    """(0.7.6) AE MAPS and COMMUNITY open into labelled sections that fold; the groups come from what is on disk"""
    need(runs, "ae-maps", "community", "community-fold")
    groups = lines(runs["open"][1], "ae change map: maps ")[0] if "open" in runs else \
        lines(runs["community"][1], "ae change map: maps ")[0]
    assert re.search(r"AE MAPS \d+", groups) and re.search(r"COMMUNITY \d+", groups)
    assert "SEARCH AND DESTROY" in groups and not re.search(r"\bAE SEARCH AND DESTROY\b", groups)
    assert "folder AE MAPS open" in " ".join(lines(runs["ae-maps"][1], "ae change map: "))
    assert "folder COMMUNITY open" in " ".join(lines(runs["community"][1], "ae change map: "))
    folded = lines(runs["community-fold"][1], "ae change map: section ")
    assert folded and folded[-1].endswith(" folded")


def test_the_nhe_disc(runs):
    need(runs, "nhe")
    text = runs["nhe"][1]
    assert "NHE" in lines(text, "ae change map: maps ")[0]
    assert lines(text, "ae change map: open ")[0] == "(filter NHE)"


if __name__ == "__main__":
    c = menus.config()
    if not c:
        sys.exit("needs the ae_test harness config and a built game ($AE_MENUS_BUILD)")
    folder = menus.out_dir(c, "change-map")
    done = run_cases(c, folder, tuple(sys.argv[1:]))
    for n, (result, text, frame) in done.items():
        print(n, frame, lines(text, "ae change map: ")[-4:])
