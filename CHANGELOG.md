# Changelog

What has changed in Halo CE: Arena Evolved, newest first. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/). The first
release is 0.1.0-beta, a pre-release; the sections before it are dated.

## Unreleased

### Added
- Screenshots and recordings: F9 saves the game's picture as a PNG in
  `screenshots/` beside `config.toml` ("SCREENSHOT SAVED" at the top right);
  F10 starts and stops a recording with the game's sound, an MP4 in
  `recordings/`, made by ffmpeg, which the game does not ship (next to the
  game, on the PATH, or `capture.ffmpeg_path`; without it F10 says
  "RECORDING NEEDS FFMPEG"). Recordings keep to real time at a fixed rate
  (`capture.record_fps`, 60 or 30) whatever the game's frame rate,
  `capture.record_quality` low / medium / high, and a red dot shows while
  recording (`capture.record_indicator`), never in the video; the game never
  waits for ffmpeg, and a recording whose ffmpeg fails stops at once (on
  Windows ffmpeg runs without a window). For tests: `HALO_RECORD_SECONDS`
  records that many seconds from the first frame of play, `HALO_RECORD_DIR`
  sets the folder, and `HALO_SCREENSHOT_FORMAT=png` writes the
  `HALO_SCREENSHOT_EVERY` frames as PNG (BMP stays the default). No settings
  screen yet.
- SPAWN HEAT (Settings > Game Options, under VOICE; `display.spawn_heat`,
  `HALO_SPAWN_HEAT`): with the gametype's TRAINING, the spawn markers are
  coloured by how likely each spawn is to be picked next, by CE's own spawn
  rules worked out on each machine (nothing is sent, the game's random
  numbers are untouched): MINE (the default) for your player, ENEMY for the
  other team (team games), OFF the plain green markers. Cold blue-grey to
  hot orange-red, the likeliest pulsing (all green, none pulsing, when the
  spawns that can be picked are about equally likely: the least likely at
  least 80% of the likeliest); dark red with a cross for a spawn that can't
  be picked now; the other team's CTF spawns faint. Markers and flashes fade
  as the camera comes within 4 units, to a fifth at 1.5 units, so the one
  underfoot doesn't cover the view. A ring
  flashes where a player really spawns, and debug.txt logs each spawn with
  the chance it had. Each split-screen view has its own.
- CALLOUT DETAIL (Settings > Game Options, under CALLOUTS;
  `game.callout_detail`, `HALO_CALLOUT_DETAIL`): MINIMAL says one line a
  wave of items spawning together and the first item's "is up", with no
  beeps and only the minutes and "thirty seconds left" of the clock;
  STANDARD is the calls as before; VERBOSE names every item of a wave
  ("rockets, sniper and overshield in ten") and says each one up in turn.
  Cori has the new lines: "power weapons", "power items" and "weapons and
  power items" in ten / are up, "and", "in ten" and "powerup is up".

### Changed
- Split screen: every view has its own safe area, 7.5% of its own height in
  from its top and foot and 7.5% of its own width in from its sides (the
  share the title-safe frame takes of one view), and the camera aims through
  its middle. The reticle and aim now sit at the centre of each half and
  quarter (they were 16 lines low or high in halves and 29.5 units off to the
  side in quarters, about 36 and 66 pixels at 1080p), and the HUD is laid out
  the same in every half and every quarter, upper and lower, left and right.
  The view shows the world at half of one view's scale (2% closer than
  before). One view is unchanged.
- The power list (TIMERS, TRAINING) is a column stacked over the match clock
  in the bottom right corner of every view: two aligned columns, NAME and
  TIME, the times flush right on the clock's right edge, the soonest at the
  foot; an item at both bases on one line ("R/B SNIPER", R and B in the
  bases' colours); at most four lines (the three soonest and "+N"); a line
  ten seconds from its spawn or nearer drawn brighter; hidden while that
  view's scoreboard shows (the clock stays), so a 16-player board never meets
  it. `display.power_list` (`HALO_POWER_LIST`) `"top_left"` keeps the single
  line in the top left corner with one view.
- The match clock's digits are tabular (each in a cell as wide as the widest
  digit), so it no longer shifts sideways as the seconds tick; the power
  column's times use the same cells, their colons in one line.
