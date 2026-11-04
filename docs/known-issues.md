# Known issues and limits in 0.7.5-beta

Arena Evolved is in beta. Releases since 0.7.2 are testing builds: each is checked on Windows and Linux, but not every
feature is play-tested as a whole. This list is what we already know. Report anything else in Issues (a bug report with your
`debug.txt`). More answers are in the [FAQ](faq.md).

## Playing together

- **Network version 41.** Arena Evolved 0.7.5 joins ChupathingyCE and OpenCE build-334 and later. Older OpenCE builds and
  Arena Evolved 0.7.4 and older cannot join 0.7.5 games.
- **Vehicles and small arms.** Under VEHICLES DESTRUCTIBLE only explosives (grenades, rockets, tank shells, fuel rods) damage
  the stock vehicles; small arms and vehicle guns do not yet. Fixed in the next release.
- **Beta weapons.** The NEEDLER, BOLT SNIPER, COMBAT RIFLE, SIDEKICK, HJ MAGNUM, BRUTE SHOT and TERRITORIES are betas, and the
  ENERGY SWORD is an alpha: numbers and feel will change. Every weapon pack is rebuilt for 0.7.5, so both sides of a game
  need this release's packs.
- **Arena Evolved's extras are for Arena Evolved players.** A player on another build can join Halo CE's own gametypes, not
  gametypes that need weapon packs. A player whose packs are off is turned away, and the players in the game read why in chat.
- **MCC alone has no original Xbox maps.** With MCC and no disc image you cannot join a game on an original Xbox map (for example one
  an OpenCE player hosts). Add a disc image to join those. Without MCC you cannot join a game on MCC's stock maps.
- **Workshop maps are played on a player's own game.** Dedicated servers do not serve them. Everyone in the game needs MCC and
  the same Workshop item. A new subscription shows after you restart Arena Evolved.
- **The SERVER BROWSER lists only games whose hosts list them**, and it needs COMMUNITY SERVICES. If you declined, it is empty
  until you turn COMMUNITY SERVICES on in SETTINGS > ONLINE.
- **VPNs.** Hosting online with a VPN on does not work: turn the VPN off to host.
- **Linux firewalls** block LAN games silently until you open UDP 5150, 5151, 5160 and TCP 5153.
- **Dedicated server:** Linux only (x64 and arm64). The program is still called `chupathingyce-server`.
- **Host leaves = the game ends.** There is no host hand-off yet. A team whose players all leave waits one minute in Arena Evolved's
  team gametypes (WAIT FOR PLAYERS), then the game ends.

## Installing and first start

- **Windows warns about an unknown publisher.** `halo.exe` and `ffmpeg.exe` are not code-signed. See the [FAQ](faq.md).
- **Steam Deck:** if the first-start window does not open in Game Mode, start the game once from Desktop Mode.
- **macOS and Android builds are untested by us.** A Mac needs a disc image (MCC does not run on a Mac). From 0.7.5 the
  Mac download carries the weapon packs, voices and sounds; on 0.7.4 get the extras zip, see the FAQ (Mac).
- **A map you don't have can end in "There's a problem with the disc you're using"** instead of a clear message, for
  community maps the host keeps loose in its `maps` folder. Ask the host to pick a map you have.
- **A Steam library with unusual characters in its path** (Windows): fixed for Windows 10 version 1903 and later in 0.7.3. On
  older Windows, move the library to a plain path.
- **MCC in an unusual place:** the first-start window's Choose MCC folder button is the way to point the game at it. Later, set
  `game.mcc_path` in `config.toml`; Arena Evolved's menus have no folder picker.
- **Windows firewall:** if you answered no when Windows asked, allow the game under Windows Security > Firewall > Allow an app.

## Menus

- **The PC (classic) menus hide Arena Evolved's screens.** The pause menu, CREATE GAME's live lobby, MY PARTY, CHANGE MAP's MCC and
  WORKSHOP folders, JOIN BY ADDRESS, KEY BINDINGS and RESTART GAME are only in Arena Evolved's own menus.
- **Pause menu at 1080p:** a GAME OPTIONS value can be cut at UI SCALE 100%.
- **Scoreboard:** with split screen it can sit off centre, the title can be cut, and the PING column is crowded at 2 or 4 players.
- **Some ultrawide screens:** weapon models can look stretched at the edges; the battle rifle and carbine can look fish-eyed at a
  high field of view.

## Bots

- **Bots can get stuck** on some maps and spots: the Weld ramp, parts of Frigid Keep's trench, and at times while they fight.
- **Bot drivers can hit walls and rocks**, and bots in a tank's side seats do not get off at good moments yet.
- Bots do not use every mode's tricks yet (for example ONE IN THE CHAMBER, FRAG NATION, MELEE ONLY). They play, but plainly.

## Maps, vehicles and weapons

- **Flamethrower flame looks dim on Halo PC / Custom Edition maps** next to MCC's maps (same data, a draw difference we are still tracing).
  On original Xbox maps the unfinished flamethrower is a rocket launcher, as in Halo CE.
- **A rolled Warthog can fall through Blood Gulch** at one spot in the far south. Rare; we have no reliable repeat.
- **Hugeass:** the jet's aiming square is stronger than the map maker meant, and some canyon walls can flicker. Tell us where.
- **Cache copy:** a community Xbox map rebuilt under the same name can keep playing the old copy until `save/z` in your save folder is cleared.
  This matters to map makers.
- **Search and Destroy / Bomb** has one bomb site (the defenders' flag). A second site, hold-to-plant and a "bomb planted" voice line are planned.
- **Grifball** is still a work in progress.
- **Voice callouts** for the fuel rod gun and the flamethrower are silent (their timers and waypoints work).

## Medals and HUD

- **Medals:** with MCC installed, medal pictures come from MCC (31 more are Arena Evolved's own). Without MCC a medal shows as its name.
- **Vehicle seat panel and sniper zoom HUD** in third person are plain; the seat panel in HUD STYLE AE rolls open seats into a line.
- **Name labels** can overlap in crowded fights.

## Controllers

- **Steam Input** hides a controller's back paddles from the game; set them in Steam, or start the game outside Steam.
- **DualSense triggers and lightbar on Linux** need the included udev rule (`70-ae-dualsense.rules`) once.
- **Switch 2 Pro controller:** works over USB; HD rumble is not done.

## Not here yet (planned)

- Merge with the newest OpenCE builds (network version 41).
- A team balancer, hosted playlists with map voting, a players and controllers screen with drop-in split screen, an easier
  invite in the lobby and pause menu, and host moderation tools.
- Custom campaign mods from the Workshop (under study, not promised).
- A second bomb site, more vehicles and more bot work.
- Packs and starts for more weapons.
