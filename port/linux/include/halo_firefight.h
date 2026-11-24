/*
HALO_FIREFIGHT.H

port: Halo: Reach Firefight public API header, expanded variant/settings
structures, enums, state machine globals, and minimal hook declarations for
the self-contained C modules in port/linux/game/:
- firefight.c
- firefight_spawner.c
- firefight_generators.c
- firefight_hud.c
- firefight_stats.c
- firefight_network.c
*/

#ifndef __HALO_FIREFIGHT_H
#define __HALO_FIREFIGHT_H
#pragma once

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "halo_port_limits.h"

/* ---------- constants & enums */

/* firefight_options' firefight_plasma_flinch: its value set by this build,
0 (OFF) to 4 (100%) under it */
#define FIREFIGHT_PLASMA_FLINCH_SET 0x80

/* Halo: Reach Firefight 13 core progression skulls (mapped onto the 40-skull engine) */
enum firefight_skull_bit
{
	_firefight_skull_iron_bit = 0,
	_firefight_skull_black_eye_bit,
	_firefight_skull_tough_luck_bit,
	_firefight_skull_catch_bit,
	_firefight_skull_cloud_bit,
	_firefight_skull_famine_bit,
	_firefight_skull_tilt_bit,
	_firefight_skull_mythic_bit,
	_firefight_skull_thunderstorm_bit,
	_firefight_skull_blind_bit,
	_firefight_skull_boom_bit,
	_firefight_skull_cowbell_bit,
	_firefight_skull_grunt_birthday_party_bit,
	NUMBER_OF_FIREFIGHT_SKULLS
};

enum
{
	_firefight_difficulty_easy = 0,
	_firefight_difficulty_normal,
	_firefight_difficulty_heroic,
	_firefight_difficulty_legendary,
	NUMBER_OF_FIREFIGHT_DIFFICULTIES
};

/* Feature 1 & 6: Expanded Enemy Faction Toggles */
enum
{
	_firefight_faction_covenant = 0,       /* Covenant Only (Default) */
	_firefight_faction_mixed,              /* All Combined */
	_firefight_faction_flood,              /* Flood Only */
	_firefight_faction_grunts,             /* Grunts Only */
	_firefight_faction_elites_heavies,     /* Elites & Heavies */
	_firefight_faction_sentinels,          /* Sentinels Only */
	_firefight_faction_covenant_flood,     /* Covenant + Flood */
	_firefight_faction_covenant_sentinels, /* Covenant + Sentinels */
	_firefight_faction_flood_sentinels,    /* Flood + Sentinels */
	_firefight_faction_unsc_humans,        /* UNSC Humans */
	NUMBER_OF_FIREFIGHT_FACTIONS
};

/* Feature 1: Enemy Inter-Faction Diplomacy */
enum
{
	_firefight_diplomacy_allied_against_players = 0, /* Default: Covenant/Flood/Sentinels ally vs players */
	_firefight_diplomacy_classic_infighting,         /* Classic 3-Way Infighting */
	NUMBER_OF_FIREFIGHT_DIPLOMACY_MODES
};

/* Feature 1: Progressive Per-Set Enemy Health & Shield Escalation */
enum
{
	_firefight_escalation_off = 0,     /* +0% per Set */
	_firefight_escalation_low,         /* +15% per Set */
	_firefight_escalation_medium,      /* +25% per Set (Default) */
	_firefight_escalation_high,        /* +40% per Set */
	_firefight_escalation_extreme,     /* +60% per Set */
	NUMBER_OF_FIREFIGHT_ESCALATION_MODES
};

/* Feature 2: Granular Per-Unit Enemy Type Toggle Bits (12 unit classes) */
enum firefight_unit_toggle_bit
{
	_ff_unit_covenant_grunts_bit = 0,
	_ff_unit_covenant_jackals_bit,
	_ff_unit_covenant_elites_bit,
	_ff_unit_covenant_specops_stealth_bit,
	_ff_unit_covenant_zealots_bit,
	_ff_unit_covenant_hunters_bit,
	_ff_unit_flood_infection_bit,
	_ff_unit_flood_combat_bit,
	_ff_unit_flood_carrier_bit,
	_ff_unit_sentinels_bit,
	_ff_unit_human_marines_bit,
	_ff_unit_human_odst_heavies_bit,
	NUMBER_OF_FIREFIGHT_UNIT_TOGGLES,
	FIREFIGHT_UNIT_MASK_ALL = ((1 << NUMBER_OF_FIREFIGHT_UNIT_TOGGLES) - 1)
};

/* Feature 3: Dropship Insertion Mode */
enum
{
	_firefight_dropship_auto = 0,      /* Auto (Outdoor Maps) */
	_firefight_dropship_frequent,      /* Frequent */
	_firefight_dropship_boss_only,     /* Boss Waves Only */
	_firefight_dropship_off,           /* Off */
	NUMBER_OF_FIREFIGHT_DROPSHIP_MODES
};

/* Feature 3: Next Wave Start Condition */
enum
{
	_firefight_next_wave_reach_default = 0, /* <= 2 Enemies Remaining (Reach Default) */
	_firefight_next_wave_clear_all,         /* All Enemies Killed (100% Wave Clear) */
	_firefight_next_wave_timer_30s,         /* 30s Timer Or Wave Clear */
	_firefight_next_wave_timer_45s,         /* 45s Timer Or Wave Clear */
	_firefight_next_wave_timer_60s,         /* 60s Timer Or Wave Clear */
	_firefight_next_wave_timer_90s,         /* 90s Timer Or Wave Clear */
	_firefight_next_wave_timer_120s,        /* 120s Timer Or Wave Clear */
	NUMBER_OF_FIREFIGHT_NEXT_WAVE_MODES
};

/* Feature 6: Player Faction / MCC Arcade Loadout Preset Mode */
enum
{
	_firefight_player_faction_spartan = 0, /* UNSC Spartan / Standard Loadout (Default) */
	_firefight_player_faction_elite,       /* Legacy alias */
	NUMBER_OF_FIREFIGHT_PLAYER_FACTION_MODES
};

enum
{
	_firefight_loadout_standard = 0,       /* Assault Rifle + Magnum */
	_firefight_loadout_marksman,           /* Magnum + Shotgun */
	_firefight_loadout_sniperfight,        /* Sniper Rifle + Magnum */
	_firefight_loadout_rocketfight,        /* Rocket Launcher + Magnum */
	_firefight_loadout_plasmafight,        /* Plasma Rifle + Plasma Pistol */
	_firefight_loadout_fiesta,             /* Random 2 Weapons Every Spawn */
	NUMBER_OF_FIREFIGHT_LOADOUT_PRESETS
};

/* VEHICLES, a game's (Server Setup's GAME SETTINGS, on a map with vehicles
of its own placed: firefight_map_has_vehicles), kept in its options'
firefight_vehicle_spawns: the players' vehicles are the map's own placements'
(MAP CONTENT's choices kept). ALL AT SPAWN: every one there from the start;
ORDNANCE REWARDS: one more of them at random with each ordnance drop after
the first, there for the rest of the game; NONE: none. Each made again at a
round's start where it went */
enum
{
	_firefight_vehicle_spawns_all = 0,
	_firefight_vehicle_spawns_ordnance,
	_firefight_vehicle_spawns_none,
	NUMBER_OF_FIREFIGHT_VEHICLE_SPAWNS
};

/* MAP CONTENT, a game's (Server Setup's MAP CONTENT section, a Custom
Edition map's with content of its own: firefight_map_content_kinds), kept in
its options' firefight_map_content: the enemies, weapons and vehicles the
waves, the ordnance drops and the players' vehicles are made of. Its low bits
the mode: VANILLA, the game's own (the campaign's, copied in where the map has
not got them); INCLUDE MAP, those and the map's own beside them; ONLY MAP, the
map's own alone, each kind falling back to VANILLA's where the map has none of
it. 1 is VANILLA: the byte was LAST MAN BOOST, which variants saved as TRUE.
Its high bits ADVANCED's, each kind ALL ON, CUSTOM or OFF: OFF a kind left
VANILLA whatever the mode; CUSTOM only those of the map's of the kind the
host's list for the map (firefight_map_content_tag_on) has on */
enum
{
	_firefight_map_content_vanilla = 0,
	_firefight_map_content_include_map = 2,
	_firefight_map_content_only_map = 3,
};
#define FIREFIGHT_MAP_CONTENT_MODE_MASK 0x03
#define FIREFIGHT_MAP_CONTENT_ENEMIES_CUSTOM 0x04
#define FIREFIGHT_MAP_CONTENT_WEAPONS_CUSTOM 0x08
#define FIREFIGHT_MAP_CONTENT_ENEMIES_OFF 0x10
#define FIREFIGHT_MAP_CONTENT_WEAPONS_OFF 0x20
#define FIREFIGHT_MAP_CONTENT_VEHICLES_OFF 0x40
#define FIREFIGHT_MAP_CONTENT_VEHICLES_CUSTOM 0x80
/* (the kinds, as the lists and the saved file have them) */
enum
{
	_firefight_map_content_kind_enemies = 0,
	_firefight_map_content_kind_weapons,
	_firefight_map_content_kind_vehicles,
	NUMBER_OF_FIREFIGHT_MAP_CONTENT_KINDS
};
/* (firefight_map_content_kinds': what a map has of its own) */
#define FIREFIGHT_MAP_CONTENT_HAS_ENEMIES 0x01
#define FIREFIGHT_MAP_CONTENT_HAS_WEAPONS 0x02
#define FIREFIGHT_MAP_CONTENT_HAS_VEHICLES 0x04

