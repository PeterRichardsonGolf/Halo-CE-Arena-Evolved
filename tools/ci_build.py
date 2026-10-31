#!/usr/bin/env python3
"""Builds one port in one configuration, as the GitHub workflow does
(.github/workflows/build.yml), and collects what it built into dist/:

    python tools/ci_build.py linux debug
    python tools/ci_build.py android release
    python tools/ci_build.py server-x64 release --alpine
    python tools/ci_build.py linux profile
    python tools/ci_build.py web release

Builds are portable (any x86-64 processor; for the arm64 server, any
64-bit ARM), so they run on other computers. Debug builds skip link-time and profile-guided optimisation,
which only make the build slower; release builds use both, as a local
release build does (profile-guided optimisation needs clang 22 or later,
and is skipped with an older one). A profile build is a debug build with
configure.py --profile, so that the profiling build is built too.
CI_COMPILER_LAUNCHER (ccache, say) is passed on as --compiler-launcher.
(AE) HALO_BUNDLE_SDL3=<folder> puts that folder's libSDL3.so.0 and SDL3
license into the linux64 download (the portable one: the workflow's
linux64-portable job).

The version comes from the environment (tools/version.py): HALO_VERSION
(0.1.0-beta, or 0.1.0-beta-nightly.42) when set; VERSION's own for the
workflow's build of the release's tag, v<VERSION>, which is also a
release's (HALO_RELEASE_BUILD, else that tag build: its updater looks for
newer releases, port/linux/src/updater.c). Any other build is VERSION's
-dev, and never looks for updates.

The Android app is signed with a release key when
port/android/keystore.properties is there (a release workflow writes it;
port/android/README.md): arena-evolved-android-<config>.apk. Without it
(pull requests, forks), the debug build has the runner's own key,
arena-evolved-android-debug-testkey.apk, which installs over nothing
signed with another key, and the release build is unsigned,
arena-evolved-android-release-unsigned.apk. A profile build's app is the
debug variant's.

The dedicated server (server-x86, server-x64, server-arm64:
tools/server_build.py) is built against musl, so that it is one static
program: on a musl system (Alpine Linux) as it is, or with --alpine in
Alpine's Docker container of the server's architecture (an x86 one runs on
an x86-64 machine, an arm64 one on an arm64 machine); it goes into
dist/chupathingyce-server-linux-<arch> (-debug for a debug build), with
its README and playlists. A release's server there is stripped of its debug
information; a debug build's keeps it.

The web build (port/web/README.md, upstream's) needs Emscripten (emcc on
the PATH, as emsdk_env.sh puts it); dist/halo-web-<config>/ is the whole
site, which a static host serves as it is.

    python tools/ci_build.py sums release/

writes release/SHA256SUMS: a line for each file of the folder (the
release's assets: the workflow zips each build there), "<sha256 hex>  <name>",
as sha256sum writes it and `sha256sum -c` reads it. The workflow signs it
with the release key (tools/release_sign.sh: SHA256SUMS.sig) and publishes
both with the release.
"""

import argparse
import hashlib
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

from tools.version import base_version, commit, release_build, version  # noqa: E402

# what each port's build leaves, and what goes into dist/
OUTPUTS = {
    # (and the SDL3 the portable build brings, tools/linux_build.py, with its
    # zlib license, as the release carries the other libraries')
    "linux": ["build/linux/halo", "build/linux/libSDL3.so.0", "build/linux/SDL3-LICENSE.txt"],
    # the native 64-bit build (ninja linux64; tools/linux64_build.py)
    "linux64": ["build/linux64/halo"],
    "windows": ["build/windows/halo.exe", "build/windows/SDL3.dll"],
    # the native 64-bit build (ninja windows64; tools/windows_build.py)
    "windows64": ["build/windows64/halo.exe", "build/windows64/SDL3.dll"],
    "android": [],  # the APK, below
    # the application (universal and self-contained: --portable), whole
    "macos": ["build/macos/ArenaEvolved.app"],
    # the dedicated server (ninja server-<arch>; tools/server_build.py)
    "server-x86": ["build/server-x86/chupathingyce-server"],
    "server-x64": ["build/server-x64/chupathingyce-server"],
    "server-arm64": ["build/server-arm64/chupathingyce-server"],
    "web": [],  # the site, below
}
# the servers' Alpine Linux (--alpine), and Docker's name for each one's
# architecture
ALPINE_IMAGE = "alpine:3.22"
ALPINE_PACKAGES = ["clang", "lld", "llvm", "gcc", "musl-dev", "linux-headers", "python3", "samurai", "ccache"]
DOCKER_PLATFORMS = {"server-x86": "linux/386", "server-x64": "linux/amd64", "server-arm64": "linux/arm64"}
APKS = {
    "debug": "port/android/app/build/outputs/apk/debug/app-debug.apk",
    "release": "port/android/app/build/outputs/apk/release/app-release.apk",
}
# what Gradle calls the release build without a key (app/build.gradle)
UNSIGNED_APK = "port/android/app/build/outputs/apk/release/app-release-unsigned.apk"


