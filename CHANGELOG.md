# Changelog

What has changed in Halo CE: Arena Evolved, newest first. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/). There are no
releases yet, so the sections are dated.

## Unreleased

### Changed
- TRAINING's waypoints are labelled: the item and the time to its spawn
  ("OVERSHIELD 0:08"), the item alone once it is on the map, following the
  arrow to the view's edge when off screen. Blood Gulch's mixed spawn reads
  OS/CAMO until it spawns, then OVERSHIELD or CAMO. Labels are red or blue
  for an item at a team's base (nearer the team's CTF flag, else its player
  spawns), the HUD's colour in the middle. An item at both bases is named
  with its side, RED SNIPER / BLUE SNIPER, in the power list too; its
  10-second callout is `red_<item>` / `blue_<item>` when the voice pack has
  the clip (Boarding Action's red and blue rockets).
- CALLOUTS: item calls in any gametype (no longer only with TIMERS or
  TRAINING). A voice pack may have a beep per moment (`beep_minute`,
  `beep_tick` at :20/:30/:40, `beep_item` before item calls; each falls back
  to `beep`), "<item> in ten" clips for the 10-second calls and "<item> is
  up" clips said at each spawn. Calls are planned ahead from the clips'
  lengths: an item's call that would meet the clock's "ten" (or another
  call) moves earlier, items spawning together are called back to back,
  and none is dropped at :50. A VOICE that is no folder falls back to the
  first in `voices/`; packs only in the mod's `voices/` are listed; the clip
  playing stops when the game ends or pauses or CALLOUTS turns off, which
  also frees the clips; a rejected WAV's log says why.

### Added
- CALLOUTS (Settings > Game Options, `game.callouts`): spoken callouts in
  multiplayer, as Halo 1: NHE's voice timer says them. ITEMS: each power
  item's name (rockets, sniper, overshield, camo) 10 seconds before it
  spawns, from its real spawn time on any map, and "five" to "one" before
  the rockets. ITEMS + CLOCK adds
  NHE's talking timer: "N minutes" each minute, "thirty seconds left",
  "twenty seconds", "ten" to "one" before each minute, and its beeps. One
  call at a time, the items' first. Silent on Halo 1: NHE's maps, during
  the PRE-GAME COUNTDOWN and after the game. VOICE (`game.callout_voice`)
  picks a folder of `voices/`; for now the only voice is the player's own
  import of NHE's clips (`tools/import_nhe_voice.py`), not shipped.
- HUD AREA (Settings > Game Options, `display.hud_area`): FULL, 16:9 or 4:3.
  On a wider screen, 16:9 or 4:3 keeps the whole HUD (meters, motion sensor,
  ammo, messages, scoreboard, postgame screens, match clock, power list,
  off-screen waypoint arrows) in a part of the screen of that shape at its
  middle; split screen views' outer sides move in. The view stays wide.
- COMPACT HUD (Settings > Game Options, `display.compact_hud`): with one
  player, the game's smaller split-screen HUD (meters, motion sensor,
  messages). The reticle stays full size.
- SCOREBOARD FADE (Settings > Game Options, `display.scoreboard_fade`): the
  scoreboard fades in and out INSTANT, FAST (0.25 s), NORMAL (0.5 s, as
  before) or SLOW (1 s).
- TRAINING (indicator options) shows a waypoint over each power item (rockets,
  sniper rifle, overshield, camo) from 10 seconds before it spawns to 20
  seconds after, as Halo 1: NHE's Training did, and green markers on the floor
  at the player spawns the gametype uses (within 25 units, hidden behind
  walls). Off on Halo 1: NHE's own maps, which draw their own.
- NO SPREAD has three levels: OFF, NHE (as before) and FULL, where every
  pistol and sniper rifle shot goes exactly where you aim, held fire too (no
  bloom). Every Arena Evolved gametype has FULL; the Halo 1: NHE-style ones
  keep NHE.
- Arena Evolved PRO gametypes: AE PRO FFA, AE PRO TS, AE PRO CTF, AE PRO KING
  and AE PRO BALL (5-second respawn and suicide penalty, no motion sensor but
  in FFA, no TIMERS; slayers without a time limit, the others 15 minutes).
