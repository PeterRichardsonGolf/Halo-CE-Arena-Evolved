#!/usr/bin/env python3
r"""ae_gametype_change: one command for a change to AE's seeded gametypes (a new seed revision).

    python3 tools/ae_gametype_change.py --gametypes "AE FFA SNPR" \
        --set universal_variant.respawn_time_growth=0 --why "..." [--decided "owner, 2026-10-20"] [--dry-run]
    python3 tools/ae_gametype_change.py --gametypes "AE BR&slayer" --set score_to_win=30 --why "..."
    python3 tools/ae_gametype_change.py --gametypes "AE COMP" --adjust no_odd_man_out --why "..."
    python3 tools/ae_gametype_change.py --list "AE&team"            (what a selection picks, nothing else)

What it changes (--set, repeatable, and --adjust, repeatable):

  a row field     struct arena_gametype's own fields, by name: score_to_win, time_limit, respawn_time,
                  suicide_penalty, radar, vehicle_set, custom_loadout, weapon_set, loadout_primary,
                  loadout_secondary, no_map_weapons, friendly_fire, goal_radar, nhe_mode, starting_frags,
                  ball_melee, hit_feedback, weapon_pack, pack_loadout, flags. The value is C, as the table
                  writes it (score_to_win=30, respawn_time=ARENA_COMPETITIVE_RESPAWN, time_limit=0).
  a variant field the gametype audit's column names (port/linux/game/ae_gametype_audit.c, tools/
                  ae_gametype_audit.py): universal_variant.*, universal_variant.flags.<bit>,
                  game_engine_variant.<engine>.*, options.*. These are set by an adjust function (the row's
                  .adjust), never by editing a shipped one: the tool writes a new one, which first calls the
                  row's old adjust (if any) and then sets the fields; one that exists with the same body is
                  reused.
  --adjust NAME   an existing adjust (arena_gametype_NAME, e.g. no_speed_changes) or a preset
                  (no_respawn_growth, no_odd_man_out): the same as its --set lines.

Which gametypes (--gametypes, repeatable; "&" intersects): a stored name ("AE FFA SNPR"), a set of
arena_gametype_names.c ("AE", "AE BR", "AE COMP", "OCTAGON", "NHE", "TRAINING"), an engine ("slayer", "ctf",
"oddball", "king", "race"), "team" or "ffa". An AE BR gametype and its AE twin always change together (the self-check
holds them equal but for the battle rifle start): a missing twin is added, and said so (--no-twins: not, for a change
to the AE one's start).

What it does (without --dry-run, on the build box: --box HOST, default the ae_test config's box.ssh; tree
--box-tree, default ~/ae-gtchange; results under --box-results):
  1. copies this tree to the box tree, builds linux64, seeds a fresh save root (a hosted bloodgulch game with
     HALO_ARENA_AUDIT=1, as the golden files were made) and reads every seed's block hash (seed_compare.py) and
     its audit (z/arena_audit.tsv). The hashes must equal the committed golden_seeds_rev<current>.json's (else it
     stops: the pipeline changed); they are the migrations' old_block_hash;
  2. edits source/saved games/arena_gametypes.c: ARENA_GAMETYPES_REVISION + 1, the new adjust function(s), the
     table rows, one same-name migration per changed gametype (old_row the current row, new_row the new one, in
     designated form; old_block_hash from step 1), a line in the migrations' comment; and tools/ae_test/
     seed_rules.py's expectations (the fields it knows how to say; the rest are listed for a person);
  3. builds the edit on the box, seeds a fresh root (its blocks: golden_seeds_rev<new>.json), and checks: the
     self-check says nothing; only the chosen gametypes' files differ from step 1's, in the same folders; a
     step-1 root with an edited copy of a changed gametype and a player's own copy of one migrates as the game
     promises (the unedited ones updated in place, backed up, then the same as a fresh root's; the edited one
     and the player's own byte-identical; every other file untouched); each changed gametype hosted once
     passes seed_rules (--no-rules-check skips it);
  4. only when every check passes: writes the edits here, golden_seeds_rev<new>.json, and the change into
     tools/ae_test/gametype_changes.json (what tools/test_ae_gametype_change.py checks the committed state
     against), and prints the plan, the audit diff (ae_gametype_audit.py --diff, old seeds -> new seeds) and the
     checks. Its reports are in the ae_test out_dir, gametype_change/<stamp>.

--dry-run prints the plan, the C it would write, and the expected audit diff (the values the source says; with
--audit-from TSV the old ones too); with --box it also runs steps 1 and 3 on the box (nothing is written here).

Still a person's: the decision and its --why, the CHANGELOG line, the commit, and any field seed_rules has no
words for (listed). A change to the battle rifle start's fields (ARENA_BR_START) is refused: edit it by hand.
The box-side steps are this script's "_box" subcommand (run there by the steps above, not by hand).
"""
import argparse
import datetime
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import textwrap
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TEST_DIR = ROOT / "tools" / "ae_test"
sys.path.insert(0, str(TEST_DIR))
sys.path.append(str(ROOT / "tools"))  # (after ae_test: tools/harness is a package of its own)

SOURCE = Path("source/saved games/arena_gametypes.c")
NAMES = Path("source/saved games/arena_gametype_names.c")
AUDIT_C = Path("port/linux/game/ae_gametype_audit.c")
SEED_RULES = Path("tools/ae_test/seed_rules.py")
MANIFEST = Path("tools/ae_test/gametype_changes.json")
GOLDEN = "tools/ae_test/golden_seeds_rev{}.json"
TOOL = "tools/ae_gametype_change.py"

WIDTH = 120
TICKS_PER_SECOND = 30
SETS = {"AE": "ae", "AE BR": "ae_br", "AE COMP": "ae_comp", "OCTAGON": "octagon", "NHE": "nhe", "TRAINING": "training"}
ENGINES = ("ctf", "slayer", "oddball", "king", "race")
# --adjust presets: their --set lines
PRESETS = {
    "no_respawn_growth": [("universal_variant.respawn_time_growth", "0")],
    "no_odd_man_out": [("universal_variant.odd_man_out", "0")],
}
# the game's own constants an expression may use (beyond arena_gametypes.c's enums and defines)
CONSTANTS = {"TICKS_PER_SECOND": TICKS_PER_SECOND, "TRUE": 1, "FALSE": 0, "NONE": -1}
# the hosted game the seeds come from (the golden files' recipe)
SEED_SPEC = {"map": "bloodgulch", "start": 5, "exit_after": 20}
RULES_SPEC = {"map": "bloodgulch", "bots": 2, "start": 6, "exit_after": 28}
# a saved gametype's score_to_win byte (universal_variant at 0x1c, score_to_win 0x24 into it): an "edited" copy
SCORE_OFFSET = 0x40


def read(path):
    return Path(path).read_text(encoding="latin-1")


def write(path, text):
    Path(path).write_text(text, encoding="latin-1")


def die(message):
    raise SystemExit(f"ae_gametype_change: {message}")


def squash(text):
    return re.sub(r"\s+", " ", text).strip()


def visual_len(line):
    return len(line.expandtabs(4))


# ---------------------------------------------------------------------------------------------- C parsing

def strip_comments(text):
    return re.sub(r"/\*.*?\*/", lambda m: " " * len(m[0]), text, flags=re.S)


