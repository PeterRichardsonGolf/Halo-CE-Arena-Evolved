/*
FORGE_CACHE_BUILDER.H

Standalone and runtime Custom Edition (v609) and Halo PC (v7) cache parser,
schema-driven tag cloner, PC-to-CE bitmap/sound internalizer, gbxmodel
geometry packer, scenario modifier, and "Save Copy As..." exporter (.map,
140x114 .bmp, and .txt).
*/

#ifndef __FORGE_CACHE_BUILDER_H
#define __FORGE_CACHE_BUILDER_H
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- constants */

#define FORGE_MAX_PLACED_OBJECTS 2048
#define FORGE_MAX_PLAYER_SPAWNS 512
#define FORGE_MAX_NETGAME_FLAGS 512
#define FORGE_MAX_NETGAME_EQUIPMENT 200
#define FORGE_MAX_AI_SPAWNS 256
#define FORGE_MAX_AI_ACTORS 64
#define FORGE_MAX_DEFAULT_OBJECT_EDITS 1024
#define FORGE_TAG_PATH_MAX 256

enum forge_placed_object_category
{
	_forge_object_scenery = 0,
	_forge_object_vehicle,
	_forge_object_weapon,
	_forge_object_equipment,
	_forge_object_machine,
	_forge_object_control,
	_forge_object_light_fixture,
	_forge_object_biped,
	NUMBER_OF_FORGE_OBJECT_CATEGORIES
};

enum forge_ai_team_designator
{
	_forge_ai_team_friendly = 0, /* Allies with players (_game_team_player / _game_team_human) */
	_forge_ai_team_enemy_covenant = 1, /* Hostile Covenant (_game_team_covenant) */
	_forge_ai_team_enemy_flood = 2, /* Hostile Flood (_game_team_flood) */
	_forge_ai_team_enemy_sentinel = 3 /* Hostile Sentinel (_game_team_sentinel) */
};

/* ---------- structures */

struct forge_placed_object_entry
{
	int32_t category; /* enum forge_placed_object_category */
	uint32_t group_tag; /* 'scen', 'vehi', 'weap', 'eqip', 'mach', 'ctrl', 'lifi', 'bipd' */
	char tag_path[FORGE_TAG_PATH_MAX];
	float x, y, z;
	float yaw, pitch, roll;
	int16_t team_index;
	int16_t bsp_index;
	uint16_t placement_flags;
	uint16_t gametype_flags; /* 0 = All Modes, bit0=CTF, bit1=Slayer, bit2=Oddball, bit3=KOTH, bit4=Race */
	uint8_t source_cache_id;
	uint8_t active;
	/* (a vehicle's ALWAYS SPAWN: forge_live_instance's, saved as the
	OBJECT_ALWAYS_SPAWN line before its OBJECT line) */
	uint8_t always_spawn;
	/* (an emitter's DEAL DAMAGE off: forge_live_instance's, saved as the
	OBJECT_NO_DAMAGE line before its OBJECT line) */
	uint8_t no_damage;
	/* (a scenery's COLLISION off, saved as the OBJECT_NO_COLLISION line before
	its OBJECT line) */
	uint8_t no_collision;
	/* (its SCALE, 0 for none: forge_live_instance's, saved as the
	OBJECT_SCALE line before its OBJECT line) */
	float scale;
};

struct forge_default_object_edit
{
	uint32_t group_tag;
	uint8_t deleted;
	uint8_t active;
	uint16_t gametype_flags; /* 0 = All Modes, bit0=CTF, bit1=Slayer, bit2=Oddball, bit3=KOTH, bit4=Race */
	char tag_path[FORGE_TAG_PATH_MAX];
	float orig_x, orig_y, orig_z;
	float new_x, new_y, new_z;
	float new_yaw, new_pitch, new_roll;
};

struct forge_player_spawn_entry
{
	float x, y, z;
	float yaw;
	int16_t team_index; /* 0 = Red, 1 = Blue, -1 = FFA/Any */
	int16_t bsp_index;
	int16_t type0; /* game mode filter (0 = all, 1 = ctf, 2 = slayer, 3 = oddball, 4 = king, 5 = race) */
	int16_t type1;
	int16_t type2;
	int16_t type3;
};

struct forge_netgame_flag_entry
{
	float x, y, z;
	float yaw;
	int16_t type; /* _netgame_flag_ctf_flag, _netgame_flag_hill_flag, _netgame_flag_oddball_ball_spawn, _netgame_flag_race_track, _netgame_flag_teleport_from, _netgame_flag_teleport_to */
	int16_t usage_id; /* team index (0=Red, 1=Blue) or teleporter pair ID / hill order */
	char weapon_group_path[FORGE_TAG_PATH_MAX];
};

struct forge_netgame_equipment_entry
{
	uint32_t flags; /* bit 0 = levitate */
	int16_t type0;
	int16_t type1;
	int16_t type2;
	int16_t type3;
	int16_t team_index;
	int16_t spawn_time_seconds;
	float x, y, z;
	float yaw;
	char item_collection_path[FORGE_TAG_PATH_MAX]; /* 'itmc' tag path */
};

