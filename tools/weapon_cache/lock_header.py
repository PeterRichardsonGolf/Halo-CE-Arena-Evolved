#!/usr/bin/env python3
"""Writes port/linux/game/ae_weapon_cache_lock.h from tools/weapon_cache/cache.lock (JSON: "serial" and "cache_sha256", the
hex of the cache's SHA-256), so a release executable refuses any other cache. With no lock file it writes the DEV default.
    lock_header.py [cache.lock] [out.h]
"""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
TEMPLATE = """/* Written by tools/weapon_cache/lock_header.py from cache.lock; do not edit (see the checked-in default's comment). */
#ifndef AE_WEAPON_CACHE_LOCK_H
#define AE_WEAPON_CACHE_LOCK_H

#define AE_WEAPON_CACHE_LOCK_PRESENT {present}
#define AE_WEAPON_CACHE_LOCK_SERIAL {serial}
#define AE_WEAPON_CACHE_LOCK_SHA256 {{ {sha} }}

#endif
"""


def render(lock):
    if not lock:
        return TEMPLATE.format(present=0, serial=0, sha="0")
    digest = bytes.fromhex(lock["cache_sha256"])
    if len(digest) != 32:
        raise SystemExit("cache_sha256 is not 32 bytes")
    return TEMPLATE.format(present=1, serial=int(lock["serial"]), sha=", ".join(f"0x{b:02x}" for b in digest))


def main(arguments):
    lock_path = Path(arguments[0]) if arguments else ROOT / "tools" / "weapon_cache" / "cache.lock"
    out = Path(arguments[1]) if len(arguments) > 1 else ROOT / "port" / "linux" / "game" / "ae_weapon_cache_lock.h"
    lock = json.loads(lock_path.read_text()) if lock_path.is_file() else None
    out.write_text(render(lock))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
