# Changelog

What has changed in Halo CE: Arena Evolved, newest first. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/). The first
release is 0.1.0-beta, a pre-release; the sections before it are dated.

## 0.7.0-beta - 2026-10-31

### Changed
- Seed revision 27. SPAWNS MODERN in every AE gametype; the NHE set and Halo CE's own gametypes stay CLASSIC. The AE BTB
  and AE COMP gametypes (AE COMP 2V2 included) start with the EVO RIFLE and an assault rifle; their BR versions keep the
  battle rifle and every other AE gametype keeps its start. Your own gametypes and any AE gametype you edited are not
  changed; the unedited ones update in place, backed up first.
- MCC: game.mcc_use defaults to yes (MCC's maps listed when AE finds MCC).
- CREATE GAME: a new lobby starts OFFLINE (this PC only); NETWORK picks LAN or HOST ONLINE, and the lobby keeps the
  last choice until the game closes.

### Added
- Two new rifles as weapon packs (slots 6 and 7), each with its own profile, reticle and scope:
  - EVO RIFLE: a single-shot rifle (one round a pull, 15-round magazine, 60 at spawn, 75 in reserve; four shots pop
    a shield, the fifth to the head kills), a 2x scope that draws its zoom and the rounds loaded live, a holo sight.
  - DMR: a steady marksman rifle (15 rounds, a shot every third of a second, 3x scope, no bloom).
  - One reload fills the magazine. The EVO RIFLE's and the DMR's shield pop on the shot that breaks it.
  - Every weapon pack can be a start weapon (all seven at once included); CHANGE GAMETYPE's START switch picks
    PISTOL, BR or EVO.
- The battle rifle's numbers: four bursts kill (1.53 s), a steady view with a rumble on each round, its scope with
  no zoom label. Each AE rifle's reticle grows a little with each shot and comes back (the battle rifle's once a
  burst).
- SPAWNS (GAME OPTIONS > RESPAWN): CLASSIC or MODERN. MODERN scores the map's spawn points against the living
  players and recent deaths, skips points an enemy can see and keeps you away from the fight; the side a team
  spawns on follows its flag.
- GAME OPTIONS regrouped into topics, with filter chips (ALL, CHANGED, CLASSIC, COMMUNITY, AE, NHE: LB / RB) and a
  badge on each row saying where the rule comes from; LT / RT move between groups.
- Pause menu: RESTART GAME and RESTART ROUND for the host (a round game restarts the round, its number and the score
  kept).
- CUSTOM in SETTINGS > CONTROLS > BUTTON LAYOUT and the profile editor: the custom button layout, its buttons set in
  the classic menus' Controls screen.
- Bots: far shots, squads that split and regroup, flag crews and escorts for the carrier, driving on open maps, a
  warthog loaded up fast when you honk (passengers leave with the driver), out of a shotgun's range they take the
  other weapon, the DMR rated and its ammo picked up.
- Windows: ffmpeg.exe (FFmpeg 9.0.2, GPL) comes in the download, so recording works out of the box; its licence and
  source note beside it.
- CHANGE MAP and CHANGE GAMETYPE: LT / RT move to the next folder without opening it. Stock MCC maps show their
  facts. LOADING SCREEN rows moved to SETTINGS > VIDEO.

- JOIN BY ADDRESS: join a host by its IP address, a computer's name or a private network's name (with :port after it
  when the host uses another port). Y on JOIN LAN GAMES and on SERVER BROWSER opens it (JOIN WITH INVITE moves to LB
  on SERVER BROWSER); RECENT keeps the newest 10 hosts joined this way with the host's name and when, A joins one
  again, Y forgets it. A host that does not answer in 10 seconds shows what to check. On a pad's keyboard the ' key
  types a colon in an address.
- FRAME RATE LIMIT can be typed: 30 to 1000. Enter (or a click on the value) opens the field in place; left and right
  step to the list's neighbours from a typed value; on a pad, ALL VALUES shows the typed value in its place and ends
  with TYPE A NUMBER...

### Fixed
- With AE's menus on, a new copy is asked the COMMUNITY SERVICES question again (the classic screen, before AE's main
  menu): it was never asked, so the server browser and invites stayed off. AE's own screen comes in a later release.
- The classic menus' MENUS screen has its ARENA EVOLVED row again: picking it starts the game again in AE's menus.
- BUTTON LAYOUT's CUSTOM plays your own layout (it played DEFAULT); the pause menu's RESTART GAME and RESTART ROUND ask
  and act (they only beeped).
- The classic main menu no longer flashes before AE's at start.
- The HUD's top line ends with the version (it overlapped the match clock); the performance line in the HUD's
  colours.
- A pause card for a gametype with no stored name shows its mode's.
- TEST RUN's note box: Right and Delete work, Backspace no longer moves the cursor.

### Known limits
- Bots: a driver and passenger may leave their vehicle to chase an enemy flag carrier on foot.
- AE gametypes you edited keep your version: the revision 27 update (MODERN spawns, the EVO RIFLE start) skips them.

## 0.6.0-beta - 2026-10-30

MCC support: Halo: The Master Chief Collection's own Halo CE maps, stock and Steam Workshop, played straight from
its install, with no Xbox data and no conversion.

### Added
- MCC's maps read in place: AE finds the Steam install of Halo: The Master Chief Collection and plays its stock
  multiplayer maps (`<name>@mcc`) and the Halo CE maps of every Workshop item you subscribe to
  (`<name>@ws<item id>`), with their textures and sounds from MCC's own files. A first start without Xbox data
  boots from MCC's menus file. When AE finds MCC its maps are listed by default; Settings > MAP FILES turns MCC's
  files off, or picks its folder.
- CHANGE MAP: a STOCK MCC folder beside STOCK OG and STOCK GEARBOX, and one WORKSHOP folder that opens into the
  known collections (PRO EDITION, SOCIAL MAP PACK, NHE MAP PACK, 2V2 MAPS, CE+ BEYOND) and OTHER. A Workshop map
  shows its Workshop picture and a WORKSHOP badge; a map Steam is still updating is dimmed and says so.
- The weapon packs (battle rifle, SMG and the others) on MCC's maps, as on a Custom Edition map.
- The map list holds 2048 maps (was 256), so a large Workshop library fits.

### Network
- An MCC map in a network game is matched by its level name and map version. A player without the map is told
  what it needs: MCC installed (or its folder chosen in Settings > MAP FILES) for a stock MCC map, or the Workshop
  item's number to subscribe to. Every machine in a game on an MCC map needs MCC.
- Workshop maps are played on a host's own game (listen host); dedicated servers do not serve them yet.

### Known limits
- The Workshop maps are found once, as the game starts: subscribe to a new item, or turn MCC's files on in
  Settings, then restart the game to see its maps.
- Windows: a Steam library in a folder whose name has characters outside the system's code page may show no
  Workshop maps.
- No download of a missing Workshop map yet (planned: subscribe and download from the game, 0.6.1).
- Players without MCC cannot join a game on MCC's stock maps; they play the Custom Edition / Xbox stock maps as
  before (a bridge between the two is planned).

### Credits
- MCC map loading builds on GulchCE's MCC work (CC0).

## 0.5.3-beta - 2026-10-29

The second half of the fix for Macs.

### Fixed
- macOS: the game starts. With 0.5.2's window it stopped at once: the lens flares' and lights' visibility counts were
  copied through a query buffer (OpenGL 4.4 and 4.5, which a Mac lacks); they are read on the CPU there now.

## 0.5.2-beta - 2026-10-29

A fix for Macs.

### Fixed
- macOS: the game draws again. A Mac's OpenGL (4.1) has no glCreateQueries, which the renderer required, so the
  window stayed black (a beach ball) while the game ran unseen; the older glGenQueries is used there now.

## 0.5.1-beta - 2026-10-29

OpenCE's newest fixes, ChupathingyCE's Custom Edition and netcode fixes, and a clearer LAN search.

### Added
- From OpenCE (main, 70c5e600e): display.resolution_scaling takes "75%", "50%" and "25%" (draw part of the
  resolution each way and scale it up); objects' animated functions are drawn from the inputs their tick used.
- RECENT in CHANGE MAP and CHANGE GAMETYPE folds like the other folders: open at first, A folds it, and each picker
  keeps its choice (recent-folds.txt).
