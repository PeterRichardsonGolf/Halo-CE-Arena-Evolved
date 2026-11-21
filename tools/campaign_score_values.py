#!/usr/bin/env python3
"""Writes the campaign's scoring values (port/linux/game/ae_campaign_score.c's)
from port/linux/game/campaign_score_values.json, as
port/linux/game/campaign_score_values.c:

    python3 tools/campaign_score_values.py [--check]

configure.py runs it (write()) and lists the JSON among its inputs, so that
ninja reruns configure.py, and so this, when a value changes. --check fails
when the C file is not what the JSON makes (tools/test_campaign_score_values.py).

The JSON's actor types are actor_types.h's (elite, grunt, marine, ...), its
skulls are SKULL_KEYS' (the skulls' keys: iron, black_eye, ...) and its medals
ae_campaign_medals_rules.c's: a name of none of them is an error, not a value
that never scores. A file that is not valid JSON, bigger than MAXIMUM_BYTES,
nested deeper than MAXIMUM_DEPTH or with more than MAXIMUM_ROWS rows in a
list is refused with its reason before anything is written."""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
JSON_PATH = ROOT / "port/linux/game/campaign_score_values.json"
C_PATH = ROOT / "port/linux/game/campaign_score_values.c"
ACTOR_TYPES = ROOT / "source/ai/actor_types.h"
MEDALS_C = ROOT / "port/linux/game/ae_campaign_medals_rules.c"
# the table is read as text, bounded: a JSON bigger than this, nested deeper or with a list longer is damaged or hostile
MAXIMUM_BYTES = 256 * 1024
MAXIMUM_DEPTH = 8
MAXIMUM_ROWS = 512
# the skulls' keys: the multipliers' names (the skulls themselves are not in this game yet: their multiplier is
# ae_campaign_score_skulls_multiplier's, 1.0 until they are)
SKULL_KEYS = ['anger', 'armistice', 'assassins', 'bandanna', 'black_eye', 'blind', 'boom', 'catch', 'confetti_party', 'efficient', 'envy', 'eye_patch', 'famine', 'floor_is_lava', 'fog', 'foreign', 'ghost', 'give_and_take', 'grunt_birthday_party', 'grunt_funeral', 'iron', 'johnny_ammo', 'leadhead', 'm_rating', 'malfunction', 'mythic', 'need_glasses', 'outbreak', 'pinata', 'protected', 'recession', 'rip_and_tear', 'scarab', 'sputnik', 'streaking', 'swarm', 'thats_just_wrong', 'thunderstorm', 'tough_luck', 'woe_is_me']
MEDAL_WEAPONS = ("sniper", "shotgun", "flame", "grenade_stick", "splatter")
DIFFICULTIES = ("easy", "normal", "heroic", "legendary")
STYLES = ("regular", "one_shot_one_kill", "headshot", "melee", "backsmack")
COMBINES = ("multiply", "highest")


class ValuesError(Exception):
    pass


def actor_types() -> list:
    """actor_types.h's actor types, in its enum's order (elite first)"""
    text = ACTOR_TYPES.read_text()
    enum = re.search(r"_actor_elite\s*=\s*0,(.*?)NUMBER_OF_ACTOR_TYPES", text, re.S)
    if not enum:
        raise ValuesError(f"{ACTOR_TYPES}: no actor type enum")
    return ["elite"] + re.findall(r"_actor_(\w+)\s*,", enum.group(1))


def skull_keys() -> list:
    """the skulls' keys, in the order the multipliers list them"""
    return list(SKULL_KEYS)


def medal_keys() -> list:
    """ae_campaign_medals_rules.c's medals' keys, in their table's order"""
    text = MEDALS_C.read_text()
    table = re.search(r"ae_campaign_medal_descriptions\[AE_CAMPAIGN_MEDAL_COUNT\]\s*=\s*\{(.*?)\n\};", text, re.S)
    if not table:
        raise ValuesError(f"{MEDALS_C}: no ae_campaign_medal_descriptions table")
    return re.findall(r'\{\s*"([a-z_]+)",', table.group(1))


def _c_string(text: str) -> str:
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def _c_float(value) -> str:
    return f"{float(value)!r}f"


def _number(where: str, value, integer: bool = False):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValuesError(f"{where}: {value!r} is not a number")
    if integer and value != int(value):
        raise ValuesError(f"{where}: {value!r} is not a whole number of points")
    return int(value) if integer else float(value)


