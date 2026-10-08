#!/usr/bin/env python3
"""gametype_file: a COPY of a save root with a saved gametype's variant bytes patched and signed again, for tests.

    python3 tools/ae_test/gametype_file.py --from <save root> --to <new folder> --name "NHE 1V1" --set 0x1d=3
    python3 tools/ae_test/gametype_file.py --from <save root> --to <new folder> --name "AE TEAM SLY" --own "TS 50"
        (--own: the copy gets one more saved gametype, --name's file saved again under that name, as a player's
        own made in the editor would be: its folder is the one the game keeps that name in, xbox_xapi.c's
        save_name_hash; the original stays)

A saved gametype is u/UDATA/<folder>/blam.lst in a save root: the game variant (0x68 bytes, its name in
the first 24 as UTF-16), then its signature, a SHA-1 over "halo-linux content signature\\0" and those
bytes (port/linux/src/xbox_xapi.c XCalculateSignature*, saved_game_file_generate_checksum), then the PC
options with their own signature. A file whose signature does not match is refused ("checksum failed"), so
a hand-patched one must be signed again.

Copy first, never in place: --from is copied to --to (which must not exist) and only the copy is patched.
Real save roots (~/.local/share/halo-linux-*) and Quiver Launcher installs are refused as either, and so is
/tmp. Only the variant's bytes are patched, with byte values (0..255), in a file that was signed before, of
a name no other saved gametype has. Then play the copy: run.py --save-from <new folder>.
"""
import argparse
import hashlib
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import harness  # noqa: E402

KEY = b"halo-linux content signature\0"
VARIANT_SIZE = 0x68
SIGNATURE_SIZE = 20
NAME_SIZE = 24


def signature(block):
    return hashlib.sha1(KEY + bytes(block[:VARIANT_SIZE])).digest()


def signed(block):
    return bytes(block[VARIANT_SIZE:VARIANT_SIZE + SIGNATURE_SIZE]) == signature(block)


def sign(block):
    """the block with the variant's signature written after it"""
    b = bytearray(block)
    b[VARIANT_SIZE:VARIANT_SIZE + SIGNATURE_SIZE] = signature(b)
    return bytes(b)


def name_of(block):
    return bytes(block[:NAME_SIZE]).decode("utf-16-le", "replace").split("\0", 1)[0]


def real_root(path):
    """a real save root (~/.local/share/halo-linux-*) or a Quiver Launcher install, or inside one"""
    p = Path(harness.expand(path)).resolve()
    share = (Path.home() / ".local" / "share").resolve()
    for q in [p, *p.parents]:
        if q.parent == share and q.name.startswith("halo-linux"):
            return True
        if q.name == "Apps" and q.parent.name == "Quiver Launcher":
            return True
    return False


def check_path(path, what):
    harness.check_not_tmp(path, what)
    if real_root(path):
        raise SystemExit(f"gametype_file: {what} {path} is a real save root or a Quiver install; use a copy")


def find(save_root, name):
    """the saved gametype of that stored name (case aside, as the game's lists sort them), else None; two of
    that name are refused (the game would take whichever it lists first)"""
    hits = []
    for f in sorted((Path(save_root) / "u" / "UDATA").glob("*/blam.lst")):
        b = f.read_bytes()
        if len(b) >= VARIANT_SIZE + SIGNATURE_SIZE and name_of(b).upper() == name.upper():
            hits.append(f)
    if len(hits) > 1:
        raise SystemExit(f"gametype_file: {len(hits)} saved gametypes named '{name}' in {save_root}: "
                         + ", ".join(str(h) for h in hits))
    return hits[0] if hits else None


def check_values(values):
    for offset, value in values.items():
        if not 0 <= offset < VARIANT_SIZE:
            raise SystemExit(f"gametype_file: offset {offset:#x} is not in the variant (0..{VARIANT_SIZE - 1:#x})")
        if not 0 <= value <= 0xFF:
            raise SystemExit(f"gametype_file: {value} at {offset:#x} is not a byte (0..255)")


def _patch_copy(path, values):
    """{offset: byte} written into the variant of a file of the copy, signed again"""
    b = bytearray(Path(path).read_bytes())
    for offset, value in values.items():
        b[offset] = value
    Path(path).write_bytes(sign(b))


def save_folder_name(name):
    """the folder (u/UDATA/<this>) the game keeps a saved game of that name in: xbox_xapi.c's save_name_hash,
    FNV-1a over the UTF-16 units, its low 48 bits in 12 upper-case hex digits"""
    h = 1469598103934665603
    units = name.encode("utf-16-le")
    for i in range(0, len(units), 2):
        h = ((h ^ (units[i] | units[i + 1] << 8)) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return "%012X" % (h & 0xFFFFFFFFFFFF)


def add_own_copy(root, name, own_name, values):
    """in a copied root: --name's file saved again as the player's own own_name (its folder, SaveMeta.xbx, the
    name in the variant), values patched into it; the new file"""
    if len(own_name) > 11:
        raise SystemExit(f"gametype_file: '{own_name}' is longer than a stored name (11)")
    if find(root, own_name):
        raise SystemExit(f"gametype_file: a saved gametype '{own_name}' exists in {root}")
    f = find(root, name)
    folder = f.parent.parent / save_folder_name(own_name)
    if folder.exists():
        raise SystemExit(f"gametype_file: {folder} exists")
    shutil.copytree(f.parent, folder)
    b = bytearray((folder / "blam.lst").read_bytes())
    b[0:NAME_SIZE] = (own_name.encode("utf-16-le") + b"\0" * NAME_SIZE)[:NAME_SIZE]
    for offset, value in values.items():
        b[offset] = value
    (folder / "blam.lst").write_bytes(sign(b))
    (folder / "SaveMeta.xbx").write_bytes(own_name.encode("utf-16-le"))
    return folder / "blam.lst"


def copy_and_patch(from_root, to_root, name, values, own_name=None):
    """--from copied to --to (new), and that gametype patched in the copy; the copy's file"""
    check_path(from_root, "--from")
    check_path(to_root, "--to")
    from_root, to_root = harness.expand(from_root), harness.expand(to_root)
    if to_root.exists():
        raise SystemExit(f"gametype_file: --to {to_root} exists; give a new folder")
    check_values(values)
    f = find(from_root, name)
    if not f:
        raise SystemExit(f"gametype_file: no saved gametype '{name}' in {from_root}")
    if not signed(f.read_bytes()):
        raise SystemExit(f"gametype_file: {f} is not a signed saved gametype")
    shutil.copytree(from_root, to_root, symlinks=False)
    if own_name:
        return add_own_copy(to_root, name, own_name, values)
    copy = to_root / f.relative_to(from_root)
    _patch_copy(copy, values)
    return copy


def main(argv):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--from", dest="from_root", required=True, help="the save root to copy (never written)")
    p.add_argument("--to", dest="to_root", required=True, help="a new folder for the patched copy")
    p.add_argument("--name", required=True, help="the gametype's stored name")
    p.add_argument("--set", action="append", default=[], help="OFFSET=BYTE (e.g. 0x1d=3: nhe_mode); repeat")
    p.add_argument("--own", help="save --name's file again as the player's own gametype of this name (--set "
                   "patches that one)")
    a = p.parse_args(argv)
    values = {}
    for kv in a.set:
        k, _, v = kv.partition("=")
        values[int(k, 0)] = int(v, 0)
    f = copy_and_patch(a.from_root, a.to_root, a.name, values, own_name=a.own)
    print(f"gametype_file: {f} ({name_of(f.read_bytes())}) patched {', '.join(f'{k:#x}={v}' for k, v in values.items())}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
