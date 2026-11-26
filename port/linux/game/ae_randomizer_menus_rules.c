/*
AE_RANDOMIZER_MENUS_RULES.C

See ae_randomizer_menus_rules.h.
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "ae_randomizer_menus_rules.h"

#define DOT "\xE2\x80\xA2"
#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

/* ---------- the choices */

static const struct ae_rnd_choice off_on[] = { { "OFF", "false" }, { "ON", "true" } };
static const struct ae_rnd_choice extra[] = { { "OFF", "1" }, { "2X", "2" }, { "4X", "4" }, { "8X", "8" }, { "16X", "16" }, { "32X", "32" } };
static const struct ae_rnd_choice enemy_pools[] = { { "ANY", "any" }, { "COVENANT", "covenant" }, { "FLOOD", "flood" },
	{ "HUMAN", "human" }, { "ONE TYPE", "one" }, { "ITEM SET", "tag_injector" } };
static const struct ae_rnd_choice ally_pools[] = { { "AS ENEMIES", "as_enemies" }, { "ANY", "any" }, { "COVENANT", "covenant" },
	{ "FLOOD", "flood" }, { "HUMAN", "human" }, { "ONE TYPE", "one" }, { "ITEM SET", "tag_injector" } };
static const struct ae_rnd_choice actor_types[] = { { "GRUNT", "grunt" }, { "ELITE", "elite" }, { "JACKAL", "jackal" },
	{ "HUNTER", "hunter" }, { "COMBAT FORM", "flood_combat" }, { "CARRIER FORM", "flood_carrier" }, { "MARINE", "marine" },
	{ "ENGINEER", "engineer" }, { "SENTINEL", "sentinel" }, { "ITEM SET", "tag_injector" } };
static const struct ae_rnd_choice grunt_ranks[] = { { "ANY", "any" }, { "MINOR", "minor" }, { "MAJOR", "major" }, { "SPEC OPS", "specops" } };
static const struct ae_rnd_choice elite_ranks[] = { { "ANY", "any" }, { "MINOR", "minor" }, { "MAJOR", "major" }, { "SPEC OPS", "specops" },
	{ "ZEALOT", "zealot" }, { "STEALTH", "stealth" } };
static const struct ae_rnd_choice two_ranks[] = { { "ANY", "any" }, { "MINOR", "minor" }, { "MAJOR", "major" } };
static const struct ae_rnd_choice flood_forms[] = { { "ANY", "any" }, { "HUMAN", "human" }, { "ELITE", "elite" } };
static const struct ae_rnd_choice weapon_pools[] = { { "ANY", "any" }, { "COVENANT", "covenant" }, { "HUMAN", "human" },
	{ "PRECISION", "precision" }, { "AUTOMATIC", "automatic" }, { "POWER", "power" }, { "ONE TYPE", "one" }, { "ITEM SET", "tag_injector" } };
#define PICKUP_WEAPONS { "PISTOL", "pistol" }, { "ASSAULT RIFLE", "assault_rifle" }, { "SHOTGUN", "shotgun" }, \
	{ "SNIPER RIFLE", "sniper_rifle" }, { "ROCKETS", "rocket_launcher" }, { "FLAMETHROWER", "flamethrower" }, \
	{ "PLASMA PISTOL", "plasma_pistol" }, { "PLASMA RIFLE", "plasma_rifle" }, { "NEEDLER", "needler" }
static const struct ae_rnd_choice pickup_types[] = { PICKUP_WEAPONS, { "ITEM SET", "tag_injector" } };
static const struct ae_rnd_choice weapon_types[] = { PICKUP_WEAPONS, { "FUEL ROD", "fuel_rod" }, { "ENERGY SWORD", "energy_sword" },
	{ "ITEM SET", "tag_injector" } };
static const struct ae_rnd_choice loadouts[] = { { "DEFAULT", "default" }, { "RANDOM", "random" }, { "SET", "set" } };
/* PLAYER LOADOUT's picker, in this order: Content Evolved's nine, AE's weapons (AE WEAPONS IN LEVELS' table: their registry ids,
"ae:<id>"), then the weapons of the item set that plays (slots filled by ae_rnd_loadout_set_weapons, "tag:<hash>"; a slot with no
value is not offered) */
#define STOCK_LOADOUT { "PISTOL", "pistol" }, { "ASSAULT RIFLE", "assault_rifle" }, { "SHOTGUN", "shotgun" }, \
	{ "SNIPER RIFLE", "sniper_rifle" }, { "ROCKETS", "rocket_launcher" }, { "PLASMA PISTOL", "plasma_pistol" }, \
	{ "PLASMA RIFLE", "plasma_rifle" }, { "NEEDLER", "needler" }, { "FLAMETHROWER", "flamethrower" }
