/*
AE_CE_HOST_RULES.C

Arena Evolved joining a Content Evolved host (ae_ce_host_rules.h).
*/

#include "ae_ce_host_rules.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* ---------- the signature */

int ae_ce_host_signed(unsigned version)
{
	return (version & AE_CE_HOST_MARK) == AE_CE_HOST_MARK && version <= 0xFFFF;
}

unsigned ae_ce_host_mod_of(unsigned version)
{
	return version & ~(unsigned)AE_CE_HOST_MARK & 0xFFFF;
}

int ae_ce_host_classify(unsigned version, unsigned base_version, unsigned flags, unsigned our_version)
{
	unsigned mod;

	if (!ae_ce_host_signed(version))
		return AE_CE_HOST_NOT_CE;
	mod = ae_ce_host_mod_of(version);
	if (base_version != our_version)
		return AE_CE_HOST_BASE_OTHER;
	if (mod < AE_CE_HOST_MOD_VERSION)
		return AE_CE_HOST_MOD_OLDER;
	if (mod > AE_CE_HOST_MOD_VERSION)
		return AE_CE_HOST_MOD_NEWER;
	if (flags & AE_CE_HOST_FLAG_TAG_SHARE)
		return AE_CE_HOST_TAG_SHARE;
	return AE_CE_HOST_JOIN;
}

int ae_ce_host_listing_taken(int modded, int mod_version)
{
	return !modded || mod_version == AE_CE_HOST_MOD_VERSION;
}

int ae_ce_host_listing_engine(int modded, int engine, int ae_firefight)
{
	return modded && engine == AE_CE_HOST_ENGINE_CE_FIREFIGHT ? ae_firefight : engine;
}

int ae_ce_host_class_joinable(int host_class)
{
	return host_class == AE_CE_HOST_JOIN;
}

/* ---------- the join check */

void ae_ce_host_join_decide(int host_class, int randomizer_pack_missing, int skulls_pack_missing, int tell_player,
	struct ae_ce_host_join_verdict *verdict)
{
	if (!verdict)
		return;
	memset(verdict, 0, sizeof(*verdict));
	switch (host_class)
	{
	case AE_CE_HOST_NOT_CE:
		verdict->joinable = 1;
		return;
	case AE_CE_HOST_JOIN:
		if (!randomizer_pack_missing && !skulls_pack_missing)
		{
			verdict->joinable = 1;
			verdict->say = AE_CE_HOST_SAY_JOINABLE;
			verdict->log = tell_player != 0;
			return;
		}
		verdict->say = AE_CE_HOST_SAY_PACKS_MISSING;
		verdict->request_packs = tell_player ?
			(randomizer_pack_missing ? AE_CE_HOST_PACK_RANDOMIZER : 0) | (skulls_pack_missing ? AE_CE_HOST_PACK_SKULLS : 0) : 0;
		break;
	case AE_CE_HOST_MOD_OLDER:
		verdict->say = AE_CE_HOST_SAY_MOD_VERSION_OLDER;
		break;
	case AE_CE_HOST_MOD_NEWER:
		verdict->say = AE_CE_HOST_SAY_MOD_VERSION_NEWER;
		break;
	case AE_CE_HOST_TAG_SHARE:
		verdict->say = AE_CE_HOST_SAY_TAG_SHARE;
		break;
	default:
		verdict->say = AE_CE_HOST_SAY_NETWORK_VERSION;
		break;
	}
	verdict->log = tell_player != 0;
	verdict->tell_player = tell_player != 0;
}

/* ---------- the joined host's state */

int ae_ce_host_tags_leave(long count, unsigned long hash, long host_count, unsigned long host_hash, int replay_playing)
{
	return !replay_playing && (count != host_count || hash != host_hash);
}

