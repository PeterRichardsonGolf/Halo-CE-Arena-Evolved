#!/usr/bin/env python3
"""(AE) The SDL3 of the portable 64-bit Linux download, built from source on
Ubuntu 22.04 (the CI job `linux64-portable`, .github/workflows/build.yml).

Ubuntu 22.04 and Debian 12 have no SDL3 package, and the download must not
depend on the system's, so SDL3 is built here, at the release the other
ports use (tools/linux_sysroot.py has its version and SHA-256, and the
drivers it must have been built with), and goes into the zip beside halo
(tools/ci_build.py, HALO_BUNDLE_SDL3; the executable's rpath is $ORIGIN).

    python3 tools/linux64_portable_deps.py <prefix>

builds libdecor's headers and library (Wayland window decorations: Ubuntu
22.04's libdecor is 0.1.0, SDL3 needs 0.2.0 to build; the library is only
read for its name, SDL loads the system's libdecor when it starts), then
SDL3 against it, into <prefix>: <prefix>/include, <prefix>/lib/libSDL3.so.0
and <prefix>/SDL3-LICENSE.txt. A <prefix> already built (its stamp) is left
as it is. The compile needs <prefix>/include on CPATH and <prefix>/lib on
LIBRARY_PATH (the workflow sets them).

The build needs, from apt: cmake meson ninja-build pkg-config
wayland-protocols libwayland-dev libxkbcommon-dev libx11-dev libxext-dev
libxcursor-dev libxi-dev libxfixes-dev libxrandr-dev libxrender-dev
libxss-dev libgl-dev libegl-dev libpipewire-0.3-dev libpulse-dev
libasound2-dev libudev-dev libdbus-1-dev libcairo2-dev libpango1.0-dev
(docs/dependencies.md).
"""

import io
import os
import shutil
import subprocess
import sys
import tarfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

from tools.linux_build import SDL_OPTIONS  # noqa: E402
from tools.linux_sysroot import SDL_SHA256, SDL_URLS, SDL_VERSION, check_sdl, download, extract  # noqa: E402

LIBDECOR_VERSION = "0.2.2"
LIBDECOR_URLS = [
    f"https://gitlab.freedesktop.org/libdecor/libdecor/-/archive/{LIBDECOR_VERSION}/libdecor-{LIBDECOR_VERSION}.tar.gz",
]
LIBDECOR_SHA256 = "40a1d8be07d8b1f66e8fb98a1f4a84549ca6bf992407198a5055952be80a8525"


def run(command, **kwargs):
    print("+", " ".join(str(part) for part in command), flush=True)
    subprocess.run([str(part) for part in command], check=True, **kwargs)


def fetch(urls, digest, destination: Path) -> None:
    if destination.exists():
        shutil.rmtree(destination)
    destination.mkdir(parents=True)
    with tarfile.open(fileobj=io.BytesIO(download(urls, digest)), mode="r:gz") as archive:
        extract(archive, destination, strip=1)


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    prefix = Path(sys.argv[1]).resolve()
    stamp = prefix / "stamp"
    wanted = f"SDL3 {SDL_VERSION} {SDL_SHA256}\nlibdecor {LIBDECOR_VERSION} {LIBDECOR_SHA256}\n"
    if stamp.is_file() and stamp.read_text() == wanted:
        print(f"{prefix}: built already", flush=True)
        return 0
    work = prefix.parent / (prefix.name + "-work")
    for folder in (prefix, work):
        if folder.exists():
            shutil.rmtree(folder)
    prefix.mkdir(parents=True)
    environment = dict(os.environ, PKG_CONFIG_PATH=f"{prefix}/lib/pkgconfig", CC=os.environ.get("CC", "clang"))

    fetch(LIBDECOR_URLS, LIBDECOR_SHA256, work / "libdecor")
    run(["meson", "setup", work / "libdecor-build", work / "libdecor", f"--prefix={prefix}", "--libdir=lib",
         "--buildtype=release", "-Ddemo=false", "-Dgtk=disabled"], env=environment)
    run(["ninja", "-C", work / "libdecor-build", "install"])

    fetch(SDL_URLS, SDL_SHA256, work / "SDL3")
    # (no --fresh: Ubuntu 22.04's CMake 3.22 predates it; a clean build folder instead)
    shutil.rmtree(work / "SDL3-build", ignore_errors=True)
    run(["cmake", "-S", work / "SDL3", "-B", work / "SDL3-build", "-G", "Ninja",
         f"-DCMAKE_INSTALL_PREFIX={prefix}", f"-DCMAKE_PREFIX_PATH={prefix}", "-DCMAKE_INSTALL_LIBDIR=lib",
         f"-DCMAKE_C_COMPILER={environment['CC']}", *SDL_OPTIONS], env=environment)
    run(["ninja", "-C", work / "SDL3-build", "install"])
    # the drivers a desktop needs (X11, Wayland, PipeWire, PulseAudio, ALSA,
    # udev for the controllers), each loaded when SDL starts
    check_sdl(work / "SDL3-build" / "include-config-release" / "build_config" / "SDL_build_config.h")
    shutil.copy2(work / "SDL3" / "LICENSE.txt", prefix / "SDL3-LICENSE.txt")
    if not (prefix / "lib" / "libSDL3.so.0").exists():
        print("error: no libSDL3.so.0 was built", file=sys.stderr)
        return 1
    shutil.rmtree(work)
    stamp.write_text(wanted)
    return 0


if __name__ == "__main__":
    sys.exit(main())
