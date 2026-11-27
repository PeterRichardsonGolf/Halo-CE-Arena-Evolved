/*
 * OpenCE : Faber ("OpenCE - Faber.exe")
 * Interactive In-Engine AI Placement System (source/ai/ integration).
 * Spawns native Halo: Combat Evolved NPC actors & bipeds (Marines, Elites,
 * Grunts, Jackals, Hunters, Flood, Sentinels) assigned to Friendly or Enemy
 * combat teams.
 * Strictly modular inside FORGE CODE/ with zero modifications to source/ or port/.
 */

#include "cseries.h"
#include "cseries/errors.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "game/game.h"
#include "game/game_allegiance.h"
#include "tag_files/tag_groups.h"
#include "tag_files/tag_files.h"
#include "objects/objects.h"
#include "objects/object_definitions.h"
#include "units/units.h"
#include "ai/ai.h"
#include "ai/actors.h"
#include "ai/actor_definitions.h"
#include "ai/actions.h"
#include "ai/ai_scenario_definitions.h"
#include "ai/encounters.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "scenario/scenario.h"
#include "math/real_math.h"
#include "scenario/scenario_definitions.h"
#include "physics/collisions.h"
#include "units/unit_definitions.h"
#include "units/biped_definitions.h"
#include "items/items.h"
#include "items/weapons.h"
#include "items/weapon_definitions.h"
#include "objects/damage.h"
#include "effects/effect_definitions.h"
#include "effects/effects.h"
#include "halo_skulls.h"

#include "forge_engine.h"
#include "forge_volumes.h"
#include "forge_ai_routes.h"

/* (the C library's own, not cseries.h's debug allocator: these blocks are
calloc's, and live across maps) */
#ifndef HALO_64BIT /* AE hook */
#undef malloc
#undef free
#endif /* AE hook */

extern void *cache_files_tag_instances(long *count);
extern boolean unit_can_hold_weapon_definition(long unit_index, long weapon_definition_index);
extern boolean firefight_active(void);

static short map_forge_team_to_engine_team(int16_t forge_team_designator)
{
	switch (forge_team_designator)
	{
	case _forge_ai_team_friendly_human:
		return _game_team_player; /* Friendly to Spartan players (Human/Player/Blue) */
	case _forge_ai_team_enemy_flood:
		return _game_team_flood;
	case _forge_ai_team_enemy_sentinel:
		return _game_team_sentinel;
	case _forge_ai_team_enemy_covenant:
	default:
		return _game_team_covenant; /* Hostile Enemy Covenant / Red */
	}
}

int32_t forge_ai_spawn_unit_live(
	char const *actor_variant_path,
	float x, float y, float z, float yaw,
	int16_t team_designator,
	int32_t *out_unit_index)
{
	long actv_idx = NONE;
	long unit_def_idx = NONE;
	long unit_index = NONE;
	long actor_index = NONE;
	short engine_team = map_forge_team_to_engine_team(team_designator);
	struct object_placement_data placement;

	if (out_unit_index)
		*out_unit_index = NONE;
	if (!actor_variant_path || !actor_variant_path[0])
		return NONE;

	/* Look up the actor_variant ('actv') tag in the active map */
	actv_idx = forge_resolve_engine_tag('actv', actor_variant_path, NULL);
	if (actv_idx != NONE)
	{
		struct actor_variant_definition *actv_def =
			(struct actor_variant_definition *)tag_get('actv', actv_idx);
		if (actv_def && actv_def->unit_reference.index != NONE)
		{
			unit_def_idx = actv_def->unit_reference.index;
		}
	}

	/* Fallback to resolving a biped ('bipd') directly if the current map has not yet baked this 'actv' */
	if (unit_def_idx == NONE)
	{
		unit_def_idx = forge_resolve_engine_tag('bipd', actor_variant_path, NULL);
	}
	if (unit_def_idx == NONE)
		return NONE;

	object_placement_data_new(&placement, unit_def_idx, NONE);
	placement.position.x = x;
	placement.position.y = y;
	placement.position.z = z;
	placement.forward.i = (real)cos((double)yaw);
	placement.forward.j = (real)sin((double)yaw);
	placement.forward.k = 0.0f;
	placement.up.i = 0.0f;
	placement.up.j = 0.0f;
	placement.up.k = 1.0f;
	placement.owner_team_index = engine_team;

	/* Tint friendly vs enemy change-colors so team allegiance is visually clear */
	if (team_designator == _forge_ai_team_friendly_human)
	{
		placement.change_colors[0].red = 0.20f;
		placement.change_colors[0].green = 0.55f;
		placement.change_colors[0].blue = 0.95f;
	}
	else
	{
		placement.change_colors[0].red = 0.90f;
		placement.change_colors[0].green = 0.22f;
		placement.change_colors[0].blue = 0.18f;
	}

	unit_index = object_new(&placement);
	if (unit_index == NONE)
		return NONE;

	if (out_unit_index)
		*out_unit_index = (int32_t)unit_index;

	if (actv_idx != NONE)
	{
		/* Equip the unit with the actor variant's weapon, grenades, shields & vitality */
		actor_customize_unit(actv_idx, unit_index);

		/* Create and attach the live Halo CE AI actor brain */
		actor_index = actor_create_for_unit(
			FALSE,
			unit_index,
			actv_idx,
			NONE,
			NONE,
			FALSE,
			NONE,
			FALSE,
			_actor_action_alert,
			_actor_action_alert,
			NONE,
			NONE);

		if (actor_index != NONE)
		{
			actor_set_team(actor_index, engine_team);
			actor_set_dormant(actor_index, FALSE);
			actor_braindead(actor_index, FALSE);
		}
	}

	return (int32_t)actor_index;
}

/* ---------- AI spawns

A map's AI is its encounters: squads with starting locations, firing positions
to fight from, and the engine's own placing and respawning. Forge shows each
starting location as a marker and the level's scenario plays what the markers
say, rebuilt as the AI starts (forge_ai_encounters_apply).

The rebuild never moves what a script can name: the map's encounters and squads
keep their indices, with their starting locations as the markers left them. A
marker the map did not have, or one changed past what a starting location can
hold (its team, its numbers), gets a squad of its own, in an encounter added
after the map's: a copy of the nearest of the map's, so it has that encounter's
firing positions to move between. A spawn with no firing positions near, as
on a map with no AI at all, gets an encounter made here, with firing positions
found on the ground around it (forge_ai_firing_positions_add). Only a level
with spawns does any of this: a map that uses no AI is left as it is. Nothing
is written to the map's tags, which every level load reads afresh: a saved map
keeps its spawns as lines of its trailer's text, on stock and Custom Edition
maps alike.

The Actors tab is Forge's pack's actors, read as they are placed, as the rest of
its palette is. A saved map names those its spawns use, which any game of it
copies in as it loads, the same on every machine, so a unit the host's
encounters make is the same tag on its clients (randomizer.c). A Forge game
played online copies in all of them for that reason. */

#define FORGE_AI_MAX_ADDED_ENCOUNTERS 64
/* (weapons a level's spawns can name: a starting location's spare flag bits) */
#define FORGE_AI_MAX_WEAPONS 127
#define FORGE_AI_LOCATION_WEAPON_SHIFT 1
#define FORGE_AI_LOCATION_OWN_FLAGS ((1 << FORGE_AI_LOCATION_WEAPON_SHIFT) - 1)
/* a spawn's game modes, a bit each. 0, as older maps and the map's own locations
have, reads as all but Firefight: Firefight plays only what was set to */
#define FORGE_AI_MODE_FIREFIGHT 0x20u
#define FORGE_AI_MODES_NOT_FIREFIGHT 0x1Fu
#define FORGE_AI_MODES_ALL (FORGE_AI_MODES_NOT_FIREFIGHT | FORGE_AI_MODE_FIREFIGHT)
/* a spawn this far from any firing position gets its own (world units) */
#define FORGE_AI_NEAR_FIRING_POSITION 25.0f
/* made firing positions: each spawn's, on rings around it, in an encounter of
at most 512 (encounters.c) */
#define FORGE_AI_FIRING_POSITIONS_PER_SPAWN 25
#define FORGE_AI_MADE_SQUADS_PER_ENCOUNTER 20
/* what a made squad is, as the maps' own are: guarding, all but the two
letters no map's use, its timer running */
#define FORGE_AI_SQUAD_STATE 7
/* (a new spawn's HUNT/FOLLOW START DISTANCE and FOLLOW DISTANCE, world units) */
#define FORGE_AI_GOAL_START_DEFAULT 6
#define FORGE_AI_GOAL_FOLLOW_DEFAULT 3
/* (HUNT's HUNT RANGE as the goal is chosen: goal_start, 0 anywhere) */
#define FORGE_AI_GOAL_HUNT_RANGE_DEFAULT 30
#define FORGE_AI_SQUAD_POSITION_GROUPS 0x03FFFF03u
#define FORGE_AI_SQUAD_POSITION_GROUP_COUNT 6

typedef char forge_ai_encounter_size_assert[sizeof(struct encounter_definition) == 0xB0 ? 1 : -1];
typedef char forge_ai_squad_size_assert[sizeof(struct squad_definition) == 0xE8 ? 1 : -1];

struct forge_ai_tag_instance
{
	long group_tag;
	long parent_group_tags[2];
	long tag_index;
	XPTR(char) name; /* AE hook */
	XPTR(void) base_address; /* AE hook */
	unsigned long unused[2];
};

/* an encounter added for spawns of one team around one of the map's */
struct forge_ai_added_encounter
{
	short team;
	short source_encounter;
	short squad_count;
	short squads_filled;
	boolean respawns;
	struct squad_definition *squads;
	/* (a made encounter's, source_encounter NONE) */
	struct firing_position_definition *firing_positions;
	short firing_position_count;
	/* (a GOAL's spawn's alone: forge_ai_goal_squad_make) */
	boolean goal;
};

/* ---------- the actors the palette offers

A campaign has an actor for each rank with each weapon. Forge offers a rank
once, and the spawn's WEAPON row arms it: the palette lists these kinds, in
their groups, each from the first of its tags. A rank's other tags stay good in
a saved map's spawn, and show as their kind. */
enum
{
	_forge_ai_group_grunts,
	_forge_ai_group_jackals,
	_forge_ai_group_elites,
	_forge_ai_group_hunters,
	_forge_ai_group_engineers,
	_forge_ai_group_marines,
	_forge_ai_group_crew,
	_forge_ai_group_flood,
	_forge_ai_group_sentinels,
	_forge_ai_group_other,
	NUMBER_OF_FORGE_AI_GROUPS
};

static char const *const s_ai_group_names[NUMBER_OF_FORGE_AI_GROUPS] =
{
	"GRUNTS", "JACKALS", "ELITES", "HUNTERS", "ENGINEERS", "MARINES", "CREW", "FLOOD", "SENTINELS", "OTHER"
};

/* a tag's kind: the first whose prefix its leaf starts with, and that has
`also` in it if it asks for it. A kind with no name is not offered */
static struct forge_ai_kind
{
	short group;
	char const *prefix, *also, *name;
} const s_ai_kinds[] =
{
	{ _forge_ai_group_grunts, "grunt flee", NULL, NULL },
	{ _forge_ai_group_grunts, "grunt minor", NULL, "GRUNT MINOR" },
	{ _forge_ai_group_grunts, "grunt major", NULL, "GRUNT MAJOR" },
	{ _forge_ai_group_grunts, "grunt specops", NULL, "GRUNT SPECOPS" },
	{ _forge_ai_group_jackals, "jackal minor", NULL, "JACKAL MINOR" },
	{ _forge_ai_group_jackals, "jackal major", NULL, "JACKAL MAJOR" },
	{ _forge_ai_group_elites, "elite minor", NULL, "ELITE MINOR" },
	{ _forge_ai_group_elites, "elite major", NULL, "ELITE MAJOR" },
	{ _forge_ai_group_elites, "elite specops", NULL, "ELITE SPECOPS" },
	{ _forge_ai_group_elites, "elite commander", NULL, "ELITE COMMANDER" },
	{ _forge_ai_group_elites, "stealth elite major", NULL, "ELITE STEALTH MAJOR" },
	{ _forge_ai_group_elites, "stealth elite", NULL, "ELITE STEALTH" },
	{ _forge_ai_group_hunters, "hunter major", NULL, "HUNTER MAJOR" },
	{ _forge_ai_group_hunters, "hunter", NULL, "HUNTER" },
	{ _forge_ai_group_engineers, "engineer major", NULL, "ENGINEER MAJOR" },
	{ _forge_ai_group_engineers, "engineer", NULL, "ENGINEER" },
	{ _forge_ai_group_marines, "marine_armored", " major", "ARMORED MARINE MAJOR" },
	{ _forge_ai_group_marines, "marine_armored", NULL, "ARMORED MARINE" },
	{ _forge_ai_group_marines, "marine_wounded", NULL, "WOUNDED MARINE" },
	{ _forge_ai_group_marines, "marine_suicidal", NULL, "SUICIDAL MARINE" },
	{ _forge_ai_group_marines, "marine", " major", "MARINE MAJOR" },
	{ _forge_ai_group_marines, "marine", NULL, "MARINE" },
	{ _forge_ai_group_crew, "crewman", NULL, "CREWMAN" },
	{ _forge_ai_group_flood, "floodcombat elite", NULL, "FLOOD COMBAT ELITE" },
	{ _forge_ai_group_flood, "floodcombat_human", NULL, "FLOOD COMBAT HUMAN" },
	{ _forge_ai_group_flood, "floodcarrier", NULL, "FLOOD CARRIER" },
	{ _forge_ai_group_flood, "flood_infection", NULL, "FLOOD INFECTION" },
	{ _forge_ai_group_sentinels, "sentinel_shielded", " major", "SHIELDED SENTINEL MAJOR" },
	{ _forge_ai_group_sentinels, "sentinel_shielded", NULL, "SHIELDED SENTINEL" },
	{ _forge_ai_group_sentinels, "sentinel_defensive", NULL, "DEFENSIVE SENTINEL" },
	{ _forge_ai_group_sentinels, "sentinel", " major", "SENTINEL MAJOR" },
	{ _forge_ai_group_sentinels, "sentinel", NULL, "SENTINEL" },
};

/* a kind's tag not to arm it from when another is there */
static char const *const s_ai_poor_tags[] = { "unarmed", "airdef", "ranged", "melee", "cinematic" };

static char const *forge_ai_path_leaf(char const *path);

/* the kind of an actor's tag; FORGE_AI_KIND_OTHER for an actor of no kind (a
Custom Edition map's), FORGE_AI_KIND_HIDDEN for one not offered */
int forge_ai_kind_of(char const *path)
{
	char lower[FORGE_TAG_PATH_MAX];
	char const *leaf = forge_ai_path_leaf(path ? path : "");
	size_t i;
	int kind;

	for (i = 0; leaf[i] && i + 1 < sizeof(lower); i++)
		lower[i] = (char)tolower((unsigned char)leaf[i]);
	lower[i] = 0;
	for (kind = 0; kind < (int)NUMBEROF(s_ai_kinds); kind++)
	{
		struct forge_ai_kind const *entry = &s_ai_kinds[kind];

		if (!strncmp(lower, entry->prefix, strlen(entry->prefix)) && (!entry->also || strstr(lower, entry->also)))
			return entry->name ? kind : FORGE_AI_KIND_HIDDEN;
	}
	return FORGE_AI_KIND_OTHER;
}

char const *forge_ai_kind_name(int kind)
{
	return kind >= 0 && kind < (int)NUMBEROF(s_ai_kinds) ? s_ai_kinds[kind].name : NULL;
}

int forge_ai_kind_group(int kind)
{
	return kind >= 0 && kind < (int)NUMBEROF(s_ai_kinds) ? s_ai_kinds[kind].group : _forge_ai_group_other;
}

int forge_ai_group_count(void)
{
	return NUMBER_OF_FORGE_AI_GROUPS;
}

char const *forge_ai_group_name(int group)
{
	return group >= 0 && group < NUMBER_OF_FORGE_AI_GROUPS ? s_ai_group_names[group] : "";
}

/* FORGE_AI_RANDOM_PATH's or FORGE_AI_GROUP_PATH's group, else NONE */
static int forge_ai_path_group(char const *path, char const *prefix)
{
	size_t length = strlen(prefix);

	return path && !strncmp(path, prefix, length) ? atoi(path + length) : NONE;
}

int forge_ai_row_is_random(struct forge_catalog_entry const *entry)
{
	return forge_ai_path_group(entry->tag_path, FORGE_AI_RANDOM_PATH) != NONE;
}

int forge_ai_row_group(struct forge_catalog_entry const *entry)
{
	int group = forge_ai_path_group(entry->tag_path, FORGE_AI_RANDOM_PATH);

	if (group == NONE)
		group = forge_ai_path_group(entry->tag_path, FORGE_AI_GROUP_PATH);
	return group != NONE ? group : forge_ai_kind_group(forge_ai_kind_of(entry->tag_path));
}

int forge_ai_tag_is_poor(char const *path)
{
	int i;

	for (i = 0; i < (int)NUMBEROF(s_ai_poor_tags); i++)
	{
		if (strstr(path, s_ai_poor_tags[i]))
			return 1;
	}
	return 0;
}

/* a spawn of any of its group's ranks: its squad, and the ranks in the palette */
#define FORGE_AI_MAX_RANKS 8
struct forge_ai_random_squad
{
	struct squad_definition const *squad;
	short palette[FORGE_AI_MAX_RANKS];
	short count;
};

/* what the scenario plays in place of its own blocks */
static struct
{
	struct forge_ai_random_squad *randoms;
	int random_count;
	struct encounter_definition *encounters;
	struct squad_definition *squads;
	struct actor_starting_location *locations;
	struct tag_reference *palette;
	struct firing_position_definition *firing_positions;
	struct move_position_definition *move_positions;
	void *original_encounters;
	long original_encounter_count;
	void *original_palette;
	long original_palette_count;
	long weapons[FORGE_AI_MAX_WEAPONS];
	short weapon_count;
} s_built;

/* a saved map's spawns, read as its tags load: `fresh` until its AI starts */
static struct
{
	struct forge_ai_spawn_entry *entries;
	int count;
	int replace_own;
	int has_spawns;
	struct forge_ai_actor_entry *actors;
	int actor_count;
	int fresh;
	int applied;
} s_saved;


/* edit_state.ai_spawns stands for the level's spawns */
static int s_markers_scanned;

/* Forge mode: the AI stands still as models on its markers, none of it
playing, the encounters hidden from the engine so none respawn */
static int s_ai_held;
/* (diagnostic: the first units a level's AI places, in debug.txt) */
static int s_ai_units_placed_logged;
static struct scenario *s_held_scenario;
static long s_held_encounter_count;

static struct forge_ai_tag_instance *forge_ai_tag_instances(long *count)
{
	*count = 0;
	return (struct forge_ai_tag_instance *)cache_files_tag_instances(count);
}

void forge_ai_preview_update(int32_t live_idx);

static long forge_ai_tag_find(uint32_t group_tag, char const *path)
{
	long count, i;
	struct forge_ai_tag_instance *instances = forge_ai_tag_instances(&count);

	if (!path || !path[0])
		return NONE;
	for (i = 0; instances && i < count; i++)
	{
		if ((uint32_t)instances[i].group_tag == group_tag && instances[i].base_address && instances[i].name &&
			!_stricmp(XBOX_POINTER(char const, instances[i].name), path)) /* AE hook */
		{
			return instances[i].tag_index;
		}
	}
	return NONE;
}

/* a tag a spawn names: read from Forge's pack, in a running Forge game, if the
level has not got it */
static long forge_ai_tag_need(uint32_t group_tag, char const *path)
{
	long tag = forge_ai_tag_find(group_tag, path);

	if (tag == NONE && path && path[0] && g_forge_session_enabled && g_forge.live_instance_count > 0)
		tag = forge_resolve_engine_tag(group_tag, path, NULL);
	return tag;
}

static struct encounter_definition *forge_ai_original_encounters(struct scenario const *scen, long *count)
{
	if (s_built.encounters && scen->ai_encounters.address == XBOX_ADDRESS(s_built.encounters)) /* AE hook */
	{
		*count = s_built.original_encounter_count;
		return (struct encounter_definition *)s_built.original_encounters;
	}
	*count = scen == s_held_scenario ? s_held_encounter_count : scen->ai_encounters.count;
	return XBOX_POINTER(struct encounter_definition, scen->ai_encounters.address); /* AE hook */
}

/* the map's own encounters, whatever the level's AI has made of them: Firefight
finds its spawn points in them, played or not */
struct encounter_definition *forge_ai_map_encounters(long *count)
{
	struct scenario *scen = global_scenario_try_and_get();

	*count = 0;
	return scen ? forge_ai_original_encounters(scen, count) : NULL;
}

static struct tag_reference *forge_ai_original_palette(struct scenario const *scen, long *count)
{
	if (s_built.palette && scen->ai_actor_palette.address == XBOX_ADDRESS(s_built.palette)) /* AE hook */
	{
		*count = s_built.original_palette_count;
		return (struct tag_reference *)s_built.original_palette;
	}
	*count = scen->ai_actor_palette.count;
	return XBOX_POINTER(struct tag_reference, scen->ai_actor_palette.address); /* AE hook */
}

static void forge_ai_goals_reset(void);

/* the map's own blocks back, and what stood in for them gone */
static void forge_ai_scenario_restore(struct scenario *scen)
{
	if (s_built.encounters && scen->ai_encounters.address == XBOX_ADDRESS(s_built.encounters)) /* AE hook */
	{
		scen->ai_encounters.address = XBOX_ADDRESS(s_built.original_encounters); /* AE hook */
		scen->ai_encounters.count = s_built.original_encounter_count;
	}
	if (s_built.palette && scen->ai_actor_palette.address == XBOX_ADDRESS(s_built.palette)) /* AE hook */
	{
		scen->ai_actor_palette.address = XBOX_ADDRESS(s_built.original_palette); /* AE hook */
		scen->ai_actor_palette.count = s_built.original_palette_count;
	}
	free(s_built.randoms);
	free(s_built.encounters);
	free(s_built.squads);
	free(s_built.locations);
	free(s_built.palette);
	free(s_built.firing_positions);
	free(s_built.move_positions);
	memset(&s_built, 0, sizeof(s_built));
	forge_ai_goals_reset();
}

int forge_ai_map_editable(void)
{
	struct scenario *scen = global_scenario_try_and_get();

	return scen && scen->type == _scenario_type_multiplayer;
}

int forge_ai_map_supported(void)
{
	struct scenario *scen = global_scenario_try_and_get();
	struct encounter_definition *encounters;
	long count = 0, i;

	if (!forge_ai_map_editable())
		return 0;
	encounters = forge_ai_original_encounters(scen, &count);
	for (i = 0; encounters && i < count; i++)
	{
		if (encounters[i].squads.count > 0)
			return 1;
	}
	return 0;
}

static unsigned int forge_ai_entry_modes(struct forge_ai_spawn_entry const *entry)
{
	unsigned int flags = entry->gametype_flags & ~(unsigned int)FORGE_AI_RANDOM_RANK;

	return flags ? flags : FORGE_AI_MODES_NOT_FIREFIGHT;
}

static int forge_ai_entry_plays(struct forge_ai_spawn_entry const *entry)
{
	unsigned int flags = forge_ai_entry_modes(entry);
	long engine;

	if (!entry->active)
		return 0;
	/* (a Forge game plays them all, to edit them) */
	if (!game_engine_running() || (g_forge_session_enabled && !firefight_active()))
		return 1;
	engine = game_engine_get_variant()->game_engine_index;
	return engine < first_usable_game_engine_index || ((flags >> (engine - first_usable_game_engine_index)) & 1u) != 0;
}

/* a spawn with a squad of its own */
static int forge_ai_entry_own_squad(struct forge_ai_spawn_entry const *entry)
{
	/* (a GOAL's spawn has its squad, and an encounter, to itself: forge_ai_goals_*) */
	return entry->encounter_index < 0 || entry->detached || (entry->gametype_flags & FORGE_AI_RANDOM_RANK) ||
		entry->goal != _forge_ai_goal_none;
}

/* whether a team is a player's in this match: a team game's red or blue, or
any team a player of a free for all is on */
static boolean forge_ai_players_team(short team_index)
{
	struct data_iterator iterator;
	struct player_datum *player;

	if (game_engine_has_teams())
		return team_index == 0 || team_index == 1;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (player->team_index == team_index)
			return TRUE;
	}
	return FALSE;
}

int forge_ai_team_enmity(short our_team_index, short other_team_index)
{
	short other;

	if ((our_team_index == FORGE_AI_TEAM_ALL_PLAYERS) == (other_team_index == FORGE_AI_TEAM_ALL_PLAYERS))
		return NONE;
	/* (a match of a multiplayer map's, not Firefight's: its players are all
	on one team there, and a campaign's team 6 is its own) */
	if (!g_forge.initialized || !game_engine_running() || firefight_active())
		return NONE;
	other = our_team_index == FORGE_AI_TEAM_ALL_PLAYERS ? other_team_index : our_team_index;
	return forge_ai_players_team(other) ? FALSE : NONE;
}