/* Feature 7: Pre-Round Preparation Countdown */
enum
{
	_firefight_prep_5s = 0,            /* 5s (Fast) */
	_firefight_prep_15s,               /* 15s */
	_firefight_prep_30s,               /* 30s (Default with Vehicles) */
	_firefight_prep_45s,               /* 45s */
	_firefight_prep_60s,               /* 60s */
	NUMBER_OF_FIREFIGHT_PREP_MODES
};

/* Feature 8: Skull Progression Mode */
enum
{
	_firefight_skull_prog_standard = 0,     /* Standard Reach Progression */
	_firefight_skull_prog_random_round,     /* Random Skull Each Round */
	_firefight_skull_prog_custom_pool,      /* Custom Rotation Pool */
	_firefight_skull_prog_off,              /* Off */
	NUMBER_OF_FIREFIGHT_SKULL_PROG_MODES
};

/* Feature 8: Player Trait & Sandbox Modifiers */
enum
{
	_firefight_shields_normal = 0,     /* 1x */
	_firefight_shields_none,           /* 0x */
	_firefight_shields_overshield,     /* 2x */
	_firefight_shields_super,          /* 4x */
	NUMBER_OF_FIREFIGHT_SHIELD_MODES
};

enum
{
	_firefight_speed_100 = 0,
	_firefight_speed_75,
	_firefight_speed_125,
	_firefight_speed_150,
	NUMBER_OF_FIREFIGHT_SPEED_MODES
};

enum
{
	_firefight_gravity_100 = 0,
	_firefight_gravity_50,
	_firefight_gravity_75,
	_firefight_gravity_150,
	NUMBER_OF_FIREFIGHT_GRAVITY_MODES
};

enum
{
	_firefight_ammo_normal = 0,
	_firefight_ammo_infinite_reserve,
	_firefight_ammo_bottomless_clip,
	NUMBER_OF_FIREFIGHT_AMMO_MODES
};

enum
{
	_firefight_wave_size_100 = 0,      /* Normal (100%) */
	_firefight_wave_size_50,           /* Small (50%) */
	_firefight_wave_size_150,          /* Large (150%) */
	_firefight_wave_size_200,          /* Horde (200%) */
	_firefight_wave_size_250,          /* Swarm (250%) */
	_firefight_wave_size_300,          /* Legion (300%) */
	NUMBER_OF_FIREFIGHT_WAVE_SIZE_MODES
};

enum
{
	_firefight_boss_wave_5_only = 0,   /* Wave 5 Only (Default) */
	_firefight_boss_every_3rd,         /* Every 3rd Wave */
	_firefight_boss_rush,              /* Boss Rush (All Waves) */
	NUMBER_OF_FIREFIGHT_BOSS_MODES
};

enum
{
	_firefight_enemy_loadout_default = 0, /* Each enemy's own weapons */
	_firefight_enemy_loadout_human,       /* Any human weapon */
	_firefight_enemy_loadout_covenant,    /* Any Covenant weapon */
	_firefight_enemy_loadout_random,      /* Any weapon of either */
	NUMBER_OF_FIREFIGHT_ENEMY_LOADOUTS
};

enum
{
	_firefight_friendly_ai_off = 0,    /* Off */
	_firefight_friendly_ai_round_duo,  /* 2 Allied Units per Round */
	_firefight_friendly_ai_set_squad,  /* 4 Allied Squad per Set */
	NUMBER_OF_FIREFIGHT_FRIENDLY_AI_MODES
};

enum
{
	_firefight_generator_fail_all = 0,
	_firefight_generator_fail_any,
	NUMBER_OF_FIREFIGHT_GENERATOR_FAIL_MODES
};

/* (port) what an ordnance pod brings (ordnance_pod_kinds): a weapon; an ammo
crate, which fills the weapon in hand of the first player to walk into it
whose weapon wants it; a health pack; an overshield. The last two are the
level's powerup on the pod when it has one, else the pod gives it to the
first player to walk into it who wants it, as the ammo crate does */
enum firefight_ordnance_pod_kind
{
	_ff_pod_kind_weapon = 0,
	_ff_pod_kind_ammo_crate,
	_ff_pod_kind_health,
	_ff_pod_kind_overshield,
	NUMBER_OF_FF_POD_KINDS
};

enum
{
	_firefight_ordnance_off = 0,
	_firefight_ordnance_per_set,
	_firefight_ordnance_per_round,
	_firefight_ordnance_frequent,
	NUMBER_OF_FIREFIGHT_ORDNANCE_MODES
};

/* how many of a wave's enemies go for the generators instead of the players
(0, a new variant's, is the default) */
enum
{
	_firefight_generator_attackers_50 = 0,
	_firefight_generator_attackers_none,
	_firefight_generator_attackers_25,
	_firefight_generator_attackers_75,
	_firefight_generator_attackers_all,
	NUMBER_OF_FIREFIGHT_GENERATOR_ATTACKER_MODES
};

/* firefight_starting_skulls[1]'s: its words are enum skull's bits. A variant
saved without it has the old presets', firefight_starting_skulls_get's */
#define FIREFIGHT_STARTING_SKULLS_EXACT 0x80000000UL
/* (firefight_tag_profile's: always in a hash, so that 0, and the skull mask
that field once was, are none) */
#define FIREFIGHT_TAG_PROFILE_MARK 0x80000000UL

enum
{
	FIREFIGHT_ROUNDS_PER_SET = 5,
	FIREFIGHT_WAVES_PER_ROUND = 3,
	FIREFIGHT_MAX_GENERATORS = 3,
	FIREFIGHT_MAX_TRACKED_ENEMIES = 32,
	/* (the players' vehicles: a map's placements, firefight_spawn_player_vehicles) */
	FIREFIGHT_MAX_SET_VEHICLES = 32,
	/* (a drop's pods, a weapon each: firefight_spawner.c; the first so many
	where mod version 103's state had them, firefight_network.c) */
	FIREFIGHT_MAX_ORDNANCE_PODS = 48,
	FIREFIGHT_ORDNANCE_PODS_BEFORE_104 = 4,
	/* (port) a drop's pods come down in groups, so many seconds apart: 3 to
	8 of them, by how many pods (firefight_ordnance_pod_release_delay) */
	FIREFIGHT_ORDNANCE_MIN_GROUPS = 3,
	FIREFIGHT_ORDNANCE_MAX_GROUPS = 8,
	FIREFIGHT_ORDNANCE_GROUP_TICKS = 3 * 30,
	/* (port) a pod no one player's (ordnance_pod_owners): everyone's */
	FIREFIGHT_ORDNANCE_POD_ANYONE = 0xFF,
	FIREFIGHT_MAX_SPAWN_ZONES_PER_WAVE = 3,
	FIREFIGHT_MAX_BOTS = 16,
	FIREFIGHT_BONUS_ROUND_TICKS = 60 * 30, /* 60 seconds at 30 Hz */
	FIREFIGHT_STRAGGLER_THRESHOLD = 3,
	FIREFIGHT_REINFORCEMENT_THRESHOLD = 2,
	FIREFIGHT_DEFAULT_SCORE_LIFE_BONUS = 10000,
	NUMBER_OF_BUILT_IN_FIREFIGHT_PROFILES = 8
};

enum firefight_phase_state
{
	_ff_phase_pre_set = 0,
	_ff_phase_pre_round,
	_ff_phase_wave_spawning,
	_ff_phase_wave_active,
	_ff_phase_wave_clear,
	_ff_phase_bonus_round,
	_ff_phase_set_complete,
	_ff_phase_victory,
	_ff_phase_defeat,
	NUMBER_OF_FIREFIGHT_PHASES
};