def matching(text, start):
    """the index of the bracket closing text[start] (comments and strings skipped)"""
    pairs = {"{": "}", "(": ")"}
    depth = 0
    i = start
    in_string = False
    while i < len(text):
        c = text[i]
        if in_string:
            if c == "\\":
                i += 2
                continue
            if c == '"':
                in_string = False
        elif text.startswith("/*", i):
            i = text.index("*/", i) + 2
            continue
        elif c == '"':
            in_string = True
        elif c in pairs:
            depth += 1
        elif c in ")}":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    die(f"no closing bracket for {text[start:start + 40]!r}")


def split_items(text, base=0):
    """the spans (absolute: + base) of an initializer's top-level comma-separated items, whitespace trimmed"""
    spans = []
    depth = 0
    start = 0
    in_string = False
    i = 0
    while i < len(text):
        c = text[i]
        if in_string:
            if c == "\\":
                i += 2
                continue
            if c == '"':
                in_string = False
        elif c == '"':
            in_string = True
        elif c in "({":
            depth += 1
        elif c in ")}":
            depth -= 1
        elif c == "," and depth == 0:
            spans.append((start, i))
            start = i + 1
        i += 1
    spans.append((start, len(text)))
    out = []
    for s, e in spans:
        seg = text[s:e]
        if seg.strip():
            lead = len(seg) - len(seg.lstrip())
            out.append((base + s + lead, base + s + len(seg.rstrip())))
    return out


def struct_fields(source):
    """struct arena_gametype's fields, in order"""
    m = re.search(r"struct arena_gametype\n\{(.*?)\n\};", source, re.S)
    body = re.sub(r"/\*.*?\*/", "", m[1], flags=re.S)
    fields = []
    for decl in body.split(";"):
        decl = decl.strip()
        if not decl:
            continue
        f = re.search(r"\(\*(\w+)\)", decl)
        fields.append(f[1] if f else re.search(r"(\w+)\s*(?:\[[^\]]*\])?$", decl)[1])
    return fields


def macro_fields(source):
    """{macro: {field: value}} for the #defines whose body is designated initialisers (ARENA_BR_START)"""
    out = {}
    for m in re.finditer(r"^#define (\w+)[ \t]*\\\n((?:.*\\\n)*.*)$", source, re.M):
        body = m[2].replace("\\\n", " ")
        items = [body[s:e] for s, e in split_items(body)]
        if items and all(re.match(r"\.\w+\s*=", it) for it in items):
            out[m[1]] = {re.match(r"\.(\w+)", it)[1]: squash(it.split("=", 1)[1]) for it in items}
    return out


class Item:
    """one initializer item of a row: a positional or designated field, or a macro of designated ones"""

    def __init__(self, kind, field, value, span, value_span):
        self.kind, self.field, self.value, self.span, self.value_span = kind, field, value, span, value_span

    def designated(self):
        return f".{self.field} = {self.value}" if self.kind != "macro" else self.value


class Row:
    def __init__(self, source, start, end, fields, macros):
        """source[start] is the row's '{', source[end] its '}'"""
        self.start, self.end = start, end
        body = source[start + 1:end]
        if "/*" in body:
            die(f"a comment inside a row: {squash(body)[:60]}")
        self.items = []
        positional = 0
        for s, e in split_items(body, start + 1):
            text = source[s:e]
            m = re.match(r"\.(\w+)\s*=\s*", text, re.S)
            if m:
                self.items.append(Item("designated", m[1], squash(text[m.end():]), (s, e), (s + m.end(), e)))
            elif squash(text) in macros or (self.items and any(i.kind != "positional" for i in self.items)):
                # (a macro of designated fields right after the positional ones, too: the OCTAGON drills'
                # ARENA_OCTAGON_BR_PRIMARY)
                self.items.append(Item("macro", None, squash(text), (s, e), (s, e)))
            else:
                self.items.append(Item("positional", fields[positional], squash(text), (s, e), (s, e)))
                positional += 1
        self.values = {}
        self.macro_values = {}
        for item in self.items:
            if item.kind == "macro":
                for k, v in macros.get(item.value, {}).items():
                    self.macro_values[k] = (item.value, v)
            else:
                self.values[item.field] = item.value
        self.name = self.values["name"].strip('"')

    def item(self, field):
        return next((i for i in self.items if i.field == field), None)

    def value(self, field):
        if field in self.values:
            return self.values[field]
        if field in self.macro_values:
            return self.macro_values[field][1]
        return None

    def designated_items(self):
        return [i.designated() for i in self.items]


def wrap_items(prefix, items, suffix, continuation, width=WIDTH):
    """items joined by ", " after prefix, wrapped at width (tabs as 4) with continuation lines"""
    pieces = [it + "," for it in items[:-1]] + [items[-1] + suffix]
    lines = []
    current = prefix + pieces[0]
    for piece in pieces[1:]:
        if visual_len(current + " " + piece) <= width:
            current += " " + piece
        else:
            lines.append(current)
            current = continuation + piece
    lines.append(current)
    return "\n".join(lines)


def block_after(source, marker):
    """(start, end) of the initializer after marker: its '{' and its '}'"""
    at = source.index(marker)
    open_at = source.index("{", at)
    return open_at, matching(source, open_at)


def rows_in(source, start, end, fields, macros):
    """the rows ({ ... } at depth one) of the initializer source[start..end]"""
    rows = []
    i = start + 1
    while i < end:
        if source.startswith("/*", i):
            i = source.index("*/", i) + 2
            continue
        if source[i] == "{":
            close = matching(source, i)
            rows.append(Row(source, i, close, fields, macros))
            i = close + 1
            continue
        i += 1
    return rows


def adjust_functions(source):
    """{function name: [statements]} of the adjusts defined in arena_gametypes.c ((void)options; and return; left
    out)"""
    out = {}
    for m in re.finditer(r"^static void (arena_gametype_\w+)\(\n\tstruct game_variant \*variant,\n\tstruct "
                         r"game_variant_options \*options\)\n\{\n(.*?)^\}\n", source, re.M | re.S):
        statements = [squash(s) + ";" for s in strip_comments(m[2]).split(";") if squash(s)]
        out[m[1]] = [s for s in statements if s not in ("(void)options;", "return;")]
    return out


def audit_columns(audit_source):
    """{column: (owner, member path or flag bit, kind)} of ae_gametype_audit.c's table: owner variant/options/flag"""
    out = {}
    for m in re.finditer(r'\bVC\("([^"]+)",\s*([\w.]+),\s*(\w+)\)', audit_source):
        out[m[1]] = ("variant", m[2], m[3])
    for m in re.finditer(r'\bEC\(game_engine_(\w+),\s*"([^"]+)",\s*([\w.]+),\s*(\w+)\)', audit_source):
        out[m[2]] = ("variant", m[3], m[4])
    for m in re.finditer(r'\bOC\("([^"]+)",\s*([\w.]+),\s*(\w+)\)', audit_source):
        out[m[1]] = ("options", m[2], m[3])
    for m in re.finditer(r'\bFLAG_BIT\("(\w+)",\s*(\w+)\)', audit_source):
        out["universal_variant.flags." + m[1]] = ("flag", m[2], "_audit_bits_long")
    return out


def constants(source):
    """{name: int} of arena_gametypes.c's enum entries and number #defines that evaluate"""
    raw = {}
    for m in re.finditer(r"^\t(\w+) = ([^,\n]+),", strip_comments(source), re.M):
        raw[m[1]] = m[2]
    for m in re.finditer(r"^#define (\w+) ([^\\\n]+)$", source, re.M):
        raw.setdefault(m[1], m[2])
    known = dict(CONSTANTS)
    for _ in range(4):
        for name, expr in raw.items():
            if name not in known:
                value = evaluate(expr, known)
                if value is not None:
                    known[name] = value
    return known


