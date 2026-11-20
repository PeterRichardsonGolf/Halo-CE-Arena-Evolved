/*
RANDOMIZER.C

A campaign level can swap its actors and the weapons they carry, and the
weapons lying in it, without changing the map file. The actors are taken
from a pool (any of them, one faction, or one type). A tag the level does
not have is copied from another campaign map by the tag importer
(tag_import.c), dependencies included, from tag_pack/ae_randomizer.pack,
which tools/tag_pack.py writes from the campaign maps (its manifest is
port/assets/tag_packs/ae_randomizer.toml); a level whose pack is not written
copies nothing and plays as the map has it.

The choice is a hash of the map, the setup, the placement and a roll made
when the level loads, so starting it again is a different layout (or the
same one, with game.randomizer_seed). A checkpoint reloads the saved actors,
not the tags. A starting location with a command list, and a squad named by a
conversation, keep the actor the level gave them. The player's allies are
swapped by a setup of their own (game.randomizer_allies, from a pool of
their own, or the enemies'). A weapon a script names is left where it was.

PLAYER LOADOUT (game.randomizer_loadout) gives the players other weapons than
the starting profile's: two at random or the two chosen, at the spawn and
whenever a script gives the player a profile (player_add_equipment).

Off unless game.randomizer is on, and only for the ten campaign levels played
alone or in split screen on one machine: a network game is never randomized.

The level's copies (the randomizer's actors and weapons) are made as its tags
load, and a saved game of the level carries a number of them
(randomizer_save_signature): one made with other copies than the level has
now is not taken, and the level starts over.
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "cseries/errors.h"
#include "cseries/cseries_windows.h"
#include "ai/actor_definitions.h"
#include "ai/actor_placement.h"
#include "ai/actors.h"
#include "ai/ai_scenario_definitions.h"
#include "game/game.h"
#include "items/weapon_definitions.h"
#include "main/main.h"
#include "networking/network_game_globals.h"
#include "objects/objects.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "tag_files/tag_files.h"
#include "tag_files/tag_groups.h"
#include "units/unit_definitions.h"
#include "units/units.h"

#include "tag_import.h"
#include "tag_pack.h"
#include "tag_field_bits.h"
#include "ae_tag_loaded.h"
#include "randomizer.h"

void platform_log(char const *format, ...);

#define POOL_MAX 256
#define WEAPON_MAX 128
#define PROTECTED_MAX 128
#define POD_SQUADS_MAX 64
#define WEAPON_TRIES 8

/* PLAYER LOADOUT (game.randomizer_loadout): the starting profile's weapons,
two at random or the two chosen */
enum
{
	LOADOUT_DEFAULT = 0,
	LOADOUT_RANDOM,
	LOADOUT_SET,
	LOADOUT_COUNT,
	LOADOUT_WEAPONS = 2
};

enum
{
	POOL_ANY = 0,
	POOL_COVENANT,
	POOL_FLOOD,
	POOL_HUMAN,
	POOL_ONE,
	TYPE_GRUNT = 0,
	TYPE_ELITE,
	TYPE_JACKAL,
	TYPE_HUNTER,
	TYPE_FLOOD_COMBAT,
	TYPE_FLOOD_CARRIER,
	TYPE_MARINE,
	TYPE_FLOOD_INFECTION,
	TYPE_ENGINEER,
	TYPE_SENTINEL,
	TYPE_COUNT,
	TYPE_NONE = -1
};

/* the weapons a pool is made of: the campaign's hand weapons. The fuel rod
and the energy sword are only the actors' */
enum
{
	WEAPON_PISTOL = 0,
	WEAPON_ASSAULT_RIFLE,
	WEAPON_SHOTGUN,
	WEAPON_SNIPER_RIFLE,
	WEAPON_ROCKET_LAUNCHER,
	WEAPON_FLAMETHROWER,
	WEAPON_PLASMA_PISTOL,
	WEAPON_PLASMA_RIFLE,
	WEAPON_NEEDLER,
	WEAPON_FUEL_ROD,
	WEAPON_ENERGY_SWORD,
	WEAPON_KIND_COUNT,
	WEAPON_NONE = -1,
	WEAPON_POOL_ANY = 0,
	WEAPON_POOL_COVENANT,
	WEAPON_POOL_HUMAN,
	WEAPON_POOL_PRECISION,
	WEAPON_POOL_AUTOMATIC,
	WEAPON_POOL_POWER,
	WEAPON_POOL_ONE,
	WEAPON_POOL_COUNT
};

static struct
{
	char const *tag;
	char const *setting;
	int covenant;
	int precision;
	int automatic;
	int power;
	int actors_only;
} const weapon_kinds[WEAPON_KIND_COUNT] =
{
	{ "weapons\\pistol\\pistol", "pistol", 0, 1, 0, 0, 0 },
	{ "weapons\\assault rifle\\assault rifle", "assault_rifle", 0, 0, 1, 0, 0 },
	{ "weapons\\shotgun\\shotgun", "shotgun", 0, 0, 0, 1, 0 },
	{ "weapons\\sniper rifle\\sniper rifle", "sniper_rifle", 0, 1, 0, 1, 0 },
	{ "weapons\\rocket launcher\\rocket launcher", "rocket_launcher", 0, 0, 0, 1, 0 },
	/* (the campaign's last levels': only chosen alone) */
	{ "weapons\\flamethrower\\flamethrower", "flamethrower", 0, 0, 0, 1, 0 },
	{ "weapons\\plasma pistol\\plasma pistol", "plasma_pistol", 1, 0, 0, 0, 0 },
	{ "weapons\\plasma rifle\\plasma rifle", "plasma_rifle", 1, 0, 1, 0, 0 },
	{ "weapons\\needler\\needler", "needler", 1, 0, 1, 0, 0 },
	{ "weapons\\fuel rod gun\\fuel rod", "fuel_rod", 1, 0, 0, 1, 1 },
	{ "weapons\\energy sword\\energy sword", "energy_sword", 1, 0, 0, 1, 1 },
};