enum firefight_banner_type
{
	_ff_banner_none = 0,
	_ff_banner_set_start,
	_ff_banner_round_start,
	_ff_banner_wave_start,
	_ff_banner_boss_wave,
	_ff_banner_reinforcements,
	_ff_banner_bonus_round,
	_ff_banner_set_complete,
	_ff_banner_skull_activated,
	_ff_banner_last_man_standing,
	_ff_banner_generator_under_attack,
	_ff_banner_generator_locked_down,
	_ff_banner_generator_destroyed,
	_ff_banner_ordnance_drop,
	_ff_banner_heavy_ordnance_drop,
	_ff_banner_dropship_inbound,
	_ff_banner_vehicle_deployed,
	_ff_banner_bonus_life,
	_ff_banner_victory,
	_ff_banner_defeat,
	NUMBER_OF_FIREFIGHT_BANNERS
};

enum firefight_enemy_tier
{
	_ff_tier_fodder = 0,     /* Grunts, Flood Infection, Crewmen */
	_ff_tier_infantry,       /* Jackals, Flood Combat, Sentinels, Marines */
	_ff_tier_commander,      /* Elites, Flood Carriers, ODSTs */
	_ff_tier_boss_heavy,     /* Hunters, Elite Zealots, Stealth Elites, Rocket ODSTs */
	NUMBER_OF_FIREFIGHT_ENEMY_TIERS
};

enum firefight_unit_subtype
{
	_ff_sub_grunt = 0,
	_ff_sub_jackal,
	_ff_sub_elite_minor,
	_ff_sub_elite_major,
	_ff_sub_elite_stealth,
	_ff_sub_elite_zealot,
	_ff_sub_hunter,
	_ff_sub_other,
	NUMBER_OF_FF_UNIT_SUBTYPES
};

/* ---------- structures */

/* Halo: Reach Firefight core variant rules (24 bytes = 0x18 bytes,
fitting inside union game_engine_variant so verify_game_variant_size == 0x68) */
struct game_engine_firefight_variant
{
	short set_limit;              /* 0 = Unlimited, 1..10 sets */
	short starting_lives;         /* 0, 3, 5, 7, 10, 15, 20, -1 = Unlimited */
	short bonus_lives_per_round;  /* 0..5 (default 1) */
	short bonus_lives_per_set;    /* 0..10 (default 3) */
	short max_lives_cap;          /* 0 = Unlimited, 5..50 (default 10) */
	short generator_count;        /* 0 = Off, 1..3 = Alpha/Beta/Gamma Generators */
	unsigned long custom_skulls;  /* bitmask of enum firefight_skull_bit locked ON */
	byte difficulty;              /* enum _firefight_difficulty_* */
	byte wave_faction;            /* enum _firefight_faction_* */
	byte generator_fail_mode;     /* enum _firefight_generator_fail_* */
	byte ordnance_drops;          /* enum _firefight_ordnance_* */
	byte bonus_round_enabled;     /* boolean: 60s Grunt Birthday bonus round after Round 3 */
	byte progression_skulls;      /* boolean / mode: auto-enable Round/Set skulls */
	byte generator_attackers;     /* enum _firefight_generator_attackers_* */
	byte vehicle_drops_enabled;   /* boolean: vehicle drop enabled */
};

typedef struct game_engine_firefight_variant firefight_variant;
typedef char verify_firefight_variant_size[sizeof(struct game_engine_firefight_variant) == 0x18 ? 1 : -1];

struct firefight_generator_state
{
	long object_index;
	real_point3d position;
	short health;
	short max_health;
	short shield;
	short max_shield;
	short lockdown_ticks;
	short cooldown_ticks;
	short under_attack_ticks;
	boolean active;
	boolean alive;
};

struct firefight_dropship_flight
{
	long vehicle_index;
	real_point3d drop_zone;
	real_point3d hover_position;
	real approach_angle;
	real scale; /* how it looks this tick, which clients are told */
	real cloak;
	short phase; /* 0 = inactive, 1 = approach, 2 = hover & unload, 3 = departure */
	short elapsed_ticks;
	short timer_ticks;
	short squad_to_spawn;
	short forced_team;
	byte wave_faction;
	boolean is_boss_wave;
	boolean is_pelican;
	boolean payload_deployed;
	/* what brings it (enum firefight_drop_carrier): the Flood's is no ship,
	so vehicle_index stays NONE and firefight_globals' flood_drop shows it */
	byte carrier;
	byte pad_carrier[3];
};

enum firefight_drop_carrier
{
	_ff_drop_carrier_spirit = 0,
	_ff_drop_carrier_pelican,
	_ff_drop_carrier_flood,
	NUMBER_OF_FIREFIGHT_DROP_CARRIERS
};

typedef struct firefight_dropship_flight firefight_dropship_state;
#define firefight_dropship_state firefight_dropship_flight

struct firefight_player_stats
{
	long score;
	short kills;
	short boss_kills;
	short assists;
	short deaths;
	short current_spree;
	short best_spree;
	short multikill_count;
	long last_kill_tick;
};

struct firefight_globals
{
	/* Progression state */
	short current_set;
	short current_round;
	short current_wave;
	short phase_state;
	long phase_timer_ticks;
	long wave_elapsed_ticks;
	long session_elapsed_ticks;    /* Feature 4: Total Session Game Time Counter */
	long next_wave_timer_ticks;    /* Feature 3: Countdown to next wave (if timer trigger active) */
	boolean in_bonus_round;
	boolean match_won;
	boolean match_lost;
	boolean elite_player_mode;     /* Feature 6: Covenant Elite / Heretic Mode */

	/* Shared lives pool */
	short shared_lives_remaining; /* -1 = Unlimited */
	short max_lives_cap;
	boolean last_man_standing_announced;
	short wipe_defeat_timer_ticks;

	/* Skulls, Escalation & Scoring */
	unsigned long active_skulls;
	unsigned long round_skulls;
	unsigned long set_skulls;
	/* those of active_skulls that will go again: a round's, the bonus
	round's (the HUD pulses them) */
	unsigned long temporary_skulls;
	long team_score;
	long next_bonus_life_score;
	real active_multiplier;
	real set_vitality_scale;       /* Feature 1: Per-Set Enemy Health/Shield Escalation */

	/* Enemy tracking & multi-zone wave spawner */
	short enemies_alive;
	short enemies_queued;
	short enemies_total_in_wave;
	boolean using_mp_squad_spawner;
	boolean straggler_navpoints_active;
	long tracked_enemy_units[FIREFIGHT_MAX_TRACKED_ENEMIES];
	byte tracked_enemy_tiers[FIREFIGHT_MAX_TRACKED_ENEMIES];
	real_point3d wave_spawn_zones[FIREFIGHT_MAX_SPAWN_ZONES_PER_WAVE];
	short wave_spawn_zone_count;

	/* Dropships & Set-Tracked Vehicles (Features 3 & 7) */
	struct firefight_dropship_state active_dropship;
	long deployed_set_vehicles[FIREFIGHT_MAX_SET_VEHICLES];
	short deployed_set_vehicle_count;

	/* Generator defense & Ordnance */
	short generator_count;
	short generators_alive;
	struct firefight_generator_state generators[FIREFIGHT_MAX_GENERATORS];
	real_point3d last_ordnance_position;
	short ordnance_navpoint_ticks;
	boolean last_ordnance_is_heavy;
	/* the ordnance pods (firefight_ordnance_pod_update): the game time they
	were let go at, NONE (or 0) for none, and where each lands; in the state
	the host sends (last_ordnance_position the first's, the HUD's) */
	long ordnance_pod_drop_time;
	real_point3d ordnance_pod_positions[FIREFIGHT_MAX_ORDNANCE_PODS];
	short ordnance_pod_count;
	/* port: how many pods each player's set is (the pods in order, a set to
	each of the game's players by their order: each player's HUD marks its own
	set alone); 0: one set for all (a host from before) */
	short ordnance_pods_per_player;
	/* port (mod version 105): each pod's player (its place among the game's
	players, as firefight_hud.c counts them; FIREFIGHT_ORDNANCE_POD_ANYONE for
	everyone's), what it brings (enum firefight_ordnance_pod_kind), and which
	pods were used up (an ammo crate, or a powerup the pod gave itself) */
	byte ordnance_pod_owners[FIREFIGHT_MAX_ORDNANCE_PODS];
	byte ordnance_pod_kinds[FIREFIGHT_MAX_ORDNANCE_PODS];
	unsigned long ordnance_pod_spent[(FIREFIGHT_MAX_ORDNANCE_PODS + 31) / 32];
	/* (the host's) how many drops this game has had (the first is a weapon
	each), and the game's tick its first comes at (5 to 10 seconds in) */
	short ordnance_drops_made;
	long first_ordnance_tick;

	/* port (mod version 106): the Flood's drop, Keyes' Flood form grown out
	of the floor (firefight_flood_drop_update, every machine's, as the pods):
	the game time it started at (NONE or 0: none), where, its facing; in the
	state the host sends */
	long flood_drop_time;
	real_point3d flood_drop_position;
	real flood_drop_facing;

	/* HUD banner state */
	short banner_type;
	short banner_data;
	short banner_timer_ticks;
	short banner_total_ticks;

