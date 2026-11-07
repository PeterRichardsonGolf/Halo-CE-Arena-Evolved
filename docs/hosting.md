# Hosting an Arena Evolved server

A dedicated Arena Evolved server is `halo --server` (`halo.exe --server` on Windows): the game's own program with no window, hosting games one after another from a rotation, with a web page and a remote console for the person who runs it. This page picks the way to run it. The server package's `GUIDE.md` has the details of everything below (rotation lines, bots, listing, API keys, backups).

Whatever the way, three things hold:

- The server plays what the machine it runs on has. A `<map>@mcc` entry needs Halo: The Master Chief Collection with Halo CE and both its DLC pieces; `<map>@ws<item id>` needs the Workshop item subscribed on that Steam account; an Xbox-format map or a Custom Edition map needs its file in the data folder. The server never sends MCC's maps or Workshop items to players. Players who do not have them need their own copy.
- Open the firewall first. Internet play needs only **UDP 2302**. The game's LAN ports (UDP 5150, 5151, 5160, TCP 5150, 5153) are for your own network and stay off the internet. The web page is on `127.0.0.1` and stays there.
- Run it as an account that has nothing else in it (no Steam login you play on, no SSH keys), or in a container. The server reads packets from anyone who joins.

A server started from the game (HOST A SERVER in the menus, on Linux and Windows) is found on the local network, by its invite link and by its address (JOIN BY ADDRESS); the master servers never list it, because it has no publisher key. A listing is for the packages below: `halo --server --keygen` makes the key, and the address players use must reach the machine.

| Your case | Take | Where |
| --- | --- | --- |
| A Linux machine or VPS, with or without a gaming Steam on it | **1. Linux package** (tar, systemd unit) | `ae-server-preview2.4-0.7.7-linux64.tar.gz` |
| The same, and you like containers | **2. Container** (Dockerfile, compose) | in the same package |
| A Windows PC that has MCC (your gaming PC) | **3. Windows package** (zip, start .bat, startup task) | `ae-server-preview2.4-0.7.7-windows64.zip` |
| A VPS with no Steam | **4. Linux package + `sync-mcc.sh`** from the PC that has MCC | in the Linux package |

## 1. Linux machine or VPS (the package)

Needs Debian 12 or any 64-bit Linux with glibc 2.35 or newer. The package carries its own SDL3.

```
tar xzf ae-server-preview2.4-0.7.7-linux64.tar.gz -C ~
cd ~/ae-server
nano rotation.txt
sh start-server.sh
```

To keep it running: `ae-server.service` is a hardened systemd user unit (`GUIDE.md`, "Run as a service"). Give the server its own account (`sudo adduser --disabled-password aeserver`), and a read-only view of MCC if it is on the same machine (a read-only bind mount of `steamapps/common/Halo The Master Chief Collection` and `steamapps/workshop/content/976730`; `HALO_MCC_PATH` names the first). Firewall: `GUIDE.md`, "Ports and firewall" has nftables, ufw and iptables lines.

## 2. A container

The package has a `Dockerfile`, `container-start.sh` and `compose.yaml`. The container runs as uid 10001 with no capabilities, no new privileges and a read-only root; the program and Arena Evolved's weapon data are in the image, everything the server writes is in one volume, and your Steam library is mounted read-only. Only UDP 2302 is published.

```
cd ~/ae-server
STEAM=$HOME/.local/share/Steam docker compose up -d --build
docker compose logs -f
docker compose exec ae-server cat /var/lib/ae-server/data/first-admin-password.txt
```

`STEAM` is the folder that holds `steamapps/`. Do not publish the LAN ports: Docker's published ports go around ufw. For a game on a home network only, use `network_mode: host` in place of `ports:`, and firewall the host. The web page is also only reachable from the host with `network_mode: host`.

## 3. A Windows PC that has MCC

Unzip `ae-server`, edit `rotation.txt`, double-click `start-server.bat`. The server finds your Steam copy of MCC the way the game does. Run `firewall-rules.bat` as administrator once. `install-task.bat` (administrator) makes a scheduled task that starts it with Windows; it runs as SYSTEM, so put `HALO_MCC_PATH` in `server-settings.bat` first. A Windows service (`sc create`) is not offered: `halo.exe` does not speak the service protocol. `GUIDE.md`, "Windows home PC", has all of it.

Hosting from the PC you play on works, but the server and a running game both want UDP 5150: stop the server before you play on that PC, or give it its own address (`HALO_NET_ADDRESS`).

## 4. A VPS with no Steam

Steam is not needed on the server, only MCC's files. `sync-mcc.sh` (in the Linux package) copies them from a PC that has MCC and Steam to the server, with the Steam folder shape the server expects, and only what it needs: the Halo CE maps (about 1.4 GB, the same files the game checks for) and the Workshop items your rotation names.

```
sh sync-mcc.sh --from "$HOME/.local/share/Steam" --to aeserver@vps.example:/srv/ae-steam --rotation rotation.txt
```

On the server, set `HALO_MCC_PATH="/srv/ae-steam/steamapps/common/Halo The Master Chief Collection"` (in `start-server.sh`, or `STEAM=/srv/ae-steam` for the container) and restart. Run the script again after you subscribe to a Workshop item or Steam updates one. `--dry-run` lists what it would copy. On Windows it runs in Git Bash. The source is only read.

MCC and its Workshop items belong to their owners, and to you through your Steam account: copy them only to a server you run for yourself.

## Which maps and gametypes

`rotation.txt` takes one entry per map and gametype, `map:gametype`, with Arena Evolved's own gametypes (`ae_team_sly`, `ae_ctf`, ...) and your saved gametypes. `GUIDE.md`, "Rotation lines", lists the forms. Arena Evolved players can join any entry; players on OpenCE or ChupathingyCE (network version 41) can join the entries that use Halo CE's own gametypes and maps they have ("Who can join").
