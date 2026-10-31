/*
AE_GLUE_SETUP_OPTIONS.C

The game setup spike's GAME OPTIONS glue. See ae_glue_setup_options.h. The encodings are menu_functions.c's
gametype_options[] kinds (gametype_option_value / gametype_option_value_set), in the same order of raw values.
*/

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "game/game_engine.h"
#include "interface/player_ui.h"
#include "networking/network_game_globals.h"
#include "networking/network_server_manager.h"
#include "saved games/arena_gametypes.h"
#include "saved games/playlist_display_name.h"
#include "saved games/playlist_profile.h"
#include "saved games/saved_game_files.h"
#include "ae_glue_setup_options.h"
#include "ae_saved_limits.h"
#include "ae_bomb_snd_rules.h"
#include "ae_hit_feedback_rules.h"
#include "ae_splatter_rules.h"
#include "ae_sensor_range_rules.h"
#include "ae_spawn_modern.h"
#include "ae_glue_setup_maps.h"
#include "ae_silent_rules.h"
#include "ae_weapon_pack.h"
#include "ae_weapon_rules.h"
#include "ae_weapon_slots.h"

void platform_log(char const *format, ...);
/* ui_widget_event_handler_functions.c (declared as menu_functions.c does) */
short ui_widget_port_gametypes(long *indices, short maximum, short *last_used);
/* network_server_manager.c's (its states are its own: pregame is 0, as map_share.c reads them) */
word network_game_server_get_state(struct network_game_server *server, short *substate);
#define SERVER_STATE_PREGAME 0

typedef char verify_vehicle_kinds[AE_SETUP_VEHICLE_KINDS == NUMBER_OF_VARIANT_VEHICLES ? 1 : -1];
typedef char verify_engines[AE_SETUP_ENGINE_CTF == game_engine_ctf && AE_SETUP_ENGINE_SLAYER == game_engine_slayer &&
	AE_SETUP_ENGINE_ODDBALL == game_engine_oddball && AE_SETUP_ENGINE_KING == game_engine_king &&
	AE_SETUP_ENGINE_RACE == game_engine_race && AE_SETUP_ENGINE_BOMB == game_engine_bomb ? 1 : -1];

enum
{
	_encoding_none,             /* no row: the option is not in this build */
	_encoding_long,             /* a long of the variant */
	_encoding_byte,             /* a byte of the variant, read as 0 / 1 */
	_encoding_flag,             /* a bit of the variant's flags (argument), its value 1 when set */
	_encoding_flags,            /* bits of the variant's flags (argument: their mask), the value itself */
	_encoding_health,           /* the variant's health, tenths */
	_encoding_short,            /* a short of the options */
	_encoding_option_byte,      /* a byte of the options */
	_encoding_radar,            /* the options' radar players, and the variant's motion sensor bit */
	_encoding_nhe_mode,         /* the variant's NHE MODE byte */
	_encoding_ball_melee,       /* the low nibble of a byte of the variant, LETHAL (1) or else STOCK */
	_encoding_spawns,           /* bits 4-5 of that same byte: 0 CLASSIC, 1 MODERN spawns (ae_spawn_modern.h) */
	_encoding_goal_radar,       /* the variant's goal radar, 3: nav points in line of sight (bit 27) */
	_encoding_pad5,             /* a field of the variant's pad5: argument its shift (2 bits) */
	_encoding_pack_bit,         /* a bit of the variant's pack_loadout (argument), its value 1 when set */
	_encoding_vehicle_set,      /* a side's vehicle set (argument: the side) */
	_encoding_vehicle_count,    /* a side's count of a vehicle (argument: side * kinds + kind) */
	_encoding_sound_preset,     /* SPLIT SCREEN while both SILENT fields are SILENT */
	_encoding_name,             /* the stored name: one value, set by ae_setup_edit_set_name */
	_encoding_pack_slot,        /* a weapon pack slot in the variant's weapon_pack mask (argument: the slot's bit) */
	_encoding_loadout_slot,     /* PRIMARY / SECONDARY: the game's weapons, then a pack slot's (argument: the slot) */
	_encoding_frags,            /* the variant's starting_frags: 0 the game's own, 255 none, 1-4 */
	_encoding_summary,          /* a row that opens a screen (WEAPON PACKS): one value, nothing stored, never changed */
	_encoding_byte_value,       /* a byte of the variant, its value itself */
	_encoding_byte_bit,         /* a bit of a byte of the variant (argument: its mask), its value 1 when set */
	_encoding_melee_only,       /* Bomb's MELEE ONLY bit, and the PC options it goes with (game_variant_options_default) */
	_encoding_opence_mode       /* the options' OpenCE MODE, and the teams it plays on (menu_functions.c gametype_options_save) */
};

struct encoding { short kind; short offset; unsigned long argument; short count; long values[20]; };

#define VARIANT_FIELD(field) (short)offsetof(struct game_variant, field)
#define OPTIONS_FIELD(field) (short)offsetof(struct game_variant_options, field)
#define LONG(field, count, ...) { _encoding_long, VARIANT_FIELD(field), 0, count, { __VA_ARGS__ } }
#define BYTE(field, ...) { _encoding_byte, VARIANT_FIELD(field), 0, 2, { __VA_ARGS__ } }
#define BIT(bit, ...) { _encoding_flag, 0, FLAG(bit), 2, { __VA_ARGS__ } }
#define BITS(mask, count, ...) { _encoding_flags, 0, (mask), count, { __VA_ARGS__ } }
#define SHORT(field, count, ...) { _encoding_short, OPTIONS_FIELD(field), 0, count, { __VA_ARGS__ } }
#define OPTION_BYTE(field, count, ...) { _encoding_option_byte, OPTIONS_FIELD(field), 0, count, { __VA_ARGS__ } }
#define SCORE(count, ...) LONG(universal_variant.score_to_win, count, __VA_ARGS__)
#define BOMB_FIELD(field) VARIANT_FIELD(game_engine_variant.bomb.field)
#define HEALTH_STYLE(style) ((unsigned long)(style) << _game_variant_health_style_first_bit)
#define DROP(drop) ((unsigned long)(drop) << _game_variant_drop_secondary_first_bit)
#define SET_VALUES { 0, 1, 2, 3, 4, 5, 6, 7, VARIANT_VEHICLE_SET_PC, VARIANT_VEHICLE_SET_CUSTOM }
#define VEHICLE_SET(side) { _encoding_vehicle_set, 0, side, 10, SET_VALUES }
#define VEHICLE_COUNT(side, kind) { _encoding_vehicle_count, 0, (side) * NUMBER_OF_VARIANT_VEHICLES + (kind), 5, \
	{ 0, 1, 2, 3, 4 } }

static struct encoding const encodings[AE_SETUP_OPTIONS] =
{
	[AE_SO_KILLS_TO_WIN] = SCORE(11, 5, 10, 15, 25, 50, 75, 100, 150, 200, 250, 500),
	[AE_SO_CAPTURES_TO_WIN] = SCORE(5, 1, 3, 5, 10, 15),
	[AE_SO_HILL_SCORE] = SCORE(5, 1, 2, 5, 10, 15),
	[AE_SO_BALL_SCORE] = SCORE(5, 1, 2, 5, 10, 15),
	[AE_SO_LAPS_TO_WIN] = SCORE(6, 1, 3, 5, 10, 15, 25),
	[AE_SO_ROUNDS_TO_WIN] = SCORE(7, 1, 2, 3, 4, 5, 7, 10),
	/* (ARENA OPTIONS' finer TIME LIMIT, minutes) */
	[AE_SO_TIME_LIMIT] = SHORT(time_limit, 18, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 15, 20, 25, 30, 45, 60),
	[AE_SO_COUNTDOWN] = BIT(_game_variant_pregame_countdown_bit, 0, 1),
	[AE_SO_NAME] = { _encoding_name, 0, 0, 1, { 0 } },

