# Changelog

What has changed in Halo CE: Arena Evolved, newest first. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/). The first
release is 0.1.0-beta, a pre-release; the sections before it are dated.

## Unreleased

- **PS5 DualSense: adaptive triggers, lightbar and player lights.** R2 has a profile for each weapon, from the
  weapon's own numbers (pistol, assault rifle, sniper rifle, shotgun, rocket launcher, plasma pistol, plasma rifle,
  needler, flamethrower, battle rifle): a feel as you hold and pull it, a kick at each shot, and the weapon's state
  first: free while reloading, switching weapons or empty, the plasma rifle locked while overheated, the plasma
  pistol building with its charge, a powerful pulse at each shot and a big heavy kick as an overcharged shot goes. In
  a vehicle a gunner's seat takes the vehicle's gun, any other seat none; a
  carried ball or flag leaves R2 free. L2 is light while zoomed, and buzzes while an overshield charges or as you
  become the juggernaut. The lightbar takes your colour (your team's in a team game; black armour shows a dim white),
  pulses while an overshield charges, breathes while you have one, dims with active camouflage and flashes white as
  you become the juggernaut; LIGHTBAR SHIELD shows your shield instead, as the HUD's meter does (its own colours,
  dimmer as it drains, blinking when low, the empty colour when down, a white flash on the pop with COMBAT FEEDBACK
  ARENA EVOLVED, the overshield's layer colours, dark while dead). The player lights show your split-screen number.
  Settings > Gamepads: ADAPTIVE TRIGGERS, TRIGGER STRENGTH (LIGHT, NORMAL, STRONG) and LIGHTBAR (OFF, PLAYER COLOR,
  SHIELD), on by default; no other controller is touched,
  and the triggers and lightbar are given back when you quit. Windows and macOS through SDL; on Linux the effects
  are written to the pad's hidraw device, which Steam's udev rules open, or the new `70-ae-dualsense.rules` in the
  Linux downloads (README: "DualSense on Linux without Steam"). Bluetooth and the DualSense Edge are handled but not
  yet tried on hardware.

- Built on ChupathingyCE 0.7.5b and OpenCE build-206: network version 29.
  Arena Evolved now plays with OpenCE build-206 and ChupathingyCE 0.7.5b
  (both ways), no longer with 0.2.5-beta and older or OpenCE build-205 and
  older (network 28).
- Custom Edition maps keep their own names: a game names its map as it is
  (`hugeass`), its version the map's hash. Maps that 0.2.3-beta to
  0.2.5-beta renamed for their hash (`hugeass_1a2b3c4d.map`) get their own
  names back.
- Map sharing (from the host and GET ALL) follows the new names.
- New from ChupathingyCE: MAP DOWNLOADS (Settings > Network): a Custom
  Edition map a game needs can come from the community map repository,
  asked first. A version mark on the main menu.
- Map loading hardening from OpenCE: a tag's name inside another tag's data
  is dropped; a structure's lightmap vertices after a cut are none.
- The lobby: B from a pad with no player in the game (one whose player just
  left, a spare pad, or the keyboard once player 1 left) takes this PC out
  of the lobby. It did nothing, and the lobby looked stuck.

- **AE JUGGERNAUT plays like Halo 2's**: to 15; the juggernaut has a full overshield (it climbs back after a hit
  and does not wear off), deals extra damage and moves at normal speed (stock CE:
  slow). Only the juggernaut's kills score, one point each; killing the juggernaut makes you the juggernaut but scores
  nothing. When the juggernaut kills himself or dies to the map, or there is none (the start, he quit), a random
  player who is alive becomes the juggernaut. The host decides; the rule is part of the gametype (no row in the
  editor). An AE JUGGERNAUT you never changed updates on the first start (backed up first); one you edited is kept
  as it is.
- **AE BR gametypes**: 25 new gametypes with the **battle rifle in hand** and the pistol second, each a twin of an
  Arena Evolved one (the casual set, the free for all variants, PRACTICE and AE COMP; not the single-weapon modes,
  RACE, RALLY or TRAINING), in their own block between Arena Evolved and AE COMP: FFA AE BR SLAYER, TEAM AE BR
  SLAYER, TEAM AE BR COMP CTF, ... They are added on the first start (seed revision 11); nothing you have is changed,
  and a gametype of yours with one of their names is kept. They need the battle rifle's pack, as the rifle does.
