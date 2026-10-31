"""tools/ae_gametype_change.py: its reading of arena_gametypes.c, the edits it plans (on this tree's source, in memory),
its checks of a box run, and the committed state of every change it made (tools/ae_test/gametype_changes.json:
the migrations, the golden files, seed_rules' expectations)."""
import json
import re
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import ae_gametype_change as change  # noqa: E402

SOURCE = change.Source.load()


def migration_rows(text):
    """[(revision, old name, old row, new name, new row, hash)] of the designated-form migrations"""
    table = text[text.index("arena_gametype_migrations[] ="):text.index("arena_gametype_test_migration =")]
    return re.findall(r'\{ \.revision = (\d+),\s*\.old_row = \{ (\.name = "([^"]+)".*?) \},\s*\.new_row = '
                      r'\{ (\.name = "([^"]+)".*?) \},\s*\.old_block_hash = "([0-9a-f]{40})" \}', table, re.S)


def test_reads_the_table():
    assert len(SOURCE.rows) == 115
    assert SOURCE.fields[:10] == ["name", "build", "flags", "score_to_win", "time_limit", "respawn_time",
                                  "suicide_penalty", "radar", "vehicle_set", "custom_loadout"]
    assert SOURCE.fields[-1] == "adjust"
    assert set(SOURCE.macros["ARENA_BR_START"]) == {"loadout_primary", "loadout_secondary", "weapon_pack",
                                                    "pack_loadout", "starting_frags"}
    row = SOURCE.by_name["AE BR TS"]
    assert row.value("score_to_win") == "ARENA_TEAM_SLAYER_SCORE"
    assert row.macro_values["weapon_pack"] == ("ARENA_BR_START", "_weapon_pack_battle_rifle")
    # (a macro right after the positional fields is a macro, not the next positional field)
    row = SOURCE.by_name["AE OCT BR"]
    assert row.value("weapon_set") is None
    assert row.macro_values["weapon_pack"] == ("ARENA_OCTAGON_BR_PRIMARY", "_weapon_pack_battle_rifle")
    assert "ARENA_OCTAGON_BR_PRIMARY" in row.designated_items()
    assert len(SOURCE.twins) == 34
    assert set(SOURCE.catalog) >= set(SOURCE.by_name)
    assert SOURCE.columns["universal_variant.respawn_time_growth"] == ("variant", "universal_variant.respawn_time_growth",
                                                                       "_audit_long")
    assert SOURCE.columns["options.time_limit"][0] == "options"
    assert SOURCE.columns["universal_variant.flags.practice"] == ("flag", "_game_variant_practice_bit",
                                                                  "_audit_bits_long")
    assert SOURCE.adjusts["arena_gametype_no_speed_changes"] == [
        "variant->game_engine_variant.slayer.no_death_bonus = TRUE;",
        "variant->game_engine_variant.slayer.no_kill_penalty = TRUE;"]


def test_designated_form_matches_the_frozen_rows():
    """a row in designated form, as a migration freezes it, is the table row's text: every table row whose name's
    last migration is in designated form (revision 14 on) reads the same as that migration's new row"""
    last = {}
    for revision, old_row, old_name, new_row, new_name, _ in migration_rows(SOURCE.text):
        # (revision 14 on: frozen from the table's text; older designated rows froze literal values)
        last[new_name] = new_row if int(revision) >= 14 else None
    last = {n: r for n, r in last.items() if r}
    assert len(last) >= 27
    for name, new_row in last.items():
        if name in SOURCE.by_name:
            assert change.squash(", ".join(SOURCE.by_name[name].designated_items())) == change.squash(new_row), name


