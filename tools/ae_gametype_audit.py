#!/usr/bin/env python3
"""ae_gametype_audit: the gametype audit's report (debug.arena_audit's arena_audit.tsv; ae_gametype_audit.c).

    python3 tools/ae_gametype_audit.py arena_audit.tsv [-o REPORT.md] [--source ROOT]
    python3 tools/ae_gametype_audit.py --diff OLD.tsv NEW.tsv [-o DIFF.md]

The TSV has a row per seeded gametype ("ae": every field of its file's game_variant and PC options) and a row per
stock builder ("stock"). The report has, per engine, a table of every AE gametype by the fields that vary among them
or differ from their stock base, each cell marked:

    *  differs from the gametype's stock base (its build_game_variant_* in arena_gametypes.c's table)
    !  unlike its peers (one gametype of its group, the rest agreeing: same set, its COMP gametypes apart; same set
       and engine; a set's 2V2s)
    H  (on a column) hidden: the PC menus' gametype editor has no row for it (menu_functions.c's gametype_options,
       the AE rows drawn beside them, and the screens in port/assets/menus that show them)

and lists, first, the findings: non-competitive values (DEATH BONUS, KILL PENALTY, respawn growth, odd man out),
hidden fields away from their stock value, the peers' odd ones out, and each AE BR gametype unlike its AE twin
(the battle rifle start aside). --diff lists what changed between two
audits (two builds, or a save root before and after).
"""
import argparse
import csv
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
META = ["kind", "stored_name", "display_name", "set", "engine", "base", "file", "as_seeded"]
# fields never compared: the variant's own bookkeeping (playlist_profile.c's system default bit)
IGNORED = {"game_variant.flags"}
# shown without a gametype_options row: the game type itself (its edit screen)
ALWAYS_SHOWN = {"game_variant.game_engine_index"}
# the AE rows drawn beside gametype_options (menu_functions.c calls them; their files say which bits they edit)
AE_MENU_ROWS = {
    "ae_silent_menu_rows(": ["universal_variant.pad5.silent_movement", "universal_variant.pad5.silent_weapons"],
    "ae_hit_feedback_menu_rows(": ["universal_variant.pad5.hit_feedback"],
    "ae_rifle_menu_rows(": ["universal_variant.weapon_pack", "universal_variant.pack_loadout.primary",
                            "universal_variant.pack_loadout.secondary"],
}
# the vehicle screens (menu_functions.c's vehicles_update): each side's set and counts, the variant's set from red's
VEHICLE_SCREEN_MARK = "options->vehicle_counts["
VEHICLE_FIELDS_PREFIX = ("options.vehicle_set.", "options.vehicle_counts.")
# a gametype_options kind that edits more than its offset says
KIND_FIELDS = {
    "_option_health": ["universal_variant.health"],
    "_option_radar": ["universal_variant.flags.draw_object_in_motion_sensor"],
    "_option_nhe_mode": ["universal_variant.nhe_mode"],
    "_option_goal_radar": ["universal_variant.goal_radar", "universal_variant.flags.objective_in_sight"],
}
# game_engine.h's aliases of a flag bit's name
FLAG_ALIASES = {"health_regeneration": "health_style_first"}
# the battle rifle start (arena_gametypes.c's ARENA_BR_START): what an AE BR gametype has unlike its AE twin
BR_START = {"universal_variant.weapon_pack", "universal_variant.pack_loadout.primary",
            "universal_variant.pack_loadout.secondary", "universal_variant.pack_loadout.map_spawns",
            "options.loadout", "options.primary_weapon", "options.secondary_weapon"}
# (column, value that is not competitive, what it does) for the AE gametypes
NONCOMPETITIVE = [
    ("game_engine_variant.slayer.no_death_bonus", lambda v: v == "0",
     "DEATH BONUS on: each death makes the player faster (speed x1.1 per death, game_engine_slayer.c)"),
    ("game_engine_variant.slayer.no_kill_penalty", lambda v: v == "0",
     "KILL PENALTY on: each kill slows the killer (down to x0.9, game_engine_slayer.c)"),
    ("universal_variant.respawn_time_growth", lambda v: v not in ("", "0"),
     "RESPAWN TIME GROWTH: each death lengthens the next respawn (game_engine.c)"),
    ("universal_variant.odd_man_out", lambda v: v == "1",
     "ODD MAN OUT on"),
]


