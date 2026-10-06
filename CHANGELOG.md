# Changelog

What has changed in Halo CE: Arena Evolved, newest first. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/). There are no
releases yet, so the sections are dated.

## Unreleased

### Added
- MULTIPLAYER GAME TIMER (Settings > Game Options, `display.match_clock`)
  has a fourth value, BOTH (`both`): the corner clock shows the time
  remaining where it always is, with the time elapsed smaller (0.7 of it)
  directly above it, right-aligned to the same edge; with no time limit, the
  time elapsed alone. Help: "Display the time remaining, with the time
  elapsed above it."

- ARENA OPTIONS has a TIME LIMIT row, first: NONE or 10, 15, 20, 25, 30 or
  45 MINUTES, the Halo PC gametype screens' own choices, for the same
  setting. Its description in the gametype editor and Server Setup says so.
- TRAINING waypoints are smaller: the arrows (and their distances) at 60% of the game's own nav points, which are unchanged, and the labels'
  text at 0.65 scale. An arrow whose label found no clear place now keeps one by the arrow, instead of going unlabelled.
- CAMPAIGN TIMER (Settings > Game Options, `display.campaign_timer`, off by
  default): an MCC-style clock of the time played on a campaign level, drawn
  where the match clock is (bottom right, lined up with the motion sensor).
  Game time: stopped in the pause menu, the cutscenes counted (hidden while
  they play); a revert does not take time back; a new or restarted level
  starts at 0:00, a resumed saved game at its time. Game Options' rows are
  closer together (26, not 30) to fit it above the help line.
- The Cori beta voice pack ships with every build (`port/assets/voices/cori`,
  76 clips, made with Piper and the public-domain en_GB-cori-high voice,
  released CC0; credits in its folder). Every build copies it beside the game
  as `voices/` (linux, linux64, windows, windows64, macOS next to the bundle
  and in the release folders; not yet Android's APK). Its builder, picked
  takes and beeps are in `tools/voices/` (optional).

### Changed
- The performance overlay is drawn at the power list's size: the same font
  and line height, at 0.7 of it with one or two views and 0.6 with three or
  four (it was 0.8), in the same colours (yellow under 60 FPS, red under
  30). At the top left the power list stacks under it; at the top right it
  is alone.
- With two views, the power list is drawn as with three or four: the grouped
  line at the foot of each view, centred between the motion sensor and the
  clock, the same in the upper and the lower view (it was over the ammo in
  the upper view and under it in the lower). One view keeps it in the top
  left corner.
- The scoreboard no longer shows the PLAYED / LEFT times: it is back to the
  stock layout (title, headings, rows) in every view, one column or two. The
  match clock is the corner clock alone.
- `debug.test_input`'s scripted menu presses (`menu:`) are held 250 ms of
  their second, not 150, so that a slow frame (a test run at a low priority)
  does not miss one.
- ARENA OPTIONS' TIME LIMIT has finer steps and an hour: NONE, 1 to 10
  minutes by one, then 12, 15, 20, 25, 30, 45 and 60 (the Halo PC gametype
  screens' own TIME LIMIT rows keep their choices). A gametype's time limit
  that is none of them is still kept unless the row is moved.
- Settings > Game Options' MATCH CLOCK row is MULTIPLAYER GAME TIMER, as in
  the Master Chief Collection: OFF, TIME REMAINING or TIME ELAPSED (the same
  `display.match_clock` values: off, down, up), with the Collection's help
  for each value ("Display the time remaining until the game ends."). The
  screen's spinners are wider to fit. A settings screen's row can now have a
  help line for each of its values.
- The power list (TIMERS and TRAINING, "SNIPER 0:22   CAMO 0:52") moves:
  with one or two players it is small, in the top left corner of each view
  (under the performance overlay when that is at the top left, clear of the
  ammo and grenade display: under it in a lower split-screen view, which
  moves the HUD's messages down under the list); with three or four players
  every view has the short grouped line ("0:21 R/B SNIPER OS   0:51 ROCKETS")
  at its foot, between the motion sensor and the clock. A line too long for
  its room is grouped, then made smaller; entries are no longer left off
  (only at the smallest size, as a last resort).
- TRAINING's waypoints (arrows and labels) are hidden in a view while its
  scoreboard shows, so they no longer draw over the scoreboard's text; the
  other views keep theirs.
- The scoreboard's times ("2:25 PLAYED · 7:35 LEFT", "PLAYED" alone without
  a time limit) keep a line of their own directly under the title in every
  view, centred over the table, in the title's font and grey (they were
  right-aligned, in the HUD's blue). A view too narrow for the whole line shortens it
  ("2:25 · 7:35 LEFT", then "7:35 LEFT"), chosen by its length so that it
  does not change while the scoreboard is open.
- Merged OpenCE build-126..128 and ChupathingyCE 0.6.7b: co-op's triggers,
  cutscenes and failed missions work for every player, a client waits on
  its floor for the host's BSP, extra enemies spread onto free ground,
  glass and destructible scenery break the same on every machine, a client
  never reverts its game on its own. Network version 17 is announced; hosts
  of versions 11 through 17 can be joined (OpenCE build-128 and
  ChupathingyCE 0.6.7b host 17 and join only 17).
- Merged ChupathingyCE's main (its 0.6.6b): Halo PC / Custom Edition maps'
  compatibility sweep, protected maps among them; the dedicated server's
  commands, console, control API and web admin page; brokers.txt built in;
  `debug.solo_game`; its Delta docs and network version table. Network
  version 16 is announced; hosts of versions 11 through 16 can be joined
  (OpenCE build-76 and later, ChupathingyCE).
- Merged OpenCE build-113..125 and the commit after it: online co-op, the
  campaign played together over LAN and the internet (host a game, choose
  SINGLEPLAYER on the Map screen, then a level and its difficulty), with
  co-op's later fixes (every
  machine on the host's BSP, doors and elevators, Flood, dropships,
  allegiances, vehicles destroyed by the host), Server Setup's co-op
  FRIENDLY FIRE and EXTRA ENEMIES, a kick command, New Game's and the Map
  screen's SINGLEPLAYER / MULTIPLAYER chooser, room for more than 256
  actors, and (the commit after build-125) much faster frames with many
  enemies. Network version 16. Co-op's Server Setup hides ARENA OPTIONS with
  the other gametype rows.
- The gametype editor's OK keeps a setting whose value is not one of its
  spinner's choices (a 60-minute time limit from another gametype file, say)
  unless that spinner was moved, instead of rounding it to the nearest choice.
- MATCH CLOCK and CAMPAIGN TIMER: the clock's right edge lines up with the
  right end of the shield and health meters' bar in the top right corner (the
  part of the meter's bitmap that shows, so split screen's smaller meters and
  HUD AREA / COMPACT HUD too), its foot still level with the motion sensor's
  "15m". It is drawn at the Master Chief Collection's size (digits about 20
  pixels tall at 1080p) instead of four fifths of the HUD font. With no meters
  drawn it still lines up with the motion sensor.
