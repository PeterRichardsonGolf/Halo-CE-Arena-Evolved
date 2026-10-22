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
#include "saved games/playlist_profile.h"
#include "saved games/saved_game_files.h"
#include "ae_glue_setup_options.h"
#include "ae_saved_limits.h"
#include "ae_hit_feedback_rules.h"
#include "ae_sensor_range_rules.h"
#include "ae_silent_rules.h"
#include "ae_weapon_rules.h"

void platform_log(char const *format, ...);
/* ui_widget_event_handler_functions.c (declared as menu_functions.c does) */
short ui_widget_port_gametypes(long *indices, short maximum, short *last_used);
/* network_server_manager.c's (its states are its own: pregame is 0, as map_share.c reads them) */
word network_game_server_get_state(struct network_game_server *server, short *substate);
#define SERVER_STATE_PREGAME 0

typedef char verify_vehicle_kinds[AE_SETUP_VEHICLE_KINDS == NUMBER_OF_VARIANT_VEHICLES ? 1 : -1];
typedef char verify_engines[AE_SETUP_ENGINE_CTF == game_engine_ctf && AE_SETUP_ENGINE_SLAYER == game_engine_slayer &&
	AE_SETUP_ENGINE_ODDBALL == game_engine_oddball && AE_SETUP_ENGINE_KING == game_engine_king &&
	AE_SETUP_ENGINE_RACE == game_engine_race ? 1 : -1];

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
	_encoding_ball_melee,       /* a byte of the variant, LETHAL (1) or else STOCK */
	_encoding_goal_radar,       /* the variant's goal radar, 3: nav points in line of sight (bit 27) */
	_encoding_pad5,             /* a field of the variant's pad5: argument its shift (2 bits) */
	_encoding_pack_bit,         /* a bit of the variant's pack_loadout (argument), its value 1 when set */
	_encoding_vehicle_set,      /* a side's vehicle set (argument: the side) */
	_encoding_vehicle_count,    /* a side's count of a vehicle (argument: side * kinds + kind) */
	_encoding_sound_preset      /* SPLIT SCREEN while both SILENT fields are SILENT */
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
	/* (ARENA OPTIONS' finer TIME LIMIT, minutes) */
	[AE_SO_TIME_LIMIT] = SHORT(time_limit, 18, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 15, 20, 25, 30, 45, 60),
	[AE_SO_COUNTDOWN] = BIT(_game_variant_pregame_countdown_bit, 0, 1),

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

	[AE_SO_TEAM_PLAY] = BYTE(universal_variant.teams, 1, 0),
	[AE_SO_FRIENDLY_FIRE] = SHORT(friendly_fire, 4, _friendly_fire_off, _friendly_fire_on, _friendly_fire_shields_only,
		_friendly_fire_explosives_only),
	[AE_SO_BETRAYAL_PENALTY] = SHORT(friendly_fire_penalty, 4, 0, 5, 10, 15),
	[AE_SO_FORCE_EVEN_TEAMS] = OPTION_BYTE(auto_team_balance, 2, 0, 1),
	[AE_SO_FRIENDS_ON_SCREEN] = BIT(_game_variant_allow_friendly_navpoints_bit, 1, 0),

	/* (the PC's sets, then the Xbox's NO GRENADES) */
	[AE_SO_WEAPON_SET] = LONG(universal_variant.weapon_set, 14, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 10),
	[AE_SO_MAP_WEAPONS] = OPTION_BYTE(no_map_weapons, 2, 0, 1),
	[AE_SO_GRENADES_ON_MAP] = { _encoding_pack_bit, 0, AE_MAP_GRENADES_OFF_BIT, 2, { 0, 1 } },
	[AE_SO_STARTING_EQUIPMENT] = BIT(_game_variant_generic_starting_equipment_bit, 0, 1),
	[AE_SO_LOADOUT] = OPTION_BYTE(loadout, 2, _loadout_category, _loadout_custom),
	[AE_SO_PRIMARY] = OPTION_BYTE(primary_weapon, 10, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9),
	[AE_SO_SECONDARY] = OPTION_BYTE(secondary_weapon, 10, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9),
	[AE_SO_INFINITE_GRENADES] = BIT(_game_variant_infinite_grenades_bit, 1, 0),
	[AE_SO_INFINITE_AMMO] = { _encoding_pack_bit, 0, AE_WEAPON_INFINITE_AMMO_BIT, 2, { 0, 1 } },

	[AE_SO_VEHICLE_RESPAWN] = SHORT(vehicle_respawn_time, 7, 0, 30, 60, 90, 120, 180, 300),
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
	[AE_SO_RACE_TYPE] = LONG(game_engine_variant.race.race_type, 3, 0, 1, 2),
	[AE_SO_TEAM_SCORING] = LONG(game_engine_variant.race.team_scoring, 3, 0, 1, 2),
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
	case _encoding_option_byte: return o[encoding->offset];
	case _encoding_radar: return options->radar_players;
	case _encoding_ball_melee: return v[encoding->offset] == 1;
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
	case _encoding_nhe_mode: variant->universal_variant.nhe_mode = (byte)value; break;
	case _encoding_ball_melee: v[encoding->offset] = (byte)value; break;
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
	if (!encoding || !setup_edit.open || encoding->kind == _encoding_sound_preset)
		return 0;
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
	narrow_name(rules.variant.human_readable_game_description, name, sizeof(name));
	platform_log("ae game options: begin '%s' (file %ld) engine %ld", name, profile, rules.variant.game_engine_index);
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

int ae_setup_edit_save_as_new(char *name, int size)
{
	wchar_t wide[MAXIMUM_GAME_VARIANT_NAME_LENGTH];
	char stored[16], candidate[16];
	long profile = NONE;
	int number, index;

	if (!setup_edit.open || size <= 0)
		return 0;
	narrow_name(setup_edit.edit.variant.human_readable_game_description, stored, sizeof(stored));
	for (number = 2; number < 100; number++)
	{
		char suffix[4];
		int length, keep;

		snprintf(suffix, sizeof(suffix), " %d", number);
		length = (int)strlen(suffix);
		keep = (int)strlen(stored);
		if (keep > MAXIMUM_GAME_VARIANT_NAME_LENGTH - 1 - length)
			keep = MAXIMUM_GAME_VARIANT_NAME_LENGTH - 1 - length;
		snprintf(candidate, sizeof(candidate), "%.*s%s", keep, stored, suffix);
		for (index = 0; candidate[index]; index++)
			wide[index] = (wchar_t)(unsigned char)candidate[index];
		wide[index] = 0;
		if (saved_game_file_name_unique(wide))
			break;
	}
	if (number >= 100)
	{
		platform_log("ae game options: save as new: no free name for '%s'", stored);
		return 0;
	}
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

int ae_setup_edit_apply(void)
{
	struct network_game_server *server = global_network_game_server_get();
	struct game_variant variant;
	char name[16];

	if (!setup_edit.open)
		return 0;
	/* (the options first: the server keeps the menus' options only for the menus' own variant) */
	variant = setup_edit.edit.variant;
	player_ui_set_game_variant_options(&setup_edit.edit.options);
	player_ui_set_game_variant(&variant);
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
