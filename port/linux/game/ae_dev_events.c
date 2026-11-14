/*
AE_DEV_EVENTS.C

DEV MODE's event stream (ae_dev_events.h). The writer, and the per-tick watch of the game's state: what changed since
the last tick, as events. Nothing of it runs while DEV MODE is off.
*/

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "items/weapons.h"
#include "main/main.h"
#include "objects/objects.h"
#include "scenario/scenario.h"
#include "tag_files/tag_files.h"
#include "units/units.h"
#include "../src/port_config.h"
#include "../src/posix.h"

#include "ae_dev_events.h"
#include "ae_dev_events_rules.h"
#include "ae_dev_mode.h"

/* (stdio.h's fopen takes Xbox paths under the data root, as the game's code uses it: the stream's path is a host
path in the config folder, so this one is the C library's; remove too, for the old streams) */
#undef fopen
#undef remove

void platform_log(char const *format, ...);
unsigned long system_milliseconds(void);

/* the stream names read from the folder at most, for the clean-up (more: the rest wait for a later run) */
#define OLD_STREAMS 1024

/* the vehicles and items followed at once */
#define TRACKED 256

static FILE *stream;
static int stream_failed;
static char stream_path[512];
static unsigned long stream_opened_at;

/* each player slot as the last tick left it */
static struct
{
	long player_index; /* NONE: no player in the slot */
	long unit_index;   /* NONE: dead or not spawned */
	long weapon_definition;
	short loaded, total;
	long individual, team;
} players[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];

/* the vehicles and items seen: their object index, definition, whether dead (a vehicle), seen this tick */
static struct
{
	long object_index, definition_index;
	boolean dead, seen, vehicle;
} tracked[TRACKED];
static short tracked_count;

static boolean game_was_running;
/* the game has ticked once (its globals are up): error()'s lines come from the start of the run, before the game's
time can be read */
static boolean ticked;
static long last_game_time;

int ae_dev_events_on(void)
{
	return ae_dev_mode_on();
}

const char *ae_dev_events_path(void)
{
	return stream_path;
}

void ae_dev_event_text(char *out, size_t size, const char *text)
{
	size_t used = 0;

	if (!size)
		return;
	for (; text && *text && used + 1 < size; text++)
	{
		unsigned char c = (unsigned char)*text;

		if (c == '"' || c == '\\')
		{
			if (used + 2 >= size)
				break;
			out[used++] = '\\';
			out[used++] = (char)c;
		}
		else if (c < 0x20)
		{
			if (used + 6 >= size)
				break;
			snprintf(out + used, size - used, "\\u%04x", c);
			used += 6;
		}
		else
			out[used++] = (char)c;
	}
	out[used] = 0;
}

/* the folder's old streams beyond the newest AE_DEV_EVENTS_KEEP removed (the one just opened counts as one), only files
named as a stream (ae_dev_events_rules.h); one line saying how many went */
static void old_streams_remove(char const *folder)
{
	static char names[OLD_STREAMS][32];
	static char const *listed[OLD_STREAMS];
	static int picked[OLD_STREAMS];
	char name[256], path[512];
	void *directory = posix_directory_open(folder);
	int count = 0, removed = 0, index, picks;

	if (!directory)
		return;
	while (count < OLD_STREAMS && posix_directory_next(directory, name, sizeof name))
	{
		if (!ae_dev_events_is_stream(name))
			continue;
		snprintf(names[count], sizeof names[count], "%s", name);
		listed[count] = names[count];
		count++;
	}
	posix_directory_close(directory);
	picks = ae_dev_events_to_remove(listed, count, AE_DEV_EVENTS_KEEP, picked);
	for (index = 0; index < picks; index++)
	{
		if (snprintf(path, sizeof path, "%s/%s", folder, listed[picked[index]]) < (int)sizeof path && !remove(path))
			removed++;
	}
	platform_log("dev events: %d old event files removed (the newest %d kept)%s", removed, AE_DEV_EVENTS_KEEP,
		removed < picks ? ", some could not be" : "");
}

