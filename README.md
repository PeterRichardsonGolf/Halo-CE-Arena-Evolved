<h1 align="center">Halo CE: Arena Evolved</h1>

<p align="center"><b>Competitive Halo: Combat Evolved, easy to pick up, on any PC.</b></p>

Arena Evolved (AE) is a build of Halo: Combat Evolved's Xbox engine, running
natively on modern computers, made for competitive and arena play. It brings
the competitive rules the Xbox community played for years (as in Halo 1: NHE)
into the game itself, so they work on every map without modded maps or a
modded Xbox, and adds the quality-of-life features later Halo games made
standard: item timers, a match clock, training aids, regenerating health
styles and more.

It is built on [OpenCE](https://github.com/OpenCommunityEdition/OpenCE), the
port of the Halo: Combat Evolved decompilation, by way of
[ChupathingyCE](https://github.com/ChupathingyCE/chupathingyce), and follows
both closely.

> **Status: early and in active development.** There are no releases yet;
> build it yourself (below). The Linux 64-bit build is the one played and
> tested; the other platforms build from the same code but are untested here.

What has changed, release by release: [CHANGELOG.md](CHANGELOG.md).

## Goals

- **Competitive rules in the engine:** Halo 1: NHE's rules, built natively,
  so they work on every map, on any PC, without modded maps or a modded Xbox.
- **The later games' features in CE:** options and quality-of-life from Halo
  2, Halo 3, Reach, Halo 4 and Infinite, and from the Master Chief
  Collection.
- **Easy to learn:** item timers, training aids and indicators that make CE's
  competitive play approachable for new players.
- **Close to OpenCE:** merge its releases and offer fixes to the shared code
  back upstream.

## What it adds

**Gametype options** (a gametype's **ARENA OPTIONS** screen, in the gametype
editor and in Server Setup)

| Option | What it does |
| --- | --- |
| FALL DAMAGE | Off: no landing damage from any height (pits and kill zones still kill). |
| HEALTH | CLASSIC (stock: health packs only), REACH (health recovers to a third), HALO 2 (health refills with the shields), HALO 3 (health refills after the shields). Health packs still spawn. |
| NO SPREAD | The pistol's first shot and the sniper rifle's unzoomed shots go exactly where you aim, as competitive play has them. Other weapons are unchanged. |
| PRACTICE MODE | Every weapon and powerup respawns every 30 seconds. |
| PRE-GAME COUNTDOWN | A 3-2-1 on a black screen before the game starts; you can look around but not move or shoot. The host holds every player to it, including players on builds without it. |

Plus, in the gametype's indicator options: **TIMERS** and **TRAINING**.
TIMERS shows the power list only: the next spawns of the rockets, sniper
rifle, overshield and camo, in the top corner opposite the performance
overlay with one view, and in the margin above or below each view's HUD in
split screen. The match clock is its own setting (below).

These are the host's rules. Players on Arena Evolved see and play them all.
Players who join on a build without them still play the host's rules where
the host decides: it holds them to the countdown and respawns items as the
gametype says. What their own game draws and how their own shots spread is
up to their build.

**Arena Evolved gametypes**, ready in the gametype list the first time you
play:
- **Arena Evolved:** AE SLAYER, AE TEAM SLY, AE CTF, AE KING, AE ODDBALL and
  AE TRAINING. Each adds no fall damage, HALO 2 health, NO SPREAD and the
  PRE-GAME COUNTDOWN to its stock gametype, with a pistol in hand and an
  assault rifle. AE TRAINING also has TIMERS and TRAINING, and plays to 500.
- **Halo 1: NHE-style:** NHE 1V1, NHE 2V2 TS, NHE CTF, NHE POWERUP, NHE
  VANILLA, NHE TRAIN and PRACTICE. On Halo 1: NHE's maps their vehicle set
  picks NHE's mode (Timer Only, NHE & Powerups, Vanilla, Training). All have
  NO SPREAD and the PRE-GAME COUNTDOWN, except NHE VANILLA, which has
  neither. PRACTICE has NO SPREAD and PRACTICE MODE.

**Split screen**
- Split screen in System Link and online lobbies (ADD PLAYER), and a lobby
  you can start on your own.
- SETTINGS from the pause menu in the campaign, for each player in split
  screen, changing that player's own profile.
- The in-game scoreboard centred in each view.

**Controls and display**
- 14 controller layouts, including the Anniversary set and MCC's Universal
  Reclaimer, Zoom & Shoot, Bump & Jump and Green Fingers; layouts that put
  actions on the d-pad don't also move you with it.
- A performance overlay in the HUD's style (Settings > Video > PERFORMANCE:
  FPS, or FPS with frame times and draws; top left or top right).
- A match clock, as in the Master Chief Collection (Settings > Game Options >
  MATCH CLOCK: OFF, COUNT DOWN or COUNT UP). COUNT DOWN shows the time left
  (counting up when there is no time limit), COUNT UP the time played. It
  sits in the bottom right corner of each view, lined up with the motion
  sensor. The scoreboard (hold BACK) shows both times, such as
  "1:33 PLAYED · 8:27 LEFT" (just "1:33 PLAYED" with no time limit), on its
  title row, or just under the title where the two don't fit side by side.
  On Halo 1: NHE's maps it replaces their own clock.

**Mods**
- Settings > Game Options > MOD picks a folder in `mods/`; its `maps` are
  played over the game's own, and the game restarts into it.
- Halo 1: NHE's maps play as a mod (`mods/NHE`), with its timer, training and
  modes working. NHE's files are not included: use your own download.
- Xbox community maps (the v5 map pack format) and Halo PC (Custom Edition)
  maps, with the Custom Edition shared files from your own Halo PC install or
  from Halo: The Master Chief Collection on Steam (`halo1/maps/custom_edition`).

**Privacy**
- Games you host are not listed publicly, and the games you join are not
  reported to any stats site, unless you turn them on (Settings > Network).

## Where it's going

Three rulesets under one roof:

1. **NHE**: the Xbox competitive rules, faithfully.
2. **Casual**: those rules with timers, training aids and the quality-of-life
   options on.
3. **Hardcore**: closer to Halo Infinite's competitive settings, researched
   rather than guessed.

Next up: item spawn waypoints and spawn markers in team colours in the
style of Halo Infinite, and voice callouts for item spawns and the clock with
swappable voice packs.

## You need your own copy of Halo

Arena Evolved doesn't include the game's maps, sounds or art. You need an
Xbox disc image (`.iso` or `.xiso`) of Halo: Combat Evolved; any region works.
The first time it starts, the game asks for the image and copies its `maps`
folder (about 2 GB) out of it.

On Linux, the maps and settings (`config.toml`) go next to the `halo`
executable, and the saves in `~/.local/share/halo-linux`. Everything the port
adds is in `config.toml`, written the first time the game starts with every
setting listed and explained; see [port/linux/README.md](port/linux/README.md).

## Building it yourself

You need Python 3, [ninja](https://ninja-build.org/) and clang (or gcc on
Linux). The game supplies the Xbox SDK declarations it uses, so you don't need
the SDK.

```sh
python3 configure.py --release
ninja linux64        # build/linux64/halo
```

| Target | Result | Instructions |
| --- | --- | --- |
| `ninja linux64` | `build/linux64/halo` (64-bit, tested) | [port/linux/README.md](port/linux/README.md) |
| `ninja linux` | `build/linux/halo` (32-bit) | [port/linux/README.md](port/linux/README.md) |
| `ninja windows64` | `build/windows64/halo.exe` | [port/windows/README.md](port/windows/README.md) |
| `ninja macos` | the Mac app | [port/macos/README.md](port/macos/README.md) |
| `ninja server` | the dedicated server (Linux) | [server/docs/building.md](server/docs/building.md) |

Re-run `configure.py` when source files or the menus are added.

## How it relates to OpenCE and ChupathingyCE

- [OpenCE](https://github.com/OpenCommunityEdition/OpenCE) is where the port is
  made. Arena Evolved merges its releases.
- [ChupathingyCE](https://github.com/ChupathingyCE/chupathingyce), a community
  build of OpenCE, is the base Arena Evolved grew from: its online games,
  Custom Edition and HaloMD map support and fixes come with it, and their
  documentation in this repository is theirs.
- Fixes to the shared game code are offered back upstream.

## Credits

- The decompilation: [punpckhdq/halo](https://github.com/punpckhdq/halo) and
  [bnunu/halo-1](https://github.com/bnunu/halo-1), of the Xbox build 2342.
- The port: [OpenCE](https://github.com/OpenCommunityEdition/OpenCE) and its
  contributors.
- [ChupathingyCE](https://github.com/ChupathingyCE/chupathingyce): Milenko and
  contributors, and the contributors credited there and in each commit.
- Halo 1: Neutral Host Edition (NHE), the Xbox competitive mod whose rules
  Arena Evolved reimplements: the NHE team. Arena Evolved's versions are its
  own code; it is not made or endorsed by the NHE team.
- Fonts: [Noto Sans](https://fonts.google.com/noto) (SIL OFL) and
  [Kenney's Input Prompts](https://kenney.nl/assets/input-prompts) (CC0).
- Libraries: SDL3, stb, Mbed TLS, miniupnpc, KCP, tomlc17, musl's maths,
  extract-xiso, and Project Nayuki's QR Code generator. Their licenses are
  beside them in `port/third_party`.

Halo is a trademark of Microsoft. Halo CE: Arena Evolved is a fan project, not
made or endorsed by Microsoft, Bungie or 343 Industries, and includes none of
the game's content. The code is released under [CC0](LICENSE.md).
