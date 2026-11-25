# Host a server

How to host a game from Halo CE: Arena Evolved for friends on your network or on the internet, how to play with
OpenCE players, and how to run the dedicated server on Linux or Windows. If you only want to play, read
[How to play](how-to-play.md) first.

## On this page

- [Three ways to host](#three-ways-to-host)
- [Host from the game](#host-from-the-game)
- [Firewall and ports](#firewall-and-ports)
- [VPNs](#vpns)
- [Play with OpenCE players](#play-with-opence-players)
- [The dedicated server](#the-dedicated-server)
- [Troubleshooting](#troubleshooting)

## Three ways to host

| Way | For | Needs |
| --- | --- | --- |
| **OFFLINE** game | You, and split-screen players on your PC | Nothing. The pause menu really pauses. |
| **LAN** game | Players on your home or LAN-party network | The same network (not a guest Wi-Fi) and open ports (below). |
| **HOST ONLINE** game | Players anywhere on the internet | Internet play on, no VPN, and your router able to open a port (UPnP, below). |
| **Dedicated server** | A game that stays up all the time | A Linux or Windows machine. No player sits at it. |

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
| **COMMUNITY SERVICES** | asked at the first start (DECLINE or ACCEPT) | The server browser and invites go through a community service. OFF: nothing goes there, and the server browser lists nothing. |
| **UPNP PORT FORWARDING** | ON | Opens the game's port on your router so players can join you. |
| **LIST HOSTED GAMES** | OFF | Games you host show in the server browser. A game is listed in it only with this ON and **PUBLIC** ON in CREATE GAME. Choosing ACCEPT on the first-start COMMUNITY SERVICES page, with its LIST GAMES I HOST switch ON, turns it on. |
| **DOWNLOAD MAPS** | ON | Get a map you lack from the host. |
| **WORKSHOP FROM STEAM** (MAPS) | ON | Joining a server that plays a Workshop map you lack: with STEAM ACCOUNT signed in, get it from Steam (the game's map first, at most 12 maps and 1 GB a server). |
| **INTERNET SHARING** | ON | Hosting: also send your maps to players who are not on your local network. |
| **INTERNET DOWNLOADS** | ON | Also get maps from hosts over the internet. |
| **DOWNLOAD LIMIT** | 4 GB | The most map data your PC takes from one host, from 256 MB to 64 GB. |
| **HOSTED GAME STATS** | ON | Send your hosted games' scores to the stats site. It only matters for a listed game, so with LIST HOSTED GAMES off nothing is sent. |

Map sharing speed: a game you host at home sends maps at up to 16 MB/s when every player is on your local network and 4 MB/s
otherwise (`network.map_share_upload`), and a dedicated server at up to 10 MB/s. A player takes at most 4 GB (DOWNLOAD LIMIT) from one host. The game's own multiplayer
maps and MCC's maps are never sent.

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
fixed `network.tunnel_port` and forward that UDP port to your PC by hand. (A dedicated server uses 2302 unless you set
another.)

**Windows** asks the first time the game opens the network (hosting a game or searching a LAN). Allow it on private networks at least, and on public ones too if
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

Hosting online with a VPN on does not work. The VPN answers for your PC, so players on the internet cannot reach you.
The CREATE GAME screen says "A VPN is on (name): players can't reach your game through it. Turn the VPN off to host
online." Turn the VPN off while you host.

The same goes for a VPN running on your **router**: the game cannot see it, so no warning shows, but players still
cannot reach you. Turn the router's VPN off while you host, or exclude the hosting PC from it in the router's settings.

## Play with OpenCE players

Arena Evolved is made from OpenCE's code and grew from ChupathingyCE. It plays online with builds that use the **same
network version**. Arena Evolved 1.0.0-rc7 uses **network version 44**.

| Other build | Can it join Arena Evolved 1.0.0-rc7, and can you join it? |
| --- | --- |
| OpenCE build-355 to build-387 | Yes, both ways (network version 44). |
| OpenCE build-354 and older | No (network version 43 and lower). |
| Arena Evolved older than 0.9.0 | No. |
| Arena Evolved 1.0.0-rc5 and rc6 | Yes. Games that use AE's weapons need the same weapon version; an older player is told to update. |

- The game list and the lobby tell a player who cannot join and why: "Host is on version N and needs to update to M
  or newer" or "Host is on version N, you're on M". The top of `debug.txt` names your network version.
- The host's gametype rules decide for everyone in the game. A player on another build gets the rules where the host decides
  them, and plays the game as their own build has it; AE's extras show only on AE.
- Rules that only Arena Evolved has (weapon packs, SILENT MOVEMENT, Bomb and so on) mean that only Arena Evolved
  players with the same content can join. A player on another build who cannot play a rule is turned away with a message, or
  dropped from the game.
- To join a game on an original Xbox map, you need the Xbox maps, so a disc image. A player with MCC alone has MCC's stock
  maps, not the original Xbox ones.

To meet a player on another build, host one of Halo CE's own gametypes (the STOCK folder): the AE rules are on only in AE
gametypes.

## The dedicated server

A dedicated server is the game as a program of its own: `halo --server` (`halo.exe --server` on Windows). It has no window,
sound or player, and it hosts games one after another from a rotation of maps and game types, around the clock, with a web
page and a remote console for the person who runs it. The server package's `GUIDE.md` has the details of everything below
(rotation lines, bots, listing, backups); the [hosting guide](https://github.com/PeterRichardsonGolf/Halo-CE-Arena-Evolved/blob/source-1.0.0-rc7/docs/hosting.md)
in the release source picks the way to run it.

**Download.** The Releases page has the server packages:

| Package | For |
| --- | --- |
| `arena-evolved-<version>-server-linux-x64.zip` | A Linux machine or a VPS (Debian 12 or any 64-bit Linux with glibc 2.35 or newer). It has a systemd unit and a container compose example. |
| `arena-evolved-<version>-server-windows-x64.zip` | A Windows PC that has MCC: a start .bat, firewall rules and a startup task |

**Quick start (Linux).**

```sh
unzip arena-evolved-<version>-server-linux-x64.zip -d ~
cd ~/ae-server
nano rotation.txt
sh start-server.sh
```

On Windows, unzip, edit `rotation.txt` and double-click `start-server.bat`; run `firewall-rules.bat` as administrator once.

**What it plays.** The server plays what the machine it runs on has. A `<map>@mcc` rotation entry needs Halo: The Master Chief
Collection with Halo CE and both its DLC pieces; `<map>@ws<item id>` needs the Workshop item subscribed on that Steam
account; an Xbox-format or Custom Edition map needs its file in the data folder. A VPS with no Steam can use `sync-mcc.sh`
(in the Linux package) to copy MCC's Halo CE maps and the Workshop items the rotation names from a PC that has MCC. The server
never sends MCC's maps or Workshop items to players: players who lack them need their own copy (or STEAM ACCOUNT). It does
send community and Custom Edition maps. MCC and its Workshop items belong to their owners: copy them only to a server you run
for yourself.

**The rotation.** `rotation.txt` takes one entry per map and gametype, `map:gametype`, with Arena Evolved's own gametypes
(`ae_team_sly`, `ae_ctf`, ...) and your saved gametypes. It holds up to 256 entries and 16384 characters (rotations over 128
entries need a server running 1.0.0-rc7; older servers keep the first 128). The server fills empty seats with bots if you
ask (`server.bots_fill`), runs the map and gametype vote between games, and can read a playlist from CREATE GAME's HOST A
SERVER. Arena Evolved players can join any entry; OpenCE players can join the entries that use Halo CE's own gametypes and
maps they have.

**Ports.** Internet play needs only **UDP 2302** (or the port you set with `HALO_NET_TUNNEL_PORT`). The game's LAN ports (UDP
5150, 5151, 5160, TCP 5150, 5153) are for your own network and stay off the internet. On a cloud server, allow incoming UDP on
that port in the provider's firewall and on the machine (`sudo ufw allow 2302/udp`). Run the server as an account that has
nothing else in it (no Steam login you play on, no SSH keys), or in a container: it reads packets from anyone who joins.

**Listing.** A server started from the game (HOST A SERVER, on Linux and Windows) is found on the local network, by its invite
link and by its address; the master servers list only server packages that have a publisher key (`halo --server --keygen`
makes it).

## Troubleshooting

| What you see | What to do |
| --- | --- |
| Friends cannot find your LAN game | Same network, not a guest Wi-Fi. Open ports 5150-5151/udp, 5160/udp, 5153/tcp. On Linux see above. |
| "No games found. On Linux a firewall can hide LAN games" | Open the ports above on the PC that does the searching, and on the host. |
| Friends cannot join your online game | Turn any VPN off — on the PC **and on the router**. Check **INTERNET PLAY** and **UPNP PORT FORWARDING** are ON. If your router has no UPnP, forward the UDP port by hand. |
| "A VPN is on (name): players can't reach your game through it." | Turn the VPN off to host online. |
| "Internet play is off" | **SETTINGS > ONLINE > INTERNET PLAY** ON. |
| The invite row shows "BEING MADE..." and stays so | The game makes the invite itself, once it is hosting for the internet. If it stays, hosting online did not start: check **INTERNET PLAY** is ON (the row says "INTERNET PLAY IS OFF" when it is not) and look in `debug.txt` for the lines about Internet play. |
| An invite link says "That is not an invite" | The link is cut off or damaged. Copy it again. |
| Joiners see "Host is on version N and needs to update to M or newer" | Update your game, or ask them to. |
| An OpenCE player cannot join | Their build must be on the same network version (44): OpenCE build-355 to build-387. See "Play with OpenCE players". |
| A player without a map gets nothing | **DOWNLOAD MAPS** must be ON for them. Over the internet, your **INTERNET SHARING** and their **INTERNET DOWNLOADS** must be ON. Port 5153/tcp must be open on a LAN. |
| "Needs ..." for a Workshop map | The map is from a Steam Workshop item. Players need the same Workshop item: subscribed in Steam, or fetched by STEAM ACCOUNT as they join. A server plays it only if the item is subscribed on its own Steam account. |
| Dedicated server shows, but nobody can join | A cloud firewall is dropping UDP. Allow the server's UDP port (2302 by default) in the provider's firewall and on the machine. |
| A rotation entry is skipped or the server stops at start | `<map>@mcc` entries need MCC with Halo CE and both its DLC pieces on the server's machine, `<map>@ws<item>` entries need the item subscribed on its Steam account, and other maps need their file in the data folder. The server's `debug.txt` names the entry. |
