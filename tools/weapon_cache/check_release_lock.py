#!/usr/bin/env python3
"""Release-kit check: the cache to ship is the cache this tree's executable is locked to.
    check_release_lock.py <ae_content.aecache> [cache.lock]
Passes only when ae_weapon_cache_lock.h is PRESENT 1 and equals what lock_header.py writes from the lock, and the cache is
a valid cache whose serial and stored digest are the lock's (and the plain file SHA-256 too, when the lock has it).
Exit 0 = ok, 1 = refused (one line says why)."""
import hashlib
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(ROOT / "tools" / "weapon_pack"))
import lock_header  # noqa: E402
import aepack  # noqa: E402


def check(cache_path, lock_path=None):
    """returns an error text, or None when the cache is the locked one"""
    lock_path = Path(lock_path) if lock_path else HERE / "cache.lock"
    if not lock_path.is_file():
        return "no cache.lock"
    lock = json.loads(lock_path.read_text())
    header_path = ROOT / "port" / "linux" / "game" / "ae_weapon_cache_lock.h"
    text = header_path.read_text()
    if "#define AE_WEAPON_CACHE_LOCK_PRESENT 1" not in text:
        return "ae_weapon_cache_lock.h is not PRESENT 1 (a DEV build)"
    if text != lock_header.render(lock):
        return "ae_weapon_cache_lock.h does not match cache.lock (run lock_header.py)"
    image = Path(cache_path).read_bytes()
    try:
        parsed = aepack.read_cache(image)
    except aepack.Refused as refused:
        return "not a valid cache: %s" % refused
    if parsed.header["serial"] != int(lock["serial"]):
        return "serial %d, the lock names %d" % (parsed.header["serial"], int(lock["serial"]))
    if parsed.sha256.hex() != lock["cache_sha256"]:
        return "stored digest %s, the lock names %s" % (parsed.sha256.hex()[:16], lock["cache_sha256"][:16])
    if lock.get("file_sha256") and hashlib.sha256(image).hexdigest() != lock["file_sha256"]:
        return "file SHA-256 differs from the lock's"
    return None


def main(arguments):
    if not arguments:
        print(__doc__)
        return 1
    error = check(arguments[0], arguments[1] if len(arguments) > 1 else None)
    print("check_release_lock: " + (error or "ok"))
    return 1 if error else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
