"""tools/ae_gametype_audit.py on a small sample of debug.arena_audit's TSV (ae_gametype_audit.c), and its reading of
this tree's menus (which fields the PC gametype editor shows) and seed table (each gametype's stock base)."""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import ae_gametype_audit as audit  # noqa: E402

COLUMNS = ["universal_variant.teams", "universal_variant.starting_frags", "universal_variant.flags.no_shields",
           "universal_variant.flags.pregame_countdown", "universal_variant.score_to_win",
           "game_engine_variant.slayer.no_death_bonus", "game_engine_variant.slayer.no_kill_penalty",
           "game_engine_variant.ctf.assault", "options.time_limit", "game_variant.flags"]
HEADER = audit.META + COLUMNS


def row(kind, name, set_name, engine, base, values, file="file", as_seeded="1"):
    return [kind, name, name, set_name, engine, base, file, as_seeded] + values


SAMPLE_ROWS = [
    # teams, frags, no_shields, countdown, score, no_death_bonus, no_kill_penalty, ctf.assault, time, flags
    row("ae", "AE FFA SLAY", "AE", "slayer", "", ["0", "2", "0", "1", "25", "0", "0", "", "15", "0"]),
    row("ae", "AE TEAM SLY", "AE", "slayer", "", ["1", "2", "0", "1", "50", "0", "0", "", "15", "0"]),
    row("ae", "AE 2V2 SLY", "AE", "slayer", "", ["1", "2", "0", "1", "50", "1", "1", "", "15", "0"]),
    row("ae", "AE SWAT", "AE", "slayer", "", ["1", "0", "1", "1", "50", "0", "0", "", "15", "0"]),
    row("ae", "AE SNIPERS", "AE", "slayer", "", ["1", "2", "0", "0", "50", "0", "0", "", "15", "0"]),
    row("ae", "AE CTF", "AE", "ctf", "", ["1", "2", "0", "1", "3", "", "", "0", "15", "0"]),
    row("ae", "AE ROCKETS", "AE", "", "", [""] * len(COLUMNS), file="missing", as_seeded=""),
    row("stock", "slayer", "", "slayer", "slayer", ["0", "0", "0", "0", "25", "0", "0", "", "0", "1"]),
    row("stock", "team_slayer", "", "slayer", "team_slayer", ["1", "0", "0", "0", "25", "0", "0", "", "0", "1"]),
    row("stock", "ctf", "", "ctf", "ctf", ["1", "0", "0", "0", "3", "", "", "1", "0", "1"]),
]


def write_sample(path, rows=SAMPLE_ROWS):
    path.write_text("\n".join("\t".join(r) for r in [HEADER] + rows) + "\n")
    return path


def test_menus_shown_fields():
    shown = audit.shown_fields(ROOT)
    assert shown is not None
    for field in ("game_engine_variant.slayer.no_death_bonus", "options.time_limit", "universal_variant.lives",
                  "universal_variant.flags.health_style_second", "universal_variant.flags.objective_in_sight",
                  "universal_variant.pad5.hit_feedback", "universal_variant.weapon_pack"):
        assert field in shown, field
    for field in ("universal_variant.starting_frags", "universal_variant.pad5.juggernaut_h2",
                  "game_engine_variant.ctf.reset_on_capture", "game_engine_variant.oddball.ball_spawn_delay"):
        assert field not in shown, field
    assert audit.is_shown("options.vehicle_counts.red.ghost", shown)


def test_seed_bases_from_the_table():
    bases = audit.seed_bases(ROOT)
    assert bases["AE FFA SLAY"] == "slayer"
    assert bases["AE TEAM SLY"] == "team_slayer"
    assert bases["AE CTF"] == "ctf"
    assert len(bases) > 50