/* a conversation's participant, as the scenario's tag schema has it: the
name at 0x24 is the squad the conversation keeps */
struct randomizer_participant
{
	word pad00;
	word flags;
	short selection_type;
	short actor_type;
	short preexisting_object_name_index;
	short new_attach_object_name_index;
	byte unknown0C[0x0C];
	short dialogue_variants[6];
	char ai_index_name[32];
	long runtime_ai_index;
	byte unknown48[0x0C];
};

typedef char randomizer_participant_size[sizeof(struct randomizer_participant) == 0x54 ? 1 : -1];

/* the actors one side's are swapped for */
struct actor_pool
{
	int on;
	int pool;
	int type;
};

struct setup
{
	struct actor_pool enemies;
	/* the player's allies */
	struct actor_pool allies;
	int pickups;
	int weapons;
	int pickup_pool;
	int pickup_type;
	int weapon_pool;
	int weapon_type;
	/* PLAYER LOADOUT (LOADOUT_), and SET's two weapons: "none" or a campaign
	weapon's setting */
	int loadout;
	char loadout_weapons[LOADOUT_WEAPONS][32];
};

extern boolean tag_index_is_group(long tag_index, long group_tag);
extern void *cache_files_tag_instances(long *count);
extern int config_boolean(char const *name);
extern long config_integer(char const *name);
extern char const *config_string(char const *name);
extern int config_set(char const *name, char const *value);

/* the loaded map's tag, as cache_files.c has it */
struct cache_file_tag_instance
{
	long group_tag;
	long parent_group_tags[2];
	long tag_index;
	XPTR(char) name;
	XPTR(void) base_address;
	unsigned long unused[2];
};

typedef char randomizer_tag_table_size[
	sizeof(struct cache_file_tag_instance) == 0x20 ? 1 : -1];

/* the actor variants each side's squads may become */
static struct actor_choice
{
	long variants[POOL_MAX];
	long count;
} enemy_choice, ally_choice;
static struct weapon_choice
{
	long definitions[WEAPON_MAX];
	long count;
} pickup_choice, actor_weapon_choice, loadout_choice;
static char protected_names[PROTECTED_MAX][32];
static int protected_count;
static struct setup current;
static int loaded;
static char map_name_leaf[32];
/* PLAYER LOADOUT, as the level loading had it: SET's weapons (RANDOM's are
loadout_choice's), and whether the level has some to give */
static long loadout_set[LOADOUT_WEAPONS];
static int loadout_ready;
static int loadout_imports;
static unsigned long placement_roll;
static int placement_roll_ready;
/* the level's tags copied in, and the hash of its table with them */
static int imported;
static unsigned long import_signature;
/* the infection forms of each squad swapped so far, and the last
placement's: counted from the level's start */
static struct
{
	long encounter;
	long squad;
	long count;
} pod_squads[POD_SQUADS_MAX];
static long pod_squads_next;
static int placement_left_out;
/* the level's infection forms so far, for the log */
static long pods_swapped;
static long pods_left_out;
static long pods_kept;
/* the swaps the log names: the first few of a level */
#define SWAPS_LOGGED 10
static int swaps_logged[3];
enum { LOG_ACTOR = 0, LOG_PICKUP, LOG_WEAPON };

static unsigned long mix(unsigned long value)
{
	value &= 0xFFFFFFFFUL;
	value ^= value >> 16;
	value = (value * 0x7FEB352DUL) & 0xFFFFFFFFUL;
	value ^= value >> 15;
	value = (value * 0x846CA68BUL) & 0xFFFFFFFFUL;
	value ^= value >> 16;

	return value;
}

static unsigned long mix_text(unsigned long seed, char const *text)
{
	for (; text && *text; text++)
		seed = mix(seed ^ (unsigned char)*text);

	return seed;
}

static unsigned long mix_bits(unsigned long seed, float value)
{
	unsigned int bits = 0;

	memcpy(&bits, &value, sizeof(value) < sizeof(bits) ? sizeof(value) : sizeof(bits));

	return mix(seed ^ bits);
}

static int campaign_leaf(char const *name, char *leaf, unsigned int leaf_size)
{
	char const *stripped;
	int index;

	if (!name || !name[0])
		return 0;
	stripped = tag_name_strip_path(name);
	for (index = 0; index < 10; index++)
	{
		char const *path = main_get_solo_level_name(index);

		if (!_stricmp(name, path) || !_stricmp(stripped, tag_name_strip_path(path)))
		{
			snprintf(leaf, leaf_size, "%s", tag_name_strip_path(path));
			return 1;
		}
	}

	return 0;
}

static int name_index(char const *text, char const *const *names, int count, int otherwise)
{
	int index;

	for (index = 0; text && index < count; index++)
	{
		if (!_stricmp(text, names[index]))
			return index;
	}

	return otherwise;
}

static int pool_id(char const *text)
{
	static char const *const names[] = { "any", "covenant", "flood", "human", "one" };

	return name_index(text, names, (int)NUMBEROF(names), POOL_ANY);
}

/* game.randomizer_type's values (the flood's infection forms are not one) */
static struct
{
	char const *name;
	int type;
} const type_names[] =
{
	{ "grunt", TYPE_GRUNT },
	{ "elite", TYPE_ELITE },
	{ "jackal", TYPE_JACKAL },
	{ "hunter", TYPE_HUNTER },
	{ "flood_combat", TYPE_FLOOD_COMBAT },
	{ "flood_carrier", TYPE_FLOOD_CARRIER },
	{ "marine", TYPE_MARINE },
	{ "engineer", TYPE_ENGINEER },
	{ "sentinel", TYPE_SENTINEL },
};

static int type_id(char const *text)
{
	int index;

	for (index = 0; text && index < (int)NUMBEROF(type_names); index++)
	{
		if (!_stricmp(text, type_names[index].name))
			return type_names[index].type;
	}

	return TYPE_GRUNT;
}

static int weapon_pool_id(char const *text)
{
	static char const *const names[WEAPON_POOL_COUNT] = {
		"any", "covenant", "human", "precision", "automatic", "power", "one"
	};

	return name_index(text, names, WEAPON_POOL_COUNT, WEAPON_POOL_ANY);
}