static boolean stream_open(void)
{
	char folder[400];
	time_t now;
	struct tm *local;
	char stamp[32];

	if (stream)
		return TRUE;
	if (stream_failed)
		return FALSE;
	config_folder(folder, sizeof folder);
	if (strlen(folder) + 4 < sizeof folder)
		strcat(folder, "/dev");
	posix_make_directory(folder);
	now = time(NULL);
	local = localtime(&now);
	if (!local || !strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", local))
		snprintf(stamp, sizeof stamp, "%ld", (long)now);
	snprintf(stream_path, sizeof stream_path, "%s/events-%s.jsonl", folder, stamp);
	stream = fopen(stream_path, "a");
	if (!stream)
	{
		platform_log("dev events: could not open %s", stream_path);
		stream_path[0] = 0;
		stream_failed = TRUE;
		return FALSE;
	}
	stream_opened_at = system_milliseconds();
	platform_log("dev events: %s", stream_path);
	old_streams_remove(folder);
	return TRUE;
}

void ae_dev_event(const char *ev, const char *members_format, ...)
{
	char members[768];
	va_list arguments;

	if (!ae_dev_events_on() || !stream_open())
		return;
	members[0] = 0;
	if (members_format && members_format[0])
	{
		va_start(arguments, members_format);
		vsnprintf(members, sizeof members, members_format, arguments);
		va_end(arguments);
	}
	fprintf(stream, "{\"t\":%lu,\"tick\":%ld,\"ev\":\"%s\"%s%s}\n", system_milliseconds() - stream_opened_at,
		game_in_progress() ? game_time_get() : -1L, ev, members[0] ? "," : "", members);
	fflush(stream);
}

void ae_dev_events_log(char const *line)
{
	static int writing;
	char text[400];
	size_t length;

	if (writing || !line || !ticked || !ae_dev_events_on())
		return;
	writing = 1;
	ae_dev_event_text(text, sizeof text, line);
	/* (its "\r\n", escaped: cut) */
	length = strlen(text);
	while (length >= 2 && text[length - 2] == '\\' && (text[length - 1] == 'n' || text[length - 1] == 'r'))
		text[length -= 2] = 0;
	ae_dev_event("log", "\"text\":\"%s\"", text);
	writing = 0;
}

/* ---------- the watch */

static void tag_text(long definition_index, char *out, size_t size)
{
	char const *name = definition_index != NONE ? tag_get_name(definition_index) : NULL;

	ae_dev_event_text(out, size, name ? name : "");
}

static void players_forget(void)
{
	short slot;

	for (slot = 0; slot < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; slot++)
	{
		players[slot].player_index = NONE;
		players[slot].unit_index = NONE;
		players[slot].weapon_definition = NONE;
		players[slot].loaded = players[slot].total = 0;
		players[slot].individual = players[slot].team = 0;
	}
}

static void players_watch(void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	boolean present[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	short slot;

	memset(present, 0, sizeof present);
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		long unit_index = player->unit_index, weapon_index = NONE, definition = NONE, individual, team;
		short loaded = 0, total = 0;

		slot = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index);
		if (slot < 0 || slot >= HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
			continue;
		present[slot] = TRUE;
		if (players[slot].player_index != iterator.datum_index)
		{
			players[slot].player_index = iterator.datum_index;
			players[slot].unit_index = NONE;
			players[slot].weapon_definition = NONE;
			players[slot].individual = players[slot].team = 0;
		}
		if (unit_index != NONE && TEST_FLAG(unit_get(unit_index)->object.damage_flags, _object_dead_bit))
			unit_index = NONE;
		if (unit_index != players[slot].unit_index)
		{
			if (unit_index != NONE)
			{
				real_point3d at;

				object_get_origin(unit_index, &at);
				ae_dev_event("spawn", "\"player\":%d,\"x\":%.2f,\"y\":%.2f,\"z\":%.2f", (int)slot, at.x, at.y, at.z);
			}
			else
				ae_dev_event("death", "\"player\":%d", (int)slot);
			players[slot].unit_index = unit_index;
		}
		if (unit_index != NONE)
		{
			struct unit_datum *unit = unit_get(unit_index);
			short current = unit->unit.current_weapon_index;

			if (current >= 0 && current < MAXIMUM_WEAPONS_PER_UNIT)
				weapon_index = unit->unit.weapon_object_indices[current];
			if (weapon_index != NONE)
			{
				struct weapon_datum *weapon = weapon_get(weapon_index);

				definition = weapon->definition_index;
				loaded = weapon->weapon.magazines[0].rounds_loaded;
				total = weapon->weapon.magazines[0].rounds_total;
			}
		}
		if (definition != players[slot].weapon_definition)
		{
			char name[160];

			tag_text(definition, name, sizeof name);
			if (unit_index != NONE)
				ae_dev_event("weapon", "\"player\":%d,\"weapon\":\"%s\",\"loaded\":%d,\"total\":%d", (int)slot, name,
					(int)loaded, (int)total);
		}
		else if (definition != NONE && (loaded > players[slot].loaded || total > players[slot].total))
		{
			char name[160];

			tag_text(definition, name, sizeof name);
			ae_dev_event("ammo", "\"player\":%d,\"weapon\":\"%s\",\"loaded\":%d,\"total\":%d", (int)slot, name,
				(int)loaded, (int)total);
		}
		players[slot].weapon_definition = definition;
		players[slot].loaded = loaded;
		players[slot].total = total;
		if (game_engine && game_engine->get_player_score)
		{
			individual = game_engine->get_player_score(iterator.datum_index, _get_score_individual);
			team = game_engine->get_player_score(iterator.datum_index, _get_score_team);
			if (individual != players[slot].individual || team != players[slot].team)
			{
				ae_dev_event("score", "\"player\":%d,\"individual\":%ld,\"team\":%ld", (int)slot, individual, team);
				players[slot].individual = individual;
				players[slot].team = team;
			}
		}
	}
	for (slot = 0; slot < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; slot++)
	{
		if (!present[slot])
			players[slot].player_index = NONE;
	}
}

static void objects_watch_kind(unsigned long mask, boolean vehicle)
{
	struct object_iterator iterator;

	object_iterator_new(&iterator, mask, 0);
	while (object_iterator_next(&iterator))
	{
		struct object_datum *object = object_get(iterator.index);
		boolean dead = vehicle && TEST_FLAG(object->object.damage_flags, _object_dead_bit);
		short index;

		/* (an item held is no longer one on the map: the pickup is the player's weapon event) */
		if (!vehicle && object->object.parent_object_index != NONE)
			continue;
		for (index = 0; index < tracked_count; index++)
		{
			if (tracked[index].object_index == iterator.index)
				break;
		}
		if (index == tracked_count)
		{
			char name[160];

			if (tracked_count == TRACKED)
				continue;
			tracked[index].object_index = iterator.index;
			tracked[index].definition_index = object->definition_index;
			tracked[index].dead = FALSE;
			tracked[index].vehicle = vehicle;
			tracked_count++;
			tag_text(object->definition_index, name, sizeof name);
			ae_dev_event("object_new", "\"kind\":\"%s\",\"tag\":\"%s\",\"id\":%ld,\"x\":%.2f,\"y\":%.2f,\"z\":%.2f",
				vehicle ? "vehicle" : "item", name, iterator.index, object->object.position.x,
				object->object.position.y, object->object.position.z);
		}
		tracked[index].seen = TRUE;
		if (dead && !tracked[index].dead)
		{
			char name[160];

			tag_text(tracked[index].definition_index, name, sizeof name);
			ae_dev_event("vehicle_dead", "\"tag\":\"%s\",\"id\":%ld", name, iterator.index);
			tracked[index].dead = TRUE;
		}
	}
}

static void objects_watch(void)
{
	short index, kept = 0;

	for (index = 0; index < tracked_count; index++)
		tracked[index].seen = FALSE;
	objects_watch_kind(_object_mask_vehicle, TRUE);
	objects_watch_kind(_object_mask_equipment | _object_mask_weapon, FALSE);
	for (index = 0; index < tracked_count; index++)
	{
		if (!tracked[index].seen)
		{
			char name[160];

			tag_text(tracked[index].definition_index, name, sizeof name);
			ae_dev_event("object_gone", "\"kind\":\"%s\",\"tag\":\"%s\",\"id\":%ld",
				tracked[index].vehicle ? "vehicle" : "item", name, tracked[index].object_index);
			continue;
		}
		tracked[kept++] = tracked[index];
	}
	tracked_count = kept;
}

void ae_dev_events_tick(void)
{
	boolean running;

	if (!ae_dev_events_on())
		return;
	ticked = TRUE;
	running = game_engine_running() && !game_engine_game_over();
	/* (a new game: its time starts again) */
	if (game_was_running && game_time_get() < last_game_time)
		game_was_running = FALSE;
	if (running && !game_was_running)
	{
		char map[160];
		char const *name = main_get_map_name();

		ae_dev_event_text(map, sizeof map, name ? name : "");
		ae_dev_event("game_start", "\"map\":\"%s\",\"engine\":\"%s\"", map,
			game_engine && game_engine->name ? game_engine->name : "");
		players_forget();
		tracked_count = 0;
	}
	if (!running && game_was_running)
		ae_dev_event("game_over", "");
	game_was_running = running;
	last_game_time = game_time_get();
	if (!running)
		return;
	players_watch();
	objects_watch();
}
