/*
AE_SPAWNER_RULES.C

The pure rules of the weapon spawners (ae_spawner_rules.h).
*/

#include "ae_spawner_rules.h"

static char const *const stock_words[AE_STOCK_COUNT] =
{
	"assault_rifle", "flamethrower", "gravity_rifle", "needler", "pistol", "plasma_pistol", "plasma_rifle",
	"rocket_launcher", "shotgun", "sniper_rifle", "sword"
};

/* the stock table */
static signed char const stock_class[AE_STOCK_COUNT] =
{
	AE_SPAWNER_AUTOMATIC,	/* assault rifle */
	AE_SPAWNER_HEAVY,	/* flamethrower */
	AE_SPAWNER_HEAVY,	/* gravity rifle (the fuel rod) */
	AE_SPAWNER_AUTOMATIC,	/* needler */
	AE_SPAWNER_PRECISION,	/* pistol: where the battle rifle goes */
	AE_SPAWNER_PISTOL,	/* plasma pistol */
	AE_SPAWNER_AUTOMATIC,	/* plasma rifle */
	AE_SPAWNER_HEAVY,	/* rocket launcher */
	AE_SPAWNER_CLOSE,	/* shotgun */
	AE_SPAWNER_LONG_RANGE,	/* sniper rifle */
	AE_SPAWNER_MELEE	/* sword */
};

static signed char const stock_tier[AE_STOCK_COUNT] =
{
	AE_SPAWNER_BASE,	/* assault rifle */
	AE_SPAWNER_POWER,	/* flamethrower */
	AE_SPAWNER_POWER,	/* gravity rifle */
	AE_SPAWNER_BASE,	/* needler */
	AE_SPAWNER_BASE,	/* pistol */
	AE_SPAWNER_BASE,	/* plasma pistol */
	AE_SPAWNER_BASE,	/* plasma rifle */
	AE_SPAWNER_POWER,	/* rocket launcher */
	AE_SPAWNER_POWER,	/* shotgun: a one-shot kill up close, timed as a power weapon */
	AE_SPAWNER_POWER,	/* sniper rifle */
	AE_SPAWNER_POWER	/* sword */
};

static char const *const class_words[AE_SPAWNER_CLASS_COUNT] =
{
	"PRECISION", "AUTOMATIC", "PISTOL", "CLOSE", "LONG_RANGE", "HEAVY", "MELEE", "ANY"
};

static char const *const tier_words[AE_SPAWNER_TIER_COUNT] = { "BASE", "POWER", "ANY" };

static char const *const logic_words[AE_SPAWNER_LOGIC_COUNT] = { "static", "pickup", "expired", "trigger" };

static int same_word(char const *a, char const *b)
{
	if (!a || !b) return 0;
	for (; *a && *b; a++, b++)
	{
		int ca = *a, cb = *b;
		if (ca >= 'a' && ca <= 'z') ca -= 'a' - 'A';
		if (cb >= 'a' && cb <= 'z') cb -= 'a' - 'A';
		if (ca == '-') ca = '_';
		if (cb == '-') cb = '_';
		if (ca != cb) return 0;
	}
	return *a == 0 && *b == 0;
}

static int word_index(char const *const *words, int count, char const *word)
{
	int index;

	for (index = 0; index < count; index++)
		if (same_word(words[index], word)) return index;
	return -1;
}

int ae_spawner_stock_class(int stock)
{
	return stock >= 0 && stock < AE_STOCK_COUNT ? stock_class[stock] : -1;
}

int ae_spawner_stock_tier(int stock)
{
	return stock >= 0 && stock < AE_STOCK_COUNT ? stock_tier[stock] : -1;
}

int ae_spawner_stock_of_word(char const *word)
{
	return word_index(stock_words, AE_STOCK_COUNT, word);
}

char const *ae_spawner_stock_word(int stock)
{
	return stock >= 0 && stock < AE_STOCK_COUNT ? stock_words[stock] : "";
}

int ae_spawner_tier_of(int weapon_tier, long own_seconds)
{
	if (weapon_tier == AE_SPAWNER_POWER) return AE_SPAWNER_POWER;
	if (own_seconds >= AE_SPAWNER_POWER_OWN_SECONDS) return AE_SPAWNER_POWER;
	return weapon_tier == AE_SPAWNER_BASE ? AE_SPAWNER_BASE : AE_SPAWNER_TIER_ANY;
}

int ae_spawner_class_of_weights(int const weights[AE_SPAWNER_CLASS_COUNT])
{
	int cls, best = AE_SPAWNER_CLASS_ANY, best_weight = 0, tie = 0;

	for (cls = 0; cls < AE_SPAWNER_CLASS_ANY; cls++)
	{
		if (weights[cls] > best_weight)
		{
			best = cls;
			best_weight = weights[cls];
			tie = 0;
		}
		else if (weights[cls] > 0 && weights[cls] == best_weight)
			tie = 1;
	}
	return best_weight > 0 && !tie ? best : AE_SPAWNER_CLASS_ANY;
}

int ae_spawner_merges(real const a[3], real const b[3], unsigned long games_a, unsigned long games_b)
{
	real dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];

	if ((games_a & games_b) == 0) return 0;
	return dx * dx + dy * dy + dz * dz <= AE_SPAWNER_MERGE_RADIUS * AE_SPAWNER_MERGE_RADIUS;
}