	[AE_SO_LIVES] = LONG(universal_variant.lives, 4, 0, 1, 3, 5),
	[AE_SO_RESPAWN_TIME] = LONG(universal_variant.respawn_time, 5, 0, 150, 225, 300, 450),
	[AE_SO_RESPAWN_GROWTH] = LONG(universal_variant.respawn_time_growth, 4, 0, 150, 300, 450),
	[AE_SO_SUICIDE_PENALTY] = LONG(universal_variant.suicide_penalty, 4, 0, 150, 300, 450),
	[AE_SO_SHIELDS] = BIT(_game_variant_no_shields_bit, 0, 1),
	[AE_SO_MAXIMUM_HEALTH] = { _encoding_health, 0, 0, 6, { 5, 10, 15, 20, 30, 40 } },
	[AE_SO_ACTIVE_CAMO] = BIT(_game_variant_always_invisible_bit, 1, 0),
	[AE_SO_MOTION_SENSOR] = { _encoding_radar, 0, 0, 4,
		{ _radar_players_all, _radar_players_friends, _radar_players_enemies, _radar_players_none } },
	[AE_SO_SENSOR_RANGE] = BITS(AE_SENSOR_RANGE_MASK, 4, 0, 1UL << AE_SENSOR_RANGE_SHIFT, 2UL << AE_SENSOR_RANGE_SHIFT,
		3UL << AE_SENSOR_RANGE_SHIFT),
	[AE_SO_ODD_MAN_OUT] = BYTE(universal_variant.odd_man_out, 1, 0),
	[AE_SO_CONTROLLER_CROUCH] = BIT(_game_variant_controller_crouch_bit, 1, 0),
	/* (MELEE UNZOOMS: bit 9, ae_melee_unzoom_rules.h; ON sets it, OFF, every gametype from before, has 0) */
	[AE_SO_MELEE_UNZOOMS] = BIT(_game_variant_melee_unzoom_bit, 1, 0),
	/* (MOUSE AIM ASSIST: bit 10, ae_mouse_aim_rules.h; OFF sets it, ALLOWED, every gametype from before, has 0) */
	[AE_SO_MOUSE_AIM_ASSIST] = BIT(_game_variant_block_mouse_aim_assist_bit, 1, 0),
	/* (OpenCE's gametypes' options, game_modes.c: menu_functions.c's gametype_options' values) */
	[AE_SO_HARDCORE] = OPTION_BYTE(hardcore, 2, FALSE, TRUE),

	[AE_SO_TEAM_PLAY] = BYTE(universal_variant.teams, 1, 0),
	[AE_SO_FRIENDLY_FIRE] = SHORT(friendly_fire, 4, _friendly_fire_off, _friendly_fire_on, _friendly_fire_shields_only,
		_friendly_fire_explosives_only),
	/* (FRIENDLY SPLATTER: pad5 bit 3, ae_splatter_rules.h; FAST ONLY sets it, HALO CE, every gametype from before, has 0) */
	[AE_SO_FRIENDLY_SPLATTER] = { _encoding_byte_bit, VARIANT_FIELD(universal_variant.pad5), AE_SPLATTER_BYTE_BIT, 2,
		{ 0, 1 } },
	[AE_SO_BETRAYAL_PENALTY] = SHORT(friendly_fire_penalty, 4, 0, 5, 10, 15),
	[AE_SO_FORCE_EVEN_TEAMS] = OPTION_BYTE(auto_team_balance, 2, 0, 1),
	[AE_SO_FRIENDS_ON_SCREEN] = BIT(_game_variant_allow_friendly_navpoints_bit, 1, 0),

