/*
AE_LAYOUT.C

AE map layout files applied to the loaded scenario (ae_layout.h). The file's reading and checks are pure
(ae_layout_rules.c); this half finds the file, checks it was made for this map file, and changes the scenario's
netgame items and placed vehicles in memory.
*/

#include "cseries/cseries.h"
#include "cseries/cseries_windows.h"
#include "cache/cache_files.h"
#include "game/game_engine.h"
#include "items/equipment_definitions.h"
#include "items/item_definitions.h"
#include "items/weapon_definitions.h"
#include "objects/object_types.h"
#include "physics/collision_bsp.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "tag_files/tag_files.h"
#include "tag_files/tag_groups.h"
#include "units/unit_definitions.h"
#include "units/vehicle_definitions.h"
#include "halo_map_families.h"
#include "../src/map_file_hash.h"

#include "ae_layout.h"
#include "ae_layout_rules.h"
#include "ae_map_folders.h"

#include <xtl.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void platform_log(char const *format, ...);

#define item_collection_definition_get(index) \
	((struct item_collection_definition *)tag_get(ITEM_COLLECTION_DEFINITION_TAG, (index)))

/* a vehicle definition's head: the unit's, then the vehicle's flags and type (vehicles.c's struct vehicle_definition,
private there) */
struct ae_vehicle_definition_head
{
	struct unit_definition unit;
	unsigned long flags;
	short vehicle_type;
};
#define AE_VEHICLE_TYPE_TURRET 6 /* vehicles.c's _vehicle_type_turret */
#define AE_EQUIPMENT_CREATED_AT_REST_BIT 0 /* game_engine.c's _equipment_created_at_rest_bit */
/* an entry a layout line faced (its facing): game_engine.c faces an Xbox map's item by it too, where the Xbox faced
items only one way (an unused bit of the entry's flags: the game reads the first alone) */
#define AE_EQUIPMENT_LAYOUT_FACING_BIT 30
/* how far above the floor a "drop" item is made: a short fall that settles it as a dropped weapon lies */
#define AE_LAYOUT_DROP_HEIGHT 0.25f
#define AE_GAME_ENGINE_ALL 12 /* game_engine.c's _game_engine_all: an entry's game type for every gametype */

#define PATH_SIZE 260
#define DEGREES_TO_RADIANS (3.14159265358979f / 180.0f)

/* this load's state: the scenario's own item list (to build from, and to tell an applied block by), ours */
static struct
{
	struct scenario const *scenario;
	unsigned int original_address;
	long original_count;
	struct scenario_netgame_equipment *items;
	long item_count;
	/* the layout's item names by the new list's index (name item <n>: n is the map's) */
	short name_count;
	struct { long item; char label[AE_LAYOUT_LABEL_SIZE]; } names[AE_LAYOUT_MAXIMUM_NAMES];
	/* the player spawns' and netgame flags' lists, as the items' (queue 141) */
	unsigned int original_spawn_address, original_flag_address;
	long original_spawn_count, original_flag_count;
	struct player_starting_location *spawns;
	long spawn_count;
	struct scenario_netgame_flag *flags;
	long flag_count;
} applied;

static struct ae_layout layout;
/* whether layout is this map's, applied (its bot lines are read by ae_bots_travel.c) */
static boolean layout_applied;

/* the map's file name and path as the game opened it; FALSE when it has none */
static boolean map_file(char *file, long file_size, char *path, long path_size)
{
	char const *name = cache_file_loaded_map_name();
	short family;

	file[0] = path[0] = 0;
	if (!name || !name[0])
		return FALSE;
	family = map_family_parse(name, file, file_size);
	if (!file[0])
		return FALSE;
	if (family == _map_family_xbox)
	{
		cache_files_map_file_path(file, path, path_size);
		return (boolean)(path[0] != 0);
	}
	return map_family_find(family, file, path, path_size);
}

/* <the map's folder>\layouts\<file>.txt */
static boolean layout_path(char const *map_path, char const *file, char *path, long size)
{
	char const *slash = strrchr(map_path, '\\');
	char const *other = strrchr(map_path, '/');
	long folder;

	if (other > slash)
		slash = other;
	folder = slash ? (long)(slash - map_path) + 1 : 0;
	return (boolean)(snprintf(path, (size_t)size, "%.*slayouts\\%s.txt", (int)folder, map_path, file) < size);
}

