# Playing over LAN

Back to the [README](../README.md).

Every computer must be on the same network: the same router or Wi-Fi, and not
a guest Wi-Fi, which keeps devices apart. LAN games are found through the
network's broadcasts and played over UDP ports **5150**, **5151** and **5160**,
and every computer, host or not, needs them open. Custom Edition maps a player
lacks are sent over TCP port **5153** (map sharing, below the game's own
traffic), which every computer needs open too, to receive a map and to help
send one.

**What is shared.** A host sends the maps you lack: Custom Edition maps, and
(between Arena Evolved players) community maps in the Xbox's format, from its
`maps` folder or its active mod. You get the map the game is on when you join,
with no question, and in the lobby the host's other maps you lack follow on their own, one
after another; a card at the top of the screen shows which map is coming and how much of it is here. Leaving the game
stops them, and so does turning DOWNLOAD MAPS off.
Community maps go to `maps_shared`, Custom Edition maps to `maps_ce`.

**How fast.** A host whose players are all on the local network sends maps at up to 16 MB/s
(`network.map_share_lan_upload`); a game with a player on the internet keeps 1 MB/s (`network.map_share_upload`), so
the host's upload is left for the game.

**What is never shared.** The game's own maps (its 13 multiplayer maps, the
campaign and the menus): they are never sent, and a download never replaces
them or any other file you have. One host can give you 4 GB at most.

**Internet games.** Maps are shared over the internet only when both switches
are on: the host's INTERNET SHARING (off at first) and the joining player's
INTERNET DOWNLOADS (on at first), both in Settings > Network Setup > Map Sharing.
On a local network, sharing is on by default.

**Windows** asks the first time the game starts. Allow it, on private
networks at least, and on public ones too if Windows calls your network public.

**Linux has no such question.** If a firewall is turned on, it silently
blocks LAN games: nobody sees your game, or you see nobody's (Join Game > LAN
says so when it has found nothing for 10 seconds and finds ufw or firewalld
on). Open the ports once on each Linux computer:

- Ubuntu, Zorin OS, Linux Mint, Pop!_OS (ufw): `sudo ufw allow 5150:5151/udp`,
  `sudo ufw allow 5160/udp` and `sudo ufw allow 5153/tcp`. Or, in the Firewall
  Configuration app (Gufw): Rules, +, Advanced, protocol UDP, port `5150:5151`,
  then again for `5160`, then protocol TCP, port `5153`.
- Fedora and others with firewalld:
  `sudo firewall-cmd --permanent --add-port=5150-5151/udp --add-port=5160/udp --add-port=5153/tcp`
  and then `sudo firewall-cmd --reload`.
- Not sure whether a firewall is on? `sudo ufw status` says `inactive` when
  ufw is off; `systemctl is-active firewalld` says `inactive` when firewalld is.

Turning the firewall off also works, but opening the ports keeps it on for
everything else.