def evaluate(expr, known):
    """a C integer expression's value from known names (ARENA_SET(x) as x + 1), else None"""
    text = re.sub(r"\bARENA_SET\(", "(1 + ", expr.strip())
    text = re.sub(r"\b(0[xX][0-9a-fA-F]+|\d+)[uU]?[lL]*\b", lambda m: str(int(m[1], 16) if m[1][:2].lower() == "0x" else int(m[1])), text)
    names = set(re.findall(r"[A-Za-z_]\w*", text))
    if not names <= set(known) or re.search(r"[^\w\s()+\-*/<>|&~]", text):
        return None
    try:
        return int(eval(text.replace("/", "//"), {"__builtins__": {}}, {n: known[n] for n in names}))
    except Exception:  # noqa: BLE001 (anything that does not evaluate is shown as written)
        return None


class Source:
    """arena_gametypes.c and what the tool reads from it"""

    def __init__(self, text, names_text, audit_text):
        self.text = text
        self.fields = struct_fields(text)
        self.macros = macro_fields(text)
        self.revision = int(re.search(r"ARENA_GAMETYPES_REVISION = (\d+),", text)[1])
        start, end = block_after(text, "static struct arena_gametype const arena_gametypes[] =")
        self.table_span = (start, end)
        self.rows = rows_in(text, start, end, self.fields, self.macros)
        self.by_name = {r.name: r for r in self.rows}
        mig = text[text.index("arena_gametype_migrations[] ="):text.index("arena_gametype_test_migration =")]
        self.migration_revisions = [int(n) for n in re.findall(r"\.revision = (\d+),", mig)]
        self.twins = dict(re.findall(r'\{ "(AE BR [^"]+)", "([^"]+)" \},', text))
        self.adjusts = adjust_functions(text)
        self.constants = constants(text)
        self.catalog = {m[1]: {"set": m[2], "engine": m[3], "team": m[4] == "TRUE"} for m in re.finditer(
            r'\{ "([^"]+)", "[^"]*", _arena_gametype_set_(\w+), game_engine_(\w+), (TRUE|FALSE)', names_text)}
        self.columns = audit_columns(audit_text)

    @classmethod
    def load(cls, root=ROOT):
        return cls(read(root / SOURCE), read(root / NAMES), read(root / AUDIT_C))


# ---------------------------------------------------------------------------------------------- the plan

def select(source, tokens):
    """the table's names the selection tokens pick, in table order"""
    picked = set()
    upper = {n.upper(): n for n in source.by_name}
    for token in tokens:
        hit = None
        for part in (p.strip() for p in token.split("&")):
            if part.upper() in upper:
                names = {upper[part.upper()]}
            elif part.upper() in SETS:
                names = {n for n in source.by_name if source.catalog.get(n, {}).get("set") == SETS[part.upper()]}
            elif part.lower() in ENGINES:
                names = {n for n in source.by_name if source.catalog.get(n, {}).get("engine") == part.lower()}
            elif part.lower() in ("team", "teams", "ffa"):
                team = part.lower() != "ffa"
                names = {n for n in source.by_name if source.catalog.get(n, {}).get("team") == team}
            else:
                die(f"'{part}' is no gametype, set ({', '.join(SETS)}), engine ({', '.join(ENGINES)}), team or ffa")
            hit = names if hit is None else hit & names
        if not hit:
            die(f"'{token}' picks no gametype")
        picked |= hit
    return [r.name for r in source.rows if r.name in picked]


def add_twins(source, names):
    """names with every AE BR twin pair completed, and the ones added"""
    pairs = list(source.twins.items())
    out = list(names)
    added = []
    for br, ae in pairs:
        for have, other in ((br, ae), (ae, br)):
            if have in out and other not in out:
                out.append(other)
                added.append(other)
    order = [r.name for r in source.rows]
    return sorted(out, key=order.index), added


def parse_sets(source, sets, adjusts):
    """(row changes {field: value}, variant steps [("call", function) | ("set", column, value)])"""
    row = {}
    steps = []
    row_fields = [f for f in source.fields if f not in ("name", "build", "adjust")]
    for name in adjusts:
        if f"arena_gametype_{name}" in source.adjusts:
            steps.append(("call", f"arena_gametype_{name}"))
        elif name in PRESETS:
            steps += [("set", c, v) for c, v in PRESETS[name]]
        else:
            die(f"--adjust {name}: no arena_gametype_{name} in arena_gametypes.c and no preset "
                f"({', '.join(PRESETS)})")
    for text in sets:
        key, sep, value = text.partition("=")
        key, value = key.strip(), value.strip()
        if not sep or not value:
            die(f"--set {text}: want FIELD=VALUE")
        if key.startswith("row."):
            key = key[4:]
        if "." not in key:
            if key not in row_fields:
                die(f"--set {key}: no such row field ({', '.join(row_fields)})")
            row[key] = value
        else:
            if key not in source.columns:
                die(f"--set {key}: no such audit column (ae_gametype_audit.c)")
            owner, _, kind = source.columns[key]
            if kind == "_audit_bits_byte":
                die(f"--set {key}: a bit field of a byte: use the row field instead")
            steps.append(("set", key, value))
    if not row and not steps:
        die("nothing to change: give --set or --adjust")
    return row, steps


def statement(source, column, value):
    owner, path, kind = source.columns[column]
    if kind in ("_audit_boolean", "_audit_bits_long"):
        value = {"1": "TRUE", "0": "FALSE", "on": "TRUE", "off": "FALSE", "true": "TRUE",
                 "false": "FALSE"}.get(value.lower(), value)
    if owner == "flag":
        return f"SET_FLAG(variant->universal_variant.flags, {path}, {value});"
    return f"{'options' if owner == 'options' else 'variant'}->{path} = {value};"


def step_statements(source, steps):
    out = []
    for step in steps:
        if step[0] == "call":
            out.append(f"{step[1]}(variant, options);")
        else:
            out.append(statement(source, step[1], step[2]))
    return out


def steps_name(steps, explicit=None):
    """the new adjust's own name (after arena_gametype_)"""
    if explicit:
        return explicit
    sets = [(s[1], s[2]) for s in steps if s[0] == "set"]
    for preset, lines in PRESETS.items():
        if sets == lines and len(sets) == len(steps):
            return preset
    parts = []
    for step in steps:
        if step[0] == "call":
            parts.append(step[1][len("arena_gametype_"):])
        else:
            value = re.sub(r"\W+", "_", step[2].lower()).strip("_")
            parts.append(f"{step[1].rsplit('.', 1)[1]}_{value}")
    return "_and_".join(parts)