	/* (the PC's sets, then the Xbox's NO GRENADES) */
	[AE_SO_WEAPON_SET] = LONG(universal_variant.weapon_set, 14, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 10),
	[AE_SO_MAP_WEAPONS] = OPTION_BYTE(no_map_weapons, 2, 0, 1),
	[AE_SO_GRENADES_ON_MAP] = { _encoding_pack_bit, 0, AE_MAP_GRENADES_OFF_BIT, 2, { 0, 1 } },
	[AE_SO_WEAPON_PACKS] = { _encoding_summary, 0, 0, 1, { 0 } },
	/* (the packs: the weapon_pack mask's bits, slot n = bit n - 1; PACK WEAPONS ON MAP: pack_loadout bit 4) */
	[AE_SO_PACK_BATTLE_RIFLE] = { _encoding_pack_slot, 0, AE_WEAPON_SLOT_BIT(AE_WEAPON_SLOT_BATTLE_RIFLE), 2, { 0, 1 } },
	[AE_SO_PACK_SMG] = { _encoding_pack_slot, 0, AE_WEAPON_SLOT_BIT(AE_WEAPON_SLOT_SMG), 2, { 0, 1 } },
	[AE_SO_PACK_SMG_SILENCED] = { _encoding_pack_slot, 0, AE_WEAPON_SLOT_BIT(AE_WEAPON_SLOT_SMG_SILENCED), 2, { 0, 1 } },
	[AE_SO_PACK_CARBINE] = { _encoding_pack_slot, 0, AE_WEAPON_SLOT_BIT(AE_WEAPON_SLOT_CARBINE), 2, { 0, 1 } },
	[AE_SO_PACK_AR_SILENCED] = { _encoding_pack_slot, 0, AE_WEAPON_SLOT_BIT(AE_WEAPON_SLOT_AR_SILENCED), 2, { 0, 1 } },
	[AE_SO_PACK_DMR] = { _encoding_pack_slot, 0, AE_WEAPON_SLOT_BIT(AE_WEAPON_SLOT_DMR), 2, { 0, 1 } },
	[AE_SO_PACK_EVO_RIFLE] = { _encoding_pack_slot, 0, AE_WEAPON_SLOT_BIT(AE_WEAPON_SLOT_EVO_RIFLE), 2, { 0, 1 } },
	[AE_SO_PACKS_ON_MAP] = { _encoding_pack_bit, 0, PACK_MAP_SPAWNS_BIT, 2, { 0, 1 } },
	[AE_SO_STARTING_GRENADES] = { _encoding_frags, 0, 0, 6, { 0, STARTING_GRENADES_NONE, 1, 2, 3, 4 } },
	[AE_SO_STARTING_EQUIPMENT] = BIT(_game_variant_generic_starting_equipment_bit, 0, 1),
	[AE_SO_LOADOUT] = OPTION_BYTE(loadout, 2, _loadout_category, _loadout_custom),
	[AE_SO_PRIMARY] = { _encoding_loadout_slot, 0, _pack_loadout_primary, 17, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
		13, 14, 15, 16 } },
	[AE_SO_SECONDARY] = { _encoding_loadout_slot, 0, _pack_loadout_secondary, 17, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
		12, 13, 14, 15, 16 } },
	[AE_SO_INFINITE_GRENADES] = BIT(_game_variant_infinite_grenades_bit, 1, 0),
	[AE_SO_INFINITE_AMMO] = { _encoding_pack_bit, 0, AE_WEAPON_INFINITE_AMMO_BIT, 2, { 0, 1 } },
	[AE_SO_BOTTOMLESS_CLIP] = { _encoding_pack_bit, 0, AE_WEAPON_BOTTOMLESS_CLIP_BIT, 2, { 0, 1 } },

	[AE_SO_VEHICLE_RESPAWN] = SHORT(vehicle_respawn_time, 7, 0, 30, 60, 90, 120, 180, 300),
	/* (VEHICLES DESTRUCTIBLE: bit 11, ae_vehicle_death_rules.h; ON sets it, OFF, every gametype from before, has 0) */
	[AE_SO_VEHICLES_DESTRUCTIBLE] = BIT(_game_variant_vehicles_destructible_bit, 1, 0),
	[AE_SO_VEHICLE_SET_RED] = VEHICLE_SET(0),
	[AE_SO_VEHICLE_COUNT_RED + 0] = VEHICLE_COUNT(0, 0),
	[AE_SO_VEHICLE_COUNT_RED + 1] = VEHICLE_COUNT(0, 1),
	[AE_SO_VEHICLE_COUNT_RED + 2] = VEHICLE_COUNT(0, 2),
	[AE_SO_VEHICLE_COUNT_RED + 3] = VEHICLE_COUNT(0, 3),
	[AE_SO_VEHICLE_COUNT_RED + 4] = VEHICLE_COUNT(0, 4),
	[AE_SO_VEHICLE_COUNT_RED + 5] = VEHICLE_COUNT(0, 5),
	[AE_SO_VEHICLE_SET_BLUE] = VEHICLE_SET(1),
	[AE_SO_VEHICLE_COUNT_BLUE + 0] = VEHICLE_COUNT(1, 0),
	[AE_SO_VEHICLE_COUNT_BLUE + 1] = VEHICLE_COUNT(1, 1),
	[AE_SO_VEHICLE_COUNT_BLUE + 2] = VEHICLE_COUNT(1, 2),
	[AE_SO_VEHICLE_COUNT_BLUE + 3] = VEHICLE_COUNT(1, 3),
	[AE_SO_VEHICLE_COUNT_BLUE + 4] = VEHICLE_COUNT(1, 4),
	[AE_SO_VEHICLE_COUNT_BLUE + 5] = VEHICLE_COUNT(1, 5),

	/* (the PC's rows' names and labels are crossed, menu_functions.c: each row here sets what its label says) */
	[AE_SO_DEATH_BONUS] = BYTE(game_engine_variant.slayer.no_death_bonus, 0, 1),
	[AE_SO_KILL_PENALTY] = BYTE(game_engine_variant.slayer.no_kill_penalty, 0, 1),
	[AE_SO_KILL_IN_ORDER] = BYTE(game_engine_variant.slayer.kill_in_order, 1, 0),
	[AE_SO_OPENCE_MODE] = { _encoding_opence_mode, OPTIONS_FIELD(mode), 0, 4,
		{ _game_mode_none, _game_mode_swat, _game_mode_infection, _game_mode_gun_game } },
	[AE_SO_INFECTION_ZOMBIES] = OPTION_BYTE(infection_initial_zombies, 6, 1, 2, 3, 4, 8, 16),
	[AE_SO_INFECTION_SPEED] = OPTION_BYTE(infection_zombie_speed, 6, 100, 110, 125, 150, 175, 200),
	[AE_SO_INFECTION_DAMAGE] = OPTION_BYTE(infection_skull_damage, 4, 100, 150, 200, 250),
	[AE_SO_INFECTION_RESPAWN] = OPTION_BYTE(infection_respawn_time, 6, 1, 3, 5, 10, 15, 30),
	[AE_SO_INFECTION_COUNTDOWN] = OPTION_BYTE(infection_countdown, 5, 0, 5, 10, 15, 30),
	[AE_SO_INFECTION_SHIELDS] = OPTION_BYTE(infection_zombie_shields, 2, FALSE, TRUE),
	[AE_SO_GUN_GAME_ORDER] = OPTION_BYTE(gun_game_order, 3, _gun_game_order_strongest_first,
		_gun_game_order_weakest_first, _gun_game_order_random),
	[AE_SO_GUN_GAME_MELEE] = OPTION_BYTE(gun_game_melee_demotes, 2, FALSE, TRUE),
	[AE_SO_ASSAULT] = BYTE(game_engine_variant.ctf.assault, 1, 0),
	[AE_SO_SINGLE_FLAG] = LONG(game_engine_variant.ctf.single_flag_time, 6, 0, 1800, 3600, 5400, 9000, 18000),
	[AE_SO_FLAG_MUST_RESET] = BYTE(game_engine_variant.ctf.flag_must_reset, 1, 0),
	[AE_SO_FLAG_AT_HOME] = BYTE(game_engine_variant.ctf.flag_at_home_to_score, 1, 0),
	[AE_SO_MOVING_HILL] = BYTE(game_engine_variant.king.moving_hill, 1, 0),
	[AE_SO_TRAIT_WITH_BALL] = LONG(game_engine_variant.oddball.trait_with_ball, 4, 0, 1, 2, 3),
	[AE_SO_TRAIT_WITHOUT_BALL] = LONG(game_engine_variant.oddball.trait_without_ball, 4, 0, 1, 2, 3),
	[AE_SO_SPEED_WITH_BALL] = LONG(game_engine_variant.oddball.speed_with_ball, 3, 1, 0, 2),
	[AE_SO_BALL_TYPE] = LONG(game_engine_variant.oddball.oddball_ball_type, 3, 0, 1, 2),
	[AE_SO_RANDOM_START] = BYTE(game_engine_variant.oddball.random_start, 1, 0),
	[AE_SO_BALL_COUNT] = LONG(game_engine_variant.oddball.ball_spawn_count, 16, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
		13, 14, 15, 16),
	[AE_SO_BALL_MELEE] = { _encoding_ball_melee, VARIANT_FIELD(universal_variant.ball_melee), 0, 2, { 0, 1 } },
	[AE_SO_SPAWNS] = { _encoding_spawns, VARIANT_FIELD(universal_variant.ball_melee), 0, 2, { AE_SPAWNS_CLASSIC,
		AE_SPAWNS_MODERN } },
	[AE_SO_RACE_TYPE] = LONG(game_engine_variant.race.race_type, 3, 0, 1, 2),
	[AE_SO_TEAM_SCORING] = LONG(game_engine_variant.race.team_scoring, 3, 0, 1, 2),
	/* (Bomb's, seconds: game_engine_variant_cleanup's ranges) */
	[AE_SO_BOMB_ARM_TIME] = LONG(game_engine_variant.bomb.arm_time, 5, 0, 1, 3, 5, 10),
	[AE_SO_BOMB_RESET_TIME] = LONG(game_engine_variant.bomb.reset_time, 5, 0, 10, 15, 30, 60),
	[AE_SO_BOMB_ROUND_TIME] = LONG(game_engine_variant.bomb.round_time, 6, 0, 60, 120, 180, 300, 600),
	[AE_SO_BOMB_NEW_ROUND] = { _encoding_byte_bit, BOMB_FIELD(flags), FLAG(_bomb_new_round_on_score_bit), 2, { 1, 0 } },
	[AE_SO_BOMB_MELEE_ONLY] = { _encoding_melee_only, BOMB_FIELD(flags), FLAG(_bomb_melee_only_bit), 2, { 1, 0 } },
	[AE_SO_BOMB_DROP] = { _encoding_byte_bit, BOMB_FIELD(flags), AE_BOMB_SND_DROP_FLAG, 2, { 1, 0 } },
	[AE_SO_BOMB_TRAITS] = { _encoding_byte_value, BOMB_FIELD(traits), 0, NUMBER_OF_BOMB_TRAITS,
		{ _bomb_traits_none, _bomb_traits_grifball, _bomb_traits_open_pitch } },
	[AE_SO_OBJECTIVES] = { _encoding_goal_radar, 0, 0, 4, { 0, 1, 2, 3 } },

	[AE_SO_HEALTH] = BITS(GAME_VARIANT_HEALTH_STYLE_MASK, 4, HEALTH_STYLE(_health_style_classic),
		HEALTH_STYLE(_health_style_reach), HEALTH_STYLE(_health_style_halo2), HEALTH_STYLE(_health_style_halo3)),
	[AE_SO_FALL_DAMAGE] = BIT(_game_variant_no_falling_damage_bit, 0, 1),
	[AE_SO_NO_SPREAD] = BITS(GAME_VARIANT_NO_SPREAD_MASK, 3, 0, GAME_VARIANT_NO_SPREAD_NHE, GAME_VARIANT_NO_SPREAD_FULL),
	[AE_SO_COMBAT_FEEDBACK] = { _encoding_pad5, 0, 0, 2, { AE_HIT_FEEDBACK_CLASSIC, AE_HIT_FEEDBACK_ARENA } },
	[AE_SO_DROP_SECONDARY] = BITS(GAME_VARIANT_DROP_SECONDARY_MASK, 3, 0, DROP(_drop_secondary_always),
		DROP(_drop_secondary_always_except_power)),
	[AE_SO_PRACTICE] = BIT(_game_variant_practice_bit, 0, 1),
	[AE_SO_TIMERS] = BITS(GAME_VARIANT_TIMERS_MASK, 4, 0, FLAG(_game_variant_item_timers_bit),
		FLAG(_game_variant_item_timers_bit) | FLAG(_game_variant_item_waypoints_bit), FLAG(_game_variant_item_waypoints_bit)),
	[AE_SO_TRAINING] = BIT(_game_variant_training_bit, 0, 1),
	/* (SPAWN HEAT: ON is the bit clear) */
	[AE_SO_SPAWN_HEAT] = BIT(_game_variant_no_spawn_heat_bit, 0, 1),

	[AE_SO_SOUND_PRESET] = { _encoding_sound_preset, 0, 0, 2, { 0, 1 } },
	[AE_SO_SILENT_MOVEMENT] = { _encoding_pad5, 0, AE_SILENT_MOVEMENT_SHIFT, 3,
		{ AE_SILENT_NORMAL, AE_SILENT_JUST_ME, AE_SILENT_SILENT } },
	[AE_SO_SILENT_WEAPONS] = { _encoding_pad5, 0, AE_SILENT_WEAPONS_SHIFT, 3,
		{ AE_SILENT_NORMAL, AE_SILENT_JUST_ME, AE_SILENT_SILENT } },

	[AE_SO_NHE_MODE] = { _encoding_nhe_mode, 0, 0, NUMBER_OF_NHE_MODES, { _nhe_mode_by_vehicles, _nhe_mode_vanilla,
		_nhe_mode_timer_only, _nhe_mode_nhe_and_timer, _nhe_mode_nhe_and_powerups, _nhe_mode_training } },
	[AE_SO_NHE_EXTRAS] = BIT(_game_variant_nhe_extras_bit, 0, 1),
};

