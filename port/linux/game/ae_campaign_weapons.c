/*
AE_CAMPAIGN_WEAPONS.C

AE WEAPONS IN LEVELS and the AI squad's gear: see ae_campaign_weapons.h. The rules (kinds, table, seeded share, the
squad's pairs) are ae_campaign_weapons_rules.c's.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "ai/actors.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "items/weapon_definitions.h"
#include "main/main.h"
#include "networking/network_game_globals.h"
#include "objects/objects.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "tag_files/tag_files.h"
#include "tag_files/tag_groups.h"
#include "units/units.h"

#include "../src/port_config.h"
#include "ae_weapon_cache.h"
#include "ae_weapon_ids.h"
#include "ae_weapon_pack.h"
#include "ae_campaign_weapons.h"

void platform_log(char const *format, ...);
extern boolean tag_index_is_group(long tag_index, long group_tag);

enum
{
	SPOTS = 64,
	LOGGED = 12
};

struct spot
{
	float x, y, z;
};

static struct
{
	char map[48];
	int pickups, actors, kept, left_out, actors_refused;
	int logged;
	unsigned long unavailable_said;
	int none_said;
	int spot_count;
	struct spot spot[SPOTS];
	int squad_said;
} state;

/* a level that is not the one the counts are of starts them again (a checkpoint reload of the same level keeps them) */
static void level_check(void)
{
	char const *map = main_get_map_name();

	if (!map)
		map = "";
	if (strncmp(state.map, map, sizeof state.map - 1))
	{
		memset(&state, 0, sizeof state);
		snprintf(state.map, sizeof state.map, "%s", map);
	}
}

/* a campaign level of this machine's own (alone, or the host of co-op) with the setting on */
static int active(void)
{
	struct scenario *scenario;

	if (!config_boolean("game.campaign_ae_weapons") || game_engine_running() || network_game_distributed_client())
		return 0;
	scenario = global_scenario_try_and_get();
	if (!scenario || scenario->type != _scenario_type_solo)
		return 0;
	level_check();
	return 1;
}

static int share(void)
{
	return ae_campaign_weapons_share_clamp(config_integer("game.campaign_ae_weapons_share"));
}

/* the weapon's definition when the level has it, else NONE with one line (once for each weapon) */
static long definition_of(unsigned int id)
{
	struct ae_weapon_registry_row const *row = ae_weapon_registry_row(id);
	long definition_index;

	if (!id || !ae_weapon_id_available(id))
	{
		if (id < 32 && !(state.unavailable_said & (1UL << id)))
		{
			state.unavailable_said |= 1UL << id;
			platform_log("campaign weapons: the %s is not on this level (no weapon pack is added to it, or it was refused: see the "
				"weapon pack lines): the level's own weapon stays", row ? row->display : "weapon");
		}
		return NONE;
	}
	definition_index = ae_weapon_id_definition(id);
	return definition_index;
}

long ae_campaign_weapons_definition(long definition_index, void *scenario_object)
{
	struct scenario_object_datum *object = scenario_object;
	int kind;
	unsigned long seed;
	unsigned int id;
	long picked;

	if (definition_index == NONE || !object || !active())
		return definition_index;
	/* (a weapon a script names stays where it is, as a weapon that is the pack's already) */
	if (object->name_index != NONE || !tag_index_is_group(definition_index, WEAPON_DEFINITION_TAG) ||
		ae_weapon_cache_is_own(definition_index))
		return definition_index;
	kind = ae_campaign_weapons_kind_of(tag_get_name(definition_index));
	if (kind == AE_CAMPAIGN_WEAPONS_NONE)
	{
		state.left_out++;
		return definition_index;
	}
	seed = ae_campaign_weapons_seed(state.map, object->position.x, object->position.y, object->position.z, 1);
	if (!ae_campaign_weapons_share_takes(seed, share()))
	{
		state.kept++;
		return definition_index;
	}
	id = ae_campaign_weapons_pick(kind, seed);
	picked = definition_of(id);
	if (picked == NONE)
		return definition_index;
	state.pickups++;
	if (state.spot_count < SPOTS)
	{
		int each;

		for (each = 0; each < state.spot_count; each++)
			if (state.spot[each].x == object->position.x && state.spot[each].y == object->position.y &&
				state.spot[each].z == object->position.z)
				break;
		if (each == state.spot_count)
		{
			state.spot[each].x = object->position.x;
			state.spot[each].y = object->position.y;
			state.spot[each].z = object->position.z;
			state.spot_count++;
		}
	}
	if (state.logged < LOGGED)
	{
		struct ae_weapon_registry_row const *row = ae_weapon_registry_row(id);

		state.logged++;
		platform_log("campaign weapons: %s on the ground at %.1f %.1f %.1f -> %s", ae_campaign_weapons_kind_name(kind),
			object->position.x, object->position.y, object->position.z, row ? row->display : "?");
	}
	return picked;
}

