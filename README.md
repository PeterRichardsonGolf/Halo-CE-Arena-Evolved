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

> **Status: early and in active development.** The first pre-release is
> 0.1.0-beta (below). The Linux 64-bit build is the one played and tested;
> the other platforms build from the same code but are untested here.

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
| HEALTH | CLASSIC (stock: health packs only), REACH (health recovers to a third), HALO 2 (Halo 2's timers: shields recharge 5 s after the last shield damage and fill in 2 s; health refills in 5 s, starting 10 s after the last damage that reached it, shield hits do not count), HALO 3 (health refills after the shields). Health packs still spawn. |
| NO SPREAD | OFF (stock), NHE (the pistol's first shot and the sniper rifle's unzoomed shots go exactly where you aim, as Halo 1: NHE has them) or FULL (every pistol and sniper rifle shot goes where you aim, held fire too: no spread, no bloom). Other weapons are unchanged. |
| PRACTICE MODE | Every weapon and powerup respawns every 30 seconds. |
| PRE-GAME COUNTDOWN | A 3-2-1 on a black screen before the game starts; you can look around but not move or shoot. A weapon switch (Y) pressed during it is kept and taken as it ends, as in Halo 1: NHE; any other button pressed during it does nothing until you let go of it, so nothing fires, zooms, reloads or picks up the moment it ends. The host holds every player to it, including players on builds without it. |

Plus, in the gametype's indicator options: **TIMERS** and **TRAINING**.
TIMERS shows the power list only: the next spawns of the rockets, sniper
rifle, shotgun, overshield and camo, as a column stacked over the match clock in the
bottom right corner of every view (one view and split screen alike), the
soonest at the foot, NAME and TIME in two aligned columns ("R/B SNIPER 1:50",
R and B in the bases' colours); at most four lines, "+N" for more; hidden
while that view's scoreboard shows. With one view, `display.power_list`
`"top_left"` keeps the earlier single line in the top left corner. The match clock is its own setting (below).
TRAINING adds, as Halo 1: NHE's Training mode did: a waypoint over each power
item (rockets, sniper rifle, shotgun, overshield, camo) from 10 seconds before it spawns
to 20 seconds after, and green markers on the floor (or the crate or platform a spawn stands on) at the player spawns this
gametype uses (within 25 units, hidden behind walls). Each waypoint is
labelled with the item and the time to its spawn ("OVERSHIELD 0:08"), then
the item alone once it is on the map; a spawn that can be either powerup
(Blood Gulch's) reads OS/CAMO until it spawns, then the one it spawned. The
label is red for an item at the red base, blue at the blue base, the HUD's
colour in the middle (as near to both: within 15% of the bases' distance
apart). An item a map has at both bases is RED or BLUE in its name, in the
power list too (RED SNIPER, BLUE SNIPER on Blood Gulch); an item a map has twice on one side is numbered (Boarding Action's RED ROCKETS 1 and RED ROCKETS 2). While a label shows the time to the spawn, its arrow has no distance by it. Arrows together at
the screen's edge share one list of labels, and items spawning at the same
time share a line ("0:07 ROCKETS · OS/CAMO · RED SNIPER").

With TRAINING, SPAWN HEAT (Settings > Game Options; MINE by default) colours
each spawn marker by how likely you are to spawn there next, by CE's own
spawn rules worked out on your machine: cold blue-grey is unlikely, hot
orange-red the likeliest (it pulses), dark red with a cross can't be picked
right now (an enemy within 6 m, a vehicle on it). When the spawns that can be
picked are about equally likely they are all green, none pulsing. A marker
fades as you come near it, so the one under your feet stays faint. ENEMY shows where the other
team would spawn instead. A ring flashes where a player really spawns. CE
spawns ignore line of sight: an enemy over 15 m from a spawn makes no
difference, while a teammate within 18 m makes it far likelier.

These are the host's rules. Players on Arena Evolved see and play them all.
Players who join on a build without them still play the host's rules where
the host decides: it holds them to the countdown and respawns items as the
gametype says. What their own game draws and how their own shots spread is
up to their build.

**Arena Evolved gametypes**, ready in the gametype list the first time you
play:
- **Arena Evolved (casual):** FFA AE SLAYER (25 kills), TEAM AE SLAYER (50),
  TEAM AE CTF (3 captures), TEAM AE KING, FFA AE ODDBALL and TEAM AE ODDBALL
  (5 minutes held), each with a 15-minute time limit, the motion sensor, the
  stock respawns, item TIMERS with waypoints, and both weapons dropped on death
  (DROP SECONDARY ALWAYS). The 2V2 ones (TEAM AE 2V2 SLAYER to 25, CTF, KING
  and ODDBALL) play the same rules; the player limit of 4 is the host's setting.
  FFA AE PRACTICE: every weapon and powerup each 30 seconds, 500 kills, no time
  limit, the map's vehicles. TEAM AE VANILLA and TEAM AE POWERUPS: team slayer
  that plays Halo 1: NHE's VANILLA and NHE & POWERUPS modes on NHE's maps.
- **Arena Evolved special modes** (team slayer to 50): SNIPERS (sniper rifle and
  pistol, the sniping weapon set, no motion sensor), SHOTTY SNIPERS (shotgun
  and sniper rifle, no weapons on the map), SWAT (pistols, no shields, classic
  health, no grenades), ROCKETS, SHOTGUNS and HEAVIES (75 kills, the map's
  vehicles).
- **Arena Evolved COMP** (modern NHE): FFA AE COMP SLAYER (25 kills), TEAM AE
  COMP SLAYER (50), TEAM AE COMP CTF (3 captures), TEAM AE COMP KING and TEAM
  AE COMP ODDBALL (5 minutes held). 5-second respawn and suicide penalty, 2
  frag grenades, no motion sensor, no vehicles, item TIMERS only in line of
  sight (no list, no item calls), the objective only in line of sight, both
  weapons dropped; the slayers have no time limit, the others 15 minutes.
- **FFA AE TRAINING:** item TIMERS and TRAINING's markers, plays to 500, no
  time limit.
- Every Arena Evolved gametype has no fall damage, HALO 2 health (SWAT:
  classic), NO SPREAD NHE, the PRE-GAME COUNTDOWN and generic starting
  equipment, with a pistol in hand and an assault rifle unless its mode says
  otherwise.
- **Halo 1: NHE's own 23 gametypes** (TS 50, TS 100, TS ON-OFF, TS TRAINING,
  TS PRACTICE, TS SNIPERS, FFA 50 NR/R, 1 V 1 NR/R, KOTH 5M 7S/10S, BALL 5M
  7S/10S, eight CTFs and CTF WIZARD), with NHE's values: classic health, fall
  damage, NO SPREAD NHE, the countdown, NHE EXTRAS (everyone joins red, no team
  swap, the dead camera on you, the match clock counting up), no death bonus or
  kill penalty in slayer. On Halo 1: NHE's maps each plays its NHE mode; on
  other maps no vehicles.
- The lists show display names (TEAM AE SLAYER over the stored AE TEAM SLY):
  the Arena Evolved set, AE COMP, the NHE set, TRAINING, then your own
  gametypes, before the built-in ones. Seeded gametypes you edit are kept as
  you made them when a newer build updates the others.

**Split screen**
- Split screen in System Link and online lobbies (ADD PLAYER), and a lobby
  you can start on your own.
- SETTINGS from the pause menu in the campaign, for each player in split
  screen, changing that player's own profile; in a match one player edits
  a profile at a time, and B closes only your own pause.
- The in-game scoreboard centred in each view.

**Controls and display**
- 14 controller layouts, including the Anniversary set and MCC's Universal
  Reclaimer, Zoom & Shoot, Bump & Jump and Green Fingers; layouts that put
  actions on the d-pad don't also move you with it.
- A performance overlay in the HUD's style (Settings > Video > PERFORMANCE:
  FPS, or FPS with frame times and draws; top left or top right).
- A match clock, as in the Master Chief Collection (Settings > Game Options >
  MULTIPLAYER GAME TIMER: OFF, TIME REMAINING, TIME ELAPSED or BOTH). TIME
  REMAINING shows the time left (counting up when there is no time limit),
  TIME ELAPSED the time played, and BOTH the time left with the time played
  smaller above it (the time played alone with no time limit). It sits in
  the bottom right corner of each view, lined up with the motion sensor.
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
  beeps). The items' calls come first: an item alone is called on its
  second, items spawning together (a wave) start early enough to end by
  the 10-second mark, the clock's "ten" gives way to an item's call due
  then, and "is up" comes before the minute. CALLOUT DETAIL picks how much
  a wave says: MINIMAL (one line, the first item up, no beeps), STANDARD
  or VERBOSE (every item named). VOICE picks a voice pack in `voices/`. The default voice is
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
- Internet games you host start PUBLIC, as upstream's do: they show in the
  in-game Server Browser unless you set LISTING to PRIVATE in Server Setup.
  Listing them on the website game list, and reporting the games you join to a
  stats site, stay off unless you turn them on (Settings > Network).
- The game does not fetch ChupathingyCE's signed table of network versions
  (Delta) unless you turn `network.legacy_table_fetch` on (it would change
  nothing: Arena Evolved has its own wire ID). In a game, machines that speak
  Delta tell each other their platform and build; your player ID only with
  `network.share_profile` (off). `network.protocol = "opence"` turns Delta
  Peer off.

## Where it's going

Three rulesets under one roof:

1. **NHE**: the Xbox competitive rules, faithfully.
2. **Casual**: those rules with timers, training aids and the quality-of-life
   options on.
3. **Hardcore**: closer to Halo Infinite's competitive settings, researched
   rather than guessed.

Next up: a side-by-side split-screen layout, a
per-player in-game menu and HUD, and more of our own voice for the callouts
(Cori, beta, ships now), calling every item.

## Download

Releases are on the
[Releases](https://github.com/PeterRichardsonGolf/Halo-CE-Arena-Evolved/releases)
page: 0.1.0-beta, built on ChupathingyCE 0.7.0b and OpenCE build-139, is
the first, a pre-release (2026-10-06). Each platform is a zip of its own,
`arena-evolved-<platform>-release.zip`; `arena-evolved-linux64-release.zip`
(Linux, 64-bit) is the one played and tested. The dedicated server keeps
its own name, `chupathingyce-server-linux-<arch>`
([server/README.md](server/README.md)).

Arena Evolved does not update itself: get a new version from the Releases
page. Its window's title and its start-up log say which version it is.

## You need your own copy of Halo

Arena Evolved doesn't include the game's maps, sounds or art. You need an
Xbox disc image (`.iso` or `.xiso`) of Halo: Combat Evolved; any region works.
The first time it starts, the game asks for the image and copies its `maps`
folder (about 2 GB) out of it.

## Getting started

1. Start the game and point it at your Halo disc image (see above).
2. The menus are the PC version's (`display.menus` in `config.toml`):
   that's where Arena Evolved's settings live: Settings > Game Options
   (callouts, voice, mods, campaign rules, the match clock, HUD area),
   Settings > Video (the performance overlay), and in a gametype's options,
   ARENA OPTIONS and INDICATOR OPTIONS.
3. Multiplayer > Create Game > LAN, pick a map and one of the AE gametypes
   (in the gametype list's CUSTOM bank). For split screen, press START on
   each extra controller in the lobby.

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
- Online, Arena Evolved plays with ChupathingyCE 0.7.0b and OpenCE build-133
  to build-140 (network version 20), both ways, and joins games hosted on
  network versions 11 to 22, OpenCE build-141 and later's (21, 22)
  included, on the game's own maps (OpenCE's Custom Edition maps are named
  otherwise); those builds don't join Arena Evolved's games, which say 20. Older
  builds' hosts (network version 10) can't be joined, and ChupathingyCE
  0.6.8b and OpenCE build-129 to build-131 (version 18) don't join Arena
  Evolved's games.

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
