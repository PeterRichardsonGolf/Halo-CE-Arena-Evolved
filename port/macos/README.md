# macOS

`ninja macos` builds the game as a native 64-bit arm64 (Apple silicon)
executable, `build/macos/halo`, and an application bundle,
`build/macos/ArenaEvolved.app`. It is the Linux port (`port/linux`) compiled as
64-bit code: the game sources, the platform layer and the settings are the
Linux build's, and [port/linux/README.md](../linux/README.md) describes them.

## Requirements

- macOS 13 or later on Apple silicon.
- The Xcode command line tools (`xcode-select --install`): clang and the SDK.
- SDL3 and ninja from Homebrew: `brew install sdl3 ninja`. The movies' FFmpeg is
  built by the build itself (`tools/ffmpeg_build.py`).
- The game data in `assets/maps` (see the [README](../../README.md)).

```sh
python3 configure.py
ninja macos
build/macos/halo          # or open build/macos/ArenaEvolved.app
```

## 64-bit

The game was written for the Xbox's 32-bit CPU, and its data formats embed
32-bit pointers: cache files hold tag data whose blocks and references point
at each other by address, game state is saved and restored as raw memory, and
Direct3D resources carry physical addresses. The 32-bit Linux and Windows
builds and Android keep 32-bit pointers (Android as ILP32 code in a 64-bit
process), but a 64-bit macOS process cannot: arm64 macOS reserves the low
4 GB of every process. So this build is 64-bit code, as the 64-bit Linux and
Windows builds are, with two changes that the 32-bit builds never see:

- **Pointers inside Xbox data** (`HALO_64BIT`). The platform layer reserves
  4 GB at a fixed host address (`port/linux/src/xbox_memory.c`) and puts
  everything the game's data can point at there: the contiguous memory (game
  state, tag cache, Direct3D resources) and the game's heap
  (`port/linux/src/xbox_heap.c`). Xbox address X is host address base + X.
  Structures laid out like the Xbox's declare their pointer fields
  `XPTR(type)`, a 32-bit Xbox address, and convert with `xbox_pointer` and
  `XBOX_ADDRESS` (`source/cseries/xbox_address.h`). Everywhere else these are
  plain pointers and the conversions do nothing, so the 32-bit builds and the
  byte-matching build compile what they always did. Code that differs on
  64-bit is under `#ifdef HALO_64BIT`, and the size asserts of structures
  whose layout 64-bit pointers change are left out there.
- **`long`**. MSVC's `long` is 32 bits; macOS's is 64. The build compiles a
  copy of every source with the Xbox's ABI (the game, the platform layer, the
  Xbox SDK declarations) in which `long` is spelled `int` and `%ld` is `%d`
  (`tools/lp64_rewrite.py`, into `build/macos/lp64`). The sources keep
  `long`; diagnostics name the original files.

The build turns the conversions that would truncate a 64-bit pointer into
errors (`-Werror=pointer-to-int-cast` and the like), so a place that still
treats an Xbox address as a pointer, or the reverse, does not compile.

## macOS

- OpenGL 4.1 (through Metal) is the newest macOS has. The renderer does
  without the later features it uses elsewhere, as the Android build does
  with OpenGL ES: clip control is done in the vertex shader
  (`HALO_GL_NO_CLIP_CONTROL`), mipmap copies are blits, and visibility tests
  wait for their results. Buffer uploads are unsynchronized appends, which
  Apple's driver needs to keep up (`d3d8_gl.c`, `buffer_append`).
- Vertical sync is the OpenGL context's own (`port/macos/src/macos_video.c`):
  SDL 3.4's waits for the main thread's run loop, which the game's first
  frames do not run.
- Apple silicon has 16 KB pages; page protection works in host pages
  (`xbox_memory.c`, `memory_watch.c`).
- The first start's disc extraction copies the disc's `bink/` folder (the
  `.bik` movies) beside `maps/`, only the files that are missing. The game
  reads `d:\bink\<name>.bik` there, and falls back to `assets/bink/`.
- Bink video (the intro, attract and credits movies) is decoded with FFmpeg
  behind the Bink calls the game makes (`port/linux/src/bink_ffmpeg.c`, the
  same as the Linux and Android builds). FFmpeg is built here for each
  architecture and linked statically, so the application for other Macs
  (`configure.py --portable`) carries no Homebrew library. A missing or
  unreadable movie is skipped, as on the Xbox.
- The application bundle (`port/macos/bundle.py`, `Info.plist`) registers the
  `halo://` and Discord URL schemes that internet play invites use.
- Command-W (the Window menu's Close) does not close the game, as W moves
  the player forward. Command-Q quits on a second press within two seconds
  (Q is the flashlight); the window's close button, the menus' Quit and the
  Dock's quit at once (`sdl_platform.c`).

## Playing the download

The download is a folder, `arena-evolved-macos-release`, with `ArenaEvolved.app`, the licences, and the game's own
folders `packs`, `voices` and `sounds` beside the application (the weapon packs, the callout voices, the horn).

- **First open.** The application is signed ad hoc, not notarized, so macOS refuses a double-click the first time:
  right-click (or Control-click) `ArenaEvolved.app`, choose Open, then Open again. If the dialog has no Open, choose
  Apple menu > System Settings > Privacy & Security, scroll to the message about ArenaEvolved, and choose Open Anyway.
  Once is enough.
- **The data folder.** The game keeps its maps, `packs`, `voices`, `sounds`, `config.toml` and the log in
  `~/Library/Application Support/ChupathingyCE` (Finder: Go > Go to Folder..., paste that path; the name is
  upstream's, kept for now). The saved games and the profiles are in its `saves` folder (in `~/.local/share/halo-linux` on a Mac that had them there from an older version).
- **The log.** `debug.txt` is in the data folder. Attach it to a bug report.
- **Packs, voices and sounds.** The first start after the download copies them from the folder beside the application
  (or from `ArenaEvolved.app/Contents/Resources`, where a build that carries them keeps them) into the data folder; the
  log says `extras: packs, voices, sounds found in <folder>: N files copied`. Keep the three folders beside the
  application for that first start; afterwards the application can go anywhere, such as Applications. A newer download
  replaces older files in the data folder the same way. Without them the game says it cannot join games that use
  Arena Evolved's weapons, and callouts and the horn are silent.
- **If nothing was copied.** When macOS opens a downloaded application from where it was unpacked, it may run a
  temporary copy of it, and the folders beside the real one cannot be seen; the log then says `macOS opened the
  application from a temporary copy`. Copy `packs`, `voices` and `sounds` into the data folder yourself (do not put
  them inside the application: that breaks its signature and macOS then refuses to open it).

## Status

A beta. The game plays its movies, campaign and multiplayer, and joins and
hosts internet games with the Linux and Windows builds. Known problem:
vertical sync is not yet confirmed to pace the frames.