	/* Per-player statistics */
	struct firefight_player_stats player_stats[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
};

/* Per-Map Personal Best High Score Record (firefight_stats.c) */
struct firefight_map_record
{
	long best_score;
	short best_set;
	short best_round;
	short best_wave;
	long best_seconds;
	short best_difficulty;
	short best_skulls;
	long best_medals;
};

/* Lifetime Career Stats, MP/Ranked Stats, Achievements & 38-Medal Cabinet (firefight_stats.c) */
enum halo_achievement_id
{
	/* Campaign levels (10) */
	_ach_camp_a10 = 0,
	_ach_camp_a30,
	_ach_camp_a50,
	_ach_camp_b30,
	_ach_camp_b40,
	_ach_camp_c10,
	_ach_camp_c20,
	_ach_camp_c40,
	_ach_camp_d20,
	_ach_camp_d40,
	/* Campaign level difficulties (3) */
	_ach_lvl_normal,
	_ach_lvl_heroic,
	_ach_lvl_legendary,
	/* Full Campaign completion & difficulties (4) */
	_ach_camp_complete,
	_ach_camp_normal,
	_ach_camp_heroic,
	_ach_camp_legendary,
	/* Firefight 1 Set + per-map 1 Set (11) */
	_ach_ff_first_set,
	_ach_ff_a10,
	_ach_ff_a30,
	_ach_ff_a50,
	_ach_ff_b30,
	_ach_ff_b40,
	_ach_ff_c10,
	_ach_ff_c20,
	_ach_ff_c40,
	_ach_ff_d20,
	_ach_ff_d40,
	/* Map Skulls & Elite Skull-Carrier (11) */
	_ach_skull_iron,
	_ach_skull_mythic,
	_ach_skull_boom,
	_ach_skull_bandanna,
	_ach_skull_fog,
	_ach_skull_famine,
	_ach_skull_black_eye,
	_ach_skull_eye_patch,
	_ach_skull_recession,
	_ach_skull_malfunction,
	_ach_skull_elite_hunter,
	HALO_ACHIEVEMENT_COUNT
};

struct firefight_career_stats
{
	long total_matches;
	long total_sets_completed;
	long total_waves_cleared;
	long total_kills;
	long total_boss_kills;
	long total_score;
	long total_seconds;
	long total_deaths;
	long total_assists;
	long best_kill_spree;
	long medal_counts[38];
	long mp_medal_counts[38];
	long mp_matches;
	long mp_kills;
	long mp_deaths;
	long mp_assists;
	long mp_score;
	long ranked_matches;
	long ranked_kills;
	long ranked_deaths;
	long ranked_assists;
	long ranked_csr;
	long ranked_points;
	long ranked_wins;
	long ranked_losses;
	long ranked_draws;
	unsigned long achievements_lo;
	unsigned long achievements_hi;
	long career_xp;
	/* (Firefight's alone: total_matches, total_kills and the like are every
	mode's; total_sets_completed, total_waves_cleared and total_boss_kills
	are Firefight's already) */
	long ff_matches;
	long ff_kills;
	long ff_deaths;
	long ff_assists;
	long ff_score;
	long ff_seconds;
	long ff_best_spree;
};

enum firefight_challenge_category
{
	_ff_challenge_cat_campaign = 0,
	_ff_challenge_cat_firefight = 1,
	_ff_challenge_cat_multiplayer = 2,
	FIREFIGHT_CHALLENGE_CATEGORY_COUNT = 3
};

enum firefight_challenge_period
{
	_ff_challenge_period_daily = 0,
	_ff_challenge_period_weekly = 1,
	FIREFIGHT_CHALLENGE_PERIOD_COUNT = 2
};

struct firefight_challenge_view
{
	char title[64];
	char description[96];
	long progress;
	long target;
	long xp_reward;
	boolean completed;
};

/* the service ranks' tours and the ranks in each (Halo 3's 42 UNSC ranks, as
the insignia in rank_icons.bin: tools/rank_icon_assets.py) */
#define FIREFIGHT_SERVICE_TOURS 11
#define FIREFIGHT_SERVICE_RANKS 42

struct firefight_mcc_rank_info
{
	short tour_number;       /* 1..11 (Tour 1 Bronze .. Tour 11 Reclaimer) */
	char const *tour_name;   /* e.g. "Tour 1 (Bronze)" */
	short rank_in_tour;      /* 1..42 (Recruit .. 5 Star General) */
	char const *rank_name;   /* e.g. "Recruit", "Brigadier", "5 Star General" */
	long total_xp;
	long xp_in_rank;
	long xp_for_next_rank;
};

struct firefight_h3_ranked_info
{
	short rank_index;        /* 0..41 (Recruit .. General (Grade 4)) */
	char const *rank_name;   /* e.g. "Recruit", "Brigadier", "General (Grade 4)" */
	long points;             /* 0..100 (+2 per win, -2 per loss, 0 per draw) */
	long next_rank_points;   /* Points required for next rank (or 100 at max) */
	long wins;
	long losses;
	long draws;
};

#define FIREFIGHT_NAMEPLATE_COUNT 10

/* (the emblem lists are not part of this build: one default emblem) */
#define FIREFIGHT_EMBLEM_DEFAULT { 0, 0, TRUE, 0, 0, 0, 0 }

struct firefight_emblem_info
{
	short foreground;
	short background;
	boolean secondary_layer;
	short primary_color;
	short secondary_color;
	short background_color;
	short nameplate;
};

enum
{
	FIREFIGHT_POSTGAME_XP_LINES = 10
};

/* one source of a match's XP (count: how many, 0 for no number) */
struct firefight_postgame_xp_line
{
	char label[40];
	long xp;
	long count;
};

struct firefight_postgame_xp_summary
{
	long start_xp;
	long current_xp;
	long combat_xp;
	long medal_xp;
	long match_bonus_xp;
	long total_gained_xp;
	struct firefight_mcc_rank_info before_rank;
	struct firefight_mcc_rank_info after_rank;
	boolean promoted;
	struct firefight_postgame_xp_line lines[FIREFIGHT_POSTGAME_XP_LINES];
	short line_count;
};

/* ---------- Public API: firefight.c */

extern struct game_engine firefight_engine;
extern struct firefight_globals ff_state;

struct game_variant;
/* Firefight's own options: the host's choices (game_variant_options carries
none of them in this fork). firefight_options_get is what the game runs by:
the dev command's start sets it (firefight_options_set) */
struct firefight_options
{
	byte friendly_fire;
	byte loadout;
	byte primary_weapon;
	byte secondary_weapon;
	unsigned long firefight_custom_skulls;
	short firefight_generator_health;
	short firefight_score_life_threshold;
	byte firefight_enemy_health_scale;
	byte firefight_enemy_shield_scale;
	byte firefight_bonus_seconds;
	byte firefight_faction_diplomacy;
	unsigned long firefight_tag_profile;
	unsigned long firefight_starting_skulls[2];
	unsigned short firefight_unit_toggles;
	byte firefight_set_escalation;
	byte firefight_dropship_mode;
	byte firefight_next_wave_mode;
	byte firefight_player_faction_mode;
	byte firefight_vehicle_spawns;
	byte firefight_plasma_flinch;
	byte firefight_unused_vehicle;
	byte firefight_prep_countdown;
	byte firefight_skull_progression_mode;
	byte firefight_player_shields;
	byte firefight_player_speed;
	byte firefight_player_gravity;
	byte firefight_infinite_ammo;
	byte firefight_map_content;
	byte firefight_wave_size_scale;
	byte firefight_boss_wave_mode;
	byte firefight_enemy_loadout;
	byte firefight_friendly_ai;
};

struct firefight_options const *firefight_options_get(void);
void firefight_options_set(struct firefight_options const *options);

extern struct game_variant global_variant;
boolean firefight_active(void);
real firefight_player_stun_scale(void);
/* (actor_combat.c) Firefight's enemies' firing, of their tags': their rate
and bursts by it, their pauses by its inverse (1: as they are) */
real firefight_enemy_fire_pacing(short team_index);
real firefight_enemy_aim_error_scale(short team_index);
/* (firefight_spawner.c) the tracked enemies gone in a teleport's flash, not
killed (the bonus round over) */
void firefight_teleport_out_tracked_enemies(void);
/* how far an actor walks a straight line on the level towards a point (at
most most units, no drop over 1.2 units, no kill volume), where it stops into
end; stopped at an edge with ground to land on beyond it, TRUE into edge and
the landing (firefight_spawner.c) */
real firefight_straight_walk(real_point3d const *from, real_point3d const *to, real most, real_point3d *end,
	boolean *edge, real_point3d *landing);
/* the same with its most climb per half unit (`step_up`: firefight_straight_walk's
0.8) and, `objects` 1, round objects as well as the level's walls; 2, round
units too, but `ignore_object_index` (the walker) */
real firefight_straight_walk_ex(real_point3d const *from, real_point3d const *to, real most, real_point3d *end,
	boolean *edge, real_point3d *landing, real step_up, int objects, long ignore_object_index);
/* a spot to one side to walk to first, when the straight walk to `to` is
blocked short of it: FALSE for none (firefight_spawner.c) */
boolean firefight_straight_detour(real_point3d const *from, real_point3d const *to, real step_up, int objects,
	long ignore_object_index, real_point3d *via);
/* an actor's chosen drop over an edge: no falling damage until it lands;
the drops landed each Firefight tick; forgotten at a game's start */
void firefight_actor_drop(long unit_index);
void firefight_drops_update(void);
void firefight_drops_reset(void);
boolean firefight_build_variant_by_name(char const *name, struct game_variant *out_variant);
boolean firefight_get_variant_by_name(struct game_variant *out_variant, char const *name);
/* ae_firefight_doors.c: Firefight starts only with DEV MODE on (a client plays the host's game) */
boolean firefight_engine_allowed(void);
boolean firefight_engine_refused(void);
short firefight_engine_index_kept(short engine_index);
struct game_variant *build_game_variant_firefight(struct game_variant *variant);
struct game_variant *build_game_variant_generator_defense(struct game_variant *variant);
struct game_variant *build_game_variant_gruntpocalypse(struct game_variant *variant);
struct game_variant *build_game_variant_rocketfight(struct game_variant *variant);
struct game_variant *build_game_variant_floodfight(struct game_variant *variant);
struct game_variant *build_game_variant_fiesta(struct game_variant *variant);
struct game_variant *build_game_variant_grunt_fiesta(struct game_variant *variant);
struct game_variant *build_game_variant_mythic_firefight(struct game_variant *variant);
void firefight_variant_cleanup(struct game_variant *variant);
void firefight_variant_options_default(struct firefight_options *options);
void firefight_builtin_variant_options(struct game_variant const *variant, struct firefight_options *options);
struct firefight_globals const *firefight_globals_get(void);
struct firefight_globals *firefight_globals_mutable(void);
unsigned long firefight_active_skulls_get(void);
boolean firefight_skull_active(enum firefight_skull_bit skull);
boolean firefight_40_skull_active(int skull_enum_index);
/* whether a skull on is on for now only, a round's or the bonus round's */
boolean firefight_40_skull_temporary(int skull_enum_index);
/* the skulls a variant starts with, as enum skull's bits: its rules' and,
from older variants, its own and its options' Firefight skulls */
void firefight_starting_skulls_get(
	struct game_engine_firefight_variant const *variant,
	struct firefight_options const *options,
	unsigned long skulls[2]);
/* ... and set, wholly in its options */
void firefight_starting_skulls_set(
	struct game_engine_firefight_variant *variant,
	struct firefight_options *options,
	unsigned long const skulls[2]);
/* a Tag Injector profile's folder as firefight_tag_profile has it */
unsigned long firefight_tag_profile_hash(char const *folder);
real firefight_score_multiplier_get(void);
real firefight_set_vitality_scale_get(void);
boolean firefight_should_end_game(void);
boolean firefight_player_out_of_lives(long player_index);
boolean firefight_player_is_out_of_lives(long player_index);
long firefight_compute_respawn_timer(long dead_player_index);
/* DEV MODE (firefight.c): the round's wave begun now; its enemies gone; a skull on for the round */
boolean firefight_dev_wave(short wave);
boolean firefight_dev_clear(void);
boolean firefight_dev_skull(char const *name);
void firefight_postspawn_player_update(long player_index);
long firefight_player_kills(long player_index);
long firefight_player_assists(long player_index);
boolean firefight_spectating_active(void);
boolean firefight_elite_mode_active(void);
long firefight_session_elapsed_seconds(void);
void firefight_format_session_time(wchar_t *out_buf, long count);
void firefight_setup_faction_allegiances(void);
/* whether a team fights the players in this game: the marines' only in a
UNSC wave (not game_team_is_enemy's, which Woe Is Me makes every team) */
boolean firefight_team_is_hostile(short team_index);
real firefight_enemy_vitality_scale(short team_index, boolean is_shield);
void firefight_apply_unit_vitality_scaling(long unit_index);
long firefight_resolve_player_biped_tag(long player_index, long default_biped_tag);
void firefight_apply_player_spawn_traits(long player_index, long unit_index);
void firefight_notify_actor_or_unit_killed(
	long actor_index,
	long dead_unit_index,
	long killer_player_index,
	boolean headshot,
	boolean melee,
	boolean explosion,
	boolean splatter);
void firefight_notify_player_melee_hit(
	long player_index,
	long target_object_index);
void firefight_announce_banner(
	enum firefight_banner_type type,
	short data,
	short duration_ticks);
int firefight_map_reach_skull_to_40_skull(enum firefight_skull_bit bit);
short firefight_server_player_scale_percent_get(void);
void firefight_server_player_scale_percent_set(short percent);

/* ---------- randomizer.c: the level's Tag Injector profile, for the spawner */

/* the profile's roots of a tag group, loaded on this level: their count */
long randomizer_firefight_tags(unsigned long group, long *tags, long maximum);
/* an actor variant's tier (enum firefight_enemy_tier), by its type */
short randomizer_firefight_actor_tier(long variant_tag);
/* whether a weapon is a power weapon, for a heavy drop */
boolean randomizer_firefight_weapon_power(long weapon_tag);

/* ---------- Public API: firefight_spawner.c */

void firefight_spawner_tags_loaded(char const *scenario_name);
void firefight_spawner_initialize_map(void);
void firefight_spawner_dispose_map(void);
boolean firefight_select_player_spawn_point(
	long player_index,
	real_point3d *out_position,
	real *out_facing);
boolean firefight_unit_toggle_enabled(enum firefight_unit_toggle_bit unit_bit);
boolean firefight_validate_unit_toggles(
	byte wave_faction,
	unsigned short candidate_unit_mask);
short firefight_spawn_wave(
	short set_num,
	short round_num,
	short wave_num,
	boolean is_bonus);
void firefight_update_dropships_and_vehicles(void);
void firefight_dropship_apply_look(short absolute_index, real scale, real cloak);
void firefight_spawn_ordnance_drop(boolean heavy_anti_armor);
/* the ordnance pod, falling and landing, on every machine (the host's
weapons come out of it as it lands); and the last one forgotten */
void firefight_ordnance_pod_update(void);
void firefight_ordnance_pod_reset(void);
/* (port) the Flood's drop growing out of the floor and bursting, on every
machine from flood_drop_time (the host's squad comes out as it bursts) */
void firefight_flood_drop_update(void);
/* (the HUD's) the Flood form growing on this machine, NONE for none */
long firefight_flood_drop_object(void);
/* Forge's TEST DROP of a Flood drop point: the form grown and burst on this
machine, no squad */
boolean firefight_flood_drop_preview_start(
	real_point3d const *zone,
	real facing,
	long flood_form_tag,
	long burst_effect_tag,
	long puff_effect_tag);
boolean firefight_flood_drop_preview_update(real seconds);
void firefight_flood_drop_preview_stop(void);
/* (port) how long after the drop's time a pod is let go (its group's), and
the last pod's: the same on every machine, from the pod's place and how many */
long firefight_ordnance_pod_release_delay(short pod, short pod_count);
long firefight_ordnance_drop_span_ticks(void);
/* (port) the game time a pod lands at (NONE: no drop) */
long firefight_ordnance_pod_land_time(short pod);
boolean firefight_ordnance_pod_spent(short pod);
/* Forge's TEST DROP (a drop area's): so many pods let go round `area` on this
machine alone, without weapons, as a drop's would be (`area_object`, its
marker, not in their way); how many found a spot. Its update each frame:
0 once it is over (its pods gone), 1 falling, 2 landed */
short firefight_ordnance_preview_start(real_point3d const *area, long area_object, short count);
short firefight_ordnance_preview_update(void);
void firefight_ordnance_preview_stop(void);
/* why the last test drop's spots were refused, for the status line */
void firefight_ordnance_preview_refusals(char *text, long size);
/* the players' vehicles (PLAYER VEHICLE 1 to 3, and with MAP CONTENT the
map's), there from the game's start and never taken away: any gone or
destroyed is made again, at a round's start */
void firefight_spawn_player_vehicles(void);
/* VEHICLES' ORDNANCE REWARDS: one more of the map's vehicles for the players,
at random (an ordnance drop's, after the first) */
void firefight_reward_player_vehicle(void);
/* (a game's start: the players' vehicles chosen again) */
void firefight_player_vehicles_reset(void);
/* whether the players have vehicles from the start (ALL AT SPAWN, on a map
with some placed: the first round's countdown is longer) */
boolean firefight_player_vehicles_at_start(void);
/* VEHICLES' row: whether a map (by name, not loaded) has vehicles of its own
placed for the players, the game's (stock) or the map's own (own, which MAP
CONTENT takes or not); unknown maps have */
void firefight_map_has_vehicles(char const *map_name, boolean *stock, boolean *own);
/* wave enemies idle 10 s (not moved, fired nor hurt) put near the players,
out of their sight, where they can walk; taken away after 4 such moves
(firefight_spawner.c). The host's, each tick */
void firefight_relocate_idle_enemies(void);
/* MAP CONTENT's mode, as the game has it (_firefight_map_content_*) */
byte firefight_map_content(void);
/* what a map has of its own, outside the game's (FIREFIGHT_MAP_CONTENT_HAS_*):
a Custom Edition map's tags, read from its file by name (the game's own maps
none), the last map's kept */
byte firefight_map_content_kinds(char const *map_name);
/* a random loadout weapon's (Fiesta's) tag path of the map's own, by MAP
CONTENT, or NULL for one of the game's */
char const *firefight_map_loadout_weapon(char const *other);
/* a flag's or a ball's (must be readied): never handed out by Firefight */
boolean firefight_weapon_is_objective(long weapon_tag, char const *use);
/* CUSTOM's lists (a map's own of a kind, as firefight_map_content_kinds
finds them: tag names), and the host's choices of them, kept per map in
d:\\firefight_map_content.txt with the map's MAP CONTENT byte last set */
short firefight_map_content_tag_count(char const *map_name, short kind);
char const *firefight_map_content_tag_name(char const *map_name, short kind, short index);
boolean firefight_map_content_tag_on(char const *map_name, short kind, char const *tag_name);
void firefight_map_content_tag_set(char const *map_name, short kind, char const *tag_name, boolean on);
/* MAP CONTENT's PLAYER LOADOUT (the host's, kept for the map): slot 0 the
primary, 1 the secondary; the map's weapon's name, or NULL for the game's */
char const *firefight_map_loadout_get(char const *map_name, short slot);
void firefight_map_loadout_set(char const *map_name, short slot, char const *tag_name);
char const *firefight_map_loadout_override(short slot);
/* MAP CONTENT's HEALTH RULES: what a map's player has (its file read, once
for each map asked of): */
#define FIREFIGHT_MAP_HEALTH_NO_SHIELDS 0x01
#define FIREFIGHT_MAP_HEALTH_NO_SHIELD_REGEN 0x02
byte firefight_map_health_detect(char const *map_name);
/* ... and the host's choices for it, kept for the map: SHIELDS and SHIELD
REGEN 0 the map's, 1 on, 2 off; HEALTH REGEN 0 off, 1 on */
enum
{
	_firefight_health_rule_shields = 0,
	_firefight_health_rule_shield_regen,
	_firefight_health_rule_health_regen,
	NUMBER_OF_FIREFIGHT_HEALTH_RULES
};
enum
{
	_firefight_health_rule_maps = 0,
	_firefight_health_rule_on,
	_firefight_health_rule_off
};
void firefight_map_health_rules_get(char const *map_name, byte rules[NUMBER_OF_FIREFIGHT_HEALTH_RULES]);
void firefight_map_health_rules_set(char const *map_name, byte const rules[NUMBER_OF_FIREFIGHT_HEALTH_RULES]);
byte firefight_health_rule(short rule);
/* (damage.c's: a Firefight player's shield regen by the rules, -1 the
engine's own, 0 none, 1 Halo's, at the stock rate where the map has none;
and the stock stun and recharge times that go with it) */
#define FIREFIGHT_STOCK_SHIELD_STUN_SECONDS 6.0f
#define FIREFIGHT_STOCK_SHIELD_RECHARGE_SECONDS 4.0f
/* (SHIELDS ON's, on a map whose player has none: the stock player's) */
#define FIREFIGHT_GIVEN_SHIELD_VITALITY 75.0f
short firefight_player_shield_regen(long object_index);
/* the menu's check of a map's own (its tags opened for their previews): a
tag found not usable (a weapon a vehicle's, or missing what a player needs)
is off unless turned on by hand; and whether one was found usable (TRUE
before any check) */
void firefight_map_content_check(char const *map_name, boolean (*usable)(short kind, char const *tag_name));
boolean firefight_map_content_tag_usable(char const *map_name, short kind, char const *tag_name);
boolean firefight_map_content_saved(char const *map_name, byte *content);
void firefight_map_content_save(char const *map_name, byte content);
/* (actors.c) the team of a unit an actor takes */
short firefight_actor_unit_team(short owner_team_index, short default_team_index);
/* (randomizer.c) ENEMY LOADOUT's: any weapon in any enemy's hands, its
animations borrowed */
boolean firefight_enemy_weapons_borrowed(void);
char const *firefight_unit_own_weapon_label(long unit_index);
void firefight_wrecks_reset(void);
void firefight_spawn_friendly_reinforcements(short count);
boolean firefight_wave_will_have_heavy_or_boss(
	short set_num,
	short round_num,
	short wave_num);
void firefight_prune_dead_tracked_enemies(void);
void firefight_track_enemy_unit(long unit_index, byte tier);
void firefight_cleanup_natural_multiplayer_sandbox(boolean clear_weapons_and_equipment);
short firefight_queued_enemies_count(void);
/* whether the actor is one of the wave's sent at this generator's unit, which
it then fights over any player (actor_compute_prop_target_weight) */
boolean firefight_actor_attacks_generator(long actor_index, long unit_index);

/* ---------- Public API: firefight_generators.c */

void firefight_generators_initialize(short count, short max_health);
void firefight_generators_update(void);
boolean firefight_is_generator_object(
	long object_index,
	short *generator_index_out);
boolean firefight_is_generator_hidden_biped(
	long object_index);
boolean firefight_apply_generator_damage(
	long object_index,
	long attacker_player_index,
	short attacker_team_index,
	real damage_amount);
void firefight_try_trigger_generator_lockdown(long player_index);

/* ---------- Public API: firefight_hud.c */

void firefight_hud_draw(void);
void firefight_hud_post_rasterize(void);
boolean firefight_hud_format_scoreboard_title(wchar_t *title_string, long count);
void firefight_format_scoreboard_title(wchar_t *title_string, long count, boolean postgame);
/* the postgame's XP screen: whether the match has XP to show, the screen's
animation started over, and the screen (in the 640 by 480 of the postgame) */
boolean firefight_hud_xp_screen_wanted(void);
void firefight_hud_xp_screen_begin(void);
void firefight_hud_xp_screen_draw(void);
/* the count shown at its end at once: TRUE if it was still counting */
boolean firefight_hud_xp_screen_finish(void);
/* the same card over the menus after a game left without that screen: noted
as a played map is left (with the XP it gave), drawn from social_chat_draw;
while shown the menus' input is its own (menu_functions.c). A press finishes
the count, then closes it; `close` closes it at once */
void firefight_hud_xp_exit_note(void);
/* the card shown for XP given outside a game (the catch-up on an account's
past records: firefight_stats_rank_catch_up) */
void firefight_hud_xp_card_show(struct firefight_postgame_xp_summary const *summary);
boolean firefight_hud_xp_exit_shown(void);
void firefight_hud_xp_exit_press(boolean close);
void firefight_hud_xp_exit_draw(short screen_width, short screen_height);
void firefight_draw_mcc_rank_icon(short x0, short y0, short size, short tour_number, short rank_in_tour, real alpha);
void firefight_draw_h3_ranked_icon(short x0, short y0, short size, short ranked_index, real alpha);
void firefight_draw_player_emblem(short x0, short y0, short size, struct firefight_emblem_info const *emblem, real alpha);
/* what of an emblem is drawn: the editor shows the part each row chooses */
enum
{
	_firefight_emblem_part_whole,
	_firefight_emblem_part_foreground,
	_firefight_emblem_part_background
};
void firefight_draw_player_emblem_part(short x0, short y0, short size, struct firefight_emblem_info const *emblem,
	real alpha, short part);
void firefight_draw_mcc_nameplate(short x0, short y0, short x1, short y1, short nameplate_id, pixel32 armor_accent, real alpha);
void firefight_hud_pgcr_shown(void);
/* game_engine.c: a player's rank and emblem, on any machine */
void game_engine_player_rank_and_emblem(
	long player_index,
	struct firefight_mcc_rank_info *out_rank,
	struct firefight_emblem_info *out_emblem);
void firefight_draw_ce_rounded_box(
	short px0,
	short py0,
	short px1,
	short py1,
	short header_px,
	real scale,
	pixel32 tint_argb,
	pixel32 fallback_fill,
	pixel32 fallback_edge);
boolean firefight_draw_ce_rounded_pill(
	short px0,
	short py0,
	short px1,
	short py1,
	short frame_idx,
	real scale,
	pixel32 tint_argb);

/* ---------- Public API: firefight_stats.c */

void firefight_stats_on_map_initialize(void);
void firefight_stats_on_map_dispose(void);
void firefight_stats_record_match_end(char const *map_name);
void firefight_stats_record_multiplayer_match_end(char const *map_name);
void firefight_stats_record_campaign_mission_end(
	char const *map_name,
	long score,
	long kills,
	long deaths,
	long seconds,
	short difficulty,
	short skulls,
	long medals);
boolean firefight_stats_get_map_record(
	char const *map_name,
	long profile_index,
	struct firefight_map_record *out_record);
boolean firefight_stats_get_career(
	long profile_index,
	struct firefight_career_stats *out_career);
void firefight_stats_record_medal(long player_index, short medal_index);
void firefight_stats_get_player_id(long profile_index, char *out_id, size_t id_size);
short firefight_stats_get_map_record_count(long profile_index);
boolean firefight_stats_get_indexed_map_record(
	short index,
	long profile_index,
	char *out_map_key,
	size_t key_size,
	struct firefight_map_record *out_record);
boolean firefight_stats_export_json(
	long profile_index,
	char *out_path,
	size_t path_size);
boolean firefight_stats_open_web_profile(
	long profile_index,
	char *out_status,
	size_t status_size);
boolean firefight_stats_copy_player_id(
	long profile_index,
	char *out_status,
	size_t status_size);
void firefight_stats_unlock_achievement(short ach_id);
boolean firefight_stats_is_achievement_unlocked(long profile_index, short ach_id);
void firefight_stats_get_achievement_count(long profile_index, short *out_unlocked, short *out_total);
short firefight_stats_get_achievement_points(short ach_id);
void firefight_stats_get_achievement_score(long profile_index, short *out_score, short *out_max_score);
boolean firefight_stats_get_achievement_info(
	short ach_id,
	char const **out_key,
	wchar_t const **out_title,
	wchar_t const **out_desc);
void firefight_stats_notify_skull_claimed(int skull_enum_id, boolean from_elite_carrier);
boolean firefight_stats_startup_check(void);
boolean firefight_stats_generate_link_code(
	long profile_index,
	char *out_code,
	size_t code_size,
	char *out_status,
	size_t status_size);
boolean firefight_stats_poll_pending_link_popup(
	char *out_code,
	size_t code_size,
	char *out_device_id,
	size_t id_size);
void firefight_stats_notify_terminal_command_used(void);
void firefight_stats_reset_session_terminal_flag(void);
boolean firefight_stats_achievements_disabled(void);
/* an account's past records, as the server says them, which the catch-up
counts into its rank with the career's own */
struct firefight_rank_history
{
	long multiplayer_matches;
	long multiplayer_kills;
	long multiplayer_assists;
	long multiplayer_medals;
	long campaign_levels;
	long campaign_kills;
	long campaign_medals;
	long firefight_matches;
	long firefight_sets;
	long firefight_waves;
	long firefight_kills;
};

/* the rank the records are worth given, if it is more than the career's, the
XP card showing it counted up from nothing: TRUE if so */
boolean firefight_stats_rank_catch_up(struct firefight_rank_history const *history);
/* the career's XP set as it is (the server's, or 0: CLEAR RANK) */
void firefight_stats_set_career_xp(long xp);
/* a replay or Forge session: it gives no XP and records nothing */
boolean firefight_stats_session_withheld(void);
void firefight_stats_add_xp(long xp_amount, char const *reason);
void firefight_stats_add_xp_counted(long xp_amount, char const *reason, long count);
void firefight_stats_get_mcc_rank(long profile_index, struct firefight_mcc_rank_info *out_rank);
void firefight_stats_get_h3_ranked(long profile_index, struct firefight_h3_ranked_info *out_ranked);
void firefight_stats_get_emblem(long profile_index, struct firefight_emblem_info *out_emblem);
void firefight_stats_set_emblem(long profile_index, struct firefight_emblem_info const *emblem);
/* the emblem colour nearest a player's armour colour */
short firefight_emblem_color_from_armor(short armor_color);
long firefight_stats_get_session_medals(long player_index);
short firefight_stats_get_player_top_medals(
	long player_index,
	short *out_medal_ids,
	short *out_counts,
	short max_medals);
void firefight_stats_get_postgame_xp_summary(
	struct firefight_postgame_xp_summary *out_summary);
void firefight_stats_notify_enemy_kill(
	long killer_player_index,
	short category,
	boolean headshot,
	boolean melee,
	boolean grenade,
	boolean boss_or_heavy,
	long points);
short firefight_stats_get_challenges(
	long profile_index,
	short category,
	short period,
	struct firefight_challenge_view *out_list,
	short max_count);
void firefight_stats_get_challenge_summary(
	long profile_index,
	short *out_daily_done,
	short *out_daily_total,
	short *out_weekly_done,
	short *out_weekly_total);

/* ---------- Public API: firefight_network.c */

long firefight_write_network_state(byte *buffer, long size);
boolean firefight_read_network_state(
	byte const *buffer,
	long size,
	boolean first);
long game_engine_firefight_write_network_state(byte *buffer, long size);
boolean game_engine_firefight_read_network_state(
	byte const *buffer,
	long size,
	boolean first);

/* ---------- Public API: firefight_objectives.c

Optional objectives beside a Firefight game's waves (docs/firefight_objectives.md):
a round objective, a challenge for one round, and a set objective, a set
piece on the map that lasts until its set ends. The host's settings
(config.toml's game.firefight_objectives_round, _set and the rest) choose which play;
the host decides everything, and its clients are told in Firefight's state */

enum firefight_round_objective
{
	_ff_round_objective_none = 0,
	_ff_round_objective_kills,
	_ff_round_objective_no_deaths,
	_ff_round_objective_shields,
	_ff_round_objective_kill_first,
	_ff_round_objective_dont_use,
	_ff_round_objective_vehicle_kills,
	_ff_round_objective_travel,
	_ff_round_objective_weapon_only,
	NUMBER_OF_FF_ROUND_OBJECTIVES
};

enum firefight_set_objective
{
	_ff_set_objective_none = 0,
	_ff_set_objective_target,
	_ff_set_objective_generator,
	_ff_set_objective_purge,
	_ff_set_objective_data_link,
	_ff_set_objective_flag,
	_ff_set_objective_rescue,
	_ff_set_objective_oddball,
	_ff_set_objective_collect,
	_ff_set_objective_bomb,
	NUMBER_OF_FF_SET_OBJECTIVES
};

enum firefight_objective_state
{
	_ff_objective_off = 0,
	_ff_objective_running,
	_ff_objective_won,
	_ff_objective_failed,
	/* (Kill X first's, its target killed by no player: done, neither) */
	_ff_objective_tied,
	NUMBER_OF_FF_OBJECTIVE_STATES
};

/* a kill count's kind (the round objective's param) */
enum firefight_objective_kill
{
	_ff_objective_kill_headshot = 0,
	_ff_objective_kill_melee,
	_ff_objective_kill_grenade,
	_ff_objective_kill_splatter,
	/* (then a weapon class's: _ff_objective_kill_weapon + its class) */
	_ff_objective_kill_weapon,
	NUMBER_OF_FF_OBJECTIVE_KILLS = _ff_objective_kill_weapon + 5
};

/* a weapon's class, as Don't use X and the kill counts know it; then each
grenade type (_ff_objective_use_grenade + its type) */
enum firefight_objective_weapon_class
{
	_ff_objective_weapon_automatic = 0,
	_ff_objective_weapon_semi_automatic,
	_ff_objective_weapon_shotgun,
	_ff_objective_weapon_plasma,
	_ff_objective_weapon_rocket,
	NUMBER_OF_FF_OBJECTIVE_WEAPON_CLASSES,
	_ff_objective_use_grenade = NUMBER_OF_FF_OBJECTIVE_WEAPON_CLASSES
};

/* an objective enemy's rank (firefight_objective_spawn_enemy) */
enum
{
	_firefight_objective_rank_regular = 0,
	_firefight_objective_rank_heavy,
	_firefight_objective_rank_target
};

/* (set_flags') */
enum
{
	FF_OBJECTIVE_CONTESTED = 1 << 0,
	FF_OBJECTIVE_ARRIVED = 1 << 1,
	FF_OBJECTIVE_RESCUED = 1 << 2,
	FF_OBJECTIVE_HELD = 1 << 3,
	/* (Bomb's: planted, and being disarmed) */
	FF_OBJECTIVE_PLANTED = 1 << 4,
	FF_OBJECTIVE_DISARMING = 1 << 5
};

#define FF_OBJECTIVE_LABEL_LENGTH 30

#define FF_OBJECTIVE_NOTICE_LENGTH 62

/* (the view's marks: firefight_objectives_view's round_marks, set_units and
set_drops) */
#define FF_OBJECTIVE_WEAPON_MARKS 8
#define FF_OBJECTIVE_UNIT_MARKS 16
#define FF_OBJECTIVE_DROP_MARKS 8

/* what every machine shows of the objectives: the host's, sent in
Firefight's state as it is (its layout is the state's: change it only at its
end, with the mod version) */
struct firefight_objectives_view
{
	byte round_kind;
	byte round_state;
	byte round_param;
	byte round_pad;
	short round_count;
	short round_goal;
	/* (Kill X first's target: its absolute object index, NONE) */
	short round_target;
	short round_result_ticks;
	byte set_kind;
	byte set_state;
	byte set_flags;
	byte set_pad;
	/* (the VIP, the generator's target unit, the oddball's boss, the flag or
	the ball: its absolute object index, NONE) */
	short set_target;
	/* (the flag's or the ball's carrier: an absolute player index, NONE) */
	short set_carrier;
	/* (the time left, NONE: none; a hold's or a rescue's progress and its
	goal, in ticks; a generator's health and its most) */
	long set_ticks_left;
	long set_progress;
	long set_goal;
	/* (Purge's and Rescue's: those left of those there) */
	short set_count;
	short set_total;
	short set_result_ticks;
	short notice_ticks;
	real_point3d set_site;
	real set_radius;
	real_point3d set_goal_point;
	char notice[FF_OBJECTIVE_NOTICE_LENGTH + 2];
	/* (Travel's point; Vehicle kills' and Use X only's vehicle or weapon, by
	name) */
	real_point3d round_point;
	char round_label[FF_OBJECTIVE_LABEL_LENGTH + 2];
	/* (Collect's: what each player carries, by absolute index) */
	byte carried[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	/* (mod version 112) the objectives' marks, absolute object indices
	(NONE: none): Use X only's weapons lying about (a dead player's, a pod's),
	the camp's enemies (Purge's, shown near), Collect's skulls a player
	dropped, and its deposit's skull */
	short round_marks[FF_OBJECTIVE_WEAPON_MARKS];
	short set_units[FF_OBJECTIVE_UNIT_MARKS];
	short set_drops[FF_OBJECTIVE_DROP_MARKS];
	short deposit_object;
	short marks_pad;
	/* (each of set_drops' worth: how many skulls) */
	byte set_drop_values[FF_OBJECTIVE_DROP_MARKS];
};

/* the objectives as this machine shows them, and an object of the view's
(an absolute index) on this machine, NONE for none */
struct firefight_objectives_view const *firefight_objectives_view_get(void);
long firefight_objectives_object(short absolute_index);
/* (firefight_hud.c) the round objective's card and the set objective's: its
title and status, FALSE for none shown */
boolean firefight_objectives_round_card(char *title, size_t title_size, char *status, size_t status_size);
boolean firefight_objectives_set_card(char *title, size_t title_size, char *status, size_t status_size,
	short local_player_absolute);
/* (firefight.c's allow_pick_up) whether a weapon is an objective's that is
never picked up (Collect's skulls, a planted bomb) */
boolean firefight_objectives_pickup_blocked(long weapon_index);
/* (firefight_network.c) the host's, a client's copy */
void firefight_objectives_view_set(struct firefight_objectives_view const *view);
/* (firefight.c) a game's start, each tick (every machine's: the host decides,
a client shows), a round's start (when its round objective starts rolled,
the last one's ended), a wave's start (its wave objective rolled, the round
objective started at its wave), a wave's spawn, a wave's end, a round's end,
the game's end */
void firefight_objectives_initialize_for_new_map(void);
void firefight_objectives_update(void);
/* (firefight.c's post_rasterize_objects) Data Link's hill outline */
void firefight_objectives_render(void);
void firefight_objectives_round_starting(short round_num);
void firefight_objectives_wave_starting(short wave_num);
void firefight_objectives_wave_spawned(short wave_num);
void firefight_objectives_wave_ended(void);
void firefight_objectives_round_ended(void);
void firefight_objectives_dispose(void);
/* (firefight.c) an enemy killed, a player killed */
void firefight_objectives_enemy_killed(long actor_index, long dead_unit_index, long killer_player_index, boolean headshot,
	boolean melee, boolean grenade, boolean splatter);
void firefight_objectives_player_died(long dead_player_index);
/* (weapons.c, units.c) a weapon fired, a grenade thrown, by any unit */
void firefight_objectives_weapon_fired(long weapon_index, long owner_unit_index);
void firefight_objectives_grenade_thrown(long unit_index, short grenade_type);
/* (damage.c) damage to an objective's own (its generator, its marines),
the host's: TRUE when it was dealt here (and goes no further); and what
damage to a unit is scaled by (1: as it is) */
boolean firefight_objectives_apply_damage(long object_index, long attacker_player_index, short attacker_team_index,
	real damage_amount);
real firefight_objectives_damage_scale(long object_index);
/* (firefight_spawner.c) whether a player spawn is in an objective's volume,
and the unit every enemy goes for (a carrier's), NONE */
boolean firefight_objectives_spawn_blocked(real_point3d const *point);
long firefight_objectives_priority_target(void);
/* (actor_perception.c) whether an actor sees nothing: a camp's unit while the
camp has not noticed the players (a player near it, or one of it hurt) */
boolean firefight_objectives_actor_unaware(long actor_index);
/* (firefight_generators.c) whether an object is the enemy generator's hidden
target unit */
boolean firefight_objectives_generator_hidden(long object_index);
/* (menu_functions.c) a map's objectives, as its last game found them: the
set objectives it can play (1 << enum firefight_set_objective), FALSE for a
map never played */
boolean firefight_objectives_map_known(char const *map_name, unsigned long *set_available);
/* an objective's name, as the menus and the HUD have it */
char const *firefight_set_objective_name(short kind);

/* ---------- (firefight_spawner.c) an objective's units: made as a wave's are,
of its faction, but never tracked, nor sent at the players */
long firefight_objective_spawn_enemy(real_point3d const *at, real facing, short rank);
long firefight_objective_spawn_ally(real_point3d const *at, real facing);
boolean firefight_objective_allies_available(void);
long firefight_objective_spawn_vehicle(real_point3d const *at, real facing, boolean heavy);
short firefight_objective_vehicle_spots(real_point3d const *center, real radius, real_point3d *points, real *facings,
	short maximum);
long firefight_objective_ground(real_point3d const *point, real_point3d *ground);
boolean firefight_objective_reachable(real_point3d const *from, real_point3d const *to);
long firefight_objective_reach_group(real_point3d const *point);
void firefight_objective_direct_at(long actor_index, long target_unit_index);
/* (an objective's camp's unit) held where it is put: never charging the
players, fighting from its camp (FALSE: as any) */
void firefight_objective_hold_ground(long actor_index, boolean hold);
/* an ally sent after the nearest player, as Forge's FOLLOW (the friendly
reinforcements', Rescue's marines'): whether it was sent */
boolean firefight_objective_follow(long unit_index);
void firefight_adopt_friendly(long unit_index);
/* (Purge's) the players' vehicles' spots in a camp taken for its enemy
vehicles (each one there, empty, taken away, its slot held), and a slot given
back (its vehicle made again) */
short firefight_objective_take_vehicle_spots(real_point3d const *center, real radius, real_point3d *points,
	real *facings, short *slots, short maximum);
void firefight_objective_release_vehicle_slot(short slot);
/* (Bomb's, the host's) a big blast's damage at a point and round it */
void firefight_objective_blast_damage(real_point3d const *at, real spread);
/* a harmless fire on this machine alone about a point (Collect's deposit) */
void firefight_objective_fire_effect(real_point3d const *position);
/* (Use X only's) a drop of one weapon, a pod of it for each of the players
whose absolute indices `players` has */
void firefight_spawn_ordnance_weapon_for(unsigned long const *players, short words, long weapon_tag);
/* an ordnance drop for some players alone (their absolute indices' bits);
one of a single weapon for every player, and that weapon chosen */
void firefight_spawn_ordnance_drop_for(unsigned long const *players, short words);
void firefight_spawn_ordnance_weapon_for_all(long weapon_tag);
long firefight_objective_ordnance_weapon(long const *avoid, short avoid_count);
/* the vehicle types the players could be given, and one given them */
short firefight_objective_vehicle_types(long *tags, short maximum);
long firefight_objective_give_vehicle(long vehicle_tag);
/* an effect on this machine, the first of the names the map has */
void firefight_objective_effect(char const *const *names, short count, real_point3d const *position);
/* (firefight_generators.c) the generator's looks and its hidden target unit's tag */
long firefight_generator_scenery_tag(void);
long firefight_generator_unit_tag(void);

#endif /* __HALO_FIREFIGHT_H */