- Gametype names: AE SLAYER is now AE FFA SLAY and AE ODDBALL is AE FFA
  BALL, so the free for all gametypes say so (the other Arena Evolved ones
  are team games). On a save that has them, the old ones are renamed in
  place while they are as they were seeded; one you changed is kept and the
  new one added beside it, and one you deleted stays deleted.
- The gametype lists (the PC menus' and split screen's) show the custom
  gametypes in order: the Arena Evolved set, then the NHE set, then your
  own, each alphabetical; the built-in gametypes keep their order.
- Callout timing: the items' calls are planned before the clock's, so an
  item alone is called on its 10-second mark and is up on its spawn's tick
  (they were up to 6.7 seconds early and 2 to 4.5 seconds late, moved
  around the clock's "ten" and minute calls); a wave starts early enough
  to end by its 10-second mark; the clock's "ten" gives way to an item's
  call due then, and the minute comes after the spawn's "is up". An OS/CAMO
  spot's item is said up as soon as it is seen, else "powerup is up" half
  a second on (it was left out). Items spawning more often than every 10
  seconds are no longer said up.
- In a match, one player at a time edits a profile from the pause menu's
  SETTINGS: another player's SETTINGS shows "PLAYER n IS EDITING" with
  GAMEPADS and CHANGE COLOR dimmed until that player is done, and players 2
  to 4 without a profile get "NO PROFILE" (they no longer edit player 1's).
  CHANGE COLOR says the colour is used from the next time you join a game.
- Multiplayer pause screens (PC menus): B or Back closes only the pressing
  player's pause, not every player's.
- The network test's quick launch gives each local player the last-used
  profile, so their controller layouts apply.
- Merged OpenCE build-140..144: network co-op's campaign has the
  scoreboard (names and pings), a respawn starts behind its teammate, a
  gate one player passes (a script waiting on a trigger volume) brings the
  rest of the team, and only the host's crossing of a loading zone switches
  the BSP (the team comes to the host; a client on a loading zone is told
  it waits for the host); killing blows reach every client (sent again
  reliably) and a body come to rest is sent three times, so none hangs in
  the air; a player joining a game in progress starts at zero; the host's
  garbage collection no longer runs every tick in network co-op; a third
  hardening round (what another machine sends, checkpoints and cores
  checked against the game state as made before they are taken, a map
  whose header names another map refused, `loading.tga` read only as the
  loading screen's picture, a broken lights array survived and reported,
  `debug.network_corrupt` to test it); and fewer GL calls (a sampler object
  for each sampler state, a vertex array object for each vertex layout,
  the changed texture units bound in one call, the water's mip texture
  rebuilt only when a level changed, visibility tests read on the CPU no
  longer stalling). Network: OpenCE build-141 raised its version to 21,
  which is additive between 20 and 21 (NETCODE.md), so Arena Evolved still
  announces 20, which ChupathingyCE 0.7.0b joins, and now joins hosts of
  11 to 21, OpenCE build-141 to build-144's included; their clients, which
  join only their own version, don't join Arena Evolved's. A client's new
  check of the map's name takes a Custom Edition or HaloMD map's
  (`<file>@ce`, `<file>@md`), so their games still join. macOS keeps
  pointing each attribute and binding each texture unit on its own, as
  OpenGL 4.1 has neither vertex attribute binding nor multi-bind.
- Merged ChupathingyCE 0.7.0b. Network: Arena Evolved now announces
  version 20, as ChupathingyCE 0.7.0b and OpenCE build-133..139 do, and
  joins hosts of 11 to 20, so those builds and Arena Evolved join each
  other's games both ways; ChupathingyCE 0.6.8b and OpenCE build-129..131
  (18) no longer join Arena Evolved's hosts. Every public listing,
  a game with a password's and its tombstone too, is version 20 (this
  replaces the version 18 and the listings labelled by their layout of the
  entries below). A game with a password's invite still never goes to the
  game list's site. Delta, ChupathingyCE's network family, comes with it:
  Delta Peer, messages beside the game's between the machines of a game
  (UDP, the game's port + 10: 5160): each machine's platform and its
  player limits, its build ("Arena Evolved 0.1.0-beta"), and its player ID
  only with `network.share_profile`; it never decides game state, and
  `network.protocol = "opence"` turns Delta Peer off. Arena Evolved's host
  never lets a HELLO replace a machine's Delta session while it stays in
  the game (another device behind the same address could otherwise take it
  over: NETCODE.md, "Known Delta Peer limitations"). A game with a
  password's invite is read with its password at once, so one set
  meanwhile can't let its new invite reach the game list. ChupathingyCE's
  signed table of the network versions to announce and join does not
  apply to Arena Evolved: it has its own wire ID (`ae-20a`, which those
  tables have no row for), so its numbers stay 20 and 11 to 20 and no such
  table turns any of its Delta capabilities off; and it is not fetched
  unless `network.legacy_table_fetch` is turned on (off by default;
  `network.legacy_table` still sets a table by hand). The log's lines
  start `arena-evolved: `, and the game's requests say ArenaEvolved (user
  agents). Also: a dedicated server's
  game says so in the Server Browser's Rules line; the log starts with the
  build's identity (Arena Evolved's name, version, commit, platform and
  upstream base) and the network numbers; `audio.resampling` ("sinc", or
  "linear" as before OpenCE's build 130); muffling off with the reverb;
  Halo PC maps in the maps folder explained at start instead of a blue
  screen; keyboard crouch-walking, and Controls Setup's keys by the
  keyboard's own labels; a deleted profile's name can be used again; the
  PC menus' Settings on a new install makes a profile; a network game's
  pause menu no longer moves its player with the controller that works
  it; a Custom Edition map's script data checked as Halo PC wrote it; a
  banned machine's join refused as banned; the Windows build starts on
  Windows 7.
- Merged OpenCE build-139: a map's tags are checked against a schema of
  their groups before the game uses them, and each structure bsp as it
  loads; a map whose blocks lie outside its tags is refused, smaller
  problems are corrected and written to debug.txt
  (`build/linux/map_validate [--strict] map.map...` runs the same checks
  without the game). A map's scripts may call only the script functions a
  map needs: one that calls a refused one (files, saved state, the
  console, debugging, cheats, map switching, the network, player settings)
  does not run. A third hardening round (AI, caches, cutscenes, recorded
  animations). The maps that played before still play, with their scripts:
  Custom Edition maps are checked as before, not by the Xbox schema; Halo
  1: NHE's maps load, corrected (mostly their tags' parent groups); CE+ X's
  ui.map, which holds bytes that are the same once for several tags, is
  accepted (a mod's maps only: every other map is refused for such bytes,
  as upstream's check does) where those bytes are the same structure, or no runtime value of
  either and unchanged by the check; Halo PC's script functions and globals
  that Custom Edition maps call stay allowed, a Custom Edition map's
  scripts may also set rasterizer_wireframe (H2_Zanzibar's does; no other
  global a map may not set, and the number of players a map starts with is
  never more than four), and the main menu's scripts (ui.map's) may
  switch maps and flush the caches (CE+ X's menu does). NHE's Prisoner Bots
  (a10, which crashed when hosted before) calls map_reset, which is
  refused. Network version and messages unchanged by it.
- Merged OpenCE build-133..138: password-protected public lobbies (Server
  Setup's PASSWORD for a PUBLIC internet game; the Server Browser shows a
  lock and asks for the password, which opens the listed invite; an invite
  link still joins without it), the server browser's lock icon, the profile
  settings' picture of the profile's gamepad layout on GAMEPADS (the Xbox's
  five button settings; Arena Evolved's own layouts show the controller as
  before, and ABOUT keeps its picture), the port's
  own zlib for the maps', menus' and HUD's data, dynamic-light storage
  fixed, and a second hardening round (models, animations, effects, sounds,
  scripts and network messages checked; a host spends at most 10 ms a frame
  on each machine's messages). Server Setup's rows are 23 apart (was 25) for
  its twelve places. Windows crash reports (OpenCE's Sentry) are off in
  every Arena Evolved build, HALO_CRASH_REPORTS_ANY_BUILD included: no
  minidump, no question, nothing sent (the crash's lines still go to
  debug.txt).
  Network: Arena Evolved still announces version 18 (so ChupathingyCE 0.6.8b
  and OpenCE build-129..131 join its hosts) and now joins hosts of 11 to 20,
  OpenCE build-132..138's included (19 and 20 are additive). A public
  listing's version states its layout: a game with a password is listed as
  20, which browsers of 18 skip, every other game as 18. OpenCE
  build-132..138 join only their own version, so they do not join Arena
  Evolved's hosts; OpenCE build-138's Server Browser shows Arena Evolved's
  games with a password, but its players cannot join them. A game with a
  password's invite is never sent to the game list's site
  (`network.browser_url`): not listed, not claimed after the game, not in a
  joined game's report.
- Merged OpenCE build-129..132: maps' and the network's input checked
  before it is trusted (a damaged map or message is refused, not followed);
  anti-aliasing (Settings > Video ANTI-ALIASING: FXAA, SMAA, SSAA 2X, MSAA
  2X/4X/8X, `display.anti_aliasing`), PER-PIXEL LIGHTING
  (`display.per_pixel_lighting`) and SHADOW RESOLUTION
  (`display.shadow_resolution`); the audio's I3DL2 reverb and muffled sounds
  behind walls (Settings > Audio REVERB, `audio.reverb`), a resampler with a
  windowed sinc, a limiter, sound distances and Xbox ADPCM decoding fixed;
  New Game's multiplayer maps played alone; co-op's PLAYER COLLISIONS in
  Server Setup (`network.coop_player_collisions`). Settings > Video's rows
  are closer together (20, not 25) for its fourteen places. The callouts'
  voice is mixed dry (not reverberated) and goes through the new limiter.
  Network version stays 18, ChupathingyCE's: OpenCE build-132 announces 19
  and joins only 19, so it does not play with this version until
  ChupathingyCE follows it (build-129 to build-131 do).
- Merged ChupathingyCE's main (its 0.6.8b): network version 18, as OpenCE's
  build-129. Network version 18 is announced; hosts of versions 11 through
  18 can be joined (ChupathingyCE 0.6.8b and OpenCE build-129 to build-131
  host 18).

### Fixed
- Linux: Alt+F4 quits, as closing the window does (a fullscreen game's
  keyboard grab kept the desktop's shortcut from reaching it).
- No red "event handler function failed" line after saving SETTINGS.
- Halo 1: NHE's maps and Custom Edition maps load again past the merged
  map checks (NHE's ui, a10 and atlas maps end a little short of their
  header's size; NHE Blood Gulch's scripts sit 2 bytes off alignment; CE
  maps' scripts are checked against the CE tag cache).

## 0.1.0-beta - 2026-10-06

Arena Evolved's first pre-release, built on ChupathingyCE 0.7.0b and OpenCE
build-139: tagged v0.1.0-beta and published as a pre-release on 2026-10-06.
The Unreleased entries above came after it; the entries below were first
made on ChupathingyCE 0.6.7b and OpenCE build-128.

### Changed
- Arena Evolved has its own name and version: the window's title, the
  dialogs and the start-up log say "Halo CE: Arena Evolved 0.1.0-beta" (the
  log also names the upstream base), and the downloads are
  `arena-evolved-<platform>-<configuration>`. Save folders, `config.toml`
  and network play are unchanged, and it still plays with OpenCE and
  ChupathingyCE of the same network version.
- The self-updater is off: no build of Arena Evolved looks for, offers or
  installs a new version (and never a ChupathingyCE one). New versions are
  on the Releases page.

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
- One text size per view for all of Arena Evolved's HUD text: the power
  list, TRAINING's waypoint labels (and their distances and arrows), BOTH's
  time played and the performance overlay (in the first view) are drawn at
  the view's text scale, 0.7 of the HUD's smaller font in one view and
  smaller in a smaller view (about 0.6 in half the screen, 0.53 in a
  quarter); the clock is a step larger by the same rule (0.9 in one view).
  The overlay was 0.8 and the waypoint labels 0.65 in every view. The
  overlay keeps its colours (yellow under 60 FPS, red under 30); at the top
  left the power list stacks under it, at the top right it is alone.
- The power list's line at the foot of a split-screen view sits on the
  clock's baseline (the motion sensor's foot) in every view, upper and
  lower (the lower views had it at the screen's bottom edge, under their
  clock), and stops two of the clock's digits short of the clock's left
  edge (the wider of BOTH's lines), made smaller to fit rather than crowd it.
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