def test_selection():
    br_slayers = change.select(SOURCE, ["AE BR&slayer"])
    assert len(br_slayers) == 12 and all(n.startswith("AE BR ") for n in br_slayers)
    assert len(change.select(SOURCE, ["NHE&ctf"])) == 9
    assert change.select(SOURCE, ["ae ffa snpr"]) == ["AE FFA SNPR"]
    assert change.select(SOURCE, ["AE COMP&team"]) == ["AE COMP TS", "AE COMP CTF", "AE COMP KOH", "AE COMP OB",
                                                       "AE C2V2 TS", "AE C2V2 CTF", "AE C2V2 KOH", "AE C2V2 OB"]
    names, added = change.add_twins(SOURCE, ["AE TEAM SLY", "AE FFA SNPR"])
    assert added == ["AE BR TS"] and names == ["AE TEAM SLY", "AE FFA SNPR", "AE BR TS"]
    with pytest.raises(SystemExit):
        change.select(SOURCE, ["AE NOPE"])
    with pytest.raises(SystemExit):
        change.select(SOURCE, ["NHE&race&ffa"])


def test_evaluate():
    known = SOURCE.constants
    assert change.evaluate("ARENA_COMPETITIVE_RESPAWN", known) == 150
    assert change.evaluate("ARENA_SET(4)", known) == 5
    assert change.evaluate("0x247B0020UL", known) == 0x247B0020
    assert change.evaluate("ARENA_TEAM_SLAYER_SCORE + 10", known) == 60
    assert change.evaluate("_loadout_weapon_pistol", known) is None


def plan(names, sets=(), adjusts=(), why="a test", **k):
    names, added = change.add_twins(SOURCE, change.select(SOURCE, names))
    row, steps = change.parse_sets(SOURCE, list(sets), list(adjusts))
    return change.Plan(SOURCE, names, row, steps, why, added_twins=added, **k)


def test_row_change_render():
    p = plan(["AE TEAM SLY"], ["score_to_win=60", "time_limit=12"])
    assert p.names() == ["AE TEAM SLY", "AE BR TS"]
    text = change.render(p, {"AE TEAM SLY": "a" * 40, "AE BR TS": "b" * 40})
    after = change.Source(text, "", "")
    assert after.revision == SOURCE.revision + 1
    for name in p.names():
        assert (after.by_name[name].value("score_to_win"), after.by_name[name].value("time_limit")) == ("60", "12")
    for name in SOURCE.by_name:
        if name not in p.names():
            row = SOURCE.by_name[name]
            assert after.by_name[name].designated_items() == row.designated_items(), name
    new = [m for m in migration_rows(text) if int(m[0]) == p.revision]
    assert [(m[2], m[5]) for m in new] == [("AE TEAM SLY", "a" * 40), ("AE BR TS", "b" * 40)]
    for revision, old_row, old_name, new_row, new_name, _ in new:
        assert change.squash(old_row) == change.squash(", ".join(SOURCE.by_name[old_name].designated_items()))
        assert change.squash(new_row) == change.squash(", ".join(after.by_name[new_name].designated_items()))
    assert after.migration_revisions.count(p.revision) == 2
    assert f"Revision {p.revision} (tools/ae_gametype_change.py): a test." in change.squash(text)
    rules = change.render_seed_rules(change.read(ROOT / change.SEED_RULES), p)
    space = {"__name__": "seed_rules_rendered"}
    exec(compile(rules, "seed_rules.py", "exec"), space)  # noqa: S102 (this tree's own file)
    for name in p.names():
        assert space["ROWS"][name][0]["score to win"] == "60"
        assert space["ROWS"][name][0]["time limit"] == "12 min"