static int weapon_kind_id(char const *text, int otherwise)
{
	int index;

	for (index = 0; text && index < WEAPON_KIND_COUNT; index++)
	{
		if (!_stricmp(text, weapon_kinds[index].setting))
			return index;
	}

	return otherwise;
}

static char const *const loadout_names[LOADOUT_COUNT] = { "default", "random", "set" };

/* one side's pool, as its settings have it */
static void actor_pool_from_config(struct actor_pool *pool, int ally)
{
	pool->on = config_boolean(ally ? "game.randomizer_allies" : "game.randomizer_enemies");
	pool->pool = pool_id(config_string(ally ? "game.randomizer_ally_pool" : "game.randomizer_pool"));
	pool->type = type_id(config_string(ally ? "game.randomizer_ally_type" : "game.randomizer_type"));
}

static void setup_from_config(struct setup *setup)
{
	memset(setup, 0, sizeof(*setup));
	actor_pool_from_config(&setup->enemies, 0);
	actor_pool_from_config(&setup->allies, 1);
	setup->pickups = config_boolean("game.randomizer_pickups");
	setup->weapons = config_boolean("game.randomizer_enemy_weapons");
	setup->pickup_pool = weapon_pool_id(config_string("game.randomizer_pickup_pool"));
	setup->pickup_type = weapon_kind_id(config_string("game.randomizer_pickup_type"), WEAPON_PISTOL);
	/* (the actors' weapons are not the player's to pick up, and the
	flamethrower is the campaign's last levels' alone) */
	if (weapon_kinds[setup->pickup_type].actors_only)
		setup->pickup_type = WEAPON_PISTOL;
	setup->weapon_pool = weapon_pool_id(config_string("game.randomizer_weapon_pool"));
	setup->weapon_type = weapon_kind_id(config_string("game.randomizer_weapon_type"), WEAPON_PLASMA_RIFLE);
	setup->loadout = name_index(config_string("game.randomizer_loadout"), loadout_names, LOADOUT_COUNT, LOADOUT_DEFAULT);
	snprintf(setup->loadout_weapons[0], sizeof(setup->loadout_weapons[0]), "%s",
		config_string("game.randomizer_loadout_primary"));
	snprintf(setup->loadout_weapons[1], sizeof(setup->loadout_weapons[1]), "%s",
		config_string("game.randomizer_loadout_secondary"));
	/* (the allies taken from the enemies' pool, on or off as they are) */
	if (!_stricmp(config_string("game.randomizer_ally_pool"), "as_enemies"))
	{
		int on = setup->allies.on;

		setup->allies = setup->enemies;
		setup->allies.on = on;
	}
}

static int setup_swaps(struct setup const *setup)
{
	return setup->enemies.on || setup->allies.on || setup->pickups || setup->weapons;
}

static int classify(char const *name)
{
	char lower[256];
	unsigned int index;

	if (!name || !name[0])
		return TYPE_NONE;
	for (index = 0; name[index] && index + 1 < sizeof(lower); index++)
	{
		char character = name[index];

		if (character >= 'A' && character <= 'Z')
			character = (char)(character - 'A' + 'a');
		lower[index] = character;
	}
	lower[index] = 0;
	if (strstr(lower, "floodcombat") || strstr(lower, "flood_combat"))
		return TYPE_FLOOD_COMBAT;
	if (strstr(lower, "floodcarrier") || strstr(lower, "flood_carrier"))
		return TYPE_FLOOD_CARRIER;
	if (strstr(lower, "flood"))
		return TYPE_FLOOD_INFECTION;
	if (strstr(lower, "grunt"))
		return TYPE_GRUNT;
	if (strstr(lower, "jackal"))
		return TYPE_JACKAL;
	if (strstr(lower, "hunter"))
		return TYPE_HUNTER;
	if (strstr(lower, "elite"))
		return TYPE_ELITE;
	if (strstr(lower, "sentinel"))
		return TYPE_SENTINEL;
	if (strstr(lower, "marine") || strstr(lower, "crew"))
		return TYPE_MARINE;
	if (strstr(lower, "bugger") || strstr(lower, "engineer"))
		return TYPE_ENGINEER;

	return TYPE_NONE;
}

/* which of the campaign's hand weapons a weapon tag is, by its name */
static int weapon_kind(char const *name)
{
	int index;

	for (index = 0; name && index < WEAPON_KIND_COUNT; index++)
	{
		if (!_stricmp(name, weapon_kinds[index].tag))
			return index;
	}

	return WEAPON_NONE;
}

static int weapon_pool_allows(int pool, int type, int kind, int for_pickups)
{
	if (kind == WEAPON_NONE || (for_pickups && weapon_kinds[kind].actors_only))
		return 0;
	/* (the flamethrower is in no pool of several: chosen alone it is the one) */
	if (kind == WEAPON_FLAMETHROWER && pool != WEAPON_POOL_ONE)
		return 0;
	switch (pool)
	{
	case WEAPON_POOL_COVENANT: return weapon_kinds[kind].covenant;
	case WEAPON_POOL_HUMAN: return !weapon_kinds[kind].covenant;
	case WEAPON_POOL_PRECISION: return weapon_kinds[kind].precision;
	case WEAPON_POOL_AUTOMATIC: return weapon_kinds[kind].automatic;
	case WEAPON_POOL_POWER: return weapon_kinds[kind].power;
	case WEAPON_POOL_ONE: return kind == type;
	}

	return 1;
}

/* a weapon PLAYER LOADOUT may give that a level copies: SET's two, or
RANDOM's any campaign hand weapon (with the pickups on, it takes theirs,
which their pool copies) */
static int loadout_wanted(int kind)
{
	if (!loadout_imports || kind == WEAPON_NONE)
		return 0;
	if (current.loadout == LOADOUT_SET)
		return !_stricmp(current.loadout_weapons[0], weapon_kinds[kind].setting) ||
			!_stricmp(current.loadout_weapons[1], weapon_kinds[kind].setting);
	return !current.pickups && weapon_pool_allows(WEAPON_POOL_ANY, 0, kind, 1);
}