class Plan:
    """a change: its gametypes, the new table rows and functions, the migrations (hashes filled in later)"""

    def __init__(self, source, names, row_changes, steps, why, decided=None, adjust_name=None, added_twins=(), also=None):
        if "*/" in why or (decided and "*/" in decided):
            die("--why and --decided may not hold */")
        self.source = source
        self.revision = source.revision + 1
        self.why = why.strip().rstrip(".")
        self.decided = decided
        self.row_changes = row_changes
        self.steps = steps
        self.added_twins = list(added_twins)
        self.also = also or {}  # gametype: the steps it takes after the others'
        self.notes = []
        self.functions = {}  # name: (statements, comment)
        self.changes = {}  # gametype: {field: (old, new)}
        common_steps = steps
        for name in names:
            row = source.by_name[name]
            change = {}
            steps = common_steps + self.also.get(name, [])
            own = steps_name(steps, adjust_name) if steps else None
            for field, value in row_changes.items():
                if field not in row.values and field in row.macro_values:
                    die(f"{name}: {field} comes from {row.macro_values[field][0]}: change it by hand")
                old_value = row.value(field)
                same = squash(old_value) == squash(value) if old_value is not None else \
                    evaluate(value, source.constants) == 0
                if not same:
                    change[field] = (row.value(field), value)
            if steps:
                old = row.values.get("adjust")
                if len(steps) == 1 and steps[0][0] == "call" and not old:
                    new = steps[0][1]
                elif old and len(steps) == 1 and steps[0][0] == "call" and old == steps[0][1]:
                    new = old
                else:
                    statements = ([f"{old}(variant, options);"] if old else []) + step_statements(source, steps)
                    new = "arena_gametype_" + (f"{old[len('arena_gametype_'):]}_and_{own}" if old else own)
                    self.add_function(new, statements, old, steps)
                if new != old:
                    change["adjust"] = (old, new)
            if change:
                self.changes[name] = change
            else:
                self.notes.append(f"{name}: already so in the source; left out")
        if not self.changes:
            die("no gametype changes")
        self.hashes = {}

    def add_function(self, name, statements, old, steps):
        existing = self.source.adjusts.get(name)
        if existing is not None:
            if existing != statements:
                die(f"{name} exists with another body; give --adjust-name")
            return
        if name in self.functions:
            return
        what = ", ".join(self.describe_step(s) for s in steps)
        comment = (f"revision {self.revision}" + (f" ({self.decided})" if self.decided else "") +
                   f": {self.why}. " + (f"{old}, then " if old else "") + what +
                   f". Made by {TOOL}. Shipped: never edited (a change is a new function)")
        self.functions[name] = (statements, comment)

    @staticmethod
    def describe_step(step):
        return step[1] if step[0] == "call" else f"{step[1]} {step[2]}"

    def describe_change(self, change):
        """a gametype's change in words: its fields, and what its new adjust sets"""
        out = []
        for field, (_, new) in change.items():
            if field == "adjust":
                out.append(", ".join(self.describe_step(s) for s in self.steps) + f" ({new})")
            else:
                out.append(f"{field} {new}")
        return ", ".join(out)

    def names(self):
        return list(self.changes)

    def summary(self):
        lines = [f"Seed revision {self.source.revision} -> {self.revision}: {self.why}."]
        if self.added_twins:
            lines.append("AE BR twins added (the self-check holds each pair equal): " + ", ".join(self.added_twins))
        for name, change in self.changes.items():
            lines.append(f"  {name}: " + "; ".join(f"{f} {old or '(none)'} -> {new}"
                                                    for f, (old, new) in change.items()))
        for name, (statements, _) in self.functions.items():
            lines.append(f"  new adjust {name}: " + " ".join(statements))
        lines += [f"  note: {n}" for n in self.notes]
        return "\n".join(lines)


# ---------------------------------------------------------------------------------------------- rendering

def comment_block(text, indent="\t", width=WIDTH):
    """a /* ... */ comment wrapped at width"""
    body = textwrap.wrap(text, width=width - visual_len(indent) - 3, break_long_words=False, break_on_hyphens=False)
    body[0] = "/* " + body[0]
    body[-1] += " */"
    return "\n".join((indent if i == 0 else indent) + line for i, line in enumerate(body))


def function_text(name, statements, comment):
    uses_options = any("options" in s for s in statements)
    lines = [comment_block(comment, indent="", width=80), f"static void {name}(",
             "\tstruct game_variant *variant,", "\tstruct game_variant_options *options)", "{"]
    if not uses_options:
        lines.append("\t(void)options;")
    lines += ["\t" + s for s in statements]
    lines += ["", "\treturn;", "}", ""]
    return "\n".join(lines)


def edit_row(source_text, row, change):
    """the row's text with the change spliced in (re-flowed when a line grows past WIDTH)"""
    text = source_text[row.start:row.end + 1]
    edits = []  # (start, end, new) relative to the row
    adjust_item = row.item("adjust")
    for field, (_, value) in change.items():
        item = row.item(field)
        if item:
            s, e = item.value_span
            edits.append((s - row.start, e - row.start, value))
        elif adjust_item is not None and field != "adjust":
            at = adjust_item.span[0] - row.start
            edits.append((at, at, f".{field} = {value}, "))
        else:
            at = row.items[-1].span[1] - row.start
            edits.append((at, at, f", .{field} = {value}"))
    for s, e, new in sorted(edits, reverse=True):
        text = text[:s] + new + text[e:]
    before = set(source_text[row.start:row.end + 1].splitlines())
    if all(visual_len(line) <= WIDTH or line in before for line in text.splitlines()):
        return text
    body = text[1:-1]
    items = [squash(body[s:e]) for s, e in split_items(body)]
    return wrap_items("{ ", items, " }", "\t\t")[0:]


def render(plan, hashes):
    """(arena_gametypes.c, seed_rules.py) as the plan makes them, hashes {name: old_block_hash}"""
    source = plan.source
    text = source.text
    # (the table rows, last first so the spans stay right)
    for name in sorted(plan.changes, key=lambda n: source.by_name[n].start, reverse=True):
        row = source.by_name[name]
        new_row = edit_row(text, row, plan.changes[name])
        text = text[:row.start] + new_row + text[row.end + 1:]
    edited = Source(text, "", "")  # (rows re-read: the new rows' designated form)
    # (the migrations: appended to the table)
    old_word = f"revision {plan.source.revision}"
    entries = [comment_block(
        f"revision {plan.revision}" + (f" ({plan.decided})" if plan.decided else "") + f": {plan.why}. " +
        "; ".join(f"{n}: " + plan.describe_change(c) for n, c in plan.changes.items()) +
        f". The old rows are {old_word}'s seeds; the old files' hashes from a root seeded by the {old_word} tree "
        f"(golden_seeds_rev{plan.source.revision}.json). Made by {TOOL}")]
    for name in plan.changes:
        old = source.by_name[name].designated_items()
        new = edited.by_name[name].designated_items()
        entries.append("\t{ .revision = %d,\n%s\n%s\n\t\t.old_block_hash = \"%s\" }," % (
            plan.revision, wrap_items("\t\t.old_row = { ", old, " },", "\t\t\t"),
            wrap_items("\t\t.new_row = { ", new, " },", "\t\t\t"), hashes.get(name, "<from the box>")))
    end = text.index("};\n\n/* debug.arena_test_migration")
    text = text[:end] + "\n".join(entries) + "\n" + text[end:]
    # (the migrations' comment)
    # (its last sentence, wherever the lines break it)
    words = r"\s+".join(re.escape(w) for w in "A row whose old and new names are the same is a value update "
                                                "(in place, under its name).".split())
    m = re.search(words, text)
    line_start = text.rindex("\n", 0, m.start()) + 1
    para_end = m.end()
    sentence = f"Revision {plan.revision} ({TOOL}): {plan.why}. "
    para = text[line_start:m.start()] + sentence + text[m.start():para_end]
    text = text[:line_start] + "\n".join(textwrap.wrap(squash(para), width=78, break_long_words=False,
                                                       break_on_hyphens=False)) + text[para_end:]
    # (the revision)
    text = text.replace(f"ARENA_GAMETYPES_REVISION = {plan.source.revision},",
                        f"ARENA_GAMETYPES_REVISION = {plan.revision},", 1)
    # (the new adjusts: prototypes and definitions)
    if plan.functions:
        protos = "".join(f"static void {n}(\n\tstruct game_variant *variant,\n\tstruct game_variant_options "
                         f"*options);\n" for n in plan.functions)
        at = text.index("\n/* ---------- globals */")
        text = text[:at] + protos + text[at:]
        defs = "\n".join(function_text(n, s, c) for n, (s, c) in plan.functions.items())
        at = text.index("/* a seeded gametype's rules, in the log */")
        text = text[:at] + defs + "\n" + text[at:]
    return text


