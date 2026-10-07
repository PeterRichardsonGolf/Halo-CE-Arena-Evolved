#!/usr/bin/env python3
"""gametype_file: a saved gametype's variant bytes patched and signed again, for tests.

    python3 tools/ae_test/gametype_file.py <save root> --name "NHE 1V1" --set 0x1d=3 [--set 0x08=...]

A saved gametype is u/UDATA/<folder>/blam.lst in a save root: the game variant (0x68 bytes, its name in
the first 24 as UTF-16), then its signature, a SHA-1 over "halo-linux content signature\\0" and those
bytes (port/linux/src/xbox_xapi.c XCalculateSignature*, saved_game_file_generate_checksum), then the PC
options with their own signature. A file whose signature does not match is refused ("checksum failed"), so
a hand-patched one must be signed again. Only the variant's bytes are patched here; a file that was not
signed before is refused. Never point it at a real save root: patch a copy (run.py --save-from).
"""
import argparse
import hashlib
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


def find(save_root, name):
    """the saved gametype of that stored name (case aside, as the game's lists sort them), else None"""
    for f in sorted((Path(save_root) / "u" / "UDATA").glob("*/blam.lst")):
        b = f.read_bytes()
        if len(b) >= VARIANT_SIZE + SIGNATURE_SIZE and name_of(b).upper() == name.upper():
            return f
    return None


def patch(path, values):
    """{offset: byte} written into the variant, signed again"""
    path = Path(path)
    b = bytearray(path.read_bytes())
    if len(b) < VARIANT_SIZE + SIGNATURE_SIZE or not signed(b):
        raise SystemExit(f"gametype_file: {path} is not a signed saved gametype")
    for offset, value in values.items():
        if not 0 <= offset < VARIANT_SIZE:
            raise SystemExit(f"gametype_file: offset {offset:#x} is not in the variant (0..{VARIANT_SIZE - 1:#x})")
        b[offset] = value & 0xFF
    path.write_bytes(sign(b))


def main(argv):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("save_root")
    p.add_argument("--name", required=True, help="the gametype's stored name")
    p.add_argument("--set", action="append", default=[], help="OFFSET=BYTE (e.g. 0x1d=3: nhe_mode); repeat")
    a = p.parse_args(argv)
    harness.check_not_tmp(a.save_root, "save root")
    f = find(a.save_root, a.name)
    if not f:
        raise SystemExit(f"gametype_file: no saved gametype '{a.name}' in {a.save_root}")
    values = {}
    for kv in a.set:
        k, _, v = kv.partition("=")
        values[int(k, 0)] = int(v, 0)
    patch(f, values)
    print(f"gametype_file: {f} ({name_of(f.read_bytes())}) patched {', '.join(f'{k:#x}={v}' for k, v in values.items())}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
