"""The campaign's scoring values: port/linux/game/campaign_score_values.c is
what tools/campaign_score_values.py makes of its JSON, and the JSON's names
are actor_types.h's and skulls.c's."""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import campaign_score_values  # noqa: E402


def _generate_from(values: dict) -> str:
    """the C file a JSON of these values makes (the real one left as it is)"""
    saved = campaign_score_values.JSON_PATH.read_text()
    try:
        campaign_score_values.JSON_PATH.write_text(json.dumps(values))
        return campaign_score_values.generate()
    finally:
        campaign_score_values.JSON_PATH.write_text(saved)


def _rejects(values: dict, words: str):
    try:
        _generate_from(values)
    except campaign_score_values.ValuesError as error:
        assert words in str(error), error
        return
    raise AssertionError(f"accepted, expected an error about {words}")


def test_c_file_is_the_json():
    """the checked-in C file is what the JSON makes"""
    assert campaign_score_values.C_PATH.read_text() == campaign_score_values.generate()


def test_values():
    """the difficulties, the par times (in minutes) and the time's brackets"""
    text = campaign_score_values.generate()
    assert "{ 0.5f, 1.0f, 2.0f, 4.0f }," in text
    assert '{ "a10", 900 },' in text
    assert '{ "c10", 720 },' in text
    assert "{ -1.0f, 0.5f }," in text
    assert "{ 0.8f, 1.0f, 1.3f, 1.8f }," in text
    assert "\t1.2f," in text


def test_names_are_checked():
    """an actor type or a skull that does not exist is an error"""
    values = json.loads(campaign_score_values.JSON_PATH.read_text())
    bad = json.loads(json.dumps(values))
    bad["enemies"]["brute"] = {"default": 20}
    _rejects(bad, "not an actor type")
    bad = json.loads(json.dumps(values))
    bad["skulls"]["multipliers"]["tilt"] = 1.2
    _rejects(bad, "not a skull")
    bad = json.loads(json.dumps(values))
    bad["penalties"]["death"] = 25
    _rejects(bad, "should be negative")
    bad = json.loads(json.dumps(values))
    bad["time"]["par_times"]["a10"] = "15:00"
    _rejects(bad, "is not a number")


def test_actor_types_and_skulls_are_read():
    types = campaign_score_values.actor_types()
    assert types[:3] == ["elite", "jackal", "grunt"]
    assert "mounted_weapon" in types
    assert "iron" in campaign_score_values.skull_keys()


def test_every_skull_is_listed():
    """each skull key has its multiplier in the JSON"""
    values = json.loads(campaign_score_values.JSON_PATH.read_text())
    listed = set(values["skulls"]["multipliers"])
    missing = [key for key in campaign_score_values.skull_keys() if key not in listed]
    assert not missing, f"skulls with no multiplier: {missing}"


def test_every_medal_has_points():
    """each of ae_campaign_medals_rules.c's medals has its points in the JSON, and its AE medal number is one of
    ae_medal_ids.h's (or -1: no match)"""
    values = json.loads(campaign_score_values.JSON_PATH.read_text())
    keys = campaign_score_values.medal_keys()
    assert len(keys) == 38
    missing = [key for key in keys if key not in values["medals"]["points"]]
    assert not missing, f"medals with no points: {missing}"
    ids = (ROOT / "port/linux/game/ae_medal_ids.h").read_text()
    names = set(re.findall(r"^AE_MEDAL\((\w+),", ids, re.M))
    table = (ROOT / "port/linux/game/ae_campaign_medals_rules.c").read_text()
    rows = re.findall(r'\{ "(\w+)", "[^"]+", (AE_MEDAL_\w+|-1) \}', table)
    assert [key for key, _ in rows] == keys
    unmatched = [key for key, ae in rows if ae == "-1"]
    assert unmatched == ["stick_spree", "assassin", "incineration"], unmatched
    for key, ae in rows:
        assert ae == "-1" or ae[len("AE_MEDAL_"):] in names, (key, ae)


def test_a_damaged_or_oversized_table_is_refused_with_a_reason():
    """the JSON is parsed bounded: size, depth, rows, and its text"""
    good = campaign_score_values.JSON_PATH.read_text()
    assert campaign_score_values.load(good)["difficulty"]["normal"] == 1.0
    for text, words in [
        (good[: len(good) // 2], "campaign_score_values.json"),
        ("[1, 2]", "not an object"),
        (" " * (campaign_score_values.MAXIMUM_BYTES + 1), "bigger than"),
        ('{"a":' * 20 + "1" + "}" * 20, "nested deeper"),
        ('{"a":' * 100000 + "1" + "}" * 100000, "campaign_score_values.json"),
        ("{" + ", ".join(f'"k{i}": 1' for i in range(campaign_score_values.MAXIMUM_ROWS + 1)) + "}", "keys"),
        ('{"a": [' + ", ".join("1" for _ in range(campaign_score_values.MAXIMUM_ROWS + 1)) + "]}", "rows"),
    ]:
        try:
            campaign_score_values.load(text)
        except campaign_score_values.ValuesError as error:
            assert words in str(error), (words, error)
        else:
            raise AssertionError(f"accepted, expected an error about {words}")
