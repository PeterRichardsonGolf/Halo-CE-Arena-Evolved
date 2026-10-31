# Dependencies

What each download needs to run, and each build needs to compile. The
versions are the ones CI uses (`.github/workflows/build.yml`) and the build
scripts pin (`tools/`); a newer one usually works. The short version is in
the [README](../README.md#system-requirements).

## To play

Every download: your own Halo: Combat Evolved Xbox disc image (`.iso` or
`.xiso`; the game copies its `maps` folder out of it the first time it
starts). Custom Edition maps (`maps_ce`) also need Halo PC's `bitmaps.map`,
`sounds.map` and `loc.map`, which can come from Halo: The Master Chief
Collection's `halo1/maps/custom_edition` folder (Steam, Linux or Windows):
the game offers to read them there and never copies or changes MCC's files.

| Download | Operating system | Graphics | Libraries it needs from the system |
| --- | --- | --- | --- |
| `windows64` | Windows 10 or 11, 64-bit | OpenGL 4.5 core (`port/linux/src/sdl_platform.c`) | none: `halo.exe` imports ADVAPI32, bcrypt, CRYPT32, dbghelp, IPHLPAPI, KERNEL32, USER32, WINHTTP, WINMM, WS2_32 and `SDL3.dll`, which the zip has; no Visual C++ runtime |
| `linux64` (portable) | x86-64, glibc 2.35 or later: Ubuntu 22.04+, Zorin OS 17/18, Mint 21/22, Pop!_OS, Debian 12 | OpenGL 4.5 core | `libc` and `libm` only to start. SDL3 (`libSDL3.so.0`) is in the zip, found through the rpath `$ORIGIN`. SDL loads, when it starts and only if present: X11 (`libX11.so.6` and the X extensions) or Wayland (`libwayland-client.so.0`, `libdecor-0.so.0` for window borders on GNOME), PipeWire (`libpipewire-0.3.so.0`), PulseAudio (`libpulse.so.0`) or ALSA (`libasound.so.2`) for sound, `libudev.so.1` and D-Bus for controllers. A desktop has them. |
| `linux64-arch` | x86-64 with a glibc as new as Arch Linux's (2.43 when this was written) | OpenGL 4.5 core | `libc`, `libm` and the system's `libSDL3.so.0` (`sdl3` on Arch); the rest is loaded by SDL as above |
| `macos` | macOS 13+, Apple silicon | OpenGL 4.1 core (through Metal) | the application brings SDL3 |
| `android` | Android 9 (API 28)+, 64-bit ARM | OpenGL ES 3.0 (3.2 where there) | none |
| `server` | any Linux of its architecture (x86, x86-64, arm64) | none | none: one static program (musl) |

macOS and Android are upstream's ports: we do not test them.

The portable Linux zip is checked by CI on `ubuntu:22.04`, `ubuntu:24.04` and
`debian:12` (`linux64-portable-check`, `tools/ci_check_linux_portable.sh`):
all its libraries are found, no symbol asks for a glibc after 2.35, halo
links to nothing but SDL3, libc and libm, and it starts.

## To build

Everywhere: Python 3 (3.10 or later), ninja, and clang with lld (22 in CI;
profile-guided optimisation needs 22, an older clang builds without it).
`python configure.py --portable --release` is what CI configures. ccache
speeds up rebuilds (`--compiler-launcher ccache`). Re-run `configure.py`
when source files change. The Xbox SDK is not needed.

### Linux, 64-bit, Arch Linux (CI job `linux64`; the `-linux64-arch` download)

```sh
pacman -S --needed git zstd python ninja ccache clang lld gcc sdl3
python configure.py --portable --release
ninja linux64
```

`sdl3` brings SDL3 3.4.x's headers and library. The executable links the
system's `libSDL3.so.0`, `libm` and `libc`.

### Linux, 64-bit, portable (CI job `linux64-portable`; the `-linux64` download)

Built on Ubuntu 22.04 (glibc 2.35), so that it starts on that and every later
system. Ubuntu 22.04 and Debian 12 have no SDL3 package, so SDL3 is built
from source and shipped beside `halo`.

```sh
apt-get install -y build-essential git zstd python3 cmake meson ninja-build pkg-config ccache \
  curl gnupg ca-certificates
# clang 22 and lld 22 from apt.llvm.org (jammy, llvm-toolchain-jammy-22)
curl -fsSL https://apt.llvm.org/llvm-snapshot.gpg.key | gpg --dearmor -o /usr/share/keyrings/apt.llvm.org.gpg
echo "deb [signed-by=/usr/share/keyrings/apt.llvm.org.gpg] https://apt.llvm.org/jammy/ llvm-toolchain-jammy-22 main" \
  > /etc/apt/sources.list.d/llvm.list
apt-get update && apt-get install -y clang-22 lld-22 llvm-22
export PATH=/usr/lib/llvm-22/bin:$PATH
# what SDL3 and libdecor build against (they load the libraries when they start)
apt-get install -y wayland-protocols libwayland-dev libxkbcommon-dev libx11-dev libxext-dev \
  libxcursor-dev libxi-dev libxfixes-dev libxrandr-dev libxrender-dev libxss-dev libgl-dev \
  libegl-dev libpipewire-0.3-dev libpulse-dev libasound2-dev libudev-dev libdbus-1-dev \
  libcairo2-dev libpango1.0-dev
# SDL3 3.4.16 and libdecor 0.2.2 (pinned, with their SHA-256) into ./.portable-sdl3
python3 tools/linux64_portable_deps.py .portable-sdl3
export CPATH=$PWD/.portable-sdl3/include LIBRARY_PATH=$PWD/.portable-sdl3/lib
HALO_BUNDLE_SDL3=$PWD/.portable-sdl3 python3 tools/ci_build.py linux64 release
```

`tools/linux64_portable_deps.py` refuses an SDL3 built without the X11,
Wayland, PipeWire, PulseAudio, ALSA or udev drivers
(`tools/linux_sysroot.py`, `SDL_REQUIRED`). libdecor is built only so that
SDL can read its name: the zip does not carry it, and SDL loads the system's
when it is there. `tools/ci_build.py` puts `libSDL3.so.0` and
`SDL3-LICENSE.txt` (zlib) into the zip when `HALO_BUNDLE_SDL3` names the folder.

### Linux, 32-bit (CI job `linux`)

`pacman -S --needed git zstd python python-pytest ninja ccache clang lld gcc lib32-glibc lib32-gcc-libs cmake pkgconf wayland`
with `[multilib]` enabled. The `--portable` build downloads Debian 11's
32-bit glibc 2.31 and SDL3 3.4.16's source (`tools/linux_sysroot.py`) and
brings its own SDL3. `python -m pytest tools/...` and the other tests are
listed in the workflow.

### Windows (CI jobs `windows`, `windows64`; runner `windows-2025`)

Build on Windows (no cross-build from Linux): Visual Studio or its Build
Tools with the C++ workload (the x86 and x64 MSVC libraries and a Windows 10
or 11 SDK; the MSVC compiler is not used), LLVM (clang, lld), Python 3 and
ninja (`scoop install llvm python ninja`). CI adds ninja 1.12.1 and
ccache 4.10.2. `configure.py` downloads SDL 3.4.16's Visual C++ package.
Details: [port/windows/README.md](../port/windows/README.md).

### macOS (CI job `macos`; runner `macos-15`)

Xcode command line tools, `brew install ninja ccache` (and `sdl3 ffmpeg` to
build with Homebrew's libraries); the portable build clones SDL3
`release-3.4.16`. [port/macos/README.md](../port/macos/README.md).

### Android (CI job `android`; runner `ubuntu-24.04`)

The runner's Android SDK and NDK, CMake and a JDK 17 or later for Gradle,
ninja 1.12.1 and ccache 4.10.2; compile SDK 35, minimum SDK 28. SDL3
`release-3.4.16` is cloned. [port/android/README.md](../port/android/README.md).

### Dedicated server (CI job `server`)

One static program per architecture, built against musl in Docker's
`alpine:3.22` with `clang lld llvm gcc musl-dev linux-headers python3
samurai ccache` (`python3 tools/ci_build.py server-x64 release --alpine`).
[server/docs/building.md](../server/docs/building.md).

## SDL3

One release for every port: **3.4.16** (`tools/linux_sysroot.py`,
`tools/windows_build.py`, `tools/android_build.py`). SDL3 is zlib-licensed; the Linux and macOS downloads carry its license
(`SDL3-LICENSE.txt`) as they do the other libraries'.
