/*
BENCHMARK.C

From Halo Content Evolved (Watanoge/halo-content-evolved), AGPL-3.0-or-later. Changed for Arena Evolved: the stage
list, the bots' count, the log lines and the results' file.

A benchmark run without the menus (debug.benchmark in config.toml, HALO_BENCHMARK), the same on every run and every
machine, for comparing builds and settings: it plays a list of maps one after another, measures each for its seconds,
writes what it measured to benchmark.txt and quits (debug.benchmark_quit).

debug.benchmark is "standard" (STANDARD_STAGES), or a list of map:seconds (a campaign level, a10 to d40, or a
multiplayer map, by name), separated by commas. The standard list is the four multiplayer maps, bloodgulch,
damnation, sidewinder and chillout, 15 seconds each, a local Slayer game with debug.benchmark_bots computer players,
who fight with what they find: an open canyon, an enclosed hall of lights and effects, a wide snowfield, and a small
arena where the fighting fills the screen. A campaign level is named in a list and is not in the standard one. A
stage "replay:<file>" is read and answered "not in this build": this build has no replay playback.

A campaign level starts as the Solo menu starts one (normal difficulty, no saved game), each cinematic skipped as a
button press would while it plays and can be, once each SKIP_SECONDS, and is measured from the first second the
player has a unit and no skippable cinematic plays. A multiplayer map starts as debug.network_test's local games do
(network_test.c). Either way the player cannot be hurt (as cheat_deathless_player), WARM_UP_SECONDS go by unmeasured
(a level's first textures and shaders), and while it is measured the player's view turns at TURN_DEGREES a second, a
little down: every run sees the same views in the same order, whatever its frame rate (debug.benchmark_manual leaves
it to the player, to measure the game as played). The bots still play as they will, so two runs differ by a little.
bots.count is set for the run and put back as it was when the benchmark is done.

Each map's frames are timed by the performance counter, from one of the main loop's frames to the next (what the
player sees: vsync and the GPU's waits included). benchmark.txt has a line a map - average frames per second, the 1%
low (the frames per second of the slowest 1% of frames), the median, 99th percentile and longest frame, the frames
over 33 ms - and one for all of them, with the settings that matter. It goes into the data folder. Each map's line is
also written to halo.log.

Called from the main loop every frame (main.c), as network_test.c is.
*/

#include "cseries.h"
#include "main/main.h"
#include "bink/bink_playback.h"
#include "cutscene/cinematics.h"
#include "interface/player_ui.h"
#include "interface/ui_widget.h"
#include "networking/network_game_globals.h"
#include "networking/network_client_manager.h"
#include "networking/network_server_manager.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "hs/hs.h"
#include "memory/data.h"
#include "bots.h"
#include "ae_variant_ext.h"
#include "ae_benchmark_rules.h"
#include "game/cheats.h"
#include "../src/port_config.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <xtl.h>

/* (the data folder is a host path: not the game's Xbox-path file calls) */
#undef fopen

void platform_log(char const *format, ...);
const char *platform_data_root(void);
void platform_request_quit(void);

#define STANDARD_STAGES "bloodgulch:15,damnation:15,sidewinder:15,chillout:15"
#define MAXIMUM_STAGES 16
#define WARM_UP_SECONDS 3.0
#define TURN_DEGREES 18.0
#define LOOK_DOWN_DEGREES 5.0
/* the most a level takes to load and play its cinematics before it is measured anyway */
#define CONTROL_TIMEOUT_SECONDS 180.0
#define MAXIMUM_FRAMES 262144
/* the least time between two skips of a level's cinematics (a skip takes a moment to take) */
#define SKIP_SECONDS 5.0

enum
{
	_benchmark_off,
	_benchmark_menu,
	_benchmark_loading,
	_benchmark_warming,
	_benchmark_measuring,
	_benchmark_leaving,
	_benchmark_done,
};

struct benchmark_stage
{
	char map[32];
	/* the level it loads: levels\<map>\<map> (a campaign level), levels\test\<map>\<map> (a multiplayer map) */
	char level[160];
	/* started as a campaign level (the Solo menu's way, its cinematics skipped) */
	boolean campaign;
	double seconds;
	/* what was measured */
	long frames;
	double average_fps, low_fps, median_ms, p99_ms, longest_ms;
	long over_33;
	boolean timed_out;
	/* why it was not measured (its game ended before it was, or never began; a replay stage), or
	empty */
	char failed[160];
};