/* a weapon to copy from the other maps: one a pool the setup chose names.
"any" is the level's own weapons and those its actors carry, as it was */
static int weapon_wanted(char const *name)
{
	int kind = weapon_kind(name);

	return loadout_wanted(kind) || (current.pickups && current.pickup_pool != WEAPON_POOL_ANY &&
			weapon_pool_allows(current.pickup_pool, current.pickup_type, kind, 1)) ||
		(current.weapons && current.weapon_pool != WEAPON_POOL_ANY &&
			weapon_pool_allows(current.weapon_pool, current.weapon_type, kind, 0));
}

static int pool_allows(struct actor_pool const *pool, int kind)
{
	if (kind == TYPE_NONE)
		return 0;
	if (pool->pool == POOL_ONE)
		return kind == pool->type;
	/* (the engineers, which the campaign hardly uses, and the sentinels,
	which are not the covenant's: only chosen alone) */
	if (kind == TYPE_ENGINEER || kind == TYPE_SENTINEL)
		return 0;
	if (pool->pool == POOL_ANY)
		return 1;
	if (pool->pool == POOL_COVENANT)
		return kind == TYPE_GRUNT || kind == TYPE_ELITE || kind == TYPE_JACKAL || kind == TYPE_HUNTER;
	if (pool->pool == POOL_FLOOD)
		return kind == TYPE_FLOOD_COMBAT || kind == TYPE_FLOOD_CARRIER || kind == TYPE_FLOOD_INFECTION;
	if (pool->pool == POOL_HUMAN)
		return kind == TYPE_MARINE;

	return 0;
}

/* whether a side swapped takes this variant */
static int side_wants(struct actor_pool const *pool, int kind)
{
	return pool->on && pool_allows(pool, kind);
}

static int variant_in_pool(struct tag_import_donor *donor, long datum)
{
	long absolute = datum & 0xFFFF;
	struct actor_variant_definition *variant;
	char const *unit_name;
	int kind;

	if (datum == NONE || tag_import_tag_group(donor, absolute) != ACTOR_VARIANT_DEFINITION_TAG)
		return 0;
	variant = tag_import_tag_data(donor, absolute, sizeof(*variant));
	if (!variant)
		return 0;
	unit_name = tag_import_string(donor, FIELD_BITS(variant->unit_reference.name));
	if (!unit_name)
		return 0;
	kind = classify(unit_name);

	return side_wants(&current.enemies, kind) || side_wants(&current.allies, kind);
}

static void seed_donor(struct tag_import_donor *donor)
{
	char const *map = tag_import_donor_map(donor);
	long scenario_absolute = tag_import_scenario(donor);
	long count = tag_import_tag_count(donor);
	struct scenario *scenario;
	struct tag_reference *palette;
	long index;
	long weapons = 0;
	long actors = 0;

	if (scenario_absolute == NONE)
		return;
	scenario = tag_import_tag_data(donor, scenario_absolute, sizeof(*scenario));
	if (!scenario)
		return;
	/* the weapons a pool has that this level has not, once each */
	for (index = 0; index < count; index++)
	{
		char const *name;

		if (tag_import_tag_group(donor, index) != WEAPON_DEFINITION_TAG)
			continue;
		name = tag_import_tag_name(donor, index);
		if (!name || !weapon_wanted(name) || tag_loaded(WEAPON_DEFINITION_TAG, name) != NONE)
			continue;
		weapons += tag_import_want(donor, index);
	}
	if (weapons)
		platform_log("randomizer: %s queued %ld weapons", map, weapons);
	palette = scenario->ai_actor_palette.count > 0 && scenario->ai_actor_palette.count <= 256 ?
		tag_import_address(donor, FIELD_BITS(scenario->ai_actor_palette.address),
			(unsigned long)scenario->ai_actor_palette.count * sizeof(*palette)) : NULL;
	if ((current.enemies.on || current.allies.on) && palette)
	{
		for (index = 0; index < scenario->ai_actor_palette.count; index++)
		{
			if (variant_in_pool(donor, palette[index].index))
				actors += tag_import_want(donor, palette[index].index & 0xFFFF);
		}
		platform_log("randomizer: %s queued %ld of %ld palette tags", map, actors,
			(long)scenario->ai_actor_palette.count);
	}
}

static void add_variant(long datum)
{
	struct actor_variant_definition *variant;
	char const *unit_name;
	int kind;

	if (datum == NONE || !tag_index_is_group(datum, ACTOR_VARIANT_DEFINITION_TAG))
		return;
	variant = actor_variant_definition_get(datum);
	if (!variant)
		return;
	unit_name = variant->unit_reference.index == NONE ? NULL : tag_get_name(variant->unit_reference.index);
	kind = classify(unit_name);
	if (enemy_choice.count < POOL_MAX && pool_allows(&current.enemies, kind))
		enemy_choice.variants[enemy_choice.count++] = datum;
	if (ally_choice.count < POOL_MAX && side_wants(&current.allies, kind))
		ally_choice.variants[ally_choice.count++] = datum;
}

/* a weapon a choice may give, when its pool has it */
static void add_weapon(struct weapon_choice *choice, long datum, int pool, int type, int for_pickups)
{
	long index;

	if (choice->count >= WEAPON_MAX || datum == NONE || !tag_index_is_group(datum, WEAPON_DEFINITION_TAG) ||
		!weapon_pool_allows(pool, type, weapon_kind(tag_get_name(datum)), for_pickups))
		return;
	for (index = 0; index < choice->count; index++)
	{
		if (choice->definitions[index] == datum)
			return;
	}
	choice->definitions[choice->count++] = datum;
}

/* a block definition still at the address the map was linked with is not a
pointer on this machine (a 32-bit build's; the 64-bit one does not read it) */
static void clear_linked_definition(struct tag_block *block)
{
#ifndef HALO_64BIT
	unsigned long address;

	if (!block || !block->definition)
		return;
	address = (unsigned long)block->definition;
	if (address >= 0x80000000UL && address < 0x88000000UL)
		block->definition = NULL;
#else
	(void)block;
#endif
}

