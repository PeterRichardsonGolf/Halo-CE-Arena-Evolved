"""Every AE hook listed in port/linux/game/ae_hooks.txt is present in its upstream file (a merge that drops one,
or a line of one, fails here, not in play), and every line marked as an AE hook in an upstream file is listed."""
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LIST = ROOT / "port" / "linux" / "game" / "ae_hooks.txt"
MARK = "/* AE hook */"
# where upstream code lives; AE's own files (ae_*) and tests are not upstream
SCANNED = ("source", "port")
SUFFIXES = (".c", ".h", ".cpp", ".m", ".mm")


def hook_lines(needle):
    """A hook's lines: written joined by a literal \\n, each with its indentation trimmed."""
    return [part.strip() for part in needle.split("\\n")]


def normalised(text):
    return "\n".join(line.strip() for line in text.splitlines())


def present(needle, text):
    """Whether the hook's lines are whole lines of the text, following each other (indentation aside)."""
    return "\n" + "\n".join(hook_lines(needle)) + "\n" in "\n" + normalised(text) + "\n"


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


def test_hook_lines_must_follow_each_other():
    hook = "if (ae_ui_process()) /* AE hook */\\nreturn;"
    assert present(hook, "\tif (ae_ui_process()) /* AE hook */\n\t\treturn;\n")
    # a merge that kept the if but lost (or moved) its body
    assert not present(hook, "\tif (ae_ui_process()) /* AE hook */\n#ifdef HALO_GAME_BROWSER\n\t\treturn;\n")
    assert not present(hook, "\tif (ae_ui_process()) /* AE hook */\n")
    # whole lines only: not the end of a longer first line, nor the start of a longer last one
    assert not present(hook, "\t/* if (ae_ui_process()) /* AE hook */\n\t\treturn;\n")
    assert not present(hook, "\tif (ae_ui_process()) /* AE hook */\n\t\treturn; /* and more */\n")


def test_hooks_present():
    missing = [f"{path}: {needle}" for path, needle in hooks()
               if not present(needle, (ROOT / path).read_text(errors="replace"))]
    assert not missing, "AE hooks missing (re-attach them, see notes/ae-hooks.md):\n" + "\n".join(missing)


def test_marked_hooks_listed():
    listed = {}
    for path, needle in hooks():
        listed.setdefault(path, set()).update(hook_lines(needle))
    unlisted = []
    for path in upstream_files():
        text = path.read_text(errors="replace")
        if MARK not in text:
            continue
        relative = path.relative_to(ROOT).as_posix()
        for number, line in enumerate(text.splitlines(), 1):
            if MARK in line and line.strip() not in listed.get(relative, set()):
                unlisted.append(f"{relative}:{number}: {line.strip()}")
    assert not unlisted, "AE hook lines not listed in ae_hooks.txt:\n" + "\n".join(unlisted)