# (seed_rules.py's words for a field: column or row field -> (key, value from the evaluated int))
def _on_off(v):
    return "on" if v else "off"


def _ticks(v):
    return f"{v} ticks ({v / TICKS_PER_SECOND:g} s)"


RULES_WORDS = {
    "universal_variant.respawn_time_growth": ("respawn growth", lambda v: f"{v} ticks"),
    "universal_variant.odd_man_out": ("odd man out", _on_off),
    "universal_variant.lives": ("lives", str),
    "universal_variant.respawn_time": ("respawn", _ticks),
    "universal_variant.suicide_penalty": ("suicide penalty", _ticks),
    "universal_variant.score_to_win": ("score to win", str),
    "game_engine_variant.slayer.no_death_bonus": ("death bonus", lambda v: _on_off(not v)),
    "game_engine_variant.slayer.no_kill_penalty": ("kill penalty", lambda v: _on_off(not v)),
    "game_engine_variant.slayer.kill_in_order": ("kill in order", _on_off),
    "game_engine_variant.king.moving_hill": ("moving hill", _on_off),
    "game_engine_variant.oddball.random_start": ("random start", _on_off),
    "game_engine_variant.ctf.assault": ("assault", _on_off),
    "game_engine_variant.ctf.flag_at_home_to_score": ("flag at home to score", _on_off),
    "options.time_limit": ("time limit", lambda v: f"{v} min" if v else "none"),
    "score_to_win": ("score to win", lambda v: str(v) if v else None),
    "time_limit": ("time limit", lambda v: f"{v} min" if v else "none"),
    "respawn_time": ("respawn", lambda v: _ticks(v) if v >= 0 else None),
    "suicide_penalty": ("suicide penalty", lambda v: _ticks(v) if v >= 0 else None),
    "starting_frags": ("starting frags", lambda v: "none" if v == 0xFF else (str(v) if v else None)),
    "ball_melee": ("ball melee", lambda v: "lethal" if v == 1 else "stock"),
    "universal_variant.flags.controller_crouch": ("controller crouch", lambda v: "stays down" if v else "halo ce"),
    "universal_variant.flags.melee_unzoom": ("melee unzooms", lambda v: "on" if v else "off"),
    "universal_variant.flags.mouse_aim_assist_off": ("mouse aim assist", lambda v: "off" if v else "allowed"),
    "universal_variant.flags.vehicles_destructible": ("vehicles destructible", lambda v: "on" if v else "off"),
}


def rules_words(plan):
    """({gametype: {seed_rules key: value}}, [fields it has no words for])"""
    words, unknown = {}, set()
    for name, change in plan.changes.items():
        out = {}
        for field, (_, new) in change.items():
            pairs = [(field, new)] if field != "adjust" else \
                [(s[1], s[2]) for s in plan.steps if s[0] == "set"] + [(s[1], None) for s in plan.steps
                                                                       if s[0] == "call"]
            for key, value in pairs:
                spec = RULES_WORDS.get(key)
                v = evaluate(value, plan.source.constants) if value is not None else None
                said = spec[1](v) if spec and v is not None else None
                if said is None:
                    unknown.add(key)
                else:
                    out[spec[0]] = said
        if out:
            words[name] = out
    return words, sorted(unknown)


def render_seed_rules(text, plan):
    words, _ = rules_words(plan)
    if not words:
        return text
    groups = {}
    for name, w in words.items():
        groups.setdefault(json.dumps(w, sort_keys=True), []).append(name)
    block = textwrap.wrap(f"revision {plan.revision} ({TOOL}): {plan.why}", width=WIDTH - 2, initial_indent="# ",
                          subsequent_indent="# ", break_long_words=False, break_on_hyphens=False)
    for w, names in groups.items():
        block.append(f"for _name in ({', '.join(json.dumps(n) for n in names)},):")
        block.append(f"    ROWS[_name][0].update({json.dumps(json.loads(w))})")
    at = text.index("\n\n\ndef check(")
    return text[:at] + "\n\n" + "\n".join(block) + text[at:]


def expected_audit(plan, old_audit=None):
    """the expected audit diff: [(gametype, column, old, new)] from the source's values (old from an audit)"""
    old_rows = {}
    if old_audit:
        import ae_gametype_audit
        _, rows = ae_gametype_audit.read_tsv(old_audit)
        old_rows = {r["stored_name"]: r for r in rows if r["kind"] == "ae"}
    row_columns = {"score_to_win": ("universal_variant.score_to_win", 0), "time_limit": ("options.time_limit", 0),
                   "respawn_time": ("universal_variant.respawn_time", 0),
                   "suicide_penalty": ("universal_variant.suicide_penalty", 0),
                   "vehicle_set": ("universal_variant.vehicle_set", 0),
                   "weapon_set": ("universal_variant.weapon_set", -1),
                   "loadout_primary": ("options.primary_weapon", -1),
                   "loadout_secondary": ("options.secondary_weapon", -1),
                   "no_map_weapons": ("options.no_map_weapons", 0), "friendly_fire": ("options.friendly_fire", -1),
                   "goal_radar": ("universal_variant.goal_radar", -1), "nhe_mode": ("universal_variant.nhe_mode", 0),
                   "starting_frags": ("universal_variant.starting_frags", 0),
                   "ball_melee": ("universal_variant.ball_melee", 0),
                   "hit_feedback": ("universal_variant.pad5.hit_feedback", 0)}
    out = []
    for name, change in plan.changes.items():
        old_row = old_rows.get(name, {})
        for field, (_, new) in change.items():
            if field == "adjust":
                pairs = [(s[1], s[2]) for s in plan.steps if s[0] == "set"]
            elif field in row_columns:
                column, shift = row_columns[field]
                pairs = [(column, None if new in ("ARENA_STOCK", "ARENA_VEHICLES_STOCK") else new, shift)]
                pairs = [(c, v if v is None else str(v), s) for c, v, s in pairs]
                value = evaluate(pairs[0][1], plan.source.constants) if pairs[0][1] is not None else None
                out.append((name, column, old_row.get(column, "?"),
                            "(the stock builder's)" if pairs[0][1] is None else
                            (str(value + shift) if value is not None else new)))
                continue
            else:
                out.append((name, f"(row field {field})", "?", new))
                continue
            for column, value in pairs:
                v = evaluate(value, plan.source.constants)
                out.append((name, column, old_row.get(column, "?"), str(v) if v is not None else value))
    return out


# ---------------------------------------------------------------------------------------------- checks

