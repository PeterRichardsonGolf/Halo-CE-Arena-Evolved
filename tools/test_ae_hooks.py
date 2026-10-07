"""Every AE hook line listed in port/linux/game/ae_hooks.txt is present in its upstream file (a merge that drops
one fails here, not in play), and every line marked as an AE hook in an upstream file is listed."""
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LIST = ROOT / "port" / "linux" / "game" / "ae_hooks.txt"
MARK = "/* AE hook */"
# where upstream code lives; AE's own files (ae_*) and tests are not upstream
SCANNED = ("source", "port")
SUFFIXES = (".c", ".h", ".cpp", ".m", ".mm")


def hooks():
    for line in LIST.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        path, needle = line.split("\t", 1)
        yield path, needle


def upstream_files():
    for top in SCANNED:
        for path in (ROOT / top).rglob("*"):
            if path.suffix not in SUFFIXES or not path.is_file():
                continue
            if path.name.startswith("ae_") or "tests" in path.relative_to(ROOT).parts:
                continue
            yield path


def test_hooks_listed():
    assert list(hooks()), "ae_hooks.txt lists no hooks"


def test_hooks_present():
    missing = [f"{path}: {needle}" for path, needle in hooks()
               if needle not in (ROOT / path).read_text(errors="replace")]
    assert not missing, "AE hooks missing (re-attach them, see notes/ae-hooks.md):\n" + "\n".join(missing)


def test_marked_hooks_listed():
    listed = {}
    for path, needle in hooks():
        listed.setdefault(path, []).append(needle)
    unlisted = []
    for path in upstream_files():
        text = path.read_text(errors="replace")
        if MARK not in text:
            continue
        relative = path.relative_to(ROOT).as_posix()
        for number, line in enumerate(text.splitlines(), 1):
            if MARK in line and not any(needle in line for needle in listed.get(relative, [])):
                unlisted.append(f"{relative}:{number}: {line.strip()}")
    assert not unlisted, "AE hook lines not listed in ae_hooks.txt:\n" + "\n".join(unlisted)