/* a variant's flags word bit 0 (playlist_profile.c's _game_variant_is_system_default_bit): a built-in gametype */
#define SYSTEM_DEFAULT_BIT 0
/* the stored name's characters with the NUL (playlist_profile.c's MAXIMUM_GAME_VARIANT_NAME_LENGTH) */
#define MAXIMUM_GAME_VARIANT_NAME_LENGTH \
	((int)NUMBEROF(((struct game_variant *)0)->human_readable_game_description))

/* ---------- the edit */

static short changes_now(void);

struct rules { struct game_variant variant; struct game_variant_options options; };

static struct
{
	int open;
	long profile;
	struct rules edit, base;
	int nhe_here;      /* the picked map is an NHE map, or the NHE disc is loaded (read when the edit opens) */
} setup_edit;

static long raw_get(struct encoding const *encoding, struct rules const *rules);

static int silent_field(byte pad5, int shift)
{
	return (int)((pad5 >> shift) & AE_SILENT_FIELD_MASK);
}

static int split_screen(struct rules const *rules)
{
	byte pad5 = rules->variant.universal_variant.pad5;

	return silent_field(pad5, AE_SILENT_MOVEMENT_SHIFT) == AE_SILENT_SILENT &&
		silent_field(pad5, AE_SILENT_WEAPONS_SHIFT) == AE_SILENT_SILENT;
}

static long raw_get(struct encoding const *encoding, struct rules const *rules)
{
	struct game_variant const *variant = &rules->variant;
	struct game_variant_options const *options = &rules->options;
	byte const *v = (byte const *)variant;
	byte const *o = (byte const *)options;

	switch (encoding->kind)
	{
	case _encoding_long: return *(long const *)(v + encoding->offset);
	case _encoding_byte: return v[encoding->offset] != 0;
	case _encoding_flag: return (variant->universal_variant.flags & encoding->argument) != 0;
	case _encoding_flags: return (long)(variant->universal_variant.flags & encoding->argument);
	case _encoding_health: return (long)(variant->universal_variant.health * 10.0f + 0.5f);
	case _encoding_short: return *(short const *)(o + encoding->offset);
	case _encoding_option_byte: case _encoding_opence_mode: return o[encoding->offset];
	case _encoding_radar: return options->radar_players;
	case _encoding_ball_melee: return ae_ball_melee_of(v[encoding->offset]) == 1;
	case _encoding_spawns: return ae_spawns_of(v[encoding->offset]) == AE_SPAWNS_MODERN;
	case _encoding_nhe_mode: return game_variant_nhe_mode(variant->universal_variant.nhe_mode);
	case _encoding_goal_radar:
		return variant->universal_variant.goal_radar == 1 &&
			TEST_FLAG(variant->universal_variant.flags, _game_variant_objective_in_sight_bit) ? 3 :
			variant->universal_variant.goal_radar;
	case _encoding_pad5:
		/* (COMBAT FEEDBACK's 2 and 3 play ARENA EVOLVED: ae_hit_feedback_rules.h) */
		if (encoding->argument == 0)
			return (variant->universal_variant.pad5 & AE_HIT_FEEDBACK_BYTE_MASK) ? AE_HIT_FEEDBACK_ARENA :
				AE_HIT_FEEDBACK_CLASSIC;
		return silent_field(variant->universal_variant.pad5, (int)encoding->argument);
	case _encoding_pack_bit: return (variant->universal_variant.pack_loadout & encoding->argument) != 0;
	case _encoding_vehicle_set: return options->vehicle_set[encoding->argument];
	case _encoding_vehicle_count:
		return options->vehicle_counts[encoding->argument / NUMBER_OF_VARIANT_VEHICLES]
			[encoding->argument % NUMBER_OF_VARIANT_VEHICLES];
	case _encoding_sound_preset: return split_screen(rules);
	case _encoding_name: return 0;
	case _encoding_summary: return 0;
	case _encoding_byte_value: return v[encoding->offset];
	case _encoding_byte_bit: case _encoding_melee_only: return (v[encoding->offset] & encoding->argument) != 0;
	case _encoding_pack_slot: return (ae_weapon_slots_mask(variant->universal_variant.weapon_pack) & encoding->argument) != 0;
	case _encoding_loadout_slot:
	{
		short pack = game_variant_pack_loadout_weapon_slot(variant->universal_variant.weapon_pack,
			variant->universal_variant.pack_loadout, variant->universal_variant.flags, variant->universal_variant.ball_melee,
			(short)encoding->argument);

		return pack != AE_WEAPON_SLOT_NONE ? AE_SETUP_WEAPON_PACK_FIRST + pack - 1 :
			(encoding->argument == _pack_loadout_primary ? options->primary_weapon : options->secondary_weapon);
	}
	case _encoding_frags:
	{
		byte frags = variant->universal_variant.starting_frags;

		return frags == STARTING_GRENADES_NONE || (frags >= 1 && frags <= 4) ? frags : 0;
	}
	}
	return 0;
}

