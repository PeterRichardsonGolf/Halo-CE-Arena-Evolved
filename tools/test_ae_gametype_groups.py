"""The gametype lists' grouping (bug 101, regressed report 2026-10-15): the lists sort by the names table's set and
place (arena_gametypes.c arena_gametypes_sort), so the table is the order. Pinned here with the current seeds (AE,
AE BR, AE COMP, OCTAGON, NHE, TRAINING): in each AE set the free for all gametypes come before the team ones, the
sets keep their order, every seed is in the table (an unknown one sorts after all sets, alphabetically, mixing FFA
and TEAM), AE BR lists the twins in the order of the AE gametypes they copy. The lists themselves sort by category (owner
2026-10-20, "by play style, BR inside": arena_gametype_names.c's categories table): AE CASUAL, AE COMP, AE PARTY, AE
WEAPONS, AE PRACTICE, then NHE's in their table order; every one of AE's in the table once, each battle rifle twin
right after the gametype it copies."""
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
    assert order == ["casual", "comp", "party", "weapons", "practice"]


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
    assert len(br) == len(twin) == 25
    ffa = [s for s, _, st, t in ROWS if st == "ae_br" and not t]
    assert br[:len(ffa)] == ffa