/* the team an AI spawn plays on in this match: its own, or the players'
teams as the match has them (FORGE_AI_TEAM_*) */
static short forge_ai_match_team(short team)
{
	if (team != FORGE_AI_TEAM_ALL_PLAYERS && team != FORGE_AI_TEAM_RED && team != FORGE_AI_TEAM_BLUE)
		return team;
	if (!game_engine_running() || firefight_active())
		return _game_team_player;
	if (game_engine_has_teams() && team != FORGE_AI_TEAM_ALL_PLAYERS)
		return team == FORGE_AI_TEAM_RED ? 0 : 1;
	return FORGE_AI_TEAM_ALL_PLAYERS;
}

static short forge_ai_entry_team(struct forge_ai_spawn_entry const *entry, long variant)
{
	struct actor_variant_definition *definition;

	if (entry->team != _game_team_default)
		return forge_ai_match_team(entry->team);
	definition = (struct actor_variant_definition *)tag_get('actv', variant);
	if (definition->unit_reference.index != NONE)
		return unit_definition_get(definition->unit_reference.index)->unit.default_team;
	return entry->team;
}

/* the map's encounter with a firing position near a point, nearest first,
NONE with none */
static short forge_ai_nearest_encounter(
	struct encounter_definition const *encounters,
	long count,
	struct forge_ai_spawn_entry const *entry)
{
	short nearest = NONE;
	float nearest_distance = FORGE_AI_NEAR_FIRING_POSITION * FORGE_AI_NEAR_FIRING_POSITION;
	long e, p;

	for (e = 0; e < count; e++)
	{
		struct firing_position_definition const *positions =
			XBOX_POINTER(struct firing_position_definition const, encounters[e].firing_positions.address); /* AE hook */

		for (p = 0; p < encounters[e].firing_positions.count; p++)
		{
			float dx = positions[p].position.x - entry->x;
			float dy = positions[p].position.y - entry->y;
			float dz = positions[p].position.z - entry->z;
			float distance = dx * dx + dy * dy + dz * dz;

			if (distance < nearest_distance)
			{
				nearest = (short)e;
				nearest_distance = distance;
			}
		}
	}
	return nearest;
}

/* firing positions for a spawn on a map with none near it: on rings around
it, where there is ground an actor could reach in a straight line. The level's
structure is there as its AI starts (scenario_load) */
static void forge_ai_firing_positions_add(
	struct forge_ai_added_encounter *added,
	struct forge_ai_spawn_entry const *entry)
{
	static float const radii[] = { 0.0f, 3.0f, 6.0f, 10.0f };
	static short const counts[] = { 1, 6, 8, 10 };
	unsigned long flags = FLAG(_collision_test_structure_bit);
	short ring, spot;

	for (ring = 0; ring < (short)NUMBEROF(radii); ring++)
	{
		for (spot = 0; spot < counts[ring]; spot++)
		{
			float angle = (float)spot * (6.283185307f / (float)counts[ring]) + (float)ring * 0.4f;
			float x = entry->x + radii[ring] * (float)cos((double)angle);
			float y = entry->y + radii[ring] * (float)sin((double)angle);
			struct firing_position_definition *position;
			struct collision_result collision;
			struct location location;
			real_point3d start;
			real_vector3d vector;

			start.x = entry->x;
			start.y = entry->y;
			start.z = entry->z + 0.6f;
			vector.i = x - entry->x;
			vector.j = y - entry->y;
			vector.k = 0.0f;
			if (radii[ring] > 0.0f && collision_test_vector(flags, &start, &vector, NONE, &collision))
				continue;
			start.x = x;
			start.y = y;
			start.z = entry->z + 2.0f;
			vector.i = 0.0f;
			vector.j = 0.0f;
			vector.k = -6.0f;
			if (!collision_test_vector(flags, &start, &vector, NONE, &collision) ||
				collision.plane.n.k < 0.7f || collision.surface_index == NONE)
			{
				continue;
			}
			scenario_location_from_point(&location, &collision.point);
			if (location.cluster_index == NONE)
				continue;
			position = &added->firing_positions[added->firing_position_count++];
			memset(position, 0, sizeof(*position));
			position->position = collision.point;
			position->group_index = 0;
			position->cluster_index = location.cluster_index;
			position->surface_index = collision.surface_index;
		}
	}
}

/* the squad of an encounter with a starting location nearest a point: what a
new squad there copies its firing position groups and manner from */
static struct squad_definition const *forge_ai_nearest_squad(
	struct encounter_definition const *encounter,
	struct forge_ai_spawn_entry const *entry)
{
	struct squad_definition const *squads = XBOX_POINTER(struct squad_definition const, encounter->squads.address); /* AE hook */
	struct squad_definition const *nearest = encounter->squads.count > 0 ? &squads[0] : NULL;
	float nearest_distance = -1.0f;
	long s, l;

	for (s = 0; s < encounter->squads.count; s++)
	{
		struct actor_starting_location const *locations =
			XBOX_POINTER(struct actor_starting_location const, squads[s].starting_locations.address); /* AE hook */

		for (l = 0; l < squads[s].starting_locations.count; l++)
		{
			float dx = locations[l].position.x - entry->x;
			float dy = locations[l].position.y - entry->y;
			float dz = locations[l].position.z - entry->z;
			float distance = dx * dx + dy * dy + dz * dz;

			if (nearest_distance < 0.0f || distance < nearest_distance)
			{
				nearest = &squads[s];
				nearest_distance = distance;
			}
		}
	}
	return nearest;
}

static short forge_ai_palette_index(struct tag_reference *palette, long *count, long capacity, long variant)
{
	long i;

	for (i = 0; i < *count; i++)
	{
		if (palette[i].index == variant)
			return (short)i;
	}
	if (*count >= capacity)
		return NONE;
	memset(&palette[*count], 0, sizeof(palette[*count]));
	palette[*count].group_tag = 'actv';
	palette[*count].name = XBOX_ADDRESS(tag_get_name(variant)); /* AE hook */
	palette[*count].name_length = (long)strlen(tag_get_name(variant)); /* AE hook */
	palette[*count].index = variant;
	return (short)(*count)++;
}

/* an entry's weapon, as a starting location's flag bits carry it */
static byte forge_ai_location_weapon_bits(struct forge_ai_spawn_entry const *entry)
{
	long weapon = forge_ai_tag_need('weap', entry->weapon_path);
	short i;

	if (weapon == NONE)
		return 0;
	for (i = 0; i < s_built.weapon_count; i++)
	{
		if (s_built.weapons[i] == weapon)
			return (byte)((i + 1) << FORGE_AI_LOCATION_WEAPON_SHIFT);
	}
	if (s_built.weapon_count >= FORGE_AI_MAX_WEAPONS)
		return 0;
	s_built.weapons[s_built.weapon_count++] = weapon;
	return (byte)(s_built.weapon_count << FORGE_AI_LOCATION_WEAPON_SHIFT);
}

static void forge_ai_location_write(
	struct actor_starting_location *location,
	struct forge_ai_spawn_entry const *entry,
	short palette_index)
{
	location->position.x = entry->x;
	location->position.y = entry->y;
	location->position.z = entry->z;
	location->facing = entry->yaw;
	location->actor_variant_index = palette_index;
	location->flags = (byte)((location->flags & FORGE_AI_LOCATION_OWN_FLAGS) | forge_ai_location_weapon_bits(entry));
}

/* the spawns the level plays, NULL for the map's own AI as it stands */
static struct forge_ai_spawn_entry const *forge_ai_level_spawns(int *count, int *replace_own)
{
	if (g_forge_session_enabled && s_markers_scanned && g_forge.live_instance_count > 0)
	{
		if (!g_forge.edit_state.ai_spawns_edited)
			return NULL;
		*count = g_forge.edit_state.ai_spawn_count;
		*replace_own = g_forge.edit_state.replace_ai_spawns;
		return g_forge.edit_state.ai_spawns;
	}
	s_markers_scanned = 0;
	s_saved.applied = s_saved.fresh;
	s_saved.fresh = 0;
	if (!s_saved.applied)
		s_saved.actor_count = 0;
	if (!s_saved.applied || !s_saved.has_spawns)
		return NULL;
	*count = s_saved.count;
	*replace_own = s_saved.replace_own;
	return s_saved.entries;
}

/* the group's ranks the level has, one tag of each, as a spawn's squad's */
static void forge_ai_random_tags_need(int group)
{
	int i;

	for (i = 0; i < g_forge.catalog_count; i++)
	{
		struct forge_catalog_entry const *row = &g_forge.catalog[i];

		if (row->group_tag == FORGE_GROUP_AI_SPAWN && !forge_ai_row_is_random(row) && forge_ai_row_group(row) == group)
			forge_ai_tag_need('actv', row->tag_path);
	}
}

static void forge_ai_random_squad_add(
	struct squad_definition const *squad,
	struct forge_ai_spawn_entry const *entry,
	long *palette_count,
	long palette_capacity)
{
	struct forge_ai_random_squad *random = &s_built.randoms[s_built.random_count];
	int group = forge_ai_kind_group(forge_ai_kind_of(entry->actor_variant_path));
	int kinds[FORGE_AI_MAX_RANKS];
	long count, i;
	struct forge_ai_tag_instance *instances;

	forge_ai_random_tags_need(group);
	instances = forge_ai_tag_instances(&count);
	random->squad = squad;
	random->count = 0;
	for (i = 0; instances && i < count && random->count < FORGE_AI_MAX_RANKS; i++)
	{
		int kind, seen;

		if ((uint32_t)instances[i].group_tag != 'actv' || !instances[i].name || !instances[i].base_address)
			continue;
		kind = forge_ai_kind_of(XBOX_POINTER(char const, instances[i].name)); /* AE hook */
		if (kind < 0 || forge_ai_kind_group(kind) != group || forge_ai_tag_is_poor(XBOX_POINTER(char const, instances[i].name))) /* AE hook */
			continue;
		for (seen = 0; seen < random->count && kinds[seen] != kind; seen++)
		{
		}
		if (seen < random->count)
			continue;
		kinds[random->count] = kind;
		random->palette[random->count++] = forge_ai_palette_index(s_built.palette, palette_count, palette_capacity,
			instances[i].tag_index);
	}
	if (random->count > 1)
		s_built.random_count++;
}

/* the actor a squad's next unit is: a random spawn's any of its ranks, each as
likely (encounters.c) */
short forge_ai_squad_palette_index(void const *squad, short palette_index)
{
	int i;

	for (i = 0; i < s_built.random_count; i++)
	{
		if (s_built.randoms[i].squad == squad)
		{
			return s_built.randoms[i].palette[seed_random_range(get_global_random_seed_address(), 0,
				s_built.randoms[i].count)];
		}
	}
	return palette_index;
}

/* ---------- AI goals: PATROL VOLUME, ROUTE, HUNT, FOLLOW

A spawn with a GOAL has a squad, and an encounter, to itself, so what its
units walk and fight from is theirs alone. The engine does the walking where
it can: a squad's move positions are where its units go out of a fight
(action_alert.c), at random (a PATROL VOLUME's, "moving randomly", which every
Forge squad was already) or in order (a ROUTE's, looping or back and forth),
and a fight, or being seen, stops it as it stops any squad's; out of the fight
they go on from where they are. Its firing positions are where they fight
from: a PATROL VOLUME's are on the ground in it and its PURSUIT RANGE, which
keeps their fight there (and one who runs out past it is sent back); the rest
have theirs on rings round where they are, laid again as they go.

A ROUTE has a leader: the squad's first unit walks it (forge_ai_route_next_position),
the others follow it, as FOLLOW's units follow a player (forge_ai_goals_update);
another takes over when it dies. HUNT's units go for their player; FOLLOW's
keep near theirs. Out of a fight only: once they fight, the engine has them.

Where the volume and the route are: Forge's own (live instances), or, as a
match of a saved map starts, before its markers are made, the saved map's
VOLUME| and AI_ROUTE| lines (forge_ai_saved_parse). */

#define FORGE_AI_GOAL_MOVE_POSITIONS 32
#define FORGE_AI_GOAL_CANDIDATES 256
/* (a route's units' leader is followed from this far, to this far) */
#define FORGE_AI_FOLLOWER_START 4.0f
#define FORGE_AI_FOLLOWER_DISTANCE 2.5f
/* (a goal unit this near its point, held from it - a squadmate on it - has
reached it) */
#define FORGE_AI_ARRIVE_DISTANCE 2.0f
/* (rings of firing positions laid again once their middle is this far behind) */
#define FORGE_AI_GOAL_RING_MOVE 6.0f
/* (a squad's alarm: once one of its units fights, is hurt or dies, all of them
are alert its ALERT TIME after the last of it - the route held, the patrol and
the rest kept - and those not fighting go to within this far of where it was) */
#define FORGE_AI_GOAL_ALARM_JOIN 4.0f
/* (the engine's encounters, encounters.c's MAXIMUM_ENCOUNTERS) */
#define FORGE_AI_ENGINE_ENCOUNTERS 128

extern boolean death_volume_column(real_point3d const *point, real height);
static struct forge_ai_spawn_entry *forge_ai_entry_of(struct forge_live_instance const *inst);
static int forge_ai_over_ground(float x, float y, float z, float height, union real_point3d *point);

struct forge_ai_goal_volume
{
	int number;
	int shape;
	float position[3];
	float yaw, pitch, roll;
	float size[3];
	float pursuit;
};

struct forge_ai_goal_route
{
	int number;
	int behavior;
	int count;
	float points[FORGE_AI_ROUTE_MAX_POINTS][3];
	short waits[FORGE_AI_ROUTE_MAX_POINTS];
	short orders[FORGE_AI_ROUTE_MAX_POINTS];
};

/* a saved map's AI PATROL volumes and routes, as its text has them */
static struct
{
	struct forge_ai_goal_volume volumes[FORGE_VOLUME_PATROL_MAX];
	int volume_count;
	struct forge_ai_goal_route routes[FORGE_AI_ROUTE_MAX];
	int route_count;
} s_saved_goals;

/* a GOAL's squad as the level plays it */
struct forge_ai_goal_slot
{
	uint8_t goal, who, start, follow;
	short encounter_index;
	struct squad_definition *squad;
	struct firing_position_definition *firing;
	struct forge_ai_goal_volume volume;
	int behavior;
	int route_started;
	/* (where the route's walk is: the point its leader walks to, NONE before
	it sets out, and which way along it, so a new leader, or the leader after
	a fight, goes on from there) */
	short route_target;
	short route_forward;
	long leader_unit;
	/* (its alarm: alert until this tick (0: none), from about here; how many
	units it had alive last update, a death sounding it) */
	long alarm_until;
	float alarm_point[3];
	int alive_count;
	int alive_now;
	boolean alarm_seen;
	/* (GO TO PLAYER: going for a player in its range, this tick: at a run) */
	long chasing_until;
	/* (its ALERT TIME, ticks; whether its team is a players' - red, blue, all
	players - whose GO TO PLAYER stops STOP DISTANCE short) */
	long alert_ticks;
	boolean friendly;
	/* (the route's leader: where it was, since when, and how often it was
	sent on again from there, to tell it is stuck) */
	float stuck_point[3];
	long stuck_since;
	int stuck_tries;
	float ring_center[3];
	/* (where its units are, this update: the rings' middle) */
	float sum[3];
	int sum_count;
};

static struct forge_ai_goal_slot s_goals[FORGE_AI_MAX_ADDED_ENCOUNTERS];
static int s_goal_count;
static short s_goal_of_encounter[FORGE_AI_ENGINE_ENCOUNTERS];

/* (the level's goals gone, with what it built) */
static void forge_ai_goals_reset(void)
{
	s_goal_count = 0;
	memset(s_goal_of_encounter, 0xFF, sizeof(s_goal_of_encounter));
}

struct forge_ai_ground
{
	real_point3d point;
	long surface_index;
	short cluster_index;
};

/* the saved map's goal lines (forge_ai_saved_parse) */
static void forge_ai_saved_goals_parse(char const *text)
{
	char const *cur;

	memset(&s_saved_goals, 0, sizeof(s_saved_goals));
	for (cur = text; (cur = strstr(cur, "VOLUME|")) != NULL; cur += 7)
	{
		struct forge_ai_goal_volume volume;
		int effect = 0, map_index = 0, flags = 0;
		float damage = 0.0f;

		/* ("AI_..." or a line's start alone) */
		if (cur > text && cur[-1] != '\n' && cur[-1] != '\r')
			continue;
		memset(&volume, 0, sizeof(volume));
		if (sscanf(cur + 7, "%d|%d|%f|%f|%f|%f|%f|%f|%f|%f|%f|%f|%d|%d", &volume.shape, &effect, &damage,
				&volume.position[0], &volume.position[1], &volume.position[2], &volume.yaw, &volume.pitch, &volume.roll,
				&volume.size[0], &volume.size[1], &volume.size[2], &map_index, &flags) < 14 ||
			effect != _forge_volume_ai_patrol || FORGE_VOLUME_PATROL_NUMBER(flags) == 0 ||
			s_saved_goals.volume_count >= FORGE_VOLUME_PATROL_MAX)
		{
			continue;
		}
		volume.number = FORGE_VOLUME_PATROL_NUMBER(flags);
		volume.pursuit = damage;
		if (volume.shape == _forge_volume_map_box)
			volume.shape = _forge_volume_box;
		s_saved_goals.volumes[s_saved_goals.volume_count++] = volume;
	}
	for (cur = text; (cur = strstr(cur, "AI_ROUTE|")) != NULL; cur += 9)
	{
		float x = 0.0f, y = 0.0f, z = 0.0f, yaw = 0.0f;
		unsigned usage = 0;
		struct forge_ai_goal_route *route = NULL;
		int number, order, k, slot;

		/* (%g: scanf's %f, but the AI_SPAWN line's is the one the tests read) */
		if (sscanf(cur + 9, "%g|%g|%g|%g|%u", &x, &y, &z, &yaw, &usage) != 5)
			continue;
		number = FORGE_AI_ROUTE_NUMBER(usage);
		order = FORGE_AI_ROUTE_ORDER(usage);
		if (number == 0)
			continue;
		for (k = 0; k < s_saved_goals.route_count && !route; k++)
		{
			if (s_saved_goals.routes[k].number == number)
				route = &s_saved_goals.routes[k];
		}
		if (!route)
		{
			if (s_saved_goals.route_count >= FORGE_AI_ROUTE_MAX)
				continue;
			route = &s_saved_goals.routes[s_saved_goals.route_count++];
			memset(route, 0, sizeof(*route));
			route->number = number;
			route->behavior = FORGE_AI_ROUTE_BEHAVIOR(usage);
		}
		if (route->count >= FORGE_AI_ROUTE_MAX_POINTS)
			continue;
		/* (in their order) */
		for (slot = route->count; slot > 0 && route->orders[slot - 1] > order; slot--)
		{
			memcpy(route->points[slot], route->points[slot - 1], sizeof(route->points[0]));
			route->waits[slot] = route->waits[slot - 1];
			route->orders[slot] = route->orders[slot - 1];
		}
		route->points[slot][0] = x;
		route->points[slot][1] = y;
		route->points[slot][2] = z;
		route->waits[slot] = (short)forge_ai_route_wait_seconds((int16_t)(uint16_t)usage);
		route->orders[slot] = (short)order;
		route->count++;
	}
}

/* an AI PATROL volume of this number: Forge's, else the saved map's */
static int forge_ai_goal_volume_get(int number, struct forge_ai_goal_volume *out)
{
	int32_t live_idx = forge_volume_patrol_find(number);
	int k;

	if (live_idx >= 0)
	{
		struct forge_live_instance const *inst = &g_forge.live_instances[live_idx];
		struct forge_volume const *volume = forge_volume_get(live_idx);

		memset(out, 0, sizeof(*out));
		out->number = number;
		out->shape = volume->shape == _forge_volume_map_box ? _forge_volume_box : volume->shape;
		out->position[0] = inst->x;
		out->position[1] = inst->y;
		out->position[2] = inst->z;
		out->yaw = inst->yaw;
		out->pitch = inst->pitch;
		out->roll = inst->roll;
		memcpy(out->size, volume->size, sizeof(out->size));
		out->pursuit = volume->damage;
		return 1;
	}
	for (k = 0; k < s_saved_goals.volume_count; k++)
	{
		if (s_saved_goals.volumes[k].number == number)
		{
			*out = s_saved_goals.volumes[k];
			return 1;
		}
	}
	return 0;
}

/* a route of this number: Forge's, else the saved map's */
static int forge_ai_goal_route_get(int number, struct forge_ai_goal_route *out)
{
	int32_t points[FORGE_AI_ROUTE_MAX_POINTS];
	int count = forge_ai_route_points(number, points, FORGE_AI_ROUTE_MAX_POINTS);
	int k;

	if (count > 0)
	{
		memset(out, 0, sizeof(*out));
		out->number = number;
		out->count = count;
		out->behavior = FORGE_AI_ROUTE_BEHAVIOR(g_forge.live_instances[points[0]].team_designator);
		for (k = 0; k < count; k++)
		{
			struct forge_live_instance const *inst = &g_forge.live_instances[points[k]];

			out->points[k][0] = inst->x;
			out->points[k][1] = inst->y;
			out->points[k][2] = inst->z;
			out->waits[k] = (short)forge_ai_route_wait_seconds(inst->team_designator);
			out->orders[k] = (short)k;
		}
		return 1;
	}
	for (k = 0; k < s_saved_goals.route_count; k++)
	{
		if (s_saved_goals.routes[k].number == number && s_saved_goals.routes[k].count > 0)
		{
			*out = s_saved_goals.routes[k];
			return 1;
		}
	}
	return 0;
}

/* the floors under (x, y) between top and bottom, highest first, that an actor
can stand on: level, in the map, out of its kill volumes */
static int forge_ai_ground_column(float x, float y, float top, float bottom, struct forge_ai_ground *out, int max)
{
	unsigned long const flags = FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit);
	real_point3d start;
	int count = 0, tries;

	start.x = x;
	start.y = y;
	start.z = top;
	for (tries = 0; tries < 6 && count < max && start.z > bottom; tries++)
	{
		struct collision_result collision;
		real_vector3d vector;
		struct location location;

		vector.i = 0.0f;
		vector.j = 0.0f;
		vector.k = bottom - start.z;
		memset(&collision, 0, sizeof(collision));
		if (!collision_test_vector(flags, &start, &vector, NONE, &collision))
			break;
		start.z = collision.point.z - 0.3f;
		if (collision.plane.n.k < 0.7f || collision.surface_index == NONE)
			continue;
		scenario_location_from_point(&location, &collision.point);
		if (location.cluster_index == NONE || death_volume_column(&collision.point, 2.0f))
			continue;
		out[count].point = collision.point;
		out[count].surface_index = collision.surface_index;
		out[count].cluster_index = location.cluster_index;
		count++;
	}
	return count;
}

/* the walkable ground at a point: just under it, else (it is under a hill,
or over a drop) the ground nearest it above or below */
static int forge_ai_point_ground(float x, float y, float z, struct forge_ai_ground *out)
{
	struct forge_ai_ground hits[6];
	float best_distance = 1.0e30f;
	int count, k, best = -1;

	if (forge_ai_ground_column(x, y, z + 1.5f, z - 12.0f, hits, 1) == 1)
	{
		*out = hits[0];
		return 1;
	}
	count = forge_ai_ground_column(x, y, z + 30.0f, z - 60.0f, hits, 6);
	for (k = 0; k < count; k++)
	{
		float distance = (float)fabs((double)(hits[k].point.z - z));

		if (distance < best_distance)
		{
			best_distance = distance;
			best = k;
		}
	}
	if (best < 0)
		return 0;
	*out = hits[best];
	return 1;
}

int forge_ai_route_ground(float x, float y, float z, float *ground_z)
{
	struct forge_ai_ground ground;

	if (!global_scenario_try_and_get() || !forge_ai_point_ground(x, y, z, &ground))
		return 0;
	*ground_z = ground.point.z;
	return 1;
}

int forge_ai_route_place_toward(float const from[3], float const to[3], float out[3])
{
	float dx = to[0] - from[0], dy = to[1] - from[1], dz = to[2] - from[2];
	float length = (float)sqrt((double)(dx * dx + dy * dy));
	int steps = (int)(length / 0.5f) + 1, i;
	struct forge_ai_ground ground;

	if (global_scenario_try_and_get())
	{
		/* (from the spot asked for back toward the point it is placed from:
		the first with ground under it that point can see along the ground) */
		for (i = steps; i >= 0; i--)
		{
			float t = (float)i / (float)steps;
			real_point3d start;
			real_vector3d vector;
			struct collision_result collision;

			if (!forge_ai_point_ground(from[0] + dx * t, from[1] + dy * t, from[2] + dz * t, &ground))
				continue;
			start.x = from[0];
			start.y = from[1];
			start.z = from[2] + 1.0f;
			vector.i = ground.point.x - start.x;
			vector.j = ground.point.y - start.y;
			vector.k = ground.point.z + 1.0f - start.z;
			memset(&collision, 0, sizeof(collision));
			if (i > 0 && collision_test_vector(FLAG(_collision_test_structure_bit) |
					FLAG(_collision_test_front_facing_surfaces_bit) | FLAG(_collision_test_back_facing_surfaces_bit),
					&start, &vector, NONE, &collision))
			{
				continue;
			}
			out[0] = ground.point.x;
			out[1] = ground.point.y;
			out[2] = ground.point.z;
			return 1;
		}
	}
	out[0] = from[0];
	out[1] = from[1];
	out[2] = from[2];
	return 0;
}