static struct
{
	boolean checked;
	short state;
	struct benchmark_stage stages[MAXIMUM_STAGES];
	short stage_count, stage;
	LARGE_INTEGER frequency, last;
	boolean clock_started;
	/* the state's seconds so far, and the seconds the player has had control (nothing to skip); when a cinematic was last
	skipped (state seconds) */
	double state_seconds, control_seconds, skipped_at;
	/* the multiplayer setup's steps done */
	boolean map_set, player_added, started;
	/* (state_seconds when the start was asked for: a lobby frame can take seconds, as the Internet hosting's first
	one took ten, and the game is given its ten from the start, not from the stage's beginning) */
	double started_seconds;
	double yaw, measured_seconds;
	float *frame_ms;
	long frame_count;
	/* all the stages' frames, for the last line */
	double all_ms, all_frames;
	/* bots.count as the run found it, while the benchmark has set it */
	boolean bots_kept;
	long bots_before;
	/* cheat_deathless_player as the run found it, while the benchmark has set it */
	boolean deathless_kept, deathless_before;
} benchmark;

/* ---------- settings */

static boolean campaign_name(char const *name)
{
	return strlen(name) == 3 && name[0] >= 'a' && name[0] <= 'd' && name[1] >= '0' && name[1] <= '9' &&
		name[2] == '0';
}

static void benchmark_read_settings(void)
{
	char list[512];
	char const *setting = config_string("debug.benchmark");
	char *item, *next;

	benchmark.checked = TRUE;
	if (!setting || !*setting)
		return;
	snprintf(list, sizeof(list), "%s", strcmp(setting, "standard") ? setting : STANDARD_STAGES);
	for (item = list; item && *item && benchmark.stage_count < MAXIMUM_STAGES; item = next)
	{
		struct benchmark_stage *stage = &benchmark.stages[benchmark.stage_count];
		char *colon;

		next = strchr(item, ',');
		if (next)
			*next++ = 0;
		while (*item == ' ')
			item++;
		csmemset(stage, 0, sizeof(*stage));
		/* (a replay: replay:<file>[:seconds]; this build has no replay playback) */
		if (!strncmp(item, "replay:", 7))
		{
			item += 7;
			stage->failed[0] = 1;
		}
		colon = strchr(item, ':');
		if (colon)
			*colon = 0;
		if (!*item)
			continue;
		if (!ae_benchmark_seconds(colon ? colon + 1 : NULL, &stage->seconds))
		{
			platform_log("benchmark: %s: seconds must be a number above 0 and at most %.0f: left out", item,
				AE_BENCHMARK_SECONDS_MAXIMUM);
			continue;
		}
		snprintf(stage->map, sizeof(stage->map), "%s", item);
		if (stage->failed[0])
		{
			snprintf(stage->failed, sizeof(stage->failed), "not in this build (no replay playback)");
			benchmark.stage_count++;
			continue;
		}
		stage->campaign = campaign_name(stage->map);
		if (stage->campaign)
			snprintf(stage->level, sizeof(stage->level), "levels\\%s\\%s", stage->map, stage->map);
		else
			snprintf(stage->level, sizeof(stage->level), "levels\\test\\%s\\%s", stage->map, stage->map);
		benchmark.stage_count++;
	}
	if (!benchmark.stage_count)
	{
		platform_log("benchmark: debug.benchmark \"%s\" names no map", setting);
		return;
	}
	benchmark.frame_ms = malloc(MAXIMUM_FRAMES * sizeof(*benchmark.frame_ms));
	if (!benchmark.frame_ms)
		return;
	QueryPerformanceFrequency(&benchmark.frequency);
	benchmark.state = _benchmark_menu;
	platform_log("benchmark: %d maps (debug.benchmark \"%s\")", benchmark.stage_count, setting);
}

/* ---------- the player */

/* the local player's unit, or NONE */
static long benchmark_unit(void)
{
	long player_index = local_player_get_player_index(0);

	if (player_index == NONE)
		return NONE;
	return player_get(player_index)->unit_index;
}