/* the file's text into a new buffer (NUL-terminated), or NULL: none, or too big */
static char *read_file(char const *path, long *length)
{
	HANDLE file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	unsigned long size, size_high = 0, read = 0;
	char *text = NULL;

	if (file == INVALID_HANDLE_VALUE)
		return NULL;
	size = GetFileSize(file, &size_high);
	if (size != INVALID_FILE_SIZE && !size_high && size <= AE_LAYOUT_MAXIMUM_FILE_SIZE)
	{
		text = (char *)malloc(size + 1);
		if (text && (!ReadFile(file, text, size, &read, NULL) || read != size))
		{
			free(text);
			text = NULL;
		}
	}
	else
	{
		platform_log("layout: %s is bigger than 64 KB (not applied)", path);
	}
	CloseHandle(file);
	if (text)
	{
		text[size] = 0;
		*length = (long)size;
	}
	return text;
}

/* what an item collection can spawn, as an alias: whether any of its items is that */
static boolean collection_is(long collection_index, short alias)
{
	struct item_collection_definition const *collection;
	struct item_permutation_definition const *permutations;
	long index;

	if (collection_index == NONE)
		return FALSE;
	collection = item_collection_definition_get(collection_index);
	permutations = (struct item_permutation_definition const *)xbox_pointer(collection->permutations.address);
	for (index = 0; index < collection->permutations.count; index++)
	{
		long definition = permutations[index].item.index;

		if (definition == NONE || permutations[index].weight <= 0.0f)
			continue;
		switch (tag_get_group_tag(definition))
		{
		case WEAPON_DEFINITION_TAG:
			if ((alias == _ae_layout_rockets && game_engine_weapon_is_rocket_launcher(definition)) ||
				(alias == _ae_layout_sniper && game_engine_weapon_is_sniper_rifle(definition)) ||
				(alias == _ae_layout_shotgun && game_engine_weapon_is_shotgun(definition)) ||
				(alias == _ae_layout_pistol && game_engine_weapon_is_pistol(definition)))
			{
				return TRUE;
			}
			break;
		case EQUIPMENT_DEFINITION_TAG:
			if ((alias == _ae_layout_overshield &&
				equipment_definition_get(definition)->equipment.powerup_type == _equipment_powerup_overshield) ||
				(alias == _ae_layout_camo &&
				equipment_definition_get(definition)->equipment.powerup_type == _equipment_powerup_active_camouflage))
			{
				return TRUE;
			}
			break;
		}
	}
	return FALSE;
}

/* the map's own entry a layout item starts from (its collection, game types and flags), or NULL: the map lacks it */
static struct scenario_netgame_equipment const *source_entry(struct scenario_netgame_equipment const *original,
	long original_count, struct ae_layout_item const *item, long *tag_collection)
{
	long index;

	*tag_collection = NONE;
	switch (item->what)
	{
	case _ae_layout_like:
		return item->like < original_count ? &original[item->like] : NULL;
	case _ae_layout_alias:
		for (index = 0; index < original_count; index++)
			if (collection_is(original[index].item_collection.index, item->alias))
				return &original[index];
		return NULL;
	default:
		*tag_collection = tag_loaded(ITEM_COLLECTION_DEFINITION_TAG, item->tag);
		return *tag_collection == NONE ? NULL : (original_count ? &original[0] : NULL);
	}
}

/* the engine's object type of a layout's */
static short object_type_of(short type)
{
	static short const types[NUMBER_OF_AE_LAYOUT_OBJECT_TYPES] =
	{
		_object_type_scenery, _object_type_machine, _object_type_control, _object_type_light_fixture,
		_object_type_sound_scenery, _object_type_biped, _object_type_weapon, _object_type_equipment
	};

	return types[type];
}

