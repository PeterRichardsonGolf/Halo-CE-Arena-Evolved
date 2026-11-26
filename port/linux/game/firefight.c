/*
FIREFIGHT.C

port: Self-contained Halo: Reach / ODST Firefight core game engine module.
Implements:
- Core firefight_engine vtable and 6 built-in gametype variants
- Set -> Round -> Wave -> Bonus Round state machine
- Configurable Pre-Round Preparation Countdown (5s..60s)
- Configurable Next Wave Start Condition (<=2 Enemies, 100% Clear, 30s..120s Timer)
- Shared Team Lives Pool, Score Bonus Lives, and Last Man Standing / Wipe logic
- Enemy Inter-Faction Diplomacy (Allied vs Players or Classic 3-Way Infighting)
- Progressive Per-Set Enemy Health & Shield Escalation
- Playable Covenant Elite / Heretic Mode vs UNSC Marines & ODSTs
- Granular Skull Progression, Custom Pool, Exclude Mask, and 40-Skull bridge
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cseries/cseries.h"
#include "ai/actors.h"
#include "ai/actor_definitions.h"
#include "ai/ai.h"
#include "game/cheats.h"
#include "cseries/errors.h"
#include "game/game.h"
#include "game/game_allegiance.h"
#include "game/game_engine.h"
#include "game/game_globals.h"
#include "game/players.h"
#include "interface/hud_messaging.h"
#include "items/equipment_definitions.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"
#include "main/main.h"
#include "math/real_math.h"
#include "memory/data.h"
#include "networking/network_game_globals.h"
#include "objects/damage.h"
#include "objects/object_definitions.h"
#include "physics/collision_model_definitions.h"
#include "objects/objects.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "tag_files/tag_files.h"
#include "tag_files/tag_groups.h"
#include "text/unicode.h"
#include "units/bipeds.h"
#include "units/units.h"

#include "halo_firefight.h"
#include "ae_firefight_glue.h"
#include "halo_port_limits.h"
#include "bots.h"
#include "halo_skulls.h"

/* ---------- globals */

struct firefight_globals ff_state;
static short g_server_player_scale_percent = 0;

short firefight_server_player_scale_percent_get(
	void)
{
	return g_server_player_scale_percent;
}

void firefight_server_player_scale_percent_set(
	short percent)
{
	g_server_player_scale_percent = (short)PIN(percent, 0, 100);
}

extern struct game_variant global_variant;
boolean network_game_distributed_client(void);

/* ---------- built-in variant profiles */

struct firefight_builtin_profile
{
	char const *variant_name;
	wchar_t const *display_name;
	short set_limit;
	short starting_lives;
	short bonus_lives_per_round;
	short bonus_lives_per_set;
	short max_lives_cap;
	short generator_count;
	unsigned long custom_skulls;
	byte difficulty;
	byte wave_faction;
	byte generator_fail_mode;
	byte ordnance_drops;
	byte bonus_round_enabled;
	byte progression_skulls;
	byte generator_attackers;
	byte vehicle_drops_enabled;
	/* its loadout (_loadout_weapon_*; 0 and 0: Firefight's own, the assault
	rifle and pistol of firefight_variant_options_default), and whether
	it starts with Outbreak, which is no Reach skull (custom_skulls has not
	got it): firefight_builtin_variant_options */
	byte primary_weapon;
	byte secondary_weapon;
	byte outbreak;
	/* its WAVE SQUAD SIZE and ENEMY LOADOUT (0: the defaults) */
	byte wave_size;
	byte enemy_loadout;
};

static struct firefight_builtin_profile const built_in_firefight_profiles[NUMBER_OF_BUILT_IN_FIREFIGHT_PROFILES] =
{
	/* 0: Standard Firefight */
	{
		"firefight",
		L"Firefight",
		0, 7, 1, 3, 15, 0,
		0UL,
		_firefight_difficulty_normal,
		_firefight_faction_covenant,
		_firefight_generator_fail_all,
		_firefight_ordnance_per_round,
		TRUE, TRUE, _firefight_generator_attackers_50, TRUE
	},
	/* 1: Generator Defense */
	{
		"generator_defense",
		L"Generator Defense",
		3, 7, 1, 3, 15, 3,
		0UL,
		_firefight_difficulty_normal,
		_firefight_faction_covenant,
		_firefight_generator_fail_all,
		_firefight_ordnance_per_round,
		TRUE, TRUE, _firefight_generator_attackers_50, TRUE
	},
	/* 2: Gruntpocalypse */
	{
		"gruntpocalypse",
		L"Gruntpocalypse",
		1, 10, 1, 3, 20, 0,
		(1UL << _firefight_skull_grunt_birthday_party_bit) |
			(1UL << _firefight_skull_catch_bit) |
			(1UL << _firefight_skull_cowbell_bit),
		_firefight_difficulty_heroic,
		_firefight_faction_grunts,
		_firefight_generator_fail_all,
		_firefight_ordnance_per_round,
		TRUE, FALSE, _firefight_generator_attackers_50, FALSE,
		_loadout_weapon_pistol, _loadout_weapon_none
	},
	/* 3: Rocketfight */
	{
		"rocketfight",
		L"Rocketfight",
		1, 10, 1, 3, 20, 0,
		(1UL << _firefight_skull_cowbell_bit) |
			(1UL << _firefight_skull_boom_bit),
		_firefight_difficulty_heroic,
		_firefight_faction_mixed,
		_firefight_generator_fail_all,
		_firefight_ordnance_frequent,
		TRUE, TRUE, _firefight_generator_attackers_50, TRUE,
		_loadout_weapon_rocket_launcher, _loadout_weapon_pistol
	},
	/* 4: Floodfight */
	{
		"floodfight",
		L"Floodfight",
		0, 7, 1, 3, 15, 0,
		(1UL << _firefight_skull_tough_luck_bit),
		_firefight_difficulty_heroic,
		_firefight_faction_flood,
		_firefight_generator_fail_all,
		_firefight_ordnance_per_round,
		TRUE, TRUE, _firefight_generator_attackers_50, TRUE,
		_loadout_weapon_shotgun, _loadout_weapon_assault_rifle, TRUE, _firefight_wave_size_250
	},
	/* 5: Mythic Firefight */
	{
		"mythic_firefight",
		L"Mythic Firefight",
		0, 5, 1, 2, 10, 0,
		(1UL << _firefight_skull_mythic_bit) |
			(1UL << _firefight_skull_tilt_bit) |
			(1UL << _firefight_skull_thunderstorm_bit) |
			(1UL << _firefight_skull_tough_luck_bit),
		_firefight_difficulty_legendary,
		_firefight_faction_covenant,
		_firefight_generator_fail_all,
		_firefight_ordnance_per_round,
		TRUE, TRUE, _firefight_generator_attackers_50, TRUE
	},
	/* 6: Fiesta: Firefight's rules, two random weapons at each spawn */
	{
		"fiesta",
		L"Fiesta",
		0, 7, 1, 3, 15, 0,
		0UL,
		_firefight_difficulty_normal,
		_firefight_faction_covenant,
		_firefight_generator_fail_all,
		_firefight_ordnance_per_round,
		TRUE, TRUE, _firefight_generator_attackers_50, TRUE,
		_loadout_weapon_random, _loadout_weapon_random, FALSE, 0, _firefight_enemy_loadout_random
	},
	/* 7: Grunt Fiesta: Gruntpocalypse's rules, and the same */
	{
		"grunt_fiesta",
		L"Grunt Fiesta",
		1, 10, 1, 3, 20, 0,
		(1UL << _firefight_skull_grunt_birthday_party_bit) |
			(1UL << _firefight_skull_catch_bit) |
			(1UL << _firefight_skull_cowbell_bit),
		_firefight_difficulty_heroic,
		_firefight_faction_grunts,
		_firefight_generator_fail_all,
		_firefight_ordnance_per_round,
		TRUE, FALSE, _firefight_generator_attackers_50, FALSE,
		_loadout_weapon_random, _loadout_weapon_random, FALSE, 0, _firefight_enemy_loadout_random
	}
};

/* ---------- skull mapping helpers */

int firefight_map_reach_skull_to_40_skull(
	enum firefight_skull_bit bit)
{
	switch (bit)
	{
	case _firefight_skull_iron_bit:                 return _skull_iron;
	case _firefight_skull_black_eye_bit:            return _skull_black_eye;
	case _firefight_skull_tough_luck_bit:           return _skull_tough_luck;
	case _firefight_skull_catch_bit:                return _skull_catch;
	case _firefight_skull_cloud_bit:                return _skull_fog;
	case _firefight_skull_famine_bit:               return _skull_famine;
	case _firefight_skull_tilt_bit:                 return _skull_ghost;
	case _firefight_skull_mythic_bit:               return _skull_mythic;
	case _firefight_skull_thunderstorm_bit:         return _skull_thunderstorm;
	case _firefight_skull_blind_bit:                return _skull_blind;
	case _firefight_skull_boom_bit:                 return _skull_boom;
	case _firefight_skull_cowbell_bit:              return _skull_sputnik;
	case _firefight_skull_grunt_birthday_party_bit: return _skull_grunt_birthday_party;
	default:                                        return NONE;
	}
}

/* PLASMA FLINCH's share of a plasma hit's stun a player takes in Firefight
(units.c's unit_damage_aftermath; 1 out of Firefight: the game's own) */
real firefight_player_stun_scale(
	void)
{
	struct firefight_options const *opts;
	byte flinch;

	if (!firefight_active())
		return 1.0f;
	opts = firefight_options_get();
	flinch = opts ? opts->firefight_plasma_flinch : 0;
	if (!(flinch & FIREFIGHT_PLASMA_FLINCH_SET))
		return 0.0f;
	return 0.25f * (real)PIN(flinch & ~FIREFIGHT_PLASMA_FLINCH_SET, 0, 4);
}

/* (actor_combat.c) how often Firefight's enemies fire, of what their tags
have: their rate and their bursts this much, the pauses between bursts by
its inverse. Firefight's waves see the players across the map and charge
them (firefight_spawner.c's pursuit), so as the campaign's they fired too
often; 1 for anyone else's, and outside Firefight */
/* (the host's setting game.firefight_enemy_fire_rate, percent) */
long config_integer(const char *name);
real firefight_enemy_fire_pacing(
	short team_index)
{
	if (!firefight_active() || team_index == NONE || !firefight_team_is_hostile(team_index))
		return 1.0f;
	return (real)PIN(config_integer("game.firefight_enemy_fire_rate"), 10, 100) / 100.0f;
}

