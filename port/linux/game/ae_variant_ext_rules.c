/* AE's extra gametype settings: the rules (see the header) */
#include "ae_variant_ext_rules.h"

#include <string.h>

#define TICKS_PER_SECOND_EXT 30
#define NONE_EXT (-1L)

void ae_variant_ext_default(struct ae_variant_ext *ext)
{
	memset(ext, 0, sizeof(*ext));
}

int ae_variant_ext_is_default(struct ae_variant_ext const *ext)
{
	struct ae_variant_ext zero;

	memset(&zero, 0, sizeof(zero));
	return !memcmp(ext, &zero, sizeof(zero));
}

static int clamp_byte(unsigned char *value, unsigned limit)
{
	if (*value < limit)
		return 1;
	*value = 0;
	return 0;
}

static int range_byte(unsigned char *value, unsigned low, unsigned high)
{
	if (!*value || (*value >= low && *value <= high))
		return 1;
	*value = 0;
	return 0;
}

int ae_variant_ext_sanitize(struct ae_variant_ext *ext)
{
	int kept = 1;
	unsigned char waypoint = (unsigned char)(ext->oddball_flags & 0x0F);

	kept &= clamp_byte(&ext->rounds, AE_EXT_ROUNDS_COUNT);
	/* (round time up to 20 minutes) */
	kept &= range_byte(&ext->round_time_10s, 1, 120);
	if (ext->match_flags & ~0x03)
		ext->match_flags &= 0x03, kept = 0;
	kept &= clamp_byte(&ext->respawn_modifier, AE_EXT_RESPAWN_MODIFIER_COUNT);
	if (ext->player_flags & ~0x1F)
		ext->player_flags &= 0x1F, kept = 0;
	kept &= range_byte(&ext->max_active_players, 2, 16);
	kept &= clamp_byte(&ext->weapon_respawn, AE_EXT_WEAPON_RESPAWN_COUNT);
	if (ext->slayer_flags & ~0x07)
		ext->slayer_flags &= 0x07, kept = 0;
	kept &= range_byte(&ext->king_move_10s, 1, 30);
	if (ext->king_flags & ~0x1F)
		ext->king_flags &= 0x1F, kept = 0;
	if (waypoint >= AE_EXT_WAYPOINT_COUNT || (ext->oddball_flags & 0x80))
	{
		ext->oddball_flags = (unsigned char)(ext->oddball_flags & 0x70);
		kept = 0;
	}
	kept &= clamp_byte(&ext->ctf_flag_type, AE_EXT_OBJECTIVE_COUNT);
	kept &= range_byte(&ext->ctf_reset_time, 1, 120);
	if (ext->ctf_flags & 0x80)
		ext->ctf_flags &= 0x7F, kept = 0;
	kept &= clamp_byte(&ext->ctf_own_flag_waypoint, AE_EXT_WAYPOINT_COUNT);
	kept &= clamp_byte(&ext->bomb_type, AE_EXT_OBJECTIVE_COUNT);
	kept &= clamp_byte(&ext->bomb_enemy_indicator, AE_EXT_WAYPOINT_COUNT);
	kept &= clamp_byte(&ext->mode, AE_EXT_MODE_COUNT);
	kept &= clamp_byte(&ext->juggernaut_movement, AE_EXT_MOVEMENT_COUNT);
	if (ext->juggernaut_flags & 0x80)
		ext->juggernaut_flags &= 0x7F, kept = 0;
	kept &= range_byte(&ext->territories, 1, 8);
	kept &= range_byte(&ext->control_time, 1, 60);
	kept &= range_byte(&ext->contest_time, 1, 60);
	kept &= clamp_byte(&ext->carrier_movement, AE_EXT_MOVEMENT_COUNT);
	if (ext->round_flags & ~AE_EXT_ROUND_FLAGS)
		ext->round_flags &= AE_EXT_ROUND_FLAGS, kept = 0;
	kept &= range_byte(&ext->zone_wait_s, 10, 250);
	kept &= range_byte(&ext->zone_shrink_s, 10, 250);
	kept &= range_byte(&ext->revive_10ths, 5, 100);
	kept &= range_byte(&ext->bleedout_s, 5, 250);
	{
		size_t index;

		for (index = 0; index < sizeof(ext->reserved); index++)
		{
			if (ext->reserved[index])
				ext->reserved[index] = 0, kept = 0;
		}
	}
	return kept;
}

