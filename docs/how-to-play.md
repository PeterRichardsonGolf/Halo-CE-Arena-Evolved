# How to play Halo CE: Arena Evolved

From download to your first game, and what to do when something goes wrong. If you want to host a server for other
people, read [Host a server](host-a-server.md) after this page.

## On this page

- [Before you start](#before-you-start)
- [Install and update](#install-and-update)
- [First start](#first-start)
- [Your profile](#your-profile)
- [Menus basics](#menus-basics)
- [Join a game](#join-a-game)
- [Host a game](#host-a-game)
- [Bots](#bots)
- [Split screen](#split-screen)
- [Maps](#maps)
- [Game types and GAME OPTIONS](#game-types-and-game-options)
- [Controls](#controls)
- [Settings overview](#settings-overview)
- [Recording and screenshots](#recording-and-screenshots)
- [Where files are](#where-files-are)
- [Troubleshooting](#troubleshooting)
- [Reporting a problem](#reporting-a-problem)

## Before you start

Arena Evolved (AE) is a fan port of Halo: Combat Evolved for PC. It has none of the game's maps, sounds or
art. You need your own copy of the game, in one of these two forms.

**Option 1: Halo: The Master Chief Collection (MCC) on Steam.** This is the easiest. You need the game and both of
its Halo CE pieces of content:

- **Halo: Combat Evolved Anniversary**
- **MCC - H1: Multiplayer**

To check that you have both:

1. Open Steam and go to your Library.
2. Right-click **Halo: The Master Chief Collection** and choose **Properties**.
3. Open **DLC**. Both items above must be there and ticked (installed).
4. If a file is missing or damaged, go to **Installed Files** in the same Properties window and choose **Verify
   integrity of game files**. Steam repairs the install.

AE finds the Steam install by itself and plays MCC's own Halo CE multiplayer maps from MCC's files. It also plays the
Halo CE maps of the Steam Workshop items you subscribe to. Nothing is copied or converted.

**Option 2: a disc image of your own Xbox disc.** A `.iso` or `.xiso` file you made from your own original Xbox disc of
Halo: Combat Evolved, from any region. Not needed for MCC's maps. It adds crossplay with players who don't have MCC
(such as OpenCE players), the original Xbox maps, and the community's maps and mods made for the original Xbox version.
The first start copies the `maps` folder out of it (about 2 GB).

Use only a copy of the game you own. Arena Evolved never provides, links to or helps find game files.

You can have both. Custom Edition maps also need `bitmaps.map`, `sounds.map` and `loc.map` from Halo PC or Custom
Edition. With MCC installed, AE reads them from MCC's `custom_edition` folder.

**System.**

| | |
| --- | --- |
| Windows | Windows 10 or 11, 64-bit, with OpenGL 4.5. Nothing else to install. |
| Linux | x86-64. The normal download needs glibc 2.35 or later (Ubuntu 22.04 and later, Mint, Zorin OS, Pop!_OS, Debian 12). Arch and other rolling distributions use their own download. |
| macOS | macOS 13 or later, on Apple silicon. This build is upstream's port and is not tested by the AE team. |
| Graphics | OpenGL 4.5 (macOS: OpenGL 4.1) |

## Install and update

**Download.** Take the zip for your system from the Releases page of this repository:

| Download | For |
| --- | --- |
| `arena-evolved-<version>-windows64.zip` | Windows 10 or 11, 64-bit |
| `arena-evolved-<version>-linux64.zip` | Most Linux distributions (SDL3 included) |
| `arena-evolved-<version>-linux64-arch.zip` | Arch and other rolling distributions (uses the system's `sdl3`) |
| `arena-evolved-<version>-macos.tar` | macOS 13 or later, Apple silicon |

**Install.** There is no installer.

1. Unpack the zip into a folder you own, for example `Documents\Arena Evolved` on Windows or a folder in your home
   folder on Linux. Do not unpack it into a folder Windows protects (such as Program Files): the game writes its settings,
   screenshots and log next to its program.
2. Start `halo.exe` (Windows) or `halo` (Linux).

**With Quiver.** You can also get AE with the Quiver launcher (see the link in the README). Quiver downloads and
updates it for you.

**Updates.** On Windows and Linux, the game looks for a new release when it starts and asks first:

> A new version of Halo CE: Arena Evolved is out (new; this is old). Do you want to update? The game will close and
> start the new version.

The window is titled "Halo CE: Arena Evolved: new version". The buttons are **Yes**, **No** and **Do not ask again**. Yes downloads the new version, checks its signature and
installs it. Your saves and settings stay as they are. Updates are signed: the game refuses an update that is not.

- **Do not ask again** asks once more ("Stop asking about new versions?"). Yes turns the check off.
- To turn it back on: **SETTINGS > SYSTEM > UPDATES > AUTO UPDATE**.
- On macOS the game does not update itself. It asks "Do you want to open its download page?" and opens the release
  page.
- The game only talks to GitHub for this, and never updates without asking.

## First start

What you see the first time depends on your copy of the game.

<!-- SHOT: the first-start dialog (no MCC) with its five buttons -->

### MCC found

With Halo: The Master Chief Collection installed on Steam, with both Halo CE pieces of content, the game asks
nothing. The window opens, the menus appear, and MCC's maps are in the map lists. You can go to [Your
profile](#your-profile).

Steam can be on any drive. AE reads Steam's library list to find it.

### MCC found, but a piece is missing

If MCC is there but the Halo CE content is not complete, the game cannot play from it. The first-start window names
what is missing. For example:

> Arena Evolved found Halo: The Master Chief Collection, but it is missing MCC - H1: Multiplayer.
>
> In Steam, open your Library, right-click Halo: The Master Chief Collection, choose Properties, then DLC, and tick it.
> Let Steam download it, then choose Installed Files > Verify integrity of game files, and start Arena Evolved again.

If all the files are there but Steam is still installing or updating MCC, the window says: "Wait until Steam is done,
then start Arena Evolved again."

1. Open Steam > Library > right-click **Halo: The Master Chief Collection** > **Properties** > **DLC**.
2. Make sure **Halo: Combat Evolved Anniversary** and **MCC - H1: Multiplayer** are both installed.
3. Then **Installed Files** > **Verify integrity of game files**.
4. Start AE again.

The window has these buttons.

| Button | What it does |
| --- | --- |
| **Open the help page** | Opens this page in your browser. |
| **Choose MCC folder** | Lets you pick the MCC folder yourself, if Steam keeps it somewhere AE did not look. Pick the folder that holds `halo1` (in Steam's library: `steamapps\common\Halo The Master Chief Collection`). If the folder is not MCC, or lacks a piece, the game says so. A folder that is whole is kept in `game.mcc_path`. |
| **Use a disc image** | Goes on to the disc image question (below). |
| **Quit** | Closes the game. |

### No MCC

Without MCC, the first-start window says: "Arena Evolved plays Halo with the maps from your Xbox disc. How do you want
to add them?" It also tells you that, with no disc, you can install MCC from Steam (with both pieces of Halo CE
content) and start again.

| Button | What it does |
| --- | --- |
| **Xbox disc image** | Opens a file picker. Choose your Xbox disc image (`.iso` or `.xiso`). The game copies its `maps` folder next to the game and shows the progress. This takes a few minutes and about 2 GB. |
| **Halo PC or Custom Edition** | Optional. Adds the maps from your Halo PC CD image (`.iso`) or the Custom Edition installer (`halocesetup_en_1.00.exe`). |
| **Make the folders** | Makes the empty map folders so you can put the files in yourself. |
| **Choose MCC folder** | Use this if you have MCC but AE did not find it. Pick the folder that holds `halo1`. |
| **Quit** | Closes the game. |

**Tips for the disc image.**

- If a disc image is already in the game's folder (a name ending in `.iso` or `.xiso`), the game names it and uses it
  without a file picker.
- On Linux the file picker needs the desktop portal or `zenity`. If neither is installed, the game tells you to put the
  image in the game's folder and start again, or install `zenity`.
- The copy goes to a folder called `maps.partial` and is renamed `maps` when it is whole. If it stops early, the
  game asks again at the next start.
- The maps need free disk space of about 2 GB. The save folder also keeps a cache of up to about 800 MB.

### The "map folders" question

If you have older folders of Halo PC maps (`maps/ce`, `md_maps`), the game may offer to move them into `maps_ce` and
`maps_md`. **Move them** moves each folder whole, and nothing is copied or deleted. **Leave them** keeps them where
they are, and their maps still play.

### Windows asks about the firewall

The first time you start the game on Windows, Windows asks if the game may use the network. Allow it, on private
networks at least. Without it, LAN and online games do not work.

## Your profile

Your profile keeps your name, your controls and your view and HUD settings. AE keeps one profile for each player on
the PC, so split screen players each have their own.

- **PROFILES** on the main menu lists them. **NEW PROFILE** makes one.
- A name is up to 11 characters and cannot use every character. If a name is taken, the game says "That name is
  already used".
- A profile in use by a player cannot be changed ("A player is playing with it").
- Guest players (Guest 2, 3, 4) share player 1's profile. A guest's settings are not saved.
- Settings marked as yours (CONTROLS, VIEW, HUD) are saved in your profile. Settings for the whole PC (VIDEO, AUDIO,
  ONLINE and so on) are saved once, in `config.toml`. The top of each SETTINGS page says which kind it is.

## Menus basics

AE has menus of its own. They work with a controller, a mouse or a keyboard.

| You see | Controller | Keyboard and mouse |
| --- | --- | --- |
| Select | A | Enter or click |
| Back | B | Esc |
| Change a value | Left / Right | Left / Right, or click |
| Pages and tabs | LB / RB | Q / E |
| Extra actions shown at the bottom of the screen | X, Y | F, R |

The help line at the bottom of each screen says what the focused row does and shows the buttons it uses.

The main menu has **MULTIPLAYER**, **CAMPAIGN**, **SETTINGS**, **PROFILES** and **QUIT**.

**MULTIPLAYER** has **CREATE GAME**, **JOIN WITH INVITE**, **SERVER BROWSER** and **JOIN LAN GAMES**. On the right is
**MY PARTY**: the players on this PC. Split screen has no row of its own: other players join in MY PARTY (see [Split
screen](#split-screen)).

If you prefer the older PC-version menus, **SETTINGS > SYSTEM > MENUS** switches to them. The game restarts. The
older menus stay as they were, and some AE screens (such as JOIN BY ADDRESS and the AE pause menu) are only in the AE
menus.

Language and menu size are in **SETTINGS > ACCESSIBILITY**.

## Join a game

You can join a game in four ways. All of them start from **MULTIPLAYER**.

### SERVER BROWSER

1. Choose **SERVER BROWSER**.
2. Pick a game from the list. Use the tabs to show **ALL GAMES** or only **DEDICATED** servers.
3. Press A. The lobby opens, read-only, until the host starts the game.

The browser needs **SETTINGS > ONLINE > INTERNET PLAY** and **COMMUNITY SERVICES** on. A new copy asks about
COMMUNITY SERVICES at the start, before AE's main menu: a screen titled COMMUNITY SERVICES with the buttons **USE
COMMUNITY SERVICES** and **NOT NOW**, and a **LIST HOSTED GAMES** switch.
**SERVER PINGS** (off by default) shows each game's delay, but every host then sees your address.

### JOIN LAN GAMES

For games on the same network.

1. Choose **JOIN LAN GAMES**. The list fills as the game finds hosts.
2. Pick one and press A.

If nothing is found for 10 seconds on Linux and a firewall is on, the screen says:

> No games found. On Linux a firewall can hide LAN games: see README > Playing over LAN (UDP 5150-5151, 5160; TCP 5153)

See [Host a server](host-a-server.md#firewall-and-ports) for the commands.

### JOIN WITH INVITE

A host who plays online can copy an invite link. It looks like `halo://join/...`.

1. Copy the link.
2. Choose **JOIN WITH INVITE** and paste it. Or set **SETTINGS > ONLINE > JOIN FROM CLIPBOARD** to use a link on your
   clipboard when the game comes to the front: **ASK** asks "Join the game from your clipboard?" (YES or NO), **ALWAYS**
   joins at once, **NEVER** does nothing. The default is NEVER.

If the link is not valid, the game says "That is not an invite". If the host is on an older game version, it says the
host needs to update.

### JOIN BY ADDRESS

Use this to join a friend directly by IP address or name, for example over a private network.

1. Press **Y** on **JOIN LAN GAMES** or on **SERVER BROWSER**. (On the server browser, JOIN WITH INVITE moves to LB.)
2. Type the host's address: an IP address (`192.168.1.20`), a computer's name or a private network's name. Add
   `:port` after it when the host uses another port. Without a port, the game uses port **5150**, the game's own port (the one LAN games use). The host's
   firewall must let UDP 5150 and 5160 in (see [Host a server](host-a-server.md#firewall-and-ports)).
3. Press A. The game says "JOINING..." and waits up to 10 seconds.

An address is up to 64 characters and may have letters, digits, dots, dashes and one colon before the port. The port
is a number from 1 to 65535. On a controller's keyboard, the `'` key types a colon. Y pastes the text on your clipboard.

**RECENT** lists the 10 newest hosts you joined this way, with the host's name and when. A joins one again, Y forgets it.

If the host does not answer, the screen says:

> Can't reach (address). Check the address, that the host's game is open, and that its firewall lets Arena Evolved
> through.

### Maps you lack

When you join a game, the game gives you the map the host is on. If you do not have it, the host sends it as you join,
and a card at the top of the lobby shows the progress. This is on by default (**SETTINGS > ONLINE > MAPS > DOWNLOAD
MAPS**). Over the internet it also needs the host's **INTERNET SHARING** and your **INTERNET DOWNLOADS**. See
[Maps](#maps). The game's own maps are never sent.

## Host a game

1. **MULTIPLAYER > CREATE GAME**. The lobby opens at once.
2. Pick a map and a game type (see below).
3. **NETWORK** picks who can join. A new lobby is **OFFLINE**.

| NETWORK | Who can join |
| --- | --- |
| **OFFLINE** | Only this PC. The pause menu really pauses the game. |
| **LAN** | Players on your local network. |
| **HOST ONLINE** | Players on the internet, with your invite. |

4. With HOST ONLINE, **PUBLIC** lists your game in everyone's server browser. Off means invite only. Listing also
   needs **SETTINGS > ONLINE > LIST HOSTED GAMES** (off until you turn it on). **MAX PLAYERS**
   counts bots too.
5. **INVITE** copies your game's invite link (`halo://join/...`) so you can paste it to friends. Anyone with it can
   join.
6. Set **BOTS** if you want them, then choose **START GAME**. The game starts after a short countdown (5 seconds).

Players you invite appear in the lobby. Extra players on your own PC press START on their controllers in MY PARTY
(see [Split screen](#split-screen)).

**A VPN blocks hosting.** If a VPN is on, the INVITE row's help says:

> A VPN is on (name): players can't reach your game through it. Turn the VPN off to host online.

Turn the VPN off while you host online. For LAN games, AE finds games with a VPN on.

If you do not want to be seen, leave NETWORK on OFFLINE or LAN. Hosted games are not listed on any website, and stats
and joined-game reports are off, unless you turn them on in **SETTINGS > ONLINE > PRIVACY**. Choosing **USE COMMUNITY
SERVICES** at the first start turns on SHARE GAME STATS and SHARE HOSTED STATS as well, and LIST HOSTED GAMES if its
switch on that screen is ON.

Ports, UPnP and the firewall are in [Host a server](host-a-server.md).

## Bots

Bots are computer players. They play every mode, in six levels: **RECRUIT, EASY, NORMAL, HEROIC, LEGENDARY, DEMON**.

- In **CREATE GAME**, the **BOTS** row sets how many (OFF, 1 to 31, then 47, 63, 95 and 127) and **BOT SKILL** sets the level.
  Bots fill any LAN or internet game you host. Past 31 the game runs, but the bots get lost and stuck on big maps and
  swamp the fights: 31 or fewer play best. `bots.count` in `config.toml` takes any number up to 127.
- **BOT SETUP** has **SHOOTING SKILL** and **MOVEMENT SKILL** (each can follow BOT SKILL or have its own level),
  **PLAYFIGHT** (bots move and aim but never shoot, for practice) and **BOT TEAMS**.
- **SHUFFLE TEAMS** mixes the teams.
- The pause menu's **BOTS** row changes the count and skill during a game, if you are the host.
- Bots have "BOT " before their names. A bot steps aside when a player joins a full game.

## Split screen

Up to four people can play on one PC, in a game on this PC, on a LAN or online.

1. In the lobby or on the main menu, find **MY PARTY**.
2. Each extra player presses **START** on their controller. The first device to press START is player 1.
3. Each player picks a profile and a controller. **NEW PROFILE** makes one. B leaves the seat.

Details:

- The CONTROLLER row on a player's card lists every device. Taking one another player has swaps the two. **ASSIGN** (or
  Y on your own pad) waits for a press on the controller you want.
- A controller that is unplugged keeps its player's seat until you plug it back in.
- Each player has their own pause menu and settings.
- A joined machine shows the rows of the lobby. The host's player 1 owns the bot rows and the lobby's own rows.
- If several people share one set of speakers, set **SOUND PRESET: SPLIT SCREEN** in the gametype's GAME OPTIONS (the SOUND topic).

## Maps

Open **CHANGE MAP** in the lobby (or from the pause menu as host). Maps are in folders.

| Folder | What is in it |
| --- | --- |
| **STOCK OG** | The original Xbox maps, from your disc image. |
| **STOCK GEARBOX** | The Custom Edition (Gearbox) maps. |
| **STOCK MCC** | MCC's own Halo CE multiplayer maps, from your MCC install. Shown as `[MCC]` in the lobby. |
| **WORKSHOP** | The maps of Steam Workshop items you subscribe to. It opens into collections (PRO EDITION, SOCIAL MAP PACK, NHE MAP PACK, 2V2 MAPS, CE+ BEYOND) and **OTHER**. |
| Map packs and your own folders | Community maps keep their own folders and pages. |
| **FAVOURITES**, **RECENT** | Maps you starred and maps you played lately. |

A Workshop map shows its Workshop picture and a **WORKSHOP** badge. A map that Steam is still updating is dimmed and
says "STEAM IS UPDATING IT". Search and filters are at the top of the screen.

**Workshop maps.**

1. In Steam, subscribe to a Halo: The Master Chief Collection Workshop item that has Halo CE maps.
2. Let Steam finish downloading it.
3. **Restart AE.** The game finds Workshop maps once, as it starts. A new subscription shows after a restart.

On Windows, a Steam library in a folder with characters outside the system's code page may show no Workshop maps.
Workshop maps are played on a host's own game: dedicated servers do not serve them yet. Every machine in a game on an MCC map
needs MCC.

**Joining a game on an MCC map.** If you lack it, the game tells you what is needed:

- For a stock MCC map: "This game plays (map) from Halo: MCC's own files. Install Halo: The Master Chief Collection in
  Steam, or choose its folder in Settings > MAP FILES, to join."
- For a Workshop map: "This game plays (map) from Halo: MCC Workshop item (number). Subscribe to it in Steam (with Halo:
  The Master Chief Collection installed) to join."

Players without MCC cannot join a game on MCC's stock maps.

**MCC's files can be turned off.** **SETTINGS > SYSTEM > FILES > MCC FILES** has ASK, ON (always) and OFF (never).
The default is ON. Some messages call this "Settings > MAP FILES": that is the same setting on the profile menu of the
older PC menus, which can also pick the MCC folder. In AE's menus, set the MCC folder in `config.toml`
(`game.mcc_path`).

**Community maps and packs.** Put Xbox-format community maps in the `maps` folder (restart after adding or removing
maps). Halo PC and Custom Edition maps go in `maps_ce`, HaloMD maps in `maps_md` and your Halo PC disc's maps in
`maps_pc`. Folders you add inside these show as map folders. A map is known by its file name, so keep the same files on
every player's PC.

**Modded discs** (such as Halo 1: NHE) go in `discs/<name>/maps`. **SETTINGS > SYSTEM > FILES > MODDED DISC** picks
one. The game restarts to play it.

**Map layouts.** A map can have a layout file (`maps/layouts/<map>.txt`) that moves, adds or removes items, vehicles,
spawns and flags. The host sends its layout to the players.

## Game types and GAME OPTIONS

A game type is a set of rules. AE's own are built in: no file in your save, and the same rules for everyone.

Open **CHANGE GAMETYPE** in the lobby. The folders are:

| Folder | What is in it |
| --- | --- |
| **AE FFA** | Free for all: slayer, king, oddball, COMP slayer, snipers, rockets. |
| **AE 2V2** | Two against two, including the COMP 2V2 set. |
| **AE 4V4** | Team slayer, CTF, king, oddball, and the team COMP set. |
| **AE BTB** | Big team, 8 against 8, with heavy vehicles. |
| **AE OBJECTIVE** | The round modes: Search and Destroy and Bomb. |
| **AE ACTION SACK** | Fun modes: phantoms, juggernaut, race, SWAT, Frag Nation, One in the Chamber, Melee Only, Needles, Snowball Fight, Grifball. |
| **AE PRACTICE** | Practice, training and the OCTAGON warm-up drills. |
| **CLASSIC COMP** | Halo 1: NHE's gametypes. |
| **OPENCE** | OpenCE's own gametypes (Hardcore, SWAT, Infection, Gun Game). |
| **STOCK** | Halo CE's own gametypes. |

Above them are **RECENT**, **FAVOURITES** and **MY GAMETYPES** (your own).

Every AE gametype has a **START** switch: **PISTOL**, **BR** or **EVO**. It is the weapon you spawn with. AE BTB and AE
COMP start on EVO.

**GAME OPTIONS** changes the rules of the gametype you picked. Rules are in topics. The chips at the top (ALL,
CHANGED, CLASSIC, COMMUNITY, AE, CLASSIC COMP) filter the rows; LB and RB change the chip, LT and RT move between groups. A
badge on each row says where the rule comes from. **OPTIONS** on a built-in gametype saves your own copy, which
appears in MY GAMETYPES.

Some rules, and their defaults on AE gametypes:

| Rule | What it does |
| --- | --- |
| **FALL DAMAGE** | Off in AE gametypes: no damage from landing. |
| **HEALTH** | Style names HALO CE, REACH, HALO 2 and HALO 3. Sets how shields and health recharge. |
| **NO SPREAD** | OFF, FIRST SHOT or FULL: whether shots go exactly where you aim. |
| **PRE-GAME COUNTDOWN** | A 3-2-1 before the game starts. |
| **COMBAT FEEDBACK** | HALO CE or ARENA EVOLVED: how a shield pop and a recharge look. |
| **SPAWNS** | CLASSIC or MODERN. MODERN keeps you away from the fight. Default MODERN on AE gametypes, CLASSIC on the Halo CE and NHE ones. |
| **TIMERS**, **TRAINING** | Power item timers, waypoints and spawn markers. |
| **SILENT MOVEMENT**, **SILENT WEAPONS** | For split screen on shared speakers. |

The rules are the host's. A gametype that a map cannot play falls back to Slayer, and the game says so.

**Weapon packs.** The battle rifle, EVO RIFLE, DMR, SMG, carbine and suppressed weapons are weapon packs in the `packs`
folder. **SETTINGS > SYSTEM > FILES > WEAPON PACKS** turns packs off on this PC. With packs off, you cannot join a
game that uses them.

## Controls

### Controller

**SETTINGS > CONTROLS > BUTTON LAYOUT** picks a layout. Each player has their own. The layouts are DEFAULT, SWAP
TRIGGERS, SWAP A L TRIGGER, SWAP B L TRIGGER, SWAP B R STICK, BUMPER JUMPER, FISHSTICKS, HERO, RECON, THE DUKE,
ANNIVERSARY, ZOOM & SHOOT, BUMP & JUMP, GREEN FINGERS, one more MCC layout, and **CUSTOM**.

**CUSTOM LAYOUT** lists the ten actions with their buttons.

1. Choose an action with A. The game waits 8 seconds for a press.
2. Press the button you want. The action already on it takes the old one, so no button is lost or doubled.
3. START and BACK cannot move, and the D-pad cannot take an action. Esc, START or BACK cancels.
4. **RESET** or Y goes back to DEFAULT. When you leave, you can **KEEP** or **UNDO**.

Your layout is saved in your profile (`ae.toml`).

Other pad rows: **STICK LAYOUT** (DEFAULT, SOUTHPAW, LEGACY, LEGACY SOUTHPAW), **LEFT/RIGHT STICK DEADZONE**,
**VIBRATION**, **GYRO AIMING**, and for a PlayStation DualSense **ADAPTIVE TRIGGERS**, **TRIGGER STRENGTH** and
**LIGHTBAR**. Nintendo Switch Pro and Switch 2 Pro controllers work. Buttons on the back of a pad can press any Xbox
button (**BACK PADDLES**).

### Keyboard and mouse

**SETTINGS > CONTROLS > KEYBOARD > KEY BINDINGS** gives two keys to every action. LB and RB change the page there.

Default keys:

| Action | Keys |
| --- | --- |
| Move | W A S D |
| Jump | Space |
| Crouch | Left Ctrl, C (Linux and Windows) |
| Fire | Left mouse button |
| Throw grenade | Right mouse button, G |
| Melee | F, mouse button 4 |
| Reload | R |
| Zoom | Z, middle mouse button |
| Switch weapon | Mouse wheel, 1 |
| Switch grenade | X |
| Action (pick up, enter a vehicle) | E |
| Flashlight | Q |
| Mark (enemy, item or place) | B |
| Scoreboard | Tab |
| Pause menu | Esc |
| Text chat (everyone / team) | T / Y |
| Push to talk | V |
| Screenshot | F10 |
| Record video | F9 |
| Fullscreen or window | F11 |

**ZOOM** can be TOGGLE, HOLD or BOTH. **WHEEL ZOOM** steps the zoom level with the wheel. **MOUSE STYLE** CLASSIC or
MCC uses MCC's sensitivity numbers.

## Settings overview

**SETTINGS** has these pages.

| Page | What is in it |
| --- | --- |
| **CONTROLS** | Your buttons, sticks, mouse and keys. Every player has their own. |
| **VIDEO** | Screen, graphics and frame rate. One set for this PC. |
| **VIEW** | What you see in first person, and your shields. Every player has their own. |
| **AUDIO** | Volume, speakers and voice. One set for this PC. |
| **HUD** | Your HUD, names, scoreboard and training aids. Every player has their own. |
| **CHAT AND STREAMING** | Text chat and the streamer word filter. |
| **ONLINE** | Internet play, map downloads and what you share. |
| **ACCESSIBILITY** | Menu size, reduced motion, help and language. |
| **SYSTEM** | The menus, files, recording and this version. |

Every setting has a line of help. A dot shows a changed row, UNDO puts the opened values back.

A few that people look for:

| Setting | Where | Default | What it does |
| --- | --- | --- | --- |
| DISPLAY MODE | VIDEO > DISPLAY | borderless (windowed on macOS) | FULLSCREEN, BORDERLESS or WINDOWED. A change asks you to keep it, else it goes back in 10 seconds. |
| FRAME RATE LIMIT | VIDEO > DISPLAY | AUTO | With V-SYNC off, the most frames a second. AUTO is twice your screen's refresh rate. Type a number from 30 to 1000 or pick from the list. |
| PERFORMANCE, PING | VIDEO > PERFORMANCE | | Frame rate overlay; PING shows your delay to the host. |
| HUD STYLE | HUD | | The AE HUD or Halo CE's. |
| HUD AREA, COMPACT HUD | HUD | | Where the HUD sits and how big. |
| MULTIPLAYER GAME TIMER | HUD | | The match clock: OFF, REMAINING, ELAPSED or BOTH. |
| CALLOUTS | AUDIO > CALLOUTS | OFF | Spoken calls for power items: OFF, ITEMS or ITEMS + CLOCK. CALLOUT DETAIL: MINIMAL, STANDARD (default), VERBOSE. CALLOUT VOICE picks the voice. |
| INTERNET PLAY | ONLINE > CONNECTION | ON | OFF keeps you to LAN and this PC. |
| UPNP PORT FORWARDING | ONLINE > CONNECTION | ON | Opens the game's port on your router. |
| DOWNLOAD MAPS | ONLINE > MAPS | ON | Get a map you do not have from the host. |
| AUTO UPDATE | SYSTEM > UPDATES | ON | Look for a new version at start and ask first. |
| MCC FILES | SYSTEM > FILES | ON | Use MCC's files. |
| WEAPON PACKS | SYSTEM > FILES | ON | Read the `packs` folder. |
| STREAMER MODE | CHAT AND STREAMING | OFF | Star out blocked words. |
| MENUS | SYSTEM > MENUS | ARENA EVOLVED | AE's menus or the older PC menus. |

Every setting is also in `config.toml` next to the game, written at the first start with a note on each. The
`[section]` and key names there are shown in the notes of the settings.

## Recording and screenshots

- **F10** saves a PNG screenshot (every split-screen view). They go to the `screenshots` folder beside `config.toml`.
- **F9** starts and stops a video recording, with the game's sound. They go to `recordings` beside `config.toml`.
  A red dot shows at the top right while it records (it is not in the video).
- The Windows download includes `ffmpeg.exe`, which recording needs. If you delete it, recording is off, and
  screenshots still work.
- On Linux and macOS, recording uses your own `ffmpeg`: put it next to the game or on your `PATH`. Or set `capture.ffmpeg_path`
  in `config.toml`. Without it, F9 shows "RECORDING NEEDS FFMPEG" and does nothing.
- The screenshot key can be changed in **KEY BINDINGS**. F9 cannot.

## Where files are

| What | Where |
| --- | --- |
| Settings | `config.toml`, next to the game's program |
| Maps | `maps` folder next to the game (Xbox maps); `maps_ce`, `maps_md`, `maps_pc` for Halo PC maps |
| Weapon packs | `packs` next to the game |
| Screenshots, recordings | `screenshots` and `recordings` next to `config.toml` |
| Saves, profiles, cache | The save folder: on Linux `~/.local/share/halo-linux`; on Windows `%APPDATA%\halo`; on macOS a `saves` folder in the game's folder in Application Support. You can change it with `paths.saves` in `config.toml`. |
| Cache | `cache` in the save folder. It is made again as needed, so it is safe to delete. |
| Shader cache | `shader_cache` in the save folder. Safe to delete. |
| Log | `debug.txt`, in the game's data folder (the folder with `maps`, next to the game unless `paths.data` says otherwise). `paths.logs` moves it. |

## Troubleshooting

Find your message or what you see in the left column.

| What you see | What to do |
| --- | --- |
| A first-start window asks for a disc image, but you have MCC | MCC was not found or is not complete. Check **Halo: Combat Evolved Anniversary** and **MCC - H1: Multiplayer** under Steam > Library > right-click **Halo: The Master Chief Collection** > **Properties** > **DLC**. Then **Installed Files** > **Verify integrity of game files**. Or choose **Choose MCC folder** and pick the folder that holds `halo1`. |
| "MCC FOUND, BUT ITS CUSTOM EDITION FILES ARE MISSING. CHECK THE INSTALL IN STEAM." | The MCC DLC is not installed or damaged. Steps above. |
| "THAT FOLDER HAS NO CUSTOM EDITION FILES. PICK THE MCC FOLDER, THE ONE THAT HOLDS halo1." | You picked the wrong folder. Pick the one that has a `halo1` folder inside. |
| "THAT FOLDER'S PATH IS TOO LONG TO KEEP (UNDER 256 CHARACTERS)." | Use a shorter path, or move the Steam library. |
| "THIS BUILD HAS NO FOLDER DIALOG: SET game.mcc_path." or "THE FOLDER DIALOG COULD NOT OPEN. SET game.mcc_path IN config.toml INSTEAD." | Open `config.toml`, find `mcc_path` in the `[game]` section and put the folder in quotes. |
| "That file is not an Xbox disc image, a Halo PC CD image or the Custom Edition installer." | Pick a different file. The game reads Xbox disc images (`.iso`, `.xiso`). |
| "No file dialog could be shown (...)." on Linux | Install `zenity`, or put the disc image in the game's folder and start again. |
| A disc image copy stopped | Start again. The game finds `maps.partial` and asks for the image again. Free up space if the disk is full. |
| The window opens and closes, or stays black | Check `debug.txt` (see [Reporting a problem](#reporting-a-problem)). Your graphics need OpenGL 4.5 (macOS: 4.1). Update your graphics driver. |
| No games in JOIN LAN GAMES on Linux ("No games found. On Linux a firewall can hide LAN games") | Open the ports (UDP 5150-5151, 5160; TCP 5153). See [Host a server](host-a-server.md#firewall-and-ports). |
| No games in JOIN LAN GAMES, any system | Both PCs must be on the same network, not a guest Wi-Fi. Windows must have allowed the game through its firewall. |
| "The host did not answer" or "Can't reach (address). Check the address, that the host's game is open, and that its firewall lets Arena Evolved through." | Check the address, that the host's game is running and not full, and that its firewall allows UDP 5150. |
| "That is not an invite" | The link is damaged or not a `halo://join/...` link. Ask for a new one. |
| "That game can't be joined from this version" | The invite is for a game on a different version. Update. |
| "Host is on version N and needs to update to M or newer" | The host must update. Tell them. |
| "Host is on version N, you're on M" | Update the game or ask the host to. |
| "Internet play is off" | Turn on **SETTINGS > ONLINE > INTERNET PLAY**. |
| "Lobby full" | The game has no seat. Try again later. |
| "No game found on the LAN" | The host closed the lobby, or you are on a different network. |
| "Map not installed: (name)" | You lack the map. Join again with **DOWNLOAD MAPS** on, or install it. |
| "Not hosting a game" | The action only works for the host. |
| "Not while a game is running" | Wait for the game to end. |
| "Could not host the game" | The game could not start its network server. `debug.txt` has the lines from that moment. |
| "Needs Steam Workshop item (number) (subscribe to it in Steam) to join" in a game list | The game plays a Workshop map you lack. Subscribe to that Workshop item in Steam, let it finish, then restart AE. |
| "This game plays (map) from Halo: MCC Workshop item (number). Subscribe to it in Steam (with Halo: The Master Chief Collection installed) to join." | Subscribe to that item in Steam, wait for the download, restart AE. |
| "This game plays (map) from Halo: MCC's own files. Install Halo: The Master Chief Collection in Steam, or choose its folder in Settings > MAP FILES, to join." | You need MCC installed (with both Halo CE pieces of content). |
| "...has MCC's files turned off. Turn them on to join." | Turn on **SETTINGS > SYSTEM > FILES > MCC FILES**. |
| "(file).map of Workshop item N is missing. Check the item in Steam." | In Steam, unsubscribe and subscribe again, or verify the item. |
| "(file).map of Workshop item N can't be read: Steam may be updating it." | Wait for Steam to finish updating it, then try again. |
| "MCC's (file).map is missing. Verify MCC's files in Steam." | Steam > Library > **Halo: The Master Chief Collection** > **Properties** > **Installed Files** > **Verify integrity of game files**. |
| A new Workshop subscription is not in CHANGE MAP | Restart AE. Workshop maps are found once at start. |
| Workshop folder is empty on Windows | A Steam library folder with characters outside the system's code page may not be read. Move the library to a plain path. |
| Others cannot join your online game | Turn any VPN off. Check **UPNP PORT FORWARDING** is on. See [Host a server](host-a-server.md). |
| "A VPN is on (name): players can't reach your game through it. Turn the VPN off to host online." | Turn the VPN off while you host online. |
| Windows asked about the firewall and you said no | Open Windows Security > Firewall > Allow an app, and allow the game on private networks (public too if Windows calls your network public). |
| A player cannot join because of weapon packs | Their **WEAPON PACKS** setting is off, or they lack the `packs` folder. Turn it on; keep the `packs` folder with the game. |
| A player cannot join a game with SILENT MOVEMENT or SILENT WEAPONS | Only builds 0.2.3 or later can. Tell them to update. |
| Update question keeps asking | Choose **Do not ask again**, or set **SETTINGS > SYSTEM > UPDATES > AUTO UPDATE** off. |
| You turned off the update check and want it back | **SETTINGS > SYSTEM > UPDATES > AUTO UPDATE** ON. Or set `auto = true` in the `[update]` section of `config.toml`. |
| Recording says "RECORDING NEEDS FFMPEG" | Windows: put `ffmpeg.exe` back next to the game. Linux and macOS: install ffmpeg and put it on your `PATH`. |
| Controller does nothing | Check it in the CONTROLLER row of your player card in MY PARTY. A controller used by Steam Input is seen as one virtual controller. On Linux, a DualSense's trigger effects need the udev rule (see below). |
| DualSense triggers or lightbar do nothing on Linux without Steam | Run once from the game's folder: `sudo cp 70-ae-dualsense.rules /etc/udev/rules.d/ && sudo udevadm control --reload`. Then unplug and connect the controller again. |
| Back paddles do nothing | With Steam running, Steam Input takes the controller and the game sees no back buttons. Set them in Steam, or start the game outside Steam. |
| Bots do nothing in a gametype | Bots play every mode, Bomb, Grifball and Search and Destroy too. Check **BOTS** is not OFF and **MAX PLAYERS** has room. |
| The classic menu is showing | **SETTINGS > SYSTEM > MENUS** was set to the PC menus. Set it to ARENA EVOLVED (the classic menus' MENUS screen has an ARENA EVOLVED row too). |
| Nothing you changed in GAME OPTIONS applies | The host's gametype decides for everyone. If you are a player, ask the host. |

## Reporting a problem

If you find a bug:

1. Note the version of the game (shown at the top of the HUD, in the window title and at the start of `debug.txt`).
2. Say what you did, what you expected and what happened.
3. Add the log file `debug.txt` (see [Where files are](#where-files-are)). It is plain text. It holds your computer's graphics driver name and
   file paths, and on Windows the paths include your Windows user name (the save folder is under
   `C:\Users\<name>\AppData\Roaming\halo`). Look through it before you share it.
4. Open an issue in this repository's Issues tab.

Crash reports are off in every AE build: nothing is written to send and nothing is asked.