/* the ground in a volume (each side `margin` further out), spread: at most
`max` of it, each as far from the others as can be */
/* whether an object stands on a spot of the ground (a Forge block over
the floor): the paths know only the structure, so a unit sent there walks into
it and pushes for good. A ray down onto it and one up from it, both sides of
the object's surfaces */
static boolean forge_ai_ground_covered(real_point3d const *point)
{
	unsigned long const flags = FLAG(_collision_test_objects_bit) | FLAG(_collision_test_objects_scenery_bit) |
		FLAG(_collision_test_objects_machines_bit) | FLAG(_collision_test_objects_vehicles_bit) |
		FLAG(_collision_test_objects_light_fixtures_bit) | FLAG(_collision_test_objects_controls_bit);
	struct collision_result collision;
	real_point3d start;
	real_vector3d vector;

	start = *point;
	start.z += 1.8f;
	vector.i = vector.j = 0.0f;
	vector.k = -1.65f;
	memset(&collision, 0, sizeof(collision));
	if (collision_test_vector(flags, &start, &vector, NONE, &collision))
		return TRUE;
	start = *point;
	start.z += 0.15f;
	vector.k = 1.65f;
	memset(&collision, 0, sizeof(collision));
	return collision_test_vector(flags, &start, &vector, NONE, &collision);
}

static int forge_ai_volume_ground(struct forge_ai_goal_volume const *volume, float margin, struct forge_ai_ground *out,
	int max)
{
	static struct forge_ai_ground candidates[FORGE_AI_GOAL_CANDIDATES];
	static float nearest[FORGE_AI_GOAL_CANDIDATES];
	float reach = 0.5f * (float)sqrt((double)(volume->size[0] * volume->size[0] + volume->size[1] * volume->size[1] +
		volume->size[2] * volume->size[2])) + margin;
	float step = reach * 2.0f / 16.0f;
	float gx, gy;
	int count = 0, chosen = 0, k;

	if (step < 1.25f)
		step = 1.25f;
	for (gx = -reach; gx <= reach + 0.01f; gx += step)
	{
		for (gy = -reach; gy <= reach + 0.01f; gy += step)
		{
			struct forge_ai_ground column[4];
			int floors, f;

			if (gx * gx + gy * gy > reach * reach)
				continue;
			floors = forge_ai_ground_column(volume->position[0] + gx, volume->position[1] + gy,
				volume->position[2] + reach + 0.5f, volume->position[2] - reach - 0.5f, column, 4);
			for (f = 0; f < floors && count < FORGE_AI_GOAL_CANDIDATES; f++)
			{
				float raised[3];

				raised[0] = column[f].point.x;
				raised[1] = column[f].point.y;
				raised[2] = column[f].point.z + 0.3f;
				if (forge_volume_shape_contains(volume->shape, volume->position, volume->yaw, volume->pitch,
						volume->roll, volume->size, margin, raised) &&
					!forge_ai_ground_covered(&column[f].point))
				{
					candidates[count++] = column[f];
				}
			}
		}
	}
	if (count == 0)
		return 0;
	/* (the first nearest its middle; then each the farthest from those chosen) */
	{
		int best = 0;
		float best_distance = 1.0e30f;

		for (k = 0; k < count; k++)
		{
			float dx = candidates[k].point.x - volume->position[0], dy = candidates[k].point.y - volume->position[1];

			if (dx * dx + dy * dy < best_distance)
			{
				best_distance = dx * dx + dy * dy;
				best = k;
			}
		}
		out[chosen++] = candidates[best];
		for (k = 0; k < count; k++)
			nearest[k] = distance_squared3d(&candidates[k].point, &candidates[best].point);
	}
	while (chosen < max && chosen < count)
	{
		int best = -1;
		float best_distance = 0.25f;

		for (k = 0; k < count; k++)
		{
			if (nearest[k] > best_distance)
			{
				best_distance = nearest[k];
				best = k;
			}
		}
		if (best < 0)
			break;
		out[chosen++] = candidates[best];
		for (k = 0; k < count; k++)
		{
			float d = distance_squared3d(&candidates[k].point, &candidates[best].point);

			if (d < nearest[k])
				nearest[k] = d;
		}
	}
	return chosen;
}

/* a firing position on the ground there */
static void forge_ai_firing_position_set(struct firing_position_definition *position, struct forge_ai_ground const *ground)
{
	memset(position, 0, sizeof(*position));
	position->position = ground->point;
	position->group_index = 0;
	position->cluster_index = ground->cluster_index;
	position->surface_index = ground->surface_index;
}

/* `count` firing positions, all of them filled (the engine reads them by
index, so a squad's never shrink): those found, again in turn after them, or
the one point when none was found */
static void forge_ai_firing_positions_fill(struct firing_position_definition *positions, int count,
	struct forge_ai_ground const *found, int found_count, real_point3d const *fallback)
{
	int k;

	for (k = 0; k < count; k++)
	{
		if (found_count > 0)
		{
			forge_ai_firing_position_set(&positions[k], &found[k % found_count]);
		}
		else
		{
			struct location location;

			memset(&positions[k], 0, sizeof(positions[k]));
			positions[k].position = *fallback;
			scenario_location_from_point(&location, fallback);
			positions[k].cluster_index = location.cluster_index;
			positions[k].surface_index = NONE;
		}
	}
}

/* rings of firing positions round a point: the ground on them an actor
could walk to in a straight line from it */
static int forge_ai_rings_ground(float x, float y, float z, struct forge_ai_ground *out, int max)
{
	static float const radii[] = { 0.0f, 3.0f, 6.0f, 10.0f };
	static short const counts[] = { 1, 6, 8, 10 };
	unsigned long flags = FLAG(_collision_test_structure_bit);
	int found = 0, ring, spot;

	for (ring = 0; ring < (int)NUMBEROF(radii); ring++)
	{
		for (spot = 0; spot < counts[ring] && found < max; spot++)
		{
			float angle = (float)spot * (6.283185307f / (float)counts[ring]) + (float)ring * 0.4f;
			float px = x + radii[ring] * (float)cos((double)angle);
			float py = y + radii[ring] * (float)sin((double)angle);
			struct collision_result collision;
			real_point3d start;
			real_vector3d vector;

			start.x = x;
			start.y = y;
			start.z = z + 0.6f;
			vector.i = px - x;
			vector.j = py - y;
			vector.k = 0.0f;
			if (radii[ring] > 0.0f && collision_test_vector(flags, &start, &vector, NONE, &collision))
				continue;
			found += forge_ai_ground_column(px, py, z + 2.0f, z - 4.0f, &out[found], 1);
		}
	}
	return found;
}

static void forge_ai_goal_rings_lay(struct forge_ai_goal_slot *slot, float x, float y, float z)
{
	struct forge_ai_ground found[FORGE_AI_FIRING_POSITIONS_PER_SPAWN];
	real_point3d middle;
	int count = forge_ai_rings_ground(x, y, z, found, FORGE_AI_FIRING_POSITIONS_PER_SPAWN);

	middle.x = x;
	middle.y = y;
	middle.z = z;
	forge_ai_firing_positions_fill(slot->firing, FORGE_AI_FIRING_POSITIONS_PER_SPAWN, found, count, &middle);
	slot->ring_center[0] = x;
	slot->ring_center[1] = y;
	slot->ring_center[2] = z;
}

static void forge_ai_move_position_set(struct move_position_definition *move, real_point3d const *point,
	long surface_index, short cluster_index, float wait_lower, float wait_upper)
{
	memset(move, 0, sizeof(*move));
	move->position = *point;
	move->weight = 1.0f;
	move->time_lower_bound = wait_lower;
	move->time_upper_bound = wait_upper;
	move->animation_reference_index = NONE;
	move->sequence_id = 0;
	move->cluster_index = cluster_index;
	move->surface_index = surface_index;
}

/* whether a spawn plays its goal: one with nothing to go by (a missing volume
or route) has none */
static int forge_ai_entry_goal_plays(struct forge_ai_spawn_entry const *entry)
{
	struct forge_ai_goal_volume volume;
	struct forge_ai_goal_route route;

	switch (entry->goal)
	{
	case _forge_ai_goal_patrol:
		return forge_ai_goal_volume_get(entry->goal_link, &volume);
	case _forge_ai_goal_route:
		return forge_ai_goal_route_get(entry->goal_link, &route);
	case _forge_ai_goal_hunt:
		return 1;
	default:
		return 0;
	}
}

/* a GOAL's squad: its move positions (`moves`, room for
FORGE_AI_GOAL_MOVE_POSITIONS), its state and its encounter's firing positions
(room for FORGE_AI_FIRING_POSITIONS_PER_SPAWN) */
static void forge_ai_goal_squad_make(struct forge_ai_spawn_entry const *entry, short encounter_index,
	struct squad_definition *squad, struct move_position_definition *moves, struct firing_position_definition *firing)
{
	struct forge_ai_goal_slot *slot;
	real_point3d spawn_point;
	int move_count = 0, k;

	if (s_goal_count >= FORGE_AI_MAX_ADDED_ENCOUNTERS || encounter_index < 0 ||
		encounter_index >= FORGE_AI_ENGINE_ENCOUNTERS)
	{
		return;
	}
	slot = &s_goals[s_goal_count];
	memset(slot, 0, sizeof(*slot));
	slot->goal = entry->goal;
	slot->who = entry->goal_who;
	slot->start = entry->goal_start;
	slot->follow = entry->goal_follow;
	slot->alert_ticks = (long)(entry->goal_alert ? entry->goal_alert : FORGE_AI_GOAL_ALERT_DEFAULT) * 30;
	slot->friendly = entry->team == FORGE_AI_TEAM_RED || entry->team == FORGE_AI_TEAM_BLUE ||
		entry->team == FORGE_AI_TEAM_ALL_PLAYERS;
	slot->encounter_index = encounter_index;
	slot->squad = squad;
	slot->firing = firing;
	slot->leader_unit = NONE;
	slot->route_target = NONE;
	slot->route_forward = 1;
	spawn_point.x = entry->x;
	spawn_point.y = entry->y;
	spawn_point.z = entry->z;
	switch (entry->goal)
	{
	case _forge_ai_goal_patrol:
	{
		struct forge_ai_ground found[FORGE_AI_GOAL_MOVE_POSITIONS];
		int count;

		if (!forge_ai_goal_volume_get(entry->goal_link, &slot->volume))
			return;
		count = forge_ai_volume_ground(&slot->volume, 0.0f, found, FORGE_AI_GOAL_MOVE_POSITIONS);
		for (k = 0; k < count; k++)
			forge_ai_move_position_set(&moves[move_count++], &found[k].point, found[k].surface_index,
				found[k].cluster_index, 1.0f, 4.0f);
		/* (their fight: the volume's ground and its PURSUIT RANGE's) */
		count = forge_ai_volume_ground(&slot->volume, slot->volume.pursuit, found, FORGE_AI_FIRING_POSITIONS_PER_SPAWN);
		forge_ai_firing_positions_fill(firing, FORGE_AI_FIRING_POSITIONS_PER_SPAWN, found, count, &spawn_point);
		if (count == 0)
			forge_ai_goal_rings_lay(slot, entry->x, entry->y, entry->z);
		squad->initial_state = squad->default_state = 7; /* (moving randomly) */
		error(_error_silent, "forge ai: a spawn patrols volume #%d: %d points on its ground, %d to fight from",
			entry->goal_link, move_count, count);
		break;
	}
	case _forge_ai_goal_route:
	{
		struct forge_ai_goal_route route;

		if (!forge_ai_goal_route_get(entry->goal_link, &route))
			return;
		slot->behavior = route.behavior;
		for (k = 0; k < route.count && move_count < FORGE_AI_GOAL_MOVE_POSITIONS; k++)
		{
			struct forge_ai_ground ground;
			real_point3d point;
			struct location location;

			point.x = route.points[k][0];
			point.y = route.points[k][1];
			point.z = route.points[k][2];
			if (forge_ai_point_ground(point.x, point.y, point.z, &ground))
			{
				forge_ai_move_position_set(&moves[move_count++], &ground.point, ground.surface_index,
					ground.cluster_index, (float)route.waits[k], (float)route.waits[k]);
			}
			else
			{
				scenario_location_from_point(&location, &point);
				forge_ai_move_position_set(&moves[move_count++], &point, NONE, location.cluster_index,
					(float)route.waits[k], (float)route.waits[k]);
			}
		}
		/* (round the spawn, where they stand: their fight is there until
		they walk on, the rings following them; the route's first point may be
		far off) */
		forge_ai_goal_rings_lay(slot, entry->x, entry->y, entry->z);
		/* (4: looping, 5: back and forth; STAY AT END loops, and stops at the
		end: forge_ai_route_next_position) */
		squad->initial_state = squad->default_state = route.behavior == _forge_ai_route_ping_pong ? 5 : 4;
		error(_error_silent, "forge ai: a spawn walks route #%d: %d points, %s", entry->goal_link, move_count,
			forge_ai_route_behavior_name(route.behavior));
		break;
	}
	default:
		forge_ai_goal_rings_lay(slot, entry->x, entry->y, entry->z);
		break;
	}
	squad->move_positions.address = XBOX_ADDRESS(move_count > 0 ? moves : NULL); /* AE hook */
	squad->move_positions.count = move_count;
	s_goal_of_encounter[encounter_index] = (short)s_goal_count;
	s_goal_count++;
}

static struct forge_ai_goal_slot *forge_ai_goal_of_actor(struct actor_datum const *actor)
{
	long encounter;
	short slot;

	if (!actor || actor->meta.swarm || actor->meta.encounter_index == NONE || s_goal_count == 0)
		return NULL;
	encounter = DATUM_INDEX_TO_ABSOLUTE_INDEX(actor->meta.encounter_index);
	if (encounter < 0 || encounter >= FORGE_AI_ENGINE_ENCOUNTERS)
		return NULL;
	slot = s_goal_of_encounter[encounter];
	return slot >= 0 && slot < s_goal_count ? &s_goals[slot] : NULL;
}

/* whether a goal's squad is alarmed (forge_ai_goals_update) */
static boolean forge_ai_goal_alarmed(struct forge_ai_goal_slot const *slot)
{
	return slot->alarm_until != 0 && game_time_get() < slot->alarm_until;
}

/* the route's next point from `at` the way it goes (`*forward`, turned at a
PING PONG's ends), NONE past a STAY AT END's last */
static short forge_ai_route_step(struct forge_ai_goal_slot const *slot, short at, short count, short *forward)
{
	if (count <= 1)
		return NONE;
	switch (slot->behavior)
	{
	case _forge_ai_route_ping_pong:
		if (*forward && at >= count - 1)
			*forward = 0;
		else if (!*forward && at <= 0)
			*forward = 1;
		return (short)(*forward ? at + 1 : at - 1);
	case _forge_ai_route_stay_at_end:
		*forward = 1;
		return at >= count - 1 ? NONE : (short)(at + 1);
	default:
		*forward = 1;
		return (short)((at + 1) % count);
	}
}

static short forge_ai_patrol_point_pick(struct forge_ai_goal_slot const *slot, real_point3d const *at);

/* each ROUTE unit's own walk along its route: the point it goes to and
which way it goes (a PING PONG's), by the actor (its unit: a slot used again
starts again) */
static struct
{
	long unit_index;
	short target;
	short forward;
	boolean started;
} s_route_walks[MAXIMUM_ACTORS];

static void *forge_ai_route_walk_of(long actor_index, struct actor_datum const *actor)
{
	int index = DATUM_INDEX_TO_ABSOLUTE_INDEX(actor_index);

	if (index < 0 || index >= MAXIMUM_ACTORS)
		return NULL;
	if (s_route_walks[index].unit_index != actor->meta.unit_index)
	{
		s_route_walks[index].unit_index = actor->meta.unit_index;
		s_route_walks[index].target = NONE;
		s_route_walks[index].forward = 1;
		s_route_walks[index].started = FALSE;
	}
	return &s_route_walks[index];
}

/* a route unit that cannot reach its point: on to the next, its walk's own */
static void forge_ai_route_walk_skip(struct forge_ai_goal_slot const *slot, long actor_index,
	struct actor_datum const *actor)
{
	int index = DATUM_INDEX_TO_ABSOLUTE_INDEX(actor_index);
	short count, next;

	if (!slot || !slot->squad || !forge_ai_route_walk_of(actor_index, actor) || s_route_walks[index].target == NONE)
		return;
	count = (short)MIN(slot->squad->move_positions.count, FORGE_AI_GOAL_MOVE_POSITIONS);
	next = forge_ai_route_step(slot, s_route_walks[index].target, count, &s_route_walks[index].forward);
	error(_error_silent, "forge ai: a route unit cannot reach point %d: on to %d", (int)s_route_walks[index].target,
		(int)next);
	if (next != NONE && next >= 0 && next < count)
		s_route_walks[index].target = next;
}

/* (from action_alert.c's next position) where a ROUTE's unit goes next. Each
walks the route on its own, as a unit alone does (a squad following its
leader crowded onto one point, and stood there in each other's way): the first
point the first time; on from the point reached; and the point it was going to
when it starts again after a fight, so none goes back to the first. None while
the squad is alarmed, none past the end of a STAY AT END route */
short forge_ai_route_next_position(long actor_index, short move_position_order, short current_position_index,
	boolean *handled)
{
	struct actor_datum *actor = actor_try_and_get(actor_index);
	struct forge_ai_goal_slot *slot = forge_ai_goal_of_actor(actor);
	struct move_position_definition const *moves;
	short count, next;
	int index;

	(void)move_position_order;
	*handled = FALSE;
	/* (a PATROL's next point: away from where its squadmates stand and go,
	or they crowd one spot and stand in each other's way) */
	if (slot && slot->goal == _forge_ai_goal_patrol && slot->squad && actor)
	{
		next = forge_ai_patrol_point_pick(slot, &actor->input.position.body_position);
		*handled = next != NONE;
		return next;
	}
	if (!slot || slot->goal != _forge_ai_goal_route || !slot->squad || !forge_ai_route_walk_of(actor_index, actor))
		return NONE;
	*handled = TRUE;
	index = DATUM_INDEX_TO_ABSOLUTE_INDEX(actor_index);
	count = (short)MIN(slot->squad->move_positions.count, FORGE_AI_GOAL_MOVE_POSITIONS);
	if (count <= 0 || forge_ai_goal_alarmed(slot))
		return NONE;
	moves = XBOX_POINTER(struct move_position_definition const, slot->squad->move_positions.address); /* AE hook */
	if (!s_route_walks[index].started)
	{
		s_route_walks[index].started = TRUE;
		s_route_walks[index].forward = 1;
		s_route_walks[index].target = 0;
		return 0;
	}
	/* (starting again: on to where its walk was going, unless that is where
	it stands) */
	if (current_position_index == NONE || current_position_index != s_route_walks[index].target)
	{
		short target = s_route_walks[index].target;

		if (target == NONE || target < 0 || target >= count)
			return NONE;
		if (distance_squared3d(&actor->input.position.body_position, &moves[target].position) >
			FORGE_AI_ARRIVE_DISTANCE * FORGE_AI_ARRIVE_DISTANCE)
		{
			return target;
		}
		current_position_index = target;
	}
	next = forge_ai_route_step(slot, current_position_index, count, &s_route_walks[index].forward);
	if (next == NONE || next < 0 || next >= count)
	{
		/* (STAY AT END: there for good) */
		return NONE;
	}
	s_route_walks[index].target = next;
	return next;
}

/* (action_alert.c's control) whether a goal's unit goes at a fight's pace out
of one: its SPEED's RUN, or its squad alarmed */
boolean forge_ai_actor_runs(long actor_index)
{
	struct forge_ai_goal_slot const *slot = forge_ai_goal_of_actor(actor_try_and_get(actor_index));

	return slot != NULL && (FORGE_AI_GOAL_RUNS(slot->who) || forge_ai_goal_alarmed(slot) ||
		game_time_get() < slot->chasing_until);
}

/* (actor_moving.c) whether an actor that finds no path walks a straight line
there, as Firefight's do: a goal's */
boolean forge_ai_actor_walks_straight(long actor_index)
{
	struct forge_ai_goal_slot *slot = forge_ai_goal_of_actor(actor_try_and_get(actor_index));

	return slot != NULL;
}

/* the nearest living player of a goal's (any, red, blue), NONE for none */
static long forge_ai_goal_player(int who, real_point3d const *from, real_point3d *where)
{
	struct data_iterator iterator;
	struct player_datum *player;
	long best = NONE;
	float best_distance = 1.0e30f;
	boolean teams = game_engine_running() && game_engine_has_teams();

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct unit_datum *unit = (struct unit_datum *)object_try_and_get_and_verify_type(player->unit_index,
			_object_mask_unit);
		union real_point3d origin;
		float distance;

		if (!unit || TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
			continue;
		if (teams && who != _forge_ai_goal_who_any && player->team_index != (who == _forge_ai_goal_who_red ? 0 : 1))
			continue;
		object_get_origin(player->unit_index, &origin);
		distance = distance_squared3d((real_point3d const *)&origin, from);
		if (distance < best_distance)
		{
			best_distance = distance;
			best = player->unit_index;
			where->x = origin.x;
			where->y = origin.y;
			where->z = origin.z;
		}
	}
	return best;
}

/* an actor sent toward `target` (on the ground there), to stop `stop` short of
it, a step aside by its place in the squad */
static void forge_ai_goal_move_toward(long actor_index, struct actor_datum *actor, real_point3d const *target,
	float stop, int place)
{
	real_point3d const *from = &actor->input.position.body_position;
	float dx = from->x - target->x, dy = from->y - target->y;
	float length = (float)sqrt((double)(dx * dx + dy * dy));
	struct forge_ai_ground ground;
	real_point3d destination;
	long surface_index;

	if (length < 0.01f)
	{
		dx = 1.0f;
		dy = 0.0f;
		length = 1.0f;
	}
	dx /= length;
	dy /= length;
	destination.x = target->x + dx * stop - dy * (float)((place % 5) - 2) * 1.2f * (stop > 0.0f ? 1.0f : 0.0f);
	destination.y = target->y + dy * stop + dx * (float)((place % 5) - 2) * 1.2f * (stop > 0.0f ? 1.0f : 0.0f);
	destination.z = target->z;
	/* (the engine paths only to a point with its ground's surface: with
	NONE it drops the order at once, and the actor stands, turned to it. No
	ground there: its own surface, so the path it cannot find becomes the
	straight walk goal actors have, actor_moving.c) */
	if (forge_ai_point_ground(destination.x, destination.y, destination.z, &ground))
	{
		destination = ground.point;
		surface_index = ground.surface_index;
	}
	else
	{
		surface_index = actor->input.pathfinding_surface_index;
	}
	actor_move_to_point(actor_index, &destination, surface_index, NONE);
}


/* a goal unit walking to its point (a PATROL's, a ROUTE's) that is within
FORGE_AI_ARRIVE_DISTANCE of it and held there - not moving, or a squadmate on
the point - has reached it: its point taken as where it stands, so its wait
counts down and it goes on (action_alert_update), not pushing for the point */
static float s_unit_last_at[MAXIMUM_ACTORS][3];

static boolean forge_ai_goal_arrive_near(struct forge_ai_goal_slot const *slot, long actor_index,
	struct actor_datum *actor)
{
	struct alert_state_data *alert = &actor->state.action_data.alert;
	real_point3d const *at = &actor->input.position.body_position;
	int index = DATUM_INDEX_TO_ABSOLUTE_INDEX(actor_index);
	float to_point, dx, dy, dz;
	boolean held, crowded = FALSE;

	if (index < 0 || index >= MAXIMUM_ACTORS || actor->state.action != _actor_action_alert ||
		alert->target_move_position_index == NONE || alert->pending_move_position_index != NONE)
	{
		return FALSE;
	}
	dx = at->x - s_unit_last_at[index][0];
	dy = at->y - s_unit_last_at[index][1];
	dz = at->z - s_unit_last_at[index][2];
	s_unit_last_at[index][0] = at->x;
	s_unit_last_at[index][1] = at->y;
	s_unit_last_at[index][2] = at->z;
	to_point = distance3d(at, &alert->target_position_definition.position);
	if (to_point < 0.5f || to_point > FORGE_AI_ARRIVE_DISTANCE)
		return FALSE;
	held = dx * dx + dy * dy + dz * dz < 0.15f * 0.15f;
	if (!held)
	{
		struct data_iterator mates;
		struct actor_datum *mate;

		data_iterator_new(&mates, actor_data);
		while (!crowded && (mate = (struct actor_datum *)data_iterator_next(&mates)) != NULL)
		{
			if (mate != actor && forge_ai_goal_of_actor(mate) == slot &&
				distance3d(&mate->input.position.body_position, &alert->target_position_definition.position) < 1.5f)
			{
				crowded = TRUE;
			}
		}
	}
	if (!held && !crowded)
		return FALSE;
	alert->target_position_definition.position = *at;
	actor_move_halt(actor_index);
	return TRUE;
}