#define AE_LOADOUT { "COMBAT AR", "ae:12" }, { "SUPPRESSED AR", "ae:5" }, { "TACTICAL SMG", "ae:15" }, { "SIDEARM", "ae:13" }, \
	{ "HEAVY REVOLVER", "ae:17" }, { "BOLT SNIPER", "ae:9" }, { "BRUTE SHOT", "ae:11" }
static char slot_label[AE_RND_LOADOUT_SET_SLOTS][48], slot_value[AE_RND_LOADOUT_SET_SLOTS][24];
#define SLOT(n) { slot_label[n], slot_value[n] }
#define SLOTS SLOT(0), SLOT(1), SLOT(2), SLOT(3), SLOT(4), SLOT(5), SLOT(6), SLOT(7), SLOT(8), SLOT(9), SLOT(10), SLOT(11), SLOT(12), \
	SLOT(13), SLOT(14), SLOT(15)
static const struct ae_rnd_choice primaries[] = { STOCK_LOADOUT, AE_LOADOUT, SLOTS };
static const struct ae_rnd_choice secondaries[] = { { "NONE", "none" }, STOCK_LOADOUT, AE_LOADOUT, SLOTS };
/* (AE's weapons: the registry ids of ae_campaign_weapons_rules.c's table, in the picker's order) */
static const unsigned int ae_loadout_ids[AE_RND_LOADOUT_AE_COUNT] = { 12, 5, 15, 13, 17, 9, 11 };

#define CH(a) a, COUNT(a)
#define ROW(page, kind, when, label, key, help, choices, weapons) { page, kind, when, label, key, help, choices, weapons }
#define NONE_CHOICES NULL, 0

