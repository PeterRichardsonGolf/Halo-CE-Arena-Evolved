# Changelog

What has changed in Halo CE: Arena Evolved, newest first. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/). There are no
releases yet, so the sections are dated.

## Unreleased

### Added
- The scoreboard shows the time played and the time left ("1:33 PLAYED ·
  8:27 LEFT", or just the time played with no time limit) on its title row,
  or under the title where the two don't fit side by side.
- The "game rules" line in `debug.txt` gives the gametype's time limit.

### Changed
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
  PRE-GAME COUNTDOWN on (NHE VANILLA has neither; PRACTICE has PRACTICE MODE).
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