def _combine(where: str, value) -> int:
    if value not in COMBINES:
        raise ValuesError(f"{where}: combine is one of {', '.join(COMBINES)}, not {value!r}")
    return COMBINES.index(value)


def _par_seconds(level: str, minutes) -> int:
    """a par time, in minutes (15.5: 15:30), in whole seconds"""
    minutes = _number(f"time.par_times.{level}", minutes)
    if minutes <= 0:
        raise ValuesError(f"time.par_times.{level}: {minutes} minutes is no time")
    return int(round(minutes * 60))


class _Writer:
    """the C file's lines: its word lists and tables, then the values"""

    def __init__(self):
        self.lines = []
        self.names = 0

    def name(self, kind: str) -> str:
        self.names += 1
        return f"campaign_score_{kind}_{self.names}"

    def words(self, where: str, words) -> str:
        if isinstance(words, str):
            words = [words]
        if not isinstance(words, list) or not words or not all(isinstance(word, str) and word for word in words):
            raise ValuesError(f"{where}: match is a word or a list of words")
        name = self.name("words")
        self.lines.append(f"static char const *const {name}[] = {{ "
                          + ", ".join(_c_string(word.lower()) for word in words) + ", NULL };")
        return name

    def matches(self, where: str, entries, sign: int) -> tuple:
        if not isinstance(entries, list):
            raise ValuesError(f"{where}: a list of {{ match, points }}")
        rows = []
        for index, entry in enumerate(entries):
            here = f"{where}[{index}]"
            if not isinstance(entry, dict) or "match" not in entry or "points" not in entry:
                raise ValuesError(f"{here}: needs match and points")
            points = _number(f"{here}.points", entry["points"], integer=True)
            if sign * points < 0:
                raise ValuesError(f"{here}.points: {points} should be {'positive' if sign > 0 else 'negative'}")
            rows.append(f"\t{{ {self.words(here, entry['match'])}, {points} }},")
        if not rows:
            return "NULL", 0
        name = self.name("matches")
        self.lines.append(f"static struct campaign_score_match_value const {name}[] =\n{{")
        self.lines += rows
        self.lines.append("};")
        return name, len(rows)

    def actors(self, where: str, table, types: list, sign: int) -> tuple:
        if not isinstance(table, dict):
            raise ValuesError(f"{where}: an object of actor types")
        rows = []
        for actor, entry in table.items():
            if actor.startswith("_"):
                continue
            here = f"{where}.{actor}"
            if actor not in types:
                raise ValuesError(f"{here}: not an actor type (actor_types.h: {', '.join(types)})")
            if not isinstance(entry, dict) or "default" not in entry:
                raise ValuesError(f"{here}: needs a default")
            points = _number(f"{here}.default", entry["default"], integer=True)
            if sign * points < 0:
                raise ValuesError(f"{here}.default: {points} should be {'positive' if sign > 0 else 'negative'}")
            variants, count = self.matches(f"{here}.variants", entry.get("variants", []), sign)
            rows.append(f"\t{{ {types.index(actor)}, {points}, {count}, {variants} }}, /* {actor} */")
        if not rows:
            return "NULL", 0
        name = self.name("actors")
        self.lines.append(f"static struct campaign_score_actor_value const {name}[] =\n{{")
        self.lines += rows
        self.lines.append("};")
        return name, len(rows)


def _bounded(value, depth: int = 0) -> None:
    """a parsed value no deeper than MAXIMUM_DEPTH, no list or object longer than MAXIMUM_ROWS"""
    if depth > MAXIMUM_DEPTH:
        raise ValuesError(f"nested deeper than {MAXIMUM_DEPTH} levels")
    if isinstance(value, dict):
        if len(value) > MAXIMUM_ROWS:
            raise ValuesError(f"an object of {len(value)} keys (at most {MAXIMUM_ROWS})")
        for item in value.values():
            _bounded(item, depth + 1)
    elif isinstance(value, list):
        if len(value) > MAXIMUM_ROWS:
            raise ValuesError(f"a list of {len(value)} rows (at most {MAXIMUM_ROWS})")
        for item in value:
            _bounded(item, depth + 1)