def run(command, cwd=ROOT):
    print("+", " ".join(str(part) for part in command), flush=True)
    subprocess.run([str(part) for part in command], cwd=cwd, check=True)


def write_sums(folder: Path) -> int:
    """folder/SHA256SUMS: each file's SHA-256 and name, in name order (the
    sums file itself and its signature left out)"""
    lines = []
    for path in sorted(folder.iterdir(), key=lambda item: item.name):
        if not path.is_file() or path.name in ("SHA256SUMS", "SHA256SUMS.sig"):
            continue
        digest = hashlib.sha256()
        with path.open("rb") as file:
            for chunk in iter(lambda: file.read(1 << 20), b""):
                digest.update(chunk)
        lines.append(f"{digest.hexdigest()}  {path.name}\n")
    if not lines:
        print(f"{folder}: no files to sum", file=sys.stderr)
        return 1
    (folder / "SHA256SUMS").write_text("".join(lines), encoding="utf-8", newline="\n")
    print(f"{folder / 'SHA256SUMS'}: {len(lines)} files", flush=True)
    return 0


def main() -> int:
    if len(sys.argv) == 3 and sys.argv[1] == "sums":
        return write_sums(Path(sys.argv[2]))
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("platform", choices=sorted(OUTPUTS))
    parser.add_argument("config", choices=["debug", "release", "profile"])
    parser.add_argument("--alpine", action="store_true",
                        help="a server: build it in Alpine Linux's Docker container (musl, static)")
    args = parser.parse_args()
    server = args.platform.startswith("server-")
    if args.alpine:
        if not server:
            parser.error("--alpine builds the servers only")
        return alpine_build(args.platform, args.config)

    configure = [sys.executable, "configure.py", "--portable"]
    if server:
        # (the profiles are the 32-bit game's, which the servers' clang may
        # not read; a server's work is light)
        configure.append("--pgo=off")
    if args.config == "release":
        configure.append("--release")
    else:
        configure += ["--lto=off", "--pgo=off"]
    if args.config == "profile":
        configure.append("--profile")
    launcher = os.environ.get("CI_COMPILER_LAUNCHER")
    if launcher:
        configure += ["--compiler-launcher", launcher]
    # a release is VERSION's, and nothing else is one
    if release_build() and version() != base_version():
        print(f"error: a release build of {version()}, but VERSION is {base_version()}", file=sys.stderr)
        return 1
    print(f"version {version()}{' (a release)' if release_build() else ''}", flush=True)
    run(configure)

    if args.platform == "web":
        # the whole site (tools/web_build.py), its licences in licenses.txt
        run(["ninja", "web"])
        dist = ROOT / "dist" / f"halo-web-{args.config}"
        if dist.exists():
            shutil.rmtree(dist)
        shutil.copytree(ROOT / "build/web/site", dist)
        print(f"build/web/site -> {dist.relative_to(ROOT)}", flush=True)
        return 0
    apk_names = {}
    if args.platform == "android":
        # the native part, then the app around it (Gradle's variant of the
        # same name), named for its signature
        run(["ninja", "android"])
        gradlew = "gradlew.bat" if os.name == "nt" else "./gradlew"
        variant = "release" if args.config == "release" else "debug"
        for stale in (APKS[variant], UNSIGNED_APK):
            (ROOT / stale).unlink(missing_ok=True)
        run([gradlew, "--console=plain", "-q", f"assemble{variant.capitalize()}"], cwd=ROOT / "port/android")
        if (ROOT / "port/android/keystore.properties").exists():
            apk, name = APKS[variant], f"arena-evolved-android-{variant}.apk"
        elif variant == "debug":
            apk, name = APKS["debug"], "arena-evolved-android-debug-testkey.apk"
        else:
            apk, name = UNSIGNED_APK, "arena-evolved-android-release-unsigned.apk"
        outputs = [apk]
        apk_names[apk] = name
    else:
        run(["ninja", args.platform])
        outputs = OUTPUTS[args.platform]

    if server:
        return server_dist(args.platform, args.config, outputs[0])

    dist = ROOT / "dist" / f"arena-evolved-{args.platform}-{args.config}"
    if dist.exists():
        shutil.rmtree(dist)
    dist.mkdir(parents=True)
    for output in outputs:
        if (ROOT / output).is_dir():
            # (a bundle: its symbolic links as they are, for its signature)
            shutil.copytree(ROOT / output, dist / Path(output).name, symlinks=True)
        else:
            shutil.copy2(ROOT / output, dist / apk_names.get(output, Path(output).name))
        print(f"{output} -> {dist.relative_to(ROOT)}", flush=True)
    if args.platform == "macos":
        # (the SDL3 inside it, the build's own: zlib license)
        shutil.copy2(ROOT / "build/macos/third_party/SDL3/LICENSE.txt", dist / "SDL3-LICENSE.txt")
    if args.platform in ("windows", "windows64"):
        # the symbols of halo.exe and SDL3.dll, apart (players do not need
        # them): tools/symbolize_crash.py reads debug.txt's crash lines and
        # the crash reports' calls (port/windows/src/win32_crash.c) with
        # them, and a debugger the reports' minidumps; halo.map, the
        # linker's map, names the functions without LLVM's tools
        symbols = ROOT / "dist" / f"arena-evolved-{args.platform}-{args.config}-symbols"
        sdl_arch = "x64" if args.platform == "windows64" else "x86"
        if symbols.exists():
            shutil.rmtree(symbols)
        symbols.mkdir(parents=True)
        for pdb in [ROOT / f"build/{args.platform}/halo.pdb", ROOT / f"build/{args.platform}/halo.map",
                    *sorted((ROOT / "build/windows/third_party").glob(f"SDL3-*/lib/{sdl_arch}/SDL3.pdb"))]:
            # (AE: its workflow uploads no symbols: a PDB that is not there is
            # skipped, not a failed build)
            if not pdb.is_file():
                print(f"{pdb.relative_to(ROOT)}: not there; no symbols copied for it", flush=True)
                continue
            shutil.copy2(pdb, symbols)
            print(f"{pdb.relative_to(ROOT)} -> {symbols.relative_to(ROOT)}", flush=True)
    # OpenCE's own licence, the AGPL, which asks each copy to carry its text
    shutil.copy2(ROOT / "LICENSE", dist / "LICENSE.txt")
    # (and the GPL 3.0 of ChupathingyCE's code from its 0.8.0, which it is
    # combined under: README.md, "Licence")
    shutil.copy2(ROOT / "LICENSES/GPL-3.0.txt", dist / "GPL-3.0.txt")
    if args.platform != "web":
        # the game list's and the self-updater's TLS and internet play's DTLS
        # with browsers (port/third_party/mbedtls), in every native build,
        # whose Apache license asks the same
        shutil.copy2(ROOT / "port/third_party/mbedtls/LICENSE", dist / "mbedtls-LICENSE.txt")
    # internet play's UPnP (port/third_party/miniupnpc), in every build,
    # whose BSD license asks binaries to carry its notice
    shutil.copy2(ROOT / "port/third_party/miniupnpc/LICENSE", dist / "miniupnpc-LICENSE.txt")
    # the text's fonts (port/assets/fonts), embedded in every build, whose
    # SIL Open Font License asks each copy to carry it
    shutil.copy2(ROOT / "port/assets/fonts/Overpass-OFL.txt", dist / "Overpass-OFL.txt")
    shutil.copy2(ROOT / "port/assets/fonts/CyborgInterfaceGothic-OFL.txt", dist / "CyborgInterfaceGothic-OFL.txt")
    # (OpenCE, the menus' titles' face, which Online Games draws its title in)
    shutil.copy2(ROOT / "port/assets/fonts/OpenCE-OFL.txt", dist / "OpenCE-OFL.txt")
    # the overlay's fonts (port/linux/ui/fonts), in every build: Noto Sans's
    # license asks the same; Kenney's Input Prompts are CC0, credited all
    # the same
    shutil.copy2(ROOT / "port/linux/ui/fonts/OFL.txt", dist / "NotoSans-OFL.txt")
    shutil.copy2(ROOT / "port/linux/ui/fonts/KENNEY-CC0.txt", dist / "Kenney-Input-Prompts-CC0.txt")
    # the menus' XML parser (port/third_party/expat), in every build, whose
    # MIT license asks copies to carry its notice
    shutil.copy2(ROOT / "port/third_party/expat/COPYING", dist / "expat-COPYING.txt")
    # MCC's texture decoder is distributed under bcdec's MIT option.
    shutil.copy2(ROOT / "port/third_party/bcdec/LICENSE", dist / "bcdec-LICENSE.txt")
    # voice chat's codec (port/third_party/opus), in every build, whose BSD
    # license asks binaries to carry its notice
    shutil.copy2(ROOT / "port/third_party/opus/COPYING", dist / "opus-COPYING.txt")
    # the JSON tokenizer (port/third_party/jsmn) of the server browser's
    # lists and the dedicated server's API, in every build, whose MIT
    # license asks copies to carry its notice
    shutil.copy2(ROOT / "port/third_party/jsmn/LICENSE", dist / "jsmn-LICENSE.txt")
    # voice chat's speaker icons and the Mods menu's warning icons
    # (port/assets/icons/lucide, drawn into the menus' bitmaps), whose ISC
    # license asks copies to carry its notice
    shutil.copy2(ROOT / "port/assets/icons/lucide/LICENSE", dist / "lucide-LICENSE.txt")
    # internet play's MQTT brokers, a file beside the game (network.brokers_file;
    # Android's APK has its own copy; the macOS application reads its settings
    # folder in Application Support, never beside the app, so it uses the
    # game's built-in list)
    if args.platform != "android" and args.platform != "macos":
        shutil.copy2(ROOT / "port/assets/network/brokers.txt", dist / "brokers.txt")
        # the built-in callout voices, a folder beside the game
        shutil.copytree(ROOT / "port/assets/voices", dist / "voices")
        # (AE) the vehicle horn's sound, CMT's (port/assets/sounds/CREDITS.md), a folder beside the game
        shutil.copytree(ROOT / "port/assets/sounds", dist / "sounds")
        # the master servers that list dedicated servers, for the server
        # browser and the dedicated server (network.masters_file and
        # server.masters_file, docs/dedicated_server.md)
        masters = ROOT / "port/assets/network/masters.txt"
        if masters.exists():
            shutil.copy2(masters, dist / "masters.txt")
    bundle_sdl3 = os.environ.get("HALO_BUNDLE_SDL3")
    if args.platform == "linux64" and bundle_sdl3:
        # (AE) the portable download brings its SDL3 beside halo (the
        # executable's rpath is $ORIGIN), built by
        # tools/linux64_portable_deps.py in the folder this names, with its
        # zlib license; the Arch build uses the system's
        prefix = Path(bundle_sdl3)
        shutil.copy2(prefix / "lib" / "libSDL3.so.0", dist / "libSDL3.so.0")
        shutil.copy2(prefix / "SDL3-LICENSE.txt", dist / "SDL3-LICENSE.txt")
    if args.platform in ("linux", "linux64"):
        # (AE) the game's icon, for a launcher or desktop entry to point at
        # (Windows' is in halo.exe, macOS's in the bundle; tools/ae_logo.py)
        shutil.copy2(ROOT / "port/assets/icon/ae-icon-256.png", dist / "arena-evolved.png")
        # (AE) the udev rule that lets a DualSense's adaptive triggers and lightbar work without Steam's rules
        # (port/linux/src/ae_ds5.c; the README's "DualSense on Linux without Steam")
        shutil.copy2(ROOT / "port/assets/udev/70-ae-dualsense.rules", dist / "70-ae-dualsense.rules")
    if args.platform in ("linux", "linux64", "windows", "windows64"):
        # the dedicated server's examples: init.txt's commands and gametype
        # files (docs/dedicated_server.md, "Files")
        shutil.copy2(ROOT / "deploy/init.txt.example", dist / "init.txt.example")
        shutil.copytree(ROOT / "deploy/gametypes", dist / "gametypes")
    if args.platform in ("linux", "linux64"):
        # halo --server's script, and the ways to deploy a server
        # (deploy/: systemd, Docker, Pterodactyl, Kubernetes, other panels)
        shutil.copy2(ROOT / "deploy/halo-server", dist / "halo-server")
        shutil.copytree(ROOT / "deploy", dist / "deploy", ignore=shutil.ignore_patterns("*.zip", "__pycache__"))
    return 0