/* the Xbox's vehicle set: red's, if it is one of the Xbox's (menu_functions.c vehicles_keep) */
static void vehicle_set_mirror(struct rules *rules)
{
	byte red = rules->options.vehicle_set[0];

	rules->variant.universal_variant.vehicle_set = red <= 4 ? red : 0;
}

static void silent_set(struct rules *rules, int shift, int value)
{
	byte *pad5 = &rules->variant.universal_variant.pad5;

	*pad5 = (byte)((*pad5 & ~(AE_SILENT_FIELD_MASK << shift)) | (((unsigned)value & AE_SILENT_FIELD_MASK) << shift));
}

/* the weapon_pack mask changed: the loadout slots keep their packs' weapons where those are still named */
static void pack_mask_set(struct rules *rules, unsigned int mask)
{
	struct universal_variant *universal = &rules->variant.universal_variant;
	unsigned int old_mask = ae_weapon_slots_mask(universal->weapon_pack);

	ae_weapon_slots_loadout_remap(old_mask, mask, &universal->pack_loadout, &universal->flags, &universal->ball_melee);
	universal->weapon_pack = (byte)mask;
}

/* a loadout slot to a value: a game weapon (the pack source off), or a pack slot's weapon (its pack turned on; the
slot's own weapon keeps the stock one the pack falls back to): any named pack, by its rank (ae_weapon_slots.h) */
static int loadout_slot_set(struct rules *rules, short loadout_slot, long value)
{
	struct universal_variant *universal = &rules->variant.universal_variant;
	byte *weapon = loadout_slot == _pack_loadout_primary ? &rules->options.primary_weapon :
		&rules->options.secondary_weapon;

	if (value < AE_SETUP_WEAPON_PACK_FIRST)
	{
		ae_weapon_slots_rank_set(&universal->pack_loadout, &universal->flags, &universal->ball_melee, (unsigned int)loadout_slot, 0);
		*weapon = (byte)value;
		return 1;
	}
	{
		short pack = (short)(value - AE_SETUP_WEAPON_PACK_FIRST + 1);
		unsigned int mask = ae_weapon_slots_mask(universal->weapon_pack) | AE_WEAPON_SLOT_BIT(pack);
		unsigned int source = ae_weapon_slots_source(mask, pack);

		if (!source)
			return 0;
		pack_mask_set(rules, mask);
		ae_weapon_slots_rank_set(&universal->pack_loadout, &universal->flags, &universal->ball_melee, (unsigned int)loadout_slot, source);
		*weapon = (byte)ae_weapon_slots_fallback(pack);
		return 1;
	}
}

static void raw_set(struct encoding const *encoding, long value, struct rules *rules)
{
	struct game_variant *variant = &rules->variant;
	struct game_variant_options *options = &rules->options;
	byte *v = (byte *)variant;
	byte *o = (byte *)options;

	switch (encoding->kind)
	{
	case _encoding_long: *(long *)(v + encoding->offset) = value; break;
	case _encoding_byte: v[encoding->offset] = (byte)value; break;
	case _encoding_flag:
		if (value)
			variant->universal_variant.flags |= encoding->argument;
		else
			variant->universal_variant.flags &= ~encoding->argument;
		break;
	case _encoding_flags:
		variant->universal_variant.flags = (variant->universal_variant.flags & ~encoding->argument) |
			((unsigned long)value & encoding->argument);
		break;
	case _encoding_health: variant->universal_variant.health = (real)value / 10.0f; break;
	case _encoding_short: *(short *)(o + encoding->offset) = (short)value; break;
	case _encoding_option_byte: o[encoding->offset] = (byte)value; break;
	case _encoding_opence_mode:
		/* (Infection is played on teams, Gun Game free for all: game_modes_variant_mode) */
		o[encoding->offset] = (byte)value;
		if (value == _game_mode_infection)
			variant->universal_variant.teams = TRUE;
		else if (value == _game_mode_gun_game)
			variant->universal_variant.teams = FALSE;
		break;
	case _encoding_nhe_mode: variant->universal_variant.nhe_mode = (byte)value; break;
	case _encoding_ball_melee: v[encoding->offset] = ae_ball_melee_set(v[encoding->offset], (int)value); break;
	case _encoding_spawns: v[encoding->offset] = ae_spawns_set(v[encoding->offset], (int)value); break;
	case _encoding_goal_radar:
		/* (LINE OF SIGHT: nav points, in sight only) */
		variant->universal_variant.goal_radar = value == 3 ? 1 : value;
		SET_FLAG(variant->universal_variant.flags, _game_variant_objective_in_sight_bit, value == 3);
		break;
	case _encoding_radar:
		/* (and the Xbox's flag: other players on the tracker or not) */
		options->radar_players = (byte)value;
		if (value == _radar_players_none)
			variant->universal_variant.flags &= ~FLAG(_game_variant_draw_object_in_motion_sensor_bit);
		else
			variant->universal_variant.flags |= FLAG(_game_variant_draw_object_in_motion_sensor_bit);
		break;
	case _encoding_pad5:
		if (encoding->argument == 0)
			variant->universal_variant.pad5 = (byte)((variant->universal_variant.pad5 & ~AE_HIT_FEEDBACK_BYTE_MASK) |
				((unsigned)value & AE_HIT_FEEDBACK_BYTE_MASK));
		else
			silent_set(rules, (int)encoding->argument, (int)value);
		break;
	case _encoding_pack_bit:
		if (value)
			variant->universal_variant.pack_loadout |= (byte)encoding->argument;
		else
			variant->universal_variant.pack_loadout &= (byte)~encoding->argument;
		break;
	case _encoding_vehicle_set:
		options->vehicle_set[encoding->argument] = (byte)value;
		vehicle_set_mirror(rules);
		break;
	case _encoding_vehicle_count:
		/* (a count changed makes the side's set CUSTOM, menu_functions.c vehicles_update) */
		options->vehicle_counts[encoding->argument / NUMBER_OF_VARIANT_VEHICLES]
			[encoding->argument % NUMBER_OF_VARIANT_VEHICLES] = (byte)PIN(value, 0, MAXIMUM_VARIANT_VEHICLE_COUNT);
		options->vehicle_set[encoding->argument / NUMBER_OF_VARIANT_VEHICLES] = VARIANT_VEHICLE_SET_CUSTOM;
		vehicle_set_mirror(rules);
		break;
	case _encoding_sound_preset:
		/* (SPLIT SCREEN: both SILENT; CUSTOM: the base's two back, or NORMAL when the base was SPLIT SCREEN too) */
		if (value)
		{
			silent_set(rules, AE_SILENT_MOVEMENT_SHIFT, AE_SILENT_SILENT);
			silent_set(rules, AE_SILENT_WEAPONS_SHIFT, AE_SILENT_SILENT);
		}
		else
		{
			byte base = setup_edit.base.variant.universal_variant.pad5;
			int keep = !split_screen(&setup_edit.base);

			silent_set(rules, AE_SILENT_MOVEMENT_SHIFT, keep ? silent_field(base, AE_SILENT_MOVEMENT_SHIFT) : 0);
			silent_set(rules, AE_SILENT_WEAPONS_SHIFT, keep ? silent_field(base, AE_SILENT_WEAPONS_SHIFT) : 0);
		}
		break;
	case _encoding_name: break;
	case _encoding_summary: break;
	case _encoding_pack_slot:
	{
		unsigned int mask = ae_weapon_slots_mask(variant->universal_variant.weapon_pack);

		pack_mask_set(rules, value ? (mask | encoding->argument) : (mask & ~encoding->argument));
		break;
	}
	case _encoding_loadout_slot:
		if (!loadout_slot_set(rules, (short)encoding->argument, value))
			platform_log("ae game options: no source reaches that pack weapon (two packs at most)");
		break;
	case _encoding_frags: variant->universal_variant.starting_frags = (byte)value; break;
	case _encoding_byte_value: v[encoding->offset] = (byte)value; break;
	case _encoding_byte_bit:
	case _encoding_melee_only:
		if (value)
			v[encoding->offset] |= (byte)encoding->argument;
		else
			v[encoding->offset] &= (byte)~encoding->argument;
		/* (MELEE ONLY on: no start weapons and none on the map, as a new melee-only gametype has them; off: the
		weapon set's start and the map's weapons again, where the start was melee only's none + none) */
		if (encoding->kind == _encoding_melee_only)
		{
			if (value)
			{
				options->loadout = _loadout_custom;
				options->primary_weapon = _loadout_weapon_none;
				options->secondary_weapon = _loadout_weapon_none;
				options->no_map_weapons = TRUE;
			}
			else if (options->loadout == _loadout_custom && options->primary_weapon == _loadout_weapon_none &&
				options->secondary_weapon == _loadout_weapon_none)
			{
				options->loadout = _loadout_category;
				options->no_map_weapons = FALSE;
			}
		}
		break;
	}
}

