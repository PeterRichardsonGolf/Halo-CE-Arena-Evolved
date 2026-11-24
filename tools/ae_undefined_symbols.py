#!/usr/bin/env python3
"""Every symbol a game object references must be defined by another object of the link or by a library of it.

The Linux release link is link-time optimised: a call nothing defines can be optimised away, so the link succeeds
while the same tree fails to link on Windows (lld-link: undefined symbol). The objects of such a build are LLVM
bitcode whose symbol tables still list every reference, so this reads those tables (llvm-nm reads bitcode and native
objects alike) and checks each undefined name against:

  - the definitions of all objects the link names (taken from the link's build statement in build.ninja),
  - the symbols of the shared libraries and archives named by the link line's libs (libc, libm, SDL3, ffmpeg ...),
  - the compiler runtime (libclang_rt.builtins),
  - the names in tools/ae_undefined_symbols_allow.txt (symbols the linker or the loader supply, one reason each).

Usage: ae_undefined_symbols.py <build dir, e.g. build/linux64> [--allow FILE] [--target halo]
Prints "symbol  referenced by object.o" for each offender; exit 1 if any, 2 if the inputs cannot be read.
"""
import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_ALLOW = ROOT / "tools" / "ae_undefined_symbols_allow.txt"
ROOT_OF = [ROOT]
NM = shutil.which("llvm-nm") or "nm"
CHUNK = 400


def logical_lines(text):
    """build.ninja lines with '$' continuations joined (the next line's indentation becomes one space)."""
    joined = []
    pending = None
    for line in text.splitlines():
        if pending is not None:
            line = pending + " " + line.strip()
            pending = None
        if line.endswith("$"):
            pending = line[:-1].rstrip()
            continue
        joined.append(line)
    return joined


def words(text):
    """Split a ninja line on unescaped spaces; '$ ' is a space in a path, '$:' a colon, '$$' a dollar."""
    return [re.sub(r"\$([ :$])", r"\1", word) for word in re.split(r"(?<!\$) +", text.strip()) if word]


def link_statement(ninja_text, output):
    """(inputs, variables) of the build statement for `output`."""
    lines = logical_lines(ninja_text)
    for index, line in enumerate(lines):
        if not line.startswith("build " + output + ":"):
            continue
        inputs = words(re.split(r"(?<!\$):", line, maxsplit=1)[1])[1:]
        inputs = inputs[:inputs.index("|")] if "|" in inputs else inputs
        variables = {}
        for follow in lines[index + 1:]:
            if not follow.startswith(" "):
                break
            key, _, value = follow.strip().partition(" = ")
            variables[key] = value
        return inputs, variables
    raise SystemExit(f"no build statement for {output} in build.ninja")


def run_nm(args, files):
    """(object-or-archive-member, symbol, type letter) for every symbol of the files."""
    rows = []
    for start in range(0, len(files), CHUNK):
        part = [str(f) for f in files[start:start + CHUNK]]
        out = subprocess.run([NM, "--format=posix", *args, *part], capture_output=True, text=True)
        if out.returncode != 0 and not out.stdout:
            raise SystemExit("nm failed: " + out.stderr.strip()[:300])
        current = part[0] if len(part) == 1 else ""
        for line in out.stdout.splitlines():
            fields = line.split()
            if not fields:
                continue
            if fields[0].endswith(":") and len(fields) <= 2:
                current = fields[0][:-1]
                continue
            if len(fields) >= 2 and len(fields[1]) == 1:
                rows.append((current, fields[0].split("@")[0], fields[1]))
    return rows


def resolve_script(path):
    """The real library files behind a path: a linker script (libm.so) names them, an ELF or archive is itself."""
    with open(path, "rb") as handle:
        head = handle.read(8)
    if head.startswith(b"\x7fELF") or head.startswith(b"!<arch>"):
        return [path]
    names = re.findall(r"(/\S+\.(?:so[.\d]*|a))\b", path.read_text(errors="replace"))
    return [Path(n).resolve() for n in names if Path(n).exists()]