/* the view turned on: TURN_DEGREES a second, a little down */
static void benchmark_turn(double seconds)
{
	real_vector3d facing;
	double pitch = -LOOK_DOWN_DEGREES * 3.14159265358979 / 180.0;

	benchmark.yaw += seconds * TURN_DEGREES * 3.14159265358979 / 180.0;
	facing.i = (real)(cos(benchmark.yaw) * cos(pitch));
	facing.j = (real)(sin(benchmark.yaw) * cos(pitch));
	facing.k = (real)sin(pitch);
	player_control_set_facing(0, &facing);
}

/* ---------- the results */

static int float_compare(void const *first, void const *second)
{
	float a = *(float const *)first, b = *(float const *)second;

	return a < b ? -1 : a > b;
}

static void benchmark_stage_results(struct benchmark_stage *stage)
{
	long count = benchmark.frame_count, index, slowest;
	double total = 0.0, slow_total = 0.0;

	stage->frames = count;
	if (count < 2)
		return;
	for (index = 0; index < count; index++)
		total += benchmark.frame_ms[index];
	qsort(benchmark.frame_ms, (size_t)count, sizeof(*benchmark.frame_ms), float_compare);
	slowest = count / 100 > 0 ? count / 100 : 1;
	for (index = count - slowest; index < count; index++)
		slow_total += benchmark.frame_ms[index];
	stage->average_fps = 1000.0 * count / total;
	stage->low_fps = 1000.0 * slowest / slow_total;
	stage->median_ms = benchmark.frame_ms[count / 2];
	stage->p99_ms = benchmark.frame_ms[count - slowest];
	stage->longest_ms = benchmark.frame_ms[count - 1];
	for (index = 0; index < count; index++)
		stage->over_33 += benchmark.frame_ms[index] > 33.333f;
	benchmark.all_ms += total;
	benchmark.all_frames += count;
	platform_log("benchmark: %s: %.1f frames a second on average, %.1f the 1%% low, %.2f ms the median frame, "
		"%.2f the 99th percentile, %.2f the longest, %ld over 33 ms (%ld frames in %.1f s)%s", stage->map,
		stage->average_fps, stage->low_fps, stage->median_ms, stage->p99_ms, stage->longest_ms, stage->over_33,
		count, total / 1000.0, stage->timed_out ? "; measured without the player's control" : "");
}

static void benchmark_write(void)
{
	char path[600], stamp[32];
	time_t now = time(NULL);
	FILE *file;
	short index;

	snprintf(path, sizeof(path), "%s/benchmark.txt", platform_data_root());
	file = fopen(path, "w");
	if (!file)
	{
		platform_log("benchmark: cannot write %s", path);
		return;
	}
	strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
	fprintf(file, "Arena Evolved benchmark, %s, on %s\n", stamp,
#if defined(_WIN32)
		"Windows"
#elif defined(__APPLE__)
		"macOS"
#elif defined(__ANDROID__)
		"Android"
#else
		"Linux"
#endif
	);
	fprintf(file, "settings: resolution \"%s\", anti_aliasing \"%s\", shadow_resolution %ld, "
		"vsync %s, max_fps %ld, bots %ld\n\n", config_string("display.resolution"),
		config_string("display.anti_aliasing"), config_integer("display.shadow_resolution"),
		config_boolean("display.vsync") ? "on" : "off", config_integer("display.max_fps"),
		config_integer("debug.benchmark_bots"));
	fprintf(file, "%-12s %7s %7s %8s %9s %9s %8s %8s %8s\n", "map", "seconds", "frames", "avg fps", "1 pct low",
		"median ms", "p99 ms", "max ms", ">33 ms");
	for (index = 0; index < benchmark.stage_count; index++)
	{
		struct benchmark_stage const *stage = &benchmark.stages[index];

		if (stage->failed[0])
		{
			fprintf(file, "%-12s not measured: %s\n", stage->map, stage->failed);
			continue;
		}
		fprintf(file, "%-12s %7.1f %7ld %8.1f %9.1f %9.2f %8.2f %8.2f %8ld%s\n", stage->map, stage->seconds,
			stage->frames, stage->average_fps, stage->low_fps, stage->median_ms, stage->p99_ms, stage->longest_ms,
			stage->over_33, stage->timed_out ? "  (the player had no control: measured anyway)" : "");
	}
	if (benchmark.all_frames > 0)
	{
		fprintf(file, "%-12s %7.1f %7ld %8.1f\n", "all", benchmark.all_ms / 1000.0, (long)benchmark.all_frames,
			1000.0 * benchmark.all_frames / benchmark.all_ms);
	}
	fclose(file);
	platform_log("benchmark: the results are in %s", path);
}