# ---------- reading

def read_tsv(path):
    """(columns, rows): rows as dicts by column name"""
    with open(path, newline="") as handle:
        reader = csv.reader(handle, delimiter="\t", quoting=csv.QUOTE_NONE)
        header = next(reader)
        rows = [dict(zip(header, line + [""] * (len(header) - len(line)))) for line in reader if line]
    return header, rows


def field_columns(header):
    return [c for c in header if c not in META and c not in IGNORED]


def seed_bases(source_root):
    """{stored name: stock builder name} from arena_gametypes.c's table (empty when the source is not there)"""
    path = Path(source_root) / "source" / "saved games" / "arena_gametypes.c"
    if not path.exists():
        return {}
    text = path.read_text(errors="replace")
    start = text.find("static struct arena_gametype const arena_gametypes[] =")
    if start < 0:
        return {}
    end = text.find("\n};", start)
    return {name: builder for name, builder in
            re.findall(r'\{\s*"([^"]+)",\s*build_game_variant_(\w+)', text[start:end])}


def _flag_name(token):
    name = token[len("_game_variant_"):-len("_bit")]
    return "universal_variant.flags." + FLAG_ALIASES.get(name, name)


def _mask_bits(header_text):
    """{MASK macro: [flag columns]} from game_engine.h's #defines of FLAG(...) sets"""
    masks = {}
    for match in re.finditer(r"#define\s+(GAME_VARIANT_\w+)\s+((?:[^\n]*\\\n)*[^\n]*)", header_text):
        flags = re.findall(r"FLAG\((_game_variant_\w+_bit)\)", match.group(2))
        if flags:
            masks[match.group(1)] = [_flag_name(f) for f in flags]
        else:
            # a mask made of another (GAME_VARIANT_NO_SPREAD_FULL = GAME_VARIANT_NO_SPREAD_MASK)
            other = re.findall(r"\b(GAME_VARIANT_\w+)\b", match.group(2))
            if other and other[0] in masks:
                masks[match.group(1)] = masks[other[0]]
    return masks


def shown_fields(source_root):
    """the columns the PC menus' gametype editor shows a row for, or None when the source is not there"""
    menu = Path(source_root) / "port" / "linux" / "game" / "menu_functions.c"
    engine_h = Path(source_root) / "source" / "game" / "game_engine.h"
    if not menu.exists() or not engine_h.exists():
        return None
    text = menu.read_text(errors="replace")
    masks = _mask_bits(engine_h.read_text(errors="replace"))
    start = text.find("static struct gametype_option const gametype_options[] =")
    if start < 0:
        return None
    end = text.find("\n};", start)
    table = text[start:end]
    screens = Path(source_root) / "port" / "assets" / "menus"
    screen_text = "\n".join(p.read_text(errors="replace") for p in screens.rglob("*.xml")) if screens.exists() else None

    shown = set(ALWAYS_SHOWN)
    # each row: { "spinner", _option_kind, ... } up to the next row's start
    rows = re.split(r'\n\s*\{\s*(?=")', table)
    for row in rows[1:]:
        head = re.match(r'"([^"]+)",\s*(_option_\w+),', row)
        if not head:
            continue
        spinner, kind = head.groups()
        if kind == "_option_setting":
            continue
        # (an instance's name is its definition's cut to 31 characters)
        if screen_text is not None and spinner[:31] not in screen_text:
            continue
        for field in re.findall(r"VARIANT_FIELD\(([\w.]+)\)", row):
            shown.add(field)
        for field in re.findall(r"OPTIONS_FIELD\(([\w.]+)\)", row):
            shown.add("options." + field)
        for flag in re.findall(r"FLAG\((_game_variant_\w+_bit)\)", row):
            shown.add(_flag_name(flag))
        for mask in re.findall(r"\b(GAME_VARIANT_\w+_MASK)\b", row):
            shown.update(masks.get(mask, []))
        shown.update(KIND_FIELDS.get(kind, []))
    for call, fields in AE_MENU_ROWS.items():
        if call in text:
            shown.update(fields)
    if VEHICLE_SCREEN_MARK in text:
        shown.add("universal_variant.vehicle_set")
        shown.add("*vehicles")
    return shown