/* the rows, top to bottom of each page (Content Evolved's pages and their rows' order) */
static const struct ae_rnd_row rows[] =
{
	/* AI REPLACEMENT */
	ROW(AE_RND_PAGE_AI, AE_RND_ROW_CHOICE, AE_RND_WHEN_ALWAYS, "EXTRA ENEMIES", "game.randomizer_extra_enemies",
		"Each enemy squad that many times as large, with co-op's extra enemies on top.", CH(extra), 0),
	ROW(AE_RND_PAGE_AI, AE_RND_ROW_HEAD, AE_RND_WHEN_ALWAYS, "ENEMIES", NULL, "", NONE_CHOICES, 0),
	ROW(AE_RND_PAGE_AI, AE_RND_ROW_SWITCH, AE_RND_WHEN_ALWAYS, "ENEMIES", "game.randomizer_enemies",
		"On: a squad's actors are taken from the pool.", CH(off_on), 0),
	ROW(AE_RND_PAGE_AI, AE_RND_ROW_CHOICE, AE_RND_WHEN_ENEMIES, "ENEMY POOL", "game.randomizer_pool",
		"Which actors a squad may become.", CH(enemy_pools), 0),
	ROW(AE_RND_PAGE_AI, AE_RND_ROW_CHOICE, AE_RND_WHEN_ENEMY_ONE, "ENEMY TYPE", "game.randomizer_type",
		"The only actor an enemy squad may become.", CH(actor_types), 0),
	ROW(AE_RND_PAGE_AI, AE_RND_ROW_CHOICE, AE_RND_WHEN_ENEMY_GRUNT, "RANK", "game.randomizer_grunt_rank", "Which grunts.", CH(grunt_ranks), 0),
	ROW(AE_RND_PAGE_AI, AE_RND_ROW_CHOICE, AE_RND_WHEN_ENEMY_ELITE, "RANK", "game.randomizer_elite_rank",
		"Which elites. Zealots are the gold commanders.", CH(elite_ranks), 0),
	ROW(AE_RND_PAGE_AI, AE_RND_ROW_CHOICE, AE_RND_WHEN_ENEMY_JACKAL, "RANK", "game.randomizer_jackal_rank", "Which jackals.", CH(two_ranks), 0),
	ROW(AE_RND_PAGE_AI, AE_RND_ROW_CHOICE, AE_RND_WHEN_ENEMY_HUNTER, "RANK", "game.randomizer_hunter_rank", "Which hunters.", CH(two_ranks), 0),
	ROW(AE_RND_PAGE_AI, AE_RND_ROW_CHOICE, AE_RND_WHEN_ENEMY_FLOOD, "FORM OF", "game.randomizer_flood_combat_rank",
		"Which combat forms.", CH(flood_forms), 0),
	ROW(AE_RND_PAGE_AI, AE_RND_ROW_CUSTOM, AE_RND_WHEN_ENEMY_CUSTOM, "CUSTOM TYPE", "game.randomizer_custom_type",
		"The item set's actors of this type alone. Left and right choose another.", NONE_CHOICES, 0),
	ROW(AE_RND_PAGE_AI, AE_RND_ROW_HEAD, AE_RND_WHEN_ALWAYS, "ALLIES", NULL, "", NONE_CHOICES, 0),
	ROW(AE_RND_PAGE_AI, AE_RND_ROW_SWITCH, AE_RND_WHEN_ALWAYS, "ALLIES", "game.randomizer_allies",
		"On: the marines beside you are swapped too, once out of any vehicle.", CH(off_on), 0),
	ROW(AE_RND_PAGE_AI, AE_RND_ROW_CHOICE, AE_RND_WHEN_ALLIES, "ALLY POOL", "game.randomizer_ally_pool",
		"Your allies become what the enemy pool gives, ENEMIES on or off.", CH(ally_pools), 0),
	ROW(AE_RND_PAGE_AI, AE_RND_ROW_CHOICE, AE_RND_WHEN_ALLY_ONE, "ALLY TYPE", "game.randomizer_ally_type",
		"The only actor an ally may become.", CH(actor_types), 0),
	ROW(AE_RND_PAGE_AI, AE_RND_ROW_CHOICE, AE_RND_WHEN_ALLY_GRUNT, "RANK", "game.randomizer_ally_grunt_rank", "Which grunts.", CH(grunt_ranks), 0),
	ROW(AE_RND_PAGE_AI, AE_RND_ROW_CHOICE, AE_RND_WHEN_ALLY_ELITE, "RANK", "game.randomizer_ally_elite_rank",
		"Which elites. Zealots are the gold commanders.", CH(elite_ranks), 0),
	ROW(AE_RND_PAGE_AI, AE_RND_ROW_CHOICE, AE_RND_WHEN_ALLY_JACKAL, "RANK", "game.randomizer_ally_jackal_rank", "Which jackals.", CH(two_ranks), 0),
	ROW(AE_RND_PAGE_AI, AE_RND_ROW_CHOICE, AE_RND_WHEN_ALLY_HUNTER, "RANK", "game.randomizer_ally_hunter_rank", "Which hunters.", CH(two_ranks), 0),
	ROW(AE_RND_PAGE_AI, AE_RND_ROW_CHOICE, AE_RND_WHEN_ALLY_FLOOD, "FORM OF", "game.randomizer_ally_flood_combat_rank",
		"Which combat forms.", CH(flood_forms), 0),
	ROW(AE_RND_PAGE_AI, AE_RND_ROW_CUSTOM, AE_RND_WHEN_ALLY_CUSTOM, "CUSTOM TYPE", "game.randomizer_ally_custom_type",
		"The item set's actors of this type alone. Left and right choose another.", NONE_CHOICES, 0),
	/* AI WEAPONS */
	ROW(AE_RND_PAGE_WEAPONS, AE_RND_ROW_SWITCH, AE_RND_WHEN_ALWAYS, "AI WEAPONS", "game.randomizer_enemy_weapons",
		"On: an actor's weapon is swapped for one it can hold; one that can hold none keeps its own.", CH(off_on), 1),
	ROW(AE_RND_PAGE_WEAPONS, AE_RND_ROW_CHOICE, AE_RND_WHEN_WEAPONS, "WEAPON POOL", "game.randomizer_weapon_pool",
		"Which weapons an actor may be given. An actor that can hold none of them keeps its own.", CH(weapon_pools), 1),
	ROW(AE_RND_PAGE_WEAPONS, AE_RND_ROW_CHOICE, AE_RND_WHEN_WEAPON_ONE, "WEAPON TYPE", "game.randomizer_weapon_type",
		"Only enemies that can hold it get it; others keep theirs.", CH(weapon_types), 1),
	ROW(AE_RND_PAGE_WEAPONS, AE_RND_ROW_CUSTOM, AE_RND_WHEN_WEAPON_CUSTOM, "CUSTOM TYPE", "game.randomizer_weapon_custom",
		"The item set's weapons of this type alone, for those that can hold them.", NONE_CHOICES, 1),
	/* WEAPON PICKUPS */
	ROW(AE_RND_PAGE_PICKUPS, AE_RND_ROW_SWITCH, AE_RND_WHEN_ALWAYS, "WEAPON PICKUPS", "game.randomizer_pickups",
		"On: a weapon lying in the level is swapped. One a script names stays.", CH(off_on), 1),
	ROW(AE_RND_PAGE_PICKUPS, AE_RND_ROW_CHOICE, AE_RND_WHEN_PICKUPS, "PICKUP POOL", "game.randomizer_pickup_pool",
		"Which weapons a pickup may become. A weapon the mission lacks is copied from another mission.", CH(weapon_pools), 1),
	ROW(AE_RND_PAGE_PICKUPS, AE_RND_ROW_CHOICE, AE_RND_WHEN_PICKUP_ONE, "PICKUP TYPE", "game.randomizer_pickup_type",
		"The only weapon a pickup may become.", CH(pickup_types), 1),
	ROW(AE_RND_PAGE_PICKUPS, AE_RND_ROW_CUSTOM, AE_RND_WHEN_PICKUP_CUSTOM, "CUSTOM TYPE", "game.randomizer_pickup_custom",
		"The item set's weapons of this type alone. Left and right choose another.", NONE_CHOICES, 1),
	/* PLAYER LOADOUT */
	ROW(AE_RND_PAGE_LOADOUT, AE_RND_ROW_CHOICE, AE_RND_WHEN_ALWAYS, "WEAPONS", "game.randomizer_loadout",
		"DEFAULT: the mission's own. RANDOM: two for each player. SET: the two below.", CH(loadouts), 1),
	ROW(AE_RND_PAGE_LOADOUT, AE_RND_ROW_CHOICE, AE_RND_WHEN_LOADOUT_SET, "PRIMARY", "game.randomizer_loadout_primary",
		"The weapon you start with in hand.", CH(primaries), 1),
	ROW(AE_RND_PAGE_LOADOUT, AE_RND_ROW_CHOICE, AE_RND_WHEN_LOADOUT_SET, "SECONDARY", "game.randomizer_loadout_secondary",
		"The weapon you start with on your back, or none.", CH(secondaries), 1),
};

