# Host a server

How to host a game from Halo CE: Arena Evolved for friends on your network or on the internet, how to play with
OpenCE players, and how to run the dedicated server on Linux. If you only want to play, read
[How to play](how-to-play.md) first.

## On this page

- [Three ways to host](#three-ways-to-host)
- [Host from the game](#host-from-the-game)
- [Firewall and ports](#firewall-and-ports)
- [VPNs](#vpns)
- [Play with OpenCE players](#play-with-opence-players)
- [The dedicated server on Linux](#the-dedicated-server-on-linux)
- [Troubleshooting](#troubleshooting)

## Three ways to host

| Way | For | Needs |
| --- | --- | --- |
| **OFFLINE** game | You, and split-screen players on your PC | Nothing. The pause menu really pauses. |
| **LAN** game | Players on your home or LAN-party network | The same network (not a guest Wi-Fi) and open ports (below). |
| **HOST ONLINE** game | Players anywhere on the internet | Internet play on, no VPN, and your router able to open a port (UPnP, below). |
| **Dedicated server** | A game that stays up all the time | A Linux machine. No player sits at it. |

Hosting needs your own copy of Halo, as playing does. Everyone in the game needs the same maps. The game sends a map
the others lack. See [How to play: Maps](how-to-play.md#maps).

## Host from the game

1. Start the game. Open **MULTIPLAYER > CREATE GAME**.
2. Pick a map and a game type.
3. Set **NETWORK** to **LAN** or **HOST ONLINE**. (A new lobby starts **OFFLINE**. The lobby keeps your last choice until
   the game closes. Changing NETWORK opens the lobby again, and the game asks first if other players are in it.)
4. With **HOST ONLINE**:
   - **PUBLIC** lists your game in everyone's server browser. Off means invite only.
   - **INVITE** copies your invite link (`halo://join/...`). Send it to your friends. Anyone with the link can join.
5. Set **MAX PLAYERS** (bots count) and **BOTS** if you want them.
6. Choose **START GAME**.

Players on the same network can also find your LAN game in **JOIN LAN GAMES**. Friends who know your address can use
**JOIN BY ADDRESS** (see [How to play](how-to-play.md#join-by-address)).

Settings that matter, in **SETTINGS > ONLINE**:

| Setting | Default | What it does |
| --- | --- | --- |
| **INTERNET PLAY** | ON | Join and host games over the internet. OFF keeps you to LAN and this PC. |
| **COMMUNITY SERVICES** | asked at the first start | The server browser and invites go through a community service. OFF: nothing goes there. |
| **UPNP PORT FORWARDING** | ON | Opens the game's port on your router so players can join you. |
| **LIST HOSTED GAMES** | OFF | Games you host show in the server browser. A game is listed only with this ON and **PUBLIC** ON in CREATE GAME. Choosing USE COMMUNITY SERVICES at the first start can turn it on. <!-- OWNER: is PUBLIC alone enough to show a game in the OpenCE/master list (server_master.c), or is LIST HOSTED GAMES needed there too? The code gates only the community browser's listing (browser.c) on LIST HOSTED GAMES. --> |
| **DOWNLOAD MAPS** | ON | Get a map you lack from the host. |
| **INTERNET SHARING** | OFF | Hosting: also send your maps to players who are not on your local network. |
| **INTERNET DOWNLOADS** | ON | Also get maps from hosts over the internet. |
| **DOWNLOAD LIMIT** | 4 GB | The most map data your PC takes from one host, from 256 MB to 64 GB. |
| **HOSTED GAME STATS** | ON | Send your hosted games' scores to the stats site. It only matters for a listed game, so with LIST HOSTED GAMES off nothing is sent. |

Map sharing speed: up to 16 MB/s to players on your local network, and 1 MB/s when a player is on the internet, so
your upload stays free for the game. A player takes at most 4 GB (DOWNLOAD LIMIT) from one host. The game's own 13 multiplayer maps are
never sent.

## Firewall and ports

**LAN games** use these ports on every computer, host or not:

| Port | Protocol | What for |
| --- | --- | --- |
| 5150 | UDP | The game |
| 5151 | UDP | Finding games and their replies |
| 5160 | UDP | The game's own traffic |
| 5153 | TCP | Map sharing |

**Internet games** go through one UDP port, and it is the only port internet players need. Players reach each other
through it (the game finds each side's public address and sends to it from both sides), and maps are sent through it
too. By default the game picks the port. `network.tunnel_port` in `config.toml` sets a fixed one (0 picks one).
**UPNP PORT FORWARDING** asks your router to open it. If your router does not support UPnP, or it is turned off, set a
fixed `network.tunnel_port` and forward that UDP port to your PC by hand.

**Windows** asks the first time the game starts. Allow it on private networks at least, and on public ones too if
Windows calls your network public. If you said no by mistake: Windows Security > Firewall & network protection >
Allow an app through firewall, and allow the game.

**Linux** has no such question. If a firewall is on, it silently blocks LAN games: nobody sees your game, or you see
nobody's. **JOIN LAN GAMES** says so when it finds nothing for 10 seconds and finds ufw or firewalld on. Open the
ports once on each Linux computer:

- Ubuntu, Zorin OS, Linux Mint, Pop!_OS (ufw):

  ```sh
  sudo ufw allow 5150:5151/udp
  sudo ufw allow 5160/udp
  sudo ufw allow 5153/tcp
  ```

  Or in the Firewall Configuration app (Gufw): Rules, +, Advanced, protocol UDP, port `5150:5151`; then again for
  `5160`; then protocol TCP, port `5153`.
- Fedora and others with firewalld:

  ```sh
  sudo firewall-cmd --permanent --add-port=5150-5151/udp --add-port=5160/udp --add-port=5153/tcp
  sudo firewall-cmd --reload
  ```

- Not sure whether a firewall is on? `sudo ufw status` says `inactive` when ufw is off. `systemctl is-active firewalld`
  says `inactive` when firewalld is off.

Turning the firewall off also works. Opening the ports keeps it on for everything else.

**Same network.** Every computer must be on the same router or Wi-Fi, and not a guest Wi-Fi, which keeps devices apart.

## VPNs

- **Hosting online with a VPN on does not work.** The VPN answers for your PC, so players on the internet cannot reach
  you. The CREATE GAME screen says "A VPN is on (name): players can't reach your game through it. Turn the VPN off to
  host online." Turn the VPN off while you host.
- **LAN games** are found with a VPN on: the game searches each of your local networks, and joins a hosted game at
  your local network address when the default route is a VPN's.
- **Private networks (such as Tailscale).** Friends can use **JOIN BY ADDRESS** with your private-network name or
  address. Without a port, the game joins at UDP port 5150, so 5150 and 5160 must be reachable on the host's private
  address, as for a LAN game.

## Play with OpenCE players

Arena Evolved is made from OpenCE's code and plays online with OpenCE builds that use the **same network version**. A
classic OpenCE player and an Arena Evolved player can be in one game.

- The game list and the lobby tell a player who cannot join and why: "Host is on version N and needs to update to M
  or newer" or "Host is on version N, you're on M".
- The host's gametype rules decide for everyone in the game. OpenCE players get the rules where the host decides them.
- Rules that only Arena Evolved has (weapon packs, SILENT MOVEMENT, Bomb and so on) mean that only Arena Evolved
  players with the same content can join. An OpenCE player who cannot play a rule is turned away with a message, or
  dropped from the game.
- Which OpenCE builds match: the network version of this release is **39**. It plays with OpenCE build-282 and later
  up to build-333, and not with Arena Evolved 0.4.3 and older, OpenCE build-281 and older (network 38), or OpenCE
  build-334 and newer (network 40 and higher).
- ChupathingyCE players can join an Arena Evolved host, and Arena Evolved players can join a ChupathingyCE host
  (tested with ChupathingyCE 0.8.1). They play the game as their own build has it; AE's extras show only on AE.

To meet a classic player in a game on Halo CE's own gametypes, host a stock gametype: the AE rules are on only in AE
gametypes.

## The dedicated server on Linux

The dedicated server is the game as a program of its own. It has no window, sound or player, and it hosts games by
itself from a playlist of maps and game types, around the clock.

**Download.** The Releases page has `arena-evolved-<version>-server-linux-<arch>.zip` files. Pick your machine's with `uname -m`:

| `uname -m` | Download for |
| --- | --- |
| `x86_64` | `arena-evolved-<version>-server-linux-x64.zip`: most servers and PCs (Intel, AMD 64-bit) |
| `aarch64` | `arena-evolved-<version>-server-linux-arm64.zip`: 64-bit ARM: Oracle Cloud's free tier, Raspberry Pi 4 and 5 with a 64-bit OS |

The program inside is called `chupathingyce-server`.

A waiting server uses about 2% of one CPU core and about 215 MB of memory.

**Quick start.**

1. Unpack the zip. You get the server program (`chupathingyce-server`), a `playlists` folder, `blocked_names.txt` and a
   README.
2. Make a data folder with a `maps` folder in it. Copy in `ui.map` and all the multiplayer maps (about 300 MB) from
   your Xbox disc image. Halo PC maps go in folders beside `maps`: `maps_ce` for Custom Edition (with its
   `bitmaps.map`, `sounds.map` and `loc.map`), `maps_md` for HaloMD and `maps_pc` for Halo PC retail maps.
3. Put a playlist in the data folder's `playlists` folder (the download's or your own).
4. Start it from the data folder:

   ```sh
   cd /path/to/data
   HALO_DEDICATED=playlists/free_for_all.txt HALO_DEDICATED_NAME="My Server" /path/to/chupathingyce-server
   ```

The server prints what it is doing. Leave it running. Ctrl+C stops it and takes its game off the lists.

- The data folder is the current folder unless `HALO_DATA_ROOT` says otherwise.
- The log is `debug.txt` in the data folder (`HALO_LOG_ROOT` moves it). At 64 MB it is renamed `debug.old.txt`.
- Saves go in `~/.local/share/halo-linux` (or `$XDG_DATA_HOME/halo-linux`), unless `HALO_SAVE_ROOT` says otherwise.
- To keep it running with restarts, or several on one machine, use Docker or systemd. The source download has a
  Dockerfile and systemd files in its `server/deploy` folder.

**What it does.**

- Hosts a game with no player of its own. It is listed in the in-game server browser (unless
  `HALO_DEDICATED_PUBLIC` is `false`).
- Plays the playlist's entries in order. Once enough players join (`HALO_DEDICATED_MINIMUM_PLAYERS`, 1 by default), the
  lobby counts down by itself.
- Takes up to `HALO_DEDICATED_MAXIMUM_PLAYERS` players (12 by default).
- Ends a game nobody has scored in for 5 minutes, or 30 seconds after everyone leaves.
- Takes commands on its console, a startup file or a control panel (off unless you turn it on with
  `HALO_DEDICATED_CONTROL`). It has moderators, admins and owners of its own.

Dedicated servers do not serve Steam Workshop maps yet. Workshop maps are played on a host's own game.

**Home or cloud.** On a home connection you do not need to open or forward ports. On a cloud server (Oracle, AWS,
Google, Azure and the like), open one UDP port:

1. Give internet play a fixed port: `HALO_NET_TUNNEL_PORT=2302` (any free port). The log line `this machine's public
   address is ...:2302` shows the port in use.
2. Allow incoming UDP on that port in the provider's firewall (an ingress rule, source `0.0.0.0/0`, protocol UDP).
3. Allow it on the machine too. With ufw: `sudo ufw allow 2302/udp`. On Oracle's Ubuntu images:
   `sudo iptables -I INPUT -p udp --dport 2302 -j ACCEPT`, then `sudo netfilter-persistent save`.

A system link (LAN) server uses UDP 5150 and 5160 instead.

## Troubleshooting

| What you see | What to do |
| --- | --- |
| Friends cannot find your LAN game | Same network, not a guest Wi-Fi. Open ports 5150-5151/udp, 5160/udp, 5153/tcp. On Linux see above. |
| "No games found. On Linux a firewall can hide LAN games" | Open the ports above on the PC that does the searching, and on the host. |
| Friends cannot join your online game | Turn any VPN off. Check **INTERNET PLAY** and **UPNP PORT FORWARDING** are ON. If your router has no UPnP, forward the UDP port by hand. |
| "A VPN is on (name): players can't reach your game through it." | Turn the VPN off to host online. |
| "Internet play is off" | **SETTINGS > ONLINE > INTERNET PLAY** ON. |
| The invite row shows "BEING MADE..." and stays so | The game makes the invite itself, once it is hosting for the internet. If it stays, hosting online did not start: check **INTERNET PLAY** is ON (the row says "INTERNET PLAY IS OFF" when it is not) and look in `debug.txt` for the lines about Internet play. |
| An invite link says "That is not an invite" | The link is cut off or damaged. Copy it again. |
| Joiners see "Host is on version N and needs to update to M or newer" | Update your game, or ask them to. |
| An OpenCE player cannot join | Their build must be on the same network version. See "Play with OpenCE players". |
| A player without a map gets nothing | **DOWNLOAD MAPS** must be ON for them. Over the internet, your **INTERNET SHARING** and their **INTERNET DOWNLOADS** must be ON. Port 5153/tcp must be open on a LAN. |
| "Needs ..." for a Workshop map | The map is from a Steam Workshop item. The host plays it on its own game. Players need the same Workshop item subscribed in Steam. |
| Dedicated server stops at once with "no maps" | The data folder needs `maps/ui.map`. Start the server from the data folder, or set `HALO_DATA_ROOT`. |
| Dedicated server: "cannot read the playlist" | `HALO_DEDICATED` is a path inside the data folder: `playlists/my_playlist.txt`. |
| Dedicated server sits in the lobby and never starts | Every multiplayer map must be in `maps`, not just the playlist's. With `HALO_DEDICATED_MINIMUM_PLAYERS` above 1, it waits for that many. |
| Dedicated server shows, but nobody can join | A cloud firewall is dropping UDP. See "Home or cloud" above. |
| Dedicated server: "cannot reserve the Xbox address space" (arm64) | The kernel gives the program fewer addresses than it needs. Use a kernel with 39 or more address bits, or tell us your system. |