/* the tags' loading calls us before scenario_load stores global_scenario.
The scenario tag is already in the table. */
static struct scenario *loaded_scenario(void)
{
	long count = 0;
	struct cache_file_tag_instance *instances = cache_files_tag_instances(&count);
	long index;

	if (!instances)
		return NULL;
	for (index = 0; index < count; index++)
	{
		if (instances[index].group_tag == SCENARIO_TAG && instances[index].base_address)
			return scenario_definition_get(instances[index].tag_index);
	}

	return NULL;
}

/* PLAYER LOADOUT's weapon a SET value names, of the level's tags (NONE:
"none", or one it has not got) */
static long loadout_weapon(char const *value)
{
	int kind = weapon_kind_id(value, WEAPON_NONE);

	if (kind == WEAPON_NONE || weapon_kinds[kind].actors_only)
		return NONE;

	return tag_loaded(WEAPON_DEFINITION_TAG, weapon_kinds[kind].tag);
}

/* PLAYER LOADOUT's weapons, with the level's tags all in: SET's two, or
RANDOM's pool: the pickups' with them on, else every campaign hand weapon
the level has. With neither to give, the level's own */
static void loadout_build(void)
{
	long count = 0;
	struct cache_file_tag_instance *instances = cache_files_tag_instances(&count);
	long index;

	loadout_choice.count = 0;
	if (current.loadout == LOADOUT_SET)
	{
		loadout_set[0] = loadout_weapon(current.loadout_weapons[0]);
		loadout_set[1] = loadout_weapon(current.loadout_weapons[1]);
		loadout_ready = loadout_set[0] != NONE || loadout_set[1] != NONE;
		if (!loadout_ready)
			platform_log("randomizer: the level has neither weapon of the loadout (%s, %s)",
				current.loadout_weapons[0], current.loadout_weapons[1]);
		return;
	}
	if (current.pickups)
		loadout_choice = pickup_choice;
	for (index = 0; !loadout_choice.count && instances && index < count; index++)
	{
		if (instances[index].group_tag == WEAPON_DEFINITION_TAG)
			add_weapon(&loadout_choice, instances[index].tag_index, WEAPON_POOL_ANY, 0, 1);
	}
	loadout_ready = loadout_choice.count > 0;
	platform_log("randomizer: spawn loadout: %ld weapons to choose among", loadout_choice.count);
}

static void build_pools(void)
{
	struct scenario *scenario = loaded_scenario();
	long count = 0;
	struct cache_file_tag_instance *instances = cache_files_tag_instances(&count);
	long index;

	enemy_choice.count = 0;
	ally_choice.count = 0;
	pickup_choice.count = 0;
	actor_weapon_choice.count = 0;
	if (!setup_swaps(&current))
		return;
	for (index = 0; instances && index < count; index++)
	{
		if (instances[index].group_tag == ACTOR_VARIANT_DEFINITION_TAG)
			add_variant(instances[index].tag_index);
	}
	/* a pool of its own: each of its weapons the level has, or was given */
	for (index = 0; instances && index < count; index++)
	{
		if (instances[index].group_tag != WEAPON_DEFINITION_TAG)
			continue;
		if (current.pickup_pool != WEAPON_POOL_ANY)
			add_weapon(&pickup_choice, instances[index].tag_index, current.pickup_pool, current.pickup_type, 1);
		add_weapon(&actor_weapon_choice, instances[index].tag_index, current.weapon_pool, current.weapon_type, 0);
	}
	/* "any": the level's own lying about, and those its actors carry */
	if (current.pickup_pool == WEAPON_POOL_ANY)
	{
		if (scenario && scenario->weapon_palette.count > 0 && scenario->weapon_palette.count <= 256)
		{
			clear_linked_definition(&scenario->weapon_palette);
			for (index = 0; index < scenario->weapon_palette.count; index++)
			{
				struct scenario_object_palette_entry *entry = tag_block_get_element_with_size(
					&scenario->weapon_palette, index, sizeof(*entry));

				if (entry)
					add_weapon(&pickup_choice, entry->reference.index, WEAPON_POOL_ANY, 0, 1);
			}
		}
		for (index = 0; index < enemy_choice.count + ally_choice.count; index++)
		{
			struct actor_variant_definition *variant = actor_variant_definition_get(index < enemy_choice.count ?
				enemy_choice.variants[index] : ally_choice.variants[index - enemy_choice.count]);

			if (variant)
				add_weapon(&pickup_choice, variant->ranged_combat.reference.index, WEAPON_POOL_ANY, 0, 1);
		}
	}
}

static void remember_protected(char const *name)
{
	if (!name || !name[0] || protected_count >= PROTECTED_MAX)
		return;
	snprintf(protected_names[protected_count], sizeof(protected_names[0]), "%s", name);
	protected_count++;
}

static void build_protected(void)
{
	struct scenario *scenario = loaded_scenario();
	long conversation_index;

	protected_count = 0;
	if (!scenario || scenario->ai_conversations.count <= 0 || scenario->ai_conversations.count > 256)
		return;
	clear_linked_definition(&scenario->ai_conversations);
	for (conversation_index = 0; conversation_index < scenario->ai_conversations.count; conversation_index++)
	{
		struct ai_conversation *conversation = tag_block_get_element_with_size(
			&scenario->ai_conversations, conversation_index, sizeof(*conversation));
		long participant_index;

		if (!conversation || conversation->participants.count <= 0 || conversation->participants.count > 64)
			continue;
		clear_linked_definition(&conversation->participants);
		for (participant_index = 0; participant_index < conversation->participants.count; participant_index++)
		{
			struct randomizer_participant *participant = tag_block_get_element_with_size(
				&conversation->participants, participant_index, sizeof(*participant));

			if (participant)
				remember_protected(participant->ai_index_name);
		}
	}
}

static int squad_is_protected(char const *name)
{
	int index;

	if (!name || !name[0])
		return 0;
	for (index = 0; index < protected_count; index++)
	{
		if (!_stricmp(name, protected_names[index]))
			return 1;
	}

	return 0;
}