/* any goal unit out of a fight but its route's leader (a PATROL's, a
route's follower). The engine's alert walk can leave one standing for good:
a failed path keeps its point's wait, which counts down only at the point
(action_alert_update); one held short of its point (a Forge object, a
squadmate on it) pushes there; one out of the walk (guarding, or a walk with no
order or no point) waits for nothing. So a unit that has neither moved half a
unit nor had its wait count down in FORGE_AI_UNIT_STUCK_TICKS is put back on its
walk: the squad's walk again if it is out of it, else another point (a PATROL),
or a step aside, then its point again (a follower). A failed path is given up
in a second, and that PATROL point retired. Each logged with the unit's state */
#define FORGE_AI_UNIT_STUCK_TICKS (4 * 30)
static struct
{
	long unit_index;
	long since;
	float at[3];
	short wait_ticks;
	unsigned char tries;
} s_unit_stuck[MAXIMUM_ACTORS];

static void forge_ai_patrol_point_retire(struct forge_ai_goal_slot *slot, short index);
/* (actor_moving.c's, in no header) */
extern void actor_path_clear(long actor_index);
static short forge_ai_patrol_point_pick(struct forge_ai_goal_slot const *slot, real_point3d const *at);

/* a PATROL's point for a unit with none: by weight, a couple of units off */
static short forge_ai_patrol_point_pick(struct forge_ai_goal_slot const *slot, real_point3d const *at)
{
	struct move_position_definition const *moves;
	short count, k, chosen = NONE;
	float total = 0.0f, roll;
	char crowded[FORGE_AI_GOAL_MOVE_POSITIONS];

	if (!slot || !slot->squad)
		return NONE;
	moves = XBOX_POINTER(struct move_position_definition const, slot->squad->move_positions.address); /* AE hook */
	count = (short)MIN(slot->squad->move_positions.count, FORGE_AI_GOAL_MOVE_POSITIONS);
	/* (none where a squadmate stands or is going: they crowded one spot) */
	{
		struct data_iterator mates;
		struct actor_datum *mate;

		memset(crowded, 0, sizeof(crowded));
		data_iterator_new(&mates, actor_data);
		while ((mate = (struct actor_datum *)data_iterator_next(&mates)) != NULL)
		{
			if (forge_ai_goal_of_actor(mate) != slot || &mate->input.position.body_position == at)
				continue;
			if (mate->state.action == _actor_action_alert &&
				mate->state.action_data.alert.target_move_position_index >= 0 &&
				mate->state.action_data.alert.target_move_position_index < count)
			{
				crowded[mate->state.action_data.alert.target_move_position_index] = 1;
			}
			for (k = 0; k < count; k++)
			{
				if (distance_squared3d(&mate->input.position.body_position, &moves[k].position) < 9.0f)
					crowded[k] = 1;
			}
		}
	}
	for (k = 0; k < count; k++)
	{
		if (moves[k].weight > 0.0f && !crowded[k] && distance_squared3d(at, &moves[k].position) > 4.0f)
			total += moves[k].weight;
	}
	/* (all crowded: any) */
	if (total <= 0.0f)
	{
		memset(crowded, 0, sizeof(crowded));
		for (k = 0; k < count; k++)
		{
			if (moves[k].weight > 0.0f && distance_squared3d(at, &moves[k].position) > 4.0f)
				total += moves[k].weight;
		}
	}
	if (total <= 0.0f)
		return NONE;
	roll = real_random_range(0.0f, total);
	for (k = 0; k < count; k++)
	{
		if (moves[k].weight > 0.0f && !crowded[k] && distance_squared3d(at, &moves[k].position) > 4.0f)
		{
			chosen = k;
			roll -= moves[k].weight;
			if (roll <= 0.0f)
				break;
		}
	}
	return chosen;
}

static void forge_ai_goal_unit_unstick(struct forge_ai_goal_slot *slot, long actor_index, struct actor_datum *actor,
	int place, long now, int patrol)
{
	struct alert_state_data *alert = &actor->state.action_data.alert;
	real_point3d const *at = &actor->input.position.body_position;
	int slot_index = DATUM_INDEX_TO_ABSOLUTE_INDEX(actor_index);
	int walking = actor->state.action == _actor_action_alert;
	short target = walking ? alert->target_move_position_index : NONE;
	short wait = walking ? alert->wait_ticks : 0;
	float dx, dy, dz, to_point;

	if (slot_index < 0 || slot_index >= MAXIMUM_ACTORS)
		return;
	to_point = target == NONE ? 0.0f : distance3d(at, &alert->target_position_definition.position);
	dx = at->x - s_unit_stuck[slot_index].at[0];
	dy = at->y - s_unit_stuck[slot_index].at[1];
	dz = at->z - s_unit_stuck[slot_index].at[2];
	/* (moving, or its wait counting down at a point: well) */
	if (s_unit_stuck[slot_index].unit_index != actor->meta.unit_index || dx * dx + dy * dy + dz * dz > 0.25f ||
		(wait > 0 && wait < s_unit_stuck[slot_index].wait_ticks))
	{
		if (s_unit_stuck[slot_index].unit_index != actor->meta.unit_index)
			s_unit_stuck[slot_index].tries = 0;
		/* (a point reached: the tries over) */
		if (target != NONE && to_point < 1.0f)
			s_unit_stuck[slot_index].tries = 0;
		s_unit_stuck[slot_index].unit_index = actor->meta.unit_index;
		s_unit_stuck[slot_index].since = now;
		s_unit_stuck[slot_index].at[0] = at->x;
		s_unit_stuck[slot_index].at[1] = at->y;
		s_unit_stuck[slot_index].at[2] = at->z;
		s_unit_stuck[slot_index].wait_ticks = wait;
		return;
	}
	s_unit_stuck[slot_index].wait_ticks = wait;
	/* (a failed path: given up in a second, its point no PATROL's) */
	if (walking && target != NONE && alert->pending_move_position_index == NONE && to_point > 1.0f &&
		!actor_path_has_path(actor_index) && now - s_unit_stuck[slot_index].since >= 30)
	{
		error(_error_silent, "forge ai: a goal unit has no path to its point %d (%.1f away, standing on surface %ld): "
			"another", (int)target, (double)to_point, (long)actor->input.pathfinding_surface_index);
		/* (the point's fault only when the unit stands where paths start) */
		if (patrol && actor->input.pathfinding_surface_index != NONE)
			forge_ai_patrol_point_retire(slot, target);
		alert->wait_ticks = 0;
		action_alert_flush_position_indices(actor_index);
		s_unit_stuck[slot_index].since = now;
		return;
	}
	if (now - s_unit_stuck[slot_index].since < FORGE_AI_UNIT_STUCK_TICKS)
		return;
	s_unit_stuck[slot_index].since = now;
	{
		/* (the engine's own view of its walk, and its squad's nearness) */
		struct path_result const *path = &actor->control.path.path;
		real_point3d const *step = path->valid && path->step_index >= 0 && path->step_index < path->step_count ?
			&path->steps[(int)path->step_index].point : at;
		struct data_iterator mates;
		struct actor_datum *mate;
		float nearest = 1.0e9f;
		int close = 0;

		data_iterator_new(&mates, actor_data);
		while ((mate = (struct actor_datum *)data_iterator_next(&mates)) != NULL)
		{
			float d;

			if (mate == actor || forge_ai_goal_of_actor(mate) != slot)
				continue;
			d = distance3d(at, &mate->input.position.body_position);
			if (d < nearest)
				nearest = d;
			if (d < 2.0f)
				close++;
		}
		error(_error_silent, "forge ai: a %s unit stands at (%.1f %.1f %.1f): action %d, combat %d, order %d, point %d, "
			"pending %d, wait %d, path %d (dest type %d, step %d/%d %.1f away, finishes %d), %.1f from its point, "
			"moving %d, complete %d, thwarted %d, at dest %d, midair %d, emergency %.2f, throttle %.2f, "
			"nearest mate %.1f, %d within 2, try %d",
			patrol ? "patrol" : "route", (double)at->x, (double)at->y, (double)at->z, (int)actor->state.action,
			(int)actor->state.combat_status, walking ? (int)alert->move_position_order : -1, (int)target,
			walking ? (int)alert->pending_move_position_index : -1, (int)wait, (int)path->valid,
			(int)actor->control.path.destination_orders.destination_type, (int)path->step_index, (int)path->step_count,
			(double)distance3d(at, step), (int)path->steps_finish_path, (double)to_point, (int)actor->control.moving,
			(int)actor->control.movement_complete, (int)actor->control.movement_thwarted,
			(int)actor->control.path.at_destination, (int)actor->input.in_midair,
			(double)actor->control.vector_avoidance_rotation_emergency_instantaneous,
			(double)magnitude3d(&actor->output.throttle), nearest > 1.0e8f ? -1.0 : (double)nearest, close,
			(int)s_unit_stuck[slot_index].tries);
	}
	/* (out of its walk, or on one with no order: the squad's walk again) */
	if (!walking || alert->move_position_order == 0)
	{
		if (slot->squad)
			actor_action_set_default_state(actor_index, slot->squad->default_state);
		return;
	}
	alert->wait_ticks = 0;
	if (patrol)
	{
		/* (another point, chosen here, for the walk may find none: its old
		path gone with it) */
		actor_path_clear(actor_index);
		action_alert_flush_position_indices(actor_index);
		alert->pending_move_position_index = forge_ai_patrol_point_pick(slot, at);
		return;
	}
	if (target == NONE)
	{
		action_alert_flush_position_indices(actor_index);
		return;
	}
	if ((s_unit_stuck[slot_index].tries++ & 1) == 0)
	{
		/* (a step aside, square to the way it was going, left or right in turn) */
		real_point3d const *goal = &alert->target_position_definition.position;
		float gx = goal->x - at->x, gy = goal->y - at->y;
		float length = (float)sqrt((double)(gx * gx + gy * gy));
		float side = ((s_unit_stuck[slot_index].tries >> 1) + place) & 1 ? 2.5f : -2.5f;
		real_point3d aside;

		if (length < 0.01f)
		{
			gx = 1.0f;
			gy = 0.0f;
			length = 1.0f;
		}
		aside.x = at->x - gy / length * side - gx / length;
		aside.y = at->y + gx / length * side - gy / length;
		aside.z = at->z;
		forge_ai_goal_move_toward(actor_index, actor, &aside, 0.0f, place);
	}
	else
	{
		/* (its point again; a fourth time stuck, the next point instead) */
		if (s_unit_stuck[slot_index].tries >= 4)
		{
			forge_ai_route_walk_skip(slot, actor_index, actor);
			s_unit_stuck[slot_index].tries = 0;
		}
		action_alert_flush_position_indices(actor_index);
	}
}

/* a PATROL's point a unit gave up on: weighed nothing from now on, so the
random walk (action_alert_next_position) picks it no more - unless it is the
last left */
static void forge_ai_patrol_point_retire(struct forge_ai_goal_slot *slot, short index)
{
	struct move_position_definition *moves;
	short count, k, left = 0;

	if (!slot || !slot->squad || index < 0)
		return;
	moves = XBOX_POINTER(struct move_position_definition, slot->squad->move_positions.address); /* AE hook */
	count = (short)MIN(slot->squad->move_positions.count, FORGE_AI_GOAL_MOVE_POSITIONS);
	if (index >= count)
		return;
	for (k = 0; k < count; k++)
	{
		if (k != index && moves[k].weight > 0.0f)
			left++;
	}
	if (left > 0)
	{
		moves[index].weight = 0.0f;
		error(_error_silent, "forge ai: patrol point %d retired, %d left", (int)index, (int)left);
	}
}

/* whether a goal's unit is in a fight, or has just been hurt: what sounds its
squad's alarm */
static boolean forge_ai_goal_unit_alarmed(struct actor_datum const *actor)
{
	return actor->state.combat_status >= 2 || actor->state.action == _actor_action_fight ||
		actor->state.action == _actor_action_search || actor->state.action == _actor_action_flee ||
		actor->state.action == _actor_action_charge || actor->input.recent_body_damage > 0.0f ||
		actor->input.recent_shield_damage > 0.0f;
}