/* an AI spawn, a Forge marker: the map's own starting location of a squad
(encounter_index on), or one of its own, whose squad the level makes for it
(forge_ai_encounters_apply). The map's own takes its squad's numbers until one
is changed: then it is `detached`, and has its own squad too */
struct forge_ai_spawn_entry
{
	char actor_variant_path[FORGE_TAG_PATH_MAX]; /* 'actv' tag path */
	char weapon_path[FORGE_TAG_PATH_MAX]; /* 'weap' tag path, empty for the AI's own */
	float x, y, z;
	float yaw;
	int16_t team; /* the game's team (_game_team_*), 0: the unit's own */
	int16_t unit_count; /* placed when the game starts */
	int16_t respawn_alive; /* kept alive by respawning, 0: no respawning */
	int16_t respawn_delay_seconds;
	int16_t respawn_total; /* 0: endless */
	uint16_t gametype_flags; /* 0 = all but Firefight, bit0=CTF, bit1=Slayer, bit2=Oddball, bit3=KOTH, bit4=Race, bit5=Firefight */
	int16_t encounter_index; /* the map's own starting location, or -1 */
	int16_t squad_index;
	int16_t location_index;
	uint8_t detached;
	uint8_t active;
	uint8_t spawn_effect; /* forge_ai.c's s_ai_spawn_effects: as each unit appears */
	uint8_t cloaked; /* its units cloaked as they appear, and kept so (they flicker as they fire, as a cloaked elite does) */
	uint8_t hover_height; /* a flying unit kept this many world units over the ground (sentinels, drones), 0: as it flies */
	uint8_t goal; /* enum forge_ai_goal (forge_engine.h): what its units do out of a fight */
	uint8_t goal_link; /* PATROL VOLUME's or ROUTE's number */
	uint8_t goal_who; /* HUNT's and FOLLOW's players: any, red or blue; top bit the SPEED's RUN (FORGE_AI_GOAL_RUN_BIT) */
	uint8_t goal_start; /* HUNT and FOLLOW move once their player is farther than this (world units) */
	uint8_t goal_follow; /* GO TO PLAYER's STOP DISTANCE, a friendly team's (world units) */
	uint8_t goal_alert; /* ALERT TIME: seconds its squad stays alert once one is hurt or dies (0: FORGE_AI_GOAL_ALERT_DEFAULT) */
};

/* a campaign tag a saved map's spawns use: copied into the level as it loads,
on every machine, from the map named (randomizer.c) */
struct forge_ai_actor_entry
{
	uint32_t group_tag; /* 'actv' or 'weap' */
	char path[FORGE_TAG_PATH_MAX];
	char map[32]; /* the campaign map it is copied from */
};

struct forge_scenario_edit_state
{
	/* If true, replaces the base map's existing spawns/flags/equipment/objects with these arrays;
	   if false, appends these entries onto the base map's existing scenario blocks. */
	int replace_player_spawns;
	int replace_netgame_flags;
	int replace_netgame_equipment;
	int replace_placed_objects;
	int replace_ai_spawns; /* the map's own squads are in ai_spawns: one left out is gone */
	int ai_spawns_edited; /* (nothing is saved of a map's AI that was not touched) */

	int32_t player_spawn_count;
	struct forge_player_spawn_entry player_spawns[FORGE_MAX_PLAYER_SPAWNS];

	int32_t netgame_flag_count;
	struct forge_netgame_flag_entry netgame_flags[FORGE_MAX_NETGAME_FLAGS];

	int32_t netgame_equipment_count;
	struct forge_netgame_equipment_entry netgame_equipment[FORGE_MAX_NETGAME_EQUIPMENT];

	int32_t placed_object_count;
	struct forge_placed_object_entry placed_objects[FORGE_MAX_PLACED_OBJECTS];

	int32_t ai_spawn_count;
	struct forge_ai_spawn_entry ai_spawns[FORGE_MAX_AI_SPAWNS];

	int32_t ai_actor_count;
	struct forge_ai_actor_entry ai_actors[FORGE_MAX_AI_ACTORS];

	int32_t default_edit_count;
	struct forge_default_object_edit default_edits[FORGE_MAX_DEFAULT_OBJECT_EDITS];
};

/* Summary of a map's 2D floorplan & existing placements (read by the map exporter) */
struct forge_bsp_edge_2d
{
	float x0, y0, z0;
	float x1, y1, z1;
};

struct forge_map_blueprint_data
{
	char map_name[64];
	int16_t scenario_type; /* 0 = solo/co-op, 1 = multiplayer */
	float min_x, max_x;
	float min_y, max_y;
	float min_z, max_z;

	int32_t edge_count;
	struct forge_bsp_edge_2d *edges; /* heap-allocated array */

	struct forge_scenario_edit_state initial_state;
};

/* ---------- prototypes */

/* Exports a modified scenario map to custom_maps/<output_map_name>.map (.bmp, .txt) ("Save Copy As..." UI action) */
int forge_export_scenario_map(
	char const *base_map_path,
	struct forge_scenario_edit_state const *edits,
	char const *output_map_name);

/* Saves a map made on an MCC multiplayer map (version 13) to `output_map_path`
(an Xbox path, d:\mcc_maps\forge\<name>.map): `base_map_path`'s file as it is,
with Forge's trailer, whose "Base Map:" is `base_level` (mcc_maps\<package>\<name>).
None of the base map's resource files are copied: the saved map reads them as
the base map does (mcc_package.c's mcc_package_forge_base). */
int forge_export_mcc_scenario_map(
	char const *base_map_path,
	char const *base_level,
	struct forge_scenario_edit_state const *edits,
	char const *output_map_name,
	char const *output_map_path);

/* Loads 2D floorplan collision edges and initial scenario placements from any CE (v609) or PC (v7) .map file */
int forge_load_map_blueprint(
	char const *map_path,
	struct forge_map_blueprint_data *out_blueprint);

void forge_free_map_blueprint(
	struct forge_map_blueprint_data *blueprint);

#ifdef __cplusplus
}
#endif

#endif /* __FORGE_CACHE_BUILDER_H */