def check_results(plan, data):
    """the problems (empty: every check passed). data: what the box steps wrote, read back:
    old/new: {golden, debug, record}; migrations: {label: {before, after, debug, record, backups, edited, own}};
    rules: {name: [passed, problems]}"""
    problems = []
    rev = plan.revision
    names = set(plan.names())
    old, new = data["old"]["golden"], data["new"]["golden"]
    if data["new"]["record"].strip() != f"#revision {rev}":
        problems.append(f"fresh root: record says {data['new']['record'].strip()!r}, not #revision {rev}")
    for label in ("old", "new"):
        for line in data[label]["debug"].splitlines():
            if "self-check" in line:
                problems.append(f"{label} fresh root: {line.strip()[:160]}")
    if sorted(old) != sorted(new):
        problems.append("the fresh roots hold other gametypes: " +
                        ", ".join(sorted(set(old) ^ set(new))))
    changed = {n for n in new if n in old and new[n]["block"] != old[n]["block"]}
    for n in sorted(names - changed):
        problems.append(f"{n}: its file did not change (the change does nothing to it?)")
    for n in sorted(changed - names):
        problems.append(f"{n}: its file changed, but it is not in the change")
    for n in new:
        if n in old and new[n]["folder"] != old[n]["folder"]:
            problems.append(f"{n}: moved folder")
    for label, m in data.get("migrations", {}).items():
        before, after, debug = m["before"], m["after"], m["debug"]
        if m["record"].strip() != f"#revision {rev}":
            problems.append(f"{label}: record says {m['record'].strip()!r}")
        for line in debug.splitlines():
            if "self-check" in line:
                problems.append(f"{label}: {line.strip()[:160]}")
        if sorted(before) != sorted(after):
            problems.append(f"{label}: gametypes added or removed: {sorted(set(before) ^ set(after))}")
        for n, b in before.items():
            a = after.get(n)
            if a is None:
                continue
            if n in names and n != m.get("edited"):
                if a["block"] != new[n]["block"]:
                    problems.append(f"{label}: {n} not the same as a fresh revision {rev} root's")
                if f"updated arena gametype '{n}' (revision {rev})" not in debug:
                    problems.append(f"{label}: no 'updated arena gametype '{n}' (revision {rev})' in debug.txt")
                if not any(f"revision_{rev}" in p and b["folder"] in p for p in m["backups"]):
                    problems.append(f"{label}: no revision_{rev} backup of {n} ({b['folder']})")
            elif a["file"] != b["file"]:
                problems.append(f"{label}: {n} changed (it must stay byte-identical)")
        if m.get("edited") and \
                f"arena gametype '{m['edited']}' kept as it is (changed since seeded; revision {rev})" not in debug:
            problems.append(f"{label}: no 'kept as it is' line for the edited {m['edited']}")
    words, _ = rules_words(plan)
    for n, (passed, bad) in data.get("rules", {}).items():
        mine = {k: v for k, v in bad.items() if k in words.get(n, {}) or k == "run"}
        if mine:
            problems.append(f"seed_rules {n}: {mine}")
    return problems


def rules_warnings(plan, data):
    """seed_rules' mismatches in the fields the change does not touch: its expectations out of date before it"""
    words, _ = rules_words(plan)
    out = []
    for n, (passed, bad) in data.get("rules", {}).items():
        other = {k: v for k, v in bad.items() if k not in words.get(n, {}) and k != "run"}
        if other:
            out.append(f"seed_rules {n} (fields this change does not touch; its table is out of date): {other}")
    return out


def migration_cases(names):
    """the migration roots to try: [(label, edited name or None, (name, own name) or None)]"""
    names = list(names)
    if len(names) >= 2:
        return [("migrate", names[0], (names[1], "MY " + names[1][-8:].strip()))]
    return [("migrate-own", None, (names[0], "MY " + names[0][-8:].strip())), ("migrate-edit", names[0], None)]


# ---------------------------------------------------------------------------------------------- the box

class Box:
    def __init__(self, host, tree, results):
        import harness
        self.harness = harness
        self.host, self.tree, self.results = host, tree, results

    def ssh(self, command, capture=False):
        r = subprocess.run(["ssh", self.host, command], text=True, capture_output=capture)
        if r.returncode:
            die(f"on {self.host}: {command[:120]} failed ({r.returncode})" +
                (f": {(r.stderr or '').strip()[-400:]}" if capture else ""))
        return r.stdout if capture else None

    def sync(self):
        q = self.harness.remote_shell_path
        self.ssh(f"mkdir -p {q(self.tree)} {q(self.results)}")
        excludes = [".git", "build/", "build.ninja", ".ninja_deps", ".ninja_log", ".pytest_cache", "__pycache__",
                    ".ae_test_configure"]
        # (by content, with no times kept: a file that differs gets a new time there)
        cmd = ["rsync", "-rlpD", "--checksum", "--delete", "--protect-args"] + [f"--exclude={e}" for e in excludes] + \
            [f"{ROOT}/", f"{self.host}:{self.harness.remote_rsync_path(self.tree)}/"]
        if subprocess.run(cmd).returncode:
            die("rsync to the box failed")
        # (the files an earlier run put there edited: whatever their time, ninja must rebuild them)
        self.ssh(f"cd {q(self.tree)} && touch " + " ".join(shlex.quote(str(p)) for p in (SOURCE, SEED_RULES)))

    def put(self, relpath, text):
        q = self.harness.remote_shell_path
        r = subprocess.run(["ssh", self.host, f"cat > {q(self.tree)}/{shlex.quote(str(relpath))}"],
                           input=text.encode("latin-1"))
        if r.returncode:
            die(f"writing {relpath} on the box failed")

    def step(self, *args):
        q = self.harness.remote_shell_path
        self.ssh(f"cd {q(self.tree)} && python3 {TOOL} _box --results {q(self.results)} " +
                 " ".join(shlex.quote(a) for a in args))

    def fetch(self, local):
        local.mkdir(parents=True, exist_ok=True)
        cmd = ["rsync", "-a", "--protect-args", "--exclude=roots/", "--exclude=games/",
               f"{self.host}:{self.harness.remote_rsync_path(self.results)}/", f"{local}/"]
        if subprocess.run(cmd).returncode:
            die("rsync from the box failed")


def load_results(local, plan, with_migrations=True, with_rules=True):
    def text(name):
        p = local / name
        return p.read_text(errors="replace") if p.exists() else ""

    def golden(name):
        p = local / name
        return json.loads(p.read_text()) if p.exists() else {}

    data = {label: {"golden": golden(f"{label}-golden.json"), "debug": text(f"{label}-debug.txt"),
                    "record": text(f"{label}-record.txt")} for label in ("old", "new")}
    if with_migrations:
        data["migrations"] = {}
        for label, edited, own in migration_cases(plan.names()):
            data["migrations"][label] = {
                "before": golden(f"{label}-before.json"), "after": golden(f"{label}-golden.json"),
                "debug": text(f"{label}-debug.txt"), "record": text(f"{label}-record.txt"),
                "backups": text(f"{label}-backups.txt").splitlines(), "edited": edited, "own": own}
    if with_rules:
        rules = local / "rules.json"
        data["rules"] = json.loads(rules.read_text()) if rules.exists() else \
            {n: [False, {"run": "missing"}] for n in plan.names()}
    return data