def load(text: str) -> dict:
    """the values' JSON parsed, or refused with its reason: too big, damaged, not an object, too deep or too long"""
    if len(text.encode("utf-8")) > MAXIMUM_BYTES:
        raise ValuesError(f"{JSON_PATH.name}: bigger than {MAXIMUM_BYTES} bytes")
    try:
        values = json.loads(text)
    except (json.JSONDecodeError, RecursionError) as error:
        raise ValuesError(f"{JSON_PATH.name}: {error}")
    if not isinstance(values, dict):
        raise ValuesError(f"{JSON_PATH.name}: not an object")
    try:
        _bounded(values)
    except ValuesError as error:
        raise ValuesError(f"{JSON_PATH.name}: {error}")
    return values


def generate() -> str:
    """the C file campaign_score_values.json makes"""
    values = load(JSON_PATH.read_text())
    types = actor_types()
    skulls = skull_keys()
    out = _Writer()

    difficulty = values.get("difficulty", {})
    difficulties = [_number(f"difficulty.{name}", difficulty.get(name)) for name in DIFFICULTIES]

    time = values.get("time", {})
    brackets = time.get("brackets", [])
    if not brackets:
        raise ValuesError("time.brackets: at least one")
    bracket_rows = []
    last = -1.0
    for index, bracket in enumerate(brackets):
        up_to = bracket.get("up_to_par")
        multiplier = _number(f"time.brackets[{index}].multiplier", bracket.get("multiplier"))
        if up_to is None:
            if index != len(brackets) - 1:
                raise ValuesError(f"time.brackets[{index}]: only the last has no up_to_par")
            up_to = -1.0
        else:
            up_to = _number(f"time.brackets[{index}].up_to_par", up_to)
            if up_to <= last:
                raise ValuesError(f"time.brackets[{index}].up_to_par: {up_to} after {last}: in order, rising")
            last = up_to
        bracket_rows.append(f"\t{{ {_c_float(up_to)}, {_c_float(multiplier)} }},")
    out.lines.append("static struct campaign_score_time_bracket const campaign_score_time_brackets[] =\n{")
    out.lines += bracket_rows
    out.lines.append("};")
    par_difficulty = time.get("par_difficulty", {})
    par_difficulties = [_number(f"time.par_difficulty.{name}", par_difficulty.get(name, 1.0)) for name in DIFFICULTIES]
    daily_challenge_par = _number("time.daily_challenge_par", time.get("daily_challenge_par", 1.0))
    for where, value in [*zip((f"time.par_difficulty.{name}" for name in DIFFICULTIES), par_difficulties),
                         ("time.daily_challenge_par", daily_challenge_par)]:
        if value <= 0:
            raise ValuesError(f"{where}: {value} should be more than 0")
    par_rows = [f"\t{{ {_c_string(level.lower())}, {_par_seconds(level, text)} }},"
                for level, text in time.get("par_times", {}).items() if not level.startswith("_")]
    if par_rows:
        out.lines.append("static struct campaign_score_par_time const campaign_score_par_times[] =\n{")
        out.lines += par_rows
        out.lines.append("};")

    style = values.get("style", {})
    style_combine = _combine("style", style.get("combine", "multiply"))
    styles = [_number(f"style.{name}", style.get(name)) for name in STYLES]

    enemies, enemy_count = out.actors("enemies", values.get("enemies", {}), types, 1)
    vehicles, vehicle_count = out.matches("vehicles", values.get("vehicles", []), 1)
    penalties = values.get("penalties", {})
    penalty_points = [_number(f"penalties.{name}", penalties.get(name), integer=True)
                      for name in ("death", "betrayal", "checkpoint_revert")]
    for name, points in zip(("death", "betrayal", "checkpoint_revert"), penalty_points):
        if points > 0:
            raise ValuesError(f"penalties.{name}: {points} should be negative (or 0)")
    allies, ally_count = out.actors("penalties.allies", penalties.get("allies", {}), types, -1)
    ally_vehicles, ally_vehicle_count = out.matches("penalties.vehicles", penalties.get("vehicles", []), -1)

    skull_values = values.get("skulls", {})
    skull_combine = _combine("skulls", skull_values.get("combine", "multiply"))
    skull_default = _number("skulls.default", skull_values.get("default", 1.0))
    skull_rows = []
    for key, multiplier in skull_values.get("multipliers", {}).items():
        if key not in skulls:
            raise ValuesError(f"skulls.multipliers.{key}: not a skull (the skulls' keys: {', '.join(skulls)})")
        skull_rows.append(f"\t{{ {_c_string(key)}, {_c_float(_number(f'skulls.multipliers.{key}', multiplier))} }},")
    if skull_rows:
        out.lines.append("static struct campaign_score_skull_value const campaign_score_skull_values[] =\n{")
        out.lines += skull_rows
        out.lines.append("};")

    medals = values.get("medals", {})
    keys = medal_keys()
    medal_rows = []
    for key, points in medals.get("points", {}).items():
        if key.startswith("_"):
            continue
        if key not in keys:
            raise ValuesError(f"medals.points.{key}: not a medal (ae_campaign_medals_rules.c: {', '.join(keys)})")
        medal_rows.append(f"\t{{ {_c_string(key)}, {_number(f'medals.points.{key}', points, integer=True)} }},")
    if medal_rows:
        out.lines.append("static struct campaign_score_medal_value const campaign_score_medal_values[] =\n{")
        out.lines += medal_rows
        out.lines.append("};")
    weapons = medals.get("weapons", {})
    for name in weapons:
        if not name.startswith("_") and name not in MEDAL_WEAPONS:
            raise ValuesError(f"medals.weapons.{name}: not one of {', '.join(MEDAL_WEAPONS)}")
    weapon_lists = [out.words(f"medals.weapons.{name}", weapons[name]) if weapons.get(name) else "NULL"
                    for name in MEDAL_WEAPONS]
    medal_numbers = [
        _c_float(_number("medals.multi_kill_seconds", medals.get("multi_kill_seconds", 4))),
        str(_number("medals.perfection_kills", medals.get("perfection_kills", 15), integer=True)),
        _c_float(_number("medals.close_call_health", medals.get("close_call_health", 0.25))),
        str(_number("medals.history", medals.get("history", 8), integer=True)),
        _c_float(_number("medals.popup_seconds", medals.get("popup_seconds", 3))),
        "1" if medals.get("scaled", True) else "0",
    ]

    body = [
        "struct campaign_score_values const campaign_score_values =",
        "{",
        "\t{ " + ", ".join(_c_float(value) for value in difficulties) + " },",
        f"\t{len(bracket_rows)}, campaign_score_time_brackets,",
        f"\t{len(par_rows)}, {'campaign_score_par_times' if par_rows else 'NULL'},",
        "\t{ " + ", ".join(_c_float(value) for value in par_difficulties) + " },",
        f"\t{_c_float(daily_challenge_par)},",
        f"\t{style_combine},",
        *(f"\t{_c_float(value)}," for value in styles),
        f"\t{enemy_count}, {enemies},",
        f"\t{vehicle_count}, {vehicles},",
        *(f"\t{points}," for points in penalty_points),
        f"\t{ally_count}, {allies},",
        f"\t{ally_vehicle_count}, {ally_vehicles},",
        f"\t{skull_combine},",
        f"\t{_c_float(skull_default)},",
        f"\t{len(skull_rows)}, {'campaign_score_skull_values' if skull_rows else 'NULL'},",
        *(f"\t{value}," for value in medal_numbers),
        "\t{ " + ", ".join(weapon_lists) + " },",
        f"\t{len(medal_rows)}, {'campaign_score_medal_values' if medal_rows else 'NULL'},",
        "};",
    ]
    head = [
        "/*",
        "CAMPAIGN_SCORE_VALUES.C",
        "",
        "Written by tools/campaign_score_values.py from campaign_score_values.json:",
        "edit that, not this (configure.py, and so ninja, writes this again).",
        "*/",
        "",
        "#include <stddef.h>",
        "",
        '#include "campaign_score_values.h"',
        "",
    ]
    return "\n".join(head + out.lines + [""] + body) + "\n"


def write() -> bool:
    """the C file written, if it is not already what the JSON makes: TRUE
    when it was"""
    text = generate()
    if C_PATH.exists() and C_PATH.read_text() == text:
        return False
    C_PATH.write_text(text)
    return True


def main() -> int:
    try:
        if "--check" in sys.argv[1:]:
            if not C_PATH.exists() or C_PATH.read_text() != generate():
                print(f"{C_PATH} is not what {JSON_PATH.name} makes: run tools/campaign_score_values.py")
                return 1
            return 0
        print(f"{C_PATH}: {'written' if write() else 'unchanged'}")
    except ValuesError as error:
        print(f"campaign score values: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