/* a fresh load of a mission. A checkpoint revert does not load the tags
again, so the actors already placed keep this roll. game.randomizer_seed, when
set, is the roll */
static void placement_roll_new(void)
{
	static unsigned long salt;
	unsigned long seed = (unsigned long)config_integer("game.randomizer_seed");

	salt++;
	placement_roll = seed ? mix(seed) : mix(system_milliseconds() ^ salt);
	if (!placement_roll)
		placement_roll = 1;
	placement_roll_ready = 1;
	platform_log("randomizer: roll %08lx%s", placement_roll, seed ? " (game.randomizer_seed)" : "");
}

unsigned long randomizer_roll(void)
{
	return placement_roll;
}

static unsigned long placement_seed(long encounter_index, long squad_index, long location_index, float const *position)
{
	unsigned long seed = mix_text(0x524E444DUL, map_name_leaf);

	seed = mix(seed ^ placement_roll);
	seed = mix(seed ^ (unsigned long)current.enemies.on);
	seed = mix(seed ^ ((unsigned long)current.pickups << 1));
	seed = mix(seed ^ ((unsigned long)current.weapons << 2));
	seed = mix(seed ^ (unsigned long)current.enemies.pool);
	seed = mix(seed ^ (unsigned long)current.enemies.type);
	seed = mix(seed ^ (unsigned long)encounter_index);
	seed = mix(seed ^ (unsigned long)squad_index);
	seed = mix(seed ^ (unsigned long)location_index);
	if (position)
	{
		seed = mix_bits(seed, position[0]);
		seed = mix_bits(seed, position[1]);
		seed = mix_bits(seed, position[2]);
	}

	return seed;
}

static long pool_pick(long const *pool, long count, unsigned long seed)
{
	if (count <= 0)
		return NONE;

	return pool[seed % (unsigned long)count];
}

/* the randomizer's: each other campaign map's actors and weapons the
setup's pools take, from its pack (tag_import.c) */
static void gather_campaign(char const *leaf)
{
	int level;
	int enemies = current.enemies.on || current.allies.on;
	int pickups = current.pickups && current.pickup_pool != WEAPON_POOL_ANY;
	int weapons = current.weapons && current.weapon_pool != WEAPON_POOL_ANY;

	if (!enemies && !pickups && !weapons && !loadout_imports)
		return;
	platform_log("randomizer: reading actors and weapons from the other campaign maps");
	for (level = 0; level < 10; level++)
	{
		char const *other = tag_name_strip_path(main_get_solo_level_name(level));
		struct tag_import_donor *donor;

		if (!_stricmp(other, leaf))
			continue;
		donor = tag_import_open(RANDOMIZER_PACK, other);
		if (!donor)
			continue;
		seed_donor(donor);
		tag_import_release(donor);
	}
}

void randomizer_tags_unloaded(void)
{
	if (pods_swapped || pods_left_out || pods_kept)
	{
		platform_log("randomizer: infection forms: %ld swapped, %ld left out, %ld placed as they were",
			pods_swapped, pods_left_out, pods_kept);
	}
	pods_swapped = 0;
	pods_left_out = 0;
	pods_kept = 0;
	memset(swaps_logged, 0, sizeof(swaps_logged));
	/* the level is going away. The next start rolls again */
	if (loaded || loadout_ready)
		placement_roll_ready = 0;
	loadout_ready = 0;
	loadout_imports = 0;
	if (imported)
	{
		tag_import_level_unloaded();
		ae_tag_table_changed();
	}
	imported = 0;
	import_signature = 0;
	enemy_choice.count = 0;
	ally_choice.count = 0;
	pickup_choice.count = 0;
	actor_weapon_choice.count = 0;
	protected_count = 0;
	loaded = 0;
	memset(pod_squads, 0, sizeof(pod_squads));
	pod_squads_next = 0;
}

int randomizer_imported(void)
{
	return imported;
}

unsigned long randomizer_save_signature(void)
{
	return imported ? import_signature : 0;
}

void randomizer_tags_loaded(char const *scenario_name)
{
	struct scenario *scenario;
	char leaf[32];
	int randomize;
	int loadout;
	long before_count = 0;
	unsigned long before_hash = 0;
	long after_count = 0;
	unsigned long after_hash = 0;

	randomizer_tags_unloaded();
	/* (off: nothing is read, begun or logged) */
	if (!config_boolean("game.randomizer"))
		return;
	scenario = loaded_scenario();
	if ((scenario && scenario->type == _scenario_type_main_menu) || !campaign_leaf(scenario_name, leaf, sizeof(leaf)))
		return;
	/* (a network game, a client's or a host's, is never randomized) */
	if (network_game_get_game() || network_game_distributed_client())
	{
		platform_log("randomizer: %s is in a network game: not randomized", leaf);
		return;
	}
	setup_from_config(&current);
	randomize = setup_swaps(&current);
	loadout = current.loadout != LOADOUT_DEFAULT;
	loadout_imports = loadout;
	if (!randomize && !loadout)
	{
		loadout_imports = 0;
		return;
	}
	if (!tag_pack_ready(RANDOMIZER_PACK))
	{
		platform_log("randomizer: %s plays as the map has it: tag_pack/%s.pack is %s", leaf, RANDOMIZER_PACK,
			tag_pack_busy() ? "being written" : "not written");
		randomizer_pack_write();
		memset(&current, 0, sizeof(current));
		loadout_imports = 0;
		return;
	}
	placement_roll_new();
	snprintf(map_name_leaf, sizeof(map_name_leaf), "%s", leaf);
	tag_import_tag_table_fingerprint(&before_count, &before_hash);
	if (tag_import_begin())
	{
		gather_campaign(leaf);
		tag_import_run();
		tag_import_end();
		tag_import_log("randomizer");
	}
	tag_import_tag_table_fingerprint(&after_count, &after_hash);
	if (after_count != before_count)
	{
		imported = 1;
		import_signature = (mix(after_hash ^ (unsigned long)after_count) << 1) | 1;
		ae_tag_table_changed();
	}
	platform_log("randomizer: %s tag table before: %ld tags, fingerprint %08lx; after: %ld tags, fingerprint %08lx",
		leaf, before_count, before_hash, after_count, after_hash);
	if (randomize)
	{
		build_pools();
		build_protected();
		loaded = 1;
	}
	if (loadout)
		loadout_build();
	platform_log("randomizer: %s, %ld enemy actors, %ld ally actors, %ld pickup weapons, %ld actor weapons",
		leaf, enemy_choice.count, ally_choice.count, pickup_choice.count, actor_weapon_choice.count);
}

