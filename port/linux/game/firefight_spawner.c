/*
FIREFIGHT_SPAWNER.C

port: Self-contained Firefight wave spawner, faction/unit roster manager,
2-3 zone randomized squad placement, animated Spirit/Pelican dropships,
Boss Wave vehicle drops, Safe Player Ordnance/Heavy Weapon drops,
Friendly AI Reinforcements, and Round-Scheduled Player Vehicles.
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries/cseries.h"
#include "cseries/errors.h"
#include "math/real_math.h"
#include "ai/ai.h"
#include "ai/ai_globals.h"
#include "ai/ai_scenario_definitions.h"
#include "ai/actors.h"
#include "ai/actor_definitions.h"
#include "ai/actor_types.h"
#include "ai/encounters.h"
#include "ai/props.h"
#include "cache/cache_files.h"
#include "effects/effect_definitions.h"
#include "effects/effects.h"
#include "game/game.h"
#include "game/game_allegiance.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "interface/hud_messaging.h"
#include "items/equipment_definitions.h"
#include "items/item_definitions.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"
#include "memory/data.h"
#include "networking/network_game_globals.h"
#include "objects/damage.h"
#include "game/game_globals.h"
#include "objects/damage_effect_definitions.h"
#include "items/projectile_definitions.h"
#include "objects/objects.h"
#include "models/model_definitions.h"
#include "physics/collision_bsp_definitions.h"
#include "physics/collisions.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "structures/structure_bsp_definitions.h"
#include "structures/structures.h"
#include "death_volumes.h"
#include "tag_files/tag_files.h"
#include "tag_files/tag_groups.h"
#include "units/biped_definitions.h"
#include "units/bipeds.h"
#include "units/unit_definitions.h"
#include "units/units.h"
#include "units/vehicle_definitions.h"
#include "units/vehicles.h"

#include "bots.h"
#include "cev/cev_names.h"
#include "cev/custom_edition_cache.h"
#include "mcc_cache.h"
#include <stdint.h>
#include "halo_firefight.h"
#include "halo_firefight_route.h"
#include "halo_skulls.h"

static boolean mcc_cache_tag_names_walk(char const *map_name,
	int (*visit)(void *context, unsigned long group_tag, char const *name), void *context);

/* ---------- constants */

enum
{
	MAX_ROSTER_VARIANTS = 64,
	MAX_SPAWN_POINTS = 128,
	MAX_FRIENDLY_UNITS = 8,
	MAX_ORDNANCE_TAG_POOL = 24,
	FIREFIGHT_SAFE_ACTIVE_ACTOR_CAP = 28,
	DROPSHIP_PHASE_NONE = 0,
	DROPSHIP_PHASE_DESCENDING = 1,
	DROPSHIP_PHASE_HOVER_DEPLOY = 2,
	DROPSHIP_PHASE_ASCENDING = 3,

	/* Custom Forge Firefight Spawn Marker ('ffsp' -> scenario_netgame_flag.type == 15) */
	FF_NETGAME_FLAG_FIREFIGHT_SPAWN = 15,
	/* (Forge's generator objective, firefight_generators.c's: no spawn point) */
	FF_NETGAME_FLAG_FIREFIGHT_GENERATOR = 16,
	FF_SPAWN_TYPE_AI_INFANTRY       = 0,
	FF_SPAWN_TYPE_AI_AND_VEHICLES   = 1,
	FF_SPAWN_TYPE_ENEMY_VEHICLE     = 2,
	FF_SPAWN_TYPE_ENEMY_GHOST       = 3,
	FF_SPAWN_TYPE_ENEMY_HEAVY_VEH   = 4,
	/* (an enemy drop point taking any drop; those of one kind alone are
	halo_firefight_route.h's FF_DROP_SPAWN_TYPE_*) */
	FF_SPAWN_TYPE_DROPSHIP_ZONE     = 5,
	FF_SPAWN_TYPE_PLAYER_VEHICLE    = 6,

	FF_ROUND_ALL       = 0,
	FF_ROUND_1         = 1,
	FF_ROUND_2         = 2,
	FF_ROUND_3         = 3,
	FF_ROUND_4         = 4,
	FF_ROUND_5         = 5,
	FF_ROUND_1_TO_3    = 6,
	FF_ROUND_4_TO_5    = 7,
	FF_ROUND_BONUS     = 8,

	FF_WAVE_ALL        = 0,
	FF_WAVE_1          = 1,
	FF_WAVE_2          = 2,
	FF_WAVE_3_BOSS     = 3
};

#define FF_UNPACK_SPAWN_TYPE(u)   ((int)((u) & 0x0F))
#define FF_UNPACK_ROUND_FILTER(u) ((int)(((u) >> 4) & 0x0F))
#define FF_UNPACK_WAVE_FILTER(u)  ((int)(((u) >> 8) & 0x07))

/* ---------- structures */

struct firefight_roster_entry
{
	long variant_tag_index;
	short faction;
	short unit_toggle_bit;
	byte tier;
	byte subtype;
	boolean is_human_ally_or_enemy;
	/* the map's own (MAP CONTENT): neither copied in nor a profile's */
	boolean maps_own;
	/* (one of them MAP ENEMY AI's CUSTOM turns off) */
	boolean custom_off;
};

/* ---------- globals */

extern struct game_variant global_variant;
boolean actor_set_active(long actor_index, boolean active);

static struct firefight_roster_entry g_roster[MAX_ROSTER_VARIANTS];
static short g_roster_count = 0;

static real_point3d g_spawn_points[MAX_SPAWN_POINTS];
/* (each tracked enemy's follow, by its slot: firefight_relocate_idle_enemies) */
static struct
{
	long unit_index;
	real_point3d position;
	real vitality;
	long last_fired;
	long active_tick;
	short moves;
} g_idle[FIREFIGHT_MAX_TRACKED_ENEMIES];
static long g_idle_last_check;
static real g_spawn_facings[MAX_SPAWN_POINTS];
static short g_spawn_point_count = 0;
/* the players' base: the map's starting location everyone spawns about
(firefight_select_player_spawn_point), chosen at the first spawn of a map */
#define FF_PLAYER_BASE_SPAWNS 8
static short g_player_base_location = NONE;

static long g_friendly_units[MAX_FRIENDLY_UNITS];
static short g_friendly_unit_count = 0;

static long ff_power_weapon_tags[MAX_ORDNANCE_TAG_POOL];
static short ff_power_weapon_tag_count = 0;
static long ff_standard_weapon_tags[MAX_ORDNANCE_TAG_POOL];
static short ff_standard_weapon_tag_count = 0;
static long ff_ammo_tags[MAX_ORDNANCE_TAG_POOL];
static short ff_ammo_tag_count = 0;
static long ff_grenade_tags[MAX_ORDNANCE_TAG_POOL];
static short ff_grenade_tag_count = 0;

/* a variant's Tag Injector profile: its AI, which join the waves whatever
the faction, and its weapons, which join the drops */
static long ff_custom_enemy_tags[MAX_ROSTER_VARIANTS];
static byte ff_custom_enemy_tiers[MAX_ROSTER_VARIANTS];
static short ff_custom_enemy_count = 0;
static long ff_custom_weapon_tags[MAX_ROSTER_VARIANTS];
static boolean ff_custom_weapon_power[MAX_ROSTER_VARIANTS];
static short ff_custom_weapon_count = 0;

static short g_queued_reinforcement_enemies = 0;
static byte g_queued_reinforcement_subtypes[256];
static byte g_dropship_squad_subtypes[32];
static short g_dropship_squad_subtype_count = 0;
/* (the wave being spawned's) the factions (1 << FF_DROP_KIND_*) its enemies
are of, a drop point spawning one of which select_wave_spawn_zones puts in
slot 0; and the factions of the drop point it put there (0: slot 0 is no
drop point) */
static unsigned g_drop_kinds_wanted = 0;
static unsigned g_zone0_drop_kinds = 0;
static real g_active_wave_facings[FIREFIGHT_MAX_SPAWN_ZONES_PER_WAVE];

/* MAP CONTENT's: the map's own enemies (ones the roster has no place for:
those it has are its entries of maps_own), weapons and vehicles; and, for
ONLY MAP, whether the roster's picks keep to its entries of the map's */
enum
{
	MAX_MAP_VEHICLES = 16
};
static long ff_map_enemy_tags[MAX_ROSTER_VARIANTS];
static byte ff_map_enemy_tiers[MAX_ROSTER_VARIANTS];
static short ff_map_enemy_count = 0;
static long ff_map_weapon_tags[MAX_ROSTER_VARIANTS];
static boolean ff_map_weapon_power[MAX_ROSTER_VARIANTS];
static boolean ff_map_weapon_stock[MAX_ROSTER_VARIANTS];
static short ff_map_weapon_count = 0;
static long ff_map_vehicle_tags[MAX_MAP_VEHICLES];
static boolean ff_map_vehicle_stock[MAX_MAP_VEHICLES];
static short ff_map_vehicle_count = 0;
static boolean g_roster_map_only = FALSE;

/* the ordnance pod's (firefight_ordnance_pod_update) */
enum
{
	FF_POD_FALL_TICKS = 2 * TICKS_PER_SECOND,
	FF_POD_STAY_TICKS = 40 * TICKS_PER_SECOND,
	/* (a pod's weapon: a drop of more takes more pods, one each) */
	FF_POD_MAX_CONTENTS = FIREFIGHT_MAX_ORDNANCE_PODS,
	/* the drop areas Forge marks (FF_DROP_AREA_TAG) */
	FF_MAX_DROP_AREAS = 16
};
#define FF_POD_FALL_HEIGHT 45.0f
/* port: how far over a pod's spot a kill volume refuses it */
#define FF_POD_KILL_VOLUME_HEIGHT 10.0f
/* the pod: the game's props' capsule (tools/prop_assets.py), else Two
Betrayals' storage crate. The capsule is its Firefight copy, the drop area's
tag, which is the props' alone: the map it was taken from has its own
altis\scenery\capsule\capsule (a weapon spawner, a health pack in it), which a
level plays in place of the props' of that name */
#define FF_POD_TAG "firefight\\props\\drop_area_pod"
#define FF_POD_FALLBACK_TAG "scenery\\c_storage\\c_storage"
/* an ammo crate's pod: the game's resupply rack (the props', weapon_racks.c's),
which any player by it refills at with action for as long as it stands; a
drop has one at most, everyone's. Without the props, the pod as the others,
the weapon in hand refilled by walking into it, once */
#define FF_POD_RACK_TAG "firefight\\props\\weapon_rack_resupply"
/* the capsule stands on its end: its model lies along its x axis, from its
origin at its base (x 0.14) to its top (x -1.0), its open side +z */
#define FF_POD_CAPSULE_BASE 0.141f
/* where its weapon stands in it: so far out of its middle towards its open
side, and so high */
#define FF_POD_CAPSULE_WEAPON_OUT 0.12f
#define FF_POD_CAPSULE_WEAPON_HEIGHT 0.67f
/* a drop area, Forge's FIREFIGHT category's (tools/prop_assets.py's copy of
the capsule, the pods' own tag): where the pods come down, near it, instead
of near the player. Only a marker, taken away on every machine as Firefight
plays (before any pod is made) */
#define FF_DROP_AREA_TAG FF_POD_TAG

/* (the host's) what the pods in the air hold, each pod's (NONE: nothing, or
taken out already); then, landed, each weapon (or powerup) held against its
pod's side at its place till it's taken (or knocked off); and which pods give
what they bring themselves, to a player walking into them (an ammo crate, or
a powerup the level has no tag of) */
static long g_pod_contents[FF_POD_MAX_CONTENTS];
static short g_pod_content_count = 0;
static boolean g_pod_touch[FIREFIGHT_MAX_ORDNANCE_PODS];
static struct
{
	long object_index;
	short pod;
	real_point3d position;
	real_vector3d forward;
	real_vector3d up;
} g_pod_held[FF_POD_MAX_CONTENTS];
static short g_pod_held_count = 0;
/* (each machine's) where each pod shown is (port: each lets go with its
group, firefight_ordnance_pod_release_delay), each one's scenery, how high it
falls from, and the drop they show; and the pods it has seen used up (its
own players told so) */
enum
{
	_pod_phase_waiting = 0,
	_pod_phase_falling,
	_pod_phase_landed,
	_pod_phase_gone
};
static byte g_pod_phase[FIREFIGHT_MAX_ORDNANCE_PODS];
static unsigned long g_pod_spent_seen[(FIREFIGHT_MAX_ORDNANCE_PODS + 31) / 32];
static long g_pod_objects[FIREFIGHT_MAX_ORDNANCE_PODS];
static real g_pod_fall_heights[FIREFIGHT_MAX_ORDNANCE_PODS];
static long g_pod_shown_time = NONE;
static long g_pod_shown_tag = NONE;
/* (each machine's) whether the pods shown are the capsule (else the crate) */
static boolean g_pod_capsule = FALSE;
/* (each machine's) the drop areas the level has, found once it plays */
static real_point3d g_drop_areas[FF_MAX_DROP_AREAS];
static short g_drop_area_count = 0;
static boolean g_drop_areas_found = FALSE;
/* why spots were refused, by check (pod_spot_passes), and the first object
a ray down met: what the test drop says when none is found */
enum
{
	_pod_spot_no_ground,
	_pod_spot_object_under,
	_pod_spot_slope,
	_pod_spot_outside_map,
	_pod_spot_unseen,
	_pod_spot_cramped,
	_pod_spot_roofed,
	_pod_spot_crowded,
	_pod_spot_too_near,
	_pod_spot_enemies,
	_pod_spot_kill_volume,
	NUMBER_OF_POD_SPOT_REFUSALS
};
static long g_pod_spot_refusals[NUMBER_OF_POD_SPOT_REFUSALS];
static long g_pod_spot_blocker = NONE;
#define POD_SPOT_REFUSED(reason) (g_pod_spot_refusals[(reason)]++, FALSE)


static boolean map_vehicles_only(void);
static long pick_map_vehicle(boolean heavy, boolean custom_only);
static long pick_map_weapon(boolean heavy, boolean custom_only);
static boolean map_weapons_only(void);
static boolean weapon_tag_stock(char const *name, char const *lowered);
static boolean vehicle_tag_flies(long vehicle_tag);
static boolean vehicle_tag_hovers(long vehicle_tag);

static unsigned long g_spawner_rng = 0x9E3779B9UL;
static long g_last_aggro_refresh_tick = 0;
/* the generator unit each tracked enemy is sent at (NONE: the players) */
static long g_generator_attack_units[FIREFIGHT_MAX_TRACKED_ENEMIES];

/* ---------- helpers */

static unsigned long spawner_rand(void)
{
	g_spawner_rng = g_spawner_rng * 1664525UL + 1013904223UL;
	return (g_spawner_rng >> 8) & 0xFFFFFFUL;
}

static real spawner_rand_range(real low, real high)
{
	real t = (real)(spawner_rand() & 0xFFFF) / 65535.0f;
	return low + (high - low) * t;
}

static void lowercase_copy(char const *src, char *dst, size_t dst_size)
{
	size_t i = 0;

	if (!src || !dst_size)
	{
		if (dst_size > 0)
			dst[0] = '\0';
		return;
	}
	for (; src[i] != '\0' && i + 1 < dst_size; i++)
	{
		char c = src[i];
		dst[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
	}
	dst[i] = '\0';
}

/* ---------- Unit Toggle Validation Guard (Section 4.2 & 4.9) */

boolean firefight_unit_toggle_enabled(
	enum firefight_unit_toggle_bit unit_bit)
{
	struct firefight_options const *opts = firefight_options_get();
	unsigned short mask = (opts && opts->firefight_unit_toggles != 0) ?
		opts->firefight_unit_toggles : FIREFIGHT_UNIT_MASK_ALL;

	if (unit_bit < 0 || unit_bit >= NUMBER_OF_FIREFIGHT_UNIT_TOGGLES)
		return TRUE;
	return (mask & (1U << unit_bit)) != 0;
}

static unsigned short faction_relevant_unit_mask(
	byte wave_faction)
{
	unsigned short cov = (1U << _ff_unit_covenant_grunts_bit) |
		(1U << _ff_unit_covenant_jackals_bit) |
		(1U << _ff_unit_covenant_elites_bit) |
		(1U << _ff_unit_covenant_specops_stealth_bit) |
		(1U << _ff_unit_covenant_zealots_bit) |
		(1U << _ff_unit_covenant_hunters_bit);
	unsigned short fld = (1U << _ff_unit_flood_infection_bit) |
		(1U << _ff_unit_flood_combat_bit) |
		(1U << _ff_unit_flood_carrier_bit);
	unsigned short snt = (1U << _ff_unit_sentinels_bit);
	unsigned short hum = (1U << _ff_unit_human_marines_bit) |
		(1U << _ff_unit_human_odst_heavies_bit);

	switch (wave_faction)
	{
	case _firefight_faction_covenant:           return cov;
	case _firefight_faction_flood:              return fld;
	case _firefight_faction_grunts:             return (1U << _ff_unit_covenant_grunts_bit);
	case _firefight_faction_elites_heavies:     return cov & ~((1U << _ff_unit_covenant_grunts_bit) | (1U << _ff_unit_covenant_jackals_bit));
	case _firefight_faction_sentinels:          return snt;
	case _firefight_faction_covenant_flood:     return cov | fld;
	case _firefight_faction_covenant_sentinels: return cov | snt;
	case _firefight_faction_flood_sentinels:    return fld | snt;
	case _firefight_faction_unsc_humans:        return hum;
	case _firefight_faction_mixed:
	default:                                    return cov | fld | snt | hum;
	}
}

boolean firefight_validate_unit_toggles(
	byte wave_faction,
	unsigned short candidate_unit_mask)
{
	unsigned short relevant = faction_relevant_unit_mask(wave_faction);

	return (candidate_unit_mask & relevant) != 0;
}

/* ---------- Roster Scanning & Spawn Point Discovery */

int tag_import_tag_imported(long tag_index);
boolean tag_index_is_group(long tag_index, long group_tag);
boolean tag_injector_tag_injected(long tag_index);
int randomizer_stock_tag_exists(long group_tag, char const *name);

/* whether a tag is the map's own: not one copied in from the campaign's
(the randomizer's, the skulls', Firefight's), nor one a Tag Injector profile
added */
static boolean tag_is_maps_own(long tag_index)
{
	return tag_index != NONE && !tag_import_tag_imported(tag_index) && !tag_injector_tag_injected(tag_index);
}

byte firefight_map_content(void)
{
	struct firefight_options const *opts = firefight_options_get();
	byte mode = opts ? (byte)(opts->firefight_map_content & FIREFIGHT_MAP_CONTENT_MODE_MASK) : 0;

	if (mode != _firefight_map_content_include_map && mode != _firefight_map_content_only_map)
		return _firefight_map_content_vanilla;
	return mode;
}

/* ... for one kind (FIREFIGHT_MAP_CONTENT_*_OFF, its advanced row's): VANILLA
when that's turned off */
static byte map_content_of(byte off_bit)
{
	struct firefight_options const *opts = firefight_options_get();

	return opts && (opts->firefight_map_content & off_bit) ? _firefight_map_content_vanilla : firefight_map_content();
}

static boolean name_has_word(char const *lowered, char const *const *words)
{
	short i;

	for (i = 0; words[i]; i++)
	{
		if (strstr(lowered, words[i]))
			return TRUE;
	}
	return FALSE;
}

static void classify_actor_variant(long variant_index)
{
	char const *raw_name;
	char name[256];
	struct actor_variant_definition *variant_def;
	struct firefight_roster_entry *entry;

	if (g_roster_count >= MAX_ROSTER_VARIANTS || variant_index == NONE)
		return;

	variant_def = actor_variant_definition_get(variant_index);
	if (!variant_def || variant_def->unit_reference.index == NONE || variant_def->actor_reference.index == NONE)
		return;

	raw_name = tag_get_name(variant_index);
	if (!raw_name)
		return;
	lowercase_copy(raw_name, name, sizeof(name));

	if (strstr(name, "cinematic") || strstr(name, "cortana") || strstr(name, "keyes") ||
		strstr(name, "monitor") || strstr(name, "engineer"))
	{
		return;
	}

	entry = &g_roster[g_roster_count];
	csmemset(entry, 0, sizeof(*entry));
	entry->variant_tag_index = variant_index;
	entry->maps_own = tag_is_maps_own(variant_index);
	entry->custom_off = FALSE;
	entry->faction = NONE;
	entry->unit_toggle_bit = NONE;
	entry->tier = _ff_tier_infantry;
	entry->subtype = _ff_sub_other;

	if (strstr(name, "characters\\grunt\\"))
	{
		entry->faction = _firefight_faction_covenant;
		entry->unit_toggle_bit = _ff_unit_covenant_grunts_bit;
		entry->tier = strstr(name, "heavy") ? _ff_tier_commander : _ff_tier_fodder;
		entry->subtype = _ff_sub_grunt;
	}
	else if (strstr(name, "characters\\jackal\\"))
	{
		entry->faction = _firefight_faction_covenant;
		entry->unit_toggle_bit = _ff_unit_covenant_jackals_bit;
		entry->tier = _ff_tier_infantry;
		entry->subtype = _ff_sub_jackal;
	}
	else if (strstr(name, "characters\\elite\\"))
	{
		entry->faction = _firefight_faction_covenant;
		if (strstr(name, "commander") || strstr(name, "sword") || strstr(name, "zealot"))
		{
			entry->unit_toggle_bit = _ff_unit_covenant_zealots_bit;
			entry->tier = _ff_tier_boss_heavy;
			entry->subtype = _ff_sub_elite_zealot;
		}
		else if (strstr(name, "specops") || strstr(name, "stealth"))
		{
			entry->unit_toggle_bit = _ff_unit_covenant_specops_stealth_bit;
			entry->tier = _ff_tier_commander;
			entry->subtype = _ff_sub_elite_stealth;
		}
		else if (strstr(name, "major"))
		{
			entry->unit_toggle_bit = _ff_unit_covenant_elites_bit;
			entry->tier = _ff_tier_commander;
			entry->subtype = _ff_sub_elite_major;
		}
		else
		{
			entry->unit_toggle_bit = _ff_unit_covenant_elites_bit;
			entry->tier = _ff_tier_commander;
			entry->subtype = _ff_sub_elite_minor;
		}
	}
	else if (strstr(name, "characters\\hunter\\"))
	{
		entry->faction = _firefight_faction_covenant;
		entry->unit_toggle_bit = _ff_unit_covenant_hunters_bit;
		entry->tier = _ff_tier_boss_heavy;
		entry->subtype = _ff_sub_hunter;
	}
	else if (strstr(name, "flood_infection") || strstr(name, "flood infection"))
	{
		entry->faction = _firefight_faction_flood;
		entry->unit_toggle_bit = _ff_unit_flood_infection_bit;
		entry->tier = _ff_tier_fodder;
	}
	else if (strstr(name, "floodcombat"))
	{
		entry->faction = _firefight_faction_flood;
		entry->unit_toggle_bit = _ff_unit_flood_combat_bit;
		entry->tier = _ff_tier_infantry;
	}
	else if (strstr(name, "floodcarrier"))
	{
		entry->faction = _firefight_faction_flood;
		entry->unit_toggle_bit = _ff_unit_flood_carrier_bit;
		entry->tier = _ff_tier_commander;
	}
	else if (strstr(name, "characters\\sentinel\\"))
	{
		entry->faction = _firefight_faction_sentinels;
		entry->unit_toggle_bit = _ff_unit_sentinels_bit;
		entry->tier = strstr(name, "major") ? _ff_tier_commander : _ff_tier_infantry;
	}
	else if (strstr(name, "characters\\marine") || strstr(name, "characters\\crewman\\"))
	{
		entry->faction = _firefight_faction_unsc_humans;
		entry->is_human_ally_or_enemy = TRUE;
		if (strstr(name, "armored") || strstr(name, "rocket") || strstr(name, "sniper"))
		{
			entry->unit_toggle_bit = _ff_unit_human_odst_heavies_bit;
			entry->tier = strstr(name, "rocket") ? _ff_tier_boss_heavy : _ff_tier_commander;
		}
		else
		{
			entry->unit_toggle_bit = _ff_unit_human_marines_bit;
			entry->tier = strstr(name, "crewman") ? _ff_tier_fodder : _ff_tier_infantry;
		}
	}

	if (entry->faction != NONE && entry->unit_toggle_bit != NONE)
	{
		g_roster_count++;
	}
}

static boolean point_is_in_valid_bsp(real_point3d const *pt)
{
	struct location loc;

	if (!pt)
		return FALSE;
	if (scenario_leaf_index_from_point(pt) == NONE)
		return FALSE;
	scenario_location_from_point(&loc, pt);
	return loc.cluster_index != NONE;
}

static boolean is_sealed_campaign_intro_spot(real_point3d const *pos)
{
	char const *scen_name;

	if (!pos || global_scenario_index == NONE)
		return FALSE;
	scen_name = tag_get_name(global_scenario_index);
	if (!scen_name)
		return FALSE;

	/* a30 (Halo): skip the sealed Bumblebee lifepod at (-275.3, -730.4, -102.7) */
	if (strstr(scen_name, "a30"))
	{
		if (pos->x > -292.0f && pos->x < -258.0f &&
			pos->y > -748.0f && pos->y < -712.0f)
		{
			return TRUE;
		}
	}
	/* a10 (Pillar of Autumn): skip sealed cryo tubes at (-57.8, 37.9) */
	if (strstr(scen_name, "a10"))
	{
		if (pos->x > -72.0f && pos->x < -45.0f &&
			pos->y > 24.0f && pos->y < 52.0f)
		{
			return TRUE;
		}
	}
	return FALSE;
}

static void add_spawn_point(real_point3d const *pos, real facing)
{
	short i;

	if (!pos || g_spawn_point_count >= MAX_SPAWN_POINTS)
		return;
	if (is_sealed_campaign_intro_spot(pos))
		return;

	for (i = 0; i < g_spawn_point_count; i++)
	{
		if (distance_squared3d(&g_spawn_points[i], pos) < 4.0f)
			return;
	}

	g_spawn_points[g_spawn_point_count] = *pos;
	g_spawn_facings[g_spawn_point_count] = facing;
	g_spawn_point_count++;
}

static boolean is_stock_map_stem(char const *stem)
{
	static char const *const k_stock_maps[] = {
		/* Stock Multiplayer (19) */
		"beavercreek", "bloodgulch", "boardingaction", "carousel", "chillout",
		"damnation", "dangercanyon", "deathisland", "gephyrophobia", "hangemhigh",
		"icefields", "infinity", "longest", "prisoner", "putput",
		"ratrace", "sidewinder", "timberland", "wizard",
		/* Stock Campaign (10) */
		"a10", "a30", "a50", "b30", "b40",
		"c10", "c20", "c40", "d20", "d40"
	};
	char lowered[128];
	size_t i;

	if (!stem || !stem[0])
		return FALSE;

	lowercase_copy(stem, lowered, sizeof(lowered));
	for (i = 0; i < NUMBEROF(k_stock_maps); i++)
	{
		if (strcmp(lowered, k_stock_maps[i]) == 0)
			return TRUE;
	}
	return FALSE;
}

static boolean is_default_stock_map(void)
{
	char const *loaded_name = cache_file_loaded_map_name();
	char const *scen_path = (global_scenario_index != NONE) ? tag_get_name(global_scenario_index) : NULL;
	char loaded_stem[128];

	if (loaded_name && loaded_name[0])
	{
		char const *slash = strrchr(loaded_name, '\\');
		char *at_sep;
		if (!slash)
			slash = strrchr(loaded_name, '/');
		lowercase_copy(slash ? (slash + 1) : loaded_name, loaded_stem, sizeof(loaded_stem));
		at_sep = strchr(loaded_stem, '@');
		if (at_sep)
			*at_sep = '\0';

		if (is_stock_map_stem(loaded_stem))
			return TRUE;

		/* If this is a Forge-saved variant (e.g. bloodgulch_forge), check whether its base scenario is a stock map */
		if (strstr(loaded_stem, "_forge") != NULL ||
			(scen_path && (strncmp(scen_path, "levels\\forge\\", 13) == 0 || strncmp(scen_path, "levels/forge/", 13) == 0)))
		{
			if (scen_path && scen_path[0])
			{
				char const *leaf = scen_path;
				char const *p;
				for (p = scen_path; *p; p++)
				{
					if (*p == '\\' || *p == '/')
						leaf = p + 1;
				}
				return is_stock_map_stem(leaf);
			}
		}

		/* Any other custom_maps/<name>.map is a 3rd-party custom map, even if its author kept a stock scenario tag path */
		return FALSE;
	}

	if (scen_path && scen_path[0])
	{
		char const *leaf = scen_path;
		char const *p;
		for (p = scen_path; *p; p++)
		{
			if (*p == '\\' || *p == '/')
				leaf = p + 1;
		}
		return is_stock_map_stem(leaf);
	}

	return TRUE;
}

static boolean ff_marker_matches_round(int round_filter, short round_num, boolean is_bonus)
{
	if (is_bonus)
		return (round_filter == FF_ROUND_ALL || round_filter == FF_ROUND_BONUS);

	switch (round_filter)
	{
	case FF_ROUND_ALL:    return TRUE;
	case FF_ROUND_1:      return (round_num == 1);
	case FF_ROUND_2:      return (round_num == 2);
	case FF_ROUND_3:      return (round_num == 3);
	case FF_ROUND_4:      return (round_num == 4);
	case FF_ROUND_5:      return (round_num == 5);
	case FF_ROUND_1_TO_3: return (round_num >= 1 && round_num <= 3);
	case FF_ROUND_4_TO_5: return (round_num >= 4 && round_num <= 5);
	case FF_ROUND_BONUS:  return FALSE;
	default:              return TRUE;
	}
}

static boolean ff_marker_matches_wave(int wave_filter, short wave_num, boolean is_bonus, boolean is_boss_wave)
{
	if (is_bonus)
		return TRUE;

	switch (wave_filter)
	{
	case FF_WAVE_ALL:    return TRUE;
	case FF_WAVE_1:      return (wave_num == 1);
	case FF_WAVE_2:      return (wave_num == 2);
	case FF_WAVE_3_BOSS: return (wave_num == 3 || is_boss_wave);
	default:             return TRUE;
	}
}

static short count_custom_ff_spawn_markers(unsigned long allowed_type_mask)
{
	struct scenario *scen = global_scenario_try_and_get();
	short count = 0;
	short i;

	if (!scen)
		return 0;

	for (i = 0; i < scen->netgame_flags.count; i++)
	{
		struct scenario_netgame_flag *flag = TAG_BLOCK_GET_ELEMENT(
			&scen->netgame_flags, i, struct scenario_netgame_flag);
		int stype;
		if (!flag || flag->type != FF_NETGAME_FLAG_FIREFIGHT_SPAWN)
			continue;
		stype = FF_UNPACK_SPAWN_TYPE(flag->team_index);
		/* (a route's points, and the players' spawns: no enemy's) */
		if (stype == FF_ROUTE_SPAWN_TYPE || stype == FF_PLAYER_SPAWN_TYPE)
			continue;
		if (allowed_type_mask == 0xFFFFFFFFUL || (allowed_type_mask & (1UL << stype)) != 0)
			count++;
	}
	return count;
}

static boolean map_has_any_custom_ff_spawns(void)
{
	return count_custom_ff_spawn_markers(0xFFFFFFFFUL) > 0;
}

static short collect_matching_custom_ff_spawns(
	unsigned long allowed_type_mask,
	short round_num,
	short wave_num,
	boolean is_bonus,
	boolean is_boss_wave,
	boolean allow_round_wave_fallback,
	real_point3d *out_pts,
	real *out_facings,
	int *out_types,
	short max_out)
{
	struct scenario *scen = global_scenario_try_and_get();
	short count = 0;
	short pass;
	short i;

	if (!scen || max_out <= 0)
		return 0;

	/* Pass 0: Exact (round + wave) match
	 * Pass 1 (if allow_round_wave_fallback): Exact round match, any wave
	 * Pass 2 (if allow_round_wave_fallback): Any round, any wave */
	for (pass = 0; pass < (allow_round_wave_fallback ? 3 : 1) && count == 0; pass++)
	{
		for (i = 0; i < scen->netgame_flags.count && count < max_out; i++)
		{
			struct scenario_netgame_flag *flag = TAG_BLOCK_GET_ELEMENT(
				&scen->netgame_flags, i, struct scenario_netgame_flag);
			int stype, rfilt, wfilt;

			if (!flag || flag->type != FF_NETGAME_FLAG_FIREFIGHT_SPAWN)
				continue;

			stype = FF_UNPACK_SPAWN_TYPE(flag->team_index);
			rfilt = FF_UNPACK_ROUND_FILTER(flag->team_index);
			wfilt = FF_UNPACK_WAVE_FILTER(flag->team_index);

			if ((allowed_type_mask & (1UL << stype)) == 0)
				continue;
			if (pass == 0 && (!ff_marker_matches_round(rfilt, round_num, is_bonus) ||
				!ff_marker_matches_wave(wfilt, wave_num, is_bonus, is_boss_wave)))
			{
				continue;
			}
			if (pass == 1 && !ff_marker_matches_round(rfilt, round_num, is_bonus))
				continue;

			if (out_pts)
				out_pts[count] = flag->position;
			if (out_facings)
				out_facings[count] = flag->facing;
			if (out_types)
				out_types[count] = stype;
			count++;
		}
	}

	return count;
}

static void discover_map_spawn_points(void)
{
	struct scenario *scen = global_scenario_get();
	struct encounter_definition *encounters;
	long encounter_count;
	short i, j;

	g_spawn_point_count = 0;
	if (!scen)
		return;

	/* 1. AI encounter squad starting locations: the map's, as its AI doesn't
	play here and Forge has emptied those squads (port/forge/src/forge_ai.c) */
#if OPENCE_ENABLE_FORGE
	{
		extern struct encounter_definition *forge_ai_map_encounters(long *count);

		encounters = forge_ai_map_encounters(&encounter_count);
	}
#else
	encounters = (struct encounter_definition *)xbox_pointer(scen->ai_encounters.address);
	encounter_count = scen->ai_encounters.count;
#endif
	for (i = 0; encounters && i < encounter_count && g_spawn_point_count < MAX_SPAWN_POINTS; i++)
	{
		struct encounter_definition *enc = &encounters[i];

		for (j = 0; j < enc->squads.count && g_spawn_point_count < MAX_SPAWN_POINTS; j++)
		{
			struct squad_definition *sq = TAG_BLOCK_GET_ELEMENT(
				&enc->squads, j, struct squad_definition);
			short k;
			if (!sq)
				continue;

			for (k = 0; k < sq->starting_locations.count && g_spawn_point_count < MAX_SPAWN_POINTS; k++)
			{
				struct actor_starting_location *loc = TAG_BLOCK_GET_ELEMENT(
					&sq->starting_locations, k, struct actor_starting_location);
				if (loc)
					add_spawn_point(&loc->position, loc->facing);
			}
		}
	}

	/* 2. Scenario player starting locations */
	for (i = 0; i < scen->players.count && g_spawn_point_count < MAX_SPAWN_POINTS; i++)
	{
		struct player_starting_location *loc = TAG_BLOCK_GET_ELEMENT(
			&scen->players, i, struct player_starting_location);
		if (loc)
			add_spawn_point(&loc->position, loc->facing);
	}

	/* 3. Scenario netgame flags & equipment points (excluding custom Firefight Spawn markers) */
	for (i = 0; i < scen->netgame_flags.count && g_spawn_point_count < MAX_SPAWN_POINTS; i++)
	{
		struct scenario_netgame_flag *flag = TAG_BLOCK_GET_ELEMENT(
			&scen->netgame_flags, i, struct scenario_netgame_flag);
		if (flag && flag->type != FF_NETGAME_FLAG_FIREFIGHT_SPAWN &&
			flag->type != FF_NETGAME_FLAG_FIREFIGHT_GENERATOR)
			add_spawn_point(&flag->position, flag->facing);
	}
	for (i = 0; i < scen->netgame_equipment.count && g_spawn_point_count < MAX_SPAWN_POINTS; i++)
	{
		struct scenario_netgame_equipment *eq = TAG_BLOCK_GET_ELEMENT(
			&scen->netgame_equipment, i, struct scenario_netgame_equipment);
		if (eq)
			add_spawn_point(&eq->position, eq->facing);
	}
}

static short count_valid_in_bsp_spawn_points(void)
{
	short count = 0;
	short i;

	for (i = 0; i < g_spawn_point_count; i++)
	{
		if (point_is_in_valid_bsp(&g_spawn_points[i]))
			count++;
	}
	return count;
}

/* ---------- where enemies may stand: ground under a point, and the ground
the players can walk to

A map's netgame flags and equipment include spots the players never come to:
an MCC map's other gametypes' items in a sealed staging room, flags behind a
wall, out past the map's edge (Promenade's). A point inside the structure's
cluster passes point_is_in_valid_bsp all the same, so the waves' spots are
also asked for ground under them and for that ground to be walkable from the
players' starting locations (or a teleporter's ends):

- the walkable surfaces (the structure's pathfinding flags, as bots_routes.c
  reads them) are joined across the edges they share, and across a step's
  riser (a surface between two walkable ones no taller than FF_REACH_STEP),
  as stairs are built;
- the ground surfaces under the starting locations and teleporters make the
  groups that count; a point on walkable ground of no such group is dropped;
- a point whose ground is not walkable (a steep slope, a crate's top: the
  structure's surface under it) is kept, as is the whole list when the map
  has no walkable surfaces or the filter would leave fewer than
  FF_REACH_LEAST points (a map of jumps and lifts: its spots as they were).

debug.txt has what was dropped and why ("firefight: spawn points"). */


#define FF_PATHFINDING_WALKABLE_BIT 6
#define FF_REACH_CORNERS 16
#define FF_REACH_STEP 0.8f
#define FF_REACH_GROUND_DEPTH 6.0f
#define FF_REACH_LEAST 4

static struct
{
	struct structure_bsp const *structure;
	long surface_count;
	long *parent;
} g_reach;

static struct collision_bsp const *reach_collision_bsp(void)
{
	struct structure_bsp const *structure = g_reach.structure;

	if (!structure || structure->collision_bsp.count <= 0)
		return NULL;
	return TAG_BLOCK_GET_ELEMENT(&structure->collision_bsp, 0, struct collision_bsp);
}

static boolean reach_walkable(long surface_index)
{
	byte const *flags;

	if (surface_index < 0 || surface_index >= g_reach.surface_count)
		return FALSE;
	flags = (byte const *)xbox_pointer(g_reach.structure->pathfinding_surfaces.address);
	return TEST_FLAG(flags[surface_index], FF_PATHFINDING_WALKABLE_BIT);
}

/* the surface's corners and the surface across each edge (bots_routes_corners') */
static long reach_corners(struct collision_bsp const *bsp, long surface_index, real_point3d *corners, long *neighbours)
{
	struct collision_surface const *surface;
	long edge_index, count = 0;

	if (surface_index < 0 || surface_index >= bsp->surfaces.count)
		return 0;
	surface = TAG_BLOCK_GET_ELEMENT(&bsp->surfaces, surface_index, struct collision_surface);
	edge_index = surface->first_edge_index;
	do
	{
		struct collision_edge const *edge;
		boolean right;

		if (edge_index < 0 || edge_index >= bsp->edges.count)
			return 0;
		edge = TAG_BLOCK_GET_ELEMENT(&bsp->edges, edge_index, struct collision_edge);
		right = edge->surface_indices[1] == surface_index;
		if (!VALID_INDEX(edge->vertex_indices[right], bsp->vertices.count))
			return 0;
		corners[count] = TAG_BLOCK_GET_ELEMENT(&bsp->vertices, edge->vertex_indices[right], struct collision_vertex)->point;
		neighbours[count] = edge->surface_indices[!right];
		count++;
		edge_index = edge->edge_indices[right];
	} while (edge_index != surface->first_edge_index && count < FF_REACH_CORNERS);
	return count;
}

static long reach_root(long surface_index)
{
	long root = surface_index;

	while (g_reach.parent[root] != root)
		root = g_reach.parent[root];
	while (g_reach.parent[surface_index] != root)
	{
		long next = g_reach.parent[surface_index];

		g_reach.parent[surface_index] = root;
		surface_index = next;
	}
	return root;
}

static void reach_join(long a, long b)
{
	long root_a = reach_root(a), root_b = reach_root(b);

	if (root_a != root_b)
		g_reach.parent[root_b] = root_a;
}

static void reach_release(void)
{
	/* (the game's free, debug_memory.c's, takes no NULL) */
	if (g_reach.parent)
		free(g_reach.parent);
	g_reach.parent = NULL;
	g_reach.structure = NULL;
	g_reach.surface_count = 0;
}

/* the groups of walkable surfaces, of the structure as it is now; FALSE for
a structure with none */
static boolean reach_build(void)
{
	struct structure_bsp const *structure = global_structure_bsp_get();
	struct collision_bsp const *bsp;
	long index;

	reach_release();
	if (!structure || structure->collision_bsp.count <= 0 || structure->pathfinding_surfaces.count <= 0)
		return FALSE;
	g_reach.structure = structure;
	bsp = reach_collision_bsp();
	g_reach.surface_count = MIN((long)bsp->surfaces.count, (long)structure->pathfinding_surfaces.count);
	g_reach.parent = (long *)malloc(sizeof(long) * (size_t)MAX(g_reach.surface_count, 1));
	if (!g_reach.parent)
	{
		reach_release();
		return FALSE;
	}
	for (index = 0; index < g_reach.surface_count; index++)
		g_reach.parent[index] = index;
	for (index = 0; index < g_reach.surface_count; index++)
	{
		real_point3d corners[FF_REACH_CORNERS];
		long neighbours[FF_REACH_CORNERS];
		long count, corner;

		if (!reach_walkable(index))
			continue;
		count = reach_corners(bsp, index, corners, neighbours);
		for (corner = 0; corner < count; corner++)
		{
			long other = neighbours[corner];
			real_point3d riser_corners[FF_REACH_CORNERS];
			long riser_neighbours[FF_REACH_CORNERS];
			long riser_count, k;
			real low = REAL_MAX, high = -REAL_MAX;

			if (other < 0 || other >= g_reach.surface_count)
				continue;
			if (reach_walkable(other))
			{
				reach_join(index, other);
				continue;
			}
			/* (a step's riser: what is across it joins this one) */
			riser_count = reach_corners(bsp, other, riser_corners, riser_neighbours);
			for (k = 0; k < riser_count; k++)
			{
				low = MIN(low, riser_corners[k].z);
				high = MAX(high, riser_corners[k].z);
			}
			if (riser_count < 3 || high - low > FF_REACH_STEP)
				continue;
			for (k = 0; k < riser_count; k++)
			{
				if (reach_walkable(riser_neighbours[k]))
					reach_join(index, riser_neighbours[k]);
			}
		}
	}
	return TRUE;
}

/* the structure's surface under a point (NONE: no ground within
FF_REACH_GROUND_DEPTH), and where */
static long reach_ground(real_point3d const *point, real_point3d *ground)
{
	struct collision_result collision;
	real_point3d start = *point;
	real_vector3d down = { 0.0f, 0.0f, -FF_REACH_GROUND_DEPTH };

	start.z += 0.4f;
	if (collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit),
		&start, &down, NONE, &collision) && collision.type == _collision_result_structure)
	{
		if (ground)
			*ground = collision.point;
		return collision.surface_index;
	}
	return NONE;
}

/* port: where to bring the players something (the ordnance pods, the friendly
reinforcements): a player on foot, `first` before the rest; with all of them
flying (an aircraft in air support, many storeys up) the floor under the
first that has floor under it, not a kill volume's, rather than the air
beside them. FALSE: none of them has any */
#define FF_FLOOR_DEPTH 150.0f
#define FF_FLOOR_ON_FOOT 2.5f
static boolean players_floor_anchor(real_point3d const *positions, long const *units, short count, short first,
	real_point3d *out, long *out_unit)
{
	short pass, n;

	for (pass = 0; pass < 2; pass++)
	{
		for (n = 0; n < count; n++)
		{
			short i = (short)((first + n) % count);
			struct collision_result collision;
			real_point3d start = positions[i];
			real_vector3d down = { 0.0f, 0.0f, -FF_FLOOR_DEPTH };
			real height;

			start.z += 0.4f;
			if (!collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit),
					&start, &down, NONE, &collision) ||
				collision.type != _collision_result_structure)
			{
				continue;
			}
			height = positions[i].z - collision.point.z;
			if (pass == 0 && height > FF_FLOOR_ON_FOOT)
				continue;
			if (pass == 0)
			{
				*out = positions[i];
			}
			else
			{
				if (death_volume_column(&collision.point, 2.0f))
					continue;
				*out = collision.point;
				out->z += 0.3f;
				error(_error_silent, "firefight: the players are all %.1f units or more over the floor: "
					"brought to the floor under one, %.1f units down", FF_FLOOR_ON_FOOT, height);
			}
			if (out_unit)
				*out_unit = units ? units[i] : NONE;
			return TRUE;
		}
	}
	return FALSE;
}

/* port: how far an actor walks a straight line on the level (the AI's
fallback when its path search fails: actor_moving.c), towards a point, at
most most units: on while the ground at each half unit is no more than
FF_STRAIGHT_STEP_UP above the last nor FF_STRAIGHT_DROP below it, no wall
between, and out of the volumes the map kills in (death_volumes.c); the
point it stops at into end. House 2's roofs: the line ran on over their
edges, 6 to 8 units down, and the grunts and jackals died of the fall.
Stopped at an edge, where it would land beyond it (ground within
FF_STRAIGHT_LANDING_DEPTH, in the map, out of the kill volumes) into
landing, TRUE into edge: firefight_actor_drop's */
#define FF_STRAIGHT_STEP 0.5f
#define FF_STRAIGHT_STEP_UP 0.8f
#define FF_STRAIGHT_DROP 1.2f
#define FF_STRAIGHT_LANDING_DEPTH 30.0f
real firefight_straight_walk(real_point3d const *from, real_point3d const *to, real most, real_point3d *end,
	boolean *edge, real_point3d *landing)
{
	return firefight_straight_walk_ex(from, to, most, end, edge, landing, FF_STRAIGHT_STEP_UP, 0, NONE);
}

/* (the objects a walk goes round: not units, not what lies on the ground) */
#define FF_STRAIGHT_OBJECT_FLAGS (FLAG(_collision_test_objects_bit) | FLAG(_collision_test_objects_scenery_bit) | \
	FLAG(_collision_test_objects_machines_bit) | FLAG(_collision_test_objects_vehicles_bit) | \
	FLAG(_collision_test_objects_controls_bit) | FLAG(_collision_test_objects_light_fixtures_bit))

real firefight_straight_walk_ex(real_point3d const *from, real_point3d const *to, real most, real_point3d *end,
	boolean *edge, real_point3d *landing, real step_up, int objects, long ignore_object_index)
{
	/* (the walls: the level's; 1, its objects too; 2, and its units, but the
	walker's own) */
	unsigned long const flags = FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit) |
		(objects ? FF_STRAIGHT_OBJECT_FLAGS : 0) | (objects > 1 ? FLAG(_collision_test_objects_bipeds_bit) : 0);
	unsigned long const ground_flags = FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit) |
		(objects ? FF_STRAIGHT_OBJECT_FLAGS : 0);
	real_point3d ground;
	real_vector3d along;
	real length, walked = 0.0f;

	*end = *from;
	*edge = FALSE;
	*landing = *from;
	if (reach_ground(from, &ground) == NONE)
		return 0.0f;
	*end = ground;
	along.i = to->x - from->x;
	along.j = to->y - from->y;
	along.k = 0.0f;
	length = (real)sqrt(along.i * along.i + along.j * along.j);
	if (length < 0.01f)
		return 0.0f;
	along.i /= length;
	along.j /= length;
	most = MIN(most, length);
	while (walked + FF_STRAIGHT_STEP <= most + 0.01f)
	{
		struct collision_result collision;
		real_point3d start = ground, next;
		real_vector3d step = { along.i * FF_STRAIGHT_STEP, along.j * FF_STRAIGHT_STEP, 0.0f };
		real_vector3d down = { 0.0f, 0.0f, -(step_up + FF_STRAIGHT_DROP) };

		/* (a wall before it) */
		start.z += step_up + 0.1f;
		if (collision_test_vector(flags, &start, &step, ignore_object_index, &collision))
			break;
		/* (ground no further down than a safe drop; else an edge, and
		where a drop over it lands) */
		start.x += step.i;
		start.y += step.j;
		if (!collision_test_vector(ground_flags, &start, &down, NONE, &collision) ||
			collision.type != _collision_result_structure)
		{
			real_vector3d deep = { 0.0f, 0.0f, -(step_up + FF_STRAIGHT_LANDING_DEPTH) };
			struct location location;

			/* (a little past the edge, so the fall clears it) */
			start.x += step.i * 2.0f;
			start.y += step.j * 2.0f;
			if (collision_test_vector(ground_flags, &start, &deep, NONE, &collision) &&
				collision.type == _collision_result_structure && !death_volume_column(&collision.point, 2.0f))
			{
				scenario_location_from_point(&location, &collision.point);
				if (location.cluster_index != NONE)
				{
					*edge = TRUE;
					*landing = collision.point;
				}
			}
			break;
		}
		next = collision.point;
		if (death_volume_column(&next, 2.0f))
			break;
		ground = next;
		walked += FF_STRAIGHT_STEP;
	}
	*end = ground;
	return walked;
}

/* port: a straight walk blocked short of its point (a rock, a parked
vehicle, a slope too steep): a spot off to one side, walked to all the way,
from which the point is walked to further than from here - all the way at
best. FALSE: none, round either side */
boolean firefight_straight_detour(real_point3d const *from, real_point3d const *to, real step_up, int objects,
	long ignore_object_index, real_point3d *via)
{
	static real const angles[] = { 0.6f, -0.6f, 1.2f, -1.2f, 1.8f, -1.8f };
	static real const lengths[] = { 3.0f, 5.0f };
	real_point3d end, landing, best_via;
	boolean edge, found = FALSE;
	real dx = to->x - from->x, dy = to->y - from->y;
	real full = (real)sqrt(dx * dx + dy * dy);
	real base, best = 0.0f;
	int a, l;

	if (full < 1.0f)
		return FALSE;
	base = firefight_straight_walk_ex(from, to, 8.0f, &end, &edge, &landing, step_up, objects, ignore_object_index);
	best = base + 1.0f;
	dx /= full;
	dy /= full;
	for (a = 0; a < (int)NUMBEROF(angles); a++)
	{
		real c = (real)cos(angles[a]), sn = (real)sin(angles[a]);
		real rx = dx * c - dy * sn, ry = dx * sn + dy * c;

		for (l = 0; l < (int)NUMBEROF(lengths); l++)
		{
			real_point3d candidate, reached;
			real first, second, rest;

			candidate.x = from->x + rx * lengths[l];
			candidate.y = from->y + ry * lengths[l];
			candidate.z = from->z;
			first = firefight_straight_walk_ex(from, &candidate, lengths[l], &reached, &edge, &landing, step_up, objects, ignore_object_index);
			if (first < lengths[l] - 0.51f)
				continue;
			rest = (real)sqrt((to->x - reached.x) * (to->x - reached.x) + (to->y - reached.y) * (to->y - reached.y));
			second = firefight_straight_walk_ex(&reached, to, 8.0f, &end, &edge, &landing, step_up, objects, ignore_object_index);
			/* (from there all the way, or as far as a walk goes: taken at once) */
			if (second >= MIN(rest, 8.0f) - 0.51f)
			{
				*via = reached;
				return TRUE;
			}
			if (second > best)
			{
				best = second;
				best_via = reached;
				found = TRUE;
			}
		}
	}
	if (found)
		*via = best_via;
	return found;
}

/* port: an actor's drop over an edge it chose (actor_moving.c: no path, the
one it fights below, ground to land on): no falling damage from the drop,
until it lands (or FF_DROP_TICKS); a fall it did not choose hurts as
ever. firefight_drops_update, each Firefight tick, lands them */
#define FF_DROPS 32
#define FF_DROP_TICKS (TICKS_PER_SECOND * 6)
static struct
{
	long unit_index;
	long started_tick;
	boolean fell;
	boolean had_flag;
} ff_drops[FF_DROPS];
static long ff_drops_count;

void firefight_actor_drop(long unit_index)
{
	struct unit_datum *unit = unit_try_and_get(unit_index);
	long index, slot = NONE;

	if (!unit)
		return;
	for (index = 0; index < ff_drops_count; index++)
	{
		if (ff_drops[index].unit_index == unit_index)
		{
			ff_drops[index].started_tick = game_time_get();
			return;
		}
	}
	if (ff_drops_count >= FF_DROPS)
		return;
	slot = ff_drops_count++;
	ff_drops[slot].unit_index = unit_index;
	ff_drops[slot].started_tick = game_time_get();
	ff_drops[slot].fell = FALSE;
	ff_drops[slot].had_flag = TEST_FLAG(unit->unit.flags, _unit_no_falling_damage_bit);
	SET_FLAG(unit->unit.flags, _unit_no_falling_damage_bit, TRUE);
	error(_error_silent, "firefight: %s drops over an edge after its target (no falling damage until it lands)",
		tag_get_name(object_get(unit_index)->definition_index));
}

void firefight_drops_update(void)
{
	long index;

	for (index = 0; index < ff_drops_count;)
	{
		struct unit_datum *unit = unit_try_and_get(ff_drops[index].unit_index);
		boolean done = !unit;

		if (unit)
		{
			real vertical = unit->object.translational_velocity.k;

			if (vertical < -0.05f)
				ff_drops[index].fell = TRUE;
			done = (ff_drops[index].fell && vertical > -0.01f) ||
				game_time_get() - ff_drops[index].started_tick > FF_DROP_TICKS;
			if (done && !ff_drops[index].had_flag)
				SET_FLAG(unit->unit.flags, _unit_no_falling_damage_bit, FALSE);
		}
		if (done)
			ff_drops[index] = ff_drops[--ff_drops_count];
		else
			index++;
	}
}

void firefight_drops_reset(void)
{
	ff_drops_count = 0;
}

/* the walkable group of the ground under a point: NONE for ground that is
not walkable or none */
static long reach_group_at(real_point3d const *point)
{
	long surface_index = reach_ground(point, NULL);

	if (!g_reach.parent || !reach_walkable(surface_index))
		return NONE;
	return reach_root(surface_index);
}

/* whether a spot a unit is put at stays this side of the structure from
where it was meant to be, with ground under it */
static boolean spot_reachable_from(real_point3d const *from, real_point3d const *to)
{
	struct collision_result collision;
	real_point3d start = *from, end = *to;
	real_vector3d along;

	start.z += 0.5f;
	end.z += 0.5f;
	vector_from_points3d(&start, &end, &along);
	if (collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit),
		&start, &along, NONE, &collision))
	{
		return FALSE;
	}
	return reach_ground(to, NULL) != NONE;
}

/* the spawn points left with ground under them and on ground the players can
walk to (above) */
static void filter_unreachable_spawn_points(void)
{
	struct scenario *scen = global_scenario_get();
	long seeds[64];
	short seed_count = 0, i, no_ground = 0, cut_off = 0, kept = 0, in_kill_volume = 0;
	boolean grounded[MAX_SPAWN_POINTS];
	boolean reachable[MAX_SPAWN_POINTS];
	boolean filter_groups;

	if (!scen || g_spawn_point_count <= 0)
		return;
	filter_groups = reach_build();
	if (filter_groups)
	{
		for (i = 0; i < scen->players.count && seed_count < (short)NUMBEROF(seeds); i++)
		{
			struct player_starting_location *loc = TAG_BLOCK_GET_ELEMENT(
				&scen->players, i, struct player_starting_location);
			long group = loc ? reach_group_at(&loc->position) : NONE;

			if (group != NONE)
				seeds[seed_count++] = group;
		}
		for (i = 0; i < scen->netgame_flags.count && seed_count < (short)NUMBEROF(seeds); i++)
		{
			struct scenario_netgame_flag *flag = TAG_BLOCK_GET_ELEMENT(
				&scen->netgame_flags, i, struct scenario_netgame_flag);
			long group;

			if (!flag || (flag->type != _netgame_flag_teleporter_source && flag->type != _netgame_flag_teleporter_target))
				continue;
			group = reach_group_at(&flag->position);
			if (group != NONE)
				seeds[seed_count++] = group;
		}
		if (seed_count == 0)
			filter_groups = FALSE;
	}

	for (i = 0; i < g_spawn_point_count; i++)
	{
		real_point3d ground;
		long surface_index = reach_ground(&g_spawn_points[i], &ground);
		short s;

		/* port: ground in or under a volume the map kills in is none */
		if (surface_index != NONE && death_volume_column(&ground, 4.0f))
		{
			surface_index = NONE;
			in_kill_volume++;
		}
		grounded[i] = surface_index != NONE;
		reachable[i] = grounded[i];
		if (!grounded[i])
		{
			no_ground++;
			continue;
		}
		if (!filter_groups || !reach_walkable(surface_index))
			continue;
		{
			long group = reach_root(surface_index);

			for (s = 0; s < seed_count && seeds[s] != group; s++)
				;
			if (s == seed_count)
			{
				reachable[i] = FALSE;
				cut_off++;
			}
		}
	}
	for (i = 0; i < g_spawn_point_count; i++)
		kept += reachable[i] ? 1 : 0;
	error(_error_silent, "firefight: spawn points: %d, %d with no ground under them (%d of those in a kill volume), %d on "
		"ground the players cannot walk to (%s, %d starting groups)", g_spawn_point_count, no_ground, in_kill_volume, cut_off,
		filter_groups ? "walkable groups" : "no walkable groups", seed_count);
	if (kept < FF_REACH_LEAST)
	{
		/* (too few: the grounded ones, else all as they were) */
		short grounded_count = 0;

		for (i = 0; i < g_spawn_point_count; i++)
			grounded_count += grounded[i] ? 1 : 0;
		error(_error_silent, "firefight: spawn points: only %d would be left: keeping %s", kept,
			grounded_count >= FF_REACH_LEAST ? "those with ground" : "them all");
		for (i = 0; i < g_spawn_point_count; i++)
			reachable[i] = grounded_count >= FF_REACH_LEAST ? grounded[i] : TRUE;
	}
	kept = 0;
	for (i = 0; i < g_spawn_point_count; i++)
	{
		if (!reachable[i])
			continue;
		g_spawn_points[kept] = g_spawn_points[i];
		g_spawn_facings[kept] = g_spawn_facings[i];
		kept++;
	}
	g_spawn_point_count = kept;
	reach_release();
}

static void ensure_active_firefight_structure_bsp(void)
{
	struct scenario *scen = global_scenario_get();
	short current_valid;
	short best_bsp = 0;
	short best_count = 0;
	short bsp_idx;
	short write_idx = 0;
	short i;

	if (!scen)
		return;

	current_valid = count_valid_in_bsp_spawn_points();
	if (current_valid < 4 && scen->structure_bsp_references.count > 1)
	{
		best_bsp = global_structure_bsp_index_get();
		if (best_bsp == NONE)
			best_bsp = 0;
		best_count = current_valid;

		for (bsp_idx = 0; bsp_idx < scen->structure_bsp_references.count; bsp_idx++)
		{
			short c;
			if (bsp_idx != global_structure_bsp_index_get())
				scenario_switch_structure_bsp(bsp_idx);
			c = count_valid_in_bsp_spawn_points();
			if (c > best_count)
			{
				best_count = c;
				best_bsp = bsp_idx;
			}
			if (c >= 12)
				break;
		}
		if (global_structure_bsp_index_get() != best_bsp)
			scenario_switch_structure_bsp(best_bsp);
	}

	/* Keep only points verified inside the active Structure BSP */
	for (i = 0; i < g_spawn_point_count; i++)
	{
		if (point_is_in_valid_bsp(&g_spawn_points[i]))
		{
			g_spawn_points[write_idx] = g_spawn_points[i];
			g_spawn_facings[write_idx] = g_spawn_facings[i];
			write_idx++;
		}
	}
	g_spawn_point_count = write_idx;
	filter_unreachable_spawn_points();
}

boolean firefight_select_player_spawn_point(
	long player_index,
	real_point3d *out_position,
	real *out_facing)
{
	struct scenario *scen = global_scenario_get();
	short valid_player_spawns[64];
	short valid_player_count = 0;
	short abs_player = player_index != NONE ? (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index) : 0;
	short i;

	if (!out_position || !out_facing || !scen)
		return FALSE;

	if (g_spawn_point_count == 0)
	{
		discover_map_spawn_points();
		ensure_active_firefight_structure_bsp();
	}

	/* Forge's Firefight player spawns, over the map's starting locations: the
	round's first (any round's else), a player's first spawn on one of its own,
	a respawn on any */
	{
		real_point3d marker_points[32];
		real marker_facings[32];
		short marker_count = collect_matching_custom_ff_spawns(1UL << FF_PLAYER_SPAWN_TYPE,
			ff_state.in_bonus_round ? 0 : ff_state.current_round, 1, ff_state.in_bonus_round, FALSE, TRUE,
			marker_points, marker_facings, NULL, 32);
		short valid[32];
		short valid_count = 0;

		for (i = 0; i < marker_count; i++)
		{
			/* (none in an objective's volume while it lasts: firefight_objectives.c) */
			if (point_is_in_valid_bsp(&marker_points[i]) && !firefight_objectives_spawn_blocked(&marker_points[i]))
				valid[valid_count++] = i;
		}
		if (valid_count > 0)
		{
			struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;
			short pick = player && player->statistics.deaths > 0 ?
				(short)(spawner_rand() % (unsigned long)valid_count) : (short)(abs_player % valid_count);

			*out_position = marker_points[valid[pick]];
			*out_facing = marker_facings[valid[pick]];
			return TRUE;
		}
	}

	for (i = 0; i < scen->players.count && valid_player_count < 64; i++)
	{
		struct player_starting_location *loc = TAG_BLOCK_GET_ELEMENT(
			&scen->players, i, struct player_starting_location);
		if (!loc)
			continue;
		if (is_sealed_campaign_intro_spot(&loc->position))
			continue;
		if (point_is_in_valid_bsp(&loc->position) && !firefight_objectives_spawn_blocked(&loc->position))
			valid_player_spawns[valid_player_count++] = i;
	}

	if (valid_player_count > 0)
	{
		/* everyone at one base, by the ordnance drop that lands by the first
		living player: the starting locations nearest the first one chosen,
		nearest first. A player's first spawn has a place of its own there, a
		respawn any of them */
		short nearest[FF_PLAYER_BASE_SPAWNS];
		real nearest_distance[FF_PLAYER_BASE_SPAWNS];
		short nearest_count = 0;
		struct player_starting_location *base;
		struct player_starting_location *loc;
		struct player_datum *player = player_index != NONE && player_index != 0 ?
			player_try_and_get(player_index) : NULL;
		short turn = abs_player;

		for (i = 0; i < valid_player_count && valid_player_spawns[i] != g_player_base_location; i++)
			;
		if (i == valid_player_count)
			g_player_base_location = valid_player_spawns[spawner_rand() % (unsigned long)valid_player_count];
		base = TAG_BLOCK_GET_ELEMENT(&scen->players, g_player_base_location, struct player_starting_location);
		for (i = 0; i < valid_player_count; i++)
		{
			real distance;
			short place;

			loc = TAG_BLOCK_GET_ELEMENT(&scen->players, valid_player_spawns[i], struct player_starting_location);
			distance = distance_squared3d(&base->position, &loc->position);
			for (place = nearest_count; place > 0 && nearest_distance[place - 1] > distance; place--)
			{
				if (place < FF_PLAYER_BASE_SPAWNS)
				{
					nearest[place] = nearest[place - 1];
					nearest_distance[place] = nearest_distance[place - 1];
				}
			}
			if (place < FF_PLAYER_BASE_SPAWNS)
			{
				nearest[place] = valid_player_spawns[i];
				nearest_distance[place] = distance;
				if (nearest_count < FF_PLAYER_BASE_SPAWNS)
					nearest_count++;
			}
		}
		if (player && player->statistics.deaths > 0)
			turn = (short)(spawner_rand() % (unsigned long)nearest_count);
		loc = TAG_BLOCK_GET_ELEMENT(&scen->players, nearest[turn % nearest_count], struct player_starting_location);
		*out_position = loc->position;
		*out_facing = loc->facing;
		return TRUE;
	}

	if (g_spawn_point_count > 0)
	{
		short pick = (short)((abs_player * 3 + (short)spawner_rand()) % g_spawn_point_count);
		if (pick < 0)
			pick = 0;
		*out_position = g_spawn_points[pick];
		*out_facing = g_spawn_facings[pick];
		return TRUE;
	}

	return FALSE;
}

/* whether a weapon is a game's objective, a flag or a ball, by the bit the
engine knows them by (weapons.c's weapon_is_flag: "must be readied"), not by
its name: a map's own may be named anything. Never a loadout's, a pod's nor
an enemy's: `use` names what asked for it (a loadout's: the name it
looked up), logged once for each of the first few */
boolean firefight_weapon_is_objective(long weapon_tag, char const *use)
{
	static long s_logged[8];
	static short s_logged_count;
	struct weapon_definition *definition;
	short index;

	if (weapon_tag == NONE || !tag_index_is_group(weapon_tag, WEAPON_DEFINITION_TAG))
		return FALSE;
	definition = weapon_definition_get(weapon_tag);
	if (!definition || !((definition->weapon.flags >> 3) & 1))
		return FALSE;
	if (use)
	{
		for (index = 0; index < s_logged_count && s_logged[index] != weapon_tag; index++)
			;
		if (index == s_logged_count)
		{
			if (s_logged_count < (short)NUMBEROF(s_logged))
				s_logged[s_logged_count++] = weapon_tag;
			error(_error_silent, "firefight: %s left out %s (a flag or a ball: must be readied)", use,
				tag_get_name(weapon_tag) ? tag_get_name(weapon_tag) : "?");
		}
	}
	return TRUE;
}

static void scan_ordnance_weapon_and_equipment_tags(void)
{
	struct tag_iterator iter;
	long tag_index;

	ff_power_weapon_tag_count = 0;
	ff_standard_weapon_tag_count = 0;
	ff_ammo_tag_count = 0;
	ff_grenade_tag_count = 0;

	tag_iterator_new(&iter, WEAPON_DEFINITION_TAG);
	while ((tag_index = tag_iterator_next(&iter)) != NONE)
	{
		char const *raw_name = tag_get_name(tag_index);
		char name[256];

		if (!raw_name)
			continue;
		lowercase_copy(raw_name, name, sizeof(name));

		/* Exclude vehicle turrets, flags, oddballs, gravity rifle, and AI-only unpickable weapons */
		if (strstr(name, "vehicles\\") || strstr(name, "flag") || strstr(name, "ball") ||
			strstr(name, "gravity") || strstr(name, "energy sword") || strstr(name, "sentinel") ||
			strstr(name, "gun turret") || strstr(name, "needler\\needler") ||
			firefight_weapon_is_objective(tag_index, "the game's ordnance"))
		{
			continue;
		}

		if (strstr(name, "rocket launcher") ||
			strstr(name, "sniper rifle") ||
			strstr(name, "shotgun") ||
			strstr(name, "plasma_cannon") ||
			strstr(name, "flamethrower"))
		{
			if (ff_power_weapon_tag_count < MAX_ORDNANCE_TAG_POOL)
				ff_power_weapon_tags[ff_power_weapon_tag_count++] = tag_index;
			if ((strstr(name, "shotgun") || strstr(name, "sniper rifle")) &&
				ff_standard_weapon_tag_count < MAX_ORDNANCE_TAG_POOL)
			{
				ff_standard_weapon_tags[ff_standard_weapon_tag_count++] = tag_index;
			}
		}
		else if (strstr(name, "assault rifle") ||
			strstr(name, "pistol") ||
			strstr(name, "plasma rifle") ||
			strstr(name, "plasma pistol") ||
			strstr(name, "mp_needler"))
		{
			if (ff_standard_weapon_tag_count < MAX_ORDNANCE_TAG_POOL)
				ff_standard_weapon_tags[ff_standard_weapon_tag_count++] = tag_index;
		}
	}

	tag_iterator_new(&iter, EQUIPMENT_DEFINITION_TAG);
	while ((tag_index = tag_iterator_next(&iter)) != NONE)
	{
		char const *raw_name = tag_get_name(tag_index);
		char name[256];

		if (!raw_name)
			continue;
		lowercase_copy(raw_name, name, sizeof(name));

		if (strstr(name, "frag grenade") || strstr(name, "plasma grenade"))
		{
			if (ff_grenade_tag_count < MAX_ORDNANCE_TAG_POOL)
				ff_grenade_tags[ff_grenade_tag_count++] = tag_index;
		}
		else if (strstr(name, "ammo") ||
			strstr(name, "health pack") ||
			strstr(name, "shield") ||
			strstr(name, "camouflage") ||
			strstr(name, "double speed"))
		{
			if (ff_ammo_tag_count < MAX_ORDNANCE_TAG_POOL)
				ff_ammo_tags[ff_ammo_tag_count++] = tag_index;
		}
	}
}

/* the variant's profile's tags, from the level's (randomizer.c) */
static void scan_custom_tags(void)
{
	short index;

	ff_custom_enemy_count = 0;
	ff_custom_weapon_count = 0;
	{
		long found[MAX_ROSTER_VARIANTS];
		long count = randomizer_firefight_tags(ACTOR_VARIANT_DEFINITION_TAG, found, MAX_ROSTER_VARIANTS);

		for (index = 0; index < count; index++)
		{
			struct actor_variant_definition *variant_def = actor_variant_definition_get(found[index]);

			if (!variant_def || variant_def->unit_reference.index == NONE || variant_def->actor_reference.index == NONE)
				continue;
			ff_custom_enemy_tags[ff_custom_enemy_count] = found[index];
			ff_custom_enemy_tiers[ff_custom_enemy_count++] = (byte)randomizer_firefight_actor_tier(found[index]);
		}
		count = randomizer_firefight_tags(WEAPON_DEFINITION_TAG, found, MAX_ROSTER_VARIANTS);
		for (index = 0; index < count; index++)
		{
			if (firefight_weapon_is_objective(found[index], "the variant's profile"))
				continue;
			ff_custom_weapon_tags[ff_custom_weapon_count] = found[index];
			ff_custom_weapon_power[ff_custom_weapon_count++] = randomizer_firefight_weapon_power(found[index]);
		}
	}
	if (ff_custom_enemy_count || ff_custom_weapon_count)
		error(_error_silent, "firefight: the variant's profile adds %d AI and %d weapons", ff_custom_enemy_count,
			ff_custom_weapon_count);
}

/* whether the roster has any entry ONLY MAP lets the waves pick */
static boolean roster_has_maps_own(void)
{
	short i;

	for (i = 0; i < g_roster_count; i++)
	{
		if (g_roster[i].maps_own && !g_roster[i].custom_off)
			return TRUE;
	}
	return FALSE;
}

/* whether ONLY MAP keeps the roster's pick from this entry */
static boolean roster_entry_left_out(struct firefight_roster_entry const *e)
{
	return (g_roster_map_only && !e->maps_own) || e->custom_off;
}

/* one of the profile's AI, or of the map's own (MAP CONTENT), in place of a
stock enemy: half the time, or every time when ONLY MAP leaves the roster
none to pick; of the tier wanted, else of any below the bosses (NONE: the
roster's) */
static long pick_custom_enemy(byte wanted_tier, byte *out_tier)
{
	long tags[MAX_ROSTER_VARIANTS * 2];
	byte tiers[MAX_ROSTER_VARIANTS * 2];
	short pool = 0;
	short candidates[MAX_ROSTER_VARIANTS * 2];
	short count = 0;
	short index;
	boolean always = g_roster_map_only && !roster_has_maps_own();

	for (index = 0; index < ff_custom_enemy_count; index++)
	{
		tags[pool] = ff_custom_enemy_tags[index];
		tiers[pool++] = ff_custom_enemy_tiers[index];
	}
	for (index = 0; index < ff_map_enemy_count; index++)
	{
		tags[pool] = ff_map_enemy_tags[index];
		tiers[pool++] = ff_map_enemy_tiers[index];
	}
	if (!pool || (!always && (spawner_rand() & 1)))
		return NONE;
	for (index = 0; index < pool; index++)
	{
		if (tiers[index] == wanted_tier)
			candidates[count++] = index;
	}
	if (!count)
	{
		for (index = 0; index < pool; index++)
		{
			if (tiers[index] != _ff_tier_boss_heavy || wanted_tier == _ff_tier_boss_heavy)
				candidates[count++] = index;
		}
	}
	/* (all of them bosses, and none wanted: a boss anyway, when there's
	nothing else to pick) */
	if (!count && always)
	{
		for (index = 0; index < pool; index++)
			candidates[count++] = index;
	}
	if (!count)
		return NONE;
	index = candidates[spawner_rand() % (unsigned long)count];
	if (out_tier)
		*out_tier = tiers[index];
	return tags[index];
}

static boolean is_custom_enemy(long variant_tag_index)
{
	short index;

	for (index = 0; index < ff_custom_enemy_count; index++)
	{
		if (ff_custom_enemy_tags[index] == variant_tag_index)
			return TRUE;
	}
	for (index = 0; index < ff_map_enemy_count; index++)
	{
		if (ff_map_enemy_tags[index] == variant_tag_index)
			return TRUE;
	}
	return FALSE;
}

/* one of the profile's weapons for a drop (a heavy one's its power weapons
first), NONE without any */
static long pick_custom_weapon(boolean heavy)
{
	short candidates[MAX_ROSTER_VARIANTS];
	short count = 0;
	short index;

	for (index = 0; heavy && index < ff_custom_weapon_count; index++)
	{
		if (ff_custom_weapon_power[index])
			candidates[count++] = index;
	}
	if (!count)
	{
		for (index = 0; index < ff_custom_weapon_count; index++)
			candidates[count++] = index;
	}
	return count ? ff_custom_weapon_tags[candidates[spawner_rand() % (unsigned long)count]] : NONE;
}

static void scan_map_content(void);

void firefight_spawner_tags_loaded(char const *scenario_name)
{
	(void)scenario_name;
}

void firefight_spawner_initialize_map(void)
{
	struct tag_iterator iter;
	long tag_index;
	short i;

	g_roster_count = 0;
	g_player_base_location = NONE;
	for (i = 0; i < MAX_FRIENDLY_UNITS; i++)
		g_friendly_units[i] = NONE;
	g_friendly_unit_count = 0;
	g_queued_reinforcement_enemies = 0;
	ff_state.enemies_queued = 0;
	g_spawner_rng = (unsigned long)(game_time_get() + 0x5A17C3UL);
	g_last_aggro_refresh_tick = 0;
	for (i = 0; i < FIREFIGHT_MAX_TRACKED_ENEMIES; i++)
		g_generator_attack_units[i] = NONE;

	tag_iterator_new(&iter, ACTOR_VARIANT_DEFINITION_TAG);
	while ((tag_index = tag_iterator_next(&iter)) != NONE)
	{
		classify_actor_variant(tag_index);
	}

	scan_ordnance_weapon_and_equipment_tags();
	scan_custom_tags();
	scan_map_content();
	discover_map_spawn_points();
	ensure_active_firefight_structure_bsp();
}

void firefight_spawner_dispose_map(void)
{
	short i;

	ff_custom_enemy_count = 0;
	ff_custom_weapon_count = 0;
	ff_map_enemy_count = 0;
	ff_map_weapon_count = 0;
	ff_map_vehicle_count = 0;
	g_roster_map_only = FALSE;
	g_roster_count = 0;
	g_spawn_point_count = 0;
	/* (the map's objects went with it) */
	for (i = 0; i < FIREFIGHT_MAX_ORDNANCE_PODS; i++)
		g_pod_objects[i] = NONE;
	g_pod_shown_time = NONE;
	g_drop_area_count = 0;
	g_drop_areas_found = FALSE;
	g_pod_content_count = 0;
	g_pod_held_count = 0;
	g_friendly_unit_count = 0;
	g_queued_reinforcement_enemies = 0;
	ff_state.enemies_queued = 0;
	/* (the idle enemies' follow, and the walkable groups it kept) */
	for (i = 0; i < FIREFIGHT_MAX_TRACKED_ENEMIES; i++)
		g_idle[i].unit_index = NONE;
	g_idle_last_check = 0;
	reach_release();
	ff_power_weapon_tag_count = 0;
	ff_standard_weapon_tag_count = 0;
	ff_ammo_tag_count = 0;
	ff_grenade_tag_count = 0;
}

short firefight_queued_enemies_count(void)
{
	short dropship_pending = (ff_state.active_dropship.phase != DROPSHIP_PHASE_NONE &&
		!ff_state.active_dropship.payload_deployed &&
		ff_state.active_dropship.squad_to_spawn > 0) ?
		ff_state.active_dropship.squad_to_spawn : 0;

	return (short)(g_queued_reinforcement_enemies + dropship_pending);
}

/* ---------- Tracked Enemy Helpers */

void firefight_track_enemy_unit(
	long unit_index,
	byte tier)
{
	short i;

	if (unit_index == NONE)
		return;
	for (i = 0; i < FIREFIGHT_MAX_TRACKED_ENEMIES; i++)
	{
		if (ff_state.tracked_enemy_units[i] == NONE)
		{
			ff_state.tracked_enemy_units[i] = unit_index;
			ff_state.tracked_enemy_tiers[i] = tier;
			ff_state.enemies_alive++;
			return;
		}
	}
}

void firefight_prune_dead_tracked_enemies(void)
{
	short alive = 0;
	short i;

	for (i = 0; i < FIREFIGHT_MAX_TRACKED_ENEMIES; i++)
	{
		long u_idx = ff_state.tracked_enemy_units[i];
		if (u_idx != NONE)
		{
			struct unit_datum *u = (struct unit_datum *)object_try_and_get_and_verify_type(
				u_idx, _object_mask_unit);
			if (u && !TEST_FLAG(u->object.damage_flags, _object_dead_bit))
			{
				alive++;
			}
			else
			{
				ff_state.tracked_enemy_units[i] = NONE;
				ff_state.tracked_enemy_tiers[i] = 0;
			}
		}
	}
	ff_state.enemies_alive = alive;
}

/* ---------- Living Player & Spawn Zone Queries */

static short get_living_player_positions(real_point3d *out_positions, long *out_units, short max_count)
{
	struct data_iterator iter;
	struct player_datum *player;
	short count = 0;

	if (!player_data)
		return 0;

	data_iterator_new(&iter, player_data);
	while ((player = data_iterator_next(&iter)) != NULL && count < max_count)
	{
		if (player->unit_index != NONE)
		{
			struct unit_datum *unit = (struct unit_datum *)object_try_and_get_and_verify_type(
				player->unit_index, _object_mask_unit);
			if (unit && !TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
			{
				if (out_positions)
					object_get_origin(player->unit_index, &out_positions[count]);
				if (out_units)
					out_units[count] = player->unit_index;
				count++;
			}
		}
	}

	return count;
}

static short get_living_human_player_positions(
	real_point3d *out_positions,
	long *out_units,
	short max_count)
{
	struct data_iterator iter;
	struct player_datum *player;
	short count = 0;

	if (!player_data)
		return 0;

	data_iterator_new(&iter, player_data);
	while ((player = data_iterator_next(&iter)) != NULL && count < max_count)
	{
		/* (a game's bots go where they like: the waves come to the people) */
		if (player->unit_index != NONE && !bots_machine_is_bot(player->network_player_data.machine_index))
		{
			struct unit_datum *unit = (struct unit_datum *)object_try_and_get_and_verify_type(
				player->unit_index, _object_mask_unit);
			if (unit && !TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
			{
				if (out_positions)
					object_get_origin(player->unit_index, &out_positions[count]);
				if (out_units)
					out_units[count] = player->unit_index;
				count++;
			}
		}
	}
	return count;
}

/* how far from the players a wave's zones are wanted: a big map's far side
is a walk, and a wave there is one the players go looking for */
#define FF_ZONE_NEAR_DISTANCE 40.0f

/* the one of the players nearest the rest of them, whom the waves and the
drops come to (a player gone off alone doesn't drag them away) */
static short central_player_index(real_point3d const *positions, short count)
{
	real_point3d middle = { 0.0f, 0.0f, 0.0f };
	real best_distance = 0.0f;
	short best = 0;
	short i;

	for (i = 0; i < count; i++)
	{
		middle.x += positions[i].x / (real)count;
		middle.y += positions[i].y / (real)count;
		middle.z += positions[i].z / (real)count;
	}
	for (i = 0; i < count; i++)
	{
		real distance = distance3d(&positions[i], &middle);

		if (i == 0 || distance < best_distance)
		{
			best_distance = distance;
			best = i;
		}
	}
	return best;
}

/* the candidates, nearest first, cut to those near the point: at least one
more than the wave wants, however far (the map's nearest, if none are near).
What's left for the wave to pick from; a small map loses none of them */
static short zones_keep_near(
	real_point3d const *points,
	short *indices,
	short count,
	real_point3d const *anchor,
	short desired)
{
	short kept = 0;
	short i;

	for (i = 1; i < count; i++)
	{
		short index = indices[i];
		real distance = distance3d(&points[index], anchor);
		short j = i;

		while (j > 0 && distance3d(&points[indices[j - 1]], anchor) > distance)
		{
			indices[j] = indices[j - 1];
			j--;
		}
		indices[j] = index;
	}
	while (kept < count && distance3d(&points[indices[kept]], anchor) <= FF_ZONE_NEAR_DISTANCE)
		kept++;
	return MAX(kept, MIN(count, (short)(desired + 1)));
}

static short select_wave_spawn_zones(
	real_point3d *out_zones,
	real *out_facings,
	short desired_zones,
	short round_num,
	short wave_num,
	boolean is_bonus,
	boolean is_boss_wave)
{
	real_point3d player_positions[16];
	short human_count = get_living_human_player_positions(player_positions, NULL, 16);
	struct scenario *scen = global_scenario_get();
	short candidate_indices[MAX_SPAWN_POINTS];
	short candidate_count = 0;
	short selected = 0;
	short i, s;

	g_zone0_drop_kinds = 0;

	/* Custom Forge Firefight Spawn Override:
	 * When the map has placed Firefight Infantry/Dropship Spawn markers ('ffsp' / flag->type == 15),
	 * limit AI wave spawning to the user's placed Firefight Spawn markers! */
	{
		unsigned long infantry_mask =
			(1UL << FF_SPAWN_TYPE_AI_INFANTRY) |
			(1UL << FF_SPAWN_TYPE_AI_AND_VEHICLES) |
			FF_DROP_POINT_TYPE_MASK;
		if (count_custom_ff_spawn_markers(infantry_mask) > 0)
		{
			real_point3d custom_pts[MAX_SPAWN_POINTS];
			real custom_facings[MAX_SPAWN_POINTS];
			int custom_types[MAX_SPAWN_POINTS];
			short custom_count = collect_matching_custom_ff_spawns(
				infantry_mask,
				round_num,
				wave_num,
				is_bonus,
				is_boss_wave,
				TRUE,
				custom_pts,
				custom_facings,
				custom_types,
				MAX_SPAWN_POINTS);

			if (custom_count > 0)
			{
				/* Prefer custom markers >= 5.0u from living players if multiple markers exist */
				for (i = 0; i < custom_count; i++)
				{
					real min_player_dist = 9999.0f;
					short p;
					for (p = 0; p < human_count; p++)
					{
						real d = distance3d(&custom_pts[i], &player_positions[p]);
						if (d < min_player_dist)
							min_player_dist = d;
					}
					if (human_count == 0 || min_player_dist >= 5.0f)
						candidate_indices[candidate_count++] = i;
				}
				if (candidate_count == 0)
				{
					for (i = 0; i < custom_count; i++)
						candidate_indices[candidate_count++] = i;
				}
				if (human_count > 0)
				{
					candidate_count = zones_keep_near(
						custom_pts,
						candidate_indices,
						candidate_count,
						&player_positions[central_player_index(player_positions, human_count)],
						desired_zones);
				}

				/* An enemy drop point spawning a faction the wave has
				(g_drop_kinds_wanted) goes in slot 0, so the drop comes there */
				for (i = 0; i < candidate_count; i++)
				{
					short c_idx = candidate_indices[i];
					unsigned kinds = ff_drop_point_kinds(custom_types[c_idx]);

					if ((kinds & g_drop_kinds_wanted) == 0)
						continue;
					out_zones[0] = custom_pts[c_idx];
					out_facings[0] = custom_facings[c_idx];
					g_zone0_drop_kinds = kinds;
					selected = 1;
					candidate_indices[i] = candidate_indices[--candidate_count];
					break;
				}

				while (selected < desired_zones && candidate_count > 0)
				{
					short pick_slot = (short)(spawner_rand() % (unsigned long)candidate_count);
					short c_idx = candidate_indices[pick_slot];
					boolean too_close = FALSE;

					for (s = 0; s < selected; s++)
					{
						if (distance3d(&out_zones[s], &custom_pts[c_idx]) < 2.5f)
						{
							too_close = TRUE;
							break;
						}
					}

					candidate_indices[pick_slot] = candidate_indices[--candidate_count];
					if (!too_close || candidate_count == 0)
					{
						out_zones[selected] = custom_pts[c_idx];
						out_facings[selected] = custom_facings[c_idx];
						selected++;
					}
				}

				return selected;
			}
		}
	}

	if (g_spawn_point_count == 0)
	{
		discover_map_spawn_points();
		filter_unreachable_spawn_points();
	}

	/* Pass 1: Strict safe distance from living players (>= 16u), generators (>= 12u), and player spawns (>= 10u) */
	for (i = 0; i < g_spawn_point_count; i++)
	{
		real min_player_dist = 9999.0f;
		real min_gen_dist = 9999.0f;
		real min_spawn_dist = 9999.0f;
		short p, g;

		if (!point_is_in_valid_bsp(&g_spawn_points[i]))
			continue;

		for (p = 0; p < human_count; p++)
		{
			real d = distance3d(&g_spawn_points[i], &player_positions[p]);
			if (d < min_player_dist)
				min_player_dist = d;
		}

		for (g = 0; g < ff_state.generator_count && g < FIREFIGHT_MAX_GENERATORS; g++)
		{
			if (ff_state.generators[g].alive)
			{
				real d = distance3d(&g_spawn_points[i], &ff_state.generators[g].position);
				if (d < min_gen_dist)
					min_gen_dist = d;
			}
		}

		if (scen)
		{
			for (p = 0; p < scen->players.count && p < 8; p++)
			{
				struct player_starting_location *loc = TAG_BLOCK_GET_ELEMENT(
					&scen->players, p, struct player_starting_location);
				if (loc)
				{
					real d = distance3d(&g_spawn_points[i], &loc->position);
					if (d < min_spawn_dist)
						min_spawn_dist = d;
				}
			}
		}

		if ((human_count == 0 || (min_player_dist >= 16.0f && min_player_dist <= 85.0f)) &&
			min_gen_dist >= 12.0f &&
			min_spawn_dist >= 10.0f)
		{
			candidate_indices[candidate_count++] = i;
		}
	}

	/* Pass 2: Relaxed safe distance on smaller maps (>= 11u from players and >= 9u from generators) */
	if (candidate_count == 0)
	{
		for (i = 0; i < g_spawn_point_count; i++)
		{
			real min_player_dist = 9999.0f;
			real min_gen_dist = 9999.0f;
			short p, g;

			if (!point_is_in_valid_bsp(&g_spawn_points[i]))
				continue;

			for (p = 0; p < human_count; p++)
			{
				real d = distance3d(&g_spawn_points[i], &player_positions[p]);
				if (d < min_player_dist)
					min_player_dist = d;
			}
			for (g = 0; g < ff_state.generator_count && g < FIREFIGHT_MAX_GENERATORS; g++)
			{
				if (ff_state.generators[g].alive)
				{
					real d = distance3d(&g_spawn_points[i], &ff_state.generators[g].position);
					if (d < min_gen_dist)
						min_gen_dist = d;
				}
			}

			if ((human_count == 0 || min_player_dist >= 11.0f) && min_gen_dist >= 9.0f)
			{
				candidate_indices[candidate_count++] = i;
			}
		}
	}

	/* Pass 3: Pick valid BSP spawn points that are farthest from players and generators */
	if (candidate_count == 0)
	{
		short best_idx = NONE;
		real best_clearance = -1.0f;

		for (i = 0; i < g_spawn_point_count; i++)
		{
			real min_dist = 9999.0f;
			short p, g;

			if (!point_is_in_valid_bsp(&g_spawn_points[i]))
				continue;

			for (p = 0; p < human_count; p++)
			{
				real d = distance3d(&g_spawn_points[i], &player_positions[p]);
				if (d < min_dist)
					min_dist = d;
			}
			for (g = 0; g < ff_state.generator_count && g < FIREFIGHT_MAX_GENERATORS; g++)
			{
				if (ff_state.generators[g].alive)
				{
					real d = distance3d(&g_spawn_points[i], &ff_state.generators[g].position);
					if (d < min_dist)
						min_dist = d;
				}
			}
			if (min_dist >= 7.0f)
				candidate_indices[candidate_count++] = i;
			if (min_dist > best_clearance)
			{
				best_clearance = min_dist;
				best_idx = i;
			}
		}
		if (candidate_count == 0 && best_idx != NONE)
			candidate_indices[candidate_count++] = best_idx;
	}

	if (human_count > 0)
	{
		candidate_count = zones_keep_near(
			g_spawn_points,
			candidate_indices,
			candidate_count,
			&player_positions[central_player_index(player_positions, human_count)],
			desired_zones);
	}

	while (selected < desired_zones && candidate_count > 0)
	{
		short pick_slot = (short)(spawner_rand() % (unsigned long)candidate_count);
		short spawn_idx = candidate_indices[pick_slot];
		boolean too_close = FALSE;

		for (s = 0; s < selected; s++)
		{
			if (distance3d(&out_zones[s], &g_spawn_points[spawn_idx]) < 7.0f)
			{
				too_close = TRUE;
				break;
			}
		}

		candidate_indices[pick_slot] = candidate_indices[--candidate_count];

		if (!too_close || candidate_count == 0)
		{
			out_zones[selected] = g_spawn_points[spawn_idx];
			out_facings[selected] = g_spawn_facings[spawn_idx];
			selected++;
		}
	}

	/* (no spot of the map's: a ring about the first player, each spot this
	side of the structure with ground under it, nearer where 20 units is
	behind a wall; one of none such is the ring's as it was) */
	if (selected == 0 && human_count > 0)
	{
		short tries;

		/* port: about a player on foot, else the floor under them (a pilot's
		ring was in the air) */
		players_floor_anchor(player_positions, NULL, human_count, 0, &player_positions[0], NULL);

		for (s = 0; s < desired_zones; s++)
		{
			real angle = (real)s * (2.0f * 3.14159265f / (real)desired_zones) + spawner_rand_range(0.0f, 1.0f);
			real_point3d candidate = player_positions[0];
			boolean found = FALSE;

			for (tries = 0; tries < 8 && !found; tries++)
			{
				real try_angle = angle + (real)tries * 0.785398f;
				real reach;

				for (reach = 20.0f; reach >= 8.0f && !found; reach -= 4.0f)
				{
					candidate = player_positions[0];
					candidate.x += (real)cos(try_angle) * reach;
					candidate.y += (real)sin(try_angle) * reach;
					candidate.z += 0.3f;
					if (spot_reachable_from(&player_positions[0], &candidate))
					{
						found = TRUE;
						angle = try_angle;
					}
				}
			}
			if (!found)
			{
				candidate = player_positions[0];
				candidate.x += (real)cos(angle) * 20.0f;
				candidate.y += (real)sin(angle) * 20.0f;
				candidate.z += 0.3f;
				error(_error_silent, "firefight: no open ground about the player for a wave's spot: one 20 units off");
			}
			out_zones[selected] = candidate;
			out_facings[selected] = angle + 3.14159265f;
			selected++;
		}
	}

	return selected;
}

/* ---------- AI Actor Creation & Target Aggro Direction */

enum
{
	AI_UNIT_EFFECT_MAGIC_SIGHT = 3
};

static void spawn_insertion_beacon_effect(
	real_point3d const *position,
	 long owner_unit_index,
	 byte wave_faction,
	 boolean is_boss_wave)
{
	long effect_tag = NONE;
	char const *marker_names[1];
	real_point3d marker_points[1];
	real_vector3d marker_forwards[1];

	if (!position)
		return;

	if (wave_faction == _firefight_faction_flood)
		effect_tag = tag_loaded(EFFECT_DEFINITION_TAG, "characters\\floodcarrier\\effects\\carrier burst");
	if (effect_tag == NONE && is_boss_wave)
		effect_tag = tag_loaded(EFFECT_DEFINITION_TAG, "weapons\\plasma grenade\\effects\\explosion");
	if (effect_tag == NONE)
		effect_tag = tag_loaded(EFFECT_DEFINITION_TAG, "effects\\coop teleport");
	if (effect_tag == NONE)
		return;

	marker_names[0] = "";
	marker_points[0] = *position;
	marker_points[0].z += 0.35f;
	marker_forwards[0] = *global_up3d;

	skulls_effect_harmless = TRUE;
	effect_new_unattached_from_markers(
		effect_tag,
		owner_unit_index,
		NULL,
		1,
		marker_names,
		marker_points,
		marker_forwards,
		0.0f,
		0.0f,
		NULL,
		NULL,
		TRUE);
	skulls_effect_harmless = FALSE;
}

/* Firefight's enemies' firing range (world units), of their variant's own */
#define FF_ENEMY_FIRING_RANGE_MIN 10.0f
long config_integer(const char *name);

static void patch_actor_for_firefight_pursuit(
	struct actor_definition *actor_def,
	struct actor_variant_definition *variant_def)
{
	if (actor_def)
	{
		if (actor_def->perception.maximum_vision_distance < 250.0f)
			actor_def->perception.maximum_vision_distance = 250.0f;
		actor_def->perception.central_vision_angle = _pi;
		actor_def->perception.maximum_vision_angle = _pi;
		actor_def->perception.peripheral_vision_angle = _pi;
		if (actor_def->perception.peripheral_distance < 250.0f)
			actor_def->perception.peripheral_distance = 250.0f;
		if (actor_def->perception.hearing_distance < 250.0f)
			actor_def->perception.hearing_distance = 250.0f;

		SET_FLAG(actor_def->flags, _actor_definition_can_see_in_darkness_bit, TRUE);
		SET_FLAG(actor_def->flags, _actor_definition_always_charge_bit, TRUE);
		SET_FLAG(actor_def->flags, _actor_definition_charge_in_attacking_mode_bit, TRUE);
		SET_FLAG(actor_def->flags, _actor_definition_crouch_try_not_to_move_bit, FALSE);
		SET_FLAG(actor_def->flags, _actor_definition_crouch_when_guarding_bit, FALSE);
		/* (not the combat forms: their crouch is the shamble, and they must
		crouch to fire, so charging without it they never shot) */
		if (actor_def->type != _actor_combat_form)
			SET_FLAG(actor_def->flags, _actor_definition_defensive_crouch_while_charging_bit, FALSE);

		/* (not the combat forms: berserk they fire only within their berserk
		range, under 3 units, where they melee instead, so an armed one never
		shot. Without it they still charge, and those with no gun still melee) */
		if (actor_def->type == _actor_hunter ||
			actor_def->type == _actor_carrier_form ||
			actor_def->type == _actor_infection_form)
		{
			SET_FLAG(actor_def->flags, _actor_definition_always_berserk_in_attacking_mode_bit, TRUE);
			if (actor_def->berserk.proximity_berserk_distance < 45.0f)
				actor_def->berserk.proximity_berserk_distance = 45.0f;
		}
	}

	if (variant_def)
	{
		/* (their own range, within reason: 35 for all of them had the
		grunts' pistols firing from across the map; a sniper's no farther
		than a rifle's) */
		if (variant_def->ranged_combat.maximum_firing_range <= 0.0f)
			variant_def->ranged_combat.maximum_firing_range = 18.0f;
		variant_def->ranged_combat.maximum_firing_range = PIN(variant_def->ranged_combat.maximum_firing_range,
			FF_ENEMY_FIRING_RANGE_MIN, (real)PIN(config_integer("game.firefight_enemy_firing_range"), 10, 45));
		if (variant_def->ranged_combat.modified_vision_range < 350.0f)
			variant_def->ranged_combat.modified_vision_range = 350.0f;
		if (variant_def->ranged_combat.combat_range_lower_bound > 1.5f ||
			variant_def->ranged_combat.combat_range_lower_bound <= 0.0f)
		{
			variant_def->ranged_combat.combat_range_lower_bound = 1.5f;
		}
		if (variant_def->ranged_combat.combat_range_upper_bound > 10.0f ||
			variant_def->ranged_combat.combat_range_upper_bound < 4.0f)
		{
			variant_def->ranged_combat.combat_range_upper_bound = 7.5f;
		}
	}
}

static void direct_actor_toward_unit(long actor_index, long target_unit_index)
{
	struct actor_datum *actor;
	struct object_datum *target_obj;
	long prop_index;
	boolean is_generator_target = FALSE;
	short gen_slot = NONE;

	if (actor_index == NONE || target_unit_index == NONE)
		return;

	actor = actor_try_and_get(actor_index);
	if (!actor)
		return;

	actor->meta.force_active = TRUE;
	actor->meta.dormant_desire = FALSE;
	if (!actor->meta.active)
		actor_set_active(actor_index, TRUE);
	if (actor->meta.active && actor->meta.dormant)
		actor_set_dormant(actor_index, FALSE);
	if (!actor->meta.active)
		return;

	actor->external_orders.defending = FALSE;
	actor->emotions.currently_defending = FALSE;
	actor->emotions.forced_to_charge = TRUE;
	if (actor->state.artificial_combat_status < 4)
		actor->state.artificial_combat_status = 4;

	/* prop_get_base_by_unit_index requires target_unit_index to be a biped unit */
	target_obj = object_try_and_get_and_verify_type(target_unit_index, _object_mask_biped);
	if (!target_obj)
		return;

	is_generator_target = firefight_is_generator_object(target_unit_index, &gen_slot);

	prop_index = prop_get_base_by_unit_index(actor_index, target_unit_index, TRUE, TRUE);
	if (prop_index != NONE)
	{
		struct prop_datum *prop = prop_try_and_get(prop_index);

		/* Resolve orphan props to their base parent prop before magic sight acknowledgment */
		if (prop && prop->state >= _prop_state_uninspected_orphan && prop->state <= _prop_state_inspected_orphan)
		{
			prop_index = prop->parent_prop_index;
			prop = prop_index != NONE ? prop_try_and_get(prop_index) : NULL;
		}
		if (prop && (actor->target.target_prop_index != prop_index || prop->state != _prop_state_acknowledged))
		{
			actor_handle_unit_effect(actor_index, prop_index, AI_UNIT_EFFECT_MAGIC_SIGHT);
			prop = prop_try_and_get(prop_index);
		}
		if (prop && prop->state >= _prop_state_becoming_unacknowledged && prop->state <= _prop_state_acknowledged)
		{
			prop->enemy = TRUE;
			prop->ally = FALSE;
			prop->preferred_target = TRUE;
			prop->ineffability = 3; /* _actor_perception_unmistakable */
			if (!is_generator_target && actor->target.target_prop_index == NONE)
			{
				prop->currently_damaging_me = TRUE;
				prop->ticks_since_damage = 0;
			}
			actor->target.target_prop_index = prop_index;
			actor->target.target_type = 4; /* _actor_target_acknowledged_enemy */
		}
	}

	if (!actor->meta.swarm && actor->meta.unit_index != NONE)
	{
		struct unit_datum *u = (struct unit_datum *)object_try_and_get_and_verify_type(
			actor->meta.unit_index, _object_mask_unit);
		if (u && u->object.parent_object_index == NONE)
		{
			real_point3d u_pos;
			real_point3d t_pos;
			real dist;

			object_get_origin(actor->meta.unit_index, &u_pos);
			object_get_origin(target_unit_index, &t_pos);
			dist = distance3d(&u_pos, &t_pos);

			if (!actor->control.path.path.valid && dist > 3.5f)
			{
				if (prop_index != NONE)
					actor_move_to_prop(actor_index, prop_index, is_generator_target ? 3.5f : 5.0f);
				else
					actor_move_to_point(actor_index, &t_pos, NONE, NONE);
			}

			if (magnitude_squared3d(&u->object.translational_velocity) < 0.01f && dist > 4.0f)
			{
				real_vector3d dir;
				vector_from_points3d(&u_pos, &t_pos, &dir);
				dir.k = 0.0f;
				if (normalize3d(&dir) > 0.0f)
				{
					unit_port_run(actor->meta.unit_index, &dir);
				}
			}

			/* If targeting a generator and within firing range with clear aim, encourage firing at the generator */
			if (is_generator_target && dist <= 22.0f)
			{
				real_vector3d aim_dir;
				t_pos.z += 0.55f;
				vector_from_points3d(&u_pos, &t_pos, &aim_dir);
				if (normalize3d(&aim_dir) > 0.0f)
				{
					u->unit.desired_aiming_vector = aim_dir;
					u->unit.desired_looking_vector = aim_dir;
					u->unit.desired_facing_vector = aim_dir;
					if (dist <= 16.0f && (game_time_get() % 30) < 18)
					{
						SET_FLAG(u->unit.control_flags, _unit_control_weapon_primary_trigger_bit, TRUE);
						u->unit.primary_trigger = 1.0f;
					}
				}
			}
		}
	}
}

/* whether a tracked enemy's slot is of those GENERATOR ATTACKERS sends at the
generators: every other slot for half, and so on, as slots fill from the first */
static boolean generator_attacker_slot(short slot)
{
	static byte const slot_order[4] = { 0, 2, 1, 3 };
	short quarters;

	switch (global_variant.game_engine_variant.firefight.generator_attackers)
	{
	case _firefight_generator_attackers_none: quarters = 0; break;
	case _firefight_generator_attackers_25:   quarters = 1; break;
	case _firefight_generator_attackers_75:   quarters = 3; break;
	case _firefight_generator_attackers_all:  quarters = 4; break;
	default:                                  quarters = 2; break;
	}
	return slot_order[slot & 3] < quarters;
}

boolean firefight_actor_attacks_generator(long actor_index, long unit_index)
{
	short i;

	if (unit_index == NONE || ff_state.generator_count <= 0)
		return FALSE;
	for (i = 0; i < FIREFIGHT_MAX_TRACKED_ENEMIES; i++)
	{
		if (g_generator_attack_units[i] == unit_index && ff_state.tracked_enemy_units[i] != NONE)
		{
			struct unit_datum *u = (struct unit_datum *)object_try_and_get_and_verify_type(
				ff_state.tracked_enemy_units[i], _object_mask_unit);

			if (u && u->unit.actor_index == actor_index)
				return TRUE;
		}
	}
	return FALSE;
}

/* ---------- Idle enemies brought to the players

A wave enemy that has not moved (FF_IDLE_MOVE units), fired or been hurt for
FF_IDLE_TICKS is taken to be stuck, or somewhere far, behind a long way
round or a teleporter, the players will not find it: it is put where the
players can walk to (on the ground of a walkable group a player stands on:
the reach groups above), FF_RELOCATE_NEAREST to FF_RELOCATE_FARTHEST from
the nearest player and out of every player's sight (behind walls, or behind
them), with the insertion beacon's effect, its path forgotten and the
players made known to it. FF_RELOCATE_PER_SECOND at most a second; one
moved FF_RELOCATE_MOST times and idle again is taken away (its slot freed,
as one killed, so the wave can end). An enemy riding a vehicle is let out
of it first. The host's alone (the waves are); "firefight: relocated" lines
in debug.txt, with where it was. */

#define FF_IDLE_TICKS (10 * TICKS_PER_SECOND)
#define FF_IDLE_CHECK_TICKS TICKS_PER_SECOND
#define FF_IDLE_MOVE 2.0f
#define FF_RELOCATE_NEAREST 15.0f
#define FF_RELOCATE_FARTHEST 32.0f
#define FF_RELOCATE_WANTED 20.0f
#define FF_RELOCATE_PER_SECOND 2
#define FF_RELOCATE_MOST 4


/* the tick a unit's weapon last fired (NONE: none, or never) */
static long idle_unit_last_fired(long unit_index, struct unit_datum const *unit)
{
	long weapon_index = unit_inventory_get_weapon(unit_index, unit->unit.current_weapon_index);
	struct weapon_datum *weapon = weapon_index == NONE ? NULL : weapon_try_and_get(weapon_index);

	return weapon ? weapon->weapon.game_time_last_fired : NONE;
}

/* whether a spot is out of a player's sight: behind them, or the structure
between */
static boolean relocate_spot_unseen(real_point3d const *spot, long player_unit)
{
	struct unit_datum *unit = unit_try_and_get(player_unit);
	struct collision_result collision;
	real_point3d eye;
	real_point3d target = *spot;
	real_vector3d along;
	real length;

	if (!unit)
		return TRUE;
	object_get_origin(player_unit, &eye);
	eye.z += 0.6f;
	target.z += 0.6f;
	vector_from_points3d(&eye, &target, &along);
	length = magnitude3d(&along);
	if (length > 0.001f &&
		(along.i * unit->unit.aiming_vector.i + along.j * unit->unit.aiming_vector.j +
			along.k * unit->unit.aiming_vector.k) / length < -0.25f)
	{
		return TRUE;
	}
	return collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit),
		&eye, &along, NONE, &collision);
}

/* a spot for an idle enemy, near the players, as the header says; FALSE
for none */
static boolean relocate_spot(real_point3d const *players, long const *player_units, short player_count,
	real_point3d const *taken, short taken_count, real_point3d *out)
{
	long groups[16];
	real_point3d candidates[MAX_SPAWN_POINTS + 48];
	short candidate_count = 0;
	short group_count = 0;
	real best_score = REAL_MAX;
	boolean found = FALSE;
	short i;
	short p;

	if (!g_reach.parent)
		reach_build();
	for (p = 0; p < player_count && group_count < (short)NUMBEROF(groups); p++)
	{
		long group = g_reach.parent ? reach_group_at(&players[p]) : NONE;

		if (group != NONE)
			groups[group_count++] = group;
	}
	for (i = 0; i < g_spawn_point_count && candidate_count < MAX_SPAWN_POINTS; i++)
		candidates[candidate_count++] = g_spawn_points[i];
	/* (and a ring round each player, a few of them) */
	for (p = 0; p < MIN(player_count, 2); p++)
	{
		short ring;

		for (ring = 0; ring < 3; ring++)
		{
			real radius = FF_RELOCATE_NEAREST + 5.0f * (real)ring;
			short angle;

			for (angle = 0; angle < 8 && candidate_count < (short)NUMBEROF(candidates); angle++)
			{
				real theta = (real)angle * (6.2831853f / 8.0f) + (real)ring * 0.4f;

				candidates[candidate_count] = players[p];
				candidates[candidate_count].x += radius * cosf(theta);
				candidates[candidate_count].y += radius * sinf(theta);
				candidates[candidate_count].z += 1.0f;
				candidate_count++;
			}
		}
	}
	for (i = 0; i < candidate_count; i++)
	{
		real_point3d ground;
		long surface_index = reach_ground(&candidates[i], &ground);
		real nearest = REAL_MAX;
		short nearest_player = NONE;
		boolean unseen = TRUE;
		boolean apart = TRUE;
		real score;
		short t;

		if (surface_index == NONE || !point_is_in_valid_bsp(&ground))
			continue;
		if (group_count > 0)
		{
			long group = reach_walkable(surface_index) ? reach_root(surface_index) : NONE;
			short g;

			for (g = 0; g < group_count && groups[g] != group; g++)
				;
			if (g == group_count)
				continue;
		}
		for (p = 0; p < player_count; p++)
		{
			real distance = (real)sqrt(distance_squared3d(&players[p], &ground));

			if (distance < nearest)
			{
				nearest = distance;
				nearest_player = p;
			}
		}
		if (nearest_player == NONE || nearest < FF_RELOCATE_NEAREST || nearest > FF_RELOCATE_FARTHEST)
			continue;
		for (t = 0; t < taken_count && apart; t++)
			apart = distance_squared3d(&taken[t], &ground) > 9.0f;
		for (p = 0; p < player_count && unseen; p++)
			unseen = relocate_spot_unseen(&ground, player_units[p]);
		if (!apart || !unseen)
			continue;
		score = (real)fabs(nearest - FF_RELOCATE_WANTED) + (real)(spawner_rand() % 100) * 0.03f;
		if (score < best_score)
		{
			best_score = score;
			*out = ground;
			found = TRUE;
		}
	}
	return found;
}

/* (a slot's follow begun again: a new enemy in it, or one just moved) */
static void idle_slot_reset(short slot, long unit_index, struct unit_datum const *unit, long now)
{
	if (g_idle[slot].unit_index != unit_index)
		g_idle[slot].moves = 0;
	g_idle[slot].unit_index = unit_index;
	object_get_origin(unit_index, &g_idle[slot].position);
	g_idle[slot].vitality = unit->object.body_vitality + unit->object.shield_vitality;
	g_idle[slot].last_fired = idle_unit_last_fired(unit_index, unit);
	g_idle[slot].active_tick = now;
}

void firefight_relocate_idle_enemies(void)
{
	real_point3d players[16];
	long player_units[16];
	real_point3d taken[FF_RELOCATE_PER_SECOND];
	short taken_count = 0;
	short player_count;
	long now = game_time_get();
	short slot;

	if (now - g_idle_last_check < FF_IDLE_CHECK_TICKS && now >= g_idle_last_check)
		return;
	g_idle_last_check = now;
	player_count = get_living_human_player_positions(players, player_units, 16);
	for (slot = 0; slot < FIREFIGHT_MAX_TRACKED_ENEMIES; slot++)
	{
		long unit_index = ff_state.tracked_enemy_units[slot];
		struct unit_datum *unit;
		real_point3d position;
		real vitality;
		long fired;
		real_point3d spot;
		boolean active;

		if (unit_index == NONE)
		{
			g_idle[slot].unit_index = NONE;
			continue;
		}
		unit = unit_try_and_get(unit_index);
		if (!unit || TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
			continue;
		if (g_idle[slot].unit_index != unit_index)
		{
			idle_slot_reset(slot, unit_index, unit, now);
			continue;
		}
		object_get_origin(unit_index, &position);
		vitality = unit->object.body_vitality + unit->object.shield_vitality;
		fired = idle_unit_last_fired(unit_index, unit);
		active = distance_squared3d(&position, &g_idle[slot].position) > FF_IDLE_MOVE * FF_IDLE_MOVE ||
			vitality < g_idle[slot].vitality - 0.001f || fired != g_idle[slot].last_fired;
		g_idle[slot].vitality = vitality;
		g_idle[slot].last_fired = fired;
		if (active)
		{
			g_idle[slot].position = position;
			g_idle[slot].active_tick = now;
			continue;
		}
		if (now - g_idle[slot].active_tick < FF_IDLE_TICKS || player_count <= 0 ||
			taken_count >= FF_RELOCATE_PER_SECOND)
		{
			continue;
		}
		/* (moved as often as it may be: taken away, the wave's as one killed) */
		if (g_idle[slot].moves >= FF_RELOCATE_MOST)
		{
			error(_error_silent, "firefight: relocated %s %d times and idle again at (%.1f %.1f %.1f): taken away",
				tag_get_name(object_get(unit_index)->definition_index), (int)g_idle[slot].moves, position.x, position.y,
				position.z);
			if (unit->unit.actor_index != NONE)
				actor_delete(unit->unit.actor_index, FALSE);
			object_delete(unit_index);
			ff_state.tracked_enemy_units[slot] = NONE;
			ff_state.tracked_enemy_tiers[slot] = 0;
			if (ff_state.enemies_alive > 0)
				ff_state.enemies_alive--;
			g_idle[slot].unit_index = NONE;
			continue;
		}
		if (!relocate_spot(players, player_units, player_count, taken, taken_count, &spot))
		{
			/* (nowhere now: tried again in a while) */
			g_idle[slot].active_tick = now - FF_IDLE_TICKS + 3 * TICKS_PER_SECOND;
			continue;
		}
		if (unit->unit.parent_seat_index != NONE)
			unit_exit_seat_end(unit_index);
		if (unit->object.parent_object_index != NONE)
			unit_detach_from_parent(unit_index);
		spot.z += 0.1f;
		object_set_position(unit_index, &spot, NULL, NULL);
		taken[taken_count++] = spot;
		g_idle[slot].moves++;
		error(_error_silent, "firefight: relocated %s (%d of %d) from (%.1f %.1f %.1f) to (%.1f %.1f %.1f): idle %ld s",
			tag_get_name(object_get(unit_index)->definition_index), (int)g_idle[slot].moves, FF_RELOCATE_MOST, position.x,
			position.y, position.z, spot.x, spot.y, spot.z, (now - g_idle[slot].active_tick) / TICKS_PER_SECOND);
		spawn_insertion_beacon_effect(&spot, unit_index, 0, FALSE);
		if (unit->unit.actor_index != NONE)
		{
			struct actor_datum *actor = actor_try_and_get(unit->unit.actor_index);
			short nearest = 0;
			short p;

			if (actor)
			{
				actor->control.path.path.valid = FALSE;
				actor->control.path.at_destination = FALSE;
			}
			for (p = 1; p < player_count; p++)
			{
				if (distance_squared3d(&players[p], &spot) < distance_squared3d(&players[nearest], &spot))
					nearest = p;
			}
			direct_actor_toward_unit(unit->unit.actor_index, player_units[nearest]);
		}
		idle_slot_reset(slot, unit_index, unit, now);
	}
}

/* (set while an objective's units are made, firefight_objective_spawn_*: no
unit made then is a wave's, not even a vehicle's driver, so none is tracked
nor sent at the players) */
static boolean g_objective_spawning;

static long spawn_single_actor_at(
	long variant_tag_index,
	byte tier,
	real_point3d const *base_pos,
	real facing,
	short forced_team_index,
	boolean is_wave_enemy)
{
	struct actor_variant_definition *variant_def;
	struct actor_definition *actor_def;
	struct object_placement_data placement;
	real_point3d pos;
	long unit_index;
	long actor_index;
	boolean saved_swarm_flag;

	if (!ai_globals || !ai_globals->ai_initialized_for_map || variant_tag_index == NONE)
		return NONE;

	variant_def = actor_variant_definition_get(variant_tag_index);
	if (!variant_def || variant_def->unit_reference.index == NONE || variant_def->actor_reference.index == NONE)
		return NONE;

	actor_def = actor_definition_get(variant_def->actor_reference.index);
	if (!actor_def)
		return NONE;

	patch_actor_for_firefight_pursuit(actor_def, variant_def);
	/* (the profile's AI fight the players, whatever team its unit is of) */
	if (forced_team_index == NONE && is_custom_enemy(variant_tag_index))
		forced_team_index = _game_team_covenant;

	/* (about the zone's spot, not through a wall nor off a ledge from it) */
	pos = *base_pos;
	pos.x += spawner_rand_range(-1.8f, 1.8f);
	pos.y += spawner_rand_range(-1.8f, 1.8f);
	if (!spot_reachable_from(base_pos, &pos))
		pos = *base_pos;
	pos.z += 0.25f;

	object_placement_data_new(&placement, variant_def->unit_reference.index, NONE);
	placement.position = pos;
	vector3d_from_angle(&placement.forward, facing);
	if (forced_team_index != NONE)
		placement.owner_team_index = forced_team_index;

	unit_index = object_new(&placement);
	if (unit_index == NONE)
		return NONE;

	if (unit_get(unit_index)->object.type == _object_type_biped)
	{
		biped_fix_position(unit_index, NONE, &placement.position, NULL, 1.0f, TRUE, FALSE, FALSE);
	}

	actor_customize_unit(variant_tag_index, unit_index);

	/* Always create Firefight wave actors as individual non-swarm units so each has a 1-to-1 unit_index
	   and never references an unallocated swarm_datum (#65535). */
	saved_swarm_flag = TEST_FLAG(actor_def->flags, _actor_definition_swarm_actor_bit);
	SET_FLAG(actor_def->flags, _actor_definition_swarm_actor_bit, FALSE);

	actor_index = actor_create_for_unit(
		FALSE,
		unit_index,
		variant_tag_index,
		NONE,
		NONE,
		FALSE,
		NONE,
		FALSE,
		2,
		0,
		NONE,
		0);

	SET_FLAG(actor_def->flags, _actor_definition_swarm_actor_bit, saved_swarm_flag);

	if (actor_index == NONE)
	{
		object_delete(unit_index);
		return NONE;
	}

	if (forced_team_index != NONE)
	{
		struct actor_datum *actor = actor_get(actor_index);
		actor->meta.team_index = forced_team_index;
		actor_set_team(actor_index, forced_team_index);
	}

	/* Properly activate and wake the actor so object header and actor dormancy stay in sync */
	{
		struct actor_datum *actor = actor_get(actor_index);
		actor->meta.force_active = TRUE;
		actor->meta.dormant_desire = FALSE;
		if (!actor->meta.active)
			actor_set_active(actor_index, TRUE);
		if (actor->meta.active && actor->meta.dormant)
			actor_set_dormant(actor_index, FALSE);
	}
	object_set_automatic_deactivation(unit_index, FALSE);
	actor_verify_activation(actor_index);

	if (is_wave_enemy && !g_objective_spawning)
	{
		long player_units[16];
		short player_count = get_living_player_positions(NULL, player_units, 16);
		long target_unit = NONE;

		if (player_count > 0)
			target_unit = player_units[spawner_rand() % (unsigned long)player_count];

		if (target_unit != NONE)
			direct_actor_toward_unit(actor_index, target_unit);

		firefight_track_enemy_unit(unit_index, tier);
	}

	return unit_index;
}

/* ---------- Variant Selection for Waves */

static boolean faction_matches_entry(
	byte wave_faction,
	struct firefight_roster_entry const *e)
{
	switch (wave_faction)
	{
	case _firefight_faction_covenant:
		return e->faction == _firefight_faction_covenant;
	case _firefight_faction_flood:
		return e->faction == _firefight_faction_flood;
	case _firefight_faction_grunts:
		return e->unit_toggle_bit == _ff_unit_covenant_grunts_bit;
	case _firefight_faction_elites_heavies:
		return e->faction == _firefight_faction_covenant &&
			(e->tier == _ff_tier_commander || e->tier == _ff_tier_boss_heavy);
	case _firefight_faction_sentinels:
		return e->faction == _firefight_faction_sentinels;
	case _firefight_faction_covenant_flood:
		return e->faction == _firefight_faction_covenant || e->faction == _firefight_faction_flood;
	case _firefight_faction_covenant_sentinels:
		return e->faction == _firefight_faction_covenant || e->faction == _firefight_faction_sentinels;
	case _firefight_faction_flood_sentinels:
		return e->faction == _firefight_faction_flood || e->faction == _firefight_faction_sentinels;
	case _firefight_faction_unsc_humans:
		return e->faction == _firefight_faction_unsc_humans;
	case _firefight_faction_mixed:
	default:
		if (e->is_human_ally_or_enemy && !ff_state.elite_player_mode)
			return FALSE;
		return TRUE;
	}
}

/* the bonus round is the grunts', whatever the variant's enemies are (a map
with none: its own) */
static byte firefight_wave_faction(byte wave_faction, boolean is_bonus)
{
	short i;

	for (i = 0; is_bonus && i < g_roster_count; i++)
	{
		if (g_roster[i].unit_toggle_bit == _ff_unit_covenant_grunts_bit)
			return _firefight_faction_grunts;
	}
	return wave_faction;
}

/* the side a dropship is on: the wave's team, its faction's in a free for all */
static short firefight_faction_team(byte wave_faction)
{
	switch (wave_faction)
	{
	case _firefight_faction_flood:
	case _firefight_faction_flood_sentinels:
		return _game_team_flood;
	case _firefight_faction_sentinels:
		return _game_team_sentinel;
	case _firefight_faction_unsc_humans:
		return _game_team_human;
	default:
		return _game_team_covenant;
	}
}

static boolean firefight_team_fights_players(short team_index)
{
	return team_index == _game_team_covenant || team_index == _game_team_flood ||
		team_index == _game_team_sentinel ||
		(team_index == _game_team_human &&
			global_variant.game_engine_variant.firefight.wave_faction == _firefight_faction_unsc_humans);
}

/* unit_new gives a unit its tag's team only with no game engine running, so
what isn't placed with a team has none here, an enemy of every team: a
Spirit's gunner, Outbreak's Flood, a carrier's infection forms. They get
their tag's, and with the enemies allied the wave's, as spawn_single_actor_at
gives the units it places */
short firefight_actor_unit_team(short owner_team_index, short default_team_index)
{
	struct firefight_options const *opts;
	short team_index = owner_team_index;

	if (!firefight_active())
		return owner_team_index;
	if ((team_index == NONE || team_index == _game_team_default) && firefight_team_fights_players(default_team_index))
		team_index = default_team_index;
	opts = firefight_options_get();
	if (firefight_team_fights_players(team_index) &&
		(!opts || opts->firefight_faction_diplomacy == _firefight_diplomacy_allied_against_players))
	{
		team_index = global_variant.game_engine_variant.firefight.wave_faction == _firefight_faction_unsc_humans ?
			_game_team_human : _game_team_covenant;
	}
	return team_index;
}

static long pick_wave_enemy_variant(
	byte wave_faction,
	boolean is_bonus,
	boolean want_heavy_or_boss,
	byte *out_tier)
{
	short candidates[MAX_ROSTER_VARIANTS];
	short candidate_count = 0;
	short i;
	long custom = pick_custom_enemy(
		is_bonus ? _ff_tier_fodder : want_heavy_or_boss ? _ff_tier_boss_heavy : _ff_tier_infantry, out_tier);

	if (custom != NONE)
		return custom;

	for (i = 0; i < g_roster_count; i++)
	{
		struct firefight_roster_entry const *e = &g_roster[i];

		if (roster_entry_left_out(e) || !faction_matches_entry(wave_faction, e))
			continue;
		if (!firefight_unit_toggle_enabled((enum firefight_unit_toggle_bit)e->unit_toggle_bit))
			continue;

		if (is_bonus)
		{
			if (e->tier != _ff_tier_fodder)
				continue;
		}
		else if (want_heavy_or_boss)
		{
			if (e->tier != _ff_tier_boss_heavy && e->tier != _ff_tier_commander)
				continue;
		}
		else
		{
			if (e->tier == _ff_tier_boss_heavy)
				continue;
		}
		candidates[candidate_count++] = i;
	}

	if (candidate_count == 0)
	{
		for (i = 0; i < g_roster_count; i++)
		{
			struct firefight_roster_entry const *e = &g_roster[i];
			if (roster_entry_left_out(e) || !faction_matches_entry(wave_faction, e))
				continue;
			if (!want_heavy_or_boss && e->tier == _ff_tier_boss_heavy)
				continue;
			candidates[candidate_count++] = i;
		}
	}

	if (candidate_count == 0)
	{
		for (i = 0; i < g_roster_count; i++)
		{
			if (!roster_entry_left_out(&g_roster[i]) &&
				(!g_roster[i].is_human_ally_or_enemy || ff_state.elite_player_mode))
			{
				candidates[candidate_count++] = i;
			}
		}
	}

	/* (ONLY MAP: the map's own enemies, whatever their faction, before the
	game's: a map with Covenant alone still plays a Flood variant) */
	if (candidate_count == 0 && g_roster_map_only)
	{
		long custom;

		for (i = 0; i < g_roster_count; i++)
		{
			if (g_roster[i].maps_own && !g_roster[i].custom_off &&
				(!g_roster[i].is_human_ally_or_enemy || ff_state.elite_player_mode ||
					wave_faction == _firefight_faction_unsc_humans))
			{
				candidates[candidate_count++] = i;
			}
		}
		custom = candidate_count == 0 ? pick_custom_enemy(
			is_bonus ? _ff_tier_fodder : want_heavy_or_boss ? _ff_tier_boss_heavy : _ff_tier_infantry, out_tier) : NONE;
		if (custom != NONE)
			return custom;
	}

	if (candidate_count == 0)
		return NONE;

	{
		short idx = candidates[spawner_rand() % (unsigned long)candidate_count];
		if (out_tier)
			*out_tier = g_roster[idx].tier;
		return g_roster[idx].variant_tag_index;
	}
}

static long pick_variant_for_subtype(
	byte desired_subtype,
	byte wave_faction,
	byte *out_tier,
	boolean *out_force_stealth,
	boolean *out_force_sword)
{
	short candidates[MAX_ROSTER_VARIANTS];
	short candidate_count = 0;
	short i;
	byte target_subtype = desired_subtype;

	if (out_force_stealth)
		*out_force_stealth = FALSE;
	if (out_force_sword)
		*out_force_sword = FALSE;

	if (wave_faction == _firefight_faction_grunts)
		target_subtype = _ff_sub_grunt;

	/* 0. The variant's own AI, of the tier the subtype is */
	{
		long custom = pick_custom_enemy(
			target_subtype == _ff_sub_grunt ? _ff_tier_fodder :
			target_subtype == _ff_sub_elite_major || target_subtype == _ff_sub_elite_stealth ? _ff_tier_commander :
			target_subtype == _ff_sub_elite_zealot || target_subtype == _ff_sub_hunter ? _ff_tier_boss_heavy :
			_ff_tier_infantry,
			out_tier);

		if (custom != NONE)
			return custom;
	}

	/* 1. Exact subtype match among enabled faction units */
	for (i = 0; i < g_roster_count; i++)
	{
		struct firefight_roster_entry const *e = &g_roster[i];

		if (roster_entry_left_out(e) || !faction_matches_entry(wave_faction, e))
			continue;
		if (!firefight_unit_toggle_enabled((enum firefight_unit_toggle_bit)e->unit_toggle_bit))
			continue;
		if (e->subtype == target_subtype)
			candidates[candidate_count++] = i;
	}

	if (candidate_count > 0)
	{
		short idx = candidates[spawner_rand() % (unsigned long)candidate_count];
		if (out_tier)
			*out_tier = g_roster[idx].tier;
		if (target_subtype == _ff_sub_elite_stealth && out_force_stealth)
			*out_force_stealth = TRUE;
		if (target_subtype == _ff_sub_elite_zealot && out_force_sword)
			*out_force_sword = TRUE;
		return g_roster[idx].variant_tag_index;
	}

	/* 2. Covenant Elite fallback when a map only has standard minor/major Elite tags loaded */
	if (target_subtype == _ff_sub_elite_stealth ||
		target_subtype == _ff_sub_elite_zealot ||
		target_subtype == _ff_sub_elite_major ||
		target_subtype == _ff_sub_elite_minor)
	{
		for (i = 0; i < g_roster_count; i++)
		{
			struct firefight_roster_entry const *e = &g_roster[i];
			if (roster_entry_left_out(e) || !faction_matches_entry(wave_faction, e))
				continue;
			if (e->subtype == _ff_sub_elite_major || e->subtype == _ff_sub_elite_minor)
				candidates[candidate_count++] = i;
		}
		if (candidate_count > 0)
		{
			short idx = candidates[spawner_rand() % (unsigned long)candidate_count];
			if (out_tier)
				*out_tier = (target_subtype == _ff_sub_elite_zealot) ? _ff_tier_boss_heavy : _ff_tier_commander;
			if (target_subtype == _ff_sub_elite_stealth && out_force_stealth)
				*out_force_stealth = TRUE;
			if (target_subtype == _ff_sub_elite_zealot && out_force_sword)
				*out_force_sword = TRUE;
			return g_roster[idx].variant_tag_index;
		}
	}

	/* 3. Non-Covenant faction or missing unit tag fallback by matching tier (never upgrading fodder/infantry to boss) */
	{
		byte desired_tier = _ff_tier_infantry;
		if (target_subtype == _ff_sub_grunt)
			desired_tier = _ff_tier_fodder;
		else if (target_subtype == _ff_sub_jackal || target_subtype == _ff_sub_elite_minor)
			desired_tier = _ff_tier_infantry;
		else if (target_subtype == _ff_sub_elite_major || target_subtype == _ff_sub_elite_stealth)
			desired_tier = _ff_tier_commander;
		else if (target_subtype == _ff_sub_elite_zealot || target_subtype == _ff_sub_hunter)
			desired_tier = _ff_tier_boss_heavy;

		for (i = 0; i < g_roster_count; i++)
		{
			struct firefight_roster_entry const *e = &g_roster[i];
			if (roster_entry_left_out(e) || !faction_matches_entry(wave_faction, e))
				continue;
			if (!firefight_unit_toggle_enabled((enum firefight_unit_toggle_bit)e->unit_toggle_bit))
				continue;
			if (e->tier == desired_tier)
				candidates[candidate_count++] = i;
		}
		if (candidate_count > 0)
		{
			short idx = candidates[spawner_rand() % (unsigned long)candidate_count];
			if (out_tier)
				*out_tier = g_roster[idx].tier;
			return g_roster[idx].variant_tag_index;
		}

		return pick_wave_enemy_variant(
			wave_faction,
			target_subtype == _ff_sub_grunt,
			desired_tier == _ff_tier_boss_heavy,
			out_tier);
	}
}

extern boolean unit_can_hold_weapon_definition(long unit_index, long weapon_definition_index);

/* ENEMY LOADOUT: any weapon in any enemy's hands, as the randomizer's crazy
weapons (randomizer_borrowed_weapon_label) */
boolean firefight_enemy_weapons_borrowed(void)
{
	struct firefight_options const *opts;

	if (!firefight_active())
		return FALSE;
	opts = firefight_options_get();
	return opts && opts->firefight_enemy_loadout != _firefight_enemy_loadout_default;
}

/* the animations of the weapon a unit's actor carries, for a weapon the
labels nearest it don't fit */
char const *firefight_unit_own_weapon_label(long unit_index)
{
	struct unit_datum *unit = unit_try_and_get(unit_index);
	short i;

	for (i = 0; unit && i < g_roster_count; i++)
	{
		struct actor_variant_definition *variant = actor_variant_definition_get(g_roster[i].variant_tag_index);

		if (variant->unit_reference.index == unit->definition_index && variant->ranged_combat.reference.index != NONE)
			return weapon_definition_get(variant->ranged_combat.reference.index)->weapon.label;
	}
	return NULL;
}

/* a wave enemy's weapons taken for one of a weapon tag */
static void firefight_give_enemy_weapon(long unit_index, long definition)
{
	struct object_placement_data placement;
	long weapon_index;

	if (firefight_weapon_is_objective(definition, "an enemy's weapon"))
		return;
	object_placement_data_new(&placement, definition, NONE);
	object_get_origin(unit_index, &placement.position);
	weapon_index = object_new(&placement);
	if (weapon_index == NONE)
		return;
	unit_delete_all_weapons(unit_index);
	if (!unit_add_weapon_to_inventory(unit_index, weapon_index, _unit_add_weapon_replace))
		object_delete(weapon_index);
}

/* a wave enemy armed by ENEMY LOADOUT, with what it can hold of the choice
(Flood infection forms, carriers and the like hold nothing, and keep what
they have). Humans' first, then the Covenant's */
static void firefight_arm_wave_enemy(long unit_index)
{
	static char const *const weapon_paths[] =
	{
		"weapons\\assault rifle\\assault rifle",
		"weapons\\pistol\\pistol",
		"weapons\\shotgun\\shotgun",
		"weapons\\sniper rifle\\sniper rifle",
		"weapons\\rocket launcher\\rocket launcher",
		"weapons\\flamethrower\\flamethrower",
		"weapons\\plasma pistol\\plasma pistol",
		"weapons\\plasma rifle\\plasma rifle",
		"weapons\\needler\\mp_needler",
		"weapons\\needler\\needler",
		"weapons\\plasma_cannon\\plasma_cannon",
		"weapons\\fuel rod gun\\fuel rod",
	};
	enum { HUMAN_WEAPONS = 6 };
	struct firefight_options const *opts = firefight_options_get();
	long held[NUMBEROF(weapon_paths) + MAX_ROSTER_VARIANTS];
	short first = 0;
	short last = (short)NUMBEROF(weapon_paths);
	short count = 0;
	short i;
	struct object_placement_data placement;
	long weapon_index;

	if (!firefight_enemy_weapons_borrowed())
		return;
	if (opts->firefight_enemy_loadout == _firefight_enemy_loadout_human)
		last = HUMAN_WEAPONS;
	else if (opts->firefight_enemy_loadout == _firefight_enemy_loadout_covenant)
		first = HUMAN_WEAPONS;
	/* (MAP CONTENT: the map's own weapons, beside the game's or alone; ONLY
	MAP's the game's if the unit can hold none of the map's) */
	if (map_content_of(FIREFIGHT_MAP_CONTENT_WEAPONS_OFF) != _firefight_map_content_vanilla)
	{
		boolean custom_only = !map_weapons_only();

		for (i = 0; i < ff_map_weapon_count; i++)
		{
			if ((!custom_only || !ff_map_weapon_stock[i]) &&
				unit_can_hold_weapon_definition(unit_index, ff_map_weapon_tags[i]))
			{
				held[count++] = ff_map_weapon_tags[i];
			}
		}
	}
	if (count && map_weapons_only())
		last = first;
	for (i = first; i < last; i++)
	{
		long definition = tag_loaded(WEAPON_DEFINITION_TAG, weapon_paths[i]);

		if (definition != NONE && unit_can_hold_weapon_definition(unit_index, definition))
			held[count++] = definition;
	}
	if (!count)
		return;
	firefight_give_enemy_weapon(unit_index, held[spawner_rand() % (unsigned long)count]);
}

/* the percent of wave enemies given one of the map's own weapons with
INCLUDE MAP (ONLY MAP's all that can hold one), ENEMY LOADOUT's DEFAULT */
#define FF_MAP_WEAPON_ENEMY_PERCENT 40

/* a wave enemy of DEFAULT's ENEMY LOADOUT, with MAP CONTENT's weapons on,
given one of the map's that its unit has the animations to hold (a weapon of
a label it has: no animations borrowed, as ENEMY LOADOUT's choices do) */
static void firefight_arm_wave_enemy_map(long unit_index)
{
	long held[MAX_ROSTER_VARIANTS];
	boolean only = map_weapons_only();
	short count = 0;
	short i;

	if (map_content_of(FIREFIGHT_MAP_CONTENT_WEAPONS_OFF) == _firefight_map_content_vanilla || ff_map_weapon_count <= 0)
		return;
	if (!only && (long)(spawner_rand() % 100) >= FF_MAP_WEAPON_ENEMY_PERCENT)
		return;
	for (i = 0; i < ff_map_weapon_count; i++)
	{
		if ((only || !ff_map_weapon_stock[i]) && unit_can_hold_weapon_definition(unit_index, ff_map_weapon_tags[i]))
			held[count++] = ff_map_weapon_tags[i];
	}
	if (count)
		firefight_give_enemy_weapon(unit_index, held[spawner_rand() % (unsigned long)count]);
}

static long spawn_subtype_actor_at(
	byte subtype,
	byte wave_faction,
	real_point3d const *base_pos,
	real facing,
	short forced_team_index,
	boolean is_wave_enemy)
{
	byte tier = _ff_tier_infantry;
	boolean force_stealth = FALSE;
	boolean force_sword = FALSE;
	long variant = pick_variant_for_subtype(
		subtype,
		wave_faction,
		&tier,
		&force_stealth,
		&force_sword);
	long unit_index;

	if (variant == NONE)
		return NONE;

	unit_index = spawn_single_actor_at(
		variant,
		tier,
		base_pos,
		facing,
		forced_team_index,
		is_wave_enemy);
	if (unit_index == NONE)
		return NONE;

	if (force_stealth)
	{
		struct unit_datum *u = unit_try_and_get(unit_index);
		if (u)
		{
			SET_FLAG(u->unit.flags, _unit_active_camouflaged_bit, TRUE);
			u->unit.active_camouflage = 1.0f;
		}
	}

	if (firefight_enemy_weapons_borrowed())
	{
		firefight_arm_wave_enemy(unit_index);
	}
	else if (!force_sword)
	{
		/* (MAP CONTENT: the map's own weapons in enemies' hands too) */
		firefight_arm_wave_enemy_map(unit_index);
	}
	else if (force_sword)
	{
		long sword_def = tag_loaded(WEAPON_DEFINITION_TAG, "weapons\\energy sword\\energy sword");
		if (sword_def != NONE && !unit_has_weapon_definition_index(unit_index, sword_def))
		{
			struct object_placement_data w_place;
			long sword_obj;

			object_placement_data_new(&w_place, sword_def, NONE);
			object_get_origin(unit_index, &w_place.position);
			sword_obj = object_new(&w_place);
			if (sword_obj != NONE)
			{
				if (unit_can_use_weapon(unit_index, sword_obj))
				{
					unit_delete_all_weapons(unit_index);
					if (!unit_add_weapon_to_inventory(unit_index, sword_obj, _unit_add_weapon_starting))
						object_delete(sword_obj);
				}
				else
				{
					object_delete(sword_obj);
				}
			}
		}
	}

	return unit_index;
}

/* ---------- Animated Dropship Insertion & Boss Vehicles (Features 3 & 7) */

enum
{
	_ff_unit_seat_ai_enterable_bit = 1,
	_ff_vehicle_ai_driver_enable_bit = 11,
	_ff_vehicle_ai_driver_flying_bit = 12,
	_ff_vehicle_ai_driver_nondirectional_bit = 13,
	_ff_vehicle_ai_driver_hovering_bit = 14
};

struct ff_vehicle_ai_tag_layout
{
	struct unit_definition unit;
	unsigned long flags;
	short type;
	short pad;
	real maximum_forward_speed;
	real maximum_reverse_speed;
	real speed_acceleration;
	real speed_deceleration;
	byte __unknown308[0x78];
	real ai_sideslip_distance;
	real ai_destination_radius;
	real ai_avoidance_distance;
	real ai_pathfinding_radius;
	real ai_charge_repeat_time;
	real ai_strafing_stop_range;
	real ai_oversteer_angle_lower_bound;
	real ai_oversteer_angle_upper_bound;
	real ai_steering_max_angle;
	real ai_steering_max_throttle;
};

/* (vehicles.c's vehicle types) */
enum
{
	FF_VEHICLE_TYPE_HUMAN_PLANE = 3,
	FF_VEHICLE_TYPE_ALIEN_SCOUT = 4,
	FF_VEHICLE_TYPE_ALIEN_FIGHTER = 5,
	FF_VEHICLE_TYPE_TURRET = 6
};

typedef char ff_vehicle_ai_type_offset_assert[
	offsetof(struct ff_vehicle_ai_tag_layout, type) == 0x2F4 ? 1 : -1];
typedef char ff_vehicle_ai_flags_offset_assert[
	offsetof(struct ff_vehicle_ai_tag_layout, flags) == 0x2F0 ? 1 : -1];
typedef char ff_vehicle_ai_max_speed_offset_assert[
	offsetof(struct ff_vehicle_ai_tag_layout, maximum_forward_speed) == 0x2F8 ? 1 : -1];
typedef char ff_vehicle_ai_sideslip_offset_assert[
	offsetof(struct ff_vehicle_ai_tag_layout, ai_sideslip_distance) == 0x380 ? 1 : -1];
typedef char ff_vehicle_ai_dest_radius_offset_assert[
	offsetof(struct ff_vehicle_ai_tag_layout, ai_destination_radius) == 0x384 ? 1 : -1];
typedef char ff_vehicle_ai_charge_offset_assert[
	offsetof(struct ff_vehicle_ai_tag_layout, ai_charge_repeat_time) == 0x390 ? 1 : -1];
typedef char ff_vehicle_ai_max_throttle_offset_assert[
	offsetof(struct ff_vehicle_ai_tag_layout, ai_steering_max_throttle) == 0x3A4 ? 1 : -1];

/* ---------- Wrecks

A dead vehicle is garbage, as a body is, and Firefight's bodies keep the game
collecting it: a wreck would go the moment no one looked at it. So a wreck is
taken off the garbage and kept here, the oldest going when there are too many. */

enum
{
	FF_MAX_WRECKS = 12,
	FF_MAX_ENEMY_VEHICLES = 32
};

static long g_ff_wrecks[FF_MAX_WRECKS];
static short g_ff_next_wreck;
/* (the vehicles the waves brought, which leave a wreck; an empty one of the
map's is only cleared away) */
static long g_ff_enemy_vehicles[FF_MAX_ENEMY_VEHICLES];
static short g_ff_next_enemy_vehicle;

void firefight_wrecks_reset(void)
{
	short i;

	for (i = 0; i < FF_MAX_WRECKS; i++)
		g_ff_wrecks[i] = NONE;
	for (i = 0; i < FF_MAX_ENEMY_VEHICLES; i++)
		g_ff_enemy_vehicles[i] = NONE;
	g_ff_next_wreck = 0;
	g_ff_next_enemy_vehicle = 0;
}

static void enemy_vehicle_note(long vehicle_index)
{
	g_ff_enemy_vehicles[g_ff_next_enemy_vehicle] = vehicle_index;
	g_ff_next_enemy_vehicle = (short)((g_ff_next_enemy_vehicle + 1) % FF_MAX_ENEMY_VEHICLES);
}

static boolean enemy_vehicle_noted(long vehicle_index)
{
	short i;

	for (i = 0; i < FF_MAX_ENEMY_VEHICLES; i++)
	{
		if (g_ff_enemy_vehicles[i] == vehicle_index)
			return TRUE;
	}
	return FALSE;
}

static void wreck_keep(long vehicle_index)
{
	long oldest = g_ff_wrecks[g_ff_next_wreck];

	if (oldest != NONE && oldest != vehicle_index &&
		object_try_and_get_and_verify_type(oldest, _object_mask_vehicle))
	{
		object_delete(oldest);
	}
	g_ff_wrecks[g_ff_next_wreck] = vehicle_index;
	g_ff_next_wreck = (short)((g_ff_next_wreck + 1) % FF_MAX_WRECKS);
	object_set_garbage(vehicle_index, FALSE);
	SET_FLAG(object_get(vehicle_index)->object.flags, _object_cannot_be_garbage_bit, TRUE);
}

/* whether the vehicle's model has a wrecked look (the Scorpion and Warthog don't) */
static boolean vehicle_has_wreck_model(long vehicle_index)
{
	struct object_definition *definition = object_definition_get(object_get(vehicle_index)->definition_index);
	struct model *model;
	short region_index;

	if (definition->object.model.index == NONE)
		return FALSE;
	model = model_definition_get(definition->object.model.index);
	for (region_index = 0; region_index < model->regions.count; region_index++)
	{
		struct model_region *region = TAG_BLOCK_GET_ELEMENT(&model->regions, region_index, struct model_region);
		short permutation_index;

		for (permutation_index = 0; permutation_index < region->permutations.count; permutation_index++)
		{
			struct model_region_permutation *permutation = TAG_BLOCK_GET_ELEMENT(
				&region->permutations, permutation_index, struct model_region_permutation);

			if (!_stricmp(permutation->name, "~damaged"))
				return TRUE;
		}
	}
	return FALSE;
}

/* a vehicle whose driver is gone goes up. Not by damage: an empty Ghost or
Banshee takes none, and an _mp one has no body to lose */
static void vehicle_wreck(long vehicle_index)
{
	object_permute_region(vehicle_index, "~damaged", NONE, TRUE);
	object_deplete_body(vehicle_index);
	unit_died(vehicle_index, FALSE);
}

/* the _mp Ghost and Banshee have no body to lose, so one that's the enemy's
would never leave a wreck: the campaign's goes first where it's loaded */
static boolean enemy_vehicle_tag_superseded(char const *name)
{
	return (!_stricmp(name, "vehicles\\ghost\\ghost_mp") &&
			tag_loaded(VEHICLE_DEFINITION_TAG, "vehicles\\ghost\\ghost") != NONE) ||
		(!_stricmp(name, "vehicles\\banshee\\banshee_mp") &&
			tag_loaded(VEHICLE_DEFINITION_TAG, "vehicles\\banshee\\banshee") != NONE);
}

static long find_loaded_vehicle_tag(char const *const *names, short count)
{
	short i;

	for (i = 0; i < count; i++)
	{
		long idx = tag_loaded(VEHICLE_DEFINITION_TAG, names[i]);
		if (idx != NONE)
			return idx;
	}
	return NONE;
}

static void project_point_to_ground(real_point3d *pt, real hover_offset)
{
	struct collision_result coll;
	real_point3d ray_start;
	real_vector3d ray_down = { 0.0f, 0.0f, -18.0f };

	if (!pt)
		return;
	ray_start = *pt;
	ray_start.z += 2.0f;
	if (collision_test_vector(FLAG(_collision_test_structure_bit), &ray_start, &ray_down, NONE, &coll))
	{
		pt->x = coll.point.x;
		pt->y = coll.point.y;
		pt->z = coll.point.z + hover_offset;
	}
	else
	{
		pt->z += hover_offset;
	}
}

static void patch_vehicle_for_ai_driver(long vehicle_tag_index, boolean is_banshee, boolean is_hover)
{
	struct ff_vehicle_ai_tag_layout *vdef;

	if (vehicle_tag_index == NONE)
		return;
	vdef = (struct ff_vehicle_ai_tag_layout *)tag_get(VEHICLE_DEFINITION_TAG, vehicle_tag_index);
	if (!vdef)
		return;

	SET_FLAG(vdef->flags, _ff_vehicle_ai_driver_enable_bit, TRUE);
	if (is_banshee)
	{
		SET_FLAG(vdef->flags, _ff_vehicle_ai_driver_flying_bit, TRUE);
	}
	else
	{
		SET_FLAG(vdef->flags, _ff_vehicle_ai_driver_nondirectional_bit, TRUE);
		if (is_hover)
			SET_FLAG(vdef->flags, _ff_vehicle_ai_driver_hovering_bit, TRUE);
	}

	if (vdef->ai_sideslip_distance <= 0.0f)
		vdef->ai_sideslip_distance = is_banshee ? 4.0f : 2.5f;
	if (vdef->ai_destination_radius <= 0.0f)
		vdef->ai_destination_radius = is_banshee ? 3.5f : 2.0f;
	if (vdef->ai_avoidance_distance <= 0.0f)
		vdef->ai_avoidance_distance = is_banshee ? 8.0f : 4.0f;
	if (vdef->ai_pathfinding_radius <= 0.0f)
		vdef->ai_pathfinding_radius = 1.5f;
	if (vdef->ai_charge_repeat_time <= 0.0f)
		vdef->ai_charge_repeat_time = 2.5f;
	if (vdef->ai_strafing_stop_range <= 0.0f)
		vdef->ai_strafing_stop_range = is_banshee ? 6.0f : 4.5f;
	if (vdef->ai_steering_max_angle <= 0.0f)
		vdef->ai_steering_max_angle = 0.7853982f;
	if (vdef->ai_steering_max_throttle <= 0.0f)
		vdef->ai_steering_max_throttle = 1.0f;

	if (vdef->unit.unit.seats.count > 0)
	{
		struct unit_seat *seat0 = TAG_BLOCK_GET_ELEMENT(&vdef->unit.unit.seats, 0, struct unit_seat);
		if (seat0)
		{
			SET_FLAG(seat0->flags, _ff_unit_seat_ai_enterable_bit, TRUE);
			SET_FLAG(seat0->flags, _unit_seat_driver_bit, TRUE);
			SET_FLAG(seat0->flags, _unit_seat_gunner_bit, TRUE);
			/* A wraith's driver rides out of sight inside the hull, as its tag has it:
			 * Forge shows every vehicle's driver, and this one has no pose that fits */
			if (tag_get_name(vehicle_tag_index) && strstr(tag_get_name(vehicle_tag_index), "wraith") != NULL)
				SET_FLAG(seat0->flags, _unit_seat_invisible_bit, TRUE);
		}
	}
}

/* a driver with the animations for seat 0, the tag's label changed if not: and
never left on "stand" from a driver who had nothing better, as the tag is
every vehicle of its kind's */
static void vehicle_seat_fit_driver(long driver_unit, long vehicle_index, struct unit_seat *seat)
{
	static char const *const fallback_seat_labels[] = {
		"W-driver", "G-driver", "B-driver", "S-driver", "stand"
	};
	short lbl_idx;

	if (unit_has_animation_to_enter_seat(driver_unit, vehicle_index, 0) && _stricmp(seat->label, "stand") != 0)
		return;
	for (lbl_idx = 0; lbl_idx < (short)NUMBEROF(fallback_seat_labels); lbl_idx++)
	{
		strncpy(seat->label, fallback_seat_labels[lbl_idx], TAG_STRING_LENGTH);
		seat->label[TAG_STRING_LENGTH] = '\0';
		if (unit_has_animation_to_enter_seat(driver_unit, vehicle_index, 0))
			break;
	}
}

/* seats a vehicle's driver. Entering is cut short here, and it's the animation's
end that hides a driver in an invisible seat, so that's done here too */
static void vehicle_seat_driver(long driver_unit, long vehicle_index, struct unit_seat *seat)
{
	struct unit_datum *v_unit = unit_get(vehicle_index);

	if (unit_has_animation_to_enter_seat(driver_unit, vehicle_index, 0) &&
		unit_enter_seat(driver_unit, vehicle_index, 0))
	{
		unit_abort_animation(driver_unit);
		unit_close(vehicle_index);
		v_unit->unit.driver_object_index = driver_unit;
		v_unit->unit.gunner_object_index = driver_unit;
		v_unit->unit.player_index = NONE;
		SET_FLAG(v_unit->unit.flags, _unit_actively_controlled_bit, TRUE);
		if (TEST_FLAG(seat->flags, _unit_seat_invisible_bit))
			object_set_visibility(driver_unit, FALSE);
	}
}

/* how well an Elite variant suits a driver's seat: 0 for one to keep on foot.
Sword carriers have nothing to do in a vehicle, and commanders are the wave's
gold leaders */
static short elite_driver_rank(char const *name)
{
	if (strstr(name, "elite") == NULL || strstr(name, "sword") != NULL || strstr(name, "stealth") != NULL ||
		strstr(name, "special") != NULL || strstr(name, "specops") != NULL)
	{
		return 0;
	}
	if (strstr(name, "major") != NULL)
		return 3;
	return strstr(name, "minor") != NULL ? 2 : 1;
}

static long pick_vehicle_driver_variant(long fallback_variant, byte wave_faction)
{
	short i;
	short best_rank = 0;
	long best_elite = NONE;
	long marine_candidate = NONE;

	/* Prefer an Elite actor variant because Elites have W-driver, B-driver, and G-driver seat animations */
	for (i = 0; i < g_roster_count; i++)
	{
		long v_idx = g_roster[i].variant_tag_index;
		char const *name = v_idx != NONE ? tag_get_name(v_idx) : NULL;
		short rank;
		if (!name)
			continue;
		rank = elite_driver_rank(name);
		if (rank > best_rank)
		{
			best_rank = rank;
			best_elite = v_idx;
		}
		if (strstr(name, "marine") != NULL || strstr(name, "odst") != NULL)
			marine_candidate = v_idx;
	}

	if (wave_faction != _firefight_faction_unsc_humans && best_elite != NONE)
		return best_elite;

	if (wave_faction == _firefight_faction_unsc_humans && marine_candidate != NONE)
		return marine_candidate;

	for (i = 0; i < g_roster_count; i++)
	{
		long v_idx = g_roster[i].variant_tag_index;
		char const *name = v_idx != NONE ? tag_get_name(v_idx) : NULL;
		if (name && strstr(name, "elite") != NULL)
			return v_idx;
	}

	if (marine_candidate != NONE)
		return marine_candidate;

	return fallback_variant;
}

/* (the flight in progress, set up by start_dropship_insertion) */
static struct ff_route_flight g_dropship_flight;

/* a flight to the zone at this spot: on its route if Forge gave it one, which
this says, or else the automatic path */
static boolean dropship_flight_prepare(struct ff_route_flight *flight, real_point3d const *zone, real facing)
{
	struct scenario *scen = global_scenario_try_and_get();
	real best_distance = 2.0f;
	int route_id = 0;
	boolean has_route = FALSE;
	short i;

	csmemset(flight, 0, sizeof(*flight));

	for (i = 0; scen && i < scen->netgame_flags.count; i++)
	{
		struct scenario_netgame_flag *flag = TAG_BLOCK_GET_ELEMENT(
			&scen->netgame_flags, i, struct scenario_netgame_flag);

		if (flag && flag->type == FF_NETGAME_FLAG_FIREFIGHT_SPAWN &&
			ff_drop_point_kinds(FF_UNPACK_SPAWN_TYPE(flag->team_index)) != 0 &&
			FF_ROUTE_ID(flag->team_index) != 0)
		{
			real distance = distance3d(&flag->position, zone);

			if (distance < best_distance)
			{
				best_distance = distance;
				route_id = FF_ROUTE_ID(flag->team_index);
			}
		}
	}

	for (i = 0; scen && route_id != 0 && i < scen->netgame_flags.count; i++)
	{
		struct scenario_netgame_flag *flag = TAG_BLOCK_GET_ELEMENT(
			&scen->netgame_flags, i, struct scenario_netgame_flag);

		if (flag && flag->type == FF_NETGAME_FLAG_FIREFIGHT_SPAWN &&
			FF_UNPACK_SPAWN_TYPE(flag->team_index) == FF_ROUTE_SPAWN_TYPE &&
			FF_ROUTE_ID(flag->team_index) == route_id)
		{
			int leg = FF_ROUTE_POINT_LEG(flag->team_index);

			ff_route_leg_add(
				&flight->legs[leg],
				FF_ROUTE_POINT_ORDER(flag->team_index),
				flag->position.x,
				flag->position.y,
				flag->position.z);
			flight->styles[leg] = FF_ROUTE_POINT_STYLE(flag->team_index);
			has_route = TRUE;
		}
	}

	if (!has_route)
		ff_route_flight_automatic(flight, zone->x, zone->y, zone->z);
	ff_route_flight_finish(flight, zone->x, zone->y, zone->z, facing);
	return has_route;
}

/* a ship for a flight, facing along it and out of everything's way */
static long dropship_new(long dropship_tag, struct ff_route_flight const *flight, short team)
{
	struct ff_route_pose pose = ff_route_flight_pose(flight, 0.0f);
	struct object_placement_data placement;
	struct object_datum *d_obj;
	long dropship_index;

	object_placement_data_new(&placement, dropship_tag, NONE);
	placement.position.x = pose.position.x;
	placement.position.y = pose.position.y;
	placement.position.z = pose.position.z;
	placement.forward.i = cosine(pose.heading);
	placement.forward.j = sine(pose.heading);
	placement.forward.k = 0.0f;
	normalize3d(&placement.forward);
	placement.owner_team_index = team;
	dropship_index = object_new(&placement);
	if (dropship_index == NONE)
		return NONE;

	d_obj = object_try_and_get_and_verify_type(dropship_index, _object_mask_vehicle);
	vehicle_hover(dropship_index, TRUE);
	object_set_automatic_deactivation(dropship_index, FALSE);
	if (d_obj)
	{
		SET_FLAG(d_obj->object.flags, _object_no_collisions_bit, TRUE);
		SET_FLAG(d_obj->object.flags, _object_at_rest_bit, TRUE);
		SET_FLAG(d_obj->object.damage_flags, _object_cannot_take_damage_bit, TRUE);
	}
	return dropship_index;
}

/* a ship's gunner (seated by the AI after the ship is placed) is on the ship's
side, so that it doesn't shoot the squad it brought */
static void dropship_gunner_follow_ship(struct unit_datum *ship)
{
	long child_index = ship->object.first_child_object_index;
	short safety = 0;

	while (child_index != NONE && safety++ < 16)
	{
		struct object_datum *child = object_try_and_get(child_index);

		if (!child)
			break;
		if (TEST_FLAG(_object_mask_unit, child->object.type) &&
			child->object.owner_team_index != ship->object.owner_team_index)
		{
			long actor_index = ((struct unit_datum *)child)->unit.actor_index;

			child->object.owner_team_index = ship->object.owner_team_index;
			if (actor_index != NONE)
			{
				actor_get(actor_index)->meta.team_index = ship->object.owner_team_index;
				actor_set_team(actor_index, ship->object.owner_team_index);
			}
		}
		child_index = child->object.next_object_index;
	}
}

/* puts a ship where its flight has it on this tick; FALSE when it's gone */
static boolean dropship_fly(long vehicle_index, struct ff_route_pose const *pose)
{
	struct unit_datum *ship = vehicle_index != NONE ?
		(struct unit_datum *)object_try_and_get_and_verify_type(vehicle_index, _object_mask_vehicle) : NULL;
	real_point3d position;
	real_vector3d forward;
	real_vector3d up;

	if (!ship)
		return FALSE;

	position.x = pose->position.x;
	position.y = pose->position.y;
	position.z = pose->position.z;
	forward.i = cosine(pose->heading);
	forward.j = sine(pose->heading);
	forward.k = pose->pitch;
	up.i = -forward.i * pose->pitch;
	up.j = -forward.j * pose->pitch;
	up.k = 1.0f;
	normalize3d(&forward);
	normalize3d(&up);

	/* (set before the move: that's what works out its nodes at this size. A
	camouflage left alone wears off a little each tick, so it's held here) */
	ship->object.scale = pose->scale;
	ship->unit.active_camouflage = pose->cloak;
	object_set_position(vehicle_index, &position, &forward, &up);
	vehicle_hover(vehicle_index, TRUE);
	SET_FLAG(ship->object.flags, _object_no_collisions_bit, TRUE);
	SET_FLAG(ship->object.flags, _object_at_rest_bit, TRUE);
	dropship_gunner_follow_ship(ship);
	return TRUE;
}

/* a slipspace end's flash, as a flight starts and just before it finishes */
static void dropship_flash(
	struct ff_route_flight const *flight,
	real previous_tick,
	real tick,
	struct ff_route_pose const *pose,
	byte wave_faction,
	boolean is_boss_wave)
{
	real flash_tick = (real)(ff_route_flight_total_ticks(flight) - FF_ROUTE_FLASH_TICKS_BEFORE_END);
	boolean starting = previous_tick < 0.0f && flight->styles[FF_ROUTE_APPROACH] == FF_ROUTE_STYLE_SLIPSPACE;
	boolean ending = previous_tick < flash_tick && tick >= flash_tick &&
		flight->styles[FF_ROUTE_DEPARTURE] == FF_ROUTE_STYLE_SLIPSPACE;

	if (starting || ending)
	{
		real_point3d position;

		position.x = pose->position.x;
		position.y = pose->position.y;
		position.z = pose->position.z;
		spawn_insertion_beacon_effect(&position, NONE, wave_faction, is_boss_wave);
	}
}

/* ---------- A flight tested in Forge: the same flight, with no one aboard

Forge's TEST DROP flies one ship and then the next on the same flight, the
second setting off as the first leaves, so two may be in the air at once */

#define FF_DROPSHIP_PREVIEW_SHIPS 2

static struct
{
	long vehicle_index;
	struct ff_route_flight flight;
	real tick;
	short phase;
} g_dropship_preview[FF_DROPSHIP_PREVIEW_SHIPS] = { { NONE }, { NONE } };

static void dropship_preview_stop_ship(short ship)
{
	if (g_dropship_preview[ship].vehicle_index != NONE &&
		object_try_and_get_and_verify_type(g_dropship_preview[ship].vehicle_index, _object_mask_vehicle))
	{
		object_delete(g_dropship_preview[ship].vehicle_index);
	}
	g_dropship_preview[ship].vehicle_index = NONE;
	g_dropship_preview[ship].phase = 0;
}

void firefight_dropship_preview_stop(void)
{
	short ship;

	for (ship = 0; ship < FF_DROPSHIP_PREVIEW_SHIPS; ship++)
		dropship_preview_stop_ship(ship);
}

/* a ship flown to the zone beside any already in the air (the oldest's place
taken when there's no room) */
boolean firefight_dropship_preview_start(real_point3d const *zone, real facing, long dropship_tag)
{
	struct ff_route_pose pose;
	short ship = 0;
	short k;

	for (k = FF_DROPSHIP_PREVIEW_SHIPS - 1; k >= 0; k--)
	{
		if (g_dropship_preview[k].vehicle_index == NONE)
			ship = k;
	}
	dropship_preview_stop_ship(ship);
	dropship_flight_prepare(&g_dropship_preview[ship].flight, zone, facing);
	g_dropship_preview[ship].vehicle_index = dropship_new(dropship_tag, &g_dropship_preview[ship].flight, _game_team_covenant);
	if (g_dropship_preview[ship].vehicle_index == NONE)
		return FALSE;

	g_dropship_preview[ship].tick = 0.0f;
	pose = ff_route_flight_pose(&g_dropship_preview[ship].flight, 0.0f);
	g_dropship_preview[ship].phase = (short)pose.phase;
	dropship_fly(g_dropship_preview[ship].vehicle_index, &pose);
	dropship_flash(&g_dropship_preview[ship].flight, -1.0f, 0.0f, &pose, _firefight_faction_covenant, FALSE);
	return TRUE;
}

/* a frame of the test flights: the newest ship's phase, or 0 once none is in
the air */
short firefight_dropship_preview_update(real seconds)
{
	short newest_phase = 0;
	short ship;

	for (ship = 0; ship < FF_DROPSHIP_PREVIEW_SHIPS; ship++)
	{
		struct ff_route_pose pose;
		real previous_tick = g_dropship_preview[ship].tick;

		if (g_dropship_preview[ship].vehicle_index == NONE)
			continue;
		g_dropship_preview[ship].tick += seconds * (real)FF_ROUTE_TICKS_PER_SECOND;
		if (g_dropship_preview[ship].tick >= (real)ff_route_flight_total_ticks(&g_dropship_preview[ship].flight))
		{
			dropship_preview_stop_ship(ship);
			continue;
		}
		pose = ff_route_flight_pose(&g_dropship_preview[ship].flight, g_dropship_preview[ship].tick);
		if (!dropship_fly(g_dropship_preview[ship].vehicle_index, &pose))
		{
			g_dropship_preview[ship].vehicle_index = NONE;
			g_dropship_preview[ship].phase = 0;
			continue;
		}
		dropship_flash(&g_dropship_preview[ship].flight, previous_tick, g_dropship_preview[ship].tick, &pose,
			_firefight_faction_covenant, FALSE);
		g_dropship_preview[ship].phase = (short)pose.phase;
	}
	{
		real newest_tick = 0.0f;

		for (ship = 0; ship < FF_DROPSHIP_PREVIEW_SHIPS; ship++)
		{
			if (g_dropship_preview[ship].vehicle_index != NONE &&
				(newest_phase == 0 || g_dropship_preview[ship].tick < newest_tick))
			{
				newest_phase = g_dropship_preview[ship].phase;
				newest_tick = g_dropship_preview[ship].tick;
			}
		}
	}
	return newest_phase;
}

/* whether any test ship is leaving the zone now (FF_ROUTE_PHASE_DEPARTURE) */
boolean firefight_dropship_preview_departing(void)
{
	short ship;

	for (ship = 0; ship < FF_DROPSHIP_PREVIEW_SHIPS; ship++)
	{
		if (g_dropship_preview[ship].vehicle_index != NONE && g_dropship_preview[ship].phase == FF_ROUTE_PHASE_DEPARTURE)
			return TRUE;
	}
	return FALSE;
}

/* a client's ship: what the host's state says of how it looks */
void firefight_dropship_apply_look(short absolute_index, real scale, real cloak)
{
	struct object_iterator iterator;
	struct object_datum *object;

	object_iterator_new(&iterator, _object_mask_vehicle, 0);
	while ((object = object_iterator_next(&iterator)) != NULL)
	{
		if (DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.index) == absolute_index)
		{
			object->object.scale = scale;
			((struct unit_datum *)object)->unit.active_camouflage = cloak;
			/* (a client's HUD marks the ship with these) */
			ff_state.active_dropship.vehicle_index = iterator.index;
			ff_state.active_dropship.phase = DROPSHIP_PHASE_DESCENDING;
			return;
		}
	}
}

/* ---------- What brings a wave's drop, by its enemies */

/* the drop kinds (1 << FF_DROP_KIND_*) of the enemies a wave of this faction
can have: the Flood's, the humans', and the rest by Spirit */
static unsigned wave_drop_kinds(byte wave_faction)
{
	unsigned kinds = 0;
	short i;

	for (i = 0; i < g_roster_count; i++)
	{
		struct firefight_roster_entry const *e = &g_roster[i];

		if (roster_entry_left_out(e) || !faction_matches_entry(wave_faction, e) ||
			!firefight_unit_toggle_enabled((enum firefight_unit_toggle_bit)e->unit_toggle_bit))
		{
			continue;
		}
		kinds |= 1U << (e->faction == _firefight_faction_flood ? FF_DROP_KIND_FLOOD :
			e->faction == _firefight_faction_unsc_humans ? FF_DROP_KIND_HUMAN : FF_DROP_KIND_COVENANT);
	}
	return kinds;
}

/* whether a kind's carrier can be made here: its ship's tag or Keyes' Flood
form's (randomizer.c's gather_firefight copies that in from d20) */
static boolean drop_kind_carrier_loaded(int kind)
{
	if (kind == FF_DROP_KIND_FLOOD)
		return tag_loaded('scen', FF_FLOOD_DROP_TAG) != NONE;
	return tag_loaded(VEHICLE_DEFINITION_TAG, "vehicles\\c_dropship\\c_dropship") != NONE ||
		tag_loaded(VEHICLE_DEFINITION_TAG, "vehicles\\pelican\\pelican") != NONE;
}

/* the faction a drop of this kind's squad is of: the wave's, unless the wave
has more than one kind, when it's those of the kind alone (a Flood form
brings no Elites) */
static byte drop_kind_faction(byte wave_faction, int kind, unsigned kinds)
{
	if ((kinds & (kinds - 1)) == 0)
		return wave_faction;
	switch (kind)
	{
	case FF_DROP_KIND_FLOOD:
		return _firefight_faction_flood;
	case FF_DROP_KIND_HUMAN:
		return _firefight_faction_unsc_humans;
	default:
		switch (wave_faction)
		{
		case _firefight_faction_mixed:          return _firefight_faction_covenant_sentinels;
		case _firefight_faction_covenant_flood: return _firefight_faction_covenant;
		case _firefight_faction_flood_sentinels: return _firefight_faction_sentinels;
		default:                                return wave_faction;
		}
	}
}

/* the faction of a wave's drop at its zone 0, or -1 for none there: of those
the wave has, one the drop point there spawns (select_wave_spawn_zones's
g_zone0_drop_kinds), or any at a spot that's no drop point, picked */
static int wave_drop_kind_choose(unsigned kinds)
{
	int choices[FF_DROP_KIND_COUNT];
	int choice_count = 0;
	int kind;

	if (g_zone0_drop_kinds != 0)
		kinds &= g_zone0_drop_kinds;
	for (kind = FF_DROP_KIND_COVENANT; kind < FF_DROP_KIND_COUNT; kind++)
	{
		if ((kinds & (1U << kind)) != 0 && drop_kind_carrier_loaded(kind))
			choices[choice_count++] = kind;
	}
	if (choice_count == 0)
	{
		/* (a drop point of none of the wave's factions brings none) */
		if (g_zone0_drop_kinds != 0)
			return -1;
		/* (a Flood wave with no Flood form here comes by ship, as before) */
		return drop_kind_carrier_loaded(FF_DROP_KIND_COVENANT) ? FF_DROP_KIND_COVENANT : -1;
	}
	return choices[spawner_rand() % (unsigned long)choice_count];
}

/* a Flood drop: Keyes' Flood form grows out of the floor at the zone and
bursts (firefight_flood_drop_update, on every machine from the state's
flood_drop_time); firefight_update_dropships_and_vehicles lets the squad out
at the burst. No ship, and no sky needed */
static boolean start_flood_drop(
	real_point3d const *landing_zone,
	real facing,
	byte wave_faction,
	boolean is_boss_wave,
	short squad_to_spawn,
	short forced_team)
{
	real_point3d ground = *landing_zone;

	project_point_to_ground(&ground, 0.0f);
	ff_state.active_dropship.vehicle_index = NONE;
	ff_state.active_dropship.drop_zone = ground;
	ff_state.active_dropship.hover_position = ground;
	ff_state.active_dropship.approach_angle = facing + _pi;
	ff_state.active_dropship.phase = DROPSHIP_PHASE_DESCENDING;
	ff_state.active_dropship.elapsed_ticks = 0;
	ff_state.active_dropship.timer_ticks = FF_FLOOD_DROP_TOTAL_TICKS;
	ff_state.active_dropship.squad_to_spawn = squad_to_spawn;
	ff_state.active_dropship.forced_team = forced_team;
	ff_state.active_dropship.wave_faction = wave_faction;
	ff_state.active_dropship.is_boss_wave = is_boss_wave;
	ff_state.active_dropship.is_pelican = FALSE;
	ff_state.active_dropship.carrier = _ff_drop_carrier_flood;
	ff_state.active_dropship.payload_deployed = FALSE;
	ff_state.active_dropship.scale = 1.0f;
	ff_state.active_dropship.cloak = 0.0f;

	ff_state.flood_drop_position = ground;
	ff_state.flood_drop_facing = facing;
	ff_state.flood_drop_time = MAX(1, game_time_get());

	firefight_announce_banner(_ff_banner_dropship_inbound, _ff_drop_carrier_flood, 3 * TICKS_PER_SECOND);
	return TRUE;
}

static boolean start_dropship_insertion(
	byte carrier,
	real_point3d const *landing_zone,
	real facing,
	byte wave_faction,
	boolean is_boss_wave,
	short squad_to_spawn,
	short forced_team)
{
	static char const *const spirit_tags[] = { "vehicles\\c_dropship\\c_dropship", "vehicles\\pelican\\pelican" };
	static char const *const pelican_tags[] = { "vehicles\\pelican\\pelican", "vehicles\\c_dropship\\c_dropship" };
	long dropship_tag = NONE;
	long dropship_index = NONE;
	struct collision_result coll;
	real_point3d ray_start;
	real_vector3d ray_up = { 0.0f, 0.0f, 10.5f };
	boolean has_sky_clearance = TRUE;
	real approach_angle;
	struct ff_route_pose first_pose;
	boolean has_route;

	boolean is_pelican = carrier == _ff_drop_carrier_pelican;

	if (!landing_zone)
		return FALSE;

	/* Delete any previous dropship before starting a new insertion */
	if (ff_state.active_dropship.vehicle_index != NONE)
	{
		object_delete(ff_state.active_dropship.vehicle_index);
		ff_state.active_dropship.vehicle_index = NONE;
	}
	ff_state.active_dropship.phase = DROPSHIP_PHASE_NONE;
	ff_state.active_dropship.squad_to_spawn = 0;
	ff_state.active_dropship.payload_deployed = TRUE;

	if (carrier == _ff_drop_carrier_flood)
		return start_flood_drop(landing_zone, facing, wave_faction, is_boss_wave, squad_to_spawn, forced_team);

	/* Trigger Covenant Teleport / Orbital Insertion Beacon flash + sound at the zone */
	spawn_insertion_beacon_effect(landing_zone, NONE, wave_faction, is_boss_wave);

	/* Require >= 10.0f world units of vertical ceiling clearance above the drop zone via upward raycast
	 * (a placed route was laid out around whatever is overhead, so it is taken as flyable) */
	has_route = dropship_flight_prepare(&g_dropship_flight, landing_zone, facing);
	ray_start = *landing_zone;
	ray_start.z += 0.5f;
	if (!has_route && collision_test_vector(FLAG(_collision_test_structure_bit), &ray_start, &ray_up, NONE, &coll))
	{
		if (coll.t * 10.5f < 10.0f)
			has_sky_clearance = FALSE;
	}

	if (!has_sky_clearance)
		return FALSE;

	dropship_tag = is_pelican ?
		find_loaded_vehicle_tag(pelican_tags, 2) :
		find_loaded_vehicle_tag(spirit_tags, 2);
	if (dropship_tag == NONE)
		return FALSE;

	/* Approach angle from drop_zone out to start_pos; the dropship faces inward toward drop_zone */
	approach_angle = facing + _pi;

	dropship_index = dropship_new(
		dropship_tag, &g_dropship_flight, forced_team != NONE ? forced_team : firefight_faction_team(wave_faction));
	if (dropship_index == NONE)
		return FALSE;

	ff_state.active_dropship.vehicle_index = dropship_index;
	ff_state.active_dropship.drop_zone = *landing_zone;
	ff_state.active_dropship.hover_position.x = landing_zone->x;
	ff_state.active_dropship.hover_position.y = landing_zone->y;
	ff_state.active_dropship.hover_position.z = landing_zone->z + FF_ROUTE_HOVER_HEIGHT;
	ff_state.active_dropship.approach_angle = approach_angle;
	ff_state.active_dropship.phase = DROPSHIP_PHASE_DESCENDING;
	ff_state.active_dropship.elapsed_ticks = 0;
	ff_state.active_dropship.timer_ticks = (short)ff_route_flight_total_ticks(&g_dropship_flight);
	ff_state.active_dropship.squad_to_spawn = squad_to_spawn;
	ff_state.active_dropship.forced_team = forced_team;
	ff_state.active_dropship.wave_faction = wave_faction;
	ff_state.active_dropship.is_boss_wave = is_boss_wave;
	ff_state.active_dropship.is_pelican = is_pelican;
	ff_state.active_dropship.carrier = is_pelican ? _ff_drop_carrier_pelican : _ff_drop_carrier_spirit;
	ff_state.active_dropship.payload_deployed = FALSE;

	first_pose = ff_route_flight_pose(&g_dropship_flight, 0.0f);
	dropship_fly(dropship_index, &first_pose);
	dropship_flash(&g_dropship_flight, -1.0f, 0.0f, &first_pose, wave_faction, is_boss_wave);

	firefight_announce_banner(_ff_banner_dropship_inbound, ff_state.active_dropship.carrier, 3 * TICKS_PER_SECOND);
	return TRUE;
}

static long spawn_boss_vehicle_at(
	real_point3d const *zone,
	real facing,
	long driver_variant,
	short enemy_team,
	boolean under_dropship)
{
	static char const *const dropship_vehicle_tags[] = {
		"vehicles\\wraith\\wraith",
		"vehicles\\ghost\\ghost_mp",
		"vehicles\\ghost\\ghost"
	};
	static char const *const boss_vehicle_tags[] = {
		"vehicles\\wraith\\wraith",
		"vehicles\\banshee\\banshee",
		"vehicles\\banshee\\banshee_mp",
		"vehicles\\ghost\\ghost_mp",
		"vehicles\\ghost\\ghost",
		"vehicles\\scorpion\\scorpion_mp",
		"vehicles\\scorpion\\scorpion"
	};
	long candidates[8];
	short candidate_count = 0;
	long vehicle_tag;
	char const *v_name;
	boolean is_banshee = FALSE;
	boolean is_hover = FALSE;
	struct object_placement_data placement;
	long vehicle_index;
	long chosen_driver_variant;
	long driver_unit;
	short i;

	if (under_dropship)
	{
		for (i = 0; i < (short)NUMBEROF(dropship_vehicle_tags); i++)
		{
			long idx = tag_loaded(VEHICLE_DEFINITION_TAG, dropship_vehicle_tags[i]);
			if (idx != NONE && !enemy_vehicle_tag_superseded(dropship_vehicle_tags[i]))
				candidates[candidate_count++] = idx;
		}
	}
	if (candidate_count == 0)
	{
		for (i = 0; i < (short)NUMBEROF(boss_vehicle_tags); i++)
		{
			long idx = tag_loaded(VEHICLE_DEFINITION_TAG, boss_vehicle_tags[i]);
			if (idx != NONE && !enemy_vehicle_tag_superseded(boss_vehicle_tags[i]))
				candidates[candidate_count++] = idx;
		}
	}
	/* (MAP CONTENT: the map's own vehicles beside the game's, or alone) */
	if (map_content_of(FIREFIGHT_MAP_CONTENT_VEHICLES_OFF) != _firefight_map_content_vanilla && ff_map_vehicle_count > 0)
	{
		boolean only = map_vehicles_only();
		long map_vehicle = pick_map_vehicle(!under_dropship, !only);

		if (only)
			candidate_count = 0;
		if (map_vehicle != NONE && candidate_count < (short)NUMBEROF(candidates))
			candidates[candidate_count++] = map_vehicle;
	}
	if (candidate_count == 0)
		return NONE;

	vehicle_tag = candidates[spawner_rand() % (unsigned long)candidate_count];
	v_name = tag_get_name(vehicle_tag);
	if (v_name)
	{
		is_banshee = vehicle_tag_flies(vehicle_tag);
		is_hover = !is_banshee && vehicle_tag_hovers(vehicle_tag);
	}

	patch_vehicle_for_ai_driver(vehicle_tag, is_banshee, is_hover);

	object_placement_data_new(&placement, vehicle_tag, NONE);
	placement.position = *zone;
	if (is_banshee)
	{
		/* Spawn Banshees 8.0f-12.0f units in the air so they immediately fly in and attack */
		struct collision_result coll;
		real_point3d ray_start = *zone;
		real_vector3d ray_up = { 0.0f, 0.0f, 13.0f };
		real air_z = spawner_rand_range(8.5f, 11.5f);

		ray_start.z += 0.5f;
		if (collision_test_vector(FLAG(_collision_test_structure_bit), &ray_start, &ray_up, NONE, &coll))
		{
			real max_z = coll.t * 13.0f - 1.5f;
			if (max_z < air_z)
				air_z = MAX(2.5f, max_z);
		}
		placement.position.z += air_z;
	}
	else
	{
		project_point_to_ground(&placement.position, 0.65f);
	}
	vector3d_from_angle(&placement.forward, facing);
	placement.owner_team_index = enemy_team;

	/* (Firefight's own, whatever the game type's vehicle set, which only placements follow) */
	game_engine_vehicle_always_spawn = TRUE;
	vehicle_index = object_new(&placement);
	game_engine_vehicle_always_spawn = FALSE;
	if (vehicle_index == NONE)
		return NONE;

	enemy_vehicle_note(vehicle_index);
	object_set_automatic_deactivation(vehicle_index, FALSE);
	object_activate(vehicle_index);

	chosen_driver_variant = pick_vehicle_driver_variant(
		driver_variant,
		global_variant.game_engine_variant.firefight.wave_faction);
	driver_unit = spawn_single_actor_at(
		chosen_driver_variant,
		_ff_tier_boss_heavy,
		&placement.position,
		facing,
		enemy_team,
		TRUE);
	if (driver_unit != NONE)
	{
		struct unit_datum *v_unit = unit_get(vehicle_index);
		struct unit_datum *d_unit = unit_try_and_get(driver_unit);
		struct unit_definition *v_def = v_unit ? unit_definition_get(v_unit->definition_index) : NULL;

		if (v_unit && d_unit && v_def && v_def->unit.seats.count > 0)
		{
			struct unit_seat *seat0 = TAG_BLOCK_GET_ELEMENT(&v_def->unit.seats, 0, struct unit_seat);

			SET_FLAG(v_unit->unit.flags, _unit_actively_controlled_bit, FALSE);
			v_unit->object.owner_team_index = enemy_team;

			/* Ensure the driver biped animation graph matches seat 0's label so unit_enter_seat succeeds */
			if (seat0)
			{
				vehicle_seat_fit_driver(driver_unit, vehicle_index, seat0);
				vehicle_seat_driver(driver_unit, vehicle_index, seat0);
			}
		}

		if (d_unit && d_unit->unit.actor_index != NONE)
		{
			struct actor_datum *d_actor = actor_get(d_unit->unit.actor_index);
			long player_units[16];
			short player_count = get_living_player_positions(NULL, player_units, 16);

			if (d_actor && d_unit->object.parent_object_index == vehicle_index)
			{
				d_actor->input.vehicle_index = vehicle_index;
				d_actor->input.vehicle_passenger = FALSE;
				d_actor->input.vehicle_gunner = TRUE;
				if (is_banshee)
				{
					d_actor->state.flying = TRUE;
					d_actor->input.vehicle_driver_type = _actor_vehicle_driver_directional_flying;
					vehicle_hover(vehicle_index, FALSE);
				}
				else
				{
					d_actor->input.vehicle_driver_type = is_hover ?
						_actor_vehicle_driver_hovering_ground :
						_actor_vehicle_driver_nondirectional_ground;
				}
			}
			if (player_count > 0)
				direct_actor_toward_unit(d_unit->unit.actor_index, player_units[0]);
		}
	}

	return vehicle_index;
}

static long spawn_specific_enemy_vehicle_at(
	boolean want_wraith,
	real_point3d const *zone,
	real facing,
	short enemy_team,
	boolean exact_pad)
{
	static char const *const ghost_tags[] = {
		"vehicles\\ghost\\ghost",
		"vehicles\\ghost\\ghost_mp"
	};
	static char const *const wraith_tags[] = {
		"vehicles\\wraith\\wraith",
		"vehicles\\scorpion\\scorpion_mp",
		"vehicles\\scorpion\\scorpion",
		"vehicles\\banshee\\banshee",
		"vehicles\\banshee\\banshee_mp"
	};
	long vehicle_tag = want_wraith ?
		find_loaded_vehicle_tag(wraith_tags, (short)NUMBEROF(wraith_tags)) :
		find_loaded_vehicle_tag(ghost_tags, (short)NUMBEROF(ghost_tags));
	byte driver_tier = _ff_tier_commander;
	/* (MAP CONTENT: the map's own in its place, ONLY MAP's always, INCLUDE
	MAP's half the time) */
	long map_vehicle = map_vehicles_only() ? pick_map_vehicle(want_wraith, FALSE) :
		map_content_of(FIREFIGHT_MAP_CONTENT_VEHICLES_OFF) == _firefight_map_content_include_map && (spawner_rand() & 1) ?
		pick_map_vehicle(want_wraith, TRUE) : NONE;
	long driver_variant = pick_variant_for_subtype(
		_ff_sub_elite_major,
		global_variant.game_engine_variant.firefight.wave_faction,
		&driver_tier,
		NULL,
		NULL);

	if (map_vehicle != NONE)
		vehicle_tag = map_vehicle;
	if (vehicle_tag == NONE)
	{
		return spawn_boss_vehicle_at(zone, facing, driver_variant, enemy_team, !want_wraith);
	}

	{
		boolean is_banshee = vehicle_tag_flies(vehicle_tag);
		boolean is_hover = !is_banshee && vehicle_tag_hovers(vehicle_tag);
		struct object_placement_data placement;
		long vehicle_index;
		long chosen_driver_variant;
		long driver_unit;

		patch_vehicle_for_ai_driver(vehicle_tag, is_banshee, is_hover);
		object_placement_data_new(&placement, vehicle_tag, NONE);
		placement.position = *zone;
		if (!exact_pad)
		{
			placement.position.x += spawner_rand_range(-2.0f, 2.0f);
			placement.position.y += spawner_rand_range(-2.0f, 2.0f);
			if (!is_banshee && !spot_reachable_from(zone, &placement.position))
				placement.position = *zone;
		}
		if (is_banshee)
			placement.position.z += 9.0f;
		else
			project_point_to_ground(&placement.position, 0.65f);
		vector3d_from_angle(&placement.forward, facing);
		placement.owner_team_index = enemy_team;

		/* (Firefight's own, whatever the game type's vehicle set, which only placements follow) */
		game_engine_vehicle_always_spawn = TRUE;
		vehicle_index = object_new(&placement);
		game_engine_vehicle_always_spawn = FALSE;
		if (vehicle_index == NONE)
			return NONE;

		enemy_vehicle_note(vehicle_index);
		object_set_automatic_deactivation(vehicle_index, FALSE);
		object_activate(vehicle_index);

		chosen_driver_variant = pick_vehicle_driver_variant(
			driver_variant,
			global_variant.game_engine_variant.firefight.wave_faction);
		driver_unit = spawn_single_actor_at(
			chosen_driver_variant,
			_ff_tier_boss_heavy,
			&placement.position,
			facing,
			enemy_team,
			TRUE);
		if (driver_unit != NONE)
		{
			struct unit_datum *v_unit = unit_get(vehicle_index);
			struct unit_datum *d_unit = unit_try_and_get(driver_unit);
			struct unit_definition *v_def = v_unit ? unit_definition_get(v_unit->definition_index) : NULL;

			if (v_unit && d_unit && v_def && v_def->unit.seats.count > 0)
			{
				struct unit_seat *seat0 = TAG_BLOCK_GET_ELEMENT(&v_def->unit.seats, 0, struct unit_seat);

				SET_FLAG(v_unit->unit.flags, _unit_actively_controlled_bit, FALSE);
				v_unit->object.owner_team_index = enemy_team;

				if (seat0)
				{
					vehicle_seat_fit_driver(driver_unit, vehicle_index, seat0);
					vehicle_seat_driver(driver_unit, vehicle_index, seat0);
				}
			}

			if (d_unit && d_unit->unit.actor_index != NONE)
			{
				struct actor_datum *d_actor = actor_get(d_unit->unit.actor_index);
				long player_units[16];
				short player_count = get_living_player_positions(NULL, player_units, 16);

				if (d_actor && d_unit->object.parent_object_index == vehicle_index)
				{
					d_actor->input.vehicle_index = vehicle_index;
					d_actor->input.vehicle_passenger = FALSE;
					d_actor->input.vehicle_gunner = TRUE;
					if (is_banshee)
					{
						d_actor->state.flying = TRUE;
						d_actor->input.vehicle_driver_type = _actor_vehicle_driver_directional_flying;
						vehicle_hover(vehicle_index, FALSE);
					}
					else
					{
						d_actor->input.vehicle_driver_type = is_hover ?
							_actor_vehicle_driver_hovering_ground :
							_actor_vehicle_driver_nondirectional_ground;
					}
				}
				if (player_count > 0)
					direct_actor_toward_unit(d_unit->unit.actor_index, player_units[0]);
			}
		}

		return vehicle_index;
	}
}

/* ---------- Wave Spawning Entry Point */

struct firefight_wave_spec
{
	byte grunts;
	byte jackals;
	byte minor_elites;
	byte major_elites;
	byte invis_elites;
	byte sword_elites;
	byte hunters;
	byte ghosts;
	byte wraiths;
};

/* Exact per-round / per-wave enemy composition across 5 Rounds x 3 Waves per Set (scaled to 60% of prior counts):
 * - Round 1 Total: 12 Grunts, 9 Jackals, 6 Minor Elites, 2 Major Elites
 * - Round 2 Total: 18 Grunts, 12 Jackals, 6 Minor Elites, 4 Major Elites, 1 Sword Elite
 * - Round 3 Total: 18 Grunts, 15 Jackals, 6 Minor Elites, 5 Major Elites, 3 Invis Elites, 1 Sword Elite
 * - Round 4 Total: 15 Grunts, 9 Jackals, 6 Minor Elites, 4 Major Elites, 2 Invis Elites, 1 Sword Elite, 3 Hunters, 3 Ghosts
 * - Round 5 Total: 15 Grunts, 9 Jackals, 6 Minor Elites, 5 Major Elites, 3 Invis Elites, 1 Sword Elite, 3 Hunters, 3 Ghosts, 2 Wraiths
 */
static struct firefight_wave_spec const k_firefight_wave_specs[5][3] =
{
	/* Round 1 */
	{
		{ 4, 3, 2, 0, 0, 0, 0, 0, 0 },
		{ 4, 3, 2, 1, 0, 0, 0, 0, 0 },
		{ 4, 3, 2, 1, 0, 0, 0, 0, 0 }
	},
	/* Round 2 */
	{
		{ 6, 4, 2, 1, 0, 0, 0, 0, 0 },
		{ 6, 4, 2, 1, 0, 0, 0, 0, 0 },
		{ 6, 4, 2, 2, 0, 1, 0, 0, 0 }
	},
	/* Round 3 */
	{
		{ 6, 5, 2, 1, 1, 0, 0, 0, 0 },
		{ 6, 5, 2, 2, 1, 0, 0, 0, 0 },
		{ 6, 5, 2, 2, 1, 1, 0, 0, 0 }
	},
	/* Round 4 */
	{
		{ 5, 3, 2, 1, 0, 0, 1, 1, 0 },
		{ 5, 3, 2, 1, 1, 0, 1, 1, 0 },
		{ 5, 3, 2, 2, 1, 1, 1, 1, 0 }
	},
	/* Round 5 */
	{
		{ 5, 3, 2, 1, 1, 0, 1, 1, 0 },
		{ 5, 3, 2, 2, 1, 0, 1, 1, 1 },
		{ 5, 3, 2, 2, 1, 1, 1, 1, 1 }
	}
};

static short count_active_session_players(void)
{
	struct data_iterator iter;
	struct player_datum *player;
	short total = 0;

	if (!player_data)
		return 1;
	data_iterator_new(&iter, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iter)) != NULL)
	{
		if (!player->quit_out_of_game)
			total++;
	}
	return MAX(total, (short)1);
}

static short scale_enemy_count(byte base_count, real combined_mult)
{
	short scaled;

	if (base_count == 0)
		return 0;
	scaled = (short)((real)base_count * combined_mult + 0.5f);
	return MAX(scaled, (short)1);
}

static void append_wave_subtypes(
	byte *out_list,
	short *io_count,
	short max_cap,
	byte subtype,
	short count)
{
	short i;

	for (i = 0; i < count && *io_count < max_cap; i++)
	{
		out_list[(*io_count)++] = subtype;
	}
}

boolean firefight_wave_will_have_heavy_or_boss(
	short set_num,
	short round_num,
	short wave_num)
{
	struct firefight_options const *opts = firefight_options_get();
	byte boss_mode = opts ? opts->firefight_boss_wave_mode : _firefight_boss_wave_5_only;

	(void)set_num;
	if (boss_mode == _firefight_boss_rush)
		return TRUE;
	if (boss_mode == _firefight_boss_every_3rd)
		return (wave_num == FIREFIGHT_WAVES_PER_ROUND);
	return (round_num >= 4 && wave_num == FIREFIGHT_WAVES_PER_ROUND);
}

short firefight_spawn_wave(
	short set_num,
	short round_num,
	short wave_num,
	boolean is_bonus)
{
	struct game_engine_firefight_variant const *ff = &global_variant.game_engine_variant.firefight;
	struct firefight_options const *opts = firefight_options_get();
	real facings[FIREFIGHT_MAX_SPAWN_ZONES_PER_WAVE];
	short zone_count;
	unsigned drop_kinds;
	byte wave_subtypes[256];
	short total_to_spawn = 0;
	short immediate_to_spawn;
	byte wave_faction = firefight_wave_faction(ff->wave_faction, is_bonus);
	short forced_team = NONE;
	short spawned_count = 0;
	boolean is_boss_wave = firefight_wave_will_have_heavy_or_boss(set_num, round_num, wave_num);
	short r_idx = (short)PIN(round_num - 1, 0, 4);
	short w_idx = (short)PIN(wave_num - 1, 0, 2);
	struct firefight_wave_spec const *spec = &k_firefight_wave_specs[r_idx][w_idx];
	short active_players = count_active_session_players();
	short extra_players = (short)MAX(active_players - 1, 0);
	short scale_pct = firefight_server_player_scale_percent_get();
	real player_mult = 1.0f + (real)extra_players * ((real)scale_pct / 100.0f);
	real wave_size_mult = 1.0f;
	real combined_mult;
	short i;

	firefight_prune_dead_tracked_enemies();

	if (opts)
	{
		switch (opts->firefight_wave_size_scale)
		{
		case _firefight_wave_size_50:  wave_size_mult = 0.5f; break;
		case _firefight_wave_size_150: wave_size_mult = 1.5f; break;
		case _firefight_wave_size_200: wave_size_mult = 2.0f; break;
		case _firefight_wave_size_250: wave_size_mult = 2.5f; break;
		case _firefight_wave_size_300: wave_size_mult = 3.0f; break;
		default:                       wave_size_mult = 1.0f; break;
		}
	}
	combined_mult = player_mult * wave_size_mult;

	if (is_bonus)
	{
		append_wave_subtypes(
			wave_subtypes,
			&total_to_spawn,
			(short)NUMBEROF(wave_subtypes),
			_ff_sub_grunt,
			scale_enemy_count(10, combined_mult));
	}
	else
	{
		append_wave_subtypes(wave_subtypes, &total_to_spawn, (short)NUMBEROF(wave_subtypes),
			_ff_sub_grunt, scale_enemy_count(spec->grunts, combined_mult));
		append_wave_subtypes(wave_subtypes, &total_to_spawn, (short)NUMBEROF(wave_subtypes),
			_ff_sub_jackal, scale_enemy_count(spec->jackals, combined_mult));
		append_wave_subtypes(wave_subtypes, &total_to_spawn, (short)NUMBEROF(wave_subtypes),
			_ff_sub_elite_minor, scale_enemy_count(spec->minor_elites, combined_mult));
		append_wave_subtypes(wave_subtypes, &total_to_spawn, (short)NUMBEROF(wave_subtypes),
			_ff_sub_elite_major, scale_enemy_count(spec->major_elites, combined_mult));
		append_wave_subtypes(wave_subtypes, &total_to_spawn, (short)NUMBEROF(wave_subtypes),
			_ff_sub_elite_stealth, scale_enemy_count(spec->invis_elites, combined_mult));
		append_wave_subtypes(wave_subtypes, &total_to_spawn, (short)NUMBEROF(wave_subtypes),
			_ff_sub_elite_zealot, scale_enemy_count(spec->sword_elites, combined_mult));
		append_wave_subtypes(wave_subtypes, &total_to_spawn, (short)NUMBEROF(wave_subtypes),
			_ff_sub_hunter, scale_enemy_count(spec->hunters, combined_mult));
	}

	{
		short available_slots = (short)MAX(0, FIREFIGHT_SAFE_ACTIVE_ACTOR_CAP - ff_state.enemies_alive);
		immediate_to_spawn = MIN(total_to_spawn, available_slots);
		g_queued_reinforcement_enemies = (short)MAX(0, total_to_spawn - immediate_to_spawn);
		for (i = 0; i < g_queued_reinforcement_enemies; i++)
		{
			g_queued_reinforcement_subtypes[i] = wave_subtypes[immediate_to_spawn + i];
		}
		ff_state.enemies_queued = g_queued_reinforcement_enemies;
	}

	drop_kinds = wave_drop_kinds(wave_faction);
	g_drop_kinds_wanted = drop_kinds;
	zone_count = select_wave_spawn_zones(
		ff_state.wave_spawn_zones,
		facings,
		(wave_num == 1 && !is_bonus) ? 2 : FIREFIGHT_MAX_SPAWN_ZONES_PER_WAVE,
		round_num,
		wave_num,
		is_bonus,
		is_boss_wave);
	g_drop_kinds_wanted = 0;
	ff_state.wave_spawn_zone_count = zone_count;
	for (i = 0; i < zone_count && i < FIREFIGHT_MAX_SPAWN_ZONES_PER_WAVE; i++)
		g_active_wave_facings[i] = facings[i];
	if (zone_count == 0)
		return 0;

	if (!opts || opts->firefight_faction_diplomacy == _firefight_diplomacy_allied_against_players)
	{
		forced_team = (wave_faction == _firefight_faction_unsc_humans) ?
			_game_team_human : _game_team_covenant;
	}

	/* Teleport / Orbital Insertion Beacon effects & Real 3D Flying Dropship on wave spawn zones */
	{
		boolean dropship_spawned = FALSE;
		short dropship_squad_count = 0;

		g_dropship_squad_subtype_count = 0;
		if (!is_bonus && (!opts || opts->firefight_dropship_mode != _firefight_dropship_off))
		{
			byte d_mode = opts ? opts->firefight_dropship_mode : _firefight_dropship_auto;
			int drop_kind = wave_drop_kind_choose(drop_kinds);

			if (drop_kind >= 0 &&
				(d_mode == _firefight_dropship_frequent ||
				(d_mode == _firefight_dropship_boss_only && is_boss_wave) ||
				(d_mode == _firefight_dropship_auto)))
			{
				short z_idx;
				short zone0_share = (short)((immediate_to_spawn + zone_count - 1) / zone_count);
				dropship_squad_count = PIN(zone0_share, 3, 8);

				dropship_spawned = start_dropship_insertion(
					drop_kind == FF_DROP_KIND_FLOOD ? _ff_drop_carrier_flood :
						drop_kind == FF_DROP_KIND_HUMAN ? _ff_drop_carrier_pelican : _ff_drop_carrier_spirit,
					&ff_state.wave_spawn_zones[0],
					facings[0],
					drop_kind_faction(wave_faction, drop_kind, drop_kinds),
					is_boss_wave,
					dropship_squad_count,
					forced_team);
				if (!dropship_spawned)
					dropship_squad_count = 0;

				for (z_idx = 1; z_idx < zone_count; z_idx++)
				{
					spawn_insertion_beacon_effect(
						&ff_state.wave_spawn_zones[z_idx],
						NONE,
						wave_faction,
						is_boss_wave);
				}
			}
		}

		for (i = 0; i < immediate_to_spawn; i++)
		{
			short z = (short)(i % zone_count);
			byte subtype = wave_subtypes[i];
			boolean is_heavy_unit = (subtype == _ff_sub_hunter || subtype == _ff_sub_elite_zealot);
			long unit_index;

			/* Hold zone 0's squad onboard the approaching 3D dropship for Phase 2 unload */
			if (dropship_spawned && z == 0 && dropship_squad_count > 0)
			{
				if (g_dropship_squad_subtype_count < (short)NUMBEROF(g_dropship_squad_subtypes))
					g_dropship_squad_subtypes[g_dropship_squad_subtype_count++] = subtype;
				dropship_squad_count--;
				spawned_count++;
				continue;
			}

			unit_index = spawn_subtype_actor_at(
				subtype,
				wave_faction,
				&ff_state.wave_spawn_zones[z],
				facings[z],
				forced_team,
				TRUE);
			if (unit_index != NONE)
			{
				if (i < zone_count && (!opts || opts->firefight_dropship_mode != _firefight_dropship_off))
				{
					real_point3d unit_pos;
					object_get_origin(unit_index, &unit_pos);
					spawn_insertion_beacon_effect(&unit_pos, unit_index, wave_faction, is_heavy_unit);
				}
				spawned_count++;
			}
		}

		/* Spawn Round-Scheduled or Marker-Scheduled Enemy Vehicles:
		 * - Default stock maps without custom Firefight Spawn markers keep normal Round 4/5 vehicle spawns.
		 * - Custom maps OR any map with placed Firefight Spawn markers only spawn enemy vehicles at approved
		 *   vehicle spawn points (AI + VEHICLES, ENEMY VEHICLE, ENEMY GHOST, ENEMY HEAVY VEH) matching the round/wave! */
		if (!is_bonus && zone_count > 0)
		{
			short enemy_team = forced_team != NONE ? forced_team : _game_team_covenant;
			boolean require_approved_veh_markers = (map_has_any_custom_ff_spawns() || !is_default_stock_map());
			short v;

			if (require_approved_veh_markers)
			{
				real_point3d veh_pts[32];
				real veh_facings[32];
				int veh_types[32];
				unsigned long ghost_mask =
					(1UL << FF_SPAWN_TYPE_AI_AND_VEHICLES) |
					(1UL << FF_SPAWN_TYPE_ENEMY_VEHICLE) |
					(1UL << FF_SPAWN_TYPE_ENEMY_GHOST);
				unsigned long wraith_mask =
					(1UL << FF_SPAWN_TYPE_AI_AND_VEHICLES) |
					(1UL << FF_SPAWN_TYPE_ENEMY_VEHICLE) |
					(1UL << FF_SPAWN_TYPE_ENEMY_HEAVY_VEH);
				short ghost_pad_count = collect_matching_custom_ff_spawns(
					ghost_mask, round_num, wave_num, is_bonus, is_boss_wave, FALSE,
					veh_pts, veh_facings, veh_types, 32);
				short ghosts_to_spawn = spec->ghosts;

				if (ghost_pad_count > 0)
				{
					if (ghosts_to_spawn == 0)
					{
						short dedicated_ghosts = 0;
						for (v = 0; v < ghost_pad_count; v++)
						{
							if (veh_types[v] == FF_SPAWN_TYPE_ENEMY_GHOST ||
								veh_types[v] == FF_SPAWN_TYPE_ENEMY_VEHICLE)
							{
								dedicated_ghosts++;
							}
						}
						ghosts_to_spawn = MIN(dedicated_ghosts, (short)2);
					}
					for (v = 0; v < ghosts_to_spawn; v++)
					{
						short p_idx = (short)((v + (short)spawner_rand()) % ghost_pad_count);
						if (spawn_specific_enemy_vehicle_at(
							FALSE,
							&veh_pts[p_idx],
							veh_facings[p_idx],
							enemy_team,
							v == 0 || ghost_pad_count > 1) != NONE)
						{
							spawned_count++;
						}
					}
				}

				{
					short wraith_pad_count = collect_matching_custom_ff_spawns(
						wraith_mask, round_num, wave_num, is_bonus, is_boss_wave, FALSE,
						veh_pts, veh_facings, veh_types, 32);
					short wraiths_to_spawn = spec->wraiths;

					if (wraith_pad_count > 0)
					{
						if (wraiths_to_spawn == 0)
						{
							short dedicated_heavy = 0;
							for (v = 0; v < wraith_pad_count; v++)
							{
								if (veh_types[v] == FF_SPAWN_TYPE_ENEMY_HEAVY_VEH)
									dedicated_heavy++;
							}
							wraiths_to_spawn = MIN(dedicated_heavy, (short)1);
						}
						for (v = 0; v < wraiths_to_spawn; v++)
						{
							short p_idx = (short)((v + (short)spawner_rand()) % wraith_pad_count);
							if (spawn_specific_enemy_vehicle_at(
								TRUE,
								&veh_pts[p_idx],
								veh_facings[p_idx],
								enemy_team,
								v == 0 || wraith_pad_count > 1) != NONE)
							{
								spawned_count++;
							}
						}
					}
				}
			}
			else
			{
				for (v = 0; v < spec->ghosts; v++)
				{
					short z = (short)((zone_count - 1 - (v % zone_count) + zone_count) % zone_count);
					if (spawn_specific_enemy_vehicle_at(
						FALSE,
						&ff_state.wave_spawn_zones[z],
						facings[z],
						enemy_team,
						FALSE) != NONE)
					{
						spawned_count++;
					}
				}
				for (v = 0; v < spec->wraiths; v++)
				{
					short z = (short)(v % zone_count);
					if (spawn_specific_enemy_vehicle_at(
						TRUE,
						&ff_state.wave_spawn_zones[z],
						facings[z],
						enemy_team,
						FALSE) != NONE)
					{
						spawned_count++;
					}
				}
			}
		}
	}

	return (short)(spawned_count + g_queued_reinforcement_enemies);
}

/* ---------- Safe Player Ammo & Heavy Ordnance Drops (Feature 5) */

static void snap_resupply_point_to_ground(real_point3d *pt)
{
	struct collision_result coll;
	real_point3d ray_start;
	real_vector3d ray_down = { 0.0f, 0.0f, -6.0f };

	if (!pt)
		return;
	ray_start = *pt;
	ray_start.z += 1.2f;
	if (collision_test_vector(FLAG(_collision_test_structure_bit), &ray_start, &ray_down, NONE, &coll))
	{
		pt->x = coll.point.x;
		pt->y = coll.point.y;
		pt->z = coll.point.z + 0.25f;
	}
	else
	{
		pt->z += 0.30f;
	}
}

static boolean point_is_safe_from_enemy_zones(real_point3d const *pt, real min_distance)
{
	short z;

	if (!pt)
		return FALSE;
	for (z = 0; z < ff_state.wave_spawn_zone_count && z < FIREFIGHT_MAX_SPAWN_ZONES_PER_WAVE; z++)
	{
		if (distance3d(pt, &ff_state.wave_spawn_zones[z]) < min_distance)
			return FALSE;
	}
	return TRUE;
}

static void calculate_safe_player_resupply_point(
	real_point3d const *player_pos,
	long player_unit_index,
	real_point3d *out_drop_center)
{
	static real const candidate_distances[] = { 6.0f, 7.5f, 5.0f, 9.0f };
	static real const candidate_angle_offsets[] = {
		0.0f, 0.5235988f, -0.5235988f, 1.0471976f, -1.0471976f,
		1.5707963f, -1.5707963f, 3.1415927f
	};
	struct unit_datum *u = player_unit_index != NONE ?
		(struct unit_datum *)object_try_and_get_and_verify_type(player_unit_index, _object_mask_unit) : NULL;
	real base_angle = 0.0f;
	short d_idx, a_idx;

	if (!player_pos || !out_drop_center)
		return;

	if (u && (u->object.forward.i != 0.0f || u->object.forward.j != 0.0f))
		base_angle = (real)atan2(u->object.forward.j, u->object.forward.i);

	/* Pass 1: 5.0 to 9.0 world units in front/around player, inside BSP, and >= 20u from enemy spawn zones */
	for (d_idx = 0; d_idx < (short)NUMBEROF(candidate_distances); d_idx++)
	{
		real dist = candidate_distances[d_idx];
		for (a_idx = 0; a_idx < (short)NUMBEROF(candidate_angle_offsets); a_idx++)
		{
			real ang = base_angle + candidate_angle_offsets[a_idx];
			real_point3d candidate = *player_pos;

			candidate.x += (real)cos(ang) * dist;
			candidate.y += (real)sin(ang) * dist;
			snap_resupply_point_to_ground(&candidate);
			if (point_is_in_valid_bsp(&candidate) && point_is_safe_from_enemy_zones(&candidate, 20.0f))
			{
				*out_drop_center = candidate;
				return;
			}
		}
	}

	/* Pass 2: 5.0 to 9.0 world units in front/around player, inside BSP */
	for (d_idx = 0; d_idx < (short)NUMBEROF(candidate_distances); d_idx++)
	{
		real dist = candidate_distances[d_idx];
		for (a_idx = 0; a_idx < (short)NUMBEROF(candidate_angle_offsets); a_idx++)
		{
			real ang = base_angle + candidate_angle_offsets[a_idx];
			real_point3d candidate = *player_pos;

			candidate.x += (real)cos(ang) * dist;
			candidate.y += (real)sin(ang) * dist;
			snap_resupply_point_to_ground(&candidate);
			if (point_is_in_valid_bsp(&candidate))
			{
				*out_drop_center = candidate;
				return;
			}
		}
	}

	*out_drop_center = *player_pos;
	out_drop_center->z += 0.35f;
}

static long spawn_protected_resupply_item(long tag_index, real_point3d const *center, real offset_x, real offset_y)
{
	struct object_placement_data placement;
	long obj_index;

	if (tag_index == NONE || !center)
		return NONE;

	object_placement_data_new(&placement, tag_index, NONE);
	placement.position = *center;
	placement.position.x += offset_x;
	placement.position.y += offset_y;
	snap_resupply_point_to_ground(&placement.position);
	if (!point_is_in_valid_bsp(&placement.position))
		placement.position = *center;

	obj_index = object_new(&placement);
	if (obj_index != NONE)
	{
		struct object_datum *obj = object_get(obj_index);
		if (obj)
			SET_FLAG(obj->object.flags, _object_cannot_be_garbage_bit, TRUE);
	}
	return obj_index;
}

static long find_loaded_tag_by_substring(unsigned long group_tag, char const *exact_path, char const *sub1, char const *sub2)
{
	struct tag_iterator iter;
	long idx;

	if (exact_path)
	{
		idx = tag_loaded(group_tag, exact_path);
		if (idx != NONE)
			return idx;
	}

	tag_iterator_new(&iter, group_tag);
	while ((idx = tag_iterator_next(&iter)) != NONE)
	{
		char const *raw_name = tag_get_name(idx);
		char lower[256];

		if (!raw_name)
			continue;
		lowercase_copy(raw_name, lower, sizeof(lower));
		if (sub1 && !strstr(lower, sub1))
			continue;
		if (sub2 && !strstr(lower, sub2))
			continue;
		return idx;
	}
	return NONE;
}

static void pod_contents_spawn(void);
static boolean ordnance_pod_spot(real_point3d const *anchor, long anchor_unit, boolean at_area,
	real_point3d const *others, short other_count, real_point3d *out_position);
static void pods_shape_choose(void);
static boolean drop_area_nearest(real_point3d const *point, real_point3d *area);

void firefight_ordnance_preview_refusals(char *text, long size);

/* ---- What the pods hold: of two pools, the regular weapons and the heavy
(power) ones, each gone through in a shuffled order before any comes again
(a bag each, kept from drop to drop), a pod a heavy one
FF_ORDNANCE_HEAVY_PERCENT of the time (a heavy drop's first pod for each
player always). The game's: the standard and power weapons the level has;
INCLUDE MAP's the map's own beside them, ONLY MAP's the map's alone (each
by its damage: map_weapon_heavy); and the variant's profile's, one for each
player, of a bag of its own. */

#define FF_ORDNANCE_HEAVY_PERCENT 30
#define FF_ORDNANCE_BAG_SIZE 96

enum
{
	_ff_ordnance_bag_regular,
	_ff_ordnance_bag_heavy,
	_ff_ordnance_bag_custom,
	NUMBER_OF_FF_ORDNANCE_BAGS
};

static struct
{
	long tags[FF_ORDNANCE_BAG_SIZE];
	short count;
	short next;
	unsigned long signature;
} g_ordnance_bags[NUMBER_OF_FF_ORDNANCE_BAGS];

static void ordnance_pool_add(long *pool, short *count, long tag)
{
	short index;

	if (tag == NONE || *count >= FF_ORDNANCE_BAG_SIZE || firefight_weapon_is_objective(tag, "an ordnance pool"))
		return;
	for (index = 0; index < *count; index++)
	{
		if (pool[index] == tag)
			return;
	}
	pool[(*count)++] = tag;
}

static void ordnance_bag_shuffle(short bag)
{
	short index;

	for (index = (short)(g_ordnance_bags[bag].count - 1); index > 0; index--)
	{
		short other = (short)(spawner_rand() % (unsigned long)(index + 1));
		long swap = g_ordnance_bags[bag].tags[index];

		g_ordnance_bags[bag].tags[index] = g_ordnance_bags[bag].tags[other];
		g_ordnance_bags[bag].tags[other] = swap;
	}
	g_ordnance_bags[bag].next = 0;
}

/* the next of a bag's weapons (its pool new: shuffled again); NONE for an
empty pool */
static long ordnance_bag_next(short bag, long const *pool, short count)
{
	unsigned long signature = 2166136261UL;
	short index;

	for (index = 0; index < count; index++)
		signature = (signature ^ (unsigned long)pool[index]) * 16777619UL;
	if (signature != g_ordnance_bags[bag].signature || count != g_ordnance_bags[bag].count)
	{
		csmemcpy(g_ordnance_bags[bag].tags, pool, (size_t)count * sizeof(long));
		g_ordnance_bags[bag].count = count;
		g_ordnance_bags[bag].signature = signature;
		ordnance_bag_shuffle(bag);
	}
	if (count <= 0)
		return NONE;
	if (g_ordnance_bags[bag].next >= count)
	{
		long last = g_ordnance_bags[bag].tags[count - 1];

		ordnance_bag_shuffle(bag);
		/* (not the one just given first again) */
		if (count > 1 && g_ordnance_bags[bag].tags[0] == last)
		{
			g_ordnance_bags[bag].tags[0] = g_ordnance_bags[bag].tags[count - 1];
			g_ordnance_bags[bag].tags[count - 1] = last;
		}
	}
	return g_ordnance_bags[bag].tags[g_ordnance_bags[bag].next++];
}

/* (a weapon one of the game's: imported, or the map's own of a stock name) */
static boolean ordnance_tag_stock(long tag_index)
{
	char lowered[256];
	char const *name = tag_get_name(tag_index);

	if (!name)
		return FALSE;
	lowercase_copy(name, lowered, sizeof(lowered));
	return !tag_is_maps_own(tag_index) || weapon_tag_stock(name, lowered);
}

/* the pools of a drop: regular and heavy weapons, as the header says */
static void ordnance_pools_build(long *regular, short *regular_count, long *heavy, short *heavy_count)
{
	byte mode = map_content_of(FIREFIGHT_MAP_CONTENT_WEAPONS_OFF);
	boolean only = map_weapons_only();
	short index;

	*regular_count = 0;
	*heavy_count = 0;
	if (!only)
	{
		/* (the game's own alone: a map's weapon of a like name is MAP
		CONTENT's) */
		for (index = 0; index < ff_standard_weapon_tag_count; index++)
		{
			short power;
			boolean is_power = FALSE;

			for (power = 0; power < ff_power_weapon_tag_count; power++)
				is_power |= ff_power_weapon_tags[power] == ff_standard_weapon_tags[index];
			if (!is_power && ordnance_tag_stock(ff_standard_weapon_tags[index]))
				ordnance_pool_add(regular, regular_count, ff_standard_weapon_tags[index]);
		}
		for (index = 0; index < ff_power_weapon_tag_count; index++)
		{
			if (ordnance_tag_stock(ff_power_weapon_tags[index]))
				ordnance_pool_add(heavy, heavy_count, ff_power_weapon_tags[index]);
		}
	}
	if (only || mode == _firefight_map_content_include_map)
	{
		for (index = 0; index < ff_map_weapon_count; index++)
		{
			if (!only && ff_map_weapon_stock[index])
				continue;
			if (ff_map_weapon_power[index])
				ordnance_pool_add(heavy, heavy_count, ff_map_weapon_tags[index]);
			else
				ordnance_pool_add(regular, regular_count, ff_map_weapon_tags[index]);
		}
	}
}

/* port: what a drop brings each player: a weapon always, and bonuses by its
health (its body's vitality; a player with no body, whole), each rolled on its
own, one of each at most, any or none of them (percent). The game's first
drop is a weapon each, nothing else */
static struct ff_ordnance_odds
{
	byte extra_weapon;
	byte ammo_crate;
	byte overshield;
	byte health;
} const g_ordnance_odds[] = {
	{ 10, 20, 20, 0 },  /* half its health or more */
	{ 10, 10, 30, 30 }, /* under half */
	{ 10, 10, 30, 60 }, /* under a quarter */
};

static boolean ordnance_roll(byte percent)
{
	return (long)(spawner_rand() % 100) < (long)percent;
}

/* a pod's weapon: of the variant's profile's half the time when it has
any, else a heavy one (when `heavy`, and there are any) or a regular one */
static long ordnance_weapon_pick(
	boolean heavy,
	long const *regular, short regular_count,
	long const *heavy_pool, short heavy_count,
	long const *custom, short custom_count)
{
	if (custom_count && ((!regular_count && !heavy_count) || (spawner_rand() & 1)))
		return ordnance_bag_next(_ff_ordnance_bag_custom, custom, custom_count);
	if (heavy && !heavy_count)
		heavy = FALSE;
	else if (!heavy && !regular_count)
		heavy = TRUE;
	if (heavy ? !heavy_count : !regular_count)
		return NONE;
	return heavy ? ordnance_bag_next(_ff_ordnance_bag_heavy, heavy_pool, heavy_count) :
		ordnance_bag_next(_ff_ordnance_bag_regular, regular, regular_count);
}

static boolean ordnance_powerup_usable(long tag_index, short powerup_type)
{
	struct equipment_definition *definition = equipment_definition_get(tag_index);

	/* (a powerup of no time does nothing when taken: players.c) */
	return definition && definition->equipment.powerup_type == powerup_type &&
		definition->equipment.powerup_time > 0.0f;
}

/* the level's powerup of the kind (the game's own by name first); NONE when
it has none: the pod gives it itself then */
static long ordnance_powerup_tag(short powerup_type)
{
	char const *path = powerup_type == _equipment_powerup_health ? "powerups\\health pack" : "powerups\\over shield";
	long tag_index = tag_loaded(EQUIPMENT_DEFINITION_TAG, path);
	struct tag_iterator iterator;

	if (tag_index != NONE && ordnance_powerup_usable(tag_index, powerup_type))
		return tag_index;
	tag_iterator_new(&iterator, EQUIPMENT_DEFINITION_TAG);
	while ((tag_index = tag_iterator_next(&iterator)) != NONE)
	{
		if (ordnance_powerup_usable(tag_index, powerup_type))
			return tag_index;
	}
	return NONE;
}

/* a drop's pods as they are rolled, before they're the drop's */
struct ff_ordnance_roll
{
	short count;
	long tags[FF_POD_MAX_CONTENTS];
	byte kinds[FF_POD_MAX_CONTENTS];
	byte owners[FF_POD_MAX_CONTENTS];
};

static void ordnance_roll_add(struct ff_ordnance_roll *roll, short kind, byte owner, long tag_index)
{
	if (roll->count >= FF_POD_MAX_CONTENTS)
		return;
	if (kind == _ff_pod_kind_weapon &&
		(tag_index == NONE || firefight_weapon_is_objective(tag_index, "an ordnance pod")))
	{
		return;
	}
	roll->tags[roll->count] = tag_index;
	roll->kinds[roll->count] = (byte)kind;
	roll->owners[roll->count++] = owner;
}

/* (firefight_spawn_ordnance_drop_for: the players, by their absolute index, a
drop is for, when it is for some alone) */
static unsigned long g_ordnance_players[(HALO_PORT_MAXIMUM_NETWORK_PLAYERS + 31) / 32];
static boolean g_ordnance_players_only;
/* (firefight_spawn_ordnance_weapon_for_all: every player's pod this weapon,
and nothing else, NONE for a drop's own) */
static long g_ordnance_forced_weapon = NONE;

void firefight_spawn_ordnance_drop(boolean heavy_anti_armor)
{
	real_point3d player_positions[16];
	long player_units[16];
	short player_count = get_living_player_positions(player_positions, player_units, 16);
	short round_num = (short)MAX(1, ff_state.current_round);
	long pistol_weap;
	long ar_weap;
	long shotgun_weap;
	long sniper_weap;
	long rocket_weap;
	real_point3d anchor;
	long anchor_unit;
	short pod_count;
	short i;

	if (player_count == 0)
	{
		real fallback_facing = 0.0f;
		/* At tick 0 before players spawn their initial biped, wait for tick 5 resupply */
		if (ff_state.session_elapsed_ticks < 2)
			return;
		if (!firefight_select_player_spawn_point(0, &player_positions[0], &fallback_facing))
			return;
		player_units[0] = NONE;
		player_count = 1;
	}

	if (ff_power_weapon_tag_count == 0 && ff_standard_weapon_tag_count == 0)
		scan_ordnance_weapon_and_equipment_tags();

	pistol_weap = find_loaded_tag_by_substring(WEAPON_DEFINITION_TAG, "weapons\\pistol\\pistol", "pistol", NULL);
	ar_weap = find_loaded_tag_by_substring(WEAPON_DEFINITION_TAG, "weapons\\assault rifle\\assault rifle", "assault rifle", NULL);
	shotgun_weap = find_loaded_tag_by_substring(WEAPON_DEFINITION_TAG, "weapons\\shotgun\\shotgun", "shotgun", NULL);
	sniper_weap = find_loaded_tag_by_substring(WEAPON_DEFINITION_TAG, "weapons\\sniper rifle\\sniper rifle", "sniper rifle", NULL);
	rocket_weap = find_loaded_tag_by_substring(WEAPON_DEFINITION_TAG, "weapons\\rocket launcher\\rocket launcher", "rocket launcher", NULL);

	/* (grenades topped up as a drop comes; the rest is what the pods bring:
	an ammo crate, a health pack) */
	for (i = 0; i < player_count; i++)
	{
		struct unit_datum *u = player_units[i] != NONE ?
			(struct unit_datum *)object_try_and_get_and_verify_type(player_units[i], _object_mask_unit) : NULL;

		if (!u)
			continue;
		if (u->unit.grenade_counts[_unit_grenade_human_fragmentation] < 4)
			u->unit.grenade_counts[_unit_grenade_human_fragmentation] =
				(signed char)MIN(4, u->unit.grenade_counts[_unit_grenade_human_fragmentation] + 2);
		if (u->unit.grenade_counts[_unit_grenade_covenant_plasma] < 4)
			u->unit.grenade_counts[_unit_grenade_covenant_plasma] =
				(signed char)MIN(4, u->unit.grenade_counts[_unit_grenade_covenant_plasma] + 2);
	}

	/* (the pods come to the player nearest the rest: people, not bots) */
	{
		real_point3d human_positions[16];
		long human_units[16];
		short human_count = get_living_human_player_positions(human_positions, human_units, 16);
		short central;

		if (human_count > 0)
		{
			central = central_player_index(human_positions, human_count);
			anchor = human_positions[central];
			anchor_unit = human_units[central];
			/* port: one on foot, else the floor under them (a pilot's
			pods came down at the aircraft's height) */
			players_floor_anchor(human_positions, human_units, human_count, central, &anchor, &anchor_unit);
		}
		else
		{
			central = central_player_index(player_positions, player_count);
			anchor = player_positions[central];
			anchor_unit = player_units[central];
			players_floor_anchor(player_positions, player_units, player_count, central, &anchor, &anchor_unit);
		}
	}

	/* port: what the pods hold, each player's set (by its place among the
	game's players, as firefight_hud.c counts them, which marks a player's own
	alone), full of ammunition: a weapon, the drop's first a heavy one on a
	heavy drop (round 4 on, or one called for anti-armour), the rest heavy
	FF_ORDNANCE_HEAVY_PERCENT of the time, of the bags above; and the rest of
	g_ordnance_odds, by the player's health */
	{
		long regular[FF_ORDNANCE_BAG_SIZE];
		long heavy_pool[FF_ORDNANCE_BAG_SIZE];
		long custom[FF_ORDNANCE_BAG_SIZE];
		short regular_count;
		short heavy_count;
		short custom_count = 0;
		boolean heavy = round_num >= 4 || heavy_anti_armor;
		boolean first_drop = ff_state.ordnance_drops_made == 0;
		boolean give_and_take = skull_active(_skull_give_and_take);
		long health_tag = ordnance_powerup_tag(_equipment_powerup_health);
		long shield_tag = ordnance_powerup_tag(_equipment_powerup_overshield);
		struct ff_ordnance_roll roll;
		struct data_iterator iterator;
		struct player_datum *player;
		short order = 0;
		boolean rack = tag_loaded('scen', FF_POD_RACK_TAG) != NONE;
		boolean ammo_crate_rolled = FALSE;

		ordnance_pools_build(regular, &regular_count, heavy_pool, &heavy_count);
		for (i = 0; i < ff_custom_weapon_count; i++)
			ordnance_pool_add(custom, &custom_count, ff_custom_weapon_tags[i]);
		/* (none of a pool: the other's, the game's few by name the last) */
		if (!regular_count && !heavy_count)
		{
			ordnance_pool_add(regular, &regular_count, pistol_weap);
			ordnance_pool_add(regular, &regular_count, ar_weap);
			ordnance_pool_add(heavy_pool, &heavy_count, shotgun_weap);
			ordnance_pool_add(heavy_pool, &heavy_count, sniper_weap);
			ordnance_pool_add(heavy_pool, &heavy_count, rocket_weap);
		}
		roll.count = 0;
		ff_state.ordnance_drops_made++;
		if (!player_data)
			return;
		data_iterator_new(&iterator, player_data);
		while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL &&
			roll.count < FF_POD_MAX_CONTENTS)
		{
			struct unit_datum *unit;
			struct ff_ordnance_odds const *odds;
			real health = 1.0f;
			byte owner;

			if (player->quit_out_of_game)
				continue;
			owner = (byte)MIN(order, FIREFIGHT_ORDNANCE_POD_ANYONE - 1);
			order++;
			/* (a drop for some players alone: an objective's reward) */
			if (g_ordnance_players_only && ((short)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index) < 0 ||
				(short)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index) >= HALO_PORT_MAXIMUM_NETWORK_PLAYERS ||
				!(g_ordnance_players[(short)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index) >> 5] & (1UL << ((short)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index) & 31)))))
			{
				continue;
			}
			unit = player->unit_index != NONE ?
				(struct unit_datum *)object_try_and_get_and_verify_type(player->unit_index, _object_mask_unit) : NULL;
			if (unit && !TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
				health = unit->object.body_vitality;
			odds = &g_ordnance_odds[health < 0.25f ? 2 : health < 0.5f ? 1 : 0];
			ordnance_roll_add(&roll, _ff_pod_kind_weapon, owner, g_ordnance_forced_weapon != NONE ? g_ordnance_forced_weapon :
				ordnance_weapon_pick(heavy || ordnance_roll(FF_ORDNANCE_HEAVY_PERCENT),
				regular, regular_count, heavy_pool, heavy_count, custom, custom_count));
			if (first_drop || g_ordnance_forced_weapon != NONE)
				continue;
			if (ordnance_roll(odds->extra_weapon))
			{
				ordnance_roll_add(&roll, _ff_pod_kind_weapon, owner, ordnance_weapon_pick(
					ordnance_roll(FF_ORDNANCE_HEAVY_PERCENT),
					regular, regular_count, heavy_pool, heavy_count, custom, custom_count));
			}
			/* (the rack: one a drop, for everyone) */
			if (ordnance_roll(odds->ammo_crate) && !(rack && ammo_crate_rolled))
			{
				ordnance_roll_add(&roll, _ff_pod_kind_ammo_crate, rack ? (byte)FIREFIGHT_ORDNANCE_POD_ANYONE : owner, NONE);
				ammo_crate_rolled = TRUE;
			}
			if (ordnance_roll(odds->overshield))
				ordnance_roll_add(&roll, _ff_pod_kind_overshield, owner, shield_tag);
			/* (Give & Take: a health pack is never taken, players.c) */
			if (!give_and_take && ordnance_roll(odds->health))
				ordnance_roll_add(&roll, _ff_pod_kind_health, owner, health_tag);
		}
		/* (no weapon to be had for anyone: no drop, the last one's left as it is) */
		if (!roll.count)
			return;

		/* (pods still in the air with the last drop's: their weapons out now,
		where those land) */
		pod_contents_spawn();
		g_pod_held_count = 0;
		ff_state.ordnance_pods_per_player = 0;
		for (i = 0; i < roll.count; i++)
		{
			g_pod_contents[i] = roll.tags[i];
			g_pod_touch[i] = (roll.kinds[i] == _ff_pod_kind_ammo_crate && !rack) ||
				(roll.kinds[i] != _ff_pod_kind_weapon && roll.kinds[i] != _ff_pod_kind_ammo_crate && roll.tags[i] == NONE);
			ff_state.ordnance_pod_kinds[i] = roll.kinds[i];
			ff_state.ordnance_pod_owners[i] = roll.owners[i];
		}
		g_pod_content_count = roll.count;
		csmemset(ff_state.ordnance_pod_spent, 0, sizeof(ff_state.ordnance_pod_spent));
	}

	/* where each pod lands (ordnance_pod_spot), apart from each other: round
	the drop area nearest the player, when Forge marked any, else near the
	player */
	pod_count = (short)PIN(g_pod_content_count, 1, FIREFIGHT_MAX_ORDNANCE_PODS);
	pods_shape_choose();
	csmemset(g_pod_spot_refusals, 0, sizeof(g_pod_spot_refusals));
	g_pod_spot_blocker = NONE;
	{
		real_point3d area;
		boolean around_area = drop_area_nearest(&anchor, &area);
		short fallen_back = 0;

		for (i = 0; i < pod_count; i++)
		{
			if (around_area && ordnance_pod_spot(&area, NONE, TRUE, ff_state.ordnance_pod_positions, i,
				&ff_state.ordnance_pod_positions[i]))
				continue;
			if (!ordnance_pod_spot(&anchor, anchor_unit, FALSE, ff_state.ordnance_pod_positions, i,
				&ff_state.ordnance_pod_positions[i]))
			{
				/* (nowhere that passes: the old way, before the player) */
				calculate_safe_player_resupply_point(&anchor, anchor_unit, &ff_state.ordnance_pod_positions[i]);
				ff_state.ordnance_pod_positions[i].x += (real)(i % 8) * 1.5f;
				ff_state.ordnance_pod_positions[i].y += (real)(i / 8) * 1.5f;
				snap_resupply_point_to_ground(&ff_state.ordnance_pod_positions[i]);
				fallen_back++;
			}
		}
		if (fallen_back)
		{
			char reasons[256];

			firefight_ordnance_preview_refusals(reasons, sizeof(reasons));
			error(_error_silent, "firefight: %d of %d ordnance pods found no spot (%s): put in a row; refused: %s",
				fallen_back, pod_count, around_area ? "round a drop area, then the player" : "near the player", reasons);
		}
	}
	ff_state.ordnance_pod_count = pod_count;
	ff_state.last_ordnance_position = ff_state.ordnance_pod_positions[0];
	ff_state.ordnance_navpoint_ticks = (short)(20 * TICKS_PER_SECOND + firefight_ordnance_drop_span_ticks());
	ff_state.last_ordnance_is_heavy = (round_num >= 4 || heavy_anti_armor);

	/* (let go now: every machine shows them falling, and their weapons come
	out on them as they land, firefight_ordnance_pod_update) */
	ff_state.ordnance_pod_drop_time = game_time_get();

	firefight_announce_banner(
		(round_num >= 4 || heavy_anti_armor) ? _ff_banner_heavy_ordnance_drop : _ff_banner_ordnance_drop,
		0,
		3 * TICKS_PER_SECOND);

	/* (none with an objective's drop, firefight_spawn_ordnance_drop_for) */
	if (g_ordnance_players_only || g_ordnance_forced_weapon != NONE)
		return;
	/* (VEHICLES' ORDNANCE REWARDS: a vehicle with each drop past the game's
	first, after its banner) */
	if (ff_state.ordnance_drops_made > 1)
		firefight_reward_player_vehicle();
}

/* (firefight_objectives.c) a drop as firefight_spawn_ordnance_drop's, its pods
for the players whose absolute indices `players` has alone: an objective's
reward to those who did most */
/* (firefight_objectives.c's Use X only) a drop of one weapon, a pod of it for
each player, nothing else in it */
void firefight_spawn_ordnance_weapon_for_all(long weapon_tag)
{
	if (weapon_tag == NONE)
		return;
	g_ordnance_forced_weapon = weapon_tag;
	firefight_spawn_ordnance_drop(FALSE);
	g_ordnance_forced_weapon = NONE;
}

/* (firefight_objectives.c's Use X only) a weapon of the drops' own, never
`avoid` (what the players start with): NONE for none */
long firefight_objective_ordnance_weapon(long const *avoid, short avoid_count)
{
	long regular[FF_ORDNANCE_BAG_SIZE];
	long heavy_pool[FF_ORDNANCE_BAG_SIZE];
	long candidates[2 * FF_ORDNANCE_BAG_SIZE];
	short regular_count;
	short heavy_count;
	short count = 0;
	short i;

	if (ff_power_weapon_tag_count == 0 && ff_standard_weapon_tag_count == 0)
		scan_ordnance_weapon_and_equipment_tags();
	ordnance_pools_build(regular, &regular_count, heavy_pool, &heavy_count);
	for (i = 0; i < regular_count + heavy_count; i++)
	{
		long tag = i < regular_count ? regular[i] : heavy_pool[i - regular_count];
		short a;

		if (tag == NONE || firefight_weapon_is_objective(tag, "an objective's weapon"))
			continue;
		for (a = 0; a < avoid_count && avoid[a] != tag; a++)
			;
		if (a == avoid_count)
			candidates[count++] = tag;
	}
	return count ? candidates[spawner_rand() % (unsigned long)count] : NONE;
}

/* (firefight_objectives.c's Use X only, a player without its weapon and none
of it lying about) a pod of the weapon for each of the players alone */
void firefight_spawn_ordnance_weapon_for(unsigned long const *players, short words, long weapon_tag)
{
	short word;

	if (weapon_tag == NONE)
		return;
	csmemset(g_ordnance_players, 0, sizeof(g_ordnance_players));
	for (word = 0; word < words && word < (short)NUMBEROF(g_ordnance_players); word++)
		g_ordnance_players[word] = players[word];
	g_ordnance_players_only = TRUE;
	g_ordnance_forced_weapon = weapon_tag;
	firefight_spawn_ordnance_drop(FALSE);
	g_ordnance_forced_weapon = NONE;
	g_ordnance_players_only = FALSE;
}

void firefight_spawn_ordnance_drop_for(unsigned long const *players, short words)
{
	short word;

	csmemset(g_ordnance_players, 0, sizeof(g_ordnance_players));
	for (word = 0; word < words && word < (short)NUMBEROF(g_ordnance_players); word++)
		g_ordnance_players[word] = players[word];
	g_ordnance_players_only = TRUE;
	firefight_spawn_ordnance_drop(ff_state.current_round >= 4);
	g_ordnance_players_only = FALSE;
}

/* ---------- The Ordnance Pods

A drop comes down from the sky in pods, a weapon in each: a capsule each (the
game's props: tools/prop_assets.py), else a Covenant storage crate (c40's
scenery\c_storage, from the game modes' pack: randomizer.c's gather_firefight),
round the drop area nearest the player when Forge marked any,
falling for FF_POD_FALL_TICKS onto ordnance_pod_positions, their weapons held
upright against their sides once they land, full of ammunition, till taken.
A pod is scenery, which the netcode doesn't carry: each machine makes its
own, from the time the host let them go and where they land (Firefight's
state), so they fall alike everywhere; the weapons are the host's, as any. */

/* (a pod's half width it is kept from walls and others by, and how far
apart pods land: the crate's, or the capsule's, which is slim) */
#define FF_POD_CRATE_CLEARANCE 1.7f
#define FF_POD_CRATE_APART 4.5f
#define FF_POD_CAPSULE_CLEARANCE 0.8f
#define FF_POD_CAPSULE_APART 1.8f
static real g_pod_clearance = FF_POD_CRATE_CLEARANCE;
static real g_pod_apart = FF_POD_CRATE_APART;
/* an object a pod's spot pays no heed to: a test drop's drop area, which
Firefight would have taken away (NONE: none) */
static long g_pod_spot_ignored = NONE;

/* the pod's tag: the capsule, else the crate (NONE: neither) */
static long pod_tag_get(boolean *capsule)
{
	long tag = tag_loaded('scen', FF_POD_TAG);

	*capsule = tag != NONE;
	return tag != NONE ? tag : tag_loaded('scen', FF_POD_FALLBACK_TAG);
}

/* (the host's, as it drops them) how far apart the pods are, by their shape */
static void pods_shape_choose(void)
{
	boolean capsule;

	pod_tag_get(&capsule);
	g_pod_clearance = capsule ? FF_POD_CAPSULE_CLEARANCE : FF_POD_CRATE_CLEARANCE;
	g_pod_apart = capsule ? FF_POD_CAPSULE_APART : FF_POD_CRATE_APART;
}

/* (each machine's) the level's drop areas, once it plays: where each stands,
and its marker taken away (scenery, which each machine has its own of) */
static void drop_areas_find(void)
{
	long tag;
	struct object_iterator iterator;
	long markers[FF_MAX_DROP_AREAS];
	short marker_count = 0;
	short index;

	if (g_drop_areas_found)
		return;
	g_drop_areas_found = TRUE;
	g_drop_area_count = 0;
	tag = tag_loaded('scen', FF_DROP_AREA_TAG);
	if (tag == NONE)
		return;
	object_iterator_new(&iterator, _object_mask_scenery, 0);
	while (object_iterator_next(&iterator) && marker_count < FF_MAX_DROP_AREAS)
	{
		struct object_datum *object = object_get(iterator.index);

		short pod;

		if (object->definition_index != tag)
			continue;
		for (pod = 0; pod < FIREFIGHT_MAX_ORDNANCE_PODS && g_pod_objects[pod] != iterator.index; pod++)
		{
		}
		if (pod < FIREFIGHT_MAX_ORDNANCE_PODS)
			continue;
		g_drop_areas[g_drop_area_count++] = object->object.position;
		markers[marker_count++] = iterator.index;
	}
	for (index = 0; index < marker_count; index++)
		object_delete(markers[index]);
	if (g_drop_area_count)
		error(_error_silent, "firefight: %d drop areas for the ordnance pods", g_drop_area_count);
}

/* the drop area nearest a point; FALSE when the level has none */
static boolean drop_area_nearest(real_point3d const *point, real_point3d *area)
{
	real best = REAL_MAX;
	short index;

	drop_areas_find();
	for (index = 0; index < g_drop_area_count; index++)
	{
		real distance = distance_squared3d(point, &g_drop_areas[index]);

		if (distance < best)
		{
			best = distance;
			*area = g_drop_areas[index];
		}
	}
	return g_drop_area_count > 0;
}

static boolean pod_ray(unsigned long flags, real_point3d const *start, real_vector3d const *vector,
	struct collision_result *collision)
{
	/* (a test drop's drop area marker is passed through: real drops have
	taken it away) */
	return collision_test_vector(flags, start, vector, g_pod_spot_ignored, collision);
}

/* whether a pod may land at the ground under `candidate` (which it becomes):
flat ground of the map's (not a vehicle's roof nor a crate's), seen from
where the player's head is (not behind or in a wall), nothing within its
width on any side, open sky over it (`sky` units at least), no vehicle or
person on it, apart from the drop's other pods */
static boolean pod_spot_passes(
	real_point3d const *eye,
	real_point3d *candidate,
	real sky,
	boolean away_from_enemies,
	real_point3d const *others,
	short other_count)
{
	static unsigned long const structure = FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit);
	unsigned long const solid = structure | FLAG(_collision_test_objects_bit) |
		FLAG(_collision_test_objects_vehicles_bit) | FLAG(_collision_test_objects_scenery_bit) |
		FLAG(_collision_test_objects_machines_bit) | FLAG(_collision_test_objects_bipeds_bit);
	struct collision_result collision;
	real_point3d ground;
	real_point3d start;
	real_vector3d vector;
	short side;

	/* (the ground: what a ray down meets first must be the map's, and level) */
	start = *candidate;
	start.z = eye->z + 2.0f;
	vector.i = 0.0f;
	vector.j = 0.0f;
	vector.k = -12.0f;
	if (!pod_ray(solid, &start, &vector, &collision))
		return POD_SPOT_REFUSED(_pod_spot_no_ground);
	/* (a structure hit leaves object_index as it was: only an object hit
	sets it) */
	if (collision.type == _collision_result_object)
	{
		struct object_datum *under = object_try_and_get(collision.object_index);

		if (g_pod_spot_blocker == NONE && under)
			g_pod_spot_blocker = under->definition_index;
		return POD_SPOT_REFUSED(_pod_spot_object_under);
	}
	if (collision.plane.n.k < 0.8f)
		return POD_SPOT_REFUSED(_pod_spot_slope);
	ground = collision.point;
	/* (a point a little over the floor: one on it can be in no leaf) */
	start = ground;
	start.z += 0.3f;
	if (!point_is_in_valid_bsp(&start))
		return POD_SPOT_REFUSED(_pod_spot_outside_map);
	/* port: not in a volume the map kills in, nor on the ground under one
	(Splash Site's sea floor under "killme2": death_volumes.c) */
	if (death_volume_column(&ground, FF_POD_KILL_VOLUME_HEIGHT))
		return POD_SPOT_REFUSED(_pod_spot_kill_volume);
	/* (seen from the player: no wall between, nor the pod inside one) */
	start = ground;
	start.z += 0.8f;
	vector_from_points3d(eye, &start, &vector);
	if (pod_ray(structure, eye, &vector, &collision) && collision.t < 0.98f)
		return POD_SPOT_REFUSED(_pod_spot_unseen);
	/* (its width clear round it, low and high) */
	for (side = 0; side < 8; side++)
	{
		real angle = (real)side * (_pi / 4.0f);
		short level;

		for (level = 0; level < 2; level++)
		{
			start = ground;
			start.z += level ? 1.3f : 0.45f;
			vector.i = (real)cos(angle) * g_pod_clearance;
			vector.j = (real)sin(angle) * g_pod_clearance;
			vector.k = 0.0f;
			if (pod_ray(solid, &start, &vector, &collision))
				return POD_SPOT_REFUSED(_pod_spot_cramped);
		}
	}
	/* (the sky open over it: its fall) */
	start = ground;
	start.z += 0.5f;
	vector.i = 0.0f;
	vector.j = 0.0f;
	vector.k = FF_POD_FALL_HEIGHT;
	if (pod_ray(solid, &start, &vector, &collision) && collision.t * FF_POD_FALL_HEIGHT < sky)
		return POD_SPOT_REFUSED(_pod_spot_roofed);
	/* (no vehicle, person or generator about it: an object's own size too) */
	{
		struct object_iterator iterator;
		struct object_datum *object;

		object_iterator_new(&iterator, _object_mask_vehicle | _object_mask_biped | _object_mask_scenery, 0);
		while ((object = (struct object_datum *)object_iterator_next(&iterator)) != NULL)
		{
			real dx = object->object.bounding_sphere_center.x - ground.x;
			real dy = object->object.bounding_sphere_center.y - ground.y;
			real reach = object->object.bounding_sphere_radius + g_pod_clearance + 0.5f;

			if (TEST_FLAG(object->object.damage_flags, _object_dead_bit) && object->object.type == _object_type_biped)
				continue;
			if (iterator.index == g_pod_spot_ignored)
				continue;
			/* (a building of scenery's sphere is all about it: its walls are
			what the rays above find) */
			if (object->object.type == _object_type_scenery && object->object.bounding_sphere_radius > 4.0f)
				continue;
			if (dx * dx + dy * dy < reach * reach &&
				object->object.bounding_sphere_center.z - object->object.bounding_sphere_radius < ground.z + 3.0f &&
				object->object.bounding_sphere_center.z + object->object.bounding_sphere_radius > ground.z - 0.5f)
			{
				if (g_pod_spot_blocker == NONE)
					g_pod_spot_blocker = object->definition_index;
				return POD_SPOT_REFUSED(_pod_spot_crowded);
			}
		}
	}
	for (side = 0; side < other_count; side++)
	{
		if (distance3d(&others[side], &ground) < g_pod_apart)
			return POD_SPOT_REFUSED(_pod_spot_too_near);
	}
	if (away_from_enemies && !point_is_safe_from_enemy_zones(&ground, 20.0f))
		return POD_SPOT_REFUSED(_pod_spot_enemies);
	*candidate = ground;
	return TRUE;
}

/* a pod's landing spot near the player: before them first, a little way
off, then round them and further; open sky and clear of the enemies first,
then a lower ceiling, then nearer the enemies. Round a drop area
(`at_area`): on it first, then out from it. FALSE if nowhere passes */
static boolean ordnance_pod_spot(
	real_point3d const *anchor,
	long anchor_unit,
	boolean at_area,
	real_point3d const *others,
	short other_count,
	real_point3d *out_position)
{
	static real const player_distances[] = { 6.0f, 7.5f, 5.0f, 9.0f, 4.0f, 11.0f, 13.5f };
	static real const area_distances[] = { 0.0f, 1.9f, 3.6f, 5.3f, 7.0f, 8.7f, 10.5f };
	real const *distances = at_area ? area_distances : player_distances;
	static real const turns[] = {
		0.0f, 0.5235988f, -0.5235988f, 1.0471976f, -1.0471976f, 1.5707963f, -1.5707963f,
		2.0943951f, -2.0943951f, 2.6179939f, -2.6179939f, 3.1415927f
	};
	static real const skies[] = { 12.0f, 4.0f, 4.0f };
	struct unit_datum *unit = anchor_unit != NONE ?
		(struct unit_datum *)object_try_and_get_and_verify_type(anchor_unit, _object_mask_unit) : NULL;
	real facing = 0.0f;
	real_point3d eye = *anchor;
	short pass;

	if (unit && (unit->object.forward.i != 0.0f || unit->object.forward.j != 0.0f))
		facing = (real)atan2(unit->object.forward.j, unit->object.forward.i);
	eye.z += 0.6f;
	for (pass = 0; pass < (short)NUMBEROF(skies); pass++)
	{
		short d, t;

		for (d = 0; d < (short)NUMBEROF(player_distances); d++)
		{
			for (t = 0; t < (short)NUMBEROF(turns); t++)
			{
				real_point3d candidate = *anchor;

				candidate.x += (real)cos(facing + turns[t]) * distances[d];
				candidate.y += (real)sin(facing + turns[t]) * distances[d];
				if (pod_spot_passes(&eye, &candidate, skies[pass], pass < 2, others, other_count))
				{
					*out_position = candidate;
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}

/* a weapon's magazines full, reserve and loaded both */
static void pod_weapon_fill(long weapon_index)
{
	struct weapon_datum *weapon = (struct weapon_datum *)object_try_and_get_and_verify_type(
		weapon_index, _object_mask_weapon);
	struct weapon_definition *definition = weapon ? weapon_definition_get(weapon->definition_index) : NULL;
	short m;

	for (m = 0; definition && m < definition->weapon.magazines.count && m < 2; m++)
	{
		struct weapon_magazine_definition *magazine = TAG_BLOCK_GET_ELEMENT(
			&definition->weapon.magazines, m, struct weapon_magazine_definition);

		if (magazine->rounds_total_maximum > 0)
			weapon->weapon.magazines[m].rounds_total = magazine->rounds_total_maximum;
		if (magazine->rounds_loaded_maximum > 0)
			weapon->weapon.magazines[m].rounds_loaded = magazine->rounds_loaded_maximum;
	}
}

static real pod_yaw(short pod);

/* where a pod's weapon is: in the capsule, standing in its open side; or
against the crate's side that way (a ray in from outside meets its scenery,
and the weapon stands just off it, upright, its back to the pod); without
the pod, so far from where it landed */
static void pod_slot_place(short pod, real_point3d *position, real_vector3d *forward, real_vector3d *up)
{
	real_point3d const *center = &ff_state.ordnance_pod_positions[pod];
	real angle = pod_yaw(pod);
	real_vector3d outward;
	real distance = g_pod_capsule ? FF_POD_CAPSULE_WEAPON_OUT : 1.2f;
	real height = g_pod_capsule ? FF_POD_CAPSULE_WEAPON_HEIGHT : 0.55f;

	outward.i = (real)cos(angle);
	outward.j = (real)sin(angle);
	outward.k = 0.0f;
	if (!g_pod_capsule && g_pod_objects[pod] != NONE && object_try_and_get(g_pod_objects[pod]))
	{
		struct collision_result collision;
		real_point3d start = *center;
		real_vector3d inward;

		start.x += outward.i * 4.0f;
		start.y += outward.j * 4.0f;
		start.z += height;
		inward.i = -outward.i * 4.0f;
		inward.j = -outward.j * 4.0f;
		inward.k = 0.0f;
		if (collision_test_vector(FLAG(_collision_test_objects_bit) | FLAG(_collision_test_objects_scenery_bit) |
				FLAG(_collision_test_front_facing_surfaces_bit), &start, &inward, NONE, &collision) &&
			collision.object_index == g_pod_objects[pod])
		{
			distance = MAX(0.2f, 4.0f * (1.0f - collision.t)) + 0.14f;
		}
	}
	*position = *center;
	position->x += outward.i * distance;
	position->y += outward.j * distance;
	position->z += height;
	*forward = *global_up3d;
	*up = outward;
}

/* a pod's weapon (or powerup), out on it as it lands, full, and held there
(pod_weapons_hold); a powerup stands upright in it */
static void pod_content_spawn(short pod)
{
	struct object_placement_data placement;
	long tag_index;
	long object_index;

	if (pod < 0 || pod >= g_pod_content_count || g_pod_contents[pod] == NONE)
		return;
	tag_index = g_pod_contents[pod];
	g_pod_contents[pod] = NONE;
	object_placement_data_new(&placement, tag_index, NONE);
	pod_slot_place(pod, &placement.position, &placement.forward, &placement.up);
	if (ff_state.ordnance_pod_kinds[pod] != _ff_pod_kind_weapon)
	{
		placement.forward = placement.up;
		placement.up = *global_up3d;
	}
	object_index = object_new(&placement);
	if (object_index == NONE)
		return;
	SET_FLAG(object_get(object_index)->object.flags, _object_cannot_be_garbage_bit, TRUE);
	pod_weapon_fill(object_index);
	if (g_pod_held_count < FF_POD_MAX_CONTENTS)
	{
		g_pod_held[g_pod_held_count].object_index = object_index;
		g_pod_held[g_pod_held_count].pod = pod;
		g_pod_held[g_pod_held_count].position = placement.position;
		g_pod_held[g_pod_held_count].forward = placement.forward;
		g_pod_held[g_pod_held_count++].up = placement.up;
	}
}

/* every pod's not out yet (a new drop's coming: the last one's out where
they land) */
static void pod_contents_spawn(void)
{
	short pods = (short)PIN(ff_state.ordnance_pod_count, 1, FIREFIGHT_MAX_ORDNANCE_PODS);
	short pod;

	for (pod = 0; pod < g_pod_content_count && pod < pods; pod++)
		pod_content_spawn(pod);
	g_pod_content_count = 0;
}

/* (the host's, each tick the pods stand) each weapon kept at its place on its
pod, still: let go once a player has it, or something knocked it away */
static void pod_weapons_hold(boolean release)
{
	short i;

	for (i = 0; i < g_pod_held_count; i++)
	{
		long object_index = g_pod_held[i].object_index;
		struct object_datum *object = object_index != NONE ?
			object_try_and_get_and_verify_type(object_index, _object_mask_weapon | _object_mask_equipment) : NULL;

		if (!object || object->object.parent_object_index != NONE || release ||
			distance3d(&object->object.position, &g_pod_held[i].position) > 0.6f)
		{
			g_pod_held[i].object_index = NONE;
			continue;
		}
		object->object.translational_velocity = *global_zero_vector3d;
		object->object.angular_velocity = *global_zero_vector3d;
		object_set_position(object_index, &g_pod_held[i].position, &g_pod_held[i].forward, &g_pod_held[i].up);
	}
	if (release)
		g_pod_held_count = 0;
}

/* (the host's) a pod gone: what's on it falls to the ground */
static void pod_weapons_release(short pod)
{
	short i;

	for (i = 0; i < g_pod_held_count; i++)
	{
		if (g_pod_held[i].pod == pod)
			g_pod_held[i].object_index = NONE;
	}
}

boolean firefight_ordnance_pod_spent(short pod)
{
	return pod >= 0 && pod < FIREFIGHT_MAX_ORDNANCE_PODS &&
		(ff_state.ordnance_pod_spent[pod >> 5] & (1UL << (pod & 31))) != 0;
}

/* how near a player walks to a pod's spot for what the pod gives itself:
across, and up or down */
#define FF_POD_TOUCH_REACH 1.4f
#define FF_POD_TOUCH_HEIGHT 2.0f

static boolean pod_unit_touches(short pod, long unit_index, real reach)
{
	struct object_datum *unit = object_try_and_get_and_verify_type(unit_index, _object_mask_unit);
	real_point3d const *spot = &ff_state.ordnance_pod_positions[pod];

	return unit && !TEST_FLAG(unit->object.damage_flags, _object_dead_bit) &&
		distance2d((real_point2d const *)&unit->object.position, (real_point2d const *)spot) < reach &&
		fabs(unit->object.position.z - spot->z) < FF_POD_TOUCH_HEIGHT;
}

/* the weapon in the unit's hands' reserve full (one of charge charged);
whether it wanted it */
static boolean pod_unit_weapon_refill(long unit_index)
{
	struct unit_datum *unit = (struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);
	short slot = unit ? unit->unit.current_weapon_index : NONE;
	long weapon_index = slot >= 0 && slot < MAXIMUM_WEAPONS_PER_UNIT ? unit->unit.weapon_object_indices[slot] : NONE;
	struct weapon_datum *weapon = weapon_index != NONE ?
		(struct weapon_datum *)object_try_and_get_and_verify_type(weapon_index, _object_mask_weapon) : NULL;
	struct weapon_definition *definition = weapon ? weapon_definition_get(weapon->definition_index) : NULL;
	boolean filled = FALSE;
	short m;

	if (!definition)
		return FALSE;
	if (!definition->weapon.magazines.count && weapon->weapon.age > 0.0f)
	{
		weapon->weapon.age = 0.0f;
		filled = TRUE;
	}
	for (m = 0; m < definition->weapon.magazines.count && m < 2; m++)
	{
		struct weapon_magazine_definition *magazine = TAG_BLOCK_GET_ELEMENT(
			&definition->weapon.magazines, m, struct weapon_magazine_definition);

		if (magazine->rounds_total_maximum > 0 &&
			weapon->weapon.magazines[m].rounds_total < magazine->rounds_total_maximum)
		{
			weapon->weapon.magazines[m].rounds_total = magazine->rounds_total_maximum;
			filled = TRUE;
		}
	}
	return filled;
}

/* (the host's) the landed pods that give what they bring themselves: to the
first player on foot to walk into one who wants it (a weapon not full; a body
hurt; shields not overcharged yet), then it's used up */
static void pods_touch_update(short pods)
{
	struct data_iterator iterator;
	struct player_datum *player;
	short pod;

	if (!player_data)
		return;
	for (pod = 0; pod < pods; pod++)
	{
		if (g_pod_phase[pod] != _pod_phase_landed || !g_pod_touch[pod] || firefight_ordnance_pod_spent(pod))
			continue;
		data_iterator_new(&iterator, player_data);
		while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		{
			struct object_datum *unit = player->unit_index != NONE ? object_try_and_get(player->unit_index) : NULL;
			boolean used = FALSE;

			if (!unit || unit->object.parent_object_index != NONE ||
				!pod_unit_touches(pod, player->unit_index, FF_POD_TOUCH_REACH))
			{
				continue;
			}
			switch (ff_state.ordnance_pod_kinds[pod])
			{
			case _ff_pod_kind_ammo_crate: used = pod_unit_weapon_refill(player->unit_index); break;
			case _ff_pod_kind_health: used = !skull_active(_skull_give_and_take) && object_restore_body(player->unit_index); break;
			case _ff_pod_kind_overshield: used = object_double_charge_shield(player->unit_index); break;
			}
			if (used)
			{
				ff_state.ordnance_pod_spent[pod >> 5] |= 1UL << (pod & 31);
				break;
			}
		}
	}
}

/* (each machine's) a pod used up since it last looked: its own players at it
told what they got */
static void pods_spent_messages(short pods)
{
	short pod;

	for (pod = 0; pod < pods; pod++)
	{
		unsigned long bit = 1UL << (pod & 31);
		struct data_iterator iterator;
		struct player_datum *player;
		wchar_t const *text;

		if (!(ff_state.ordnance_pod_spent[pod >> 5] & bit) || (g_pod_spent_seen[pod >> 5] & bit))
			continue;
		g_pod_spent_seen[pod >> 5] |= bit;
		switch (ff_state.ordnance_pod_kinds[pod])
		{
		case _ff_pod_kind_ammo_crate: text = L"Ammo crate: weapon refilled."; break;
		case _ff_pod_kind_health: text = L"Health restored."; break;
		case _ff_pod_kind_overshield: text = L"Overshield!"; break;
		default: continue;
		}
		if (!player_data)
			continue;
		data_iterator_new(&iterator, player_data);
		while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		{
			if (player->local_player_index != NONE && player->unit_index != NONE &&
				pod_unit_touches(pod, player->unit_index, FF_POD_TOUCH_REACH * 1.5f))
			{
				hud_print_message(player->local_player_index, text);
			}
		}
	}
}

static void pod_effect(char const *effect_name, real_point3d const *position)
{
	long effect_tag = tag_loaded(EFFECT_DEFINITION_TAG, effect_name);
	char const *marker_names[1] = { "" };
	real_point3d marker_points[1];
	real_vector3d marker_forwards[1];

	if (effect_tag == NONE || !position)
		return;
	marker_points[0] = *position;
	marker_points[0].z += 0.2f;
	marker_forwards[0] = *global_up3d;
	skulls_effect_harmless = TRUE;
	effect_new_unattached_from_markers(effect_tag, NONE, NULL, 1, marker_names, marker_points, marker_forwards,
		0.0f, 0.0f, NULL, NULL, TRUE);
	skulls_effect_harmless = FALSE;
}

static void pods_delete_shown(void)
{
	short pod;

	for (pod = 0; pod < FIREFIGHT_MAX_ORDNANCE_PODS; pod++)
	{
		if (g_pod_objects[pod] != NONE && object_try_and_get_and_verify_type(g_pod_objects[pod], _object_mask_scenery))
			object_delete(g_pod_objects[pod]);
		g_pod_objects[pod] = NONE;
	}
}

/* the way a pod is turned, as on every machine */
static real pod_yaw(short pod)
{
	return (real)(((unsigned long)ff_state.ordnance_pod_drop_time + (unsigned long)pod * 977UL) * 2654435761UL %
		6283UL) / 1000.0f;
}

/* where a pod is, so many ticks after it was let go: falling faster and
faster from its height to the ground, turned as on every machine. The
capsule stands on its base (its model's x down), its open side the way it
is turned (its model's z); the crate lies as it is made */
/* (an ammo crate's pod the rack, when the level has it: FF_POD_RACK_TAG) */
static long pod_rack_tag(short pod)
{
	if (pod < 0 || pod >= FIREFIGHT_MAX_ORDNANCE_PODS || ff_state.ordnance_pod_kinds[pod] != _ff_pod_kind_ammo_crate)
		return NONE;
	return tag_loaded('scen', FF_POD_RACK_TAG);
}

static void pod_position_at(short pod, long elapsed, real_point3d *position, real_vector3d *forward, real_vector3d *up)
{
	real t = PIN((real)elapsed / (real)FF_POD_FALL_TICKS, 0.0f, 1.0f);
	real yaw = pod_yaw(pod);

	*position = ff_state.ordnance_pod_positions[pod];
	position->z += g_pod_fall_heights[pod] * (1.0f - t * t);
	/* (the rack stands as Forge places it) */
	if (g_pod_capsule && pod_rack_tag(pod) == NONE)
	{
		position->z += FF_POD_CAPSULE_BASE;
		forward->i = 0.0f;
		forward->j = 0.0f;
		forward->k = -1.0f;
		vector3d_from_angle(up, yaw);
	}
	else
	{
		vector3d_from_angle(forward, yaw);
		*up = *global_up3d;
	}
}

/* port: a drop's groups, 3 to 8 by how many pods (two a group; fewer pods
than 3, one a group), each FIREFIGHT_ORDNANCE_GROUP_TICKS after the last; the
pods in order (each player's set together, firefight_spawn_ordnance_drop) */
static short ordnance_group_count(short pod_count)
{
	short groups = (short)PIN((pod_count + 1) / 2, FIREFIGHT_ORDNANCE_MIN_GROUPS, FIREFIGHT_ORDNANCE_MAX_GROUPS);

	return (short)MIN(groups, MAX(pod_count, 1));
}

long firefight_ordnance_pod_release_delay(short pod, short pod_count)
{
	if (pod_count <= 0)
		return 0;
	pod = (short)PIN(pod, 0, pod_count - 1);
	return (long)(pod * ordnance_group_count(pod_count) / pod_count) * FIREFIGHT_ORDNANCE_GROUP_TICKS;
}

long firefight_ordnance_drop_span_ticks(void)
{
	short pods = (short)PIN(ff_state.ordnance_pod_count, 1, FIREFIGHT_MAX_ORDNANCE_PODS);

	return firefight_ordnance_pod_release_delay((short)(pods - 1), pods);
}

long firefight_ordnance_pod_land_time(short pod)
{
	short pods = (short)PIN(ff_state.ordnance_pod_count, 1, FIREFIGHT_MAX_ORDNANCE_PODS);

	if (ff_state.ordnance_pod_drop_time == NONE || ff_state.ordnance_pod_drop_time <= 0)
		return NONE;
	return ff_state.ordnance_pod_drop_time + firefight_ordnance_pod_release_delay(pod, pods) + FF_POD_FALL_TICKS;
}

static void flood_drop_reset(void);

void firefight_ordnance_pod_reset(void)
{
	short pod;

	ff_state.ordnance_pod_drop_time = NONE;
	ff_state.ordnance_pod_count = 0;
	for (pod = 0; pod < FIREFIGHT_MAX_ORDNANCE_PODS; pod++)
	{
		g_pod_objects[pod] = NONE;
		g_pod_phase[pod] = _pod_phase_waiting;
		g_pod_touch[pod] = FALSE;
	}
	csmemset(g_pod_spent_seen, 0, sizeof(g_pod_spent_seen));
	g_pod_shown_time = NONE;
	g_pod_content_count = 0;
	g_pod_held_count = 0;
	/* (the host's) the game's first drop, 5 to 10 seconds in (port) */
	ff_state.ordnance_drops_made = 0;
	ff_state.first_ordnance_tick = (long)(5 * TICKS_PER_SECOND + spawner_rand() % (5 * TICKS_PER_SECOND + 1));
	/* (a game's start: the level's objects are made again) */
	g_drop_area_count = 0;
	g_drop_areas_found = FALSE;
	flood_drop_reset();
}

/* (each machine's) a pod's scenery made as its group lets go, from as high
as the sky over it lets it */
static void pod_let_go(short pod, long elapsed, long pod_tag)
{
	struct collision_result collision;
	real_point3d ray_start = ff_state.ordnance_pod_positions[pod];
	real_vector3d ray_up = { 0.0f, 0.0f, FF_POD_FALL_HEIGHT };

	g_pod_fall_heights[pod] = FF_POD_FALL_HEIGHT;
	ray_start.z += 1.0f;
	if (collision_test_vector(FLAG(_collision_test_structure_bit), &ray_start, &ray_up, NONE, &collision))
		g_pod_fall_heights[pod] = MAX(3.0f, collision.t * FF_POD_FALL_HEIGHT - 1.5f);
	if (elapsed < FF_POD_FALL_TICKS)
		pod_effect("effects\\coop teleport", &ff_state.ordnance_pod_positions[pod]);
	g_pod_objects[pod] = NONE;
	if (pod_tag != NONE)
	{
		struct object_placement_data placement;

		object_placement_data_new(&placement, pod_tag, NONE);
		pod_position_at(pod, elapsed, &placement.position, &placement.forward, &placement.up);
		g_pod_objects[pod] = object_new(&placement);
		if (g_pod_objects[pod] != NONE)
		{
			SET_FLAG(object_get(g_pod_objects[pod])->object.flags, _object_cannot_be_garbage_bit, TRUE);
			object_set_automatic_deactivation(g_pod_objects[pod], FALSE);
		}
	}
}

void firefight_ordnance_pod_update(void)
{
	long drop_time = ff_state.ordnance_pod_drop_time;
	short pods = (short)PIN(ff_state.ordnance_pod_count, 1, FIREFIGHT_MAX_ORDNANCE_PODS);
	boolean host = !network_game_distributed_client();
	boolean standing = FALSE;
	long elapsed;
	real_point3d position;
	real_vector3d forward;
	real_vector3d up;
	short pod;

	/* (the drop areas' markers go as the level plays) */
	drop_areas_find();
	/* (0: Firefight's state as it was cleared, no drop) */
	if (drop_time == NONE || drop_time <= 0)
	{
		pods_delete_shown();
		g_pod_shown_time = NONE;
		if (host)
			pod_weapons_hold(TRUE);
		return;
	}
	elapsed = MAX(0, game_time_get() - drop_time);

	/* a new drop: its pods let go group by group (port) */
	if (g_pod_shown_time != drop_time)
	{
		g_pod_shown_tag = pod_tag_get(&g_pod_capsule);
		pods_delete_shown();
		g_pod_shown_time = drop_time;
		for (pod = 0; pod < FIREFIGHT_MAX_ORDNANCE_PODS; pod++)
			g_pod_phase[pod] = _pod_phase_waiting;
		/* (a late joiner's: none it never saw used told of) */
		csmemcpy(g_pod_spent_seen, ff_state.ordnance_pod_spent, sizeof(g_pod_spent_seen));
		/* (a client's marker of them; the host set its own as it dropped them) */
		if (!host && elapsed < firefight_ordnance_drop_span_ticks() + FF_POD_FALL_TICKS)
			ff_state.ordnance_navpoint_ticks = (short)(20 * TICKS_PER_SECOND + firefight_ordnance_drop_span_ticks());
		if (g_pod_shown_tag == NONE)
			error(_error_silent, "firefight: no %s for the ordnance pods: their weapons land without them", FF_POD_TAG);
	}

	for (pod = 0; pod < pods; pod++)
	{
		long pod_elapsed = elapsed - firefight_ordnance_pod_release_delay(pod, pods);

		if (pod_elapsed < 0)
		{
			standing = TRUE;
			continue;
		}
		if (g_pod_phase[pod] == _pod_phase_waiting)
		{
			/* (long gone before this machine saw it: a late joiner, a replay
			sought past it) */
			if (pod_elapsed >= FF_POD_FALL_TICKS + FF_POD_STAY_TICKS)
			{
				if (host)
					pod_content_spawn(pod);
				g_pod_phase[pod] = _pod_phase_gone;
				continue;
			}
			pod_let_go(pod, pod_elapsed, pod_rack_tag(pod) != NONE ? pod_rack_tag(pod) : g_pod_shown_tag);
			g_pod_phase[pod] = _pod_phase_falling;
		}
		if (g_pod_phase[pod] == _pod_phase_falling)
		{
			if (pod_elapsed < FF_POD_FALL_TICKS)
			{
				if (g_pod_objects[pod] != NONE)
				{
					pod_position_at(pod, pod_elapsed, &position, &forward, &up);
					object_set_position(g_pod_objects[pod], &position, &forward, &up);
				}
				standing = TRUE;
				continue;
			}
			if (g_pod_objects[pod] != NONE)
			{
				pod_position_at(pod, FF_POD_FALL_TICKS, &position, &forward, &up);
				object_set_position(g_pod_objects[pod], &position, &forward, &up);
			}
			/* (only a late joiner or a replay sought past it misses the blast) */
			if (pod_elapsed < FF_POD_FALL_TICKS + TICKS_PER_SECOND)
			{
				pod_effect("weapons\\frag grenade\\effects\\explosion", &ff_state.ordnance_pod_positions[pod]);
				pod_effect("effects\\coop teleport", &ff_state.ordnance_pod_positions[pod]);
			}
			if (host)
				pod_content_spawn(pod);
			g_pod_phase[pod] = _pod_phase_landed;
		}
		if (g_pod_phase[pod] == _pod_phase_landed)
		{
			if (pod_elapsed >= FF_POD_FALL_TICKS + FF_POD_STAY_TICKS)
			{
				if (g_pod_objects[pod] != NONE && object_try_and_get_and_verify_type(g_pod_objects[pod], _object_mask_scenery))
					object_delete(g_pod_objects[pod]);
				g_pod_objects[pod] = NONE;
				/* (what's left on it falls to the ground with it gone) */
				if (host)
					pod_weapons_release(pod);
				g_pod_phase[pod] = _pod_phase_gone;
				continue;
			}
			standing = TRUE;
		}
	}

	if (host)
	{
		pod_weapons_hold(FALSE);
		pods_touch_update(pods);
	}
	pods_spent_messages(pods);

	/* (the drop's last pod gone) */
	if (!standing)
	{
		pods_delete_shown();
		if (host)
		{
			pod_weapons_hold(TRUE);
			ff_state.ordnance_pod_drop_time = NONE;
		}
	}
}

/* ---------- The Flood's Drop: Keyes' Flood form out of the floor

Scenery, which the netcode doesn't carry: each machine grows its own from the
time the host started it (flood_drop_time in Firefight's state), as the pods,
and the host lets the squad out at the burst (flood_drop_host_update). Forge's
TEST DROP of a Flood drop point shows the same on its machine alone. */

struct flood_drop_show
{
	long object_index;
	long last_tick; /* the last tick shown (-1: none yet) */
	real full_scale;
	real depth;     /* how far under the floor it starts */
	real_point3d ground;
	real facing;
	long tag; /* Keyes' Flood form's (NONE: none here) */
	/* its burst's effect and the gore it throws up (NONE: none) */
	long burst_effect;
	long puff_effect;
};

static void flood_drop_show_clear(struct flood_drop_show *show)
{
	if (show->object_index != NONE && object_try_and_get_and_verify_type(show->object_index, _object_mask_scenery))
		object_delete(show->object_index);
	show->object_index = NONE;
}

/* (the first of these loaded here, NONE for none) */
static long flood_drop_effect_find(char const *const *names, short count)
{
	short i;

	for (i = 0; i < count; i++)
	{
		long tag = tag_loaded(EFFECT_DEFINITION_TAG, names[i]);

		if (tag != NONE)
			return tag;
	}
	return NONE;
}

/* (a carrier form's pop, the Flood's own burst: c10's, d40's; made harmless) */
static char const *const g_flood_drop_burst_effects[] = {
	"characters\\floodcarrier\\effects\\body destroyed",
	"effects\\flood bit skin explode",
	"effects\\flood bit death",
	"weapons\\frag grenade\\effects\\explosion",
};
static char const *const g_flood_drop_puff_effects[] = {
	"characters\\flood_captain\\punchspray",
	"effects\\flood bit death",
};

/* (the effects: those given, else those of the names loaded here) */
static void flood_drop_show_begin(
	struct flood_drop_show *show,
	real_point3d const *ground,
	real facing,
	long tag,
	long burst_effect,
	long puff_effect)
{
	flood_drop_show_clear(show);
	show->tag = tag;
	show->burst_effect = burst_effect != NONE ? burst_effect :
		flood_drop_effect_find(g_flood_drop_burst_effects, (short)NUMBEROF(g_flood_drop_burst_effects));
	show->puff_effect = puff_effect != NONE ? puff_effect :
		flood_drop_effect_find(g_flood_drop_puff_effects, (short)NUMBEROF(g_flood_drop_puff_effects));
	show->last_tick = -1;
	show->ground = *ground;
	show->facing = facing;
	show->full_scale = 1.0f;
	show->depth = 2.0f;
}

/* an effect at a spot, harmless (as pod_effect, by its tag) */
static void flood_drop_effect(long effect_tag, real_point3d const *position)
{
	char const *marker_names[1] = { "" };
	real_point3d marker_points[1];
	real_vector3d marker_forwards[1];

	if (effect_tag == NONE)
		return;
	marker_points[0] = *position;
	marker_forwards[0] = *global_up3d;
	skulls_effect_harmless = TRUE;
	effect_new_unattached_from_markers(effect_tag, NONE, NULL, 1, marker_names, marker_points, marker_forwards,
		0.0f, 0.0f, NULL, NULL, TRUE);
	skulls_effect_harmless = FALSE;
}

/* shows the form as it is this many ticks in; FALSE once it's over */
static boolean flood_drop_show_tick(struct flood_drop_show *show, long elapsed)
{
	struct ff_flood_drop_pose pose = ff_flood_drop_pose_at((real)elapsed);
	long previous = show->last_tick;

	show->last_tick = elapsed;
	if (elapsed >= FF_FLOOD_DROP_TOTAL_TICKS)
	{
		flood_drop_show_clear(show);
		return FALSE;
	}

	if (pose.burst)
	{
		/* (only a late joiner, or a replay sought past it, misses the burst) */
		if (previous < FF_FLOOD_DROP_BURST_TICK && elapsed < FF_FLOOD_DROP_BURST_TICK + TICKS_PER_SECOND)
		{
			real_point3d middle = show->ground;
			short i;

			middle.z += 0.8f;
			flood_drop_effect(show->burst_effect, &middle);
			for (i = 0; i < 3; i++)
			{
				real_point3d bit = show->ground;
				real angle = show->facing + (real)i * (_pi * 2.0f / 3.0f);

				bit.x += cosine(angle) * 1.0f;
				bit.y += sine(angle) * 1.0f;
				bit.z += 0.4f;
				flood_drop_effect(show->puff_effect != NONE ? show->puff_effect : show->burst_effect, &bit);
			}
		}
		flood_drop_show_clear(show);
		return TRUE;
	}

	if (show->object_index == NONE && previous < 0)
	{
		long tag = show->tag;

		if (tag != NONE)
		{
			struct object_placement_data placement;

			show->full_scale = FF_FLOOD_DROP_SCALE;
			/* (from its top just under the floor to its bottom on it) */
			show->depth = FF_FLOOD_DROP_MODEL_TOP * show->full_scale;
			object_placement_data_new(&placement, tag, NONE);
			placement.position = show->ground;
			/* (made over the floor: one made under it is outside the map, and
			flood_drop_show_place gives it the floor's place from then on) */
			placement.position.z += 0.5f;
			placement.forward.i = cosine(show->facing);
			placement.forward.j = sine(show->facing);
			placement.forward.k = 0.0f;
			placement.up = *global_up3d;
			show->object_index = object_new(&placement);
			if (show->object_index != NONE)
			{
				struct object_datum *object = object_get(show->object_index);

				SET_FLAG(object->object.flags, _object_cannot_be_garbage_bit, TRUE);
				SET_FLAG(object->object.flags, _object_no_collisions_bit, TRUE);
				object_set_automatic_deactivation(show->object_index, FALSE);
			}
			if (show->object_index == NONE)
				error(_error_silent, "firefight: the Flood's drop form could not be made");
		}
		else
		{
			error(_error_silent, "firefight: no %s for the Flood's drop: its squad comes out of the floor alone",
				FF_FLOOD_DROP_TAG);
		}
	}

	/* (dirt and gore thrown up as it pushes out) */
	if (elapsed < FF_FLOOD_DROP_GROW_TICKS &&
		(previous < 0 || previous / FF_FLOOD_DROP_PUFF_TICKS != elapsed / FF_FLOOD_DROP_PUFF_TICKS))
	{
		flood_drop_effect(show->puff_effect, &show->ground);
	}

	if (show->object_index != NONE && object_try_and_get_and_verify_type(show->object_index, _object_mask_scenery))
	{
		struct object_datum *object = object_get(show->object_index);
		real yaw = show->facing + pose.turn;
		real_point3d position = show->ground;
		real_vector3d up;
		real_vector3d forward;
		real along;

		up.i = pose.lean_i;
		up.j = pose.lean_j;
		up.k = 1.0f;
		normalize3d(&up);
		forward.i = cosine(yaw);
		forward.j = sine(yaw);
		forward.k = 0.0f;
		along = forward.i * up.i + forward.j * up.j + forward.k * up.k;
		forward.i -= up.i * along;
		forward.j -= up.j * along;
		forward.k -= up.k * along;
		normalize3d(&forward);
		real scale = MAX(FF_ROUTE_SMALLEST_SCALE, show->full_scale * pose.size);
		struct location floor_location;
		real_point3d above_floor = show->ground;

		floor_location.cluster_index = NONE;

		/* (its bottom on the floor, grown; under it by its whole height, sunk) */
		position.z -= FF_FLOOD_DROP_MODEL_BOTTOM * scale + pose.sink * show->depth;

		/* (set before the move: that's what works out its nodes at this size.
		Its one animation, d20's stand unarmed idle, has its root where Keyes
		was in that level, some 130 units off: it stands in its model's own
		pose instead) */
		object->object.scale = scale;
		object->object.animation.state.index = NONE;
		SET_FLAG(object->object.flags, _object_animates_automatically_bit, FALSE);
		object_set_position(show->object_index, &position, &forward, &up);
		/* its origin is under the floor as it grows, outside the map, where it
		would be in no cluster and never drawn: it's put where the floor is */
		above_floor.z += 0.3f;
		object = object_get(show->object_index);
		if (TEST_FLAG(object->object.flags, _object_outside_of_map_bit) ||
			object->object.location.cluster_index == NONE)
		{
			scenario_location_from_point(&floor_location, &above_floor);
			/* (it must be off the map to be put back on it: objects.c asserts) */
			if (floor_location.cluster_index != NONE &&
				TEST_FLAG(object->object.flags, _object_connected_to_map_bit))
			{
				object_disconnect_from_map(show->object_index);
				object_reconnect_to_map(show->object_index, &floor_location);
			}
		}
	}
	else
	{
		show->object_index = NONE;
	}
	return TRUE;
}

/* (each machine's) the Flood drop it shows, and the drop time it's of */
static struct flood_drop_show g_flood_drop_show = { NONE, -1 };
static long g_flood_drop_shown_time = NONE;

void firefight_flood_drop_update(void)
{
	long drop_time = ff_state.flood_drop_time;

	if (drop_time == NONE || drop_time <= 0)
	{
		flood_drop_show_clear(&g_flood_drop_show);
		g_flood_drop_shown_time = NONE;
		return;
	}
	if (g_flood_drop_shown_time != drop_time)
	{
		flood_drop_show_begin(&g_flood_drop_show, &ff_state.flood_drop_position, ff_state.flood_drop_facing,
			tag_loaded('scen', FF_FLOOD_DROP_TAG), NONE, NONE);
		g_flood_drop_shown_time = drop_time;
	}
	if (g_flood_drop_show.last_tick < FF_FLOOD_DROP_TOTAL_TICKS)
		flood_drop_show_tick(&g_flood_drop_show, MAX(0, game_time_get() - drop_time));
}

/* (the HUD's) the Flood form growing now, to mark (NONE: none) */
long firefight_flood_drop_object(void)
{
	return g_flood_drop_show.object_index;
}

static void flood_drop_reset(void)
{
	/* (a game's start: the level's objects are made again) */
	g_flood_drop_show.object_index = NONE;
	g_flood_drop_show.last_tick = -1;
	g_flood_drop_shown_time = NONE;
	ff_state.flood_drop_time = NONE;
}

/* ---------- A Flood drop tested in Forge: the same, with no squad */

static struct flood_drop_show g_flood_drop_preview = { NONE, -1 };
static real g_flood_drop_preview_tick = -1.0f;

void firefight_flood_drop_preview_stop(void)
{
	flood_drop_show_clear(&g_flood_drop_preview);
	g_flood_drop_preview_tick = -1.0f;
}

boolean firefight_flood_drop_preview_start(
	real_point3d const *zone,
	real facing,
	long flood_form_tag,
	long burst_effect_tag,
	long puff_effect_tag)
{
	real_point3d ground = *zone;

	firefight_flood_drop_preview_stop();
	if (flood_form_tag == NONE)
		return FALSE;
	project_point_to_ground(&ground, 0.0f);
	flood_drop_show_begin(&g_flood_drop_preview, &ground, facing, flood_form_tag, burst_effect_tag, puff_effect_tag);
	error(_error_silent, "firefight: test flood: form %08lX, burst effect %08lX, gore effect %08lX",
		(unsigned long)flood_form_tag, (unsigned long)g_flood_drop_preview.burst_effect,
		(unsigned long)g_flood_drop_preview.puff_effect);
	g_flood_drop_preview_tick = 0.0f;
	flood_drop_show_tick(&g_flood_drop_preview, 0);
	return TRUE;
}

/* a frame of the test: TRUE while it's still going */
boolean firefight_flood_drop_preview_update(real seconds)
{
	if (g_flood_drop_preview_tick < 0.0f)
		return FALSE;
	g_flood_drop_preview_tick += seconds * (real)TICKS_PER_SECOND;
	if (!flood_drop_show_tick(&g_flood_drop_preview, (long)g_flood_drop_preview_tick))
	{
		g_flood_drop_preview_tick = -1.0f;
		return FALSE;
	}
	return TRUE;
}

/* ---------- The Ordnance Pods' Test Drop (Forge's drop area: TEST DROP)

So many pods let go round a drop area, as a drop would be, on this machine
alone and without weapons: each falls as a drop's does, lands with its blast,
and stands a while. Its pods are scenery of this machine's, as a drop's are. */

enum
{
	FF_TEST_DROP_MAX_PODS = 8,
	FF_TEST_DROP_STAY_TICKS = 12 * TICKS_PER_SECOND
};

static struct
{
	long drop_time;
	short count;
	boolean landed;
	boolean capsule;
	long objects[FF_TEST_DROP_MAX_PODS];
	real_point3d positions[FF_TEST_DROP_MAX_PODS];
	real heights[FF_TEST_DROP_MAX_PODS];
	real yaws[FF_TEST_DROP_MAX_PODS];
} g_test_drop = { NONE };

static void test_drop_pod_place(short pod, long elapsed, real_point3d *position, real_vector3d *forward,
	real_vector3d *up)
{
	real t = PIN((real)elapsed / (real)FF_POD_FALL_TICKS, 0.0f, 1.0f);

	*position = g_test_drop.positions[pod];
	position->z += g_test_drop.heights[pod] * (1.0f - t * t);
	if (g_test_drop.capsule)
	{
		position->z += FF_POD_CAPSULE_BASE;
		forward->i = 0.0f;
		forward->j = 0.0f;
		forward->k = -1.0f;
		vector3d_from_angle(up, g_test_drop.yaws[pod]);
	}
	else
	{
		vector3d_from_angle(forward, g_test_drop.yaws[pod]);
		*up = *global_up3d;
	}
}

void firefight_ordnance_preview_refusals(char *text, long size)
{
	static char const *const names[NUMBER_OF_POD_SPOT_REFUSALS] = {
		"no ground", "object under", "slope", "outside map", "unseen", "cramped", "roofed", "crowded", "too near",
		"enemies", "kill volume"
	};
	short reason;

	if (!text || size <= 0)
		return;
	text[0] = 0;
	for (reason = 0; reason < NUMBER_OF_POD_SPOT_REFUSALS; reason++)
	{
		if (g_pod_spot_refusals[reason])
		{
			snprintf(text + strlen(text), (size_t)size - strlen(text), "%s%s %ld", text[0] ? ", " : "", names[reason],
				g_pod_spot_refusals[reason]);
		}
	}
	if (g_pod_spot_blocker != NONE)
	{
		char const *name = tag_get_name(g_pod_spot_blocker);

		snprintf(text + strlen(text), (size_t)size - strlen(text), " (first object: %s)", name ? name : "?");
	}
	if (!text[0])
		snprintf(text, (size_t)size, "none");
}

void firefight_ordnance_preview_stop(void)
{
	short pod;

	for (pod = 0; pod < g_test_drop.count; pod++)
	{
		if (g_test_drop.objects[pod] != NONE &&
			object_try_and_get_and_verify_type(g_test_drop.objects[pod], _object_mask_scenery))
			object_delete(g_test_drop.objects[pod]);
		g_test_drop.objects[pod] = NONE;
	}
	g_test_drop.count = 0;
	g_test_drop.drop_time = NONE;
}

short firefight_ordnance_preview_start(
	real_point3d const *area,
	long area_object,
	short count)
{
	long pod_tag;
	short pod;

	firefight_ordnance_preview_stop();
	csmemset(g_pod_spot_refusals, 0, sizeof(g_pod_spot_refusals));
	g_pod_spot_blocker = NONE;
	pod_tag = pod_tag_get(&g_test_drop.capsule);
	if (pod_tag == NONE || !area)
	{
		error(_error_silent, "firefight: test drop: no pod tag (%s nor %s) is loaded", FF_POD_TAG, FF_POD_FALLBACK_TAG);
		return 0;
	}
	pods_shape_choose();
	count = (short)PIN(count, 1, FF_TEST_DROP_MAX_PODS);
	for (pod = 0; pod < count; pod++)
	{
		struct collision_result collision;
		real_point3d ray_start;
		real_vector3d ray_up = { 0.0f, 0.0f, FF_POD_FALL_HEIGHT };
		real_point3d *spot = &g_test_drop.positions[g_test_drop.count];

		boolean found;

		g_pod_spot_ignored = area_object;
		found = ordnance_pod_spot(area, NONE, TRUE, g_test_drop.positions, g_test_drop.count, spot);
		g_pod_spot_ignored = NONE;
		if (!found)
			break;
		g_test_drop.heights[g_test_drop.count] = FF_POD_FALL_HEIGHT;
		ray_start = *spot;
		ray_start.z += 1.0f;
		if (collision_test_vector(FLAG(_collision_test_structure_bit), &ray_start, &ray_up, NONE, &collision))
			g_test_drop.heights[g_test_drop.count] = MAX(3.0f, collision.t * FF_POD_FALL_HEIGHT - 1.5f);
		g_test_drop.yaws[g_test_drop.count] = (real)((unsigned long)(game_time_get() + pod * 977) * 2654435761UL %
			6283UL) / 1000.0f;
		g_test_drop.objects[g_test_drop.count] = NONE;
		g_test_drop.count++;
	}
	{
		char reasons[256];

		firefight_ordnance_preview_refusals(reasons, sizeof(reasons));
		error(_error_silent, "firefight: test drop at (%.2f, %.2f, %.2f): %d of %d pods (%s) found room; refused: %s",
			area->x, area->y, area->z, g_test_drop.count, count, g_test_drop.capsule ? "capsule" : "crate", reasons);
	}
	g_test_drop.drop_time = game_time_get();
	g_test_drop.landed = FALSE;
	for (pod = 0; pod < g_test_drop.count; pod++)
	{
		struct object_placement_data placement;

		object_placement_data_new(&placement, pod_tag, NONE);
		test_drop_pod_place(pod, 0, &placement.position, &placement.forward, &placement.up);
		g_test_drop.objects[pod] = object_new(&placement);
		if (g_test_drop.objects[pod] != NONE)
		{
			SET_FLAG(object_get(g_test_drop.objects[pod])->object.flags, _object_cannot_be_garbage_bit, TRUE);
			object_set_automatic_deactivation(g_test_drop.objects[pod], FALSE);
		}
		pod_effect("effects\\coop teleport", &g_test_drop.positions[pod]);
	}
	if (!g_test_drop.count)
		g_test_drop.drop_time = NONE;
	return g_test_drop.count;
}

short firefight_ordnance_preview_update(void)
{
	long elapsed;
	short pod;

	if (g_test_drop.drop_time == NONE)
		return 0;
	elapsed = game_time_get() - g_test_drop.drop_time;
	if (elapsed < 0 || elapsed >= FF_POD_FALL_TICKS + FF_TEST_DROP_STAY_TICKS)
	{
		firefight_ordnance_preview_stop();
		return 0;
	}
	for (pod = 0; pod < g_test_drop.count; pod++)
	{
		real_point3d position;
		real_vector3d forward;
		real_vector3d up;

		if (g_test_drop.objects[pod] == NONE ||
			!object_try_and_get_and_verify_type(g_test_drop.objects[pod], _object_mask_scenery))
			continue;
		if (elapsed > FF_POD_FALL_TICKS && g_test_drop.landed)
			continue;
		test_drop_pod_place(pod, elapsed, &position, &forward, &up);
		object_set_position(g_test_drop.objects[pod], &position, &forward, &up);
	}
	if (elapsed >= FF_POD_FALL_TICKS && !g_test_drop.landed)
	{
		g_test_drop.landed = TRUE;
		for (pod = 0; pod < g_test_drop.count; pod++)
		{
			pod_effect("weapons\\frag grenade\\effects\\explosion", &g_test_drop.positions[pod]);
			pod_effect("effects\\coop teleport", &g_test_drop.positions[pod]);
		}
	}
	return elapsed < FF_POD_FALL_TICKS ? 1 : 2;
}

/* ---------- Friendly AI Reinforcements */

void firefight_spawn_friendly_reinforcements(short count)
{
	real_point3d player_positions[16];
	short player_count = get_living_player_positions(player_positions, NULL, 16);
	real_point3d anchor;
	short living_friendlies = 0;
	long candidate_variants[MAX_ROSTER_VARIANTS];
	short candidate_count = 0;
	short i;

	if (count <= 0 || player_count == 0)
		return;

	for (i = 0; i < g_friendly_unit_count; i++)
	{
		if (g_friendly_units[i] != NONE)
		{
			struct unit_datum *u = (struct unit_datum *)object_try_and_get_and_verify_type(
				g_friendly_units[i], _object_mask_unit);
			if (u && !TEST_FLAG(u->object.damage_flags, _object_dead_bit))
				living_friendlies++;
			else
				g_friendly_units[i] = NONE;
		}
	}

	if (living_friendlies >= count)
		return;

	for (i = 0; i < g_roster_count; i++)
	{
		if (ff_state.elite_player_mode)
		{
			if (!g_roster[i].is_human_ally_or_enemy &&
				g_roster[i].faction == _firefight_faction_covenant)
			{
				candidate_variants[candidate_count++] = g_roster[i].variant_tag_index;
			}
		}
		else
		{
			if (g_roster[i].is_human_ally_or_enemy)
				candidate_variants[candidate_count++] = g_roster[i].variant_tag_index;
		}
	}

	if (candidate_count == 0)
		return;

	/* port: by a player on foot, else on the floor under them, not at a
	pilot's height (they fell from the aircraft's); none, a spawn point */
	{
		real facing = 0.0f;

		if (!players_floor_anchor(player_positions, NULL, player_count, 0, &anchor, NULL) &&
			!firefight_select_player_spawn_point(0, &anchor, &facing))
		{
			return;
		}
	}

	for (i = living_friendlies; i < count; i++)
	{
		long variant = candidate_variants[spawner_rand() % (unsigned long)candidate_count];
		real_point3d spawn_pt = anchor;
		real_point3d ground;
		short ally_team = ff_state.elite_player_mode ? _game_team_covenant : _game_team_human;
		long unit_index;

		spawn_pt.x += spawner_rand_range(-3.5f, 3.5f);
		spawn_pt.y += spawner_rand_range(-3.5f, 3.5f);
		/* (on the ground there, this side of any wall; else where the anchor is) */
		if (spot_reachable_from(&anchor, &spawn_pt) && reach_ground(&spawn_pt, &ground) != NONE)
			spawn_pt = ground;
		else
			spawn_pt = anchor;
		unit_index = spawn_single_actor_at(variant, _ff_tier_infantry, &spawn_pt, 0.0f, ally_team, FALSE);
		if (unit_index != NONE && g_friendly_unit_count < MAX_FRIENDLY_UNITS)
		{
			g_friendly_units[g_friendly_unit_count++] = unit_index;
		}
	}
}

/* an ally kept with the nearest player, as Forge's FOLLOW: sent near them
(a spot 2 to 3.5 units off, on the ground) once more than FF_FOLLOW_IDLE away
while nothing is in its sight, or FF_FOLLOW_ALWAYS away whatever it is doing;
never charging off after what it sees. Whether it was sent */
#define FF_FOLLOW_IDLE 5.0f
#define FF_FOLLOW_ALWAYS 10.0f
boolean firefight_objective_follow(long unit_index)
{
	struct unit_datum *unit = unit_index != NONE ?
		(struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit) : NULL;
	struct actor_datum *actor;
	real_point3d positions[16];
	short count;
	real_point3d at;
	real best = 1.0e9f;
	short nearest = NONE;
	short i;

	if (!unit || TEST_FLAG(unit->object.damage_flags, _object_dead_bit) || unit->unit.actor_index == NONE ||
		(actor = actor_try_and_get(unit->unit.actor_index)) == NULL)
	{
		return FALSE;
	}
	actor->external_orders.disable_charging = TRUE;
	actor->emotions.forced_to_charge = FALSE;
	count = get_living_player_positions(positions, NULL, 16);
	object_get_origin(unit_index, &at);
	for (i = 0; i < count; i++)
	{
		real distance = distance3d(&at, &positions[i]);

		if (distance < best)
		{
			best = distance;
			nearest = i;
		}
	}
	if (nearest == NONE || best <= FF_FOLLOW_IDLE ||
		(best <= FF_FOLLOW_ALWAYS && actor->target.target_prop_index != NONE))
	{
		return FALSE;
	}
	{
		real angle = spawner_rand_range(0.0f, 2.0f * _pi);
		real distance = spawner_rand_range(2.0f, 3.5f);
		real_point3d spot = positions[nearest];
		real_point3d ground;
		long surface;

		spot.x += cosine(angle) * distance;
		spot.y += sine(angle) * distance;
		if (!spot_reachable_from(&positions[nearest], &spot))
			spot = positions[nearest];
		surface = reach_ground(&spot, &ground);
		if (surface == NONE)
			surface = reach_ground(&positions[nearest], &ground);
		if (surface == NONE)
			return FALSE;
		actor_move_to_point(unit->unit.actor_index, &ground, surface, NONE);
	}
	return TRUE;
}

/* (firefight_objectives.c's Rescue, its round over) a marine of an
objective's made one of the friendly reinforcements, following the players */
void firefight_adopt_friendly(long unit_index)
{
	short i;

	if (unit_index == NONE)
		return;
	for (i = 0; i < g_friendly_unit_count; i++)
	{
		if (g_friendly_units[i] == unit_index)
			return;
		if (g_friendly_units[i] == NONE)
		{
			g_friendly_units[i] = unit_index;
			return;
		}
	}
	if (g_friendly_unit_count < MAX_FRIENDLY_UNITS)
		g_friendly_units[g_friendly_unit_count++] = unit_index;
}

/* (the host's) the friendly reinforcements with the players, so often */
#define FF_FOLLOW_TICKS 20
static void friendlies_follow_update(void)
{
	short i;

	if (!g_friendly_unit_count || game_time_get() % FF_FOLLOW_TICKS)
		return;
	for (i = 0; i < g_friendly_unit_count; i++)
	{
		if (g_friendly_units[i] != NONE)
			firefight_objective_follow(g_friendly_units[i]);
	}
}

/* ---------- Round-Scheduled Player Vehicle Spawning & End-of-Round Despawning (Feature 7) */

/* a vehicle placement (custom_edition_objects.c has the fields past the object) */
struct ff_scenario_vehicle
{
	struct scenario_object_datum object;
	byte unused[0x50];
};

typedef char ff_scenario_vehicle_size_assert[
	sizeof(struct ff_scenario_vehicle) == 0x78 ? 1 : -1];

/* a placement players could use: not a campaign level's flyers, scripted
props and turrets, whose spots are in the air or on a wall */
static boolean map_vehicle_tag_drivable(char const *name)
{
	static char const *const unusable[] = {
		"pelican", "dropship", "lifepod", "fighterbomber", "turret", "gun tower",
		"falling_box", "cinematic", "playback", "cryotube", "chair"
	};
	short i;

	if (!name)
		return FALSE;
	for (i = 0; i < (short)NUMBEROF(unusable); i++)
	{
		if (strstr(name, unusable[i]))
			return FALSE;
	}
	return TRUE;
}

static boolean map_vehicle_spot_clear(real_point3d const *position)
{
	struct object_iterator iterator;
	struct object_datum *object;

	object_iterator_new(&iterator, _object_mask_vehicle, 0);
	while ((object = (struct object_datum *)object_iterator_next(&iterator)) != NULL)
	{
		if (distance3d(&object->object.position, position) < 4.0f)
			return FALSE;
	}
	return TRUE;
}

/* the free spot of the map's own vehicle placements nearest the players, one
of this vehicle's if it has any. Forge's placed vehicles are among them, and a
close quarters map has none: FALSE, and it goes without */
static boolean map_vehicle_spot(long vehicle_tag, real_point3d *out_position, real *out_facing)
{
	struct scenario *scen = global_scenario_try_and_get();
	real_point3d player_positions[16];
	short player_count = get_living_human_player_positions(player_positions, NULL, 16);
	real_point3d anchor;
	short pass;

	if (!scen)
		return FALSE;
	if (player_count > 0)
	{
		anchor = player_positions[central_player_index(player_positions, player_count)];
	}
	else
	{
		real facing = 0.0f;

		if (!firefight_select_player_spawn_point(0, &anchor, &facing))
			return FALSE;
	}

	for (pass = 0; pass < 2; pass++)
	{
		real best_distance = 0.0f;
		boolean found = FALSE;
		short i;

		for (i = 0; i < scen->vehicles.count; i++)
		{
			struct ff_scenario_vehicle *placement = TAG_BLOCK_GET_ELEMENT(
				&scen->vehicles, i, struct ff_scenario_vehicle);
			short palette_index = placement->object.palette_entry_index;
			struct scenario_object_palette_entry *palette;
			real distance;

			if (!VALID_INDEX(palette_index, scen->vehicle_palette.count))
				continue;
			palette = TAG_BLOCK_GET_ELEMENT(
				&scen->vehicle_palette, palette_index, struct scenario_object_palette_entry);
			if (palette->reference.index == NONE ||
				(pass == 0 && palette->reference.index != vehicle_tag) ||
				!map_vehicle_tag_drivable(tag_get_name(palette->reference.index)) ||
				!point_is_in_valid_bsp(&placement->object.position) ||
				!map_vehicle_spot_clear(&placement->object.position))
			{
				continue;
			}
			distance = distance3d(&placement->object.position, &anchor);
			if (!found || distance < best_distance)
			{
				found = TRUE;
				best_distance = distance;
				*out_position = placement->object.position;
				*out_facing = placement->object.rotation.yaw;
			}
		}
		if (found)
			return TRUE;
	}
	return FALSE;
}

/* ---------- MAP CONTENT: the map's own enemies, weapons and vehicles */

/* a tier for one of the map's AI the roster has no place for, by its words */
static byte map_enemy_tier(char const *variant_name, char const *unit_name)
{
	static char const *const boss_words[] = { "hunter", "zealot", "boss", "ultra", "chieftain", "wraith", NULL };
	static char const *const commander_words[] = {
		"elite", "major", "commander", "carrier", "odst", "brute", "captain", "honor", "spec", NULL
	};
	static char const *const fodder_words[] = { "grunt", "infection", "crew", "drone", "bugger", "pup", NULL };
	char lowered[512];

	lowercase_copy(variant_name ? variant_name : "", lowered, 256);
	if (unit_name)
	{
		size_t used = strlen(lowered);

		lowered[used++] = ' ';
		lowercase_copy(unit_name, lowered + used, sizeof(lowered) - used);
	}
	if (name_has_word(lowered, boss_words))
		return _ff_tier_boss_heavy;
	if (name_has_word(lowered, commander_words))
		return _ff_tier_commander;
	if (name_has_word(lowered, fodder_words))
		return _ff_tier_fodder;
	return _ff_tier_infantry;
}

/* whether a weapon is a power weapon for a heavy drop, by its words */
static boolean map_weapon_power(char const *lowered)
{
	static char const *const power_words[] = {
		"rocket", "fuel", "sword", "shotgun", "launcher", "cannon", "hammer", "flame", "laser", "sniper",
		"mortar", "splaser", "railgun", "spartan", NULL
	};

	return name_has_word(lowered, power_words);
}

/* a map weapon a heavy drop's (a power weapon) by its damage: its
projectiles explode (detonation damage FF_HEAVY_EXPLOSION_DAMAGE or more,
FF_HEAVY_EXPLOSION_RADIUS across or wider), or a trigger pull does
FF_HEAVY_SHOT_DAMAGE or more (its projectiles' impact or detonation damage,
all of a shot's: a shotgun's pellets); one with no damage to read (a melee
weapon's) by its words. Each written to the log, to tune these by */
#define FF_HEAVY_SHOT_DAMAGE 70.0f
#define FF_HEAVY_EXPLOSION_DAMAGE 30.0f
#define FF_HEAVY_EXPLOSION_RADIUS 1.5f

static real damage_effect_upper(long damage_tag, real *radius)
{
	struct damage_effect_definition const *effect;

	if (damage_tag == NONE || !tag_index_is_group(damage_tag, DAMAGE_EFFECT_DEFINITION_TAG))
		return 0.0f;
	effect = (struct damage_effect_definition const *)tag_get(DAMAGE_EFFECT_DEFINITION_TAG, damage_tag);
	if (!effect || !(effect->damage.damage_upper_bound >= 0.0f) || effect->damage.damage_upper_bound > 100000.0f)
		return 0.0f;
	if (radius)
		*radius = effect->cutoff_radius;
	return effect->damage.damage_upper_bound;
}

static boolean map_weapon_heavy(long weapon_tag, char const *lowered)
{
	struct weapon_definition const *definition = weapon_definition_get(weapon_tag);
	real shot_damage = 0.0f;
	boolean explosive = FALSE;
	boolean heavy;
	long index;

	for (index = 0; definition && index < definition->weapon.triggers.count; index++)
	{
		struct weapon_trigger_definition const *trigger = TAG_BLOCK_GET_ELEMENT(&definition->weapon.triggers, index,
			struct weapon_trigger_definition);
		struct projectile_definition const *projectile;
		real radius = 0.0f;
		real impact;
		real detonation;

		if (trigger->projectile.index == NONE || !tag_index_is_group(trigger->projectile.index, PROJECTILE_DEFINITION_TAG))
			continue;
		projectile = projectile_definition_get(trigger->projectile.index);
		if (!projectile)
			continue;
		impact = damage_effect_upper(projectile->projectile.impact_damage.index, NULL);
		detonation = damage_effect_upper(projectile->projectile.attached_detonation_damage.index, &radius);
		if (detonation >= FF_HEAVY_EXPLOSION_DAMAGE && radius >= FF_HEAVY_EXPLOSION_RADIUS)
			explosive = TRUE;
		shot_damage = MAX(shot_damage, MAX(impact, detonation) * (real)MAX(1, trigger->projectiles_per_shot));
	}
	heavy = explosive || shot_damage >= FF_HEAVY_SHOT_DAMAGE || (shot_damage <= 0.0f && map_weapon_power(lowered));
	error(_error_silent, "firefight: map weapon %s: %.0f a shot%s: %s", tag_get_name(weapon_tag), shot_damage,
		explosive ? ", explosive" : "", heavy ? "HEAVY" : "regular");
	return heavy;
}

/* whether a vehicle is one of the game's own, as stock Halo has them */
static boolean vehicle_tag_stock(char const *lowered)
{
	static char const *const stock_prefixes[] = {
		"vehicles\\warthog\\", "vehicles\\rwarthog\\", "vehicles\\scorpion\\", "vehicles\\ghost\\",
		"vehicles\\banshee\\", "vehicles\\wraith\\", "vehicles\\pelican\\", "vehicles\\c_dropship\\",
		"vehicles\\c gun turret\\", "vehicles\\c_gun_turret\\", "vehicles\\lifepod", NULL
	};
	short i;

	for (i = 0; stock_prefixes[i]; i++)
	{
		if (!strncmp(lowered, stock_prefixes[i], strlen(stock_prefixes[i])))
			return TRUE;
	}
	return FALSE;
}

/* whether a weapon is one of the game's own: the campaign's (the
randomizer's pack has it) or stock multiplayer's */
static boolean weapon_tag_stock(char const *name, char const *lowered)
{
	return randomizer_stock_tag_exists(WEAPON_DEFINITION_TAG, name) ||
		(!strncmp(lowered, "weapons\\", 8) && (strstr(lowered, "mp_") || strstr(lowered, "weapons\\pistol\\") ||
			strstr(lowered, "weapons\\assault rifle\\") || strstr(lowered, "weapons\\shotgun\\") ||
			strstr(lowered, "weapons\\sniper rifle\\") || strstr(lowered, "weapons\\rocket launcher\\") ||
			strstr(lowered, "weapons\\plasma") || strstr(lowered, "weapons\\needler\\") ||
			strstr(lowered, "weapons\\flamethrower\\") || strstr(lowered, "weapons\\fuel rod") ||
			strstr(lowered, "weapons\\frag grenade\\") || strstr(lowered, "weapons\\energy sword\\") ||
			strstr(lowered, "weapons\\ball\\") || strstr(lowered, "weapons\\flag\\") ||
			strstr(lowered, "weapons\\gravity rifle\\")));
}

/* (by its name: a weapon players can carry, not a vehicle's, a flag or
ball, nor one only the campaign's AI or scripts hold) */
static boolean map_weapon_name_usable(char const *lowered)
{
	return !(strstr(lowered, "vehicles\\") || strstr(lowered, "flag") || strstr(lowered, "ball") ||
		strstr(lowered, "gravity") || strstr(lowered, "sentinel") || strstr(lowered, "gun turret") ||
		strstr(lowered, "cinematic") || strstr(lowered, "turret") || strstr(lowered, "energy sword\\energy sword") ||
		strstr(lowered, "needler\\needler"));
}

/* whether a weapon has what a player's or an enemy's needs (as the Tag
Injector's needs list has it: tag_injector.c's preview_needs): its model and
collision (lying on the ground), its first-person model and animations, its
HUD, and a projectile a trigger fires (none at all: a melee weapon's, by its
name) */
static boolean map_weapon_complete(struct weapon_definition const *definition, char const *lowered)
{
	struct weapon_interface_definition const *interface = &definition->weapon.interface_definition;
	boolean fires = FALSE;
	long index;

	if (definition->object.model.index == NONE || definition->object.collision_model.index == NONE ||
		interface->first_person_model.index == NONE || interface->first_person_animations.index == NONE ||
		interface->hud_interface.index == NONE)
	{
		return FALSE;
	}
	for (index = 0; index < definition->weapon.triggers.count; index++)
	{
		struct weapon_trigger_definition const *trigger = TAG_BLOCK_GET_ELEMENT(&definition->weapon.triggers, index,
			struct weapon_trigger_definition);

		if (trigger->projectile.index != NONE)
			fires = TRUE;
	}
	return fires || strstr(lowered, "sword") || strstr(lowered, "hammer") || strstr(lowered, "melee");
}

/* whether a weapon is a vehicle's alone: a vehicle starts with it, and no
biped nor any actor variant has it */
static boolean map_weapon_vehicles_only(long weapon_tag)
{
	struct tag_iterator iter;
	long tag_index;
	boolean vehicles = FALSE;
	long index;

	tag_iterator_new(&iter, VEHICLE_DEFINITION_TAG);
	while (!vehicles && (tag_index = tag_iterator_next(&iter)) != NONE)
	{
		struct unit_definition const *unit = (struct unit_definition const *)tag_get(VEHICLE_DEFINITION_TAG, tag_index);

		for (index = 0; unit && index < unit->unit.initial_weapons.count && !vehicles; index++)
		{
			/* (a unit's initial weapon: its reference first, 0x24 bytes) */
			struct tag_reference const *reference = (struct tag_reference const *)((byte const *)
				(byte const *)xbox_pointer(unit->unit.initial_weapons.address) + index * 0x24);

			vehicles = reference->index == weapon_tag;
		}
	}
	if (!vehicles)
		return FALSE;
	tag_iterator_new(&iter, BIPED_DEFINITION_TAG);
	while ((tag_index = tag_iterator_next(&iter)) != NONE)
	{
		struct unit_definition const *unit = (struct unit_definition const *)tag_get(BIPED_DEFINITION_TAG, tag_index);

		for (index = 0; unit && index < unit->unit.initial_weapons.count; index++)
		{
			struct tag_reference const *reference = (struct tag_reference const *)((byte const *)
				(byte const *)xbox_pointer(unit->unit.initial_weapons.address) + index * 0x24);

			if (reference->index == weapon_tag)
				return FALSE;
		}
	}
	tag_iterator_new(&iter, ACTOR_VARIANT_DEFINITION_TAG);
	while ((tag_index = tag_iterator_next(&iter)) != NONE)
	{
		struct actor_variant_definition const *variant = actor_variant_definition_get(tag_index);

		if (variant && variant->ranged_combat.reference.index == weapon_tag &&
			variant->unit_reference.index != NONE && tag_index_is_group(variant->unit_reference.index, BIPED_DEFINITION_TAG))
		{
			return FALSE;
		}
	}
	return TRUE;
}

static boolean map_weapon_usable(long weapon_tag, char const *lowered)
{
	struct weapon_definition *definition = weapon_definition_get(weapon_tag);

	return definition && !((definition->weapon.flags >> 3) & 1) && map_weapon_name_usable(lowered) &&
		map_weapon_complete(definition, lowered) && !map_weapon_vehicles_only(weapon_tag);
}

/* (by its name: an actor variant that fights, not the cinematics' nor the
players' allies) */
static boolean map_enemy_name_fights(char const *lowered)
{
	return !(strstr(lowered, "cinematic") || strstr(lowered, "cortana") || strstr(lowered, "keyes") ||
		strstr(lowered, "monitor") || strstr(lowered, "captain") || strstr(lowered, "marine") ||
		strstr(lowered, "crewman") || strstr(lowered, "pilot"));
}

static boolean map_content_custom_off(byte custom_bit, short kind, long tag_index);
static boolean map_content_custom_on_by_hand(byte custom_bit, short kind, long tag_index);

static void scan_map_content(void)
{
	byte content = firefight_map_content();
	byte enemies = map_content_of(FIREFIGHT_MAP_CONTENT_ENEMIES_OFF);
	struct tag_iterator iter;
	long tag_index;
	short native_roster = 0;
	short i;

	ff_map_enemy_count = 0;
	ff_map_weapon_count = 0;
	ff_map_vehicle_count = 0;
	g_roster_map_only = FALSE;
	if (content == _firefight_map_content_vanilla)
		return;

	/* the map's enemies: its own of the roster's kinds are its entries; the
	rest here, if they fight (not the cinematics', nor allies') */
	for (i = 0; i < g_roster_count; i++)
	{
		g_roster[i].custom_off = g_roster[i].maps_own && enemies != _firefight_map_content_vanilla &&
			map_content_custom_off(FIREFIGHT_MAP_CONTENT_ENEMIES_CUSTOM, _firefight_map_content_kind_enemies,
				g_roster[i].variant_tag_index);
		if (g_roster[i].maps_own && !g_roster[i].custom_off)
			native_roster++;
	}
	tag_iterator_new(&iter, ACTOR_VARIANT_DEFINITION_TAG);
	while (enemies != _firefight_map_content_vanilla &&
		(tag_index = tag_iterator_next(&iter)) != NONE && ff_map_enemy_count < MAX_ROSTER_VARIANTS)
	{
		struct actor_variant_definition *variant = actor_variant_definition_get(tag_index);
		struct unit_definition *unit;
		char lowered[256];
		boolean listed = FALSE;

		if (!tag_is_maps_own(tag_index) || !variant || variant->unit_reference.index == NONE ||
			variant->actor_reference.index == NONE || !tag_index_is_group(variant->unit_reference.index, BIPED_DEFINITION_TAG))
		{
			continue;
		}
		for (i = 0; i < g_roster_count && !listed; i++)
			listed = g_roster[i].variant_tag_index == tag_index;
		lowercase_copy(tag_get_name(tag_index), lowered, sizeof(lowered));
		if (listed || !map_enemy_name_fights(lowered) ||
			map_content_custom_off(FIREFIGHT_MAP_CONTENT_ENEMIES_CUSTOM, _firefight_map_content_kind_enemies, tag_index))
		{
			continue;
		}
		unit = (struct unit_definition *)tag_get(BIPED_DEFINITION_TAG, variant->unit_reference.index);
		if (!unit || unit->unit.default_team == _game_team_player || unit->unit.default_team == _game_team_human)
			continue;
		ff_map_enemy_tags[ff_map_enemy_count] = tag_index;
		ff_map_enemy_tiers[ff_map_enemy_count++] = map_enemy_tier(tag_get_name(tag_index),
			tag_get_name(variant->unit_reference.index));
	}
	/* (INCLUDE MAP: those beside the game's; ONLY MAP: its own alone, if it
	has any to fight) */
	g_roster_map_only = enemies == _firefight_map_content_only_map && (native_roster > 0 || ff_map_enemy_count > 0);

	tag_iterator_new(&iter, WEAPON_DEFINITION_TAG);
	while (map_content_of(FIREFIGHT_MAP_CONTENT_WEAPONS_OFF) != _firefight_map_content_vanilla &&
		(tag_index = tag_iterator_next(&iter)) != NONE && ff_map_weapon_count < MAX_ROSTER_VARIANTS)
	{
		char lowered[256];

		if (!tag_is_maps_own(tag_index))
			continue;
		lowercase_copy(tag_get_name(tag_index), lowered, sizeof(lowered));
		/* (one that is not usable alone where CUSTOM has it turned on by hand;
		those of vehicles and the like are kept for them all the same) */
		if (!map_weapon_name_usable(lowered) || firefight_weapon_is_objective(tag_index, "MAP CONTENT") ||
			(!map_weapon_usable(tag_index, lowered) &&
				!map_content_custom_on_by_hand(FIREFIGHT_MAP_CONTENT_WEAPONS_CUSTOM, _firefight_map_content_kind_weapons,
					tag_index)) ||
			map_content_custom_off(FIREFIGHT_MAP_CONTENT_WEAPONS_CUSTOM, _firefight_map_content_kind_weapons, tag_index))
		{
			if (map_weapon_name_usable(lowered) && !map_weapon_usable(tag_index, lowered))
				error(_error_silent, "firefight: MAP CONTENT leaves %s out (not a weapon players or enemies can use)",
					tag_get_name(tag_index));
			continue;
		}
		ff_map_weapon_tags[ff_map_weapon_count] = tag_index;
		ff_map_weapon_power[ff_map_weapon_count] = map_weapon_heavy(tag_index, lowered);
		ff_map_weapon_stock[ff_map_weapon_count++] = weapon_tag_stock(tag_get_name(tag_index), lowered);
	}

	tag_iterator_new(&iter, VEHICLE_DEFINITION_TAG);
	while (map_content_of(FIREFIGHT_MAP_CONTENT_VEHICLES_OFF) != _firefight_map_content_vanilla &&
		(tag_index = tag_iterator_next(&iter)) != NONE && ff_map_vehicle_count < MAX_MAP_VEHICLES)
	{
		struct ff_vehicle_ai_tag_layout *vehicle = (struct ff_vehicle_ai_tag_layout *)tag_get(VEHICLE_DEFINITION_TAG, tag_index);
		char lowered[256];

		if (!tag_is_maps_own(tag_index) || !vehicle || vehicle->unit.unit.seats.count <= 0 ||
			vehicle->type == FF_VEHICLE_TYPE_TURRET)
		{
			continue;
		}
		lowercase_copy(tag_get_name(tag_index), lowered, sizeof(lowered));
		if (!map_vehicle_tag_drivable(lowered) ||
			map_content_custom_off(FIREFIGHT_MAP_CONTENT_VEHICLES_CUSTOM, _firefight_map_content_kind_vehicles, tag_index))
		{
			continue;
		}
		ff_map_vehicle_tags[ff_map_vehicle_count] = tag_index;
		ff_map_vehicle_stock[ff_map_vehicle_count++] = vehicle_tag_stock(lowered);
	}
	error(_error_silent, "firefight: MAP CONTENT %s: the map's own %d enemies (and %d of the roster's), %d weapons, "
		"%d vehicles%s", content == _firefight_map_content_only_map ? "ONLY MAP" : "INCLUDE MAP", ff_map_enemy_count,
		native_roster, ff_map_weapon_count, ff_map_vehicle_count,
		content == _firefight_map_content_only_map && !g_roster_map_only ? " (no enemies of its own: the game's)" : "");
}

/* ---- CUSTOM's lists: a map's own of each kind, by their names (read from
its file without loading it), and the host's choices of them, per map */

#define FF_MC_LIST_MAX 128
#define FF_MC_NAME_SIZE 128
#define FF_MC_SAVED_MAX 384
#define FF_MC_FILE "d:\\firefight_map_content.txt"

static char const ff_mc_kind_letters[NUMBER_OF_FIREFIGHT_MAP_CONTENT_KINDS] = { 'e', 'w', 'v' };

static struct
{
	char map[128];
	byte kinds;
	short counts[NUMBER_OF_FIREFIGHT_MAP_CONTENT_KINDS];
	char names[NUMBER_OF_FIREFIGHT_MAP_CONTENT_KINDS][FF_MC_LIST_MAX][FF_MC_NAME_SIZE];
	/* (those found not usable, by firefight_map_content_check: off unless
	turned on by hand) */
	boolean unusable[NUMBER_OF_FIREFIGHT_MAP_CONTENT_KINDS][FF_MC_LIST_MAX];
	boolean checked;
} g_mc_list;

/* (the saved file's section for one map: its byte, the tags turned off) */
static struct
{
	char map[96];
	boolean known;
	byte content;
	short count;
	char kinds[FF_MC_SAVED_MAX];
	char names[FF_MC_SAVED_MAX][FF_MC_NAME_SIZE];
} g_mc_saved;

/* a map's name as the file keys it: no folder, lowercase, no Forge '@' part */
static void map_content_stem(char const *map_name, char *stem, size_t size)
{
	char const *slash;
	char *cut;

	map_name = map_name ? map_name : "";
	if ((slash = strrchr(map_name, '\\')) != NULL)
		map_name = slash + 1;
	if ((slash = strrchr(map_name, '/')) != NULL)
		map_name = slash + 1;
	lowercase_copy(map_name, stem, size);
	if ((cut = strchr(stem, '@')) != NULL)
		*cut = 0;
	if ((cut = strstr(stem, ".map")) != NULL && !cut[4])
		*cut = 0;
}

/* (firefight_map_content_kinds' walk of a map's tags) */
static int map_content_kinds_visit(void *context, unsigned long group_tag, char const *name)
{
	short kind = NONE;
	char lowered[256];

	(void)context;
	lowercase_copy(name, lowered, sizeof(lowered));
	if (group_tag == ACTOR_VARIANT_DEFINITION_TAG && map_enemy_name_fights(lowered) &&
		!randomizer_stock_tag_exists(ACTOR_VARIANT_DEFINITION_TAG, name))
	{
		kind = _firefight_map_content_kind_enemies;
	}
	else if (group_tag == WEAPON_DEFINITION_TAG && map_weapon_name_usable(lowered) && !weapon_tag_stock(name, lowered))
		kind = _firefight_map_content_kind_weapons;
	else if (group_tag == VEHICLE_DEFINITION_TAG && map_vehicle_tag_drivable(lowered) && !strstr(lowered, "turret") &&
		!vehicle_tag_stock(lowered))
	{
		kind = _firefight_map_content_kind_vehicles;
	}
	if (kind == NONE)
		return TRUE;
	g_mc_list.kinds |= (byte)(1 << kind);
	if (g_mc_list.counts[kind] < FF_MC_LIST_MAX)
		snprintf(g_mc_list.names[kind][g_mc_list.counts[kind]++], FF_MC_NAME_SIZE, "%s", name);
	return TRUE;
}

/* (the map's lists, read once for each map asked of) */
static void map_content_list_read(char const *map_name)
{
	char const *stem;

	if (!map_name || !map_name[0] || !strcmp(g_mc_list.map, map_name))
		return;
	memset(g_mc_list.counts, 0, sizeof(g_mc_list.counts));
	memset(g_mc_list.unusable, 0, sizeof(g_mc_list.unusable));
	g_mc_list.checked = FALSE;
	g_mc_list.kinds = 0;
	/* (the game's own maps have none of their own: only a Custom Edition
	map's or an MCC map's file is read, its tags' names alone) */
	stem = tag_name_strip_path(map_name);
	if (custom_edition_level_name(map_name) && !is_stock_map_stem(stem))
		custom_edition_map_tag_names_walk(map_name, map_content_kinds_visit, NULL);
	else if (mcc_level_name(map_name))
		mcc_cache_tag_names_walk(map_name, map_content_kinds_visit, NULL);
	snprintf(g_mc_list.map, sizeof(g_mc_list.map), "%s", map_name);
	error(_error_silent, "firefight: %s's own content: %d enemies, %d weapons, %d vehicles", map_name,
		g_mc_list.counts[0], g_mc_list.counts[1], g_mc_list.counts[2]);
}

byte firefight_map_content_kinds(char const *map_name)
{
	if (!map_name || !map_name[0])
		return 0;
	map_content_list_read(map_name);
	return g_mc_list.kinds;
}

short firefight_map_content_tag_count(char const *map_name, short kind)
{
	if (kind < 0 || kind >= NUMBER_OF_FIREFIGHT_MAP_CONTENT_KINDS || !map_name || !map_name[0])
		return 0;
	map_content_list_read(map_name);
	return g_mc_list.counts[kind];
}

char const *firefight_map_content_tag_name(char const *map_name, short kind, short index)
{
	if (index < 0 || index >= firefight_map_content_tag_count(map_name, kind))
		return NULL;
	return g_mc_list.names[kind][index];
}

void firefight_map_content_check(char const *map_name, boolean (*usable)(short kind, char const *tag_name))
{
	short kind;
	short index;

	if (!map_name || !map_name[0] || !usable)
		return;
	map_content_list_read(map_name);
	if (g_mc_list.checked)
		return;
	g_mc_list.checked = TRUE;
	for (kind = 0; kind < NUMBER_OF_FIREFIGHT_MAP_CONTENT_KINDS; kind++)
	{
		for (index = 0; index < g_mc_list.counts[kind]; index++)
		{
			g_mc_list.unusable[kind][index] = !usable(kind, g_mc_list.names[kind][index]);
			if (g_mc_list.unusable[kind][index])
				error(_error_silent, "firefight: %s's %s is not usable: off unless turned on", map_name,
					g_mc_list.names[kind][index]);
		}
	}
}

boolean firefight_map_content_tag_usable(char const *map_name, short kind, char const *tag_name)
{
	short index;

	if (kind < 0 || kind >= NUMBER_OF_FIREFIGHT_MAP_CONTENT_KINDS || !tag_name)
		return TRUE;
	map_content_list_read(map_name);
	for (index = 0; index < g_mc_list.counts[kind]; index++)
	{
		if (!strcmp(g_mc_list.names[kind][index], tag_name))
			return !g_mc_list.unusable[kind][index];
	}
	return TRUE;
}

/* (the saved file's section for the map, read once for each map asked of) */
static void map_content_saved_read(char const *map_name)
{
	char stem[96];
	char line[FF_MC_NAME_SIZE + 96];
	boolean ours = FALSE;
	FILE *file;

	map_content_stem(map_name, stem, sizeof(stem));
	if (!stem[0] || !strcmp(g_mc_saved.map, stem))
		return;
	snprintf(g_mc_saved.map, sizeof(g_mc_saved.map), "%s", stem);
	g_mc_saved.known = FALSE;
	g_mc_saved.content = _firefight_map_content_vanilla;
	g_mc_saved.count = 0;
	if ((file = fopen(FF_MC_FILE, "r")) == NULL)
		return;
	while (fgets(line, sizeof(line), file))
	{
		line[strcspn(line, "\r\n")] = 0;
		if (!strncmp(line, "map ", 4))
		{
			char *space = strrchr(line + 4, ' ');

			if (!space)
			{
				ours = FALSE;
				continue;
			}
			*space = 0;
			ours = !strcmp(line + 4, stem);
			if (ours)
			{
				g_mc_saved.known = TRUE;
				g_mc_saved.content = (byte)strtoul(space + 1, NULL, 10);
			}
		}
		else if (ours && line[0] && line[1] == ' ' && line[2] && g_mc_saved.count < FF_MC_SAVED_MAX)
		{
			g_mc_saved.kinds[g_mc_saved.count] = line[0];
			snprintf(g_mc_saved.names[g_mc_saved.count++], FF_MC_NAME_SIZE, "%s", line + 2);
		}
	}
	fclose(file);
}

/* (the file again: every other map's sections as they were, the map's own
after them) */
static void map_content_saved_write(void)
{
	static char rest[64 * 1024];
	size_t length = 0;
	char line[FF_MC_NAME_SIZE + 96];
	boolean ours = FALSE;
	FILE *file;
	short index;

	if (!g_mc_saved.map[0])
		return;
	if ((file = fopen(FF_MC_FILE, "r")) != NULL)
	{
		while (fgets(line, sizeof(line), file))
		{
			size_t size;

			if (!strncmp(line, "map ", 4))
			{
				char name[sizeof(line)];
				char *space;

				snprintf(name, sizeof(name), "%s", line + 4);
				name[strcspn(name, "\r\n")] = 0;
				space = strrchr(name, ' ');
				if (space)
					*space = 0;
				ours = !strcmp(name, g_mc_saved.map);
			}
			size = strlen(line);
			if (ours || length + size + 1 >= sizeof(rest))
				continue;
			memcpy(rest + length, line, size);
			length += size;
			if (size && line[size - 1] != '\n')
				rest[length++] = '\n';
		}
		fclose(file);
	}
	if ((file = fopen(FF_MC_FILE, "w")) == NULL)
	{
		error(_error_silent, "firefight: could not write %s", FF_MC_FILE);
		return;
	}
	fwrite(rest, 1, length, file);
	fprintf(file, "map %s %u\n", g_mc_saved.map, (unsigned)g_mc_saved.content);
	for (index = 0; index < g_mc_saved.count; index++)
		fprintf(file, "%c %s\n", g_mc_saved.kinds[index], g_mc_saved.names[index]);
	fclose(file);
}

boolean firefight_map_content_saved(char const *map_name, byte *content)
{
	map_content_saved_read(map_name);
	if (g_mc_saved.known && content)
		*content = g_mc_saved.content;
	return g_mc_saved.known;
}

void firefight_map_content_save(char const *map_name, byte content)
{
	map_content_saved_read(map_name);
	if (!g_mc_saved.map[0] || (g_mc_saved.known && g_mc_saved.content == content))
		return;
	g_mc_saved.known = TRUE;
	g_mc_saved.content = content;
	map_content_saved_write();
}

/* (the map's section's choice of a tag, by its name: its kind's letter
turns it off, the letter in capitals on by hand, a tag not usable) */
static short map_content_saved_find(short kind, char const *tag_name)
{
	short index;

	for (index = 0; index < g_mc_saved.count; index++)
	{
		if ((g_mc_saved.kinds[index] == ff_mc_kind_letters[kind] ||
			g_mc_saved.kinds[index] == ff_mc_kind_letters[kind] - 'a' + 'A') && !strcmp(g_mc_saved.names[index], tag_name))
		{
			return index;
		}
	}
	return NONE;
}

/* (its choice, else on if usable) */
static boolean map_content_tag_on_usable(char const *map_name, short kind, char const *tag_name, boolean usable)
{
	short found;

	map_content_saved_read(map_name);
	found = map_content_saved_find(kind, tag_name);
	if (found == NONE)
		return usable;
	return g_mc_saved.kinds[found] != ff_mc_kind_letters[kind];
}

boolean firefight_map_content_tag_on(char const *map_name, short kind, char const *tag_name)
{
	if (kind < 0 || kind >= NUMBER_OF_FIREFIGHT_MAP_CONTENT_KINDS || !tag_name)
		return TRUE;
	return map_content_tag_on_usable(map_name, kind, tag_name, firefight_map_content_tag_usable(map_name, kind, tag_name));
}

void firefight_map_content_tag_set(char const *map_name, short kind, char const *tag_name, boolean on)
{
	short found;
	boolean usable;

	if (kind < 0 || kind >= NUMBER_OF_FIREFIGHT_MAP_CONTENT_KINDS || !tag_name || !tag_name[0])
		return;
	usable = firefight_map_content_tag_usable(map_name, kind, tag_name);
	map_content_saved_read(map_name);
	if (!g_mc_saved.map[0])
		return;
	found = map_content_saved_find(kind, tag_name);
	if (found != NONE)
	{
		memmove(&g_mc_saved.kinds[found], &g_mc_saved.kinds[found + 1], (size_t)(g_mc_saved.count - found - 1));
		memmove(g_mc_saved.names[found], g_mc_saved.names[found + 1], (size_t)(g_mc_saved.count - found - 1) * FF_MC_NAME_SIZE);
		g_mc_saved.count--;
	}
	/* (off: its letter; on, of one not usable: in capitals; else nothing) */
	if ((!on || !usable) && g_mc_saved.count < FF_MC_SAVED_MAX)
	{
		g_mc_saved.kinds[g_mc_saved.count] = on ? (char)(ff_mc_kind_letters[kind] - 'a' + 'A') : ff_mc_kind_letters[kind];
		snprintf(g_mc_saved.names[g_mc_saved.count++], FF_MC_NAME_SIZE, "%s", tag_name);
	}
	map_content_saved_write();
}

/* MAP CONTENT's PLAYER LOADOUT: the map's weapons the players spawn with,
in place of the game's, its primary ('p') and secondary ('s') each a line of
the map's section; none set, the game's loadout */
static char const ff_mc_loadout_letters[2] = { 'p', 's' };

static short map_loadout_saved_find(short slot)
{
	short index;

	for (index = 0; index < g_mc_saved.count; index++)
	{
		if (g_mc_saved.kinds[index] == ff_mc_loadout_letters[slot])
			return index;
	}
	return NONE;
}

static void map_content_saved_remove(short found)
{
	memmove(&g_mc_saved.kinds[found], &g_mc_saved.kinds[found + 1], (size_t)(g_mc_saved.count - found - 1));
	memmove(g_mc_saved.names[found], g_mc_saved.names[found + 1], (size_t)(g_mc_saved.count - found - 1) * FF_MC_NAME_SIZE);
	g_mc_saved.count--;
}

char const *firefight_map_loadout_get(char const *map_name, short slot)
{
	short found;

	if (slot < 0 || slot > 1 || !map_name || !map_name[0])
		return NULL;
	map_content_saved_read(map_name);
	found = map_loadout_saved_find(slot);
	return found == NONE ? NULL : g_mc_saved.names[found];
}

/* (a weapon in one slot alone: set as one, it leaves the other; NULL
clears the slot) */
void firefight_map_loadout_set(char const *map_name, short slot, char const *tag_name)
{
	short other;
	short found;

	if (slot < 0 || slot > 1 || !map_name || !map_name[0])
		return;
	map_content_saved_read(map_name);
	if (!g_mc_saved.map[0])
		return;
	if ((found = map_loadout_saved_find(slot)) != NONE)
		map_content_saved_remove(found);
	other = (short)(1 - slot);
	if (tag_name && (found = map_loadout_saved_find(other)) != NONE && !strcmp(g_mc_saved.names[found], tag_name))
		map_content_saved_remove(found);
	if (tag_name && tag_name[0] && g_mc_saved.count < FF_MC_SAVED_MAX)
	{
		g_mc_saved.kinds[g_mc_saved.count] = ff_mc_loadout_letters[slot];
		snprintf(g_mc_saved.names[g_mc_saved.count++], FF_MC_NAME_SIZE, "%s", tag_name);
	}
	map_content_saved_write();
}

/* (in the game, the host's: the map played's choice for a slot, a weapon
the level has that a player can hold, else NULL for the game's) */
char const *firefight_map_loadout_override(short slot)
{
	char const *loaded = cache_file_loaded_map_name();
	char const *name = loaded && loaded[0] ? firefight_map_loadout_get(loaded, slot) : NULL;
	long tag;

	if (!name)
		return NULL;
	tag = tag_loaded(WEAPON_DEFINITION_TAG, name);
	if (tag == NONE || firefight_weapon_is_objective(tag, "PLAYER LOADOUT"))
	{
		error(_error_silent, "firefight: PLAYER LOADOUT's %s is not in %s: the game's loadout", name, loaded);
		return NULL;
	}
	return name;
}

/* ---- MAP CONTENT's HEALTH RULES: a map whose player has no shields (GTA)
or shields that never come back (Aerowalk's, health and armour picked up),
read from its file (its globals' multiplayer unit's collision model), and
the host's choices for it: SHIELDS and SHIELD REGEN (MAP'S, ON, OFF), HEALTH
REGEN (OFF, ON), an 'h' line of the map's section */

/* (a recharge this long or longer is none: Aerowalk's is 900 s) */
#define FF_HEALTH_NO_REGEN_SECONDS 60.0f

static struct
{
	char map[128];
	byte rules;
	boolean read;
	float values[4];
} g_mc_health;

/* (an MCC map's player health is not read from its file in this build) */
static boolean map_health_read_mcc(char const *map_name, float out[4])
{
	(void)map_name;
	(void)out;
	return FALSE;
}

/* (nor its tags' names walked: its content is as if it had none) */
static boolean mcc_cache_tag_names_walk(char const *map_name,
	int (*visit)(void *context, unsigned long group_tag, char const *name), void *context)
{
	(void)map_name;
	(void)visit;
	(void)context;
	return FALSE;
}

byte firefight_map_health_detect(char const *map_name)
{
	char const *stem;

	if (!map_name || !map_name[0])
		return 0;
	if (!strcmp(g_mc_health.map, map_name))
		return g_mc_health.rules;
	snprintf(g_mc_health.map, sizeof(g_mc_health.map), "%s", map_name);
	g_mc_health.rules = 0;
	stem = tag_name_strip_path(map_name);
	g_mc_health.read = (custom_edition_level_name(map_name) && !is_stock_map_stem(stem) &&
			custom_edition_map_player_shields(map_name, g_mc_health.values)) ||
		(mcc_level_name(map_name) && map_health_read_mcc(map_name, g_mc_health.values));
	if (g_mc_health.read)
	{
		if (g_mc_health.values[1] <= 0.0f)
			g_mc_health.rules |= FIREFIGHT_MAP_HEALTH_NO_SHIELDS;
		else if (g_mc_health.values[3] >= FF_HEALTH_NO_REGEN_SECONDS || g_mc_health.values[2] >= FF_HEALTH_NO_REGEN_SECONDS)
			g_mc_health.rules |= FIREFIGHT_MAP_HEALTH_NO_SHIELD_REGEN;
		error(_error_silent, "firefight: %s's player: body %.0f, shields %.0f, stun %.1f s, recharge %.1f s%s%s", map_name,
			g_mc_health.values[0], g_mc_health.values[1], g_mc_health.values[2], g_mc_health.values[3],
			(g_mc_health.rules & FIREFIGHT_MAP_HEALTH_NO_SHIELDS) ? ": no shields" : "",
			(g_mc_health.rules & FIREFIGHT_MAP_HEALTH_NO_SHIELD_REGEN) ? ": no shield regen" : "");
	}
	return g_mc_health.rules;
}

/* (the rules' changes, for the game's copy of them) */
static unsigned long g_health_rules_serial = 1;

static short map_health_saved_find(void)
{
	short index;

	for (index = 0; index < g_mc_saved.count; index++)
	{
		if (g_mc_saved.kinds[index] == 'h')
			return index;
	}
	return NONE;
}

void firefight_map_health_rules_get(char const *map_name, byte rules[NUMBER_OF_FIREFIGHT_HEALTH_RULES])
{
	short found;
	short rule;

	memset(rules, 0, NUMBER_OF_FIREFIGHT_HEALTH_RULES);
	if (!map_name || !map_name[0])
		return;
	map_content_saved_read(map_name);
	if ((found = map_health_saved_find()) == NONE)
		return;
	for (rule = 0; rule < NUMBER_OF_FIREFIGHT_HEALTH_RULES; rule++)
	{
		char const *value = g_mc_saved.names[found] + rule * 2;

		if (strlen(g_mc_saved.names[found]) > (size_t)(rule * 2) && *value >= '0' && *value <= '2')
			rules[rule] = (byte)(*value - '0');
	}
}

void firefight_map_health_rules_set(char const *map_name, byte const rules[NUMBER_OF_FIREFIGHT_HEALTH_RULES])
{
	byte now[NUMBER_OF_FIREFIGHT_HEALTH_RULES];
	short found;

	if (!map_name || !map_name[0])
		return;
	firefight_map_health_rules_get(map_name, now);
	if (!memcmp(now, rules, NUMBER_OF_FIREFIGHT_HEALTH_RULES) || !g_mc_saved.map[0])
		return;
	if ((found = map_health_saved_find()) != NONE)
		map_content_saved_remove(found);
	if ((rules[0] || rules[1] || rules[2]) && g_mc_saved.count < FF_MC_SAVED_MAX)
	{
		g_mc_saved.kinds[g_mc_saved.count] = 'h';
		snprintf(g_mc_saved.names[g_mc_saved.count++], FF_MC_NAME_SIZE, "%u %u %u", (unsigned)rules[0],
			(unsigned)rules[1], (unsigned)rules[2]);
	}
	g_health_rules_serial++;
	map_content_saved_write();
}

/* (in the game: the map played's, read again when they change) */
byte firefight_health_rule(short rule)
{
	static char map[128];
	static unsigned long serial;
	static byte rules[NUMBER_OF_FIREFIGHT_HEALTH_RULES];
	char const *loaded = cache_file_loaded_map_name();

	if (rule < 0 || rule >= NUMBER_OF_FIREFIGHT_HEALTH_RULES || !loaded || !loaded[0])
		return 0;
	if (serial != g_health_rules_serial || strcmp(map, loaded))
	{
		serial = g_health_rules_serial;
		snprintf(map, sizeof(map), "%s", loaded);
		firefight_map_health_rules_get(loaded, rules);
	}
	return rules[rule];
}

/* (in the game: whether CUSTOM leaves a tag of the map's out, the host's
choices for the map played) */
static boolean map_content_custom_off(byte custom_bit, short kind, long tag_index)
{
	struct firefight_options const *opts = firefight_options_get();
	char const *loaded = cache_file_loaded_map_name();

	if (!opts || !(opts->firefight_map_content & custom_bit) || !loaded || !loaded[0] || tag_index == NONE)
		return FALSE;
	/* (the tags here are those already found usable, or turned on by hand) */
	return !map_content_tag_on_usable(loaded, kind, tag_get_name(tag_index), TRUE);
}

/* (in the game: whether CUSTOM has a tag of the map's that is not usable
turned on by hand) */
static boolean map_content_custom_on_by_hand(byte custom_bit, short kind, long tag_index)
{
	struct firefight_options const *opts = firefight_options_get();
	char const *loaded = cache_file_loaded_map_name();

	if (!opts || !(opts->firefight_map_content & custom_bit) || !loaded || !loaded[0] || tag_index == NONE)
		return FALSE;
	return map_content_tag_on_usable(loaded, kind, tag_get_name(tag_index), FALSE);
}

/* whether MAP CONTENT has the map's weapons in place of the game's: ONLY
MAP, and the map has some */
static boolean map_weapons_only(void)
{
	return map_content_of(FIREFIGHT_MAP_CONTENT_WEAPONS_OFF) == _firefight_map_content_only_map && ff_map_weapon_count > 0;
}

static boolean map_vehicles_only(void)
{
	return map_content_of(FIREFIGHT_MAP_CONTENT_VEHICLES_OFF) == _firefight_map_content_only_map && ff_map_vehicle_count > 0;
}

/* a random loadout's weapon (Fiesta's, a custom loadout's RANDOM) of the
map's own (MAP CONTENT): ONLY MAP's always one of them; INCLUDE MAP's one of
its own beside the game's half the time; other than `other` (the loadout's
other weapon) where it can be; NULL for one of the game's */
char const *firefight_map_loadout_weapon(char const *other)
{
	byte mode = map_content_of(FIREFIGHT_MAP_CONTENT_WEAPONS_OFF);
	short candidates[MAX_ROSTER_VARIANTS];
	short count = 0;
	short index;
	boolean only = mode == _firefight_map_content_only_map;

	if (mode == _firefight_map_content_vanilla || ff_map_weapon_count <= 0)
		return NULL;
	if (!only && (spawner_rand() & 1))
		return NULL;
	for (index = 0; index < ff_map_weapon_count; index++)
	{
		char const *name = tag_get_name(ff_map_weapon_tags[index]);

		if ((only || !ff_map_weapon_stock[index]) && name && (!other || strcmp(other, name)))
			candidates[count++] = index;
	}
	/* (one weapon of its own alone: both hands it) */
	if (!count && only)
		return tag_get_name(ff_map_weapon_tags[spawner_rand() % (unsigned long)ff_map_weapon_count]);
	return count ? tag_get_name(ff_map_weapon_tags[candidates[spawner_rand() % (unsigned long)count]]) : NULL;
}

/* one of the map's weapons: of its own beside the game's (INCLUDE MAP's,
custom_only), a power weapon first for a heavy drop; NONE without any */
static long pick_map_weapon(boolean heavy, boolean custom_only)
{
	short candidates[MAX_ROSTER_VARIANTS];
	short count = 0;
	short pass;
	short i;

	for (pass = heavy ? 0 : 1; pass < 2 && !count; pass++)
	{
		for (i = 0; i < ff_map_weapon_count; i++)
		{
			if ((!custom_only || !ff_map_weapon_stock[i]) && (pass == 1 || ff_map_weapon_power[i]))
				candidates[count++] = i;
		}
	}
	return count ? ff_map_weapon_tags[candidates[spawner_rand() % (unsigned long)count]] : NONE;
}

/* a vehicle type's flying (a plane's or a fighter's) or hovering (a scout's) */
static boolean vehicle_tag_flies(long vehicle_tag)
{
	struct ff_vehicle_ai_tag_layout *vehicle = (struct ff_vehicle_ai_tag_layout *)tag_get(VEHICLE_DEFINITION_TAG, vehicle_tag);
	char const *name = tag_get_name(vehicle_tag);

	return (vehicle && (vehicle->type == FF_VEHICLE_TYPE_HUMAN_PLANE || vehicle->type == FF_VEHICLE_TYPE_ALIEN_FIGHTER)) ||
		(name && strstr(name, "banshee") != NULL);
}

static boolean vehicle_tag_hovers(long vehicle_tag)
{
	struct ff_vehicle_ai_tag_layout *vehicle = (struct ff_vehicle_ai_tag_layout *)tag_get(VEHICLE_DEFINITION_TAG, vehicle_tag);
	char const *name = tag_get_name(vehicle_tag);

	return (vehicle && vehicle->type == FF_VEHICLE_TYPE_ALIEN_SCOUT) ||
		(name && (strstr(name, "wraith") != NULL || strstr(name, "ghost") != NULL));
}

/* a heavy vehicle (a tank's), by its words */
static boolean vehicle_tag_heavy(long vehicle_tag)
{
	static char const *const heavy_words[] = { "wraith", "scorpion", "tank", "mammoth", "elephant", NULL };
	char lowered[256];

	lowercase_copy(tag_get_name(vehicle_tag), lowered, sizeof(lowered));
	return name_has_word(lowered, heavy_words);
}

/* one of the map's vehicles for the enemies: heavy or light as wanted where
it has both kinds; of its own beside the game's (custom_only) */
static long pick_map_vehicle(boolean heavy, boolean custom_only)
{
	short candidates[MAX_MAP_VEHICLES];
	short count = 0;
	short pass;
	short i;

	for (pass = 0; pass < 2 && !count; pass++)
	{
		for (i = 0; i < ff_map_vehicle_count; i++)
		{
			if ((!custom_only || !ff_map_vehicle_stock[i]) && (pass == 1 || vehicle_tag_heavy(ff_map_vehicle_tags[i]) == heavy))
				candidates[count++] = i;
		}
	}
	return count ? ff_map_vehicle_tags[candidates[spawner_rand() % (unsigned long)count]] : NONE;
}

/* ---------- The players' vehicles: the map's own placements' (VEHICLES)

Each slot of deployed_set_vehicles is one of the map's vehicle placements a
player could drive (player_vehicles_build), made where the map has it: ALL
AT SPAWN all of them from the start; ORDNANCE REWARDS one more, at random,
a few seconds after each ordnance drop past the game's first (on a PLAYER
VEH PAD, where Forge marked any); NONE none. A slot's vehicle once made is
the players' for the game: made again at a round's start where it went */

enum
{
	/* (a reward's vehicle after the drop's banner) */
	FF_VEHICLE_REWARD_DELAY_TICKS = 4 * TICKS_PER_SECOND
};

static struct
{
	boolean built;
	short count;
	short placements[FIREFIGHT_MAX_SET_VEHICLES];
	boolean earned[FIREFIGHT_MAX_SET_VEHICLES];
	boolean rewarded[FIREFIGHT_MAX_SET_VEHICLES];
	/* (a slot whose spot an objective's enemy vehicle took: not made again
	till it gives it back, firefight_objective_release_vehicle_slot) */
	boolean held[FIREFIGHT_MAX_SET_VEHICLES];
	short rewards_pending;
	long reward_time;
} g_player_vehicles;

static byte player_vehicle_spawns(void)
{
	struct firefight_options const *opts = firefight_options_get();

	return opts ? (byte)PIN(opts->firefight_vehicle_spawns, 0, NUMBER_OF_FIREFIGHT_VEHICLE_SPAWNS - 1) :
		(byte)_firefight_vehicle_spawns_all;
}

/* a vehicle the players may have: one they could drive, the game's own, or
the map's own where MAP CONTENT takes them (and its CUSTOM list has it on) */
static boolean player_vehicle_tag_allowed(long tag_index)
{
	char lowered[256];

	if (tag_index == NONE)
		return FALSE;
	lowercase_copy(tag_get_name(tag_index), lowered, sizeof(lowered));
	if (!map_vehicle_tag_drivable(lowered))
		return FALSE;
	if (vehicle_tag_stock(lowered))
		return TRUE;
	return map_content_of(FIREFIGHT_MAP_CONTENT_VEHICLES_OFF) != _firefight_map_content_vanilla &&
		!map_content_custom_off(FIREFIGHT_MAP_CONTENT_VEHICLES_CUSTOM, _firefight_map_content_kind_vehicles, tag_index);
}

/* (the placement's vehicle type, NONE for none) */
static long player_vehicle_placement_tag(struct scenario *scen, short placement_index,
	struct ff_scenario_vehicle **out_placement)
{
	struct ff_scenario_vehicle *placement;
	struct scenario_object_palette_entry *palette;
	short palette_index;

	if (!scen || !VALID_INDEX(placement_index, scen->vehicles.count))
		return NONE;
	placement = TAG_BLOCK_GET_ELEMENT(&scen->vehicles, placement_index, struct ff_scenario_vehicle);
	palette_index = placement->object.palette_entry_index;
	if (!VALID_INDEX(palette_index, scen->vehicle_palette.count))
		return NONE;
	palette = TAG_BLOCK_GET_ELEMENT(&scen->vehicle_palette, palette_index, struct scenario_object_palette_entry);
	if (out_placement)
		*out_placement = placement;
	return palette->reference.index;
}

/* the map's placements the players' vehicles are, once a game (NONE's
none); ALL AT SPAWN's each earned */
static void player_vehicles_build(void)
{
	struct scenario *scen;
	byte spawns;
	short i;

	if (g_player_vehicles.built || !(scen = global_scenario_try_and_get()))
		return;
	g_player_vehicles.built = TRUE;
	g_player_vehicles.count = 0;
	spawns = player_vehicle_spawns();
	if (spawns == _firefight_vehicle_spawns_none)
		return;
	for (i = 0; i < scen->vehicles.count && g_player_vehicles.count < FIREFIGHT_MAX_SET_VEHICLES; i++)
	{
		struct ff_scenario_vehicle *placement = NULL;
		long tag_index = player_vehicle_placement_tag(scen, i, &placement);
		short slot = g_player_vehicles.count;

		if (!placement || !player_vehicle_tag_allowed(tag_index) || !point_is_in_valid_bsp(&placement->object.position))
			continue;
		g_player_vehicles.placements[slot] = i;
		g_player_vehicles.earned[slot] = spawns == _firefight_vehicle_spawns_all;
		g_player_vehicles.rewarded[slot] = FALSE;
		g_player_vehicles.count++;
	}
	error(_error_silent, "firefight: %d of the map's %d vehicle placements are the players' (%s)",
		g_player_vehicles.count, scen->vehicles.count,
		spawns == _firefight_vehicle_spawns_all ? "all at spawn" : "ordnance rewards");
}

void firefight_player_vehicles_reset(void)
{
	csmemset(&g_player_vehicles, 0, sizeof(g_player_vehicles));
	g_player_vehicles.reward_time = NONE;
}

boolean firefight_player_vehicles_at_start(void)
{
	if (player_vehicle_spawns() != _firefight_vehicle_spawns_all)
		return FALSE;
	player_vehicles_build();
	return g_player_vehicles.count > 0;
}

/* a free PLAYER VEH PAD (the nth's first), for a reward's vehicle */
static boolean player_vehicle_pad(short nth, real_point3d *out_position, real *out_facing)
{
	real_point3d pad_pts[16];
	real pad_facings[16];
	int pad_types[16];
	short pad_count = collect_matching_custom_ff_spawns(
		(1UL << FF_SPAWN_TYPE_PLAYER_VEHICLE), 1, 1, FALSE, FALSE, TRUE,
		pad_pts, pad_facings, pad_types, 16);
	short tries;

	for (tries = 0; tries < pad_count; tries++)
	{
		short pick = (short)((nth + tries) % pad_count);
		real_point3d position = pad_pts[pick];

		project_point_to_ground(&position, 0.75f);
		if (map_vehicle_spot_clear(&position))
		{
			*out_position = position;
			*out_facing = pad_facings[pick];
			return TRUE;
		}
	}
	return FALSE;
}

/* (whether the slot's vehicle is still there to drive: not deleted, not a
wreck) */
static boolean player_vehicle_present(long vehicle_index)
{
	struct object_datum *object = vehicle_index != NONE ?
		object_try_and_get_and_verify_type(vehicle_index, _object_mask_vehicle) : NULL;

	return object && !TEST_FLAG(object->object.damage_flags, _object_dead_bit);
}

/* a slot's vehicle made: where the map has it (a reward's on a pad first),
else at the nearest free spot of the map's vehicles */
static boolean player_vehicle_make(short slot, short nth)
{
	struct ff_scenario_vehicle *placement = NULL;
	long tag_index = player_vehicle_placement_tag(global_scenario_try_and_get(),
		g_player_vehicles.placements[slot], &placement);
	struct object_placement_data placement_data;
	real_point3d position;
	real facing;
	long vehicle_index;

	if (!placement || tag_index == NONE)
		return FALSE;
	position = placement->object.position;
	facing = placement->object.rotation.yaw;
	if (!(g_player_vehicles.rewarded[slot] && player_vehicle_pad(nth, &position, &facing)) &&
		!map_vehicle_spot_clear(&position) && !map_vehicle_spot(tag_index, &position, &facing))
	{
		return FALSE;
	}
	object_placement_data_new(&placement_data, tag_index, NONE);
	placement_data.position = position;
	vector3d_from_angle(&placement_data.forward, facing);
	placement_data.owner_team_index = _game_team_player;
	vehicle_index = object_new(&placement_data);
	if (vehicle_index == NONE)
		return FALSE;
	ff_state.deployed_set_vehicles[slot] = vehicle_index;
	ff_state.deployed_set_vehicle_count++;
	return TRUE;
}

void firefight_spawn_player_vehicles(void)
{
	short slot;
	short made = 0;
	short nth = 0;

	/* (after the map's own vehicles are cleared away: firefight_update's
	second tick) */
	if (network_game_distributed_client() || ff_state.session_elapsed_ticks < 2)
		return;
	player_vehicles_build();
	ff_state.deployed_set_vehicle_count = 0;
	for (slot = 0; slot < FIREFIGHT_MAX_SET_VEHICLES; slot++)
	{
		if (slot < g_player_vehicles.count && player_vehicle_present(ff_state.deployed_set_vehicles[slot]))
		{
			ff_state.deployed_set_vehicle_count++;
			nth++;
			continue;
		}
		ff_state.deployed_set_vehicles[slot] = NONE;
		if (slot >= g_player_vehicles.count || !g_player_vehicles.earned[slot] || g_player_vehicles.held[slot])
			continue;
		if (player_vehicle_make(slot, nth))
		{
			nth++;
			made++;
		}
	}
	if (made)
		firefight_announce_banner(_ff_banner_vehicle_deployed, MAX(1, ff_state.current_round), 3 * TICKS_PER_SECOND);
}

/* (firefight_objectives.c's Purge) the players' vehicles' spots within
`radius` of `center` for its enemy vehicles: each one there empty of players
taken away, its slot held (made again once given back:
firefight_objective_release_vehicle_slot). The spots, which way, each its
slot; at most `maximum` */
short firefight_objective_take_vehicle_spots(real_point3d const *center, real radius, real_point3d *points,
	real *facings, short *slots, short maximum)
{
	struct scenario *scen = global_scenario_try_and_get();
	short count = 0;
	short slot;
	short far = 0, gone = 0, moved = 0, ridden = 0;

	if (!scen || !center || network_game_distributed_client())
		return 0;
	player_vehicles_build();
	for (slot = 0; slot < g_player_vehicles.count && count < maximum; slot++)
	{
		struct ff_scenario_vehicle *placement = NULL;
		long vehicle = ff_state.deployed_set_vehicles[slot];
		struct object_datum *object;
		struct object_iterator iterator;
		struct object_datum *rider;
		boolean occupied = FALSE;

		if (g_player_vehicles.held[slot] ||
			player_vehicle_placement_tag(scen, g_player_vehicles.placements[slot], &placement) == NONE || !placement ||
			distance3d(&placement->object.position, center) > radius)
		{
			far++;
			continue;
		}
		/* (none there now: the spot is free) */
		if (!player_vehicle_present(vehicle))
		{
			gone++;
			points[count] = placement->object.position;
			facings[count] = placement->object.rotation.yaw;
			slots[count] = NONE;
			count++;
			continue;
		}
		object = object_get(vehicle);
		if (distance3d(&object->object.position, &placement->object.position) > 4.0f)
		{
			moved++;
			points[count] = placement->object.position;
			facings[count] = placement->object.rotation.yaw;
			slots[count] = NONE;
			count++;
			continue;
		}
		object_iterator_new(&iterator, _object_mask_unit, 0);
		while ((rider = (struct object_datum *)object_iterator_next(&iterator)) != NULL)
		{
			if (rider->object.parent_object_index == vehicle)
				occupied = TRUE;
		}
		if (occupied)
		{
			ridden++;
			continue;
		}
		object_delete(vehicle);
		ff_state.deployed_set_vehicles[slot] = NONE;
		g_player_vehicles.held[slot] = TRUE;
		points[count] = placement->object.position;
		facings[count] = placement->object.rotation.yaw;
		slots[count] = slot;
		count++;
	}
	error(_error_silent, "firefight: a camp's vehicle spots: %d of the players' %d placements in reach, %d taken from "
		"them, %d free (gone %d, driven off %d), %d ridden; %d not in reach", (int)(g_player_vehicles.count - far),
		(int)g_player_vehicles.count, (int)(count - gone - moved), (int)(gone + moved), (int)gone, (int)moved,
		(int)ridden, (int)far);
	return count;
}

/* ... a held slot given back: its vehicle made again, where the map has it */
void firefight_objective_release_vehicle_slot(short slot)
{
	if (slot < 0 || slot >= FIREFIGHT_MAX_SET_VEHICLES || !g_player_vehicles.held[slot])
		return;
	g_player_vehicles.held[slot] = FALSE;
	if (slot < g_player_vehicles.count && g_player_vehicles.earned[slot] &&
		!player_vehicle_present(ff_state.deployed_set_vehicles[slot]))
	{
		player_vehicle_make(slot, 0);
	}
}

void firefight_reward_player_vehicle(void)
{
	if (network_game_distributed_client() || player_vehicle_spawns() != _firefight_vehicle_spawns_ordnance)
		return;
	g_player_vehicles.rewards_pending++;
	g_player_vehicles.reward_time = game_time_get() + FF_VEHICLE_REWARD_DELAY_TICKS;
}

/* (each tick, the host's: a reward's vehicle, when its time comes; one of
the map's not yet the players', at random, till they have all) */
static void player_vehicle_rewards_update(void)
{
	short candidates[FIREFIGHT_MAX_SET_VEHICLES];
	short count = 0;
	short slot;
	short nth = 0;

	if (g_player_vehicles.rewards_pending <= 0 || g_player_vehicles.reward_time == NONE ||
		game_time_get() < g_player_vehicles.reward_time)
	{
		return;
	}
	g_player_vehicles.rewards_pending--;
	g_player_vehicles.reward_time = g_player_vehicles.rewards_pending > 0 ?
		game_time_get() + FF_VEHICLE_REWARD_DELAY_TICKS : NONE;
	player_vehicles_build();
	for (slot = 0; slot < g_player_vehicles.count; slot++)
	{
		if (!g_player_vehicles.earned[slot])
			candidates[count++] = slot;
		else if (g_player_vehicles.rewarded[slot])
			nth++;
	}
	if (!count)
		return;
	slot = candidates[spawner_rand() % (unsigned long)count];
	g_player_vehicles.earned[slot] = TRUE;
	g_player_vehicles.rewarded[slot] = TRUE;
	if (player_vehicle_make(slot, nth))
		firefight_announce_banner(_ff_banner_vehicle_deployed, MAX(1, ff_state.current_round), 3 * TICKS_PER_SECOND);
}

/* ---------- VEHICLES' row: whether a map (not loaded) has vehicles placed */

static struct
{
	char map[128];
	boolean stock;
	boolean own;
} g_map_vehicles;

/* (the game's own maps with vehicles placed) */
static boolean stock_map_has_vehicles(char const *stem)
{
	static char const *const with_vehicles[] = {
		"bloodgulch", "dangercanyon", "deathisland", "gephyrophobia", "icefields", "infinity",
		"sidewinder", "timberland",
		"a10", "a30", "a50", "b30", "b40", "c10", "c20", "c40", "d20", "d40"
	};
	short i;

	for (i = 0; i < (short)NUMBEROF(with_vehicles); i++)
	{
		if (!strcmp(stem, with_vehicles[i]))
			return TRUE;
	}
	return FALSE;
}

static int map_vehicles_visit(void *context, unsigned long group_tag, char const *name)
{
	char lowered[256];

	(void)context;
	if (group_tag != VEHICLE_DEFINITION_TAG)
		return TRUE;
	lowercase_copy(name, lowered, sizeof(lowered));
	if (!map_vehicle_tag_drivable(lowered))
		return TRUE;
	if (vehicle_tag_stock(lowered))
		g_map_vehicles.stock = TRUE;
	else
		g_map_vehicles.own = TRUE;
	return TRUE;
}

void firefight_map_has_vehicles(char const *map_name, boolean *stock, boolean *own)
{
	char stem[96];

	*stock = TRUE;
	*own = FALSE;
	if (!map_name || !map_name[0])
		return;
	if (strcmp(g_map_vehicles.map, map_name))
	{
		boolean read;

		snprintf(g_map_vehicles.map, sizeof(g_map_vehicles.map), "%s", map_name);
		g_map_vehicles.stock = FALSE;
		g_map_vehicles.own = FALSE;
		map_content_stem(map_name, stem, sizeof(stem));
		if (is_stock_map_stem(stem))
		{
			g_map_vehicles.stock = stock_map_has_vehicles(stem);
			read = TRUE;
		}
		else if (custom_edition_level_name(map_name))
			read = custom_edition_map_tag_names_walk(map_name, map_vehicles_visit, NULL);
		else if (mcc_level_name(map_name))
			read = mcc_cache_tag_names_walk(map_name, map_vehicles_visit, NULL);
		else
			read = FALSE;
		/* (a map not read: as if it had them) */
		if (!read)
			g_map_vehicles.stock = TRUE;
		error(_error_silent, "firefight: %s's vehicles: %s%s%s", map_name,
			g_map_vehicles.stock ? "the game's" : "", g_map_vehicles.stock && g_map_vehicles.own ? ", " : "",
			g_map_vehicles.own ? "its own" : !g_map_vehicles.stock ? "none" : "");
	}
	*stock = g_map_vehicles.stock;
	*own = g_map_vehicles.own;
}

static boolean vehicle_has_player_rider(long vehicle_index)
{
	struct data_iterator iter;
	struct player_datum *player;

	if (!player_data || vehicle_index == NONE)
		return FALSE;

	data_iterator_new(&iter, player_data);
	while ((player = data_iterator_next(&iter)) != NULL)
	{
		if (player->unit_index != NONE)
		{
			long parent = object_get_ultimate_parent(player->unit_index);
			if (parent == vehicle_index)
				return TRUE;
		}
	}

	return FALSE;
}

static void eject_all_vehicle_passengers(long vehicle_index)
{
	struct object_iterator iter;
	struct object_datum *obj;
	long riders[32];
	short rider_count = 0;
	short i;

	if (vehicle_index == NONE)
		return;

	object_iterator_new(&iter, _object_mask_unit, 0);
	while ((obj = (struct object_datum *)object_iterator_next(&iter)) != NULL && rider_count < 32)
	{
		if (iter.index != vehicle_index &&
			(obj->object.parent_object_index == vehicle_index ||
				object_get_ultimate_parent(iter.index) == vehicle_index))
		{
			riders[rider_count++] = iter.index;
		}
	}

	for (i = 0; i < rider_count; i++)
	{
		struct unit_datum *u = (struct unit_datum *)object_try_and_get_and_verify_type(riders[i], _object_mask_unit);
		if (!u)
			continue;
		if (u->unit.parent_seat_index != NONE)
			unit_exit_seat_end(riders[i]);
		if (u->object.parent_object_index != NONE)
			unit_detach_from_parent(riders[i]);
	}
}

static boolean is_tracked_firefight_vehicle(long vehicle_index)
{
	short i;

	if (vehicle_index == NONE)
		return FALSE;
	if (ff_state.active_dropship.vehicle_index == vehicle_index)
		return TRUE;
	if (firefight_is_generator_object(vehicle_index, NULL))
		return TRUE;
	for (i = 0; i < FIREFIGHT_MAX_SET_VEHICLES; i++)
	{
		if (ff_state.deployed_set_vehicles[i] == vehicle_index)
			return TRUE;
	}
	for (i = 0; i < FIREFIGHT_MAX_TRACKED_ENEMIES; i++)
	{
		long u_idx = ff_state.tracked_enemy_units[i];
		if (u_idx != NONE)
		{
			struct object_datum *u_obj = object_try_and_get(u_idx);
			if (u_obj && u_obj->object.parent_object_index == vehicle_index)
				return TRUE;
		}
	}
	return FALSE;
}

void firefight_cleanup_natural_multiplayer_sandbox(boolean clear_weapons_and_equipment)
{
	struct scenario *scen = global_scenario_try_and_get();
	struct object_iterator iter;
	struct object_datum *obj;
	long to_delete[128];
	short delete_count = 0;
	short i;

	if (!scen || scen->type != _scenario_type_multiplayer)
		return;

	/* Delete any naturally spawned unoccupied map vehicles not tracked in Firefight's scheduled round vehicle array */
	object_iterator_new(&iter, _object_mask_vehicle, 0);
	while ((obj = (struct object_datum *)object_iterator_next(&iter)) != NULL && delete_count < 128)
	{
		long obj_index = iter.index;
		if (!is_tracked_firefight_vehicle(obj_index) &&
			!vehicle_has_player_rider(obj_index) &&
			obj->object.first_child_object_index == NONE)
		{
			to_delete[delete_count++] = obj_index;
		}
	}

	/* Clear natural ground weapons & equipment at match start (tick == 1) before Firefight ordnance drops */
	if (clear_weapons_and_equipment)
	{
		object_iterator_new(&iter, _object_mask_item, 0);
		while ((obj = (struct object_datum *)object_iterator_next(&iter)) != NULL && delete_count < 128)
		{
			/* (on the ground: a weapon in a unit's inventory has no parent
			until it is in hand, and is off the map. The first spawn's are
			already there, this tick) */
			if (obj->object.parent_object_index == NONE &&
				TEST_FLAG(obj->object.flags, _object_connected_to_map_bit) &&
				!TEST_FLAG(obj->object.flags, _object_cannot_be_garbage_bit))
			{
				to_delete[delete_count++] = iter.index;
			}
		}
	}

	for (i = 0; i < delete_count; i++)
	{
		if (object_try_and_get_and_verify_type(to_delete[i], 0xFFFFFFFFUL))
			object_delete(to_delete[i]);
	}
}

/* ---------- Per-Tick Dropship & Aggro Update */

/* (the host's) a Flood drop's tick: its squad out where the form bursts, in a
ring round it, each facing out and sent at a player */
static void flood_drop_host_update(void)
{
	short elapsed = ff_state.active_dropship.elapsed_ticks;

	ff_state.active_dropship.phase = elapsed < FF_FLOOD_DROP_BURST_TICK ?
		DROPSHIP_PHASE_DESCENDING : DROPSHIP_PHASE_ASCENDING;
	if (elapsed >= FF_FLOOD_DROP_BURST_TICK && !ff_state.active_dropship.payload_deployed)
	{
		long player_units[16];
		short player_count = get_living_player_positions(NULL, player_units, 16);
		short to_unload = ff_state.active_dropship.squad_to_spawn;
		real facing = ff_state.active_dropship.approach_angle + _pi;
		short s_idx;

		for (s_idx = 0; s_idx < to_unload; s_idx++)
		{
			byte subtype = (s_idx < g_dropship_squad_subtype_count) ?
				g_dropship_squad_subtypes[s_idx] : _ff_sub_grunt;
			real angle = facing + (_pi * 2.0f * (real)s_idx) / (real)MAX(1, to_unload);
			real radius = 0.9f + 0.5f * (real)(s_idx & 1);
			real_point3d out_pos = ff_state.active_dropship.drop_zone;
			long u_spawned;

			out_pos.x += cosine(angle) * radius;
			out_pos.y += sine(angle) * radius;
			out_pos.z += 0.5f;
			project_point_to_ground(&out_pos, 0.25f);

			u_spawned = spawn_subtype_actor_at(
				subtype,
				ff_state.active_dropship.wave_faction,
				&out_pos,
				angle,
				ff_state.active_dropship.forced_team,
				TRUE);
			if (u_spawned != NONE && player_count > 0)
			{
				struct unit_datum *u_dat = unit_try_and_get(u_spawned);

				if (u_dat && u_dat->unit.actor_index != NONE)
					direct_actor_toward_unit(u_dat->unit.actor_index, player_units[s_idx % player_count]);
			}
		}

		g_dropship_squad_subtype_count = 0;
		ff_state.active_dropship.payload_deployed = TRUE;
		ff_state.active_dropship.squad_to_spawn = 0;
	}

	ff_state.active_dropship.elapsed_ticks++;
	if (ff_state.active_dropship.timer_ticks > 0)
		ff_state.active_dropship.timer_ticks--;
	if (ff_state.active_dropship.elapsed_ticks >= FF_FLOOD_DROP_TOTAL_TICKS)
	{
		ff_state.active_dropship.phase = DROPSHIP_PHASE_NONE;
		ff_state.active_dropship.payload_deployed = TRUE;
		ff_state.active_dropship.squad_to_spawn = 0;
		ff_state.active_dropship.carrier = _ff_drop_carrier_spirit;
	}
}

void firefight_update_dropships_and_vehicles(void)
{
	short i;
	long now = game_time_get();

	player_vehicle_rewards_update();
	friendlies_follow_update();

	/* Drain queued reinforcement enemies as active alive enemies fall below the safe 28-actor cap */
	if (g_queued_reinforcement_enemies > 0 &&
		ff_state.wave_spawn_zone_count > 0 &&
		ff_state.enemies_alive < FIREFIGHT_SAFE_ACTIVE_ACTOR_CAP - 2)
	{
		struct game_engine_firefight_variant const *ff = &global_variant.game_engine_variant.firefight;
		struct firefight_options const *opts = firefight_options_get();
		byte wave_faction = firefight_wave_faction(ff->wave_faction, ff_state.in_bonus_round);
		short forced_team = (!opts || opts->firefight_faction_diplomacy == _firefight_diplomacy_allied_against_players) ?
			((wave_faction == _firefight_faction_unsc_humans) ? _game_team_human : _game_team_covenant) : NONE;
		short slots = (short)MIN(g_queued_reinforcement_enemies, FIREFIGHT_SAFE_ACTIVE_ACTOR_CAP - ff_state.enemies_alive);
		real_point3d player_positions[16];
		short player_count = get_living_human_player_positions(player_positions, NULL, 16);

		/* (the players may have left the wave's zones behind: a queue is
		drained over the whole wave) */
		if (player_count > 0)
		{
			real_point3d const *anchor = &player_positions[central_player_index(player_positions, player_count)];
			boolean zone_near = FALSE;
			short z;

			for (z = 0; z < ff_state.wave_spawn_zone_count; z++)
			{
				if (distance3d(&ff_state.wave_spawn_zones[z], anchor) <= FF_ZONE_NEAR_DISTANCE)
					zone_near = TRUE;
			}
			if (!zone_near)
			{
				real_point3d zones[FIREFIGHT_MAX_SPAWN_ZONES_PER_WAVE];
				real facings[FIREFIGHT_MAX_SPAWN_ZONES_PER_WAVE];
				short zone_count = select_wave_spawn_zones(
					zones,
					facings,
					FIREFIGHT_MAX_SPAWN_ZONES_PER_WAVE,
					ff_state.current_round,
					ff_state.current_wave,
					ff_state.in_bonus_round,
					firefight_wave_will_have_heavy_or_boss(
						ff_state.current_set, ff_state.current_round, ff_state.current_wave));

				for (z = 0; z < zone_count; z++)
				{
					ff_state.wave_spawn_zones[z] = zones[z];
					g_active_wave_facings[z] = facings[z];
				}
				if (zone_count > 0)
					ff_state.wave_spawn_zone_count = zone_count;
			}
		}

		for (i = 0; i < slots && g_queued_reinforcement_enemies > 0; i++)
		{
			short z = (short)((g_queued_reinforcement_enemies + i) % ff_state.wave_spawn_zone_count);
			byte subtype = g_queued_reinforcement_subtypes[0];
			short q;

			for (q = 0; q + 1 < g_queued_reinforcement_enemies; q++)
				g_queued_reinforcement_subtypes[q] = g_queued_reinforcement_subtypes[q + 1];
			g_queued_reinforcement_enemies--;

			if (spawn_subtype_actor_at(
					subtype,
					wave_faction,
					&ff_state.wave_spawn_zones[z],
					g_active_wave_facings[z],
					forced_team,
					TRUE) == NONE)
			{
				break;
			}
		}
		ff_state.enemies_queued = g_queued_reinforcement_enemies;
	}

	if (ff_state.active_dropship.phase != DROPSHIP_PHASE_NONE &&
		ff_state.active_dropship.carrier == _ff_drop_carrier_flood)
	{
		flood_drop_host_update();
	}
	else if (ff_state.active_dropship.phase != DROPSHIP_PHASE_NONE)
	{
		long v_idx = ff_state.active_dropship.vehicle_index;
		short elapsed = ff_state.active_dropship.elapsed_ticks;
		short flight_end = (short)ff_route_flight_total_ticks(&g_dropship_flight);
		real angle = ff_state.active_dropship.approach_angle;
		real cos_a = cosine(angle);
		real sin_a = sine(angle);
		struct ff_route_pose pose = ff_route_flight_pose(&g_dropship_flight, (real)elapsed);

		ff_state.active_dropship.phase = (short)pose.phase;
		ff_state.active_dropship.scale = pose.scale;
		ff_state.active_dropship.cloak = pose.cloak;
		dropship_flash(
			&g_dropship_flight,
			(real)elapsed - 1.0f < 0.0f ? 0.0f : (real)elapsed - 1.0f,
			(real)elapsed,
			&pose,
			ff_state.active_dropship.wave_faction,
			ff_state.active_dropship.is_boss_wave);

		if (pose.phase == FF_ROUTE_PHASE_HOVER)
		{
			if (!ff_state.active_dropship.payload_deployed)
			{
				long player_units[16];
				short player_count = get_living_player_positions(NULL, player_units, 16);
				real drop_facing = angle + _pi;
				real right_x = sin_a;
				real right_y = -cos_a;
				short s_idx;
				short to_unload = ff_state.active_dropship.squad_to_spawn;

				spawn_insertion_beacon_effect(
					&ff_state.active_dropship.drop_zone,
					NONE,
					ff_state.active_dropship.wave_faction,
					ff_state.active_dropship.is_boss_wave);

				for (s_idx = 0; s_idx < to_unload; s_idx++)
				{
					byte subtype = (s_idx < g_dropship_squad_subtype_count) ?
						g_dropship_squad_subtypes[s_idx] : _ff_sub_grunt;
					real side_sign = (s_idx & 1) ? 1.0f : -1.0f;
					real bay_along = ((real)(s_idx / 2) - 1.0f) * 1.4f;
					real_point3d bay_pos;
					long u_spawned;

					bay_pos.x = ff_state.active_dropship.hover_position.x +
						right_x * (side_sign * 2.1f) - cos_a * bay_along;
					bay_pos.y = ff_state.active_dropship.hover_position.y +
						right_y * (side_sign * 2.1f) - sin_a * bay_along;
					bay_pos.z = ff_state.active_dropship.drop_zone.z;
					project_point_to_ground(&bay_pos, 0.25f);

					u_spawned = spawn_subtype_actor_at(
						subtype,
						ff_state.active_dropship.wave_faction,
						&bay_pos,
						drop_facing,
						ff_state.active_dropship.forced_team,
						TRUE);
					if (u_spawned != NONE && player_count > 0)
					{
						struct unit_datum *u_dat = unit_try_and_get(u_spawned);
						if (u_dat && u_dat->unit.actor_index != NONE)
						{
							direct_actor_toward_unit(
								u_dat->unit.actor_index,
								player_units[s_idx % player_count]);
						}
					}
				}

				g_dropship_squad_subtype_count = 0;
				ff_state.active_dropship.payload_deployed = TRUE;
				ff_state.active_dropship.squad_to_spawn = 0;
			}
		}
		if (v_idx != NONE && !dropship_fly(v_idx, &pose))
			ff_state.active_dropship.vehicle_index = NONE;

		ff_state.active_dropship.elapsed_ticks++;
		if (ff_state.active_dropship.timer_ticks > 0)
			ff_state.active_dropship.timer_ticks--;

		if (ff_state.active_dropship.elapsed_ticks >= flight_end)
		{
			if (ff_state.active_dropship.vehicle_index != NONE)
			{
				object_delete(ff_state.active_dropship.vehicle_index);
				ff_state.active_dropship.vehicle_index = NONE;
			}
			ff_state.active_dropship.phase = DROPSHIP_PHASE_NONE;
			ff_state.active_dropship.payload_deployed = TRUE;
			ff_state.active_dropship.squad_to_spawn = 0;
		}
	}

	/* Clean up any enemy vehicles (Ghosts, Wraiths, Banshees, etc.) whose driver died or fell off
	   so driverless Ghosts never keep driving or firing on their own, and despawn them immediately. */
	{
		struct object_iterator v_iter;
		struct unit_datum *v_unit;
		long dead_vehicles[32];
		short dead_vehicle_count = 0;
		long new_wrecks[32];
		short new_wreck_count = 0;

		object_iterator_new(&v_iter, _object_mask_vehicle, 0);
		while ((v_unit = (struct unit_datum *)object_iterator_next(&v_iter)) != NULL && dead_vehicle_count < 32)
		{
			long v_idx = v_iter.index;
			long drv_idx;
			boolean has_living_driver = FALSE;
			boolean is_player_set_vehicle = FALSE;
			char const *v_name;
			boolean is_ghost = FALSE;
			boolean is_wreck = TEST_FLAG(v_unit->object.damage_flags, _object_dead_bit);
			short sv;

			if (v_idx == ff_state.active_dropship.vehicle_index ||
				firefight_is_generator_object(v_idx, NULL))
			{
				continue;
			}
			if (is_wreck && new_wreck_count < (short)NUMBEROF(new_wrecks) &&
				!TEST_FLAG(v_unit->object.flags, _object_cannot_be_garbage_bit))
			{
				new_wrecks[new_wreck_count++] = v_idx;
			}
			if (vehicle_has_player_rider(v_idx))
				continue;

			for (sv = 0; sv < FIREFIGHT_MAX_SET_VEHICLES; sv++)
			{
				if (ff_state.deployed_set_vehicles[sv] == v_idx)
				{
					is_player_set_vehicle = TRUE;
					break;
				}
			}

			drv_idx = v_unit->unit.driver_object_index;
			if (drv_idx != NONE)
			{
				struct unit_datum *drv = (struct unit_datum *)object_try_and_get_and_verify_type(drv_idx, _object_mask_unit);
				if (drv &&
					!TEST_FLAG(drv->object.damage_flags, _object_dead_bit) &&
					drv->object.parent_object_index == v_idx)
				{
					has_living_driver = TRUE;
				}
			}

			if (!has_living_driver)
			{
				/* Check if any living unit is still parented to this vehicle */
				long child_idx = v_unit->object.first_child_object_index;
				short safety = 0;
				while (child_idx != NONE && safety++ < 16)
				{
					struct object_datum *c_obj = object_try_and_get(child_idx);
					if (!c_obj)
						break;
					if (TEST_FLAG(_object_mask_unit, c_obj->object.type) &&
						!TEST_FLAG(c_obj->object.damage_flags, _object_dead_bit))
					{
						has_living_driver = TRUE;
						break;
					}
					child_idx = c_obj->object.next_object_index;
				}
			}

			if (!has_living_driver)
			{
				/* Immediately stop throttle and weapon triggers on any driverless vehicle */
				v_unit->unit.throttle.i = 0.0f;
				v_unit->unit.throttle.j = 0.0f;
				v_unit->unit.throttle.k = 0.0f;
				v_unit->unit.primary_trigger = 0.0f;
				SET_FLAG(v_unit->unit.control_flags, _unit_control_weapon_primary_trigger_bit, FALSE);
				SET_FLAG(v_unit->unit.control_flags, _unit_control_weapon_secondary_trigger_bit, FALSE);
				SET_FLAG(v_unit->unit.flags, _unit_actively_controlled_bit, FALSE);
				v_unit->unit.driver_object_index = NONE;
				v_unit->unit.gunner_object_index = NONE;

				v_name = tag_get_name(v_unit->definition_index);
				is_ghost = (v_name && strstr(v_name, "ghost") != NULL);

				/* (one that was destroyed stays: wreck_keep) */
				if (!is_wreck && !is_player_set_vehicle &&
					(is_ghost || v_unit->object.owner_team_index != _game_team_player))
				{
					dead_vehicles[dead_vehicle_count++] = v_idx;
				}
			}
			else if (is_wreck)
			{
				dead_vehicles[dead_vehicle_count++] = v_idx;
			}
		}

		for (i = 0; i < dead_vehicle_count; i++)
		{
			eject_all_vehicle_passengers(dead_vehicles[i]);
			if (TEST_FLAG(object_get(dead_vehicles[i])->object.damage_flags, _object_dead_bit))
				continue;
			if (enemy_vehicle_noted(dead_vehicles[i]) && vehicle_has_wreck_model(dead_vehicles[i]))
				vehicle_wreck(dead_vehicles[i]);
			else
				object_delete(dead_vehicles[i]);
		}

		for (i = 0; i < new_wreck_count; i++)
			wreck_keep(new_wrecks[i]);
	}

	/* Drive seated enemy boss vehicles every tick so Wraiths, Banshees, Ghosts, and Scorpions
	   actively steer, fly/drive toward, and fire at players even on Multiplayer maps without BSP pathfinding */
	{
		real_point3d player_positions[16];
		long player_units[16];
		short player_count = get_living_player_positions(player_positions, player_units, 16);

		if (player_count > 0)
		{
			for (i = 0; i < FIREFIGHT_MAX_TRACKED_ENEMIES; i++)
			{
				long u_idx = ff_state.tracked_enemy_units[i];
				struct unit_datum *u;
				struct unit_datum *v_unit;
				long v_idx;

				if (u_idx == NONE)
					continue;
				u = (struct unit_datum *)object_try_and_get_and_verify_type(u_idx, _object_mask_unit);
				if (!u || TEST_FLAG(u->object.damage_flags, _object_dead_bit) || u->object.parent_object_index == NONE)
					continue;

				v_idx = u->object.parent_object_index;
				v_unit = (struct unit_datum *)object_try_and_get_and_verify_type(v_idx, _object_mask_vehicle);
				if (!v_unit || TEST_FLAG(v_unit->object.damage_flags, _object_dead_bit))
					continue;

				if (v_unit->unit.driver_object_index != u_idx)
				{
					v_unit->unit.driver_object_index = u_idx;
					v_unit->unit.gunner_object_index = u_idx;
					SET_FLAG(v_unit->unit.flags, _unit_actively_controlled_bit, TRUE);
				}
				object_activate(v_idx);

				/* Reset triggers each tick before deciding whether to fire */
				SET_FLAG(u->unit.control_flags, _unit_control_weapon_primary_trigger_bit, FALSE);
				SET_FLAG(v_unit->unit.control_flags, _unit_control_weapon_primary_trigger_bit, FALSE);
				u->unit.primary_trigger = 0.0f;
				v_unit->unit.primary_trigger = 0.0f;

				{
					real_point3d v_pos;
					short best_p = 0;
					real best_dist_sq = 999999.0f;
					short p;
					real dist;
					real_vector3d to_target;

					object_get_origin(v_idx, &v_pos);
					for (p = 0; p < player_count; p++)
					{
						real d_sq = distance_squared3d(&v_pos, &player_positions[p]);
						if (d_sq < best_dist_sq)
						{
							best_dist_sq = d_sq;
							best_p = p;
						}
					}

					dist = square_root(MAX(0.0f, best_dist_sq));
					vector_from_points3d(&v_pos, &player_positions[best_p], &to_target);
					to_target.k += 0.35f;
					if (normalize3d(&to_target) > 0.0f)
					{
						char const *v_name = tag_get_name(v_unit->definition_index);
						boolean is_banshee = (v_name && strstr(v_name, "banshee") != NULL);
						real_vector3d move_facing = to_target;

						u->unit.desired_aiming_vector = to_target;
						u->unit.desired_looking_vector = to_target;
						v_unit->unit.desired_aiming_vector = to_target;
						v_unit->unit.desired_looking_vector = to_target;

						if (!is_banshee)
							move_facing.k = 0.0f;
						if (normalize3d(&move_facing) > 0.0f)
						{
							u->unit.desired_facing_vector = move_facing;
							v_unit->unit.desired_facing_vector = move_facing;
						}

						if (magnitude_squared3d(&u->unit.throttle) < 0.01f)
						{
							if (is_banshee)
							{
								u->unit.throttle.i = 1.0f;
								u->unit.throttle.j = 0.0f;
								u->unit.throttle.k = (v_pos.z < player_positions[best_p].z + 4.0f) ? 0.35f : -0.1f;
							}
							else if (dist > 11.0f)
							{
								u->unit.throttle.i = 0.90f;
								u->unit.throttle.j = 0.0f;
								u->unit.throttle.k = 0.0f;
							}
							else if (dist < 5.5f)
							{
								u->unit.throttle.i = -0.45f;
								u->unit.throttle.j = 0.25f;
								u->unit.throttle.k = 0.0f;
							}
							else
							{
								u->unit.throttle.i = 0.20f;
								u->unit.throttle.j = ((now / 45) & 1) ? 0.55f : -0.55f;
								u->unit.throttle.k = 0.0f;
							}
							v_unit->unit.throttle = u->unit.throttle;
						}

						if (dist < 42.0f &&
							dot_product3d(&v_unit->object.forward, &to_target) > 0.55f &&
							(now % 45) < 28)
						{
							SET_FLAG(u->unit.control_flags, _unit_control_weapon_primary_trigger_bit, TRUE);
							SET_FLAG(v_unit->unit.control_flags, _unit_control_weapon_primary_trigger_bit, TRUE);
							u->unit.primary_trigger = 1.0f;
							v_unit->unit.primary_trigger = 1.0f;
						}
					}
				}
			}
		}
	}

	/* Refresh enemy perception and full-map pursuit of nearest living player or generator,
	   staggered across 15 ticks (~2 actors per tick) to eliminate per-second AI pathfinding spikes */
	if (now != g_last_aggro_refresh_tick)
	{
		real_point3d player_positions[16];
		long player_units[16];
		short player_count = get_living_player_positions(player_positions, player_units, 16);
		long open_gen_units[FIREFIGHT_MAX_GENERATORS];
		real_point3d open_gen_positions[FIREFIGHT_MAX_GENERATORS];
		short open_gen_count = 0;
		short g;
		short tick_mod = (short)((now >= 0 ? now : -now) % 15);

		for (g = 0; g < ff_state.generator_count && g < FIREFIGHT_MAX_GENERATORS; g++)
		{
			struct firefight_generator_state const *gen = &ff_state.generators[g];
			if (gen->alive && gen->lockdown_ticks == 0 && gen->object_index != NONE &&
				object_try_and_get(gen->object_index) != NULL)
			{
				open_gen_units[open_gen_count] = gen->object_index;
				open_gen_positions[open_gen_count] = gen->position;
				open_gen_count++;
			}
		}

		g_last_aggro_refresh_tick = now;
		if (player_count > 0)
		{
			for (i = 0; i < FIREFIGHT_MAX_TRACKED_ENEMIES; i++)
			{
				long u_idx;
				if (((i + tick_mod) % 15) != 0)
					continue;
				u_idx = ff_state.tracked_enemy_units[i];
				g_generator_attack_units[i] = NONE;
				if (u_idx != NONE)
				{
					struct unit_datum *u = (struct unit_datum *)object_try_and_get_and_verify_type(
						u_idx, _object_mask_unit);
					if (u && !TEST_FLAG(u->object.damage_flags, _object_dead_bit) && u->unit.actor_index != NONE)
					{
						real_point3d u_pos;
						long chosen_target = NONE;
						short best_p = 0;
						real best_p_dist_sq = 999999.0f;
						short p;

						object_get_origin(u_idx, &u_pos);

						for (p = 0; p < player_count; p++)
						{
							real d = distance_squared3d(&u_pos, &player_positions[p]);
							if (d < best_p_dist_sq)
							{
								best_p_dist_sq = d;
								best_p = p;
							}
						}

						/* GENERATOR ATTACKERS' share goes for the nearest unlocked generator
						   even under fire, turning only on a player right beside it */
						if (open_gen_count > 0 && generator_attacker_slot(i) &&
							best_p_dist_sq > 6.0f * 6.0f)
						{
							short best_g = 0;
							real best_g_dist_sq = 999999.0f;
							for (g = 0; g < open_gen_count; g++)
							{
								real d_sq = distance_squared3d(&u_pos, &open_gen_positions[g]);
								if (d_sq < best_g_dist_sq)
								{
									best_g_dist_sq = d_sq;
									best_g = g;
								}
							}
							chosen_target = open_gen_units[best_g];
							g_generator_attack_units[i] = chosen_target;
						}

						if (chosen_target == NONE)
							chosen_target = player_units[best_p];
						/* (an objective's carrier, the oddball's or the
						flag's, is every enemy's: firefight_objectives.c) */
						{
							long carrier = firefight_objectives_priority_target();

							if (carrier != NONE)
								chosen_target = carrier;
						}

						direct_actor_toward_unit(u->unit.actor_index, chosen_target);
					}
				}
			}
		}
	}
}

/* (firefight.c, the bonus round over) its enemies gone, not killed: each in
the teleport's flash, with its vehicle when no player is in it; untracked */
void firefight_teleport_out_tracked_enemies(void)
{
	short slot;

	for (slot = 0; slot < FIREFIGHT_MAX_TRACKED_ENEMIES; slot++)
	{
		long unit_index = ff_state.tracked_enemy_units[slot];
		struct unit_datum *unit = unit_index != NONE ? unit_try_and_get(unit_index) : NULL;

		ff_state.tracked_enemy_units[slot] = NONE;
		ff_state.tracked_enemy_tiers[slot] = 0;
		if (!unit)
			continue;
		{
			real_point3d at;
			long vehicle = unit->object.parent_object_index;

			object_get_origin(unit_index, &at);
			spawn_insertion_beacon_effect(&at, NONE, _firefight_faction_covenant, FALSE);
			object_delete(unit_index);
			if (vehicle != NONE && object_try_and_get_and_verify_type(vehicle, _object_mask_vehicle))
			{
				struct object_iterator iterator;
				struct object_datum *rider;
				boolean player_in = FALSE;

				object_iterator_new(&iterator, _object_mask_unit, 0);
				while ((rider = (struct object_datum *)object_iterator_next(&iterator)) != NULL)
				{
					if (rider->object.parent_object_index == vehicle &&
						((struct unit_datum *)rider)->unit.player_index != NONE)
					{
						player_in = TRUE;
					}
				}
				if (!player_in)
					object_delete(vehicle);
			}
		}
	}
	ff_state.enemies_alive = 0;
}

/* ---------- An objective's units (firefight_objectives.c)

Made as a wave's are, of the waves' faction and with their weapons, but no
wave's: not tracked (a wave's clear does not wait for them), not sent at the
players (firefight_objectives.c keeps them where it wants them), and their
team the waves' */

/* the waves' team, as firefight_spawn_wave has it: its faction's when the
enemies are allied, else none (each unit its tag's) */
static short objective_enemy_team(byte wave_faction)
{
	struct firefight_options const *opts = firefight_options_get();

	if (!opts || opts->firefight_faction_diplomacy == _firefight_diplomacy_allied_against_players)
		return wave_faction == _firefight_faction_unsc_humans ? _game_team_human : _game_team_covenant;
	return NONE;
}

/* whether a faction has the Covenant's ranks (an Elite Zealot, a Major) */
static boolean objective_faction_covenant(byte wave_faction)
{
	switch (wave_faction)
	{
	case _firefight_faction_covenant:
	case _firefight_faction_elites_heavies:
	case _firefight_faction_mixed:
	case _firefight_faction_covenant_flood:
	case _firefight_faction_covenant_sentinels:
		return TRUE;
	default:
		return FALSE;
	}
}

long firefight_objective_spawn_enemy(real_point3d const *at, real facing, short rank)
{
	byte faction = global_variant.game_engine_variant.firefight.wave_faction;
	short team = objective_enemy_team(faction);
	long unit_index = NONE;

	if (!at || !ai_globals || !ai_globals->ai_initialized_for_map)
		return NONE;
	g_objective_spawning = TRUE;
	/* (a target's: the highest rank its faction has) */
	if (rank >= _firefight_objective_rank_target && objective_faction_covenant(faction))
	{
		static byte const target_subtypes[] = { _ff_sub_elite_zealot, _ff_sub_elite_major };
		short i;

		for (i = 0; i < (short)NUMBEROF(target_subtypes) && unit_index == NONE; i++)
			unit_index = spawn_subtype_actor_at(target_subtypes[i], faction, at, facing, team, FALSE);
	}
	if (unit_index == NONE)
	{
		byte tier = _ff_tier_infantry;
		long variant = pick_wave_enemy_variant(faction, FALSE, rank >= _firefight_objective_rank_heavy, &tier);

		unit_index = variant != NONE ? spawn_single_actor_at(variant, tier, at, facing, team, FALSE) : NONE;
		if (unit_index != NONE)
		{
			if (firefight_enemy_weapons_borrowed())
				firefight_arm_wave_enemy(unit_index);
			else
				firefight_arm_wave_enemy_map(unit_index);
		}
	}
	g_objective_spawning = FALSE;
	return unit_index;
}

long firefight_objective_spawn_ally(real_point3d const *at, real facing)
{
	long candidates[MAX_ROSTER_VARIANTS];
	short count = 0;
	short i;

	if (!at || !ai_globals || !ai_globals->ai_initialized_for_map)
		return NONE;
	for (i = 0; i < g_roster_count; i++)
	{
		if (ff_state.elite_player_mode ?
			(!g_roster[i].is_human_ally_or_enemy && g_roster[i].faction == _firefight_faction_covenant) :
			g_roster[i].is_human_ally_or_enemy)
		{
			candidates[count++] = g_roster[i].variant_tag_index;
		}
	}
	if (!count)
		return NONE;
	return spawn_single_actor_at(candidates[spawner_rand() % (unsigned long)count], _ff_tier_infantry, at, facing,
		ff_state.elite_player_mode ? _game_team_covenant : _game_team_human, FALSE);
}

boolean firefight_objective_allies_available(void)
{
	short i;

	for (i = 0; i < g_roster_count; i++)
	{
		if (ff_state.elite_player_mode ?
			(!g_roster[i].is_human_ally_or_enemy && g_roster[i].faction == _firefight_faction_covenant) :
			g_roster[i].is_human_ally_or_enemy)
		{
			return TRUE;
		}
	}
	return FALSE;
}

long firefight_objective_spawn_vehicle(real_point3d const *at, real facing, boolean heavy)
{
	byte faction = global_variant.game_engine_variant.firefight.wave_faction;
	short team = objective_enemy_team(faction);
	long vehicle_index;

	if (faction == _firefight_faction_flood)
	{
		error(_error_silent, "firefight: an objective's enemy vehicle: none for the Flood (no drivers)");
		return NONE;
	}
	if (!at || !ai_globals || !ai_globals->ai_initialized_for_map)
		return NONE;
	g_objective_spawning = TRUE;
	vehicle_index = spawn_specific_enemy_vehicle_at(heavy, at, facing, team != NONE ? team : _game_team_covenant, TRUE);
	g_objective_spawning = FALSE;
	if (vehicle_index == NONE)
		error(_error_silent, "firefight: an objective's enemy vehicle at (%.1f %.1f %.1f) wasn't made", at->x, at->y, at->z);
	return vehicle_index;
}

/* the map's vehicle placements within `radius` of `center`, at most
`maximum`: where, and which way */
short firefight_objective_vehicle_spots(real_point3d const *center, real radius, real_point3d *points, real *facings,
	short maximum)
{
	struct scenario *scen = global_scenario_try_and_get();
	short count = 0;
	short i;

	if (!scen || !center)
		return 0;
	for (i = 0; i < scen->vehicles.count && count < maximum; i++)
	{
		struct ff_scenario_vehicle *placement = TAG_BLOCK_GET_ELEMENT(&scen->vehicles, i, struct ff_scenario_vehicle);
		short palette_index = placement->object.palette_entry_index;
		struct scenario_object_palette_entry *palette;
		real_point3d spot = placement->object.position;
		short tries;

		if (!VALID_INDEX(palette_index, scen->vehicle_palette.count))
			continue;
		palette = TAG_BLOCK_GET_ELEMENT(&scen->vehicle_palette, palette_index, struct scenario_object_palette_entry);
		if (palette->reference.index == NONE || !map_vehicle_tag_drivable(tag_get_name(palette->reference.index)) ||
			!point_is_in_valid_bsp(&spot) || distance3d(&spot, center) > radius)
		{
			continue;
		}
		/* (a players' vehicle there: beside it) */
		for (tries = 0; tries < 4 && !map_vehicle_spot_clear(&spot); tries++)
		{
			real angle = placement->object.rotation.yaw + (real)tries * (_pi * 0.5f);

			spot = placement->object.position;
			spot.x += cosine(angle) * 5.0f;
			spot.y += sine(angle) * 5.0f;
		}
		if (!map_vehicle_spot_clear(&spot) || !spot_reachable_from(&placement->object.position, &spot))
			continue;
		points[count] = spot;
		facings[count] = placement->object.rotation.yaw;
		count++;
	}
	return count;
}

/* the ground under a point: its surface (an actor's path's), NONE for none
there; whether a spot is this side of the structure from another, with
ground; and the walkable group of the ground under a point (NONE: none), the
same for two points a unit walks between */
long firefight_objective_ground(real_point3d const *point, real_point3d *ground)
{
	return reach_ground(point, ground);
}

boolean firefight_objective_reachable(real_point3d const *from, real_point3d const *to)
{
	return spot_reachable_from(from, to);
}

long firefight_objective_reach_group(real_point3d const *point)
{
	if (!g_reach.parent)
		reach_build();
	return reach_group_at(point);
}

/* an objective's unit sent at another (a player, a carrier), as a wave's
enemy is */
void firefight_objective_direct_at(long actor_index, long target_unit_index)
{
	direct_actor_toward_unit(actor_index, target_unit_index);
}

/* a camp's unit kept to it: defending (no charge at what it sees, whatever
its actor's ALWAYS CHARGE, which Firefight's pursuit sets), its orders
firefight_objectives.c's moves back into the camp */
void firefight_objective_hold_ground(long actor_index, boolean hold)
{
	struct actor_datum *actor = actor_index != NONE ? actor_try_and_get(actor_index) : NULL;

	if (!actor)
		return;
	actor->external_orders.defending = hold;
	actor->external_orders.disable_charging = hold;
	if (hold)
		actor->emotions.forced_to_charge = FALSE;
}

/* Collect's deposit's fire: the first the map has of a burning effect (the
game's names, else any effect named so), harmless as the pods' are; none
found, the teleport's glow */
static long g_objective_fire_tag = NONE;
static long g_objective_fire_scenario = NONE;

void firefight_objective_fire_effect(real_point3d const *position)
{
	static char const *const names[] =
	{
		"effects\\burning large", "effects\\burning medium", "effects\\burning small", "effects\\burning",
		"weapons\\flamethrower\\effects\\flame", "scenery\\emitters\\burning_flame\\effects\\burning",
	};

	if (!position)
		return;
	if (g_objective_fire_scenario != global_scenario_index)
	{
		struct tag_iterator iterator;
		long tag_index;
		short i;

		g_objective_fire_scenario = global_scenario_index;
		g_objective_fire_tag = NONE;
		for (i = 0; i < (short)NUMBEROF(names) && g_objective_fire_tag == NONE; i++)
			g_objective_fire_tag = tag_loaded(EFFECT_DEFINITION_TAG, names[i]);
		tag_iterator_new(&iterator, EFFECT_DEFINITION_TAG);
		while (g_objective_fire_tag == NONE && (tag_index = tag_iterator_next(&iterator)) != NONE)
		{
			char lowered[256];

			lowercase_copy(tag_get_name(tag_index), lowered, sizeof(lowered));
			if ((strstr(lowered, "burning") || strstr(lowered, "flame")) && !strstr(lowered, "explo") &&
				!strstr(lowered, "impact") && !strstr(lowered, "death"))
			{
				g_objective_fire_tag = tag_index;
			}
		}
		if (g_objective_fire_tag == NONE)
			g_objective_fire_tag = tag_loaded(EFFECT_DEFINITION_TAG, "effects\\coop teleport");
		error(_error_silent, "firefight objectives: the deposit's fire is %s",
			g_objective_fire_tag != NONE ? tag_get_name(g_objective_fire_tag) : "none");
	}
	if (g_objective_fire_tag == NONE)
		return;
	pod_effect(tag_get_name(g_objective_fire_tag), position);
}

/* the vehicle types the players could be given (drivable, the game's or
the map's own MAP CONTENT takes), at most `maximum` */
short firefight_objective_vehicle_types(long *tags, short maximum)
{
	struct tag_iterator iterator;
	long tag_index;
	short count = 0;

	tag_iterator_new(&iterator, VEHICLE_DEFINITION_TAG);
	while ((tag_index = tag_iterator_next(&iterator)) != NONE && count < maximum)
	{
		char lowered[256];

		lowercase_copy(tag_get_name(tag_index), lowered, sizeof(lowered));
		if (strstr(lowered, "turret") || strstr(lowered, "dropship") || strstr(lowered, "pelican") ||
			strstr(lowered, "spirit") || !player_vehicle_tag_allowed(tag_index))
		{
			continue;
		}
		tags[count++] = tag_index;
	}
	return count;
}

/* a vehicle of a type for the players: one already there and empty of
enemies, else one made at the map's nearest free spot of that type, else by
the players; NONE for none */
long firefight_objective_give_vehicle(long vehicle_tag)
{
	struct object_iterator iterator;
	struct object_datum *object;
	struct object_placement_data placement;
	real_point3d players[16];
	short player_count = get_living_human_player_positions(players, NULL, 16);
	real_point3d position;
	real facing = 0.0f;
	long vehicle_index;

	if (vehicle_tag == NONE)
		return NONE;
	object_iterator_new(&iterator, _object_mask_vehicle, 0);
	while ((object = (struct object_datum *)object_iterator_next(&iterator)) != NULL)
	{
		if (object->definition_index == vehicle_tag && !TEST_FLAG(object->object.damage_flags, _object_dead_bit) &&
			!enemy_vehicle_noted(iterator.index))
		{
			return iterator.index;
		}
	}
	if (!map_vehicle_spot(vehicle_tag, &position, &facing))
	{
		short tries;

		if (!player_count)
			return NONE;
		position = players[central_player_index(players, player_count)];
		for (tries = 0; tries < 8; tries++)
		{
			real angle = spawner_rand_range(0.0f, 2.0f * _pi);
			real_point3d spot = position;
			real_point3d ground;

			spot.x += cosine(angle) * 6.0f;
			spot.y += sine(angle) * 6.0f;
			if (spot_reachable_from(&position, &spot) && reach_ground(&spot, &ground) != NONE &&
				map_vehicle_spot_clear(&ground))
			{
				position = ground;
				facing = angle;
				break;
			}
		}
		project_point_to_ground(&position, 0.75f);
	}
	object_placement_data_new(&placement, vehicle_tag, NONE);
	placement.position = position;
	vector3d_from_angle(&placement.forward, facing);
	placement.owner_team_index = _game_team_player;
	/* (Firefight's own, whatever the game type's vehicle set, which only placements follow) */
	game_engine_vehicle_always_spawn = TRUE;
	vehicle_index = object_new(&placement);
	game_engine_vehicle_always_spawn = FALSE;
	if (vehicle_index != NONE)
		object_set_automatic_deactivation(vehicle_index, FALSE);
	return vehicle_index;
}

/* (the host's: Bomb's going off) a big blast's damage, a tank shell's (else
a rocket's, a grenade's: the first the map has) at a point and four round it
`spread` off, hurting all it reaches, the players too; the looks are each
machine's (firefight_objective_effect) */
void firefight_objective_blast_damage(real_point3d const *at, real spread)
{
	static char const *const names[] =
	{
		"vehicles\\scorpion\\shell explosion", "vehicles\\scorpion\\tank shell explosion",
		"vehicles\\wraith\\wraith mortar explosion", "weapons\\rocket launcher\\explosion",
		"weapons\\frag grenade\\explosion", "weapons\\plasma grenade\\explosion"
	};
	long damage_tag = NONE;
	short index;

	if (!at || network_game_distributed_client())
		return;
	for (index = 0; index < (short)NUMBEROF(names) && damage_tag == NONE; index++)
		damage_tag = tag_loaded(DAMAGE_EFFECT_DEFINITION_TAG, names[index]);
	if (damage_tag == NONE)
	{
		struct tag_iterator iterator;
		long tag_index;

		tag_iterator_new(&iterator, DAMAGE_EFFECT_DEFINITION_TAG);
		while (damage_tag == NONE && (tag_index = tag_iterator_next(&iterator)) != NONE)
		{
			char lowered[256];

			lowercase_copy(tag_get_name(tag_index), lowered, sizeof(lowered));
			if (strstr(lowered, "explosion") && (strstr(lowered, "shell") || strstr(lowered, "rocket")))
				damage_tag = tag_index;
		}
	}
	error(_error_silent, "firefight objectives: the bomb's blast deals %s",
		damage_tag != NONE ? tag_get_name(damage_tag) : "nothing (no explosion's damage here)");
	if (damage_tag == NONE)
		return;
	for (index = 0; index < 5; index++)
	{
		struct damage_data damage;
		real_point3d point = *at;

		if (index)
		{
			real angle = (real)(index - 1) * 0.5f * _pi;

			point.x += cosine(angle) * spread;
			point.y += sine(angle) * spread;
		}
		point.z += 0.5f;
		damage_data_new(&damage, damage_tag);
		damage.origin = damage.epicenter = point;
		scenario_location_from_point(&damage.location, &point);
		area_of_effect_cause_damage(&damage, NONE);
	}
}

/* an effect on this machine alone (as the pods' are), the first of `names`
the map has */
void firefight_objective_effect(char const *const *names, short count, real_point3d const *position)
{
	short i;

	for (i = 0; i < count; i++)
	{
		if (tag_loaded(EFFECT_DEFINITION_TAG, names[i]) != NONE)
		{
			pod_effect(names[i], position);
			return;
		}
	}
}