def test_variant_change_render():
    """a variant field goes through a new adjust: after the row's old one (called), or alone; one that exists with
    the same body is reused; the BR start's fields and other engines' fields are refused"""
    # (AE FFA ROCK's own adjust: no speed changes, CONTROLLER CROUCH STAYS DOWN since revision 21, MELEE UNZOOMS ON
    # and MOUSE AIM ASSIST OFF since revision 22, VEHICLES DESTRUCTIBLE ON since revision 23, FRIENDLY SPLATTER FAST
    # ONLY and the weapon packs on since revision 25)
    p = plan(["AE FFA ROCK"], ["universal_variant.respawn_time_growth=0"])
    old = ("arena_gametype_no_speed_changes_and_controller_crouch_stays_down_and_melee_unzooms_and_mouse_aim_assist_off"
           "_and_vehicles_destructible_1_and_friendly_splatter_fast_and_weapon_packs_on_and_spawns_modern")
    function = old + "_and_no_respawn_growth"
    assert p.changes["AE FFA ROCK"]["adjust"] == (old, function)
    text = change.render(p, {})
    assert change.adjust_functions(text)[function] == [
        f"{old}(variant, options);", "variant->universal_variant.respawn_time_growth = 0;"]
    assert text.count(f"static void {function}(") == 2  # (prototype and definition)
    # (a gametype with no adjust of its own: KOTH 5M 7S of the NHE set; every AE gametype has CONTROLLER CROUCH's
    # since revision 21)
    p = plan(["KOTH 5M 7S"], adjusts=["no_odd_man_out"])
    text = change.render(p, {})
    assert change.adjust_functions(text)["arena_gametype_no_odd_man_out"] == [
        "variant->universal_variant.odd_man_out = FALSE;"]
    assert "\t(void)options;\n\tvariant->universal_variant.odd_man_out = FALSE;" in text
    p = plan(["KOTH 5M 7S"], adjusts=["no_speed_changes"])
    assert p.changes["KOTH 5M 7S"]["adjust"] == (None, "arena_gametype_no_speed_changes") and not p.functions
    p = plan(["AE PRACTICE"], ["universal_variant.flags.practice=0"])
    assert "SET_FLAG(variant->universal_variant.flags, _game_variant_practice_bit, FALSE);" in \
        change.render(p, {})
    with pytest.raises(SystemExit):
        plan(["AE BR FFA"], ["starting_frags=3"])
    with pytest.raises(SystemExit):
        plan(["AE CTF"], ["universal_variant.pad5.hit_feedback=0"])
    with pytest.raises(SystemExit):
        change.main(["--gametypes", "AE CTF", "--set", "game_engine_variant.slayer.kill_in_order=1", "--why", "x",
                     "--dry-run"])
    with pytest.raises(SystemExit):
        plan(["AE CTF"], ["score_to_win=ARENA_CTF_SCORE"])  # (no change)


def results(p, **override):
    old = {n: {"block": n + "-old", "file": n + "-f", "folder": n[:3]} for n in ("AE FFA SNPR", "AE CTF", "TS 50")}
    new = dict(old, **{"AE FFA SNPR": {"block": "snpr-new", "file": "x", "folder": "AE "}})
    rev = p.revision
    data = {"old": {"golden": old, "debug": "", "record": f"#revision {rev - 1}\n"},
            "new": {"golden": new, "debug": "", "record": f"#revision {rev}\n"},
            "migrations": {
                "migrate-own": {"before": dict(old, **{"MY FFA SNPR": {"block": "o", "file": "o", "folder": "MY"}}),
                                "after": dict(new, **{"MY FFA SNPR": {"block": "o", "file": "o", "folder": "MY"}}),
                                "debug": f"updated arena gametype 'AE FFA SNPR' (revision {rev})",
                                "record": f"#revision {rev}", "backups": [f"revision_{rev}/AE /blam.lst"],
                                "edited": None, "own": ("AE FFA SNPR", "MY FFA SNPR")},
                "migrate-edit": {"before": dict(old, **{"AE FFA SNPR": {"block": "e", "file": "e", "folder": "AE "}}),
                                 "after": dict(old, **{"AE FFA SNPR": {"block": "e", "file": "e", "folder": "AE "}}),
                                 "debug": f"arena gametype 'AE FFA SNPR' kept as it is (changed since seeded; "
                                          f"revision {rev})", "record": f"#revision {rev}", "backups": [],
                                 "edited": "AE FFA SNPR", "own": None}},
            "rules": {"AE FFA SNPR": [True, {}]}}
    for key, value in override.items():
        data[key] = value
    return data