- The full-screen scoreboard shows its times ("1:33 PLAYED · 8:27 LEFT") at
  the end of the title's row, right-aligned to the panel, when it has two
  columns, instead of on a row of their own. A one-column scoreboard keeps them
  on their own row (always, so they do not move as the score changes), which
  now comes from the margin under the column: a 16-player free-for-all no
  longer pushes its last player into a second column. Split-screen views keep
  the times on their own row.
- TRAINING's spawn markers find their floor on scenery and machines too
  (crates, platforms, bridges), not only the map's structure, so a spawn on
  one has its ring on top, not inside it (on the stock maps: a spawn on a
  flag base on Hang 'Em High and on Prisoner). Bipeds, vehicles, items and
  projectiles are still ignored; the ray stays 0.5 over to 1 under the spawn.
- PRE-GAME COUNTDOWN: a button pressed during it, other than SWITCH WEAPON
  (Y, still kept and taken as it ends, as Halo 1: NHE's), does nothing until
  it is let go of, even after the countdown: a zoom, a pickup or reload, a
  grenade switch or a trigger held through the end no longer act at its
  last tick. The keyboard's reload key is held back with X.
- `game.callout_voice` defaults to `cori` (CALLOUTS stays off by default). NHE's
  imported voice remains an option.
- CALLOUTS: items spawning together are called together when the voice pack
  has the line, before the separate calls: an overshield and a camo,
  `overshield_camo_in_ten` and `overshield_camo_up`; three items or more,
  `powerups_in_ten`, then each one's "is up". Items called with their side
  (`red_sniper`) keep their own calls. Blood Gulch's mixed spawn is
  `overshield_or_camo_in_ten`, and at the spawn the item it really spawned
  is up (waited for up to a second, else not called). Calls that do not
  fit back to back go around the clock's; if they still do not fit, the
  items at both bases lose their sides. A pack without these lines (NHE's)
  makes the calls it has, as before.
- Only TRAINING and CALLOUTS look for the item Blood Gulch's mixed spawn
  made; other games no longer walk the map's objects for it.
- TRAINING's waypoints are labelled: the item and the time to its spawn
  ("OVERSHIELD 0:08"), the item alone once it is on the map. Labels keep
  clear of the arrows, their distances, the power list, the clock and each
  other, moving at most two lines; arrows together at the view's edge share
  one list beside them, and labels with the same time share it ("0:07
  ROCKETS · OS/CAMO · RED SNIPER"). Blood Gulch's mixed spawn reads OS/CAMO
  until it spawns, then OVERSHIELD or CAMO (on a joined machine too). Labels
  are red or blue for an item at a team's base (nearer both teams' CTF
  flags, else their player spawns; bases too close together give no sides),
  the HUD's colour in the middle. An item at both bases is named with its
  side, RED SNIPER / BLUE SNIPER, in the power list too; a power list too
  long for its line gives each shared time once ("0:10 OS/CAMO ROCKETS R/B
  SNIPER OS"). Its 10-second callout is `red_<item>` / `blue_<item>` when
  the voice pack has both (Boarding Action's red and blue rockets).
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
- Arena Evolved 2V2 gametypes: AE 2V2 SLY (25 kills), AE 2V2 CTF, AE 2V2 KING
  and AE 2V2 BALL (team oddball), with the casual rules; seeded into existing
  saves too. (A gametype cannot hold a player limit: set 4 on the host.)
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

### Removed
- Split Screen from the PC menus' Multiplayer (it opened the Xbox's split
  screen screens): a lobby's ADD PLAYER (or another controller's START) adds
  split-screen players to System Link and online games, as upstream's menus do.

### Fixed
- Split screen: a player's pause > SETTINGS (the PC menus' profile and
  settings screens) ran off the bottom of their view, drawn at full-screen
  size in it. With two players the screens are now drawn at half size in the
  player's half, centred (shape kept; the dim covers the whole half); with
  three or four players they are drawn over the whole screen, after the
  views, one player's at a time (the lowest player's; another player's is
  drawn smaller in their own view, under it). One player's menus and the Xbox pause screens are unchanged.
  (Automated tests: `debug.test_input` menu presses can name another
  controller, `2:start`.)
- Pause > SETTINGS > profile > RENAME in a match crashed the game: the Xbox
  virtual keyboard's tags live in ui.map, which a match has not loaded. The
  keyboard now refuses to open (and does nothing) when its tags are not
  loaded, and RENAME is hidden in the in-game pause SETTINGS (it stays in the
  main menu's).
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
