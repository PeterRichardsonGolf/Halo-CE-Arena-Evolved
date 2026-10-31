"""Release audit (weapon pack plan Task 18): every debug.* setting in port/linux/src/port_config.c is off by default,
so a release build plays as a player expects with no config file and no environment. Off is false, 0, an empty
string, or one of the rows listed in INERT with the reason its default does nothing on its own.

Test-only rows (off by default, never for players): debug.test_run and debug.test_run_auto (TEST RUN, DEV MODE only),
like debug.network_test*."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CONFIG = ROOT / "port" / "linux" / "src" / "port_config.c"

ROW = re.compile(r'\{\s*"(debug\.[a-z0-9_.]+)",\s*(_config_\w+),\s*("(?:[^"\\]|\\.)*")')
OFF = {'"false"', '"0"', '"0.0"', '"\\"\\""'}
# rows whose default is not off but which do nothing unless another setting turns them on (or are not a test aid)
INERT = {
    "debug.network_test_start": "seconds into a debug.network_test game (none unless debug.network_test is set)",
    "debug.network_test_local_players": "local players of a debug.network_test host",
    "debug.network_test_profiles": "profiles of a debug.network_test host's players",
    "debug.telnet_console_port": "the port of debug.telnet_console (off)",
    "debug.gpu_flush_draws": "-1 is off",
    "debug.gpu_trace_frame": "-1 is off",
    "debug.screenshot_format": "the format of debug.screenshot_every's screenshots (0: none)",
    "debug.memory_watch": "Android's write tracking by page protection, upstream's normal mode (not a test aid)",
    "debug.profile_record_when": "when debug.profile_record (off) starts; profiling builds only",
    "debug.profile_memory": "debug.profile_record's buffer; profiling builds only",
    "debug.bot_links": "the bots' normal use of teleporters and layout jumps (bots only; false is for tests)",
    "debug.network_test_cue_players": "players debug.network_test_cues waits for (none unless cues are set: OpenCE's)",
    "debug.bot_vehicles": "the bots' normal vehicle seats, 1 = guns and riding (bots only; 0 and 2 are for tests)",
    "debug.network_bots": "the host's bots in network games (owner 0.3.7: on; none unless bots.count asks; false is "
                          "0.3.6's LAN-only 3)",
    "debug.reticle_growth": "each AE rifle profile's own reticle growth (-1, the normal); a percent is for tests",
    "debug.ae_lobby_countdown_ms": "CREATE GAME's START GAME countdown, its normal length (0.4.3: 5, 4, 3, 2, 1); 0, "
                                   "a start at once, is for tests",
}


def rows():
    return ROW.findall(CONFIG.read_text())


def test_rows_found():
    names = [name for name, _, _ in rows()]
    assert len(names) > 50
    assert len(names) == len(set(names)), "a debug.* row is listed twice"
    # (a settings row is { "debug.x", _config_...; OpenCE's --probe and --server presets are { "debug.x", "value" })
    assert len(re.findall(r'\{ "debug\.[^"]+",\s*_config_', CONFIG.read_text())) == len(names), \
        "a debug.* row the pattern does not read"


def test_every_debug_row_is_off_by_default():
    on = [(name, default) for name, _, default in rows() if default not in OFF and name not in INERT]
    assert not on, f"debug.* rows on by default in a release: {on}"


def test_inert_rows_still_exist():
    names = {name for name, _, _ in rows()}
    assert set(INERT) <= names, set(INERT) - names
