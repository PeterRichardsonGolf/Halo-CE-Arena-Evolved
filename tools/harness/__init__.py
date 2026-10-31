"""Asset-free regression tests: the engine's real code compiled against a fake world (README.md)."""

import functools
import os
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
HARNESS = Path(__file__).resolve().parent


def read(relative):
    """A source file of the repository, as the compiler reads it."""
    return (ROOT / relative).read_text(encoding="latin-1")


def function(source, name):
    """A function's definition (static, inline or not), taken by matching braces; LookupError if not found."""
    match = re.search(r"^(?:static |__inline )?[\w *]+?\b" + re.escape(name) + r"\(\s*[^;{]*\)\s*\{", source, re.M)
    if not match:
        raise LookupError(f"function not found in the sources: {name}")
    end = source.index("{", match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


def inline(source, name):
    """An __inline header function, as a static one."""
    return function(source, name).replace("__inline", "static", 1)


def enum_with(source, member):
    """The whole enum that declares a member."""
    at = source.index(member)
    start = source.rindex("enum", 0, at)
    return source[start:source.index("};", at) + 2]


def structure(source, name):
    """An actual named structure's definition, including nested unions/structures."""
    match = re.search(r"^struct\s+" + re.escape(name) + r"\s*\{", source, re.M)
    if not match:
        raise LookupError(f"structure not found in the sources: {name}")
    end = source.index("{", match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():source.index(";", end) + 1]


# the console ports' macros (ChupathingyCE's port/xbox and the rest): never defined by the desktop builds the
# harness stands for, so a constant's console-only definition is not the one it takes
CONSOLE_MACROS = re.compile(r"\bHALO_(?:CONSOLE_\w+|XBOX_CONSOLE|SWITCH_CONSOLE|WINDOW_REBASE|WINDOW_TRANSLATED)\b")


def desktop_source(source):
    """The source without the branches only a console build compiles: a conditional whose macros are all console
    ones is taken as false (its #else as true); any other conditional keeps both its branches."""
    out, stack = [], []
    for line in source.splitlines(keepends=True):
        directive = re.match(r"\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)", line)
        if directive:
            kind, rest = directive.group(1), directive.group(2)
            names = re.findall(r"\b[A-Za-z_]\w*\b", rest.replace("defined", " "))
            console = bool(names) and all(CONSOLE_MACROS.fullmatch(n) or n.isdigit() for n in names)
            if kind in ("if", "ifdef", "ifndef"):
                # (None: undecided, both branches kept)
                value = None if not console else (kind == "ifndef")
                stack.append([value, value is True])
            elif kind == "elif" and stack:
                if stack[-1][1]:
                    stack[-1][0] = False
                else:
                    stack[-1][0] = None if not console else False
            elif kind == "else" and stack:
                if stack[-1][0] is not None:
                    stack[-1][0] = not stack[-1][1] and not stack[-1][0]
            elif kind == "endif" and stack:
                stack.pop()
            if stack and stack[-1][0] is True:
                stack[-1][1] = True
            continue
        if all(frame[0] is not False for frame in stack):
            out.append(line)
    return "".join(out)


def constant(source, name):
    """An enum constant's or #define's integer value, following a name it is set to (as the port's capacities are),
    as the desktop builds have it (desktop_source)."""
    source = desktop_source(source)
    match = re.search(r"\b" + re.escape(name) + r"\s*=\s*(\w+)", source) or \
        re.search(r"#define\s+" + re.escape(name) + r"\s+(\w+)", source)
    if not match:
        raise LookupError(f"constant not found in the sources: {name}")
    value = match.group(1)
    if re.fullmatch(r"0x[0-9A-Fa-f]+|\d+", value):
        return int(value, 0)
    return constant(source + read("port/linux/include/halo_port_capacity.h"), value)


def mutated(text, before, after):
    """The code with one deliberate fault (a negative control); LookupError once the code no longer has it."""
    if text.count(before) != 1:
        raise LookupError(f"negative control no longer applies (found {text.count(before)} times): {before!r}")
    return text.replace(before, after, 1)


_directory = tempfile.TemporaryDirectory(prefix="halo-harness-")


CHECK_FAILED = 1  # a case's exit status when a CHECK fails (harness.h)


@functools.lru_cache(maxsize=None)
def build(test, generated):
    """tests/<test>.c compiled with the generated includes ((name, text), ...) for the game's 32-bit ABI."""
    work = Path(_directory.name) / f"{test}-{abs(hash(generated)):x}"
    work.mkdir(parents=True, exist_ok=True)
    for name, text in generated:
        (work / name).write_text(text)
    executable = work / test
    command = [os.environ.get("CC", "clang"), "-m32", "-std=gnu99", "-O2", "-Wall", "-Werror", "-Wno-unused-function",
               "-Wno-unused-variable", "-I", str(HARNESS / "include"), "-I", str(work),
               str(HARNESS / "tests" / f"{test}.c"), "-o", str(executable), "-lm"]
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f"{test}.c does not compile:\n{result.stderr}")
    return executable


def run(executable, case):
    """Run one case: (exit status, output)."""
    result = subprocess.run([str(executable), case], capture_output=True, text=True, errors="replace", timeout=60)
    return result.returncode, (result.stdout + result.stderr).strip()