/* the map's own items and vehicles, by the numbers a layout names them with (written when a layout file exists, so
that one can be written from the log) */
static void log_map(struct scenario *scenario)
{
	struct scenario_netgame_equipment const *items =
		(struct scenario_netgame_equipment const *)xbox_pointer(applied.original_address);
	long element_size = 0;
	struct tag_block *placements = scenario_get_object_type_scenario_datums(scenario, _object_type_vehicle,
		&element_size);
	long index;
	short type;

	for (index = 0; index < applied.original_count; index++)
	{
		short alias;

		for (alias = 0; alias < NUMBER_OF_AE_LAYOUT_ALIASES; alias++)
			if (collection_is(items[index].item_collection.index, alias))
				break;
		platform_log("layout: map item %ld: %s at %.2f %.2f %.2f facing %.0f respawn %ds game types %d %d %d %d", index,
			alias < NUMBER_OF_AE_LAYOUT_ALIASES ? ae_layout_alias_name(alias) : "other", items[index].position.x,
			items[index].position.y, items[index].position.z, items[index].facing / DEGREES_TO_RADIANS,
			(int)items[index].spawn_time, items[index].game_type[0], items[index].game_type[1], items[index].game_type[2],
			items[index].game_type[3]);
	}
	for (index = 0; placements && element_size > 0 && index < placements->count; index++)
	{
		struct scenario_object_datum const *placement =
			(struct scenario_object_datum const *)tag_block_get_element_with_size(placements, index, element_size);
		short entry = placement->palette_entry_index;
		long definition = entry >= 0 && entry < scenario->vehicle_palette.count ?
			TAG_BLOCK_GET_ELEMENT(&scenario->vehicle_palette, entry, struct scenario_object_palette_entry)->reference.index :
			NONE;

		platform_log("layout: map vehicle %ld: palette %d type %d at %.2f %.2f %.2f", index, entry,
			definition == NONE ? -1 : (int)((struct ae_vehicle_definition_head const *)
				tag_get(VEHICLE_DEFINITION_TAG, definition))->vehicle_type,
			placement->position.x, placement->position.y, placement->position.z);
	}
	/* (the other placed objects, so that a remove scenery / remove object line can be written from the log: each
	type's palette names, then each placement's entry and spot) */
	for (type = 0; type < NUMBER_OF_AE_LAYOUT_OBJECT_TYPES; type++)
	{
		struct tag_block *palette = scenario_get_object_type_scenario_palette(scenario, object_type_of(type));

		for (index = 0; index < palette->count; index++)
		{
			long definition = TAG_BLOCK_GET_ELEMENT(palette, index, struct scenario_object_palette_entry)->reference.index;

			platform_log("layout: map %s palette %ld: %s", ae_layout_object_type_name(type), index,
				definition == NONE ? "(none)" : tag_get_name(definition));
		}
		element_size = 0;
		placements = scenario_get_object_type_scenario_datums(scenario, object_type_of(type), &element_size);
		for (index = 0; placements && element_size > 0 && index < placements->count; index++)
		{
			struct scenario_object_datum const *placement =
				(struct scenario_object_datum const *)tag_block_get_element_with_size(placements, index, element_size);

			platform_log("layout: map %s %ld: palette %d at %.2f %.2f %.2f", ae_layout_object_type_name(type), index,
				(int)placement->palette_entry_index, placement->position.x, placement->position.y, placement->position.z);
		}
	}
}

#define FLOOR_LOOK_ABOVE 0.5f     /* world units above the spot the look down starts */
#define FLOOR_LOOK_DOWN 60.0f     /* how far it looks */
#define FLOOR_CLEARANCE 0.05f     /* the item this far above the floor found */

/* the floor under a point (the level's collision, looked straight down from just above it): the point moved onto it */
static boolean floor_under(real_point3d *point)
{
	struct collision_bsp *bsp = global_collision_bsp_get();
	struct collision_bsp_test_vector_result result;
	real_point3d origin = *point;
	real_vector3d down;

	if (!bsp)
		return FALSE;
	origin.z += FLOOR_LOOK_ABOVE;
	down.i = 0.0f;
	down.j = 0.0f;
	down.k = -FLOOR_LOOK_DOWN;
	if (!collision_bsp_test_vector(1, bsp, 0, NULL, &origin, &down, 1.0f, &result))
		return FALSE;
	point->z = origin.z + down.k * result.t + FLOOR_CLEARANCE;
	return TRUE;
}

