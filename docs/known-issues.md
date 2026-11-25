# Known issues and limits in 1.0.0-rc7

Arena Evolved 1.0.0-rc7 is a release candidate on the way to 1.0: more release candidates will follow, with no date for 1.0.
Each release is checked on Windows and Linux (64-bit), but not every feature is play-tested as a whole. This list is what
we already know. Report anything else in Issues (a bug report with your `debug.txt`). More answers are in the [FAQ](faq.md).

## Campaign

- **AI SQUAD is a test feature.** Indoors a squad member can still be held up at an obstacle you jumped; a member stuck where it cannot get to
  you is brought up behind you after a few seconds. CONTINUE from a saved game plays without the squad for now, and so does
  split screen. Their movement, orders and cover get a larger pass in a later version.
- **Workshop campaign maps** whose scripts the game has to switch off say so on the HUD as the mission starts; such a mission
  may not play correctly.

## Playing together

- **Network version 44.** Arena Evolved 1.0.0-rc7 joins OpenCE build-355 to build-387, and Arena Evolved 0.9.0 and later. OpenCE
  builds older than build-355 and Arena Evolved builds older than 0.9.0 cannot join.
- **AE's weapons need the same weapon version.** A game that uses AE's weapons needs the same weapon version on both sides.
  rc5, rc6 and rc7 share one; a player on an older version is told to update. A player on another build can join Halo CE's own
  gametypes, not gametypes that need AE's weapon packs.
- **Medals on a dedicated server.** Medals you earn in a game on a dedicated server may not show on your screen; the post-game
  report still counts them.
- **Rotations over 128 entries.** A dedicated server's rotation holds up to 256 entries from rc7. A server on rc6 or older
  keeps the first 128.
- **Beta weapons.** The NEEDLER, BOLT SNIPER, COMBAT RIFLE, SIDEKICK, HJ MAGNUM, BRUTE SHOT and HEAVY REVOLVER are betas, and
  the ENERGY SWORD is an alpha: numbers and feel will change.
- **MCC alone has no original Xbox maps.** With MCC and no disc image you cannot join a game on an original Xbox map (for
  example one an OpenCE player hosts). Add a disc image to join those. Without MCC's files (an install or a STEAM ACCOUNT
  sign-in) you cannot join a game on MCC's stock maps.
- **A new Workshop subscription** made in Steam shows after you restart Arena Evolved. Maps downloaded by STEAM ACCOUNT are
  listed as soon as the download ends.
- **The SERVER BROWSER lists only games whose hosts list them**, and it needs COMMUNITY SERVICES. If you declined, it is empty
  until you turn COMMUNITY SERVICES on in SETTINGS > ONLINE.
- **VPNs.** Hosting online with a VPN on does not work: turn the VPN off to host.
- **Linux firewalls** block LAN games silently until you open UDP 5150, 5151, 5160 and TCP 5153.
- **Host leaves = the game ends** (outside a dedicated server). There is no host hand-off.

## Installing and first start

- **Platforms.** Windows and Linux (64-bit) are tested with every release. The macOS (Apple silicon) build, the Android build
  (unsigned) and the browser build come from the same code and are not tested by us; they may not work.
- **Windows warns about an unknown publisher.** `halo.exe` and `ffmpeg.exe` are not code-signed. See the [FAQ](faq.md).
- **Steam Deck:** if the first-start window does not open in Game Mode, start the game once from Desktop Mode.
- **A Mac has no MCC.** Use a disc image of your own Xbox disc, or a STEAM ACCOUNT sign-in (also untested on a Mac).
- **STEAM ACCOUNT** has no sign-in on the dedicated server, the web build or the 32-bit builds. A busy Steam keeps the sign-in
  and waits, up to 10 minutes between tries.
- **MCC in an unusual place:** the first-start window's Choose MCC folder button is the way to point the game at it. Later, set
  `game.mcc_path` in `config.toml`.
- **Windows firewall:** if you answered no when Windows asked, allow the game under Windows Security > Firewall > Allow an app.

## Menus

- **The PC (classic) menus hide Arena Evolved's screens.** The pause menu, CREATE GAME's live lobby, MY PARTY, MAPS & MODS,
  AE TOURNAMENT, CHANGE MAP's MCC and WORKSHOP folders, JOIN BY ADDRESS, KEY BINDINGS and RESTART GAME are only in Arena
  Evolved's own menus.
- **Scoreboard:** with split screen it can sit off centre, and the PING column is crowded at 2 or 4 players.
- **Some ultrawide screens:** weapon models can look stretched at the edges at a high field of view.
- **Split screen** campaign and Workshop campaigns play without the AI SQUAD for now (see Campaign).

## Bots

- **Bots can get stuck** on some maps and spots, and at times while they fight.
- **Bots drive on a few maps only.** They take vehicles where a map's route file has routes (MCC's Blood Gulch, Danger Canyon,
  Death Island, Ice Fields, Sidewinder, Timberland, Gephyrophobia, Infinity and the Workshop map Cliffhanger); on other maps
  they stay on foot until their driving is tuned.
- Bots do not use every mode's tricks yet. They play, but plainly.

## Maps and weapons

- **Cache copy:** a community Xbox map rebuilt under the same name can keep playing the old copy until `save/z` in your save
  folder is cleared. This matters to map makers.
- **Search and Destroy / Bomb** has one bomb site (the defenders' flag).
- **Grifball** is still a work in progress.
- **Voice callouts** for the fuel rod gun and the flamethrower are silent (their timers and waypoints work).

## Medals and HUD

- **Medals:** with MCC installed, medal pictures come from MCC (the rest are Arena Evolved's own). Without MCC a medal shows as
  its name.
- **Name labels** can overlap in crowded fights.

## Controllers

- **Steam Input** hides a controller's back paddles from the game while it drives the controller; the rows Steam's layout owns
  are greyed in the menus with the reason. Set them in Steam, or start the game outside Steam.
- **DualSense triggers and lightbar on Linux** need the included udev rule (`70-ae-dualsense.rules`) once. The new Steam
  Controller on Linux needs the system to give the game access to it; a note in the menus says how.
- **Switch 2 Pro controller:** works over USB; HD rumble is not done.