int ae_rnd_row_count(void)
{
	return COUNT(rows);
}

struct ae_rnd_row const *ae_rnd_row(int index)
{
	return index >= 0 && index < COUNT(rows) ? &rows[index] : NULL;
}

/* ---------- reading */

static const char *get(struct ae_rnd_facts const *facts, const char *key)
{
	const char *value = facts && facts->get ? facts->get(facts->context, key) : NULL;

	return value ? value : "";
}

static int is_true(struct ae_rnd_facts const *facts, const char *key)
{
	return !strcmp(get(facts, key), "true");
}

static int is(struct ae_rnd_facts const *facts, const char *key, const char *value)
{
	return !strcmp(get(facts, key), value);
}

int ae_rnd_row_shown(struct ae_rnd_row const *row, struct ae_rnd_facts const *facts)
{
	int enemies, enemy_one, allies, ally_one;

	if (!row)
		return 0;
	enemies = is_true(facts, "game.randomizer_enemies");
	enemy_one = enemies && is(facts, "game.randomizer_pool", "one");
	allies = is_true(facts, "game.randomizer_allies");
	ally_one = allies && is(facts, "game.randomizer_ally_pool", "one");
	switch (row->when)
	{
	case AE_RND_WHEN_ALWAYS: return 1;
	case AE_RND_WHEN_ENEMIES: return enemies;
	case AE_RND_WHEN_ENEMY_ONE: return enemy_one;
	case AE_RND_WHEN_ENEMY_GRUNT: return enemy_one && is(facts, "game.randomizer_type", "grunt");
	case AE_RND_WHEN_ENEMY_ELITE: return enemy_one && is(facts, "game.randomizer_type", "elite");
	case AE_RND_WHEN_ENEMY_JACKAL: return enemy_one && is(facts, "game.randomizer_type", "jackal");
	case AE_RND_WHEN_ENEMY_HUNTER: return enemy_one && is(facts, "game.randomizer_type", "hunter");
	case AE_RND_WHEN_ENEMY_FLOOD: return enemy_one && is(facts, "game.randomizer_type", "flood_combat");
	case AE_RND_WHEN_ENEMY_CUSTOM: return enemy_one && is(facts, "game.randomizer_type", "tag_injector");
	case AE_RND_WHEN_ALLIES: return allies;
	case AE_RND_WHEN_ALLY_ONE: return ally_one;
	case AE_RND_WHEN_ALLY_GRUNT: return ally_one && is(facts, "game.randomizer_ally_type", "grunt");
	case AE_RND_WHEN_ALLY_ELITE: return ally_one && is(facts, "game.randomizer_ally_type", "elite");
	case AE_RND_WHEN_ALLY_JACKAL: return ally_one && is(facts, "game.randomizer_ally_type", "jackal");
	case AE_RND_WHEN_ALLY_HUNTER: return ally_one && is(facts, "game.randomizer_ally_type", "hunter");
	case AE_RND_WHEN_ALLY_FLOOD: return ally_one && is(facts, "game.randomizer_ally_type", "flood_combat");
	case AE_RND_WHEN_ALLY_CUSTOM: return ally_one && is(facts, "game.randomizer_ally_type", "tag_injector");
	case AE_RND_WHEN_WEAPONS: return is_true(facts, "game.randomizer_enemy_weapons");
	case AE_RND_WHEN_WEAPON_ONE: return is_true(facts, "game.randomizer_enemy_weapons") && is(facts, "game.randomizer_weapon_pool", "one");
	case AE_RND_WHEN_WEAPON_CUSTOM: return is_true(facts, "game.randomizer_enemy_weapons") && is(facts, "game.randomizer_weapon_pool", "one") &&
		is(facts, "game.randomizer_weapon_type", "tag_injector");
	case AE_RND_WHEN_PICKUPS: return is_true(facts, "game.randomizer_pickups");
	case AE_RND_WHEN_PICKUP_ONE: return is_true(facts, "game.randomizer_pickups") && is(facts, "game.randomizer_pickup_pool", "one");
	case AE_RND_WHEN_PICKUP_CUSTOM: return is_true(facts, "game.randomizer_pickups") && is(facts, "game.randomizer_pickup_pool", "one") &&
		is(facts, "game.randomizer_pickup_type", "tag_injector");
	case AE_RND_WHEN_LOADOUT_SET: return is(facts, "game.randomizer_loadout", "set");
	}
	return 0;
}