/* bots.count for the run: the multiplayer stages' (benchmark_bots), then as it was (-1) */
static void benchmark_bots_set(long count)
{
	char text[16];

	if (count < 0)
	{
		if (!benchmark.bots_kept)
			return;
		count = benchmark.bots_before;
		benchmark.bots_kept = FALSE;
	}
	else if (!benchmark.bots_kept)
	{
		benchmark.bots_before = config_integer("bots.count");
		benchmark.bots_kept = TRUE;
	}
	snprintf(text, sizeof(text), "%ld", count);
	config_set("bots.count", text);
}

/* ---------- the stages */

static void benchmark_start_stage(struct benchmark_stage const *stage)
{
	benchmark.map_set = benchmark.player_added = benchmark.started = FALSE;
	benchmark.control_seconds = 0.0;
	benchmark.skipped_at = -SKIP_SECONDS;
	if (stage->campaign)
	{
		/* as the Solo menu starts a level (menu_functions.c's campaign_start) */
		benchmark_bots_set(-1);
		game_connection_set(_game_connection_local);
		main_set_map_name(stage->level);
		main_disallow_persistent_storage();
		main_set_difficulty(_game_difficulty_level_normal);
		main_menu_switch_to_single_player();
		platform_log("benchmark: %s, the campaign level", stage->map);
		return;
	}
	/* a local game, as debug.network_test's "local:" sets one up (network_test.c), with its bots */
	benchmark_bots_set(config_integer("debug.benchmark_bots"));
	main_set_multiplayer_map_name(stage->level);
	ui_widgets_close_all();
	dispose_global_network_game_server();
	dispose_global_network_game_client();
	network_game_accept_remote_connections(FALSE);
	game_engine_playlist_initialize();
	if (create_global_network_game_server())
	{
		game_engine_playlist_begin();
		game_connection_set(_game_connection_network_server);
	}
	if (!global_network_game_client_get())
		create_global_network_game_client();
	ui_widget_load_by_name_or_tag(
		"ui\\shell\\main_menu\\multiplayer_type_select\\split_screen\\pregame\\splitscreen_pregame_wrapper_normal",
		NONE, NULL, NONE, NONE, NONE, NONE);
	platform_log("benchmark: %s, a local Slayer game with %ld bots", stage->map, config_integer("debug.benchmark_bots"));
}

/* the local game's lobby: its map and variant, a player, the start (network_test.c's steps) */
static void benchmark_multiplayer_setup(struct benchmark_stage const *stage)
{
	if (!benchmark.map_set && benchmark.state_seconds >= 1.0 && global_network_game_server_get())
	{
		struct game_variant variant;

		network_game_server_change_map_name(global_network_game_server_get(), stage->level);
		variant = *game_engine_get_variant_by_name(&variant, "slayer");
		player_ui_set_game_variant(&variant);
		ae_variant_ext_pending_apply(); /* (AE's own rules of the gametype, as network_test.c's lobby sets them) */
		network_game_server_change_game_variant(global_network_game_server_get(), &variant);
		benchmark.map_set = TRUE;
	}
	if (!benchmark.player_added && benchmark.state_seconds >= 2.0 && global_network_game_client_get())
		benchmark.player_added = network_game_client_add_player(global_network_game_client_get(), 0);
	if (!benchmark.started && benchmark.player_added && benchmark.state_seconds >= 3.0)
	{
		benchmark.started = TRUE;
		benchmark.started_seconds = benchmark.state_seconds;
		network_game_client_request_immediate_start();
	}
}

/* whether the player has control: a unit, the game running, nothing to skip */
static boolean benchmark_in_control(struct benchmark_stage const *stage)
{
	if (!game_in_progress() || benchmark_unit() == NONE)
		return FALSE;
	/* (a cinematic that is playing and can be skipped is waited for, skipped; a skippable stretch with none playing, as a
	level's opening, is the player's: its skip, a revert to where it began, would only begin it again) */
	if (stage->campaign)
		return !(cinematic_in_progress() && cinematic_can_be_skipped());
	return game_engine_running();
}

