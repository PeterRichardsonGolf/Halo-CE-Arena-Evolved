# What Arena Evolved adds

Every option, in detail. The [README](../README.md) has the short version; the gametypes are in
[gametypes.md](gametypes.md), LAN play in [lan.md](lan.md).

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
| HEALTH | HALO CE (stock: health packs only), REACH (REACH timers: shields recharge 6 s after the last hit and fill in 3 s; health refills 4 s after the last damage that reached it, shield hits do not count, fast near empty and slower near full, up to the top of the third it is in, about 1.1 s, 1.5 s and 2.6 s a third; a health pack fills the rest and ends the shields' wait), HALO 2 (HALO 2 timers: shields recharge 5 s after the last shield damage and fill in 2 s; health refills in 5 s, starting 10 s after the last damage that reached it, shield hits do not count), HALO 3 (the same as HALO 2: HALO 3 uses HALO 2's numbers and rule). Health packs still spawn. |
| COMBAT FEEDBACK | HALO CE (stock: the game's own shield flare) or ARENA EVOLVED, which shows on a player's body what the fight has come to: a shield about to fail bursts white, a broken shield flickers until it recharges, a recharge sweeps from feet to head, and the shot that finishes a player after breaking the shield throws a blood pop where it struck. Drawing only: no damage value changes, nothing new on the HUD, and stock players in the game are unaffected. Multiplayer only. |
| NO SPREAD | OFF (stock), FIRST SHOT (the pistol's first shot from rest and the sniper rifle's unzoomed shots go exactly where you aim; held pistol fire still spreads) or FULL (every pistol and sniper rifle shot goes where you aim, held fire too: no spread, no bloom). Other weapons are unchanged. |
| PRACTICE MODE | Every weapon and powerup respawns every 30 seconds. |
| PRE-GAME COUNTDOWN | A 3-2-1 on a black screen before the game starts; you can look around but not move or shoot. A weapon switch (Y) pressed during it is kept and taken as it ends, as in Halo 1: NHE; any other button pressed during it does nothing until you let go of it, so nothing fires, zooms, reloads or picks up the moment it ends. The host holds every player to it, including players on builds without it. |
| SILENT MOVEMENT | NORMAL (stock), JUST ME (you hear your own footsteps, jumps, landings and slides; everyone else's make no sound for you) or SILENT (nobody's, yours included). |
| SILENT WEAPONS | The same three for readying and putting away a weapon (the weapon-switch sounds), first and third person. Firing, reloading, melee and pickup sounds (the overshield's too) always play. |
| SOUND PRESET | SPLIT SCREEN sets SILENT MOVEMENT and SILENT WEAPONS both to SILENT, for players sharing one set of speakers (JUST ME can't tell whose ears a sound is for there); CUSTOM leaves the two rows as you set them. |
| BALL MELEE | Oddball gametypes only. STOCK (the ball's melee takes the shield and a little health, and kills from behind) or LETHAL (the ball carrier's melee kills in one hit, shields and overshield included: same reach, no hits through walls, friendly fire as the gametype sets it). |

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

**Split screen**
- **SPLIT SCREEN** in the PC menus' Multiplayer: a game on this PC for up to four players and up to 15 bots. First a
  JOIN screen: the first device to press START (ENTER or SPACE on the keyboard) is PLAYER 1, the next PLAYER 2, so the
  keyboard and mouse can be any player. Each player picks their own profile there with their own LEFT and RIGHT (NEW
  PROFILE makes one), B or ESC leaves the seat, and CONTINUE goes on to the map, the gametype and GAME SETUP (the
  gametype and its options; no network rows), with the bots' rows in the lobby. A controller unplugged keeps its
  player's seat until it is plugged back in; a device that presses START in the lobby joins there.
- **Controller picker:** each JOIN card has a CONTROLLER above its PROFILE (a player's own UP and DOWN choose the
  row, LEFT and RIGHT change it): it lists every device connected, and taking one another player has swaps the two.
  ASSIGN (or Y on your own pad) waits for a press on the controller you want: press any button on it (a key gives
  the keyboard; ESC, the seat's own B or 10 seconds end it). A seat's device pressed flashes its name, so two pads
  of the same kind can be told apart. An empty card's OR PICK ONE seats a free device with the mouse. In the lobby,
  each of this PC's players (two or more) has the same CONTROLLER and ASSIGN on their row, in a split screen game
  and when you host a LAN or internet game (there the keyboard stays player 1's). The mouse can use every arrow,
  value and button.
- **PLAYFIGHT** (Settings > Game Options > BOTS): bots that move and aim but never shoot, for practice.
- Split screen in System Link and online lobbies (ADD PLAYER), and a lobby
  you can start on your own.
- **Bots** (0.3.0, Settings > Game Options > BOTS): up to 15 in a split
  screen game, and (0.3.7) in any LAN or internet game you host, up to 127
  ("BOT " before their names, the teams filled evenly); RECRUIT (for
  new and young players), EASY, NORMAL, HEROIC, LEGENDARY or DEMON (0.3.5: the
  LEGENDARY of before, above a new LEGENDARY halfway from HEROIC). Each level
  fights like a person of that level: how they
  strafe and jump, when to strike, when to throw a grenade, headshots once
  your shields are down, backing off when beaten, teammates told where you
  are at HEROIC and up. SHOOTING SKILL and MOVEMENT SKILL set the aim and
  the movement to levels of their own. They play every mode: Slayer, CTF (taking, carrying, returning and defending the flag),
  Oddball, King of the Hill, Juggernaut and Race. They take a Warthog's gun
  or a gun turret near them and ride with a teammate who drives (you too).
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
  feels it. The weapon packs have their own: the SMG a fast buzz, the
  silenced SMG a softer, lower one, the carbine a crisp click lighter than
  the pistol's, the battle rifle a firm click and a pulse a round. L2 is the
  grenades (a hair trigger: a tiny tick early in the pull, a rumble as one
  is thrown, a dry click with none), light while zoomed. The lightbar takes your colour (your team's in
  a team game), pulses while an overshield charges, dims with active
  camouflage and flashes when you become the juggernaut; or, with LIGHTBAR
  on SHIELD, it shows your shield as the HUD's meter does. The player lights
  show your split-screen number. Settings > Gamepads: ADAPTIVE TRIGGERS,
  TRIGGER STRENGTH (LIGHT, NORMAL, STRONG) and LIGHTBAR (OFF, PLAYER COLOR,
  SHIELD), on by default; other controllers are never touched. On Linux
  without Steam, see [DualSense on Linux without
  Steam](#dualsense-on-linux-without-steam).
- **Back buttons and paddles:** a Nintendo Switch 2 Pro Controller's GR and GL (an Xbox Elite's or a DualSense Edge's
  upper paddles) press the Xbox button that `input.back_right` and `input.back_left` name in `config.toml` (`"a"`,
  `"b"`, `"left_stick"`, ...), so they follow your controller layout; `"none"` by default. With Steam running, Steam
  Input takes the controller and the game sees no back buttons.
- **Nintendo controllers:** the Switch Pro Controller and the Switch 2 Pro Controller play (on Linux, tested on
  CachyOS; not tried on Windows).
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
  CLOCK), as Halo 1: NHE's voice timer: the power items' calls (where the
  gametype's TIMERS show the items, or with TRAINING) 10 seconds before they spawn ("rockets in ten"), five to one before
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

**Maps**
- **Map folders:** folders in your maps folder (and in `maps_ce`) are pages of the PC map lists (STOCK, your folders
  A-Z, MULTIPLAYER), so a map pack keeps its own folder. A map is still known by its name, so players may keep it in
  different folders.
- **Map layout files** (`maps/layouts/<map>.txt`) move, add and remove a map's items, vehicles, spawns and flags, so a
  map pack can fix a map without changing it: a map that lacks CTF flags gets them. A game on a map without a mode's
  flags (CTF, King, Oddball, Race) plays Slayer and says so, instead of crashing.
- **Halo Custom Edition's Gearbox maps rebuilt with Invader** (Danger Canyon, Death Island, Gephyrophobia, Ice Fields,
  Infinity, Timberland; your own files) load, and a high-resolution HUD's numbers draw at the right size.
- A mod's main menu too big for the game's menu cache (the Halo 2 Mappack Collection's) is passed over with a line in
  the log, and the game's own menus load.

**Modded discs**
- A modded disc is a whole set of maps made for the Xbox (Halo 1: NHE, CE+ X,
  Patch Edition). Settings > Game Options > MODDED DISCS picks a folder in
  `discs/`; its `maps` are played over the game's own, and the game restarts
  into it (`game.disc` in config.toml). Discs that earlier releases kept in
  `mods/` are still found and played from there; nothing is moved. `mods/` is
  now OpenCE's client-side mods' folder.
- Halo 1: NHE's maps play as a modded disc (`discs/NHE`), with its timer,
  training and modes working. NHE's files are not included: get Halo 1: NHE 1.0
  and its gametypes pack from the NHE team's site,
  [halo1nhe.com](https://halo1nhe.com/#download), and put its maps in
  `discs/NHE/maps`.
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


## DualSense on Linux without Steam

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

