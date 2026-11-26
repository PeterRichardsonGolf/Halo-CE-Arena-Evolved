/*
AE_BOT_RECORD_DEMO.C

DEV MODE's made-up bot records (ae_bot_record_demo.h).
*/

#include "ae_bot_record_demo.h"
#include "ae_bots_style_rules.h"
#include "ae_social_names_rules.h"

#include <stdio.h>
#include <string.h>

static char const *const demo_playlists[4] = { "team-slayer", "free-for-all", "doubles", "big-team" };
static char const *const demo_maps[6] = { "prisoner", "hangemhigh", "chillout", "bloodgulch", "wizard", "damnation" };
static char const *const demo_weapons[6] = { "BATTLE RIFLE", "ASSAULT RIFLE", "SNIPER RIFLE", "SHOTGUN", "FRAG GRENADE",
	"PLASMA PISTOL" };

static unsigned long next(unsigned long *state)
{
	unsigned long x = *state ? *state : 0x9e3779b9ul;

	x ^= (x << 13) & 0xfffffffful;
	x ^= x >> 17;
	x ^= (x << 5) & 0xfffffffful;
	*state = x & 0xfffffffful;
	return *state;
}

static long between(unsigned long *state, long low, long high)
{
	return low + (long)(next(state) % (unsigned long)(high - low + 1));
}

/* a bot's versus line with profile: index 0 a RIVAL, 1 a TEAMMATE, 2 a FIRST MEETING, the rest a spread of leads */
static void demo_versus(struct ae_bot_records *records, struct ae_bot_record const *bot, char const *profile, int index,
	unsigned long *state)
{
	struct ae_bot_versus *versus;

	if (records->versus_count >= AE_BOT_RECORD_VERSUS)
		return;
	versus = &records->versus[records->versus_count++];
	memset(versus, 0, sizeof(*versus));
	ae_bot_record_key(profile, versus->profile, sizeof(versus->profile));
	/* (the same size: a whole copy, never a format over the same table) */
	memcpy(versus->bot, bot->key, sizeof(versus->bot));
	if (index == 1)
	{
		versus->with = bot->games < 3 ? bot->games : 3;
		versus->with_won = versus->with > 0 ? versus->with - 1 : 0;
	}
	else if (index == 2)
	{
		versus->against = 1;
		versus->it_won = 1;
	}
	else
	{
		versus->against = index == 0 ? 14 : between(state, 1, bot->games > 1 ? bot->games - 1 : 1);
		versus->with = bot->games - versus->against > 0 ? bot->games - versus->against : 0;
		if (index == 0)
		{
			versus->you_won = 6;
			versus->it_won = 8;
		}
		else
		{
			versus->you_won = between(state, 0, versus->against);
			versus->it_won = between(state, 0, versus->against - versus->you_won);
		}
		versus->with_won = versus->with ? between(state, 0, versus->with) : 0;
	}
	versus->its_kills = (versus->against + versus->with) * between(state, 1, 3);
	versus->your_kills = (versus->against + versus->with) * between(state, 1, 3);
	versus->last = bot->last;
	versus->lastgame.has = 1;
	snprintf(versus->lastgame.playlist, sizeof(versus->lastgame.playlist), "%s", demo_playlists[next(state) % 4]);
	snprintf(versus->lastgame.map, sizeof(versus->lastgame.map), "%s", demo_maps[next(state) % 6]);
	versus->lastgame.result = (int)(next(state) % 3);
	versus->lastgame.kills = between(state, 2, 20);
	versus->lastgame.deaths = between(state, 2, 18);
	versus->lastgame.assists = between(state, 0, 9);
	versus->lastgame.spree = between(state, 1, 9);
	versus->lastgame.its_kills = between(state, 0, 6);
	versus->lastgame.place = strcmp(versus->lastgame.playlist, "free-for-all") ? -1 : (int)between(state, 0, 7);
}