def run_box(plan, box, new_source, new_rules, rules_check=True):
    """steps 1 and 3 on the box: (hashes {name: old block}, data for check_results); the box tree ends edited"""
    box.sync()
    box.step("seed", "--label", "old")
    local = box.local
    box.fetch(local)
    old = json.loads((local / "old-golden.json").read_text())
    missing = [n for n in plan.names() if n not in old]
    if missing:
        die(f"not seeded by the current tree: {missing}")
    hashes = {n: old[n]["block"] for n in plan.names()}
    committed = ROOT / GOLDEN.format(plan.source.revision)
    if committed.exists():
        golden = json.loads(committed.read_text())
        differ = sorted(n for n in old if n in golden and golden[n]["block"] != old[n]["block"])
        if differ or sorted(golden) != sorted(old):
            die(f"the box's revision {plan.source.revision} seeds are not {committed.name}'s ({differ[:5]}...): "
                "the seeding pipeline changed; find out why before a new revision")
    text = new_source(hashes)
    box.put(SOURCE, text)
    box.put(SEED_RULES, new_rules)
    box.step("seed", "--label", "new")
    for label, edited, own in migration_cases(plan.names()):
        args = ["patch", "--from", "old", "--label", label]
        if edited:
            args += ["--edit", edited]
        if own:
            args += ["--own", f"{own[0]}={own[1]}"]
        box.step(*args)
        box.step("seed", "--label", label, "--save-from", label)
    if rules_check:
        box.step("rules", "--from", "new", *plan.names())
    box.fetch(local)
    return hashes, text, load_results(local, plan, with_rules=rules_check)


# ---------------------------------------------------------------------------------------------- box side

def box_build(results):
    if not Path("build.ninja").exists():
        with open(results / "configure.log", "w") as f:
            if subprocess.run([sys.executable, "configure.py", "--release", "--compiler-launcher", "ccache"],
                              stdout=f, stderr=subprocess.STDOUT).returncode:
                die(f"configure failed ({results}/configure.log)")
    with open(results / "build.log", "a") as f:
        if subprocess.run(["ninja", f"-j{os.cpu_count() or 8}", "linux64"], stdout=f,
                          stderr=subprocess.STDOUT).returncode:
            tail = (results / "build.log").read_text(errors="replace").splitlines()[-20:]
            die("ninja linux64 failed:\n" + "\n".join(tail))


def box_game(results, label, spec):
    """one hosted game of this tree's build: (its save root, its debug.txt text)"""
    import harness
    cfg = harness.load_config(None)
    harness.apply_load_policy(cfg)
    run_id = f"gtchange-{results.name}-{label}"
    spec = harness.parse_spec(dict(spec, name=label, build=str(Path("build/linux64/halo").resolve())))
    out = results / "games" / label
    result = harness.play(cfg, spec, out, harness.GameSlots(cfg, 1), keep_work=True, run_id=run_id)
    work = harness.expand(cfg["work_dir"]) / run_id / harness.safe_name(label)
    debug = (out / "debug.txt").read_text(errors="replace") if (out / "debug.txt").exists() else ""
    return work, work / "save", debug, result


def box_main(argv):
    import seed_compare
    p = argparse.ArgumentParser(prog="ae_gametype_change.py _box")
    p.add_argument("step", choices=("seed", "patch", "rules"))
    p.add_argument("--results", required=True)
    p.add_argument("--label")
    p.add_argument("--save-from", help="a label of a root already made")
    p.add_argument("--from", dest="from_label")
    p.add_argument("--edit")
    p.add_argument("--own")
    p.add_argument("names", nargs="*")
    a = p.parse_args(argv)
    results = Path(os.path.expanduser(a.results)).resolve()
    roots = results / "roots"
    roots.mkdir(parents=True, exist_ok=True)
    if a.step == "seed":
        box_build(results)
        spec = dict(SEED_SPEC, env={"HALO_ARENA_AUDIT": "1", "SDL_AUDIO_DRIVER": "dummy"})
        if a.save_from:
            spec["save_from"] = str(roots / a.save_from)
            before = seed_compare.gametype_hashes(roots / a.save_from)
            (results / f"{a.label}-before.json").write_text(json.dumps(before, indent=1, sort_keys=True) + "\n")
        work, save, debug, result = box_game(results, a.label, spec)
        if not save.is_dir():
            die(f"the {a.label} game made no save root: {result.get('status')} {result.get('why')}")
        target = roots / (a.label if not a.save_from else f"{a.label}-after")
        if target.exists():
            shutil.rmtree(target)
        # (the gametypes, the record and the backups; not the map caches, up to 300 MB each)
        shutil.copytree(save, target, symlinks=True,
                        ignore=lambda folder, names: [n for n in names if re.match(r"cache\d+\.map$|shader_cache", n)])
        shutil.rmtree(work.parent, ignore_errors=True)
        golden = subprocess.run([sys.executable, "tools/ae_test/seed_compare.py", str(target)], capture_output=True,
                                text=True, check=True).stdout
        (results / f"{a.label}-golden.json").write_text(golden)
        (results / f"{a.label}-debug.txt").write_text(debug)
        record = target / "z" / "saved" / "playlists" / "arena_gametypes.txt"
        (results / f"{a.label}-record.txt").write_text(record.read_text().splitlines()[0] + "\n"
                                                       if record.exists() else "")
        audit = target / "z" / "arena_audit.tsv"
        if audit.exists():
            shutil.copy2(audit, results / f"{a.label}-audit.tsv")
        backups = target / "z" / "saved" / "playlists_backup"
        (results / f"{a.label}-backups.txt").write_text(
            "".join(f"{f.relative_to(backups)}\n" for f in sorted(backups.rglob("*"))) if backups.exists() else "")
        (results / f"{a.label}-result.json").write_text(json.dumps(result, indent=1, default=str) + "\n")
        print(f"_box seed {a.label}: {result.get('status')}, {len(json.loads(golden))} gametypes")
    elif a.step == "patch":
        import gametype_file
        source_root, target = roots / a.from_label, roots / a.label
        if target.exists():
            shutil.rmtree(target)
        if a.edit:
            f = gametype_file.find(source_root, a.edit)
            value = (f.read_bytes()[SCORE_OFFSET] + 7) % 256
            gametype_file.copy_and_patch(source_root, target, a.edit, {SCORE_OFFSET: value})
            source_root = target
        if a.own:
            name, own = a.own.split("=", 1)
            if a.edit:
                staged = roots / f"{a.label}-staged"
                if staged.exists():
                    shutil.rmtree(staged)
                target.rename(staged)
                source_root = staged
            gametype_file.copy_and_patch(source_root, target, name, {}, own_name=own)
            if a.edit:
                shutil.rmtree(staged)
        print(f"_box patch {a.label}: edited {a.edit or '-'}, own copy {a.own or '-'}")
    else:
        import seed_rules
        runs = results / "rules"
        for name in a.names:
            spec = dict(RULES_SPEC, saved_gametype=name, save_from=str(roots / a.from_label),
                        env={"SDL_AUDIO_DRIVER": "dummy"})
            if seed_rules.ROWS.get(name, ({}, 0, 0))[0].get("auto team balance") == "on":
                spec["env"]["HALO_NETWORK_TEST_AUTO_BALANCE"] = "1"
            label = "seed-" + name.replace(" ", "_")
            work, _, debug, _ = box_game(results, label, spec)
            shutil.rmtree(work.parent, ignore_errors=True)
            out = runs / label / "game"
            out.mkdir(parents=True, exist_ok=True)
            (out / "debug.txt").write_text(debug)
        checked = {n: list(v) for n, v in seed_rules.check(runs, "seed-").items() if n in a.names}
        (results / "rules.json").write_text(json.dumps(checked, indent=1, default=str) + "\n")
        print(f"_box rules: {sum(1 for v in checked.values() if v[0])}/{len(checked)} pass")
    return 0