int ae_rnd_choice_offered(struct ae_rnd_row const *row, int choice, struct ae_rnd_facts const *facts)
{
	const char *value;

	if (!row || choice < 0 || choice >= row->choice_count)
		return 0;
	value = row->choices[choice].value;
	if (!value[0])
		return 0;
	if (!strcmp(value, "tag_injector"))
		return (row->weapons ? facts->weapon_types : facts->actor_types) > 0;
	if (!strcmp(value, "engineer") || !strcmp(value, "sentinel"))
		return is_true(facts, "game.randomizer_crazy") && is_true(facts, "game.randomizer_crazy_enemies");
	if (!strcmp(value, "flamethrower"))
		return is_true(facts, "game.randomizer_crazy") && is_true(facts, "game.randomizer_crazy_flamethrower");
	return 1;
}

int ae_rnd_choice_index(struct ae_rnd_row const *row, const char *value)
{
	int each;

	for (each = 0; row && value && each < row->choice_count; each++)
		if (!strcmp(row->choices[each].value, value))
			return each;
	return 0;
}

int ae_rnd_choice_step(struct ae_rnd_row const *row, int index, int direction, struct ae_rnd_facts const *facts)
{
	int tries, next = index;

	if (!row || row->choice_count < 1)
		return index;
	for (tries = 0; tries < row->choice_count; tries++)
	{
		next = (next + direction + row->choice_count) % row->choice_count;
		if (ae_rnd_choice_offered(row, next, facts))
			return next;
	}
	return index;
}