/* the value's place in the row's list: its own, else the nearest (menu_functions.c gametype_option_index) */
static int value_index(struct encoding const *encoding, long value)
{
	short index, best = 0;
	long best_distance = 0x7FFFFFFF;

	for (index = 0; index < encoding->count; index++)
	{
		long distance = encoding->values[index] > value ? encoding->values[index] - value : value - encoding->values[index];

		if (distance < best_distance)
		{
			best_distance = distance;
			best = index;
		}
	}
	return best;
}

static struct encoding const *encoding_of(int option)
{
	return option >= 0 && option < AE_SETUP_OPTIONS && encodings[option].kind != _encoding_none ?
		&encodings[option] : NULL;
}

static int access_get(void *context, int option)
{
	struct encoding const *encoding = encoding_of(option);

	(void)context;
	return encoding && setup_edit.open ? value_index(encoding, raw_get(encoding, &setup_edit.edit)) : -1;
}

static int access_changed(void *context, int option)
{
	struct encoding const *encoding = encoding_of(option);

	(void)context;
	/* (SOUND PRESET follows the SILENT rows: their changes are counted, not it) */
	if (!encoding || !setup_edit.open || encoding->kind == _encoding_sound_preset || encoding->kind == _encoding_summary)
		return 0;
	if (encoding->kind == _encoding_name)
		return memcmp(setup_edit.edit.variant.human_readable_game_description,
			setup_edit.base.variant.human_readable_game_description,
			sizeof(setup_edit.edit.variant.human_readable_game_description)) != 0;
	return raw_get(encoding, &setup_edit.edit) != raw_get(encoding, &setup_edit.base);
}

static void narrow_name(wchar_t const *wide, char *name, int size)
{
	int index;

	if (size <= 0)
		return;
	for (index = 0; index < size - 1 && index < MAXIMUM_GAME_VARIANT_NAME_LENGTH - 1 && wide[index]; index++)
		name[index] = wide[index] < 0x80 ? (char)wide[index] : '?';
	name[index] = 0;
}

static int names_match(char const *a, char const *b)
{
	for (; *a && *b; a++, b++)
	{
		char x = *a >= 'a' && *a <= 'z' ? (char)(*a - 32) : *a, y = *b >= 'a' && *b <= 'z' ? (char)(*b - 32) : *b;

		if (x != y)
			return 0;
	}
	return *a == *b;
}

int ae_setup_edit_begin(long profile)
{
	struct rules rules;
	char name[16];

	csmemset(&rules, 0, sizeof(rules));
	if (!playlist_profile_get(profile, &rules.variant) || !playlist_profile_get_options(profile, &rules.options))
	{
		platform_log("ae game options: begin: gametype %ld can't be read", profile);
		return 0;
	}
	setup_edit.edit = rules;
	setup_edit.base = rules;
	setup_edit.profile = profile;
	setup_edit.open = 1;
	setup_edit.nhe_here = ae_setup_current_map_is_nhe();
	narrow_name(rules.variant.human_readable_game_description, name, sizeof(name));
	platform_log("ae game options: begin '%s' (file %ld) engine %ld", name, profile, rules.variant.game_engine_index);
	return 1;
}

int ae_setup_edit_begin_rules(struct game_variant const *variant, struct game_variant_options const *options)
{
	char name[16];

	if (!variant || !options)
		return 0;
	csmemset(&setup_edit.edit, 0, sizeof(setup_edit.edit));
	setup_edit.edit.variant = *variant;
	setup_edit.edit.options = *options;
	setup_edit.base = setup_edit.edit;
	setup_edit.profile = NONE;
	setup_edit.open = 1;
	setup_edit.nhe_here = ae_setup_current_map_is_nhe();
	narrow_name(variant->human_readable_game_description, name, sizeof(name));
	platform_log("ae game options: begin '%s' (no file) engine %ld", name, variant->game_engine_index);
	return 1;
}

int ae_setup_edit_begin_named(char const *name)
{
	long indices[AE_SAVED_GAMES_MAXIMUM];
	short count, last = 0, index;

	count = ui_widget_port_gametypes(indices, (short)NUMBEROF(indices), &last);
	if (count <= 0)
		return 0;
	for (index = 0; name && name[0] && index < count; index++)
	{
		struct game_variant variant;
		char stored[16];

		if (!playlist_profile_get(indices[index], &variant))
			continue;
		narrow_name(variant.human_readable_game_description, stored, sizeof(stored));
		if (names_match(stored, name))
			return ae_setup_edit_begin(indices[index]);
	}
	if (name && name[0])
		platform_log("ae game options: no gametype '%s': the last used one", name);
	return ae_setup_edit_begin(indices[last >= 0 && last < count ? last : 0]);
}

