# The dedicated server: reference

`halo --server` runs the game as a host with no window, sound or player of
its own: other machines join it on the LAN, by its invite link or from the
server browser, as they join any host. `port/linux/README.md`, "Dedicated
server", says how to run one; this document is the reference for what it
takes and answers: the command line, the exit codes, the commands, the
HTTP API, the remote console (RCON) over WebSocket, the events, the log and
the files. `docs/master_protocol.md` is the master servers' protocol, which
the server publishes to and the server browser reads.

The code is `port/linux/src/server_*.c` (the platform side: the mode, the
standard input, the log, the rotation's grammar, the HTTP and WebSocket
server, the master servers' publisher) and `port/linux/game/server_*.c`
(the game side: the director that plays one game after another, the
commands, the gametypes, the status). The Android and web builds have no
dedicated server: they compile those files to nothing, but for
`server_json.c` (the JSON reader, which the server browser's list of master
servers uses in every build) and `server_rotation.c` (the rotation's
grammar and the gametype rules), which every build compiles.

## Panels and fleets

A game panel (Pterodactyl, PufferPanel, MCSManager: `deploy/`) runs one
server and reads its standard output; `deploy/systemd` and `deploy/docker`
run servers on one machine. For many servers on many machines, `opence-fleet`
([github.com/OpenCommunityEdition/opence-fleet](https://github.com/OpenCommunityEdition/opence-fleet))
is one control plane over them all: an agent on each machine runs the
servers and drives each through this document's API, as the owner from
the machine itself (`server.web_owner_password`, below). What it relies on
is named in the status's and the RCON's `capabilities`: `owner_web` (the
owner's role over the web, from loopback), `ban_address` (`sv_ban_address`,
`POST /api/bans`), `stop` (`sv_stop`, `POST /api/stop`, the `drain` event),
`event_addresses` (addresses in the player events) and `identity` (the
status's `pid`, `instance`...).

## Command line

| Argument | What it does |
| --- | --- |
| `--server` | Dedicated server mode (as `HALO_SERVER=1` or `server.enabled = true`) |
| `--config <path>` | The settings file; default `config.toml` beside the executable. `HALO_CONFIG_FILE`, when it is set, wins over `--config` |
| `--version` | Prints `OpenCE build-<n> (network version <v>)` and exits 0 |
| `--keygen` | Writes a new publisher key (`server.publisher_key_file`) unless one exists, prints its public key and key id, exits 0; exits 3 when the key cannot be made, read or written |
| `--sign <text>` | Prints the publisher key's Ed25519 signature of the text, in hex (an application's proof of key), exits 0; exits 3 without a text or a usable key |
| `--probe [<invite>]` | A look at a server, for a list site that shows its state: joins nothing and never starts a server (whatever `--server`, `HALO_SERVER` or `server.enabled` say). It finds the invite's game over its tunnel (without an invite, the LAN's first) and prints it as one line of JSON, the game of the invite's host alone (never another game that answers), and exits 0; when nothing answers in 30 seconds it prints `{"ok":false,"error":"no game answered in 30 seconds"}` and exits 1. Internet play is as set (`network.online`, on by default), which an invite needs |
| `--help` | Prints these, exits 0 |

`--version`, `--help`, `--keygen`, `--sign` and `--probe` write no settings: the
settings file is read, never completed or made. `--probe` wins over
`--server`, `HALO_SERVER` and `server.enabled`: a probe never writes to
`config.toml`.

A probe's line:

```json
{"ok":true,"name":"OpenCE Server","map":"bloodgulch","gametype":"ctf","players":5,"max_players":16,
 "teams":true,"in_progress":true,"open":true,"dedicated":true,"network_version":38,"kind":0}
```

`gametype` is the game engine's name (`ctf`, `slayer`, `oddball`, `king`,
`race`, or `none`), `kind` the OpenCE gametype's kind the host sends (0 for
none), `open` whether it takes players now.

The release has a `halo-server` script beside `halo`, which runs
`halo --server` with its arguments.

## Exit codes and signals