int ae_bot_records_demo(struct ae_bot_records *records, unsigned long seed, int bots, char const *profile)
{
	int picked[AE_BOT_RECORD_DEMO_MOST];
	unsigned long state = seed * 2654435761ul + 1ul;
	int index, count;

	ae_bot_records_init(records);
	if (bots <= 0)
		bots = AE_BOT_RECORD_DEMO_BOTS;
	if (bots > AE_BOT_RECORD_DEMO_MOST)
		bots = AE_BOT_RECORD_DEMO_MOST;
	count = ae_social_names_pick(seed, bots, NULL, 0, picked);
	records->clock = 200;
	for (index = 0; index < count; index++)
	{
		struct ae_bot_record *bot = &records->bot[records->count++];
		char const *name = ae_social_names_get(picked[index]);
		struct ae_bots_style style;
		int weapon, first = (int)(next(&state) % 6);

		memset(bot, 0, sizeof(*bot));
		ae_bot_record_key(name, bot->key, sizeof(bot->key));
		snprintf(bot->name, sizeof(bot->name), "%s", name ? name : "?");
		/* (the most games first, for a list that shows its order at once) */
		bot->games = index == 0 ? 23 : index == 2 ? 1 : 2 + (bots - index) * 20 / bots + between(&state, 0, 2);
		bot->wins = between(&state, 0, bot->games);
		bot->ties = bot->games - bot->wins > 0 ? between(&state, 0, (bot->games - bot->wins) / 5) : 0;
		bot->kills = bot->games * between(&state, 6, 16);
		bot->deaths = bot->games * between(&state, 6, 16);
		bot->assists = bot->games * between(&state, 1, 5);
		bot->best_spree = between(&state, 2, 12);
		bot->last = records->clock - index * 3 - between(&state, 0, 2);
		bot->first = bot->last - bot->games * 2 > 0 ? bot->last - bot->games * 2 : 1;
		snprintf(bot->first_playlist, sizeof(bot->first_playlist), "%s", demo_playlists[next(&state) % 4]);
		bot->shooting = (int)between(&state, 2, 6);
		bot->movement = (int)between(&state, 2, 6);
		bot->tier_low = (int)between(&state, 3, 12);
		bot->tier_high = bot->tier_low + (int)between(&state, 0, 4);
		ae_bots_style_from_hash(ae_social_name_hash(name), &style);
		bot->style = style.preset;
		bot->favourite = style.weapon;
		bot->rank = AE_BOT_RECORD_NO_RANK;
		bot->weapon_count = (int)between(&state, 2, AE_BOT_RECORD_WEAPONS);
		for (weapon = 0; weapon < bot->weapon_count; weapon++)
		{
			snprintf(bot->weapons[weapon].name, sizeof(bot->weapons[weapon].name), "%s", demo_weapons[(first + weapon) % 6]);
			bot->weapons[weapon].kills = bot->kills / (2 + weapon * 2);
		}
		{
			/* (rc2: the shared numbers all there; the fourth bot a record from before, 2 games counted, none with damage;
			the kinds, medals and maps in step) */
			long kind[AE_RS_KINDS][5];
			struct ae_rs_weapon weapons[AE_RS_WEAPONS];
			int weapon_count, kinds, medal;

			ae_rs_demo(&bot->x, weapons, &weapon_count, bot->map, &bot->map_count, AE_BOT_RECORD_MAPS,
				(int)between(&state, 5, 9), bot->games, bot->wins, bot->kills, bot->deaths, bot->assists,
				index == 3 ? 2 : bot->games, index == 3 ? 0 : bot->games, seed + (unsigned long)index);
			bot->x.ties = 0;
			ae_rs_demo_kinds(kind, bot->games, bot->wins, bot->kills, bot->deaths, bot->assists, seed + (unsigned long)index);
			for (kinds = 0; kinds < AE_BOT_RECORD_KINDS; kinds++)
			{
				bot->kind[kinds].games = kind[kinds][0];
				bot->kind[kinds].wins = kind[kinds][1];
				bot->kind[kinds].kills = kind[kinds][2];
				bot->kind[kinds].deaths = kind[kinds][3];
				bot->kind[kinds].assists = kind[kinds][4];
			}
			for (medal = 0; medal < 12; medal++)
				bot->medals[(seed * 7 + (unsigned long)medal * 9 + (unsigned long)index) % AE_MEDAL_COUNT] =
					(unsigned long)between(&state, 1, 40);
		}
		if (profile && profile[0])
			demo_versus(records, bot, profile, index, &state);
	}
	return count;
}