/* the team an actor of this variant fights for in this encounter: the
encounter's, or with the default team, its unit's own */
static short variant_team(long variant_index, struct encounter_definition const *encounter)
{
	struct actor_variant_definition *variant;
	struct unit_definition *unit;

	if (encounter->team_index != _game_team_default)
		return encounter->team_index;
	variant = actor_variant_definition_get(variant_index);
	if (!variant || variant->unit_reference.index == NONE ||
		!tag_index_is_group(variant->unit_reference.index, UNIT_DEFINITION_TAG))
		return _game_team_default;
	unit = unit_definition_get(variant->unit_reference.index);

	return unit ? unit->unit.default_team : _game_team_default;
}

/* a variant's type, by its unit's name */
static int variant_type(long variant_index)
{
	struct actor_variant_definition *variant = variant_index == NONE ? NULL :
		actor_variant_definition_get(variant_index);

	if (!variant || variant->unit_reference.index == NONE)
		return TYPE_NONE;

	return classify(tag_get_name(variant->unit_reference.index));
}

/* whether a squad's next infection form to be swapped is, of one in three of
its: its first, then every third, counted as the squad places them, its
respawns too */
static int pod_swapped(long encounter_block, long squad_index)
{
	long count;
	long index;

	for (index = 0; index < POD_SQUADS_MAX; index++)
	{
		if (pod_squads[index].count > 0 && pod_squads[index].encounter == encounter_block &&
			pod_squads[index].squad == squad_index)
			break;
	}
	/* (a squad not counted yet takes the oldest's place) */
	if (index == POD_SQUADS_MAX)
	{
		index = pod_squads_next;
		pod_squads_next = (pod_squads_next + 1) % POD_SQUADS_MAX;
		pod_squads[index].encounter = encounter_block;
		pod_squads[index].squad = squad_index;
		pod_squads[index].count = 0;
	}
	count = pod_squads[index].count++;

	return count % 3 == 0;
}

int randomizer_actor_left_out(void)
{
	return placement_left_out;
}

long randomizer_actor_variant(long original, long encounter_index, long squad_index, void *starting_location)
{
	struct actor_starting_location *location = starting_location;
	struct scenario *scenario;
	struct encounter_definition *encounter;
	struct squad_definition *squad;
	long encounter_block;
	long location_index = 0;
	unsigned long seed;
	int ally;
	struct actor_choice const *side;

	placement_left_out = 0;
	if (!loaded || original == NONE || !location)
		return original;
	if (!current.enemies.on && !current.allies.on)
		return original;
	if (encounter_index == NONE || squad_index == NONE)
		return original;
	if (location->command_list_index != NONE)
		return original;
	scenario = global_scenario_get();
	encounter_block = DATUM_INDEX_TO_ABSOLUTE_INDEX(encounter_index);
	encounter = scenario ? tag_block_get_element_with_size(&scenario->ai_encounters, encounter_block, sizeof(*encounter)) :
		NULL;
	if (!encounter)
		return original;
	/* marines who fight with the player are swapped by the allies' setup,
	the rest by the enemies'. Most of the levels' marines are in encounters of
	the default team, which is their unit's */
	{
		short team = variant_team(original, encounter);

		ally = team == _game_team_player || team == _game_team_human;
	}
	side = ally ? &ally_choice : &enemy_choice;
	if (!(ally ? current.allies.on : current.enemies.on) || side->count <= 0)
		return original;
	squad = tag_block_get_element_with_size(&encounter->squads, squad_index, sizeof(*squad));
	if (!squad)
		return original;
	if (squad_is_protected(squad->name))
		return original;
	{
		struct actor_starting_location *first = tag_block_get_element_with_size(
			&squad->starting_locations, 0, sizeof(*first));

		if (first && location >= first && location < first + squad->starting_locations.count)
			location_index = (long)(location - first);
	}
	seed = placement_seed(encounter_block, squad_index, location_index, &location->position.x);
	{
		long picked = pool_pick(side->variants, side->count, seed);

		if (picked == NONE)
			return original;
		/* an infection form to be swapped for another actor: only one in
		three of a squad's, the rest not placed (actor_place) */
		if (variant_type(original) == TYPE_FLOOD_INFECTION)
		{
			if (variant_type(picked) == TYPE_FLOOD_INFECTION)
				pods_kept++;
			else if (!pod_swapped(encounter_block, squad_index))
			{
				pods_left_out++;
				placement_left_out = 1;
				return NONE;
			}
			else
				pods_swapped++;
		}

		if (swaps_logged[LOG_ACTOR] < SWAPS_LOGGED && picked != original)
		{
			swaps_logged[LOG_ACTOR]++;
			platform_log("randomizer: actor %s -> %s (encounter %ld, squad %s)", tag_name_strip_path(tag_get_name(original)),
				tag_name_strip_path(tag_get_name(picked)), encounter_block, squad->name);
		}

		return picked;
	}
}

long randomizer_weapon_definition(long definition_index, void *scenario_object)
{
	struct scenario_object_datum *object = scenario_object;
	unsigned long seed;
	long picked;

	if (!loaded || definition_index == NONE || !object)
		return definition_index;
	if (!current.pickups || pickup_choice.count <= 0)
		return definition_index;
	if (object->name_index != NONE || !tag_index_is_group(definition_index, WEAPON_DEFINITION_TAG))
		return definition_index;
	seed = placement_seed(object->palette_entry_index, object->name_index, 0, &object->position.x);
	picked = pool_pick(pickup_choice.definitions, pickup_choice.count, seed);

	if (swaps_logged[LOG_PICKUP] < SWAPS_LOGGED && picked != NONE && picked != definition_index)
	{
		swaps_logged[LOG_PICKUP]++;
		platform_log("randomizer: pickup %s -> %s", tag_name_strip_path(tag_get_name(definition_index)),
			tag_name_strip_path(tag_get_name(picked)));
	}

	return picked == NONE ? definition_index : picked;
}

