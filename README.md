<h1 align="center">
  <picture>
    <source media="(prefers-color-scheme: light)" srcset="port/assets/branding/readme-banner-light-1280x320.png">
    <img src="port/assets/branding/readme-banner-1280x320.png" alt="Halo CE: Arena Evolved" width="100%">
  </picture>
</h1>

<p align="center"><b>Halo: Combat Evolved with modern quality of life, on any PC. Every addition optional, CE's balance kept.</b></p>

Arena Evolved (AE) is Halo: Combat Evolved's Xbox engine running natively on modern computers. It is a fork of
[OpenCE](https://github.com/OpenCommunityEdition/OpenCE), the port of the Halo: Combat Evolved decompilation, and
grew from [ChupathingyCE](https://github.com/ChupathingyCE/chupathingyce). Its focus:

- **Modern quality of life:** what later Halo games made standard (item timers, a match clock, bots, split screen with
  each player's own profile and settings, map downloads from the host, menus that work with any controller or a mouse
  and keyboard), and fixes for the annoyances the game has always had.
- **New gameplay:** new gametypes, weapons and vehicle features in the spirit of the later games, tuned to fit Halo CE's
  sandbox rather than replace it. The magnum stays the magnum; a new weapon earns its place against Halo CE's own.

It is inspired by the grassroots Halo CE community's rules, MLG's and today's HCS settings, with ideas of its own.
[Halo 1: NHE](https://halo1nhe.com/)'s own maps and gametypes stay supported as an option.

> **Status: early and in active development.** The current pre-release is 0.3.9.1-beta. Supported: **Windows, Linux
> and macOS**. The other platforms OpenCE's automated builds cover (Android and more) build from the same code and are
> released with major releases, or sooner when players ask for them.

## Optional by design

Nothing Arena Evolved adds is forced on you. If you want Halo CE as it was:

- **Play it on another client.** [OpenCE](https://github.com/OpenCommunityEdition/OpenCE),
  [ChupathingyCE](https://github.com/ChupathingyCE/chupathingyce) or the original Xbox give you the classic game, and
  online Arena Evolved plays with OpenCE builds on its network version, so classic and AE players can still meet in
  one game ([upstream.md](docs/upstream.md)).
- **Or turn the additions off in Arena Evolved.** Halo CE's own gametypes play by Halo CE's rules: the AE rules (no
  fall damage, health styles, weapon packs, controller crouch and more) are on only in the AE gametypes, and each is a
  row in a gametype's GAME OPTIONS you can switch off. As the host, your gametype decides for everyone in the game.
  HUD STYLE CE draws Halo CE's own HUD, Settings > System > MENUS brings back the PC version's menus, and callouts,
  timers, waypoints and the other aids each have their own setting.

## What's in it

- **Competitive rules on any map:** no fall damage, HALO 2 or HALO 3 health, no spread, a pre-game countdown,
  practice mode, silent movement for split screen, a lethal oddball, all on a gametype's ARENA OPTIONS.
- **Ready-made gametypes, in playlist folders:** AE FFA, AE 2V2, AE 4V4, AE BTB (big team), AE ACTION SACK and AE
  PRACTICE (with OCTAGON warm-up drills: infinite ammo, instant respawns), each with its battle rifle version, new
  modes (Bomb, Grifball, Gun Game, Infection), and Halo 1: NHE's own
  ([gametypes.md](docs/gametypes.md)).
- **Item timers and training:** the power items' next spawns by the match clock, waypoints over them, spawn markers
  coloured by how likely you are to spawn there, spoken callouts.
- **New weapons, tuned to Halo CE's sandbox:** the battle rifle (three-round bursts, a 3x scope), an SMG, a carbine and
  suppressed SMG and assault rifle, as weapon packs (in the Windows and Linux downloads) that a gametype puts on any
  map or turns off. A test pack adds vehicle damage and wrecks.
- **Split screen on PC:** up to four players on one PC, locally, on a LAN and online, each with their own profile,
  controller layout and settings.
- **Bots** that play every mode like a person of their level, in six levels from RECRUIT to DEMON: up to 31 in a
  game on one PC (change how many from the pause menu, mid-game), and in any LAN or internet game you host, up to
  a big team game's worth and more (127), "BOT " before their names, filling the teams evenly and stepping aside
  when a player joins a full game. They play as a team:
  a wingman sticks with each player, they know the power items' timers, regroup when outnumbered, hear gunfire, pace
  their pistol shots, scope back in after a hit and take the high ground.
- **LAN nights:** players who lack a map get it from the host as they join, with its progress on the lobby's
  header (Settings > Network > INTERNET DOWNLOADS turns it off); map packs keep their own folders and pages in the
  map lists, and community, Custom Edition and HaloMD maps all play.
- **Controllers:** a PS5 DualSense's adaptive triggers and lightbar, 14 layouts including MCC's, back buttons and
  paddles, Nintendo Switch Pro controllers.
- **Menus of its own:** CREATE GAME and its lobby, CHANGE MAP and CHANGE GAMETYPE with folders and filters, a
  gametype editor, KEY BINDINGS, and every setting with a line of help.
- **Display:** a combat HUD of its own (or Halo CE's), a match clock and campaign timer like the Master Chief
  Collection's, waypoint pictures, keyboard keys shown in the game's prompts when you play with a keyboard, a
  performance overlay.
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
| `arena-evolved-<version>-macos.tar` | macOS 13 or later on Apple silicon. |
| `-android-unsigned.zip` and other platforms | Android 9 on 64-bit ARM and the other platforms OpenCE builds: with major releases or on request, untested by us. |
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
2. The menus are Arena Evolved's own (Settings > System > MENUS switches to the PC version's). The game's settings are
   under SETTINGS; a gametype's rules are in its GAME OPTIONS.
3. MULTIPLAYER > CREATE GAME, pick LAN or HOST ONLINE, a map and an AE gametype, and set the lobby's BOTS. Extra
   players on this PC press START on their controllers in the lobby.

Every setting the port adds is in `config.toml` beside the game, written the first time it starts with each one
explained ([port/linux/README.md](port/linux/README.md)).

## Playing over LAN

Every computer needs to be on the same network (not a guest Wi-Fi) with UDP ports **5150**, **5151** and **5160**
and TCP port **5153** open. Windows asks the first time the game starts: allow it. On Linux a firewall blocks LAN
games silently: open the ports once (`sudo ufw allow 5150:5151/udp`, `sudo ufw allow 5160/udp`,
`sudo ufw allow 5153/tcp`). Players who lack the host's maps get them from the host. Details, firewalld and map
sharing: [lan.md](docs/lan.md).

## Where it's going

More of the same focus: quality of life and the old annoyances ironed out, new gameplay that respects Halo CE's
balance, and bots good enough to fill any game. Next: an in-game pause menu and post-game results in Arena Evolved's
style, a scoreboard for big team games, more weapons and vehicles, and the newest OpenCE builds merged in.

## For developers

- **Building:** Python 3, ninja, clang with lld (or gcc on Linux) and SDL3: `python3 configure.py --release` then
  `ninja linux64`. Every platform and package list: [building.md](docs/building.md).
- **Upstream:** [OpenCE](https://github.com/OpenCommunityEdition/OpenCE) is where the port is made, and Arena
  Evolved merges its releases and offers fixes and features back to it, credited to Arena Evolved. Online, Arena
  Evolved is network version 38 and plays with OpenCE builds on it (build-268 to build-281) and ChupathingyCE 0.8.0f
  and later:
  [upstream.md](docs/upstream.md).
- **Contributing:** [CONTRIBUTING.md](CONTRIBUTING.md). Arena Evolved's own code is in `port/linux/game/ae_*`; it
  changes the upstream files only by single marked lines (`/* AE hook */`, listed in `port/linux/game/ae_hooks.txt`).

## Credits

- The decompilation: [punpckhdq/halo](https://github.com/punpckhdq/halo) and
  [bnunu/halo-1](https://github.com/bnunu/halo-1), of the Xbox build 2342.
- The port: [OpenCE](https://github.com/OpenCommunityEdition/OpenCE) and its
  contributors (its main contributors are Jonas Volman, @bnunu, and MrBruh,
  @cybersecurity). The menus' blue arrow mouse cursor (`port/assets/cursor`,
  from OpenCE build-250) is pugzysixtyfour's art.
- [ChupathingyCE](https://github.com/ChupathingyCE/chupathingyce): Milenko and
  contributors, and the contributors credited there and in each commit. From
  ChupathingyCE 0.8.0: ugoboom (Zebouski), HD HUDs on Custom Edition maps
  (MCC's "0.5 HUD scale" and Chimera's "force HUD use high resolution scale"
  flags) and the loading screen's sound on the SDL ports. The scoreboard's
  platform logos are one-colour marks from [Simple Icons](https://simpleicons.org)
  (CC0) and Wikimedia Commons (public domain), plus ChupathingyCE's own Xbox 360
  ring and Wii U GamePad; the names and logos are their owners' trademarks, used
  only to identify platforms. HaloMD map names come from
  [MacGamingMods](https://macgamingmods.com)' public HaloMD mod list.
- High-res HUD art on the Xbox maps (HUD STYLE CE): Aerocatia's and Jessie's HUD
  art from [Halo PC Restored](https://github.com/Aerocatia/halopc-restored)
  (release r220), brought in by ChupathingyCE and used with Aerocatia's
  permission on the condition that both are credited;
  `port/assets/hud/NOTICE.md` lists which files are theirs.
- Controller button icons by snapchipper, CC0 (`port/assets/prompts`).
- [Halo 1: Neutral Host Edition (NHE)](https://halo1nhe.com/), the Xbox
  competitive mod whose rules inspired Arena Evolved: the NHE team. Arena
  Evolved's versions are its own code; it is not made or endorsed by the NHE
  team. Download NHE itself from [halo1nhe.com](https://halo1nhe.com/#download).
- Fonts: [Overpass](https://fonts.google.com/specimen/Overpass) (SIL OFL; the
  game's text and the logo's wordmark), Cyborg Interface Gothic by fizzy (SIL
  OFL; made to match Halo's interface font from Overpass and glyphs of their
  own, ChupathingyCE's high-res text font),
  [Noto Sans](https://fonts.google.com/noto) (SIL OFL) and
  [Kenney's Input Prompts](https://kenney.nl/assets/input-prompts) (CC0).
- The battle rifle's model, textures, animations and sounds are CMT's, from the source release of The Silent
  Cartographer: Evolved (1.4.1c), used with credit; its scope and gun-screen art are redrawn here, and its aiming
  reticle follows crosshair 033 of [Kenney's Crosshair Pack](https://kenney.nl/assets/crosshair-pack) (CC0). The
  pack's own credits file is `packs/ae_battle_rifle.CREDITS.txt`.
- The vehicle damage pack's effects and wrecks (`packs/ae_vehicle_damage.aepack`) are CMT's, from the same TSC:E
  1.4.1c source release (the Evolved team), used with credit; its credits file is
  `packs/ae_vehicle_damage.CREDITS.txt`.
- The vehicle horn (`sounds/horn.wav`) is CMT's Warthog horn from the same TSC:E 1.4.1c source release (the Evolved
  team), used with credit (`port/assets/sounds/CREDITS.md`).
- The logo (ORBIT) is Arena Evolved's own drawing (`port/assets/branding`,
  made by `tools/ae_logo.py`); it uses no third-party art. It is not under
  the code's licence (see "Licence" below).
- Streamer mode's word list (OpenCE's, `port/assets/text_filter`) is made from Surge AI's "The Obscenity List"
  (MIT License, Copyright (c) Surge AI: the notice is in [AUTHORS.md](AUTHORS.md)) and the public-domain
  [english-words](https://github.com/dwyl/english-words) list.
- Libraries: SDL3, stb, Mbed TLS, miniupnpc, KCP, tomlc17, musl's maths,
  Expat, Monocypher, zlib, SMAA, and Project Nayuki's QR Code generator.
  Their licenses are beside them in `port/third_party`.

Halo is a trademark of Microsoft. Halo CE: Arena Evolved is a non-commercial
fan project, not made or endorsed by Microsoft, Bungie, 343 Industries or Halo
Studios, and includes none of the game's content: you need your own copy of the
game. It is free, and must never be sold (Microsoft's Game Content Usage Rules
allow free fan projects only).

## Testing

The tests are listed in [AGENTS.md](AGENTS.md) ("Testing"). Most run
without the game data, in GitHub Actions too.

One check needs the retail maps, so it runs on your own computer (or on a
self-hosted runner), not in GitHub Actions: `tools/net_compare.py` plays
two builds together in a network game on one computer and checks that they
stay in step. Run it when a change can alter how a build calculates the
game, as a change to the simulation, the compiler or the maths does,
before it is merged. For example, to check a build with such a change
against a build without it:

```sh
python tools/net_compare.py --a build/linux/halo --b <other checkout>/build/linux/halo \
    --data <folder with maps/> --maps bloodgulch,beavercreek,hangemhigh --swap
```

Each game prints PASS or FAIL. A FAIL names the first player, tick and
value that differ. Refer to "Comparing two builds" in
[port/linux/NETCODE.md](port/linux/NETCODE.md).

## Licence

Arena Evolved is free software: you can give it to others and change it under
the terms of the GNU Affero General Public License as published by the Free
Software Foundation, version 3 of the License or (at your option) any later
version (`AGPL-3.0-or-later`), as OpenCE is. It comes with no warranty. The
full text is in [LICENSE](LICENSE).

- **From the change.** Arena Evolved follows OpenCE, which moved to the AGPL
  at its build-237. The AGPL applies from the merge commit that brought
  OpenCE build-237 into Arena Evolved, and to each release built from that
  commit or a later one (0.3.5 and after). Earlier releases (0.3.4-beta and
  before), and the source at every earlier commit, were released under
  [CC0 1.0](LICENSES/CC0-1.0.txt) and stay that way.
- **What Arena Evolved builds on.** [OpenCE](https://github.com/OpenCommunityEdition/OpenCE)
  is under the AGPL 3.0 from its build-237 and under CC0 1.0 before it. The
  decompilation of [punpckhdq/halo](https://github.com/punpckhdq/halo) is
  under the AGPL 3.0 from 17 October 2026, and under CC0 1.0 before that
  date. [bnunu/halo-1](https://github.com/bnunu/halo-1) is under CC0 1.0.
  ChupathingyCE's code up to its 0.7.5d is under CC0 1.0; its code from
  0.8.0 (16 October 2026 on) is under the GNU General Public License 3.0 or
  later (`GPL-3.0-or-later`, the text in [LICENSES/GPL-3.0.txt](LICENSES/GPL-3.0.txt)).
  Arena Evolved includes ChupathingyCE 0.8.0b; the GPL 3.0 (section 13) lets
  that code be combined with the AGPL 3.0 work it is in, and those parts keep
  their GPL terms.
- **Contributions** are under `AGPL-3.0-or-later`.
- **Hosts and servers.** If you change the game and let other persons play
  with your version over a network (for example, as the host of an internet
  game, or on a web site), the AGPL asks you to offer them the source of
  your version (section 13). The game as released has its source here, and
  each release's source is its tag.
- **Third-party code, fonts and art** keep their own licences, which are
  next to them (`port/third_party`, `port/assets/fonts`, `port/linux/ui/fonts`,
  `port/assets/icons`, `port/assets/prompts`, `port/assets/hud`,
  `port/assets/text_filter`) and listed under Credits
  above. The release downloads carry their notices.
- **The logo** is the exception: the ORBIT mark, the ARENA EVOLVED wordmark
  and the app icons made from them are all rights reserved by the project.
  You may use them unmodified to refer or link to Arena Evolved (in articles,
  videos, server lists), but not as another project's branding. The files
  and terms: [port/assets/branding/LICENSE.md](port/assets/branding/LICENSE.md).
- **Halo is not covered.** The licence covers the work of Arena Evolved's and
  OpenCE's authors. It gives no rights in Halo, its maps or anything else
  that Microsoft owns.

If you use Arena Evolved's code, a credit is appreciated: "Halo CE: Arena
Evolved by PeterRichardsonGolf", with a link to this repository.
