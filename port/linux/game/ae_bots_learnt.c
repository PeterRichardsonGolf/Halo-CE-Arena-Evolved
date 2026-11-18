/*
AE_BOTS_LEARNT.C

What the bots keep of a map from game to game (ae_bots_learnt.h; the rules: ae_bots_learnt_rules.c): the map's learnt file
read and written in the config folder, its wedges and stuck spots handed to ae_path_cost, its jumps to ae_bots_travel's
links, and the people's jumps watched.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "objects/objects.h"
#include "units/units.h"
#include "units/bipeds.h"
#include "../src/port_config.h"
#include "../src/posix.h"

/* (the config folder is a host path: not the game's Xbox-path file calls, port/linux/include/stdio.h; remove and rename
too, or the write's rename fails and only the .new is left, owner's TEST RUN 11-15) */
#undef fopen
#undef remove
#undef rename

#include "ae_layout.h"
#include "ae_layout_rules.h"
#include "ae_path_cost.h"
#include "ae_bots_learnt.h"
#include "ae_bots_learnt_rules.h"

int config_boolean(char const *name);
unsigned long config_changes(void);
void platform_log(char const *format, ...);

#define FOLDER "bots_learnt"
#define FILE_TEXT 16384
/* a person's jumps waiting out their 5 s after landing */
#define PENDING 8
#define PEOPLE 4
/* a step this long in one tick is a teleport, not a flight */
#define TELEPORT_STEP 2.0f

static struct
{
	/* the map's state is the file's for this header; this game's news in it */
	boolean ready;
	struct ae_learnt_map map;
	/* this game: started (its first tick seen), merged into the counts (its end), its seconds of play with bots and of
	driving (ticks) */
	boolean game_open;
	boolean game_merged;
	long last_tick;
	long play_ticks;
	long drive_ticks;
	long drove_tick;
} learnt;

static struct
{
	boolean airborne;
	real_point3d ground;
	real_point3d takeoff;
	long takeoff_tick;
	long unit_index;
	/* stood on the ground since its unit was new (a spawn, a vehicle's exit): a takeoff is only from the ground */
	boolean grounded;
	/* this flight moved too far in one tick (a teleporter): not a jump */
	boolean spoiled;
	real_point3d last;
} people[PEOPLE];

static struct
{
	boolean used;
	long player_index;
	long unit_index;
	float from[3];
	float to[3];
	float air_seconds;
	long landed_tick;
} pending[PENDING];

boolean ae_bots_learnt_on(void)
{
	static unsigned long changes = 0xFFFFFFFFUL;
	static boolean on = TRUE;

	if (config_changes() != changes)
	{
		changes = config_changes();
		on = (boolean)(config_boolean("bots.learn_map") != 0);
	}
	return on;
}

/* the learnt file of the map of this header ("layout N <file> <hash>"): its path */
static boolean file_path(char const *header, char *path, int size, boolean make_folder)
{
	char folder[400], file[64];
	int version;
	unsigned int hash;
	char *dot;

	if (sscanf(header, "layout %d %63s %x", &version, file, &hash) != 3)
		return FALSE;
	/* (the map file's own name, without .map, of letters, digits, _ - and . only) */
	{
		char *c;

		for (c = file; *c; c++)
		{
			if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '_' ||
				*c == '-' || *c == '.'))
				*c = '_';
		}
	}
	dot = strrchr(file, '.');
	if (dot && !strcmp(dot, ".map"))
		*dot = 0;
	config_folder(folder, sizeof folder);
	if (make_folder)
	{
		char made[420];

		snprintf(made, sizeof made, "%s/%s", folder, FOLDER);
		posix_make_directory(made);
	}
	return snprintf(path, (size_t)size, "%s/%s/%s.txt", folder, FOLDER, file) < size;
}