/* the two weapons of a player's loadout: SET's, or RANDOM's, each player's
own, the same for a level, from a roll of the level's. NONE for none */
static void loadout_of_player(long player_index, long *weapons)
{
	long count = loadout_choice.count;
	unsigned long seed = mix(placement_roll ^ mix((unsigned long)player_index + 1));
	long first;

	if (current.loadout == LOADOUT_SET)
	{
		weapons[0] = loadout_set[0];
		weapons[1] = loadout_set[1] == loadout_set[0] ? NONE : loadout_set[1];
		return;
	}
	first = (long)(seed % (unsigned long)count);
	weapons[0] = loadout_choice.definitions[first];
	weapons[1] = count < 2 ? NONE :
		loadout_choice.definitions[(first + 1 + (long)(mix(seed) % (unsigned long)(count - 1))) % count];
}

int randomizer_player_loadout(long unit_index, int reset_equipment)
{
	struct unit_datum *unit;
	long weapons[LOADOUT_WEAPONS];
	int first = 1;
	short index;

	if (!loadout_ready || unit_index == NONE)
		return 0;
	unit = unit_try_and_get(unit_index);
	if (!unit || unit->unit.player_index == NONE)
		return 0;
	/* (a script's weapons added to those held: a player keeps what it has) */
	if (!reset_equipment && unit_get_weapon_count(unit_index) > 0)
		return 1;
	loadout_of_player(unit->unit.player_index, weapons);
	for (index = 0; index < LOADOUT_WEAPONS; index++)
	{
		struct object_placement_data placement_data;
		long weapon_index;

		if (weapons[index] == NONE)
			continue;
		object_placement_data_new(&placement_data, weapons[index], unit_index);
		weapon_index = object_new(&placement_data);
		if (weapon_index == NONE)
			continue;
		if (unit_add_weapon_to_inventory(unit_index, weapon_index, first ? _unit_add_weapon_starting : _unit_add_weapon_normal))
			first = 0;
		else
			object_delete(weapon_index);
	}

	return 1;
}

int randomizer_actor_weapon(long weapon_definition, long unit_index)
{
	struct unit_datum *unit;
	unsigned long seed;
	long attempt;

	if (!loaded || !current.weapons || actor_weapon_choice.count <= 0 || weapon_definition == NONE ||
		unit_index == NONE)
		return 0;
	unit = unit_try_and_get(unit_index);
	if (!unit || unit->unit.player_index != NONE)
		return 0;
	/* the place it spawned, not the unit's index: a checkpoint's unit is a
	new one, and the weapon has to be the one it had */
	seed = placement_seed(weapon_definition, 0, 0, &unit->object.position.x);
	for (attempt = 0; attempt < WEAPON_TRIES; attempt++)
	{
		struct object_placement_data placement_data;
		long chosen = pool_pick(actor_weapon_choice.definitions, actor_weapon_choice.count, mix(seed + (unsigned long)attempt));
		long weapon_index;

		if (chosen == NONE)
			break;
		object_placement_data_new(&placement_data, chosen, unit_index);
		weapon_index = object_new(&placement_data);
		/* (one the unit has no animations to hold is refused, and deleted) */
		if (weapon_index != NONE && unit_add_weapon_to_inventory(unit_index, weapon_index, _unit_add_weapon_replace))
		{
			if (swaps_logged[LOG_WEAPON] < SWAPS_LOGGED)
			{
				swaps_logged[LOG_WEAPON]++;
				platform_log("randomizer: an actor's weapon %s -> %s", tag_name_strip_path(tag_get_name(weapon_definition)),
					tag_name_strip_path(tag_get_name(chosen)));
			}
			return 1;
		}
		if (weapon_index != NONE)
			object_delete(weapon_index);
	}

	return 0;
}

void randomizer_pack_state(struct randomizer_pack_state *state)
{
	struct tag_pack_state pack;

	tag_pack_state(RANDOMIZER_PACK, &pack);
	state->writing = pack.writing;
	state->maps_done = pack.maps_done;
	state->maps_wanted = pack.maps_wanted;
	state->done = pack.done;
	state->failed = pack.failed;
	snprintf(state->reason, sizeof(state->reason), "%s", pack.reason);
}

void randomizer_pack_write(void)
{
	tag_pack_request(RANDOMIZER_PACK);
	tag_pack_start_now();
}

void randomizer_frame(void)
{
	static int requested;

	if (!config_boolean("game.randomizer"))
	{
		requested = 0;
		if (tag_pack_busy())
			tag_pack_update(FALSE);
		return;
	}
	tag_pack_update(FALSE);
	if (!requested)
	{
		requested = 1;
		randomizer_pack_write();
	}
}

void randomizer_reroll(void)
{
	config_set("game.randomizer_seed", "0");
	placement_roll_ready = 0;
}

void randomizer_status(char *text, unsigned int size)
{
	long count = 0;
	unsigned long hash = 0;

	struct randomizer_pack_state pack;

	randomizer_pack_state(&pack);
	tag_import_tag_table_fingerprint(&count, &hash);
	snprintf(text, size,
		"randomizer %s, seed %ld: level %s %s, roll %08lx, %ld enemy %ld ally actors, %ld pickup %ld actor weapons, "
		"loadout %s, copied tags %s, table %ld tags %08lx, pack %s %d of %d maps%s%s",
		config_boolean("game.randomizer") ? "on" : "off", config_integer("game.randomizer_seed"),
		map_name_leaf[0] ? map_name_leaf : "-", loaded || loadout_ready ? "randomized" : "not randomized",
		placement_roll, enemy_choice.count, ally_choice.count, pickup_choice.count, actor_weapon_choice.count,
		loadout_ready ? "given" : "default", imported ? "yes" : "no", count, hash,
		pack.writing ? "writing" : pack.done ? "done" : pack.failed ? "failed" : "absent", pack.maps_done,
		pack.maps_wanted, pack.reason[0] ? ": " : "", pack.reason);
}