/* each tick of the host's (forge_ai_units_update): the goals' units out of
a fight sent on, a ROUTE's leader chosen again when it dies, a PATROL's
units kept to its PURSUIT RANGE, and the others' firing positions laid again
where they have gone.

A squad's alarm: one unit of it fighting, hurt or killed alerts them all, for
its ALERT TIME after the last of it. Meanwhile the route is held
(forge_ai_route_next_position gives its leader nowhere), the leader stops
where it is, and those not fighting go to where it was, at a run, which has
them see what it saw; the fight's firing positions laid round there. Once it
is over the leader walks on to the point it was going to */
static void forge_ai_goals_update(long now)
{
	struct data_iterator iterator;
	struct actor_datum *actor;
	int k;

	if (s_goal_count == 0 || s_ai_held || !actor_data)
		return;
	for (k = 0; k < s_goal_count; k++)
	{
		struct unit_datum *leader = s_goals[k].leader_unit == NONE ? NULL :
			(struct unit_datum *)object_try_and_get_and_verify_type(s_goals[k].leader_unit, _object_mask_unit);

		if (!leader || TEST_FLAG(leader->object.damage_flags, _object_dead_bit) || leader->unit.actor_index == NONE)
			s_goals[k].leader_unit = NONE;
		s_goals[k].sum[0] = s_goals[k].sum[1] = s_goals[k].sum[2] = 0.0f;
		s_goals[k].sum_count = 0;
		s_goals[k].alive_now = 0;
		s_goals[k].alarm_seen = FALSE;
	}
	/* (first: which squads are alarmed, and where) */
	data_iterator_new(&iterator, actor_data);
	while ((actor = (struct actor_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct forge_ai_goal_slot *slot = forge_ai_goal_of_actor(actor);
		struct unit_datum *unit;
		real_point3d const *at;

		if (!slot)
			continue;
		unit = (struct unit_datum *)object_try_and_get_and_verify_type(actor->meta.unit_index, _object_mask_unit);
		if (!unit || TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
			continue;
		at = &actor->input.position.body_position;
		slot->alive_now++;
		slot->sum[0] += at->x;
		slot->sum[1] += at->y;
		slot->sum[2] += at->z;
		slot->sum_count++;
		if (forge_ai_goal_unit_alarmed(actor))
		{
			if (!slot->alarm_seen)
			{
				slot->alarm_point[0] = at->x;
				slot->alarm_point[1] = at->y;
				slot->alarm_point[2] = at->z;
			}
			slot->alarm_seen = TRUE;
		}
	}
	for (k = 0; k < s_goal_count; k++)
	{
		struct forge_ai_goal_slot *slot = &s_goals[k];
		boolean died = slot->alive_now < slot->alive_count;
		boolean was_alarmed = slot->alarm_until != 0 && now < slot->alarm_until;

		/* (a death with none of them fighting: about where they are) */
		if (died && !slot->alarm_seen && slot->sum_count > 0)
		{
			slot->alarm_point[0] = slot->sum[0] / (float)slot->sum_count;
			slot->alarm_point[1] = slot->sum[1] / (float)slot->sum_count;
			slot->alarm_point[2] = slot->sum[2] / (float)slot->sum_count;
		}
		if (slot->alarm_seen || died)
		{
			slot->alarm_until = now + slot->alert_ticks;
			/* (its fight round where it began, not where the rings were) */
			if (!was_alarmed && slot->goal != _forge_ai_goal_patrol)
				forge_ai_goal_rings_lay(slot, slot->alarm_point[0], slot->alarm_point[1], slot->alarm_point[2]);
			if (!was_alarmed)
				error(_error_silent, "forge ai: goal squad %d alarmed (%s), %d alive", k, died ? "a death" : "a fight",
					slot->alive_now);
		}
		else if (was_alarmed && now + 1 >= slot->alarm_until)
		{
			error(_error_silent, "forge ai: goal squad %d calm again: back on its walk", k);
		}
		slot->alive_count = slot->alive_now;
	}
	data_iterator_new(&iterator, actor_data);
	while ((actor = (struct actor_datum *)data_iterator_next(&iterator)) != NULL)
	{
		long actor_index = iterator.datum_index;
		struct forge_ai_goal_slot *slot = forge_ai_goal_of_actor(actor);
		struct unit_datum *unit;
		real_point3d const *at;
		boolean idle, alarmed;
		int place = iterator.absolute_index;

		if (!slot)
			continue;
		unit = (struct unit_datum *)object_try_and_get_and_verify_type(actor->meta.unit_index, _object_mask_unit);
		if (!unit || TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
			continue;
		/* (they go on with no one near: the engine would let them sleep) */
		actor->meta.force_active = TRUE;
		actor->meta.dormant_desire = FALSE;
		at = &actor->input.position.body_position;
		alarmed = forge_ai_goal_alarmed(slot);
		/* (a third of a second apart, each its own tick) */
		if ((place + now) % 10 != 0 || unit->object.parent_object_index != NONE)
			continue;
		idle = actor->state.combat_status < 2 && (actor->state.action == _actor_action_alert ||
			actor->state.action == _actor_action_guard || actor->state.action == _actor_action_none);
		if (alarmed)
		{
			/* (the alarm: those not fighting to where it is, or held there) */
			if (idle)
			{
				real_point3d alarm;

				alarm.x = slot->alarm_point[0];
				alarm.y = slot->alarm_point[1];
				alarm.z = slot->alarm_point[2];
				if (actor->state.action == _actor_action_alert)
					action_alert_flush_position_indices(actor_index);
				if (distance3d(at, &alarm) > FORGE_AI_GOAL_ALARM_JOIN + 1.5f)
					forge_ai_goal_move_toward(actor_index, actor, &alarm, FORGE_AI_GOAL_ALARM_JOIN, place);
				else
					actor_move_halt(actor_index);
			}
			if (slot->goal == _forge_ai_goal_patrol)
				actor->external_orders.disable_charging = TRUE;
			continue;
		}
		switch (slot->goal)
		{
		case _forge_ai_goal_patrol:
		{
			float point[3];

			if (idle && !forge_ai_goal_arrive_near(slot, actor_index, actor))
				forge_ai_goal_unit_unstick(slot, actor_index, actor, place, now, 1);
			actor->external_orders.disable_charging = TRUE;
			point[0] = at->x;
			point[1] = at->y;
			point[2] = at->z + 0.3f;
			/* (out past its PURSUIT RANGE and not fighting from where it may:
			back to the patrol, which walks it in) */
			if (!idle && actor->state.action != _actor_action_fight &&
				!forge_volume_shape_contains(slot->volume.shape, slot->volume.position, slot->volume.yaw,
					slot->volume.pitch, slot->volume.roll, slot->volume.size, slot->volume.pursuit + 1.0f, point))
			{
				actor_action_set_default_state(actor_index, NONE);
			}
			break;
		}
		case _forge_ai_goal_route:
			/* (each on its own walk: forge_ai_route_next_position) */
			if (idle && !forge_ai_goal_arrive_near(slot, actor_index, actor))
				forge_ai_goal_unit_unstick(slot, actor_index, actor, place, now, 0);
			break;
		case _forge_ai_goal_hunt:
			/* (GO TO PLAYER: the nearest player within RANGE - anywhere, 0 - is
			gone to, seen or not. A team of players' stops STOP DISTANCE short
			of them, at its SPEED; any other goes all the way, at a run, and
			fights them as it comes on them) */
			if (idle)
			{
				real_point3d where;

				if (forge_ai_goal_player(FORGE_AI_GOAL_WHO(slot->who), at, &where) != NONE)
				{
					float distance = distance3d(at, &where);
					float stop = slot->friendly ? (float)slot->follow : 0.0f;

					if ((slot->start == 0 || distance <= (float)slot->start) && distance > stop + 1.0f)
					{
						if (!slot->friendly)
							slot->chasing_until = now + 45;
						forge_ai_goal_move_toward(actor_index, actor, &where, stop, place);
					}
				}
			}
			break;
		}
	}
	/* (the firing positions laid again round where they have gone) */
	if (now % 30 == 0)
	{
		for (k = 0; k < s_goal_count; k++)
		{
			struct forge_ai_goal_slot *slot = &s_goals[k];
			float x, y, z, dx, dy, dz;

			if (slot->goal == _forge_ai_goal_patrol || slot->sum_count == 0)
				continue;
			x = slot->sum[0] / (float)slot->sum_count;
			y = slot->sum[1] / (float)slot->sum_count;
			z = slot->sum[2] / (float)slot->sum_count;
			dx = x - slot->ring_center[0];
			dy = y - slot->ring_center[1];
			dz = z - slot->ring_center[2];
			if (dx * dx + dy * dy + dz * dz > FORGE_AI_GOAL_RING_MOVE * FORGE_AI_GOAL_RING_MOVE)
				forge_ai_goal_rings_lay(slot, x, y, z);
		}
	}
}

/* ---------- the lines to the goals, in Forge mode */

/* where a spawn's goal is drawn to: an AI PATROL volume's middle, a route's
first point; and the live index that stands for it (-1: none) */
static int forge_ai_goal_link_point(struct forge_ai_spawn_entry const *entry, float point[3], int32_t *target)
{
	int32_t points[1];

	*target = -1;
	if (entry->goal == _forge_ai_goal_patrol)
		*target = forge_volume_patrol_find(entry->goal_link);
	else if (entry->goal == _forge_ai_goal_route && forge_ai_route_points(entry->goal_link, points, 1) == 1)
		*target = points[0];
	if (*target < 0)
		return 0;
	point[0] = g_forge.live_instances[*target].x;
	point[1] = g_forge.live_instances[*target].y;
	point[2] = g_forge.live_instances[*target].z + 0.3f;
	return 1;
}

/* whether a goal's volume or route is held or pointed at */
static int forge_ai_goal_target_selected(struct forge_ai_spawn_entry const *entry, int32_t target)
{
	int32_t selected[2];
	int k;

	selected[0] = g_forge.grabbed_live_index;
	selected[1] = g_forge.hovered_live_index;
	for (k = 0; k < 2; k++)
	{
		struct forge_live_instance const *inst;

		if (selected[k] < 0 || selected[k] >= g_forge.live_instance_count)
			continue;
		if (selected[k] == target)
			return 1;
		inst = &g_forge.live_instances[selected[k]];
		if (entry->goal == _forge_ai_goal_route && forge_ai_route_is(inst) &&
			FORGE_AI_ROUTE_NUMBER(inst->team_designator) == entry->goal_link)
		{
			return 1;
		}
	}
	return 0;
}

void forge_ai_goal_links_render(void (*draw_line)(float const *from, float const *to))
{
	int32_t i;

	if (!draw_line)
		return;
	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		struct forge_live_instance const *inst = &g_forge.live_instances[i];
		struct forge_ai_spawn_entry const *entry;
		float from[3], to[3];
		int32_t target;

		if (!inst->active || inst->group_tag != FORGE_GROUP_AI_SPAWN)
			continue;
		entry = forge_ai_entry_of(inst);
		if (!entry || (entry->goal != _forge_ai_goal_patrol && entry->goal != _forge_ai_goal_route) ||
			!forge_ai_goal_link_point(entry, to, &target))
		{
			continue;
		}
		if (i != g_forge.grabbed_live_index && i != g_forge.hovered_live_index &&
			!forge_ai_goal_target_selected(entry, target))
		{
			continue;
		}
		from[0] = inst->x;
		from[1] = inst->y;
		from[2] = inst->z + 0.5f;
		draw_line(from, to);
	}
}

void ae_forge_ai_set(int on); /* AE hook */
static void forge_ai_encounters_build(void)
{
	struct scenario *scen = global_scenario_try_and_get();
	struct forge_ai_spawn_entry const *entries;
	struct encounter_definition *encounters;
	struct tag_reference *original_palette;
	struct forge_ai_added_encounter added[FORGE_AI_MAX_ADDED_ENCOUNTERS];
	short *entry_added = NULL;
	long *entry_variant = NULL;
	/* (the spawns that play their GOAL, and the move positions they have) */
	boolean *entry_goal = NULL;
	long goal_squads = 0, move_cursor = 0;
	long original_count = 0, original_palette_count = 0, palette_count;
	long palette_room;
	long own_squad_count = 0, squad_cursor = 0, location_cursor = 0, made_squads = 0, firing_cursor = 0;
	int entry_count = 0, replace_own = 0, added_count = 0;
	int firefight = firefight_active();
	long e, s;
	int i;

	if (!scen)
		return;
	forge_ai_scenario_restore(scen);
	ae_forge_ai_set(0); /* AE hook */
	encounters = XBOX_POINTER(struct encounter_definition, scen->ai_encounters.address); /* AE hook */
	original_count = scen->ai_encounters.count;
	original_palette = XBOX_POINTER(struct tag_reference, scen->ai_actor_palette.address); /* AE hook */
	original_palette_count = scen->ai_actor_palette.count;
	entries = forge_ai_level_spawns(&entry_count, &replace_own);
	s_ai_units_placed_logged = 0;
	/* (diagnostic, debug.txt: why a level's Forge AI does or does not play) */
	if (scen->type == _scenario_type_multiplayer && (entries || s_saved.has_spawns || g_forge_session_enabled))
	{
		error(_error_silent, "forge ai: building the level's AI: %d spawns (%s), replace own %d, map encounters %ld, "
			"session %d, live %d, saved applied %d has %d count %d, engine running %d, engine %ld, firefight %d, connection %d",
			entry_count, entries == NULL ? "none" : entries == s_saved.entries ? "saved map" : "Forge edits",
			replace_own, original_count, g_forge_session_enabled, (int)g_forge.live_instance_count, s_saved.applied,
			s_saved.has_spawns, s_saved.count, (int)game_engine_running(),
			game_engine_running() ? (long)game_engine_get_variant()->game_engine_index : -1L, firefight,
			(int)game_connection());
	}
	/* (a map that uses no AI is left alone, and a campaign level always is) */
	if (scen->type != _scenario_type_multiplayer || (!entries && (!firefight || original_count == 0)))
		return;
	ae_forge_ai_set(1); /* AE hook */
	/* (Firefight has none of the map's AI, only spawns set to play in it: its
	squads stay, empty, for the map's scripts and the encounters' indices) */
	if (firefight)
		replace_own = 1;

	/* (each spawn's actor, and a random one's ranks) */
	palette_room = (long)entry_count * (FORGE_AI_MAX_RANKS + 1);
	for (e = 0; e < original_count; e++)
		own_squad_count += encounters[e].squads.count;
	s_built.encounters = (struct encounter_definition *)calloc(
		(size_t)(original_count + FORGE_AI_MAX_ADDED_ENCOUNTERS), sizeof(*s_built.encounters));
	s_built.squads = (struct squad_definition *)calloc((size_t)(own_squad_count + entry_count + 1), sizeof(*s_built.squads));
	s_built.locations = (struct actor_starting_location *)calloc((size_t)(entry_count + 1), sizeof(*s_built.locations));
	s_built.palette = (struct tag_reference *)calloc((size_t)(original_palette_count + palette_room + 1), sizeof(*s_built.palette));
	s_built.randoms = (struct forge_ai_random_squad *)calloc((size_t)(entry_count + 1), sizeof(*s_built.randoms));
	s_built.random_count = 0;
	entry_added = (short *)malloc((size_t)(entry_count + 1) * sizeof(*entry_added));
	entry_variant = (long *)malloc((size_t)(entry_count + 1) * sizeof(*entry_variant));
	entry_goal = (boolean *)calloc((size_t)(entry_count + 1), sizeof(*entry_goal));
	if (!s_built.encounters || !s_built.squads || !s_built.locations || !s_built.palette || !s_built.randoms ||
		!entry_added || !entry_variant || !entry_goal)
	{
		platform_log("Forge: out of memory for the level's AI spawns");
		forge_ai_scenario_restore(scen);
		free(entry_added);
		free(entry_variant);
		free(entry_goal);
		return;
	}
	if (original_palette_count > 0)
		memcpy(s_built.palette, original_palette, (size_t)original_palette_count * sizeof(*s_built.palette));
	palette_count = original_palette_count;

	/* which spawns play, and the added encounters theirs go in */
	for (i = 0; i < entry_count; i++)
	{
		struct forge_ai_spawn_entry const *entry = &entries[i];
		short team, source;
		int a;

		entry_added[i] = NONE;
		entry_variant[i] = NONE;
		if (!forge_ai_entry_plays(entry))
		{
			error(_error_silent, "forge ai: spawn %d (%s) does not play in this game (active %d, modes %04x)", i,
				entry->actor_variant_path, (int)entry->active, forge_ai_entry_modes(entry));
			continue;
		}
		entry_variant[i] = forge_ai_tag_need('actv', entry->actor_variant_path);
		if (entry_variant[i] == NONE)
		{
			error(_error_silent, "forge ai: spawn %d's actor is not in this level: %s", i, entry->actor_variant_path);
			continue;
		}
		error(_error_silent, "forge ai: spawn %d plays: %s (tag %08lx), %d units, team %d, encounter %d, own squad %d", i,
			entry->actor_variant_path, (unsigned long)entry_variant[i], (int)entry->unit_count, (int)entry->team,
			(int)entry->encounter_index, forge_ai_entry_own_squad(entry));
		if (!forge_ai_entry_own_squad(entry))
			continue;
		team = forge_ai_entry_team(entry, entry_variant[i]);
		/* (a GOAL's spawn: an encounter of its own, made here, while the
		engine has room for one; else it stands as one with none) */
		entry_goal[i] = forge_ai_entry_goal_plays(entry) && added_count < FORGE_AI_MAX_ADDED_ENCOUNTERS &&
			original_count + added_count < FORGE_AI_ENGINE_ENCOUNTERS;
		if (entry->goal != _forge_ai_goal_none && !entry_goal[i])
			error(_error_silent, "forge ai: spawn %d's goal %d (#%d) has nothing to go by: it plays with none", i,
				(int)entry->goal, (int)entry->goal_link);
		source = entry_goal[i] ? NONE : forge_ai_nearest_encounter(encounters, original_count, entry);
		/* (a GOAL's spawn never shares: it is a new encounter, past the
		others - a loop it skipped left `a` 0, and every goal spawn after the
		first went into the first encounter, one slot, one leader and one set
		of firing positions for them all) */
		for (a = entry_goal[i] ? added_count : 0; a < added_count; a++)
		{
			if (!added[a].goal && added[a].team == team && added[a].source_encounter == source &&
				added[a].squad_count < (source == NONE ? FORGE_AI_MADE_SQUADS_PER_ENCOUNTER : MAXIMUM_SQUADS_PER_ENCOUNTER))
			{
				break;
			}
		}
		if (a == added_count)
		{
			if (added_count >= FORGE_AI_MAX_ADDED_ENCOUNTERS || original_count + added_count >= FORGE_AI_ENGINE_ENCOUNTERS)
				continue;
			memset(&added[a], 0, sizeof(added[a]));
			added[a].team = team;
			added[a].source_encounter = source;
			added[a].goal = entry_goal[i];
			added_count++;
		}
		if (entry_goal[i])
			goal_squads++;
		added[a].squad_count++;
		added[a].respawns = added[a].respawns || entry->respawn_alive > 0;
		entry_added[i] = (short)a;
		if (source == NONE)
			made_squads++;
	}
	if (made_squads > 0)
	{
		s_built.firing_positions = (struct firing_position_definition *)calloc(
			(size_t)made_squads * FORGE_AI_FIRING_POSITIONS_PER_SPAWN, sizeof(*s_built.firing_positions));
		if (goal_squads > 0)
			s_built.move_positions = (struct move_position_definition *)calloc(
				(size_t)goal_squads * FORGE_AI_GOAL_MOVE_POSITIONS, sizeof(*s_built.move_positions));
		if (!s_built.firing_positions || (goal_squads > 0 && !s_built.move_positions))
		{
			platform_log("Forge: out of memory for the level's AI spawns");
			forge_ai_scenario_restore(scen);
			free(entry_added);
			free(entry_variant);
			free(entry_goal);
			return;
		}
	}

	/* the map's encounters and squads, in their places */
	if (original_count > 0)
		memcpy(s_built.encounters, encounters, (size_t)original_count * sizeof(*s_built.encounters));
	if (replace_own)
	{
		for (e = 0; e < original_count; e++)
		{
			struct squad_definition *squads = &s_built.squads[squad_cursor];

			if (encounters[e].squads.count <= 0)
				continue;
			memcpy(squads, XBOX_POINTER(void, encounters[e].squads.address), (size_t)encounters[e].squads.count * sizeof(*squads)); /* AE hook */
			s_built.encounters[e].squads.address = XBOX_ADDRESS(squads); /* AE hook */
			squad_cursor += encounters[e].squads.count;
			for (s = 0; s < encounters[e].squads.count; s++)
			{
				struct squad_definition *squad = &squads[s];
				struct actor_starting_location const *own_locations =
					XBOX_POINTER(struct actor_starting_location const, squad->starting_locations.address); /* AE hook */
				long own_location_count = squad->starting_locations.count;
				struct actor_starting_location *locations = &s_built.locations[location_cursor];
				long count = 0;

				for (i = 0; i < entry_count; i++)
				{
					struct forge_ai_spawn_entry const *entry = &entries[i];

					if (entry->encounter_index != e || entry->squad_index != s || entry_variant[i] == NONE ||
						forge_ai_entry_own_squad(entry) || entry->location_index < 0 ||
						entry->location_index >= own_location_count)
					{
						continue;
					}
					locations[count] = own_locations[entry->location_index];
					{
						short palette_index = forge_ai_palette_index(s_built.palette, &palette_count,
							original_palette_count + palette_room, entry_variant[i]);

						/* (NONE: the squad's own actor) */
						forge_ai_location_write(&locations[count], entry,
							palette_index == squad->actor_palette_index ? (short)NONE : palette_index);
					}
					count++;
				}
				location_cursor += count;
				/* (the rank chosen is the rank placed: no chance of the next one up) */
				if (count > 0)
					squad->major_upgrade = _actor_major_upgrade_none;
				squad->starting_locations.address = XBOX_ADDRESS(locations); /* AE hook */
				squad->starting_locations.count = count;
				if (count == 0)
				{
					squad->min_count = 0;
					squad->max_count = 0;
					squad->respawn_min_actors = 0;
					squad->respawn_max_actors = 0;
				}
			}
		}
	}

	/* then the added encounters, each spawn with a squad of its own */
	for (i = 0; i < added_count; i++)
	{
		struct forge_ai_added_encounter *a = &added[i];
		struct encounter_definition *encounter = &s_built.encounters[original_count + i];

		a->squads = &s_built.squads[squad_cursor];
		squad_cursor += a->squad_count;
		if (a->source_encounter != NONE)
		{
			*encounter = encounters[a->source_encounter];
		}
		else
		{
			memset(encounter, 0, sizeof(*encounter));
			encounter->version = 1;
			a->firing_positions = &s_built.firing_positions[firing_cursor];
			firing_cursor += (long)a->squad_count * FORGE_AI_FIRING_POSITIONS_PER_SPAWN;
		}
		snprintf(encounter->name, sizeof(encounter->name), "forge_ai_%d", i);
		encounter->team_index = a->team;
		encounter->flags &= ~(unsigned long)(1 << 0); /* (created with the level) */
		if (a->respawns)
			encounter->flags |= 1 << 1; /* (respawning, each squad's delay alone) */
		encounter->respawn_time_lower_bound = 0.0f;
		encounter->respawn_time_upper_bound = 0.0f;
		encounter->squads.address = XBOX_ADDRESS(a->squads); /* AE hook */
		encounter->squads.count = a->squad_count;
		encounter->platoons.address = XBOX_ADDRESS(NULL); /* AE hook */
		encounter->platoons.count = 0;
	}
	for (i = 0; i < entry_count; i++)
	{
		struct forge_ai_spawn_entry const *entry = &entries[i];
		struct forge_ai_added_encounter *a;
		struct squad_definition const *pattern;
		struct squad_definition *squad;
		struct actor_starting_location *location;
		short palette_index;

		if (entry_added[i] == NONE)
			continue;
		a = &added[entry_added[i]];
		pattern = a->source_encounter != NONE ? forge_ai_nearest_squad(&encounters[a->source_encounter], entry) : NULL;
		palette_index = forge_ai_palette_index(s_built.palette, &palette_count,
			original_palette_count + palette_room, entry_variant[i]);
		squad = &a->squads[a->squads_filled];
		location = &s_built.locations[location_cursor++];
		if (pattern)
		{
			*squad = *pattern;
		}
		else
		{
			short group;

			memset(squad, 0, sizeof(*squad));
			squad->initial_state = FORGE_AI_SQUAD_STATE;
			squad->default_state = FORGE_AI_SQUAD_STATE;
			squad->flags = FLAG(_squad_timer_starts_immediately_bit);
			for (group = 0; group < FORGE_AI_SQUAD_POSITION_GROUP_COUNT; group++)
				squad->firing_position_groups[group] = FORGE_AI_SQUAD_POSITION_GROUPS;
			if (!entry_goal[i])
				forge_ai_firing_positions_add(a, entry);
		}
		snprintf(squad->name, sizeof(squad->name), "forge_ai_%d", a->squads_filled);
		a->squads_filled++;
		squad->actor_palette_index = palette_index;
		/* (the rank chosen is the rank placed: no chance of the next one up) */
		squad->major_upgrade = _actor_major_upgrade_none;
		if (entry->gametype_flags & FORGE_AI_RANDOM_RANK)
			forge_ai_random_squad_add(squad, entry, &palette_count, original_palette_count + palette_room);
		squad->platoon_index = NONE;
		squad->maneuver_squad_index = NONE;
		squad->flags &= ~(unsigned long)FLAG(_squad_delay_forever_bit);
		squad->squad_delay_timer = 0.0f;
		squad->min_count = entry->unit_count;
		squad->max_count = entry->unit_count;
		/* (none at once: each waits out the delay) */
		squad->respawn_min_actors = 0;
		squad->respawn_max_actors = entry->respawn_alive;
		squad->respawn_total_count = entry->respawn_total;
		squad->respawn_time_lower_bound = (real)entry->respawn_delay_seconds;
		squad->respawn_time_upper_bound = (real)entry->respawn_delay_seconds;
		/* (a GOAL's: its move positions, state and firing positions, the
		encounter's whole FORGE_AI_FIRING_POSITIONS_PER_SPAWN) */
		if (entry_goal[i] && a->firing_positions)
		{
			forge_ai_goal_squad_make(entry, (short)(original_count + entry_added[i]), squad,
				&s_built.move_positions[move_cursor], a->firing_positions);
			move_cursor += FORGE_AI_GOAL_MOVE_POSITIONS;
			a->firing_position_count = FORGE_AI_FIRING_POSITIONS_PER_SPAWN;
		}
		memset(location, 0, sizeof(*location));
		location->cluster_index = NONE;
		location->noncombat_sequence_id = NONE;
		location->default_state = squad->default_state;
		location->initial_state = squad->initial_state;
		location->command_list_index = NONE;
		forge_ai_location_write(location, entry, NONE);
		squad->starting_locations.address = XBOX_ADDRESS(location); /* AE hook */
		squad->starting_locations.count = 1;
	}
	for (i = 0; i < added_count; i++)
	{
		if (added[i].source_encounter == NONE)
		{
			s_built.encounters[original_count + i].firing_positions.address = XBOX_ADDRESS(added[i].firing_positions); /* AE hook */
			s_built.encounters[original_count + i].firing_positions.count = added[i].firing_position_count;
		}
	}
	free(entry_added);
	free(entry_variant);
	free(entry_goal);

	s_built.original_encounters = encounters;
	s_built.original_encounter_count = original_count;
	s_built.original_palette = original_palette;
	s_built.original_palette_count = original_palette_count;
	scen->ai_encounters.address = XBOX_ADDRESS(s_built.encounters); /* AE hook */
	scen->ai_encounters.count = original_count + added_count;
	scen->ai_actor_palette.address = XBOX_ADDRESS(s_built.palette); /* AE hook */
	scen->ai_actor_palette.count = palette_count;
	error(_error_silent, "forge ai: the level has %ld encounters (%d added, %ld firing positions made), palette %ld",
		(long)scen->ai_encounters.count, added_count, firing_cursor, palette_count);
}

/* (from ai_initialize_for_new_map, the game's type known, every tag in) */
void forge_ai_encounters_apply(void)
{
	struct scenario *scen = global_scenario_try_and_get();

	if (!scen)
		return;
	/* (a new level is never held, and the count kept was the last one's) */
	if (!g_forge_session_enabled || g_forge.live_instance_count == 0)
	{
		s_ai_held = 0;
		s_held_scenario = NULL;
	}
	else if (s_held_scenario == scen)
	{
		scen->ai_encounters.count = s_held_encounter_count;
		s_held_scenario = NULL;
	}
	forge_ai_encounters_build();
	if (s_ai_held)
	{
		error(_error_silent, "forge ai: the AI is held (Forge mode): its %ld encounters hidden",
			(long)scen->ai_encounters.count);
		s_held_scenario = scen;
		s_held_encounter_count = scen->ai_encounters.count;
		scen->ai_encounters.count = 0;
	}
}

/* ---------- how a spawn's units appear

Each effect's tag is the map's or the campaign's, by these names (all twelve
multiplayer maps have the same effects: those need no map here). The others
are copied in as a spawn's actors are: forge_ai_spawn_effect_wanted for an
online game, forge_ai_saved_tags_collect for a saved map. A level without one
shows the teleport, which every map has */
#define FORGE_AI_EFFECT_TELEPORT "effects\\coop teleport"

enum
{
	_forge_ai_effect_none = 0,
	_forge_ai_effect_teleport,
	_forge_ai_effect_cloak,
};

static struct
{
	char const *label;
	char const *path; /* NULL: not a tag */
	char const *map; /* the campaign level with it, NULL: every map has it */
} const s_ai_spawn_effects[] =
{
	{ "NONE", NULL, NULL },
	{ "TELEPORT", FORGE_AI_EFFECT_TELEPORT, NULL },
	{ "CLOAK", NULL, NULL },
	{ "PLASMA BLAST", "weapons\\plasma grenade\\effects\\explosion", NULL },
	{ "FLOOD BURST", SKULLS_OUTBREAK_BURST_TAG, "c20" },
};

int forge_ai_spawn_effect_wanted(char const *name)
{
	int i;

	for (i = 0; i < (int)NUMBEROF(s_ai_spawn_effects); i++)
	{
		if (s_ai_spawn_effects[i].map && !_stricmp(s_ai_spawn_effects[i].path, name))
			return 1;
	}
	return 0;
}

void forge_ai_spawn_effect_play(int effect, union real_point3d const *point)
{
	char const *marker_names[1];
	real_point3d marker_points[1];
	real_vector3d marker_forwards[1];
	long definition;

	if (effect < 0 || effect >= (int)NUMBEROF(s_ai_spawn_effects) || !s_ai_spawn_effects[effect].path)
		return;
	definition = forge_ai_tag_find('effe', s_ai_spawn_effects[effect].path);
	if (definition == NONE)
		definition = forge_ai_tag_find('effe', FORGE_AI_EFFECT_TELEPORT);
	if (definition == NONE)
		return;
	marker_names[0] = "";
	marker_points[0] = *point;
	marker_forwards[0] = *global_up3d;
	/* (it hurts no one, and leaves nothing to trip over) */
	skulls_effect_harmless = TRUE;
	skulls_effect_no_objects = TRUE;
	effect_new_unattached_from_markers(definition, NONE, NULL, 1, marker_names, marker_points, marker_forwards,
		0.0f, 0.0f, NULL, NULL, TRUE);
	skulls_effect_harmless = FALSE;
	skulls_effect_no_objects = FALSE;
}

/* ---------- a spawn's units as they start

The first at its point, the next around it in a cross and its corners, north
being the way the spawn faces, then the same a step further out. A place in a
wall or with no ground under it is the point's. Its respawns are all the
point's (encounter_place_actor's `spawning`) */
#define FORGE_AI_GRID_SPACING 0.8f
#define FORGE_AI_GRID_PLACES 16

static signed char const s_ai_grid[FORGE_AI_GRID_PLACES][2] =
{
	/* north, east */
	{ 0, 0 },
	{ 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
	{ 1, 1 }, { -1, 1 }, { -1, -1 }, { 1, -1 },
	{ 2, 0 }, { -2, 0 }, { 0, 2 }, { 0, -2 },
	{ 2, 2 }, { -2, 2 }, { -2, -2 },
};

/* (what the level's ground gave, kept for the preview, which asks each frame) */
struct forge_ai_grid_layout
{
	float x, y, z, yaw;
	short count;
	union real_point3d points[FORGE_AI_GRID_PLACES];
};
static struct forge_ai_grid_layout *s_ai_grid_layouts;
/* the next unit of each spawn to start: a spawn starts them all again each time
the AI does, so this goes round */
static short s_ai_grid_next[FORGE_MAX_AI_SPAWNS];
/* the effect of the unit being placed, and its spawn (-1: none of Forge's) */
static int s_ai_placing_effect;
static int s_ai_placing_entry = -1;

static void forge_ai_grid_place(struct forge_ai_spawn_entry const *entry, int index, union real_point3d *point)
{
	unsigned long const flags = FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit);
	float north = (float)s_ai_grid[index][0] * FORGE_AI_GRID_SPACING;
	float east = (float)s_ai_grid[index][1] * FORGE_AI_GRID_SPACING;
	float forward_x = (float)cos((double)entry->yaw), forward_y = (float)sin((double)entry->yaw);
	real_point3d start;
	real_vector3d vector;
	struct collision_result collision;

	point->x = entry->x;
	point->y = entry->y;
	point->z = entry->z;
	if (!index)
		return;
	/* (a clear way there at waist height, then the ground) */
	start.x = entry->x;
	start.y = entry->y;
	start.z = entry->z + 0.5f;
	vector.i = forward_x * north + forward_y * east;
	vector.j = forward_y * north - forward_x * east;
	vector.k = 0.0f;
	memset(&collision, 0, sizeof(collision));
	if (collision_test_vector(flags, &start, &vector, NONE, &collision))
		return;
	start.x += vector.i;
	start.y += vector.j;
	vector.i = 0.0f;
	vector.j = 0.0f;
	vector.k = -2.0f;
	memset(&collision, 0, sizeof(collision));
	if (!collision_test_vector(flags, &start, &vector, NONE, &collision))
		return;
	point->x = start.x;
	point->y = start.y;
	point->z = collision.point.z + 0.05f;
}

static struct forge_ai_grid_layout const *forge_ai_grid_layout(struct forge_ai_spawn_entry const *entry, int slot)
{
	struct forge_ai_grid_layout *layout;
	int i;

	if (!s_ai_grid_layouts)
		s_ai_grid_layouts = (struct forge_ai_grid_layout *)calloc(FORGE_MAX_AI_SPAWNS, sizeof(*s_ai_grid_layouts));
	if (!s_ai_grid_layouts || slot < 0 || slot >= FORGE_MAX_AI_SPAWNS)
		return NULL;
	layout = &s_ai_grid_layouts[slot];
	if (layout->x != entry->x || layout->y != entry->y || layout->z != entry->z || layout->yaw != entry->yaw ||
		layout->count != entry->unit_count)
	{
		layout->x = entry->x;
		layout->y = entry->y;
		layout->z = entry->z;
		layout->yaw = entry->yaw;
		layout->count = entry->unit_count;
		for (i = 0; i < entry->unit_count && i < FORGE_AI_GRID_PLACES; i++)
			forge_ai_grid_place(entry, i, &layout->points[i]);
	}
	return layout;
}

/* the spawns the level is playing, as forge_ai_level_spawns chose them. That is
asked once, as the AI is built: it uses up what a saved map brought */
static struct forge_ai_spawn_entry const *forge_ai_playing_spawns(int *count)
{
	if (g_forge_session_enabled && s_markers_scanned && g_forge.live_instance_count > 0)
	{
		if (!g_forge.edit_state.ai_spawns_edited)
			return NULL;
		*count = g_forge.edit_state.ai_spawn_count;
		return g_forge.edit_state.ai_spawns;
	}
	if (!s_saved.applied || !s_saved.has_spawns)
		return NULL;
	*count = s_saved.count;
	return s_saved.entries;
}

extern boolean tag_index_is_group(long tag_index, long group_tag);

/* whether a spawn's actor is a biped that flies (sentinels, drones): HEIGHT's */
static int forge_ai_entry_flies(struct forge_ai_spawn_entry const *entry)
{
	long unit_definition = forge_ai_actor_unit_definition(entry->actor_variant_path);

	return unit_definition != NONE && tag_index_is_group(unit_definition, 'bipd') &&
		TEST_FLAG(biped_definition_get(unit_definition)->biped.flags, _biped_flying_bit);
}

/* the point `height` over the ground under (x, y, z): 1 if there is ground */
static int forge_ai_over_ground(float x, float y, float z, float height, union real_point3d *point)
{
	unsigned long const flags = FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit);
	real_point3d start;
	real_vector3d vector;
	struct collision_result collision;

	start.x = x;
	start.y = y;
	start.z = z + 0.5f;
	vector.i = 0.0f;
	vector.j = 0.0f;
	vector.k = -100.0f;
	memset(&collision, 0, sizeof(collision));
	if (!collision_test_vector(flags, &start, &vector, NONE, &collision))
		return 0;
	point->x = x;
	point->y = y;
	point->z = collision.point.z + height;
	return 1;
}

/* (from encounter_place_actor) where a unit of a squad starts: the location, or
`place`, a copy of it where the spawn's layout has this one */
struct actor_starting_location *forge_ai_location_for_unit(
	struct actor_starting_location *location,
	boolean spawning,
	struct actor_starting_location *place)
{
	int count = 0, i;
	struct forge_ai_spawn_entry const *entries = forge_ai_playing_spawns(&count);
	struct forge_ai_grid_layout const *layout;
	int unit;

	s_ai_placing_effect = _forge_ai_effect_none;
	s_ai_placing_entry = -1;
	if (!entries)
		return location;
	for (i = 0; i < count && i < FORGE_MAX_AI_SPAWNS; i++)
	{
		if (entries[i].active && entries[i].x == location->position.x && entries[i].y == location->position.y &&
			entries[i].z == location->position.z)
		{
			break;
		}
	}
	if (i == count || i == FORGE_MAX_AI_SPAWNS)
		return location;
	s_ai_placing_effect = entries[i].spawn_effect;
	s_ai_placing_entry = i;
	*place = *location;
	if (!spawning && entries[i].unit_count >= 2 && forge_ai_entry_own_squad(&entries[i]))
	{
		unit = s_ai_grid_next[i] % entries[i].unit_count;
		s_ai_grid_next[i] = (short)(unit + 1);
		layout = forge_ai_grid_layout(&entries[i], i);
		if (unit && unit < FORGE_AI_GRID_PLACES && layout)
		{
			place->position.x = layout->points[unit].x;
			place->position.y = layout->points[unit].y;
			place->position.z = layout->points[unit].z;
		}
	}
	/* (a flyer with a HEIGHT starts there, as Forge mode shows it:
	forge_ai_spawn_hover_point) */
	if (entries[i].hover_height && forge_ai_entry_flies(&entries[i]))
	{
		union real_point3d raised;

		if (forge_ai_over_ground(place->position.x, place->position.y, place->position.z,
				(float)entries[i].hover_height, &raised))
		{
			place->position.z = raised.z;
		}
	}
	return place;
}

/* ---------- a spawn's units as they play: CLOAKED and HEIGHT

A cloaked spawn's unit has the engine's cloak, as an actor variant with active
camouflage has: it shimmers as it fires or is hit, and comes back (units.c),
and the network carries it to the clients (network_actors.c). A spawn with a
HEIGHT keeps its flying units (bipeds that fly: sentinels, drones) that high
over the ground under them, pushed up as they sink and down as they climb far
past it, the host's, each tick (forge_ai_units_update) */
#define FORGE_AI_KEPT_UNITS 512
/* (held there all the time, not pushed back once it strays, which made it
bounce: its rise or fall is the height it is off by, times P, plus what it
has been off by over time, times I, which takes up the engine's own pull
down; world units a tick) */
#define FORGE_AI_HOVER_P 0.06f
#define FORGE_AI_HOVER_I 0.002f
#define FORGE_AI_HOVER_BIAS (1.0f / (float)TICKS_PER_SECOND)
/* (the fastest it is moved, world units a tick: 2.5 a second up, 1.5 down) */
#define FORGE_AI_HOVER_RISE (2.5f / (float)TICKS_PER_SECOND)
#define FORGE_AI_HOVER_SINK (1.5f / (float)TICKS_PER_SECOND)

static struct
{
	long unit_index;
	float height;
	float bias;
} s_ai_kept[FORGE_AI_KEPT_UNITS];
static int s_ai_kept_count;
static long s_ai_kept_tick = -1;

/* HEIGHT's choices, world units over the ground (0: none, the unit starts at
its marker and flies as the engine has it) */
static int16_t const s_ai_hover_heights[] = { 0, 1, 2, 3, 4, 5, 6, 8, 10, 12, 15, 20, 25 };

/* (a height from a saved map or a peer: one of the choices, else the nearest
below it) */
static int forge_ai_hover_height_valid(int height)
{
	int i, valid = 0;

	for (i = 0; i < (int)NUMBEROF(s_ai_hover_heights); i++)
	{
		if (s_ai_hover_heights[i] <= height)
			valid = s_ai_hover_heights[i];
	}
	return valid;
}

static void forge_ai_unit_keep(long unit_index, int entry_index)
{
	int count = 0;
	struct forge_ai_spawn_entry const *entries = forge_ai_playing_spawns(&count);
	struct forge_ai_spawn_entry const *entry;
	struct unit_datum *unit;
	struct biped_definition const *definition;

	if (!entries || entry_index < 0 || entry_index >= count)
		return;
	entry = &entries[entry_index];
	unit = (struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);
	if (!unit)
		return;
	if (entry->cloaked)
	{
		SET_FLAG(unit->unit.flags, _unit_active_camouflaged_bit, TRUE);
		unit->unit.active_camouflage = 1.0f;
	}
	if (!entry->hover_height || !object_try_and_get_and_verify_type(unit_index, _object_mask_biped))
		return;
	definition = biped_definition_get(unit->definition_index);
	if (!TEST_FLAG(definition->biped.flags, _biped_flying_bit))
		return;
	if (s_ai_kept_count == FORGE_AI_KEPT_UNITS)
	{
		int i, kept = 0;

		/* (room made of those gone) */
		for (i = 0; i < s_ai_kept_count; i++)
		{
			if (object_try_and_get_and_verify_type(s_ai_kept[i].unit_index, _object_mask_biped))
				s_ai_kept[kept++] = s_ai_kept[i];
		}
		s_ai_kept_count = kept;
		if (s_ai_kept_count == FORGE_AI_KEPT_UNITS)
			return;
	}
	s_ai_kept[s_ai_kept_count].unit_index = unit_index;
	s_ai_kept[s_ai_kept_count].height = (float)entry->hover_height;
	s_ai_kept[s_ai_kept_count].bias = 0.0f;
	s_ai_kept_count++;
}

/* 1 with how high the unit is over the ground (or the water's floor) under it */
static int forge_ai_unit_altitude(long unit_index, float *altitude)
{
	unsigned long const flags = FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit);
	union real_point3d origin;
	real_vector3d vector;
	struct collision_result collision;

	object_get_origin(unit_index, &origin);
	vector.i = 0.0f;
	vector.j = 0.0f;
	vector.k = -100.0f;
	memset(&collision, 0, sizeof(collision));
	if (!collision_test_vector(flags, (real_point3d *)&origin, &vector, NONE, &collision))
		return 0;
	*altitude = origin.z - collision.point.z;
	return 1;
}

void forge_ai_units_update(void)
{
	long now = game_time_get();
	int i, kept = 0;

	if (s_ai_kept_tick >= 0 && now < s_ai_kept_tick)
	{
		/* (another level, or the same again: its units are gone) */
		s_ai_kept_count = 0;
	}
	if (now == s_ai_kept_tick)
		return;
	s_ai_kept_tick = now;
	if (game_connection() == _game_connection_network_client)
	{
		s_ai_kept_count = 0;
		return;
	}
	forge_ai_goals_update(now);
	for (i = 0; i < s_ai_kept_count; i++)
	{
		struct unit_datum *unit = (struct unit_datum *)object_try_and_get_and_verify_type(s_ai_kept[i].unit_index,
			_object_mask_biped);
		float altitude, target = s_ai_kept[i].height;
		float error, rise;

		/* (dead: let it fall) */
		if (!unit || unit->unit.actor_index == NONE)
			continue;
		s_ai_kept[kept++] = s_ai_kept[i];
		/* (held still: Forge mode's models, a vehicle's seat) */
		if (unit->object.parent_object_index != NONE || !forge_ai_unit_altitude(s_ai_kept[i].unit_index, &altitude))
			continue;
		error = target - altitude;
		s_ai_kept[kept - 1].bias += error * FORGE_AI_HOVER_I;
		if (s_ai_kept[kept - 1].bias > FORGE_AI_HOVER_BIAS)
			s_ai_kept[kept - 1].bias = FORGE_AI_HOVER_BIAS;
		else if (s_ai_kept[kept - 1].bias < -FORGE_AI_HOVER_BIAS)
			s_ai_kept[kept - 1].bias = -FORGE_AI_HOVER_BIAS;
		rise = error * FORGE_AI_HOVER_P + s_ai_kept[kept - 1].bias;
		if (rise > FORGE_AI_HOVER_RISE)
			rise = FORGE_AI_HOVER_RISE;
		else if (rise < -FORGE_AI_HOVER_SINK)
			rise = -FORGE_AI_HOVER_SINK;
		unit->object.translational_velocity.k = rise;
	}
	s_ai_kept_count = kept;
}

static void forge_ai_weapon_give(long unit_index, long weapon_definition)
{
	struct object_placement_data placement;
	long weapon_index;

	if (!unit_can_hold_weapon_definition(unit_index, weapon_definition))
		return;
	object_placement_data_new(&placement, weapon_definition, unit_index);
	weapon_index = object_new(&placement);
	if (weapon_index != NONE && !unit_add_weapon_to_inventory(unit_index, weapon_index, _unit_add_weapon_replace))
		object_delete(weapon_index);
}

/* (from actor_place, the unit armed as its actor is) */
void forge_ai_unit_placed(long unit_index, struct actor_starting_location const *location)
{
	short slot = (short)(location->flags >> FORGE_AI_LOCATION_WEAPON_SHIFT);

	if (s_ai_units_placed_logged < 4)
	{
		s_ai_units_placed_logged++;
		error(_error_silent, "forge ai: a unit placed at (%.1f %.1f %.1f)", location->position.x,
			location->position.y, location->position.z);
	}

	if (slot > 0 && slot <= s_built.weapon_count)
		forge_ai_weapon_give(unit_index, s_built.weapons[slot - 1]);
	forge_ai_unit_keep(unit_index, s_ai_placing_entry);
	s_ai_placing_entry = -1;
	/* (not as the level loads: there is no one to see it, and no clients yet) */
	if (s_ai_placing_effect != _forge_ai_effect_none && game_time_get() > 0)
	{
		if (s_ai_placing_effect == _forge_ai_effect_cloak)
		{
			struct unit_datum *unit = (struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);

			/* (without the flag it fades, in the four seconds the engine takes) */
			if (unit)
				unit->unit.active_camouflage = 1.0f;
		}
		else
		{
			union real_point3d origin;

			object_get_origin(unit_index, &origin);
			forge_ai_spawn_effect_play(s_ai_placing_effect, &origin);
			forge_network_send_spawn_effect(s_ai_placing_effect, &origin);
		}
	}
	s_ai_placing_effect = _forge_ai_effect_none;
}

/* any weapon in any AI's hands: its animations are borrowed, as the campaign
randomizer's crazy weapons do (randomizer_borrowed_weapon_label), in a level
with spawns armed, and while they stand as models in Forge */
int forge_ai_weapons_borrowed(void)
{
	return s_built.weapon_count > 0 || (g_forge_session_enabled && forge_ai_map_editable());
}

/* the animations of the weapon a unit's actor carries, for a weapon none of
the labels nearest it fit: what any armed AI can always hold something with */
char const *forge_ai_unit_own_weapon_label(long unit_index)
{
	struct unit_datum *unit = unit_try_and_get(unit_index);
	long count, i;
	struct forge_ai_tag_instance *instances = forge_ai_tag_instances(&count);

	if (!unit)
		return NULL;
	for (i = 0; instances && i < count; i++)
	{
		struct actor_variant_definition *variant;

		if ((uint32_t)instances[i].group_tag != 'actv' || !instances[i].base_address)
			continue;
		variant = (struct actor_variant_definition *)tag_get('actv', instances[i].tag_index);
		if (variant->unit_reference.index == unit->definition_index &&
			variant->ranged_combat.reference.index != NONE)
		{
			return weapon_definition_get(variant->ranged_combat.reference.index)->weapon.label;
		}
	}
	return NULL;
}

/* ---------- saved spawns */

static void forge_ai_saved_tags_parse(char const *text, char const *line, uint32_t group_tag)
{
	char const *cur = text;
	size_t line_size = strlen(line);

	while (s_saved.actors && (cur = strstr(cur, line)) != NULL && s_saved.actor_count < FORGE_MAX_AI_ACTORS)
	{
		struct forge_ai_actor_entry *actor = &s_saved.actors[s_saved.actor_count];

		cur += line_size;
		memset(actor, 0, sizeof(*actor));
		actor->group_tag = group_tag;
		if (sscanf(cur, "%31[^|]|%255[^\r\n]", actor->map, actor->path) == 2)
			s_saved.actor_count++;
	}
}

void forge_ai_saved_parse(char const *text)
{
	char const *cur;
	int replace_own = 0;

	s_saved.count = 0;
	s_saved.actor_count = 0;
	s_saved.has_spawns = 0;
	s_saved.fresh = 1;
	/* (the AI PATROL volumes and routes the spawns' goals go by) */
	forge_ai_saved_goals_parse(text);
	if (!s_saved.actors)
		s_saved.actors = (struct forge_ai_actor_entry *)calloc(FORGE_MAX_AI_ACTORS, sizeof(*s_saved.actors));
	forge_ai_saved_tags_parse(text, "AI_ACTOR|", 'actv');
	forge_ai_saved_tags_parse(text, "AI_WEAPON|", 'weap');
	cur = strstr(text, "AI_SPAWNS_SAVED|");
	if (!cur || sscanf(cur + 16, "%d", &replace_own) != 1)
		return;
	if (!s_saved.entries)
		s_saved.entries = (struct forge_ai_spawn_entry *)calloc(FORGE_MAX_AI_SPAWNS, sizeof(*s_saved.entries));
	if (!s_saved.entries)
		return;
	while ((cur = strstr(cur, "AI_SPAWN|")) != NULL && s_saved.count < FORGE_MAX_AI_SPAWNS)
	{
		struct forge_ai_spawn_entry *entry = &s_saved.entries[s_saved.count];
		int team = 0, count = 0, alive = 0, delay = 0, total = 0, encounter = 0, squad = 0, location = 0, detached = 0;
		unsigned int gametypes = 0;
		int fields;

		memset(entry, 0, sizeof(*entry));
		fields = sscanf(cur + 9, "%f|%f|%f|%f|%d|%d|%d|%d|%d|%u|%d|%d|%d|%d|%255[^|\r\n]|%255[^|\r\n]",
			&entry->x, &entry->y, &entry->z, &entry->yaw, &team, &count, &alive, &delay, &total, &gametypes,
			&encounter, &squad, &location, &detached, entry->actor_variant_path, entry->weapon_path);
		cur += 9;
		if (fields < 15)
			continue;
		/* (last on the line, as a map from before it has none, and a spawn with
		no weapon of its own stops the scan short of it) */
		{
			char const *effect = strstr(cur, "|fx=");

			if (effect && effect < cur + strcspn(cur, "\r\n"))
			{
				int index = atoi(effect + 4);

				if (index > 0 && index < (int)NUMBEROF(s_ai_spawn_effects))
					entry->spawn_effect = (uint8_t)index;
			}
		}
		/* (after it: CLOAKED and HEIGHT, none on a map saved before them) */
		{
			char const *end = cur + strcspn(cur, "\r\n");
			char const *cloaked = strstr(cur, "|ck=");
			char const *height = strstr(cur, "|ht=");

			if (cloaked && cloaked < end)
				entry->cloaked = atoi(cloaked + 4) ? 1 : 0;
			if (height && height < end)
				entry->hover_height = (uint8_t)forge_ai_hover_height_valid(atoi(height + 4));
		}
		/* (after those: the GOAL, none on a map saved before it) */
		{
			static char const *const keys[6] = { "|gl=", "|gn=", "|gw=", "|gs=", "|gf=", "|ga=" };
			uint8_t *fields[6];
			char const *end = cur + strcspn(cur, "\r\n");
			int k;

			fields[0] = &entry->goal;
			fields[1] = &entry->goal_link;
			fields[2] = &entry->goal_who;
			fields[3] = &entry->goal_start;
			fields[4] = &entry->goal_follow;
			fields[5] = &entry->goal_alert;
			for (k = 0; k < 6; k++)
			{
				char const *at = strstr(cur, keys[k]);

				if (at && at < end)
					*fields[k] = (uint8_t)PIN(atoi(at + 4), 0, 255);
			}
			if (entry->goal == FORGE_AI_GOAL_LEGACY_FOLLOW)
				entry->goal = _forge_ai_goal_hunt;
			if (entry->goal >= NUMBER_OF_FORGE_AI_GOALS)
				entry->goal = _forge_ai_goal_none;
		}
		entry->team = (int16_t)team;
		entry->unit_count = (int16_t)count;
		entry->respawn_alive = (int16_t)alive;
		entry->respawn_delay_seconds = (int16_t)delay;
		entry->respawn_total = (int16_t)total;
		entry->gametype_flags = (uint16_t)gametypes;
		entry->encounter_index = (int16_t)encounter;
		entry->squad_index = (int16_t)squad;
		entry->location_index = (int16_t)location;
		entry->detached = (uint8_t)detached;
		entry->active = 1;
		s_saved.count++;
	}
	s_saved.replace_own = replace_own;
	s_saved.has_spawns = 1;
}

/* a level that is no saved map */
void forge_ai_saved_clear(void)
{
	memset(&s_saved_goals, 0, sizeof(s_saved_goals));
	s_saved.count = 0;
	s_saved.actor_count = 0;
	s_saved.has_spawns = 0;
	s_saved.fresh = 0;
}

int forge_ai_saved_actor_count(void)
{
	return s_saved.actor_count;
}

char const *forge_ai_saved_actor_path(int index)
{
	return s_saved.actors[index].path;
}

char const *forge_ai_saved_actor_map(int index)
{
	return s_saved.actors[index].map;
}

unsigned long forge_ai_saved_actor_group(int index)
{
	return s_saved.actors[index].group_tag;
}

/* ---------- markers */

static int forge_ai_marker_add(int entry_index)
{
	struct forge_ai_spawn_entry const *entry = &g_forge.edit_state.ai_spawns[entry_index];
	struct forge_live_instance *inst;

	if (g_forge.live_instance_count >= FORGE_MAX_LIVE_INSTANCES)
		return 0;
	inst = &g_forge.live_instances[g_forge.live_instance_count++];
	forge_network_slot_made((int32_t)(g_forge.live_instance_count - 1)); /* AE hook */
	memset(inst, 0, sizeof(*inst));
	inst->active = 1;
	inst->object_index = NONE;
	inst->actor_index = NONE;
	inst->edit_entry_index = entry_index;
	inst->group_tag = FORGE_GROUP_AI_SPAWN;
	inst->is_default_scenario_object = 1;
	inst->netgame_flag_type = -1;
	inst->team_designator = entry->team;
	inst->orig_x = inst->x = entry->x;
	inst->orig_y = inst->y = entry->y;
	inst->orig_z = inst->z = entry->z;
	inst->yaw = entry->yaw;
	snprintf(inst->tag_path, sizeof(inst->tag_path), "%s", entry->actor_variant_path);
	return 1;
}

/* the level's spawns as markers, in the same order on every machine: a saved
map's, else its starting locations as the map has them. A map with more than
Forge can show (a campaign level's) keeps its own, and takes new ones */
void forge_ai_markers_scan(void)
{
	struct forge_scenario_edit_state *edits = &g_forge.edit_state;
	struct scenario *scen = global_scenario_try_and_get();
	struct encounter_definition *encounters;
	struct tag_reference *palette;
	long encounter_count = 0, palette_count = 0, total = 0;
	long e, s, l;
	int i;

	if (!scen || !g_forge_session_enabled || !forge_ai_map_editable())
		return;
	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		if (g_forge.live_instances[i].active && g_forge.live_instances[i].group_tag == FORGE_GROUP_AI_SPAWN)
			return;
	}
	edits->ai_spawn_count = 0;
	edits->ai_spawns_edited = 0;
	edits->replace_ai_spawns = 0;
	s_markers_scanned = 1;
	if (s_saved.applied && s_saved.has_spawns)
	{
		edits->ai_spawn_count = s_saved.count;
		edits->replace_ai_spawns = s_saved.replace_own;
		edits->ai_spawns_edited = 1;
		memcpy(edits->ai_spawns, s_saved.entries, (size_t)s_saved.count * sizeof(*edits->ai_spawns));
	}
	else
	{
		encounters = forge_ai_original_encounters(scen, &encounter_count);
		palette = forge_ai_original_palette(scen, &palette_count);
		for (e = 0; e < encounter_count; e++)
		{
			struct squad_definition const *squads = XBOX_POINTER(struct squad_definition const, encounters[e].squads.address); /* AE hook */

			for (s = 0; s < encounters[e].squads.count; s++)
				total += squads[s].starting_locations.count;
		}
		edits->replace_ai_spawns = total <= FORGE_MAX_AI_SPAWNS;
		if (!edits->replace_ai_spawns)
		{
			platform_log("Forge: %ld AI starting locations are more than Forge shows (%d): the map keeps them",
				total, FORGE_MAX_AI_SPAWNS);
			encounter_count = 0;
		}
		for (e = 0; e < encounter_count; e++)
		{
			struct squad_definition const *squads = XBOX_POINTER(struct squad_definition const, encounters[e].squads.address); /* AE hook */

			for (s = 0; s < encounters[e].squads.count; s++)
			{
				struct squad_definition const *squad = &squads[s];
				struct actor_starting_location const *locations =
					XBOX_POINTER(struct actor_starting_location const, squad->starting_locations.address); /* AE hook */

				for (l = 0; l < squad->starting_locations.count; l++)
				{
					struct forge_ai_spawn_entry *entry = &edits->ai_spawns[edits->ai_spawn_count];
					short index = locations[l].actor_variant_index != NONE ?
						locations[l].actor_variant_index : squad->actor_palette_index;

					if (index < 0 || index >= palette_count || palette[index].index == NONE)
						continue;
					memset(entry, 0, sizeof(*entry));
					snprintf(entry->actor_variant_path, sizeof(entry->actor_variant_path), "%s",
						tag_get_name(palette[index].index));
					entry->x = locations[l].position.x;
					entry->y = locations[l].position.y;
					entry->z = locations[l].position.z;
					entry->yaw = locations[l].facing;
					entry->team = encounters[e].team_index;
					entry->unit_count = squad->min_count;
					entry->respawn_alive = MAX(squad->respawn_min_actors, squad->respawn_max_actors);
					entry->respawn_delay_seconds = (int16_t)squad->respawn_time_upper_bound;
					entry->respawn_total = squad->respawn_total_count;
					entry->encounter_index = (int16_t)e;
					entry->squad_index = (int16_t)s;
					entry->location_index = (int16_t)l;
					entry->active = 1;
					edits->ai_spawn_count++;
				}
			}
		}
	}
	for (i = 0; i < edits->ai_spawn_count; i++)
	{
		if (edits->ai_spawns[i].active && !forge_ai_marker_add(i))
			break;
	}
}

