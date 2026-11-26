/*
AE_SCRIPT_NOTICE.C

SCRIPT NOTICE: see ae_script_notice_rules.h. The lines go to local player 0's HUD message list (hud_print_message: the
list HUD STYLE AE draws, ae_hud_messages.c), which keeps each for the level's own message time. Every machine runs the
script check on its own copy of the map, so a co-op guest sees its own count, the same as the host's.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "cache/cache_files.h"
#include "game/game.h"
#include "cutscene/cinematics.h"
#include "camera/director.h"
#include "game/game_engine.h"
#include "interface/hud.h"
#include "interface/hud_messaging.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"

#include "../src/port_config.h"
#include "ae_script_notice.h"
#include "ae_script_notice_rules.h"

void platform_log(char const *format, ...);

static struct
{
	long refused;
	int shown;
	char map[64];
} notice;

void ae_script_notice_set(long refused)
{
	char const *map = cache_file_loaded_map_name();

	notice.refused = refused;
	notice.shown = FALSE;
	snprintf(notice.map, sizeof(notice.map), "%s", map ? map : "");
}

int ae_script_notice_script_forced(char const *script_name)
{
	return ae_script_notice_forced(config_string("debug.script_refuse"), script_name);
}

static void print_line(char const *text)
{
	wchar_t line[AE_SCRIPT_NOTICE_LINE_SIZE];
	short at;

	for (at = 0; text[at] && at < (short)NUMBEROF(line) - 1; at++)
		line[at] = (wchar_t)(unsigned char)text[at];
	line[at] = 0;
	hud_print_message(0, line);
}

void ae_script_notice_tick(void)
{
	char first[AE_SCRIPT_NOTICE_LINE_SIZE], second[AE_SCRIPT_NOTICE_LINE_SIZE];
	char const *map;
	struct scenario *scenario;
	int campaign_running;

	if (notice.refused <= 0 || notice.shown)
		return;
	map = cache_file_loaded_map_name();
	/* (the count belongs to the level it was made for: a level the check did not run for, as one in the Xbox format, has none) */
	if (!map || strcmp(map, notice.map))
	{
		notice.refused = 0;
		return;
	}
	scenario = global_scenario_try_and_get();
	campaign_running = game_in_progress() && !game_engine_running() && scenario && scenario->type == _scenario_type_solo;
	if (!ae_script_notice_due(notice.refused, notice.shown, campaign_running, cinematic_in_progress() || (director_camera_scripted && *director_camera_scripted) || (hud_scripted_globals && !hud_scripted_globals->show_hud), game_time_get()))
		return;
	notice.shown = TRUE;
	if (ae_script_notice_lines(notice.refused, first, second))
	{
		/* (the list puts the newest message on top: the second line first, so the two read in order) */
		print_line(second);
		print_line(first);
		platform_log("script notice: shown for %ld scripts", notice.refused);
	}
}

void ae_script_notice_status(char *text, unsigned long size)
{
	char const *named = config_string("debug.script_refuse");

	snprintf(text, size, "script_refuse: %s%s; this level: %ld scripts switched off, line %s", named && named[0] ? "refusing " : "off",
		named && named[0] ? named : "", notice.refused, notice.shown ? "shown" : "not shown");
}