def alpine_build(platform: str, config: str) -> int:
    """the server, built by this script in Alpine's container of its
    architecture with the checkout mounted in it (and its compile cache,
    with CCACHE_DIR in the checkout)"""
    environment = []
    for name in ("CI_COMPILER_LAUNCHER", "CCACHE_DIR", "CCACHE_BASEDIR", "CCACHE_MAXSIZE", "HALO_VERSION",
                 "HALO_RELEASE_BUILD"):
        value = os.environ.get(name)
        if not value:
            continue
        # (the checkout is /src in the container)
        if name in ("CCACHE_DIR", "CCACHE_BASEDIR") and Path(value).resolve().is_relative_to(ROOT):
            value = (Path("/src") / Path(value).resolve().relative_to(ROOT)).as_posix()
        environment += ["-e", f"{name}={value}"]
    # (the checkout's commit, which git in the container may not read: the
    # checkout's owner is another user there)
    environment += ["-e", f"HALO_COMMIT={commit()}"]
    script = f"apk add --no-cache -q {' '.join(ALPINE_PACKAGES)} && python3 tools/ci_build.py {platform} {config}"
    run(["docker", "run", "--rm", "--platform", DOCKER_PLATFORMS[platform], "-v", f"{ROOT}:/src", "-w", "/src",
         *environment, ALPINE_IMAGE, "sh", "-c", script])
    return 0