static struct forge_ai_spawn_entry *forge_ai_entry_of(struct forge_live_instance const *inst)
{
	if (inst->group_tag != FORGE_GROUP_AI_SPAWN || inst->edit_entry_index < 0 ||
		inst->edit_entry_index >= g_forge.edit_state.ai_spawn_count)
	{
		return NULL;
	}
	return &g_forge.edit_state.ai_spawns[inst->edit_entry_index];
}

static struct forge_ai_spawn_entry *forge_ai_entry_live(int32_t live_idx)
{
	if (live_idx < 0 || live_idx >= g_forge.live_instance_count || !g_forge.live_instances[live_idx].active)
		return NULL;
	return forge_ai_entry_of(&g_forge.live_instances[live_idx]);
}

/* a new spawn's entry: its index, -1 with no room */
static char const *forge_ai_group_first_path(int group)
{
	int i;

	for (i = 0; i < g_forge.catalog_count; i++)
	{
		struct forge_catalog_entry const *row = &g_forge.catalog[i];

		if (row->group_tag == FORGE_GROUP_AI_SPAWN && !forge_ai_row_is_random(row) && forge_ai_row_group(row) == group)
			return row->tag_path;
	}
	return NULL;
}

int forge_ai_spawn_entry_new(int32_t live_idx, char const *variant_path, float x, float y, float z, float yaw, int16_t team)
{
	struct forge_scenario_edit_state *edits = &g_forge.edit_state;
	struct forge_ai_spawn_entry *entry;
	int random_group = forge_ai_path_group(variant_path, FORGE_AI_RANDOM_PATH);
	int index;

	(void)live_idx;
	for (index = 0; index < edits->ai_spawn_count && edits->ai_spawns[index].active; index++)
	{
	}
	if (index >= FORGE_MAX_AI_SPAWNS)
		return -1;
	if (index == edits->ai_spawn_count)
		edits->ai_spawn_count++;
	entry = &edits->ai_spawns[index];
	memset(entry, 0, sizeof(*entry));
	/* (RANDOM: its group's first rank stands for the group, as model and tag) */
	if (random_group != NONE)
	{
		variant_path = forge_ai_group_first_path(random_group);
		if (!variant_path)
			return -1;
		forge_ai_random_tags_need(random_group);
		entry->gametype_flags = FORGE_AI_RANDOM_RANK;
	}
	/* (Firefight is chosen, on a spawn's GAME MODE row) */
	entry->gametype_flags |= FORGE_AI_MODES_NOT_FIREFIGHT;
	snprintf(entry->actor_variant_path, sizeof(entry->actor_variant_path), "%s", variant_path);
	entry->x = x;
	entry->y = y;
	entry->z = z;
	entry->yaw = yaw;
	entry->team = team;
	entry->unit_count = 1;
	entry->respawn_delay_seconds = 30;
	entry->goal_start = FORGE_AI_GOAL_START_DEFAULT;
	entry->goal_follow = FORGE_AI_GOAL_FOLLOW_DEFAULT;
	entry->encounter_index = -1;
	entry->squad_index = -1;
	entry->location_index = -1;
	entry->active = 1;
	edits->ai_spawns_edited = 1;
	return index;
}

void forge_ai_spawn_moved(struct forge_live_instance const *inst)
{
	struct forge_ai_spawn_entry *entry = forge_ai_entry_of(inst);

	if (entry && (entry->x != inst->x || entry->y != inst->y || entry->z != inst->z || entry->yaw != inst->yaw))
	{
		entry->x = inst->x;
		entry->y = inst->y;
		entry->z = inst->z;
		entry->yaw = inst->yaw;
		g_forge.edit_state.ai_spawns_edited = 1;
	}
}

int forge_ai_spawn_settings_get(int32_t live_idx, struct forge_ai_spawn_entry *settings)
{
	struct forge_ai_spawn_entry *entry = forge_ai_entry_live(live_idx);

	if (!entry)
		return 0;
	*settings = *entry;
	return 1;
}