- PING on the performance line (SETTINGS > VIDEO > PERFORMANCE > PING, on by default): in a network game the line ends
  with "PING 45 MS" (the host: MAX PING, its slowest player's; "PING --" while the host is not heard). The line's
  MENU timing (FULL) is on the same line now.
- The LAN list names this computer's firewall (ufw, firewalld) when nothing is found, and the ports to let in: a
  guest's firewall hides every LAN game.

### Fixed
- From OpenCE: the level no longer flashes to the fog colour for a frame (a portal corner on a clip line); split
  screen's fog follows each window's own camera; an update with more files than the game can install is refused.
- From ChupathingyCE, Custom Edition maps: maps Halo PC loads with odd model data load; HUD bitmaps are scaled by
  their high-res flags everywhere; a shader with no maps is skipped (said once); effects read Halo PC's deterministic
  flag; players spawn with the map's multiplayer grenades; projectiles keep the velocity of what fires them; a tag
  block with elements and no address stops the load and names its tag, instead of looping.
- From ChupathingyCE, netcode: a machine joining a game in progress is told of the players who quit it; a client
  keeps the rider ejection a map's scripts set; objects that scripts move while at rest are sent as moving.
- From ChupathingyCE, HUD: meters and overlays are filtered when the HUD is scaled and honor the "use Xbox shading"
  flag; prompts space their button icons evenly with the high-res text.
- Halo CE+'s own map and gametype pickers find their lists.
- OpenCE's latency meter (display.latency_meter) is no longer drawn in the menus ("Ping -- ms" on JOIN LAN GAMES).

## 0.5.0-beta - 2026-10-29

OpenCE's latest (dedicated servers, network version 39), map packs in their own folders, and bot fixes.

### Changed
- **Network version 39** (OpenCE's since build-282). Plays online with OpenCE build-282 and later; not with Arena
  Evolved 0.4.3 and older or OpenCE build-281 and older (network 38). ChupathingyCE crossplay is not tested in this
  release.
- **Button layouts numbered as OpenCE numbers them.** FISHSTICKS is new; OpenCE's RECON is the Master Chief
  Collection's HERO, and AE's RECON stays the MCC's. A profile's layout is moved to the new numbers once, the first
  time 0.5.0 shows the profiles.
- The shader cache is OpenCE's (display.shader_cache): linked GPU programs are kept and loaded instead of linked again.

### Added
- From OpenCE (main, build-317): the dedicated server (`halo --server`) with its web admin page, RCON and master
  server listings, and the server browser's DEDICATED tab; the browser opens on ALL GAMES. A dedicated server can keep
  words out of its chat and players' names (server.chat_filter, off by default). Desktop frames are paced to the GPU
  (display.frames_in_flight). Other players' vehicles move smoothly between the host's updates, and weapon lights,
  markers and scope ticks move with the picture. A host checks where a client's shots leave from.
- **Map packs in CHANGE MAP**: a pack's maps (`maps_<pack>`, e.g. `maps_cex` for Halo CE+) get their own folder named
  by the pack, with their own names and pictures (`maps_<pack>/previews/<file>.png`), and picking one plays it.
- **Halo 1: Patch Edition** (`maps_pe`): its maps are known by name, and its build (01.10.12.1337, made from the NTSC
  maps) is accepted.
- Map layout files: `jump <percent>` sets a map's jump speed (50 to 200), for maps whose ledges were built for a
  higher jump; host and guests agree, as the layout travels with the game.
- **SNOWBALL FIGHT** (FFA, in AE ACTION SACK with Frag Nation): fists and 4 plasma grenades, none on the map; a
  plasma kill gives a plasma back.
- FRAME RATE LIMIT has every common monitor and handheld rate: 30, 40, 45, 50, 60, 72, 75, 90, 100, 120, 144, 165,
  180, 200, 240, 280, 300 and 360, besides AUTO and NONE.

### Fixed
- A pack's map shows its own name on the scoreboard, the pause card, the post-game report and the lobby (CE+'s
  Abyss showed as BLOOD GULCH, the stock map its file is named for).
- A pack's map finds its layout file (`maps_<pack>/layouts/<file>.txt`); none was ever applied before.
- An OpenCE dedicated server is no longer taken for a Delta host (both use the listing's flag 0x04): Arena Evolved
  tells a Delta host by its wire bytes.
- Joining a game from AE's menus no longer leaves you with no player (a black screen once the game starts) when the
  host's settings reached your machine a moment after it joined: the player is asked for as soon as they arrive.
- Capture the flag gametypes that earlier versions seeded are updated again: OpenCE's Kills Score had made them look
  changed, so their updates were skipped (and the log said "self-check: ERROR").
- With AE's menus on, the COMMUNITY SERVICES question no longer flashes past on the first start: turn the services on
  in SETTINGS > ONLINE > COMMUNITY SERVICES.
- **Bots: no walking off a ledge to a goal.** A bot with no route to its goal heads straight at it, jumping; that now
  stops at a drop (a railing, a pit) with the goal not down there, instead of the bot stepping off.
- Bots: a bot stuck on the same goal again and again gives it up and picks another; a bot steps down a short drop
  toward a lower goal.
- Bots: NORMAL bots keep a little more aim error up close (2 to 10 units), so their battle rifle is less deadly there.

## 0.4.3-beta - 2026-10-28

Search and Destroy's bomb, and Arena Evolved updates itself.

### Added
- **Search and Destroy: the bomb.** AE SND's bomb is a charge with a timer, carried on the back: the carrier keeps
  their weapons. An attacker picks it up by holding the action button over it; defenders can't touch it. A carrier's
  death drops it straight down where they fell, and grenades and blasts never move it. The game option BOMB DROP (off
  by default) lets a carrier drop it at their feet by holding the action button. A bomb that falls into a pit or deep
  water goes back to the attackers' spawn.
- The bomb's own look and sounds: its timer's four bars go red one by one as the fuse runs and its beacon flashes with
  each beep; it blows up where it was planted with a bigger fireball. Players near the blast are hurt less the further
  away they are, and the kills count for the planter.
- Only the attackers are told about the bomb: "You picked up the bomb", "<name> has the bomb" (with a keypad chirp)
  and "Bomb dropped by <name>"; only they see its marker. The carrier's HUD has a bomb tile that flashes in on a pickup
  and shrinks away on a drop.
- **Arena Evolved can update itself** (the Linux and Windows downloads from 0.4.3). When the game starts, it looks for
  a newer release on GitHub and asks before it installs anything. Updates are signed; your maps, settings and saves
  are never touched. SETTINGS > SYSTEM > UPDATES > AUTO UPDATE turns the check off.
- CREATE GAME: START GAME counts down 5 seconds before the game starts; only the START GAME row starts a game, and a
  pad joins only in MY PARTY.
- Bots in Search and Destroy: attackers and defenders play their sides, a carrier drops the bomb to fight.

### Fixed
- Crashes: when the player under the crosshair leaves the game, when leaving a network game, and on maps whose devices
  name a missing device group; a stack overflow in long chains of explosions; memory corruption from frames with many
  dynamic lights (ChupathingyCE's fixes).
- Custom Edition Ogg sounds made at another rate play at their own pitch (ChupathingyCE's fix).
- The dedicated server's control panel is hardened (ChupathingyCE's fix).
- Pickers: RECENT is each picker's own and shows a battle rifle pair as one row; search opens on ALL. CHANGE MAP: a map
  in a folder of its own shows in its tags' groups too.
- Bots: a full learnt list no longer fails every search; no melee through a teammate.
- The Pit (cyberdyneb2): from the alley by the K1 room the room drew empty grey, and from the room so did the alley.
  The map's own data has the doorway between them wrong (its portal joins the alley to itself); AE puts it right in
  the loaded map, on every machine, for this version of the map only (the file is unchanged).

## 0.4.2-beta - 2026-10-28

A menus patch: the lobby, the map and gametype pickers and split screen in AE's menus.

### Fixed
- Team change in AE's menus: 0.4.1's lobby gave no way to change team. Each player now picks a team when joining, the
  roster cards have team arrows (< RED >, d-pad left and right, or a click) and Y (keyboard R) changes team; the host
  has SHUFFLE TEAMS and can pin each bot's team.
- The loading screen is no longer covered by the frozen screen that started the game: the Xbox loading screen shows
  through the whole load (its picture comes from your own disc when you import one).
- Split screen: menus opened from a pause belong to the player who paused.

### Added
- MY PARTY: a panel on the main menu and in CREATE GAME with the people (this PC's pads first) and the bots apart. X
  (keyboard F), a click or RB opens the party view: team arrows, ADD PLAYER, SHUFFLE TEAMS, the BOTS rows and BOT
  SETUP...; a click on its head or its "N more" line opens the whole lobby.
- CREATE GAME: the lobby's own rows (NETWORK, PUBLIC, MAX PLAYERS, INVITE) sit on the left under the quick options;
  A on a value row opens a list titled with the row's name.
- CREATE GAME: MORE OPTIONS... holds the start weapons (LOADOUT, PRIMARY, SECONDARY: picking a weapon sets LOADOUT
  CUSTOM), the installed weapon packs (ON/OFF) and less used rules; a summary too long for its row scrolls sideways.
- CHANGE MAP and CHANGE GAMETYPE: Y (keyboard R) searches as you type; X (keyboard F) puts a map or gametype in
  FAVOURITES or your own folders (NEW FOLDER..., RENAME, DELETE); RECENT lists the last map + gametype pairs you
  hosted, and picking one sets both. Folders and RECENT are saved on this PC.
- CHANGE MAP: maps grouped by where they come from: STOCK OG, STOCK GEARBOX, NHE, COMMUNITY COMPETITIVE (all 40 maps
  of the community set named, with players, size and credits), then WARM UP, BTB, H2 DEMAKES, AE SEARCH AND DESTROY,
  MOD MAPS, CUSTOM EDITION, HALOMD.
- CHANGE GAMETYPE: an AE OBJECTIVE folder with AE SND and AE BOMB.
- GAME OPTIONS: SAVE AS NEW and SAVE AS COPY ask for the name first (a free name filled in); "Save changes?" has
  DISCARD.
- Split screen: a pad's profile pick is an inline card (several pads at once, in the lobby too), with NEW PROFILE;
  START on another pad on the main menu asks its player for a profile.

### Changed
- CREATE GAME: START no longer starts the game from anywhere: it brings the focus to START GAME, and START again, A or
  a click there starts it. Only player 1 starts the game.
- CREATE GAME: with the keyboard as player 1, the one pad's START joins it as player 2 (it started the game).
- CREATE GAME: a refused bot row says why; BOTS shows the bots that fit the open seats.
- A Nintendo pad reads its buttons by their labels in AE's menus and the TEST RUN card: A (east) selects, B (south)
  goes back, X and Y as printed; in the game its buttons stay as before.
- Keyboard: F is X and R is Y in AE's menus, as the prompts show.

## 0.4.1-beta - 2026-10-26

### Added
- SEARCH AND DESTROY bots: a WINGMAN stays with the bomb carrier (a bot or a player) to the plant and guards the
  planter; defenders HOLD their site, spread over its cover (at most one goes forward, HEROIC and up, with three or more
  alive); movers, the carrier too, take different lanes, and each round's lane and holding spot change; a bot that drops
  the bomb (or the ball, in Oddball) to fight goes back for it. The bots' game summary counts plants and detonations
  apart, wingman time, recoveries and defenders holding.

### Changed
- SEARCH AND DESTROY: the planted bomb beeps the whole fuse from where it lies, faster near the end (every 1.2 s, 0.6 s
  in the last 10 s, 0.3 s in the last 3), with the strip's fuse clock as before.
- SEARCH AND DESTROY: the bomb carrier is marked to his own team only, no longer to the enemy.
- HEROIC bots aim a little worse ("they are aimbots"): their first aim error 4 -> 5 degrees, settling a little slower,
  firing a little less tight and leading a moving target a little less; their reactions and movement are unchanged.
- Frames are paced to the GPU (OpenCE #496, by bnunu): at most three frames queued (display.frames_in_flight), so frames
  come evenly when the GPU is the slower part (a laptop went to 120 fps).

### Fixed
- SEARCH AND DESTROY: a defusing bot stands still (no strafing or jumping) from the edge of the defuse radius, so its
  defuse no longer restarts over and over (the HUD's DEFUSING came and went).
- Bots shot through teammates (a betrayal on Sali Creek): a bot holds fire, or sidesteps, while a teammate stands in its
  line of fire nearer than its target, and throws no grenade where a teammate is near the landing spot, in every team mode.

## 0.4.0-beta - 2026-10-26

### Added
- ChupathingyCE 0.8.0e to 0.8.0h merged (its main at d7fafc81f): its performance batch (enemy names' line of sight
  tested once a tick, the sound mixer letting the game in between voices, the LRU cache's check walk left out of
  release builds, the host's stream read 32 KB at a time, more dynamic vertex buffer storages) and four config.toml
  rows: `display.particle_limit` (4096), `display.particle_ticks` (off), `display.dynamic_light_limit` and
  `display.specular_light_limit` (0: every light, as before). Network play is unchanged: Arena Evolved stays on OpenCE's
  network 38 and keeps playing ChupathingyCE 0.8.0f to 0.8.0h both ways.
- TEST RUN (DEV MODE, SETTINGS > SYSTEM > DEV > TEST RUN): the owner's in-game test runner. One running game plays a list of
  tests (test-runs/<list>.txt): it sets each scenario up, shows a card with what to check, and takes PASS, FAIL, NOTE or SKIP
  (F7, F8, F4, F5; a controller holds BACK and A, B, X, Y). FAIL and NOTE save a screenshot, and a clip for a video test.
  After a NOTE it asks PASS, FAIL or CONTINUE. The card's hints follow the device last used (keys, Xbox, PlayStation, Nintendo).
- The results go to test-runs/<list>.results.jsonl next to config.toml, each record synced as it is given. A run plays the
  tests with no PASS for the list's revision; RESTART ALL plays every one; a crashed run resumes where it stopped.
- debug.test_run (HALO_TEST_RUN) starts a list at boot; debug.test_run_auto is for the automated tests. Both are test-only.
- Dev camera: at distances past 100 the player stays clear of the reticle.
- CREATE GAME / LOBBY: the host types the BOTS count (Enter on the row with a keyboard, or a click on its value; A Keep,
  B Cancel; up to 127), as SETTINGS' typed mouse values.
- BOTS past 31: the row steps OFF, 1-31, then 47, 63, 95 and 127 (or type any count up to 127; OFFLINE takes 120),
  and its help line turns the warning blue: big bot games run smoothly, but the bots get lost and stuck on big maps,
  drive poorly and swamp the fights, so 31 or fewer play best.
- display.performance full: a second line MENU x.x ms, AE's menus' process + draw time of the last frame.
- AE SEARCH AND DESTROY is a gametype (seed revision 26): TEAM AE SEARCH AND DESTROY (AE SND) in AE ACTION SACK, next
  to AE BOMB, plays 0.3.9's SEARCH AND DESTROY rounds (one life a round, plant 5 s, defuse 5 s, fuse 45 s, rounds of
  2:30, sides swap every 3 rounds, first to 4) with no game time limit; the host picks it in CHANGE GAMETYPE. The
  config row game.bomb_snd stays as a test override (SEARCH AND DESTROY in any Bomb game). Saves from before get AE
  SND seeded; edited gametypes are untouched.
- SEARCH AND DESTROY bots: attackers push together, escort the carrier and guard a planted bomb; defenders split
  between holding their site and meeting the push; one defender defuses while the others cover. Bomb and Oddball
  carriers drop it to fight when an enemy close by is engaging them and they have a loaded gun (never near the plant,
  as the last one alive, or in cover with escorts), and take it back once the fight is over; RECRUIT bots never drop.
- Map layouts: a line written "snd <line>" is read only in a SEARCH AND DESTROY game, so one layout file holds a map's
  SnD version (no teleporters, no turrets, other spawns and bomb sites). The layout log lists the map's own spawns and
  flags with their teams.
- The Pit: AE names the Custom Edition map cyberdyneb2 (Zteam's remake of The Pit) in the map list; the
  file keeps its name.
- AE PAUSE MENU (AE's menus on): START in a game opens AE's own pause menu in place of the classic one. One player: the
  list on the left and a game card on the right (map, gametype, mode and goal, the team scores or the FFA leader and
  you, the round or the time left); split screen: each player's panel in their own view, and each player pauses alone
  (a network game plays on; the others' START opens theirs). Rows: RESUME, SETTINGS (yours: AE's SETTINGS; in split
  screen drawn in your own view while the others keep their pauses), CHANGE TEAMS (team games; asks: it kills you),
  SCOREBOARD (the in-game scoreboard for now), and for the host under GAME (HOST): GAME OPTIONS and
  CHANGE MAP (the next game's: the lobby takes them as it comes back; CHANGE MAP ends this game after asking), BOTS
  (count, skill and LOBBY SETUP's bot rows, at once), END GAME (asks); then QUIT (asks). The campaign's has REVERT TO
  SAVED and SAVE AND QUIT, co-op's FRIENDLY FIRE and QUIT, and stops the game as before. Each row does what the classic
  pause's button did. With AE's menus off, the classic pause is unchanged.
- AE POST-GAME (AE's menus on): the end of a multiplayer game shows AE's own post-game in place of the game's carnage
  report. GAME RESULT first: the result in the winning team's colour (RED TEAM WINS; FFA your place spelled out,
  FIRST PLACE, in your own armour colour), the gametype on the map, then your rows coming in one by one with their numbers counting up: RESULT
  (WIN, LOSS, TIE or your place, with the score), KILLS (deaths under it), ASSISTS and BEST SPREE (kills in one life,
  new), and the FINAL SCORE card on the right (team scores or the FFA leaders, TIME PLAYED). A skips the counting,
  then goes on. Split screen: each player's own result in their own view. Then CARNAGE: the AE scoreboard of everyone
  under the result banner (RED TEAM WINS 50 - 38), with a BEST SPREE column; every player of this PC lit. Player 1's A
  takes everyone back to the lobby (the host's, as before; a guest waits for its host), B asks to leave the game. With
  AE's menus off, the game's carnage report is unchanged.
- AE SCOREBOARD (HUD STYLE AE): the scoreboard held on BACK / Tab is drawn in the AE menus' look: the gametype, the
  map and the goal over a rule; a team game's teams stacked under their score bars, FFA one list with places and each
  player's armour colour; columns K D A PING (SCORE first in every mode but Slayer); your row in the menus' focus
  style; DEAD or QUIT after a name. More than 16 players: compact rows; 25 to 40 in teams: the teams side by side;
  more than fits: the lists scroll with the wheel and Page Up/Down, FFA keeps its top three, and your row stays pinned
  at the foot while it is scrolled away. The HUD steps aside while it is up, the end of a game too. Split screen: each
  view draws its own compact board with that player's row lit. A grey ring keeps black or white armour chips in sight.
  The mouse's player picking (vote kick, mute, kick, ban) and co-op keep the classic scoreboard for now.
- KEY BINDINGS: a pad's B while a row waits for a key cancels the capture and keeps the old key (checked by a
  new suite case; OpenCE fixed the classic screen the same way).
- BOT SQUADS (ideas from F.E.A.R.'s published AI talk): bots of a team on foot fighting the same enemy form a squad
  (NORMAL and up): the nearest with the enemy in sight holds its ground and keeps firing where the enemy ducked out of
  sight, one or two others go round to a side (outdoors unless the enemy is indoors), the rest push. A player near the
  squad leads it: the bots flank round the enemy from where the player stands. `HALO_BOTS_FEAR=0` turns it off.
- BOTS AND VEHICLES (ideas from UT2004's bot scripts): bots dodge slow shots (rockets, plasma, fuel rod; HEROIC and up
  jump a rocket at their feet), step out of a moving vehicle's way, walk round a vehicle standing in theirs. Drivers
  take the wheel first, wait for a teammate coming to a seat (or one who puts a round into the vehicle), keep off walls
  and teammates, stay out of buildings (wide halls only), stop outside when the objective is indoors, ease off on ice;
  Ghosts run enemies over, Warthogs lap the enemy for their gunner, whose chain gun keeps firing. A bot thrown from a
  flipped vehicle rights it and gets back in once the fight is over (or at once when someone else rights it), and gives
  up its seat to a teammate holding the action button at it. On protected maps (stripped tag names) vehicles are told by
  their type. `HALO_BOTS_UT=0` turns these off.
- WHEELMAN: a kill by your vehicle's gunner or passenger is an assist for you as its driver, with a "Wheelman assist:
  <name>" line where your kill messages show.
- FRIENDLY SPLATTER (GAME OPTIONS, TEAMS): HALO CE, a vehicle that touches a teammate can kill them; FAST ONLY, a
  teammate is hurt only from a real speed up (none below 0.10 world units a tick, all of it from 0.20: a hard ram).
  Enemies are splattered as always. The host's rule; older builds ignore it. AE gametypes play FAST ONLY.
- DELETE in CHANGE GAMETYPE: View / Back (or the prompt, clicked) deletes one of MY GAMETYPES after a confirm, its
  gametype file too. Seeded AE and NHE gametypes, the built-ins and the one the game is set to can't be deleted.
- Vehicle horn (test): a driver's MELEE honks (CMT TSC:E's warthog horn); bot drivers honk at a teammate in their way,
  and a honk calls nearby teammate bots in, the gun first. Local games only for now: not heard over the network.
- OFFLINE (CREATE GAME's NETWORK, with LAN and HOST ONLINE): a game on this PC only, nothing listed, nobody joins, bots
  as everywhere (issue #1, a Steam Deck player: "an offline mode ... where pause pauses everything").
- The pause stops an OFFLINE game (bots and all); it runs on again as the last pause closes. LAN and HOST ONLINE games
  keep running, as in Halo CE. `HALO_PAUSE_STOPS=0` keeps OFFLINE running too.
- Bots jump with purpose in a fight: when hurt just now, or (HEROIC and up) when their enemy aims right at them;
  the random rhythm jumps come half as often.
### Changed
- AE gametypes (seed revision 25): FRIENDLY SPLATTER FAST ONLY, and all five shipped weapon packs named in every one
  (battle rifle, SMG, suppressed SMG, carbine, suppressed AR), so any of them can be a start weapon; start weapons
  unchanged. A game on an AE gametype now needs the packs on every machine (every AE build ships them). An AE
  gametype you edited keeps your edits.
- The lobby's player chips have each player's own armour colour (a team game's: the team's), as the main menu's.
- Bot gunners stay with a driver who stops: they get out after a 45 s stop with no fight, or 12 s with no driver (was
  15 s and 8 s), and at an objective with the rest.
- SUPPRESSED SMG and SUPPRESSED AR: the silenced weapon packs' names in GAME OPTIONS, WEAPON PACKS and their pickup
  messages (the packs' ids, files and layout names stay ae_smg_silenced / ae_ar_silenced, silenced_smg / silenced_ar).

### Fixed
- Bomb: a defuse's progress was the team's: a teammate who stepped in as the defuser stepped off carried it on. It is
  the defuser's own now; while that player is alive at the bomb the others wait (never faster with more players).
- SEARCH AND DESTROY: a dropped bomb went back to the attackers' flag after the gametype's untouched time; it now lies
  where it fell for the attackers to pick up, and resets only when it lies in a pit.
- Power item timers, waypoints and callouts left out items the gametype keeps off the map (SEARCH AND DESTROY's
  nothing on the map, MAP GRENADES OFF); they announced and timed them before.
- TEST RUN: cancelling a FAIL's note box took the FAIL as given; it now takes it back and the test is up again.
- HUD STYLE AE on Custom Edition and Gearbox maps: the ammo counter drew only its number ("100 x"), no round icon or
  magazine rows, as those maps lack the stock HUD sheets the panel read; it now draws them from the high-res copies of
  those sheets that ship with the game (Halo PC Restored's art), so they look as on the Xbox maps; with HIGH-RES HUD
  off it draws plain stand-ins of the same shapes.
- Custom Edition maps: the sniper rifle's two range bars drew at every zoom, unzoomed too, as blue bars with the ticks
  cut out. Halo PC's sniper HUD subtracts its ticks from their mask where the Xbox's multiplies them; drawn as the
  Xbox's multiply, the ticks fade in with the zoom as on the Xbox maps.
- HUD STYLE AE: a vehicle gun's HEAT row only when its HUD has a heat element, as Halo CE's HUD: the Warthog's chain
  gun and the Ghost's guns never heat, and their rows stayed empty. The Scorpion's CANNON row is gone too (Halo CE's
  HUD has none); the Banshee's rows are unchanged.
- Nintendo pads: the south button's prompt shows the B glyph and the east button's the A glyph in every AE prompt row
  (the A and B glyphs were swapped against X and Y, which went by position).
- Split-screen guests (players 2-4) can no longer change the lobby's BOTS and BOT SKILL rows or LOBBY SETUP; they see
  the values, and the host's player 1 changes them.
- Custom Edition maps: a bitmap wider or higher than the device's 4096 pixels is drawn instead of a flat colour.
  Remastered map packs (Halo Flawless Cowboy Edition's refined teleporter shields, field generators, needler glows,
  up to 15360 wide) are drawn from their first mipmap level that fits; one without mipmaps has its rows averaged.
- CREATE GAME: when hosting fails ("Could not host the game", e.g. the game's port is already held) the error dialog
  opens instead of crashing the game (the dialog read layout metrics the screen had not set before its first draw).
- Oddball, Juggernaut and Reverse Tag: bot carriers hide in cover (found from the map itself) and hold there until an
  enemy comes near, sees them or hurts them; escorts guard the open side.
- Bots climb between floors by stairs and ramps that the map's path data leaves out (Madhouse). A bot strafing in an
  objective fight no longer counts as stuck and drops its objective.
- Scoreboard (HUD STYLE AE) in Oddball and King: the team scores were the time held in ticks (1564 for 0:52); they
  are the game's time now, and a player's time under a minute reads 0:36, not :36.
- AE PAUSE MENU: one player's list cut BOT SKILL's AS BOT SKILL and GAME OPTIONS' gametype name, and grew too big at
  a bigger UI SCALE: its panel is 690 wide at 1080p and its text scales with the screen's height, so the values fit and
  the game card keeps the gametype's name.
- AE MAIN MENU: the PLAYERS card named player 1 "PLAYER 1" at every start until a game, CAMPAIGN or SETTINGS
  picked a profile; it now shows the profile they would pick (the one used last), with its colour.
- Bots on a rocket Warthog's gun hardly fired: their line of sight started inside their own Warthog, and up to half of
  it ended there, so they lost their enemy again and again and each new sighting began their reaction time again. The
  line now starts outside the bot's own vehicle: rocket gunners fire 35% (NORMAL) to 4 times (LEGENDARY) as often a
  seated minute; the chain gun's gunners fire as before.
- Danger Canyon (BTB CTF): half the bots stood pressed against the wall between the two flags all game. The flags are
  14 units apart straight but 150 to 230 apart on foot, round the canyon, and the path search gives up on a way that
  much longer than the straight line, so it stopped at the wall. When a search ends no nearer the goal on foot, bots
  now follow their own walk to the goal a stretch at a time. In a 15-bot test game, stuck events fell from 1122 to
  about 200 and kills rose from 2 to about 30. Bots still take no flags there: the way round is long, and the two
  teams' attackers meet on it.

## 0.3.9.1-beta - 2026-10-25

- AE menus: the game's own error messages (a join that failed, a map you don't have, a host that left or closed
  the game, a lost connection) show as AE's error dialog with the game's text and one OK; A, B, START, Enter, Esc or
  a click on OK closes it. The game's blue ATTENTION box drew over AE's screens and never closed: its START never
  reached it, and A and B went to the screen under it. In a game with no AE screen up the game's box shows as before.
- AE LOBBY: a joiner whose game is gone before it starts (the host closed it, or a map is missing) goes back to
  where it joined from, LAN GAMES or the server browser, instead of staying in a lobby of 0 players.

## 0.3.9-beta - 2026-10-24

- DEV MODE (system.dev_mode, off; this PC only, never in another machine's network game), for testing AE. Its first
  tool is the dev camera: F2 a third-person camera over the shoulder (the body drawn, the reticle on the
  shot's point, pulled in at walls), F3 the other shoulder, F6 a free camera that moves and looks as you do (jump and
  crouch raise and lower it). The keys are bindable (KEY BINDINGS > ACTIONS, shown while DEV MODE is on); a
  controller holds BACK with the d-pad's down (the camera) or right (the shoulder). The shoulder camera is on foot
  only: a vehicle, a zoom, a death or a cinematic gives the game's own camera back until it ends.
  system.dev_camera_distance sets the distance (25 to 400%). Its hit markers: white corner brackets round the reticle
  (fitted to each weapon's) when your damage lands on anything that can be damaged, by any weapon, stock or a pack's,
  grenade or melee; red and larger on a kill (0.2 s a hit, 0.43 s a kill). No menu row yet.
- BACKPACK WEAPONS (SETTINGS > VIEW > PLAYERS, ON by default): the weapon a player carries but does not hold shows on
  their body: a rifle, shotgun or launcher slung across the back, a sniper rifle upright on the
  back, a pistol, needler or plasma rifle on the right thigh. Never the flag or the ball, nor in a vehicle; camouflage
  hides it with the body. Only drawing changes: nothing goes over the network, so you see it on every player in any
  game. The idea is aLTis94's Chimera script (no code taken).

- WHEEL ZOOM (Settings > CONTROLS > MOUSE, each player's own, OFF): with the keyboard and mouse, the wheel zooms by
  itself while the weapon in hand zooms: up a level in, down a level out, in any ZOOM mode. It then doesn't switch
  weapons (the switch weapon key still does); with a weapon that can't zoom it switches weapons as before. The idea is
  aLTis94's Chimera scroll_zoom script (no code of it is used).

- PISTOL COLOR and PISTOL TRIM (Settings > VIEW > WEAPON COSMETICS, each player's own, DEFAULT): your pistol in first
  person in a multiplayer armor colour of your choice, its silver body and its dark parts. Cosmetic and only on your
  screen: others, third person and the pistol on the ground are unchanged, and nothing is sent. The stock pistol only
  (a map's own pistol keeps its look). The idea is aLTis94's Chimera pistol_color script (no code of it is used).

- DEAD ALLY MARKERS (Settings > HUD > NAMES, each player's own, ON): in a team game, a cross over a fallen
  teammate's body, seen through walls, until they respawn. It takes their name's colour (white
  with ENEMY NAME COLOR RED, so it never reads as an enemy), the rows your WAYPOINTS setting gives the item waypoints
  (the distance, the name, the cross) at WAYPOINT SIZE, and your HUD STYLE's look (AE's dark outline, or CLASSIC's
  plain text). Only drawing on your screen: it works in any game, a stock OpenCE host's too. The idea is aLTis94's
  Chimera hud_features script (no code of it is used).

- ALLY RETICLE (SETTINGS > HUD > RETICLE, ON by default): your reticle turns green when you aim at an ally, as it turns
  red over an enemy: a teammate in a team game, another player or a marine in co-op and the campaign. Never yourself,
  nor in a free for all game. Only drawing changes: nothing goes over the network, so it works in any game. The idea is
  from aLTis94's Chimera script, no code taken.
- New gametypes in AE ACTION SACK, after Chalwk's SPCLib (its rules and numbers, MIT; no script taken): ONE IN THE
  CHAMBER (a pistol with one round, a bullet kills, each kill gives a round, 3 lives, the last player standing wins),
  FRAG NATION (fists and 2 frags + 2 plasmas; a grenade kill gives grenades back: a frag a frag, a plasma a plasma, a
  stick two), MELEE ONLY (fists, nothing on the map) and NEEDLES (needlers with bottomless clips). Fists are
  ChupathingyCE's melee-only Bomb fists. Every rule is the host's, so players on stock OpenCE play them too (their
  menus call them Slayer). CHANGE GAMETYPE shows them by their full names with their own descriptions and weapons
  (they showed OpenCE's Hardcore text, and NEEDLES sat in STOCK). Bots play Frag Nation well; they don't adapt to One
  in the Chamber yet.
- A Nintendo pad (Switch Pro, Switch 2 Pro, Joy-Cons) shows Nintendo's buttons in the game's button icons (HUD and campaign help lines), where it showed the Xbox's: the Nintendo letters by their label, so the south button reads B and the east one A, as in AE's menus. Kenney's Input Prompts glyphs (CC0), in the same dark discs as the PlayStation icons.
- Bots take REVENGE: a bot killed three times in a row by the same player, with no kill back, hunts that player for a
  minute or until it kills them. RECRUIT to NORMAL chase recklessly and push the fight; HEROIC chases straight but keeps
  its head; LEGENDARY and DEMON hunt smart, holding a spot near them, higher ground first. A hunter goes where it last
  saw or heard its nemesis (or a teammate saw them), never where they are now: found no one there, it roams as usual
  until it learns more. Any player can be a nemesis, bots too, never a teammate.
- AE BOMB plays like Assault: the bomb spawns at one team's flag and that team attacks (the other can't pick it up);
  the teams swap each round, red first (game.bomb_flag_spawns). A defender stands on a planted bomb 3 seconds to save
  it (a touch did), and it goes off 5 seconds after the plant (game.bomb_defuse_time, game.bomb_fuse_time). No "play
  ball" in it. Grifball keeps its own rules.
- SEARCH AND DESTROY on it (game.bomb_snd, OFF for now, a test): classic competitive rounds: one life a round;
  the attackers win a round when the bomb goes off or every defender is dead, the defenders when they defuse it,
  every attacker is dead before the plant, or the round's 2:30 pass with none planted. Plant 5 s, defuse 5 s, fuse
  45 s, sides swap every 3 rounds, first to 4 rounds; a 6 s countdown before each round. Nothing on the map but the
  bomb (the weapons of the dead stay). Each is a config row (game.bomb_snd_*). The dead read "You are out of lives"
  until the next round.
- AE BOMB's HUD with HUD STYLE AE: AE's round strip (a cell per player lit while alive, rounds won as pips, the round
  clock, the planted bomb's fuse) and a PLANTING / DEFUSING bar replace ChupathingyCE's Bomb HUD; a client draws them
  from Bomb's own state message. Attackers see BOMB DEFUSING while a defender is on their bomb. The PRE-GAME
  COUNTDOWN runs again at the start of every round, its beep on each digit drawn. Under AE's rules a planted bomb goes
  off when its fuse ends (no extra 3 s countdown: the clock and the blast reach 0 together), the grab tone and the
  round-start "play ball" are gone. Classic HUD style keeps ChupathingyCE's Bomb HUD.
- Bots play BOMB (AE BOMB, AE GRIFBALL): they were kept out of Bomb games (a lobby set to 6 bots started with none).
  Now a bot fetches the loose bomb, carries it to the enemy goal and stands in it until it arms; its teammates escort
  it, and go on ahead to clear the goal when it is close; the other team hunts the carrier, keeps one or two guards at
  its goal and steps onto a bomb planted there to save it. In melee-only Grifball they brawl: at their target, the blow
  within reach, no trigger. All they know is on everyone's HUD (the bomb's, the carrier's and the goals' markers).
  Other modes play as before.
- INFECTION bots: a bot zombie hunts with the skull. It went for weapons it may not take, kept its distance as with a
  gun, backed away and hardly struck, so a bot first zombie could not infect anyone (Hang 'Em High, NORMAL, 300 s: 1
  player infected, 1 blow). Now it goes where it last saw or heard a human, runs at the one it sees and strikes within
  reach at every level (all 7 infected in 68 s). Bot humans keep their guns and back off from a zombie close by. Only in
  Infection: Oddball, Juggernaut and the rest play as before.
- CREATE GAME is one screen with its lobby: MULTIPLAYER > CREATE GAME opens a live lobby at once (invite only), START
  GAME is the button framed in blue with the START button's mark, and X or the LOBBY row opens LOBBY SETUP (NETWORK,
  PUBLIC, MAX PLAYERS). Players who join see the same screen as LOBBY, read only. RB (E) jumps to the lobby panel and LB
  (Q) back to the left column ("LB RB Column"), so the lobby rows need no scroll down past START GAME.
- START GAME is no longer a filled blue bar that looked like the highlighted row: only the highlight is white. It stays
  under the rows when they scroll at a bigger menu size, so it is always on screen. A player who joined no longer sees
  WAITING FOR THE HOST TO START highlighted as if it could be chosen.
- GAME OPTIONS > WEAPON PACKS: a sub-screen for the packs and the start weapons. Every pack that is on can be a start
  weapon, not only the first two.
- AE GRIFBALL and AE BOMB (seed revision 24, in AE ACTION SACK): Bomb, the game type Bungie cut, on AE's rules.
  Grifball is melee only, one hit kills, rounds of 3 minutes, first to 3; AE BOMB plays on any map with two CTF bases
  (pistol and assault rifle, arm in 3 seconds, first to 3 rounds). CHANGE GAMETYPE has a BOMB filter and GAME OPTIONS
  edits Bomb's rules. Bomb is played between Arena Evolved and ChupathingyCE machines only (stock OpenCE players are
  refused), and bots don't play it.
- VEHICLES DESTRUCTIBLE, a new gametype rule (seed revision 23: ON in every AE gametype with vehicles): vehicles are
  destroyed by explosives only (a warthog takes two rockets, a ghost or banshee one), can't be destroyed
  while someone rides them, and their wrecks clear after 10 seconds. A destroyed vehicle explodes, and its blast is
  the destroyer's (point blank, it can kill you). A first version: the damage numbers are still to be
  tuned, and bullets don't damage vehicles yet.
- TEST: the VEHICLE DAMAGE PACK (packs/ae_vehicle_damage.aepack; CMT's TSC:E damage and wrecks, credits CMT and the
  Evolved team). With VEHICLES DESTRUCTIBLE on, damaged warthogs, ghosts and banshees smoke and burn, explode as in
  TSC:E and leave burning wrecks. Each machine shows the look it has; nothing of it goes over the network. A test
  feature: tell us if a shot doesn't count in a game with it.
- Weapon packs on Custom Edition maps: every pack (and the vehicle damage pack) plays on the Gearbox maps and the custom
  maps built on the stock tags. A map without the stock assault rifle's tags plays its own weapons and the log says why.
- Weapon packs: with the battle rifle pack on, a map that ships its own battle rifle (the H2 demakes', CE+ X's)
  plays Arena Evolved's instead everywhere: its spots, placements and starting weapons give the pack's battle rifle,
  so it is the same battle rifle on every map. The log names each replaced kind of reference.
- EXPERIMENTAL: gyro aiming on PC (SETTINGS > CONTROLS > GYRO, OFF by default) for the DualSense, DualShock 4, Pro
  Controller and Joy-Con: always, or only zoomed in, with its own turn and tilt sensitivity.
- HUD STYLE AE in the campaign: help text keeps its line breaks (the tutorial's stick and button pictures drew over its
  words), and CAMPAIGN TIMER shows the corner clock in AE's digits.
- SETTINGS > ACCESSIBILITY > LOADING SCREEN: the least time the campaign's and multiplayer's loading screens stay up.
- SETTINGS has the settings only the classic menus had: AUDIO > SOUND; ONLINE > MAPS > INTERNET SHARING (your maps to
  internet joiners); SYSTEM > FILES > WEAPON PACKS, MODDED DISC (restarts) and MCC FILES; and SYSTEM > CAMPAIGN with
  FALL DAMAGE and HEALTH (HALO CE, REACH, HALO 2, HALO 3). CALLOUT VOICE lists the voices when AUDIO opens.
- The menus' mouse cursor is half OpenCE's size with Arena Evolved's menus (36 pixels at 1080p, 48 at 1440p), to sit
  with their rows; the classic menus keep OpenCE's.
- SETTINGS > CONTROLS > KEY BINDINGS: change every keyboard and mouse action's two keys in Arena Evolved's menus (it
  opened nothing before). Press a key, mouse button or the wheel; a key another action had moves over and the screen
  says so; RESTORE DEFAULTS per section or all. AUDIO's PUSH TO TALK opens it. BACK PADDLES picks the button each back
  paddle presses (each player's own).
- SETTINGS > CONTROLS > MOUSE: the sensitivities and the zoomed and vehicle scales go to 0.01: Shift with the arrows,
  or Ctrl with the wheel, steps 0.01, and Enter or a click on the value types one (held to the row's range). A pad's A
  still lists every value.
- CREATE GAME: with bots on, LOBBY SETUP (X) has the classic menus' SHOOTING SKILL, MOVEMENT SKILL, PLAYFIGHT and BOT
  TEAMS. CAMPAIGN: PLAYERS reads as information, and a line under CO-OP says where co-op is for now.
- Menus: left and right go round past a row's ends, as in MCC. CALLOUTS, CALLOUT DETAIL and CALLOUT VOICE moved from
  HUD to AUDIO. SETTINGS > CONTROLS changes the profile's rows before any game.
- Gametype files: read at each start (a seeded AE or NHE gametype is never read from or written to a file), and
  SETTINGS > SYSTEM > OPEN GAMETYPES FOLDER opens the folder.
- Callouts: the words of a call join to the frame through the game's resampler; on 5.1 they play from the front pair.
- Voices: Cori is a new voice (a Qwen3-TTS clone of Cori Samuel's LibriVox reading, public domain), and six new callout
  packs come in every download: Helljumper, Corporal, Aurora, Bastion, Selene and Solace.
- Bots: in a network game in progress, bots fill up to BOTS (new bot machines join too);
  the classic Game Options BOTS row goes up to 31.
- Per-player settings (VIEW, HUD, the mouse and the rest of SETTINGS' own rows) are saved to the profile again: since
  0.3.7 every change was lost at the next start. Player 1 also takes their profile before such a change at the menus or
  in a pause's SETTINGS.
- Games started from CREATE GAME, JOIN WITH INVITE or SERVER BROWSER play with player 1's profile (the one used last):
  the name showed as "Player" and the profile's colour and settings were not used, unless CAMPAIGN or SETTINGS had
  been opened first.
- Bots that join a LAN or online game in progress (BOTS raised mid-game) no longer show up as split-screen views on
  the host (the host drew a view for each of the first three, and its frame rate fell).
- Lowering BOTS to OFF during a network game no longer ends it: the host plays on alone, as in a game started alone.
- Team games with each team's own vehicles (CUSTOM counts, or a set per team) give each base its own vehicles:
  on Sidewinder every vehicle counted as red's (its race spawns line blue's base), so red's counts were spread over the
  whole map and blue got none (one scorpion, nothing else); Blood Gulch's blue ghosts were half red's. A map only
  places the kinds it has: Sidewinder and Blood Gulch have no rocket warthogs, banshees or turrets.
- Split screen in Arena Evolved's lobby: START on another controller adds its player (its profile, or a guest's), B on
  it takes them out; with one controller and the keyboard, the lobby panel's ADD PLAYER first. Players who joined
  another's game have ADD PLAYER too. At the main menu, START on another controller signs its player in (a card
  under player 1's; B signs them out) and the next lobby, hosted or joined, takes them in with player 1. With the
  keyboard as player 1 and one controller, START on the controller signs it in as player 2. A guest player (no
  profile of their own) plays with player 1's profile and settings, under the name Guest 2, 3 or 4.
- CAMPAIGN: with CONTINUE focused, DIFFICULTY shows the saved game's difficulty, which CONTINUE plays (it showed the
  next new game's).
- CALLOUT DETAIL VERBOSE counts five to one before every power item, not only rockets, so the list before it is read
  at an easy pace: the item's call may start up to 13 seconds before it is due.
- The log lists the vehicles a gametype asked for on each side and each vehicle placed: its side, and whether it
  stands upright.
- GUN GAME (OpenCE's): the plasma rifle and plasma pistol fire. Gun Game took them for empty (their magazine holds no
  rounds: they run on a battery) and gave a new one on every update, so their levels never fired and bots stood
  sliding with them. The bug is OpenCE's own.
- LAN games are seen with a VPN on: a VPN (ProtonVPN, WireGuard, OpenVPN) took the game's broadcasts into its tunnel,
  so no one on the local network saw a hosted game or a search, and a game found was joined at the tunnel's address.
  Broadcasts now also go to each local network's own broadcast address, and a hosted game is joined at this machine's
  local network address when the default route is a VPN's (network.broadcast and network.address, when set, still
  decide alone).
- Split screen no longer crashes when a player other than player 1 is the first to see a map's fog screen (the fog
  layers some maps have in their pits and deep water, as Waterworks' pit): that window's first fog frame wrote past
  the fog's per-player cameras over a renderer pointer (a fault in the next model draw). Three and four player split
  screen wrote past them every frame in a fog screen. The bug came with the Linux port's source (OpenCE), in every
  Arena Evolved release so far.
- CREATE GAME no longer crashes on a maps folder without beavercreek.map: with no map used last it starts on the first
  map that has its file (it chose Battle Creek, listed with or without one); MAP's arrows and CHANGE MAP pass over a
  map whose file is missing.
- Help lines in Arena Evolved's menus name the buttons of the device used last, as the footer does (Ctrl+F on the
  keyboard, Square on a PlayStation pad...); they named the Xbox pad's always.
- HUD STYLE AE draws its panels' shapes and its outlined texts in batches: far fewer draws a frame while alive (the
  frame rate dropped while alive, not while dead).
- HUD STYLE AE, split screen: the scoreboard (BACK held) is drawn at the one-player board's size, centred in each view:
  at 2, 3 and 4 players its last rows end above the score strip (they were drawn over it), and in quarter views it
  clears the motion sensor. The one-player board and HUD STYLE CLASSIC are unchanged.
- ChupathingyCE main merged (past 0.8.0d): Xbox map packs in maps_<pack> folders (<file>@<pack>), the movies through
  FFmpeg on every platform, Grifball and Bomb, game type files.
- NETWORK VERSION 38 (OpenCE build-266 and build-268 merged): plays with OpenCE build-268 both ways, and with
  ChupathingyCE 0.8.0f, 0.8.0g and 0.8.0h both ways (by Delta: Bomb and Grifball too). Arena Evolved 0.3.8 and older, OpenCE
  build-256 and older and ChupathingyCE 0.8.0e and older can no longer join it (they are told to update). SERVER
  BROWSER names OpenCE's game kinds (SWAT, HC CTF...) on LAN and internet games.
- HUD STYLE AE no longer shows a powerup's seconds twice: with POWERUP TIMERS on, its line in the middle of the
  screen stays off, as the AE HUD shows them under the shield bar. Bomb spawns on the ground between the goals on
  Longest and Beaver Creek (on top of the middle structure before: bots could not reach it).
- Test launches (debug.network_test) with the AE menus on open AE's CREATE GAME on their map and start it with the
  gametype and options the classic launch gives (its variant or saved gametype, time limit, score, flags, weapon packs,
  loadout, split screen players); they drew the game's own lobby under AE's menus, which could not start it.
- OpenCE build-271 and build-273 merged (network still 38): a client waits for its inventory before it shows the
  weapons it spawned with (#466), OBS game capture reads the finished picture (#445), and a host refuses a hit a client
  reports through a wall (#474: a modded client could kill a whole team through walls).
- OpenCE's gametypes: SWAT, INFECTION, GUN GAME and HARDCORE (Slayer, CTF, Oddball, King), in an OPENCE folder of
  CHANGE GAMETYPE, with OPENCE MODE and HARDCORE rows in GAME OPTIONS. Bots play SWAT and Gun Game; in Infection
  they hardly fight yet.
- The host's shield effects in network games (SETTINGS > ONLINE > HOSTING > SHIELD EFFECTS, PLAYERS' OWN too); co-op ALLY FRIENDLY FIRE
  (classic menus); breakable scenery shields and tripmines over the network.
- Saved gametypes keep OpenCE's longer options; gametypes from 0.3.8 upgrade unchanged (their display names move
  within the file). 0.3.8 reads a gametype saved by 0.3.9 with default options and without its display name.
- Internet play: ChupathingyCE's own broker, mqtt.chupathingy.org, joins the list (brokers.txt), so Arena Evolved
  meets both OpenCE and ChupathingyCE players on a broker of their own; the game now uses up to 5 brokers.

## 0.3.8-beta - 2026-10-22

- SETTINGS is finished: VIDEO, AUDIO, ONLINE, ACCESSIBILITY and CHAT AND STREAMING work in Arena Evolved's menus (they
  showed COMING SOON) and write the same settings as the classic menus. A display change (DISPLAY MODE, RESOLUTION,
  WINDOW SIZE, RESOLUTION SCALING) asks to keep it and goes back by itself after 10 seconds; SPEAKERS, LANGUAGE and HDR
  DISPLAY ask to restart; MENU SIZE applies at once; PLAYER NAME SIZE and WAYPOINT SIZE take you to their HUD rows.
- Back from a game, the game's own old main menu (and its video) no longer draws under Arena Evolved's main menu.
- Starting a campaign level no longer shows the main menu again for a moment before the level loads.
- Windows: the blue menu cursor shows (it was never loaded there; the system arrow showed).
- The LAN entry and its screen read JOIN LAN GAMES.
- LAN bots: leaving a game lets go of every bot seat, not only the active bots'.
- Logs: a player's controls that cannot be read are logged once ("Profile not found" filled the log every frame), the
  restart question of SETTINGS > SYSTEM > MENUS is logged, and the button prompt family is logged once per pad.

## 0.3.7-beta - 2026-10-22

- ARENA EVOLVED'S OWN MENUS are now the front end (`display.arena_menus`, on by default): MAIN MENU and MULTIPLAYER;
  CREATE GAME (NETWORK: LAN or HOST ONLINE, PRIVACY, BOTS up to 31, MAX PLAYERS 128 by default, OPEN LOBBY);
  CHANGE MAP (map folders, LB/RB filter, LT/RT folder); CHANGE GAMETYPE (the playlist folders, full names, PISTOL
  or BR START); GAME OPTIONS (every gametype rule; leaving with changes asks SAVE, SAVE AS COPY, THIS GAME ONLY or
  KEEP EDITING, and AE's, NHE's and the built-in gametypes are never saved over); the lobby; SERVER BROWSER and LAN
  GAMES; JOIN WITH INVITE; CAMPAIGN; SETTINGS (CONTROLS, VIEW and HUD, each player's own); PROFILES. Controllers,
  mouse, keyboard and touch; UI SCALE 90 to 130 % with long lists scrolling. The classic menus are one row away:
  SETTINGS > SYSTEM > MENUS (online co-op is there for now; the game restarts to switch, and the classic menus' MENUS
  row comes back).
- Games hosted from the new CREATE GAME ran on the default PC rules (the motion sensor showed teammates, no time
  limit, the map's vehicles in place of the gametype's): the gametype's own rules play now.
- Back in the lobby after a game: the menus' background scene runs again (it was black), and START GAME starts the
  next game (it went back to the main menu). The host's START keeps the lobby up until the game runs.
- Item waypoints (TIMERS with waypoints): only the nearest copy of each power item on each side gets one (Death
  Island showed about 16 at once); the power list keeps them all. TRAINING still shows every spawn.
- DualSense: a pad that moves to another port in a game (the pad before it unplugged) no longer gets both ports'
  effects in turn (the lightbar blinked and the triggers clicked at rest until the pad was disconnected).
- The scoreboard header read "FFA AE OCTAGON BR - OCTAGON" for the Octagon drills: now "FFA BR - OCTAGON".
- OpenCE build-251 to build-256 merged: the high-res HUD setting is read once per settings change (a frame-rate fix),
  and co-op on The Pillar of Autumn no longer softlocks.
- GAME OPTIONS (the new game setup screens) gets the rows it lacked: one for each WEAPON PACK (battle rifle, SMG,
  silenced SMG, carbine, silenced assault rifle; a pack that is not installed shows NOT INSTALLED), PACK WEAPONS ON
  MAP, STARTING GRENADES (the game's own, none, 1 to 4), BOTTOMLESS CLIP, a pack's weapons among PRIMARY and
  SECONDARY WEAPON (the pack turns on with its weapon; turning a pack off puts back the stock weapon), and NAME,
  which SAVE AS NEW keeps as typed. Gametypes from before read the stock value in every row.
- AE BTB, big team games for 8 against 8 (seed revision 22, added on the first start; nothing else changes): TEAM
  AE BTB SLAYER (100 kills), TEAM AE BTB CTF (3 captures), TEAM AE BTB KING and TEAM AE BTB ODDBALL (5 minutes held),
  each with a 20-minute time limit and the map's own vehicles, on AE's casual team rules; and TEAM AE BTB HEAVIES, big
  team slayer with rocket warthogs and scorpions for both teams where the map has them (the Xbox maps have no rocket
  warthog: there, their scorpions only). Each has its battle rifle version (TEAM AE BR BTB SLAYER, ...).
- AE COMP 2V2 (seed revision 22): TEAM AE COMP 2V2 SLAYER (25 kills), CTF, KING and ODDBALL, the AE COMP rules (5
  second respawns, 2 frags, no motion sensor, no vehicles, objectives only in line of sight) for two against two, each
  with its battle rifle version.
- MELEE UNZOOMS, a new gametype rule (GAME OPTIONS, seed revision 22: ON in every AE gametype; Halo 1: NHE's and the
  stock gametypes OFF): a melee while zoomed, any weapon at any zoom, takes the zoom off and strikes.
  OFF is Halo CE's own: no melee while zoomed. The host's gametype decides for every player in the game. AE
  gametypes you edited keep their own rules.
- MOUSE AIM ASSIST, a new gametype rule (GAME OPTIONS, seed revision 22: OFF in every AE gametype; Halo 1: NHE's and
  the stock gametypes ALLOWED): OFF turns the mouse's aim assist off for every player of the game, the host's
  BLOCK M+KB setting as before still blocks it too, and controllers keep their aim assist. It rides on OpenCE's own
  aim assist message, so OpenCE and ChupathingyCE players in an AE host's game follow it as well. In a game on one
  machine (split screen, bots) the gametype's rule applies too.
- The gametype lists are sorted into playlist folders: AE FFA, AE 2V2, AE 4V4, AE BTB, AE ACTION SACK (the party and
  weapon modes) and AE PRACTICE, then Halo 1: NHE's (they were AE CASUAL, AE COMP, AE PARTY, AE WEAPONS and AE
  PRACTICE).
- The AE BR gametypes' descriptions said the pistol is second; it is an assault rifle (since the two-frag
  update in 0.3.3), and they now say so.
- A save can hold 200 saved games (was 160), so the 112 seeded gametypes, the built-in ones, your profiles and your own
  gametypes all fit; the new menus list every gametype (they stopped at 128).
- DualSense: TRIGGER STRENGTH re-spaced so each setting feels clearly apart: LIGHT is a true hair trigger on both
  triggers (a tiny tick early in the pull, nothing after it, small kicks), STRONG is two steps firmer than NORMAL but
  never so stiff that a full pull tires. L2's grenade is a hair trigger on every setting (it was strong on all of
  them). The weapon packs have their own feel: the SMG a fast buzz, the silenced SMG a softer, lower buzz, the carbine a crisp click lighter than the
  pistol's, the battle rifle a firm click and a small pulse for each round of its burst.
- New weapon pack: the SILENCED AR (`ae_ar_silenced`, slot 5), the silenced assault rifle of the Halo 2 Mappack
  Collection v2's Gemini (by Dominater01, S3anyboy and GoodFellaPro with Team NHE): the stock assault rifle's numbers,
  silent to the AI, a 2x zoom, and its brass now leaves the ejection port as the stock rifle's does (it came out of
  the silencer's tip). Layouts place it as `ar_silenced`; machines tell the new pack level 5 when joining.
  `make_pack.py` builds packs from a map's tags too (`--source-map`), and reads a map whose stream ends short past its
  tags.
- Weapon packs: the battle rifle, SMG, silenced SMG and carbine melee as every stock weapon does (the stock
  assault rifle's blow, 40 to 60 and 1.4 against a Spartan), two blows (now and then three) from full, like the
  assault rifle; CMT's melee they had took 30 a blow, about five to kill. The packs are rebuilt.
- Silenced SMG retuned: 11 a round up close falling off to 4.4 by about 9.5 units, tight at first
  (0.25 degrees) and wide after 2 s of fire (5.5), a 2x zoom, a magazine of 48 (144 at start and in reserve),
  and the stock assault rifle's smaller impacts on Spartans.
- OpenCE build-250 merged. The visibility readback (which lights and lens flares are visible) is OpenCE's final
  version of PR #433, by hkmodd: nothing is asked of the graphics driver before the frame is shown, and the counts are
  read after it once the GPU says they are done, so NVIDIA's driver and the game no longer take turns. If a driver
  fails to pass the counts back that way, the game reads them the old way from then on, as 0.3.5.1 did (now
  OpenCE's own rule; Arena Evolved's extra line for it is gone).
- The menus show a blue arrow mouse cursor in place of the system's, sized to the window as Halo PC's was, and hidden
  while a controller is used (OpenCE build-250; the art is by pugzysixtyfour).
- Online co-op: the gravity lift on Truth and Reconciliation (and other scripted moves of the players) takes every
  player beyond the first two along too, instead of leaving them in the part of the level that was unloaded (OpenCE
  build-246, by eltanschauung).
- Plays with 0.3.6-beta, OpenCE build-244 to build-250 and ChupathingyCE 0.8 (network version 32).
- DualSense on Windows: the adaptive triggers, rumble, lightbar and player lights go straight to the pad's HID device
  in one whole report, as on Linux, not through SDL's queue, which merged and dropped the triggers' kicks (they felt
  hardly there). Where the HID device cannot be opened, SDL's way as before (`debug.txt` says which and why).
- Modded discs: a machine joins only a host playing the same disc. A machine on the stock maps could join a host on
  Halo 1: NHE or CE+ X and load its own Blood Gulch where the host had the disc's, and the two games went apart; it is
  now refused and told to choose the host's disc in MODDED DISCS. Without a disc nothing changes (stock OpenCE and
  ChupathingyCE machines join as before), nor with a disc that only adds maps of its own.
  With a disc that replaces stock maps, only Arena Evolved 0.3.7 and later machines on the same disc join each other:
  older Arena Evolved, OpenCE and ChupathingyCE machines on that disc are refused.
- Bots in network games: a LAN or internet game you host takes your bots (BOTS in Settings > Game Options, up to 15:
  a big team game's 16 players with you; `bots.count` in config.toml goes up to 127), not only 3 on a LAN. Past your three free controllers they sit on
  machines of the host's own, four to a machine, which every machine sees as a machine of four players, stock
  OpenCE and ChupathingyCE ones too: nothing that the machines send each other changes. Their names start "BOT ",
  so everyone can tell them from people. The lobby keeps a player slot free for someone joining; at the start of a
  team game with an odd number of players one more bot takes it, so the teams are even. In a game under way a
  person who joins a full game takes a bot's place (the bot leaves from the larger team, so the teams stay even),
  and a bot comes back when a person leaves. `debug.network_bots` is on by default; set it to false to turn it off.

## 0.3.6-beta - 2026-10-21

- Bots see an enemy above or below a ledge by the head, and aim at it: they no longer ignore someone shooting down
  at them.
- A gametype a map cannot play (King on a map with no hills) falls back to Slayer with a Slayer score (50 kills, 25
  in 1v1 and 2v2), not the mode's own small score.
- Discord's activity is off by default until Arena Evolved has its own Discord application (it showed ChupathingyCE's).
- The pause menu's box is whole with five buttons (a hosted team game's: CHANGE TEAMS, SETTINGS, END GAME): its
  bottom edge was cut off, the B and A prompts below it.
- Weapon packs: a gametype can name several packs at once (the battle rifle's, and next the SMG's, silenced
  SMG's and carbine's, each its own file in `packs`), all added to the map. Battle rifle gametypes play exactly as
  before. A host refuses a machine that lacks a pack its gametype names, with its own message (older builds just
  cannot join). A test switch (`debug.network_test_pack_map_spawns`, no gametype has it yet) spawns the battle rifle
  on the map's pistol spots and the SMG on its assault rifle spots, each spot keeping its own respawn time.
- Three new weapon packs from CMT's The Silent Cartographer: Evolved (credited in each pack): the SMG, the silenced
  SMG and the covenant carbine, with tuned numbers where they fit one weapon in one hand. The SMG fires 15 rounds
  a second from a 60-round magazine and is lethal up close (about 0.8 s inside 5 units), its damage falling to about
  a third by mid range, where the assault rifle wins (the assault rifle is the better all-round weapon; the SMG suits dual wielding; here it
  stands alone), with smaller blood on a hit. The silenced SMG is quieter, shows no tracer and is silent to bots' hearing. The carbine fires
  single rounds, 5 a second, from an 18-round magazine with a 2x zoom and no bloom: 5 rounds pop a shield and the
  sixth to the head kills, a second at best, the fastest precision kill if every round lands (the battle rifle stays
  the more forgiving rifle). Each has its own ammunition counter, reticle and pickup
  picture, and bots know how to use them. No gametype uses them yet: they are for the H2 demake maps.
- Bots of NORMAL and up zoom the battle rifle and the carbine at range, past half the reach of the rifle's aim assist
  (zoomed, it reaches as many times further as the zoom, as for a player who scopes it). After a hit knocks the zoom
  out they keep firing unzoomed until they zoom again; RECRUIT and EASY never zoom. On the octagon, a NORMAL bot's
  battle rifle hit 12% of its rounds on a player standing still and now hits 36%.
- Map layout files can place a pack weapon by name (`item smg at ...`, also `battle_rifle`, `smg_silenced`,
  `carbine`). Where the gametype has no such pack, the spot spawns the map's own pistol (battle rifle, carbine) or
  assault rifle (the SMGs).
- CONTROLLER CROUCH, a new gametype rule (ARENA OPTIONS): STAYS DOWN keeps a controller player crouched with the
  stick pushed all the way, as the keyboard's crouch key does; HALO CE stands you up at full stick, as Halo did. Every
  Arena Evolved gametype plays STAYS DOWN (a gametype you edited keeps its own rules); the Halo 1: NHE set, stock and
  older custom gametypes and the campaign play HALO CE. The host's gametype decides for everyone in the game, split
  screen players too; older builds play HALO CE. The keyboard's crouch is unchanged.
- Built on ChupathingyCE 0.8.0b (and OpenCE build-244): plays online with ChupathingyCE 0.8 both ways, on its Delta
  wire `chupa-32a` (Arena Evolved's range is that wire alone; 0.3.5's `ae-32a` games still join). Also from it:
  CHANGE TEAMS in a team game's pause menu, the follow camera on long respawn waits, opt-in powerup timers
  (`display.powerup_timers`) and the classic loading screen (`display.loading_screen_min_seconds`), the loading
  screen's sound, the far clip distance (`display.far_clip_distance`), your own font for the high-res text
  (`display.high_res_font`), the first start's choices (an Xbox disc image, a Halo PC CD image or the Custom Edition
  installer, and the disc's movies copied too), Online Games in the Xbox menus' look with pings before joining
  (`network.ping_games`, an opt-in) and co-op games kept off the server browsers, HOSTED GAME STATS
  (`network.report_hosted_games`, Settings > Network Setup), JOIN FROM CLIPBOARD's ASK, config.toml's volumes
  applied while the game runs, big Custom Edition maps (up to 1 GB), and the dedicated server's chat and moderation.
- HUD STYLE CE's high-res HUD is Aerocatia's and Jessie's art from Halo PC Restored (r220), as ChupathingyCE 0.8.0b
  has it; credited in the README.
- Not taken from ChupathingyCE 0.8: its Chupathingy menus (Arena Evolved's PC menus stay), its text chat (Arena
  Evolved's is OpenCE's; `chat.enabled` turns ChupathingyCE's on), its new-install defaults (2x multisampling, 256
  shadows), its own save and logs folders, and its Discord application.
- Sound and music keep playing when the game's window loses focus (OpenCE PR #431, by Wouter Beets: NVIDIA's vsync
  held the sound up for about a second at a time); out of focus the game paces its frames at the display's refresh.
- Mouse clicks in the menus (from ChupathingyCE): a click on the empty part of a button bar no longer presses the
  button that had the focus, list headings no longer take clicks, Online Games sorts by the column head clicked, and
  a click that closes Online Games or the invite question no longer also clicks the menu behind it.
- A copied invite link is no longer offered when the game comes to the front: JOIN FROM CLIPBOARD
  (`network.join_from_clipboard`) is NEVER by default, as in ChupathingyCE; ASK and ALWAYS still work.
- The gametype lists (Create Game's and Edit Gametypes') read the chosen gametype's rules once when it changes,
  not every frame (by thelinkin3000).
- AE SWAT no longer has grenade pickups on the map: it starts with no grenades, and the map's frag and plasma
  grenades stay away too (a gametype rule, MAP GRENADES, on in every other gametype; the weapons and powerups are
  unchanged). A SWAT you edited yourself keeps its own rules.
- The OCTAGON drills (FFA and TEAM) have no grenade pickups on the map either, as they start with none (seed
  revision 20). A drill you edited yourself keeps its own rules.
- Item waypoints line up: the distance, the name, the picture and the timer share one centre, and the stack sits over
  the arrow's own art (the double chevron for an item behind a wall sits left of its sprite's middle).
- A 10 m motion sensor in every Arena Evolved gametype that has the motion sensor on (it was the map's own, 15 m on
  the stock maps): contacts close by are easier to track. Every Arena Evolved 0.3.6 player in the game sees the host's range (older builds and ChupathingyCE draw the map's own);
  enemies' names over their heads keep the map's reach. The NHE gametypes keep the map's range, and a gametype
  you edited yourself keeps its own rules.
- Level flashing fixed: for a frame now and then, a big piece of the level (a cliff, part of a base) was not drawn
  and the fog color showed in its place, most often while looking around near portals between parts of the map. A
  rounding case in the visibility walk lost the parts of the level past one doorway (OpenCE's structure culling,
  since build-242, made the hole bigger).

## 0.3.5.2-beta - 2026-10-20

- A game with bots no longer crashes when a player's body is gone while his shots still fly (the bots' hearing looked
  the shooter up; seen on Windows with 15 bots).
- Windows builds keep their symbols on CI again, so a crash report's addresses can be named.

## 0.3.5.1-beta - 2026-10-20

- Smoother on NVIDIA graphics: the game no longer waits for the graphics driver at every frame to read back which lights
  and lens flares are visible (OpenCE PR #433, by hkmodd: since OpenCE build-242 that wait made NVIDIA's driver and the
  game take turns, about 60 fps on a fast card). Plays with 0.3.5-beta and OpenCE build-244 (network version 32).
- If a graphics driver never passes those results back that way, the game reads them the old way from then on (the
  log says so once), so lens flares and coronas keep working instead of freezing.

## 0.3.5-beta - 2026-10-20

- AE's own text (menus, the community services screen, HUD STYLE AE) no longer shows thin lines and cut letter edges on
  some graphics drivers (Mesa's Intel driver): the glyph atlas is cleared and each glyph keeps an empty border.
- The gametype lists sort by category: AE CASUAL, AE COMP, AE PARTY, AE WEAPONS, AE PRACTICE (with the OCTAGON drills),
  then NHE; each battle rifle version sits right after the gametype it copies (FFA AE SLAYER, then FFA AE BR SLAYER).
- BOTS changed during a split screen game (the pause menu's SETTINGS) takes effect at once: bots join the game in
  progress or leave it, on the teams BOT TEAMS asks for (in a team game the larger team's bot leaves first). LAN
  games keep their bots until the next lobby, as before.
- ENEMY NAME COLOR moved to Network Setup > MULTIPLAYER HUD, under PLAYER NAME SIZE, so the pause menu's SETTINGS
  has it in a game too (it was left on Network Setup's own page, which a game does not show).
- HUD STYLE AE with ENEMY NAME COLOR RED: teammates' names over heads in white, not their team's color (a red
  team's teammates no longer look like enemies).
- TEAM OCTAGON drills (seed revision 17, added on the first start; nothing else changes): TEAM AE OCTAGON BR, TEAM
  AE OCTAGON BR SNIPE, TEAM AE OCTAGON MAG and TEAM AE OCTAGON MAG SNIPE, the OCTAGON drills as team slayer on TEAM AE
  SLAYER's values (50 kills), to warm up with a team: instant respawns, no grenades, infinite ammo and bottomless
  clips.
- OCTAGON, a new group of four 1v1 practice drills for Octagon (seed revision 16, added on the first start; nothing
  else changes): FFA AE OCTAGON BR, FFA AE OCTAGON BR SNIPE, FFA AE OCTAGON MAG and FFA AE OCTAGON MAG SNIPE. Battle
  rifle or magnum in hand, with or without the sniper rifle second; free for all slayer on AE's casual rules with
  instant respawns, no grenades, infinite ammo and bottomless clips (no reloads). Infinite ammo and bottomless clip
  are gametype rules now, the host's for everyone in the game.
- FFA AE SNIPERS (seed revision 15, applied on the first start, the old file backed up): no respawn time growth.
  Stock Snipers adds 5 seconds to your respawn for each death, up to 25 seconds, earned back by kills; no other AE
  gametype has it, TEAM AE SNIPERS included. If you edited the gametype yourself, it keeps your settings.
- AE gametypes (seed revision 14, applied on the first start, the old files backed up): no death bonus and no kill
  penalty in every AE slayer gametype, so nobody speeds up after dying or slows down after a kill; everyone always
  moves at normal speed, as in Halo 1: NHE's slayers. Gametypes you edited or made yourself keep their settings.
- COMMUNITY SERVICES: the first time Arena Evolved opens its PC menus (and once after an upgrade from 0.3.4) a screen
  asks whether to use the game list and match stats the ChupathingyCE team runs at halo.milenko.org. It says what each
  one sends and who runs them, that Arena Evolved cannot vouch for them, and thanks the team. Nothing goes to that
  server until you pick USE COMMUNITY SERVICES; NOT NOW (also B or Esc) keeps everything off. Listing the games you
  host is a separate choice on the same screen, off unless you turn it on. Change it any time in Settings > Network
  Setup > COMMUNITY SERVICES.
- HUD STYLE AE: with MULTIPLAYER GAME TIMER on BOTH and a time limit, the score strip shows the time played too, as a
  small grey line over the time left (as the corner clock has it); nothing changes with the other settings or without
  a time limit.
- Bots play as a team: in a team game one bot sticks with each player as a wingman (near you, fighting beside you,
  not off on its own); NORMAL and up regroup when their team is two or more down, and back off when more enemies are
  close than teammates instead of taking a 1v2; HEROIC and up back out of a crossfire (two or more enemies with a
  line to them) to fight one angle at a time; NORMAL and up hear gunfire near them (NORMAL 15 units .. DEMON 30) and
  go to look where it came from.
- Bots go for power weapons and powerups (rockets, sniper, shotgun, overshield, camouflage) as a player who knows the
  timers does: NORMAL and below only for one lying near, HEROIC and up from farther and timed to its spawn (DEMON from
  anywhere, 15 s early), one bot of a team for each.
- Bots: below DEMON they miss more the farther you are (past about 30 m); they see a camouflaged player only close
  up, react to one later and aim worse at it; and they hold a shotgun's fire until they are close enough for it.
- Bots: a new top level, DEMON (BOT SKILL, SHOOTING SKILL and MOVEMENT SKILL): the LEGENDARY bots of before. LEGENDARY
  now sits halfway between HEROIC and DEMON (it reacts in 0.24 s, sees 48 units over 150 degrees, and tells teammates
  within 20 units what it sees). RECRUIT, EASY, NORMAL and HEROIC are unchanged.
- Bots play more like good players: they pace the pistol's shots at range instead of holding the trigger as its aim
  blooms, zoom the sniper rifle and drop out of zoom when shot, take high ground when they roam, throw a grenade where
  they last saw you, and back off as their shields pop (each from its level up).
- Bots: EASY is a little harder and NORMAL a little easier, so the two are closer together; NORMAL and up step in for
  a blow when you are close and weak, so they melee more; and they no longer see you far above or below where they
  look (they used to turn to a player straight above them at once).
- Bots: they no longer stand still under a power weapon they cannot reach (Beaver Creek's arches, Prisoner), and they
  climb onto a powerup's stand instead of giving it up.
- HUD STYLE AE draws the score strip's numbers with its own digits on every map, also a map with a counter sheet of
  its own (h2_coagulation@ce's numbers were the map's).
- The scoreboard shows the time left whenever the game has a time limit; it showed it only with MATCH CLOCK off.
- Fallout's and Weld's green spawn rings and arrows, always drawn by the maps' own scenery, are no longer drawn
  (TRAINING still shows Arena Evolved's spawn markers).
- Map downloads work the OpenCE way: no question. Join a game on a map you lack and it comes from the host on its own,
  and in the lobby the host's other maps follow, one after another (up to 4096 MB from one host). A card at the top
  of the lobby shows which map is coming, how much of it is here, and when it is ready or did not come. F6 and
  holding BACK no longer do anything for maps. DOWNLOAD MAPS (Settings > Network Setup > Map Sharing) turns it off,
  and INTERNET DOWNLOADS is now on by default.
- A map that did not download says which map, why and what to do, in a dialog whose text stays inside its frame.
- Playing on mouse and keyboard, the HUD's prompts show your key instead of a controller button: "Hold [E] to swap
  for", "Press [E] to enter driver seat", and a map's own button prompts, with the first key bound to that action in
  Controls Setup (a mouse button as a mouse with that button lit, any other key by its name, "Num 7"); the first
  seconds' "Hold BACK for score" shows your scoreboard key too ("Hold [Tab] for score"). In split screen
  only the keyboard's player sees keys. The prompt line also sits a little lower under the ammo bar, so its button or
  weapon picture no longer touches the bar.
- Map layout files: an item's `facing` now turns it on an Xbox map too (an H2 demake), as on a Custom Edition map, and
  `drop` makes an item a little above the floor so it falls and lies on its side, as a dropped weapon does; `float`
  keeps an item at the height given (a powerup over water) instead of putting it on the floor.
- The H2 Demakes Sali Creek's overshield pickup looks like Halo's overshield, not the camouflage sphere: the map gave
  it the camouflage's model (Halo's own tags have the two models' names swapped), and Arena Evolved now gives it the
  stock overshield's model, which the map carries, when the map loads.
- Licence: Arena Evolved is now free software under the GNU Affero General Public License 3.0 or later, as OpenCE is
  from its build-237 (the file LICENSE; README, "Licence"). 0.3.4-beta and earlier stay CC0. The logo keeps its own
  terms, and third-party code, fonts and art their own licences. If you change the game and let others play your
  version over a network, the AGPL asks you to offer them its source.
- Network version 32 (OpenCE build-244): this version plays with OpenCE build-244 and later, and no longer with
  Arena Evolved 0.3.0-beta to 0.3.4-beta, OpenCE build-237 and earlier, or ChupathingyCE; everyone in a game needs
  the same version (the game says which side needs to update).
- From OpenCE build-235 to build-244 (OpenCE's work, merged):
  - Settings > Video > EXTRA EFFECTS, a new screen: the SHIELD GLOW and GLOW INTENSITY rows (moved from GRAPHICS),
    OVERSHIELD (the overshield rises up the body as it charges and stays lit in its own colour; off by default, and
    a gametype with HIT FEEDBACK: ARENA EVOLVED keeps its own effects), HDR and HDR DISPLAY (light past white rolled
    off instead of clipped; on an HDR display, shown brighter than white).
  - Settings > Video > GRAPHICS: MODEL DETAIL (models' most detailed levels by your display, or always the highest).
    FOV AND VIEWMODELS: PASSENGER THIRD PERSON (a third-person view for Warthog and Scorpion passengers in the
    campaign and co-op).
  - 5.1 surround sound (`audio.channels`), a latency meter for network games (`display.latency_meter`), and the
    scoreboard's pings in its colours.
  - Hosts: Player Options' AIM ASSIST (ALLOWED, or BLOCK M+KB to turn off mouse aim assist for everyone in the
    game); Server Setup's co-op options EXTRA PICKUPS and EXTRA VEHICLES; FRIENDLY FIRE in network co-op's pause
    menu, under SETTINGS.
  - A lobby whose players are all on the host's computer (split screen in a LAN or internet lobby) can start.
  - The server browser says when this network's strict NAT stopped a join (turn on UPnP), and how many games it
    hides for being on a newer version.
  - Halo's loading screen is drawn, and the main menu's scene runs behind every menu. Structures cull the triangles
    the camera cannot see; more particles and lights are drawn a frame on desktop.
  - Fixes: no "Running Riot" for a killer with no kills in a row; weapons of one magazine no longer flash a second
    magazine's warnings; Team Oddball's built-in variant starts cleared; no shield damage sound while an overshield
    drains; a client deletes the host's flag or ball without stopping; network co-op respawns after everyone died,
    clients see the AI's grenades, elevators carry their riders alike on every machine; Change Name in a game no
    longer crashes; a unicode string ending in '|' is no longer read past its end; the updater bounds its downloads;
    settings words in any case, and an unreadable config.toml line is skipped (the log says why).
  - OpenCE's PC menus in the language of a Custom Edition's loc.map are not in Arena Evolved yet (it reads Custom
    Edition maps with ChupathingyCE's loader): the menus stay in English.
- Map layouts travel with maps: a host sends its layout for the game's map to every guest that runs Arena Evolved
  (and the layout of any other map a guest downloads from it), so both play the same level: a guest without the file
  no longer keeps the scenery the host's layout removes (Sali Creek's fusion coils, with their collision), and its bots
  get the layout's bot lines. For the match a guest uses the host's layout when it was made for the very map file it
  has, and none when the host has none; its own file is used only with an older host. After you said yes to
  downloading from the host, its layout is kept beside the map (`layouts/<map>.txt`); a file of your own that differs
  is never overwritten: the host's goes beside it as `<map>.host.txt`. OpenCE machines in the game are sent nothing
  and play as before.
- Map layout files can add vehicle spawns: `vehicle <warthog|rocket_warthog|ghost|scorpion|banshee|turret|<palette n>>
  at <x y z> [facing <degrees>] [team red|blue|neutral] [games ...]` places a vehicle the map already carries (its
  vehicle palette's, else its multiplayer warthog, ghost or scorpion) as one of the map's own vehicle spawns: the
  gametype's vehicle set, NHE MODE and the vehicle respawn treat it as the map's. A vehicle the map does not carry
  skips that line (logged).

## 0.3.4-beta - 2026-10-18

- A map layout file can now drop placed scenery and other objects: `remove scenery "protected 54"` or
  `remove object machine <name or number> near <x y z> <radius>` (a H2 remake's fusion coils, trees). Each machine
  that has the layout drops them from its own copy of the level; the game log lists every placed object of a map with
  a layout file, to write the lines from.
- A multiplayer game's pause menu SETTINGS no longer lists CHANGE COLOR: its Spartans are the main menu's pictures, which
  a game map has not got, so the screen showed an empty box. Change your color from the main menu's Settings.
- The pause box's "B =CANCEL" is no longer cut at 1080p.
- The JOIN screen's OPTIONS (a PlayStation controller's) is a pill, as on the controller, no longer the same round chip
  as START; BOTS' SHOOTING and MOVEMENT SKILL say BOT SKILL (they were cut at 1080p); the lobby's controller picker
  help fits at 1080p.
- The teleporters of the H2 Demakes Coagulation, Headlong and Warlock no longer draw their glow inside a black card:
  the maps' own shader blended a picture with no transparency, which Arena Evolved now adds instead when those maps
  load. A map's "incompatible bitmap type" errors are logged once each, not at every frame.
- Modded discs (Halo 1: NHE, CE+ X, Patch Edition and other whole sets of Xbox maps) have a folder of their own:
  `discs/<name>/maps`, chosen on Settings > Game Options > MODDED DISCS (`game.disc`; the old `game.mod` is read into
  it once). Discs already in `mods/` keep working from there and nothing is moved; move them to `discs/` when you
  like. `mods/` is now the folder of OpenCE's client-side mods.
- From OpenCE build-208 to build-213 (OpenCE's work, merged): the high-resolution Xbox HUD is complete, with a
  BUTTON STYLE row on Settings > Video > GRAPHICS (CLASSIC, GEMS or DARK GEMS button faces in the game's prompts);
  turning the high-res HUD or text on or off applies at once, without reloading the map; the campaign pause menu's
  artwork and a scripted timer's digits (which could shrink) are fixed; a voice chat packet that is not one 20 ms
  frame is dropped before it is decoded. With a PlayStation controller the prompts keep Arena Evolved's PlayStation
  icons.
- From ChupathingyCE 0.7.5d: a game listed on the game list that is then made private or given a password gets a
  new invite, so the one shown on the list no longer lets anyone in.
- From OpenCE build-217 to build-232 (OpenCE's work, merged):
  - Text chat in network games: T to everyone, Y to your team (Tab changes who hears it); a controller's D-pad Up
    opens an on-screen keyboard, unless your button layout uses D-pad Up for a control (then use a keyboard). The
    host can turn the chat off or to links only (always off while the game is listed in the server browser) and
    mute players; the scoreboard mutes a player's voice and chat for you. Settings > Network Setup > MULTIPLAYER
    HUD has the chat's settings.
  - Streamer mode (Settings > Network Setup > STREAMER MODE): slurs, swearing and insults in other players' names
    and text, and in maps' and mods' names, shown as stars, at levels you choose.
  - Client-side mods: folders of Halo PC tag files in `mods/` that play over the maps on your machine only, in any
    game: sound packs, translations and models (the main menu's MODS turns them on and off and orders them). Mods'
    bitmaps (texture packs) are not played yet in Arena Evolved, nor models of 44 nodes or more; the log says so.
  - Sound Processing in Settings > Audio Setup: the reverb and the RESAMPLER (sinc, or linear: brighter). The
    earlier `audio.resampling` setting is read into it once.
  - Vehicles nobody drives move on the other machines as they do on the host; the screen fog stays on a vehicle's
    canopy at a wide field of view; map sharing sends at most 20 MB/s from a host (Arena Evolved's LAN rate, 16 MB/s,
    stays under it) and 10 MB/s from another player.
- Arena Evolved is now on OpenCE build-232 and ChupathingyCE 0.7.5d (still network version 29: it plays with the
  same builds as before; OpenCE build-230 and later players use the same chat messages, not yet tried
  between the two builds).

- Controller picker: each split screen JOIN card has a CONTROLLER above its PROFILE: LEFT and RIGHT on that row (a
  player's own UP and DOWN choose it) give the player the next connected controller, swapping with whoever had it;
  ASSIGN (or Y) waits for a button press on the controller to use. A seat's controller flashes its name when pressed,
  and an empty card can seat a free controller with the mouse. The lobby has the same CONTROLLER and ASSIGN on each of
  this PC's players' rows, in split screen and when hosting a LAN or internet game, so four controllers on one PC are
  quick to sort out. The mouse can use all of it. Buttons on the JOIN screen are round chips (no Xbox 360 pad for an
  unknown controller any more), and nothing there is amber.
- HEALTH's REACH is now exact: health has its own wait, 4 s after the last damage that reached it
  (shield hits do not count), then refills fast to the top of the third it is in (about 1.1 s, 1.5 s and 2.6 s a
  third); shields recharge 6 s after a hit and fill in 3 s; a health pack also ends the shields' wait. Before, health
  came back only after full shields, a third every 4 s, on CE's shield timers.
- HEALTH's HALO 3 is now exact: it uses the HALO 2 numbers and rule (shields recharge 5 s
  after the last shield damage and fill in 2 s; health refills in 5 s, starting 10 s after the last damage that reached
  it, shields or none), so HALO 3 plays the same as HALO 2. Before, it was our own rule (health only after full
  shields, slower) and CE's shield timers.
- HUD STYLE AE's hints, objectives and button prompts are bigger: about 0.6 of the classic HUD's campaign text
  instead of 0.36 (24 px at 1080p in a single view, never under 16 px in any view), with button pictures 1.9 times
  the capitals' height (a thumbstick 2.4, so its RIGHT / LEFT label can be read; a weapon 2.6). With no weapon yet
  (the start of a campaign level) the lines wrap at a readable width instead of running across the screen. This
  changes multiplayer's prompts and pickup lines too. Hints and prompts are solid white now, not half see-through.
- The campaign tutorial's blinking health and shield (and a script's blinking motion sensor) blink under HUD STYLE
  AE too, in their own blue; with REDUCE MOTION they dim briefly every 2 s instead.
- A flashing help line flashes again under HUD STYLE AE: it pulses from white to the menus' ice blue.
- HUD STYLE AE no longer shows a second rounds count beside the shield bar on H2CE maps (Coagulation and others):
  their weapon HUD's total rounds number is now held back with the rest of Halo CE's weapon panel.
- Network co-op's SPECTATING line and the hint under it are bigger (24 and 20 px at 1080p, never under 16).
- Split screen's JOIN screen: Multiplayer > SPLIT SCREEN starts with four player cards. The first device to press
  START (ENTER or SPACE on the keyboard) is PLAYER 1, the next PLAYER 2, so keyboard and mouse can be any player, and
  each device stays that player's until it leaves: no more pad taking player 1 because it was plugged in first. Each
  card shows the device, the player's profile (their own LEFT and RIGHT change it; NEW PROFILE makes one), the
  armour, the controls and the device's own leave button. B or ESC leaves a seat; from a device with no seat it goes
  back. CONTINUE goes on to the map and gametype as before. A device that presses START in the lobby joins there.
- Controllers' back buttons: `input.back_right` and `input.back_left` in config.toml make a Switch 2 Pro
  Controller's GR and GL (an Xbox Elite's or DualSense Edge's upper paddles) press the Xbox button they name, so
  they follow your controller layout. Off by default; with Steam running, Steam Input takes the pad instead.
- Map sharing from a controller: in the lobby, holding BACK for a second on any controller does what F6 does
  (yes to the download question, get all of the host's maps, stop), so a guest with no keyboard can say yes.
- Controllers keep their port: a controller plugged in (or found late, as a Switch Pro controller can be on Linux)
  takes the first free port instead of moving every other controller to the next one, which gave a joined player
  another's controller and made the new controller's START fail ("port lobby join failed").
- The battle rifle's rounds rumble on every controller, not only on a DualSense (a short pulse a round).
- Map sharing on a LAN is faster: a host whose players are all on the local network sends maps at up to 16 MB/s
  (`network.map_share_lan_upload`); with a player on the internet it keeps `network.map_share_upload` (1 MB/s).
- A split screen game's setup screen is titled GAME SETUP (LAN and internet games keep SERVER SETUP).
- START GAME on Server Setup after the lobby was left (the game it hosted closed) hosts the game again, with player 1
  on the active profile, instead of doing nothing; if that cannot be done it says so ("The game closed. Press B and
  host the game again.").
- A mod's main menu map (`ui.map`) too big for the game's menu memory (such as the H2 Mappack Collection's) is left
  out with a line in the log, and the stock menus are used, instead of crashing.
- Bots: small fixes from the bots review (a bot no longer counts itself in a search, a weapon pack counts as taken only
  once picked up, a tick's travel state is cleared at its end, the trace level read once a change).
- Bots switch to their plasma pistol when their other weapon runs dry: they took it for always empty, so a bot with an
  empty sniper rifle or pistol kept it, never fired again and backed away from you. A drained plasma weapon now
  counts as empty, so they switch off it too.
- The README is a short front page; the detail is in `docs/` (features, gametypes, LAN, building, upstream).
- The web build runs again: it compiles, links and starts in the browser test (its memory window is set up as
  OpenCE's is; a merge had dropped that).

## 0.3.3-beta - 2026-10-17

- Xbox maps that carry Halo PC's pause menus (Halo OG's Digsite maps) get the Custom Edition maps' pause repair:
  no dead CHANGE SETTINGS row, and the split screen pause menus fit each player's view instead of running off it.
- Custom Edition maps rebuilt with Invader (such as Halo Custom Edition's Gearbox maps rebuilt with a high-resolution
  HUD: Danger Canyon, Death Island, Gephyrophobia, Ice Fields, Infinity, Timberland) load: their empty tag slots no
  longer refuse the whole map.
- HUD digits made for a high-resolution HUD (those maps' own) are drawn at the stock size: the flag waypoint's
  distance, the classic HUD's ammo and grenade counts and the scripted timer were twice their size.
- With HUD STYLE AE, the sniper rifle's zoomed readouts (range and elevation numbers, angle ticks, caption) show
  again.
- A gametype a map cannot play no longer crashes the game or leaves everyone dead: CTF on a map with no CTF flags
  (or King, Oddball, Race without their flags) is played as Slayer and every player is told so as the game starts;
  a mode with no spawns of its own uses all of the map's spawns.
- Map layout files can now place player spawns and netgame flags (`clear spawns`, `remove spawn <n>`, `spawn at ...`,
  `clear flags`, `remove flag <n>`, `flag ctf|hill|ball|race|teleport_from|teleport_to ... at ...`), so a map pack can
  give a map CTF flags it never had.
- Map folders: the maps folder's folders hold maps (one level: `maps/<folder>/`, and `maps_ce/<folder>/` or
  `maps/ce/<folder>/` for Custom Edition maps), and the PC menus' map lists show them as pages of the first row's
  chooser: SINGLEPLAYER, STOCK (the stock maps wherever they are), the folders A-Z by name, MULTIPLAYER (the loose
  maps). A map is still known by its name, so players may keep it in different folders. Map packs can ship their
  folders (after GulchCE's package folders).
- SPLIT SCREEN in the PC menus' Multiplayer (after Create Game's LAN): a game on this PC only, up to four players
  and up to 15 bots. The map, the gametype, Server Setup with only the gametype's options (no name, listing or
  password), then the lobby; another controller's START joins there. The lobby's last rows are BOTS, BOT SKILL, PLAYFIGHT and BOT TEAMS (team games), the same settings as Game
  Options > BOTS, changed live. After a game: the map list, the gametype, the lobby again.
- PLAYFIGHT (Game Options > BOTS, `bots.playfight`), as a training mode: the bots move, strafe and aim
  at you but never shoot, throw a grenade or strike. Aim practice for new players; split screen and LAN bots.
- Map layout files: `games all` puts an item in every gametype (a map that spawns it in CTF only).
- Bots fight like people of their level, after per-level bot tuning (its ratios, our numbers):
  RECRUIT strikes only at point blank after a moment and only now and then, never throws a grenade and is never
  quicker to react than before; EASY throws mostly when it is about
  to die; NORMAL and up go for the head once your shields are down (pistol, sniper rifle, battle rifle), jump in a
  fight now and then, and back off along a path when they are nearly beaten until their shields are back; HEROIC
  and up strike when your back is turned or your shields are low, run in to finish a fight they have won, lead your
  path with a grenade, and tell teammates where they saw you (HEROIC those near, LEGENDARY all; team games).
  LEGENDARY strafes in short quick steps, NORMAL in long ones with a pause now and then. A bot's reaction differs a little from one sighting
  to the next. Same health and damage at every level; PLAYFIGHT bots still never shoot.
- SHOOTING SKILL and MOVEMENT SKILL (Game Options > BOTS; `bots.shooting_skill`, `bots.movement_skill`): the bots'
  aim, reactions, grenades and blows, and their strafing, jumping, crouching and retreating, each at a level of its
  own, or BOT SKILL to follow BOT SKILL (the default). On the BOTS screen only: the split screen lobby keeps its rows for players.
- AE gametypes (seed revision 13, applied on the first start, the old files backed up): every AE gametype but AE
  SWAT starts with two frag grenades (Halo CE's own rule gave one in games of 9 players or more, as with bots);
  the AE BR gametypes have an assault rifle second instead of the pistol.
- HUD STYLE AE no longer shows Halo CE's name of the player under your reticle (the names over heads stay).
- Bots play every mode, not only Slayer: CTF, Oddball (its reverse tag and juggernaut too, AE JUGGER's rules
  included), King of the Hill and Race, in split screen games and in LAN games you host (up to 3). As
  bots do, each one weighs the fight against its mode's goal every moment, and keeps its choice a while: in CTF they
  take the enemy flag, bring it home (the carrier keeps running, hurt or not, and strikes what blocks it), return or
  hunt down their own, defend it (a team of three or more keeps a defender) and escort a carrier; in Oddball they go
  for the ball, keep it away from enemies and go after its holder; in King they go to the hill and hold it; in
  Juggernaut they hunt the juggernaut, who breaks away when badly hurt; in Race they run to the next flag. halo.log
  says when a bot takes, captures, drops or returns a flag, holds the ball or enters the hill, with a line a minute.
- Bots use vehicles as a player does (the action button at a seat): a bot takes a free Warthog gun or gun turret near
  its way (or when it is losing a fight near one), the gunner or passenger seat of a Warthog a teammate drives (you
  too: stop and it gets in), and fights from the gun with its own aim. A CTF carrier rides home as a passenger, never
  as a gunner (a trigger pull drops the flag). It gets out when the vehicle is wrecked or flipped, has had no driver a
  while, has stood still a while, or when its goal is near or the vehicle takes it away from it. No Banshee or
  Scorpion. Bots driving a Warthog or Ghost is a test only, off by default (`debug.bot_vehicles` 2). halo.log says
  when a bot takes or leaves a seat and why, with a line a minute.
- Bots get around better: they take teleporters on their way when that is shorter than walking (Blood Gulch's,
  Chill Out's, Sidewinder's, Wizard's, Derelict's, Rat Race's ...; not straight back through the pad they came out
  at), and a hurt bot that is not fighting goes by a health pack in reach when the gametype's HEALTH does not bring
  all of it back (HALO CE, REACH), never to one that was taken.
- Map layout files gain two lines for the bots: `bot jump at <x y z> to <x y z>` (a jump the bots' walkable
  surfaces miss: a gap, a ledge) and `bot avoid <x y z> radius <r>` (a place they keep out of). `debug.bot_hints`
  logs player 1's jumps and crouches as ready lines; halo.log's `bots: travel` line lists where bots stood stuck.

## 0.3.2-beta - 2026-10-16

A testing pre-release on top of 0.3.1-beta: the "Halo 2 for CE" maps' bullets hurt again, CALLOUTS no longer tell spawn
times a gametype hides, a wider DualSense TRIGGER STRENGTH (LIGHT a hair trigger), HUD AREA moves the AE panels too, the
map layout engine (no layout ships yet), and fixes from two reviews. Same network version as 0.3.0-beta and 0.3.1-beta
(29): they play together online. Linux 64-bit is the build played and tested; Windows is built by the same checks but
was not test-played.

- Network: with `network.legacy_table_fetch` off (the default), no signed legacy table is taken from another machine
  or read from the cache either, so Arena Evolved keeps its own network numbers as the README says.
- The lobby's map, when it is a Custom Edition map whose file is gone, falls back to another map instead of stopping
  on the damaged-disc screen.
- Bots: a LAN bot is taken out of the lobby only while its slot is still its own; aim delay history and crouch timers
  start fresh each game.
- Delta Stats with bots: the host that has the bots never records the game; a player who joins a LAN game with bots
  and has turned on joined-game reports still reports it (a joining PC cannot tell the bots from people).
- HUD: overshield and shield-trail state per player for up to 128 players; CE's messages lay out round the AE panels
  only while they are drawn; a map picture added while the game runs shows within a few seconds.
- Going back to 0.2.5-beta or older: a gametype with OTHER PLAYERS ON RADAR at ENEMIES shows no players on the
  motion sensor there (older builds do not know the value); online it plays as the host's.

- SHUFFLE puts an odd one out on red or blue at random (it was always red).
- PC lobby with LAN bots: the host's last person pressing B (to change the map or gametype) left an empty lobby that
  could not be backed out of, because the bots counted as that PC's players. Now that B leaves as the last player's
  does: the bots go too and you are back on Server Setup.
- HUD AREA (16:9 or 4:3 on a wider screen) now moves HUD STYLE AE's ammo, grenade, shield and health panels in with
  the rest of the HUD; they stayed in the screen's outer corners.
- Item waypoint labels no longer fade under a motion sensor that is not drawn (a gametype without one, after split
  screen goes to one view), or over an empty shield corner; they do fade under the score strip's top lines
  ("YOU ARE THE JUGGERNAUT", the lead).
- A guest in a CTF game sees the right return countdown for a flag dropped while all its players were dead.
- DualSense TRIGGER STRENGTH spread wider: LIGHT is now a hair trigger (every hold or click a short light break at
  the top of the pull, nothing deeper in it; kicks, buzzes and the motors at three quarters), STRONG is clearly stronger (every effect x1.75 and at least a step up, so the
  needler and plasma rifle change too; the motors x1.4). NORMAL is as before.
- Custom Edition maps whose bullets are faster than 32-bit floats can carry ("Halo 2 for CE"'s H2 Blood Gulch: the
  battle rifle, the SMG) now hurt: their projectiles' speeds are scaled into range as the map loads (the bullets had
  turned to NaN on their first tick; OpenCE and ChupathingyCE have the same bug).
- Map layout files (map pack): a map's netgame items and vehicles changed as it loads, without touching the map file
  (the engine part; no layout ships yet).
- CALLOUTS' item calls follow the gametype's TIMERS: none when TIMERS is OFF or LINE OF SIGHT (unless TRAINING is on),
  so a personal setting no longer tells you spawn times the host's gametype hides. The match clock's calls stay.
- HUD STYLE AE draws with fewer lookups a frame (the HUD's sheets once a map, settings once a change).

## 0.3.1-beta - 2026-10-16

A testing pre-release on top of 0.3.0-beta: cleaner high-res text (laid out by its own font, same-width digits), a
straight carnage report, NHE SOUND FILTER, SHUFFLE in the PC lobby, bots that crouch in fights, map pictures in the Xbox
menus too, and fixes. Same network version as 0.3.0-beta (29): they play together online. Linux 64-bit is the build
played and tested; Windows is built by the same checks but was not test-played.

- Bots at NORMAL and above crouch now and then while they fight, briefly and not again for a while (RECRUIT and EASY never do).
- NHE SOUND FILTER (on NHE OPTIONS, your own setting): mutes the plasma pistol's charging hum and the needler's shots, as Halo 1:
  NHE's maps did, on any map, for you only.
- The carnage report's numbers are right-aligned under their headers (Score, Kills, Assists, Deaths), so ones sit under
  ones and tens under tens.
- High-res text is laid out by its own font's letter widths, with same-width digits: a narrow letter no longer sits in the
  middle of a wider space, so columns of names and places start in a straight line and numbers line up digit for digit.
- SHUFFLE in the PC lobby (the host, team games): one press deals the people onto the two teams at random, evenly; bots
  then even the teams up as they always do. With bots set to play AGAINST the people it does nothing.
- The map list has a WARM-UP group: Octagon and Octagon B (when installed) come first after the stock maps, marked
  [WARM-UP] and described as warm-up maps for Slayer.
- SHOW COORDINATES (Video Setup > GRAPHICS), for map makers: your X Y Z and facing on the performance line.
- LEFT and RIGHT STICK DEADZONE (Settings > Gamepads): 5% to 40% for every controller; DEFAULT keeps the game's 27%.
- ANISOTROPIC FILTERING (Video Setup > GRAPHICS): 2x to 16x sharpens floors and walls seen at a grazing angle; off
  (the default) keeps the Xbox's look.
- ZOOM BLUR (Video Setup > FOV AND VIEWMODELS): off takes away the blur a zoomed scope puts at the screen's edges.
- The carnage report marks a player who left before the end: their Place reads Quit (the in-game scoreboard's word)
  in place of a last place, so a quitter ranked last is not mistaken for a player with someone else's numbers.
- The map pack's pictures show in the Xbox menus too (map select and lobby), as they do in the PC menus.

## 0.3.0-beta - 2026-10-16

A big testing pre-release: the score strip and the AE HUD, PS5 DualSense triggers and lightbar, bots in split screen and LAN games,
AE JUGGERNAUT with its own rules, the AE BR gametypes, the PC menus' own pictures and map pictures, built on
ChupathingyCE 0.7.5b and OpenCE build-206 (network version 29: it does not play online with 0.2.5-beta and older).
Linux 64-bit is the build played and tested; Windows is built by the same checks but was not test-played.

- From ChupathingyCE 0.7.5c/d: Delta Stats send players' names, never a hardware ID (an OpenCE player only their name,
  team, score, place, kills and deaths); a game listed under every network version shows once in Online Games; a
  refused Custom Edition map says why on screen, not the damaged disc text, and the tag cache refusal names the room
  needed and free; a private game's invite is never on the game list, as a password game's was not.
- **PS5 DualSense: adaptive triggers, lightbar and player lights.** R2 has a profile for each weapon, from the
  weapon's own numbers, in two families: human weapons are mechanical (clicks and kicks, a pulse on the heavy left
  rumble motor at each shot), Covenant weapons are energy (resistance building with heat or charge, a pulse on the
  light right motor). The pistol clicks and snaps, the assault rifle pulses with each round, the sniper rifle is
  firm with a heavy thump, the shotgun and rockets kick hard (the shotgun's pump clicks at its own animation's
  moment), the battle rifle buzzes through each burst, the plasma rifle stiffens with heat, the plasma pistol
  hums in R2 as it charges (faster once fully charged), pulses at each shot and kicks hard as an overcharged shot
  goes (with a green flash of the lightbar), the needler buzzes. Every weapon's state comes first: free while it is drawn, then a small settle as it
  is ready; free while reloading, with a bump as the magazine (or each shell) goes in; a dry click at the bottom of
  the pull when empty; locked with a venting throb while overheated, until the game lets it fire. Vehicles: the
  seat that fires the vehicle's gun (the ghost's driver, the warthog's gunner) feels that gun; other seats nothing.
  L2 is the grenades: a light hold, firmer as one winds up, a jolt as it leaves the hand (frag left motor, plasma
  right), a dry click with none; light while zoomed. It buzzes while an overshield charges or as you become the
  juggernaut. The rumble pulses follow the profile's VIBRATION setting and go into the game's own rumble. The
  lightbar takes your colour (your team's in a team game; with black armour it shows your shield, as LIGHTBAR SHIELD),
  pulses while an overshield charges, breathes while you have one, dims with active camouflage and flashes white as
  you become the juggernaut; LIGHTBAR SHIELD shows your shield instead (a full shield deep navy, then the HUD meter's own colours,
  dimmer as it drains, blinking when low, the empty colour when down, a white flash on the pop with COMBAT FEEDBACK
  ARENA EVOLVED, the overshield's layer colours, dark while dead). The player lights show your split-screen number.
  Settings > Gamepads: ADAPTIVE TRIGGERS, TRIGGER STRENGTH (LIGHT, NORMAL, STRONG) and LIGHTBAR (OFF, PLAYER COLOR,
  SHIELD), on by default; no other controller is touched, and the triggers and lightbar are given back when you
  quit. Windows and macOS through SDL; on Linux the effects are written to the pad's hidraw device, which Steam's
  udev rules open, or the new `70-ae-dualsense.rules` in the Linux downloads (README: "DualSense on Linux without
  Steam"). Bluetooth works; the DualSense Edge is handled but not yet tried on hardware. Over Bluetooth the pad gets
  one report every 50 ms at most (changes in between are merged), and on Linux the rumble travels in the same
  reports as the effects instead of a second stream from the kernel, so a busy Bluetooth radio (earbuds connected
  too) no longer drops the pad mid-fight; a pad that drops anyway gets its effects back when it reconnects.
- **Bots** in split screen Slayer games (from OpenCE pull request #367 by
  BirchWoodGod, the offline bots of the Halo CE for PS Vita port, an OpenCE
  derivative, contributed under CC0): Settings > Game Options > BOTS sets
  how many (up to 15), how well they play (EASY to LEGENDARY) and their
  teams (EVEN, or AGAINST YOU). They join the lobby of a split screen game
  (MENUS > XBOX, then Multiplayer > Split Screen) and, **up to 3, a LAN game
  you host** (Create Game > LAN: they take your PC's free controller slots,
  one always kept free, and give a slot up to a person who presses START;
  players joining from other PCs, OpenCE's and ChupathingyCE's too, play
  against them), Slayer and Team Slayer only (Arena Evolved's Slayer
  gametypes too), never in an internet game. They fight, pick up better
  weapons and keep the battle rifle. Off by default; Delta Stats records no
  game with bots. A skill set in the middle of a game applies at once.
  **RECRUIT**, below EASY, is for new and young players: it reacts late, its
  aim never quite settles, it barely strafes, rarely melees, throws no
  grenades and roams more than it hunts you. RECRUIT and EASY also aim where
  their target was a moment ago, so strafing makes them miss, and EASY
  melees less often.
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

- **AE JUGGERNAUT has its own rules**: to 15; the juggernaut has a full overshield (it climbs back after a hit
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
- **Motion sensor, enemies only**: OTHER PLAYERS ON RADAR gains ENEMIES (never your teammates), now on the Arena
  Evolved and AE BR team gametypes with the motion sensor (seed revision 12; ones you edited are kept as they are).
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
- With HUD STYLE AE, the **AE panels** take the place of CE's ammo, grenade, shield and health panels and the
  motion sensor, in their places and at their sizes: the reserve over CE's own rounds (the battery and heat bar for
  plasma weapons, a tank bar for the flamethrower, a vehicle seat's heat bars), both grenade types with the armed
  one white, the shield bar with the overshield's two levels, eight health cells, OVERSHIELD and CAMO rows with the
  time left, and a flat motion sensor with the half-range ring, bearing lines and a wedge that shows your field of
  view (narrower when zoomed). CE's contacts, warnings and sounds are its own; CLASSIC is CE's HUD as before.
  On Custom Edition maps with their own HUD, the AE panels replace that map's HUD as well: to see a map's own HUD,
  play with HUD STYLE CLASSIC.
  The panels are drawn in the score strip's style (its soft dark under-stroke, dark tracks under bars and boxes,
  glowing digits) with smooth edges; the ammo group is CE's own width; the top corners sit the same distance from
  the screen's left and right edges, and CE's messages line up under the ammo group. A low or down shield blinks
  between red and dim red; the shield bar fills from its right end and drains toward it, as CE's does, a hit's lost part held in white for half a second then fading (CE's trail), its overshield layers in the classic HUD's colours (pink, then green over it). In a vehicle seat the
  Scorpion shows its cannon only. The motion sensor draws CE's own contacts (its glowing dots, their trail and its
  sweep) on the AE disc.
- With HUD STYLE AE, **CE's messages and hints** (pick-ups, "hold X to ...", vehicle prompts) take the strip's text
  style: white, at the strip's size, small button icons, lined up under the ammo group and wrapped at twice its width.
- **Steady waypoints**: the item waypoints and their labels no longer shake with the camera (a tank's recoil, a
  nearby explosion).
- **The carnage report's B asks first**: "LEAVE THE GAME?" over the report (A leaves, B stays); only player 1's
  buttons count there. Custom Edition maps get the button icons there that they were missing.
- **PC menus' own pictures**: the Multiplayer screen's and the Edit Gametype screen's PLACE HOLDER pictures are
  replaced with Arena Evolved's own, each centred and sized alike; the map pictures in the PC menus come from a
  `previews` folder beside the maps (the map packs bring them). The menus' text icons (the carnage report's "=quit"
  and "=pick game") are sized to the text and centred on it.
- **COMBAT FEEDBACK ARENA EVOLVED: a stronger shield pop**: it lasts 0.8 s, the body stays white longer and its
  light reaches further, so the pop reads at a distance.
- **Gametype lists**: all free for all gametypes first (Arena Evolved, AE BR, AE COMP), then all team ones, then
  NHE's, TRAINING and your own.
- **Fixed:** with active camouflage, the first-person weapon went into a nearby wall (it was depth-tested against the
  world; OpenCE's and ChupathingyCE's code too).
- **Fixed:** with HUD STYLE AE the ammo group could vanish after an active camouflage pickup (it followed a piece of
  CE's weapon HUD drawn far above the view); it keeps its place.
- **Fixed:** leaving the host's screen after a game (B) could leave only Halo's background, needing a restart; it opens
  the main menu (from OpenCE pull request #374 by chestahh).
- **Fixed:** a game set up without a map picked opens on the first map you have when Derelict's file is not there (it
  stopped with "couldn't find map 'carousel'" and the dirty disc screen).
- **Fixed:** a Custom Edition map's weapon that gives off a little heat a round (the "Halo 2 for CE" maps' battle
  rifle) showed an empty heat bar under its rounds; the bar shows only while the weapon has heat.

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
  default). The label is a centred stack, in a layout with Arena
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
    sounds, linear bitmaps), memory leak fixes, and a latent-bug pass.
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
- AE MENUS (`display.arena_menus`, off by default; `HALO_ARENA_MENUS`): a modern-style front end of our own, kept apart
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
- HALO 2 health uses HALO 2 timers: shields recharge 5 s after the last
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