int ae_bot_records_demo_set(struct ae_bot_records *records, char const *name, char const *profile, char const *field,
	long value, long value2)
{
	struct ae_bot_record *bot;
	struct ae_bot_versus *versus = NULL;
	char key[AE_BOT_RECORD_KEY];

	ae_bot_record_key(name, key, sizeof(key));
	if (!key[0] || value < 0 || value2 < 0 || !field)
		return 0;
	bot = ae_bot_record_find(records, key);
	if (!bot)
	{
		if (records->count >= AE_BOT_RECORD_BOTS)
			return 0;
		bot = &records->bot[records->count++];
		memset(bot, 0, sizeof(*bot));
		snprintf(bot->key, sizeof(bot->key), "%s", key);
		snprintf(bot->name, sizeof(bot->name), "%s", name);
		bot->style = -1;
		bot->rank = AE_BOT_RECORD_NO_RANK;
		bot->first = bot->last = records->clock;
	}
	if (!strcmp(field, "games"))
		bot->games = value;
	else if (!strcmp(field, "wins"))
		bot->wins = value;
	else if (!strcmp(field, "ties"))
		bot->ties = value;
	else if (!strcmp(field, "kills"))
		bot->kills = value;
	else if (!strcmp(field, "deaths"))
		bot->deaths = value;
	else if (!strcmp(field, "with") || !strcmp(field, "vs"))
	{
		char profile_key[AE_BOT_RECORD_KEY];

		ae_bot_record_key(profile, profile_key, sizeof(profile_key));
		if (!profile_key[0])
			return 0;
		versus = ae_bot_record_versus(records, profile_key, key);
		if (!versus)
		{
			if (records->versus_count >= AE_BOT_RECORD_VERSUS)
				return 0;
			versus = &records->versus[records->versus_count++];
			memset(versus, 0, sizeof(*versus));
			snprintf(versus->profile, sizeof(versus->profile), "%s", profile_key);
			snprintf(versus->bot, sizeof(versus->bot), "%s", key);
			versus->last = bot->last;
		}
		if (!strcmp(field, "with"))
			versus->with = value;
		else
		{
			versus->you_won = value;
			versus->it_won = value2;
			if (versus->against < value + value2)
				versus->against = value + value2;
		}
	}
	else if (!strcmp(field, "counted"))
		bot->x.counted = value;
	else if (!strcmp(field, "counted_v3"))
		bot->x.counted_v3 = value;
	else if (!strcmp(field, "seconds"))
		bot->x.seconds = bot->x.seconds_v3 = value;
	else if (!strcmp(field, "caps"))
		bot->x.flag_caps = value;
	else if (!strcmp(field, "returns"))
		bot->x.flag_returns = value;
	else if (!strcmp(field, "grabs"))
		bot->x.flag_grabs = value;
	else if (!strcmp(field, "carrier"))
		bot->x.as_carrier = value;
	else if (!strcmp(field, "hill"))
		bot->x.hill_ticks = value * 30;
	else if (!strcmp(field, "ball"))
		bot->x.ball_ticks = value * 30;
	else if (!strcmp(field, "plants"))
		bot->x.bomb_plants = value;
	else if (!strcmp(field, "defuses"))
		bot->x.bomb_defuses = value;
	else if (!strcmp(field, "laps"))
		bot->x.laps = value;
	else
		return 0;
	/* (the totals stay whole: wins and ties within the games) */
	if (bot->games < bot->wins + bot->ties)
		bot->games = bot->wins + bot->ties;
	if (versus && versus->with_won > versus->with)
		versus->with_won = versus->with;
	return 1;
}