def is_shown(column, shown):
    if shown is None:
        return True
    if column in shown:
        return True
    return "*vehicles" in shown and column.startswith(VEHICLE_FIELDS_PREFIX)


# ---------- analysis

def column_engine(column):
    match = re.match(r"game_engine_variant\.(\w+)\.", column)
    return match.group(1) if match else None


def assign_bases(rows, stock, bases):
    """each AE row's stock base: the table's builder, else the stock row of its engine nearest to it"""
    for row in rows:
        name = bases.get(row["stored_name"])
        if name and name in stock:
            row["base"] = name
            continue
        candidates = [s for s in stock.values() if s["engine"] == row["engine"]]
        if not candidates or row["file"] != "file":
            continue
        best = min(candidates, key=lambda s: sum(1 for c in s if c not in META and s[c] != row.get(c, "")))
        row["base"] = best["stored_name"] + "?"


def peer_set(row):
    """a row's set, its COMP gametypes apart (AE BR's: "TEAM AE BR COMP SLAYER")"""
    words = row["display_name"].upper().split()
    if "COMP" in words and "COMP" not in row["set"].upper().split():
        return row["set"] + " COMP"
    return row["set"]


def peer_groups(rows):
    """{key: rows}: a set's gametypes (compared on the fields not of an engine), a set's of one engine, a set's
    2V2s"""
    groups = defaultdict(list)
    for row in rows:
        if row["file"] != "file":
            continue
        groups[("set", peer_set(row))].append(row)
        groups[("set+engine", peer_set(row), row["engine"])].append(row)
        if "2V2" in row["stored_name"].upper() or "2V2" in row["display_name"].upper():
            groups[("2V2", peer_set(row), row["engine"])].append(row)
    return groups


def twins(rows, columns):
    """[(row, twin, [(column, value, twin's value)])]: each AE BR gametype against the gametype of its name without
    "BR" (TEAM AE BR COMP SLAYER and TEAM AE COMP SLAYER), on every field but the battle rifle start's"""
    by_display = {r["display_name"].upper(): r for r in rows if r["file"] == "file"}
    found = []
    for row in rows:
        words = row["display_name"].upper().split()
        if "BR" not in words or row["file"] != "file":
            continue
        twin = by_display.get(" ".join(w for w in words if w != "BR"))
        if twin is None:
            continue
        changed = [(c, row.get(c, ""), twin.get(c, "")) for c in columns
                   if c not in BR_START and row.get(c, "") != twin.get(c, "")]
        found.append((row, twin, changed))
    return found


def find_outliers(rows, columns):
    """[(row, column, value, peers' value, group label, peer count)]: the one row of a group of 3+ that differs where
    the rest agree"""
    found = {}
    for key, members in peer_groups(rows).items():
        if len(members) < 3:
            continue
        for column in columns:
            if key[0] == "set" and column_engine(column):
                continue
            values = Counter(m.get(column, "") for m in members)
            if len(values) != 2:
                continue
            (common, count), (odd, odd_count) = values.most_common()
            if odd_count != 1:
                continue
            row = next(m for m in members if m.get(column, "") == odd)
            label = " / ".join(k or "(none)" for k in key[1:])
            if key[0] == "2V2":
                label += " 2V2s"
            # (the narrowest group that shows it)
            mark = (row["stored_name"], column)
            if mark not in found or count < found[mark][5]:
                found[mark] = (row, column, odd, common, label, count)
    return list(found.values())


