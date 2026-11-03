"""Ninja rules for the native 64-bit Linux build (``ninja linux64``).

The 32-bit Linux build's game and platform layer (tools/linux_build.py,
port/linux), compiled as native x86-64 code as the macOS build is
(tools/lp64_build.py: HALO_64BIT and the `long` rewrite), with glibc, the
system's SDL3 and desktop OpenGL. It links build/linux64/halo, which is
also a dedicated server (server/README.md) with the game browser.

As the 32-bit build, it honours --lto and --portable; it is not optimised
with a profile (the committed profiles are the 32-bit builds').
"""

from pathlib import Path
from typing import Any, List, Optional

from .embed_assets import hud_assets_build, hud_configure_inputs, server_ui_build, ui_fonts_build
from .linux_build import OPTIMISATION, ffmpeg_build, lto_flags, march_flag
from .lp64_build import (
    LINUX_PORT_CONFIG,
    Lp64Build,
    Lp64Host,
    load_json,
    lp64_configure_inputs,
    lp64_excluded,
    lp64_game_flags,
)
from .ninja_syntax import Writer
from .voice_assets import voices_build

LINUX64_TARGET = "--target=x86_64-linux-gnu"

# glibc restricted to ISO C by __STRICT_ANSI__, as in the 32-bit build, but
# C99's (fabsf, snprintf): an undeclared function is an error here, and
# Apple's ISO C has them too
LINUX64_GAME_FLAGS = lp64_game_flags(["-D_ISOC99_SOURCE"])

# Platform files named posix_*.c talk to glibc only, with the host's own ABI
# and no rewrite (their boundary types are posix.h's).
LINUX64_POSIX_FLAGS = [
    "-std=gnu11",
    "-D_GNU_SOURCE",
    "-D_FILE_OFFSET_BITS=64",
    OPTIMISATION,
    "-g",
    "-Wall",
    "-Werror=incompatible-pointer-types",
    "-Werror=int-conversion",
]


def linux64_configure_inputs() -> List[Path]:
    """Files whose change must re-run configure.py."""
    if not LINUX_PORT_CONFIG.is_file():
        return [Path(__file__)]
    return [Path(__file__), *lp64_configure_inputs(), *hud_configure_inputs()]


def generate_linux64_build(n: Writer, sln: Any) -> None:
    if not LINUX_PORT_CONFIG.is_file():
        # a checkout without the port (or a test fixture): nothing to emit
        return
    config = load_json(LINUX_PORT_CONFIG)
    build_dir: Path = sln.build_dir / "linux64"
    output = build_dir / "halo"
    cc = sln.linux_cc or "clang"

    n.comment("Native 64-bit Linux build (ninja linux64)")
    lp64 = Lp64Build(n, sln, "linux64", cc)
    # the movies' FFmpeg, for this target (configure.py --without-ffmpeg: the
    # stubs, and the game skips the movies). (AE) Built against the build
    # machine's system: Arena Evolved's portable 64-bit build is
    # tools/linux64_portable_deps.py's, not a Debian system root
    ffmpeg_cflags: List[str] = []
    ffmpeg_libs: List[str] = []
    ffmpeg_inputs: List[Path] = []
    if getattr(sln, "with_ffmpeg", True):
        ffmpeg_cflags, ffmpeg_libs, ffmpeg_stamp, ffmpeg_archives = ffmpeg_build(
            n, "linux64", build_dir / "ffmpeg", cc, "linux", "x86_64", [LINUX64_TARGET])
        ffmpeg_inputs = [ffmpeg_stamp, *ffmpeg_archives]
    n.rule(
        name="linux64_link",
        command=(
            "$linux64_cc $ldflags -o $out @$out.rsp $libs"
            " && $python tools/linux_link_check.py $out.rsp $out"
        ),
        description="LINUX64 LINK $out",
        rspfile="$out.rsp",
        rspfile_content="$in_newline",
    )
    generated_sources = (hud_assets_build(n, "linux64", build_dir / "generated" / "hud_hires_assets.c")
                         + ui_fonts_build(n, "linux64", build_dir / "generated" / "ui_fonts.c", sln)
                         # a dedicated server's admin page (port/assets/server_ui; port/linux/src/server_http.c)
                         + server_ui_build(n, "linux64", build_dir / "generated" / "server_ui_assets.c"))
    host = Lp64Host(
        name="linux64",
        target_flags=[LINUX64_TARGET, march_flag(sln)],
        game_flags=LINUX64_GAME_FLAGS,
        posix_flags=LINUX64_POSIX_FLAGS,
        # (no loop in them turned into glibc's wcslen, which
        # linux_link_check.py rejects: the game's wchar_t is 16-bit)
        third_party_flags=["-fno-builtin-wcslen"],
        excluded=set(lp64_excluded()),
        ffmpeg_cflags=ffmpeg_cflags,
    )
    # Link-time optimisation of the units with the Xbox's ABI, as in the
    # 32-bit build (the host-ABI units have a 32-bit wchar_t and stay native
    # objects)
    lto_cflags, lto_ldflags = lto_flags(sln, build_dir / "thinlto-cache")
    objects = lp64.objects(host, generated_sources, build_dir / "obj", lto_cflags, ffmpeg_inputs)
    # (AE) the portable build's libraries beside the executable, the SDL3 of
    # the portable download (tools/ci_build.py, tools/linux64_portable_deps.py):
    # DT_RPATH, not RUNPATH, so that it comes before a LD_LIBRARY_PATH, as
    # the 32-bit portable build has it (tools/linux_build.py). Harmless
    # where nothing is beside the executable: the system's SDL3 is used.
    rpath = ["'-Wl,-rpath,$$ORIGIN'", "-Wl,--disable-new-dtags"] if getattr(sln, "port_portable", False) else []
    n.build(
        outputs=output,
        rule="linux64_link",
        inputs=objects,
        variables={
            # (not position-independent, as the 32-bit build: the addresses
            # of a crash report's calls, in debug.txt, are the executable's
            # own, the same from run to run)
            "ldflags": " ".join([LINUX64_TARGET, "-g", "-no-pie", *lto_ldflags, *rpath,
                                     # (posix_trace_marker.c's, which the GPU driver's calls must reach)
                                     *(f"-Wl,--export-dynamic-symbol={name}"
                                       for name in ("open", "open64", "openat", "openat64"))]),
            "libs": " ".join([*ffmpeg_libs, *(f"-l{lib}" for lib in config.get("libraries", []))]),
        },
        implicit=[Path("tools/linux_link_check.py"), *ffmpeg_inputs],
    )
    # internet play's MQTT brokers, a file beside the game (network.brokers_file),
    # as the 32-bit build has it (linux_build.py)
    brokers = build_dir / "brokers.txt"
    n.rule(name="linux64_copy", command="cp $in $out", description="LINUX64 COPY $out")
    n.build(outputs=brokers, rule="linux64_copy", inputs=Path("port/assets/network/brokers.txt"))
    # the master servers of dedicated servers, a file beside the game too
    # (network.masters_file, server.masters_file)
    masters = build_dir / "masters.txt"
    n.build(outputs=masters, rule="linux64_copy", inputs=Path("port/assets/network/masters.txt"))
    voices = voices_build(n, "linux64_copy", build_dir)
    n.build(outputs="linux64", rule="phony", inputs=[output, brokers, masters, *voices])
    n.newline()