void forge_ai_spawn_settings_set(int32_t live_idx, struct forge_ai_spawn_entry const *settings)
{
	struct forge_ai_spawn_entry *entry = forge_ai_entry_live(live_idx);
	struct forge_live_instance *inst;
	float x, y, z, yaw;

	if (!entry)
		return;
	inst = &g_forge.live_instances[live_idx];
	/* (where it is stays as it is: the object's own to say) */
	x = entry->x;
	y = entry->y;
	z = entry->z;
	yaw = entry->yaw;
	*entry = *settings;
	entry->x = x;
	entry->y = y;
	entry->z = z;
	entry->yaw = yaw;
	snprintf(inst->tag_path, sizeof(inst->tag_path), "%s", entry->actor_variant_path);
	inst->team_designator = entry->team;
	g_forge.edit_state.ai_spawns_edited = 1;
	forge_ai_preview_update(live_idx);
	forge_network_send_ai_spawn(live_idx);
}

void forge_ai_spawn_deleted(struct forge_live_instance const *inst)
{
	struct forge_ai_spawn_entry *entry = forge_ai_entry_of(inst);

	if (entry)
	{
		memset(entry, 0, sizeof(*entry));
		g_forge.edit_state.ai_spawns_edited = 1;
	}
}

/* a copy is a spawn of its own, whatever it was copied from */
void forge_ai_spawn_copy_settings(int source_entry, int32_t live_idx)
{
	struct forge_ai_spawn_entry *entry = forge_ai_entry_live(live_idx);
	struct forge_ai_spawn_entry const *source;

	if (!entry || source_entry < 0 || source_entry >= g_forge.edit_state.ai_spawn_count)
		return;
	source = &g_forge.edit_state.ai_spawns[source_entry];
	snprintf(entry->weapon_path, sizeof(entry->weapon_path), "%s", source->weapon_path);
	entry->team = source->team;
	entry->unit_count = source->unit_count;
	entry->respawn_alive = source->respawn_alive;
	entry->respawn_delay_seconds = source->respawn_delay_seconds;
	entry->respawn_total = source->respawn_total;
	entry->gametype_flags = source->gametype_flags;
	entry->spawn_effect = source->spawn_effect;
	entry->cloaked = source->cloaked;
	entry->hover_height = source->hover_height;
	entry->goal = source->goal;
	entry->goal_link = source->goal_link;
	entry->goal_who = source->goal_who;
	entry->goal_start = source->goal_start;
	entry->goal_follow = source->goal_follow;
	entry->goal_alert = source->goal_alert;
	g_forge.live_instances[live_idx].team_designator = entry->team;
	forge_ai_preview_update(live_idx);
	forge_network_send_ai_spawn(live_idx);
}

/* ---------- the properties panel's rows */

static char const *forge_ai_path_leaf(char const *path)
{
	char const *slash = strrchr(path, '\\');

	return slash ? slash + 1 : path;
}

static void forge_ai_upper_copy(char *text, size_t text_size, char const *source)
{
	size_t i;

	for (i = 0; i + 1 < text_size && source[i]; i++)
		text[i] = (char)toupper((unsigned char)source[i]);
	if (text_size)
		text[i] = 0;
}

static char const *forge_ai_team_name(int team)
{
	switch (team)
	{
	case _game_team_default:  return "UNIT'S OWN";
	case _game_team_player:   return "PLAYER";
	case _game_team_human:    return "HUMAN";
	case _game_team_covenant: return "COVENANT";
	case _game_team_flood:    return "FLOOD";
	case _game_team_sentinel: return "SENTINEL";
	case FORGE_AI_TEAM_ALL_PLAYERS: return "ALL PLAYERS";
	case FORGE_AI_TEAM_RED:   return "RED TEAM";
	case FORGE_AI_TEAM_BLUE:  return "BLUE TEAM";
	default:                  return "OTHER";
	}
}

/* the TEAM row's order: the players' teams beside PLAYER */
static int16_t const s_ai_team_order[] =
{
	_game_team_default, _game_team_player, FORGE_AI_TEAM_RED, FORGE_AI_TEAM_BLUE, FORGE_AI_TEAM_ALL_PLAYERS,
	_game_team_human, _game_team_covenant, _game_team_flood, _game_team_sentinel
};

static uint16_t const s_ai_gametypes[] =
{
	FORGE_AI_MODES_ALL, 0x01u, 0x02u, 0x04u, 0x08u, 0x10u, FORGE_AI_MODE_FIREFIGHT, FORGE_AI_MODES_NOT_FIREFIGHT
};
static char const *const s_ai_gametype_names[] =
{
	"ALL MODES", "CTF ONLY", "SLAYER ONLY", "ODDBALL ONLY", "KOTH ONLY", "RACE ONLY", "FIREFIGHT ONLY", "ALL BUT FIREFIGHT"
};
static int16_t const s_ai_delays[] = { 5, 10, 15, 20, 30, 45, 60, 90, 120, 180, 300 };
static int16_t const s_ai_totals[] = { 0, 1, 2, 3, 5, 10, 20, 50 };

static int forge_ai_table_index(int16_t const *table, int count, int value)
{
	int i;

	for (i = 0; i < count; i++)
	{
		if (table[i] >= value)
			return i;
	}
	return count - 1;
}

/* a spawn's actor as Forge calls it: its kind, or the group's random */
static void forge_ai_entry_name(struct forge_ai_spawn_entry const *entry, char *text, size_t text_size)
{
	int kind = forge_ai_kind_of(entry->actor_variant_path);

	if (entry->gametype_flags & FORGE_AI_RANDOM_RANK)
		snprintf(text, text_size, "%s: RANDOM", forge_ai_group_name(forge_ai_kind_group(kind)));
	else if (kind >= 0)
		snprintf(text, text_size, "%s", forge_ai_kind_name(kind));
	else
		forge_ai_upper_copy(text, text_size, forge_ai_path_leaf(entry->actor_variant_path));
}

/* ---------- an AI spawn's GOAL, as its panel shows and sets it */

static char const *const s_ai_goal_names[NUMBER_OF_FORGE_AI_GOALS] =
{
	"NONE", "PATROL VOLUME", "ROUTE", "GO TO PLAYER"
};
static char const *const s_ai_goal_who_names[NUMBER_OF_FORGE_AI_GOAL_WHO] = { "ANY PLAYER", "RED TEAM", "BLUE TEAM" };
static int16_t const s_ai_goal_distances[] = { 0, 1, 2, 3, 4, 5, 6, 8, 10, 12, 15, 20, 25, 30, 40, 50, 75, 100 };
/* ALERT TIME's seconds */
static int16_t const s_ai_goal_alerts[] = { 3, 5, 8, 10, 12, 15, 20, 30, 45, 60, 90, 120 };

/* the panel's page (a FORGE_AI_PAGE_*) and the spawn it is open for (-1: the
first page), and the page row a BACK returned to */
enum
{
	FORGE_AI_PAGE_MAIN = 0,
	FORGE_AI_PAGE_UNIT,
	FORGE_AI_PAGE_SPAWN,
	FORGE_AI_PAGE_GOAL
};
static int32_t s_page_live_idx = -1;
static int s_page = FORGE_AI_PAGE_MAIN;
static int s_page_return_row = _forge_ai_row_unit_page;

/* whether a spawn's team is a players' (red, blue, all players): its GO TO
PLAYER has a STOP DISTANCE */
static int forge_ai_entry_friendly(struct forge_ai_spawn_entry const *entry)
{
	return entry->team == FORGE_AI_TEAM_RED || entry->team == FORGE_AI_TEAM_BLUE ||
		entry->team == FORGE_AI_TEAM_ALL_PLAYERS;
}

static int forge_ai_entry_alert_seconds(struct forge_ai_spawn_entry const *entry)
{
	return entry->goal_alert ? entry->goal_alert : FORGE_AI_GOAL_ALERT_DEFAULT;
}

/* whether a spawn's PATROL VOLUME or ROUTE is there, in Forge */
static int forge_ai_goal_link_exists(struct forge_ai_spawn_entry const *entry)
{
	int32_t points[1];

	if (entry->goal == _forge_ai_goal_patrol)
		return forge_volume_patrol_find(entry->goal_link) >= 0;
	if (entry->goal == _forge_ai_goal_route)
		return forge_ai_route_points(entry->goal_link, points, 1) > 0;
	return 1;
}

static void forge_ai_goal_summary(struct forge_ai_spawn_entry const *entry, char *text, size_t size)
{
	switch (entry->goal)
	{
	case _forge_ai_goal_patrol:
		snprintf(text, size, "PATROL #%d%s", (int)entry->goal_link, forge_ai_goal_link_exists(entry) ? "" : "?");
		break;
	case _forge_ai_goal_route:
		snprintf(text, size, "ROUTE #%d%s", (int)entry->goal_link, forge_ai_goal_link_exists(entry) ? "" : "?");
		break;
	case _forge_ai_goal_hunt:
		snprintf(text, size, "GO TO %s", s_ai_goal_who_names[FORGE_AI_GOAL_WHO(entry->goal_who)]);
		break;
	default:
		snprintf(text, size, "NONE");
		break;
	}
}

/* the GOAL row stepped: PATROL VOLUME only while there is an AI PATROL
volume, ROUTE while there is a route (each then linked to the first); 0 when
there is nothing to step to */
static int forge_ai_goal_step(struct forge_ai_spawn_entry *entry, int dir)
{
	int goal = entry->goal;
	int tries;

	for (tries = 0; tries < NUMBER_OF_FORGE_AI_GOALS; tries++)
	{
		int numbers[FORGE_AI_ROUTE_MAX];
		int count = 0;

		goal = (goal + NUMBER_OF_FORGE_AI_GOALS + dir) % NUMBER_OF_FORGE_AI_GOALS;
		if (goal == _forge_ai_goal_patrol)
			count = forge_volumes_patrol_numbers(numbers, FORGE_AI_ROUTE_MAX);
		else if (goal == _forge_ai_goal_route)
			count = forge_ai_route_numbers(numbers, FORGE_AI_ROUTE_MAX);
		else
			break;
		if (count > 0)
		{
			/* (the one it had, if it is still there) */
			int k, keep = 0;

			for (k = 0; k < count; k++)
				keep = keep || (goal == entry->goal && numbers[k] == entry->goal_link);
			if (!keep)
				entry->goal_link = (uint8_t)numbers[0];
			break;
		}
	}
	if (goal == entry->goal)
		return 0;
	entry->goal = (uint8_t)goal;
	if (goal == _forge_ai_goal_none)
		entry->goal_link = 0;
	if (!entry->goal_start && !entry->goal_follow)
	{
		entry->goal_start = FORGE_AI_GOAL_START_DEFAULT;
		entry->goal_follow = FORGE_AI_GOAL_FOLLOW_DEFAULT;
	}
	/* (goal_start is GO TO PLAYER's RANGE: its default as it is chosen) */
	if (goal == _forge_ai_goal_hunt)
		entry->goal_start = FORGE_AI_GOAL_HUNT_RANGE_DEFAULT;
	forge_set_status("%s", goal == _forge_ai_goal_patrol ? "Patrol: each unit roams the volume's ground, point to point" :
		goal == _forge_ai_goal_route ? "Route: a leader walks it, the rest follow the leader" :
		goal == _forge_ai_goal_hunt ? "Go to player: they go to the nearest player in their RANGE" :
		"No goal: they stand, and fight from around the spawn");
	return 1;
}

void forge_ai_spawn_row_text(int32_t live_idx, int row, char *label, size_t label_size, char *value, size_t value_size)
{
	struct forge_ai_spawn_entry const *entry = forge_ai_entry_live(live_idx);
	int i;

	label[0] = 0;
	value[0] = 0;
	if (!entry)
		return;
	switch (row)
	{
	case _forge_ai_row_variant:
		snprintf(label, label_size, "AI:");
		forge_ai_entry_name(entry, value, value_size);
		break;
	case _forge_ai_row_weapon:
		snprintf(label, label_size, "LOADOUT:");
		if (entry->weapon_path[0])
			forge_ai_upper_copy(value, value_size, forge_ai_path_leaf(entry->weapon_path));
		else
			snprintf(value, value_size, "AI'S OWN");
		break;
	case _forge_ai_row_team:
		snprintf(label, label_size, "TEAM:");
		snprintf(value, value_size, "%s", forge_ai_team_name(entry->team));
		break;
	case _forge_ai_row_start_count:
		snprintf(label, label_size, "AT START:");
		snprintf(value, value_size, "%d", (int)entry->unit_count);
		break;
	case _forge_ai_row_respawn_alive:
		snprintf(label, label_size, "RESPAWN:");
		if (entry->respawn_alive > 0)
			snprintf(value, value_size, "KEEP %d ALIVE", (int)entry->respawn_alive);
		else
			snprintf(value, value_size, "OFF");
		break;
	case _forge_ai_row_respawn_delay:
		snprintf(label, label_size, "RESPAWN DELAY:");
		snprintf(value, value_size, "%d SECONDS", (int)entry->respawn_delay_seconds);
		break;
	case _forge_ai_row_respawn_total:
		snprintf(label, label_size, "RESPAWN LIMIT:");
		if (entry->respawn_total > 0)
			snprintf(value, value_size, "%d", (int)entry->respawn_total);
		else
			snprintf(value, value_size, "ENDLESS");
		break;
	case _forge_ai_row_game_mode:
		snprintf(label, label_size, "GAME MODE:");
		for (i = 0; i < (int)NUMBEROF(s_ai_gametypes) && s_ai_gametypes[i] != forge_ai_entry_modes(entry); i++)
		{
		}
		snprintf(value, value_size, "%s", s_ai_gametype_names[i < (int)NUMBEROF(s_ai_gametypes) ? i : 0]);
		break;
	case _forge_ai_row_spawn_effect:
		snprintf(label, label_size, "SPAWN EFFECT:");
		snprintf(value, value_size, "%s", s_ai_spawn_effects[entry->spawn_effect < NUMBEROF(s_ai_spawn_effects) ?
			entry->spawn_effect : 0].label);
		break;
	case _forge_ai_row_cloaked:
		snprintf(label, label_size, "CLOAKED:");
		snprintf(value, value_size, "%s", entry->cloaked ? "ON" : "OFF");
		break;
	case _forge_ai_row_hover_height:
		snprintf(label, label_size, "FLYING HEIGHT:");
		/* (0: where it starts, at the marker, and not held) */
		snprintf(value, value_size, "%d UNITS", (int)entry->hover_height);
		break;
	case _forge_ai_row_unit_page:
		snprintf(label, label_size, "UNIT SETTINGS");
		forge_ai_entry_name(entry, value, value_size);
		break;
	case _forge_ai_row_spawn_page:
		snprintf(label, label_size, "SPAWN SETTINGS");
		if (entry->respawn_alive > 0)
			snprintf(value, value_size, "%d, KEEP %d", (int)entry->unit_count, (int)entry->respawn_alive);
		else
			snprintf(value, value_size, "%d AT START", (int)entry->unit_count);
		break;
	case _forge_ai_row_goal_page:
		snprintf(label, label_size, "GOAL SETTINGS");
		forge_ai_goal_summary(entry, value, value_size);
		break;
	case _forge_ai_row_goal:
		snprintf(label, label_size, "GOAL:");
		snprintf(value, value_size, "%s", s_ai_goal_names[entry->goal < NUMBER_OF_FORGE_AI_GOALS ? entry->goal : 0]);
		break;
	case _forge_ai_row_goal_target:
		snprintf(label, label_size, entry->goal == _forge_ai_goal_route ? "ROUTE:" : "PATROL VOLUME:");
		snprintf(value, value_size, "#%d%s", (int)entry->goal_link, forge_ai_goal_link_exists(entry) ? "" : " (MISSING)");
		break;
	case _forge_ai_row_goal_who:
		snprintf(label, label_size, "PLAYER:");
		snprintf(value, value_size, "%s", s_ai_goal_who_names[FORGE_AI_GOAL_WHO(entry->goal_who)]);
		break;
	case _forge_ai_row_goal_speed:
		snprintf(label, label_size, "SPEED:");
		snprintf(value, value_size, "%s", FORGE_AI_GOAL_RUNS(entry->goal_who) ? "RUN" : "WALK");
		break;
	case _forge_ai_row_goal_start:
		snprintf(label, label_size, "RANGE:");
		if (entry->goal_start == 0)
			snprintf(value, value_size, "ANYWHERE");
		else
			snprintf(value, value_size, "%d UNITS", (int)entry->goal_start);
		break;
	case _forge_ai_row_goal_follow:
		snprintf(label, label_size, "STOP DISTANCE:");
		snprintf(value, value_size, "%d UNITS", (int)entry->goal_follow);
		break;
	case _forge_ai_row_goal_alert:
		snprintf(label, label_size, "ALERT TIME:");
		snprintf(value, value_size, "%d SECONDS", forge_ai_entry_alert_seconds(entry));
		break;
	case _forge_ai_row_goal_back:
		snprintf(label, label_size, "< BACK");
		break;
	}
}

/* the next of the palette's actors or weapons: `dir` on from `path`, wrapping
through none (an empty path) when `none_allowed` */
static void forge_ai_tag_step(uint32_t group_tag, char *path, size_t path_size, int dir, int none_allowed)
{
	uint32_t palette_group = group_tag == 'actv' ? FORGE_GROUP_AI_SPAWN : group_tag;
	int first = -1, last = -1, previous = -1, next = -1, found = 0;
	int i;

	for (i = 0; i < g_forge.catalog_count; i++)
	{
		struct forge_catalog_entry const *entry = &g_forge.catalog[i];

		if (entry->group_tag != palette_group || !forge_catalog_entry_allowed(entry))
			continue;
		if (first < 0)
			first = i;
		last = i;
		if (found && next < 0)
			next = i;
		if (!found && path[0] && !_stricmp(entry->tag_path, path))
			found = 1;
		else if (!found)
			previous = i;
	}
	if (first < 0)
		return;
	if (!found)
	{
		/* (none, or a tag the palette has not got) */
		i = dir > 0 ? first : last;
	}
	else
	{
		i = dir > 0 ? next : previous;
		if (i < 0)
		{
			if (none_allowed)
			{
				path[0] = 0;
				return;
			}
			i = dir > 0 ? first : last;
		}
	}
	snprintf(path, path_size, "%s", g_forge.catalog[i].tag_path);
}

/* the spawn's next actor of its group, RANDOM among them */
static void forge_ai_variant_step(struct forge_ai_spawn_entry *entry, int dir)
{
	int kind = forge_ai_kind_of(entry->actor_variant_path);
	int group = forge_ai_kind_group(kind);
	int random = (entry->gametype_flags & FORGE_AI_RANDOM_RANK) != 0;
	short rows[64];
	int count = 0, place = 0, i;
	struct forge_catalog_entry const *row;

	for (i = 0; i < g_forge.catalog_count && count < (int)NUMBEROF(rows); i++)
	{
		row = &g_forge.catalog[i];
		if (row->group_tag != FORGE_GROUP_AI_SPAWN || !forge_catalog_entry_allowed(row) || forge_ai_row_group(row) != group)
			continue;
		if (forge_ai_row_is_random(row) ? random :
			(!random && (kind >= 0 ? forge_ai_kind_of(row->tag_path) == kind : !_stricmp(row->tag_path, entry->actor_variant_path))))
		{
			place = count;
		}
		rows[count++] = (short)i;
	}
	if (!count)
		return;
	row = &g_forge.catalog[rows[(place + count + dir) % count]];
	if (forge_ai_row_is_random(row))
	{
		char const *first = forge_ai_group_first_path(group);

		if (!first)
			return;
		snprintf(entry->actor_variant_path, sizeof(entry->actor_variant_path), "%s", first);
		entry->gametype_flags |= FORGE_AI_RANDOM_RANK;
		forge_ai_random_tags_need(group);
	}
	else
	{
		snprintf(entry->actor_variant_path, sizeof(entry->actor_variant_path), "%s", row->tag_path);
		entry->gametype_flags &= ~FORGE_AI_RANDOM_RANK;
	}
}

int forge_ai_spawn_row_step(int32_t live_idx, int row, int dir)
{
	struct forge_ai_spawn_entry *entry = forge_ai_entry_live(live_idx);
	struct forge_live_instance *inst;
	int index;

	if (!entry || !dir)
		return 0;
	/* (the pages, opened and closed) */
	if (row == _forge_ai_row_unit_page || row == _forge_ai_row_spawn_page || row == _forge_ai_row_goal_page)
	{
		s_page_live_idx = live_idx;
		s_page = row == _forge_ai_row_unit_page ? FORGE_AI_PAGE_UNIT :
			row == _forge_ai_row_spawn_page ? FORGE_AI_PAGE_SPAWN : FORGE_AI_PAGE_GOAL;
		s_page_return_row = row;
		return FORGE_AI_ROUTE_PRESS_PAGE_OPENED;
	}
	if (row == _forge_ai_row_goal_back)
	{
		s_page_live_idx = -1;
		s_page = FORGE_AI_PAGE_MAIN;
		return FORGE_AI_ROUTE_PRESS_PAGE_CLOSED;
	}
	inst = &g_forge.live_instances[live_idx];
	switch (row)
	{
	case _forge_ai_row_variant:
		forge_ai_variant_step(entry, dir);
		snprintf(inst->tag_path, sizeof(inst->tag_path), "%s", entry->actor_variant_path);
		break;
	case _forge_ai_row_weapon:
		forge_ai_tag_step('weap', entry->weapon_path, sizeof(entry->weapon_path), dir, 1);
		break;
	case _forge_ai_row_team:
	{
		int count = (int)NUMBEROF(s_ai_team_order);
		int at = 0;

		while (at < count && s_ai_team_order[at] != entry->team)
			at++;
		if (at == count)
			at = 0;
		entry->team = s_ai_team_order[(at + count + dir) % count];
		inst->team_designator = entry->team;
		entry->detached = 1;
		break;
	}
	case _forge_ai_row_start_count:
		entry->unit_count = (int16_t)((entry->unit_count + 17 + dir) % 17);
		entry->detached = 1;
		break;
	case _forge_ai_row_respawn_alive:
		entry->respawn_alive = (int16_t)((entry->respawn_alive + 17 + dir) % 17);
		entry->detached = 1;
		break;
	case _forge_ai_row_respawn_delay:
		index = forge_ai_table_index(s_ai_delays, NUMBEROF(s_ai_delays), entry->respawn_delay_seconds);
		entry->respawn_delay_seconds = s_ai_delays[(index + (int)NUMBEROF(s_ai_delays) + dir) % (int)NUMBEROF(s_ai_delays)];
		entry->detached = 1;
		break;
	case _forge_ai_row_respawn_total:
		index = forge_ai_table_index(s_ai_totals, NUMBEROF(s_ai_totals), entry->respawn_total);
		entry->respawn_total = s_ai_totals[(index + (int)NUMBEROF(s_ai_totals) + dir) % (int)NUMBEROF(s_ai_totals)];
		entry->detached = 1;
		break;
	case _forge_ai_row_game_mode:
		for (index = 0; index < (int)NUMBEROF(s_ai_gametypes) && s_ai_gametypes[index] != forge_ai_entry_modes(entry); index++)
		{
		}
		if (index == (int)NUMBEROF(s_ai_gametypes))
			index = 0;
		entry->gametype_flags = (uint16_t)((entry->gametype_flags & FORGE_AI_RANDOM_RANK) |
			s_ai_gametypes[(index + (int)NUMBEROF(s_ai_gametypes) + dir) % (int)NUMBEROF(s_ai_gametypes)]);
		break;
	case _forge_ai_row_spawn_effect:
		entry->spawn_effect = (uint8_t)((entry->spawn_effect + (int)NUMBEROF(s_ai_spawn_effects) + dir) %
			(int)NUMBEROF(s_ai_spawn_effects));
		/* (read now, in a solo Forge game, not as the unit appears) */
		if (s_ai_spawn_effects[entry->spawn_effect].path)
			forge_ai_tag_need('effe', s_ai_spawn_effects[entry->spawn_effect].path);
		break;
	case _forge_ai_row_cloaked:
		entry->cloaked = (uint8_t)!entry->cloaked;
		break;
	case _forge_ai_row_hover_height:
		index = forge_ai_table_index(s_ai_hover_heights, NUMBEROF(s_ai_hover_heights), entry->hover_height);
		entry->hover_height = (uint8_t)s_ai_hover_heights[(index + (int)NUMBEROF(s_ai_hover_heights) + dir) %
			(int)NUMBEROF(s_ai_hover_heights)];
		break;
	case _forge_ai_row_goal:
		if (!forge_ai_goal_step(entry, dir))
			return 0;
		break;
	case _forge_ai_row_goal_target:
	{
		int numbers[FORGE_AI_ROUTE_MAX];
		int count = entry->goal == _forge_ai_goal_route ? forge_ai_route_numbers(numbers, FORGE_AI_ROUTE_MAX) :
			forge_volumes_patrol_numbers(numbers, FORGE_AI_ROUTE_MAX);
		int at = 0;

		if (count == 0)
			return 0;
		while (at < count && numbers[at] != entry->goal_link)
			at++;
		entry->goal_link = (uint8_t)numbers[at == count ? 0 : (at + count + dir) % count];
		break;
	}
	case _forge_ai_row_goal_who:
		entry->goal_who = (uint8_t)((entry->goal_who & ~FORGE_AI_GOAL_WHO_MASK) |
			((FORGE_AI_GOAL_WHO(entry->goal_who) + NUMBER_OF_FORGE_AI_GOAL_WHO + dir) % NUMBER_OF_FORGE_AI_GOAL_WHO));
		break;
	case _forge_ai_row_goal_speed:
		entry->goal_who ^= FORGE_AI_GOAL_RUN_BIT;
		forge_set_status("%s", FORGE_AI_GOAL_RUNS(entry->goal_who) ?
			"Run: they go at full speed out of a fight too" : "Walk: they go at an easy, alert pace out of a fight");
		break;
	case _forge_ai_row_goal_start:
		index = forge_ai_table_index(s_ai_goal_distances, NUMBEROF(s_ai_goal_distances), entry->goal_start);
		entry->goal_start = (uint8_t)s_ai_goal_distances[(index + (int)NUMBEROF(s_ai_goal_distances) + dir) %
			(int)NUMBEROF(s_ai_goal_distances)];
		forge_set_status("They go to the nearest player this near them, seen or not (0: anywhere on the map)");
		break;
	case _forge_ai_row_goal_follow:
		index = forge_ai_table_index(s_ai_goal_distances, NUMBEROF(s_ai_goal_distances), entry->goal_follow);
		entry->goal_follow = (uint8_t)s_ai_goal_distances[(index + (int)NUMBEROF(s_ai_goal_distances) + dir) %
			(int)NUMBEROF(s_ai_goal_distances)];
		forge_set_status("They stop this far from their player");
		break;
	case _forge_ai_row_goal_alert:
		index = forge_ai_table_index(s_ai_goal_alerts, NUMBEROF(s_ai_goal_alerts), forge_ai_entry_alert_seconds(entry));
		entry->goal_alert = (uint8_t)s_ai_goal_alerts[(index + (int)NUMBEROF(s_ai_goal_alerts) + dir) %
			(int)NUMBEROF(s_ai_goal_alerts)];
		forge_set_status("Once one of them fights, is hurt or dies, all stay alert %d seconds after", (int)entry->goal_alert);
		break;
	default:
		return 0;
	}
	g_forge.edit_state.ai_spawns_edited = 1;
	forge_ai_preview_update(live_idx);
	forge_network_send_ai_spawn(live_idx);
	return 0;
}