int ae_spawner_merged_tier(int tier_a, int tier_b)
{
	if (tier_a == AE_SPAWNER_POWER || tier_b == AE_SPAWNER_POWER) return AE_SPAWNER_POWER;
	if (tier_a == AE_SPAWNER_BASE && tier_b == AE_SPAWNER_BASE) return AE_SPAWNER_BASE;
	return AE_SPAWNER_TIER_ANY;
}

int ae_spawner_default_logic(int cls, int tier)
{
	(void)cls;
	return tier == AE_SPAWNER_POWER ? AE_SPAWNER_STATIC : AE_SPAWNER_PICKUP;
}

long ae_spawner_respawn_seconds(long seconds)
{
	return seconds < AE_SPAWNER_RESPAWN_FLOOR_SECONDS ? AE_SPAWNER_RESPAWN_FLOOR_SECONDS : seconds;
}

real ae_spawner_waypoint_alpha(real distance)
{
	if (distance <= AE_SPAWNER_WAYPOINT_NEAR) return 0.0f;
	if (distance >= AE_SPAWNER_WAYPOINT_FAR) return 1.0f;
	return (distance - AE_SPAWNER_WAYPOINT_NEAR) / (AE_SPAWNER_WAYPOINT_FAR - AE_SPAWNER_WAYPOINT_NEAR);
}

char const *ae_spawner_class_word(int cls)
{
	return cls >= 0 && cls < AE_SPAWNER_CLASS_COUNT ? class_words[cls] : "";
}

char const *ae_spawner_tier_word(int tier)
{
	return tier >= 0 && tier < AE_SPAWNER_TIER_COUNT ? tier_words[tier] : "";
}

char const *ae_spawner_logic_word(int logic)
{
	return logic >= 0 && logic < AE_SPAWNER_LOGIC_COUNT ? logic_words[logic] : "";
}

int ae_spawner_class_of_word(char const *word)
{
	return word_index(class_words, AE_SPAWNER_CLASS_COUNT, word);
}

int ae_spawner_tier_of_word(char const *word)
{
	return word_index(tier_words, AE_SPAWNER_TIER_COUNT, word);
}

int ae_spawner_logic_of_word(char const *word)
{
	return word_index(logic_words, AE_SPAWNER_LOGIC_COUNT, word);
}

int ae_spawner_look_logic(int classic, int logic)
{
	if (classic)
		return AE_SPAWNER_STATIC;
	return logic;
}

int ae_spawner_look_pick(int count, unsigned char const *weights)
{
	int index, best = 0;

	if (count <= 0)
		return -1;
	if (!weights)
		return 0;
	for (index = 1; index < count; index++)
		if (weights[index] > weights[best])
			best = index;
	return best;
}

int ae_spawner_randomize_keeps(int randomize)
{
	return randomize != AE_SPAWNER_RANDOMIZE_EVERY_SPAWN;
}

int ae_spawner_randomize_draws(int randomize, int event)
{
	if (event == AE_SPAWNER_EVENT_GAME_START)
		return 1;
	if (event == AE_SPAWNER_EVENT_ROUND_START)
		return randomize == AE_SPAWNER_RANDOMIZE_EVERY_ROUND;
	if (event == AE_SPAWNER_EVENT_SPAWN)
		return !ae_spawner_randomize_keeps(randomize);
	return 0;
}

int ae_spawner_look_classic(int look)
{
	return look == 1 || look == 2;
}

int ae_spawner_look_holograms(int look)
{
	return look != 1;
}

int ae_spawner_lost_reason(long ammo_taken_at, long now)
{
	if (ammo_taken_at >= 0 && now >= ammo_taken_at && now - ammo_taken_at <= AE_SPAWNER_LOST_WINDOW_TICKS)
		return AE_SPAWNER_LOST_WALKOVER;
	return AE_SPAWNER_LOST_REMOVED;
}

char const *ae_spawner_lost_words(int reason)
{
	return reason == AE_SPAWNER_LOST_WALKOVER ? "a player's walk-over took its ammo, and the game removes a weapon it empties" :
		"removed from the game, not by a player's hand";
}

int ae_spawner_refuses_walkover(int look_classic, int own_fresh_weapon)
{
	return own_fresh_weapon && !look_classic;
}

int ae_spawner_own_seconds(int item_seconds, int collection_seconds)
{
	if (item_seconds > 0)
		return item_seconds;
	return collection_seconds > 0 ? collection_seconds : 0;
}

int ae_spawner_table_current(struct ae_spawner_table_key const *built, struct ae_spawner_table_key const *now)
{
	return built->scenario == now->scenario && built->items_address == now->items_address &&
		built->items_count == now->items_count && built->load == now->load;
}

int ae_spawner_guest_builds(int host_answer, int host_plays, int message_seen)
{
	if (message_seen)
		return 1;
	return host_answer == 1 && host_plays;
}

int ae_spawner_switch_keeps_object(int old_logic, int new_logic)
{
	return !(new_logic == AE_SPAWNER_STATIC && old_logic != AE_SPAWNER_STATIC);
}