def library_files(libs, cc):
    """Shared libraries and archives the link's libs name: -lX resolved by the compiler, .a paths as written."""
    files = []
    for token in libs.split():
        if token.startswith("-l"):
            for suffix in (".so", ".a"):
                found = subprocess.run([cc, "--target=x86_64-linux-gnu", f"-print-file-name=lib{token[2:]}{suffix}"],
                                       capture_output=True, text=True).stdout.strip()
                if found and Path(found).is_absolute() and Path(found).exists():
                    files.extend(resolve_script(Path(found).resolve()))
                    break
            else:
                print(f"warning: {token}: no library found", file=sys.stderr)
        elif token.endswith(".a"):
            files.append(Path(token) if Path(token).is_absolute() else ROOT_OF[0] / token)
    # libc (its linker script also names libc_nonshared.a: atexit) is linked even when the line names only -lm
    for name in ("libc.so", "libc.so.6"):
        found = subprocess.run([cc, "--target=x86_64-linux-gnu", f"-print-file-name={name}"],
                               capture_output=True, text=True).stdout.strip()
        if Path(found).is_absolute() and Path(found).exists():
            files.extend(resolve_script(Path(found).resolve()))
            break
    runtime = subprocess.run([cc, "--target=x86_64-linux-gnu", "-print-libgcc-file-name"],
                             capture_output=True, text=True).stdout.strip()
    if runtime and Path(runtime).exists():
        files.append(Path(runtime))
    return list(dict.fromkeys(files))


def read_allow(path):
    names = {}
    for line in Path(path).read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        name, _, reason = line.partition("#")
        names[name.strip()] = reason.strip()
    return names


def find_offenders(build_dir, allow_file=DEFAULT_ALLOW, target="halo"):
    build_dir = Path(build_dir).resolve()
    ninja = None
    for candidate in (build_dir.parent.parent / "build.ninja", ROOT / "build.ninja"):
        if candidate.exists():
            ninja = candidate
            break
    if ninja is None:
        raise SystemExit("build.ninja not found above " + str(build_dir))
    root = ninja.parent
    ROOT_OF[0] = root
    rel = build_dir.relative_to(root)
    inputs, variables = link_statement(ninja.read_text(), f"{rel}/{target}")
    objects = [root / p for p in inputs if p.endswith(".o")]
    archives = [root / p for p in inputs if p.endswith(".a")]
    missing = [o for o in objects if not o.exists()]
    if not objects or missing:
        raise SystemExit(f"link objects missing (build first): {len(missing)} of {len(objects)}, e.g. "
                         + (str(missing[0]) if missing else "none listed"))
    cc = "clang"
    libs = library_files(variables.get("libs", ""), cc) + archives
    absent = [f for f in libs if not f.exists()]
    if absent:
        raise SystemExit("link libraries missing (build first): " + str(absent[0]))
    defined = set()
    for rows, ignore in ((run_nm(["--defined-only"], objects), None),
                         (run_nm(["--defined-only", "-D"], [f for f in libs if f.suffix == ".so" or ".so." in f.name]), None),
                         (run_nm(["--defined-only"], [f for f in libs if f.suffix == ".a"]), None)):
        defined.update(name for _, name, _ in rows)
    allow = read_allow(allow_file)
    offenders = {}
    for obj, name, kind in run_nm(["--undefined-only"], objects):
        if kind in ("w", "v"):
            continue  # weak references resolve to 0 by design; tools/linux_link_check.py checks the MSVC-inline ones
        if name in defined or name in allow:
            continue
        offenders.setdefault(name, set()).add(Path(obj).name)
    return offenders


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("build_dir")
    parser.add_argument("--allow", default=str(DEFAULT_ALLOW))
    parser.add_argument("--target", default="halo")
    args = parser.parse_args()
    offenders = find_offenders(args.build_dir, args.allow, args.target)
    for name in sorted(offenders):
        print(f"{name}  referenced by {', '.join(sorted(offenders[name]))}")
    if offenders:
        print(f"{len(offenders)} undefined symbol(s): defined by no object or library of the link", file=sys.stderr)
        return 1
    print("no undefined symbols")
    return 0


if __name__ == "__main__":
    sys.exit(main())
