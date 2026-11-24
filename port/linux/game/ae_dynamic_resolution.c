/*
AE_DYNAMIC_RESOLUTION.C

TARGET FRAMERATE: the settings, the views, the rules each frame, the log, the performance line (ae_dynamic_resolution.h).
*/

#include <stdio.h>
#include <string.h>

#include "cseries/cseries.h"

#include "ae_dynamic_resolution.h"
#include "ae_dynamic_resolution_rules.h"
#include "ae_target_framerate_rules.h"

const char *config_string(const char *name);
int config_boolean(const char *name);
long config_integer(const char *name);
unsigned long config_changes(void);
void platform_log(const char *format, ...);
/* (source/game/players.c: local_player_count, 0 before the players' globals are made) */
short players_port_local_player_count(void);
/* (src/ae_dynamic_resolution_gl.c) the display's refresh interval in ms (the display mode's; LOW LATENCY's learnt period
only when the mode gives none); 0 when not known. ae_dynres_target_interval_ms makes it the interval a frame is
judged by */
float ae_dynamic_resolution_refresh_interval_ms(void);
/* (source/interface/ui_widget.c) */
boolean main_menu_is_active(void);

static struct
{
	boolean read;
	unsigned long read_at;
	/* (TARGET FRAMERATE) the choices (ae_target_framerate_choice) for one player and for split screen */
	int one_player, split, forced;
	struct ae_dynres_state state;
	/* the percent and views last logged */
	int logged_percent, logged_views;
	/* (0.9.2) display.vsync: the frames are judged by the refresh interval while it holds them */
	int vsync;
	/* (TARGET FRAMERATE) display.max_fps: FRAME RATE LIMIT's cap, which no target goes above with V-SYNC off */
	long max_fps;
	/* (0.9.2) the interval the last frame was judged by (0: the ms rules) */
	float interval_ms;
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
	dynres.one_player = strcmp(config_string("display.resolution_scaling"), "native") ? 0 :
		ae_target_framerate_choice(config_string("display.target_framerate"));
	dynres.split = ae_target_framerate_choice(config_string("display.split_target_framerate"));
	dynres.vsync = config_boolean("display.vsync") != 0;
	dynres.max_fps = config_integer("display.max_fps");
}

/* the views of the game on screen now (0 outside a game: the main menu has its players too, and a slow menu's
frames must not carry a lower scale into the game) */
static int views_now(void)
{
	short players = main_menu_is_active() ? 0 : players_port_local_player_count();

	return players < 0 ? 0 : players > 4 ? 4 : (int)players;
}

/* the frames a second held for these views (0: off), no more than the frames can reach (ae_target_framerate_hz) */
static int on_for(int views)
{
	int choice;
	float refresh_ms;

	settings_read();
	if (views <= 0)
		return 0;
	choice = views == 1 ? dynres.one_player : dynres.split;
	if (!choice)
		return 0;
	refresh_ms = ae_dynamic_resolution_refresh_interval_ms();
	return (int)(ae_target_framerate_hz(choice, dynres.vsync, refresh_ms > 0.0f ? 1000.0f / refresh_ms : 0.0f,
		dynres.max_fps) + 0.5f);
}

int ae_dynamic_resolution_policy(float frame_ms, float shown_ms)
{
	int views = views_now(), percent, on;
	float interval_ms;

	settings_read();
	on = on_for(views);
	/* (display.max_fps does not limit frames with V-SYNC on (sdl_platform.c's frame_interval_ns; an unfocused window's
	limit is the refresh), so the presented gaps the interval rules read are the display's, never a lower cap's) */
	interval_ms = ae_dynres_target_interval_ms(dynres.vsync, dynres.vsync ? ae_dynamic_resolution_refresh_interval_ms() :
		0.0f, (float)on);
	dynres.interval_ms = interval_ms;
	if (dynres.forced && views > 0)
		percent = dynres.forced;
	else
		percent = ae_dynres_frame(&dynres.state, on, views, frame_ms, interval_ms, shown_ms);
	if (percent != dynres.logged_percent || (views != dynres.logged_views && percent != 100))
	{
		const char *reason = dynres.forced && views > 0 ? "" : ae_dynres_take_reason(&dynres.state);
		char refresh[32];

		/* (the log line is the telemetry: what a test or a report reads to know the scale) */
		refresh[0] = 0;
		if (interval_ms > 0.0f)
			snprintf(refresh, sizeof(refresh), ", refresh %.1f ms", (double)interval_ms);
		platform_log("dynamic resolution: %d%% (%d view%s, target %d fps, frame %.1f ms%s)%s%s", percent, views,
			views == 1 ? "" : "s", on, (double)frame_ms, refresh, reason[0] ? " " : "", reason);
		dynres.logged_percent = percent;
		dynres.logged_views = views;
	}
	return percent;
}

int ae_dynamic_resolution_timing_wanted(void)
{
	int views = views_now();

	settings_read();
	/* (0.9.2) under V-SYNC at 100 % the rules see misses from the swaps alone: no GPU timer (ae_dynres_needs_gpu_time) */
	return views > 0 && (dynres.forced || (on_for(views) && ae_dynres_needs_gpu_time(&dynres.state, dynres.interval_ms)));
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