/* a name item line for the map's item `original`, now the list's `now` */
static void name_kept(long original, long now)
{
	short name;

	for (name = 0; name < layout.name_count && applied.name_count < AE_LAYOUT_MAXIMUM_NAMES; name++)
		if (layout.names[name].item == original)
		{
			applied.names[applied.name_count].item = now;
			strcpy(applied.names[applied.name_count].label, layout.names[name].label);
			applied.name_count++;
		}
}

boolean ae_layout_equipment_faces(long equipment_flags)
{
	return TEST_FLAG(equipment_flags, AE_EQUIPMENT_LAYOUT_FACING_BIT);
}

boolean ae_layout_item_label(long equipment_index, char *label, long size)
{
	short name;

	for (name = 0; name < applied.name_count; name++)
		if (applied.names[name].item == equipment_index && size > 0)
		{
			snprintf(label, (size_t)size, "%s", applied.names[name].label);
			return TRUE;
		}
	return FALSE;
}

static void apply_items(struct scenario *scenario)
{
	struct scenario_netgame_equipment const *original =
		(struct scenario_netgame_equipment const *)xbox_pointer(applied.original_address);
	long original_count = applied.original_count;
	long capacity = original_count + layout.item_count;
	long index, count = 0, added = 0, skipped = 0;
	struct scenario_netgame_equipment *items;

	if (!layout.clear_items && !layout.removed_count && !layout.item_count)
	{
		/* (names alone: the map's own list, its own numbers) */
		for (index = 0; index < original_count; index++)
			name_kept(index, index);
		return;
	}
	items = (struct scenario_netgame_equipment *)malloc((size_t)(capacity ? capacity : 1) * sizeof(*items));
	if (!items)
	{
		platform_log("layout: no memory for %ld items (not applied)", capacity);
		return;
	}
	memset(items, 0, (size_t)(capacity ? capacity : 1) * sizeof(*items));
	if (!layout.clear_items)
	{
		for (index = 0; index < original_count; index++)
		{
			short removed;

			for (removed = 0; removed < layout.removed_count; removed++)
				if (layout.removed[removed] == index)
					break;
			if (removed < layout.removed_count)
				continue;
			name_kept(index, count);
			items[count++] = original[index];
		}
	}
	for (index = 0; index < layout.item_count; index++)
	{
		struct ae_layout_item const *item = &layout.items[index];
		long tag_collection;
		struct scenario_netgame_equipment const *source = source_entry(original, original_count, item, &tag_collection);
		struct scenario_netgame_equipment *entry;

		if (!source)
		{
			/* (a weapon the map lacks: that one item is skipped, owner 2026-10-16) */
			platform_log("layout: line %d: the map has no %s%s%s (skipped)", item->line,
				item->what == _ae_layout_alias ? ae_layout_alias_name(item->alias) :
				item->what == _ae_layout_tag ? item->tag : "item", item->what == _ae_layout_like ? " " : "",
				item->what == _ae_layout_like ? "of that number" : "");
			skipped++;
			continue;
		}
		entry = &items[count++];
		*entry = *source;
		if (tag_collection != NONE)
			entry->item_collection.index = tag_collection;
		entry->run_time_spawned_item_index = NONE;
		entry->position.x = item->x;
		entry->position.y = item->y;
		entry->position.z = item->z;
		/* (on the floor under its spot, so a layout's height need not be exact: owner 2026-10-16, an item falling from
		its spot with its waypoint looked wrong; no floor found below: it falls there instead) */
		/* (float: at its own height, at rest: a powerup the floor under it would put under water, owner 2026-10-18) */
		if (item->float_here)
			SET_FLAG(entry->flags, AE_EQUIPMENT_CREATED_AT_REST_BIT, TRUE);
		else if (floor_under(&entry->position))
			SET_FLAG(entry->flags, AE_EQUIPMENT_CREATED_AT_REST_BIT, TRUE);
		else
			SET_FLAG(entry->flags, AE_EQUIPMENT_CREATED_AT_REST_BIT, FALSE);
		/* (drop: a little above the floor and not at rest, so it falls onto its side, as Halo 2's lie: owner
		2026-10-18, Coagulation's shotguns) */
		if (item->drop)
		{
			entry->position.z += AE_LAYOUT_DROP_HEIGHT;
			SET_FLAG(entry->flags, AE_EQUIPMENT_CREATED_AT_REST_BIT, FALSE);
		}
		SET_FLAG(entry->flags, AE_EQUIPMENT_LAYOUT_FACING_BIT, item->has_facing);
		if (item->has_facing)
			entry->facing = item->facing_degrees * DEGREES_TO_RADIANS;
		if (item->respawn_seconds > 0)
			entry->spawn_time = item->respawn_seconds;
		if (item->games_all)
		{
			entry->game_type[0] = AE_GAME_ENGINE_ALL;
			entry->game_type[1] = entry->game_type[2] = entry->game_type[3] = 0;
		}
		if (item->team != _ae_layout_team_unset)
			entry->team_index = (short)(item->team == _ae_layout_team_neutral ? NONE : item->team);
		platform_log("layout: line %d: item %ld at %.2f %.2f %.2f%s", item->line, count - 1, entry->position.x,
			entry->position.y, entry->position.z, item->drop ? " (dropped: it falls onto the floor)" :
			item->float_here ? " (at its own height)" :
			TEST_FLAG(entry->flags, AE_EQUIPMENT_CREATED_AT_REST_BIT) ? " (on the floor)" : " (no floor below: it falls)");
		added++;
	}
	/* (the game's free is debug_free, which takes no NULL) */
	if (applied.items)
		free(applied.items);
	applied.items = items;
	applied.item_count = count;
	scenario->netgame_equipment.address = XBOX_ADDRESS(items);
	scenario->netgame_equipment.count = count;
	platform_log("layout: items %ld of the map's %ld kept, %ld added, %ld skipped: %ld in all",
		count - added, original_count, added, skipped, count);
}

