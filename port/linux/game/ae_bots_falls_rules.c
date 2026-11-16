/*
AE_BOTS_FALLS_RULES.C

Arena Evolved: the bots' deaths by their own hand (ae_bots_falls_rules.h).
*/

#include <stddef.h>
#include "ae_bots_falls_rules.h"
#include "ae_bots_combat_rules.h"

#define AE_BOTS_FALLS_PI 3.14159265f

static char const *const ae_bots_death_cause_names[AE_BOTS_DEATH_CAUSES] =
{
	"other", "player", "fall", "pit", "barrier", "own_grenade", "own_explosive", "self"
};

char const *ae_bots_death_cause_name(int cause)
{
	if (cause < 0 || cause >= AE_BOTS_DEATH_CAUSES)
		return "other";
	return ae_bots_death_cause_names[cause];
}

static char ae_bots_falls_lower(char c)
{
	return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

/* whether a tag name holds a word (ignoring case) */
static int ae_bots_falls_has(char const *name, char const *word)
{
	char const *start;

	if (!name)
		return 0;
	for (start = name; *start; start++)
	{
		char const *a = start, *b = word;

		while (*a && *b && ae_bots_falls_lower(*a) == *b)
		{
			a++;
			b++;
		}
		if (!*b)
			return 1;
	}
	return 0;
}

/* the last part of a tag name (after its last backslash) */
static char const *ae_bots_falls_leaf(char const *name)
{
	char const *leaf = name, *c;

	for (c = name; *c; c++)
	{
		if (*c == '\\' || *c == '/')
			leaf = c + 1;
	}
	return leaf;
}

int ae_bots_death_cause(int known, char const *damage_name, int owner_self, int owner_none, int kill_instantly,
	int riding)
{
	if (!known)
		return AE_BOTS_DEATH_OTHER;
	/* (a rider dies with its vehicle, an act of god too: not the map's kill) */
	if (riding && !owner_self && owner_none)
		return AE_BOTS_DEATH_OTHER;
	if (owner_self)
	{
		if (ae_bots_falls_has(damage_name, "grenade"))
			return AE_BOTS_DEATH_OWN_GRENADE;
		if (ae_bots_falls_has(damage_name, "explosion") || ae_bots_falls_has(damage_name, "rocket") ||
			ae_bots_falls_has(damage_name, "plasma_cannon") || ae_bots_falls_has(damage_name, "fuel rod"))
		{
			return AE_BOTS_DEATH_OWN_EXPLOSIVE;
		}
		return AE_BOTS_DEATH_SELF;
	}
	if (!owner_none)
		return AE_BOTS_DEATH_PLAYER;
	/* (unit_kill: the globals' falling damage, at once, no owner) */
	if (kill_instantly)
		return AE_BOTS_DEATH_BARRIER;
	if (damage_name)
	{
		char const *leaf = ae_bots_falls_leaf(damage_name);

		if (ae_bots_falls_has(leaf, "distance"))
			return AE_BOTS_DEATH_PIT;
		if (ae_bots_falls_has(leaf, "falling"))
			return AE_BOTS_DEATH_FALL;
	}
	return AE_BOTS_DEATH_OTHER;
}

float ae_bots_edge_lookahead(int level)
{
	switch (level)
	{
	case AE_BOTS_LEVEL_RECRUIT: return 0.7f;
	case AE_BOTS_LEVEL_EASY: return 0.8f;
	case AE_BOTS_LEVEL_NORMAL: return 1.0f;
	case AE_BOTS_LEVEL_HEROIC: return 1.2f;
	case AE_BOTS_LEVEL_ELITE: return 1.3f;
	default: return 1.4f;
	}
}

float ae_bots_edge_attention(int level, int fighting)
{
	if (!fighting)
		return 1.0f;
	switch (level)
	{
	case AE_BOTS_LEVEL_RECRUIT: return 0.8f;
	case AE_BOTS_LEVEL_EASY: return 0.85f;
	default: return 1.0f;
	}
}

float ae_bots_edge_lethal_distance(int no_falling_damage, float maximum_distance, float full_damage_distance)
{
	float lethal = maximum_distance > 0.5f && maximum_distance < 100.0f ? maximum_distance : 15.0f;

	if (!no_falling_damage && full_damage_distance > 0.5f && full_damage_distance < lethal)
		lethal = full_damage_distance;
	return lethal;
}

int ae_bots_edge_lethal(int floor_found, float drop, float lethal_distance)
{
	return !floor_found || drop >= lethal_distance * 0.9f;
}

int ae_bots_edge_drop_allowed(float drop, float target_below, float heading_cosine)
{
	return drop > 0.0f && drop <= AE_BOTS_EDGE_SMALL_DROP && target_below >= AE_BOTS_EDGE_TARGET_BELOW &&
		heading_cosine >= 0.5f;
}

float ae_bots_edge_turn(int attempt, int side, int walking)
{
	static float const fight[AE_BOTS_EDGE_TURNS] = { 2.0f, -2.0f, 1.0f, -1.0f, 3.0f, -3.0f, 4.0f };
	static float const walk[AE_BOTS_EDGE_TURNS] = { 4.0f, 3.0f, -3.0f, 2.0f, -2.0f, 1.0f, -1.0f };

	if (attempt < 0 || attempt >= AE_BOTS_EDGE_TURNS)
		return 0.0f;
	return (walking ? walk : fight)[attempt] * (side < 0 ? -1.0f : 1.0f) * AE_BOTS_FALLS_PI * 0.25f;
}

int ae_bots_grenade_flee(int level, int own, float distance)
{
	if (!own || level < AE_BOTS_LEVEL_EASY || level >= AE_BOTS_LEVELS)
		return 0;
	return distance < AE_BOTS_GRENADE_DANGER;
}

int ae_bots_grenade_judge(int level, float roll)
{
	if (level <= AE_BOTS_LEVEL_RECRUIT || level >= AE_BOTS_LEVELS)
		return 0;
	return roll < ae_bots_dodge_chance(level);
}

int ae_bots_grenade_flee_other(int level, float noticed_seconds, int judged_flee, float distance)
{
	if (!judged_flee || level <= AE_BOTS_LEVEL_RECRUIT || level >= AE_BOTS_LEVELS)
		return 0;
	return noticed_seconds >= ae_bots_dodge_reaction(level) && distance < AE_BOTS_GRENADE_DANGER;
}

float ae_bots_explosive_nearest(int level)
{
	if (level < AE_BOTS_LEVEL_NORMAL)
		return 0.0f;
	if (level == AE_BOTS_LEVEL_NORMAL)
		return 4.0f;
	if (level == AE_BOTS_LEVEL_HEROIC)
		return 3.5f;
	if (level == AE_BOTS_LEVEL_ELITE)
		return 3.25f;
	return 3.0f;
}

int ae_bots_explosive_fire_safe(int level, float wall_distance, float target_distance)
{
	float nearest = ae_bots_explosive_nearest(level);

	return wall_distance >= nearest && target_distance >= nearest;
}

int ae_bots_grenade_throw_safe(float wall_distance, float target_distance)
{
	if (wall_distance < AE_BOTS_GRENADE_WALL || target_distance < AE_BOTS_GRENADE_BLAST)
		return 0;
	return !(wall_distance < AE_BOTS_GRENADE_SHORT && wall_distance < target_distance - AE_BOTS_GRENADE_SHORT_MARGIN);
}

float ae_bots_dodge_reaction(int level)
{
	static float const seconds[] = { 99.0f, 0.60f, 0.45f, 0.35f, 0.31f, 0.28f, 0.22f };

	return seconds[level < 0 ? 0 : level > 6 ? 6 : level];
}

float ae_bots_dodge_chance(int level)
{
	static float const chance[] = { 0.0f, 0.25f, 0.45f, 0.65f, 0.725f, 0.80f, 0.90f };

	return chance[level < 0 ? 0 : level > 6 ? 6 : level];
}

float ae_bots_dodge_reroll_chance(int level)
{
	return ae_bots_dodge_chance(level) * 0.5f;
}

int ae_bots_dodge_reroll_due(int vehicle, int judged_dodge, long now, long last_roll_tick)
{
	return vehicle && !judged_dodge && (now - last_roll_tick >= AE_BOTS_DODGE_REROLL_TICKS || now < last_roll_tick);
}

int ae_bots_tactical_move_allowed(int dodging, long now, long unstick_until_tick, long free_until_tick)
{
	return !dodging && now >= unstick_until_tick && now >= free_until_tick;
}
