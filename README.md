<h1 align="center">
  <picture>
    <source media="(prefers-color-scheme: light)" srcset="port/assets/branding/readme-banner-light-1280x320.png">
    <img src="port/assets/branding/readme-banner-1280x320.png" alt="Halo CE: Arena Evolved" width="100%">
  </picture>
</h1>

<p align="center"><b>Competitive Halo: Combat Evolved, easy to pick up, on any PC.</b></p>

Arena Evolved (AE) is Halo: Combat Evolved's Xbox engine running natively on modern computers, made for
competitive play, split screen and LAN nights. The competitive rules the Xbox community played for years are built
into the game, so they work on every map without modded maps or a modded Xbox, and the features later Halo games
made standard come with them: item timers, a match clock, training aids, bots, regenerating health styles and more.

It is the classic evolved with the modern: inspired by the grassroots Halo CE community's rules, MLG's and today's
HCS settings, with ideas of its own. [Halo 1: NHE](https://halo1nhe.com/)'s own maps and gametypes stay supported
as an option. It is built on [OpenCE](https://github.com/OpenCommunityEdition/OpenCE), the port of the Halo:
Combat Evolved decompilation, and grew from [ChupathingyCE](https://github.com/ChupathingyCE/chupathingyce).

> **Status: early and in active development.** The current pre-release is 0.3.4-beta. The Linux 64-bit build is
> the one played and tested; the Windows 64-bit build is built by the same automated checks but is not test-played
> here; the other platforms build from the same code but are untested here.

## What's in it

- **Competitive rules on any map:** no fall damage, Halo 2 or Halo 3 style health, no spread, a pre-game countdown,
  practice mode, silent movement for split screen, a lethal oddball, all on a gametype's ARENA OPTIONS.
- **Ready-made gametypes:** casual, COMP, battle rifle and free-for-all sets, special modes, training, and Halo 1:
  NHE's own 23 ([gametypes.md](docs/gametypes.md)).
- **Item timers and training:** the power items' next spawns by the match clock, waypoints over them, spawn markers
  coloured by how likely you are to spawn there, spoken callouts.
- **The battle rifle:** three-round bursts and a 3x scope, as a starting weapon (a weapon pack, in the Windows and
  Linux downloads).
- **Split screen on PC:** up to four players on one PC, locally, on a LAN and online, each with their own profile,
  controller layout and settings.
- **Bots** that play every mode like a person of their level, modelled on how Halo Infinite's bots play: up to 15
  in a split screen game, 3 in a LAN game you host.
- **LAN nights:** players get the host's maps over the network (map sharing), map packs keep their own folders and
  pages in the map lists, and community, Custom Edition and HaloMD maps all play.
- **Controllers:** a PS5 DualSense's adaptive triggers and lightbar, 14 layouts including MCC's, back buttons and
  paddles, Nintendo Switch Pro controllers.
- **Display:** a combat HUD of its own (or Halo CE's), a match clock and campaign timer like the Master Chief
  Collection's, waypoint pictures, a performance overlay.
- **Privacy first:** public game lists, stats and crash reports stay off unless you turn them on, and the game never
  updates itself.

Every option in detail: [features.md](docs/features.md). What changed in each release: [CHANGELOG.md](CHANGELOG.md).

## Download

Releases are on the [Releases](https://github.com/PeterRichardsonGolf/Halo-CE-Arena-Evolved/releases) page. Take
the zip for your system:

| Download | For |
| --- | --- |
| `arena-evolved-<version>-windows64.zip` | Windows 10 or 11, 64-bit, with OpenGL 4.5. Nothing to install. |
| `arena-evolved-<version>-linux64.zip` | Linux x86-64, most distributions (Ubuntu 22.04 and later, Mint, Zorin, Pop!_OS, Debian 12), SDL3 included. |
| `arena-evolved-<version>-linux64-arch.zip` | Arch and other rolling distributions (uses the system's `sdl3`). |
| `-macos.tar`, `-android-unsigned.zip` | macOS 13 on Apple silicon and Android 9 on 64-bit ARM: built, not tested by us. |
| `-server-linux-<arch>.zip` | The dedicated server ([server/README.md](server/README.md)). |

Full requirements and versions: [dependencies.md](docs/dependencies.md). The game says its version in its window's
title and its log.

## You need your own copy of Halo

Arena Evolved includes none of the game's maps, sounds or art. You need an Xbox disc image (`.iso` or `.xiso`) of
Halo: Combat Evolved; any region works. The first time it starts, the game asks for the image and copies its maps
out of it (about 2 GB). Custom Edition maps also need `bitmaps.map`, `sounds.map` and `loc.map` from Halo PC or
Custom Edition; if Halo: The Master Chief Collection is installed on Steam, the game finds them there and asks
before using them.

## Getting started

1. Start the game and point it at your Halo disc image.
2. The menus are the PC version's. Arena Evolved's settings are under Settings (Game Options, HUD, Video, Gamepads,
   Network) and in a gametype's ARENA OPTIONS and INDICATOR OPTIONS.
3. Multiplayer > Create Game > LAN, pick a map and an AE gametype; or Multiplayer > SPLIT SCREEN for a game on this
   PC with bots. Extra players press START on their controllers in the lobby.

Every setting the port adds is in `config.toml` beside the game, written the first time it starts with each one
explained ([port/linux/README.md](port/linux/README.md)).

## Playing over LAN

Every computer needs to be on the same network (not a guest Wi-Fi) with UDP ports **5150**, **5151** and **5160**
and TCP port **5153** open. Windows asks the first time the game starts: allow it. On Linux a firewall blocks LAN
games silently: open the ports once (`sudo ufw allow 5150:5151/udp`, `sudo ufw allow 5160/udp`,
`sudo ufw allow 5153/tcp`). Players who lack the host's maps get them from the host. Details, firewalld and map
sharing: [lan.md](docs/lan.md).

## Where it's going

Three rulesets under one roof: **AE Casual** (Arena Evolved's rules with the timers, training aids and
quality-of-life options on), **AE Comp** (modern competitive settings, closer to today's HCS, researched rather than
guessed) and **Classic Comp (NHE)** (the Xbox competitive rules, faithfully). Next: split screen's join screen, the
H2 Demakes map pack, and more of the newer Halo games' features.

## For developers

- **Building:** Python 3, ninja, clang with lld (or gcc on Linux) and SDL3: `python3 configure.py --release` then
  `ninja linux64`. Every platform and package list: [building.md](docs/building.md).
- **Upstream:** [OpenCE](https://github.com/OpenCommunityEdition/OpenCE) is where the port is made, and Arena
  Evolved merges its releases and offers fixes to the shared code back. Online, Arena Evolved is network version 29
  and plays with OpenCE and ChupathingyCE builds on it: [upstream.md](docs/upstream.md).
- **Contributing:** [CONTRIBUTING.md](CONTRIBUTING.md). Arena Evolved's own code is in `port/linux/game/ae_*`; it
  changes the upstream files only by single marked lines (`/* AE hook */`, listed in `port/linux/game/ae_hooks.txt`).

## Credits

- The decompilation: [punpckhdq/halo](https://github.com/punpckhdq/halo) and
  [bnunu/halo-1](https://github.com/bnunu/halo-1), of the Xbox build 2342.
- The port: [OpenCE](https://github.com/OpenCommunityEdition/OpenCE) and its
  contributors.
- [ChupathingyCE](https://github.com/ChupathingyCE/chupathingyce): Milenko and
  contributors, and the contributors credited there and in each commit.
- [Halo 1: Neutral Host Edition (NHE)](https://halo1nhe.com/), the Xbox
  competitive mod whose rules inspired Arena Evolved: the NHE team. Arena
  Evolved's versions are its own code; it is not made or endorsed by the NHE
  team. Download NHE itself from [halo1nhe.com](https://halo1nhe.com/#download).
- Fonts: [Overpass](https://fonts.google.com/specimen/Overpass) (SIL OFL; the
  game's text and the logo's wordmark),
  [Noto Sans](https://fonts.google.com/noto) (SIL OFL) and
  [Kenney's Input Prompts](https://kenney.nl/assets/input-prompts) (CC0).
- The battle rifle's model, textures, animations and sounds are CMT's, from the source release of The Silent
  Cartographer: Evolved (1.4.1c), used with credit; its scope and gun-screen art are redrawn here, and its aiming
  reticle follows crosshair 033 of [Kenney's Crosshair Pack](https://kenney.nl/assets/crosshair-pack) (CC0). The
  pack's own credits file is `packs/ae_battle_rifle.CREDITS.txt`.
- The logo (ORBIT) is Arena Evolved's own drawing (`port/assets/branding`,
  made by `tools/ae_logo.py`); it uses no third-party art. It is not CC0
  (see below).
- Streamer mode's word list (OpenCE's, `port/assets/text_filter`) is made from Surge AI's "The Obscenity List"
  (MIT License, Copyright (c) Surge AI: the notice is in [AUTHORS.md](AUTHORS.md)) and the public-domain
  [english-words](https://github.com/dwyl/english-words) list.
- Libraries: SDL3, stb, Mbed TLS, miniupnpc, KCP, tomlc17, musl's maths,
  extract-xiso, Expat, Monocypher, zlib, SMAA, and Project Nayuki's QR Code
  generator. Their licenses are
  beside them in `port/third_party`.

Halo is a trademark of Microsoft. Halo CE: Arena Evolved is a non-commercial
fan project, not made or endorsed by Microsoft, Bungie, 343 Industries or Halo
Studios, and includes none of the game's content: you need your own copy of the
game. It is free, and must never be sold (Microsoft's Game Content Usage Rules
allow free fan projects only).

The code is released under [CC0](LICENSE.md), as OpenCE's and ChupathingyCE's
are, so upstream and the community are free to use and improve it. If you use
it, a credit is appreciated: "Halo CE: Arena Evolved by PeterRichardsonGolf",
with a link to this repository.

The logo is the exception: the ORBIT mark, the ARENA EVOLVED wordmark and the
app icons made from them are all rights reserved by the project. You may use
them unmodified to refer or link to Arena Evolved (in articles, videos, server
lists), but not as another project's branding. The files and terms:
[port/assets/branding/LICENSE.md](port/assets/branding/LICENSE.md).