int ae_setup_edit_open(void)
{
	return setup_edit.open;
}

void ae_setup_edit_access(struct ae_setup_access *access)
{
	access->get = access_get;
	access->changed = access_changed;
	access->context = NULL;
	access->engine = setup_edit.open ? (int)setup_edit.edit.variant.game_engine_index : 0;
	access->ae_rules = setup_edit.open &&
		arena_gametype_has_ae_rules(setup_edit.base.variant.human_readable_game_description);
	access->nhe_here = setup_edit.open && setup_edit.nhe_here;
}

void ae_setup_edit_set(int option, int index)
{
	struct encoding const *encoding = encoding_of(option);

	if (!encoding || !setup_edit.open || index < 0 || index >= encoding->count)
		return;
	raw_set(encoding, encoding->values[index], &setup_edit.edit);
	platform_log("ae game options: set %s %s", ae_setup_label(NULL, option), ae_setup_value_text(option, index));
}

int ae_setup_edit_base(int option)
{
	struct encoding const *encoding = encoding_of(option);

	return encoding && setup_edit.open ? value_index(encoding, raw_get(encoding, &setup_edit.base)) : -1;
}

void ae_setup_edit_defaults(void)
{
	if (!setup_edit.open)
		return;
	setup_edit.edit = setup_edit.base;
	platform_log("ae game options: defaults");
}

void ae_setup_edit_defaults_packs(void)
{
	struct universal_variant *edit = &setup_edit.edit.variant.universal_variant;
	struct universal_variant const *base = &setup_edit.base.variant.universal_variant;
	/* (the two loadout slots' sources and MAP SPAWNS; GRENADES ON MAP, INFINITE AMMO, BOTTOMLESS CLIP are EQUIPMENT's) */
	byte mask = (byte)(PACK_LOADOUT_SOURCE_MASK << PACK_LOADOUT_SLOT_SHIFT(_pack_loadout_primary) |
		PACK_LOADOUT_SOURCE_MASK << PACK_LOADOUT_SLOT_SHIFT(_pack_loadout_secondary) | PACK_MAP_SPAWNS_BIT);

	if (!setup_edit.open)
		return;
	edit->weapon_pack = base->weapon_pack;
	edit->pack_loadout = (byte)((edit->pack_loadout & ~mask) | (base->pack_loadout & mask));
	setup_edit.edit.options.primary_weapon = setup_edit.base.options.primary_weapon;
	setup_edit.edit.options.secondary_weapon = setup_edit.base.options.secondary_weapon;
	platform_log("ae game options: defaults (weapon packs)");
}

void ae_setup_edit_name(char *name, int size)
{
	if (!setup_edit.open)
	{
		if (size > 0)
			name[0] = 0;
		return;
	}
	narrow_name(setup_edit.edit.variant.human_readable_game_description, name, size);
}

void ae_setup_edit_set_name(char const *name)
{
	wchar_t *stored = setup_edit.edit.variant.human_readable_game_description;
	int index, length = 0;

	if (!setup_edit.open || !name)
		return;
	/* (printable ASCII, at most the stored length; a name of nothing keeps the one it has) */
	csmemset(stored, 0, sizeof(setup_edit.edit.variant.human_readable_game_description));
	for (index = 0; name[index] && length < MAXIMUM_GAME_VARIANT_NAME_LENGTH - 1; index++)
	{
		unsigned char letter = (unsigned char)name[index];

		if (letter >= 0x20 && letter < 0x7F)
			stored[length++] = (wchar_t)letter;
	}
	if (!length)
		memcpy(stored, setup_edit.base.variant.human_readable_game_description,
			sizeof(setup_edit.edit.variant.human_readable_game_description));
	platform_log("ae game options: name '%s'", name);
}

char const *ae_setup_edit_reason(int option)
{
	int slot = option - AE_SO_PACK_FIRST + 1;

	/* (a pack that is not loaded here and not on in the gametype: it can't be turned on) */
	if (!setup_edit.open || slot < 1 || slot > AE_SETUP_PACKS || ae_weapon_pack_slot_loaded((short)slot))
		return NULL;
	return ae_weapon_slots_mask(setup_edit.edit.variant.universal_variant.weapon_pack) & AE_WEAPON_SLOT_BIT(slot) ?
		NULL : "NOT INSTALLED";
}

/* the name a new gametype file of the copy takes: the copy's name with " 2", " 3" ... until it is free (a name typed
in NAME kept as typed when it is free); its wide form in wide. 0 when none is free */
static int free_name(char *candidate, int size, wchar_t *wide)
{
	char stored[16];
	int number = 0, index, exact = 0;

	narrow_name(setup_edit.edit.variant.human_readable_game_description, stored, sizeof(stored));
	/* (a name typed in NAME is kept as typed when it is free) */
	if (memcmp(setup_edit.edit.variant.human_readable_game_description,
		setup_edit.base.variant.human_readable_game_description,
		sizeof(setup_edit.edit.variant.human_readable_game_description)) != 0)
	{
		for (index = 0; stored[index]; index++)
			wide[index] = (wchar_t)(unsigned char)stored[index];
		wide[index] = 0;
		if (index > 0 && saved_game_file_name_unique(wide))
		{
			snprintf(candidate, (size_t)size, "%s", stored);
			exact = 1;
		}
	}
	for (number = exact ? 100 : 2; number < 100; number++)
	{
		char suffix[4];
		int length, keep;

		snprintf(suffix, sizeof(suffix), " %d", number);
		length = (int)strlen(suffix);
		keep = (int)strlen(stored);
		if (keep > MAXIMUM_GAME_VARIANT_NAME_LENGTH - 1 - length)
			keep = MAXIMUM_GAME_VARIANT_NAME_LENGTH - 1 - length;
		snprintf(candidate, (size_t)size, "%.*s%s", keep, stored, suffix);
		for (index = 0; candidate[index]; index++)
			wide[index] = (wchar_t)(unsigned char)candidate[index];
		wide[index] = 0;
		if (saved_game_file_name_unique(wide))
			break;
	}
	if (!exact && number >= 100)
	{
		platform_log("ae game options: save as new: no free name for '%s'", stored);
		return 0;
	}
	return 1;
}

int ae_setup_edit_free_name(char *name, int size)
{
	wchar_t wide[MAXIMUM_GAME_VARIANT_NAME_LENGTH];
	char candidate[16];

	if (size > 0)
		name[0] = 0;
	if (!setup_edit.open || size <= 0 || !free_name(candidate, sizeof(candidate), wide))
		return 0;
	snprintf(name, (size_t)size, "%s", candidate);
	return 1;
}

int ae_setup_edit_save_as_new(char *name, int size)
{
	wchar_t wide[MAXIMUM_GAME_VARIANT_NAME_LENGTH];
	char candidate[16];
	long profile = NONE;
	int index;

	if (!setup_edit.open || size <= 0 || !free_name(candidate, sizeof(candidate), wide))
		return 0;
	csmemset(setup_edit.edit.variant.human_readable_game_description, 0,
		sizeof(setup_edit.edit.variant.human_readable_game_description));
	for (index = 0; wide[index] && index < MAXIMUM_GAME_VARIANT_NAME_LENGTH - 1; index++)
		setup_edit.edit.variant.human_readable_game_description[index] = wide[index];
	SET_FLAG(setup_edit.edit.variant.flags, SYSTEM_DEFAULT_BIT, FALSE);
	profile = playlist_profile_new(0, wide);
	if (profile == NONE)
	{
		platform_log("ae game options: save as new: '%s' could not be made", candidate);
		return 0;
	}
	playlist_profile_save_with_options(profile, &setup_edit.edit.variant, &setup_edit.edit.options);
	playlist_profile_wait_for_write();
	setup_edit.base = setup_edit.edit;
	setup_edit.profile = profile;
	snprintf(name, (size_t)size, "%s", candidate);
	platform_log("ae game options: saved as new '%s' (file %ld)", candidate, profile);
	return 1;
}