/* cheat_deathless_player on for the stages, and as it was before the first one when they are done or one fails */
static void benchmark_deathless(boolean on)
{
	if (on)
	{
		if (!benchmark.deathless_kept)
		{
			benchmark.deathless_before = cheat.deathless_player;
			benchmark.deathless_kept = TRUE;
		}
		hs_compile_and_evaluate("cheat_deathless_player 1");
	}
	else if (benchmark.deathless_kept)
	{
		cheat.deathless_player = benchmark.deathless_before;
		benchmark.deathless_kept = FALSE;
	}
}

static void benchmark_leave(struct benchmark_stage const *stage)
{
	if (stage->campaign)
		main_goto_main_menu();
	else
		network_game_abort();
	benchmark_bots_set(-1);
	benchmark_deathless(FALSE);
}

/* the stage's game is over or gone: back at the main menu, or no game in progress, or (a multiplayer map) its engine not running */
static boolean benchmark_game_ended(struct benchmark_stage const *stage, boolean main_menu_loaded)
{
	return main_menu_loaded || !game_in_progress() || (!stage->campaign && !game_engine_running());
}

/* the stage is not measured: its line says why; the game is left (when it is not at the main menu already) and the next stage
comes */
static void benchmark_stage_failed(struct benchmark_stage *stage, boolean main_menu_loaded, char const *why)
{
	snprintf(stage->failed, sizeof(stage->failed), "%s", why);
	platform_log("benchmark: %s: %s", stage->map, why);
	if (main_menu_loaded)
	{
		benchmark_bots_set(-1);
		benchmark_deathless(FALSE);
		benchmark.stage++;
		benchmark.state = _benchmark_menu;
	}
	else
	{
		benchmark_leave(stage);
		benchmark.state = _benchmark_leaving;
	}
	benchmark.state_seconds = 0.0;
}