const char *ae_rnd_row_value_text(struct ae_rnd_row const *row, struct ae_rnd_facts const *facts)
{
	char const *value;
	int each;

	if (!row || !row->choices)
		return "";
	value = get(facts, row->key);
	/* (a PLAYER LOADOUT weapon of AE's or of a set that no longer plays: the level's own weapon is what a game gets) */
	if (ae_rnd_loadout_value_kind(value) == AE_RND_LOADOUT_VALUE_AE || ae_rnd_loadout_value_kind(value) == AE_RND_LOADOUT_VALUE_SET)
	{
		for (each = 0; each < row->choice_count; each++)
			if (row->choices[each].value[0] && !strcmp(row->choices[each].value, value))
				return row->choices[each].label;
		return "NOT AVAILABLE";
	}
	return row->choices[ae_rnd_choice_index(row, value)].label;
}

int ae_rnd_loadout_value_kind(char const *value)
{
	if (!value || !value[0])
		return AE_RND_LOADOUT_VALUE_UNKNOWN;
	if (!strcmp(value, "none"))
		return AE_RND_LOADOUT_VALUE_NONE;
	if (!strncmp(value, "ae:", 3))
		return AE_RND_LOADOUT_VALUE_AE;
	if (!strncmp(value, "tag:", 4))
		return AE_RND_LOADOUT_VALUE_SET;
	return AE_RND_LOADOUT_VALUE_STOCK;
}

unsigned int ae_rnd_loadout_ae_id(char const *value)
{
	unsigned int id = 0;
	int each;

	if (ae_rnd_loadout_value_kind(value) != AE_RND_LOADOUT_VALUE_AE || !value[3])
		return 0;
	for (each = 3; value[each]; each++)
	{
		if (value[each] < '0' || value[each] > '9' || id > 100000u)
			return 0;
		id = id * 10u + (unsigned int)(value[each] - '0');
	}
	for (each = 0; each < AE_RND_LOADOUT_AE_COUNT; each++)
		if (ae_loadout_ids[each] == id)
			return id;
	return 0;
}

unsigned int ae_rnd_loadout_ae_id_at(int index)
{
	return index >= 0 && index < AE_RND_LOADOUT_AE_COUNT ? ae_loadout_ids[index] : 0;
}

void ae_rnd_loadout_set_weapons(char const labels[][48], char const values[][24], int count)
{
	int each;

	for (each = 0; each < AE_RND_LOADOUT_SET_SLOTS; each++)
	{
		slot_label[each][0] = 0;
		slot_value[each][0] = 0;
		if (each < count && values[each][0])
		{
			snprintf(slot_label[each], sizeof(slot_label[each]), "%s", labels[each]);
			snprintf(slot_value[each], sizeof(slot_value[each]), "%s", values[each]);
		}
	}
}

int ae_rnd_loadout_primary_missing(char const *requested, int found)
{
	int kind = ae_rnd_loadout_value_kind(requested);

	return !found && (kind == AE_RND_LOADOUT_VALUE_AE || kind == AE_RND_LOADOUT_VALUE_SET);
}

/* ---------- the hub */

static long extra_enemies(struct ae_rnd_facts const *facts)
{
	long value = 0;
	const char *text = get(facts, "game.randomizer_extra_enemies");

	while (*text >= '0' && *text <= '9')
		value = value * 10 + (*text++ - '0');
	return value < 1 ? 1 : value > 32 ? 32 : value;
}

void ae_rnd_ai_summary(struct ae_rnd_facts const *facts, char *text, size_t size)
{
	int enemies = is_true(facts, "game.randomizer_enemies"), allies = is_true(facts, "game.randomizer_allies");
	long extra_times = extra_enemies(facts);

	if (extra_times > 1)
		snprintf(text, size, "%s " DOT " %ldX", enemies && allies ? "ALL" : enemies ? "ENEMIES" : allies ? "ALLIES" : "OFF", extra_times);
	else
		snprintf(text, size, "%s", enemies && allies ? "ALL" : enemies ? "ENEMIES" : allies ? "ALLIES" : "OFF");
}

void ae_rnd_weapon_summary(struct ae_rnd_facts const *facts, int pickups, char *text, size_t size)
{
	const char *on = pickups ? "game.randomizer_pickups" : "game.randomizer_enemy_weapons";
	const char *pool = pickups ? "game.randomizer_pickup_pool" : "game.randomizer_weapon_pool";
	const char *type = pickups ? "game.randomizer_pickup_type" : "game.randomizer_weapon_type";
	int each, found = 0;

	if (!is_true(facts, on))
	{
		snprintf(text, size, "OFF");
		return;
	}
	if (!is(facts, pool, "one"))
	{
		snprintf(text, size, "ON");
		return;
	}
	for (each = 0; each < COUNT(rows); each++)
	{
		if (rows[each].key && !strcmp(rows[each].key, type))
		{
			snprintf(text, size, "ONE TYPE: %s", rows[each].choices[ae_rnd_choice_index(&rows[each], get(facts, type))].label);
			found = 1;
			break;
		}
	}
	if (!found)
		snprintf(text, size, "ON");
}

