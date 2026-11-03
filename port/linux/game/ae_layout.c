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
#include "game/game_globals.h"
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
#include "halo_map_packs.h"
#include "../src/map_file_hash.h"

#include "ae_bomb_snd_rules.h"
#include "ae_layout.h"
#include "ae_layout_rules.h"
#include "ae_layout_share_rules.h"
#include "ae_map_folders.h"
#include "ae_map_look_fixes.h"
#include "ae_map_share.h"
#include "ae_weapon_slots.h"

#include <xtl.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void platform_log(char const *format, ...);
int config_boolean(char const *name);

extern struct game_variant game_variant_global; /* (game.c's: the next game's, set before its map loads) */

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
	/* the layout's pack weapons by the new list's index (item smg at ...: ae_weapon_slots.h) */
	short pack_item_count;
	struct { long item; short slot; } pack_items[AE_LAYOUT_MAXIMUM_ITEMS];
	/* the player spawns' and netgame flags' lists, as the items' (queue 141) */
	unsigned int original_spawn_address, original_flag_address;
	long original_spawn_count, original_flag_count;
	struct player_starting_location *spawns;
	long spawn_count;
	struct scenario_netgame_flag *flags;
	long flag_count;
	/* the vehicle placements and palette with a layout's vehicle lines (0.3.5): ours in place of the map's; the
	placements past original_vehicle_count are the lines', each with its team and game types */
	byte *vehicles;
	long vehicle_count, vehicle_element_size, original_vehicle_count;
	struct scenario_object_palette_entry *vehicle_palette;
	long vehicle_palette_count;
	struct { short team; short games[4]; } added_vehicles[AE_LAYOUT_MAXIMUM_VEHICLES];
} applied;

static struct ae_layout layout;
/* whether layout is this map's, applied (its bot lines are read by ae_bots_travel.c) */
static boolean layout_applied;

/* a level's map file name and path as the game opens it; FALSE when it has none */
static boolean map_file_of(char const *name, char *file, long file_size, char *path, long path_size)
{
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
	/* (a pack's map: its file in maps_<pack>, which the file name alone doesn't name) */
	if (family == _map_family_pack)
	{
		char pack[MAP_PACK_ID_LENGTH + 1];

		return map_family_pack_id(name, pack, sizeof(pack)) && map_pack_file_path(pack, file, path, path_size);
	}
	return map_family_find(family, file, path, path_size);
}

