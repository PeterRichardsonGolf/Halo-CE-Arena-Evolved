#!/usr/bin/env python3
"""The weapon cache's id registry (tools/weapon_cache/registry.json): add, list, check, gen.

usage:
  registry.py list
  registry.py add --kind weapon|config|tag_set|grenade --key <k> [--display <D>] [--weapon <id>] [--catalogue-id <n>]
  registry.py check [--against <git ref>]      (rules of the design's 4.3; --against: append-only vs that ref's copy)
  registry.py gen [-o port/linux/game/ae_weapon_registry.inc]
Ids are assigned only here (4.3 rule 1): the next free one of the kind's range.
"""
import argparse
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
DEFAULT = os.path.join(HERE, "registry.json")
DEFAULT_INC = os.path.join(REPO, "port", "linux", "game", "ae_weapon_registry.inc")

# kind -> (first id, last id)
RANGES = {"weapon": (0x00000001, 0x00FFFFFF), "config": (0x01000001, 0x01FFFFFF), "tag_set": (0x02000001, 0x02FFFFFF),
          "grenade": (0x03000000, 0x03FFFFFF), "base": (0x04000001, 0x04000001)}
STATUSES = ("active", "held", "retired")
# where a weapon row's consumer columns come from (T0's "columns_from"): code, name_search, proposed
COLUMN_SOURCES = ("code", "name_search", "proposed")
MEDAL_ROLES = ("none", "pistol", "plasma_pistol", "assault_rifle", "smg", "battle_rifle", "evo_rifle", "dmr", "carbine",
               "plasma_rifle", "shotgun", "sniper", "rocket", "fuel_rod", "sword", "hammer", "needler", "other", "melee")
TRIGGER_FAMILIES = ("none", "battle_rifle", "smg", "smg_silenced", "carbine", "ar_silenced")
FALLBACKS = ("none", "assault_rifle", "pistol", "shotgun", "sniper_rifle", "rocket_launcher", "needler")
WEAPON_COLUMNS = ("feed", "bot", "medal_role", "trigger_family", "aliases", "stock_fallback", "menu_kind")
MENU_KINDS = ("PRECISION", "AUTOMATIC", "PISTOL", "LONG RANGE", "HEAVY", "MELEE", "CLOSE")
REPLACES = (None, "map_battle_rifle", "map_needler")
LEGACY_IDENTITY = 16  # ids 1..16 must have legacy_slot == id (4.3 rule 4)


def load(path=DEFAULT):
    with open(path) as handle:
        return json.load(handle)


def save(data, path=DEFAULT):
    with open(path, "w") as handle:
        json.dump(data, handle, indent=1)
        handle.write("\n")


def kind_of_id(value):
    for kind, (low, high) in RANGES.items():
        if low <= value <= high:
            return kind
    return None


def next_id(data, kind):
    low, high = RANGES[kind]
    used = [row["id"] for row in data["rows"] if row["kind"] == kind]
    # a weapon's first new id is above every id ever used; weapon 24 is the hammer's, so new ones start at 25
    value = max(used) + 1 if used else low
    if value > high:
        raise SystemExit(f"no free {kind} id")
    return value


