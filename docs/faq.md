# Halo CE: Arena Evolved: FAQ

Short answers for people who have never heard of Arena Evolved. For the full steps, see [How to play](how-to-play.md) and
[Host a server](host-a-server.md). What is not finished yet is in [Known issues](known-issues.md).

## 1. What do I need to play?

Your own copy of Halo: Combat Evolved, in one of two forms:

- **Halo: The Master Chief Collection on Steam**, with its **Halo: Combat Evolved Anniversary** and **MCC - H1: Multiplayer**
  content both installed. Arena Evolved finds the install by itself and plays MCC's Halo CE multiplayer maps and the
  maps of the Steam Workshop items you subscribe to. Nothing is copied or converted.
- **A disc image (`.iso` or `.xiso`) you made from your own Xbox disc** of Halo: Combat Evolved. The first start copies
  its maps out (about 2 GB).

You can have both. Windows 10 or 11 (64-bit) and most Linux distributions are the supported systems.

## 2. Do I need MCC? Can I use only a disc image?

No, you do not need MCC. A disc image is enough for the original Xbox maps, community maps and mods, and for joining
games hosted by players on other builds (OpenCE, ChupathingyCE). MCC adds MCC's own maps and every Workshop map you
subscribe to, and MCC's medal pictures.

With MCC alone you have MCC's maps, not the original Xbox ones, so you cannot join a game on an original Xbox
map (for example one hosted by an OpenCE player). Add a disc image to join those.

On a Mac there is no MCC, so a disc image is the way in.

## 3. Do I have to buy anything?

Yes, the game. Arena Evolved contains none of Halo's maps, sounds or art: you need your own copy (question 1). Arena
Evolved itself is a non-commercial fan project and must never be sold. It is open source (AGPL-3.0), and each release
has its complete source attached.

## 4. Can you give me the game files, a disc image or maps?

No. Arena Evolved never provides, links to or helps find game files, and issues asking for them are closed.

## 5. Windows says "Windows protected your PC" or my antivirus flags the program. Is it safe?

Windows shows that warning because `halo.exe` is not code-signed, so Windows does not know the publisher. The same can
happen to `ffmpeg.exe`, which the Windows zip includes for video recording.

What you can do:

1. Right-click the zip, choose **Properties**, tick **Unblock**, then unpack it. This avoids a warning on every file.
2. On the warning, choose **More info**, then **Run anyway**.
3. If your antivirus quarantines `halo.exe` or `ffmpeg.exe`, restore it and add the Arena Evolved folder as an
   exception, or do not run it if you are not comfortable. Unsigned programs are flagged now and then by mistake.
4. You can check the zip's SHA-256 against the value in the release's `updates.json`. Every release also has the
   complete source, so you can read it or build it yourself.

Unpack the zip into a folder you own (for example in Documents), not into Program Files: the game keeps its settings,
screenshots and log next to the program.

The game never updates without asking. Public game lists and stats are off until you accept them. Crash reports are
never sent.

## 6. Can I play with Master Chief Collection players?

No. Arena Evolved reads MCC's files from your own install, but it does not connect to MCC's online play. You play with
other Arena Evolved players, and with players on compatible builds (question 7). A game on an MCC map needs every
player to have MCC (and, for a Workshop map, the same Workshop item).

## 7. Can I play with OpenCE or ChupathingyCE players?

Sometimes, depending on the build. Arena Evolved 0.7.4 uses **network version 39**.

- **ChupathingyCE:** yes, both ways.
- **OpenCE builds 282 to 333:** yes. These use network version 39 too.
- **OpenCE build 334 and newer** (network version 40 and 41): not yet. They cannot join Arena Evolved and Arena Evolved cannot
  join them. Merging the newest builds is planned.
- **Arena Evolved 0.4.3 and older:** no.

Two more limits:

- Arena Evolved's extras (weapon packs, SILENT MOVEMENT, Bomb and the rest) are for Arena Evolved players. To meet a
  player on another build, host one of Halo CE's own gametypes (the STOCK folder).
- To join a game on an original Xbox map you need the Xbox maps, so a disc image (question 2).

The game's lobby and server list tell you why a join fails ("Host is on version N and needs to update to M"). The top of
`debug.txt` says your network version.

## 8. How do I host a game?

1. **MULTIPLAYER > CREATE GAME.** A new lobby starts OFFLINE (this PC only).
2. Set **NETWORK**: **LAN** for players on your network, **HOST ONLINE** for the internet.
3. Pick a map and a gametype, set **BOTS** if you want them, and choose **START GAME**.
4. With HOST ONLINE, press **INVITE** to copy a link (`halo://join/...`) and send it to your friends. They choose
   **JOIN WITH INVITE** and paste it. **PUBLIC** lists your game in the server browser (it also needs LIST GAMES I HOST on).