/* (actor_combat.c) Firefight's enemies' aim error, of their tags'
(game.firefight_enemy_aim_error, percent); 1 for anyone else's */
real firefight_enemy_aim_error_scale(
	short team_index)
{
	if (!firefight_active() || team_index == NONE || !firefight_team_is_hostile(team_index))
		return 1.0f;
	return (real)PIN(config_integer("game.firefight_enemy_aim_error"), 100, 300) / 100.0f;
}

boolean firefight_active(
	void)
{
	return game_engine != NULL &&
		global_variant.game_engine_index == game_engine_firefight;
}

struct firefight_globals const *firefight_globals_get(
	void)
{
	return &ff_state;
}

struct firefight_globals *firefight_globals_mutable(
	void)
{
	return &ff_state;
}

unsigned long firefight_active_skulls_get(
	void)
{
	return ff_state.active_skulls;
}

boolean firefight_skull_active(
	enum firefight_skull_bit skull)
{
	if (!firefight_active() || skull < 0 || skull >= NUMBER_OF_FIREFIGHT_SKULLS)
		return FALSE;

	return (ff_state.active_skulls & (1UL << skull)) != 0;
}

boolean firefight_40_skull_active(
	int skull_enum_index)
{
	struct firefight_options const *opts;
	unsigned long started[2];
	short bit;

	if (!firefight_active())
		return FALSE;

	opts = firefight_options_get();
	if (opts && skull_enum_index >= 0 && skull_enum_index < NUMBER_OF_SKULLS)
	{
		firefight_starting_skulls_get(&global_variant.game_engine_variant.firefight, opts, started);
		if (started[skull_enum_index >> 5] & (1UL << (skull_enum_index & 31)))
			return TRUE;
	}

	for (bit = 0; bit < NUMBER_OF_FIREFIGHT_SKULLS; bit++)
	{
		if (firefight_map_reach_skull_to_40_skull((enum firefight_skull_bit)bit) == skull_enum_index &&
			(ff_state.active_skulls & (1UL << bit)) != 0)
		{
			return TRUE;
		}
	}
	return FALSE;
}

boolean firefight_40_skull_temporary(
	int skull_enum_index)
{
	short bit;

	if (!firefight_active())
		return FALSE;
	for (bit = 0; bit < NUMBER_OF_FIREFIGHT_SKULLS; bit++)
	{
		if (firefight_map_reach_skull_to_40_skull((enum firefight_skull_bit)bit) == skull_enum_index &&
			(ff_state.temporary_skulls & (1UL << bit)) != 0)
		{
			return TRUE;
		}
	}
	return FALSE;
}

void firefight_starting_skulls_get(
	struct game_engine_firefight_variant const *variant,
	struct firefight_options const *options,
	unsigned long skulls[2])
{
	unsigned long reach = variant ? variant->custom_skulls : 0UL;
	short bit;

	skulls[0] = skulls[1] = 0UL;
	if (options)
	{
		skulls[0] = options->firefight_starting_skulls[0];
		skulls[1] = options->firefight_starting_skulls[1];
		reach |= options->firefight_custom_skulls;
		/* (the old presets kept four skulls past the word's in bits of other
		skulls: 1 Scarab, 2 Sputnik, 8 Streaking, 9 Swarm) */
		if (!(skulls[1] & FIREFIGHT_STARTING_SKULLS_EXACT))
		{
			static struct { short from, skull; } const moved[] = {
				{ 1, _skull_scarab }, { 2, _skull_sputnik }, { 8, _skull_streaking }, { 9, _skull_swarm } };
			short index;

			for (index = 0; index < (short)NUMBEROF(moved); index++)
			{
				if (skulls[0] & (1UL << moved[index].from))
				{
					skulls[0] &= ~(1UL << moved[index].from);
					skulls[moved[index].skull >> 5] |= 1UL << (moved[index].skull & 31);
				}
			}
		}
	}
	for (bit = 0; bit < NUMBER_OF_FIREFIGHT_SKULLS; bit++)
	{
		if (reach & (1UL << bit))
		{
			int skull = firefight_map_reach_skull_to_40_skull((enum firefight_skull_bit)bit);

			if (skull >= 0)
				skulls[skull >> 5] |= 1UL << (skull & 31);
		}
	}
	skulls[1] &= ~FIREFIGHT_STARTING_SKULLS_EXACT;
}

void firefight_starting_skulls_set(
	struct game_engine_firefight_variant *variant,
	struct firefight_options *options,
	unsigned long const skulls[2])
{
	variant->custom_skulls = 0UL;
	options->firefight_custom_skulls = 0UL;
	options->firefight_starting_skulls[0] = skulls[0];
	options->firefight_starting_skulls[1] = skulls[1] | FIREFIGHT_STARTING_SKULLS_EXACT;
}

unsigned long firefight_tag_profile_hash(
	char const *folder)
{
	unsigned long hash = 2166136261UL;

	if (!folder || !folder[0])
		return 0UL;
	for (; *folder; folder++)
		hash = (hash ^ (unsigned char)*folder) * 16777619UL;
	return (hash & 0xFFFFFFFFUL) | FIREFIGHT_TAG_PROFILE_MARK;
}

/* ---------- multipliers & escalation */

static real firefight_compute_set_vitality_scale(
	short set_number)
{
	struct firefight_options const *opts = firefight_options_get();
	short step = MAX((short)(set_number - 1), 0);
	real per_set_bonus = 0.25f;

	if (opts)
	{
		switch (opts->firefight_set_escalation)
		{
		case _firefight_escalation_off:     per_set_bonus = 0.00f; break;
		case _firefight_escalation_low:     per_set_bonus = 0.15f; break;
		case _firefight_escalation_medium:  per_set_bonus = 0.25f; break;
		case _firefight_escalation_high:    per_set_bonus = 0.40f; break;
		case _firefight_escalation_extreme: per_set_bonus = 0.60f; break;
		default:                            per_set_bonus = 0.25f; break;
		}
	}

	return 1.0f + (real)step * per_set_bonus;
}

/* the Firefight skulls on for the whole game: the variant's starting skulls
and those turned on for the level (the Skulls screen's) */
static unsigned long firefight_started_skulls(
	void)
{
	struct firefight_options const *opts = firefight_options_get();
	unsigned long skulls[2];
	unsigned long started = 0UL;
	short bit;

	firefight_starting_skulls_get(&global_variant.game_engine_variant.firefight, opts, skulls);
	for (bit = 0; bit < NUMBER_OF_FIREFIGHT_SKULLS; bit++)
	{
		int skull = firefight_map_reach_skull_to_40_skull((enum firefight_skull_bit)bit);

		if (skull >= 0 && ((skulls[skull >> 5] & (1UL << (skull & 31))) || skull_on_for_level(skull)))
			started |= 1UL << bit;
	}
	return started;
}

static void firefight_recalculate_skulls_and_multiplier(
	void)
{
	struct game_engine_firefight_variant const *ff = &global_variant.game_engine_variant.firefight;
	unsigned long started = firefight_started_skulls();
	unsigned long combined;
	real mult = 1.0f;

	/* (a skull on from the start is on: the rounds and sets adding it again
	add nothing, and never make it one that goes) */
	ff_state.round_skulls &= ~started;
	ff_state.set_skulls &= ~started;
	combined = started | ff_state.round_skulls | ff_state.set_skulls;
	if (ff_state.in_bonus_round)
	{
		combined |= (1UL << _firefight_skull_grunt_birthday_party_bit) |
			(1UL << _firefight_skull_tough_luck_bit) |
			(1UL << _firefight_skull_catch_bit) |
			(1UL << _firefight_skull_cowbell_bit);
	}
	ff_state.active_skulls = combined;
	ff_state.temporary_skulls = combined & ~(started | ff_state.set_skulls);
	ff_state.set_vitality_scale = firefight_compute_set_vitality_scale(ff_state.current_set);

	switch (ff->difficulty)
	{
	case _firefight_difficulty_easy:      mult = 0.5f;  break;
	case _firefight_difficulty_normal:    mult = 1.0f;  break;
	case _firefight_difficulty_heroic:    mult = 2.0f;  break;
	case _firefight_difficulty_legendary: mult = 3.0f;  break;
	default:                              mult = 1.0f;  break;
	}
	mult *= campaign_score_skulls_multiplier();
	mult = MAX(0.0f, mult - bots_score_multiplier_penalty());
	if (ff_state.current_set > 1)
		mult *= (1.0f + 0.15f * (real)(ff_state.current_set - 1));

	ff_state.active_multiplier = mult;
}

real firefight_score_multiplier_get(
	void)
{
	return ff_state.active_multiplier > 0.0f ? ff_state.active_multiplier : 1.0f;
}

real firefight_set_vitality_scale_get(
	void)
{
	return ff_state.set_vitality_scale > 0.0f ? ff_state.set_vitality_scale : 1.0f;
}

boolean firefight_team_is_hostile(
	short team_index)
{
	if (team_index == _game_team_default || team_index == _game_team_player)
		return FALSE;
	if (team_index == _game_team_human)
		return ff_state.elite_player_mode ||
			global_variant.game_engine_variant.firefight.wave_faction == _firefight_faction_unsc_humans;
	return TRUE;
}

real firefight_enemy_vitality_scale(
	short team_index,
	boolean is_shield)
{
	static real const scales[] = { 1.0f, 0.5f, 1.5f, 2.0f, 3.0f };
	struct firefight_options const *opts = firefight_options_get();
	byte chosen = !opts ? 0 : is_shield ? opts->firefight_enemy_shield_scale : opts->firefight_enemy_health_scale;
	real scale = scales[MIN(chosen, (byte)(NUMBEROF(scales) - 1))];

	if (!firefight_active())
		return 1.0f;
	if (!firefight_team_is_hostile(team_index))
		return 1.0f;
	if (ff_state.elite_player_mode && team_index == _game_team_covenant &&
		global_variant.game_engine_variant.firefight.wave_faction == _firefight_faction_unsc_humans)
	{
		return 1.0f;
	}

	return firefight_set_vitality_scale_get() * scale;
}

void firefight_apply_unit_vitality_scaling(
	long unit_index)
{
	struct unit_datum *unit;

	if (!firefight_active() || unit_index == NONE)
		return;
	unit = (struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);
	if (!unit || unit->unit.player_index != NONE)
		return;
	if (firefight_skull_active(_firefight_skull_tilt_bit) && unit->object.maximum_shield_vitality <= 0.0f)
	{
		unit->object.maximum_shield_vitality = MAX(unit->object.maximum_body_vitality * 0.65f, 45.0f);
		unit->object.shield_vitality = 1.0f;
	}
}

/* ---------- banners & time formatting */