| Code | Meaning |
| --- | --- |
| 0 | Stopped: `quit`, `sv_stop` (now, or at the end of its drain), SIGTERM or SIGINT |
| 1 | A fatal error in the game (a halt, a map that failed to load); the last error lines were logged. The process exits within 5 seconds of the error, even when what runs at the exit (a hook, a listing's withdrawal) does not return |
| 2 | No usable game data: `maps/ui.map`, or every rotation map, missing or not a cache file; nothing was bound |
| 3 | Bad server configuration: no rotation entry that can be played (and `server.rotation_fallback` off), `server.web_port` past 65535, a web port the server may not bind (a port below 1024 without the right to it) or whose `server.web_bind` is an address the machine does not have, or a web password (admin, moderator or viewer) shorter than 8 characters with the web port bound beyond loopback (12 beyond loopback with no `server.web_trusted_proxy`; on loopback, a warning), or the owner's (`server.web_owner_password`) shorter than 8 wherever it is bound; `--keygen` or `--sign` failing. An entry that cannot be played (a bad gametype file among them) is left out with a warning: the server stops only when none is left |
| 4 | A port in use: the tunnel's UDP port, or the web port (another program listens on it) |
| 5 | Restart asked for (`sv_restart`, now or at the end of its drain, or an update installed: `server.auto_update = "install"` (set it; the default is `check`) installs a new release as soon as a check finds it, on a thread of its own, the server going on meanwhile. Then, of a release of the same network version, it stops with 5 once the server has been empty for 10 minutes; of a new network version (one its players' games no longer join), it says "This server updates after this game" and stops with 5 when the game ends (in the lobby, after 30 seconds' notice), a game going on past `server.update_game_minutes` (30) since the install being ended as `sv_end_game` ends it (`game_ended`'s reason `update`). An install that fails is tried again at the next check: "Updates", below) |
| 128 + n | Stopped by signal n: a second SIGTERM or SIGINT, or the shutdown not done 5 seconds after the first |

SIGTERM and SIGINT stop the server: a game in progress goes to its
postgame, every machine is told, the listings are withdrawn, and it exits 0.
SIGHUP reloads the server's settings, the rotation and the gametype files
(`sv_reload`). SIGUSR1 prints the status on standard output. SIGPIPE and
SIGTTIN are ignored.

## Standard output

One line per event, for the operator and the panel:

```
2026-10-19T18:03:11Z INFO server: ready (tunnel 2302/udp, web 127.0.0.1:8080, listing public)
2026-10-19T18:03:40Z INFO player: Milenko joined (slot 0, machine 1)
2026-10-19T18:04:02Z INFO game: started: CTF on bloodgulch, 2 players
```

The fields are the time (UTC), the level (`INFO`, `WARN`, `ERROR`), the
category (`server`, `lobby`, `game`, `player`, `chat`, `moderation`, `net`,
`master`, `console`, `health`) and the text. `server: ready` is printed once,
when the lobby is open and, for a public server, its listing has been
published (or 10 seconds on, as the brokers are other people's): it is the
line a panel waits for. With `server.log_format = "json"` each line is an
object instead, and the line a panel waits for holds `"msg":"ready (`:

```json
{"time":"2026-10-19T18:03:11Z","level":"info","cat":"server","msg":"ready (tunnel 2302/udp, ...)"}
```

Every line is also kept in the log the web's viewers read (`/api/log`,
`sv_log`, the RCON's `log` events), but for these, which go to standard
output alone: the web password a first start makes, the invite (a
private server's lets anyone in; `/api/status` gives it to admins), and
the owner's console, the commands typed on standard input or read from the
init file and what they answered (a password's command is shown as `(a
password's command)` there too).

A health line comes once a minute: `health: players=3 frame_ms_max=4.1
frame_ms_avg=1.20 cpu=2.1% rss=96MB` (the longest and the mean frame of
the last minute, in milliseconds). Standard error keeps the platform
layer's `halo-linux:` lines, and `debug.txt` everything, as for a player's
game.

## Commands

The same commands come from standard input, `server.init_file` (run once at
the start, a command a line, `#`, `;` and `//` starting comments), the telnet
console (`debug.telnet_console`, loopback), the RCON, `POST /api/command`
and the web page. Each runs on the game's thread between frames. Every
command has a role; a caller may run it if their role is that one or above:
**viewer** < **moderator** < **admin** < **owner** (standard input, the
init file and the telnet console are the owner's: whoever holds the
process; so is a sign-in to the web or the RCON with
`server.web_owner_password` from the machine itself: "Signing in"). When
standard input ends and `server.console_exit_on_eof` is on, its last line
runs, whether or not a line's end ended it, and then the server stops.

A `<player>` is a slot number (`sv_players` lists them) or the start of a
name; a name that begins more than one player's is refused. A `<duration>`
is a number and a unit (`45s`, `30m`, `2h`, `7d`, `1w`), a bare number of
minutes, or `forever`; ten years at most. A word in quotes (`"two words"`)
is one word.

A command's text (a notice, a reason, `sv_set`'s value, a name) is kept to
printable ASCII: control characters (a line's end, a tab) and other bytes
outside it are dropped, as are the quotes around the whole text, and
standard input's and the init file's lines are cleaned so before they run.
The web is stricter where it takes a value as it is: `PUT /api/settings`
refuses a value with a control character (`schema`), and nothing is changed
for that setting. A command's reply is sent whole, however many lines.

Commands that are not the server's (the game's own console commands, the
scripts' globals and functions) need the owner role: from the web or the
RCON they are refused (`not a command of the server's`) but to the owner's
session. The server's
commands come first, so `kick` and `ban` are always the server's below, not
the game's console commands of those names.

| Command | Role | What it does |
| --- | --- | --- |
| `help [command]` | viewer | The commands, or one command's use |
| `sv_status [--json]` | viewer | The server's state, map, gametype, players, uptime, games played, network version, listing and next game |
| `sv_players [--json]` | viewer | Each player's slot, name, team, score, kills, deaths, assists, ping and machine; with the owner role or `server.log_addresses`, their address and hardware id |
| `sv_rotation` (`sv_mapcycle`), `sv_maps`, `sv_gametypes`, `sv_version`, `sv_log [n]` | viewer | Lists: the rotation, the maps there are, the gametypes, the build, the log's last n lines |
| `sv_bans` (`sv_banlist`), `sv_settings` | admin | Lists: the bans of `bans.txt`, the settings `sv_set` changes |
| `sv_masters`, `sv_publisher` | viewer | The master servers and how each answered; the publisher key's id and public key (never its seed) |
| `say <text>` (`sv_say`) | moderator | A notice on every machine's screen |
| `tell <player> <text>` | moderator | A notice on one player's machine |
| `kick <player>` (`sv_kick`) | moderator | Drops the player's machine; it may join again. A player still in the game whose machine has gone is taken out (a ban of one is refused: there is nothing to ban) |
| `sv_team <player> <red\|blue>` | moderator | Moves the player to that team: they die now, not counted (a round game: at their next death) (0.7.7) |
| `ban <player> [<duration>] [<reason>]` (`sv_ban`) | moderator (a day at most), admin | Drops the player's machine and bans its address and hardware id, for the duration (else for ever). A moderator must give a duration of a day or less: a ban for ever, or longer, is refused |
| `unban <number\|address\|hardware id>` (`sv_unban`) | admin | Takes a ban out of `bans.txt`; the machine may join again at once, by the LAN or the internet |
| `sv_ban_address <address\|hardware id\|discord:<id>> [<duration>] [<reason>]` | admin | Bans an IPv4 address (`a.b.c.d`), a hardware id (up to 32 hex digits) or a verified Discord account (`discord:` and its id, below), its machine here or not: its line in `bans.txt` as `ban` writes it (the other part `none`), so that `sv_bans`, `unban` and `GET /api/bans` see it, and a machine present of that address or hardware id is dropped as `ban` drops it (`player_left`'s reason `banned`). Answers `ban <n> added` (`n`: its number in `sv_bans`) |
| `sv_vpn` | viewer | The VPN policy, the netlist held (its masters, serial, ranges, age) and the joins refused for it ("VPNs and linked Discord accounts") |
| `sv_vpn_check <address>` | moderator | Whether an IPv4 address is refused as a VPN's, and by what (`netlist`, `vpn_block.txt`, `vpn_allow.txt`) |
| `sv_vpn_fetch` | admin | Fetches the masters' netlists now |
| `mute <player> [<minutes>]`, `unmute <player>` | moderator | The host's mute of a player's chat and voice for everyone |
| `sv_map <map> [<gametype>]`, `sv_map <entry>` | moderator | Plays that game (or a rotation entry, `map:gametype+rule=value`; a co-op one too), below |
| `sv_map_next` (`sv_next`) | moderator | Ends the game without its scores and plays the rotation's next |
| `sv_end_game` | moderator | Ends the game: its scores, then the next |
| `sv_rotation_set <entries>`, `sv_rotation_add <entry>`, `sv_rotation_del <n>` (`sv_mapcycle_add`, `_del`) | admin | Changes the rotation (written to `server.rotation`, 4096 characters at most) |
| `sv_rotation_begin [<n>]` (`sv_mapcycle_begin`) | moderator (0.7.7; `sv_rotation_begin admin` in `command_access.txt` locks it) | The rotation's next game is that entry; the game played now goes on. A moderator's UP NEXT in the game |
| `sv_set <setting> <value>` | admin | Changes one of the server's settings, now where it can be, and in `config.toml` (below); a setting its environment variable sets is refused. A value is 256 characters at most |
| `sv_name <name>`, `sv_maxplayers <n>`, `sv_password [<password>]`, `sv_public <0\|1>`, `sv_motd <text>` | admin | `sv_set` of `server.name`, `server.max_players`, `server.password`, `server.public`, `server.motd` |
| `sv_reload` | admin | Reads `config.toml`'s server settings, the rotation and the gametype files again (as SIGHUP) |
| `sv_stop [<seconds>\|now\|game\|empty] [<notice>]`, `sv_stop cancel` | owner | Stops the server (exit code 0): after `<seconds>` with a warning to the players ("Restart warnings", below); `now` as `quit` does; with none of them, after `server.restart_warning_seconds` (60 by default; 0 is now); `game` once the game being played is over, `empty` once nobody plays (a drain, below); `cancel` the countdown or the drain waiting. The notice is said to every machine first |
| `sv_restart [<seconds>\|now\|game\|empty] [<notice>]`, `sv_restart cancel` | owner | As `sv_stop`, with exit code 5 (restart me) |
| `sv_key_add <name> <viewer\|moderator\|admin> [<days>] [<allow>]`, `sv_key_remove <name>`, `sv_keys` | admin | Makes, revokes and lists API keys ("API keys", below). The secret is shown once, in the reply |
| `sv_backups` | admin | Lists the backups of `config.toml`, `bans.txt`, `api_keys.txt`, `admins.txt`, `command_access.txt` and `whitelist.txt` ("Backups", below) |
| `sv_admins`, `sv_admin_add <name:login\|cidr:list\|key:hex> <role> [<immunity>] [<days>] [<allow>]`, `sv_admin_remove <identity>` | admin, owner, owner | Lists, makes and takes out the admin rows ("Admins, access and the whitelist", below). A login's secret is shown once, in the reply |
| `sv_access` | admin | The commands whose role `command_access.txt` changes |
| `sv_whitelist [list]`, `sv_whitelist add <entry>`, `sv_whitelist remove <entry>`, `sv_whitelist on\|off` | admin | The whitelist (`ip:`, `hwid:`, `discord:` or `name:` entries) and `server.whitelist` |
| `sv_ban_range <a.b.c.d/n> [<duration>] [<reason>]`, `sv_ban_name <pattern> [<duration>] [<reason>]` | admin | Bans a network, or the player names a pattern matches (`*` any run, `?` one character, in either case) |
| `sv_bans_export [<file.json>]`, `sv_bans_import <file.json>` | admin | Writes the bans as JSON to a file of the data root (`bans-export.json` by default), or adds those of such a file; a ban already there is skipped, a bad one refused |
| `quit` (`exit`) | owner | Stops the server (exit code 0) |

`sv_map`'s game is played as soon as it can be, and its answer says when:
`next: <entry> (now)` in the lobby before its start was sent (the lobby
takes the entry, and the rotation's entry it replaced is played after it),
`(now: this game ends)` while a game is played (it ends at once, without
its scores), and `(once this game is over)` once the lobby's start was sent
or while a map loads (it is the next game). `sv_map` of the game being
played plays it again. The rotation goes on after it.

A drain (`sv_stop` or `sv_restart` with `game` or `empty`, `POST
/api/stop`) is a stop that waits: the listings are withdrawn as `sv_public
0` withdraws them (a new invite, the masters told), without the setting
changing; new machines are refused as while a map loads (the game is not
open: the LAN's search shows it closed, a join by the invite is told the
game is not open); then the server stops. With `game` it stops once no
game is played nor about to be: at once in the lobby with nobody in it, or
with no countdown running (fewer players than `server.min_players`);
otherwise once the game played (or about to start) is over and its
postgame shown. With `empty` it stops once nobody is on the server (at once
if nobody is now). `sv_stop cancel` (or `sv_restart cancel`) ends the
drain: the server is listed again as `server.public` says and takes new
players. A `drain` event says each (`after`: `now`, `game`, `empty` or
`cancel`), and the stop that ends a drain has the `shutdown` reason
`drained`. `/api/status`'s `drain` is the drain waiting, or `null`.

### Restart warnings

`sv_stop` and `sv_restart` with a delay (`sv_restart 90 updating`, `POST
/api/stop` with `"seconds": 90`, or none given: `server.restart_warning_seconds`,
60 by default; 0 stops at once, as `now` does) count down before they stop
the server, so the players can finish what they are doing. The players read a
chat notice as the countdown starts, then at 60, 30, 10 and 5 seconds left
(each only if the time left is still above it), with the notice as the reason:
`Server restarting in 30 seconds: updating.` Players whose chat is not on
the server's Delta wire (OpenCE's and Chupa's own clients) read it as a
game notice, in a game. When the time is up the server stops through the
same path as `sv_restart now` (the `drain` event with `after` `now`, exit code
5 for a restart, 0 for a stop). `sv_restart cancel` (or `sv_stop cancel`, or
`POST /api/stop` with `"cancel": true`) ends the countdown and tells the
players it is cancelled; a drain (`game` or `empty`) replaces a countdown.
`POST /api/stop` takes `"seconds"` (0 to 3600, a whole number) with `restart`
and `notice` as before; `"after": "delay"` without `seconds` uses the setting.
Without `seconds` or `delay`, `after` is `now`, `game` or `empty` as it was.

`server.restart_daily` (`"HH:MM"`, 24 hours, the machine's time zone; empty
by default: none) restarts the server every day at that time. The countdown
begins `server.restart_warning_seconds` before it (at least 1 second), says
`daily restart` as its reason, and ends with exit code 5; a service manager
starts the server again. A server younger than 30 seconds does not begin
one, and a cancelled one is not begun again for the same time. A bad
`HH:MM` is ignored. These settings are changed by `sv_set` as well:
`server.restart_warning_seconds` takes 0 to 3600.

A co-op entry (`<level>:coop`) is ended as a game is: by `sv_end_game`,
`sv_map_next`, `sv_map`, `server.empty_seconds` and `server.idle_minutes`.
It has no scores: the server goes back to the lobby at once (no postgame),
and a level won ends it with `game_ended`'s reason `score`.

`sv_set` (and the settings of the web page, `PUT /api/settings`) changes
these settings, which `sv_settings` and `GET /api/settings` list:
`server.name`, `server.max_players`, `server.min_players`,
`server.password`, `server.public`, `server.rotation_fallback`,
`server.lobby_seconds`, `server.postgame_seconds`, `server.empty_seconds`,
`server.idle_minutes`, `server.team_start`, `server.team_switch`,
`server.team_switch_cooldown_seconds`, `server.motd`,
`server.frame_rate`, `server.web_password`,
`server.web_moderator_password`, `server.log_format`,
`server.log_addresses`, `server.status_file`, `server.auto_update`,
`server.coop_difficulty`, `server.region`, `server.master_publish`,
`server.chat_filter`, `server.chat_filter_slurs`, `server.chat_filter_swearing`,
`server.chat_filter_insults`,
`server.master_publish_address`, `server.metrics`, `server.vpn_policy`,
`server.vpn_fail`, `server.vpn_check_lan`, `server.link_min_age_days`,
`server.link_one_seat`, `server.update_check_minutes`,
`server.update_game_minutes`, `server.max_machines_per_address`,
`server.afk_kick_minutes`, `server.afk_kick_when_full`,
`server.afk_kick_full_minutes`, `server.coop_bsp_switch`,
`server.coop_bsp_switch_cooldown_seconds`,
`server.vehicle_riding_override_gametype`, `server.map_vote`,
`server.map_vote_choices`, `server.map_vote_seconds`,
`server.restart_warning_seconds`, `server.restart_daily`, `server.backups_keep`,
`server.whitelist`, `server.reserved_slots`, `server.chat_flood_lines`,
`server.chat_flood_mute_minutes`, `server.ping_kick_ms`, `server.ping_kick_seconds`,
`server.alert_disk_mb`, `server.alert_empty_hours`, `server.alert_restart_loop`,
`server.alert_webhook` and `server.on_event` (the last two the owner's alone), and
`network.text_chat`,
`network.voice_lobby`, `network.voice_mode`, `network.voice_kbps`,
`network.votekick`, `network.vote_end_game`, `network.votekick_threshold`,
`network.votekick_percent`, `network.votekick_minutes`,
`network.votekick_ban_minutes`, `network.map_share`,
`network.map_share_upload`, `network.public_address`, `network.join_address`,
`network.coop_friendly_fire`, `network.coop_player_collisions`,
`network.coop_ally_friendly_fire`, `network.coop_team_wipe`, `network.shield_effects`,
`network.fire_origins` and `network.vehicle_riding`. Of them,
`server.status_file`, `server.auto_update` and `server.log_addresses` need
the owner role: from the web's admin, `sv_set` of one of them (`POST
/api/command`) answers `403 forbidden`, and `PUT /api/settings` leaves it
out, in its `errors` with the code `forbidden` and the message `<setting>
needs the owner role (the server's console)`; the owner's session changes
them both ways. A web password is empty or
8 characters or more. A value outside a setting's choices or range is
refused (`400 failed` from `sv_set`, `schema` in `PUT /api/settings`'s
`errors`), with `<setting> takes a whole number from <min> to <max>` (or
`from <min>`) or `<setting> is one of <a>|<b>|<c>`; a choice may be given
in either case, and is kept as the table spells it:

| Setting | Choices or range |
| --- | --- |
| `server.max_players` | 2 to 127 |
| `server.min_players` | 1 to 16 |
| `server.lobby_seconds`, `server.postgame_seconds` | 0 to 300 |
| `server.empty_seconds`, `server.idle_minutes` | 0 or more |
| `server.frame_rate` | 30 to 120 |
| `server.team_start` | `one`, `both`, `skip` |
| `server.team_switch_cooldown_seconds` | 0 to 3600 |
| `server.log_format` | `text`, `json` |
| `server.auto_update` | `off`, `check`, `install` |
| `server.coop_difficulty` | `easy`, `normal`, `hard`, `impossible` |
| `network.text_chat` | `on`, `links`, `off` |
| `server.chat_filter` | `off`, `censor`, `block` |
| `server.vpn_policy` | `off`, `link`, `block` |
| `server.vpn_fail` | `open`, `closed` |
| `server.link_min_age_days` | 0 to 3650 |
| `server.update_check_minutes` | 5 to 1440 |
| `server.update_game_minutes` | 0 to 240 |
| `server.max_machines_per_address` | 0 to 128 |
| `server.afk_kick_minutes` | 0 to 60 |
| `server.afk_kick_full_minutes` | 2 to 60 |
| `server.coop_bsp_switch` | `majority`, `anyone` |
| `server.coop_bsp_switch_cooldown_seconds` | 0 to 300 |
| `server.map_vote_choices` | 2 to 8 |
| `server.map_vote_seconds` | 5 to 120 |
| `server.chat_filter_slurs`, `server.chat_filter_swearing`, `server.chat_filter_insults` | `off`, `severe`, `strong`, `mild` |
| `network.voice_mode` | `off`, `team_proximity`, `team_enemy_proximity`, `team_global`, `team_global_enemy_proximity` |
| `network.voice_kbps` | 8 to 64 |
| `network.votekick_threshold` | `dynamic`, `static` |
| `network.votekick_percent` | 1 to 100 |
| `network.votekick_minutes` | 0 to 60 |
| `network.votekick_ban_minutes` | 1 to 1440 |
| `network.map_share_upload` | 16 to 20480 (default 10240 on a dedicated server, 4096 in a game hosted at home) |
| `network.coop_friendly_fire` | `off`, `on`, `shields_only`, `explosives_only` |
| `network.coop_team_wipe` | `auto`, `revert`, `respawn` (`auto`: a public server, `server.public` with internet play, only respawns the team) |
| `network.shield_effects` | `stock`, `glow`, `light_show`, `overshield`, `glow_overshield`, `light_show_overshield`, `players` |
| `network.vehicle_riding`, `server.vehicle_riding_override_gametype` | `true`, `false` (from the next map) |

A dedicated server may also keep words out of its chat and its players'
names (`server.chat_filter`, off by default): the words of streamer mode's
list, by group, each group from the least severity its setting names
(`server.chat_filter_slurs`, `mild` by default: every slur;
`server.chat_filter_swearing` and `server.chat_filter_insults`, `off`).
`censor` shows a line's words as stars to everyone; `block` refuses the
line (its sender is told "Your line was not sent: it has words this server
does not allow") and refuses at the door a machine whose player's name has
them, which an updated game shows as "The host refused: your player name"
(an older one says the game is closed). Each is logged (`chat filter: …`,
`refusing a machine … its name has words the server's filter keeps out`).
Links are `network.text_chat`'s, refused by default. A list of words is
not a moderator: spaced letters and look-alikes get past it.

### Votes, the map vote and players away

The players vote to kick a player (`network.votekick`: the scoreboard's
right-click, or the console's `votekick <name>`) and to end the game
(`network.vote_end_game`: `!endgame` in the chat). While a vote runs every
player sees its box at the left of the screen, and F1 votes yes, F2 no
(`controls.vote_yes`, `controls.vote_no`), or a controller's D-pad left and
right, which then do nothing else (`!yes` and `!no` in the chat do too).
The yes votes needed are `network.votekick_threshold`'s, of the players who
may vote: `dynamic` (the default) scales them, more than half up to 8, then
40% (at least 5) up to 32, then 30% (at least 13); `static` asks
`network.votekick_percent` (50 by default) of them, rounded up, and for a
kick never more than those who can vote yes. At least 2 either way, and
more yes than no; a vote the no votes have made lost ends at once. A vote to end the game may start from 30 seconds played on the
server, not in a game's first minute, and ends it as `sv_end_game` does,
its scores and postgame shown (`game_ended`'s reason `vote`).

`server.map_vote` (off by default) lets the players choose the next game:
as the lobby opens after a game with players in it, the server offers
`server.map_vote_choices` (4) entries of the rotation picked at random (not
the one just played, nor one twice) and, as the last choice, SAME AGAIN (the
game just played, once more: always offered, and marked `same_again` in
`map_vote_started`'s choices), every player's lobby opens its vote
screen, and after `server.map_vote_seconds` (20), or once every machine has
voted, the entry with the most votes is played next (a tie, or no vote, at
random among them); the rotation goes on after it. `sv_map` (an entry
interposed) and a changed rotation set the vote aside. `map_vote_started`
and `map_vote_ended` events say each.

A player away from the controls (no button pressed or let go, no stick or
trigger held, the view not turned) for `server.afk_kick_minutes` (5; 0:
never) is told a minute before and then kicked (`player_left`'s reason
`afk`, an `afk_kick` event), free to join again at once. Only time in a
game counts: not the lobby, the postgame, a cinematic, nor a player dead.
With `server.afk_kick_when_full` a player is kicked only while the game is
full, after `server.afk_kick_full_minutes` (3).

`server.max_machines_per_address` (0: no limit; 3 by default) refuses a machine whose
address (an internet play peer's real one) has that many in the game
already (`join_refused`'s reason `address_full`); an updated game shows
"The host refused: too many players from your address".

In a co-op game a loading zone (a trigger that moves the team to another
part of the level) switches only once more than half of the living players
who are not away stand in zones to the same part (`server.coop_bsp_switch`
`majority`, the default; `anyone`: the first player in one). The others see
"Waiting for N more players to cross"; a mission script's switch is not
held. After a switch the zones wait `server.coop_bsp_switch_cooldown_seconds`
(15) before another, the players told when they open; each switch is told
to everyone, logged and a `coop_bsp_switch` event names who crossed.

### Vehicle riding

`network.vehicle_riding` is the base setting. In multiplayer the
gametype's `vehicle_riding` rule ("Gametype files", below) overrides it:
a gametype without the rule is off, whatever the base says. In co-op a
rotation entry's own `riding` rule overrides it.
`server.vehicle_riding_override_gametype = true` turns the gametype's
override off, so multiplayer uses the base setting.
Both settings default to `false`; changes through `sv_set` or the web
page count from the next map. A map's own choice still wins.

For example (a co-op entry's own rule and a map's choice still win):

| `network.vehicle_riding` | `server.vehicle_riding_override_gametype` | Riding |
| --- | --- | --- |
| `true` | `false` | On in co-op; multiplayer follows each gametype |
| `false` | `true` | Off in multiplayer (unless a map turns it on) |
| `true` | `true` | On in co-op and multiplayer (unless a map turns it off) |

### Changing teams

In a team game a player changes their own team by saying `!switch` (or
`!team`) in the chat, the whole line, in either case. The line goes to no
one, not even when the player is muted: the server moves them, and everyone
sees "Guven switched to the Blue team", or tells them alone why not. The
server decides, and moves a player only

- with `server.team_switch` on (the default);
- while a team game is played: not in the postgame, nor in a game without
  teams ("!switch works only in team games"), nor in co-op, nor in
  Infection, whose teams are its roles ("Infection decides the teams");
- once in `server.team_switch_cooldown_seconds` (30; 0: no wait), each player
  ("You can switch teams again in 12 seconds");
- with the gametype's auto team balance on (`balance` in a gametype file),
  to a team that has fewer players than theirs, them counted: the new team
  is then one player bigger than the old at most. 5 against 4 may become 4
  against 5, but 4 against 4 stays ("Blue has 4 players, Red 4: that would
  unbalance the teams"). With the balance off, a player may switch whatever
  the numbers.

A living player dies as they switch, a death no one is credited with: no
suicide is scored and no kill counted, what they carry (a flag, the ball)
drops, and they respawn on the new team after the gametype's respawn time
(and its suicide penalty). They join the new team as they die, once what
they carried has dropped, never while they still hold it; if another
player kills them first, the kill is that player's and the switch is made
all the same. While a switch waits for its death, the auto team balance's
rule counts the player in the new team. A dead player changes team at once,
and their respawn goes on as it was. The cooldowns start again with each
map. The team scores stay as they were, the points a player scored staying with the
team they scored them for, and the player's own score goes with them.
Every machine sees the new team with the player's next life, as it does the
auto team balance's moves, so a client of any network version takes it.
Each switch is logged (`player`: `Guven switched to the Blue team
(!switch)`); there is no event for it, as there is none for the auto team
balance's moves.

### Updates

`server.auto_update` is `check` by default (`install` when you set it) (the Docker image's, `check`:
its image is updated instead). The server asks GitHub for the latest
release `server.update_check_minutes` (30) after the last check (the first
two minutes after it starts), and when there is a newer one says so (a
`warning` event) and, with `install`, installs it on a thread of its own.
The release's notes tell its network version: one of the same version
restarts the server (exit code 5) once it has been empty for 10 minutes;
one of a new version, which its players' games no longer join, tells the
players "This server updates after this game" and restarts it once the game
ends, in the lobby after 30 seconds' notice, and a game still going on
`server.update_game_minutes` (30; 0: no limit) after the install is ended
as `sv_end_game` ends it, its scores shown.

### VPNs and linked Discord accounts

A dedicated server refuses by default machines that join from the address
of a VPN, a Tor exit or a datacenter, as the master servers' netlist says
(`docs/master_protocol.md`, "The netlist"), and lets one in that shows a
pass of a Discord account linked at a master the server publishes to ("Player
keys, links and passes" there): a player on a VPN links a Discord account
once, and joins. `server.vpn_policy`, live:

| Value | A machine from a listed address |
| --- | --- |
| `link` (the default) | is refused with "This server blocks VPNs: link Discord in Settings, Account to join, or turn your VPN off", unless it shows a pass (which an updated game does at once, by itself, when it holds one) |
| `block` | is refused, pass or not ("This server does not allow VPNs") |
| `off` | joins |

The server fetches the netlist of each master it publishes to (the masters
of `server.masters_file`) every hour, keeps the newest of each on disk
(`netlist-<master key id>.bin` beside `bans.txt`) and checks the union of
them. Until it holds one, `server.vpn_fail` decides: `open` (the default)
lets every machine in, `closed` refuses those from the internet as
`block` would. `server.vpn_list` names a netlist file checked besides
the masters' (a full path, or beside `config.toml`; for a server that
publishes nowhere, or a test), read at the start and by `sv_reload`. Beside
`bans.txt`, `vpn_allow.txt` names ranges never refused and `vpn_block.txt`
ranges refused besides, one a line (`203.0.113.0/24`, `198.51.100.7`, or
`198.51.100.1-198.51.100.9`; `#` starts a comment), read at the start and by
`sv_reload`. Only a machine's real internet address is checked (an
internet play peer's endpoint, `distributed_real_address`): a LAN's
machines are not, unless `server.vpn_check_lan` (for tests) says so.

A pass is taken from the masters the server publishes to alone (with
`server.master_publish` and a publisher key): a server that publishes
nowhere takes none, and a player from a listed address joins it only with
`server.vpn_policy` `off`. With a pass, the server also refuses an account
younger than `server.link_min_age_days` (365 by default: a year, from the
account's id; "This server needs a Discord account a year old to join over
a VPN") and, with `server.link_one_seat` (on by default), a second machine
of an account already in the game ("Your Discord account is already
playing on this server"). A pass that does not check (another master's, out
of date, not this connection's) is refused as "Your Discord link could not
be checked". The machine of a pass is a verified Discord account: its id
and username (`discord_id`, `discord_name`, `discord_verified` in
`/api/players` and `player_joined`, with the addresses: for the owner, or
every role with `server.log_addresses`) replace what
the machine says of itself, and `ban` bans the account too. Each refusal
is logged (`refusing a machine @<address>: vpn …`) and is a `join_refused`
event. An older game is told the game is closed.

A dedicated server's text chat is its `network.text_chat`'s, listed or
not: `on` by default (lines with links refused), `links`, or `off`, live
(`sv_set network.text_chat off`). A game a player hosts has no text chat
while it is listed in the server browser; a dedicated server is the
exception, and the players who join it from the browser's dedicated servers
have the chat as it says, as long as a master server lists it (the
masters' word that it is an approved operator's; a listing's own flag is
not enough, so a dedicated server listed by the brokers alone, before its
operator is approved, has none for them).

The settings `sv_set` does not change (`server.web_viewer_password`,
`server.web_owner_password`, `server.web_port`, `server.web_bind`, `server.web_trusted_proxy`, `server.web_insecure_http`, `server.web_hosts`, the
paths, the master servers' files...) are changed in `config.toml` or their
environment variables: `sv_reload` reads the file's `server.*` settings
again, and the web port and its address are bound once, at the start. The
rotation is changed by the `sv_rotation_*` commands.

## HTTP

The server listens on `server.web_port` (8080; 0 for none) at
`server.web_bind` (`127.0.0.1`, an IPv4 address). Bound beyond loopback,
every web password that is set (admin, moderator, viewer) must have 8
characters or more, else the server exits with code 3; the owner's
(`server.web_owner_password`) must have 8 or more wherever the port is
bound. The page (`/`), the API
(`/api/...`), the RCON (`/ws`), `/healthz` and `/metrics` share the port.

Bound beyond loopback with no `server.web_trusted_proxy` (a TLS reverse
proxy), the password and the session cross the network in clear text: the
server says so at the start (a warning; `server.web_insecure_http` = true
says it is on purpose, on a network you trust), and every web password must
have 12 characters or more (else exit code 3), not 8. A browser's request
(it has an `Origin` or a `Sec-Fetch-Site`) to a `Host` that is not an address,
`localhost` or one of `server.web_hosts` (comma separated names) is refused
(`403 host`): a DNS rebinding page cannot use the server as its own. A request
without those headers (curl, a script) and a trusted proxy's are not asked.

Requests are HTTP/1.1 (or 1.0), `GET`, `HEAD`, `POST`, `PUT` or `DELETE`
(`HEAD` is answered as `GET` is, without the body, and counts as a read,
as a `GET`; `/api/login` takes `POST` alone), a head of 8 KB at most whose
lines end with CRLF (a bare LF, a CR inside a line, or more after the empty
line that ends it: `400 bad_request`) and a body of 64 KB at most, sent within 10 seconds of the
connection's opening or of its last answer (a connection that sends
nothing is closed then too). `Transfer-Encoding` and `Expect` are refused.
24 connections are taken at once; when they are all taken, the oldest that
waits for a request (no RCON, no request being answered) is closed for a
new one. A client that does not read its answers is read no further once
256 KB of them wait, and is closed past 2 MB.

Every request but `/healthz` counts against its address's allowance, the
page's files, `/metrics`, a sign-in and the RCON's upgrades (`/ws`) too: 60
a minute, or 600 with a session (429 `rate_limited` past it, with
`Retry-After: 60`). Behind `server.web_trusted_proxy`, a request's address
is the last entry of its `X-Forwarded-For` (the one the proxy appended: an
IPv4 address, `a.b.c.d` or `a.b.c.d:port`, or an IPv6 one, `v6`, `[v6]` or
`[v6]:port`), and its `X-Forwarded-Host` and `X-Forwarded-Proto` are
believed, whether or not it sends `X-Forwarded-For`; from any other
address they are ignored. IPv6 clients are counted by their /64: one
host's addresses share its allowance and its lockouts. Every answer is JSON
(`Content-Type: application/json`) unless said, with
`Cache-Control: no-store`, `X-Content-Type-Options: nosniff`,
`Referrer-Policy: same-origin` (under `no-referrer` a browser would send
the page's own changes with `Origin: null`) and
`Content-Security-Policy: default-src 'self'; frame-ancestors 'none'`.

### Signing in

`POST /api/login` with `{"password":"…"}` answers
`{"ok":true,"token":"<64 hex>","role":"admin","csrf":"<32 hex>","expires":<unix seconds>}`
and sets the cookie `opence_session=<token>; HttpOnly; SameSite=Strict;
Path=/`, with `Secure` when the request came through
`server.web_trusted_proxy` with `X-Forwarded-Proto: https` (never from
another address, whatever it says). The admin password
(`server.web_password`) gives the admin role, the moderator password
(`server.web_moderator_password`) the moderator role, the viewer password
(`server.web_viewer_password`) the viewer role; `{"by":"…"}` beside the
password names who signed in, in the log. A session lasts 12 hours, or 30
minutes without a request (the RCON's messages do not count: a page left
open asks `GET /api/session` now and then); 16 are kept at most, of every
role.

The owner's password (`server.web_owner_password`, empty by default: none)
gives the owner role, for a supervisor on the server's own machine (a
fleet's agent, a panel's daemon): what standard input may do, over the
API and the RCON (`quit`, `sv_stop`, `sv_restart`, `POST /api/stop`, the
owner's settings by `sv_set` and `PUT /api/settings`, the game's own
console commands by `POST /api/command`, the players' addresses and
hardware ids in `/api/players` and the events). It signs in only when the
connection's own peer address is in `127.0.0.0/8` and is not the trusted
proxy's: never by `X-Forwarded-For`, and never through
`server.web_trusted_proxy`, even one on the machine itself. From any other
address the right owner's password is refused with `403 forbidden` (the
RCON's `auth`: `{"code":"forbidden"}`, and the connection closes 1008),
and counts toward the address's lockout as a wrong password does. An
owner's session is used from the machine itself alone, as it was begun
(from elsewhere its token is no session: `401`, the RCON's `auth` with it
`denied`). A password that is also another role's gives the owner role
from the machine itself and the other role from elsewhere: give the owner
a password of its own.

A script passes `Authorization: Bearer <token>`. The page passes the
cookie, and on every request that changes something the header
`X-CSRF-Token: <csrf>`; the `Origin` of such a request (and of the
WebSocket's) must be the server's own. An `Origin: null` is taken only
when the browser's `Sec-Fetch-Site` says `same-origin`.

`POST /api/logout` ends the session (with the `X-CSRF-Token` by the
cookie; `GET` answers 405). `GET /api/session` answers
`{"ok":true,"role":…,"csrf":…,"expires":…}`, or 401.

When `server.web_password`, `server.web_moderator_password`,
`server.web_viewer_password` or `server.web_owner_password` changes
(`sv_set` and the settings change the first two; `sv_reload` reads any of
them from `config.toml`), the sessions of its role end and that role's RCON
connections close (1008, `the password changed`).

Five wrong passwords from an address (an IPv6 one's /64) in 10 minutes
lock it out for 10 minutes (429). Passwords are checked at 10 a minute from one
address (60 from the machine's own loopback: a supervisor, a fleet agent, which
never meets the shared ceiling), and at 120 a minute in all from the other
addresses: past a budget, `429 rate_limited` until the minute's end. An address
over its own budget uses none of the shared ceiling, so one client cannot lock
the others out of signing in. Behind a reverse proxy, set
`server.web_trusted_proxy`: without it every visitor shares the proxy's address.
A request with `X-Forwarded-For`, `-Proto` or `-Host` from an address that is
not the trusted proxy has them ignored (logged once) and is not a local
connection, even from 127.0.0.1: the owner's password is refused as from
elsewhere. A proxy that adds none of these headers cannot be told from the
machine itself: give it `server.web_trusted_proxy`. `/api/login` takes `POST` alone (any other method:
405).

### API keys

A key is a credential for a tool or an agent that is not a person's password:
it has a name, a role (`viewer`, `moderator` or `admin`; never `owner`) and
optionally an expiry and an address allow-list, and it is revoked on its
own without changing any password. A key looks like
`aek_<8 hex>_<48 hex>`. It is shown once, when it is made; the server keeps
only an HMAC-SHA-256 of its secret in `api_keys.txt` (in the data root, readable
by the server's user alone), and compares in constant time. A key has no idle
timeout and no cookie or CSRF: it is a Bearer token.

```
Authorization: Bearer aek_1a2b3c4d_<48 hex>
```

On the RCON, after the `hello`: `{"type":"auth","key":"aek_…"}` (`token` and
`password` are the session's and the password's). The key's role limits
what it may do as a session's does. A key cannot reach the owner's routes
(`POST /api/stop`, the owner's settings, the game's own console commands).
The audit lines name it: `console: key:<name>: say …`, `rcon: key:<name>
signed in`. The secret is never in a log line, an event or a reply but the
one that makes it. A key made over the console is shown on standard output
and in that reply alone (as the owner's console always is: the log's reader
does not see it).

| Route | Role | What it does |
| --- | --- | --- |
| `GET /api/keys` | admin | `{"ok":true,"keys":[{"name","id","role","created","expires","expired","allow","last_used"}]}`, without secrets (`expires` 0: never) |
| `POST /api/keys` | admin | `{"name":"dash","role":"viewer","days":30,"allow":"192.168.7.0/24"}` (`days` 0 or left out: no expiry, else 1 to 3650; `allow`: IPv4 addresses or `a.b.c.d/n` ranges, comma separated, empty: any address) answers `{"ok":true,"name","role","key":"aek_…"}` once. `400 schema` for a bad field, `409 exists` for a name in use or the 64 keys there are. A key is made with a role at or below the caller's |
| `DELETE /api/keys/<name>` | admin | Revokes the key at once: its next request is refused (`401`) and its RCON connections close (code 1008) |

`sv_key_add <name> <role> [days] [allow]`, `sv_key_remove <name>` and `sv_keys`
do the same on the console. A name is 1 to 32 characters of `a-z`, `0-9`, `_` and
`-`. An expired key, and a key used from an address its list does not allow,
are refused as a wrong key is. Every wrong key, as every wrong password, counts toward
the address's lockout (5 in 10 minutes, then 10 minutes): while an address is
locked, even a right key from it is refused (`401`), and `POST /api/login` answers
`429 locked`. `GET /api/status` and the RCON's `hello` list `api_keys` in
`capabilities`.

### Errors

```json
{"ok":false,"error":{"code":"forbidden","message":"needs the admin role"}}
```

| Status | Codes |
| --- | --- |
| 400 | `bad_request`, `schema` (a field missing or out of range), `failed` (the command ran and failed: its lines in `lines`), `rejected` (rotation entries: `rejected[]`), `invalid` (a gametype file: `line`) |
| 401 | `denied` (no session, or a wrong password) |
| 403 | `forbidden` (the role; the owner's password from beyond the machine), `csrf`, `origin` |
| 404 | `not_found` (also a command that is not one, a player or a ban that is not there) |
| 405 | `method` |
| 409 | `pinned` (a setting its environment variable sets), `changed` (a player's action whose slot holds another player now: the current name in `name`, nothing done) |
| 413 | `too_large` |
| 426 | `bad_request` (the WebSocket's version is not 13) |
| 429 | `rate_limited`, `locked` (with `Retry-After`) |
| 500 | `internal` (out of memory, a file that cannot be written) |
| 503 | `loading` (the game did not answer in 20 seconds, as while a map loads; with `Retry-After: 5`), `starting`, `busy` (8 RCON connections already; with `Retry-After: 5`) |

### Endpoints

| Method, path | Role | Body → answer |
| --- | --- | --- |
| `GET /healthz` | none | `200 {"state":"lobby"}` (or `loading`, `playing`, `postgame`) while the server hosts and its frames run; `503` with `{"state":"starting"}`, `{"state":"stalled"}` (no frame for a minute) or the other state otherwise |
| `GET /metrics` | none (`server.metrics`, off by default) | Prometheus text |
| `GET /api/status` | viewer | The status (below) |
| `GET /api/players` | viewer | `{"ok":true,"players":[<player>…]}` (with `address` and `hwid` for the owner, or every role with `server.log_addresses`; and `discord_id`, `discord_name` and `discord_verified` with them: a verified account's, else what the machine says, `""` for none) |
| `POST /api/players/{slot}/kick` | moderator | `{"name":"<the player's name, checked>"}` → `{"ok":true,"lines":[…]}`, or `409 changed` when the slot holds another player |
| `POST /api/players/{slot}/ban` | moderator | `{"name":…,"duration":"1d","reason":"…"}` (no duration: for ever, which a moderator's ban cannot be: `400 failed`) |
| `POST /api/players/{slot}/mute` | moderator | `{"name":…,"minutes":10}` |
| `POST /api/players/{slot}/unmute` | moderator | `{"name":…}` |
| `POST /api/players/{slot}/tell` | moderator | `{"name":…,"text":"…"}` |
| `POST /api/say` | moderator | `{"text":"…"}` |
| `GET /api/rotation` | viewer | `{"ok":true,"index":0,"entries":[<entry>…],"next":<entry>}` |
| `PUT /api/rotation` | admin | `{"entries":["bloodgulch:ctf",…]}` → the rotation, or `400 rejected` with `"error":{…,"rejected":[{"index":1,"entry":"…","reason":"…"}]}` and nothing changed |
| `POST /api/rotation/next` | moderator | `{}`: `sv_map_next` |
| `POST /api/rotation/play` | moderator | `{"map":"…","gametype":"…"}`: `sv_map` (`map` may be a whole rotation entry, `map:gametype+rule=value`, 255 characters at most, as the page sends one with rules; a map or gametype that cannot be played: `400 failed`) |
| `GET /api/gametypes` | viewer | `{"ok":true,"builtin":["slayer",…],"schema":{"fields":[<field>…]},"files":[{"name":…,"base":…,"description":…,"error":null}]}` (`builtin`: the Xbox's gametypes, then OpenCE's; a file that cannot be read has `base` and `description` empty and `error` saying why) |
| `GET /api/gametypes/{name}` | viewer | `{"ok":true,"name":…,"text":"<the TOML>"}` |
| `PUT /api/gametypes/{name}` | admin | `{"text":"<TOML>"}` → `{"ok":true}`, or `400 invalid` with `line` and `message` (a name is `a`–`z`, `0`–`9`, `_` and `-`, 32 at most) |
| `DELETE /api/gametypes/{name}` | admin | `{"ok":true}` |
| `GET /api/maps` | viewer | `{"ok":true,"maps":[{"name":"bloodgulch","level":"levels\\test\\bloodgulch\\bloodgulch","kind":"xbox","title":"Blood Gulch"}…]}` (`kind`: `xbox` or `custom`; `title` is the map's display name, else its `name`) |
| `GET /api/bans` | admin | `{"ok":true,"bans":[{"n":0,"when":"2026-10-19 18:00:00","ip":"…","hwid":"…","discord_id":"…","discord_verified":true,"players":"…","reason":"…","until":<unix>\|null}…]}` |
| `GET /api/bans/export` | admin | `{"ok":true,"bans":[{"when":…,"ip":"…","hwid":"…","discord_id":"…","name_pattern":"…","players":"…","reason":"…","until":<unix>\|null}…]}` (every line that bans something; the console's `sv_bans_export` writes the same to a file) |
| `POST /api/bans/import` | admin | The same shape, `{"bans":[…]}` (`ip` an address, a `a.b.c.d/n` network or empty; `hwid` hex; `discord_id` digits; `name_pattern`; `until` or `null`) → `{"ok":true,"added":2,"skipped":1,"refused":1}`: a ban already there or over is skipped, a bad one refused; `400 schema` for a body that is no list |
| `GET /api/access` | admin | `{"ok":true,"admins":[{"identity":"name:ops","role":"moderator","immunity":5,"expires":0,"expired":false,"allow":""}…],"access":[{"command":"kick","role":"admin"}…],"whitelist":{"on":false,"entries":["ip:192.168.1.0/24","name:Milenko"…]},"reserved_slots":0}`: `admins.txt` without its secrets, `command_access.txt`'s changes, `whitelist.txt` as written and `server.whitelist`, `server.reserved_slots` (the web page's Access tab; it changes them with `sv_admin_add`, `sv_admin_remove`, `sv_whitelist` and `PUT /api/settings`) |
| `GET /api/backups` | admin | `{"ok":true,"keep":20,"backups":[{"file":"config.toml","name":"config.toml.20261106-194205","time":<unix>,"size":18204}…]}`: the server's backups (below), each file's newest first; `time` from the name's stamp (UTC) |
| `POST /api/claim` | none (while the server waits to be claimed) | `{"token":"<claim-token>","password":"<10 to 100 characters>"}` → `{"ok":true,"claimed":true}` ("The first-run claim", below); `403 denied` for a wrong token, `404` when no claim waits |
| `GET /api/vpn` | viewer | `{"ok":true,"policy":"link","fail":"open","lists":[{"master":"<url>","key_id":"…","serial":12,"generated":<unix>,"fetched":<unix>\|null,"ranges":30582,"error":""}…],"file":"<server.vpn_list>","allow_ranges":0,"block_ranges":0,"held":true,"refused":{"vpn_link_required":0,"vpn_blocked":0,"vpn_link_invalid":0,"vpn_link_too_new":0,"vpn_link_in_use":0}}` (`held`: a list is held, from a master or the file) |
| `POST /api/vpn/check` | moderator | `{"ip":"203.0.113.5"}` → `{"ok":true,"listed":true,"by":"netlist"}` (`by`: `netlist`, `vpn_block.txt`, `vpn_allow.txt`, or `""`) |
| `POST /api/bans` | admin | `{"ip":"203.0.113.5","hwid":"…","discord_id":"…","names":"…","duration":"1d","reason":"…"}` (`ip`, `hwid` or `discord_id` (a verified account's: up to 20 digits), or more than one; the rest may be left out: no duration is for ever) → `{"ok":true,"n":<the new ban's number in GET /api/bans>}`: `sv_ban_address`, a machine present of the address or the hardware id dropped; `400 schema` for an address that is not IPv4, a hardware id that is not hex, a duration that is not one |
| `DELETE /api/bans/{n}` | admin | `{"ok":true}` |
| `GET /api/settings` | admin | `{"ok":true,"settings":[<setting>…]}` |
| `PUT /api/settings` | admin | `{"server.name":"…","server.max_players":16,"server.public":true,…}` (a value as JSON, a boolean, a number or a string, or its text: `"true"`, `"16"`; `null` empties it) → `{"ok":true,"applied":["server.name"],"errors":[{"name":…,"code":…,"message":…}]}`; each setting is changed or refused on its own |
| `POST /api/command` | the command's | `{"line":"sv_status"}` (511 characters at most) → `{"ok":true,"lines":["…"]}`, or a refusal (`failed`, `forbidden`, `not_found`, `pinned`) with the command's `lines` |
| `POST /api/stop` | owner | `{"after":"now\|game\|empty","notice":"…","restart":false}` (`notice` and `restart` may be left out) or `{"cancel":true}` → `{"ok":true,"lines":["…"]}`: `sv_stop` (`sv_restart` with `restart`) `after notice`, or `sv_stop cancel` (`400 failed` with no drain waiting); `400 schema` for another `after` |
| `GET /api/log?after=<n>&limit=<m>` | viewer | `{"ok":true,"lines":[{"n":12,"time":<ms>,"level":"info","cat":"game","text":"…"}…],"next":12,"missed":0}` (`limit` 1 to 1024, 256 by default; the chat's lines for admins only) |
| `GET /api/events?after=<seq>` | viewer | `{"ok":true,"events":[<event>…],"next":<seq>,"missed":0}` (`player_joined` and `player_left` with `address` and `hwid` for the owner, or every role with `server.log_addresses`) |
| `GET /ws` | | The RCON (below) |
| `GET /`, `/app.js`, `/app.css`, `/icon.svg` | none | The page (`server.web_ui`) |

`next` is the `after` of the next request: the last line's `n` (or event's
`seq`) read. An `after` past the latest (as after a restart) reads from the
start. `missed` counts lines or events that left the ring (2048 lines, 512
events) before they were asked for.

### The status

```json
{"ok":true,"time":1760900000123,"name":"OpenCE Server","state":"playing",
 "build":"build-206","network_version":38,"uptime":3600,"games_hosted":4,
 "map":"bloodgulch","level":"levels\\test\\bloodgulch\\bloodgulch","gametype":"ctf","variant":"CTF","teams":true,
 "players":5,"max_players":16,"machines":5,"countdown":-1,"game_seconds":312,"team_scores":[2,1],
 "listing":{"online":true,"public":true,"password":false,"invite":"halo://join/…"},
 "rotation":{"index":0,"count":3,"next":{"text":"sidewinder:team_slayer","map":"sidewinder","gametype":"team_slayer"}},
 "frame_ms_max":4.1,"frame_ms_avg":1.2,"tick_ms_avg":1.2,
 "masters":[{"url":"https://master1.example.org","listed":true,"probe":"ok","code":"","message":"","updated":1760899990}],
 "update":{"available":false,"build":""},
 "drain":null,"restart_warning":null,
 "pid":4021,"started":1760896400,"instance":"eu-1","region":"eu","tunnel_port":2302,
 "publisher_key_id":"3f9a2c7d1e5b8a04","web_port":8080,
 "capabilities":["events","status","rotation","settings","bans","owner_web","ban_address","stop","event_addresses","identity"]}
```

`state` is `waiting` (the menus settling), `hosting` (making the game),
`lobby`, `loading`, `playing`, `postgame` or `stopping`. `countdown` is the
lobby's seconds left, `-1` when it is not counting. `team_scores` is `null`
in a game without teams. `frame_ms_max` and `frame_ms_avg` are the longest
and the mean frame of the last minute (`tick_ms_avg` is the mean again, for
older readers). `invite` is given to admins only (empty for others). An entry of the rotation is
`{"text":"sidewinder:team_slayer+score=50","map":"sidewinder","gametype":"team_slayer","rules":{"score":50},"rejected":null}`
(`rejected` is why it cannot be played, else `null`); an entry whose map is a map pack's (`<file>@<pack>`) also has
`"map_title"` (the pack's name for the map, "Abyss") and `"pack"` (the pack's, "Halo CE+"). `drain` is the
drain waiting (`sv_stop game`...), `{"after":"game","restart":false,"since":<unix
seconds>}`, else `null`. `restart_warning` is a warned stop or restart counting down (`sv_restart 90`, the
daily restart): `{"seconds":42,"total":90,"restart":true,"reason":"updating"}` (`seconds` left), else `null`. `pid` is the server's process, `started` when it
started (seconds since 1970), `instance` the name the master servers know
it by (`server.instance`, else the slug of `server.name` they are given),
`region` `server.region`, `tunnel_port` the tunnel's UDP port
(`network.tunnel_port`; 0 when `network.online` is off), `publisher_key_id`
the publisher key's id (16 hex digits; empty without a key: `sv_publisher`)
and `web_port` `server.web_port`. `capabilities` is the RCON's hello's. The rotation's
`index` (in the status and `GET /api/rotation`) is the entry being played,
`-1` while an `sv_map` game is played or before the first lobby; `next` is
the entry after the one played last (the first while none has been).

A player is
`{"slot":0,"name":"Milenko","team":0,"score":3,"kills":3,"deaths":1,"assists":0,"ping":42,"machine":1,"joined":1760899000,"muted":false}`;
with the owner role, or `server.log_addresses`, also
`"address":"203.0.113.5","hwid":"…"`. `team` is 0 (red), 1 (blue) or -1
(no teams); `ping` is in milliseconds, `null` when it is not known yet.
`client` is the player's machine's build as its game tells it (shortly
after it joins), for every role:
`{"build":282,"flavor":"release","version":"OpenCE Linux | build 282 (release)","repository":"OpenCommunityEdition/OpenCE","mods":["trollface 1.0"]}`
(`build` 0 for a build CI did not number, `repository` empty when the build
does not know it, `mods` the mods it plays in their load order, each its
folder and its version), or `null` from a game that does not tell it. It is
what the player's game says, which a changed build can make anything.

A setting is
`{"name":"server.name","type":"string","value":"OpenCE Server","default":"OpenCE Server","comment":"…","env":"HALO_SERVER_NAME","pinned":false,"secret":false,"choices":null,"min":null,"max":null}`;
`type` is `boolean`, `integer`, `real` or `string`, and `value` and
`default` are of that type in JSON (`true`, `16`, `"OpenCE Server"`); a
reader should also take their text (`"true"`, `"16"`), which older servers
sent. `choices` lists a word setting's words, `min` and `max` bound a
number's, each `null` where there is none (the table above). `env` is
`null` for a setting no environment variable sets. A secret one (the
passwords) has `value` and `default` empty and `"set":true` when it is
set. `pinned` is a setting
its environment variable (`env`) sets: it cannot be changed.

A gametype schema field is
`{"key":"score_to_win","alias":"score","table":"universal","engine":"all","type":"integer","min":0,"max":999,"choices":null,"default":null,"description":"…"}`
(`table`: the gametype file's table it goes in, `universal`, `options`,
`ctf`, `slayer`, `oddball`, `king` or `race`; `engine`: `all`, or the game
engine whose table it is; `type`: `integer`, `boolean` or `choice`).

## The RCON

`GET /ws` with `Upgrade: websocket` and the subprotocol `opence-rcon-1`.
Every frame is text holding one JSON object. The server speaks first:

```
→ {"type":"hello","protocol":1,"server":"OpenCE Server","build":"build-206","network_version":38,
   "capabilities":["events","status","rotation","settings","bans","owner_web","ban_address","stop","event_addresses","identity","vpn","link"]}
← {"type":"auth","password":"…","by":"paul"}       or {"type":"auth","token":"<session token>"}
→ {"type":"auth","ok":true,"role":"admin"}
→ {"type":"auth","ok":false,"error":{"code":"denied"}}  and the connection closes (1008)
← {"type":"command","id":1,"line":"kick Milenko"}    or {"type":"command","id":1,"command":"say","args":["two words"]}
→ {"type":"reply","id":1,"ok":true,"lines":["Milenko kicked by the host"]}
→ {"type":"reply","id":2,"ok":false,"lines":["…why…"]}                       (the command ran and failed)
→ {"type":"reply","id":3,"ok":false,"lines":[],"error":{"code":"busy","message":"…"}}   (not run)
← {"type":"subscribe","events":["player","chat","game","moderation","status","log","master"],"after":1830}
→ {"type":"event","seq":1831,"time":1760900000123,"event":"player_joined","slot":3,"name":"Milenko",…,"replay":true}
→ {"type":"event","seq":1831,"time":…,"event":"status","status":{…the status…}}   (at the subscribe, then every 2 s)
→ {"type":"event","time":…,"event":"log","n":5120,"level":"info","cat":"game","text":"…"}   (no seq: n is its sequence)
← {"type":"ping"}
→ {"type":"pong"}
→ {"type":"overflow"}  and the connection closes (1013): the client read too slowly
→ {"type":"error","id":…,"error":{"code":"…","message":"…"}}   (an unknown type, a malformed message)
```

A connection signed in with the page's cookie (on the same origin) needs no
`auth` message: its `hello` is followed by `{"type":"auth","ok":true,…}`.

Sockets: 16 at once at most, 8 of them signed in; an address may hold 2 that have
not signed in (a 9th signed-in one answers `auth` `busy`, a 17th or a third
unauthenticated one from an address `503 busy` at the upgrade). A socket signed in
by a session (the page's cookie or a token) closes (1008, `signed out`) when that
session ends: a `POST /api/logout`, its 12 hours, or the next message after; one
signed in by a password lasts until the password changes, one by a key until the
key is revoked or expires. A word of `{"command":…,"args":[…]}` with a double quote
or a control character is refused (`bad_request`): it would split into more
arguments; give `line` for such a text.
The `capabilities` say what the server has beyond the first release's
(`events`, `status`, `rotation`, `settings`, `bans`): `owner_web`,
`ban_address`, `stop`, `event_addresses` and `identity` ("Panels and
fleets"), `vpn` (the VPN policy, `/api/vpn`, `join_refused`) and `link`
(verified Discord accounts: `discord_*` in players, bans by `discord_id`);
a client checks for one before it uses it. The owner's password
signs in from the server's own machine alone ("Signing in").
A command is a `line`, or a `command` and its `args`, which the server
joins with spaces, quoting an argument that holds a space or is empty (511
characters in all at most). Ids are the client's (a number or a string),
echoed in the reply; each command gets exactly one reply. A connection
runs one command at a time: a command sent before the last one's reply is
not run. A reply whose command ran has `ok` and the command's `lines`, and
no `error`, whether it succeeded or failed; one that was not run has `ok`
false, no lines and an `error`: `denied` (not signed in), `rate_limited`
(32 commands a second, in bursts of 64), `bad_request` (no command, or too
long), `busy` (another command's reply is awaited) or `loading` (the game
did not answer in 20 seconds; the command may still have run, and is never
sent again by the server). `after` replays the events still in the ring
since that sequence number, marked `"replay":true`. A connection holds 1024
messages for the client at most; 8 connections are taken at once (a ninth
upgrade answers `503 busy`), and each upgrade counts against its address's
allowance (above). The
event groups are `player` (`player_joined`, `player_left`, `map_share`), `chat`
(`chat`), `game` (`lobby_opened`, `countdown`, `game_started`,
`game_ended`, `postgame`, `score`, `kill`, `afk_kick`, `coop_bsp_switch`,
`map_vote_started`, `map_vote_ended`), `moderation` (`kick`, `ban`,
`unban`, `mute`, `unmute`, `speed_hack`, `join_refused`), `status` (`status`), `log`
(`log`, each log line as `{"n","level","cat","text"}`), `master`
(`master`), and `server` (`server_started`, `reloaded`, `warning`,
`drain`, `shutdown`), which every subscriber gets.
The log's lines of the `chat` category go to the `chat` group, the others
to `log`.
`status` and `log` events go to their subscribers as they come and are
not kept in the ring: `after` does not replay them (`/api/log` reads the
log's own ring). A `status` event's `seq` is the latest event's (no new
number: the same one comes again until another event), and a `log` event
has no `seq`: its `n`, the log line's number and `/api/log`'s cursor, is
its sequence. A client keeps one cursor for the events (`seq`, of the
events of the ring) and another for the log's lines (`n`); a client that
reconnects with `after` may get events it had, and drops those whose `seq`
it has seen (a `status` event is never dropped so).

## Events

Every event has `seq` (rising, never reused while the server runs; a
`status` event has the latest one's, a `log` event none: above), `time`
(milliseconds since the epoch) and `event`:

| Event | Fields |
| --- | --- |
| `server_started` | `name, build, network_version, pid, started, instance, region, tunnel_port, publisher_key_id, web_port` (as the status's), `rotation[]` (the entries' texts) |
| `lobby_opened` | `map, gametype, variant` (the gametype's name as the game shows it), `teams, index` (the rotation's entry) |
| `countdown` | `seconds` (-1: paused) |
| `game_started` | `map, gametype, variant, teams, team_scores[]` (team games), `players[]` |
| `game_ended` | `map, gametype, variant, teams, reason` (`score`, `time`, `idle`, `empty`, `command`, `next`, `update`: a new network version's install, `vote`: the players' vote to end it), `duration` (seconds), `team_scores[]` (team games), `players[]` |
| `postgame` | `seconds` |
| `player_joined` | `slot, name, team, machine`; `address, hwid, discord_id, discord_name, discord_verified` for the owner, or every role with `server.log_addresses` |
| `join_refused` | `name` (the machine's), `reason` (`vpn_link_required`, `vpn_blocked`, `vpn_link_invalid`, `vpn_link_too_new`, `vpn_link_in_use`, `address_full`); `address, hwid` as `player_joined`'s |
| `player_left` | `slot, name, reason` (`left`; `kicked` and `banned` when the server's `kick`, `ban` or `sv_ban_address` dropped the machine, `banned` for a speed hack's drop too, `afk` for a player away); `address, hwid` (as it joined) as `player_joined`'s |
| `afk_kick` | `players` (the machine's players' names), `seconds` (away) |
| `coop_bsp_switch` | `from, to` (the level's parts), `players[]` (the names of those who crossed) |
| `map_vote_started` | `seconds`, `choices[]` (`{"map","gametype","index"}`: the rotation's entries) |
| `map_vote_ended` | `map, gametype, index, votes` (the winner) |
| `chat` | `channel` (`everyone`, `team`, `vehicle`), `slot, name, text` (admins only; the lines the host sends on) |
| `kill` | `killer, victim` (slots; -1 for none), `weapon` (the killer's weapon's or the killing object's tag name, or null), `friendly_fire` |
| `score` | `map, gametype, variant, teams, team_scores[]` (team games), `players[]` (once a second at most, on a change) |
| `kick`, `ban`, `unban`, `mute`, `unmute` | `name, by, reason?, until?` |
| `speed_hack` | `machine, names, rate, ahead_ticks, action` (`kicked`, or `unverified`: its datagrams alone said so, and it stays) |
| `map_share` | `machine, map, state` (`asked`, `downloading` at each tenth, `downloaded` or `failed` once for each request), `progress` (0 to 100: a percentage already) |
| `warning` | `text` (a newer release, with `server.auto_update`) |
| `reloaded` | `rotation_changed, rejected[]` (`{"entry","reason"}` of each entry left out) |
| `master` | `url, listed, probe, code, message` |
| `drain` | `after` (`now`, `game`, `empty`, `cancel`), `restart` (exit code 5), `notice` (empty for none) |
| `shutdown` | `reason` (`quit`, `sv_stop`, `sv_restart`, `drained`: a drain's end, `signal <n>`, `update installed`, `nothing to play`, `debug.exit_after`), `code` (the exit code) |

Players in events are as in `/api/players` for the viewer role (no
addresses), but for `player_joined` and `player_left`, which carry the
player's address and hardware id (`""` when not known) to the owner, and to
every role when `server.log_addresses` is on (`/api/players`' rule), in the
RCON's events, their replay and `GET /api/events` alike. `variant` is the gametype's name as the game shows it (`Co-op`
in a co-op entry).

## Bans

A ban is a line of `bans.txt` (in `paths.logs`, else the data root). A
timed ban is kept out by its line's `until=` alone, and ends then. A ban
for ever, and a machine dropped for cheating, also keep the machine out
for the rest of the run by its address, its real address or its hardware
id. `sv_ban_address` and `POST /api/bans` write such a line for an address
or a hardware id with no player of it needed (the part not given is
`none`). `unban` takes the line out and readmits the machine by
the line's address or hardware id, whether it joins on the LAN or by
internet play.

A line's `discord_id` is a verified Discord account's when it has no
`(self-reported)` after it: `ban` writes it so for a machine that showed a
pass, and `sv_ban_address discord:<id>` and `POST /api/bans`' `discord_id`
write it. Such a line keeps out every machine that shows a pass of that
account, from any address. What a machine says of its Discord account
itself is written with `(self-reported)` and never matched: anyone could
say it.

## The rotation

`server.rotation` (and `sv_rotation_set`, `PUT /api/rotation`) is a list of
entries, `map[:gametype[+rule=value...]]`, separated by commas, semicolons
or new lines; a line that begins with `#` is a comment (to its line's end,
commas and all). The text is 4096 characters at most (the entries past it
are left out, with a warning), an entry 255 characters and the list 64
entries. `config.toml` keeps a string setting of up to 4096 characters, so
a rotation that long is saved whole and read back after a restart.

- The map is a multiplayer map's name (`bloodgulch`: the Xbox's
  `maps/<name>.map`, else a Custom Edition map of `custom_maps`),
  `custom_maps\<name>` (a Custom Edition map; `/` is taken for `\`), or a
  level's path (`levels\test\bloodgulch\bloodgulch`). A campaign level is
  played as co-op only: `<level>:coop`, at `server.coop_difficulty`.
- Arena Evolved: with Halo: The Master Chief Collection on the server's machine, `<name>@mcc` is one of MCC's own
  multiplayer maps (`bloodgulch@mcc`) and `<name>@ws<item id>` a map of a Workshop item the server's Steam account is
  subscribed to (`pat_beavercreek@ws2899342285`). They are read in place from MCC's folder (found through Steam, or
  `HALO_MCC_PATH` / `game.mcc_path`, the folder that holds `halo1/`) and need no Xbox maps: a data folder with an
  empty `maps` folder is enough. The Workshop items are read once, when the server starts. A server never sends
  these maps: a player needs MCC (and, for a Workshop map, the same item version) to join. The start-up log says
  the MCC folder, the Workshop items found and the entries it will play, and why it left one out.
- Arena Evolved's own gametypes are written with `_` for the spaces (`ae_team_sly`, `ae_ctf`); they need the weapon
  cache, `packs/ae_content.aecache` in the data folder.
- Arena Evolved's bots keep a game full (the server's machine has no player, so they sit on the server's own virtual
  machines; they are never rated). `server.bots_fill` is `auto` (the default: `minimum` for an `ae_` gametype,
  `off` for the others), `minimum` (bots are added until the people and the bots are `server.min_players`, once the
  first person has been in for `server.bots_delay` seconds), `empty_slots` (every slot up to `server.max_players`) or
  `off`; `server.bots_skill` is `recruit`, `easy`, `normal`, `heroic`, `elite`, `legendary` or `demon`; with
  `server.bots_alone` on, bot-only games keep running when nobody is in (off: the server waits, no game). A person who
  joins takes a bot's place at that bot's next death. A person who drops out of a game in progress keeps their place
  for the rest of the game: a bot plays it, and the person who comes back (the same name from the same address)
  gets the team and score back. `/api/status` has `"bots": {"count", "skill", "fill", "alone", "held"}`; `players`
  counts people only.
- The gametype (`slayer` when there is none) is a built-in's name
  (`slayer`, `team_slayer`, `slayer_pro`, `elimination`, `phantoms`,
  `endurance`, `rockets`, `snipers`, `stalker`, `ctf`, `ironctf`,
  `ctf_pro`, `invasion`, `oddball`, `team_oddball`, `reverse_tag`,
  `accumulation`, `juggernaut`, `king`, `team_king`, `king_pro`,
  `crazy_king`, `race`, `team_race`, `rally`, `team_rally`, and Halo PC's
  names `ffa`, `ffa_slayer`, `teamslayer`, `tslayer`, `koth`, `team_koth`,
  `iron_ctf`, `teamrace`, `teamoddball`), an OpenCE gametype's (`swat`,
  `infection`, `gun_game`, `hardcore`, `hardcore_ctf`, `hardcore_oddball`,
  `hardcore_king`), a gametype file's name (`gametypes/<name>.toml`), or
  `file=<name>` (a gametype a player's game saved, below). A gametype's
  name is letters, digits, `_`, `-` and `.`, in any case.
- A rule is a gametype rule's name or its short name (`score`, `time`,
  `weapons`, `ff`...; `GET /api/gametypes`'s schema lists them), with a
  value it takes; a rule of another game engine than the gametype's is
  refused. 48 rules at most; a rule given twice is the last.
- `start=<weapon>` sets the entry's first starting weapon on any gametype, an Arena Evolved one too, and keeps the rest of
  the gametype. The words are the stock ones (`none`, `random`, `assault_rifle`, `pistol`, `shotgun`, `sniper_rifle`,
  `rocket_launcher`, `plasma_pistol`, `plasma_rifle`, `needler`) or a weapon pack's: `battle_rifle`, `smg`,
  `smg_silenced`, `carbine`, `ar_silenced`, `dmr`, `evo_rifle`, `needler_pack`, `sniper_bolt`, `magnum_scoped`,
  `brute_shot`, `ar_modern`, `sidearm`, `energy_sword`, or `protomag` (the Protomag, a weapon of the weapon cache with no
  pack slot; 0.7.10's word `starter_pistol` and `digsite_scoped` still work: it starts by the game's AE WEAPONS block, so every player needs the release's weapon cache). A pack's weapon
  joins the gametype's packs. The second weapon stays the gametype's own. An unknown word is logged (`rotation: entry N start <word> is no weapon`) and the entry
  plays as written without it; the log also says `rotation: entry N start <weapon>` when an entry's start is set.
  Example: `pat_chillout@pro:ae_comp_ts+start=sidearm`. A gametype file takes it as `start = "sidearm"` in `[options]`.

A co-op entry's `riding` rule (`a10:coop+riding=true` or `=false`)
overrides `network.vehicle_riding` for that entry only. An entry without
it uses the setting; a map's own choice still wins over both.

An entry that cannot be played (a map that is not there, a bad gametype
file, a rule out of range) is left out with a warning, at the start and at
`sv_reload`; `sv_rotation_set` and `PUT /api/rotation` change nothing when
one of their entries cannot be played.

A change of the rotation (`sv_rotation_*`, `PUT /api/rotation`,
`sv_reload`) keeps its place: the next game is the entry after the one
played last, found again in the new list (the first entry only when none
has been played yet). `sv_reload` takes the rotation again when its text
changed, and also when only its entries' rejections changed (a map or a
gametype file added or taken out since).

`server.team_start` decides how a team game starts. `one` (the default)
starts it with the players of one team. `both` waits, saying nothing,
for a player on each team. `skip`, when one player waits for a team game
that `sv_map` did not ask for, plays the rotation's next entry without
teams instead (if it has one); it is checked when the lobby opens and
again when its countdown would begin. With no entry left, the server
stops with exit code 3 (2 when no rotation map is there), or plays
`bloodgulch:slayer` with `server.rotation_fallback`.

## Gametype files

A gametype file, `gametypes/<name>.toml` (the name `a`–`z`, `0`–`9`, `_`
and `-`), is a built-in or OpenCE gametype (`base`) and rules over it:

```toml
base = "team_king"     # the gametype it starts from
name = "Moving Hill"   # the name the game shows: 11 characters of printable ASCII

[universal]
score_to_win = 3

[options]
time_limit = 15
friendly_fire = "off"

[king]
moving_hill = true
```

Each rule goes in its table (the schema's `table`): `[universal]`,
`[options]`, and one game engine's, `[ctf]`, `[slayer]`, `[oddball]`,
`[king]` or `[race]`. A file gives each rule its full name
(`vehicle_set`, `team_vehicles`, `score_to_win`): the short names
(`vehicles`, `score`) are the rotation's alone. A file is 64 KB at most.
`deploy/gametypes` has examples. The server reads the `gametypes` folder
beside `config.toml` if there is one, else the data root's, and writes the
web page's files to it.

A gametype a player's game saved is played with `file=<name>`: copy the
saved gametype, as `<name>.gametype`, into the same `gametypes` folder
(beside `config.toml` if there is one, else the data root's).

`vehicle_riding` under `[universal]` is a boolean: `true` lets players
standing on a moving vehicle ride along with it; no rule means off.
Stock gametypes and older saved ones have it off. In rotation entries its
short name is `riding` (`bloodgulch:ctf+riding=true`). The dedicated
override and a map's own choice take precedence ("Vehicle riding", above).

## The listing

A public server (`server.public`, with `network.online` and
`network.public_lobby`) is published to each master server of
`masters.txt` (`docs/master_protocol.md`). A change that a list shows (the
listing made public or private, a player joining or leaving, the name, the
map) is published within 5 seconds. Made private, the server makes a new
invite and sends the masters a closed publish, which takes it off their
lists;
`network.public_lobby = false` stops the publishing too. A master's
`Retry-After` is kept (a day at most). `key_revoked` and
`publisher_revoked` stop the publishing to that master for the run;
`unknown_key`, `publisher_pending`, `application_denied` and
`listing_taken` are logged once a minute while they last.

## Admins, access and the whitelist

All of this is off until a file or setting is given: a server without `admins.txt`, `command_access.txt` and
`whitelist.txt` behaves as before.

**Admin rows.** `admins.txt` (data root, mode 0600) has a row a line: `identity`, `role`, `immunity`, `expires` (unix
seconds, 0: never), `secret hash` and `allow`, separated by tabs. Make and remove rows with `sv_admin_add` and
`sv_admin_remove` (the owner's console or password), or edit the file and run `sv_reload`; `sv_admins` lists them without
secrets. A row's role is `viewer`, `moderator` or `admin`, never `owner`.

- `name:<login>` is a person or tool that signs in with a name and a secret: `sv_admin_add name:ops moderator 5` makes
  a secret (40 hex characters, shown once; the file keeps its HMAC only). Sign in with it as Basic auth
  (`Authorization: Basic base64(ops:<secret>)`, on every request, no cookie and no CSRF) or the RCON's
  `{"type":"auth","name":"ops","secret":"..."}`. The row's `allow` list (addresses, as an API key's) limits where it works;
  wrong logins count toward the address's lockout as wrong passwords do. The log and the audit lines name it `admin:ops`.
- `cidr:<a.b.c.d/n[,...]>` is a network: the players whose machine joins from those addresses hold the row's role and
  immunity in the game. A machine of a network row with the moderator role or above also passes a banned network, the
  whitelist and the reserved slots.

- `key:<64 hex>` (0.7.7) is a player's moderator key: the game makes it from the player key. The player with that key
  holds the row's role in the game: `sv_admin_add key:<their key> admin`. A player finds their key in PROFILES, on the
  MODERATOR KEY row (A copies it; "Give this to a server owner to make you a moderator."). A game signs in to a
  server's moderation only where this PC has a role: on a server it started itself (HOST A SERVER), or where the
  server's `admins.txt` lists its key. The server offers each player a salt of its own and the salted hashes of the key
  rows it holds (message 114); the game hashes its own key with that salt and signs in only on a match, so a server that
  does not list a key never receives it. A `cidr:` row also signs a player in (the offer says their address holds a
  role).

**Running the server from inside the game (0.7.7).** A player whose key or address has a role gets two things in the
pause menu: PEOPLE's PLAYERS (each player's card: MOVE TO RED/BLUE TEAM, MUTE IN CHAT, KICK FOR 2 MINUTES, BAN FROM THIS
SERVER with a length of 1 hour, 1 day, 1 week or for ever) and SERVER's RUN THE SERVER (UP NEXT: the next game from the
rotation; END GAME NOW; SAY TO EVERYONE...; RESTART THE SERVER, warned for a minute). Each action is signed with the
player's key and runs here as the command of their role (`sv_team`, `mute`, `kick`, `ban`, `sv_rotation_begin`,
`sv_end_game`, `say`, `sv_restart`), with `command_access.txt`, immunity and the audit as for the web page; a row their
role lacks shows muted. A player with no role sees nothing new. The menus' HOST A SERVER hands the server it starts
this PC's key as `HALO_SERVER_OWNER_KEY`: that player is the server's owner in the game. `sv_team <player> <red|blue>`
(moderator) moves a player to a team: they die now, not counted (a round game: at their next death).

**Immunity.** A caller can act on (kick, ban, mute, ban by address) a player of a network row only if it has a higher role,
or an equal role and a higher immunity number (0 to 99, a login's own); a tie is refused. The owner (the console, the
owner's password) may act on anyone. A player who is no row's is below every caller. *Until the 0.8 player key exists, an
in-game player's identity is its address: give a network row, not a name, to protect someone. A `name:` row is a login
only.*

**Command access.** `command_access.txt` has a line `<command> <role>` (`#` begins a comment): `kick admin` makes kick
an admin's command; `sv_players viewer` opens one to a viewer. The role is checked where the command runs (the console,
`POST /api/command`, the RCON); the API routes that name a role of their own (`/api/players/{slot}/kick` needs a
moderator) keep it, so a command lifted to a higher role is raised there too and one lowered is reached through
`POST /api/command`. `sv_access` lists the changes; `sv_reload` reads the file again (it is also read every 15 seconds).

**The whitelist.** `server.whitelist` (off) lets only the machines in `whitelist.txt` join. An entry a line:
`ip:<a.b.c.d[/n]>`, `hwid:<hex>`, `discord:<verified account id>` or `name:<player name>`; `#` begins a comment. The first three
are checked at the door; a name only once the player has one, and is no proof (anyone can take a name): use it with
care. A listed address or an admin row (moderator or above) always passes. `sv_whitelist add|remove|list|on|off` edits
it. With the whitelist on and nothing listed, only admin rows join, and the log says so.

**Reserved slots.** `server.reserved_slots` (0) keeps that many of the last slots for machines of admin rows (moderator
or above): when only they are left, other machines are told the game is full.

**Bans by network and by name.** `sv_ban_range 203.0.113.0/24 7d spam` writes a `bans.txt` line whose `ip=` is the network;
a machine from it is refused at the door and its players dropped within a second. `sv_ban_name Sk8*` writes a line with
`namepat=`: a player whose name matches (whole name, either case, `*` and `?`) is dropped within a second of joining
and again each time it joins. A pattern needs a letter or digit in it (`*` alone is refused). `sv_unban <number>` takes
either out. Admin rows pass both.

**Moving bans between servers.** `sv_bans_export [file.json]` / `GET /api/bans/export` and `sv_bans_import file.json` /
`POST /api/bans/import` carry the bans as JSON. An imported ban is checked field by field (addresses, networks,
hardware ids, Discord ids, patterns, lengths); one already in `bans.txt` or already over is skipped, a bad one
refused, and the answer counts all three. The files are named in the data root and end in `.json`; a name with a
folder in it is refused.

## Abuse limits

All off by default.

| Setting | What |
| --- | --- |
| `server.chat_flood_lines` | A player who sends more than this many chat lines in 5 seconds is muted for `server.chat_flood_mute_minutes` (2) and the line dropped. The chat already holds a player to a burst of 5 lines and then one every 1.5 seconds, so 2 to 7 act; more never trigger. |
| `server.max_machines_per_address` | (3 by default, an upstream setting) A machine of an address that already has this many in the game is refused (rejection code `address_full`; the log says how many of its address play). 0: no limit. |
| `server.ping_kick_ms`, `server.ping_kick_seconds` | A player whose ping stays over the first for the second (30 s) is kicked and may join again. 0: off. A ping is known only in a game. |
| `server.afk_kick_minutes` | (upstream) Players away. |

## Alerts and event hooks

**Alert webhook.** `server.alert_webhook` (the owner's setting, a secret: an `https://` address, or `http://` to
127.0.0.1) receives a JSON POST for each alert, sent by a thread of its own (three tries, never a wait for the game):

```
{"source":"arena-evolved-server","server":"<server.name>","event":"disk_low","text":"...","content":"<server>: ...","time":1760900000}
```

`text` is for a log, `content` for a chat webhook (Discord reads it; Slack reads `text`). Events: `restart_loop` (the
server started `server.alert_restart_loop` (3) times within 10 minutes, counted in `server-starts.log`), `listing` (a master
refused or withdrew the listing; at most once an hour), `update_installed` (an auto-update installed a build),
`disk_low` (the data folder's disk has less than `server.alert_disk_mb` (500) megabytes free; at most once in 6 hours) and
`empty` (nobody played for `server.alert_empty_hours` hours (0: off)). Nothing is sent, or read from the disk, without a webhook.

**Event hooks.** `server.on_event` (the owner's setting) names a file of the data root with a rule a line:

```
# <event>[<field>=<value>] -> <command>; <command>
player_joined -> say Welcome {name}
player_left[reason=kicked] -> say {name} was removed
game_ended -> say GG. Next map soon.
```

An event is any name in "Events" (`player_joined`, `game_ended`, `postgame`, `map_vote_ended`, ...). An optional
`[field=value]` limits a rule to events whose field has that value. `{field}` in a command is that event field's text; an
unknown field is empty. A value is made safe: non-printable characters, `"`, `;`, `{`, `}` and `\` become `_`, so a
player's name cannot add a command or end a quoted word (quote the word yourself: `tell "{name}" hello`). Commands are
split at `;` before the fields are put in, run as an admin (never the owner: no stop, no settings that are the owner's),
at most 4 a rule and 10 a 10 seconds in all. Unknown events, unknown fields and bad lines are ignored, so a rules
file written for a newer server loads on an older one. The file is read at the start, by `sv_reload` and every 15
seconds. `docs/examples/server_rcon_bot.py` is a 125 line Python bot that does the same from outside, over the RCON with an API
key.

## The first-run claim

With `server.claim` on and no web password set, the server makes none: it writes `claim-token` (data root, mode 0600, 32 hex
characters) and says once that it is waiting to be claimed. On a terminal the line holds the link and token; as a service
the log has only the link and the file's path, so the token is not in a journal. The first visitor who gives the token sets
the admin password: `POST /api/claim {"token":"...","password":"..."}` (10 to 100 printable characters). The token file is then
removed and the route answers 404. Wrong tokens count toward the address's lockout. The password is written to `config.toml`
(`server.web_password`); to claim again, clear it, restart and the server writes a new token.

## Backups

Before the server writes `config.toml` (every `sv_set`, rotation change and
settings change), `bans.txt` (a ban, an unban, an import), `api_keys.txt` (a key made
or revoked), `admins.txt` (a row made or taken out) or `whitelist.txt` (an entry added or removed), and once as it starts, it copies the file as it is to
`backups/<name>.<YYYYMMDD-HHMMSS>` in the data root (the time is UTC), unless
the newest backup of it holds the same bytes. The rotation is in `config.toml`.
`server.backups_keep` (20 by default, 1 to 1000) is how many of each file are
kept: the older ones are deleted. These copies are the server's own, and it
deletes nothing else. A backup of `config.toml` holds the web passwords as the
file does, and is readable by the server's user alone. `sv_backups` lists them, `GET /api/backups` and the web
page's About tab with their times and sizes (the page restores none).
To go back, stop the server and copy a backup over the file. Two writes in the
same second share one backup name (the later one's old copy replaces the
earlier).

## Files

| File | What |
| --- | --- |
| `config.toml` | Every setting (beside the executable; `HALO_CONFIG_FILE`, else `--config`, names another) |
| `init.txt` (`server.init_file`) | Commands run at the start, in the data root unless a full path |
| `gametypes/<name>.toml` | Gametype files: the `gametypes` folder beside `config.toml` if there is one, else the data root's |
| `gametypes/<name>.gametype` | Gametypes saved by a player's game (`file=<name>` in the rotation): the same `gametypes` folder |
| `api_keys.txt` | The API keys' names, roles, expiry, allow-lists and hashed secrets, in the data root, mode 0600 ("API keys") |
| `admins.txt` | The admin rows: logins by name and secret (hashed) and networks with a role and an immunity, in the data root, mode 0600 ("Admins, access and the whitelist") |
| `command_access.txt`, `whitelist.txt` | A command's role changed, and the machines allowed in with `server.whitelist` (the data root; written by hand or `sv_whitelist`) |
| The file `server.on_event` names (`on_event.txt`) | Event hooks: events to server commands (the data root; "Event hooks") |
| `claim-token`, `server-starts.log` | The first-run claim's token (mode 0600, gone once claimed) and the starts the restart-loop alert counts (the data root) |
| `backups/<name>.<YYYYMMDD-HHMMSS>` | The server's own copies of `config.toml`, `bans.txt`, `api_keys.txt`, `admins.txt`, `command_access.txt` and `whitelist.txt` from before each write, in the data root ("Backups") |
| `publisher.key` | The publisher key for the master servers (beside `config.toml`). On Windows, a server's `config.toml` and `publisher.key` are given an access list of their owner alone (the entries inherited for Administrators and SYSTEM taken out) |
| `masters.txt` | The master servers (`server.masters_file`: beside `config.toml`, else beside the executable); one that names none leaves the official masters, built in |
| `bans.txt`, `cheaters.txt`, `debug.txt` | In `paths.logs`, else the data root. `debug.txt` is rolled to `debug.old.txt` when it passes 64 MB (so 128 MB at most): a server logging `debug.objective_log = full` fills it in about 14 hours, so keep what you want from `debug.old.txt` (a collector job) |
| `vpn_allow.txt`, `vpn_block.txt`, `netlist-<master key id>.bin` | Beside `bans.txt`: ranges never refused as a VPN's, ranges refused besides, and the masters' netlists held ("VPNs and linked Discord accounts") |
| `status.json` (`server.status_file`) | The status, written every 2 seconds |
| A password's file (`HALO_SERVER_WEB_PASSWORD_FILE`, `HALO_SERVER_WEB_OWNER_PASSWORD_FILE`, `HALO_SERVER_WEB_MODERATOR_PASSWORD_FILE`, `HALO_SERVER_WEB_VIEWER_PASSWORD_FILE`, `HALO_SERVER_PASSWORD_FILE`) | A container's secret: when the plain variable is not set, the file's first line (its CR and LF off) is the setting's value, which is then pinned as the plain variable's would be (`sv_set` and `PUT /api/settings` refuse it, `409 pinned`). Read once, at the start; a file that cannot be read is logged, and the setting is `config.toml`'s |

Join diagnostics: when a joining machine is dropped, `debug.txt` has lines that start `join-diag:`. A machine that
times out gets one with how many game update datagrams and stream messages the host heard from it since it joined,
how long ago the last of each came and the UDP port of the last datagram. A machine that joins a game in progress
gets one line for each message of its start (packet type, size, `ok` or `failed` with the bytes written before the
failure), and a machine whose stream closes gets one line (once per join). They log only; nothing sent changes.

## Releases

Each release has `SHA256SUMS` (the SHA-256 of each of its files, as
`sha256sum` writes them: `<hex>  <name>`, written by `tools/ci_build.py
sums`) and `SHA256SUMS.sig`: the Ed25519 signature of that file's exact
bytes by the project's release key, in 128 lowercase hex digits and a
newline (the form `opence-master-server`'s `cmd/release-sign` writes). The
public key is `deploy/release-key.pub` (64 hex digits:
`d04e8b101007d1641cace8b95fcca76f7f44709f6885c3d031b52394dbdf9376`).
`tools/release_sign.sh verify SHA256SUMS` checks the signature (OpenSSL 3),
and `sha256sum -c --ignore-missing SHA256SUMS` the files downloaded. The
build workflow signs with the `RELEASE_SIGNING_KEY` secret (the key's seed,
64 hex digits) and checks the signature against `deploy/release-key.pub`
before it publishes the release.

## The Arena Evolved master

Besides OpenCE's, the built-in master list names the Arena Evolved master, `https://master.arenaevolved.com`
(`port/linux/game/ae_master_key.h` holds its pinned key). A server with a publisher key (`--keygen`) needs no
application there: its first publish carries `public_key`, the master approves it after a UDP probe of the game port
(let UDP through, or set `network.public_address`; a server behind a tunnel that carries only the game port sets `network.join_address`, which the master probes over UDP and TCP instead). Each publish to that master also carries a signed `ae` block
(`join_now`, `join_range`, `region` from `server.region`, `entries` left empty until the playlist's audiences exist).
The master's answer says `chat_approved` (false unless the master's maintainers grant it); the status page shows it per
master. The browser of an Arena Evolved player asks `/v1/servers?ae=1` there and merges it with the other masters'
lists: one row per game. To use a private Arena Evolved master, name it in masters.txt and put its list key in the
environment variable `HALO_AE_MASTER_KEYS` (comma separated) so the AE additions are sent to it.

## Hardening, and what the operator answers for

- The 64-bit Linux binary (`ninja linux64`) and the static server build are built with a stack protector
  (`-fstack-protector-strong`) and read-only relocations (`-z relro,now`). They are not position-independent: a
  crash report's addresses are the executable's own, and the guest memory is at fixed addresses. A server that plays no
  movies needs no FFmpeg: `python configure.py --release --without-ffmpeg` leaves it out (about 1.3 MB less code, no
  Bink decoder to attack).
- Run the server as a user of its own that holds nothing else (no SSH keys, no Steam login), give it a read-only view of
  the MCC and Workshop folders, and open to the internet only the game's UDP port; the web port stays on 127.0.0.1 or
  behind a TLS proxy ("HTTP").
- A map in a `maps_<pack>/` folder is sent to joiners that ask for it (`network.map_share`), whoever made it: what is in
  those folders is the operator's to share. The game's own maps and files are never sent (`@mcc` and `@ws<id>` names
  neither). A machine may ask for 24 maps at once, one more every 2.5 seconds, and one listed file at a time is hashed
  and sent to it.
