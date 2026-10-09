"""AE's text is safe from the 64-bit builds' source rewrite (tools/lp64_rewrite.py): it rewrites the printf length
modifier `l` out of every string literal it sees ("%ld" -> "%d"), but its pattern also takes text for a conversion
("1% low" reads as "% lo": a space flag, then `l` and `o`, and comes out "1% ow"). Every string literal in AE's own C
files (port/linux/game/ae_*.c, port/linux/src/ae_*.c) must come out of rewrite_format unchanged, except a real format
in a printf-style call (printf, snprintf, sprintf, fprintf, platform_log, ...), where the rewrite is what's wanted."""
import importlib.util
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
_spec = importlib.util.spec_from_file_location("ae_lp64_rewrite", ROOT / "tools" / "lp64_rewrite.py")
lp64 = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(lp64)

# a printf-style call: its name, then an open parenthesis, before the literal in the same statement
PRINTF_CALL = re.compile(r"\b\w*(?:printf|_log)\s*\([^;{}]*$")


def ae_sources():
    for folder in (ROOT / "port" / "linux" / "game", ROOT / "port" / "linux" / "src"):
        yield from sorted(folder.glob("ae_*.c"))


def changed_literals(text):
    """(literal, the statement's text before it) for each string literal the rewrite would change"""
    for match in lp64.TOKEN.finditer(text):
        literal = match.group("string")
        if literal and lp64.rewrite_format(literal) != literal:
            start = max(text.rfind(";", 0, match.start()), text.rfind("{", 0, match.start()),
                        text.rfind("}", 0, match.start())) + 1
            yield literal, text[start:match.start()]


def test_ae_literals_survive_the_lp64_rewrite():
    bad = []
    for path in ae_sources():
        text = lp64.read(path)
        for literal, before in changed_literals(text):
            if not PRINTF_CALL.search(before):
                bad.append(f"{path.relative_to(ROOT)}: {literal}")
    assert not bad, "string literals the 64-bit rewrite would change (split them into two literals):\n" + "\n".join(bad)


def test_the_guard_sees_the_case():
    """the case that bit the gallery: text, not a call; and a real format in a call, which passes"""
    text = 'draw("165 FPS 1% low 142");\nplatform_log("%ld items", count);\n'
    found = list(changed_literals(text))
    assert len(found) == 2
    assert not PRINTF_CALL.search(found[0][1]) and PRINTF_CALL.search(found[1][1])
