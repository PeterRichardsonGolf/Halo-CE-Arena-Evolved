<h1 align="center">
  <picture>
    <source media="(prefers-color-scheme: light)" srcset="port/assets/branding/readme-banner-light-1280x320.png">
    <img src="port/assets/branding/readme-banner-1280x320.png" alt="Halo CE: Arena Evolved" width="100%">
  </picture>
</h1>

<p align="center"><b>Competitive Halo: Combat Evolved, easy to pick up, on any PC.</b></p>

Arena Evolved (AE) is a build of Halo: Combat Evolved's Xbox engine, running
natively on modern computers, made for competitive and arena play. It brings
the competitive rules the Xbox community played for years
into the game itself, so they work on every map without modded maps or a
modded Xbox, and adds the quality-of-life features later Halo games made
standard: item timers, a match clock, training aids, regenerating health
styles and more.

It is an evolution of the classic with the modern, not only the old-school
way: inspired by the grassroots Halo CE competitive community's rules and
MLG's rules, by today's HCS settings and the features of the newer Halo games,
with ideas of its own on top.
[Halo 1: NHE](https://halo1nhe.com/)'s own maps and gametypes stay supported
as an option.

It is built on [OpenCE](https://github.com/OpenCommunityEdition/OpenCE), the
port of the Halo: Combat Evolved decompilation, by way of
[ChupathingyCE](https://github.com/ChupathingyCE/chupathingyce), and follows
both closely.

> **Status: early and in active development.** The current pre-release is
> 0.2.5-beta (below). The Linux 64-bit build is the one played and tested;
> the Windows 64-bit build is built by the same automated checks but this
> release's was not test-played before it went up;
> the other platforms build from the same code but are untested here.

What has changed, release by release: [CHANGELOG.md](CHANGELOG.md).

## Goals

- **Bring new players in:** introduce new players to classic Halo and to
  arena shooters in general. An arena shooter asks a lot of a newcomer (item
  timing, spawns, map control) compared with most modern shooters; item
  timers, training aids and indicators teach it in the game.
- **Competitive rules in the engine:** the rules competitive Halo grew up on
  (grassroots CE, MLG) and the ones it plays today (HCS), built natively,
  so they work on every map, on any PC, without modded maps or a modded Xbox.
- **The later games' features in CE:** options and quality-of-life from Halo
  2, Halo 3, Reach, Halo 4 and Infinite, and from the Master Chief
  Collection.
- **Competitive split screen on PC, natively:** up to four players on one
  PC, in local games, on a LAN and online, with each player's own profile,
  controller layout and settings.
- **The ultimate CE LAN package:** one download for a LAN night: split screen,
  LAN and online play, map sharing between players, community and Custom
  Edition maps, and gametypes ready to play.
- **Every platform upstream builds:** not PC alone. Arena Evolved builds for
  each platform OpenCE and ChupathingyCE do (Windows, Linux, macOS, Android,
  the dedicated server). Linux and Windows 64-bit are the ones played and
  tested here; the others are upstream's work carried along as it is, until
  the project has the community and interest to give them their own care.
- **Close to OpenCE:** merge its releases and offer fixes to the shared code
  back upstream.

## What it adds

**The battle rifle** is a starting weapon: in a gametype's **ITEM OPTIONS**, with STARTING WEAPONS on CUSTOM, BATTLE
RIFLE is the last choice of PRIMARY and SECONDARY (three-round bursts, a 3x scope, its own reticle). It comes as a
weapon pack, the `packs` folder beside the game's `maps` (in the Windows and Linux 64-bit downloads); Settings > Game
Options > WEAPON PACKS turns packs off on a machine. Where the pack is not (packs off, or a map it cannot be added
to) the slot plays an assault rifle. While a game's gametype gives the rifle, only Arena Evolved 0.2.5 or later with
the pack can join it.

**Gametype options** (a gametype's **ARENA OPTIONS** screen, in the gametype
editor and in Server Setup)

| Option | What it does |
| --- | --- |
| FALL DAMAGE | Off: no landing damage from any height (pits and kill zones still kill). |
| HEALTH | HALO CE (stock: health packs only), REACH (health recovers to a third), HALO 2 (Halo 2's timers: shields recharge 5 s after the last shield damage and fill in 2 s; health refills in 5 s, starting 10 s after the last damage that reached it, shield hits do not count), HALO 3 (health refills after the shields). Health packs still spawn. |
| COMBAT FEEDBACK | HALO CE (stock: the game's own shield flare) or ARENA EVOLVED, which shows on a player's body what the fight has come to: a shield about to fail bursts white, a broken shield flickers until it recharges, a recharge sweeps from feet to head, and the shot that finishes a player after breaking the shield throws a blood pop where it struck. Drawing only: no damage value changes, nothing new on the HUD, and stock players in the game are unaffected. Multiplayer only. |
| NO SPREAD | OFF (stock), FIRST SHOT (the pistol's first shot from rest and the sniper rifle's unzoomed shots go exactly where you aim; held pistol fire still spreads) or FULL (every pistol and sniper rifle shot goes where you aim, held fire too: no spread, no bloom). Other weapons are unchanged. |
| PRACTICE MODE | Every weapon and powerup respawns every 30 seconds. |
| PRE-GAME COUNTDOWN | A 3-2-1 on a black screen before the game starts; you can look around but not move or shoot. A weapon switch (Y) pressed during it is kept and taken as it ends, as in Halo 1: NHE; any other button pressed during it does nothing until you let go of it, so nothing fires, zooms, reloads or picks up the moment it ends. The host holds every player to it, including players on builds without it. |
| SILENT MOVEMENT | NORMAL (stock), JUST ME (you hear your own footsteps, jumps, landings and slides; everyone else's make no sound for you) or SILENT (nobody's, yours included). |
| SILENT WEAPONS | The same three for readying and putting away a weapon (the weapon-switch sounds), first and third person. Firing, reloading, melee and pickup sounds (the overshield's too) always play. |
| SOUND PRESET | SPLIT SCREEN sets SILENT MOVEMENT and SILENT WEAPONS both to SILENT, for players sharing one set of speakers (JUST ME can't tell whose ears a sound is for there); CUSTOM leaves the two rows as you set them. |
| BALL MELEE | Oddball gametypes only. STOCK (the ball's melee takes the shield and a little health, and kills from behind) or LETHAL (the ball carrier's melee kills in one hit, shields and overshield included, as in Halo 3: same reach, no hits through walls, friendly fire as the gametype sets it). |

While SILENT MOVEMENT or SILENT WEAPONS is not NORMAL, only Arena Evolved (0.2.3 or later) can join the game: a
machine without the options would still hear the sounds, so the host turns it away (one already in the lobby when
the gametype is switched on is removed). With both NORMAL, everyone joins as before. Every gametype, the seeded ones
included, starts with both NORMAL. The options are only how a sound plays on each machine; the game itself is the
same.

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
  (5 minutes held; the ball carrier's melee kills in one hit, BALL MELEE LETHAL), each with a 15-minute time limit, the motion sensor, the
  stock respawns, item TIMERS with waypoints, and both weapons dropped on death
  (DROP SECONDARY ALWAYS). The 2V2 ones (TEAM AE 2V2 SLAYER to 25, CTF, KING
  and ODDBALL, with the lethal ball) play the same rules; the player limit of 4 is the host's setting.
  FFA AE PRACTICE: every weapon and powerup each 30 seconds, 500 kills, no time
  limit, the map's vehicles. All of Arena Evolved's gametypes play COMBAT
  FEEDBACK: ARENA EVOLVED.
- **Arena Evolved free for all variants** (the stock free for all gametypes on
  the casual rules, 15 minutes, item TIMERS with waypoints, both weapons
  dropped; each keeps its own score, lives, ball, hill and vehicles): FFA AE
  ELIMINATION (one life), PHANTOMS (invisible), ENDURANCE (5 lives), ROCKETS,
  SNIPERS, JUGGERNAUT, REVERSE TAG, ACCUMULATION, STALKER, KING, CRAZY KING
  (the hill moves), RACE and RALLY (these two need a map with race flags).
  They keep the stock ball melee (the ball in these modes is never held in
  hand, so BALL MELEE LETHAL has nothing to act on) and their own motion
  sensor, so FFA AE ROCKETS, SNIPERS, PHANTOMS, REVERSE TAG, ACCUMULATION and
  STALKER have none. FFA AE JUGGERNAUT plays Halo 2's rules: to 15, the
  juggernaut with extra damage at normal speed, only the juggernaut's kills
  score, his killer takes the role, and a random player gets it when he kills
  himself or there is none.
- **Arena Evolved special modes** (team slayer to 50): SNIPERS (sniper rifle and
  pistol, the sniping weapon set, no motion sensor), SHOTTY SNIPERS (shotgun
  and sniper rifle, no weapons on the map), SWAT (pistols, no shields, classic
  health, no grenades), ROCKETS, SHOTGUNS and HEAVIES (75 kills, the map's
  vehicles).
- **Arena Evolved BR** (0.2.6): the casual set, the free for all variants, FFA AE PRACTICE and AE COMP again with the
  **battle rifle in hand** and the pistol second (FFA AE BR SLAYER, TEAM AE BR SLAYER, TEAM AE BR COMP CTF, ... 25
  gametypes; stored as AE BR FFA, AE BR TS, AE BR C CTF, ...). Every other value is its twin's. Not made for the
  single-weapon modes (snipers, rockets, shotguns, SWAT, heavies, shotty snipers), RACE, RALLY or TRAINING. They
  need the battle rifle's pack (the `packs` folder): only machines with it join; on Custom Edition maps, and where
  the pack is not, the slot plays an assault rifle.
- **Arena Evolved COMP**: FFA AE COMP SLAYER (25 kills), TEAM AE
  COMP SLAYER (50), TEAM AE COMP CTF (3 captures), TEAM AE COMP KING and TEAM
  AE COMP ODDBALL (5 minutes held, with the lethal ball). 5-second respawn and suicide penalty, 2
  frag grenades, no motion sensor, no vehicles, item TIMERS only in line of
  sight (no list, no item calls), the objective only in line of sight, both
  weapons dropped; the slayers have no time limit, the others 15 minutes.
- **FFA AE TRAINING:** item TIMERS and TRAINING's markers, plays to 500, no
  time limit.
- Every Arena Evolved gametype has no fall damage, HALO 2 health (SWAT:
  Halo CE), NO SPREAD FIRST SHOT, the PRE-GAME COUNTDOWN and generic starting
  equipment, with a pistol in hand and an assault rifle unless its mode says
  otherwise.
- **Halo 1: NHE's own 23 gametypes** (TS 50, TS 100, TS ON-OFF, TS TRAINING,
  TS PRACTICE, TS SNIPERS, FFA 50 NR/R, 1 V 1 NR/R, KOTH 5M 7S/10S, BALL 5M
  7S/10S, eight CTFs and CTF WIZARD), with NHE's values: Halo CE health, fall
  damage, NO SPREAD FIRST SHOT, the countdown, NHE EXTRAS (everyone joins red, no team
  swap, the dead camera on you, the match clock counting up), no death bonus or
  kill penalty in slayer. On Halo 1: NHE's maps each plays its NHE mode; on
  other maps no vehicles.
  With them, TEAM NHE VANILLA and TEAM NHE POWERUPS: team slayer on Arena
  Evolved's casual values that plays Halo 1: NHE's VANILLA and NHE & POWERUPS
  modes on NHE's maps. NHE MODE and NHE EXTRAS are on a gametype's **NHE
  OPTIONS** screen.
- The lists show display names (TEAM AE SLAYER over the stored AE TEAM SLY):
  the Arena Evolved set, AE BR, AE COMP, the NHE set, TRAINING, then your own
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
- **PS5 DualSense: adaptive triggers and lightbar.** R2 has a feel for each
  weapon and a kick at each shot, human weapons mechanical (clicks, kicks,
  the heavy left rumble motor), Covenant weapons energy (resistance with
  heat or charge, the light right motor): the pistol a click and a snap,
  the assault rifle a pulse a round, the sniper rifle firm with a heavy
  thump, the shotgun a big kick and its pump, the plasma rifle stiffening
  with heat, the plasma pistol building with its charge and a blast on the
  overcharged shot, the needler a fast light buzz, the battle rifle three
  pulses a burst. The weapon's state comes first: a settle as it is ready,
  slack with a bump as the magazine goes in, a dry click when empty, locked
  with a throb while overheated. In a vehicle the seat that fires the gun
  feels it. L2 is the grenades (a jolt as one is thrown, a dry click with
  none), light while zoomed. The lightbar takes your colour (your team's in
  a team game), pulses while an overshield charges, dims with active
  camouflage and flashes when you become the juggernaut; or, with LIGHTBAR
  on SHIELD, it shows your shield as the HUD's meter does. The player lights
  show your split-screen number. Settings > Gamepads: ADAPTIVE TRIGGERS,
  TRIGGER STRENGTH (LIGHT, NORMAL, STRONG) and LIGHTBAR (OFF, PLAYER COLOR,
  SHIELD), on by default; other controllers are never touched. On Linux
  without Steam, see [DualSense on Linux without
  Steam](#dualsense-on-linux-without-steam).
- 14 controller layouts, including the Anniversary set and MCC's Universal
  Reclaimer, Zoom & Shoot, Bump & Jump and Green Fingers; layouts that put
  actions on the d-pad don't also move you with it.
- A performance overlay in the HUD's style (Settings > Video > GRAPHICS >
  PERFORMANCE: FPS, or FPS with frame times and draws; top left or top
  right).
- A match clock, as in the Master Chief Collection (Settings > HUD >
  MULTIPLAYER GAME TIMER: OFF, TIME REMAINING, TIME ELAPSED or BOTH). TIME
  REMAINING shows the time left (counting up when there is no time limit),
  TIME ELAPSED the time played, and BOTH the time left with the time played
  smaller above it (the time played alone with no time limit). It sits in
  the bottom right corner of each view, lined up with the motion sensor.
  On Halo 1: NHE's maps it replaces their own clock.
- A campaign timer, as in the Master Chief Collection (Settings > HUD >
  CAMPAIGN TIMER: OFF or ON, off by default): the time played on the level,
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
  modes working. NHE's files are not included: get Halo 1: NHE 1.0 and its
  gametypes pack from the NHE team's site, [halo1nhe.com](https://halo1nhe.com/#download),
  and put its maps in `mods/NHE/maps`.
- Xbox community maps (the v5 map pack format) and Halo PC maps, each kind
  in a folder beside `maps`: Custom Edition maps in `maps_ce` (with the
  Custom Edition shared files, `bitmaps.map`, `sounds.map` and `loc.map`,
  from your own Halo PC install or from Halo: The Master Chief Collection on
  Steam, `halo1/maps/custom_edition`, which AE finds and can read in place:
  it asks the first time such a map needs them, and Settings > MAP FILES sets
  it), HaloMD maps in `maps_md` and your
  Halo PC disc's maps in `maps_pc`. The older `maps/ce` and `md_maps`, and
  OpenCE's `custom_maps`, still play from where they are; at its first
  start nothing is moved; `game.move_old_map_folders = "ask"` offers to move
  the first two, `"yes"` moves them.

- Controls: an MCC mouse style (Halo: The Master Chief Collection's
  sensitivity number, with zoomed and vehicle scales) and hold to zoom (ZOOM:
  TOGGLE / HOLD / BOTH; the wheel or the flashlight button steps the level
  while zoom is held), both on Settings.
- Item waypoints with pictures (Settings > HUD > WAYPOINTS: TEXT, ICONS or
  BOTH), their size set by WAYPOINT SIZE (50% to 100%).

**Privacy**
- Internet games you host start PUBLIC, as upstream's do: they show in the
  in-game Server Browser unless you set LISTING to PRIVATE in Server Setup.
  Listing them on the website game list, reporting the games you join to a
  stats site, and Delta Stats (what happens in the games you host: kills
  with positions, accuracy, medals, sent to that site; on by default in
  ChupathingyCE) stay off unless you turn them on (Settings > Network).
- SERVER PINGS (Settings > Network, off by default): the Server Browser
  shows each listed game's ping. Timing a ping makes the same connection a
  join makes, so every host in the list sees your address. Off, no host is
  contacted until you join one.
- Crash reports, which ChupathingyCE's releases send to its site, are off in
  every Arena Evolved build: nothing is written to send and nothing is asked.
- The game does not fetch ChupathingyCE's signed table of network versions
  (Delta) unless you turn `network.legacy_table_fetch` on (it would change
  nothing: Arena Evolved has its own wire ID). In a game, machines that speak
  Delta tell each other their platform and build; your player ID only with
  `network.share_profile` (off). `network.protocol = "opence"` turns Delta
  Peer off.

## Where it's going

Three rulesets under one roof:

1. **AE Casual**: Arena Evolved's rules with item timers, training aids and
   the quality-of-life options on.
2. **AE Comp**: modern competitive settings, closer to today's HCS,
   researched rather than guessed.
3. **Classic Comp (NHE)**: the Xbox competitive rules, faithfully, with
   Halo 1: NHE's own maps and gametypes as an option.

Next up: a side-by-side split-screen layout, a
per-player in-game menu and HUD, and more of our own voice for the callouts
(Cori, beta, ships now), calling every item.

## System requirements

| Download | Needs |
| --- | --- |
| Windows 10 or 11, 64-bit | A graphics card and driver with OpenGL 4.5 (core profile). Nothing to install: `halo.exe` imports only Windows' own DLLs (no Visual C++ runtime), and `SDL3.dll` is in the zip. |
| Linux, any distribution (`-linux64.zip`) | 64-bit x86-64; glibc 2.35 or later (Ubuntu 22.04 and later, Zorin OS 17 and 18, Linux Mint 21 and 22, Pop!_OS, Debian 12 and similar; the build's CI starts it on Ubuntu 22.04 and 24.04 and Debian 12), a graphics driver with OpenGL 4.5 (Mesa or NVIDIA), and an X11 or Wayland desktop. SDL3 is included. Sound goes through PipeWire, PulseAudio or ALSA, whichever the system has; controllers need udev. A PlayStation controller (DualSense, DualShock 4) should work without Steam's udev rules on Linux 5.12 and later (not yet confirmed on real hardware). |
| Linux, Arch and other rolling distributions (`-linux64-arch.zip`) | The same, built on Arch Linux's glibc, so it needs a glibc as new as Arch's. SDL3 is the system's: `sdl3` on Arch (`pacman -S sdl3`; its dependencies bring the rest), or the distribution's own SDL3 package (Debian 13 and Ubuntu 25.04 have one). |
| macOS 13 or later, Apple silicon | OpenGL 4.1 through Apple's Metal layer ([port/macos/README.md](port/macos/README.md)). Not tested by us. |
| Android 9 or later, 64-bit ARM | OpenGL ES 3.0 ([port/android/README.md](port/android/README.md)). Not tested by us. |

Every download also needs your own Halo: Combat Evolved (below). Playing
Custom Edition maps needs three files of Halo PC or Custom Edition,
`bitmaps.map`, `sounds.map` and `loc.map`, in `maps_ce`; if Halo: The Master
Chief Collection is installed on Steam (Linux or Windows), the game finds them
in its `halo1/maps/custom_edition` folder and asks before using them (it never
copies or changes MCC's files). Versions and the build-time packages are in
[docs/dependencies.md](docs/dependencies.md).

## Download

Releases are on the
[Releases](https://github.com/PeterRichardsonGolf/Halo-CE-Arena-Evolved/releases)
page: 0.2.5-beta, built on ChupathingyCE 0.7.3e and OpenCE build-201
(network version 28), is the current one, a pre-release (2026-10-13); it
plays online with 0.2.3-beta, 0.2.4-beta and OpenCE build-201 and later, not with 0.2.2-beta or earlier.
Each platform is
a zip of its own: Windows and Linux as 64-bit builds only, plus macOS and
Android. For Linux there are two, and the first is the one to take:

- `arena-evolved-<version>-linux64.zip`: **Linux (any distro, SDL3
  included).** It runs on Ubuntu 22.04 and newer and similar systems
  (Zorin, Mint, Pop!_OS, Debian 12): it is built on an older base and brings
  its own SDL3 (`libSDL3.so.0`, next to `halo`).
- `arena-evolved-<version>-linux64-arch.zip`: **Linux, Arch / rolling
  distros (uses the system's SDL3).** The build we play and test; it needs a
  glibc and an SDL3 as new as Arch's.

The dedicated server keeps
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
2. The menus are the PC version's (`display.menus` in `config.toml`;
   MENUS on the main menu switches to the Xbox's and back):
   that's where Arena Evolved's settings live: Settings > Game Options
   (callouts, voice, mods, campaign rules, zoom), Settings > HUD (the match
   clock, HUD area, the item waypoints' pictures),
   Settings > Video (the performance overlay), and in a gametype's options,
   ARENA OPTIONS and INDICATOR OPTIONS.
3. Multiplayer > Create Game > LAN, pick a map and one of the AE gametypes
   (in the gametype list's CUSTOM bank). For split screen, press START on
   each extra controller in the lobby.

On Linux, the maps and settings (`config.toml`) go next to the `halo`
executable, and the saves in `~/.local/share/halo-linux`. Everything the port
adds is in `config.toml`, written the first time the game starts with every
setting listed and explained; see [port/linux/README.md](port/linux/README.md).

### DualSense on Linux without Steam

On Linux the game writes a DualSense's trigger effects and lightbar to the
controller's `/dev/hidraw` device. Steam's udev rules (Steam installed, or
the `steam-devices` package) already let you; without them the Linux
download's `70-ae-dualsense.rules` does. From the game's folder, once:

```
sudo cp 70-ae-dualsense.rules /etc/udev/rules.d/ && sudo udevadm control --reload
```

then unplug the controller (or turn it off) and connect it again. The
buttons and sticks work either way; only the effects need it. `debug.txt`
says how each DualSense is driven, or why it is not.

## Playing over LAN

Every computer must be on the same network: the same router or Wi-Fi, and not
a guest Wi-Fi, which keeps devices apart. LAN games are found through the
network's broadcasts and played over UDP ports **5150**, **5151** and **5160**,
and every computer, host or not, needs them open. Custom Edition maps a player
lacks are sent over TCP port **5153** (map sharing, below the game's own
traffic), which every computer needs open too, to receive a map and to help
send one.

**What is shared.** A host sends the maps you lack: Custom Edition maps, and
(between Arena Evolved players) community maps in the Xbox's format, from its
`maps` folder or its active mod. You get the map the game is on when you join,
and in the lobby **F6** gets all of the
host's maps you lack; press it again to stop. The game asks you once per host,
with the number of maps and their size, before anything is downloaded.
Community maps go to `maps_shared`, Custom Edition maps to `maps_ce`.

**What is never shared.** The game's own maps (its 13 multiplayer maps, the
campaign and the menus): they are never sent, and a download never replaces
them or any other file you have. One host can give you 4 GB at most.

**Internet games.** Maps are shared over the internet only when both switches
are on, and both are off at first: the host's INTERNET SHARING and the
joining player's INTERNET DOWNLOADS (Settings > Network Setup > Map Sharing).
On a local network, sharing is on by default.

**Windows** asks the first time the game starts. Allow it, on private
networks at least, and on public ones too if Windows calls your network public.

**Linux has no such question.** If a firewall is turned on, it silently
blocks LAN games: nobody sees your game, or you see nobody's (Join Game > LAN
says so when it has found nothing for 10 seconds and finds ufw or firewalld
on). Open the ports once on each Linux computer:

- Ubuntu, Zorin OS, Linux Mint, Pop!_OS (ufw): `sudo ufw allow 5150:5151/udp`,
  `sudo ufw allow 5160/udp` and `sudo ufw allow 5153/tcp`. Or, in the Firewall
  Configuration app (Gufw): Rules, +, Advanced, protocol UDP, port `5150:5151`,
  then again for `5160`, then protocol TCP, port `5153`.
- Fedora and others with firewalld:
  `sudo firewall-cmd --permanent --add-port=5150-5151/udp --add-port=5160/udp --add-port=5153/tcp`
  and then `sudo firewall-cmd --reload`.
- Not sure whether a firewall is on? `sudo ufw status` says `inactive` when
  ufw is off; `systemctl is-active firewalld` says `inactive` when firewalld is.

Turning the firewall off also works, but opening the ports keeps it on for
everything else.

## Building it yourself

You need Python 3, [ninja](https://ninja-build.org/), clang with lld (or gcc on
Linux) and, on Linux and macOS, SDL3 development files. The game supplies the
Xbox SDK declarations it uses, so you don't need the SDK.

```sh
python3 configure.py --release
ninja linux64        # build/linux64/halo
```

Packages, as the CI installs them ([docs/dependencies.md](docs/dependencies.md)
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
  [port/windows/README.md](port/windows/README.md).
- **macOS**: Xcode command line tools and `brew install sdl3 ffmpeg ninja`
  ([port/macos/README.md](port/macos/README.md)). **Android**: the NDK, CMake
  and a JDK 17 ([port/android/README.md](port/android/README.md)).

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
- Online, Arena Evolved is network version 29 (from 0.2.6-beta): it plays
  with OpenCE builds on network 29 (build-206 onward, until OpenCE changes
  its network version) and ChupathingyCE 0.7.5b, both ways, and joins only
  games hosted on version 29 (an older host is refused: the host needs to
  update). Builds that join only hosts of 28 or older (Arena Evolved
  0.2.3-beta to 0.2.5-beta, OpenCE build-205 and earlier, DamnationCE,
  ChupathingyCE 0.7.4 and earlier) don't join its games: their players are
  told the host is newer. 0.2.3-beta to 0.2.5-beta (28) play with OpenCE
  build-201 to build-205. A game on a Custom Edition map is named by the
  map's own name, as OpenCE names it, its version the map's hash, so OpenCE
  and ChupathingyCE players with the map join it too; players without it
  download it from the game's players when the host shares maps (OpenCE's
  map sharing, Server Setup > MAP SHARING; port/linux/README.md, "Map
  sharing"), or, if they say yes, from the community map repository
  (ChupathingyCE's map downloads, Settings > Network > MAP DOWNLOADS).

## Generative AI

The decompilation this project is built on, and the port it started from, were made with heavy use of generative AI. Chupathingy has its own code and releases now, but a lot of that earlier work is still in it.

We use AI tools the way developers use any other tool. A person decides what to build, reviews every change, tests it, and is responsible for it. Nothing is handed to an AI to build from start to finish without a person involved. We're also working through the code we inherited to clean up what reads as machine-written: comments that only repeat the code, unclear names, and changes nobody can explain.

If you contribute, the same applies to you: see [CONTRIBUTING](CONTRIBUTING.md#using-ai-tools).

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
