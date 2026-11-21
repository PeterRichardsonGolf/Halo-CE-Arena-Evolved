"""ACTION SACK (the swat-snipers playlist, label kept apart from its id so the rank carries over): every entry plays a
built-in TEAM gametype, and each of the five weapon sets comes up."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOCIAL = (ROOT / "port/assets/network/social.txt").read_text()
NAMES = (ROOT / "source/saved games/arena_gametype_names.c").read_text()
SETS = {"AE SWAT", "AE SNIPERS", "AE SHOTGUNS", "AE ROCKETS", "AE SHOTSNIP"}


def block():
    start = SOCIAL.index("# social: swat-snipers")
    return SOCIAL[start:SOCIAL.index("# social: ", start + 1)]


def team_seeds():
    return set(re.findall(r'^\t\{ "([^"]+)", "[^"]*", \w+, game_engine_\w+, TRUE,', NAMES, re.M))


def test_header():
    text = block()
    assert "# label: ACTION SACK" in text and "SWAT & SNIPERS" not in SOCIAL
    assert "# teams: 2 x 4" in text and "# party: 4" in text and "# kind: social" in text


def test_every_entry_is_a_team_gametype():
    entries = re.findall(r"^\S+:file=([^ ].*?)\s+# weight", block(), re.M)
    assert entries
    teams = team_seeds()
    assert {"AE SWAT", "AE SHOTSNIP"} <= teams
    assert set(entries) <= SETS and set(entries) <= teams


def test_every_set_comes_up():
    entries = re.findall(r"^\S+:file=([^ ].*?)\s+# weight", block(), re.M)
    assert set(entries) == SETS