# ---------------------------------------------------------------------------------------------- main

def write_manifest(plan, hashes):
    path = ROOT / MANIFEST
    data = json.loads(path.read_text()) if path.exists() else []
    data.append({
        "revision": plan.revision, "why": plan.why, "decided": plan.decided, "gametypes": plan.names(),
        "row": plan.row_changes, "steps": [list(s) for s in plan.steps],
        **({"also": {n: [list(s) for s in v] for n, v in plan.also.items()}} if plan.also else {}),
        "adjusts": {n: c["adjust"][1] for n, c in plan.changes.items() if "adjust" in c},
        "old_block_hashes": hashes, "golden": Path(GOLDEN.format(plan.revision)).name,
        "seed_rules": rules_words(plan)[0]})
    path.write_text(json.dumps(data, indent=1) + "\n")


def parser():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--gametypes", action="append", default=[], help="a name, set, engine, team/ffa ('&': and)")
    p.add_argument("--set", action="append", default=[], help="FIELD=VALUE (a row field, or an audit column)")
    p.add_argument("--adjust", action="append", default=[], help="an existing adjust's name or a preset")
    p.add_argument("--adjust-name", help="the new adjust's name (after arena_gametype_)")
    p.add_argument("--why", help="the reason, for the comments and the manifest")
    p.add_argument("--decided", help="who and when, e.g. 'owner, 2026-10-20'")
    p.add_argument("--no-twins", action="store_true", help="change only the chosen gametypes, not their AE BR twins "
                   "(a change to the AE one's start: the self-check takes the start off first)")
    p.add_argument("--also", action="append", default=[], metavar="SELECTION=ADJUST", help="in the same revision, these "
                   "gametypes also take this adjust, after the others' and without their AE BR twins")
    p.add_argument("--dry-run", action="store_true", help="print the plan; write nothing")
    p.add_argument("--list", metavar="SELECTION", help="print the gametypes a selection picks and stop")
    p.add_argument("--box", help="the build box's ssh name (default: the ae_test config's box.ssh); with --dry-run "
                                 "it also builds and checks there")
    p.add_argument("--box-tree", default="~/ae-gtchange", help="the box's tree (copied over from this one)")
    p.add_argument("--box-results", help="the box's results folder (default ~/halo-test/gametype-change/<stamp>)")
    p.add_argument("--audit-from", help="an arena_audit.tsv for the dry run's old values")
    p.add_argument("--no-rules-check", action="store_true", help="skip hosting each changed gametype (seed_rules)")
    p.add_argument("--out", help="the local reports folder (default: the ae_test out_dir's gametype_change/<stamp>)")
    p.add_argument("--config", help="the ae_test config")
    return p


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    if argv[:1] == ["_box"]:
        return box_main(argv[1:])
    a = parser().parse_args(argv)
    source = Source.load()
    if a.list:
        for name in select(source, [a.list]):
            c = source.catalog.get(name, {})
            print(f"{name:12} {c.get('set', '?'):9} {c.get('engine', '?'):8} adjust {source.by_name[name].values.get('adjust', '-')}")
        return 0
    if not a.gametypes or not a.why:
        die("--gametypes and --why are needed")
    names, added = (select(source, a.gametypes), []) if a.no_twins else add_twins(source, select(source, a.gametypes))
    also = {}
    for text in a.also:
        selection, _, adjust = text.partition("=")
        for name in select(source, [selection]):
            if name not in names:
                die(f"--also {text}: {name} is not among the changed gametypes")
            also.setdefault(name, []).extend(parse_sets(source, [], [adjust])[1])
    row, steps = parse_sets(source, a.set, a.adjust)
    for step in steps:
        if step[0] == "set" and step[1].startswith("game_engine_variant."):
            engine = step[1].split(".")[1]
            wrong = [n for n in names if source.catalog.get(n, {}).get("engine") != engine]
            if wrong:
                die(f"{step[1]} is a {engine} field; not {', '.join(wrong)}")
    plan = Plan(source, names, row, steps, a.why, a.decided, a.adjust_name, added, also)
    print(plan.summary())
    _, unknown = rules_words(plan)
    import harness
    cfg = harness.load_config(a.config)
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    out = Path(a.out) if a.out else harness.expand(cfg["out_dir"]) / "gametype_change" / stamp
    golden = ROOT / GOLDEN.format(source.revision)
    committed = json.loads(golden.read_text()) if golden.exists() else {}
    planned_hashes = {n: committed[n]["block"] for n in plan.names() if n in committed}
    new_rules = render_seed_rules(read(ROOT / SEED_RULES), plan)
    if a.dry_run and not a.box:
        text = render(plan, planned_hashes)
        print("\nExpected audit diff (from the source; the box run gives the real one):")
        for name, column, old, new in expected_audit(plan, a.audit_from):
            print(f"  {name}: {column}: {old} -> {new}")
        diff = subprocess.run(["diff", "-u", str(ROOT / SOURCE), "-"], input=text, text=True, capture_output=True,
                              encoding="latin-1").stdout
        rules_diff = subprocess.run(["diff", "-u", str(ROOT / SEED_RULES), "-"], input=new_rules, text=True,
                                    capture_output=True).stdout
        print(f"\nold_block_hash: from {golden.name} (the box run checks them)" if planned_hashes else "")
        print(diff + rules_diff)
        if unknown:
            print("seed_rules has no words for (check by hand): " + ", ".join(unknown))
        return 0
    host = a.box or cfg.get("box", {}).get("ssh")
    if not host:
        die("no build box: --box HOST, or box.ssh in the ae_test config")
    box = Box(host, a.box_tree, a.box_results or f"~/halo-test/gametype-change/{stamp}")
    box.local = out
    hashes, text, data = run_box(plan, box, lambda h: render(plan, h), new_rules, not a.no_rules_check)
    problems = check_results(plan, data)
    import ae_gametype_audit
    audit_diff = ae_gametype_audit.diff(out / "old-audit.tsv", out / "new-audit.tsv") \
        if (out / "old-audit.tsv").exists() and (out / "new-audit.tsv").exists() else "(no audit TSVs)\n"
    (out / "plan.txt").write_text(plan.summary() + "\n")
    (out / "audit-diff.md").write_text(audit_diff)
    (out / "problems.txt").write_text("".join(p + "\n" for p in problems))
    print("\n" + audit_diff)
    print("Checks on the box: " + ("ALL PASS" if not problems else f"{len(problems)} PROBLEMS"))
    for line in problems:
        print("  " + line)
    for line in rules_warnings(plan, data):
        print("  warning: " + line)
    if unknown:
        print("seed_rules has no words for (check by hand): " + ", ".join(unknown))
    print(f"Reports: {out}")
    if problems:
        print("Nothing written here (the box tree holds the edit).")
        return 1
    if a.dry_run:
        print("Dry run: nothing written here.")
        return 0
    write(ROOT / SOURCE, text)
    write(ROOT / SEED_RULES, new_rules)
    new_golden = ROOT / GOLDEN.format(plan.revision)
    shutil.copy2(out / "new-golden.json", new_golden)
    if not golden.exists():
        shutil.copy2(out / "old-golden.json", golden)
    write_manifest(plan, hashes)
    print(f"Written: {SOURCE}, {SEED_RULES}, {new_golden.relative_to(ROOT)}, {MANIFEST}"
          + ("" if golden.exists() else f", {golden.relative_to(ROOT)}"))
    print("Still yours: the CHANGELOG line, the commit.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