static void write_file(void)
{
	static char text[FILE_TEXT];
	char path[480], temporary[490];
	FILE *file;
	int length;

	if (!learnt.map.header[0] || !file_path(learnt.map.header, path, (int)sizeof path, TRUE))
		return;
	length = ae_learnt_write(&learnt.map, text, (int)sizeof text);
	if (length <= 0)
	{
		platform_log("bots: learnt: %s: too much to write", path);
		return;
	}
	snprintf(temporary, sizeof temporary, "%s.new", path);
	file = fopen(temporary, "wb");
	if (!file)
	{
		platform_log("bots: learnt: %s could not be written", path);
		return;
	}
	if (fwrite(text, 1, (size_t)length, file) != (size_t)length)
	{
		fclose(file);
		platform_log("bots: learnt: %s could not be written", temporary);
		return;
	}
	if (fclose(file) != 0)
	{
		platform_log("bots: learnt: %s could not be written", temporary);
		return;
	}
	/* (Windows: rename does not replace; a crash between the two leaves the .new, read when the file is missing) */
	remove(path);
	if (rename(temporary, path) != 0)
	{
		platform_log("bots: learnt: %s could not be renamed to %s", temporary, path);
		return;
	}
	platform_log("bots: learnt: %s written: %d wedge pieces, %d stuck pieces, %d jumps", path, learnt.map.wedge_count,
		learnt.map.stuck_count, learnt.map.jump_count);
}

/* the game that was going: its news into the counts and the file (once) */
static void close_game(void)
{
	if (!learnt.ready || !learnt.game_open || learnt.game_merged)
		return;
	learnt.game_merged = TRUE;
	/* (switched off: the game's news dropped, nothing written) */
	if (!ae_bots_learnt_on())
		return;
	ae_learnt_game_over(&learnt.map, (int)(learnt.drive_ticks / TICKS_PER_SECOND), (int)(learnt.play_ticks / TICKS_PER_SECOND));
	write_file();
}

void ae_bots_learnt_ready(void)
{
	static char text[FILE_TEXT];
	char header[AE_LEARNT_HEADER], path[480];
	FILE *file;

	/* (the game before: kept, whatever map comes now) */
	close_game();
	if (!ae_bots_learnt_on())
	{
		learnt.ready = FALSE;
		return;
	}
	learnt.game_open = learnt.game_merged = FALSE;
	learnt.play_ticks = learnt.drive_ticks = 0;
	learnt.last_tick = -1;
	csmemset(pending, 0, sizeof(pending));
	csmemset(people, 0, sizeof(people));
	if (!ae_layout_map_header(header, (long)sizeof(header)))
	{
		learnt.ready = FALSE;
		return;
	}
	if (learnt.ready && !strcmp(header, learnt.map.header))
		return;
	text[0] = 0;
	if (file_path(header, path, (int)sizeof path, FALSE))
	{
		char temporary[490];

		/* (the file, or the .new a write left before its rename) */
		snprintf(temporary, sizeof temporary, "%s.new", path);
		file = fopen(path, "rb");
		if (!file)
			file = fopen(temporary, "rb");
		if (file)
		{
			size_t length = fread(text, 1, sizeof text - 1, file);

			fclose(file);
			text[length] = 0;
		}
	}
	if (!ae_learnt_read(&learnt.map, text, header) && text[0])
		platform_log("bots: learnt: %s is another build of this map's: started again", path);
	learnt.ready = TRUE;
	platform_log("bots: learnt: %s: %d wedge pieces, %d stuck pieces, %d jumps", header, learnt.map.wedge_count,
		learnt.map.stuck_count, learnt.map.jump_count);
}

void ae_bots_learnt_map_gone(void)
{
	close_game();
	learnt.ready = FALSE;
}

void ae_bots_learnt_apply_wedges(void)
{
	int index, times;

	ae_bots_learnt_ready();
	if (!learnt.ready || !ae_bots_learnt_on())
		return;
	for (index = 0; index < learnt.map.wedge_count; index++)
	{
		for (times = 0; times < learnt.map.wedges[index].count; times++)
			ae_path_cost_wedge(learnt.map.wedges[index].surface);
	}
}

void ae_bots_learnt_apply_stucks(void)
{
	int index;

	ae_path_cost_stucks_clear();
	ae_bots_learnt_ready();
	if (!learnt.ready || !ae_bots_learnt_on())
		return;
	for (index = 0; index < learnt.map.stuck_count; index++)
	{
		int levels = ae_learnt_stuck_levels(learnt.map.stucks[index].count);

		if (levels > 0)
			ae_path_cost_stuck(learnt.map.stucks[index].surface, levels);
	}
}