/* whether the map's entry index is one of the removed */
static boolean removed(long const *list, short count, long index)
{
	short at;

	for (at = 0; at < count; at++)
		if (list[at] == index)
			return TRUE;
	return FALSE;
}

static void apply_spawns(struct scenario *scenario)
{
	struct player_starting_location const *original =
		(struct player_starting_location const *)xbox_pointer(applied.original_spawn_address);
	long original_count = applied.original_spawn_count;
	long capacity = original_count + layout.spawn_count, index, count = 0;
	struct player_starting_location *spawns;

	if (!layout.clear_spawns && !layout.removed_spawn_count && !layout.spawn_count)
		return;
	spawns = (struct player_starting_location *)malloc((size_t)(capacity ? capacity : 1) * sizeof(*spawns));
	if (!spawns)
	{
		platform_log("layout: no memory for %ld spawns (not applied)", capacity);
		return;
	}
	memset(spawns, 0, (size_t)(capacity ? capacity : 1) * sizeof(*spawns));
	if (!layout.clear_spawns)
		for (index = 0; index < original_count; index++)
			if (!removed(layout.removed_spawns, layout.removed_spawn_count, index))
				spawns[count++] = original[index];
	for (index = 0; index < layout.spawn_count; index++)
	{
		struct ae_layout_spawn const *spawn = &layout.spawns[index];
		struct player_starting_location *entry = &spawns[count++];
		short game;

		/* (the map's first spawn's BSP, else the first BSP: a multiplayer map has one) */
		entry->structure_bsp_reference_index = original_count > 0 ? original[0].structure_bsp_reference_index : 0;
		entry->position.x = spawn->x;
		entry->position.y = spawn->y;
		entry->position.z = spawn->z;
		entry->facing = spawn->has_facing ? spawn->facing_degrees * DEGREES_TO_RADIANS : 0.0f;
		entry->team_index = spawn->team;
		for (game = 0; game < 4; game++)
			entry->game_types[game] = spawn->games[game];
		platform_log("layout: line %d: spawn %ld at %.2f %.2f %.2f team %d", spawn->line, count - 1, entry->position.x,
			entry->position.y, entry->position.z, entry->team_index);
	}
	if (applied.spawns)
		free(applied.spawns);
	applied.spawns = spawns;
	applied.spawn_count = count;
	scenario->players.address = XBOX_ADDRESS(spawns);
	scenario->players.count = count;
	platform_log("layout: spawns %ld of the map's %ld kept, %d added: %ld in all", count - layout.spawn_count,
		original_count, layout.spawn_count, count);
}

