"""Unpacks exactly the listed files of an archive and nothing else.

CMT's source release is 5 GB and carries programs (its /util and /scripts folders) that nothing here ever runs or
unpacks; the recipe needs a few hundred of its tag files. An unzip command given an empty file list unpacks
everything, which happened once: so this takes exact names only, refuses an empty list, a name twice, a name that
is not a plain relative path (no wildcard, no "..", no folder), a name under a util or scripts folder, a name the
archive does not have, and a destination that already holds files, and writes nothing unless every name passes.

Usage: extract_allowlist.py <archive> <names file> <destination> [--strip <prefix>] [--sha256 <hex>]
       (the names file: one archive path a line; --strip: the part of each name left off below the destination)
"""
import hashlib
import os
import re
import sys
import zipfile


class Refused(Exception):
    """nothing was unpacked, with the reason"""


FORBIDDEN_FOLDERS = ("util", "scripts")
PLAIN_PART = re.compile(r"[A-Za-z0-9_ .\-+()!&',]+")


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as file:
        for chunk in iter(lambda: file.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _check_name(name, strip):
    parts = name.split("/")
    if not name or name.startswith("/") or name.endswith("/") or any(
            part in ("", ".", "..") or not PLAIN_PART.fullmatch(part) for part in parts):
        raise Refused("not a plain path: %r" % name)
    if any(part.lower() in FORBIDDEN_FOLDERS for part in parts[:-1]):
        raise Refused("under a util or scripts folder: %r" % name)
    if strip and not name.startswith(strip):
        raise Refused("outside %s: %r" % (strip, name))


def extract(archive, names, destination, strip="", sha256=None):
    """the names (archive paths) written under destination with strip left off each; returns the paths written"""
    names = list(names)
    if not names:
        raise Refused("the list of names is empty")
    seen = set()
    for name in names:
        _check_name(name, strip)
        if name.lower() in seen:
            raise Refused("listed twice: %r" % name)
        seen.add(name.lower())
    if os.path.isdir(destination) and any(os.scandir(destination)):
        raise Refused("the destination already holds files: %s" % destination)
    if sha256 is not None and sha256_file(archive) != sha256:
        raise Refused("the archive is not the one the recipe names (SHA-256)")
    with zipfile.ZipFile(archive) as opened:
        have = set(opened.namelist())
        for name in names:
            if name not in have:
                raise Refused("not in the archive: %r" % name)
        written = []
        for name in names:
            relative = name[len(strip):]
            path = os.path.join(destination, *relative.split("/"))
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with opened.open(name) as source, open(path, "wb") as out:
                for chunk in iter(lambda: source.read(1 << 20), b""):
                    out.write(chunk)
            written.append(relative)
    return written


def main(arguments):
    options = {"--strip": "", "--sha256": None}
    plain = []
    index = 0
    while index < len(arguments):
        if arguments[index] in options and index + 1 < len(arguments):
            options[arguments[index]] = arguments[index + 1]
            index += 2
        else:
            plain.append(arguments[index])
            index += 1
    if len(plain) != 3:
        print(__doc__.strip().splitlines()[-2], file=sys.stderr)
        return 2
    with open(plain[1]) as file:
        names = [line.rstrip("\n") for line in file if line.strip()]
    try:
        written = extract(plain[0], names, plain[2], strip=options["--strip"], sha256=options["--sha256"])
    except Refused as refusal:
        print("extract_allowlist: refused: %s" % refusal, file=sys.stderr)
        return 1
    print("%d files unpacked" % len(written))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
