"""The gametype lists' grouping (bug 101, regressed report 2026-10-15): the lists sort by the names table's set and
place (arena_gametypes.c arena_gametypes_sort), so the table is the order. Pinned here with the current seeds (AE,
AE BR, AE COMP, OCTAGON, NHE, TRAINING): in each AE set the free for all gametypes come before the team ones, the
sets keep their order, every seed is in the table (an unknown one sorts after all sets, alphabetically, mixing FFA
and TEAM), AE BR lists the twins in the order of the AE gametypes they copy, and OCTAGON's free for all drills
(revision 16) come after the AE sets' team block, before NHE's."""
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


def list_order():
    """the lists' order (owner 2026-10-15): one FFA block (AE, AE BR, AE COMP), one TEAM block, then OCTAGON (revision
    16), NHE, TRAINING"""
    key = {"ae": 1, "ae_br": 2, "ae_comp": 3}
    rows = []
    for i, (stored, _, st, team) in enumerate(ROWS):
        block = key[st] + (3 if team else 0) if st in key else {"octagon": 7, "nhe": 8}.get(st, 9)
        rows.append((block, i, stored, st, team))
    return sorted(rows)


def test_one_ffa_block_then_one_team_block():
    order = list_order()
    ae = [(st, team) for _, _, _, st, team in order if st in ("ae", "ae_br", "ae_comp")]
    flags = [t for _, t in ae]
    assert flags == sorted(flags) and flags.count(False) == 30 and flags.count(True) == 30
    sets = [s for s, t in ae if not t]
    assert sets == sorted(sets, key=["ae", "ae_br", "ae_comp"].index)
    assert [r[3] for r in order if r[3] in ("nhe", "training")] == [r[3] for r in order][-len([1 for r in order if r[3] in ("nhe", "training")]):]
    # (OCTAGON's four drills right after the AE sets' team block, all free for all)
    sets = [r[3] for r in order]
    octagon = [r for r in order if r[3] == "octagon"]
    assert len(octagon) == 4 and not any(r[4] for r in octagon)
    assert sets.index("octagon") == len(ae) and sets.index("nhe") == len(ae) + len(octagon)
    # (the C code's key must be the same: the names check's "list block")
    assert "arena_gametype_list_block" in (ROOT / "source/saved games/arena_gametypes.c").read_text(encoding="latin1")


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
