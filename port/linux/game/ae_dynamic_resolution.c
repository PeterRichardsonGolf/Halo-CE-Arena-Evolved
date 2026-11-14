/*
AE_DYNAMIC_RESOLUTION.C

TARGET 60 FPS: the settings, the views, the rules each frame, the log, the performance line (ae_dynamic_resolution.h).
*/

#include <stdio.h>
#include <string.h>

#include "cseries/cseries.h"

#include "ae_dynamic_resolution.h"
#include "ae_dynamic_resolution_rules.h"

const char *config_string(const char *name);
int config_boolean(const char *name);
long config_integer(const char *name);
unsigned long config_changes(void);
void platform_log(const char *format, ...);
/* (source/game/players.c: local_player_count, 0 before the players' globals are made) */
short players_port_local_player_count(void);
/* (source/interface/ui_widget.c) */
boolean main_menu_is_active(void);

static struct
{
	boolean read;
	unsigned long read_at;
	int one_player, split, forced;
	struct ae_dynres_state state;
	/* the percent and views last logged */
	int logged_percent, logged_views;
} dynres = { FALSE, 0, 0, 0, 0, { 100, 0, 0, 0, 0, 0 }, 100, 0 };

static void settings_read(void)
{
	if (dynres.read && dynres.read_at == config_changes())
		return;
	{
		int forced = ae_dynres_forced(config_integer("debug.dynamic_resolution_force"));

		if (forced && forced != dynres.forced)
			platform_log("dynamic resolution: held at %d%% (debug.dynamic_resolution_force)", forced);
		dynres.forced = forced;
	}
	dynres.read = TRUE;
	dynres.read_at = config_changes();
	/* (one player: only at the screen's resolution; RESOLUTION SCALING's smaller ones are already a low picture) */
	dynres.one_player = config_boolean("display.target_60_fps") &&
		!strcmp(config_string("display.resolution_scaling"), "native");
	dynres.split = config_boolean("display.split_target_60_fps") != 0;
}

/* the views of the game on screen now (0 outside a game: the main menu has its players too, and a slow menu's
frames must not carry a lower scale into the game) */
static int views_now(void)
{
	short players = main_menu_is_active() ? 0 : players_port_local_player_count();

	return players < 0 ? 0 : players > 4 ? 4 : (int)players;
}

static int on_for(int views)
{
	settings_read();
	if (views <= 0)
		return 0;
	return views == 1 ? dynres.one_player : dynres.split;
}

int ae_dynamic_resolution_policy(float frame_ms)
{
	int views = views_now(), percent;

	settings_read();
	if (dynres.forced && views > 0)
		percent = dynres.forced;
	else
		percent = ae_dynres_frame(&dynres.state, on_for(views), views, frame_ms);
	if (percent != dynres.logged_percent || (views != dynres.logged_views && percent != 100))
	{
		/* (the log line is the telemetry: what a test or a report reads to know the scale) */
		platform_log("dynamic resolution: %d%% (%d view%s, frame %.1f ms)", percent, views, views == 1 ? "" : "s",
			(double)frame_ms);
		dynres.logged_percent = percent;
		dynres.logged_views = views;
	}
	return percent;
}

int ae_dynamic_resolution_shown_percent(void)
{
	int views = views_now();

	settings_read();
	if (views <= 0 || (!dynres.forced && !on_for(views)))
		return -1;
	return dynres.forced ? dynres.forced : dynres.state.percent;
}

void ae_dynamic_resolution_append(char *line, long size, long level)
{
	int percent = ae_dynamic_resolution_shown_percent();
	char field[16];
	char *after, *rest;
	long length, field_length;

	if (percent < 0 || (level != 2 && level != 3) || !line || size <= 0)
		return;
	/* (after FRAME's value and its MS) */
	after = strstr(line, "FRAME ");
	after = after ? strstr(after, " MS") : NULL;
	if (!after)
		return;
	after += 3;
	sprintf(field, "  RES %d%%", percent);
	field_length = (long)strlen(field);
	length = (long)strlen(line);
	if (length + field_length >= size)
		return;
	rest = after;
	memmove(rest + field_length, rest, (size_t)(length - (rest - line) + 1));
	memcpy(rest, field, (size_t)field_length);
}