void ae_bots_learnt_wedge(long surface)
{
	if (learnt.ready && ae_bots_learnt_on())
		ae_learnt_wedge(&learnt.map, surface);
}

void ae_bots_learnt_stuck(long surface, long bot_index, real x, real y, real z)
{
	if (learnt.ready && ae_bots_learnt_on())
		ae_learnt_stuck(&learnt.map, surface, (int)bot_index, (float)x, (float)y, (float)z);
}

long ae_bots_learnt_deadly_count(void)
{
	return learnt.ready && ae_bots_learnt_on() ? learnt.map.deadly_count : 0;
}

long ae_bots_learnt_deadly_get(long index)
{
	return index >= 0 && index < ae_bots_learnt_deadly_count() ? learnt.map.deadly[index] : NONE;
}

void ae_bots_learnt_deadly(long volume, boolean deadly)
{
	if (!learnt.ready || !ae_bots_learnt_on())
		return;
	if (deadly)
		ae_learnt_deadly_add(&learnt.map, volume);
	else
		ae_learnt_deadly_remove(&learnt.map, volume);
}

long ae_bots_learnt_jump_count(void)
{
	/* (ready already: ae_bots_travel_prepare's ae_bots_learnt_apply_stucks comes before its links are read) */
	return learnt.ready && ae_bots_learnt_on() ? learnt.map.jump_count : 0;
}

boolean ae_bots_learnt_jump_get(long index, real_point3d *from, real_point3d *to)
{
	struct ae_learnt_jump const *jump;

	if (!learnt.ready || index < 0 || index >= learnt.map.jump_count)
		return FALSE;
	jump = &learnt.map.jumps[index];
	from->x = jump->from[0];
	from->y = jump->from[1];
	from->z = jump->from[2];
	to->x = jump->to[0];
	to->y = jump->to[1];
	to->z = jump->to[2];
	return TRUE;
}

void ae_bots_learnt_jump_result(real_point3d const *from, real_point3d const *to, boolean made)
{
	float a[3], b[3];

	/* (after the game's end its news is merged: a late result is not the next game's) */
	if (!learnt.ready || learnt.game_merged || !ae_bots_learnt_on())
		return;
	a[0] = from->x;
	a[1] = from->y;
	a[2] = from->z;
	b[0] = to->x;
	b[1] = to->y;
	b[2] = to->z;
	ae_learnt_jump_result(&learnt.map, ae_learnt_jump_find(&learnt.map, a, b), made ? 1 : 0);
}

void ae_bots_learnt_drove(void)
{
	long now = game_time_get();

	/* (a tick of driving, however many bots drive in it) */
	if (now != learnt.drove_tick)
	{
		learnt.drove_tick = now;
		learnt.drive_ticks++;
	}
}

