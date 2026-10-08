#!/usr/bin/env python3
"""seed_compare: a save root's saved gametypes, by stored name, against golden hashes or another root.

    python3 tools/ae_test/seed_compare.py <save root>                       # print the hashes (JSON)
    python3 tools/ae_test/seed_compare.py <save root> --golden FILE.json    # each golden gametype's content
        [--rename "AE PRO TS=AE COMP TS" ...] [--only NAME ...]
    python3 tools/ae_test/seed_compare.py <save root> --same-as <other root> [--except NAME ...]

For each u/UDATA/<folder>/blam.lst: "content", a SHA-1 of the variant after its name (bytes 0x18..0x67) and
the PC options block (0x100: header, options; not its signature), which a rename leaves alone; and "file", a
SHA-1 of the whole file (byte-identical files). --golden checks every golden name's content (a renamed one
under its new name); --same-as checks every gametype of the other root is byte-identical here. Reads only.
Exit 0 when everything matches, 1 otherwise.
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import gametype_file  # noqa: E402

OPTIONS_OFFSET = 0x100
OPTIONS_BYTES = 8 + 0x1C  # (playlist_profile_options_header, struct game_variant_options)


def gametype_hashes(save_root):
    """{stored name: {"content", "file", "folder"}}; two of one name (case aside) are refused"""
    out = {}
    for f in sorted((Path(save_root) / "u" / "UDATA").glob("*/blam.lst")):
        b = f.read_bytes()
        if len(b) < OPTIONS_OFFSET + OPTIONS_BYTES:
            continue
        name = gametype_file.name_of(b)
        if name.upper() in {n.upper() for n in out}:
            raise SystemExit(f"seed_compare: two saved gametypes named '{name}' in {save_root}")
        content = hashlib.sha1(b[0x18:gametype_file.VARIANT_SIZE] + b[OPTIONS_OFFSET:OPTIONS_OFFSET + OPTIONS_BYTES])
        out[name] = {"content": content.hexdigest(), "file": hashlib.sha1(b).hexdigest(), "folder": f.parent.name}
    return out


def compare_golden(hashes, golden, renames=None, only=None):
    """the problems: each golden name's content here (under its new name when renamed)"""
    renames = renames or {}
    problems = []
    for name, want in sorted(golden.items()):
        if only and name not in only:
            continue
        here = renames.get(name, name)
        got = hashes.get(here)
        if not got:
            problems.append(f"{here}: missing")
        elif got["content"] != want["content"]:
            problems.append(f"{here}: content differs from golden {name}")
    return problems


def compare_roots(hashes, other, excepted=None):
    """the problems: each gametype of the other root byte-identical here, in the same folder"""
    excepted = {n.upper() for n in (excepted or [])}
    problems = []
    for name, want in sorted(other.items()):
        if name.upper() in excepted:
            continue
        got = hashes.get(name)
        if not got:
            problems.append(f"{name}: missing")
        elif got["file"] != want["file"] or got["folder"] != want["folder"]:
            problems.append(f"{name}: not byte-identical (or moved)")
    return problems


def main(argv):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("save_root")
    p.add_argument("--golden", help="a JSON file of {name: {content, ...}}")
    p.add_argument("--rename", action="append", default=[], help="OLD=NEW: a golden name found under its new name")
    p.add_argument("--only", action="append", default=[], help="check only these golden names")
    p.add_argument("--same-as", help="another save root whose gametypes must be byte-identical here")
    p.add_argument("--except", dest="excepted", action="append", default=[], help="a name --same-as skips")
    a = p.parse_args(argv)
    hashes = gametype_hashes(a.save_root)
    if not a.golden and not a.same_as:
        print(json.dumps(hashes, indent=1, sort_keys=True))
        return 0
    problems = []
    if a.golden:
        renames = dict(r.split("=", 1) for r in a.rename)
        problems += compare_golden(hashes, json.loads(Path(a.golden).read_text()), renames, a.only)
    if a.same_as:
        problems += compare_roots(hashes, gametype_hashes(a.same_as), a.excepted)
    for line in problems:
        print("seed_compare: " + line)
    print(f"seed_compare: {'FAIL' if problems else 'PASS'} ({len(hashes)} gametypes)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
