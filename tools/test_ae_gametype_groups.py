"""The gametype lists' grouping (bug 101, regressed report 2026-10-15): the lists sort by the names table's set and
place (arena_gametypes.c arena_gametypes_sort), so the table is the order. Pinned here with the current seeds (AE,
AE BR, AE COMP, OCTAGON, NHE, TRAINING): in each AE set the free for all gametypes come before the team ones, the
sets keep their order, every seed is in the table (an unknown one sorts after all sets, alphabetically, mixing FFA
and TEAM), AE BR lists the twins in the order of the AE gametypes they copy. The lists themselves sort by category: the
playlist folders (owner 2026-10-22; "by play style, BR inside" before, 2026-10-20: arena_gametype_names.c's
categories table): AE FFA, AE 2V2, AE 4V4, AE BTB, AE OBJECTIVE (AE SND and AE BOMB, owner 2026-10-27), AE ACTION
SACK, AE PRACTICE, then NHE's in their table order; every one of AE's in the table once, each battle rifle twin right
after the gametype it copies."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
NAMES = (ROOT / "source/saved games/arena_gametype_names.c").read_text(encoding="latin1")
SEEDS = (ROOT / "source/saved games/arena_gametypes.c").read_text(encoding="latin1")

ROW = re.compile(r'^\t\{ "([^"]*)", "([^"]*)", _arena_gametype_set_(\w+), \w+, (TRUE|FALSE)', re.M)
ROWS = [(s, d, st, t == "TRUE") for s, d, st, t in ROW.findall(NAMES)]


def test_sets_in_order():
    sets = []
    for _, _, st, _ in ROWS:
        if not sets or sets[-1] != st:
            sets.append(st)
    assert sets == ["ae", "ae_br", "ae_comp", "octagon", "nhe", "training"]


def test_free_for_all_before_teams_in_each_ae_set():
    for st in ("ae", "ae_br", "ae_comp"):
        teams = [t for _, _, s, t in ROWS if s == st]
        assert teams == sorted(teams), f"{st}: a free for all row after a team one"
        assert teams[0] is False and teams[-1] is True


CATEGORY = re.compile(r'\{ _arena_gametype_category_(\w+), "([^"]+)" \}')
CATEGORIES = CATEGORY.findall(NAMES)


def test_categories_in_order():
    order = []
    for category, _ in CATEGORIES:
        if not order or order[-1] != category:
            order.append(category)
    assert order == ["ffa", "2v2", "4v4", "btb", "objective", "action_sack", "practice"]


def test_playlist_folders():
    """the owner's playlist folders (2026-10-22): what each holds"""
    folders = {}
    for category, stored in CATEGORIES:
        folders.setdefault(category, []).append(stored)
    assert folders["ffa"] == ["AE FFA SLAY", "AE BR FFA", "AE FFA KING", "AE BR FKING", "AE FFA BALL", "AE BR FBALL",
                              "AE COMP FFA", "AE BR C FFA", "AE FFA SNPR", "AE FFA ROCK"]
    assert folders["2v2"] == ["AE 2V2 SLY", "AE BR 2V2 S", "AE 2V2 CTF", "AE BR 2V2 C", "AE 2V2 KING", "AE BR 2V2 K",
                              "AE 2V2 BALL", "AE BR 2V2 B", "AE C2V2 TS", "AE BR C2 TS", "AE C2V2 CTF", "AE BR C2CTF",
                              "AE C2V2 KOH", "AE BR C2KOH", "AE C2V2 OB", "AE BR C2 OB"]
    assert folders["4v4"] == ["AE TEAM SLY", "AE BR TS", "AE CTF", "AE BR CTF", "AE KING", "AE BR KING", "AE TEAM OB",
                              "AE BR TM OB", "AE COMP TS", "AE BR C TS", "AE COMP CTF", "AE BR C CTF", "AE COMP KOH",
                              "AE BR C KOH", "AE COMP OB", "AE BR C OB"]
    assert folders["btb"] == ["AE BTB SLY", "AE BR BTB S", "AE BTB CTF", "AE BR BTB C", "AE BTB KING", "AE BR BTB K",
                              "AE BTB BALL", "AE BR BTB B", "AE BTB HVY", "AE BR BTB H"]
    assert folders["objective"] == ["AE SND", "AE BOMB"]
    assert len(folders["action_sack"]) == 25 and folders["action_sack"][18] == "AE GRIFBALL" and folders["action_sack"][-6:] == [
        "AE SWAT", "AE SNIPERS", "AE SHOTSNIP", "AE ROCKETS", "AE SHOTGUNS", "AE HEAVIES"]
    assert folders["practice"][:3] == ["AE PRACTICE", "AE BR PRAC", "AE TRAINING"] and len(folders["practice"]) == 11
    # (the free for all folder holds only free for all gametypes; the 2v2, 4v4 and BTB ones only team gametypes)
    team = {s: t for s, _, _, t in ROWS}
    assert not any(team[n] for n in folders["ffa"])
    assert all(team[n] for f in ("2v2", "4v4", "btb") for n in folders[f])
    names = re.search(r"arena_gametype_category_names\[NUMBER_OF_ARENA_GAMETYPE_CATEGORIES\] =\s*\{([^}]*)\}", NAMES)[1]
    assert re.findall(r'"([^"]*)"', names) == ["", "AE FFA", "AE 2V2", "AE 4V4", "AE BTB", "AE OBJECTIVE",
                                               "AE ACTION SACK", "AE PRACTICE", "NHE", "STOCK"]


def test_every_ae_gametype_in_one_category():
    listed = [stored for _, stored in CATEGORIES]
    assert len(listed) == len(set(listed))
    assert set(listed) == {s for s, _, st, _ in ROWS if st != "nhe"}


def test_br_twins_right_after_their_gametype():
    twin = dict(re.findall(r'^\t\{ "(AE BR [^"]+)", "([^"]+)" \}', SEEDS, re.M))
    listed = [stored for _, stored in CATEGORIES]
    category = dict((stored, c) for c, stored in CATEGORIES)
    for br, base in twin.items():
        assert listed.index(br) == listed.index(base) + 1, br
        assert category[br] == category[base], br
    # (the C code's key must be the same: the names check's "list block")
    assert "arena_gametype_list_block" in SEEDS and "category_order" in SEEDS


def test_display_names_say_what_the_table_says():
    for stored, display, st, team in ROWS:
        assert display.startswith("TEAM " if team else "FFA "), stored


def test_every_seed_is_in_the_table():
    seeds = set(re.findall(r'^\t\{ "([^"]+)", build_game_variant', SEEDS, re.M))
    assert seeds == {s for s, _, _, _ in ROWS}


def test_br_twins_follow_the_ae_order():
    twin = dict(re.findall(r'^\t\{ "(AE BR [^"]+)", "([^"]+)" \}', SEEDS, re.M))
    br = [s for s, _, st, _ in ROWS if st == "ae_br"]
    assert len(br) == len(twin) == 34
    ffa = [s for s, _, st, t in ROWS if st == "ae_br" and not t]
    assert br[:len(ffa)] == ffa