int ae_ce_host_state_next(int state, int event, int *clear_options)
{
	int holds_options = state == AE_CE_HOST_STATE_CE;

	if (clear_options)
		*clear_options = 0;
	if (state < AE_CE_HOST_STATE_NOT_CE || state > AE_CE_HOST_STATE_REFUSED)
		state = AE_CE_HOST_STATE_NOT_CE;
	switch (event)
	{
	case AE_CE_HOST_EVENT_JOIN_STARTED_CE:
	case AE_CE_HOST_EVENT_JOIN_STARTED_OTHER:
	case AE_CE_HOST_EVENT_JOIN_STARTED_UNKNOWN:
		if (clear_options)
			*clear_options = holds_options;
		return event == AE_CE_HOST_EVENT_JOIN_STARTED_CE ? AE_CE_HOST_STATE_CE :
			event == AE_CE_HOST_EVENT_JOIN_STARTED_UNKNOWN ? AE_CE_HOST_STATE_UNKNOWN : AE_CE_HOST_STATE_NOT_CE;
	case AE_CE_HOST_EVENT_RECORD_ACCEPTED:
		return state == AE_CE_HOST_STATE_CE || state == AE_CE_HOST_STATE_UNKNOWN ? AE_CE_HOST_STATE_CE : state;
	case AE_CE_HOST_EVENT_RECORD_REFUSED:
		if (state == AE_CE_HOST_STATE_CE || state == AE_CE_HOST_STATE_UNKNOWN)
		{
			if (clear_options)
				*clear_options = holds_options;
			return AE_CE_HOST_STATE_REFUSED;
		}
		return state;
	case AE_CE_HOST_EVENT_REJECTED:
	case AE_CE_HOST_EVENT_DISCONNECTED:
	case AE_CE_HOST_EVENT_LEFT:
	case AE_CE_HOST_EVENT_HOSTING:
	case AE_CE_HOST_EVENT_SOLO_START:
		if (clear_options)
			*clear_options = holds_options;
		/* (a refused host stays refused until the next join starts, so its later pieces do not tell again) */
		return state == AE_CE_HOST_STATE_REFUSED ? AE_CE_HOST_STATE_REFUSED : AE_CE_HOST_STATE_NOT_CE;
	}
	return state;
}

/* ---------- CE's game variant options

CE's struct game_variant_options (source/game/game_engine.h of the CE build, 0x58 bytes), with fixed-width
types: the first 0x2C bytes are the options Arena Evolved has too (CE's last five bytes, floodfection and
mode_variant, are Arena Evolved's jump_percent, weapon_pack_high, pack_start and reserved), the rest are
Firefight's. */

struct ce_options
{
	int16_t time_limit;
	int16_t friendly_fire;
	int16_t friendly_fire_penalty;
	int16_t vehicle_respawn_time;
	uint8_t auto_team_balance;
	uint8_t radar_players;
	uint8_t vehicle_set[2];
	uint8_t vehicle_counts[2][6];
	uint8_t loadout;
	uint8_t primary_weapon;
	uint8_t secondary_weapon;
	uint8_t no_map_weapons;
	uint8_t mode;
	uint8_t hardcore;
	uint8_t infection_initial_zombies;
	uint8_t infection_zombie_speed;
	uint8_t infection_skull_damage;
	uint8_t infection_respawn_time;
	uint8_t infection_countdown;
	uint8_t infection_zombie_shields;
	uint8_t gun_game_order;
	uint8_t gun_game_melee_demotes;
	uint8_t force_team_spawns;
	uint8_t floodfection[4];
	uint8_t mode_variant;
	uint32_t firefight_custom_skulls;
	int16_t firefight_generator_health;
	int16_t firefight_score_life_threshold;
	uint8_t firefight_enemy_health_scale;
	uint8_t firefight_enemy_shield_scale;
	uint8_t firefight_bonus_seconds;
	uint8_t firefight_faction_diplomacy;
	uint32_t firefight_tag_profile;
	uint32_t firefight_starting_skulls[2];
	uint16_t firefight_unit_toggles;
	uint8_t firefight_set_escalation;
	uint8_t firefight_dropship_mode;
	uint8_t firefight_next_wave_mode;
	uint8_t firefight_player_faction_mode;
	uint8_t firefight_vehicle_spawns;
	uint8_t firefight_plasma_flinch;
	uint8_t firefight_unused_vehicle;
	uint8_t firefight_prep_countdown;
	uint8_t firefight_skull_progression_mode;
	uint8_t firefight_player_shields;
	uint8_t firefight_player_speed;
	uint8_t firefight_player_gravity;
	uint8_t firefight_infinite_ammo;
	uint8_t firefight_map_content;
	uint8_t firefight_wave_size_scale;
	uint8_t firefight_boss_wave_mode;
	uint8_t firefight_enemy_loadout;
	uint8_t firefight_friendly_ai;
};

typedef char verify_ce_options_size[sizeof(struct ce_options) == AE_CE_HOST_OPTIONS_CE_SIZE ? 1 : -1];
typedef char verify_ce_options_base[offsetof(struct ce_options, firefight_custom_skulls) == AE_CE_HOST_OPTIONS_AE_SIZE ? 1 : -1];