def test_report_findings(tmp_path):
    header, rows = audit.read_tsv(write_sample(tmp_path / "audit.tsv"))
    text = audit.report(header, rows, ROOT)
    death = next(line for line in text.splitlines() if line.startswith("- **DEATH BONUS on"))
    assert "`AE FFA SLAY`" in death and "`AE SWAT`" in death and "`AE 2V2 SLY`" not in death
    assert "AE CTF" not in death and "AE ROCKETS" not in death
    assert any(line.startswith("- **KILL PENALTY on") for line in text.splitlines())
    # (starting frags: hidden, away from the stock 0)
    frags = next(line for line in text.splitlines() if line.startswith("- `universal_variant.starting_frags`"))
    assert "`AE TEAM SLY`" in frags and "`AE SWAT`" not in frags
    # (the odd ones out: SWAT's shields and frags, SNIPERS' countdown in the AE set's slayers)
    assert "`AE SNIPERS` has `universal_variant.flags.pregame_countdown` = 0" in text
    assert "`AE SWAT` has `universal_variant.flags.no_shields` = 1" in text
    assert "### Without a file" in text and "`AE ROCKETS`" in text
    # (the slayer table: its base and marks; game_variant.flags never compared)
    table = text[text.index("### slayer"):]
    assert "| `AE FFA SLAY` | AE | slayer |" in table
    assert "| `AE TEAM SLY` | AE | team_slayer |" in table
    assert "game_variant.flags" not in table
    assert "2*" in table and "0!" in table
    assert "u.starting_frags H" in table


def test_bases_without_the_source(tmp_path):
    header, rows = audit.read_tsv(write_sample(tmp_path / "audit.tsv"))
    result = audit.analyse(header, rows, tmp_path)
    assert result["shown"] is None
    bases = {r["stored_name"]: r["base"] for r in result["ae"]}
    assert bases["AE TEAM SLY"] == "team_slayer?"
    assert bases["AE CTF"] == "ctf?"


def test_diff(tmp_path):
    old = write_sample(tmp_path / "old.tsv")
    changed = [list(r) for r in SAMPLE_ROWS if r[1] != "AE CTF"]
    changed[0][audit.META.index("as_seeded")] = "0"
    changed[0][HEADER.index("game_engine_variant.slayer.no_death_bonus")] = "1"
    new = write_sample(tmp_path / "new.tsv", changed)
    text = audit.diff(old, new)
    assert "## `AE FFA SLAY` (ae)" in text
    assert "- `game_engine_variant.slayer.no_death_bonus`: 0 -> 1" in text
    assert "Gametypes removed: `AE CTF` (ae)" in text
    assert "as_seeded" not in text
    assert "1 fields changed." in text


def test_main_writes_the_report(tmp_path):
    sample = write_sample(tmp_path / "audit.tsv")
    out = tmp_path / "REPORT.md"
    assert audit.main([str(sample), "-o", str(out)]) == 0
    assert out.read_text().startswith("# Gametype audit")


def test_br_twins_and_comp_peers():
    def r(name, display, set_name, values):
        return dict({"stored_name": name, "display_name": display, "set": set_name, "engine": "slayer",
                     "file": "file"}, **values)
    rows = [r("AE COMP TS", "TEAM AE COMP SLAYER", "AE COMP", {"options.time_limit": "0", "options.loadout": "1"}),
            r("AE BR C TS", "TEAM AE BR COMP SLAYER", "AE BR", {"options.time_limit": "15", "options.loadout": "0"}),
            r("AE BR TS", "TEAM AE BR SLAYER", "AE BR", {"options.time_limit": "15", "options.loadout": "0"})]
    found = audit.twins(rows, ["options.time_limit", "options.loadout"])
    assert [(a["stored_name"], b["stored_name"], c) for a, b, c in found] == \
        [("AE BR C TS", "AE COMP TS", [("options.time_limit", "15", "0")])]
    assert audit.peer_set(rows[1]) == "AE BR COMP" and audit.peer_set(rows[2]) == "AE BR"
    assert audit.peer_set(rows[0]) == "AE COMP"