static void apply_flags(struct scenario *scenario)
{
	struct scenario_netgame_flag const *original =
		(struct scenario_netgame_flag const *)xbox_pointer(applied.original_flag_address);
	long original_count = applied.original_flag_count;
	long capacity = original_count + layout.flag_count, index, count = 0;
	struct scenario_netgame_flag *flags;

	if (!layout.clear_flags && !layout.removed_flag_count && !layout.flag_count)
		return;
	flags = (struct scenario_netgame_flag *)malloc((size_t)(capacity ? capacity : 1) * sizeof(*flags));
	if (!flags)
	{
		platform_log("layout: no memory for %ld flags (not applied)", capacity);
		return;
	}
	memset(flags, 0, (size_t)(capacity ? capacity : 1) * sizeof(*flags));
	if (!layout.clear_flags)
		for (index = 0; index < original_count; index++)
			if (!removed(layout.removed_flags, layout.removed_flag_count, index))
				flags[count++] = original[index];
	for (index = 0; index < layout.flag_count; index++)
	{
		struct ae_layout_flag const *flag = &layout.flags[index];
		struct scenario_netgame_flag *entry = &flags[count++];

		entry->position.x = flag->x;
		entry->position.y = flag->y;
		entry->position.z = flag->z;
		entry->facing = flag->has_facing ? flag->facing_degrees * DEGREES_TO_RADIANS : 0.0f;
		entry->type = flag->type;
		entry->team_index = flag->team;
		platform_log("layout: line %d: flag %ld %s at %.2f %.2f %.2f team %d", flag->line, count - 1,
			ae_layout_flag_type_name(flag->type), entry->position.x, entry->position.y, entry->position.z, entry->team_index);
	}
	if (applied.flags)
		free(applied.flags);
	applied.flags = flags;
	applied.flag_count = count;
	scenario->netgame_flags.address = XBOX_ADDRESS(flags);
	scenario->netgame_flags.count = count;
	platform_log("layout: flags %ld of the map's %ld kept, %d added: %ld in all", count - layout.flag_count,
		original_count, layout.flag_count, count);
}

static void apply_vehicles(struct scenario *scenario)
{
	long element_size = 0;
	struct tag_block *placements = scenario_get_object_type_scenario_datums(scenario, _object_type_vehicle,
		&element_size);
	struct tag_block *palette = &scenario->vehicle_palette;
	long index, removed = 0;
	short rule;

	if (!layout.vehicle_rule_count || !placements || element_size <= 0)
		return;
	for (index = 0; index < placements->count; index++)
	{
		struct scenario_object_datum *placement =
			(struct scenario_object_datum *)tag_block_get_element_with_size(placements, index, element_size);
		short entry = placement->palette_entry_index;
		long definition = NONE;

		if (entry < 0 || entry >= palette->count)
			continue;
		definition = TAG_BLOCK_GET_ELEMENT(palette, entry, struct scenario_object_palette_entry)->reference.index;
		for (rule = 0; rule < layout.vehicle_rule_count; rule++)
		{
			struct ae_layout_vehicle_rule const *wanted = &layout.vehicle_rules[rule];
			boolean match;

			if (wanted->turret)
				match = (boolean)(definition != NONE && ((struct ae_vehicle_definition_head const *)
					tag_get(VEHICLE_DEFINITION_TAG, definition))->vehicle_type == AE_VEHICLE_TYPE_TURRET);
			else
				match = (boolean)(wanted->palette_index == entry);
			if (match && wanted->near)
			{
				real dx = placement->position.x - wanted->x, dy = placement->position.y - wanted->y;
				real dz = placement->position.z - wanted->z;

				match = (boolean)(dx * dx + dy * dy + dz * dz <= wanted->radius * wanted->radius);
			}
			if (match)
				break;
		}
		if (rule < layout.vehicle_rule_count)
		{
			/* (a placement whose palette entry is NONE is not placed: objects.c object_new_from_scenario) */
			platform_log("layout: line %d: vehicle %ld (palette %d) at %.1f %.1f %.1f removed",
				layout.vehicle_rules[rule].line, index, entry, placement->position.x, placement->position.y,
				placement->position.z);
			placement->palette_entry_index = NONE;
			removed++;
		}
	}
	platform_log("layout: %ld vehicle%s removed", removed, removed == 1 ? "" : "s");
}