int ae_ext_rounds_count(struct ae_variant_ext const *ext)
{
	static const int counts[AE_EXT_ROUNDS_COUNT] = { 0, 1, 2, 4, 6, 3, 5, 7, 9 };

	return ext->rounds < AE_EXT_ROUNDS_COUNT ? counts[ext->rounds] : 0;
}

int ae_ext_rounds_to_win(struct ae_variant_ext const *ext)
{
	switch (ext->rounds)
	{
	case AE_EXT_ROUNDS_BEST_OF_3: return 2;
	case AE_EXT_ROUNDS_BEST_OF_5: return 3;
	case AE_EXT_ROUNDS_BEST_OF_7: return 4;
	case AE_EXT_ROUNDS_BEST_OF_9: return 5;
	default: return 0;
	}
}

long ae_ext_round_time_seconds(struct ae_variant_ext const *ext)
{
	return ext->round_time_10s * 10L;
}

long ae_ext_weapon_respawn_ticks(struct ae_variant_ext const *ext, long map_ticks)
{
	switch (ext->weapon_respawn)
	{
	case AE_EXT_WEAPON_RESPAWN_NONE: return NONE_EXT;
	case AE_EXT_WEAPON_RESPAWN_HALF: return map_ticks / 2;
	case AE_EXT_WEAPON_RESPAWN_DOUBLE: return map_ticks * 2;
	default: return map_ticks;
	}
}

int ae_ext_damage_taken_percent(int resistant)
{
	/* (the second-generation game's damage resistance: half) */
	return resistant ? 50 : 100;
}

int ae_ext_damage_dealt_percent(int extra)
{
	return extra ? 200 : 100;
}

int ae_ext_movement_percent(int movement)
{
	switch (movement)
	{
	case AE_EXT_MOVEMENT_SLOW: return 75;
	case AE_EXT_MOVEMENT_FAST: return 125;
	default: return 100;
	}
}

long ae_ext_king_move_seconds(struct ae_variant_ext const *ext)
{
	return ext->king_move_10s * 10L;
}

long ae_ext_respawn_wait_ticks(int modifier, long base, long team_last_left, long now_ticks)
{
	if (base <= 0)
		return base < 0 ? 0 : base;
	switch (modifier)
	{
	case AE_EXT_RESPAWN_MODIFIER_INHERITANCE:
		/* (with the teammate who died last, never sooner than the player's own wait would end... nor later) */
		return team_last_left >= 0 && team_last_left < base ? team_last_left : base;
	case AE_EXT_RESPAWN_MODIFIER_CYCLING:
	{
		/* (the next wave: waves every base ticks of game time, at least a second away) */
		long wait = base - (now_ticks >= 0 ? now_ticks % base : 0);

		return wait < TICKS_PER_SECOND_EXT ? wait + base : wait;
	}
	default:
		return base;
	}
}

int ae_ext_waypoint_shows(int rule, int held, int armed, int home)
{
	switch (rule)
	{
	case AE_EXT_WAYPOINT_ALWAYS: return 1;
	case AE_EXT_WAYPOINT_LOOSE: return !held && !home;
	case AE_EXT_WAYPOINT_HELD: return held || armed;
	case AE_EXT_WAYPOINT_OFF: return 0;
	default: return -1; /* the gametype's own rule */
	}
}

int ae_ext_territories_count(struct ae_variant_ext const *ext, int map_hills)
{
	int count = ext->territories ? ext->territories : map_hills;

	if (count > map_hills)
		count = map_hills;
	if (count > 8)
		count = 8;
	return count < 0 ? 0 : count;
}

int ae_ext_control_seconds(struct ae_variant_ext const *ext)
{
	return ext->control_time ? ext->control_time : 5;
}

int ae_ext_contest_seconds(struct ae_variant_ext const *ext)
{
	return ext->contest_time ? ext->contest_time : 5;
}
