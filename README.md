# Halo CE: Arena Evolved

**Halo: Combat Evolved with modern quality of life, on any PC. Every addition optional, CE's balance kept.**

Arena Evolved (AE) is a free, non-commercial fan project that runs Halo: Combat Evolved natively on modern computers.
It is a fork of [OpenCE](https://github.com/OpenCommunityEdition/OpenCE) and grew from
[ChupathingyCE](https://github.com/ChupathingyCE/chupathingyce).

This repository holds the releases and the issue tracker. Development happens elsewhere; each release's complete source
is attached to it.

## What's in it

- **MCC maps and Workshop mods, online:** play Halo: The Master Chief Collection's Halo CE maps and the Workshop maps
  you subscribe to with other players. MCC has no browser for modded custom games; Arena Evolved does: host from your
  own install, find games in the server browser or on your LAN, or join by address. Players joining a Workshop map
  need the same Workshop item, and the game tells them which one.
- **Online and LAN:** server browser, LAN search (found even with a VPN on), invite links, JOIN BY ADDRESS (an IP or a
  computer's name, with a recent list), dedicated servers for Linux, text and voice chat. Maps you lack come from the host
  as you join, with its layout. OFFLINE games on one PC pause for real. Plays with OpenCE builds of the same network version.
- **Bots** in six levels from RECRUIT to DEMON, up to 31 on one PC and 127 in a game you host, in every mode: a wingman
  per player, squads that flank, flag crews that escort, bombers that carry, plant and defend. They drive and ride
  vehicles, know the power items' timers, dodge rockets, take revenge, and stay out of your line of fire. A bot steps
  aside when a player joins. PLAYFIGHT bots aim but never shoot, for practice.
- **Split screen on PC:** up to four players on one PC, locally, on a LAN and online, each with their own profile,
  controller layout, settings and pause menu; a controller picker sorts four pads fast.
- **Search and Destroy, Grifball and Bomb:** one life a round, a real bomb carried on the back, held to pick up, a fuse
  timer on its face, a blast that hurts by distance; only the attackers hear about it.
- **Weapons tuned to Halo CE's sandbox:** the battle rifle (bursts, 3x), the EVO RIFLE (single shot, 2x scope with
  live zoom and rounds), the DMR (steady marksman rifle), SMG, carbine, and suppressed SMG and AR, as weapon packs that
  work on stock, Custom Edition and MCC maps. A gametype picks what you start with.
- **Ready-made gametypes in playlist folders:** AE FFA, 2V2, 4V4, BTB (big team, with heavy-vehicle versions), COMP,
  ACTION SACK (Frag Nation, One in the Chamber, Melee Only, Needles, Snowball Fight, Grifball, Bomb, Search and
  Destroy), PRACTICE with OCTAGON warm-up drills; Halo 1: NHE's gametypes as an option.
- **Rules you can set on any map,** each a row in GAME OPTIONS: fall damage, health styles, COMBAT FEEDBACK (shield pop, recharge, blood pop), no spread, a pre-game countdown, MELEE
  UNZOOMS, controller crouch, mouse aim assist, silent movement and weapons for split screen, destructible vehicles,
  friendly splatter, lethal oddball, practice mode, and SPAWNS CLASSIC or MODERN.
- **Training aids:** item timers and waypoints with pictures, spawn markers and spawn heat, spoken callouts in seven
  voices (three detail levels), a match clock, dead ally markers, a green ally reticle, an enemies-only motion sensor.
- **Host tools:** RESTART GAME and RESTART ROUND, bots and skill changed mid-game, CHANGE MAP and GAME OPTIONS from the
  pause menu, SHUFFLE TEAMS, teams pinned for bots, a gametype changed between games without leaving the lobby.
- **Controllers:** many button layouts including MCC's, DualSense adaptive triggers (a profile per weapon) and lightbar, back
  paddles, Nintendo and PlayStation prompts, gyro aiming, stick deadzones. Mouse and keyboard: KEY BINDINGS, wheel
  zoom, hold-to-zoom and MCC-style sensitivity.
- **Menus of its own:** main menu, CREATE GAME and lobby, MY PARTY, CHANGE MAP and CHANGE GAMETYPE with folders
  (STOCK, MCC, WORKSHOP, map packs), pictures, search, favourites and RECENT; its own pause menu, scoreboard and
  post-game; every setting with a line of help.
- **HUD and display:** a combat HUD of its own (or Halo CE's) with a score strip, a frame-rate limit you type (30 to
  1000), a performance line with ping, HUD area and compact HUD, backpack weapons on bodies, pistol colour.
- **Capture:** screenshots (every split view) and video recording with sound (Windows includes FFmpeg).
- **Map packs and layouts:** community, Custom Edition and modded-disc maps keep their own folders and pictures;
  layout files let a pack move, add and remove items, vehicles, spawns and flags, and set a map's jump.
- **Updates and privacy:** the game checks this page for a signed release and asks first (AUTO UPDATE turns it off).
  Public game lists, stats and crash reports stay off unless you turn them on.
- **For map makers and testers:** layout logs, SHOW COORDINATES, a dev camera, a TEST RUN list runner.

## Play it stock if you want

Nothing Arena Evolved adds is forced on you:

- **Halo CE's own gametypes play by Halo CE's rules.** The AE rules (weapon packs, health styles, no fall damage,
  MODERN spawns and the rest) are on only in the AE gametypes, and each is a row you can switch off. As the host, your
  gametype decides for everyone in the game.
- **Halo CE's look:** HUD STYLE CE draws the game's own HUD, Settings > System > MENUS brings back the PC version's
  menus, and callouts, timers, waypoints and the other aids each have their own setting.
- **Classic menus hide AE's screens.** Most of Arena Evolved's screens exist only in its own menus: the pause menu and
  post-game, CREATE GAME's live lobby and MY PARTY, CHANGE MAP's MCC, WORKSHOP and pack folders, search and favourites, JOIN BY
  ADDRESS, KEY BINDINGS, RESTART GAME and RESTART ROUND, and the gametype editor's full GAME OPTIONS. With the classic
  menus on, those are gone (the gametypes' rules still play). Switch MENUS back to use them.
- **Stock maps stay stock:** the STOCK folders hold the game's own maps as they shipped, from your disc image or MCC.
- **Play with classic clients:** online, Arena Evolved plays with OpenCE builds of the same network version, so
  classic and AE players can meet in one game.

## Download

Take the zip for your system from the [Releases](../../releases) page:

| Download | For |
| --- | --- |
| `arena-evolved-<version>-windows64.zip` | Windows 10 or 11, 64-bit, with OpenGL 4.5. Nothing to install. |
| `arena-evolved-<version>-linux64.zip` | Linux x86-64, most distributions, SDL3 included. |
| `arena-evolved-<version>-linux64-arch.zip` | Arch and other rolling distributions (uses the system's `sdl3`). |
| `arena-evolved-<version>-macos.tar` | macOS 13 or later on Apple silicon. |
| `-server-linux-<arch>.zip` | The dedicated server. |

Only the latest release is kept here.

Or get it with [Quiver](https://quiverlauncher.com/): Arena Evolved is listed in the Quiver launcher.

## You need your own copy of the game

Arena Evolved includes none of the game's maps, sounds or art. It plays from either of these, or both:

- **Halo: The Master Chief Collection on Steam.** Arena Evolved finds the install by itself and plays its Halo CE
  multiplayer maps and the Halo CE maps of every Steam Workshop item you subscribe to, straight from MCC's own files.
  No disc image needed and nothing is converted.
- **An Xbox disc image** (`.iso` or `.xiso`) of Halo: Combat Evolved, any region. The first time it starts, the game
  asks for the image and copies its maps out of it (about 2 GB).

With both, you get the disc's maps and MCC's in one list. Settings > MAP FILES turns MCC's files off or picks its
folder.

## Source and licence

Arena Evolved is free software under the GNU Affero General Public License, version 3 or (at your option) any later
version (`AGPL-3.0-or-later`). It comes with no warranty. Parts from ChupathingyCE 0.8.0 and later keep their GPL-3.0
terms.

**Source:** every release has a source tarball asset, the complete source of that release. Third-party code, fonts and
art keep their own licences, which are beside them in the tarball and in the downloads.

The ORBIT logo and the ARENA EVOLVED wordmark are all rights reserved by the project. You may use them unmodified to
refer or link to Arena Evolved, but not as another project's branding.

## FFmpeg

The Windows download includes an unmodified FFmpeg static build (`ffmpeg.exe`, GPL v3) for recording, with its licence
and `FFMPEG-SOURCE.txt`, which says where its source is. Deleting `ffmpeg.exe` turns recording off; screenshots still
work. On other systems, recording uses your own FFmpeg.

## Issues welcome

Bug reports and ideas are welcome in [Issues](../../issues). For a bug, please include:

- your system and the AE version (it is in the window's title);
- what you did and what you saw;
- `debug.txt` from the game's folder, if you can.

## Credits

- [OpenCE](https://github.com/OpenCommunityEdition/OpenCE) and its contributors, and the projects it builds on.
- [ChupathingyCE](https://github.com/ChupathingyCE/chupathingyce): Milenko and contributors.
- [GulchCE](https://github.com/chestahh/GulchCE): the MCC map loading builds on its work (CC0).
- [Halo PC Restored](https://github.com/Aerocatia/halopc-restored): high-res HUD art by Aerocatia and Jessie, used with
  permission.
- [Halo 1: Neutral Host Edition](https://halo1nhe.com/): the NHE team, whose competitive rules inspired AE. AE's versions
  are its own code, and AE is not made or endorsed by the NHE team.
- CMT: the rifles' and the vehicle damage pack's models, sounds and effects, from the source release of The Silent
  Cartographer: Evolved, used with credit. CuiGames: the EVO RIFLE's sight.
- [Kenney](https://kenney.nl): the reticle base and input prompts (CC0). snapchipper: controller icons (CC0).
  pugzysixtyfour: the menus' mouse cursor. fizzy: Cyborg Interface Gothic. Overpass and Noto Sans (SIL OFL).
- Libraries: SDL3, stb, Mbed TLS, miniupnpc, KCP, tomlc17, musl's maths, Expat, Monocypher, zlib, SMAA and Project
  Nayuki's QR Code generator.

The full list and every notice are in each release's download and source tarball.

## Legal

Halo is a trademark of its owner. Halo CE: Arena Evolved is a non-commercial fan project, not made or endorsed by the
game's owners or developers, and includes none of the game's content. It is free and must never be sold.
