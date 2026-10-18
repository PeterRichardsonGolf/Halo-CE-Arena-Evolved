# Building Arena Evolved

Back to the [README](../README.md).

You need Python 3, [ninja](https://ninja-build.org/), clang with lld (or gcc on
Linux) and, on Linux and macOS, SDL3 development files. The game supplies the
Xbox SDK declarations it uses, so you don't need the SDK.

```sh
python3 configure.py --release
ninja linux64        # build/linux64/halo
```

Packages, as the CI installs them ([docs/dependencies.md](dependencies.md)
has versions and the other targets):

- **Arch Linux** (the 64-bit build):
  `pacman -S --needed git zstd python ninja ccache clang lld gcc sdl3`.
  For the 32-bit build and the tests, enable `[multilib]` and add
  `python-pytest lib32-glibc lib32-gcc-libs cmake pkgconf wayland`.
- **Ubuntu 22.04 or later** (the portable download's build, which CI makes in
  an Ubuntu 22.04 container): `apt install build-essential git python3 cmake
  meson ninja-build pkg-config` and clang with lld from
  [apt.llvm.org](https://apt.llvm.org/) (22; Ubuntu's own clang is older),
  then SDL3 built from source, as `python3 tools/linux64_portable_deps.py
  <folder>` does (it needs the X11, Wayland, PipeWire, PulseAudio, ALSA and
  udev development packages listed in the script), and
  `CPATH=<folder>/include LIBRARY_PATH=<folder>/lib` while you build. Ubuntu
  25.04 and later have `libsdl3-dev`, and need none of that.
- **Windows**: build on Windows, with Visual Studio's Build Tools (C++
  workload, x64 libraries; the MSVC compiler itself is not used), LLVM, Python
  and ninja (`scoop install llvm python ninja`); there is no cross-build from
  Linux. `configure.py` downloads SDL3 for it. See
  [port/windows/README.md](../port/windows/README.md).
- **macOS**: Xcode command line tools and `brew install sdl3 ffmpeg ninja`
  ([port/macos/README.md](../port/macos/README.md)). **Android**: the NDK, CMake
  and a JDK 17 ([port/android/README.md](../port/android/README.md)).

| Target | Result | Instructions |
| --- | --- | --- |
| `ninja linux64` | `build/linux64/halo` (64-bit, tested) | [port/linux/README.md](../port/linux/README.md) |
| `ninja linux` | `build/linux/halo` (32-bit) | [port/linux/README.md](../port/linux/README.md) |
| `ninja windows64` | `build/windows64/halo.exe` | [port/windows/README.md](../port/windows/README.md) |
| `ninja macos` | the Mac app | [port/macos/README.md](../port/macos/README.md) |
| `ninja server` | the dedicated server (Linux) | [server/docs/building.md](../server/docs/building.md) |

Re-run `configure.py` when source files or the menus are added.