def strip_debug(program: Path) -> None:
    """removes a program's debug information (its symbols stay, for crash
    reports): llvm-strip, or binutils' strip"""
    tool = shutil.which("llvm-strip") or shutil.which("strip")
    if tool is None:
        raise SystemExit("error: no llvm-strip or strip to remove the release server's debug information")
    before = program.stat().st_size
    run([tool, "--strip-debug", program])
    print(f"{program.relative_to(ROOT)}: {before / 1e6:.1f} MB -> {program.stat().st_size / 1e6:.1f} MB "
          "(debug information removed)", flush=True)


def server_dist(platform: str, config: str, output: str) -> int:
    """dist/chupathingyce-server-linux-<arch>[-debug]: the server, its
    README, its playlists, its blocked names list, its license and the notices
    its parts' licenses ask for"""
    arch = platform[len("server-"):]
    dist = ROOT / "dist" / f"chupathingyce-server-linux-{arch}{'' if config == 'release' else '-debug'}"
    if dist.exists():
        shutil.rmtree(dist)
    (dist / "playlists").mkdir(parents=True)
    shutil.copy2(ROOT / output, dist)
    # (sv_name_filter's list, read from the data folder)
    shutil.copy2(ROOT / "server/blocked_names.txt", dist / "blocked_names.txt")
    if config == "release":
        # a release's download without its debug information (about 20 MB
        # of 33); build/server-<arch>/ keeps the full program, and a debug
        # build ships it
        strip_debug(dist / Path(output).name)
    shutil.copy2(ROOT / "server/README.md", dist / "README.md")
    for playlist in sorted((ROOT / "server/playlists").glob("*.txt")):
        shutil.copy2(playlist, dist / "playlists")
    for source, name in (("port/third_party/mbedtls/LICENSE", "mbedtls-LICENSE.txt"),
                         ("port/third_party/miniupnpc/LICENSE", "miniupnpc-LICENSE.txt"),
                         ("port/third_party/expat/COPYING", "expat-COPYING.txt"),
                         ("LICENSE", "LICENSE.txt"),
                         ("LICENSES/GPL-3.0.txt", "GPL-3.0.txt"),
                         ("port/assets/fonts/Overpass-OFL.txt", "Overpass-OFL.txt"),
                         ("port/assets/fonts/CyborgInterfaceGothic-OFL.txt", "CyborgInterfaceGothic-OFL.txt"),
                         ("port/assets/fonts/OpenCE-OFL.txt", "OpenCE-OFL.txt"),
                         ("port/linux/ui/fonts/OFL.txt", "NotoSans-OFL.txt"),
                         ("port/linux/ui/fonts/KENNEY-CC0.txt", "Kenney-Input-Prompts-CC0.txt")):
        shutil.copy2(ROOT / source, dist / name)
    print(f"{output} -> {dist.relative_to(ROOT)}", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
