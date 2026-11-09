/*
AE_MAP_AUTO_PICTURE.C

A picture for a custom map that has none (ae_map_auto_picture.h).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "memory/data.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "main/main.h"
#include "networking/network_game_globals.h"
#include "tag_files/tag_files.h"

#include "halo_ui_map_list.h"
#include "ae_map_auto_picture.h"
#include "ae_map_auto_picture_rules.h"
#include "ae_map_pictures.h"
#include <xtl.h>

void platform_log(char const *format, ...);

enum
{
	PICTURE_PATH_SIZE = 300,
	MAP_NAME_SIZE = 96,
	/* frames the shot may take (the present of the frame after the decision) before it is given up */
	GIVE_UP_FRAMES = 4
};

enum
{
	_phase_look,    /* not yet asked about this map */
	_phase_wait,    /* the map wants a picture: waiting for the first life, then its moment */
	_phase_armed,   /* the decision is made: the next frame is the shot's (the weapon is hidden from it) */
	_phase_shot,    /* the shot's frame: nothing but the 3D view is drawn */
	_phase_done
};

static struct
{
	char map[MAP_NAME_SIZE];
	char path[PICTURE_PATH_SIZE];
	int phase;
	boolean alive;
	long alive_since;
	int shot_frames;
} state;

/* the map of this game as the list names it ("file", "file@pack"), or NULL */
static char const *current_map(void)
{
	char const *path = main_get_multiplayer_map_name();

	if (!path || !path[0])
		return NULL;
	return tag_name_strip_path(path);
}

/* the first question about a map: does it want a picture (done otherwise) */
static void look(char const *map)
{
	char path[PICTURE_PATH_SIZE];
	long row;
	int candidate, stock = 0;

	snprintf(state.map, sizeof(state.map), "%s", map);
	state.phase = _phase_done;
	state.path[0] = 0;
	state.alive = FALSE;
	/* (the list filled for the name: a map a game was set up for without the menus has no row yet; a build without
	the port's map list, the web build, has no rows, so no map there is given a picture) */
#ifdef HALO_MAP_LIST
	row = ui_map_list_lookup(map);
	if (row != NONE)
		stock = ae_map_auto_picture_stock_frame(ui_map_list_picture_index(row));
	if (row != NONE && !stock)
	{
		boolean own = FALSE;

		ui_map_list_xbox_picture(map, &own);
		stock = own;
	}
#else
	row = NONE;
#endif
	candidate = ae_map_picture_missing(map, path, sizeof(path));
	/* (no row: not a map of the list, so not one to give a picture) */
	if (row != NONE && ae_map_auto_picture_wanted(map, candidate, stock, !network_game_distributed_client()))
	{
		snprintf(state.path, sizeof(state.path), "%s", path);
		state.phase = _phase_wait;
	}
}

int ae_map_auto_picture_hides_hud(short local_player_index)
{
	char const *map;
	long player_index, unit_index;

	if (local_player_index != 0)
		return state.phase == _phase_shot || state.phase == _phase_armed;
	if (state.phase == _phase_shot)
	{
		/* (a present that never reads it, no renderer: not hidden for good) */
		if (++state.shot_frames > GIVE_UP_FRAMES)
			state.phase = _phase_done;
		return state.phase == _phase_shot;
	}
	if (state.phase == _phase_armed)
	{
		state.phase = _phase_shot;
		state.shot_frames = 1;
		return TRUE;
	}
	if (!game_in_progress() || !game_engine_running())
		return FALSE;
	map = current_map();
	if (!map)
		return FALSE;
	if (strcmp(map, state.map))
		look(map);
	if (state.phase != _phase_wait)
		return FALSE;
	if (local_player_count() != 1)
	{
		state.phase = _phase_done;
		return FALSE;
	}
	player_index = local_player_get_player_index(0);
	unit_index = player_index != NONE ? player_get(player_index)->unit_index : NONE;
	if (unit_index == NONE)
	{
		/* (before the first life, or after the shot's moment: waiting) */
		if (state.alive)
			state.phase = _phase_done;
		return FALSE;
	}
	if (!state.alive)
	{
		state.alive = TRUE;
		state.alive_since = game_time_get();
	}
	{
		long ticks = game_time_get() - state.alive_since;

		if (ticks > AE_MAP_AUTO_PICTURE_LAST_TICKS)
			state.phase = _phase_done;
		else if (ae_map_auto_picture_shot_due(ticks))
			state.phase = _phase_armed;
	}
	return FALSE;
}

int ae_map_auto_picture_hides_weapon(void)
{
	return state.phase == _phase_armed || state.phase == _phase_shot;
}

int ae_map_auto_picture_capturing(void)
{
	return state.phase == _phase_shot;
}

void ae_map_auto_picture_store(unsigned char const *bgra, int width, int height)
{
	unsigned char *rgba, *png = NULL;
	unsigned int size = ae_map_auto_picture_png_size(AE_MAP_AUTO_PICTURE_WIDTH, AE_MAP_AUTO_PICTURE_HEIGHT), written = 0;
	char temporary[PICTURE_PATH_SIZE + 8];
	char *last;
	HANDLE file;
	boolean done = FALSE;
	unsigned long wrote = 0;

	state.phase = _phase_done;
	rgba = malloc((size_t)AE_MAP_AUTO_PICTURE_WIDTH * AE_MAP_AUTO_PICTURE_HEIGHT * 4);
	png = rgba ? malloc(size) : NULL;
	if (!png || !ae_map_auto_picture_scale(bgra, width, height, rgba) ||
		!ae_map_auto_picture_png(rgba, AE_MAP_AUTO_PICTURE_WIDTH, AE_MAP_AUTO_PICTURE_HEIGHT, png, size, &written))
		goto finish;
	/* (the folder, once: a map's previews folder is made with its first picture) */
	snprintf(temporary, sizeof(temporary), "%s.part", state.path);
	last = strrchr(state.path, '\\');
	if (!last)
		last = strrchr(state.path, '/');
	if (last)
	{
		char separator = *last;

		*last = 0;
		CreateDirectoryA(state.path, NULL);
		*last = separator;
	}
	file = CreateFileA(state.path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (file != INVALID_HANDLE_VALUE)
	{
		/* (a file there since: never over it) */
		CloseHandle(file);
		goto finish;
	}
	file = CreateFileA(temporary, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file != INVALID_HANDLE_VALUE)
	{
		done = WriteFile(file, png, written, &wrote, NULL) && wrote == written;
		if (!CloseHandle(file))
			done = FALSE;
		if (done)
			done = MoveFileA(temporary, state.path);
		if (!done)
			DeleteFileA(temporary);
	}
finish:
	if (done)
		platform_log("map pictures: %s made %s from spawn view", state.map, state.path);
	else
		platform_log("map pictures: %s: no picture made (%s)", state.map, state.path);
	free(png);
	free(rgba);
}