/* whether name has word in it, letters of either case alike */
static boolean name_has(char const *name, char const *word)
{
	long length = (long)strlen(word), at;

	if (!name || !length)
		return FALSE;
	for (at = 0; name[at]; at++)
	{
		long same = 0;

		while (same < length && name[at + same] && tolower((unsigned char)name[at + same]) ==
			tolower((unsigned char)word[same]))
		{
			same++;
		}
		if (same == length)
			return TRUE;
	}
	return FALSE;
}

/* remove scenery / remove object: the placements dropped as a vehicle's are. These objects are made on each machine
from its own copy of the scenario (nothing of them goes over the network), so this runs on every machine that has the
layout; one without the file keeps them (and their collision) */
static void apply_objects(struct scenario *scenario)
{
	long removed = 0;
	short type;

	if (!layout.object_rule_count)
		return;
	for (type = 0; type < NUMBER_OF_AE_LAYOUT_OBJECT_TYPES; type++)
	{
		long element_size = 0, index;
		struct tag_block *placements = scenario_get_object_type_scenario_datums(scenario, object_type_of(type),
			&element_size);
		struct tag_block *palette = scenario_get_object_type_scenario_palette(scenario, object_type_of(type));
		short rule;

		if (!placements || element_size <= 0)
			continue;
		for (index = 0; index < placements->count; index++)
		{
			struct scenario_object_datum *placement =
				(struct scenario_object_datum *)tag_block_get_element_with_size(placements, index, element_size);
			short entry = placement->palette_entry_index;
			long definition = NONE;

			if (entry < 0 || entry >= palette->count)
				continue;
			definition = TAG_BLOCK_GET_ELEMENT(palette, entry, struct scenario_object_palette_entry)->reference.index;
			for (rule = 0; rule < layout.object_rule_count; rule++)
			{
				struct ae_layout_object_rule const *wanted = &layout.object_rules[rule];
				boolean match;

				if (wanted->type != type)
					continue;
				match = wanted->by_name ? (boolean)(definition != NONE && name_has(tag_get_name(definition), wanted->name)) :
					(boolean)(wanted->palette_index == entry);
				if (match && wanted->near)
				{
					real dx = placement->position.x - wanted->x, dy = placement->position.y - wanted->y;
					real dz = placement->position.z - wanted->z;

					match = (boolean)(dx * dx + dy * dy + dz * dz <= wanted->radius * wanted->radius);
				}
				if (match)
					break;
			}
			if (rule < layout.object_rule_count)
			{
				platform_log("layout: line %d: %s %ld (palette %d) at %.1f %.1f %.1f removed", layout.object_rules[rule].line,
					ae_layout_object_type_name(type), index, entry, placement->position.x, placement->position.y,
					placement->position.z);
				placement->palette_entry_index = NONE;
				removed++;
			}
		}
	}
	platform_log("layout: %ld scenery and other object%s removed", removed, removed == 1 ? "" : "s");
}

/* whether the size bytes at address lie in a block of count elements of element_size at block */
static int memory_in(void const *block, long count, unsigned long element_size, unsigned long address, long size)
{
	unsigned long start, end;

	if (!block || size <= 0)
		return 0;
	start = (unsigned long)XBOX_ADDRESS(block);
	end = start + (unsigned long)count * element_size;
	return address >= start && address < end && (unsigned long)size <= end - address;
}

int ae_layout_memory(unsigned long address, long size)
{
	return memory_in(applied.items, applied.item_count, sizeof(*applied.items), address, size) ||
		memory_in(applied.spawns, applied.spawn_count, sizeof(*applied.spawns), address, size) ||
		memory_in(applied.flags, applied.flag_count, sizeof(*applied.flags), address, size);
}