/* a person's landed jump waits its 5 s: kept if the same unit lives then */
static void pending_tick(long now)
{
	int index;

	for (index = 0; index < PENDING; index++)
	{
		struct player_datum *player;
		struct object_datum *unit;
		float alive;

		if (!pending[index].used)
			continue;
		player = player_try_and_get(pending[index].player_index);
		unit = player && player->unit_index == pending[index].unit_index && player->unit_index != NONE ?
			object_try_and_get_and_verify_type(player->unit_index, _object_mask_unit) : NULL;
		if (!unit || TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
		{
			pending[index].used = FALSE;
			continue;
		}
		alive = (float)(now - pending[index].landed_tick) / TICKS_PER_SECOND;
		if (alive < AE_LEARNT_JUMP_ALIVE)
			continue;
		pending[index].used = FALSE;
		if (ae_learnt_jump_keep(pending[index].from, pending[index].to, pending[index].air_seconds, alive))
		{
			int kept = ae_learnt_jump_add(&learnt.map, pending[index].from, pending[index].to);

			platform_log("bots: learnt: jump %d at %.2f %.2f %.2f to %.2f %.2f %.2f (%d uses): the bots may take it from the next "
				"map load", kept, pending[index].from[0], pending[index].from[1], pending[index].from[2], pending[index].to[0],
				pending[index].to[1], pending[index].to[2], learnt.map.jumps[kept].uses);
		}
	}
}

/* the people on this machine: a jump's takeoff and landing (on foot) */
static void people_tick(long now)
{
	short local;

	for (local = 0; local < PEOPLE; local++)
	{
		long player_index = local_player_get_player_index(local);
		struct player_datum *player = player_index != NONE ? player_get(player_index) : NULL;
		struct biped_datum *biped = player && player->unit_index != NONE ? biped_try_and_get(player->unit_index) : NULL;
		boolean airborne;

		if (!biped || biped->object.parent_object_index != NONE || TEST_FLAG(biped->object.damage_flags, _object_dead_bit))
		{
			people[local].airborne = FALSE;
			people[local].unit_index = NONE;
			continue;
		}
		if (people[local].unit_index != player->unit_index)
		{
			people[local].unit_index = player->unit_index;
			people[local].airborne = FALSE;
			people[local].grounded = FALSE;
			people[local].ground = biped->object.position;
			people[local].last = biped->object.position;
		}
		airborne = (boolean)TEST_FLAG(biped->biped.flags, _biped_airborne_bit);
		{
			real dx = biped->object.position.x - people[local].last.x, dy = biped->object.position.y - people[local].last.y;
			real dz = biped->object.position.z - people[local].last.z;

			if (people[local].airborne && dx * dx + dy * dy + dz * dz > TELEPORT_STEP * TELEPORT_STEP)
				people[local].spoiled = TRUE;
		}
		people[local].last = biped->object.position;
		if (airborne && !people[local].airborne)
		{
			people[local].takeoff = people[local].ground;
			people[local].takeoff_tick = now;
			/* (in the air since a spawn or a vehicle's exit: no takeoff seen) */
			people[local].spoiled = (boolean)!people[local].grounded;
		}
		else if (!airborne && people[local].airborne && people[local].spoiled)
		{
			people[local].spoiled = FALSE;
		}
		else if (!airborne && people[local].airborne)
		{
			real_point3d const *at = &biped->object.position, *from = &people[local].takeoff;
			real dx = at->x - from->x, dy = at->y - from->y, dz = at->z - from->z;
			real distance = (real)sqrt(dx * dx + dy * dy + dz * dz);

			if (distance >= AE_LAYOUT_BOT_JUMP_SHORTEST && distance <= AE_LAYOUT_BOT_JUMP_LONGEST)
			{
				int index;

				for (index = 0; index < PENDING && pending[index].used; index++)
					;
				if (index < PENDING)
				{
					pending[index].used = TRUE;
					pending[index].player_index = player_index;
					pending[index].unit_index = player->unit_index;
					pending[index].from[0] = from->x;
					pending[index].from[1] = from->y;
					pending[index].from[2] = from->z;
					pending[index].to[0] = at->x;
					pending[index].to[1] = at->y;
					pending[index].to[2] = at->z;
					pending[index].air_seconds = (float)(now - people[local].takeoff_tick) / TICKS_PER_SECOND;
					pending[index].landed_tick = now;
				}
			}
		}
		if (!airborne)
		{
			people[local].ground = biped->object.position;
			people[local].grounded = TRUE;
		}
		people[local].airborne = airborne;
	}
}

void ae_bots_learnt_tick(void)
{
	long now;

	if (!learnt.ready || !ae_bots_learnt_on() || !game_engine_running())
		return;
	now = game_time_get();
	/* (a replay of the map without a load: the game before kept first) */
	if (learnt.game_open && now < learnt.last_tick)
	{
		close_game();
		learnt.game_open = learnt.game_merged = FALSE;
		learnt.play_ticks = learnt.drive_ticks = 0;
	}
	learnt.last_tick = now;
	if (game_engine_game_over())
	{
		close_game();
		return;
	}
	if (learnt.game_merged)
		return;
	learnt.game_open = TRUE;
	learnt.play_ticks++;
	people_tick(now);
	pending_tick(now);
}