- The log names each gametype seeded and its settings.
- The scoreboard shows the time played and the time left ("1:33 PLAYED ·
  8:27 LEFT", or just the time played with no time limit) on a row under its
  title.
- The "game rules" line in `debug.txt` gives the gametype's time limit.

### Changed
- The Arena Evolved casual gametypes have a 15-minute time limit, the TIMERS
  and the motion sensor; AE SLAYER plays to 25 kills, AE KING and AE ODDBALL
  to 5 minutes held.
- NHE 1V1, NHE 2V2 TS, NHE CTF and NHE POWERUP play Beach LAN 15's rules:
  5-second respawn and suicide penalty, no motion sensor, no time limit; NHE
  1V1 plays to 25 kills, and NHE 1V1, NHE 2V2 TS and NHE CTF are NHE & Timer
  (the warthog vehicle set) instead of Timer Only.
- These apply to gametypes seeded from now on: a save root that already has
  them keeps its own (delete them and the record file to seed them again).
- The host enforces the PRE-GAME COUNTDOWN for every player: a player on a
  build without it (OpenCE, ChupathingyCE) can no longer move off their spawn
  or land hits before it ends.
- The match clock lines up with the motion sensor: as far from the right edge
  as the sensor is from the left, level with its "15m".
- With one view, the TIMERS power list sits in the top corner opposite the
  performance overlay again, and moves below the overlay only when the two
  would overlap.
- NO SPREAD's help says exactly what it does: the pistol's first shot from
  rest and the sniper rifle's unzoomed shots go where you aim.
- The time left in the scoreboard's title reads M:SS, as the clock does.

### Fixed
- The TIMERS power list no longer stays on screen over the end of game.
- The dedicated server builds again (it lacked the Video settings' resolution
  lists' platform calls).
- No crash when a gametype list is the first screen to open after start-up
  (two threads made the first-time gametypes at once).
- The built-in Team Oddball no longer picks up random gametype options: its
  settings were built on uninitialised memory.
- Halo 1: NHE's start countdown ("3...", "2...", "1...") is centred on wide
  screens again, not pushed to the right.
- The match clock no longer shows over Halo 1: NHE's own countdown.
- A title that starts with blank lines is no longer cut off with one view;
  the cut that removes the second copy of a split-screen title applies only on
  Halo 1: NHE's maps.
- The "game rules" log line names Halo 1: NHE's maps correctly.

## 2026-10-05

### Added
- ARENA OPTIONS, a gametype screen of its own (in the gametype editor and in
  Server Setup) with FALL DAMAGE, HEALTH, NO SPREAD, PRE-GAME COUNTDOWN and
  PRACTICE MODE. Player Options is back to the stock screen.
- NO SPREAD: the pistol's first shot and the sniper rifle's unzoomed shots go
  exactly where you aim, as in Halo 1: NHE.
- PRACTICE MODE: every weapon and powerup respawns every 30 seconds, and the
  item timers follow it.
- PRE-GAME COUNTDOWN: a 3, 2, 1 on a black screen starts the game; you can
  look around but not move, fire or throw, and a weapon switch is kept for
  when it ends.
- MATCH CLOCK (Settings > Game Options: OFF, COUNT DOWN, COUNT UP): an
  MCC-style clock in the bottom right corner of each view and on the
  scoreboard. On Halo 1: NHE's maps it replaces their own clock.

### Changed
- The Arena Evolved and Halo 1: NHE-style gametypes have NO SPREAD and the
  PRE-GAME COUNTDOWN on (NHE VANILLA has neither; PRACTICE has NO SPREAD and
  PRACTICE MODE, without the countdown).
- The TIMERS power list shows the item spawns only (the clock is its own
  setting), and in split screen sits in the margin above or below each view's
  HUD.
- The split-screen scoreboard is centred in its view.
- The README describes Halo CE: Arena Evolved.

### Fixed
- A Halo 1: NHE clock title is drawn once with one view, not twice.

## 2026-10-04

### Added
- Split screen in network games, and a network game you can start alone.
- The Anniversary and Master Chief Collection button layouts, including
  Universal Reclaimer, Zoom & Shoot, Bump & Jump and Green Fingers.
- Split Screen in the PC menus' Multiplayer.
- A performance overlay (Settings > Video > PERFORMANCE: off, FPS, minimal,
  full; top left or top right), in the HUD's style.
- Mods (Settings > Game Options > MOD): play a folder of `mods/` over the
  stock maps.
- SETTINGS in the campaign's pause menu, for each split-screen player's own
  profile.
- FALL DAMAGE and HEALTH (CLASSIC, REACH, HALO 2, HALO 3) gametype options,
  and campaign versions in Settings > Game Options.
- TIMERS and TRAINING (indicator options): the power items' next spawns and
  training aids.
- The Arena Evolved and Halo 1: NHE-style gametypes, ready in the gametype
  list.
- Xbox community maps (the v5 map pack format) and Halo 1: NHE's maps, played
  as a mod.

### Changed
- Hosted games are not listed publicly and joined games are not reported to
  stats sites unless you turn them on (Settings > Network).
- The multiplayer map cache holds larger modded maps.
- Cutscene titles on wide screens move with the side of the screen they are
  on, and clear when the postgame carnage report shows.
- Merged OpenCE build-96..101: online split screen (ADD PLAYER), internet
  signalling brokers, a menu crash fix, co-op level choice by either player.
- Merged OpenCE build-102/103: a new game each time Create is chosen, and a
  fix for vertices near the camera.
- Merged OpenCE build-104..111: resolution, scaling and window size
  settings, friendly fire stops teammates' instant kills, death and co-op
  respawn timing, the split-screen divider on wide screens, gamescope
  fullscreen, Windows builds without a console.

### Fixed
- Layouts that put actions on the d-pad no longer also move you with it.
- Choosing a mod at the main menu restarts the game into it.