void firefight_announce_banner(
	enum firefight_banner_type type,
	short data,
	short duration_ticks)
{
	ff_state.banner_type = (short)type;
	ff_state.banner_data = data;
	ff_state.banner_timer_ticks = duration_ticks;
	ff_state.banner_total_ticks = duration_ticks;
}

long firefight_session_elapsed_seconds(
	void)
{
	return MAX(ff_state.session_elapsed_ticks / TICKS_PER_SECOND, 0L);
}

void firefight_format_session_time(
	wchar_t *out_buf,
	long count)
{
	long seconds = firefight_session_elapsed_seconds();
	long hours = seconds / 3600;
	long mins = (seconds / 60) % 60;
	long secs = seconds % 60;

	if (!out_buf || count <= 0)
		return;
	if (hours > 0)
		usnprintf(out_buf, count, L"%ld:%02ld:%02ld", hours, mins, secs);
	else
		usnprintf(out_buf, count, L"%02ld:%02ld", mins, secs);
}

/* ---------- faction allegiances & Elite mode */

boolean firefight_elite_mode_active(
	void)
{
	return firefight_active() && ff_state.elite_player_mode;
}

void firefight_setup_faction_allegiances(
	void)
{
	struct game_engine_firefight_variant const *ff = &global_variant.game_engine_variant.firefight;
	struct firefight_options const *opts = firefight_options_get();
	boolean allied_enemies = !opts ||
		(opts->firefight_faction_diplomacy == _firefight_diplomacy_allied_against_players);

	if (!firefight_active())
		return;

	/* Playable Elite mode is disabled (no first-person Elite arms / lifepod exit bugs) */
	ff_state.elite_player_mode = FALSE;

	/* Players have player->team_index == 0 (_game_team_default) while player/bot bipeds have
	 * unit->object.owner_team_index == 1 (_game_team_player). Ally _game_team_default with
	 * _game_team_player so aim_assist and motion_sensor treat bots and co-op players as friendlies. */
	game_allegiance_create(_game_team_default, FALSE, _game_team_player, FALSE, 0, 0, FALSE);

	if (ff->wave_faction == _firefight_faction_unsc_humans)
	{
		game_allegiance_remove(_game_team_player, _game_team_human);
		game_allegiance_remove(_game_team_player, _game_team_covenant);
		game_allegiance_remove(_game_team_default, _game_team_human);
		game_allegiance_remove(_game_team_default, _game_team_covenant);
	}
	else
	{
		/* Players are UNSC Spartans allied with UNSC Marines */
		game_allegiance_create(_game_team_player, FALSE, _game_team_human, FALSE, 0, 0, FALSE);
		game_allegiance_remove(_game_team_player, _game_team_covenant);
		game_allegiance_create(_game_team_default, FALSE, _game_team_human, FALSE, 0, 0, FALSE);
		game_allegiance_remove(_game_team_default, _game_team_covenant);
	}
	game_allegiance_remove(_game_team_player, _game_team_flood);
	game_allegiance_remove(_game_team_player, _game_team_sentinel);
	game_allegiance_remove(_game_team_default, _game_team_flood);
	game_allegiance_remove(_game_team_default, _game_team_sentinel);

	if (allied_enemies)
	{
		/* Covenant, Flood, and Sentinels unite against the players */
		game_allegiance_create(_game_team_covenant, FALSE, _game_team_flood, FALSE, 0, 0, FALSE);
		game_allegiance_create(_game_team_covenant, FALSE, _game_team_sentinel, FALSE, 0, 0, FALSE);
		game_allegiance_create(_game_team_flood, FALSE, _game_team_sentinel, FALSE, 0, 0, FALSE);
	}
	else
	{
		/* Classic 3-way faction infighting */
		game_allegiance_remove(_game_team_covenant, _game_team_flood);
		game_allegiance_remove(_game_team_covenant, _game_team_sentinel);
		game_allegiance_remove(_game_team_flood, _game_team_sentinel);
	}
}

long firefight_resolve_player_biped_tag(
	long player_index,
	long default_biped_tag)
{
	(void)player_index;
	return default_biped_tag;
}

static void firefight_give_player_weapon(
	long unit_index,
	char const *weapon_tag_path,
	long add_mode)
{
	long weap_def;
	long weap_obj;
	struct object_placement_data placement;

	if (unit_index == NONE || !weapon_tag_path)
		return;
	weap_def = tag_loaded(WEAPON_DEFINITION_TAG, weapon_tag_path);
	/* (a flag or a ball, whatever its name: none in a loadout) */
	if (weap_def == NONE || firefight_weapon_is_objective(weap_def, weapon_tag_path))
		return;
	object_placement_data_new(&placement, weap_def, NONE);
	object_get_origin(unit_index, &placement.position);
	weap_obj = object_new(&placement);
	if (weap_obj != NONE)
	{
		if (!unit_add_weapon_to_inventory(unit_index, weap_obj, add_mode))
			object_delete(weap_obj);
	}
}

static char const *const s_ff_loadout_weapon_paths[] =
{
	"weapons\\assault rifle\\assault rifle",
	"weapons\\pistol\\pistol",
	"weapons\\shotgun\\shotgun",
	"weapons\\sniper rifle\\sniper rifle",
	"weapons\\rocket launcher\\rocket launcher",
	"weapons\\plasma pistol\\plasma pistol",
	"weapons\\plasma rifle\\plasma rifle",
	"weapons\\needler\\mp_needler",
	"weapons\\needler\\needler"
};

/* a loadout's weapon's tag; a random one is any weapon of the list the map
has, other than `other` (the loadout's other weapon) */
static char const *firefight_resolve_loadout_weapon_path(byte weapon_choice, short salt, char const *other)
{
	switch (weapon_choice)
	{
	case _loadout_weapon_none:
		return NULL;
	case _loadout_weapon_random:
	{
		static unsigned long s_fiesta_rng = 0x9E3779B9UL;
		char const *pool[NUMBEROF(s_ff_loadout_weapon_paths)];
		char const *map_weapon = firefight_map_loadout_weapon(other);
		short count = 0;
		short index;

		/* (MAP CONTENT's weapons: ONLY MAP's in the game's place) */
		if (map_weapon)
			return map_weapon;
		for (index = 0; index < (short)NUMBEROF(s_ff_loadout_weapon_paths); index++)
		{
			long tag = tag_loaded(WEAPON_DEFINITION_TAG, s_ff_loadout_weapon_paths[index]);

			if (tag != NONE && !firefight_weapon_is_objective(tag, s_ff_loadout_weapon_paths[index]) &&
				(!other || strcmp(other, s_ff_loadout_weapon_paths[index]) != 0))
			{
				pool[count++] = s_ff_loadout_weapon_paths[index];
			}
		}
		if (!count)
			return s_ff_loadout_weapon_paths[0];
		s_fiesta_rng = s_fiesta_rng * 1664525UL + 1013904223UL + (unsigned long)game_time_get() + (unsigned long)salt * 97UL;
		return pool[(s_fiesta_rng >> 8) % (unsigned long)count];
	}
	case _loadout_weapon_assault_rifle:   return s_ff_loadout_weapon_paths[0];
	case _loadout_weapon_pistol:          return s_ff_loadout_weapon_paths[1];
	case _loadout_weapon_shotgun:         return s_ff_loadout_weapon_paths[2];
	case _loadout_weapon_sniper_rifle:    return s_ff_loadout_weapon_paths[3];
	case _loadout_weapon_rocket_launcher: return s_ff_loadout_weapon_paths[4];
	case _loadout_weapon_plasma_pistol:   return s_ff_loadout_weapon_paths[5];
	case _loadout_weapon_plasma_rifle:    return s_ff_loadout_weapon_paths[6];
	case _loadout_weapon_needler:
		return (tag_loaded(WEAPON_DEFINITION_TAG, s_ff_loadout_weapon_paths[7]) != NONE)
			? s_ff_loadout_weapon_paths[7]
			: s_ff_loadout_weapon_paths[8];
	default:
		return s_ff_loadout_weapon_paths[0];
	}
}

