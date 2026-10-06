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
| NO SPREAD | OFF (stock), NHE (the pistol's first shot and the sniper rifle's unzoomed shots go exactly where you aim, as Halo 1: NHE has them) or FULL (every pistol and sniper rifle shot goes where you aim, held fire too: no spread, no bloom). Other weapons are unchanged. |
| PRACTICE MODE | Every weapon and powerup respawns every 30 seconds. |
| PRE-GAME COUNTDOWN | A 3-2-1 on a black screen before the game starts; you can look around but not move or shoot. A weapon switch (Y) pressed during it is kept and taken as it ends, as in Halo 1: NHE; any other button pressed during it does nothing until you let go of it, so nothing fires, zooms, reloads or picks up the moment it ends. The host holds every player to it, including players on builds without it. |

Plus, in the gametype's indicator options: **TIMERS** and **TRAINING**.
TIMERS shows the power list only: the next spawns of the rockets, sniper
rifle, overshield and camo, in the top corner opposite the performance
overlay with one view, and in the margin above or below each view's HUD in
split screen. The match clock is its own setting (below).
TRAINING adds, as Halo 1: NHE's Training mode did: a waypoint over each power
item (rockets, sniper rifle, overshield, camo) from 10 seconds before it spawns
to 20 seconds after, and green markers on the floor (or the crate or platform a spawn stands on) at the player spawns this
gametype uses (within 25 units, hidden behind walls). Each waypoint is
labelled with the item and the time to its spawn ("OVERSHIELD 0:08"), then
the item alone once it is on the map; a spawn that can be either powerup
(Blood Gulch's) reads OS/CAMO until it spawns, then the one it spawned. The
label is red for an item at the red base, blue at the blue base, the HUD's
colour in the middle (as near to both: within 15% of the bases' distance
apart). An item a map has at both bases is RED or BLUE in its name, in the
power list too (RED SNIPER, BLUE SNIPER on Blood Gulch). Arrows together at
the screen's edge share one list of labels, and items spawning at the same
time share a line ("0:07 ROCKETS · OS/CAMO · RED SNIPER").

These are the host's rules. Players on Arena Evolved see and play them all.
Players who join on a build without them still play the host's rules where
the host decides: it holds them to the countdown and respawns items as the
gametype says. What their own game draws and how their own shots spread is
up to their build.

**Arena Evolved gametypes**, ready in the gametype list the first time you
play:
- **Arena Evolved (casual):** AE SLAYER (25 kills), AE TEAM SLY (50), AE CTF
  (3 captures), AE KING and AE ODDBALL (5 minutes held; AE KING is team
  king, a team game too, and of the two only AE ODDBALL is free for all),
  each with a 15-minute time limit, the motion sensor, TIMERS and the stock
  respawns; and AE TRAINING (TIMERS and TRAINING, plays to 500, no time
  limit).
- **Arena Evolved 2V2** (casual rules for two against two): AE 2V2 SLY (25 kills),
  AE 2V2 CTF (3 captures), AE 2V2 KING and AE 2V2 BALL (team king and team
  oddball, 5 minutes held). The player limit of 4 is the host's setting.
- **Arena Evolved PRO** (modern NHE): AE PRO FFA (25 kills), AE PRO TS (50),
  AE PRO CTF (3 captures), AE PRO KING and AE PRO BALL (team games, 5 minutes
  held). 5-second respawn and suicide penalty, no TIMERS, no motion sensor
  (but in AE PRO FFA); the slayers have no time limit, the others 15 minutes.
- Every Arena Evolved gametype has no fall damage, HALO 2 health, NO SPREAD
  FULL, the PRE-GAME COUNTDOWN and generic starting equipment, with a pistol
  in hand and an assault rifle.
- **Halo 1: NHE-style:** NHE 1V1, NHE 2V2 TS, NHE CTF, NHE POWERUP, NHE
  VANILLA, NHE TRAIN and PRACTICE. On Halo 1: NHE's maps their vehicle set
  picks NHE's mode (NHE & Timer, NHE & Powerups, Vanilla, Training, Timer
  Only). NHE 1V1 (25 kills), NHE 2V2 TS (50), NHE CTF (3 captures) and NHE
  POWERUP (50) play Beach LAN 15's rules: NHE & Timer (NHE POWERUP: NHE &
  Powerups), 5-second respawn and suicide penalty, no motion sensor, no time
  limit, NO SPREAD NHE and the PRE-GAME COUNTDOWN. NHE TRAIN has NO SPREAD NHE
  and the countdown; NHE VANILLA has neither. PRACTICE has NO SPREAD NHE and
  PRACTICE MODE, without the countdown.

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
  MULTIPLAYER GAME TIMER: OFF, TIME REMAINING or TIME ELAPSED). TIME
  REMAINING shows the time left (counting up when there is no time limit),
  TIME ELAPSED the time played. It
  sits in the bottom right corner of each view, lined up with the motion
  sensor. The scoreboard (hold BACK) shows both times, such as
  "1:33 PLAYED · 8:27 LEFT" (just "1:33 PLAYED" with no time limit), on a
  row under its title.
  On Halo 1: NHE's maps it replaces their own clock.
- A campaign timer, as in the Master Chief Collection (Settings > Game Options
  > CAMPAIGN TIMER: OFF or ON, off by default): the time played on the level,
  in the same corner as the match clock. It counts game time, so it stops in
  the pause menu; cutscenes count. Reverting to a checkpoint does not take
  time back, and starting the level again starts it at 0:00.
- Spoken callouts (Settings > Game Options > CALLOUTS: OFF, ITEMS or ITEMS +
  CLOCK), as Halo 1: NHE's voice timer, in any gametype: the power items'
  calls 10 seconds before they spawn ("rockets in ten"), five to one before
  the rockets and "rockets are up" as they spawn; ITEMS + CLOCK adds NHE's
  talking timer (the minutes, thirty and twenty seconds left, ten to one,
  beeps). An item's call that would meet the clock's moves earlier, so
  none is lost. VOICE picks a voice pack in `voices/`. The default voice is
  Cori, a beta voice that ships with every build (made with Piper and the
  public-domain en_GB-cori-high voice; the clips are CC0). NHE's own clips
  are an optional import (`tools/import_nhe_voice.py`; none ships).

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
style of Halo Infinite, and more of our own voice for the callouts (Cori, beta, ships
now), calling every item.

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

Halo is a trademark of Microsoft. Halo CE: Arena Evolved is a non-commercial
fan project, not made or endorsed by Microsoft, Bungie, 343 Industries or Halo
Studios, and includes none of the game's content: you need your own copy of the
game. It is free, and must never be sold (Microsoft's Game Content Usage Rules
allow free fan projects only).

The code is released under [CC0](LICENSE.md), as OpenCE's and ChupathingyCE's
are, so upstream and the community are free to use and improve it. If you use
it, a credit is appreciated: "Halo CE: Arena Evolved by PeterRichardsonGolf",
with a link to this repository.
