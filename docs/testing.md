# Testing Arena Evolved: the SOP

One harness, `tools/ae_test/`, does every automated game test. Use it instead of writing new scripts;
if it lacks something, add it there (with a test in `tools/test_ae_test.py`).

## Which tool for which job

| Job | Command (add `--box` to run it on the build box from the laptop) |
|---|---|
| Build a commit (`linux64`; `--targets all` = linux64, linux, both servers) | `python3 tools/ae_test/build.py --commit <rev> --box` |
| One game from a spec (map, gametype, flags, split screen, bots, mod, window, screenshots, env) | `python3 tools/ae_test/run.py --build <rev> --map bloodgulch --players 2 --bots 3 --box` |
| The per-merge smoke set, against a baseline | `python3 tools/ae_test/smoke.py --build <rev> --baseline <rev> --box` |
| Split-screen pictures, before vs after (1p/2p/3p/4p, 4p at 720p), reticle offsets | `python3 tools/ae_test/sheet.py --after <rev> --before <rev> [--reticle --flat] --box` |
| Host/client pairs with stock Chupa / stock OpenCE (version, Delta, positions, kills) | `python3 tools/ae_test/handshake.py --ae <rev> --other stock --box` |
| A Windows CI build on the Windows test box | `python3 tools/ae_test/windows.py --artifact arena-evolved-windows64-release` |
| An MP4 clip of a game (H.264 + AAC; needs ffmpeg on the machine) | `python3 tools/ae_test/record.py --build <rev> --map bloodgulch --seconds 20 --box` |
| A copy of a save root with a saved gametype's variant bytes patched and signed again (for `--save-from`; never in place, never a real save root) | `python3 tools/ae_test/gametype_file.py --from <save root> --to <new folder> --name "NHE 1V1" --set 0x1d=3` (on the machine that plays) |
| A save root's seeded gametypes against golden hashes (`tools/ae_test/golden_seeds_a17102e2.json`, a17102e2's seeds; `--rename OLD=NEW` for migrated names) or against another root (byte-identical, `--same-as`) | `python3 tools/ae_test/seed_compare.py <save root> --golden tools/ae_test/golden_seeds_a17102e2.json` |

`<rev>` is any git revision (pushed to the box as `refs/ae-test/<sha12>` and built once in its own worktree),
a named build from the config (e.g. `stock`), or a folder holding a `halo` binary. `run.py --list-keys` lists
the spec keys; `run.py --spec game.json` takes a whole spec; `--dry-run` shows the game's environment.

Every tool writes `<out_dir>/<tool>/<time stamp or --out-name>/`: `summary.txt` (one line per game, or the
table), `summary.json`, and per game `result.json`, `debug.txt`, `stdout.log`, `shots/*.png`, `bots.log`. With
`--box` the folder is copied back to the laptop's `out_dir`. Setup: copy `tools/ae_test/config.sample.json` to
`~/.config/ae_test/config.json` on each machine and fill it in (the laptop's has `box` and `windows`; the
box's has `repo`, `data_dir`, `named_builds`).

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

## The owner's game

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

- Save roots and work folders **never under /tmp** (a RAM disk on the laptop; map caches are up to 290 MB each).
  The tools refuse it, and delete each game's work folder afterwards (`--keep-work` keeps it).
- Screenshots: **PNG, never BMP**. On Linux, builds with the capture feature write PNGs themselves (run, smoke,
  sheet and handshake set `HALO_SCREENSHOT_FORMAT=png`; frames the game had to skip are counted in the result);
  older builds (stock Chupa/OpenCE in handshakes) write BMPs, which the tools convert and delete. `windows.py` still
  has the Windows build write BMPs and converts them on the laptop.
- Verdicts read `debug.txt`: exit code and the clean-exit line, asserts, exceptions, refusals ("cannot be
  played"), lost scripts ("scripts won't run"), the last network-test tick, items, scenario scripts.

## The Windows test box (a shared PC: people, children included, use it)

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
only the log line `zoom: mode ..., hold time ...` at the match start.

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