def check_rows(data):
    """list of problems in a registry (rules 2-4 and the column rule), [] when it is sound"""
    problems = []
    rows = data.get("rows", [])
    ids, keys, slots = set(), set(), set()
    for row in rows:
        rid, kind, key = row.get("id"), row.get("kind"), row.get("key")
        where = f"row {rid} ({key})"
        if kind not in RANGES:
            problems.append(f"{where}: unknown kind {kind!r}")
            continue
        if kind_of_id(rid) != kind:
            problems.append(f"{where}: id outside the {kind} range")
        if rid in ids:
            problems.append(f"{where}: duplicate id")
        ids.add(rid)
        if (kind, key) in keys:
            problems.append(f"{where}: duplicate key within {kind}")
        keys.add((kind, key))
        if row.get("status") not in STATUSES:
            problems.append(f"{where}: bad status {row.get('status')!r}")
        slot = row.get("legacy_slot")
        if slot is not None:
            if not 1 <= slot <= 24 or slot in slots:
                problems.append(f"{where}: legacy_slot {slot} out of 1-24 or used twice")
            slots.add(slot)
        if kind == "weapon" and rid <= LEGACY_IDENTITY and slot != rid:
            problems.append(f"{where}: ids 1-{LEGACY_IDENTITY} must have legacy_slot equal to the id")
        if kind == "weapon":
            problems += [f"{where}: {p}" for p in check_weapon_columns(row)]
        if kind == "config" and "weapon" not in row:
            problems.append(f"{where}: a config names its weapon")
        if kind == "config" and not row.get("display"):
            problems.append(f"{where}: a config needs a display name (the picker's)")
        if kind == "grenade" and "catalogue_id" in row and rid != RANGES["grenade"][0] + row["catalogue_id"]:
            problems.append(f"{where}: grenade id must be 0x03000000 + catalogue_id")
    for row in rows:
        if row.get("kind") == "config":
            owner = [r for r in rows if r["id"] == row.get("weapon") and r["kind"] == "weapon"]
            if not owner or owner[0]["status"] != "active":
                problems.append(f"config {row['id']}: its weapon is not an active weapon row")
    return problems


def check_weapon_columns(row):
    """every weapon row has a value in every consumer column (4.2): nothing silently 0 or missing"""
    if row.get("status") != "active":
        return []
    problems = []
    for column in WEAPON_COLUMNS:
        if column not in row or row[column] in (None, "", {}) or (column != "aliases" and row[column] == []):
            problems.append(f"missing column {column}")
    if problems:
        return problems
    if not row["display"]:
        problems.append("empty display")
    feed = row["feed"]
    if not feed.get("word") or not feed.get("picture"):
        problems.append("feed needs word and picture")
    bot = row["bot"]
    if row.get("columns_from") not in COLUMN_SOURCES:
        problems.append("columns_from unknown")
    if not (bot.get("value", 0) > 0 and bot.get("range", 0) > 0):
        problems.append("bot value and range must be set")
    if bot.get("tap_ticks") is not None and not isinstance(bot["tap_ticks"], int):
        problems.append("bot.tap_ticks: a whole number or null (the rifle profile's)")
    for flag in ("precision", "scoped"):
        if not isinstance(bot.get(flag), bool):
            problems.append(f"bot.{flag} must be true or false")
    if row["medal_role"] not in MEDAL_ROLES:
        problems.append("medal_role unknown")
    if row["trigger_family"] not in TRIGGER_FAMILIES:
        problems.append("trigger_family unknown")
    if row["menu_kind"] not in MENU_KINDS:
        problems.append("menu_kind unknown")
    if row.get("replaces_map_own") not in REPLACES:
        problems.append("replaces_map_own unknown")
    if row["stock_fallback"] not in FALLBACKS:
        problems.append("stock_fallback unknown")
    if not isinstance(row["aliases"], list) or not all(re.fullmatch(r"[a-z0-9_]+", a) for a in row["aliases"]):
        problems.append("aliases: lower-case words")
    return problems


def check_append_only(old, new):
    """problems if new removes, renumbers or rekeys a row old had (rule 3)"""
    problems = []
    now = {row["id"]: row for row in new["rows"]}
    for row in old["rows"]:
        other = now.get(row["id"])
        if other is None:
            problems.append(f"id {row['id']} ({row['key']}) was removed: retire it instead")
        elif (other["key"], other["kind"]) != (row["key"], row["kind"]):
            problems.append(f"id {row['id']} changed from {row['kind']} {row['key']} to {other['kind']} {other['key']}")
        elif row["status"] == "retired" and other["status"] != "retired":
            problems.append(f"id {row['id']} was retired and is back")
    return problems


def git_copy(ref, path="tools/weapon_cache/registry.json"):
    out = subprocess.run(["git", "-C", REPO, "show", f"{ref}:{path}"], capture_output=True, text=True)
    if out.returncode:
        return None
    return json.loads(out.stdout)