def test_check_results():
    p = plan(["AE FFA SNPR"], ["universal_variant.respawn_time_growth=0"])
    assert [m[0] for m in change.migration_cases(p.names())] == ["migrate-own", "migrate-edit"]
    assert change.check_results(p, results(p)) == []
    bad = results(p)
    bad["new"]["debug"] = "x arena gametypes self-check: ERROR: revision 15's old row ..."
    bad["new"]["golden"]["TS 50"] = {"block": "other", "file": "y", "folder": "TS "}
    bad["migrations"]["migrate-own"]["after"]["MY FFA SNPR"] = {"block": "o2", "file": "o2", "folder": "MY"}
    bad["migrations"]["migrate-own"]["backups"] = []
    bad["migrations"]["migrate-edit"]["debug"] = ""
    bad["rules"] = {"AE FFA SNPR": [False, {"respawn growth": ["0 ticks", "150 ticks"]}]}
    problems = "\n".join(change.check_results(p, bad))
    for expected in ("self-check: ERROR", "TS 50: its file changed", "MY FFA SNPR changed", "no revision_",
                     "no 'kept as it is' line", "seed_rules AE FFA SNPR"):
        assert expected in problems, expected
    assert [m[0] for m in change.migration_cases(["A", "B"])] == ["migrate"]
    # (seed_rules' mismatches in fields the change leaves alone: a warning, not a failure)
    stale = results(p, rules={"AE FFA SNPR": [False, {"starting frags": ["0", "2"]}]})
    assert change.check_results(p, stale) == []
    assert "starting frags" in change.rules_warnings(p, stale)[0]


MANIFEST = ROOT / change.MANIFEST
ENTRIES = json.loads(MANIFEST.read_text()) if MANIFEST.exists() else []


@pytest.mark.parametrize("entry", ENTRIES, ids=[f"revision {e['revision']}" for e in ENTRIES])
def test_committed_change(entry):
    """each change the tool made, as committed: its migrations (one per gametype, the old file's hash from the
    revision before's golden root), its golden file (only its gametypes' files differ from the revision before's,
    in the same folders), its adjusts, and seed_rules' expectations; the latest change's new rows are the table's"""
    import seed_rules
    revision = entry["revision"]
    golden = json.loads((ROOT / "tools" / "ae_test" / entry["golden"]).read_text())
    before = json.loads((ROOT / change.GOLDEN.format(revision - 1)).read_text())
    rows = [m for m in migration_rows(SOURCE.text) if int(m[0]) == revision]
    assert [m[2] for m in rows] == entry["gametypes"] == [m[4] for m in rows]
    for _, old_row, name, new_row, _, digest in rows:
        assert digest == entry["old_block_hashes"][name] == before[name]["block"], name
        if name in entry["adjusts"]:
            assert f".adjust = {entry['adjusts'][name]}" in change.squash(new_row), name
            assert entry["adjusts"][name] in SOURCE.adjusts, name
    # (a revision that also adds seeds, 22, names them: no migration, not in the root before)
    added = entry.get("added_seeds", [])
    assert not set(added) & set(before) and not set(added) & set(entry["gametypes"])
    assert sorted(golden) == sorted(list(before) + added)
    assert sorted(n for n in before if golden[n]["block"] != before[n]["block"]) == sorted(entry["gametypes"])
    assert all(golden[n]["folder"] == before[n]["folder"] for n in before)
    for name, words in entry["seed_rules"].items():
        for key, value in words.items():
            assert seed_rules.ROWS[name][0][key] == value, (name, key)
    if revision == SOURCE.revision:
        for _, _, _, new_row, name, _ in rows:
            assert change.squash(", ".join(SOURCE.by_name[name].designated_items())) == change.squash(new_row), name