int ae_campaign_weapons_actor_weapon(long weapon_definition, long unit_index)
{
	struct unit_datum *unit;
	struct object_placement_data placement_data;
	unsigned long seed;
	unsigned int id;
	long picked, weapon_index;
	int kind;

	if (weapon_definition == NONE || unit_index == NONE || !active())
		return 0;
	if (ae_weapon_cache_is_own(weapon_definition))
		return 0;
	kind = ae_campaign_weapons_kind_of(tag_get_name(weapon_definition));
	if (kind == AE_CAMPAIGN_WEAPONS_NONE)
		return 0;
	unit = unit_try_and_get(unit_index);
	if (!unit || unit->unit.player_index != NONE)
		return 0;
	/* (the place it spawned, not the unit's index: a checkpoint's unit is a new one with the weapon it had) */
	seed = ae_campaign_weapons_seed(state.map, unit->object.position.x, unit->object.position.y, unit->object.position.z, 2);
	if (!ae_campaign_weapons_share_takes(seed, share()))
	{
		state.kept++;
		return 0;
	}
	id = ae_campaign_weapons_pick(kind, seed);
	picked = definition_of(id);
	if (picked == NONE)
		return 0;
	object_placement_data_new(&placement_data, picked, unit_index);
	weapon_index = object_new(&placement_data);
	/* (one the unit has no animations to hold is refused, and deleted: the actor's own weapon is then given) */
	if (weapon_index != NONE && unit_add_weapon_to_inventory(unit_index, weapon_index, _unit_add_weapon_replace))
	{
		struct ae_weapon_registry_row const *row = ae_weapon_registry_row(id);

		state.actors++;
		if (state.logged < LOGGED)
		{
			state.logged++;
			platform_log("campaign weapons: an actor's %s -> %s", ae_campaign_weapons_kind_name(kind), row ? row->display : "?");
		}
		return 1;
	}
	if (weapon_index != NONE)
		object_delete(weapon_index);
	state.actors_refused++;
	return 0;
}

int ae_campaign_weapons_game_needs_cache(int campaign_game)
{
	return ae_campaign_weapons_join_needs_cache(campaign_game, config_boolean("game.campaign_ae_weapons") ? 1 : 0,
		ae_campaign_weapons_squad_is_ae(), (int)config_integer("game.companions"));
}

int ae_campaign_weapons_squad_is_ae(void)
{
	return ae_campaign_weapons_loadout_of_word(config_string("game.companions_loadout")) == AE_CAMPAIGN_WEAPONS_LOADOUT_AE;
}

int ae_campaign_weapons_give_squad(long unit_index, int slot, int reset_equipment)
{
	unsigned int ids[2];
	long definitions[2];
	int index, given = 0, first = 1;

	if (unit_index == NONE || !ae_campaign_weapons_squad_is_ae())
		return 0;
	level_check();
	ae_campaign_weapons_squad(slot, &ids[0], &ids[1]);
	for (index = 0; index < 2; index++)
	{
		definitions[index] = definition_of(ids[index]);
		if (definitions[index] == NONE)
			return 0;
	}
	/* (a script's weapons added to those held: a squad member keeps what it has) */
	if (!reset_equipment && unit_get_weapon_count(unit_index) > 0)
		return 1;
	for (index = 0; index < 2; index++)
	{
		struct object_placement_data placement_data;
		long weapon_index;

		object_placement_data_new(&placement_data, definitions[index], unit_index);
		weapon_index = object_new(&placement_data);
		if (weapon_index == NONE)
			continue;
		if (unit_add_weapon_to_inventory(unit_index, weapon_index, first ? _unit_add_weapon_starting : _unit_add_weapon_normal))
		{
			first = 0;
			given++;
		}
		else
			object_delete(weapon_index);
	}
	if (given && !(state.squad_said & (1 << (slot & 3))))
	{
		struct ae_weapon_registry_row const *primary = ae_weapon_registry_row(ids[0]), *sidearm = ae_weapon_registry_row(ids[1]);

		state.squad_said |= 1 << (slot & 3);
		platform_log("campaign weapons: squad member %d carries the %s and the %s", slot + 1, primary ? primary->display : "?",
			sidearm ? sidearm->display : "?");
	}
	return given ? 1 : 0;
}

void ae_campaign_weapons_status(char *text, unsigned int size)
{
	level_check();
	snprintf(text, size, "campaign weapons %s (share %d%%): %d weapons on the ground and %d carried swapped, %d kept, %d of other kinds, "
		"%d carried refused; squad loadout %s",
		config_boolean("game.campaign_ae_weapons") ? "on" : "off", share(), state.pickups, state.actors, state.kept, state.left_out,
		state.actors_refused, ae_campaign_weapons_squad_is_ae() ? "ae" : "standard");
}

int ae_campaign_weapons_spot(int number, float *x, float *y, float *z)
{
	if (number >= 1 && number <= state.spot_count)
	{
		*x = state.spot[number - 1].x;
		*y = state.spot[number - 1].y;
		*z = state.spot[number - 1].z;
	}
	return state.spot_count;
}