const char *ae_rnd_loadout_summary(struct ae_rnd_facts const *facts)
{
	return is(facts, "game.randomizer_loadout", "random") ? "RANDOM" : is(facts, "game.randomizer_loadout", "set") ? "SET" : "DEFAULT";
}

void ae_rnd_lobby_summary(struct ae_rnd_facts const *facts, char *text, size_t size)
{
	char parts[5][16];
	int count = 0, each, enemies = is_true(facts, "game.randomizer_enemies"), allies = is_true(facts, "game.randomizer_allies");
	size_t length;

	if (!is_true(facts, "game.randomizer"))
	{
		snprintf(text, size, "OFF");
		return;
	}
	if (enemies || allies)
		snprintf(parts[count++], sizeof(parts[0]), "%s", enemies && allies ? "ALL" : enemies ? "ENEMIES" : "ALLIES");
	if (is_true(facts, "game.randomizer_enemy_weapons"))
		snprintf(parts[count++], sizeof(parts[0]), "WEAPONS");
	if (is_true(facts, "game.randomizer_pickups"))
		snprintf(parts[count++], sizeof(parts[0]), "PICKUPS");
	if (!is(facts, "game.randomizer_loadout", "default") && get(facts, "game.randomizer_loadout")[0])
		snprintf(parts[count++], sizeof(parts[0]), "LOADOUT");
	if (extra_enemies(facts) > 1)
		snprintf(parts[count++], sizeof(parts[0]), "2X");
	if (count == 0)
	{
		snprintf(text, size, "ON");
		return;
	}
	if (count > 3)
	{
		snprintf(text, size, "%d CHANGES", count);
		return;
	}
	text[0] = 0;
	length = 0;
	for (each = 0; each < count; each++)
		length += (size_t)snprintf(text + length, length < size ? size - length : 0, "%s%s", each ? " " DOT " " : "", parts[each]);
}

/* ---------- the settings these screens write */

static const char *const extra_keys[] =
{
	"game.randomizer", "game.randomizer_crazy", "game.randomizer_crazy_weapons", "game.randomizer_crazy_enemies",
	"game.randomizer_crazy_flamethrower", "game.randomizer_crazy_pods", "game.randomizer_tag_profile", "game.tag_injection",
	"game.randomizer_profile_1", "game.randomizer_profile_2", "game.randomizer_profile_3", "game.randomizer_profile_4",
	"game.randomizer_profile_5", "game.randomizer_profile_6", "game.randomizer_profile_7", "game.randomizer_profile_8",
};

int ae_randomizer_menus_key(const char *key)
{
	int each;

	if (!key)
		return 0;
	for (each = 0; each < COUNT(rows); each++)
		if (rows[each].key && !strcmp(rows[each].key, key))
			return 1;
	for (each = 0; each < COUNT(extra_keys); each++)
		if (!strcmp(extra_keys[each], key))
			return 1;
	return 0;
}

/* ---------- the saved setups */

static const char *const setup_keys[AE_RND_SETUP_KEYS] =
{
	"game.randomizer_enemies", "game.randomizer_allies", "game.randomizer_pool", "game.randomizer_type", "game.randomizer_grunt_rank",
	"game.randomizer_elite_rank", "game.randomizer_jackal_rank", "game.randomizer_hunter_rank", "game.randomizer_flood_combat_rank",
	"game.randomizer_pickups", "game.randomizer_pickup_pool", "game.randomizer_pickup_type", "game.randomizer_enemy_weapons",
	"game.randomizer_weapon_pool", "game.randomizer_weapon_type", "", "game.randomizer_custom_type", "game.randomizer_pickup_custom",
	"game.randomizer_weapon_custom", "game.randomizer_extra_enemies", "game.randomizer_loadout", "game.randomizer_loadout_primary",
	"game.randomizer_loadout_secondary", "game.randomizer_ally_pool", "game.randomizer_ally_type", "game.randomizer_ally_custom_type",
	"game.randomizer_ally_grunt_rank", "game.randomizer_ally_elite_rank", "game.randomizer_ally_jackal_rank",
	"game.randomizer_ally_hunter_rank", "game.randomizer_ally_flood_combat_rank",
};