void firefight_apply_player_spawn_traits(
	long player_index,
	long unit_index)
{
	struct firefight_options const *opts = firefight_options_get();
	struct player_datum *player;
	struct unit_datum *unit;
	real speed_mult = 1.0f;
	real shield_mult = 1.0f;
	char const *pri_weap = "weapons\\assault rifle\\assault rifle";
	char const *sec_weap = "weapons\\pistol\\pistol";
	short frag_grens = 4;
	short plasma_grens = 2;
	byte preset = opts ? opts->firefight_player_faction_mode : _firefight_loadout_standard;
	boolean rocketfight;

	if (!firefight_active() || player_index == NONE || unit_index == NONE)
		return;
	player = player_try_and_get(player_index);
	unit = (struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);
	if (!player || !unit)
		return;

	if (opts)
	{
		switch (opts->firefight_player_speed)
		{
		case _firefight_speed_75:  speed_mult = 0.75f; break;
		case _firefight_speed_125: speed_mult = 1.25f; break;
		case _firefight_speed_150: speed_mult = 1.50f; break;
		default:                   speed_mult = 1.00f; break;
		}

		switch (opts->firefight_player_shields)
		{
		case _firefight_shields_none:       shield_mult = 0.0f; break;
		case _firefight_shields_overshield: shield_mult = 2.0f; break;
		case _firefight_shields_super:      shield_mult = 4.0f; break;
		default:                            shield_mult = 1.0f; break;
		}
	}
	player->speed_multiplier = speed_mult;
	/* (MAP CONTENT's SHIELDS, the host's for this map: OFF none at all; ON
	the stock player's on a map whose player has none, a shield given as
	Tilt's are, damage.c's object_shield_given) */
	switch (firefight_health_rule(_firefight_health_rule_shields))
	{
	case _firefight_health_rule_off:
		unit->object.maximum_shield_vitality = 0.0f;
		shield_mult = 0.0f;
		break;
	case _firefight_health_rule_on:
		if (unit->object.maximum_shield_vitality <= 0.0f)
		{
			unit->object.maximum_shield_vitality = FIREFIGHT_GIVEN_SHIELD_VITALITY;
			unit->object.shield_vitality = 1.0f;
		}
		break;
	}
	if (shield_mult <= 0.0f)
		unit->object.shield_vitality = 0.0f;
	else if (shield_mult > 1.0f)
		unit->object.shield_vitality = shield_mult;

	rocketfight = !_wcsicmp(global_variant.human_readable_game_description, L"Rocketfight") ||
		global_variant.game_engine_variant.firefight.ordnance_drops == _firefight_ordnance_frequent ||
		preset == _firefight_loadout_rocketfight;

	/* Resolve the rules' Loadout, MCC Arcade Firefight Loadout Preset, or Weapon Set */
	if (opts && opts->loadout == _loadout_custom)
	{
		pri_weap = firefight_resolve_loadout_weapon_path(opts->primary_weapon, 1, NULL);
		sec_weap = firefight_resolve_loadout_weapon_path(opts->secondary_weapon, 3, pri_weap);
		if (rocketfight)
			plasma_grens = 4;
	}
	else if (rocketfight)
	{
		pri_weap = "weapons\\rocket launcher\\rocket launcher";
		sec_weap = "weapons\\pistol\\pistol";
		frag_grens = 4;
		plasma_grens = 4;
	}
	else if (!_wcsicmp(global_variant.human_readable_game_description, L"Sniperfight") ||
		preset == _firefight_loadout_sniperfight)
	{
		pri_weap = "weapons\\sniper rifle\\sniper rifle";
		sec_weap = "weapons\\pistol\\pistol";
	}
	else if (!_wcsicmp(global_variant.human_readable_game_description, L"Marksman") ||
		preset == _firefight_loadout_marksman)
	{
		pri_weap = "weapons\\pistol\\pistol";
		sec_weap = "weapons\\shotgun\\shotgun";
	}
	else if (!_wcsicmp(global_variant.human_readable_game_description, L"Plasmafight") ||
		preset == _firefight_loadout_plasmafight)
	{
		pri_weap = "weapons\\plasma rifle\\plasma rifle";
		sec_weap = "weapons\\plasma pistol\\plasma pistol";
		frag_grens = 2;
		plasma_grens = 4;
	}
	else
	{
		switch (global_variant.universal_variant.weapon_set)
		{
		case 1: /* Pistols */
			pri_weap = "weapons\\pistol\\pistol";
			sec_weap = "weapons\\plasma pistol\\plasma pistol";
			break;
		case 2: /* Rifles */
			pri_weap = "weapons\\assault rifle\\assault rifle";
			sec_weap = "weapons\\sniper rifle\\sniper rifle";
			break;
		case 3: /* Plasma */
			pri_weap = "weapons\\plasma rifle\\plasma rifle";
			sec_weap = "weapons\\plasma pistol\\plasma pistol";
			break;
		case 4: /* Sniper */
			pri_weap = "weapons\\sniper rifle\\sniper rifle";
			sec_weap = "weapons\\pistol\\pistol";
			break;
		case 5: /* No Sniping */
			pri_weap = "weapons\\assault rifle\\assault rifle";
			sec_weap = "weapons\\shotgun\\shotgun";
			break;
		case 6: /* Rocket Launchers */
		case 9: /* Heavy Weapons */
			pri_weap = "weapons\\rocket launcher\\rocket launcher";
			sec_weap = "weapons\\pistol\\pistol";
			break;
		case 7: /* Shotguns */
			pri_weap = "weapons\\shotgun\\shotgun";
			sec_weap = "weapons\\pistol\\pistol";
			break;
		default:
			break;
		}
	}

	/* (MAP CONTENT's PLAYER LOADOUT, the host's for this map: the map's
	weapons in the slots it sets; a slot not set keeps the game's) */
	{
		char const *map_primary = firefight_map_loadout_override(0);
		char const *map_secondary = firefight_map_loadout_override(1);

		if (map_primary)
			pri_weap = map_primary;
		if (map_secondary)
			sec_weap = map_secondary;
	}

	unit_delete_all_weapons(unit_index);
	/* (the primary in hand, the secondary holstered: game_engine_give_loadout) */
	if (pri_weap)
		firefight_give_player_weapon(unit_index, pri_weap, _unit_add_weapon_replace);
	if (sec_weap && (!pri_weap || strcmp(pri_weap, sec_weap) != 0))
		firefight_give_player_weapon(unit_index, sec_weap,
			pri_weap ? _unit_add_weapon_normal : _unit_add_weapon_replace);
	unit->unit.grenade_counts[_unit_grenade_human_fragmentation] = (char)frag_grens;
	unit->unit.grenade_counts[_unit_grenade_covenant_plasma] = (char)plasma_grens;
}

void firefight_postspawn_player_update(
	long player_index)
{
	struct player_datum *player;

	if (!firefight_active() || player_index == NONE)
		return;
	player = player_try_and_get(player_index);
	if (!player || player->unit_index == NONE)
		return;
	firefight_apply_player_spawn_traits(player_index, player->unit_index);
}

/* ---------- lives & progression helpers */

static void firefight_add_shared_lives(
	short bonus_lives)
{
	if (bonus_lives <= 0 || ff_state.shared_lives_remaining < 0)
		return;
	ff_state.shared_lives_remaining += bonus_lives;
	if (ff_state.max_lives_cap > 0 && ff_state.shared_lives_remaining > ff_state.max_lives_cap)
		ff_state.shared_lives_remaining = ff_state.max_lives_cap;
	ff_state.last_man_standing_announced = FALSE;
}

static short firefight_count_living_players(
	short *out_total_players)
{
	struct data_iterator iter;
	struct player_datum *player;
	short alive = 0;
	short total = 0;

	if (!player_data)
	{
		if (out_total_players)
			*out_total_players = 0;
		return 0;
	}

	data_iterator_new(&iter, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iter)) != NULL)
	{
		if (player->quit_out_of_game)
			continue;
		total++;
		if (player->unit_index != NONE)
		{
			struct unit_datum *u = (struct unit_datum *)object_try_and_get_and_verify_type(
				player->unit_index, _object_mask_unit);

			if (u && !TEST_FLAG(u->object.damage_flags, _object_dead_bit))
				alive++;
		}
	}
	if (out_total_players)
		*out_total_players = total;
	return alive;
}

boolean firefight_player_out_of_lives(
	long player_index)
{
	(void)player_index;
	if (!firefight_active())
		return FALSE;
	if (ff_state.match_lost || ff_state.match_won)
		return TRUE;
	return ff_state.shared_lives_remaining == 0;
}

boolean firefight_player_is_out_of_lives(
	long player_index)
{
	return firefight_player_out_of_lives(player_index);
}

long firefight_compute_respawn_timer(
	long dead_player_index)
{
	long base_ticks = global_variant.universal_variant.respawn_time;

	(void)dead_player_index;
	if (base_ticks <= 0)
		base_ticks = 5 * TICKS_PER_SECOND;
	if (ff_state.current_wave == FIREFIGHT_WAVES_PER_ROUND)
		base_ticks += 2 * TICKS_PER_SECOND;
	return MAX(base_ticks, 3 * TICKS_PER_SECOND);
}

