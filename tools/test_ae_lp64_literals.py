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
# (AE's own wrappers of printf too: answer, tell_players, ae_dev_event and the service record's put take a format and
# arguments, as the game's network_event does; and the earlier literals of a call that goes on in the next line,
# "...; " in them, are taken out first)
PRINTF_CALL = re.compile(r"\b(?:\w*(?:printf|scanf|_log)|error|answer|tell_players|ae_dev_event|network_event|put)"
                         r"\s*\([^;{}]*$")
LITERAL = re.compile(r'"(?:[^"\\\n]|\\.)*"')


def ae_sources():
    for folder in (ROOT / "port" / "linux" / "game", ROOT / "port" / "linux" / "src"):
        yield from sorted(folder.glob("ae_*.c"))


def changed_literals(text):
    """(literal, the statement's text before it) for each string literal the rewrite would change"""
    # (the statement starts after a ; { or } outside the literals: a format's own "; " is not an end)
    masked = LITERAL.sub(lambda found: '"' + " " * (len(found.group(0)) - 2) + '"', text)
    for match in lp64.TOKEN.finditer(text):
        literal = match.group("string")
        if literal and lp64.rewrite_format(literal) != literal:
            start = max(masked.rfind(";", 0, match.start()), masked.rfind("{", 0, match.start()),
                        masked.rfind("}", 0, match.start())) + 1
            yield literal, text[start:match.start()]


def test_ae_literals_survive_the_lp64_rewrite():
    bad = []
    for path in ae_sources():
        text = lp64.read(path)
        for literal, before in changed_literals(text):
            if not PRINTF_CALL.search(LITERAL.sub('""', before)):
                bad.append(f"{path.relative_to(ROOT)}: {literal}")
    assert not bad, "string literals the 64-bit rewrite would change (split them into two literals):\n" + "\n".join(bad)


def test_the_guard_sees_the_case():
    """the case that bit the gallery: text, not a call; and a real format in a call, which passes"""
    text = ('draw("165 FPS 1% low 142");\nplatform_log("%ld items", count);\n'
            'error(_error_silent, "error %lu", value);\nfscanf(file, "%ld", &value);\n')
    found = list(changed_literals(text))
    assert len(found) == 4
    assert not PRINTF_CALL.search(found[0][1])
    assert all(PRINTF_CALL.search(before) for literal, before in found[1:])