const char *ae_rnd_setup_key(int index)
{
	return index >= 0 && index < AE_RND_SETUP_KEYS ? setup_keys[index] : NULL;
}

int ae_rnd_setup_name_clean(char *name)
{
	char *read = name, *write = name;

	while (*read == ' ')
		read++;
	for (; *read; read++)
	{
		char c = *read;

		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == ' ' || c == '_' || c == '-')
			*write++ = c;
	}
	while (write > name && write[-1] == ' ')
		write--;
	*write = 0;
	return write != name;
}

size_t ae_rnd_setup_format(struct ae_rnd_setup const *setup, char *text, size_t size)
{
	size_t length;
	int key;

	length = (size_t)snprintf(text, size, "%s", setup->name);
	for (key = 0; key < AE_RND_SETUP_KEYS; key++)
	{
		if (length >= size)
			return 0;
		length += (size_t)snprintf(text + length, size - length, ";%s", setup->value[key]);
	}
	return length < size ? length : 0;
}

/* text copied to fit, cut when too long */
static void copy(char *to, size_t size, const char *from)
{
	size_t length = strlen(from);

	if (length >= size)
		length = size - 1;
	memcpy(to, from, length);
	to[length] = 0;
}

int ae_rnd_setup_parse(const char *text, struct ae_rnd_setup *setup, const char *(*defaults)(const char *key))
{
	char buffer[AE_RND_SETUP_NAME + AE_RND_SETUP_KEYS * (AE_RND_SETUP_VALUE + 1) + 8];
	char *field, *next;
	int key;

	memset(setup, 0, sizeof(*setup));
	if (!text || !text[0])
		return 0;
	snprintf(buffer, sizeof(buffer), "%s", text);
	next = strchr(buffer, ';');
	if (next)
		*next++ = 0;
	if (!buffer[0])
		return 0;
	copy(setup->name, sizeof(setup->name), buffer);
	for (key = 0; key < AE_RND_SETUP_KEYS; key++)
	{
		field = next;
		next = field ? strchr(field, ';') : NULL;
		if (next)
			*next++ = 0;
		if (!setup_keys[key][0])
			setup->value[key][0] = 0;
		else if (!field || !field[0])
		{
			const char *standard = defaults ? defaults(setup_keys[key]) : NULL;

			copy(setup->value[key], sizeof(setup->value[key]), standard ? standard : "");
		}
		else
			copy(setup->value[key], sizeof(setup->value[key]), field);
	}
	return 1;
}

static int same_name(const char *a, const char *b)
{
	for (; *a && *b; a++, b++)
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	return *a == *b;
}

int ae_rnd_setups_keep(struct ae_rnd_setups *setups, struct ae_rnd_setup const *setup)
{
	int index;

	for (index = 0; index < setups->count; index++)
		if (same_name(setups->setup[index].name, setup->name))
			break;
	if (index == setups->count)
	{
		if (setups->count == AE_RND_SETUPS)
		{
			memmove(&setups->setup[0], &setups->setup[1], sizeof(setups->setup[0]) * (AE_RND_SETUPS - 1));
			index = AE_RND_SETUPS - 1;
		}
		else
			index = setups->count++;
	}
	setups->setup[index] = *setup;
	return index;
}

int ae_rnd_setups_remove(struct ae_rnd_setups *setups, int index)
{
	if (index < 0 || index >= setups->count)
		return 0;
	memmove(&setups->setup[index], &setups->setup[index + 1], sizeof(setups->setup[0]) * (size_t)(setups->count - index - 1));
	setups->count--;
	memset(&setups->setup[setups->count], 0, sizeof(setups->setup[0]));
	return 1;
}

int ae_rnd_setups_would_drop(struct ae_rnd_setups const *setups, const char *name)
{
	int index;

	if (setups->count < AE_RND_SETUPS)
		return -1;
	for (index = 0; name && index < setups->count; index++)
		if (same_name(setups->setup[index].name, name))
			return -1;
	return 0;
}

const char *ae_rnd_setup_value(struct ae_rnd_setup const *setup, const char *key)
{
	int each;

	for (each = 0; key && each < AE_RND_SETUP_KEYS; each++)
		if (setup_keys[each][0] && !strcmp(setup_keys[each], key))
			return setup->value[each];
	return NULL;
}