void benchmark_update(
	boolean main_menu_loaded)
{
	LARGE_INTEGER now;
	double seconds = 0.0;
	struct benchmark_stage *stage;

	if (!benchmark.checked)
		benchmark_read_settings();
	if (benchmark.state == _benchmark_off || benchmark.state == _benchmark_done)
		return;
	QueryPerformanceCounter(&now);
	if (benchmark.clock_started)
		seconds = (double)(now.QuadPart - benchmark.last.QuadPart) / (double)benchmark.frequency.QuadPart;
	benchmark.clock_started = TRUE;
	benchmark.last = now;
	benchmark.state_seconds += seconds;
	stage = &benchmark.stages[benchmark.stage];

	switch (benchmark.state)
	{
	case _benchmark_menu:
		if (!main_menu_loaded)
			break;
		/* (the intro movie first, as network_test.c skips it: starting a map while it plays asserts) */
		if (bink_playback_in_progress())
		{
			bink_playback_stop();
			benchmark.state_seconds = 0.0;
			break;
		}
		/* (the main menu settling) */
		if (benchmark.state_seconds < 2.0)
			break;
		if (benchmark.stage >= benchmark.stage_count)
		{
			benchmark_write();
			benchmark_bots_set(-1);
			benchmark_deathless(FALSE);
			benchmark.state = _benchmark_done;
			if (config_boolean("debug.benchmark_quit"))
				platform_request_quit();
			break;
		}
		/* (a replay stage, which this build cannot play: the next stage, its line saying why) */
		if (stage->failed[0])
		{
			platform_log("benchmark: %s: %s", stage->map, stage->failed);
			benchmark.stage++;
			break;
		}
		benchmark_start_stage(stage);
		benchmark.state_seconds = 0.0;
		benchmark.state = _benchmark_loading;
		break;
	case _benchmark_loading:
		/* (a game that ended before it was measured, as a local game whose host went down, or one that never began:
		the next map, this one's line saying so) */
		if (main_menu_loaded && ((!stage->campaign && benchmark.started &&
			benchmark.state_seconds - benchmark.started_seconds >= 10.0) ||
			benchmark.state_seconds >= CONTROL_TIMEOUT_SECONDS))
		{
			snprintf(stage->failed, sizeof(stage->failed), "%s", benchmark.started ?
				"its game ended at the main menu before it was measured" : "it did not begin");
			platform_log("benchmark: %s: %s", stage->map, stage->failed);
			/* (at the main menu already: nothing to leave; the next map's start sets its own game up) */
			benchmark_bots_set(-1);
			benchmark_deathless(FALSE);
			benchmark.stage++;
			benchmark.state = _benchmark_menu;
			benchmark.state_seconds = 0.0;
			break;
		}
		if (!stage->campaign && main_menu_loaded)
			benchmark_multiplayer_setup(stage);
		/* (each cinematic skipped while it plays and can be, as a button press would; once each SKIP_SECONDS: a skip
		reverts the game to where the skippable stretch began, each a fade through black, and skipped every frame, a
		level's opening is reverted over and over in black) */
		if (stage->campaign && game_in_progress() && !main_menu_loaded && cinematic_in_progress() &&
			cinematic_can_be_skipped() && benchmark.state_seconds - benchmark.skipped_at >= SKIP_SECONDS)
		{
			benchmark.skipped_at = benchmark.state_seconds;
			main_skip_cinematic();
			platform_log("benchmark: %s: skipped a cinematic, %.0f s in", stage->map, benchmark.state_seconds);
		}
		if (!main_menu_loaded && benchmark_in_control(stage))
			benchmark.control_seconds += seconds;
		else
			benchmark.control_seconds = 0.0;
		/* (what is waited for, every 10 seconds: a level that does not reach play says why) */
		if (!main_menu_loaded && game_in_progress() &&
			(long)(benchmark.state_seconds / 10.0) != (long)((benchmark.state_seconds - seconds) / 10.0))
		{
			platform_log("benchmark: %s: %.0f s in: a cinematic %s%s, the player %s, the game engine %s", stage->map,
				benchmark.state_seconds, cinematic_in_progress() ? "playing" : "not playing",
				cinematic_can_be_skipped() ? " (can be skipped)" : "", benchmark_unit() != NONE ? "has a unit" : "has no unit",
				game_engine_running() ? "running" : "not running");
		}
		if (benchmark.control_seconds >= 1.0 ||
			(benchmark.state_seconds >= CONTROL_TIMEOUT_SECONDS && !main_menu_loaded && game_in_progress()))
		{
			stage->timed_out = benchmark.control_seconds < 1.0;
			if (stage->timed_out)
				platform_log("benchmark: %s: the player has no control after %.0f s; measured anyway", stage->map,
					benchmark.state_seconds);
			benchmark_deathless(TRUE);
			benchmark.state = _benchmark_warming;
			benchmark.state_seconds = 0.0;
		}
		break;
	case _benchmark_warming:
		if (benchmark_game_ended(stage, main_menu_loaded))
		{
			benchmark_stage_failed(stage, main_menu_loaded, "its game ended before it was measured");
			break;
		}
		if (benchmark.state_seconds >= WARM_UP_SECONDS)
		{
			real_vector3d facing;

			player_control_get_facing_direction(0, &facing);
			benchmark.yaw = atan2((double)facing.j, (double)facing.i);
			benchmark.frame_count = 0;
			benchmark.measured_seconds = 0.0;
			benchmark.state = _benchmark_measuring;
			benchmark.state_seconds = 0.0;
			platform_log("benchmark: %s: measuring %.0f s", stage->map, stage->seconds);
		}
		break;
	case _benchmark_measuring:
		if (benchmark_game_ended(stage, main_menu_loaded))
		{
			benchmark_stage_failed(stage, main_menu_loaded, "its game ended while it was measured");
			break;
		}
		if (benchmark.frame_count < MAXIMUM_FRAMES && seconds > 0.0)
			benchmark.frame_ms[benchmark.frame_count++] = (float)(seconds * 1000.0);
		benchmark.measured_seconds += seconds;
		/* (debug.benchmark_manual: the player's own, to measure play with a pad or a mouse) */
		if (benchmark_unit() != NONE && !config_boolean("debug.benchmark_manual"))
			benchmark_turn(seconds);
		if (benchmark.measured_seconds >= stage->seconds)
		{
			platform_log("benchmark: %s: measured", stage->map);
			benchmark_stage_results(stage);
			benchmark_leave(stage);
			benchmark.state = _benchmark_leaving;
			benchmark.state_seconds = 0.0;
		}
		break;
	case _benchmark_leaving:
		/* (back at the main menu: the next map) */
		if (main_menu_loaded)
		{
			benchmark.stage++;
			benchmark.state = _benchmark_menu;
			benchmark.state_seconds = 0.0;
		}
		break;
	}
}