Players who lack a Custom Edition or community map you play get it from you as they join. Turn any VPN off while you host
online. Friends who know your address can use **JOIN BY ADDRESS** (press Y on JOIN LAN GAMES or SERVER BROWSER).

Dedicated servers are for Linux; see the "Host a server" guide.

## 9. LAN or internet? Why can nobody see my game?

- **LAN:** every PC on the same network (not a guest Wi-Fi). Windows asks the first time the game opens the network: allow it.
  On Linux a firewall blocks LAN games silently; open the ports once:
  `sudo ufw allow 5150:5151/udp`, `sudo ufw allow 5160/udp`, `sudo ufw allow 5153/tcp`.
- **Internet:** INTERNET PLAY and UPNP PORT FORWARDING on (they are by default), and turn any VPN off while you host.
  If your router has no UPnP, forward one UDP port by hand (see the "Host a server" guide).

## 10. The SERVER BROWSER is empty.

Two things to check:

1. It needs **SETTINGS > ONLINE > INTERNET PLAY** and **COMMUNITY SERVICES** on. At the first start a page called COMMUNITY
   SERVICES asks; with **DECLINE** the browser lists nothing. Turn it on in SETTINGS > ONLINE if you want it.
2. Only games whose hosts list them appear. Hosts opt in (CREATE GAME > PUBLIC, and LIST GAMES I HOST on the community page).
   A quiet hour can mean no listed games. You can always join a friend with an invite link or JOIN BY ADDRESS.

## 11. Does it work with a controller?

Yes. The menus are controller-first and work with a mouse and keyboard too. Many button layouts (including MCC's and a CUSTOM
layout per player), DualSense adaptive triggers and lightbar, Nintendo and PlayStation prompts, gyro aiming, stick dead
zones, back paddles. Up to four players can play on one PC: each extra player presses **START** on their controller in
MY PARTY (in the lobby). Keyboard and mouse have KEY BINDINGS under SETTINGS > CONTROLS.

## 12. The first-start window asks for a disc image, or says a piece of MCC is missing.

- "Found MCC but it is missing ...": in Steam, open your Library, right-click **Halo: The Master Chief Collection**, choose
  **Properties > DLC** and make sure **Halo: Combat Evolved Anniversary** and **MCC - H1: Multiplayer** are both ticked.
  Let Steam finish, then **Installed Files > Verify integrity of game files**, and start Arena Evolved again.
- If Steam keeps MCC somewhere unusual, press **Choose MCC folder** and pick the folder that holds `halo1`.
- No MCC at all: choose **Xbox disc image** and pick your `.iso` or `.xiso`. The copy takes a few minutes and about 2 GB.
- A new Workshop subscription shows after you restart Arena Evolved.

## 13. Where is `debug.txt`?

In the game's folder: next to `halo.exe` on Windows or `halo` on Linux (for Quiver, the game's folder inside Quiver's Apps
folder). It is plain text. It holds your graphics driver name and file paths, and on Windows paths with your Windows user
name, so look through it before you share it. The file keeps earlier runs, so attach it right after the problem.

Your settings (`config.toml`) are next to the program. Saves and profiles are in `~/.local/share/halo-linux` on Linux and
`%APPDATA%\halo` on Windows.

## 14. How do I report a bug?

Open an issue in this repository's **Issues** tab and choose **Bug report** (or **Crash or freeze**). Please give:

- your system and the version (it is in the window's title, for example 0.7.4-beta);
- whether the game uses MCC, a disc image or both;
- what you did, what you expected and what happened;
- `debug.txt` (question 13).

One bug per issue. Do not attach maps or disc images. Questions and ideas go in Discussions.

## 15. Does it run on Mac, Steam Deck or Android?

- **Mac (Apple silicon, macOS 13 or later):** built with every release but not tested by the Arena Evolved team. You need a
  disc image, because MCC does not run on a Mac. The game keeps its maps, settings and `debug.txt` in
  `~/Library/Application Support/ChupathingyCE` (Finder: Go > Go to Folder). In 0.7.4 the Mac download lacks the weapon
  packs, voices and sounds: get `arena-evolved-0.7.4-beta-mac-extras.zip` from the release and put its folders there.
  Macs only open the download page when there's a new version; download it and replace the app.
- **Steam Deck / Linux handhelds:** the Linux download is the one to use. If the first-start window does not open in Game
  Mode, start it once from Desktop Mode.
- **Android:** an untested build is attached to releases.