int forge_ai_spawn_rows(int32_t live_idx, int *rows)
{
	struct forge_ai_spawn_entry const *entry = forge_ai_entry_live(live_idx);
	int count = 0;

	if (!entry)
		return 0;
	if (s_page_live_idx >= 0 && s_page_live_idx != live_idx)
	{
		s_page_live_idx = -1;
		s_page = FORGE_AI_PAGE_MAIN;
	}
	if (s_page_live_idx != live_idx || s_page == FORGE_AI_PAGE_MAIN)
	{
		rows[count++] = _forge_ai_row_unit_page;
		rows[count++] = _forge_ai_row_spawn_page;
		rows[count++] = _forge_ai_row_goal_page;
		rows[count++] = _forge_ai_row_rot_snap;
		rows[count++] = _forge_ai_row_floor;
		rows[count++] = _forge_ai_row_delete;
		return count;
	}
	switch (s_page)
	{
	case FORGE_AI_PAGE_UNIT:
		rows[count++] = _forge_ai_row_variant;
		rows[count++] = _forge_ai_row_team;
		rows[count++] = _forge_ai_row_weapon;
		rows[count++] = _forge_ai_row_cloaked;
		rows[count++] = _forge_ai_row_game_mode;
		/* (a flying unit's alone: sentinels, drones) */
		if (forge_ai_entry_flies(entry))
			rows[count++] = _forge_ai_row_hover_height;
		break;
	case FORGE_AI_PAGE_SPAWN:
		rows[count++] = _forge_ai_row_start_count;
		rows[count++] = _forge_ai_row_respawn_alive;
		rows[count++] = _forge_ai_row_respawn_delay;
		rows[count++] = _forge_ai_row_respawn_total;
		rows[count++] = _forge_ai_row_spawn_effect;
		break;
	default:
		rows[count++] = _forge_ai_row_goal;
		if (entry->goal == _forge_ai_goal_patrol || entry->goal == _forge_ai_goal_route)
			rows[count++] = _forge_ai_row_goal_target;
		if (entry->goal == _forge_ai_goal_hunt)
		{
			rows[count++] = _forge_ai_row_goal_who;
			rows[count++] = _forge_ai_row_goal_start;
			/* (a team of players' stops short of them; others go all the way) */
			if (forge_ai_entry_friendly(entry))
				rows[count++] = _forge_ai_row_goal_follow;
		}
		if (entry->goal != _forge_ai_goal_none)
		{
			rows[count++] = _forge_ai_row_goal_speed;
			rows[count++] = _forge_ai_row_goal_alert;
		}
		break;
	}
	rows[count++] = _forge_ai_row_goal_back;
	return count;
}

int forge_ai_spawn_page_return_row(void)
{
	return s_page_return_row;
}

int forge_ai_spawn_page_back(int32_t live_idx)
{
	if (s_page_live_idx != live_idx || s_page == FORGE_AI_PAGE_MAIN)
		return 0;
	s_page_live_idx = -1;
	s_page = FORGE_AI_PAGE_MAIN;
	return 1;
}

char const *forge_ai_spawn_panel_title(int32_t live_idx)
{
	if (s_page_live_idx != live_idx)
		return "AI SPAWN";
	return s_page == FORGE_AI_PAGE_UNIT ? "AI UNIT SETTINGS" : s_page == FORGE_AI_PAGE_SPAWN ? "AI SPAWN SETTINGS" :
		s_page == FORGE_AI_PAGE_GOAL ? "AI GOAL SETTINGS" : "AI SPAWN";
}

void forge_ai_spawn_goal_apply(int32_t live_idx, int goal, int link, int who, int start, int follow, int alert)
{
	struct forge_ai_spawn_entry *entry = forge_ai_entry_live(live_idx);

	if (goal == FORGE_AI_GOAL_LEGACY_FOLLOW)
		goal = _forge_ai_goal_hunt;
	if (!entry || goal < 0 || goal >= NUMBER_OF_FORGE_AI_GOALS)
		return;
	if (alert >= 0)
		entry->goal_alert = (uint8_t)PIN(alert, 0, 255);
	entry->goal = (uint8_t)goal;
	entry->goal_link = (uint8_t)link;
	entry->goal_who = (uint8_t)((who & FORGE_AI_GOAL_RUN_BIT) | (who >= 0 ? FORGE_AI_GOAL_WHO(who) : 0));
	entry->goal_start = (uint8_t)start;
	entry->goal_follow = (uint8_t)follow;
	g_forge.edit_state.ai_spawns_edited = 1;
}

void forge_ai_spawn_label(struct forge_live_instance const *inst, char *text, size_t text_size)
{
	struct forge_ai_spawn_entry const *entry = forge_ai_entry_of(inst);
	char name[64];

	if (entry)
		forge_ai_entry_name(entry, name, sizeof(name));
	else
		snprintf(name, sizeof(name), "%s", forge_ai_path_leaf(inst->tag_path));
	snprintf(text, text_size, "AI: %s", name);
	if (entry && entry->unit_count == 0 && entry->respawn_alive == 0)
		snprintf(text, text_size, "AI: %s (Inactive)", name);
}

/* ---------- what a saved map needs */

int forge_ai_palette_entry_allowed(struct forge_catalog_entry const *entry)
{
	return forge_ai_map_editable() &&
		(entry->group_tag == FORGE_GROUP_AI_SPAWN || entry->group_tag == FORGE_GROUP_AI_MENU ||
		entry->group_tag == FORGE_GROUP_AI_ROUTE);
}

static void forge_ai_saved_tag_add(uint32_t group_tag, char const *path, char const *known_map)
{
	struct forge_scenario_edit_state *edits = &g_forge.edit_state;
	char map[32];
	int i;

	if (known_map)
		snprintf(map, sizeof(map), "%s", known_map);
	else if (!path[0] || !forge_campaign_tag_map(group_tag, path, map, sizeof(map)))
		return;
	for (i = 0; i < edits->ai_actor_count; i++)
	{
		if (edits->ai_actors[i].group_tag == group_tag && !_stricmp(edits->ai_actors[i].path, path))
			return;
	}
	if (edits->ai_actor_count >= FORGE_MAX_AI_ACTORS)
	{
		platform_log("Forge: more than %d actors and weapons are used: %s is left out", FORGE_MAX_AI_ACTORS, path);
		return;
	}
	edits->ai_actors[edits->ai_actor_count].group_tag = group_tag;
	snprintf(edits->ai_actors[edits->ai_actor_count].path, sizeof(edits->ai_actors[0].path), "%s", path);
	snprintf(edits->ai_actors[edits->ai_actor_count].map, sizeof(edits->ai_actors[0].map), "%s", map);
	edits->ai_actor_count++;
}

/* just before a save: the campaign's tags the spawns use, and no others. Those
the map has itself are named too; the level only copies what it has not got */
void forge_ai_saved_tags_collect(void)
{
	struct forge_scenario_edit_state *edits = &g_forge.edit_state;
	int i;

	edits->ai_actor_count = 0;
	for (i = 0; i < edits->ai_spawn_count; i++)
	{
		if (!edits->ai_spawns[i].active)
			continue;
		forge_ai_saved_tag_add('actv', edits->ai_spawns[i].actor_variant_path, NULL);
		forge_ai_saved_tag_add('weap', edits->ai_spawns[i].weapon_path, NULL);
		/* (a random spawn needs every rank it may place) */
		if (edits->ai_spawns[i].gametype_flags & FORGE_AI_RANDOM_RANK)
		{
			int group = forge_ai_kind_group(forge_ai_kind_of(edits->ai_spawns[i].actor_variant_path));
			int row;

			for (row = 0; row < g_forge.catalog_count; row++)
			{
				if (g_forge.catalog[row].group_tag == FORGE_GROUP_AI_SPAWN &&
					!forge_ai_row_is_random(&g_forge.catalog[row]) && forge_ai_row_group(&g_forge.catalog[row]) == group)
				{
					forge_ai_saved_tag_add('actv', g_forge.catalog[row].tag_path, NULL);
				}
			}
		}
		if (edits->ai_spawns[i].spawn_effect < NUMBEROF(s_ai_spawn_effects) &&
			s_ai_spawn_effects[edits->ai_spawns[i].spawn_effect].map)
		{
			forge_ai_saved_tag_add('effe', s_ai_spawn_effects[edits->ai_spawns[i].spawn_effect].path,
				s_ai_spawn_effects[edits->ai_spawns[i].spawn_effect].map);
		}
	}
	platform_log("Forge: the map's spawns use %d campaign tags", (int)edits->ai_actor_count);
}

/* ---------- network */

uint32_t forge_ai_spawn_tag_hash(uint32_t group_tag, char const *path)
{
	return path && path[0] ? forge_catalog_entry_hash(group_tag, path) : 0;
}

/* the settings a network packet gives, what it names looked up by its hash */
static int forge_ai_tag_from_hash(uint32_t group_tag, uint32_t hash, char *path, size_t path_size)
{
	long count, i;
	struct forge_ai_tag_instance *instances = forge_ai_tag_instances(&count);

	for (i = 0; instances && i < count; i++)
	{
		if ((uint32_t)instances[i].group_tag == group_tag && instances[i].name &&
			forge_catalog_entry_hash(group_tag, XBOX_POINTER(char const, instances[i].name)) == hash) /* AE hook */
		{
			snprintf(path, path_size, "%s", instances[i].name);
			return 1;
		}
	}
	return 0;
}

void forge_ai_spawn_net_apply(
	int32_t live_idx,
	uint32_t variant_hash,
	uint32_t weapon_hash,
	int team,
	int unit_count,
	int respawn_alive,
	int respawn_delay_seconds,
	int respawn_total,
	unsigned int gametype_flags,
	int detached,
	int spawn_effect)
{
	struct forge_ai_spawn_entry *entry = forge_ai_entry_live(live_idx);
	struct forge_live_instance *inst;

	if (!entry)
		return;
	inst = &g_forge.live_instances[live_idx];
	if (forge_ai_tag_from_hash('actv', variant_hash, entry->actor_variant_path, sizeof(entry->actor_variant_path)))
		snprintf(inst->tag_path, sizeof(inst->tag_path), "%s", entry->actor_variant_path);
	if (!weapon_hash)
		entry->weapon_path[0] = 0;
	else
		forge_ai_tag_from_hash('weap', weapon_hash, entry->weapon_path, sizeof(entry->weapon_path));
	entry->team = (int16_t)team;
	inst->team_designator = entry->team;
	entry->unit_count = (int16_t)unit_count;
	entry->respawn_alive = (int16_t)respawn_alive;
	entry->respawn_delay_seconds = (int16_t)respawn_delay_seconds;
	entry->respawn_total = (int16_t)respawn_total;
	entry->gametype_flags = (uint16_t)gametype_flags;
	/* (CLOAKED and HEIGHT ride with the effect: forge_ai_spawn_effect_packed) */
	if ((spawn_effect & FORGE_AI_PACKED_EFFECT_MASK) < (int)NUMBEROF(s_ai_spawn_effects))
		entry->spawn_effect = (uint8_t)(spawn_effect & FORGE_AI_PACKED_EFFECT_MASK);
	entry->cloaked = (spawn_effect & FORGE_AI_PACKED_CLOAKED) ? 1 : 0;
	entry->hover_height = (uint8_t)forge_ai_hover_height_valid(spawn_effect >> FORGE_AI_PACKED_HEIGHT_SHIFT);
	entry->detached = (uint8_t)detached;
	g_forge.edit_state.ai_spawns_edited = 1;
	forge_ai_preview_update(live_idx);
}

int forge_ai_spawn_effect_packed(struct forge_ai_spawn_entry const *entry)
{
	return (entry->spawn_effect & FORGE_AI_PACKED_EFFECT_MASK) | (entry->cloaked ? FORGE_AI_PACKED_CLOAKED : 0) |
		(forge_ai_hover_height_valid(entry->hover_height) << FORGE_AI_PACKED_HEIGHT_SHIFT);
}

int forge_ai_spawn_grid_count(int32_t live_idx)
{
	struct forge_ai_spawn_entry const *entry = forge_ai_entry_live(live_idx);

	return entry && entry->unit_count > 1 && forge_ai_entry_own_squad(entry) ? entry->unit_count : 0;
}

int forge_ai_spawn_grid_point(int32_t live_idx, int index, union real_point3d *point)
{
	struct forge_ai_spawn_entry const *entry = forge_ai_entry_live(live_idx);
	struct forge_ai_grid_layout const *layout;

	if (!entry || index < 0 || index >= FORGE_AI_GRID_PLACES || index >= entry->unit_count)
		return 0;
	layout = forge_ai_grid_layout(entry, (int)(entry - g_forge.edit_state.ai_spawns));
	if (!layout)
		return 0;
	*point = layout->points[index];
	return 1;
}

int forge_ai_spawn_hover_point(int32_t live_idx, int index, union real_point3d *point)
{
	struct forge_ai_spawn_entry const *entry = forge_ai_entry_live(live_idx);
	union real_point3d base;

	if (!entry || !entry->hover_height || !forge_ai_entry_flies(entry))
		return 0;
	if (index == 0 || !forge_ai_spawn_grid_point(live_idx, index, &base))
	{
		if (index != 0)
			return 0;
		base.x = entry->x;
		base.y = entry->y;
		base.z = entry->z;
	}
	return forge_ai_over_ground(base.x, base.y, base.z, (float)entry->hover_height, point);
}

void forge_ai_spawn_net_settings(
	int32_t live_idx,
	uint32_t *variant_hash,
	uint32_t *weapon_hash,
	struct forge_ai_spawn_entry const **settings)
{
	struct forge_ai_spawn_entry const *entry = forge_ai_entry_live(live_idx);

	*settings = entry;
	if (entry)
	{
		*variant_hash = forge_ai_spawn_tag_hash('actv', entry->actor_variant_path);
		*weapon_hash = forge_ai_spawn_tag_hash('weap', entry->weapon_path);
	}
}

/* ---------- the AI as Forge's mode changes

Forge mode shows each spawn's AI as a model on its marker: a real unit with no
actor, so the engine plays its idle, armed as it will be, and it moves and
goes with the marker as any placed object does. Spartan mode plays the AI, from
the spawns as they stand. The host's mode decides, as it runs the AI. */

/* (actors.c's own: an actor variant's change_colors block) */
struct forge_ai_variant_change_colors
{
	real_rgb_color color_lower_bound;
	real_rgb_color color_upper_bound;
	long pad[2];
};

typedef char forge_ai_variant_change_colors_size_assert[sizeof(struct forge_ai_variant_change_colors) == 0x20 ? 1 : -1];

/* how a tag's object looks with no object made of it, as the Tag Injector's
preview has it (tag_injector.c's preview_change_colors): an actor's change
colors, the middle of each's bounds, and for those it has not, its unit's, the
middle of each's first permutation's; and the shader permutation that is its
rank's armor (actor_customize_unit). actor_path is NULL for any other object */
void forge_model_look_get(long object_definition, char const *actor_path, float colors[4][3], int colored[4],
	short *forced_shader)
{
	struct object_definition const *object = object_definition != NONE ?
		(struct object_definition const *)tag_get(tag_get_group_tag(object_definition), object_definition) : NULL;
	long variant_index = actor_path ? forge_ai_tag_find('actv', actor_path) : NONE;
	struct actor_variant_definition const *variant = variant_index != NONE ?
		(struct actor_variant_definition const *)tag_get('actv', variant_index) : NULL;
	short index, channel;

	memset(colors, 0, sizeof(float) * 4 * 3);
	memset(colored, 0, sizeof(int) * 4);
	*forced_shader = object ? object->object.forced_shader_permutation_index : 0;
	for (index = 0; object && index < MIN(object->object.change_colors.count, 4); index++)
	{
		struct object_change_color_definition const *change_color = TAG_BLOCK_GET_ELEMENT(
			&object->object.change_colors, index, struct object_change_color_definition);
		struct object_change_color_permutation const *permutation;

		if (change_color->permutations.count <= 0)
			continue;
		permutation = TAG_BLOCK_GET_ELEMENT(&change_color->permutations, 0, struct object_change_color_permutation);
		colors[index][0] = (permutation->color_lower_bound.red + permutation->color_upper_bound.red) * 0.5f;
		colors[index][1] = (permutation->color_lower_bound.green + permutation->color_upper_bound.green) * 0.5f;
		colors[index][2] = (permutation->color_lower_bound.blue + permutation->color_upper_bound.blue) * 0.5f;
		colored[index] = 1;
	}
	if (variant)
	{
		if (variant->unit.forced_shader_permutation_index)
			*forced_shader = variant->unit.forced_shader_permutation_index;
		for (index = 0; index < MIN(variant->change_colors.count, 4); index++)
		{
			struct forge_ai_variant_change_colors const *change_colors = TAG_BLOCK_GET_ELEMENT(
				&variant->change_colors, index, struct forge_ai_variant_change_colors);

			colors[index][0] = (change_colors->color_lower_bound.red + change_colors->color_upper_bound.red) * 0.5f;
			colors[index][1] = (change_colors->color_lower_bound.green + change_colors->color_upper_bound.green) * 0.5f;
			colors[index][2] = (change_colors->color_lower_bound.blue + change_colors->color_upper_bound.blue) * 0.5f;
			colored[index] = 1;
		}
	}
	for (index = 0; index < 4; index++)
	{
		/* (a NaN fails every comparison) */
		for (channel = 0; channel < 3; channel++)
		{
			if (!(colors[index][channel] >= 0.0f))
				colors[index][channel] = 0.0f;
			colors[index][channel] = MIN(colors[index][channel], 1.0f);
		}
	}
}

long forge_ai_actor_unit_definition(char const *path)
{
	long variant = forge_ai_tag_need('actv', path);

	if (variant == NONE)
		return NONE;
	return ((struct actor_variant_definition *)tag_get('actv', variant))->unit_reference.index;
}

/* a model answers to Forge's user alone: no unit or vehicle meets it, and no
blast pushes it (taking no damage does not stop that: object_cause_damage) */
void forge_ai_preview_protect(long unit_index)
{
	struct unit_datum *unit = (struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);

	if (unit)
	{
		SET_FLAG(unit->object.flags, _object_no_collisions_bit, TRUE);
		SET_FLAG(unit->unit.flags, _unit_impervious_bit, TRUE);
	}
}

static long forge_ai_preview_new(struct forge_ai_spawn_entry const *entry)
{
	long unit_definition = forge_ai_actor_unit_definition(entry->actor_variant_path);
	long variant = forge_ai_tag_find('actv', entry->actor_variant_path);
	long weapon = forge_ai_tag_need('weap', entry->weapon_path);
	struct object_placement_data placement;
	struct object_datum *object;
	long unit_index;

	if (unit_definition == NONE)
		return NONE;
	object_placement_data_new(&placement, unit_definition, NONE);
	placement.position.x = entry->x;
	placement.position.y = entry->y;
	placement.position.z = entry->z + 0.1f;
	placement.forward.i = (real)cos((double)entry->yaw);
	placement.forward.j = (real)sin((double)entry->yaw);
	placement.forward.k = 0.0f;
	placement.up.i = 0.0f;
	placement.up.j = 0.0f;
	placement.up.k = 1.0f;
	SET_FLAG(placement.flags, _new_object_never_automatically_delete_bit, TRUE);
	unit_index = object_new(&placement);
	if (unit_index == NONE)
		return NONE;
	actor_customize_unit(variant, unit_index);
	if (weapon != NONE)
		forge_ai_weapon_give(unit_index, weapon);
	object = (struct object_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);
	if (object)
	{
		/* (object_cannot_take_damage is the scripts', of an object list) */
		SET_FLAG(object->object.damage_flags, _object_cannot_take_damage_bit, TRUE);
		SET_FLAG(object->object.flags, _object_cannot_be_garbage_bit, TRUE);
		SET_FLAG(object->object.flags, _object_deleted_when_deactivated_bit, FALSE);
		object_set_automatic_deactivation(unit_index, FALSE);
		forge_ai_preview_protect(unit_index);
	}
	return unit_index;
}

/* a marker's model as its settings have it, while the AI is held */
void forge_ai_preview_update(int32_t live_idx)
{
	struct forge_ai_spawn_entry const *entry = forge_ai_entry_live(live_idx);
	struct forge_live_instance *inst;

	if (!entry || !s_ai_held || game_connection() == _game_connection_network_client)
		return;
	inst = &g_forge.live_instances[live_idx];
	if (inst->object_index != NONE)
		object_delete(inst->object_index);
	inst->object_index = forge_ai_preview_new(entry);
	if (live_idx == g_forge.grabbed_live_index)
		g_forge.grabbed_object_index = inst->object_index;
}

static void forge_ai_previews_set(int shown)
{
	int i;

	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		struct forge_live_instance *inst = &g_forge.live_instances[i];

		if (!inst->active || inst->group_tag != FORGE_GROUP_AI_SPAWN)
			continue;
		if (shown)
		{
			forge_ai_preview_update(i);
		}
		else if (inst->object_index != NONE)
		{
			object_delete(inst->object_index);
			inst->object_index = NONE;
		}
	}
}

static int forge_ai_object_is_placed(long object_index)
{
	int i;

	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		if (g_forge.live_instances[i].object_index == object_index)
			return 1;
	}
	return 0;
}

/* what the AI left: dead units no player or placed object is */
static void forge_ai_corpses_delete(void)
{
	struct object_iterator iterator;
	struct unit_datum *unit;

	object_iterator_new(&iterator, _object_mask_biped, 0);
	while ((unit = (struct unit_datum *)object_iterator_next(&iterator)) != NULL)
	{
		if (unit->unit.player_index != NONE || !TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
			continue;
		if (!forge_ai_object_is_placed(iterator.index))
			object_delete(iterator.index);
	}
}

/* and the weapons dropped: loose ones a unit carried, the map's own waiting
to be taken never were. Not a flag or ball, which the game needs, nor a placed
object */
static void forge_ai_dropped_weapons_delete(void)
{
	struct object_iterator iterator;
	struct item_datum *item;

	object_iterator_new(&iterator, _object_mask_weapon, 0);
	while ((item = (struct item_datum *)object_iterator_next(&iterator)) != NULL)
	{
		if (!TEST_FLAG(item->item.flags, _item_has_been_held_bit) ||
			TEST_FLAG(item->item.flags, _item_attached_to_unit_bit) ||
			item->object.parent_object_index != NONE || weapon_is_flag(iterator.index))
		{
			continue;
		}
		if (!forge_ai_object_is_placed(iterator.index))
			object_delete(iterator.index);
	}
}

/* (a map with no AI and no spawns has nothing to stop or start, but the AI
still counts as held: the first spawn placed there shows its model at once) */
void forge_ai_forge_mode_changed(int forge_mode)
{
	if (game_connection() == _game_connection_network_client || !forge_ai_map_editable())
		return;
	if (!forge_ai_map_supported() && g_forge.edit_state.ai_spawn_count == 0)
	{
		s_ai_held = forge_mode;
		return;
	}
	if (forge_mode)
	{
		ai_erase(NONE, NONE, NONE, FALSE);
		s_ai_held = 1;
		ai_dispose_from_old_map();
		ai_initialize_for_new_map();
		forge_ai_corpses_delete();
		forge_ai_dropped_weapons_delete();
		forge_ai_previews_set(1);
	}
	else
	{
		forge_ai_previews_set(0);
		s_ai_held = 0;
		ai_erase(NONE, NONE, NONE, FALSE);
		ai_dispose_from_old_map();
		ai_initialize_for_new_map();
		ai_place();
	}
}
