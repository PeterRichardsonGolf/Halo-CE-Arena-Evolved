# Host your MCC maps on an Arena Evolved server

A dedicated Arena Evolved server for Linux and Windows (hosting cases: `docs/hosting.md`). It reads the maps of your Steam copy of Halo: The Master Chief Collection (stock maps and Workshop maps), plays Custom Edition maps and your saved gametypes, and plays Arena Evolved's own gametypes. No Xbox disc image is needed.

This is the server of an Arena Evolved release: the game's own program with no window (`halo --server`, `halo.exe --server` on Windows), the same build players run (`./halo --version` says which). It hosts games from a rotation with bots on the server (see "Bots"), sends a joining player the map being played when they lack it, and has API keys, warned restarts, backups, listing on the Arena Evolved master for the in-game server browser, several servers on one machine, BTB+ gametypes and a web page.

## What you need

- Debian 12 (or any 64-bit Linux with glibc 2.35 or newer). Nothing else is installed on the server: the package carries its own SDL3.
- Halo: The Master Chief Collection in Steam on the same machine (Proton is fine), with Halo CE and both its DLC pieces installed, and the Workshop items you want subscribed on that Steam account.
- This package. It holds Arena Evolved's weapon data (`data/packs/ae_content.aecache`).

Where Steam keeps things (Linux default):

```
~/.local/share/Steam/steamapps/common/Halo The Master Chief Collection      the game (the folder that holds halo1/)
~/.local/share/Steam/steamapps/workshop/content/976730/<item id>/           your subscribed Workshop items
```

If your Steam library is somewhere else, set `HALO_MCC_PATH` (below).

## Quick start (Linux)

```
unzip arena-evolved-<version>-server-linux-x64.zip -d ~     # makes ~/ae-server
cd ~/ae-server
nano rotation.txt                                         # the maps and gametypes to play, see "Rotation lines"
sh start-server.sh                                        # Ctrl+C stops it
```

`start-server.sh` runs `./halo --server` with these settings:

```
HALO_DATA_ROOT=~/ae-server/data         the game's data folder (data/maps can stay empty on the MCC route; the Xbox route needs ui.map there, see "Without MCC")
HALO_SAVE_ROOT=~/ae-server/save         settings, saves
HALO_SERVER_ROTATION=<rotation.txt>     what to play, in order, round and round
HALO_MCC_PATH=...                       only if Steam's folder is not found by itself (the line is in the script, commented out)
HALO_NET_TUNNEL_PORT=2302               the internet port (UDP)
```

`./halo --help` lists the names. A start prints, in plain words, what it found:

```
INFO server: MCC folder: /home/you/.local/share/Steam/steamapps/common/Halo The Master Chief Collection
INFO server: Workshop items found: 109, with 217 maps ... A new subscription needs a server restart
WARN server: rotation entry 3 (foo@ws1:slayer) left out: foo@ws1 is not available here: ...
INFO server: rotation, will play 1: bloodgulch@mcc:slayer
INFO net: a new invite (...)            the link your friends paste into the game to join is in data/invite-link.txt
INFO server: ready (tunnel 2302/udp, web 127.0.0.1:8080, listing public)
```

The first start makes the web page's admin password. Started from a terminal, it prints it once there. Started by a service or a container (the output goes to a journal), it does not: the password is in `data/first-admin-password.txt` (mode 0600; the log names only the path), and in `config.toml` beside the program (`server.web_password`, also mode 0600). The invite link is handled the same way: on the terminal, or in `data/invite-link.txt`. Delete `first-admin-password.txt` once you have it, or keep it somewhere safe.

## Without MCC (the Xbox route)

A server with no Steam copy of MCC plays Xbox-format maps (the stock Xbox maps, or the community map packs below). On that route the game still needs Halo's menu data file: copy `ui.map` into `data/maps/` (`ae-server/data/maps/ui.map`, from the maps folder of an Arena Evolved install that plays Xbox maps). Without it the server does not start. Then the maps of each pack go in their own folder beside `maps`, as in "Community map packs and map sharing".

## Rotation lines

One line in `rotation.txt`: entries separated by commas, each `map:gametype`, optionally `+time=<minutes>` or `+score=<n>` after the gametype. The server plays them in order and starts again from the first.