/* the loaded map's file name and path as the game opened it; FALSE when it has none */
static boolean map_file(char *file, long file_size, char *path, long path_size)
{
	return map_file_of(cache_file_loaded_map_name(), file, file_size, path, path_size);
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

/* this machine's own layout file for a map: its text (read_file's), the file's path in path; NULL when it has none,
and path the map's folder's file, where one would go */
static char *own_layout(char const *map_path, char const *file, char *path, long size, long *length)
{
	char root_path[PATH_SIZE], other[PATH_SIZE + 32];
	char *text;

	path[0] = 0;
	if (!layout_path(map_path, file, path, size))
		return NULL;
	text = read_file(path, length);
	/* (a map in a folder of the maps folder: its folder's layouts, else the maps folder's, ae_map_folders.c) */
	if (!text && ae_map_folders_root_path(map_path, root_path, sizeof(root_path)) &&
		layout_path(root_path, file, other, sizeof(other)))
	{
		text = read_file(other, length);
		if (text)
			snprintf(path, (size_t)size, "%s", other);
	}
	return text;
}

static boolean write_file(char const *path, char const *text, long length)
{
	HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	unsigned long written = 0;
	boolean done;

	if (file == INVALID_HANDLE_VALUE)
		return FALSE;
	done = (boolean)(WriteFile(file, text, (unsigned long)length, &written, NULL) && written == (unsigned long)length);
	CloseHandle(file);
	if (!done)
		DeleteFileA(path);
	return done;
}

/* a host's layout kept on this guest's disk (ae_layout_share_rules.h): written at own_path (its layouts folder made),
or beside it as <map>.host.txt (unless that file is the same already) */
static void keep_host_text(int disk, char const *own_path, char const *text, long length)
{
	char path[PATH_SIZE + 48];

	if (disk == AE_LAYOUT_SHARE_DISK_WRITE)
	{
		char folder[PATH_SIZE + 32];
		char *slash;

		snprintf(folder, sizeof(folder), "%s", own_path);
		slash = strrchr(folder, '\\');
		if (slash)
		{
			*slash = 0;
			CreateDirectoryA(folder, NULL);
		}
		snprintf(path, sizeof(path), "%s", own_path);
	}
	else if (disk == AE_LAYOUT_SHARE_DISK_BESIDE)
	{
		long beside_length = 0;
		char *beside;

		if (!ae_layout_share_beside_path(own_path, path, sizeof(path)))
			return;
		beside = read_file(path, &beside_length);
		if (beside && beside_length == length && !memcmp(beside, text, (size_t)length))
		{
			free(beside);
			return;
		}
		if (beside)
			free(beside);
	}
	else
	{
		return;
	}
	if (write_file(path, text, length))
		platform_log("layout: the host's layout kept in %s (%ld bytes)%s", path, length,
			disk == AE_LAYOUT_SHARE_DISK_BESIDE ? ": this machine's own file differs, and is kept as it is" : "");
	else
		platform_log("layout: the host's layout could not be written to %s", path);
}

/* a host's layout for a map (state, text): what this guest applies (and keeps on disk), given its own file (own, at
own_path, NULL for none) and the map file (file, its hash) */
static int host_choice(int state, char const *host_text, long host_length, char const *own, long own_length,
	char const *own_path, char const *file, unsigned long hash)
{
	static struct ae_layout checking;
	int fits = state == AE_LAYOUT_SHARE_HOST_TEXT &&
		ae_layout_share_fits(host_text, host_length, file, hash, &checking);
	int same = fits && own && own_length == host_length && !memcmp(own, host_text, (size_t)own_length);
	int disk = AE_LAYOUT_SHARE_DISK_NOTHING;
	int choice = ae_layout_share_decide(state, fits, own != NULL, same, ae_map_share_download_may_write(), &disk);

	if (state == AE_LAYOUT_SHARE_HOST_TEXT && !fits)
		platform_log("layout: the host's layout for %s (%ld bytes) is not one for this map file (%s %08lx): not used",
			file, host_length, file, hash);
	keep_host_text(disk, own_path, host_text, host_length);
	return choice;
}

boolean ae_layout_host_text(char const *level_name, char *text, long size, long *length)
{
	static struct ae_layout checking;
	char file[64], map_path[PATH_SIZE], path[PATH_SIZE + 32];
	unsigned int hash = 0;
	char *own;
	long own_length = 0;
	boolean fits;

	*length = 0;
	if (!level_name || !map_file_of(level_name, file, sizeof(file), map_path, sizeof(map_path)))
		return FALSE;
	own = own_layout(map_path, file, path, sizeof(path), &own_length);
	if (!own)
		return FALSE;
	/* (only one this machine applies: it reads, and was made for this very map file) */
	fits = (boolean)(own_length <= size && map_file_hash_prefix(map_path, &hash) &&
		ae_layout_share_fits(own, own_length, file, hash, &checking));
	if (fits)
	{
		memcpy(text, own, (size_t)own_length);
		*length = own_length;
	}
	free(own);
	return fits;
}

void ae_layout_keep_host_text(char const *level_name, char const *text, long length)
{
	char file[64], map_path[PATH_SIZE], path[PATH_SIZE + 32];
	unsigned int hash = 0;
	char *own;
	long own_length = 0;

	if (!level_name || !text || !map_file_of(level_name, file, sizeof(file), map_path, sizeof(map_path)) ||
		!map_file_hash_prefix(map_path, &hash))
	{
		return;
	}
	own = own_layout(map_path, file, path, sizeof(path), &own_length);
	if (path[0])
		host_choice(AE_LAYOUT_SHARE_HOST_TEXT, text, length, own, own_length, path, file, (unsigned long)hash);
	if (own)
		free(own);
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

/* whether an item collection can spawn a stock weapon of ae_weapon_slots.h's (the pistol, the assault rifle) */
static boolean collection_is_stock(long collection_index, short stock)
{
	struct item_collection_definition const *collection;
	struct item_permutation_definition const *permutations;
	long index;

	if (stock == AE_WEAPON_STOCK_PISTOL)
		return collection_is(collection_index, _ae_layout_pistol);
	if (collection_index == NONE || stock != AE_WEAPON_STOCK_ASSAULT_RIFLE)
		return FALSE;
	collection = item_collection_definition_get(collection_index);
	permutations = (struct item_permutation_definition const *)xbox_pointer(collection->permutations.address);
	for (index = 0; index < collection->permutations.count; index++)
	{
		long definition = permutations[index].item.index;

		if (definition != NONE && permutations[index].weight > 0.0f &&
			tag_get_group_tag(definition) == WEAPON_DEFINITION_TAG &&
			game_engine_weapon_is_assault_rifle(definition))
		{
			return TRUE;
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
	case _ae_layout_pack:
		/* (the map's own fallback item: what spawns where the pack is not on the map) */
		for (index = 0; index < original_count; index++)
			if (collection_is_stock(original[index].item_collection.index, ae_weapon_slots_fallback(item->pack_slot)))
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
/* the game this map loads for SEARCH AND DESTROY (ae_bomb_assault.c's ae_bomb_snd_on, from the variant set before the
map loads: the game engine is not running yet): its layout's "snd" lines are read */
static int layout_snd(void)
{
	int snd = game_variant_global.game_engine_index == game_engine_bomb && config_boolean("game.bomb_flag_spawns") &&
		(ae_bomb_snd_flag(game_variant_global.game_engine_variant.bomb.flags) || config_boolean("game.bomb_snd"));

	if (snd)
		platform_log("layout: a SEARCH AND DESTROY game: its snd lines read");
	return snd;
}

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
	/* (the map's own spawns and flags, so that remove spawn / remove flag / spawn / flag lines can be written from the
	log) */
	for (index = 0; index < scenario->players.count; index++)
	{
		struct player_starting_location const *spawn =
			TAG_BLOCK_GET_ELEMENT(&scenario->players, index, struct player_starting_location);

		platform_log("layout: map spawn %ld: team %d at %.2f %.2f %.2f facing %.0f game types %d %d %d %d", index,
			spawn->team_index, spawn->position.x, spawn->position.y, spawn->position.z, spawn->facing / DEGREES_TO_RADIANS,
			spawn->game_types[0], spawn->game_types[1], spawn->game_types[2], spawn->game_types[3]);
	}
	for (index = 0; index < scenario->netgame_flags.count; index++)
	{
		struct scenario_netgame_flag const *flag =
			TAG_BLOCK_GET_ELEMENT(&scenario->netgame_flags, index, struct scenario_netgame_flag);

		platform_log("layout: map flag %ld: %s team %d at %.2f %.2f %.2f", index, ae_layout_flag_type_name(flag->type),
			flag->team_index, flag->position.x, flag->position.y, flag->position.z);
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

short ae_layout_item_pack_slot(long equipment_index)
{
	short index;

	for (index = 0; index < applied.pack_item_count; index++)
		if (applied.pack_items[index].item == equipment_index)
			return applied.pack_items[index].slot;
	return 0;
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

	applied.pack_item_count = 0;
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
				item->what == _ae_layout_pack ? (ae_weapon_slots_fallback(item->pack_slot) == AE_WEAPON_STOCK_PISTOL ?
					"pistol (the pack weapon's fallback)" : "assault rifle (the pack weapon's fallback)") :
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
		/* (drop: a little above the floor and not at rest, so it falls onto its side, as they lie: owner
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
		if (item->what == _ae_layout_pack && applied.pack_item_count < AE_LAYOUT_MAXIMUM_ITEMS)
		{
			applied.pack_items[applied.pack_item_count].item = count - 1;
			applied.pack_items[applied.pack_item_count].slot = item->pack_slot;
			applied.pack_item_count++;
			platform_log("layout: line %d: item %ld is the %s where its pack is on the map, else the map's %s", item->line,
				count - 1, ae_weapon_slots_name(item->pack_slot),
				ae_weapon_slots_fallback(item->pack_slot) == AE_WEAPON_STOCK_PISTOL ? "pistol" : "assault rifle");
		}
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

/* a Halo PC (Custom Edition) vehicle placement's multiplayer team index and spawn flags (game_engine.c's
CE_VEHICLE_SPAWN_FLAGS_OFFSET: the low four bits place it by default in slayer, ctf, king and oddball) */
#define AE_CE_VEHICLE_TEAM_OFFSET 0x58
#define AE_CE_VEHICLE_SPAWN_FLAGS_OFFSET 0x5a
/* a vehicle placement's permutation (change colours, region permutations) after its object datum (vehicles.c's
struct scenario_vehicle) */
#define AE_VEHICLE_PERMUTATION_OFFSET 0x28
#define AE_VEHICLE_PERMUTATION_SIZE 0x20

/* a vehicle tag's kind as the gametypes' vehicle sets tell it (game_engine.c's game_engine_variant_vehicle_type: the
globals' three multiplayer vehicles, then the tag's name; a turret also by its vehicle type) */
static short vehicle_kind_of(long definition)
{
	struct game_globals *globals = scenario_get_game_globals();
	short kind;

	if (definition == NONE)
		return NUMBER_OF_AE_LAYOUT_VEHICLE_KINDS;
	if (globals && globals->multiplayer_information.count > 0)
	{
		struct game_globals_multiplayer_information *information = TAG_BLOCK_GET_ELEMENT(
			&globals->multiplayer_information, 0, struct game_globals_multiplayer_information);
		short slot;

		for (slot = 0; slot < 3 && slot < information->vehicles.count; slot++)
			if (TAG_BLOCK_GET_ELEMENT(&information->vehicles, slot, struct game_globals_vehicle)->vehicle.index ==
				definition)
			{
				return slot == 0 ? _ae_layout_vehicle_warthog : slot == 1 ? _ae_layout_vehicle_ghost :
					_ae_layout_vehicle_scorpion;
			}
	}
	kind = ae_layout_vehicle_kind_of_name(tag_get_name(definition));
	if (kind == NUMBER_OF_AE_LAYOUT_VEHICLE_KINDS && ((struct ae_vehicle_definition_head const *)
		tag_get(VEHICLE_DEFINITION_TAG, definition))->vehicle_type == AE_VEHICLE_TYPE_TURRET)
	{
		kind = _ae_layout_vehicle_turret;
	}
	return kind;
}

/* a vehicle tag of the kind the map has loaded though its palette has none: the globals' multiplayer warthog, ghost
or scorpion, else the stock tag of that kind; NONE */
static long vehicle_loaded(short kind)
{
	static char const *const stock[NUMBER_OF_AE_LAYOUT_VEHICLE_KINDS] =
	{
		"vehicles\\warthog\\mp_warthog", "vehicles\\ghost\\ghost_mp", "vehicles\\scorpion\\scorpion_mp",
		"vehicles\\rwarthog\\rwarthog", "vehicles\\banshee\\banshee_mp", "vehicles\\c gun turret\\c gun turret_mp"
	};
	struct game_globals *globals = scenario_get_game_globals();

	if (kind < 0 || kind >= NUMBER_OF_AE_LAYOUT_VEHICLE_KINDS)
		return NONE;
	if (kind <= _ae_layout_vehicle_scorpion && globals && globals->multiplayer_information.count > 0)
	{
		struct game_globals_multiplayer_information *information = TAG_BLOCK_GET_ELEMENT(
			&globals->multiplayer_information, 0, struct game_globals_multiplayer_information);

		if (kind < information->vehicles.count)
		{
			long definition = TAG_BLOCK_GET_ELEMENT(&information->vehicles, kind, struct game_globals_vehicle)->vehicle.index;

			if (definition != NONE)
				return definition;
		}
	}
	return tag_loaded(VEHICLE_DEFINITION_TAG, stock[kind]);
}

/* vehicle lines: each a vehicle placement more, of a vehicle the map carries (its palette's entry, else one added for
a tag of that kind the map has loaded), so the map's own rules place it: the gametype's vehicle set, NHE MODE's, the
Halo PC spawn flags, the respawn of vehicles (game_engine.c), on the host (vehicles are synced objects) */
static void apply_vehicle_adds(struct scenario *scenario)
{
	long element_size = 0;
	struct tag_block *placements = scenario_get_object_type_scenario_datums(scenario, _object_type_vehicle,
		&element_size);
	struct tag_block *palette = &scenario->vehicle_palette;
	long original_count, palette_count, index, count, added = 0, skipped = 0;
	byte *vehicles;
	struct scenario_object_palette_entry *entries;

	if (!layout.vehicle_count || !placements || element_size < (long)sizeof(struct scenario_object_datum))
		return;
	original_count = placements->count;
	palette_count = palette->count;
	vehicles = (byte *)malloc((size_t)(original_count + layout.vehicle_count) * (size_t)element_size);
	entries = (struct scenario_object_palette_entry *)malloc((size_t)(palette_count + layout.vehicle_count) *
		sizeof(*entries));
	if (!vehicles || !entries)
	{
		if (vehicles)
			free(vehicles);
		if (entries)
			free(entries);
		platform_log("layout: no memory for %d vehicles (not added)", layout.vehicle_count);
		return;
	}
	if (original_count)
		memcpy(vehicles, tag_block_get_element_with_size(placements, 0, element_size),
			(size_t)original_count * (size_t)element_size);
	if (palette_count)
		memcpy(entries, TAG_BLOCK_GET_ELEMENT(palette, 0, struct scenario_object_palette_entry),
			(size_t)palette_count * sizeof(*entries));
	count = original_count;
	for (index = 0; index < layout.vehicle_count; index++)
	{
		struct ae_layout_vehicle const *line = &layout.vehicles[index];
		struct scenario_object_datum *placement;
		long entry = NONE, definition = NONE, template_index = NONE, other;

		if (line->kind == _ae_layout_vehicle_palette)
		{
			if (line->palette_index < palette_count && entries[line->palette_index].reference.index != NONE)
				entry = line->palette_index;
		}
		else
		{
			for (other = 0; other < palette_count && entry == NONE; other++)
				if (vehicle_kind_of(entries[other].reference.index) == line->kind)
					entry = other;
			/* (a palette without one: a tag of that kind the map has loaded gets an entry) */
			if (entry == NONE && (definition = vehicle_loaded(line->kind)) != NONE)
			{
				for (other = palette_count; other < palette_count + added && entry == NONE; other++)
					if (entries[other].reference.index == definition)
						entry = other;
				if (entry == NONE)
				{
					entry = palette_count + added;
					if (palette_count)
						entries[entry] = entries[0];
					else
						memset(&entries[entry], 0, sizeof(entries[entry]));
					entries[entry].reference.group_tag = VEHICLE_DEFINITION_TAG;
					entries[entry].reference.index = definition;
					platform_log("layout: line %d: the map's vehicle palette has no %s: %s (a tag the map has) added "
						"as its entry %ld", line->line, ae_layout_vehicle_kind_name(line->kind), tag_get_name(definition),
						entry);
				}
			}
		}
		if (entry == NONE)
		{
			platform_log("layout: line %d: the map has no %s%s (skipped)", line->line,
				line->kind == _ae_layout_vehicle_palette ? "vehicle palette entry " : ae_layout_vehicle_kind_name(line->kind),
				line->kind == _ae_layout_vehicle_palette ? "of that number" : "");
			skipped++;
			continue;
		}
		/* (made from one of the map's own placements of that vehicle, else another vehicle's, else nothing) */
		for (other = 0; other < original_count && template_index == NONE; other++)
			if (((struct scenario_object_datum *)(vehicles + other * element_size))->palette_entry_index == entry)
				template_index = other;
		placement = (struct scenario_object_datum *)(vehicles + count * element_size);
		if (template_index != NONE)
			memcpy(placement, vehicles + template_index * element_size, (size_t)element_size);
		else
		{
			memset(placement, 0, (size_t)element_size);
			for (other = 0; other < original_count; other++)
				if (((struct scenario_object_datum *)(vehicles + other * element_size))->palette_entry_index != NONE)
				{
					memcpy(placement, vehicles + other * element_size, (size_t)element_size);
					if (element_size >= AE_VEHICLE_PERMUTATION_OFFSET + AE_VEHICLE_PERMUTATION_SIZE)
						memset((byte *)placement + AE_VEHICLE_PERMUTATION_OFFSET, 0, AE_VEHICLE_PERMUTATION_SIZE);
					break;
				}
			placement->variant_number = 0;
		}
		placement->palette_entry_index = (short)entry;
		placement->name_index = NONE;
		placement->placement_flags = 0;
		placement->position.x = line->x;
		placement->position.y = line->y;
		placement->position.z = line->z;
		placement->rotation.yaw = line->has_facing ? line->facing_degrees * DEGREES_TO_RADIANS :
			template_index != NONE ? placement->rotation.yaw : 0.0f;
		placement->rotation.pitch = 0.0f;
		placement->rotation.roll = 0.0f;
		/* (a Halo PC map's: placed in every gametype by its spawn flags, the line's game types decide; its team) */
		if (element_size >= AE_CE_VEHICLE_SPAWN_FLAGS_OFFSET + 2)
		{
			word flags;

			memcpy(&flags, (byte *)placement + AE_CE_VEHICLE_SPAWN_FLAGS_OFFSET, sizeof(flags));
			flags |= 0x000F;
			memcpy((byte *)placement + AE_CE_VEHICLE_SPAWN_FLAGS_OFFSET, &flags, sizeof(flags));
			if (line->team == _ae_layout_team_red || line->team == _ae_layout_team_blue)
				((byte *)placement)[AE_CE_VEHICLE_TEAM_OFFSET] = (byte)line->team;
		}
		applied.added_vehicles[count - original_count].team = line->team;
		memcpy(applied.added_vehicles[count - original_count].games, line->games, sizeof(line->games));
		platform_log("layout: line %d: vehicle %ld (%s, palette %ld) at %.2f %.2f %.2f facing %.0f", line->line, count,
			ae_layout_vehicle_kind_name(line->kind), entry, placement->position.x, placement->position.y,
			placement->position.z, placement->rotation.yaw / DEGREES_TO_RADIANS);
		if (entry >= palette_count + added)
			added++;
		count++;
	}
	applied.vehicles = vehicles;
	applied.vehicle_count = count;
	applied.vehicle_element_size = element_size;
	applied.original_vehicle_count = original_count;
	applied.vehicle_palette = entries;
	applied.vehicle_palette_count = palette_count + added;
	placements->address = XBOX_ADDRESS(vehicles);
	placements->count = count;
	palette->address = XBOX_ADDRESS(entries);
	palette->count = palette_count + added;
	platform_log("layout: vehicles %ld of the map's, %ld added, %ld skipped: %ld in all", original_count,
		count - original_count, skipped, count);
}

/* the index past the map's own of a placement of ours, else NONE */
static long added_vehicle(void const *placement)
{
	unsigned long address = (unsigned long)XBOX_ADDRESS(placement), start, index;

	if (!applied.vehicles || !placement || applied.vehicle_element_size <= 0)
		return NONE;
	start = (unsigned long)XBOX_ADDRESS(applied.vehicles);
	if (address < start)
		return NONE;
	index = (address - start) / (unsigned long)applied.vehicle_element_size;
	if ((address - start) % (unsigned long)applied.vehicle_element_size || (long)index < applied.original_vehicle_count ||
		(long)index >= applied.vehicle_count)
	{
		return NONE;
	}
	return (long)index - applied.original_vehicle_count;
}

boolean ae_layout_vehicle_placed(void const *placement)
{
	long added = added_vehicle(placement);
	struct game_variant *variant = game_engine_get_variant();

	if (added == NONE || !game_engine_running())
		return TRUE;
	return ae_layout_games_take(applied.added_vehicles[added].games, variant ? variant->game_engine_index : 0);
}

short ae_layout_vehicle_side(void const *placement, short side)
{
	long added = added_vehicle(placement);

	if (added == NONE)
		return side;
	if (applied.added_vehicles[added].team == _ae_layout_team_red)
		return 0;
	if (applied.added_vehicles[added].team == _ae_layout_team_blue)
		return 1;
	return side;
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
		memory_in(applied.flags, applied.flag_count, sizeof(*applied.flags), address, size) ||
		memory_in(applied.vehicles, applied.vehicle_count, (unsigned long)applied.vehicle_element_size, address, size) ||
		memory_in(applied.vehicle_palette, applied.vehicle_palette_count, sizeof(*applied.vehicle_palette), address,
			size);
}

real ae_layout_jump_multiplier(void)
{
	return layout_applied && layout.jump_percent ? layout.jump_percent / 100.0f : 1.0f;
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
	char file[64], map_path[PATH_SIZE], path[PATH_SIZE + 32], error[160];
	unsigned int hash = 0;
	char *text;
	char const *host_text = NULL;
	long length = 0, host_length = 0;
	int bad, host_state;

	if (!scenario)
		return;
	/* (this load of the map has it already: a replay without a reload) */
	if (applied.scenario == scenario &&
		((applied.items && scenario->netgame_equipment.address == XBOX_ADDRESS(applied.items)) ||
		(applied.spawns && scenario->players.address == XBOX_ADDRESS(applied.spawns)) ||
		(applied.flags && scenario->netgame_flags.address == XBOX_ADDRESS(applied.flags)) ||
		(applied.vehicle_palette && scenario->vehicle_palette.address == XBOX_ADDRESS(applied.vehicle_palette))))
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
		applied.pack_item_count = 0;
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
	if (applied.vehicles)
	{
		free(applied.vehicles);
		applied.vehicles = NULL;
		applied.vehicle_count = applied.original_vehicle_count = 0;
	}
	if (applied.vehicle_palette)
	{
		free(applied.vehicle_palette);
		applied.vehicle_palette = NULL;
		applied.vehicle_palette_count = 0;
	}
	if (!map_file(file, sizeof(file), map_path, sizeof(map_path)))
		return;
	/* (the map's BSP fixes, with or without a layout: its file and hash known here, ae_map_look_fixes.h) */
	if (map_file_hash_prefix(map_path, &hash))
		ae_map_look_fixes_map_loaded(file, (unsigned long)hash);
	else
		hash = 0;
	text = own_layout(map_path, file, path, sizeof(path), &length);
	if (!path[0])
	{
		if (text)
			free(text);
		return;
	}
	/* (a guest: its host's word on the map's layout, when its host runs this game and sent it, ae_map_share_all.c:
	the host's layout for the match, none if the host has none, else its own file as before; ae_layout_share_rules.h) */
	host_state = ae_map_share_layout_received(cache_file_loaded_map_name(), &host_text, &host_length);
	if (host_state != AE_LAYOUT_SHARE_HOST_UNKNOWN)
	{
		int choice;

		if (!map_file_hash_prefix(map_path, &hash))
			hash = 0;
		choice = host_choice(host_state, host_text, host_length, text, length, path, file, (unsigned long)hash);
		if (choice == AE_LAYOUT_SHARE_APPLY_NOTHING)
		{
			platform_log("layout: the host has no layout for %s: none applied%s", file, text ?
				" (this machine's own file is not used this match)" : "");
			if (text)
				free(text);
			return;
		}
		if (choice == AE_LAYOUT_SHARE_APPLY_HOST)
		{
			char *copy = (char *)malloc((size_t)host_length + 1);

			if (copy)
			{
				memcpy(copy, host_text, (size_t)host_length);
				copy[host_length] = 0;
				platform_log("layout: the host's layout for %s is used (%ld bytes; this machine's own file %s)", file,
					host_length, !text ? "is not there" : length == host_length && !memcmp(text, host_text,
					(size_t)length) ? "is the same" : "differs");
				if (text)
					free(text);
				text = copy;
				length = host_length;
				snprintf(path, sizeof(path), "the host's layout");
			}
		}
	}
	if (!text)
		return;
	log_map(scenario);
	bad = ae_layout_parse_for(text, length, layout_snd(), &layout, error, sizeof(error));
	free(text);
	if (bad)
	{
		platform_log("layout: %s line %d: %s (not applied)", path, bad, error);
		return;
	}
	if ((!hash && !map_file_hash_prefix(map_path, &hash)) || !ae_layout_for_map(&layout, file, (unsigned long)hash))
	{
		platform_log("layout: %s was made for %s %08lx, this map is %s %08x (not applied)", path, layout.map_file,
			layout.map_hash, file, hash);
		return;
	}
	platform_log("layout: %s (%d items, %d removed, %sclear, %d vehicle lines, %d names, %d bot jumps, %d bot avoids, "
		"%d spawns, %d flags, %d object lines, %d vehicles)", path, layout.item_count, layout.removed_count,
		layout.clear_items ? "" : "no ", layout.vehicle_rule_count, layout.name_count, layout.bot_jump_count,
		layout.bot_avoid_count, layout.spawn_count, layout.flag_count, layout.object_rule_count, layout.vehicle_count);
	if (layout.jump_percent)
		platform_log("layout: jump %d%%", layout.jump_percent);
	layout_applied = TRUE;
	apply_items(scenario);
	apply_vehicles(scenario);
	apply_vehicle_adds(scenario);
	apply_objects(scenario);
	apply_spawns(scenario);
	apply_flags(scenario);
}
