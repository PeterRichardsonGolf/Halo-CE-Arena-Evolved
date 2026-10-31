# Testing Arena Evolved: the SOP

One harness, `tools/ae_test/`, does every automated game test. Use it instead of writing new scripts;
if it lacks something, add it there (with a test in `tools/test_ae_test.py`).

## Which tool for which job

| Job | Command (add `--box` to run it on the build box from the local machine) |
|---|---|
| Build a commit (`linux64`; `--targets all` = linux64, linux, both servers) | `python3 tools/ae_test/build.py --commit <rev> --box` |
| One game from a spec (map, gametype, flags, split screen, bots, mod, window, screenshots, env) | `python3 tools/ae_test/run.py --build <rev> --map bloodgulch --players 2 --bots 3 --box` |
| The per-merge smoke set, against a baseline | `python3 tools/ae_test/smoke.py --build <rev> --baseline <rev> --box` |
| Split-screen pictures, before vs after (1p/2p/3p/4p, 4p at 720p), reticle offsets | `python3 tools/ae_test/sheet.py --after <rev> --before <rev> [--reticle --flat] --box` |
| Host/client pairs with stock Chupa / stock OpenCE (version, Delta, positions, kills) | `python3 tools/ae_test/handshake.py --ae <rev> --other stock --box` |
| The score strip: what it says in every mode (seeded gametypes, bots, debug.score_strip_log), and a joined client's strip against the host's (on the box itself) | `python3 tools/ae_test/strip_checks.py modes --build <path>`, `... pair --build <path>` |
| The AE panels: primitives on a plate, the engine reader (and a joiner's against the host's), the groups and their states, the motion sensor, split screen and vehicle seats, waypoint labels under the panels (on the box itself) | `python3 tools/ae_test/panel_checks.py primitives --build <path>`, `... engine`, `... pair`, `... groups`, `... sensor`, `... split`, `... waypoints` |
| A Windows CI build on the Windows test box | `python3 tools/ae_test/windows.py --artifact arena-evolved-windows64-release` |
| An MP4 clip of a game (H.264 + AAC; needs ffmpeg on the machine) | `python3 tools/ae_test/record.py --build <rev> --map bloodgulch --seconds 20 --box` |
| A copy of a save root with a saved gametype's variant bytes patched and signed again (for `--save-from`; never in place, never a real save root) | `python3 tools/ae_test/gametype_file.py --from <save root> --to <new folder> --name "NHE 1V1" --set 0x1d=3` (on the machine that plays) |
| A save root's seeded gametypes against golden hashes (`tools/ae_test/golden_seeds_a17102e2.json`, a17102e2's seeds; `--rename OLD=NEW` for migrated names) or against another root (byte-identical, `--same-as`) | `python3 tools/ae_test/seed_compare.py <save root> --golden tools/ae_test/golden_seeds_a17102e2.json` |
| Every field of every seeded AE gametype (the save root's `z/arena_audit.tsv`, written with `HALO_ARENA_AUDIT=1`) as a report, or two audits compared | `python3 tools/ae_gametype_audit.py <arena_audit.tsv>`, `... --diff OLD.tsv NEW.tsv` |
| A change to AE's seeded gametypes, as a new seed revision (below) | `python3 tools/ae_gametype_change.py --gametypes "AE FFA SNPR" --set universal_variant.respawn_time_growth=0 --why "..." [--dry-run]` |

Weapon pack tests (spec keys, or the same names as `run.py` options): `pack_loadout` (`primary`, `secondary`,
`both`) and `pack_map_spawns` make the test host's gametype use the weapon pack (`HALO_NETWORK_TEST_PACK_LOADOUT`,
`HALO_NETWORK_TEST_PACK_MAP_SPAWNS`; the stock loadout values stay the assault rifle / pistol fallbacks);
`pack_file` is a pack read in place of `packs/ae_battle_rifle.aepack` (linked as the data root's
`pack_file/<name>`: the game opens `debug.weapon_pack_file` / `HALO_WEAPON_PACK_FILE` by Xbox path,
`d:\pack_file\<name>`; the setting is an Xbox path under `d:\`, the data root, never a host path, and the file
must exist); `packs_from` is a folder linked as the data root's `packs/` (default: the config's `data_dir/packs`
when it exists), or `"none"` (`run.py --no-packs`) for a game with no `packs/` at all whatever the machine has. At start the game logs slot 1's pack, `weapon pack: 'ae_battle_rifle' revision
N (sha256 <16 hex>...), ...`, or `weapon pack: slot 1 (<path>) refused: <reason>; the game plays without it`;
with no `packs/` (and no `pack_file`) nothing (tools/test_weapon_pack_load.py, synthetic packs). A saved
gametype's pack bytes: `gametype_file.py --set weapon_pack=1 --set pack_loadout=1 --set pack_map_spawns=1`
(offsets 0x29, 0x2a, 0x2b; `--set` takes field names too). `expect.lines` / `expect.no_lines` (`run.py
--expect-line REGEX`, `--expect-no-line REGEX`) are regexes a line of the game's debug.txt must / must not match,
e.g. the rules line: `weapon pack: none, pack loadout: primary stock, secondary stock, battle rifle on map: off`.

The weapon pack on a map (tools/test_weapon_pack_add.py; `AE_TEST_PACK_BUILD`, and `AE_TEST_PACK_REAL` = a real
pack's path for the cases that need one, `AE_TEST_PACK_ROTATION=1` for the long cases: the four-game rotation, and an AE host and client with the pack,
which must log the same fingerprint): with `pack_loadout`
set, each map logs `weapon pack: added to <map>: ... fingerprint <16 hex>`, the first player given the rifle
`weapon pack: first spawn: the primary weapon is the pack's battle_rifle (...)`, and each map change `weapon pack:
let go from <map>: ...` with the reads served from the pack; a pack a map cannot take logs `weapon pack: refused
for <map>: <reason>; the game plays without it` (a refusal: such a case needs `expect.refusals_ok`). The test host
changes map between games with `network_test: "host:<map>+<map>...:<gametype>,<gametype>..."`: game N plays the
Nth map (the last one from then on), so the gametype list says how many games there are;
`HALO_NETWORK_TEST_TIME_LIMIT=1` makes each a minute. `debug.weapon_pack_ground` / `HALO_WEAPON_PACK_GROUND=1` (for
pictures; a host or no network game only, ignored on a client) lays one more of the pack's weapon two steps ahead
of each local player who spawns with it, and of one who spawns without it in a game that asks for the pack only for
BATTLE RIFLE ON MAP (`--pack-map-spawns` with `HALO_NETWORK_TEST_LOADOUT=<w>,<s>`: a few steps onto it show the swap
prompt, or with `<w>,0` pick it up). `handshake.py --packs-from <folder>` links that folder as both machines'
`packs/` (`none`: no packs at all).

The pack's checks against the map and the fallback (tools/test_weapon_pack_checks.py; the same three variables):
forced-failure packs made from the real one (labels `zz`, a required and an optional external renamed, the
weapon renamed to a stock weapon's name, a first-person animation node renamed), a full tag table
(`debug.weapon_pack_table_limit` / `HALO_WEAPON_PACK_TABLE_LIMIT=<tags>`, tests only), a Custom Edition map
(`homobox@ce`: the synthetic pack checked by the tag validator beside the map's tags; `timberland@ce`: the real
pack added; `h2_blood_gulch@ce`: refused for a stock tag it lacks; each skips without its map in `maps/ce`) and no
pack at all: each refused with its reason, `first spawn: assault rifle + pistol` (the stock
fallback), the console's `weapon pack: BATTLE RIFLE unavailable on <map>: <reason>` line, and the game played to
its end. The long cases: all 13 Xbox multiplayer maps in one rotation (a minute each, about 20 minutes), Halo 1:
NHE's badcreek (`discs/NHE`, or `mods/NHE` on an older data root) and CE+ X's Blood Gulch beside its own rifle (`discs/CE+ X`).

The battle rifle's behaviour (tools/test_weapon_pack_rifle.py, port/linux/game/ae_burst_rifle.c; the same three
variables, the CE+ X case with the rotation's): the set-up lines (`ae burst rifle: zoom 1 level 2.563x ...`, spread,
magazine, bullet, damage, aim assist), the cadence with `debug.burst_autofire` / `HALO_BURST_AUTOFIRE=1` (the
holder pulls again as soon as the rifle allows; `2`: one pull in the middle of each recovery, which is lost, and
another half a second after it; a host or no network game only, a client ignores it and logs so; the pulls are made
in the host's weapon update, not its player's input, so other machines of a network game do not see these bursts),
250 ms pulls with `debug.test_input` (`HALO_KEY_FIRE=K`, `key:K`) firing three rounds each, the reserve topped at
108 by a second rifle (`HALO_WEAPON_PACK_GROUND=1` and a few steps), the zoomed view's angle with `HALO_FOV=100`,
a zoom-and-fire game that ends its map (its `let go` line must count no failed read; every rifle case forbids
`could not be read from the pack`), and network_test head and body hits (`debug.network_test_damage`'s `seconds:scaleh` hits the head, its collision
model's head material). `debug.weapon_log` / `HALO_WEAPON_LOG=1` logs each round (`ae burst rifle: tick N, round
r of 3 fired, L loaded, R in reserve, recovery T`), each burst's distance from the last and the zoomed view's
angle, the firing effect of each round and the removed kick, and in a gametype with INFINITE AMMO or BOTTOMLESS CLIP
(the OCTAGON drills) each weapon's shot (`ae ammo: tick N, <weapon> fired, L loaded, R in reserve`); `debug.weapon_hit_log` / `HALO_WEAPON_HIT_LOG=1` logs
each of this machine's players' rifle rounds that hits an object (`ae rifle hit: ... head h | shield a -> b body c
-> d dead x`; the numbers are a host's or a game without network: a client deals no damage). All three are for
automated tests, default off.

The look of the pack's weapons (tools/test_weapon_pack_look.py; the same variables): the `weapon pack: battle_rifle's
look:` line (the pickup texts and the message icon its weapon entry names), the two SMOOTH bitmaps' `drawn smooth`
lines, the ammo text shown after walking over a second rifle, the swap prompt (`--pack-map-spawns`, a stock loadout,
`HALO_WEAPON_PACK_GROUND=1`, a few steps) drawing the entry's icon with no frame showing the counter's
sheet (magenta or bright green texels in the upper half), the weapon text when the rifle is picked up with one weapon
held, a variant pack whose icon is the counter's sheet (its icon
refused for the map, the prompt drawn with no icon; also an icon whose optional external bitmap the map lacks), and firing zoomed in (`debug.weapon_log`: `zoomed: local player 0 sees no
particle of the rifle or its holder`).

`<rev>` is any git revision (pushed to the box as `refs/ae-test/<sha12>` and built once in its own worktree),
a named build from the config (e.g. `stock`), or a folder holding a `halo` binary. `run.py --list-keys` lists
the spec keys; `run.py --spec game.json` takes a whole spec; `--dry-run` shows the game's environment.

Every tool writes `<out_dir>/<tool>/<time stamp or --out-name>/`: `summary.txt` (one line per game, or the
table), `summary.json`, and per game `result.json`, `debug.txt`, `stdout.log`, `shots/*.png`, `bots.log`. With
`--box` the folder is copied back to the local `out_dir`. Setup: copy `tools/ae_test/config.sample.json` to
`~/.config/ae_test/config.json` on each machine and fill it in (the local one has `box` and `windows`; the
box's has `repo`, `data_dir`, `named_builds`).

## Changing an AE gametype's seeded settings

The AE gametypes are seeded into each save root once; a later change reaches players' roots only through a new seed
revision in `source/saved games/arena_gametypes.c` (a migration per changed gametype, its old row exactly as the
revision before seeded it, the hash of that old file, and a fresh golden root). `tools/ae_gametype_change.py` does
all of it in one command, on the build box:

- `--gametypes` picks the gametypes: a stored name, a set (`AE`, `AE BR`, `AE COMP`, `NHE`, `TRAINING`), an
  engine (`slayer`, `ctf`, `oddball`, `king`, `race`), `team` or `ffa`; `&` intersects (`"AE BR&slayer"`). An AE BR
  gametype and its AE twin always change together.
- `--set` changes a row field (`score_to_win=30`, `respawn_time=ARENA_COMPETITIVE_RESPAWN`) or a variant field by
  its audit column name (`universal_variant.respawn_time_growth=0`, `game_engine_variant.slayer.kill_in_order=1`,
  `options.friendly_fire_penalty=0`); `--adjust` names an existing adjust or a preset (`no_respawn_growth`,
  `no_odd_man_out`). Variant fields go through a new adjust function that calls the row's old one first: a
  shipped adjust is never edited.
- It builds and seeds the current tree (the old files' hashes; they must equal the committed golden file's),
  writes the revision, builds and seeds the new one, and checks: a clean self-check; only the chosen gametypes'
  files changed; an old root with an edited copy and a player's own copy migrates as promised (updated in place
  and backed up, the edited and own ones left alone); each changed gametype passes `seed_rules.py`. Only then
  does it write the source, `golden_seeds_rev<N>.json`, `seed_rules.py`'s expectations and
  `tools/ae_test/gametype_changes.json`, and print the audit diff. `tools/test_ae_gametype_change.py` checks
  the committed result.
- `--dry-run` prints the plan, the source diff and the expected audit diff without the box; `--dry-run --box`
  also runs the box checks. The CHANGELOG line and the commit stay a person's.

## What a merge must run

1. `python3 -m pytest -q tools/` (with the CI's list at least, plus `tools/test_ae_test.py`).
2. `smoke.py --build <merge> --baseline <main before the merge>`: no FAIL. PRE-EXISTING (the baseline fails
   the same way) is reported, not blocking. Read the "vs baseline" notes: a changed count is a finding.
3. `handshake.py --ae <merge> --other stock` (and `--other opence` when OpenCE's netcode moved): all PASS.
4. When HUD, menus or anything drawn changed: `sheet.py --after <merge> --before <main>`; look at every PNG.
5. Before a release: `windows.py` with the CI's Windows build.

## Where tests run

- **Build box** (default): every build and game. At most `max_games_total` (4) headless games on the box from
  all helpers together; the tools wait for a slot. `--parallel auto` plays 3 at a time, or 1 when another
  helper's games already run. Builds use ccache and `jobs` (8 while others build).
- **Laptop**: the owner's machine. No games by default; never run games or heavy builds there while the
  owner plays.
- **Windows test box**: a shared PC that people play on. Only through its runner (below), never forced.

## A game running on the machine

Before anything heavy on a machine where the owner plays, the harness looks for his game: a `halo` process
(`/proc/*/exe`, also a binary replaced by a deploy) whose working folder is outside the harness's work folder,
or a listener on port 5150. By hand: `pgrep -x halo` plus `readlink /proc/<pid>/cwd`, and
`ss -ltnp | grep 5150`. Never `pgrep -f`/`pkill -f` a pattern that is in your own command line.
If he plays: slow mode (nice 19, one game at a time, `jobs_slow` build jobs, short tests). **Never kill or touch
his game.** On the box `owner_check` is off (he never plays there).

## Standard environment of a game

The harness sets: `SDL_AUDIO_DRIVER=dummy`, `SDL_GAMECONTROLLER_IGNORE_DEVICES=all`, `HALO_HIDDEN_WINDOW=1`,
`HALO_NET_ONLINE=false` (else hosted games are listed publicly), `HALO_NET_ADDRESS=127.0.0.200`,
`HALO_UPDATE_AUTO=false`, its own `HALO_DATA_ROOT` (links to the maps and mods) and `HALO_SAVE_ROOT`, a copy of
the binary (config.toml is written beside it), its own network namespace (`unshare -rn`, so games and bots
never meet), its own `Xvfb` when there is no display (started by the harness with `-displayfd`, with a private
`/tmp` in the game's own mount namespace, so every game may get `:0` and they still never meet; not `xvfb-run`,
whose cleanup can turn a clean exit into exit 1).
The game's exit code is the game's own, the display is in result.json, a game whose X server failed is not started
(the reason is in `why`), and nothing the harness started outlives it (errors, TERM, Ctrl-C): the inner runner is
pid 1 of its own pid namespace, so even a grandchild that left its process group dies with it. Without namespaces
(no `unshare` for this user) games run one at a time on the machine's own network and `/tmp`, nothing is mounted,
and a grandchild that leaves its process group can outlive the run (the one limit). Output and work folders are
never under /tmp (each game's private /tmp would hide them). A spec's `env` adds or overrides.

- Save roots and work folders **never under /tmp** (a RAM disk on the local machine; map caches are up to 290 MB each).
  The tools refuse it, and delete each game's work folder afterwards (`--keep-work` keeps it).
- Screenshots: **PNG, never BMP**. On Linux, builds with the capture feature write PNGs themselves (run, smoke,
  sheet and handshake set `HALO_SCREENSHOT_FORMAT=png`; frames the game had to skip are counted in the result);
  older builds (stock Chupa/OpenCE in handshakes) write BMPs, which the tools convert and delete. `windows.py` still
  has the Windows build write BMPs and converts them locally.
- Verdicts read `debug.txt`: exit code and the clean-exit line, asserts, exceptions, refusals ("cannot be
  played"), lost scripts ("scripts won't run"), the last network-test tick, items, scenario scripts.

## Bots changed in a game in progress

`HALO_BOTS_LIVE_TEST="<second>:<count>,..."` (tests only, no setting) writes `bots.count` at those seconds of a
local game's clock, as the pause menu's SETTINGS would (`game/ae_bots_live.c`): for example
`HALO_BOTS=1 HALO_BOTS_LIVE_TEST=20:3,40:1,60:0` with `--network-test local:bloodgulch`. halo.log/debug.txt says
`bots: test sets bots.count to N`, then `bots: bot N joined the game in progress (team T)` or `bots: bot N left the
game in progress` for each change. A LAN or internet game it hosts (`host:`) takes it too: the host's network bots
follow it, four at a time (`net bots: bot N joined the game in progress (...)` / `left the game in progress`).

## The Windows test box (a shared PC: other people use it)

The runner in `C:\halo-test\runner` decides, and the tool follows it. Its contract (kept with the box's
setup, outside this repository):

- Write `C:\halo-test\runner\request.json` with a **new run_id every time**, then start only with
  `powershell -NoProfile -ExecutionPolicy Bypass -File C:\halo-test\runner\start-test.ps1 [-Wait]`.
  **Never call `schtasks` directly.**
- If the first output line is `TEST NOT STARTED: <reason>` (exit 10), report it. **Never retry silently**, never
  force a run. With `-Wait`: exit 0 is done; exit 11 is aborted, timeout or failed.
- Idle rule: 3 minutes without input by day, no wait at night (22:00-07:00); the fullscreen-app, GPU, audio and
  controller checks always apply. Any real input during a run stops our game only (hard limit 180 s).
- Every attempt is logged in `C:\halo-test\runs\attempts.log`.
- `result.json`'s `exit_code` can be null on the current runner: not a failure by itself (status `done` is).
- Touch nothing outside `C:\halo-test`; never close other apps or change display or audio settings. Windowed
  mode comes from `HALO_DISPLAY_MODE=windowed` (the runner cannot force a window).
- CI builds: `fetch-build.ps1` takes only successful push runs on main.

## Test input: one real gamepad

In `menu:` presses, `pad:<button>` (`pad:a`, `pad:b`, `pad:start`, `pad:up`, ...) presses a button of a virtual SDL
gamepad (`ae_platform.c`), attached at the start when the setting has any `pad:` press. The game finds it as a real
pad, so its presses go through the game's own mapping of pads to ports: a lone pad shares port 0 with the keyboard,
and is port 1's while there are two players (`xinput_sdl.c` port_gamepad). The plain presses stay the keyboard's
(port 0), and `2:b` still presses port 1 directly, whatever the mapping. `debug.txt` says `test input: virtual gamepad
attached`. `tools/test_ae_lan_lobby.py` uses it for split screen on one gamepad and the keyboard.

## Test input: the mouse

`HALO_TEST_INPUT` (`debug.test_input`) is the platform's scripted player (`bot:<seed>`, `look:<seed>`, `menu:<buttons>`).
`mouse:<dx>,<dy>` (for example `mouse:100,40`; counts may be negative or fractions) adds those mouse counts to the real
ones on every 30th look of the game (about twice a second of play, for a one-player game) and, each time, logs the turn
it gave the view: `mouse test: counts 100,40 turn yaw -12.60507 pitch -5.04203 degrees (style classic, zoomed 0,
vehicle 0)`. That is the angle added to the player's facing, after the style's formula and the zoom and vehicle scales
(not a read-back of the facing, which the harness cannot observe). Match start logs `mouse: style ..., sensitivity ...,
zoom scale ..., vehicle scale ...`. `tools/test_ae_mouse.py` plays it (set `$AE_MOUSE_BUILD`, as `$AE_MENUS_BUILD` in
`tools/test_ae_menus.py`): classic 1.0 turns 100 counts 12.60507 degrees, MCC 5.67 the same within 1%, MCC 1.4 turns
1.4/45 degrees a count. With `HALO_NETWORK_TEST_VEHICLE=5` (the test seats the player as a vehicle's driver) the
lines read `vehicle 1` and the vehicle scale 0.5 halves them. The zoom scale has no game test: a zoomed player could not
be arranged headlessly (the zoom button pressed through the test input did not zoom the pistol), so its factor is
covered by `ae_mouse_test.c` and the glue's `zoomed` rule by reading (`unit_index != NONE && zoom_level != NONE`, as the
division by the weapon's magnification). The log's last field is the zoom level (-1 none).

The zoom action's modes (`input.zoom_mode`, `ae_zoom.c`) have no game test either: the test input cannot hold a button, so the
state machine is `ae_zoom_test.c`'s (every mode, both zoom-level counts, forced unzooms, the tap-time edge), and a game shows
only the log line `zoom: mode ..., hold time ..., reset ...` at the match start. The wheel, the flashlight and the double press are the unit test's too.

## MCC's Custom Edition files

`port/linux/src/ae_mcc.c` finds an MCC folder (`game.mcc_path`, then `HALO_MCC_PATH`, then Steam: Linux's
`$XDG_DATA_HOME/Steam`, `~/.local/share/Steam`, `~/.steam/steam`, `~/.steam/root`, Flatpak's and Snap's; Windows'
registry `SteamPath`, then `InstallPath`, then `C:\Program Files (x86)\Steam`), each root's `libraryfolders.vdf` and
`appmanifest_976730.acf`. `debug.txt` says `mcc: detected: ...` (found, from where; MCC found but incomplete, why; or
not found). Its unit test, `port/linux/tests/ae_mcc_test.c`, builds fake Steam trees (normal, second library, listed
without a manifest, an update under way, an empty shell, Flatpak, quotes and backslashes, a cut bitmaps.map, a moved
library, hostile VDF files). For games: `HALO_MCC_PATH` names the folder for a run (or point `XDG_DATA_HOME`, or
`HOME`, at a fake Steam tree), `HALO_MCC_USE=yes|no|ask`, and `HALO_MCC_BROWSE_RESULT=<folder>` answers Settings'
BROWSE instead of the system's folder dialog. `HALO_MENU_OPEN` opens the Map screen
(`main_menu/multiplayer_type_select/mp_map_select/mp_map_select_screen`) or MAP FILES
(`main_menu/settings_select/player_setup/player_profile_edit/map_files/map_files_screen`) directly.
`tools/test_ae_mcc.py` plays the reads, the question and NEVER ASK AGAIN across a restart with `$AE_MCC_BUILD` (a
build folder: its `config.toml` is removed before each game), `$AE_MCC_PATH` (a folder with the real files) and
`$AE_MCC_DATA` (a data root with homobox in `maps_ce` and no resource maps). The real files are the player's:
never copy them into the repository. Known limit on Windows: the detection opens files with the ANSI C runtime, as
the port's other file calls do, so an MCC folder or Steam library whose path has characters outside the system's
code page is not found (BROWSE does not help there; `maps_ce` does).

## Custom Edition maps: pause SETTINGS and the high-res HUD

A Custom Edition map's load logs, after `Custom Edition map <name>@ce: ... tags`, the pause list's dead Halo PC row
taken out (`... change_settings_button runs a function this game has not: taken out`), `menus: the pause menu has
SETTINGS`, `Custom Edition map: 2 split-screen pause screens laid out as the one player's` and `high-res hud: N of
... bitmaps in this map`. `tools/test_ae_ce_maps.py` plays homobox@ce paused (SETTINGS opened) and in two-player
split screen with `$AE_CE_BUILD` (a linux64 build folder) and `$AE_CE_DATA` (a data root whose `maps/ce` has
homobox and the three resource maps; the player's own files). Its table check runs without them. The Halo PC HUD
bitmaps the high-res textures stand for are `port/linux/src/ae_ce_hud_table.c`, written by `tools/ae_ce_hud.py`
(from an Xbox map and Custom Edition's bitmaps.map; run it again when `port/assets/hud/layout.json` changes).

## Recording

The game records itself: F10 (the Screenshot action, rebindable) saves a screenshot and F9 starts/stops a recording in play, and for tests
`HALO_RECORD_SECONDS=N` records N seconds from the first frame of play into `HALO_RECORD_DIR` (H.264 at the
`capture.record_fps` rate, 60 by default, with AAC sound, muxed by ffmpeg). `record.py --seconds N` / `run.py
--record N` set both, so give the game an exit-after past its start plus N. The clips are listed in result.json
(`recordings`) and the one-line summary (`N mp4 (path)`). With an older build (no `HALO_RECORD_SECONDS`) or
without ffmpeg on the machine, the game runs normally and the result says `record: skipped (<why>)`.

- Build box: ffmpeg installed; `record.py --box` works end to end.
- Windows test box: ffmpeg is staged but not yet deployed there, so recordings on Windows are not possible yet;
  `windows.py` does not ask for them.