struct ae_layout const *ae_layout_bot_hints(void)
{
	return layout_applied && (layout.bot_jump_count || layout.bot_avoid_count) ? &layout : NULL;
}

boolean ae_layout_map_header(char *text, long size)
{
	char file[64], map_path[PATH_SIZE];
	unsigned int hash = 0;

	if (size <= 0)
		return FALSE;
	text[0] = 0;
	if (!map_file(file, sizeof(file), map_path, sizeof(map_path)) || !map_file_hash_prefix(map_path, &hash))
		return FALSE;
	snprintf(text, (size_t)size, "layout %d %s %08x", AE_LAYOUT_VERSION, file, hash);
	return TRUE;
}

void ae_layout_apply(void)
{
	struct scenario *scenario = global_scenario_get();
	char file[64], map_path[PATH_SIZE], root_path[PATH_SIZE], path[PATH_SIZE + 32], error[160];
	unsigned int hash = 0;
	char *text;
	long length = 0;
	int bad;

	if (!scenario)
		return;
	/* (this load of the map has it already: a replay without a reload) */
	if (applied.scenario == scenario &&
		((applied.items && scenario->netgame_equipment.address == XBOX_ADDRESS(applied.items)) ||
		(applied.spawns && scenario->players.address == XBOX_ADDRESS(applied.spawns)) ||
		(applied.flags && scenario->netgame_flags.address == XBOX_ADDRESS(applied.flags))))
	{
		return;
	}
	applied.scenario = scenario;
	applied.name_count = 0;
	layout_applied = FALSE;
	applied.original_address = scenario->netgame_equipment.address;
	applied.original_count = scenario->netgame_equipment.count;
	applied.original_spawn_address = scenario->players.address;
	applied.original_spawn_count = scenario->players.count;
	applied.original_flag_address = scenario->netgame_flags.address;
	applied.original_flag_count = scenario->netgame_flags.count;
	if (applied.items)
	{
		/* (the last map's list: the tag data it was hung on is gone with that map) */
		free(applied.items);
		applied.items = NULL;
		applied.item_count = 0;
	}
	if (applied.spawns)
	{
		free(applied.spawns);
		applied.spawns = NULL;
		applied.spawn_count = 0;
	}
	if (applied.flags)
	{
		free(applied.flags);
		applied.flags = NULL;
		applied.flag_count = 0;
	}
	if (!map_file(file, sizeof(file), map_path, sizeof(map_path)) ||
		!layout_path(map_path, file, path, sizeof(path)))
	{
		return;
	}
	text = read_file(path, &length);
	/* (a map in a folder of the maps folder: its folder's layouts, else the maps folder's, ae_map_folders.c) */
	if (!text && ae_map_folders_root_path(map_path, root_path, sizeof(root_path)) &&
		layout_path(root_path, file, path, sizeof(path)))
	{
		text = read_file(path, &length);
	}
	if (!text)
		return;
	log_map(scenario);
	bad = ae_layout_parse(text, length, &layout, error, sizeof(error));
	free(text);
	if (bad)
	{
		platform_log("layout: %s line %d: %s (not applied)", path, bad, error);
		return;
	}
	if (!map_file_hash_prefix(map_path, &hash) || !ae_layout_for_map(&layout, file, (unsigned long)hash))
	{
		platform_log("layout: %s was made for %s %08lx, this map is %s %08x (not applied)", path, layout.map_file,
			layout.map_hash, file, hash);
		return;
	}
	platform_log("layout: %s (%d items, %d removed, %sclear, %d vehicle lines, %d names, %d bot jumps, %d bot avoids, "
		"%d spawns, %d flags, %d object lines)", path, layout.item_count, layout.removed_count, layout.clear_items ? "" : "no ",
		layout.vehicle_rule_count, layout.name_count, layout.bot_jump_count, layout.bot_avoid_count, layout.spawn_count,
		layout.flag_count, layout.object_rule_count);
	layout_applied = TRUE;
	apply_items(scenario);
	apply_vehicles(scenario);
	apply_objects(scenario);
	apply_spawns(scenario);
	apply_flags(scenario);
}