def analyse(header, rows, source_root):
    columns = field_columns(header)
    stock = {r["stored_name"]: r for r in rows if r["kind"] == "stock"}
    ae = [r for r in rows if r["kind"] == "ae"]
    assign_bases(ae, stock, seed_bases(source_root))
    shown = shown_fields(source_root)

    differs = {}
    for row in ae:
        base = stock.get(row["base"].rstrip("?"))
        for column in columns:
            if base is not None and row["file"] == "file" and row.get(column, "") != base.get(column, ""):
                differs[(row["stored_name"], column)] = base.get(column, "")
    outliers = find_outliers(ae, columns)
    odd = {(o[0]["stored_name"], o[1]) for o in outliers}

    noncompetitive = []
    for column, test, text in NONCOMPETITIVE:
        hit = [r for r in ae if r["file"] == "file" and test(r.get(column, ""))]
        if hit:
            noncompetitive.append((column, text, hit))
    hidden_changed = defaultdict(list)
    for (name, column), base_value in differs.items():
        if not is_shown(column, shown):
            hidden_changed[column].append(name)
    return {
        "columns": columns, "stock": stock, "ae": ae, "shown": shown, "differs": differs, "outliers": outliers,
        "odd": odd, "noncompetitive": noncompetitive, "hidden_changed": hidden_changed,
        "twins": twins(ae, columns),
    }


# ---------- the report

def _names(rows):
    return ", ".join(f"`{r['stored_name']}`" for r in rows)


def _cell(text):
    return text.replace("|", "\\|") if text else ""


def engine_table(engine, result):
    rows = [r for r in result["ae"] if r["engine"] == engine]
    if not rows:
        return []
    columns = []
    for column in result["columns"]:
        owner = column_engine(column)
        if owner and owner != engine:
            continue
        values = {r.get(column, "") for r in rows}
        marked = any((r["stored_name"], column) in result["differs"] or (r["stored_name"], column) in result["odd"]
                     for r in rows)
        if len(values) > 1 or marked:
            columns.append(column)
    bases = sorted({r["base"].rstrip("?") for r in rows if r["base"]})
    head = ["gametype", "set", "base"] + [
        c.replace("universal_variant.", "u.").replace("game_engine_variant.", "")
        + (" H" if not is_shown(c, result["shown"]) else "") for c in columns]
    lines = [f"### {engine} ({len(rows)} gametypes)", "",
             "| " + " | ".join(head) + " |", "|" + "---|" * len(head)]
    for row in rows:
        cells = [f"`{row['stored_name']}`", row["set"], row["base"]]
        if row["file"] != "file":
            cells += ["(no file)"] * len(columns)
        else:
            for column in columns:
                mark = ("*" if (row["stored_name"], column) in result["differs"] else "") + \
                    ("!" if (row["stored_name"], column) in result["odd"] else "")
                cells.append(_cell(row.get(column, "")) + mark)
        lines.append("| " + " | ".join(cells) + " |")
    for name in bases:
        base = result["stock"].get(name)
        if base is None:
            continue
        cells = [f"_stock {name}_", "", ""] + [_cell(base.get(c, "")) for c in columns]
        lines.append("| " + " | ".join(cells) + " |")
    lines.append("")
    return lines