int ae_setup_edit_set_display_name(char const *name)
{
	wchar_t wide[PLAYLIST_DISPLAY_NAME_MAXIMUM_LENGTH + 1];
	int index, set;

	if (!setup_edit.open || !name || setup_edit.profile == NONE)
		return 0;
	for (index = 0; name[index] && index < PLAYLIST_DISPLAY_NAME_MAXIMUM_LENGTH; index++)
		wide[index] = (wchar_t)(unsigned char)name[index];
	wide[index] = 0;
	set = arena_gametype_set_own_display_name(setup_edit.profile, wide);
	platform_log("ae game options: display name '%s': %s", name, set ? "set" : "refused");
	return set;
}

int ae_setup_name_taken(char const *name)
{
	long indices[AE_SAVED_GAMES_MAXIMUM];
	wchar_t wide[MAXIMUM_GAME_VARIANT_NAME_LENGTH];
	short count, last = 0, index;
	int length;

	if (!name || !name[0])
		return 0;
	/* (each listed gametype's names: its stored name, the table's display name, a player's own longer one) */
	count = ui_widget_port_gametypes(indices, (short)NUMBEROF(indices), &last);
	for (index = 0; index < count; index++)
	{
		struct game_variant variant;
		struct arena_gametype_info info;
		wchar_t own[PLAYLIST_DISPLAY_NAME_MAXIMUM_LENGTH + 1];
		char narrow[PLAYLIST_DISPLAY_NAME_MAXIMUM_LENGTH + 1];
		int at;

		if (!playlist_profile_get(indices[index], &variant))
			continue;
		narrow_name(variant.human_readable_game_description, narrow, sizeof(narrow));
		if (names_match(narrow, name))
			return 1;
		if (arena_gametype_info(variant.human_readable_game_description, &info) && info.display_name &&
			names_match(info.display_name, name))
			return 1;
		if (arena_gametype_own_display_name(indices[index], own, (short)NUMBEROF(own)))
		{
			for (at = 0; own[at] && at < (int)sizeof(narrow) - 1; at++)
				narrow[at] = own[at] < 0x80 ? (char)own[at] : '?';
			narrow[at] = 0;
			if (names_match(narrow, name))
				return 1;
		}
	}
	/* (a stored name's length: its save directory too) */
	length = (int)strlen(name);
	if (length < MAXIMUM_GAME_VARIANT_NAME_LENGTH)
	{
		for (index = 0; index < length; index++)
			wide[index] = (wchar_t)(unsigned char)name[index];
		wide[index] = 0;
		return !saved_game_file_name_unique(wide);
	}
	return 0;
}

int ae_setup_edit_can_save(void)
{
	struct game_variant const *base = &setup_edit.base.variant;

	/* (a saved file, the player's own: not a built-in, not one AE seeds (AE's, NHE's: its stored name is in
	arena_gametype_names.c), even when the player renamed the copy) */
	return setup_edit.open && setup_edit.profile != NONE && (setup_edit.profile & 0x80000000) &&
		!TEST_FLAG(base->flags, SYSTEM_DEFAULT_BIT) && !arena_gametype_info(base->human_readable_game_description, NULL);
}

int ae_setup_edit_save(void)
{
	char name[16];

	if (!ae_setup_edit_can_save())
		return 0;
	playlist_profile_save_with_options(setup_edit.profile, &setup_edit.edit.variant, &setup_edit.edit.options);
	playlist_profile_wait_for_write();
	setup_edit.base = setup_edit.edit;
	narrow_name(setup_edit.edit.variant.human_readable_game_description, name, sizeof(name));
	platform_log("ae game options: saved '%s' (file %ld)", name, setup_edit.profile);
	return 1;
}

int ae_setup_edit_apply(void)
{
	struct network_game_server *server = global_network_game_server_get();
	struct game_variant variant;
	char name[16];

	if (!setup_edit.open)
		return 0;
	/* (the variant, then its options: player_ui_set_game_variant resets the options to the defaults, and the server
	keeps the menus' options only for the menus' own variant; the other way round every game ran on the defaults:
	radar ALL, no time limit, the map's vehicles) */
	variant = setup_edit.edit.variant;
	player_ui_set_game_variant(&variant);
	player_ui_set_game_variant_options(&setup_edit.edit.options);
	/* (the server takes a gametype in its pregame only: it asserts so) */
	if (server && network_game_server_get_state(server, NULL) == SERVER_STATE_PREGAME)
		network_game_server_change_game_variant(server, &variant);
	else if (server)
		server = NULL;
	game_engine_override_game_variant(&variant);
	narrow_name(variant.human_readable_game_description, name, sizeof(name));
	platform_log("ae game options: applied '%s' (%d changed)%s", name, changes_now(),
		server ? " to the server" : "");
	return 1;
}

short ae_setup_gametypes(char (*names)[16], long *profiles, short maximum, short *current)
{
	long indices[AE_SAVED_GAMES_MAXIMUM];
	short count, last = 0, index, written = 0;

	*current = -1;
	count = ui_widget_port_gametypes(indices, (short)NUMBEROF(indices), &last);
	for (index = 0; index < count && written < maximum; index++)
	{
		struct game_variant variant;

		if (!playlist_profile_get(indices[index], &variant))
			continue;
		narrow_name(variant.human_readable_game_description, names[written], 16);
		profiles[written] = indices[index];
		if (setup_edit.open && indices[index] == setup_edit.profile)
			*current = written;
		written++;
	}
	return written;
}

int ae_setup_profile_values(long profile, int *values, int count)
{
	struct rules rules;
	int option;

	csmemset(&rules, 0, sizeof(rules));
	if (!values || !playlist_profile_get(profile, &rules.variant) || !playlist_profile_get_options(profile, &rules.options))
		return 0;
	for (option = 0; option < count && option < AE_SETUP_OPTIONS; option++)
	{
		struct encoding const *encoding = encoding_of(option);

		values[option] = encoding ? value_index(encoding, raw_get(encoding, &rules)) : -1;
	}
	return 1;
}

int ae_setup_edit_check(void)
{
	int option, ok = 1;

	for (option = 0; option < AE_SETUP_OPTIONS; option++)
	{
		struct ae_setup_row const *row = ae_setup_row(option);
		struct encoding const *encoding = encoding_of(option);

		if (row && encoding && row->value_count == encoding->count)
			continue;
		platform_log("ae game options: row %d (%s): the model has %d values, the glue %d", option,
			row ? row->label : "?", row ? row->value_count : -1, encoding ? encoding->count : -1);
		ok = 0;
	}
	return ok;
}

/* N CHANGED, through the model (the rows a gametype has) */
static short changes_now(void)
{
	struct ae_setup_access access;

	ae_setup_edit_access(&access);
	return ae_setup_changes(&access);
}