- **More saved games**: the game now lists 160 saved games (profiles, gametypes and the 26 built-in gametypes
  together; it was 100, and Arena Evolved's 86 gametypes would not have fit). **Going back to an older build**
  (0.2.5-beta and before) on the same save folder: it still lists your profiles and gametypes, but past its 100 it
  leaves out some of the built-in gametypes (12 or more, one more for each saved game of yours) and the default
  profiles, and it cannot save a new profile or gametype; with 99 or more saved games in the folder (Arena
  Evolved's 86 among them: 13 profiles and gametypes of yours) it says "too many saved games" at start. Your files are not changed; this build lists them all again.
- **HUD STYLE** (Settings > HUD, its first row): AE (the default) or CLASSIC, the HUD exactly as before. With AE, a
  **score strip** under the view in multiplayer: both sides' scores (my team or me on the left) with the clock between
  them, how close each side is to winning, and each gametype's own state (the flags in CTF, the ball's holder, the
  hill, the race's place and lap, ...). The clock is in the strip, no longer in the corner; the TIMERS list is in
  the strip's style, its foot on the motion sensor's line.
- With HUD STYLE AE, a **header over the scoreboard** (Tab, and after the game): the mode, the map and the set
  ("TEAM CTF · BLOOD GULCH - NHE"), and the goal and time ("3 CAPTURES · 15:00"), in place of the gametype's row.
- With HUD STYLE AE, the **PRE-GAME COUNTDOWN** is a large upright number (white, or your team's colour) with
  the mode, map and goal under it; the performance line, the capture notice, the names over players, the voice
  speakers' list and co-op's texts take the strip's look.

## 0.2.5-beta - 2026-10-13

A testing release: the battle rifle. Built on ChupathingyCE 0.7.3e and OpenCE build-201 as 0.2.4-beta: **network
version 28**. The Linux 64-bit build is the one played (as the host, one machine); the Windows build, and the rifle
between two machines, were not test-played before release.

- **BATTLE RIFLE**, a new starting weapon: in a gametype's ITEM OPTIONS, with STARTING WEAPONS on CUSTOM, it is the
  last choice of PRIMARY and SECONDARY. What it is: three-round bursts, a 3x scope with black markings, one reticle zoomed and unzoomed,
  an ammunition counter of its own. Its rules are Arena Evolved's code; its model, textures, animations and sounds
  are CMT's (The Silent Cartographer: Evolved, used with credit), and its reticle follows a crosshair of Kenney's
  (CC0).
  - It comes as a **weapon pack**: the `packs` folder in the Windows and Linux 64-bit downloads. Keep that folder
    beside the game's `maps`. Without it the slot plays an assault rifle.
  - **Settings > Game Options > WEAPON PACKS** (ON / OFF) turns packs off on a machine, from the next start.
  - **Joining:** while a game's gametype has the rifle on, only Arena Evolved 0.2.5 or later with the pack can
    join; anyone else is told why. Games without it are joined as before, by 0.2.3, 0.2.4 and OpenCE alike.
  - With COMBAT FEEDBACK: ARENA EVOLVED, a shield pop means one more headshot kills, and that holds for the rifle:
    one burst to the head finishes a player whose pop has shown, whatever popped the shield.
  - On Custom Edition maps the rifle is not added (the assault rifle is played).
  - No seeded gametype gives it: set it in a gametype of your own (Edit Gametypes > ITEM OPTIONS).

## 0.2.4-beta - 2026-10-13

A testing release. Built on ChupathingyCE 0.7.3e and OpenCE build-201 as
0.2.3-beta: **network version 28**, so it plays online with 0.2.3-beta and
OpenCE build-201 and later. The Linux 64-bit build is the one played (the
host's view and split screen); the Windows build and a joined machine's view
were not test-played before release.

- **COMBAT FEEDBACK: ARENA EVOLVED** (a gametype option, ARENA OPTIONS; HALO CE
  is the default and is the game as it was). What a fight has come to is shown
  on the player's body, for every player and in every split-screen view:
  - **The shield pop.** A shield that one more headshot would finish bursts
    white over the whole body, then drains away.
  - **Shield down.** A broken shield flickers in thin bands until it recharges.
  - **The recharge.** The shield comes back as a band that climbs the body,
    as far up as the shield is full; your own arms show it in first person.
  - **The blood pop.** The shot that goes through the last of a shield and
    finishes the player throws blood where it struck, in the world, so a kill
    is seen from across the map.
  It is drawing only: no damage value changes, nothing is added to the HUD
  and no hit markers. Multiplayer only. Players on OpenCE or ChupathingyCE
  can still join; they see the game's own effects. Played so far on the
  host's machine and in split screen; a joined machine draws the same from
  what the host sends (not yet played between two machines), but for the
  small burst the map adds to the shield pop.
- **All of Arena Evolved's seeded gametypes play COMBAT FEEDBACK: ARENA
  EVOLVED** from this release (casual, free for all, AE COMP and AE TRAINING).
  A gametype you changed yourself is left as it is; Halo 1: NHE's set stays
  on HALO CE.
- **NHE OPTIONS**, a new screen in the gametype editor: NHE MODE and NHE
  EXTRAS moved there from ARENA OPTIONS, which is back to twelve roomier rows.
- **TEAM AE VANILLA and TEAM AE POWERUPS are now TEAM NHE VANILLA and TEAM NHE
  POWERUPS**, listed with Halo 1: NHE's set. Their rules do not change; a copy
  you edited keeps its name.
- **NO SPREAD: NHE is now NO SPREAD: FIRST SHOT.** The same rule under a name
  that says what it does (the pistol's first shot from rest and the sniper
  rifle's unzoomed shots go exactly where you aim); your gametypes keep it.
- **HEALTH: CLASSIC is now HEALTH: HALO CE** (gametypes and CAMPAIGN HEALTH),
  beside REACH, HALO 2 and HALO 3. The same rule; nothing changes in play.
  DROP SECONDARY's CE value reads HALO CE too.

## 0.2.3-beta - 2026-10-13

Built on ChupathingyCE 0.7.3e and OpenCE build-201: **network version 28**.
It plays online with OpenCE build-201 and later, not with 0.2.2-beta or
earlier (details in the merge entry below). The Linux 64-bit build is the one
played; this Windows build was not test-played before release.

- **Map sharing for every map that is not the game's own.** A host now sends
  community maps in the Xbox's format (those in its `maps` folder and those of
  its active mod) as well as Custom Edition maps. Join a game on a map you
  lack and it is downloaded from the host; in the lobby, **F6** gets every map
  the host has that you lack, one after
  another, and the same again stops it. The line at the top of the screen
  shows what the host has and how far the download is. Community maps land in
  a new `maps_shared` folder, Custom Edition maps in `maps_ce` as before.
- **You are asked first, once per host.** The question says how many maps
  you lack and how big they are; nothing is downloaded before you answer yes
  (F6, in the lobby, once the question has been on the screen for a second).
  The yes is for the amount shown: if the host later has more for you, you
  are asked again. The stock maps (the game's own multiplayer
  and campaign maps and its menus) are never sent or replaced, a download
  never replaces a file you have, and one host can give you 4 GB at most
  (`network.map_download_limit`).
- **Internet games share maps only when both sides allow it.** Settings >
  Network Setup > Map Sharing has two new rows, both OFF at first: INTERNET
  DOWNLOADS (you, joining) and INTERNET SHARING (you, hosting). Games on your
  local network share as DOWNLOAD MAPS and Server Setup's SHARE MAPS say. This
  goes for Custom Edition maps too, which until now were also shared over the
  internet by default.
- A mod's map that only loads with its mod active (it leans on the mod's own
  data) is downloaded but then refused when it loads: play such maps with the
  mod installed.
- Maps you downloaded this way show in your own map lists from the next start
  of the game. Players on other builds (OpenCE, ChupathingyCE) get Custom
  Edition maps from an Arena Evolved host as before, and nothing else.
- **SILENT MOVEMENT and SILENT WEAPONS** (gametype options, ARENA OPTIONS).
  SILENT MOVEMENT covers footsteps, jumps, landings and slides; SILENT
  WEAPONS covers readying and putting away a weapon (the weapon-switch
  sounds), in first and third person. Each is NORMAL (as Halo CE), JUST ME
  (you hear your own, nobody else's) or SILENT (nobody's, not even your
  own). Firing, reloading, melee and pickup sounds (the overshield's too)
  always play. The host's choice counts for everyone in the game; every
  gametype, including the seeded ones, starts with both NORMAL.
- **SOUND PRESET: SPLIT SCREEN**, on the same screen: one pick sets both
  options to SILENT, for players sharing speakers on one screen (with JUST ME
  the others' sounds would still come out of the same speakers). Change
  either row and it shows CUSTOM again.
- **Only Arena Evolved joins while those options are on.** A player on
  another build (stock OpenCE or ChupathingyCE, or Arena Evolved before
  0.2.3) would still hear every footstep, so a host with SILENT MOVEMENT or
  SILENT WEAPONS on turns them away, and removes one that was already in the
  lobby when the gametype changed. Their own game can only say it could not
  join (a host has no way to show other builds a message of its own). With
  both options NORMAL, everyone joins as before.
- **Fixed:** a fully charged weapon trigger with no magazine (none of the
  stock weapons has one; a custom map's weapon can) read the memory just
  before the weapon's magazines to decide whether to fire at once. It now
  reads no magazine, as an uncharged trigger already did.
- **MENUS on the main menu: switch between the PC and Xbox menus without
  leaving the game.** It sits above QUIT in the PC menus (the other rows move
  up a little to make room) and under SETTINGS in the Xbox menus. Pick it and
  the main menu's rows turn into PC and XBOX, drawn like the rows around them,
  with the style you are using focused and marked with a dot. Pick the other
  one and the menus switch at once with a short fade (a cut with REDUCE
  MOTION), the main menu music keeps playing, and you land on MENUS again, so
  switching back is two presses. B, Esc, BACK or the BACK row returns to the
  main menu. The choice is saved (`display.menus` in `config.toml`). MENUS is
  only on the main menu, never in a lobby or a game. If you use your own
  `menus/main_menu.xml`, it has no MENUS unless you add the row
  (`port/assets/menus/README.md`).
- **Linux: PlayStation controllers without Steam** (from DamnationCE 0.3.41).
  A DualSense or DualShock 4 now goes through the kernel's own driver
  (Linux 5.12 and later), so it should work without Steam's udev rules
  (before, it was not found at all without them; not yet confirmed on real
  hardware). `debug.txt` names each controller's
  kind and device when it connects.
- **PlayStation button icons** (from DamnationCE 0.3.41). While a
  PlayStation controller (DualShock 4, DualSense) is the one you last used,
  the game's button pictures show its buttons: cross, circle, square and
  triangle for A, B, X and Y, in the Xbox-style menus' footers (SELECT, BACK),
  the icons inside menu text and the in-game prompts ("Hold X to pick up").
  Touching the keyboard, mouse or an Xbox pad brings the Xbox letters back.
- **Server browser pings** (from DamnationCE 0.3.41), an opt-in: Settings >
  Network > SERVER PINGS, **off by default**. Timing a ping makes the same
  connection a join makes, so every host in the list sees your address; off,
  no host is contacted until you join one. On, the PC menus' Server Browser
  shows each internet game's ping in milliseconds, coloured green (under 80),
  yellow (under 160) or red. It is timed the way a join reaches the host (the
  tunnel made with the game's invite, the best of three pings, then closed),
  a few hosts at a time, again every two minutes and on REFRESH (not a host
  timed in the last 30 seconds); a game with a password shows none, a full or
  starting game still shows CLOSED. Joining a host whose ping is being timed
  uses that connection. Direct Link shows the open tunnel's ping.
- **Map sharing, safer** (from DamnationCE 0.3.41). A LAN game's other players
  are only downloaded from at local network addresses, so a host can't send
  your game to connect anywhere on the internet (on a VPN "LAN" with
  public-range addresses, such as Hamachi or Radmin VPN, the map comes from
  the host alone). A downloaded map that fails
  its check (another version, a broken file) is deleted instead of staying in
  `maps_ce` under the map's name. A player still downloading the map when a
  game starts doesn't spawn until the map is in (OpenCE's), and comes back to
  the lobby with everyone else if the game ends first. A missing `custom_maps`
  folder is made at startup (OpenCE's).
- **Custom Edition maps get the pause menu's SETTINGS and the high-res HUD.**
  Their pause menu has SETTINGS (and END GAME for the host) as the Xbox maps'
  does, in place of Halo PC's CHANGE SETTINGS, which did nothing here. In split
  screen their pause box now fits each player's view. The high-res HUD draws on
  them where the map uses the stock HUD art (ammo, grenades, reticles, scopes,
  motion sensor); the few pieces Halo PC laid out differently (the shield and
  health meters, the ammo meters) keep the map's own.
- **Custom Edition maps: GAME OPTIONS in the pause menu works.** RESTART GAME
  restarts the same map and gametype, CHOOSE TEAM changes the pausing
  player's team, and in split screen the screens it opens are fitted and
  centred in the pausing player's own view. The NEW GAME row, which dropped
  the host out of the game, is gone.
- **Fixed: a controller layout could be applied to the wrong controller in
  the PC menus.** When player 1's profile was loaded before their controller
  was assigned, a layout with the d-pad bound (Universal Reclaimer) kept the
  stock d-pad movement, and a stock layout could lose it. The layout now
  follows the player whichever comes first.
- **Fixed: leaving a LAN lobby with two players on one gamepad and the
  keyboard could lock the game.** When the keyboard's player 1 left first, the
  gamepad went back to player 1's place, and "Please reconnect #2 controller."
  stayed over the lobby for good: nothing could leave it. The gamepad now stays
  player 2's while player 2 is in the game, so their B leaves too.
- **Linux: a hint when Join Game > LAN finds nothing.** After 10 seconds with no
  game found, if a firewall is on (ufw, or firewalld), the list says that a
  firewall can hide LAN games and which ports README's "Playing over LAN" opens.
- **WAYPOINT SIZE** (Settings > HUD): the item waypoints' size, 50% to 100% in
  steps of 5, default 70%. In split screen it shrinks with the view; far items
  shrink gently (to 75% at 60 m). The distance is white in a dark outline and
  the timer outlined, neither smaller than 9 screen pixels at any size.
  Waypoint names are short (SHOTGUN, in the base's colour; a number only where
  the map has two of the item within 10 m; the power list keeps full names),
  and a waypoint steps aside, or fades, where a player's name is over a head.
- **13 free for all gametypes.** FFA AE ELIMINATION, PHANTOMS, ENDURANCE,
  ROCKETS, SNIPERS, JUGGERNAUT, REVERSE TAG, ACCUMULATION, STALKER, KING, CRAZY
  KING, RACE and RALLY: Halo's stock free for all modes on Arena Evolved's casual
  rules. They are added to your gametype list on the next start; gametypes you
  made or changed are left alone. Arena Evolved's gametypes are listed free
  for all first, then team. The gametype lists (PC and Xbox menus) now
have room for 128 entries instead of 100. The game's own limit is unchanged:
it keeps 100 saved files in all (gametypes, profiles) together with the 26
built-in gametypes in one list, so with the 61 seeded ones about 10 gametypes
of your own fit before the built-in ones at the end of the list are cut.
- **Merged ChupathingyCE 0.7.3e, OpenCE build-190 and OpenCE build-201:
  network version 28.** Plays online with OpenCE builds on network 28 (build-201 onward), both
  ways; ChupathingyCE 0.7.3e players join Arena Evolved's games (Arena Evolved
  does not join theirs). Hosts of an older version (OpenCE build-200 and
  earlier, DamnationCE 0.3.41 and 0.3.40, Arena Evolved 0.2.2-beta) are
  refused, and are told to update; their players are told the host is newer.
  - **From build-201:** a player still downloading a Custom Edition map is held
    out of the game (not spawned, not counted) until the download is done,
    then joins with the same checks as any late joiner; a machine that goes
    silent while downloading times out. Map sharing is hardened (OpenCE's
    hardening, taken ahead of its release): LAN players are reached only at
    addresses of the host's kind, the host takes map connections only from
    its game's machines, Windows device names are refused as map names, a
    download's part file grows piece by piece (an empty one is deleted), a
    player that only says it is downloading is not held for long, and a
    Custom Edition map whose models or structure take more than 128 MB of
    vertices is refused. A multiplayer map's first scripted BSP switch is
    kept (the host's game type picks the sky, no level reload). The flashlight and dynamic
    lights stay on in big matches (the Xbox turned them off past 4 / 13
    players or a few vehicles). A second weapon picked up while firing is
    readied. Bodies the host moves are no longer drawn floating or in walls.
    An overshield's drain no longer flashes as a hit. A `config.toml` with a
    section twice (which made the whole file unread) is joined and read again.
    Co-op: a large fight respawns you beside the group after 10 s. Higher
    limits for see-through surfaces, faster draws; Custom Edition text fits
    its letters.
  - **Custom Edition map sharing.** Join a game on a Custom Edition map you
    don't have and it downloads from the game's players (the host and the
    others, torrent-style), with the resource maps it needs, into `maps_ce`.
    The host shares by default (Server Setup > MAP SHARING: SHARE MAPS on,
    UPLOAD LIMIT 1 MB/s); every player helps by default (Settings > Network
    Setup > MAP SHARING: DOWNLOAD MAPS on, HELP SEND MAPS on, UPLOAD LIMIT
    256 KB/s). Shared maps in `maps_ce` and `custom_maps` are renamed for
    their hash (`hugeass.map` becomes `hugeass_0e2b3ee4.map`); the menus show
    the name without it. TCP port 5153 must be open on a LAN (README).
  - **Shields.** Settings > Video > GRAPHICS: SHIELD GLOW (shields light their
    surroundings as they flare) and GLOW INTENSITY; FOV AND VIEWMODELS: ARM
    SHIELDS (the flare on your first-person arms), LEGS (your legs in first
    person) and SHIELD COLOR (your flare in an armor color, only you see it).
    All off / default, as upstream has them.
  - The lobby countdown no longer starts over when a player joins or leaves.
  - Campaign pause: the rows 30 px apart, the box grown for SETTINGS.
  - Internet play with OpenCE's browser players (WebRTC); upstream's web
    build is in the tree, not an Arena Evolved download.
  - Fewer draws and GL calls; fixes (co-op stale players, vehicle homes,
    custom HUD digits, co-op elevators).

## 0.2.2-beta - 2026-10-11

Built on ChupathingyCE 0.7.1e and OpenCE build-177 (network version 25), as
0.2.1-beta: the two play online together. Nothing new goes over the network.

- **Linux download for Ubuntu and friends.** The Linux download now runs on
  Ubuntu-based distros (Ubuntu 22.04 and later, Zorin, Mint, Pop!_OS): it is
  built on an older base and includes SDL3. The build made on Arch Linux stays,
  as `-linux64-arch`, for Arch and other rolling distributions.
- **Mouse: MCC style.** Settings > Mouse Setup > MOUSE STYLE: CLASSIC (the
  default, as before) or MCC, which uses Halo: The Master Chief Collection's
  sensitivity number (0.1 to 10; the view turns number / 45 degrees per mouse
  count; 1.0 is about 1% above CS:GO at 1), with a ZOOMED and a VEHICLE
  SENSITIVITY SCALE. The first switch to MCC keeps your feel.
- **Hold to zoom.** Settings > Game Options > ZOOM: TOGGLE (the default, as
  before), HOLD (zoomed while held) or BOTH (a tap steps the level, a hold
  zooms until you let go). While zoom is held the mouse wheel steps the level
  (it does not switch weapons) and the controller's flashlight button steps it
  (it does not toggle the light); in HOLD a tap then a hold starts at the
  second level. ZOOM RESET: FIRST LEVEL (default) or LAST USED on that weapon.
- **Custom Edition maps with MCC's files.** Custom Edition maps need Halo PC's
  `bitmaps.map`, `sounds.map` and `loc.map`. AE finds your Steam install of
  Halo: The Master Chief Collection (Linux and Windows) and can read them from
  `halo1/maps/custom_edition`, where they are: it never copies or changes your
  MCC files. It asks the first time a map needs them (USE MCC FILES / NOT NOW /
  NEVER ASK AGAIN). Settings > MAP FILES: ASK / ON / OFF, the folder in use and
  BROWSE. Files in your own `maps_ce` folder always win. The folder stays on
  your PC (config and `debug.txt` only). On Windows, a Steam folder whose path
  has characters outside the system's code page is not found.
- **HUD settings and waypoint pictures.** Settings has a new HUD entry: HUD
  AREA, COMPACT HUD, SCOREBOARD FADE, the timers and SHORT MESSAGES moved there
  (your values are kept), plus WAYPOINTS: TEXT (as before), ICONS or BOTH (the
  default). The label is a centred stack, in Halo Infinite's layout with Arena
  Evolved's colours: the distance in metres, the name, the picture, and while
  the item respawns its timer, all in the item's own colour (red or blue for a
  base's, else the HUD's), the distance and picture dimmer while it respawns.
  A weapon's picture is its own pickup picture, read from your map (nothing
  shipped; a map with none gets our burst in a badge); the overshield and the
  active camouflage have Arena Evolved's own glyphs in a filled hexagon. ICONS
  leaves out the name. Each Settings entry now shows its own picture, or none
  (no more "PLACE HOLDER" box).

## 0.2.1-beta - 2026-10-10

Built on ChupathingyCE 0.7.1e and OpenCE build-177 (network version 25). It
does not play online with 0.2.0-beta (network version 24): everyone updates.
Windows and Linux are offered as 64-bit builds only.

- Merged upstream: OpenCE build-167, ChupathingyCE 0.7.1e, OpenCE
  build-170 (network version 25), OpenCE build-174 and OpenCE build-177.
  - OpenCE build-167 (network version 24; the build-170 merge raises it to
    25). From upstream: voice chat (hold V to talk; Settings > Audio has
    VOICE CHAT, VOICE VOLUME and the output and input devices; the
    microphone stays closed until you first talk), votes to kick a player
    (right-click a player on the scoreboard; the host also gets Kick and Ban
    there), co-op's Server Setup options in screens of their own, Steam's
    on-screen keyboard for the menus' text fields, audio mixer and
    looping-sound fixes, glass and overshield drawing fixes and optional
    enhanced animations (`game.enhanced_animations`). The Screenshot key can
    now be rebound (Controls Setup > Actions) and its default is F10;
    recording moves to F9. Screenshots are still Arena Evolved's (PNG in
    `screenshots/`, every split-screen view).
  - ChupathingyCE 0.7.1e: Custom Edition maps made with MCC's tools whose
    bitmaps use its high-quality compression (BC7) now load and draw instead
    of being refused.
  - OpenCE build-170: **network version 25**. Plays with OpenCE build-169
    and later; no longer with ChupathingyCE 0.7.1e or Arena Evolved
    0.2.0-beta (version 24: a host of 24 is refused with "the host needs to
    update", and their players are told our host is newer). From upstream: a
    joining player sees others' flashlights as the host has them, Android
    touch controls with aim assist, a fix for starting on Windows 7, Custom
    Edition map fixes (pause menu centred, fewer than three vehicles, HUD
    sounds, linear bitmaps), memory leak fixes, and the decompiled code's
    latent-bug pass.
  - The game lists keep a host this build does not join (network version
    24, or one newer than this build) listed, dimmed, after the others:
    Online Games, and the PC menus' Server Browser, LAN and Direct Link
    lists. Its line says why (HOST NEEDS TO UPDATE (VERSION 24), or UPDATE
    THIS GAME TO JOIN for a newer host), and A or Enter on it shows the
    reason instead of trying to join. Online Games no longer hides hosts
    newer than this build.
  - The PC menus' merged setting rows and widened values now apply to touch
    only (upstream's #50): with a mouse, a click on a setting's label no
    longer changes its value (the mouse clicks the labels and values where
    CE's menus have them); a tap of a finger on the label still reaches the
    value.
  - OpenCE build-174 (network version still 25). From upstream: field of
    view and viewmodel settings (Settings > Video Setup > FOV AND
    VIEWMODELS: `display.fov`, `display.viewmodel_fov`,
    `display.viewmodel_visible`; the defaults keep the stock view), Video
    Setup's graphics rows on a GRAPHICS screen of their own (PERFORMANCE
    and POSITION are there too), high-res controller button icons in
    the menus, any controller drives the menus while one person plays, a
    saved profile, controls and all, is copied at once into every local
    player using it (split-screen players sharing a profile get each
    other's changes immediately), up to 32768 structure triangles a frame, co-op respawns beside the largest
    group of teammates, a profiling build (`configure.py --profile`). The
    campaign's pause SETTINGS is now upstream's code, with Arena Evolved's
    split-screen and co-op lists on top: SETTINGS sits before REVERT TO
    SAVED, its settings show only Controls, Gamepads, Mouse, Audio and Video
    Setup, and Gamepads' OK saves the profile at once.
  - OpenCE build-177 (network version still 25). From upstream: the
    high-res HUD draws the weapons' reticles from the PC HUD sheet's
    drawings that match the Xbox's exactly, and now also the rocket
    launcher's and the sniper rifle's (its 2x and 10x included); Android:
    the game starts on more devices whose Java runtime covers the memory
    it needs, and `memory_map.txt` says why when it cannot.
  - Zooming in no longer leaves colour noise along the top and left edges
    of the screen (and of a split-screen view). The scope's screen effect
    never drew the first row and column of pixels above 640x480; an old bug,
    in 0.2.0-beta and upstream too.
  - The campaign pause menu's SETTINGS screens no longer show the HUD (the
    ammo and shields) beside their titles: while they are up, that player's
    view draws no HUD (split screen: only their own view). The pause menu
    itself and a multiplayer game's pause keep the HUD.
- New logo, ORBIT (a band ring seen at an angle, amber and burnt orange),
  Arena Evolved's own drawing: it replaces the helmet as `halo.exe`'s icon,
  the window's icon, the macOS and Android app icons (Android's an adaptive
  icon with a themed, one-colour layer), and heads the README. On Windows
  the window now shows `halo.exe`'s own icon, with its small sizes drawn
  for them. The Linux download carries the icon as `arena-evolved.png`.
  Sources and exports: `port/assets/branding`, `tools/ae_logo.py`. The
  logo is not CC0 like the code: see `port/assets/branding/LICENSE.md`.

## 0.2.0-beta - 2026-10-09

Built on ChupathingyCE 0.7.1d and OpenCE build-157 (network version 24). It
does not play online with 0.1.0-beta (network version 20): everyone updates.
Windows and Linux are offered as 64-bit builds only.

### Added
- AE MENUS (`display.arena_menus`, off by default; `HALO_ARENA_MENUS`): a Reach-style front end of our own, kept apart
  from the PC menus in `port/linux/game/ae_*.c` and `port/linux/src/ae_*.c`. With the
  flag off nothing changes. This first part holds the widgets (panels,
  rows, dialogs, pager, keyboard), their layout for one to four local
  players at 720p to 4K, button prompts that drop words before they run past
  the panel, the profile screens, a gallery of test screens, and the lobby
  (roster, MAP and GAME TYPE picks that carry into the match). Not yet a
  full replacement for the PC menus; try it with the flag on.
- Gametype tiers: the Arena Evolved casual set (with 2V2, PRACTICE,
  VANILLA and POWERUPS), special modes (SNIPERS, SHOTTY SNIPERS, SWAT,
  ROCKETS, SHOTGUNS, HEAVIES), AE COMP, FFA AE TRAINING and Halo 1: NHE's
  own 23 gametypes, seeded the first time you play. The lists, lobby and
  scoreboard show display names (TEAM AE SLAYER over the stored AE TEAM
  SLY), and your own gametypes may have a longer one. Seeded gametypes from
  earlier builds are updated in place (seed revision 7); ones you edited are
  kept as you made them, and each change is backed up first
  (`saved/playlists_backup`).
- Gametype options: item TIMERS levels (HUD, HUD + WAYPOINTS, LINE OF
  SIGHT; an item's waypoint goes off once it is picked up), the objective
  only in line of sight, DROP SECONDARY (CE / ALWAYS / ALWAYS EXCEPT POWER),
  NHE MODE (the mode a Halo 1: NHE map plays), NHE EXTRAS (everyone joins
  red, no team swap, the dead camera on you, the clock counting up), SPAWN
  HEAT allowed per gametype, and BALL MELEE (oddball: LETHAL, the carrier's
  melee kills in one hit). They travel in the gametype's flag bits and spare
  bytes: no network change; stock joiners play the host's rules (the indicators, such as
  timer waypoints, line-of-sight marks and the NHE EXTRAS camera and clock, need Arena Evolved on the
  joining machine).
- HALO 2 health uses Halo 2's timers: shields recharge 5 s after the last
  shield damage and fill in 2 s (never under fire); health refills in 5 s,
  starting 10 s after the last damage that reached it.
- Game start text (the gametype's name over the mode line), short spree
  messages, and the map's name on the post-game report.
- MATCH CLOCK LABELS (`display.match_clock_labels`, `HALO_MATCH_CLOCK_LABELS`;
  off by default, config only for now): a small heading over each line of
  the corner clock, TIME LEFT or ELAPSED (with BOTH, each line its own),
  under the power column.
- ENEMY NAME COLOR (Settings > Network Setup, under PLAYER NAME SIZE;
  `display.enemy_name_color`, `HALO_ENEMY_NAME_COLOR`): `"red"` shows
  enemies' names, the one under your reticle and those over heads, in red,
  as the Master Chief Collection's; `"classic"` (the default) shows them in
  the HUD's blue (the over-head enemy names were red before).
- The shotgun is a power item (a one-shot kill up close in CE): TIMERS'
  power column has its row (SHOTGUN, R/B SHOTGUN at both bases), TRAINING
  puts a labelled waypoint over its spawns, and the callouts say "shotgun in
  ten" and "shotgun is up" (Cori has the clips; a pack without them says
  "power weapons in ten" / "power weapons are up"). SHOTGUN AS POWER
  (`display.shotgun_power`, `HALO_SHOTGUN_POWER`, on by default, config
  only) set false makes it a weapon as any other again, from the next game.
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
- Taken from open OpenCE pull requests #209 (a client's grenade shock waves
  are no longer refused: about half its grenade damage), #211 (a client no
  longer throws grenades it does not have right after spawning) and #210 (a
  player who used a teleporter no longer appears stuck on the pad they
  left); the next upstream merge already has them. AE tightened #209: the
  host takes a grenade's shock-wave report only after a real explosion of
  that same grenade type (an explosion paid as a rocket's or another
  weapon's licenses none).
- Merged ChupathingyCE 0.7.1d and OpenCE build-157.
  - Network version 24 (announced; hosts of 24 are joined): Arena
    Evolved plays with ChupathingyCE 0.7.1 and OpenCE build-149 and later
    both ways. Builds of an older version, Arena Evolved 0.1.0-beta among
    them, no longer join its games (their players are told the host is
    newer). It does not join their games either: a host below network
    version 24 is refused with "the host needs to update the game". Its wire ID is `ae-24a`.
  - Halo PC maps have folders of their own beside `maps`: `maps_ce` (Custom
    Edition), `maps_md` (HaloMD) and `maps_pc` (your Halo PC disc's, new).
    `maps/ce`, `md_maps` and OpenCE's `custom_maps` still play; the game
    moves nothing by default (`game.move_old_map_folders` is `"no"`; `"ask"`
    offers once to move the first two). A game
    on a Custom Edition map is named as OpenCE names it, so OpenCE players
    with the map join it.
  - Security (ChupathingyCE 0.7.1d): a downloaded map's scripts may call and
    set less, map strings are bounded and never a format string, a host's
    map name may not be a Windows device, a map's widgets may not run the
    menus' profile and settings functions, the Ogg Vorbis decoder hardened.
  - Delta Stats (the games you host sent to halo.milenko.org: kills with
    positions, accuracy, medals), on by default in ChupathingyCE, is OFF by
    default here: Settings > Network, SHARE HOSTED STATS
    (`network.report_events`). Crash reports, which ChupathingyCE's releases
    send, are off in every Arena Evolved build.
  - One player alone starts a system link, internet or split screen game as
    upstream has it; Arena Evolved's own rule is kept for the host's
    machine alone with several local players.
  - From OpenCE build-146 to build-157: the PC vehicle set (every vehicle
    the map places), oddball on maps with no ball spawn, every placed
    vehicle coming back, Server Setup starting at the most players, co-op's
    bringto bringing every player, stereo world sounds panned and
    reverberated, a 16 MB sound cache, sound tags from files
    (`audio.loose_sounds`, for those who make sounds), the weapon HUD's
    secondary magazine, exact health thresholds, hosting on machines up for
    more than 24.8 days, finer mouse sensitivity steps, recent errors on
    the halt screen.
  - OpenCE's own Custom Edition loader is no longer in the tree (as in
    ChupathingyCE 0.7.1): Custom Edition, HaloMD and Halo PC maps are
    ChupathingyCE's loader's, as before.
- TRAINING's waypoints: an item a map has twice on one side (or twice in
  the middle) is numbered, RED ROCKETS 1 and RED ROCKETS 2 on Boarding
  Action, in its label and in the power column when it spawns alone; and
  while a label shows the time to the spawn, the game's distance digits by
  its arrow are not drawn.
- Halo 1: NHE's maps: their own on-screen clock (their scripts' titles) is
  never drawn now. Arena Evolved's MATCH CLOCK takes its place, laid out as
  on any map with the power column over it, and MATCH CLOCK OFF shows no
  clock (it used to leave NHE's, which sat where the power column goes in
  split screen). Their Cortana talking timer, its beeps and their 3-2-1
  countdown stay.
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
- Merged OpenCE build-145 and the commit after it: the desktop builds'
  memory window is 512 MB with a 256 MB texture cache (was 128 MB), for the
  biggest community maps' textures; map loading hardened against hostile
  maps (resampling overflow, script constants' types, a map's script
  source never compiled again: a map whose scripts do not load plays
  without them, every map's model index buffers and lightmaps checked);
  a client joining a game on a map it lacks is told which and where to copy
  it (the error text wraps to its dialog). OpenCE's own Halo Custom Edition
  maps' loader and lists (custom_maps\<name>, CUSTOM SINGLEPLAYER and
  CUSTOM MULTIPLAYER) are not built into Arena Evolved: its Custom Edition
  and HaloMD maps stay ChupathingyCE's, as <file>@ce and <file>@md from
  maps/ce and md_maps, with the multiplayer maps as [CE] and [MD]; the Map
  screen's chooser offers SINGLEPLAYER and MULTIPLAYER. `map_validate` also
  checks Custom Edition maps the way OpenCE's loader reads them (a tool
  only: the game does not use that loader). Network: OpenCE build-145's
  version 22 changes no message, only the name its hosts give their Custom
  Edition maps; Arena Evolved still announces 20 and now joins hosts of 11
  to 22, OpenCE build-145's on the game's own maps (on one of its Custom
  Edition maps the player is told what it is and the game is left).
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
- Point-blank shots register with NO SPREAD on (NHE or FULL): a shot from
  touching range (a sniper rifle's unzoomed headshot, say) started inside or
  past the target and often missed it; it now starts at the player's eye
  when a unit is in the way. NO SPREAD OFF keeps CE's behaviour.
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