long firefight_player_kills(
	long player_index)
{
	short abs_idx;

	if (player_index == NONE)
		return 0;
	abs_idx = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
	if (abs_idx >= 0 && abs_idx < HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
		return ff_state.player_stats[abs_idx].kills;
	return 0;
}

long firefight_player_assists(
	long player_index)
{
	short abs_idx;

	if (player_index == NONE)
		return 0;
	abs_idx = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
	if (abs_idx >= 0 && abs_idx < HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
		return ff_state.player_stats[abs_idx].assists;
	return 0;
}

boolean firefight_spectating_active(
	void)
{
	short total = 0;
	short alive = firefight_count_living_players(&total);

	return firefight_active() && ff_state.shared_lives_remaining == 0 && alive > 0 && total > 1;
}

boolean firefight_should_end_game(
	void)
{
	return firefight_active() && (ff_state.match_won || ff_state.match_lost) && ff_state.phase_timer_ticks <= 0;
}

static long firefight_prep_countdown_ticks(
	short round_num)
{
	struct firefight_options const *opts = firefight_options_get();
	/* (the players' vehicles there from the start, ALL AT SPAWN's: the first
	round's countdown is the one they're found in) */
	boolean has_vehicles = round_num == 1 && firefight_player_vehicles_at_start();
	byte mode = opts ? opts->firefight_prep_countdown : _firefight_prep_15s;

	/* Round 1 Wave 1 starts briskly unless vehicles are deployed right away */
	if (round_num == 1 && ff_state.current_set == 1 && mode == _firefight_prep_30s && !has_vehicles)
		return 5 * TICKS_PER_SECOND;

	switch (mode)
	{
	case _firefight_prep_5s:  return 5 * TICKS_PER_SECOND;
	case _firefight_prep_15s: return 15 * TICKS_PER_SECOND;
	case _firefight_prep_30s: return (has_vehicles ? 20 : 8) * TICKS_PER_SECOND;
	case _firefight_prep_45s: return 45 * TICKS_PER_SECOND;
	case _firefight_prep_60s: return 60 * TICKS_PER_SECOND;
	default:                  return 15 * TICKS_PER_SECOND;
	}
}

static long firefight_wave_timer_limit_ticks(
	void)
{
	struct firefight_options const *opts = firefight_options_get();

	if (!opts)
		return 0;
	switch (opts->firefight_next_wave_mode)
	{
	case _firefight_next_wave_timer_30s:  return 30 * TICKS_PER_SECOND;
	case _firefight_next_wave_timer_45s:  return 45 * TICKS_PER_SECOND;
	case _firefight_next_wave_timer_60s:  return 60 * TICKS_PER_SECOND;
	case _firefight_next_wave_timer_90s:  return 90 * TICKS_PER_SECOND;
	case _firefight_next_wave_timer_120s: return 120 * TICKS_PER_SECOND;
	default:                              return 0;
	}
}

static void firefight_update_round_and_set_skulls(
	void)
{
	struct game_engine_firefight_variant const *ff = &global_variant.game_engine_variant.firefight;
	struct firefight_options const *opts = firefight_options_get();
	byte prog_mode = opts ? opts->firefight_skull_progression_mode : _firefight_skull_prog_standard;
	unsigned long newly_added = 0;
	unsigned long before = ff_state.active_skulls;

	if (!ff->progression_skulls || prog_mode == _firefight_skull_prog_off)
	{
		ff_state.round_skulls = 0;
		ff_state.set_skulls = 0;
		firefight_recalculate_skulls_and_multiplier();
		return;
	}

	if (prog_mode == _firefight_skull_prog_random_round || prog_mode == _firefight_skull_prog_custom_pool)
	{
		unsigned long pool = (prog_mode == _firefight_skull_prog_custom_pool && opts && opts->firefight_custom_skulls) ?
			opts->firefight_custom_skulls :
			((1UL << NUMBER_OF_FIREFIGHT_SKULLS) - 1UL) & ~(1UL << _firefight_skull_blind_bit);
		short candidates[NUMBER_OF_FIREFIGHT_SKULLS];
		short count = 0;
		short bit;

		/* (never one already on from the start: a round's is a new one) */
		pool &= ~firefight_started_skulls();
		for (bit = 0; bit < NUMBER_OF_FIREFIGHT_SKULLS; bit++)
		{
			if ((pool & (1UL << bit)) != 0)
				candidates[count++] = bit;
		}
		ff_state.round_skulls = 0;
		if (count > 0)
		{
			short pick = candidates[(ff_state.current_set * 7 + ff_state.current_round * 3) % count];
			ff_state.round_skulls |= (1UL << pick);
		}
	}
	else
	{
		/* Standard Halo: Reach Round Skull progression (or per-round override) */
		ff_state.round_skulls = 0;
		if (ff_state.current_round == 1)
		{
			if (ff_state.current_set > 1)
				ff_state.round_skulls |= (1UL << _firefight_skull_tough_luck_bit);
		}
		else if (ff_state.current_round == 2)
		{
			ff_state.round_skulls |= (1UL << _firefight_skull_catch_bit);
		}
		else if (ff_state.current_round >= 3)
		{
			if (ff_state.current_round == 3)
				ff_state.round_skulls |= (1UL << _firefight_skull_black_eye_bit);
			else if (ff_state.current_round == 4)
				ff_state.round_skulls |= (1UL << _firefight_skull_tough_luck_bit);
			else
				ff_state.round_skulls |= (1UL << _firefight_skull_thunderstorm_bit);
		}

		/* Set Skulls lock on permanently starting Set 2 */
		if (ff_state.current_set >= 2)
			ff_state.set_skulls |= (1UL << _firefight_skull_tilt_bit);
		if (ff_state.current_set >= 3)
			ff_state.set_skulls |= (1UL << _firefight_skull_famine_bit);
		if (ff_state.current_set >= 4)
			ff_state.set_skulls |= (1UL << _firefight_skull_mythic_bit);
		if (ff_state.current_set >= 5)
			ff_state.set_skulls |= (1UL << _firefight_skull_iron_bit);
	}

	firefight_recalculate_skulls_and_multiplier();
	newly_added = ff_state.active_skulls & ~before;
	if (newly_added != 0)
	{
		short bit;

		for (bit = 0; bit < NUMBER_OF_FIREFIGHT_SKULLS; bit++)
		{
			if (newly_added & (1UL << bit))
			{
				firefight_announce_banner(_ff_banner_skull_activated, bit, 4 * TICKS_PER_SECOND);
				break;
			}
		}
	}
}

static void firefight_begin_pre_round(
	short round_num)
{
	struct game_engine_firefight_variant const *ff = &global_variant.game_engine_variant.firefight;
	struct firefight_options const *opts = firefight_options_get();

	ff_state.current_round = round_num;
	ff_state.current_wave = 1;
	ff_state.in_bonus_round = FALSE;
	ff_state.phase_state = _ff_phase_pre_round;
	ff_state.phase_timer_ticks = firefight_prep_countdown_ticks(round_num);
	firefight_update_round_and_set_skulls();

	/* The players' vehicles made again where any went (they're there from the
	start, and stay the whole game), the round's ordnance, and friendly AI */
	if (!network_game_distributed_client())
	{
		firefight_spawn_player_vehicles();
		if (ff->ordnance_drops == _firefight_ordnance_per_round ||
			ff->ordnance_drops == _firefight_ordnance_frequent ||
			(ff->ordnance_drops == _firefight_ordnance_per_set && round_num == 1))
		{
			firefight_spawn_ordnance_drop(round_num >= 4);
		}
		if (opts)
		{
			if (opts->firefight_friendly_ai == _firefight_friendly_ai_round_duo)
				firefight_spawn_friendly_reinforcements(2);
			else if (opts->firefight_friendly_ai == _firefight_friendly_ai_set_squad && round_num == 1)
				firefight_spawn_friendly_reinforcements(4);
		}
	}

	firefight_announce_banner(
		round_num == 1 ? _ff_banner_set_start : _ff_banner_round_start,
		round_num == 1 ? ff_state.current_set : round_num,
		4 * TICKS_PER_SECOND);
	/* (the round's objective: firefight_objectives.c) */
	firefight_objectives_round_starting(round_num);
}

static void firefight_begin_wave(
	short wave_num)
{
	boolean is_boss_wave = firefight_wave_will_have_heavy_or_boss(
		ff_state.current_set, ff_state.current_round, wave_num);

	ff_state.current_wave = wave_num;
	ff_state.wave_elapsed_ticks = 0;
	ff_state.straggler_navpoints_active = FALSE;
	ff_state.phase_state = _ff_phase_wave_spawning;
	ff_state.phase_timer_ticks = 2 * TICKS_PER_SECOND;
	ff_state.next_wave_timer_ticks = firefight_wave_timer_limit_ticks();

	/* Frequent ordnance mode (e.g. Rocketfight) also drops resupply before the final wave of a round */
	if (!network_game_distributed_client() && is_boss_wave)
	{
		if (global_variant.game_engine_variant.firefight.ordnance_drops == _firefight_ordnance_frequent)
			firefight_spawn_ordnance_drop(TRUE);
	}

	firefight_announce_banner(
		wave_num == FIREFIGHT_WAVES_PER_ROUND ? _ff_banner_boss_wave : _ff_banner_wave_start,
		wave_num,
		3 * TICKS_PER_SECOND);
	/* (the wave's objective, and the round's at its wave: firefight_objectives.c) */
	firefight_objectives_wave_starting(wave_num);
}

/* ---------- kill & melee notifications */

void firefight_notify_actor_or_unit_killed(
	long actor_index,
	long dead_unit_index,
	long killer_player_index,
	boolean headshot,
	boolean melee,
	boolean explosion,
	boolean splatter)
{
	short slot;
	boolean matched_tracked = FALSE;
	byte tier = 0;

	(void)actor_index;
	(void)splatter;
	if (!firefight_active() || dead_unit_index == NONE)
		return;

	for (slot = 0; slot < FIREFIGHT_MAX_TRACKED_ENEMIES; slot++)
	{
		if (ff_state.tracked_enemy_units[slot] == dead_unit_index)
		{
			tier = ff_state.tracked_enemy_tiers[slot];
			matched_tracked = TRUE;

			ff_state.tracked_enemy_units[slot] = NONE;
			ff_state.tracked_enemy_tiers[slot] = 0;
			if (ff_state.enemies_alive > 0)
				ff_state.enemies_alive--;
			break;
		}
	}

	if (!matched_tracked)
	{
		struct unit_datum *u = unit_try_and_get(dead_unit_index);
		if (!u || u->unit.player_index != NONE || u->object.owner_team_index == _game_team_player)
			matched_tracked = FALSE;
		else
			matched_tracked = TRUE;
	}

	if (matched_tracked && killer_player_index != NONE)
	{
		short abs_idx = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(killer_player_index);

		if (abs_idx >= 0 && abs_idx < HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
		{
			ff_state.player_stats[abs_idx].kills++;
			if (tier == _ff_tier_boss_heavy)
				ff_state.player_stats[abs_idx].boss_kills++;
		}
		firefight_stats_notify_enemy_kill(
			killer_player_index,
			_ff_challenge_cat_firefight,
			headshot,
			melee,
			explosion,
			(tier == _ff_tier_boss_heavy),
			100);
	}

	firefight_prune_dead_tracked_enemies();
	firefight_objectives_enemy_killed(actor_index, dead_unit_index, killer_player_index, headshot, melee, explosion,
		splatter);
	ff_state.team_score = campaign_score_team_points();
	while (ff_state.next_bonus_life_score > 0 && ff_state.team_score >= ff_state.next_bonus_life_score)
	{
		struct firefight_options const *opts = firefight_options_get();
		long step = (opts && opts->firefight_score_life_threshold > 0) ?
			opts->firefight_score_life_threshold : FIREFIGHT_DEFAULT_SCORE_LIFE_BONUS;

		ff_state.next_bonus_life_score += step;
		firefight_add_shared_lives(1);
		firefight_announce_banner(_ff_banner_bonus_life, 1, 3 * TICKS_PER_SECOND);
	}
}

void firefight_notify_player_melee_hit(
	long player_index,
	long target_object_index)
{
	if (!firefight_active() || player_index == NONE)
		return;
	firefight_try_trigger_generator_lockdown(player_index);
	(void)target_object_index;
}

/* ---------- built-in variant & options helpers */

boolean firefight_build_variant_by_name(
	char const *name,
	struct game_variant *out_variant)
{
	short i;

	if (!name || !out_variant)
		return FALSE;

	for (i = 0; i < NUMBER_OF_BUILT_IN_FIREFIGHT_PROFILES; i++)
	{
		struct firefight_builtin_profile const *p = &built_in_firefight_profiles[i];

		if (!_stricmp(name, p->variant_name))
		{
			struct game_variant result = { 0 };
			struct game_engine_firefight_variant *ff = &result.game_engine_variant.firefight;

			ustrncpy(result.human_readable_game_description, p->display_name, 11);
			result.human_readable_game_description[11] = 0;
			result.game_engine_index = game_engine_firefight;
			result.flags = 1;
			SET_FLAG(result.universal_variant.flags, _game_variant_draw_object_in_motion_sensor_bit, TRUE);
			SET_FLAG(result.universal_variant.flags, _game_variant_allow_friendly_navpoints_bit, TRUE);
			result.universal_variant.goal_radar = 0;
			result.universal_variant.teams = TRUE;
			result.universal_variant.health = 1.0f;
			result.universal_variant.respawn_time = 5 * TICKS_PER_SECOND;
			result.universal_variant.suicide_penalty = 5 * TICKS_PER_SECOND;
			result.universal_variant.weapon_set = 0;
			result.universal_variant.vehicle_set = 0;

			ff->set_limit = p->set_limit;
			ff->starting_lives = p->starting_lives;
			ff->bonus_lives_per_round = p->bonus_lives_per_round;
			ff->bonus_lives_per_set = p->bonus_lives_per_set;
			ff->max_lives_cap = p->max_lives_cap;
			ff->generator_count = p->generator_count;
			ff->custom_skulls = p->custom_skulls;
			ff->difficulty = p->difficulty;
			ff->wave_faction = p->wave_faction;
			ff->generator_fail_mode = p->generator_fail_mode;
			ff->ordnance_drops = p->ordnance_drops;
			ff->bonus_round_enabled = p->bonus_round_enabled;
			ff->progression_skulls = p->progression_skulls;
			ff->generator_attackers = p->generator_attackers;
			ff->vehicle_drops_enabled = p->vehicle_drops_enabled;
			/* (as game_engine_variant_cleanup leaves it: a variant the game built is not one it changes) */
			firefight_variant_cleanup(&result);

			*out_variant = result;
			return TRUE;
		}
	}
	return FALSE;
}

boolean firefight_get_variant_by_name(
	struct game_variant *out_variant,
	char const *name)
{
	struct game_variant built;

	if (!firefight_build_variant_by_name(name, &built))
		return FALSE;
	if (firefight_engine_refused())
		return FALSE;
	*out_variant = built;
	return TRUE;
}

struct game_variant *build_game_variant_firefight(
	struct game_variant *variant)
{
	firefight_build_variant_by_name("firefight", variant);
	return variant;
}

struct game_variant *build_game_variant_generator_defense(
	struct game_variant *variant)
{
	firefight_build_variant_by_name("generator_defense", variant);
	return variant;
}

struct game_variant *build_game_variant_gruntpocalypse(
	struct game_variant *variant)
{
	firefight_build_variant_by_name("gruntpocalypse", variant);
	return variant;
}

struct game_variant *build_game_variant_rocketfight(
	struct game_variant *variant)
{
	firefight_build_variant_by_name("rocketfight", variant);
	return variant;
}

struct game_variant *build_game_variant_floodfight(
	struct game_variant *variant)
{
	firefight_build_variant_by_name("floodfight", variant);
	return variant;
}

struct game_variant *build_game_variant_mythic_firefight(
	struct game_variant *variant)
{
	firefight_build_variant_by_name("mythic_firefight", variant);
	return variant;
}

struct game_variant *build_game_variant_fiesta(
	struct game_variant *variant)
{
	firefight_build_variant_by_name("fiesta", variant);
	return variant;
}

struct game_variant *build_game_variant_grunt_fiesta(
	struct game_variant *variant)
{
	firefight_build_variant_by_name("grunt_fiesta", variant);
	return variant;
}

void firefight_variant_cleanup(
	struct game_variant *variant)
{
	struct game_engine_firefight_variant *ff;

	if (!variant)
		return;
	ff = &variant->game_engine_variant.firefight;
	variant->universal_variant.teams = TRUE;
	SET_FLAG(variant->universal_variant.flags, _game_variant_allow_friendly_navpoints_bit, TRUE);
	if (ff->starting_lives == 0)
		ff->starting_lives = 7;
	if (ff->max_lives_cap < 0)
		ff->max_lives_cap = 15;
	if (ff->difficulty >= NUMBER_OF_FIREFIGHT_DIFFICULTIES)
		ff->difficulty = _firefight_difficulty_normal;
	if (ff->wave_faction >= NUMBER_OF_FIREFIGHT_FACTIONS)
		ff->wave_faction = _firefight_faction_covenant;
	if (ff->generator_count < 0 || ff->generator_count > FIREFIGHT_MAX_GENERATORS)
		ff->generator_count = 0;
	if (ff->generator_attackers >= NUMBER_OF_FIREFIGHT_GENERATOR_ATTACKER_MODES)
		ff->generator_attackers = _firefight_generator_attackers_50;
}

void firefight_variant_options_default(
	struct firefight_options *options)
{
	if (!options)
		return;
	options->friendly_fire = _friendly_fire_on;
	/* (as the rules' PRIMARY and SECONDARY WEAPON rows show it) */
	options->loadout = _loadout_custom;
	options->primary_weapon = _loadout_weapon_assault_rifle;
	options->secondary_weapon = _loadout_weapon_pistol;
	options->firefight_custom_skulls = 0UL;
	options->firefight_generator_health = 2000;
	options->firefight_score_life_threshold = FIREFIGHT_DEFAULT_SCORE_LIFE_BONUS;
	options->firefight_enemy_health_scale = 0;
	options->firefight_enemy_shield_scale = 0;
	options->firefight_bonus_seconds = FIREFIGHT_BONUS_ROUND_TICKS / TICKS_PER_SECOND;
	options->firefight_faction_diplomacy = _firefight_diplomacy_allied_against_players;
	options->firefight_tag_profile = 0UL;
	options->firefight_starting_skulls[0] = 0UL;
	options->firefight_starting_skulls[1] = 0UL;
	options->firefight_unit_toggles = FIREFIGHT_UNIT_MASK_ALL;
	options->firefight_set_escalation = _firefight_escalation_medium;
	options->firefight_dropship_mode = _firefight_dropship_auto;
	options->firefight_next_wave_mode = _firefight_next_wave_timer_60s;
	options->firefight_player_faction_mode = _firefight_player_faction_spartan;
	options->firefight_vehicle_spawns = _firefight_vehicle_spawns_all;
	options->firefight_plasma_flinch = FIREFIGHT_PLASMA_FLINCH_SET | 0;
	options->firefight_unused_vehicle = 0;
	options->firefight_prep_countdown = _firefight_prep_30s;
	options->firefight_skull_progression_mode = _firefight_skull_prog_standard;
	options->firefight_player_shields = _firefight_shields_normal;
	options->firefight_player_speed = _firefight_speed_100;
	options->firefight_player_gravity = _firefight_gravity_100;
	options->firefight_infinite_ammo = _firefight_ammo_normal;
	options->firefight_map_content = _firefight_map_content_vanilla;
	options->firefight_wave_size_scale = _firefight_wave_size_100;
	options->firefight_boss_wave_mode = _firefight_boss_wave_5_only;
	options->firefight_enemy_loadout = _firefight_enemy_loadout_default;
	options->firefight_friendly_ai = _firefight_friendly_ai_off;
}

/* a built-in variant's own options, over firefight_variant_options_default's:
its loadout, shown on the rules' PRIMARY and SECONDARY WEAPON rows, and its
Outbreak. A built-in is known by its name as the variant keeps it, cut to
fit (firefight_build_variant_by_name) */
void firefight_builtin_variant_options(
	struct game_variant const *variant,
	struct firefight_options *options)
{
	short profile;

	if (!variant || !options)
		return;
	for (profile = 0; profile < NUMBER_OF_BUILT_IN_FIREFIGHT_PROFILES; profile++)
	{
		struct firefight_builtin_profile const *p = &built_in_firefight_profiles[profile];
		short index = 0;

		while (index < 11 && p->display_name[index] &&
			p->display_name[index] == variant->human_readable_game_description[index])
		{
			index++;
		}
		if (index < 11 && p->display_name[index] != variant->human_readable_game_description[index])
			continue;
		if (p->primary_weapon || p->secondary_weapon)
		{
			options->loadout = _loadout_custom;
			options->primary_weapon = p->primary_weapon;
			options->secondary_weapon = p->secondary_weapon;
		}
		if (p->outbreak)
		{
			options->firefight_starting_skulls[_skull_outbreak >> 5] |= 1UL << (_skull_outbreak & 31);
			options->firefight_starting_skulls[1] |= FIREFIGHT_STARTING_SKULLS_EXACT;
		}
		if (p->wave_size)
			options->firefight_wave_size_scale = p->wave_size;
		if (p->enemy_loadout)
			options->firefight_enemy_loadout = p->enemy_loadout;
		return;
	}
}

/* ---------- engine vtable implementation */

static void firefight_dispose(
	void)
{
}

static boolean firefight_initialize_for_new_map(
	void)
{
	struct game_engine_firefight_variant const *ff = &global_variant.game_engine_variant.firefight;
	struct firefight_options const *opts = firefight_options_get();
	short i;

	csmemset(&ff_state, 0, sizeof(ff_state));
	firefight_drops_reset();
	if (opts && opts->firefight_unit_toggles == 0)
	{
		struct firefight_options fixed = *opts;
		fixed.firefight_unit_toggles = FIREFIGHT_UNIT_MASK_ALL;
		firefight_options_set(&fixed);
		opts = firefight_options_get();
	}

	ff_state.current_set = 1;
	ff_state.current_round = 1;
	ff_state.current_wave = 1;
	ff_state.shared_lives_remaining = ff->starting_lives != 0 ? ff->starting_lives : 7;
	ff_state.max_lives_cap = ff->max_lives_cap;
	ff_state.next_bonus_life_score = (opts && opts->firefight_score_life_threshold > 0) ?
		opts->firefight_score_life_threshold : FIREFIGHT_DEFAULT_SCORE_LIFE_BONUS;
	ff_state.active_dropship.vehicle_index = NONE;
	firefight_wrecks_reset();
	for (i = 0; i < FIREFIGHT_MAX_TRACKED_ENEMIES; i++)
		ff_state.tracked_enemy_units[i] = NONE;
	for (i = 0; i < FIREFIGHT_MAX_SET_VEHICLES; i++)
		ff_state.deployed_set_vehicles[i] = NONE;
	ff_state.deployed_set_vehicle_count = 0;
	firefight_player_vehicles_reset();
	for (i = 0; i < FIREFIGHT_MAX_GENERATORS; i++)
		ff_state.generators[i].object_index = NONE;
	firefight_ordnance_pod_reset();

	/* Synchronize global campaign difficulty with Firefight variant difficulty */
	game_difficulty_level_set((short)PIN(ff->difficulty, 0, NUMBER_OF_GAME_DIFFICULTY_LEVELS - 1));

	firefight_spawner_initialize_map();
	firefight_setup_faction_allegiances();
	if (ff->generator_count > 0)
	{
		short max_hp = (opts && opts->firefight_generator_health >= 500) ?
			opts->firefight_generator_health : 2000;
		firefight_generators_initialize(ff->generator_count, max_hp);
	}
	firefight_objectives_initialize_for_new_map();

	firefight_begin_pre_round(1);
	return TRUE;
}

static void firefight_dispose_from_old_map(
	void)
{
	cheat.bottomless_clip = FALSE;
	cheat.infinite_ammo = FALSE;
	if (ff_state.session_elapsed_ticks > 0 && !ff_state.match_won && !ff_state.match_lost && global_scenario_index != NONE)
	{
		firefight_stats_record_match_end(tag_get_name(global_scenario_index));
		ff_state.session_elapsed_ticks = 0;
	}
	firefight_objectives_dispose();
	firefight_spawner_dispose_map();
}

static void firefight_player_added(
	long player_index)
{
	struct player_datum *player = player_try_and_get(player_index);

	if (!player)
		return;
	player->team_index = 0;
	if (player->unit_index != NONE)
	{
		struct unit_datum *unit = unit_try_and_get(player->unit_index);

		if (unit)
			unit->object.owner_team_index = _game_team_player;
		firefight_apply_player_spawn_traits(player_index, player->unit_index);
	}
}

static void firefight_game_ending(
	void)
{
	/* (a client's copy of the host's rules is for its game only) */
	ae_firefight_net_clear();
}

static void firefight_game_starting(
	void)
{
}

static void firefight_statistics_append(
	long statistic)
{
	(void)statistic;
}

static void firefight_handle_client_message(
	void *message)
{
	(void)message;
}

static void firefight_handle_server_message(
	void *message)
{
	(void)message;
}

static void firefight_pregame_post_rasterize(
	void)
{
}

static void firefight_post_rasterize_objects(
	void)
{
	/* (Data Link's hill: firefight_objectives.c) */
	firefight_objectives_render();
	firefight_hud_post_rasterize();
}

static void firefight_player_update_each_tick(
	long player_index)
{
	(void)player_index;
}

static void firefight_player_killed_player(
	long killing_player_index,
	long killing_object_index,
	long dead_player_index,
	boolean friendly_fire)
{
	short abs_idx;
	short total_players = 0;
	short living_players;

	(void)killing_player_index;
	(void)killing_object_index;
	(void)friendly_fire;
	if (!firefight_active() || dead_player_index == NONE)
		return;

	abs_idx = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(dead_player_index);
	if (abs_idx >= 0 && abs_idx < HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
		ff_state.player_stats[abs_idx].deaths++;

	if (ff_state.shared_lives_remaining > 0)
		ff_state.shared_lives_remaining--;
	firefight_objectives_player_died(dead_player_index);

	living_players = firefight_count_living_players(&total_players);
	if (ff_state.shared_lives_remaining == 0 && living_players == 1 && !ff_state.last_man_standing_announced)
	{
		ff_state.last_man_standing_announced = TRUE;
		firefight_announce_banner(_ff_banner_last_man_standing, 0, 4 * TICKS_PER_SECOND);
	}
}

/* MAP CONTENT's SHIELD REGEN for a Firefight player's unit (damage.c's
recharge): -1 the engine's own, 0 none, 1 Halo's own (at the stock rate
where the map's player has none). A shield SHIELDS gave recharges unless
SHIELD REGEN is OFF */
short firefight_player_shield_regen(long object_index)
{
	struct unit_datum *unit;
	struct object_definition *definition;
	byte rule;

	if (!firefight_active() || object_index == NONE)
		return -1;
	unit = (struct unit_datum *)object_try_and_get_and_verify_type(object_index, _object_mask_unit);
	if (!unit || unit->unit.player_index == NONE)
		return -1;
	rule = firefight_health_rule(_firefight_health_rule_shield_regen);
	if (rule == _firefight_health_rule_off)
		return 0;
	if (rule == _firefight_health_rule_on)
		return 1;
	definition = object_definition_get(unit->definition_index);
	if (unit->object.maximum_shield_vitality > 0.0f && definition &&
		definition->object.collision_model.index != NONE &&
		collision_model_definition_get(definition->object.collision_model.index)->resistance.maximum_shield_vitality <= 0.0f)
	{
		return 1;
	}
	return -1;
}

/* MAP CONTENT's HEALTH REGEN (the host's): a player's health coming back
as a shield does, after FIREFIGHT_HEALTH_REGEN_DELAY seconds unhurt, full
in FIREFIGHT_HEALTH_REGEN_SECONDS */
#define FIREFIGHT_HEALTH_REGEN_DELAY 5
#define FIREFIGHT_HEALTH_REGEN_SECONDS 5
static void firefight_health_regen_update(long player_absolute, struct unit_datum *unit)
{
	static real last_body[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	static short quiet_ticks[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];

	if (player_absolute < 0 || player_absolute >= HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
		return;
	if (TEST_FLAG(unit->object.damage_flags, _object_dead_bit) ||
		unit->object.body_vitality < last_body[player_absolute] - 0.0001f)
	{
		quiet_ticks[player_absolute] = 0;
	}
	else if (quiet_ticks[player_absolute] < FIREFIGHT_HEALTH_REGEN_DELAY * TICKS_PER_SECOND)
		quiet_ticks[player_absolute]++;
	else if (unit->object.body_vitality < 1.0f && unit->object.body_vitality > 0.0f)
	{
		unit->object.body_vitality = MIN(1.0f,
			unit->object.body_vitality + 1.0f / (real)(FIREFIGHT_HEALTH_REGEN_SECONDS * TICKS_PER_SECOND));
	}
	last_body[player_absolute] = unit->object.body_vitality;
}

static void firefight_apply_tick_player_traits(
	void)
{
	struct firefight_options const *opts = firefight_options_get();
	struct data_iterator iter;
	struct player_datum *player;
	byte ammo_mode = opts ? opts->firefight_infinite_ammo : _firefight_ammo_normal;

	cheat.bottomless_clip = (ammo_mode == _firefight_ammo_bottomless_clip);
	cheat.infinite_ammo = (ammo_mode != _firefight_ammo_normal);

	if (!player_data)
		return;

	data_iterator_new(&iter, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iter)) != NULL)
	{
		struct unit_datum *unit;

		if (player->team_index != 0)
			player->team_index = 0;
		if (player->unit_index == NONE)
			continue;
		unit = (struct unit_datum *)object_try_and_get_and_verify_type(player->unit_index, _object_mask_unit);
		if (!unit)
			continue;
		if (unit->object.owner_team_index != _game_team_player)
			unit->object.owner_team_index = _game_team_player;
		if (firefight_health_rule(_firefight_health_rule_health_regen) == _firefight_health_rule_on)
			firefight_health_regen_update((short)DATUM_INDEX_TO_ABSOLUTE_INDEX(iter.datum_index), unit);

		if (ammo_mode != _firefight_ammo_normal && (game_time_get() % 15) == 0)
		{
			short slot;

			for (slot = 0; slot < 4; slot++)
			{
				long weap_idx = unit->unit.weapon_object_indices[slot];
				struct weapon_datum *weap;
				struct weapon_definition *weap_def;
				short m;

				if (weap_idx == NONE)
					continue;
				weap = (struct weapon_datum *)object_try_and_get_and_verify_type(weap_idx, _object_mask_weapon);
				if (!weap)
					continue;
				weap->weapon.age = 0.0f;
				weap->weapon.heat = 0.0f;
				weap_def = weapon_definition_get(weap->definition_index);
				if (!weap_def)
					continue;
				for (m = 0; m < weap_def->weapon.magazines.count && m < 2; m++)
				{
					struct weapon_magazine_definition *mag_def = TAG_BLOCK_GET_ELEMENT(
						&weap_def->weapon.magazines, m, struct weapon_magazine_definition);
					if (!mag_def)
						continue;
					if (mag_def->rounds_total_maximum > 0 &&
						weap->weapon.magazines[m].rounds_total < mag_def->rounds_total_maximum)
					{
						weap->weapon.magazines[m].rounds_total = mag_def->rounds_total_maximum;
					}
					if (ammo_mode == _firefight_ammo_bottomless_clip &&
						mag_def->rounds_loaded_maximum > 0)
					{
						weap->weapon.magazines[m].rounds_loaded = mag_def->rounds_loaded_maximum;
						weap->weapon.magazines[m].state = 0;
						weap->weapon.magazines[m].state_timer = 0;
					}
				}
			}
		}
	}
}

static void firefight_update(
	void)
{
	struct game_engine_firefight_variant const *ff = &global_variant.game_engine_variant.firefight;
	struct firefight_options const *opts = firefight_options_get();
	short total_players = 0;
	short living_players;

	if (!firefight_active())
		return;

	/* Feature 4: Total Session Game Time Counter (advances every tick while match is active) */
	if (!ff_state.match_won && !ff_state.match_lost && game_engine_can_score())
		ff_state.session_elapsed_ticks++;

	if (ff_state.banner_timer_ticks > 0)
		ff_state.banner_timer_ticks--;
	if (ff_state.ordnance_navpoint_ticks > 0)
		ff_state.ordnance_navpoint_ticks--;
	/* (every machine's: the pod is each one's own scenery) */
	firefight_ordnance_pod_update();
	/* (and the Flood's drop, Keyes' Flood form out of the floor) */
	firefight_flood_drop_update();
	/* (the objectives: the host's decided, a client's shown) */
	firefight_objectives_update();

	if (network_game_distributed_client())
		return;

	firefight_apply_tick_player_traits();
	if (ff_state.session_elapsed_ticks <= 2)
	{
		firefight_cleanup_natural_multiplayer_sandbox(ff_state.session_elapsed_ticks == 1);
		if (ff_state.session_elapsed_ticks == 2)
			firefight_spawn_player_vehicles();
	}
	/* (the game's first drop a few seconds in, 5 to 10: firefight_ordnance_pod_reset) */
	if (ff_state.session_elapsed_ticks == MAX(5, ff_state.first_ordnance_tick) &&
		(ff->ordnance_drops == _firefight_ordnance_per_round ||
			ff->ordnance_drops == _firefight_ordnance_frequent ||
			ff->ordnance_drops == _firefight_ordnance_per_set))
	{
		firefight_spawn_ordnance_drop(FALSE);
	}
	firefight_update_dropships_and_vehicles();
	firefight_generators_update();
	firefight_prune_dead_tracked_enemies();
	/* (stuck or far-off wave enemies brought to the players) */
	if (ff_state.phase_state == _ff_phase_wave_active)
		firefight_relocate_idle_enemies();
	/* (enemies that chose to drop over an edge after the players, landed) */
	firefight_drops_update();
	ff_state.team_score = campaign_score_team_points();

	/* Check wipe / defeat condition */
	living_players = firefight_count_living_players(&total_players);
	if (!ff_state.match_lost && !ff_state.match_won && total_players > 0 && living_players == 0 &&
		(ff_state.shared_lives_remaining == 0 || firefight_skull_active(_firefight_skull_iron_bit)))
	{
		ff_state.shared_lives_remaining = 0;
		ff_state.wipe_defeat_timer_ticks++;
		if (ff_state.wipe_defeat_timer_ticks >= 3 * TICKS_PER_SECOND)
		{
			ff_state.match_lost = TRUE;
			ff_state.phase_state = _ff_phase_defeat;
			ff_state.phase_timer_ticks = 5 * TICKS_PER_SECOND;
			firefight_announce_banner(_ff_banner_defeat, 0, 5 * TICKS_PER_SECOND);
			if (global_scenario_index != NONE)
				firefight_stats_record_match_end(tag_get_name(global_scenario_index));
			return;
		}
	}
	else
	{
		ff_state.wipe_defeat_timer_ticks = 0;
	}

	if (ff_state.phase_timer_ticks > 0)
		ff_state.phase_timer_ticks--;

	switch (ff_state.phase_state)
	{
	case _ff_phase_pre_set:
	case _ff_phase_pre_round:
		if (ff_state.phase_timer_ticks <= 0)
			firefight_begin_wave(1);
		break;

	case _ff_phase_wave_spawning:
		if (ff_state.phase_timer_ticks <= 0)
		{
			short spawned = firefight_spawn_wave(
				ff_state.current_set,
				ff_state.current_round,
				ff_state.current_wave,
				FALSE);

			ff_state.enemies_total_in_wave = spawned;
			error(_error_silent, "firefight: wave %d started, %d enemies (set %d, round %d)", (int)ff_state.current_wave,
				(int)spawned, (int)ff_state.current_set, (int)ff_state.current_round);
			ff_state.phase_state = _ff_phase_wave_active;
			firefight_objectives_wave_spawned(ff_state.current_wave);
		}
		break;

	case _ff_phase_wave_active:
	{
		boolean advance_wave = FALSE;
		byte wave_mode = opts ? opts->firefight_next_wave_mode : _firefight_next_wave_reach_default;

		ff_state.wave_elapsed_ticks++;
		if (ff_state.next_wave_timer_ticks > 0)
			ff_state.next_wave_timer_ticks--;

		if (ff_state.enemies_alive > 0 && ff_state.enemies_alive <= FIREFIGHT_STRAGGLER_THRESHOLD)
			ff_state.straggler_navpoints_active = TRUE;

		if (ff_state.enemies_alive <= 0 && firefight_queued_enemies_count() <= 0)
		{
			advance_wave = TRUE;
		}
		else if (ff_state.current_wave < FIREFIGHT_WAVES_PER_ROUND &&
			firefight_queued_enemies_count() <= 0)
		{
			if (wave_mode == _firefight_next_wave_reach_default &&
				ff_state.enemies_alive <= FIREFIGHT_REINFORCEMENT_THRESHOLD &&
				ff_state.wave_elapsed_ticks >= 10 * TICKS_PER_SECOND)
			{
				advance_wave = TRUE;
			}
			else if (wave_mode >= _firefight_next_wave_timer_30s &&
				ff_state.next_wave_timer_ticks <= 0)
			{
				advance_wave = TRUE;
			}
		}

		if (advance_wave)
		{
			error(_error_silent, "firefight: wave %d cleared (set %d, round %d)", (int)ff_state.current_wave,
				(int)ff_state.current_set, (int)ff_state.current_round);
			ff_state.phase_state = _ff_phase_wave_clear;
			ff_state.phase_timer_ticks = 3 * TICKS_PER_SECOND;
		}
		break;
	}

	case _ff_phase_wave_clear:
		if (ff_state.phase_timer_ticks <= 0)
		{
			/* (the wave cleared: its objective decided; the round's
			waves: the round's ended) */
			firefight_objectives_wave_ended();
			if (ff_state.current_wave >= FIREFIGHT_WAVES_PER_ROUND)
				firefight_objectives_round_ended();
			if (ff_state.current_wave < FIREFIGHT_WAVES_PER_ROUND)
			{
				firefight_begin_wave((short)(ff_state.current_wave + 1));
			}
			else if (ff_state.current_round < FIREFIGHT_ROUNDS_PER_SET)
			{
				firefight_add_shared_lives(ff->bonus_lives_per_round);
				firefight_begin_pre_round((short)(ff_state.current_round + 1));
			}
			else if (ff->bonus_round_enabled && !ff_state.in_bonus_round)
			{
				/* Enter 60-second Bonus Round (the players keep their vehicles) */
				ff_state.in_bonus_round = TRUE;
				ff_state.phase_state = _ff_phase_bonus_round;
				{
					struct firefight_options const *bonus_opts = firefight_options_get();

					ff_state.phase_timer_ticks = bonus_opts && bonus_opts->firefight_bonus_seconds >= 30 ?
						bonus_opts->firefight_bonus_seconds * TICKS_PER_SECOND : FIREFIGHT_BONUS_ROUND_TICKS;
				}
				firefight_recalculate_skulls_and_multiplier();
				firefight_spawn_wave(ff_state.current_set, ff_state.current_round, 1, TRUE);
				firefight_announce_banner(_ff_banner_bonus_round, 0, 4 * TICKS_PER_SECOND);
			}
			else
			{
				ff_state.in_bonus_round = FALSE;
				ff_state.phase_state = _ff_phase_set_complete;
				ff_state.phase_timer_ticks = 5 * TICKS_PER_SECOND;
				firefight_add_shared_lives(ff->bonus_lives_per_set);
				firefight_objectives_round_ended();
				skulls_unlock_next_by_milestone("Firefight set completed");
				firefight_announce_banner(_ff_banner_set_complete, ff_state.current_set, 4 * TICKS_PER_SECOND);
			}
		}
		break;

	case _ff_phase_bonus_round:
		if (ff_state.enemies_alive <= 4 && ff_state.phase_timer_ticks > 8 * TICKS_PER_SECOND)
			firefight_spawn_wave(ff_state.current_set, ff_state.current_round, 1, TRUE);
		if (ff_state.phase_timer_ticks <= 0)
		{
			ff_state.in_bonus_round = FALSE;
			/* (its enemies left: teleported out, not killed) */
			firefight_teleport_out_tracked_enemies();
			firefight_recalculate_skulls_and_multiplier();
			ff_state.phase_state = _ff_phase_set_complete;
			ff_state.phase_timer_ticks = 5 * TICKS_PER_SECOND;
			firefight_add_shared_lives(ff->bonus_lives_per_set);
			firefight_objectives_round_ended();
			skulls_unlock_next_by_milestone("Firefight set completed");
			firefight_announce_banner(_ff_banner_set_complete, ff_state.current_set, 4 * TICKS_PER_SECOND);
		}
		break;

	case _ff_phase_set_complete:
		if (ff_state.phase_timer_ticks <= 0)
		{
			if (ff->set_limit > 0 && ff_state.current_set >= ff->set_limit)
			{
				ff_state.match_won = TRUE;
				ff_state.phase_state = _ff_phase_victory;
				ff_state.phase_timer_ticks = 5 * TICKS_PER_SECOND;
				firefight_announce_banner(_ff_banner_victory, ff_state.current_set, 5 * TICKS_PER_SECOND);
				campaign_score_level_won();
				if (global_scenario_index != NONE)
					firefight_stats_record_match_end(tag_get_name(global_scenario_index));
			}
			else
			{
				ff_state.current_set++;
				firefight_begin_pre_round(1);
			}
		}
		break;

	case _ff_phase_victory:
	case _ff_phase_defeat:
	default:
		break;
	}
}

static long firefight_get_player_score(
	long player_index,
	enum get_score_type score_type)
{
	if (score_type == _get_score_team)
		return campaign_score_team_points();
	return campaign_score_player_points(player_index);
}

static wchar_t *firefight_format_player_score(
	long player_index,
	wchar_t *out_str)
{
	if (out_str)
		usnprintf(out_str, 64, L"%ld", campaign_score_player_points(player_index));
	return out_str;
}

static wchar_t *firefight_format_score_name(
	wchar_t *out_str)
{
	if (out_str)
		ustrcpy(out_str, L"Score");
	return out_str;
}

static wchar_t *firefight_format_team_name(
	long team_index,
	wchar_t *out_str)
{
	if (out_str)
		usnprintf(out_str, 64, L"%ld", team_index == 0 ? campaign_score_team_points() : 0L);
	return out_str;
}

static boolean firefight_allow_pick_up(
	long unit_index,
	long weapon_index)
{
	(void)unit_index;
	/* (an objective's skulls and planted bomb: firefight_objectives.c) */
	return !firefight_objectives_pickup_blocked(weapon_index);
}

static void firefight_player_damaged_player(
	long damaging_player_index,
	long dead_player_index,
	boolean damage_type)
{
	(void)damaging_player_index;
	(void)dead_player_index;
	(void)damage_type;
}

static boolean firefight_format_message(
	long player_index,
	long message,
	long message_data,
	wchar_t *buffer,
	long buffer_size)
{
	(void)player_index;
	(void)message;
	(void)message_data;
	(void)buffer;
	(void)buffer_size;
	return FALSE;
}

static void firefight_prespawn_player_update(
	long player_index)
{
	struct player_datum *player = player_try_and_get(player_index);

	if (player)
		player->team_index = 0;
}

static boolean firefight_test_flag(
	long flag)
{
	return flag == 1;
}

static long firefight_did_player_win(
	long player_index)
{
	(void)player_index;
	return ff_state.match_won ? 1 : 0;
}

struct game_engine firefight_engine =
{
	"firefight",
	game_engine_firefight,
	firefight_dispose,
	firefight_initialize_for_new_map,
	firefight_dispose_from_old_map,
	firefight_player_added,
	firefight_game_ending,
	firefight_game_starting,
	firefight_statistics_append,
	firefight_handle_client_message,
	firefight_handle_server_message,
	firefight_pregame_post_rasterize,
	firefight_post_rasterize_objects,
	firefight_player_update_each_tick,
	NULL,
	NULL,
	NULL,
	firefight_update,
	firefight_get_player_score,
	firefight_format_player_score,
	firefight_format_score_name,
	firefight_format_team_name,
	firefight_allow_pick_up,
	firefight_player_damaged_player,
	firefight_player_killed_player,
	firefight_format_message,
	NULL,
	firefight_prespawn_player_update,
	NULL,
	NULL,
	NULL,
	firefight_test_flag,
	NULL,
	firefight_did_player_win
};

/* ---------- DEV MODE (ae_dev_cmd.c's firefight command) */

/* the round's wave <n> begun now, the pre-round countdown or the wave running skipped; FALSE when no
Firefight game is played here or a client's */
boolean firefight_dev_wave(short wave)
{
	if (!firefight_active() || network_game_distributed_client() || ff_state.match_won || ff_state.match_lost)
		return FALSE;
	firefight_begin_wave((short)PIN(wave, 1, FIREFIGHT_WAVES_PER_ROUND));
	return TRUE;
}

/* the enemies of the wave gone (as a round's end takes them out), so that it is cleared */
boolean firefight_dev_clear(void)
{
	if (!firefight_active() || network_game_distributed_client())
		return FALSE;
	firefight_teleport_out_tracked_enemies();
	firefight_prune_dead_tracked_enemies();
	return TRUE;
}

/* a Firefight skull turned on for the round, by its name ("iron", "grunt_birthday_party"); FALSE for none */
boolean firefight_dev_skull(char const *name)
{
	static char const *const names[NUMBER_OF_FIREFIGHT_SKULLS] =
	{
		"iron", "black_eye", "tough_luck", "catch", "cloud", "famine", "tilt", "mythic", "thunderstorm", "blind",
		"boom", "cowbell", "grunt_birthday_party"
	};
	short bit;

	if (!firefight_active() || !name)
		return FALSE;
	for (bit = 0; bit < NUMBER_OF_FIREFIGHT_SKULLS; bit++)
	{
		if (!_stricmp(name, names[bit]))
		{
			ff_state.round_skulls |= 1UL << bit;
			firefight_recalculate_skulls_and_multiplier();
			return TRUE;
		}
	}
	return FALSE;
}