/* each value: its offset in CE's options and its bytes, in Arena Evolved's wire table's order */
#define VALUE(field, width) { offsetof(struct ce_options, field), (width) }
static struct
{
	size_t offset;
	unsigned char width;
} const values_table[AE_CE_HOST_VALUES] =
{
	VALUE(friendly_fire, 1), /* (CE's is a short; the low byte is the rule's value) */
	VALUE(loadout, 1),
	VALUE(primary_weapon, 1),
	VALUE(secondary_weapon, 1),
	VALUE(firefight_custom_skulls, 4),
	VALUE(firefight_generator_health, 2),
	VALUE(firefight_score_life_threshold, 2),
	VALUE(firefight_enemy_health_scale, 1),
	VALUE(firefight_enemy_shield_scale, 1),
	VALUE(firefight_bonus_seconds, 1),
	VALUE(firefight_faction_diplomacy, 1),
	VALUE(firefight_tag_profile, 4),
	VALUE(firefight_starting_skulls[0], 4),
	VALUE(firefight_starting_skulls[1], 4),
	VALUE(firefight_unit_toggles, 2),
	VALUE(firefight_set_escalation, 1),
	VALUE(firefight_dropship_mode, 1),
	VALUE(firefight_next_wave_mode, 1),
	VALUE(firefight_player_faction_mode, 1),
	VALUE(firefight_vehicle_spawns, 1),
	VALUE(firefight_plasma_flinch, 1),
	VALUE(firefight_unused_vehicle, 1),
	VALUE(firefight_prep_countdown, 1),
	VALUE(firefight_skull_progression_mode, 1),
	VALUE(firefight_player_shields, 1),
	VALUE(firefight_player_speed, 1),
	VALUE(firefight_player_gravity, 1),
	VALUE(firefight_infinite_ammo, 1),
	VALUE(firefight_map_content, 1),
	VALUE(firefight_wave_size_scale, 1),
	VALUE(firefight_boss_wave_mode, 1),
	VALUE(firefight_enemy_loadout, 1),
	VALUE(firefight_friendly_ai, 1),
};
#undef VALUE

unsigned char const ae_ce_host_value_widths[AE_CE_HOST_VALUES] =
{
	1, 1, 1, 1, 4, 2, 2, 1, 1, 1, 1, 4, 4, 4, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

/* ---------- the record */

unsigned long ae_ce_host_record_size(struct ae_ce_host_layout const *layout)
{
	return layout ? layout->ae_size + AE_CE_HOST_RECORD_EXTRA : 0;
}

static unsigned long read_little(unsigned char const *at, unsigned long size)
{
	unsigned long value = 0;
	unsigned long index;

	for (index = 0; index < size && index < sizeof(value); index++)
		value |= (unsigned long)at[index] << (8 * index);

	return value;
}

static void write_little(unsigned char *at, unsigned long size, unsigned long value)
{
	unsigned long index;

	for (index = 0; index < size; index++)
		at[index] = index < sizeof(value) ? (unsigned char)(value >> (8 * index)) : 0;
}

static int layout_ok(struct ae_ce_host_layout const *layout)
{
	return layout && layout->engine_index_size >= 1 && layout->engine_index_size <= 8 &&
		layout->engine_index + layout->engine_index_size <= layout->variant_options &&
		layout->variant_options + AE_CE_HOST_OPTIONS_AE_SIZE <= layout->ae_size &&
		layout->ae_jump_percent < AE_CE_HOST_OPTIONS_AE_SIZE &&
		layout->ae_jump_percent == offsetof(struct ce_options, floodfection);
}

int ae_ce_host_record_convert(unsigned char const *ce, unsigned long ce_size, unsigned char *ae,
	struct ae_ce_host_layout const *layout, unsigned long *values)
{
	unsigned long options;
	unsigned long index;
	unsigned char option_bytes[AE_CE_HOST_OPTIONS_CE_SIZE];

	if (!ce || !ae || !values || !layout_ok(layout) || ce_size != ae_ce_host_record_size(layout))
		return AE_CE_HOST_RECORD_BAD_SIZE;
	options = layout->variant_options;
	if (read_little(ce + layout->engine_index, layout->engine_index_size) != AE_CE_HOST_ENGINE_CE_FIREFIGHT)
		return AE_CE_HOST_RECORD_NOT_FIREFIGHT;
	memcpy(option_bytes, ce + options, sizeof(option_bytes));
	for (index = 0; index < AE_CE_HOST_VALUES; index++)
	{
		unsigned char const *at = option_bytes + values_table[index].offset;
		unsigned long value = 0;

		if (values_table[index].width == 1)
			value = *at;
		else
			value = read_little(at, values_table[index].width);
		values[index] = value;
	}
	if (values[AE_CE_HOST_VALUE_TAG_PROFILE])
		return AE_CE_HOST_RECORD_TAG_PROFILE;
	/* (the record up to the end of the first 0x2C option bytes, then what follows CE's Firefight bytes) */
	memcpy(ae, ce, options + AE_CE_HOST_OPTIONS_AE_SIZE);
	memset(ae + options + layout->ae_jump_percent, 0, AE_CE_HOST_OPTIONS_AE_SIZE - layout->ae_jump_percent);
	memcpy(ae + options + AE_CE_HOST_OPTIONS_AE_SIZE, ce + options + AE_CE_HOST_OPTIONS_CE_SIZE,
		ce_size - options - AE_CE_HOST_OPTIONS_CE_SIZE);
	write_little(ae + layout->engine_index, layout->engine_index_size, layout->ae_engine_firefight);
	return AE_CE_HOST_RECORD_OK;
}