def c_string(text):
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def generate(data):
    """the checked-in C table: one row macro per active or held weapon, one alias macro per layout word"""
    lines = ["/* GENERATED by tools/weapon_cache/registry.py gen from registry.json (revision %d): do not edit.\n"
             "tools/test_weapon_cache_registry.py holds this file equal to the registry. */\n" % data["revision"]]
    lines.append("/* id, key, display, legacy_slot (0 none), status, stock fallback, feed word, feed picture, bot value, bot range,")
    lines.append("bot tap ticks (-1: the rifle profile's), bot precision, bot scoped, columns_from, medal role, trigger family, power, replaces_map_own, menu kind */")
    weapons = sorted((r for r in data["rows"] if r["kind"] == "weapon"), key=lambda r: r["id"])
    for r in weapons:
        b = r["bot"]
        lines.append("AE_WEAPON_REGISTRY_ROW(%d, %s, %s, %d, %s, %s, %s, %s, %d, %.2ff, %d, %d, %d, %s, %s, %s, %d, %s, %s)" % (
            r["id"], c_string(r["key"]), c_string(r["display"]), r.get("legacy_slot") or 0, c_string(r["status"]),
            c_string(r["stock_fallback"]), c_string(r["feed"]["word"]), c_string(r["feed"]["picture"]), b["value"],
            b["range"], -1 if b["tap_ticks"] is None else b["tap_ticks"], int(b["precision"]), int(b["scoped"]),
            c_string(r["columns_from"]),
            c_string(r["medal_role"]), c_string(r["trigger_family"]), int(r["power"]),
            c_string(r["replaces_map_own"] or ""), c_string(r["menu_kind"])))
    for r in weapons:
        for word in [r["key"]] + [a for a in r["aliases"] if a != r["key"]]:
            lines.append("AE_WEAPON_REGISTRY_ALIAS(%d, %s)" % (r["id"], c_string(word)))
    others = sorted((r for r in data["rows"] if r["kind"] != "weapon"), key=lambda r: r["id"])
    lines.append("/* id, kind, key, status (tag sets, grenade types, configs, the base layer) */")
    for r in others:
        lines.append("AE_WEAPON_REGISTRY_ENTRY(0x%08X, %s, %s, %s)" % (r["id"], c_string(r["kind"]), c_string(r["key"]),
                                                                         c_string(r["status"])))
    return "\n".join(lines) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--registry", default=DEFAULT)
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("list")
    add = sub.add_parser("add")
    add.add_argument("--kind", required=True, choices=sorted(RANGES))
    add.add_argument("--key", required=True)
    add.add_argument("--display", default="")
    add.add_argument("--weapon", type=int)
    add.add_argument("--catalogue-id", type=int)
    check = sub.add_parser("check")
    check.add_argument("--against")
    gen = sub.add_parser("gen")
    gen.add_argument("-o", "--output", default=DEFAULT_INC)
    args = parser.parse_args(argv)
    data = load(args.registry)
    if args.command == "list":
        for row in sorted(data["rows"], key=lambda r: r["id"]):
            print(f"{row['id']:>10} 0x{row['id']:08X} {row['kind']:<8} {row['status']:<8} {row['key']}")
    elif args.command == "add":
        if any(r["kind"] == args.kind and r["key"] == args.key for r in data["rows"]):
            raise SystemExit("that key exists")
        row = {"id": next_id(data, args.kind), "kind": args.kind, "key": args.key,
               "display": args.display or args.key.upper(), "part": "main", "since": data.get("next_since", "0.7.6"),
               "status": "held"}
        if args.kind == "config":
            row["weapon"] = args.weapon
            row["overrides"] = {}
        if args.kind == "grenade":
            row["catalogue_id"] = args.catalogue_id
        if args.kind == "grenade" and args.catalogue_id is not None:
            row["id"] = RANGES["grenade"][0] + args.catalogue_id
        data["rows"].append(row)
        data["revision"] += 1
        save(data, args.registry)
        print(f"{row['id']} 0x{row['id']:08X}")
    elif args.command == "check":
        problems = check_rows(data)
        if args.against:
            old = git_copy(args.against)
            if old is None:
                print(f"note: no registry at {args.against}; append-only check skipped")
            else:
                problems += check_append_only(old, data)
        for p in problems:
            print("registry:", p)
        return 1 if problems else 0
    elif args.command == "gen":
        with open(args.output, "w") as handle:
            handle.write(generate(data))
    return 0


if __name__ == "__main__":
    sys.exit(main())