| What | Entry |
| --- | --- |
| An MCC stock map | `bloodgulch@mcc:slayer` |
| A Workshop map (the number is the Workshop item's id) | `pat_beavercreek@ws2899342285:slayer` |
| A Custom Edition map | `starwars@ce:slayer` |
| An Arena Evolved gametype (a `_` for each space, `plus` for a `+`, lower case) | `bloodgulch@mcc:ae_team_sly`, `bloodgulch@mcc:ae_ctf`, `bloodgulch@mcc:ae_btb_plus_ctf` |
| A gametype you saved in the game | `starwars@ce:file=starwarsctf` |
| A map of an Xbox map pack (`maps_<pack>/<file>.map` in the data folder) | `pat_chillout@pro:ae_team_sly` (folder `maps_pro`), `warren@smp:ae_ctf` (folder `maps_smp`) |
| A time limit of 15 minutes on any of them | `bloodgulch@mcc:ae_ctf+time=15` |
| A starting weapon on any of them, the gametype otherwise unchanged | `pat_chillout@pro:ae_comp_ts+start=sidearm` |

`start=` takes `battle_rifle`, `evo_rifle`, `pistol`, `sidearm`, `magnum_scoped`, `smg`, `carbine`, `dmr` and the other weapon packs' names, or a stock weapon (`assault_rifle`, `shotgun`, `needler`...; the full list is in `docs/dedicated_server.md`). The second weapon stays the gametype's own. A word the server does not know is logged and the entry plays without it.

For example, a Custom Edition map `starwars` with a saved gametype STARWARSCTF is this line:

```
starwars@ce:file=starwarsctf
```

To set that up:

1. Copy the map file into `ae-server/data/maps_ce/starwars.map`. Take it from the maps folder of the Arena Evolved you host from (`maps_ce/`; an older install may have it in `maps/ce/` or `custom_maps/`).
2. Copy the saved gametype to `ae-server/gametypes/starwarsctf.gametype` (lower case, as it appears in the rotation). A saved gametype is a folder in the save folder of the game you host from, `u/UDATA/<random id>/`, and the file in it is `blam.lst`. Find the one that is yours:
   ```
   cd <that save folder>/u/UDATA && grep -l STARWARSCTF */SaveMeta.xbx
   cp <the folder printed>/blam.lst ~/ae-server/gametypes/starwarsctf.gametype
   ```
3. Put `starwars@ce:file=starwarsctf` in `rotation.txt` and start the server.

Custom Edition maps get their textures and sounds from MCC's files, so no `bitmaps.map` or `sounds.map` is copied. (A server with Xbox maps instead of MCC plays the same lines.)

A line the server cannot play is left out of the rotation with the reason, and the others are played. If none can be played the server stops and says so.

## Community map packs and map sharing

Put a pack's maps in its own folder beside `maps` in the data folder (`ae-server/data/maps_pro/pat_chillout.map`, `ae-server/data/maps_smp/warren.map`, ...) and name a map as `<file>@<pack>`, the folder's name without `maps_`. The server checks at start that the file is there and says so if it is not.

Players do not need the pack: when a player joins a game on a pack map they do not have, the server sends it to them (map sharing, on by default, `network.map_share`) and the game starts once it is in. Stock maps, `@mcc` and `@ws` maps are never sent (the player needs MCC for those). The server only offers maps that are community maps, so keep stock Xbox maps out of the pack folders.
Tested on this server: a client with no pack folders joined `pat_chillout@pro` on the server, downloaded the 20 MB map and played it. Downloads take a few seconds; the server does not hold the game for a player who is still downloading a map announced late, so that player waits for the next game.

## Map vote and a big rotation (an example for a public server)

`server.map_vote` (`HALO_SERVER_MAP_VOTE=true`) lets the players vote for the next game. After each game the lobby offers `server.map_vote_choices` (`HALO_SERVER_MAP_VOTE_CHOICES`, 2 to 8, default 4) entries of the rotation picked at random (never the game just played), plus SAME AGAIN. It runs `server.map_vote_seconds` (`HALO_SERVER_MAP_VOTE_SECONDS`, 5 to 120, default 20) or until everybody has voted. So the rotation is the pool the vote draws from.

- A rotation holds at most 64 entries and 4096 characters; the rest is left out with a warning.
- There are no weights. To make a mode more likely, list it on more maps.
- The rotation plays in the order of the file. To play every pass in a new random order set `server.rotation_shuffle` (`HALO_SERVER_ROTATION_SHUFFLE=true`, off by default): each pass uses every entry once, never the same map twice in a row (a new pass included), and, where the entries allow it, not the same mode twice in a row. The order is logged once a pass (`rotation shuffled: ...`). The vote still draws its choices from the whole rotation.
- The start-up check says when a map file is missing or a gametype name is unknown. It does not check that a map has what a gametype needs (flags for CTF, hills for KING and TERRITORIES): check that in a test game. Gametype names are the game's, lower case, with `_` for spaces: `ae_team_sly`, `ae_br_ts`, `ev_ctf`, `ae_comp_ts` (the competitive AE PRO modes are called AE COMP).

## Run as a service (systemd)

`ae-server.service` is a user unit: it starts `~/ae-server/start-server.sh`, restarts it if it crashes, and stops it cleanly.

```
mkdir -p ~/.config/systemd/user && cp ~/ae-server/ae-server.service ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable --now ae-server
loginctl enable-linger $USER            # keep it running when you are logged out
journalctl --user -u ae-server -f       # the log (the invite link is in data/invite-link.txt)
systemctl --user restart ae-server      # after you change rotation.txt or subscribe to a new Workshop item
```

The unit takes the rotation and the Steam folder from `start-server.sh`; edit the commented `HALO_MCC_PATH` line there if you need it. If the package is not in `~/ae-server`, change `WorkingDirectory` and `ExecStart` in the unit.

## A second server on the same machine

Everything in the package uses the folder name `ae-server` and the unit name `ae-server`. For a second install pick a name of your own, for example `ae-two`, and rename both in one go:

```
unzip arena-evolved-<version>-server-linux-x64.zip -d ~ && mv ~/ae-server ~/ae-two
sed -i 's/ae-server/ae-two/g' ~/ae-two/ae-server.service
mv ~/ae-two/ae-server.service ~/ae-two/ae-two.service
mkdir -p ~/.config/systemd/user && cp ~/ae-two/ae-two.service ~/.config/systemd/user/
systemctl --user daemon-reload && systemctl --user enable --now ae-two
```

Give the second server its own UDP port and its own address too (the next section has the whole recipe).

## Several servers on one machine

The game's LAN ports are fixed (UDP 5150 and 5160, TCP 5153), so two servers on one machine clash unless each binds those ports to its own IPv4 address. Per server:

- `export HALO_NET_ADDRESS=<its own IPv4>` in its `start-server.sh` (add secondary addresses to the network card first; a machine with one address can run one server). The server then binds the LAN ports to that address only; `ss -lun` shows 5150 and 5160 once per address.
- A tunnel port of its own: `HALO_NET_TUNNEL_PORT=2303` (and 2304 for a third), open in the firewall.
- A web page port of its own (`server.web_port`, `HALO_SERVER_WEB_PORT`), for example 8081 and 8082; the page stays on `127.0.0.1`.
- Its own folder, unit name, `rotation.txt`, and password. Nothing is shared between installs.

A game name holds 15 characters; a longer one is cut without a message. The log line `lobby: hosting` shows the server is up.

## Listing on the Arena Evolved master

A server that is public and has a publisher key can list itself on the Arena Evolved master, so players find it in the in-game server browser. Turn it on with `server.public = true` (`HALO_SERVER_PUBLIC`) and `server.master_publish = true` (`HALO_SERVER_MASTER_PUBLISH`), and give the server a publisher key: `./halo --server --keygen` writes one (`server.publisher_key_file`, `publisher.key` by default) and prints its public key. Keep that file private. The server needs `network.online` on and a real address (not 127.0.0.1); behind a tunnel set `network.public_address` (the tunnel's UDP port) and, if the tunnel also carries the game port, `network.join_address` (below, "Behind a tunnel").

The master is built into the game, so `masters.txt` can stay as shipped. A first publish is approved on its own after the master has probed your UDP port from outside, with no application. `server.region` (for example `us-west`) is shown with the entry. A server made from the official game files that is not an Arena Evolved server cannot list there.

The Arena Evolved master (master.arenaevolved.com) is coming online. Until it answers, servers log that they are not listed and clients log "no answer" once per round; both are harmless, and players can still use JOIN BY ADDRESS.

## Arena Evolved BTB+ rotations

Big team games with extra vehicles have their own gametypes. In a rotation line spell the `+` as `plus`: `bloodgulch@mcc:ae_btb_plus_ctf`. The names are `ae_btb_plus_sly`, `ae_btb_plus_ctf`, `ae_btb_plus_koh`, `ae_btb_plus_ob`, `ae_btb_plus_hvy`, and the battle rifle twins `ae_br_btb_pluss`, `ae_br_btb_plusc`, `ae_br_btb_plusk`, `ae_br_btb_plusb`, `ae_br_btb_plush`. They need a map with the vehicles and, for the extra aircraft, banshee spots; a map without them plays as a plain big team game. Maps from Xbox packs carry no vehicle spots.

## Keep the objective log out of the journal

With `debug.objective_log full` (bots, training games) the server prints about 34 lines a second. The same lines are already in `data/debug.txt`, and a systemd journal fills quickly with them. Start the server through a filter that drops them from the journal. Put this in a drop-in (`systemctl --user edit ae-server`):

```
[Service]
ExecStart=
ExecStart=/bin/bash -c "exec %h/ae-server/start-server.sh > >(grep --line-buffered -v \"ae obj:\") 2>&1"
```

`exec` keeps the server as the unit's main process, so a stop and `Restart=` still work. (`LogFilterPatterns=` did not work for a user unit.) `data/debug.txt` rolls itself over at 64 MB, so it needs no log rotation.

## A user of its own, and the sandbox

Run the server as a user that has nothing else in it: not the login you play Steam on, no SSH keys, no mail. If the server program ever had a flaw, that account is all it would reach. Make one (`sudo adduser --disabled-password aeserver`), and give it a read-only view of the MCC and Workshop folders (a read-only bind mount, or a copy of `steamapps/common/Halo The Master Chief Collection` and `steamapps/workshop/content/976730`; `HALO_MCC_PATH` says where). Then put the package and the service in that user's home as described above.

`ae-server.service` is a sandbox as well: the program cannot gain rights (`NoNewPrivileges`), cannot make memory both writable and executable (`MemoryDenyWriteExecute`), sees no other users' temporary files (`PrivateTmp`), reads the home folder but writes only the server's folder (`ProtectSystem=strict`, `ProtectHome=read-only`, `ReadWritePaths`), cannot rewrite its own program files (`ReadOnlyPaths`), has no capabilities, and may open only internet, local and unix sockets. The Steam folders stay reachable and read-only. The service files are written for the folder `~/ae-server`; if the folder is elsewhere, change the three marked paths too. An update by the server itself (`server.auto_update` is `check` by default, it only logs a newer release) cannot install in this sandbox: unpack a newer package by hand.

## Run in a container (docker or podman)

The package carries a `Dockerfile`, `container-start.sh` and `compose.yaml`. The image is `debian:12-slim` with the server in it; MCC is not in the image. The container runs as an ordinary user (uid 10001) with no capabilities, no new privileges and a read-only root. The program and Arena Evolved's weapon data are read-only in the image; everything the server writes (settings, saves, the admin password, the invite link, logs) is in the volume `/var/lib/ae-server`. Your Steam folders are mounted read-only. Only UDP 2302 is published.

```
cd ~/ae-server                                   # the unpacked package
nano rotation.txt
STEAM=$HOME/.local/share/Steam docker compose up -d --build
docker compose logs -f
docker compose exec ae-server cat /var/lib/ae-server/data/first-admin-password.txt
```

`STEAM` is the folder that holds `steamapps/` (on a server without Steam, the target folder of `sync-mcc.sh`, see "A server without Steam"). The two mounts keep the `steamapps/common/...` and `steamapps/workshop/content/976730` shape the server looks for. `rotation.txt` is mounted from the package folder; `docker compose restart ae-server` re-reads it. Settings go in the `environment:` list of `compose.yaml` (the names are `./halo --help`'s).

Rootless podman works too: `STEAM=... podman-compose up -d --build` (Ubuntu 24.04: `apt install podman podman-compose`). Tested that way: the container starts as uid 10001 on a read-only root, `restart: on-failure` brings it back after a crash, and `compose down` then `up` keeps the volume (settings, admin password, invite link). The only published port is UDP 2302, and that is enough for players: an Arena Evolved 0.7.7 client that joins by address (or from the server browser) finds the game port closed, asks the tunnel on 2302 and joins through it. Older clients need the game's LAN ports, which this setup does not publish on purpose.

The web page listens on 127.0.0.1 inside the container, which the host cannot reach; the server refuses to open it to a network without a TLS proxy. To use the page from the host run with `network_mode: host` (drop `ports:`), then `127.0.0.1:8080` is the host's own. A home network whose friends find the server by LAN search needs `network_mode: host` too, with the ports firewalled as in the next section.

## A server without Steam (sync-mcc.sh)

A VPS has no Steam and no MCC. Copy the files the server needs from the PC that has them, in the shape of a Steam library, and point `HALO_MCC_PATH` at the copy:

```
sh sync-mcc.sh --from "$HOME/.local/share/Steam" --to aeserver@vps.example:/srv/ae-steam --rotation rotation.txt
```

- `--from` is the Steam folder (the one holding `steamapps/`) or the MCC folder itself; on Windows, run it in Git Bash (`--from "/c/Program Files (x86)/Steam"`). The source is only read.
- `--to` is `user@host:/folder` (ssh, with rsync when both ends have it, else tar over ssh) or a local folder. It makes `/folder/steamapps/common/Halo The Master Chief Collection/halo1/maps/...` and `/folder/steamapps/workshop/content/976730/<item id>/`.
- It copies the Halo CE maps the game's own check asks for (`ui.map`, the campaign maps, the 19 multiplayer maps, the three Custom Edition resource maps; about 1.4 GB, once) and the Workshop item of every `<map>@ws<item id>` entry in the rotation. Entries that are not `@mcc` or `@ws` are not MCC files and are listed as not copied.
- `--dry-run` shows what it would send. Run it again after a Workshop subscription or an update: it sends only what changed.
- On the server: `HALO_MCC_PATH="/srv/ae-steam/steamapps/common/Halo The Master Chief Collection"` in `start-server.sh`, or `STEAM=/srv/ae-steam` for the container. Restart.

MCC and its Workshop items belong to their owners, and to you through your Steam account: copy them only to a server you run for yourself.

## Ports and firewall

The server uses:

| Port | What | Open to |
| --- | --- | --- |
| UDP 2302 | Internet play (the invite link, and the server browser's join through the tunnel) | the internet: the only port the internet needs |
| UDP 5150, UDP 5151, UDP 5160, TCP 5150, TCP 5153 | The game's LAN ports: UDP 5150 the game, UDP 5151 the answers to a LAN search, UDP 5160 the Arena Evolved extras (Delta), TCP 5150 the joins the host listens for, TCP 5153 the map download of a joiner who lacks the map (listens only while a map is sent) | your LAN only. Never on the internet |
| TCP 8080 on 127.0.0.1 | The built-in web page and control API | this machine only (never open it) |

Open the firewall BEFORE the first start, and let in only UDP 2302 from the internet. A server bought as a virtual machine often has no firewall at all, and then the LAN ports are open to everybody. If joiners from the internet lack a pack map and cannot get it, the map download (TCP 5153) is what is missing: it is for players on your own network, or for internet players you let through to that port on purpose (one rule, to their address). In a container, publish only UDP 2302: Docker's published ports skip ufw.

A default-drop firewall with nftables (change the LAN subnet, or leave the second rule out for an internet-only server):

```
sudo nft add table inet filter
sudo nft add chain inet filter input '{ type filter hook input priority 0; policy drop; }'
sudo nft add rule inet filter input ct state established,related accept
sudo nft add rule inet filter input iif lo accept
sudo nft add rule inet filter input tcp dport 22 accept
sudo nft add rule inet filter input udp dport 2302 accept
sudo nft add rule inet filter input ip saddr 192.168.1.0/24 udp dport '{ 5150, 5151, 5160 }' accept
sudo nft add rule inet filter input ip saddr 192.168.1.0/24 tcp dport '{ 5150, 5153 }' accept
```

To keep these rules after a restart: `sudo nft list ruleset | sudo tee /etc/nftables.conf` and `sudo systemctl enable nftables`.

It also connects out: UDP to two public STUN servers (to learn its public address), and HTTPS and TCP 1883 to the game's list servers when it is listed.

```
# ufw: internet play
sudo ufw allow 2302/udp
# ufw: LAN games (change the subnet)
sudo ufw allow from 192.168.1.0/24 to any port 5150 proto udp
sudo ufw allow from 192.168.1.0/24 to any port 5160 proto udp
sudo ufw allow from 192.168.1.0/24 to any port 5151 proto udp
sudo ufw allow from 192.168.1.0/24 to any port 5150 proto tcp
sudo ufw allow from 192.168.1.0/24 to any port 5153 proto tcp

# iptables
sudo iptables -A INPUT -p udp --dport 2302 -j ACCEPT
sudo iptables -A INPUT -p udp -s 192.168.1.0/24 -m multiport --dports 5150,5151,5160 -j ACCEPT
sudo iptables -A INPUT -p tcp -s 192.168.1.0/24 -m multiport --dports 5150,5153 -j ACCEPT
```

If a PC does not see the server in its LAN search although it is on the same network, its general broadcast may leave by another network connection. Give the game the network's own broadcast address instead, for a network 192.168.1.x that is `HALO_NET_BROADCAST=192.168.1.255` in the environment of the game, or join by the server's address (JOIN BY ADDRESS).

Players on the internet use the invite link; the server finds its public address on its own, and at home this usually needs no router setup. If friends cannot join, forward UDP 2302 on the router to the server. A VPN on the server machine or on the router stops joins from outside: turn it off for hosting.

## Bots

An Arena Evolved server can keep its games full with bots, so a game is never one player alone. The bots sit on the server (nobody's PC carries them) and are never rated.

| Setting (start-server.sh variable) | What it does |
| --- | --- |
| `server.bots_fill` (`HALO_SERVER_BOTS_FILL`) | `auto` (default): `minimum` on an `ae_` gametype, `off` on the others. `minimum`: bots are added until the people and the bots are `server.min_players`. `empty_slots`: bots fill every slot up to `server.max_players`. `off`: none. |
| `server.min_players` (`HALO_SERVER_MIN_PLAYERS`) | The size `minimum` fills to. It is 1 by default, so set it (say 8) to see bots. |
| `server.bots_skill` (`HALO_SERVER_BOTS_SKILL`) | `recruit`, `easy`, `normal` (default), `heroic`, `legendary` or `demon`. |
| `server.bots_alone` (`HALO_SERVER_BOTS_ALONE`) | `false` (default): with nobody in, the server waits: no game, no bots. `true`: bots play on their own, for a server that runs all day. |
| `server.bots_delay` (`HALO_SERVER_BOTS_DELAY`) | Seconds the lobby waits for other people after the first one is in, before bots fill it (10 by default). |

How it plays:

- A person who joins takes a bot's place at that bot's next death, never in the middle of a fight. The teams stay even.
- A person who drops out of a game keeps their place: a bot plays it, and when they come back (same name, same computer) they get their team and their score back, and a bot leaves at its next death.
- A game with only bots ends after `server.empty_seconds` unless `server.bots_alone` is on. The web page's `/api/status` shows `"bots": {"count", "skill", "fill", "alone", "held"}` and `players` counts people only.
- Bots play Slayer, CTF, Oddball, King of the Hill, Race and Bomb gametypes (ELIMINATION included, with revives).
- To see bots, set `server.min_players` (`HALO_SERVER_MIN_PLAYERS`) above 1: `minimum` fills up to that number, and with the default of 1 it adds nothing.
- With nobody in the server, no game starts and no bots are added (unless `server.bots_alone` is on).
- In `start-server.sh` the bot variables are commented out. Remove the `#` of the ones you want.

Example, a server that fills AE TEAM SLAYER to 8 with normal bots:

```
HALO_SERVER_MIN_PLAYERS=8 HALO_SERVER_BOTS_FILL=minimum sh start-server.sh
```

The start prints one line with the settings in use (`server: bots: fill minimum ... skill normal ...`) and one line each game (`bots: entry ae_team_sly: fill minimum, skill normal`). The same variables work in the service file (`Environment=` lines) and in a container's environment.

## Who can join

- Arena Evolved 0.7.6 players: every entry, with one limit for `@mcc` and `@ws` maps: when the rotation moves to the next map, they have to leave and join again (use the invite link again). The 0.7.7 game fixes this. An `@mcc` map needs their own MCC with Halo CE (stock maps and Workshop maps are read from their own Steam, the server never sends them). A `@ws` map needs the same Workshop item, in the same version as the server's: if Steam updated it on one side only, the game says so and does not start. A Custom Edition map needs the map file, as for any Custom Edition game.
- Players on OpenCE or ChupathingyCE (network version 41): entries with Halo CE's own gametypes (and maps they have). Arena Evolved's `ae_` gametypes and the MCC maps are refused with a message.
- Players come by invite link, by LAN search on your network, or by the address of the server. The in-game server browser lists a server that has a publisher key (see "Listing on the Arena Evolved master").

Tested on this server: an Arena Evolved client with MCC joined, played a game and followed the rotation to the next map (`@mcc`, `@ws`, `@ce` with a saved gametype, an `ae_` gametype). Other clients were not tested here.

## Windows home PC

The Windows package (`arena-evolved-<version>-server-windows-x64.zip`, folder `ae-server`) is the same server: `halo.exe --server`. It reads your Steam copy of MCC the way the game does (Steam's own registry entry and libraries, no setup), so `@mcc` and `@ws` entries play from the PC you game on.

```
1. Unzip the folder anywhere (C:\ae-server is a good place; not under Program Files).
2. Edit rotation.txt (one entry per line or separated by commas; lines that start with # are notes).
3. Double-click start-server.bat. Ctrl+C in its window stops it.
```

The first start makes the web page's admin password in `data\first-admin-password.txt` and the invite link in `data\invite-link.txt`. The log is `data\debug.txt`. The web page is `http://127.0.0.1:8080/` on that PC.

**Firewall.** Run `firewall-rules.bat` as administrator once. It lets UDP 2302 in from the internet and the game's LAN ports (UDP 5150, 5151, 5160, TCP 5150 and 5153) in from your own subnet only, for the `halo.exe` in this folder. `firewall-remove.bat` takes the rules out. Windows may also ask once when the server first starts; allow it on private networks for LAN games. A router in front of the PC usually needs nothing for internet play (see "Ports and firewall").

**Start with Windows.** Run `install-task.bat` as administrator. It makes a scheduled task, "Arena Evolved server", that starts the server when Windows starts, with no one logged in. It runs as SYSTEM, which has no Steam of its own: put the MCC folder in `server-settings.bat` first (`set "HALO_MCC_PATH=D:\SteamLibrary\steamapps\common\Halo The Master Chief Collection"`). `schtasks /run /tn "Arena Evolved server"` starts it, `schtasks /end /tn "Arena Evolved server"` stops it (a hard stop), `schtasks /delete /tn "Arena Evolved server" /f` removes it.

It is a task and not a Windows service because `halo.exe` does not speak the service protocol: `sc create` would only time out with error 1053. A real service mode is on the list.

A PC you game on works, but a game of Halo running on the same PC clashes with the server's ports (both want UDP 5150): stop the server before you play on that PC, or give it its own address (`HALO_NET_ADDRESS` in `server-settings.bat`, "Several servers on one machine").

## Behind a tunnel (playit.gg and others)

A tunnel agent that runs on the server's own computer (the playit.gg agent, `ssh -R`, a reverse proxy) hands every player to the server from one address, the agent's: the server's own address or `127.0.0.1`, only the port differs. Since 2.4.2 the server copes with that:

- Machines that share an address are told apart by the port their packets come from, which each Arena Evolved client confirms to the server with a token (once a second, 12 bytes). A second player from the same address no longer drops the first ("joined from the address of machine ... which is dropped" is gone), and two players behind one home router work the same way.
- A machine that joins again from the same address replaces its old self only when its hardware id is the same (or not told: the older rule). A machine of another hardware id is another player.
- `server.tunnel_addresses` (`HALO_SERVER_TUNNEL_ADDRESSES`) lists the addresses of tunnel agents. The default, `"auto"`, is this computer's own addresses and `127.0.0.1`, which is right for an agent on the same computer. For an agent on another computer give its address too: `"auto, 192.168.1.20"`. `""` turns it off. Connections from these addresses are not told apart by address for anything else either: a ban, a 2-minute kick or a vote kick never goes by the agent's address (only by the hardware id: a ban of one player cannot lock out the others), and `server.max_machines_per_address` counts the machines of one hardware id there instead of the address.
- The agent's address is no player's, so `sv_players` shows it as `0.0.0.0` there. Clients older than this server (0.7.6 and earlier) send no token: the server gives a packet source it has not seen to the one machine of the address whose port is not known yet (the one that has loaded the game, if several are not known). That is right whenever players load one after the other; two old clients that finish loading at the very same moment may have their packets dropped until the 0.7.7 game is used. A new client never has this.
- **Listing behind a tunnel.** The master probes the address the server publishes, and the server browser sends a player to it. With one tunnel for the tunnel port (`network.tunnel_port`, UDP) and `network.public_address` set to it, that is enough: a player joins through the tunnel. If you also have a tunnel for the game port, set `network.join_address` to it (`host[:port]`, the port 5150 when left out; `HALO_NET_JOIN_ADDRESS`; it needs `server.master_publish_address = true`): the server then publishes it apart from `network.public_address`, its game port answers the master's probe, and the master lists the server only when UDP and TCP answer at that address (both have to be tunnelled to the server's UDP and TCP 5150), and sends players there. A player's join then needs UDP and TCP on that one port number and nothing else: an Arena Evolved player gets a missing map over the same connection (a player of another build needs TCP 5153 for that). `network.public_address` stays the tunnel's for players who join by invite link. A name is looked up again every 5 minutes; an IPv6 address is not taken.
- Do not put this machine's public address in `server.tunnel_addresses`: players coming from there would lose their address checks.

## Known limits

- Safer defaults in this package: `server.metrics` is off (the Prometheus page on the web port, which has no password), `server.max_machines_per_address` is 3 (one address cannot fill the server; raise it for a LAN party or a shared household), `server.auto_update` is `check`. The web page and its control socket keep to `127.0.0.1`; behind a TLS proxy (not described here) they need `server.web_trusted_proxy`.

- A player who joins while the game is under way and does not have its map gets the map first (only that map, not the server's other maps), and then joins the game in progress or the next one. A map is about 20-35 MB.

- The public server list counts the bots as players (it does not tell them apart yet). `/api/status` on the web page does.
- Players on Arena Evolved 0.7.6 joining a rotation with `@mcc` or `@ws` maps must rejoin when the map changes (fixed in the 0.7.7 game).
- Bots are new and only lightly tested; no web admin accounts yet; the web page is on 127.0.0.1 only and is not needed to run the server.
- The Workshop items are read once, when the server starts. A new or updated subscription needs a server restart (`systemctl --user restart ae-server`).
- The server does not check which version of a Workshop item its players have beyond what the game itself checks when they join.

## API keys, warned restarts and backups

These come with the 0.7.7 server build. The web page and its API stay on `127.0.0.1` unless you open them (see Known limits).

**API keys** are for tools and agents, so they do not share a person's password. Make one on the server console or over the API (an admin or the owner can):

```
sv_key_add dashboard viewer            # a name, a role (viewer, moderator or admin), optional days and allow list
sv_key_add bot admin 30 192.168.1.0/24
sv_keys                                 # lists the keys, never their secrets
sv_key_remove bot                       # revokes at once
```

The secret (`aek_<8 hex>_<48 hex>`) is shown once, so copy it then. The server keeps only a hash of it in `api_keys.txt`. Use it as `Authorization: Bearer aek_...` on the API, or as `{"type":"auth","key":"aek_..."}` on the remote console (`/ws`). A key has the role you gave it (never owner), no idle timeout, an optional expiry in days and an optional list of allowed addresses (`10.0.0.5`, `192.168.1.0/24`, comma separated). The audit lines in the log name it as `key:<name>`. Five wrong keys from one address lock that address out for ten minutes, as wrong passwords do. Over the API (admin role): `GET /api/keys`, `POST /api/keys` (`{"name":"dash","role":"viewer","days":30,"allow":"..."}`), `DELETE /api/keys/<name>`. Keep keys off the open internet unless the web port is behind a TLS proxy.

**Warned restarts.** `sv_restart 90 updating` (owner console, or `POST /api/stop` with `{"restart":true,"seconds":90,"notice":"updating"}`) tells the players in chat, at the start and at 60, 30, 10 and 5 seconds: `Server restarting in 30 seconds: updating.` Then the server exits with code 5 and your service manager starts it again. `sv_restart cancel` stops the countdown and tells the players. A bare `sv_restart` or `sv_stop` waits `server.restart_warning_seconds` (60 by default; 0 means now). `sv_restart now` is at once. `server.restart_daily "04:30"` (`HALO_SERVER_RESTART_DAILY`, the machine's time zone, empty means off) restarts the server every day at that time with the same warning.

**Backups.** Before the server rewrites `config.toml` (the rotation is in it), `bans.txt` or `api_keys.txt`, and once when it starts, it copies the old file to `backups/<name>.<YYYYMMDD-HHMMSS>` in the data folder (UTC). It keeps the last `server.backups_keep` of each (`HALO_SERVER_BACKUPS_KEEP`, 20 by default) and deletes older ones, only inside `backups/`. `sv_backups` lists them. To go back, stop the server and copy a backup over the file.

## Licence and hosting rules

A plain-words summary of what we believe applies. It is not legal advice.

- **The program is open source under the GNU AGPL-3.0** (`licences/LICENSE.txt`), like OpenCE it is built on.
  - Run it unchanged: there is nothing you need to do.
  - Run a changed copy (your own patches): the AGPL asks you to offer the changed source code to the players who
    connect to your server, for example with a link in your server's welcome message or web page.
  - The source of this package is its release's source archive (see `SOURCE.txt`).
- **Game content stays yours and the game's owners'.** The server reads Halo: The Master Chief Collection from your own
  Steam copy while it runs. This package holds no maps and no MCC files, and you should not share MCC's files with others.
- **Community maps and gametypes belong to their authors.** Host them the way their authors allow; credit them.
- **No charging for access.** Fan servers for Halo games must stay non-commercial: no paid entry, paid slots or paid
  perks. Covering your own server costs with voluntary donations is the usual line; check the game's content usage
  rules if you are unsure.
- Weapon and sound credits for the AE content in `data/packs/ae_content.aecache` are in `licences/CREDITS.md`.

## Send us the log

If something goes wrong, send `~/ae-server/data/debug.txt` and the server's console output (`journalctl --user -u ae-server --since today > ae-server.log`). The log no longer holds the web page password or the invite link, but check it for anything private before you send it.