def report(header, rows, source_root, title="arena_audit.tsv"):
    result = analyse(header, rows, source_root)
    ae = result["ae"]
    lines = ["# Gametype audit", "",
             f"From `{title}`: {len(ae)} seeded gametypes "
             f"({sum(1 for r in ae if r['file'] != 'file')} without a file, "
             f"{sum(1 for r in ae if r['as_seeded'] == '0')} edited since seeding), "
             f"{len(result['stock'])} stock builders, {len(result['columns'])} fields each.", "",
             "Marks: `*` differs from its stock base, `!` unlike its peers, `H` (column) hidden: no row in the PC "
             "menus' gametype editor." + ("" if result["shown"] is not None else
                                          " (The source was not found: nothing is marked hidden.)"), "",
             "## Findings", "", "### Non-competitive values", ""]
    if not result["noncompetitive"]:
        lines.append("None.")
    for column, text, hit in result["noncompetitive"]:
        hidden = "" if is_shown(column, result["shown"]) else " (hidden)"
        lines.append(f"- **{text}** (`{column}`{hidden}), {len(hit)}: {_names(hit)}")
    lines += ["", "### Hidden fields away from their stock value", ""]
    if not result["hidden_changed"]:
        lines.append("None.")
    for column, names in sorted(result["hidden_changed"].items()):
        values = Counter(next(r for r in ae if r["stored_name"] == n).get(column, "") for n in names)
        summary = ", ".join(f"{v or '(empty)'} x{c}" for v, c in values.most_common())
        lines.append(f"- `{column}` ({summary}): " + ", ".join(f"`{n}`" for n in names))
    lines += ["", "### Unlike their peers", ""]
    if not result["outliers"]:
        lines.append("None.")
    for row, column, value, common, label, count in sorted(result["outliers"],
                                                           key=lambda o: (o[4], o[0]["stored_name"], o[1])):
        hidden = " (hidden)" if not is_shown(column, result["shown"]) else ""
        lines.append(f"- {label}: `{row['stored_name']}` has `{column}` = {value or '(empty)'}{hidden}, "
                     f"its {count} peers {common or '(empty)'}")
    lines += ["", "### AE BR unlike its AE twin", "",
              "Each AE BR gametype against the one of its name without BR, the battle rifle start aside."]
    for row, twin, changed in result["twins"]:
        if changed:
            lines.append(f"- `{row['stored_name']}` vs `{twin['stored_name']}`: " + ", ".join(
                f"`{c}` {a or '(empty)'} (twin {b or '(empty)'})" for c, a, b in changed))
    if not any(changed for _, _, changed in result["twins"]):
        lines.append("None differ.")
    missing = [r for r in ae if r["file"] != "file"]
    if missing:
        lines += ["", "### Without a file", "", _names(missing)]
    lines += ["", "## By engine", ""]
    for engine in sorted({r["engine"] for r in ae if r["engine"]}):
        lines += engine_table(engine, result)
    if result["shown"] is not None:
        hidden = [c for c in result["columns"] if not is_shown(c, result["shown"])]
        lines += ["## Hidden fields", "", "The PC menus' gametype editor shows no row for: " +
                  ", ".join(f"`{c}`" for c in hidden) + ".", ""]
    return "\n".join(lines) + "\n"


def diff(old_path, new_path):
    old_header, old_rows = read_tsv(old_path)
    new_header, new_rows = read_tsv(new_path)
    old = {(r["kind"], r["stored_name"]): r for r in old_rows}
    new = {(r["kind"], r["stored_name"]): r for r in new_rows}
    lines = ["# Gametype audit: changes", "", f"`{old_path}` -> `{new_path}`", ""]
    added_columns = [c for c in new_header if c not in old_header]
    removed_columns = [c for c in old_header if c not in new_header]
    if added_columns:
        lines.append("Fields added: " + ", ".join(f"`{c}`" for c in added_columns))
    if removed_columns:
        lines.append("Fields removed: " + ", ".join(f"`{c}`" for c in removed_columns))
    for label, keys in (("Gametypes added", [k for k in new if k not in old]),
                        ("Gametypes removed", [k for k in old if k not in new])):
        if keys:
            lines.append(f"{label}: " + ", ".join(f"`{k[1]}` ({k[0]})" for k in keys))
    columns = [c for c in new_header if c in old_header and c != "as_seeded"]
    changes = 0
    for key, row in new.items():
        before = old.get(key)
        if before is None:
            continue
        changed = [(c, before.get(c, ""), row.get(c, "")) for c in columns if before.get(c, "") != row.get(c, "")]
        if changed:
            lines += ["", f"## `{key[1]}` ({key[0]})", ""]
            lines += [f"- `{c}`: {a or '(empty)'} -> {b or '(empty)'}" for c, a, b in changed]
            changes += len(changed)
    lines += ["", f"{changes} fields changed."]
    return "\n".join(lines) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("audit", nargs="?", help="arena_audit.tsv")
    parser.add_argument("--diff", nargs=2, metavar=("OLD", "NEW"), help="compare two audits")
    parser.add_argument("-o", "--output", help="write the Markdown here (else print it)")
    parser.add_argument("--source", default=str(ROOT),
                        help="the source tree (arena_gametypes.c's bases, the menus' rows; default: this one)")
    args = parser.parse_args(argv)
    if args.diff:
        text = diff(*args.diff)
    elif args.audit:
        header, rows = read_tsv(args.audit)
        text = report(header, rows, args.source, Path(args.audit).name)
    else:
        parser.error("an audit TSV or --diff OLD NEW")
    if args.output:
        Path(args.output).write_text(text)
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
