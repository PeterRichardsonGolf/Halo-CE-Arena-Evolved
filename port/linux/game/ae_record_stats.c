/* The numbers a service record keeps beyond games and kills (see the header). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_record_stats.h"

static long positive(long value)
{
	return value > 0 ? value : 0;
}

void ae_rs_game_make(struct ae_rs_game *game, struct ae_rs_input const *in)
{
	struct ae_rs_stats *x;
	struct ae_pgcr_summary const *s = in->summary && in->summary->valid ? in->summary : NULL;

	memset(game, 0, sizeof(*game));
	x = &game->x;
	game->tied = in->tied && !in->won && !in->quit;
	x->seconds = in->quit ? 0 : positive(in->seconds);
	x->fired = positive(in->fired);
	x->hit = positive(in->hit);
	if (x->hit > x->fired)
		x->hit = x->fired;
	switch (in->mode)
	{
	case AE_RS_KIND_FLAG:
		x->flag_grabs = positive(in->objective[0]);
		x->flag_returns = positive(in->objective[1]);
		x->flag_caps = positive(in->objective[2]);
		break;
	case AE_RS_KIND_HILL:
		x->hill_ticks = positive(in->score_value);
		break;
	case AE_RS_KIND_BALL:
		x->ball_ticks = positive(in->score_value);
		x->ball_carrier_kills = positive(in->ball_carrier_kills);
		x->as_ball_carrier = positive(in->as_ball_carrier);
		break;
	case AE_RS_KIND_RACE:
		x->laps = positive(in->laps);
		x->best_lap = positive(in->best_lap_ticks);
		break;
	}
	if (!s)
		return;
	x->best_spree = s->best_spree;
	if (!s->extended)
		return;
	game->have_v3 = 1;
	x->dealt = (long)s->damage_dealt;
	x->taken = (long)s->damage_taken;
	x->headshots = s->headshots;
	x->melee = s->melee;
	x->power = s->power;
	x->grenade = s->grenade;
	x->assassin = s->assassination;
	x->splatter = s->splatter;
	x->stick = s->stick;
	x->as_carrier = s->kills_as_flag_carrier;
	x->carrier_seconds = s->flag_carrier_seconds;
	x->bomb_plants = s->bomb_plants;
	x->bomb_defuses = s->bomb_defuses;
	x->kills_v3 = positive(in->kills);
	x->seconds_v3 = x->seconds;
	x->hit_v3 = x->hit;
}

void ae_rs_add(struct ae_rs_stats *t, struct ae_rs_game const *game)
{
	struct ae_rs_stats const *x = &game->x;

	t->counted++;
	if (game->tied)
		t->ties++;
	if (x->best_spree > t->best_spree)
		t->best_spree = x->best_spree;
	t->seconds += x->seconds;
	t->fired += x->fired;
	t->hit += x->hit;
	t->flag_caps += x->flag_caps;
	t->flag_returns += x->flag_returns;
	t->flag_grabs += x->flag_grabs;
	t->hill_ticks += x->hill_ticks;
	t->ball_ticks += x->ball_ticks;
	t->ball_carrier_kills += x->ball_carrier_kills;
	t->as_ball_carrier += x->as_ball_carrier;
	t->laps += x->laps;
	if (x->best_lap > 0 && (t->best_lap <= 0 || x->best_lap < t->best_lap))
		t->best_lap = x->best_lap;
	if (!game->have_v3)
		return;
	t->counted_v3++;
	t->dealt += x->dealt;
	t->taken += x->taken;
	t->headshots += x->headshots;
	t->melee += x->melee;
	t->power += x->power;
	t->grenade += x->grenade;
	t->assassin += x->assassin;
	t->splatter += x->splatter;
	t->stick += x->stick;
	t->as_carrier += x->as_carrier;
	t->carrier_seconds += x->carrier_seconds;
	t->bomb_plants += x->bomb_plants;
	t->bomb_defuses += x->bomb_defuses;
	t->kills_v3 += x->kills_v3;
	t->seconds_v3 += x->seconds_v3;
	t->hit_v3 += x->hit_v3;
}

/* ---- weapons and maps */

/* a name made safe for the file: "|", ",", ":" and control characters as "?", spaces at the ends gone, cut */
static void safe(char const *name, char *out, size_t size)
{
	size_t at = 0;

	if (!size)
		return;
	while (name && *name == ' ')
		name++;
	for (; name && *name && at + 1 < size; name++)
	{
		unsigned char c = (unsigned char)*name;

		out[at++] = (char)(c == '|' || c == ',' || c == ':' || c < 32 || c == 127 ? '?' : c);
	}
	while (at > 0 && out[at - 1] == ' ')
		at--;
	out[at] = 0;
}

void ae_rs_weapon_add(struct ae_rs_weapon *list, int *count, int limit, char const *name, long kills)
{
	char clean[AE_RS_WEAPON_NAME];
	int index, fewest = 0;

	safe(name, clean, sizeof(clean));
	if (!clean[0] || kills <= 0 || limit <= 0)
		return;
	for (index = 0; index < *count; index++)
		if (!strcmp(list[index].name, clean))
		{
			list[index].kills += kills;
			break;
		}
	if (index == *count)
	{
		if (*count < limit)
			index = (*count)++;
		else
		{
			for (index = 1; index < *count; index++)
				if (list[index].kills < list[fewest].kills)
					fewest = index;
			if (list[fewest].kills >= kills)
				return;
			index = fewest;
		}
		snprintf(list[index].name, sizeof(list[index].name), "%s", clean);
		list[index].kills = kills;
	}
	/* (the most kills first) */
	while (index > 0 && list[index].kills > list[index - 1].kills)
	{
		struct ae_rs_weapon swap = list[index];

		list[index] = list[index - 1];
		list[index - 1] = swap;
		index--;
	}
}

static int same_name(char const *a, char const *b)
{
	for (; *a && *b; a++, b++)
	{
		char x = *a, y = *b;

		if (x >= 'A' && x <= 'Z')
			x = (char)(x + 32);
		if (y >= 'A' && y <= 'Z')
			y = (char)(y + 32);
		if (x != y)
			return 0;
	}
	return *a == *b;
}

void ae_rs_map_add(struct ae_rs_map *list, int *count, int limit, char const *name, int won, long kills, long deaths)
{
	char clean[AE_RS_MAP_NAME];
	struct ae_rs_map *map = NULL;
	int index, fewest = 0;

	safe(name, clean, sizeof(clean));
	if (!clean[0] || limit <= 0)
		return;
	for (index = 0; index < *count; index++)
		if (same_name(list[index].name, clean))
		{
			map = &list[index];
			break;
		}
	if (!map)
	{
		if (*count < limit)
			map = &list[(*count)++];
		else
		{
			for (index = 1; index < *count; index++)
				if (list[index].games < list[fewest].games)
					fewest = index;
			map = &list[fewest];
		}
		memset(map, 0, sizeof(*map));
		snprintf(map->name, sizeof(map->name), "%s", clean);
	}
	map->games++;
	if (won)
		map->wins++;
	map->kills += positive(kills);
	map->deaths += positive(deaths);
}

/* whether map a goes before map b: more games, then more wins, then by name */
static int map_before(struct ae_rs_map const *a, struct ae_rs_map const *b)
{
	return a->games > b->games || (a->games == b->games && (a->wins > b->wins ||
		(a->wins == b->wins && strcmp(a->name, b->name) < 0)));
}

int ae_rs_map_order(struct ae_rs_map const *list, int count, int *out, int most)
{
	int index, taken = 0;

	for (index = 0; index < count; index++)
	{
		int at = taken;

		while (at > 0 && map_before(&list[index], &list[out[at - 1]]))
			at--;
		if (at >= most)
			continue;
		/* (an insertion into a list capped at `most`: the last drops off) */
		if (taken < most)
			taken++;
		memmove(&out[at + 1], &out[at], sizeof(int) * (size_t)(taken - at - 1));
		out[at] = index;
	}
	return taken;
}

/* ---- the file */

/* the numbers in the order of the groups: each group's cells */
static int cells(struct ae_rs_stats *s, long **out, int *group_end)
{
	int n = 0, g = 0;

#define CELL(f) out[n++] = &s->f
#define END() group_end[g++] = n
	CELL(ties); CELL(best_spree); CELL(seconds); END();
	CELL(fired); CELL(hit); END();
	CELL(dealt); CELL(taken); END();
	CELL(headshots); CELL(melee); CELL(power); CELL(grenade); CELL(assassin); CELL(splatter); CELL(stick); END();
	CELL(flag_caps); CELL(flag_returns); CELL(flag_grabs); CELL(as_carrier); CELL(carrier_seconds); END();
	CELL(hill_ticks); CELL(ball_ticks); CELL(ball_carrier_kills); CELL(as_ball_carrier); END();
	CELL(laps); CELL(best_lap); CELL(bomb_plants); CELL(bomb_defuses); END();
	CELL(counted); CELL(counted_v3); CELL(kills_v3); CELL(seconds_v3); CELL(hit_v3); END();
#undef CELL
#undef END
	return g;
}

int ae_rs_stats_write(struct ae_rs_stats const *stats, char *out, size_t size)
{
	struct ae_rs_stats copy = *stats;
	long *cell[48];
	int end[16], groups = cells(&copy, cell, end), g, n = 0, at = 0;

	for (g = 0; g < groups; g++)
	{
		int written = snprintf(out + at, size - (size_t)at, "%s", g ? " | " : "");

		if (written < 0 || (size_t)(at + written) >= size)
			return -1;
		at += written;
		for (; n < end[g]; n++)
		{
			written = snprintf(out + at, size - (size_t)at, "%s%ld", n && n != (g ? end[g - 1] : 0) ? " " : "", *cell[n]);
			if (written < 0 || (size_t)(at + written) >= size)
				return -1;
			at += written;
		}
	}
	return at;
}

int ae_rs_stats_read(char const *text, struct ae_rs_stats *stats)
{
	long *cell[48];
	int end[16], groups, g, n = 0;
	char const *at = text;

	memset(stats, 0, sizeof(*stats));
	groups = cells(stats, cell, end);
	for (g = 0; g < groups && *at; g++)
	{
		int start = g ? end[g - 1] : 0;
		char const *stop = strchr(at, '|');

		n = start;
		for (;;)
		{
			char *next;
			long value;

			while (*at == ' ')
				at++;
			if (!*at || at == stop || *at == '|')
				break;
			value = strtol(at, &next, 10);
			if (next == at || value < 0 || (stop && next > stop))
			{
				memset(stats, 0, sizeof(*stats));
				return 0;
			}
			if (n < end[g])
				*cell[n++] = value;
			at = next;
		}
		if (stop)
			at = stop + 1;
		else
			break;
	}
	return 1;
}

char const *ae_rs_after_groups(char const *text)
{
	int group;

	for (group = 0; group < AE_RS_GROUPS; group++)
	{
		text = strchr(text, '|');
		if (!text)
			return NULL;
		text++;
	}
	while (*text == ' ')
		text++;
	return text;
}

/* ---- the words */

void ae_rs_number_text(long value, char *out, size_t size)
{
	char digits[24], text[40];
	int length, at = 0, index;
	unsigned long magnitude = value < 0 ? (unsigned long)(-value) : (unsigned long)value;

	if (!size)
		return;
	length = snprintf(digits, sizeof(digits), "%lu", magnitude);
	if (value < 0)
		text[at++] = '-';
	for (index = 0; index < length; index++)
	{
		if (index > 0 && (length - index) % 3 == 0)
			text[at++] = ',';
		text[at++] = digits[index];
	}
	text[at] = 0;
	snprintf(out, size, "%s", text);
}

void ae_rs_ratio_text(long ratio100, char *out, size_t size)
{
	snprintf(out, size, "%ld.%02ld", ratio100 / 100, ratio100 % 100);
}

long ae_rs_ratio100(long a, long b)
{
	a = positive(a);
	return b > 0 ? (a * 100 + b / 2) / b : a * 100;
}

long ae_rs_kda100(long kills, long assists, long deaths)
{
	kills = positive(kills);
	assists = positive(assists);
	return deaths > 0 ? ((kills * 3 + assists) * 100 + (deaths * 3) / 2) / (deaths * 3) : kills * 100 + assists * 33;
}

long ae_rs_percent(long part, long whole)
{
	return whole > 0 ? (positive(part) * 100 + whole / 2) / whole : 0;
}

void ae_rs_time_text(long seconds, char *out, size_t size)
{
	long minutes = positive(seconds) / 60, hours = minutes / 60;

	if (hours >= 24)
		snprintf(out, size, "%ldD %ldH", hours / 24, hours % 24);
	else if (hours > 0)
		snprintf(out, size, "%ldH %02ldM", hours, minutes % 60);
	else
		snprintf(out, size, "%ldM", minutes);
}

void ae_rs_clock_text(long ticks, char *out, size_t size)
{
	long seconds = positive(ticks) / AE_PGCR_TICKS_PER_SECOND;

	snprintf(out, size, "%ld:%02ld", seconds / 60, seconds % 60);
}

int ae_rs_known(struct ae_rs_stats const *stats)
{
	return stats->counted > 0;
}

int ae_rs_known_v3(struct ae_rs_stats const *stats)
{
	return stats->counted_v3 > 0;
}

int ae_rs_from_games(long counted, long games, char *out, size_t size)
{
	if (size)
		out[0] = 0;
	if (games <= 0 || counted >= games)
		return 0;
	snprintf(out, size, "FROM %ld %s", counted > 0 ? counted : 0, counted == 1 ? "GAME" : "GAMES");
	return 1;
}

/* ---- DEV MODE's made-up numbers */

static unsigned long demo_next(unsigned long *state)
{
	unsigned long x = *state ? *state : 0x9e3779b9ul;

	x ^= (x << 13) & 0xfffffffful;
	x ^= x >> 17;
	x ^= (x << 5) & 0xfffffffful;
	*state = x & 0xfffffffful;
	return *state;
}

static long demo_between(unsigned long *state, long low, long high)
{
	return low + (long)(demo_next(state) % (unsigned long)(high - low + 1));
}

static char const *const demo_map_names[12] = { "Blood Gulch", "Prisoner", "Damnation", "Hang 'Em High", "Chill Out",
	"Wizard", "Longest", "Derelict", "Boarding Action", "Sidewinder", "Ice Fields", "Danger Canyon" };
static char const *const demo_weapon_names[6] = { "battle rifle", "sniper rifle", "assault rifle", "shotgun", "plasma pistol",
	"rocket launcher" };

void ae_rs_demo(struct ae_rs_stats *x, struct ae_rs_weapon *weapons, int *weapon_count, struct ae_rs_map *maps, int *map_count,
	int map_limit, int map_wanted, long games, long wins, long kills, long deaths, long assists, long counted, long counted_v3,
	unsigned long seed)
{
	unsigned long state = seed * 2654435761ul + 7ul;
	long left, per, index;
	int count;

	memset(x, 0, sizeof(*x));
	if (counted > games)
		counted = games;
	if (counted_v3 > counted)
		counted_v3 = counted;
	if (counted < 0)
		counted = 0;
	if (counted_v3 < 0)
		counted_v3 = 0;
	x->counted = counted;
	x->counted_v3 = counted_v3;
	x->best_spree = counted > 0 ? demo_between(&state, 4, 14) : 0;
	x->seconds = counted * demo_between(&state, 420, 780);
	x->fired = counted * demo_between(&state, 60, 140);
	x->hit = x->fired * demo_between(&state, 28, 46) / 100;
	x->flag_caps = counted > 0 ? 1 + counted * demo_between(&state, 0, 2) / 3 : 0;
	x->flag_returns = counted > 0 ? 1 + counted * demo_between(&state, 0, 4) / 3 : 0;
	x->flag_grabs = x->flag_caps + x->flag_returns + counted / 2;
	x->hill_ticks = counted > 2 ? 30 * demo_between(&state, 40, 300) : 0;
	x->ball_ticks = counted > 1 ? 30 * demo_between(&state, 20, 120) : 0;
	x->laps = counted > 3 ? demo_between(&state, 2, 9) : 0;
	x->best_lap = x->laps ? 30 * demo_between(&state, 40, 90) : 0;
	if (counted_v3 > 0)
	{
		long share = counted_v3 * 100 / (games > 0 ? games : 1);
		long k = kills * share / 100;

		x->kills_v3 = k;
		x->seconds_v3 = x->seconds * counted_v3 / counted;
		x->hit_v3 = x->hit * counted_v3 / counted;
		x->dealt = k * demo_between(&state, 85, 130);
		x->taken = deaths * share / 100 * demo_between(&state, 70, 110) + 1;
		x->headshots = k * demo_between(&state, 25, 45) / 100;
		x->melee = k * demo_between(&state, 3, 10) / 100;
		x->power = k * demo_between(&state, 8, 24) / 100;
		x->grenade = k * demo_between(&state, 3, 9) / 100;
		x->assassin = x->melee / 4;
		x->splatter = demo_between(&state, 0, 2) * counted_v3 / 3;
		x->stick = k * demo_between(&state, 1, 5) / 100;
		x->as_carrier = x->flag_caps * 2;
		x->carrier_seconds = x->flag_caps * 18;
		x->bomb_plants = (long)(counted_v3 > 1 ? demo_between(&state, 1, 6) : 0);
		x->bomb_defuses = x->bomb_plants / 2;
	}
	(void)wins;
	(void)assists;
	/* the weapons: the most kills first, with the kills of the counted games split in falling shares (none before a game counted) */
	*weapon_count = 0;
	for (index = 0; index < AE_RS_WEAPONS && counted > 0; index++)
	{
		long share = (games > 0 ? kills * counted / games : 0) / (2 + index * 2 + (long)(demo_next(&state) % 3));

		if (share > 0)
			ae_rs_weapon_add(weapons, weapon_count, AE_RS_WEAPONS, demo_weapon_names[(index + (long)(seed % 3)) % 6], share);
	}
	/* the maps: map_wanted of them (at most map_limit and the 12 names), the first played the most */
	*map_count = 0;
	count = map_wanted < map_limit ? map_wanted : map_limit;
	if (count > 12)
		count = 12;
	left = counted;
	for (index = 0; index < count && left > 0; index++)
	{
		struct ae_rs_map *map = &maps[(*map_count)++];

		/* (the games spread over the maps left, the first ones the most) */
		per = (left + (count - index) - 1) / (count - index);
		if (per > left)
			per = left;
		memset(map, 0, sizeof(*map));
		snprintf(map->name, sizeof(map->name), "%s", demo_map_names[(index + (long)(seed % 5)) % 12]);
		map->games = per;
		map->wins = per * demo_between(&state, 20, 70) / 100;
		map->kills = kills * per / (games > 0 ? games : 1) * demo_between(&state, 70, 140) / 100;
		map->deaths = deaths * per / (games > 0 ? games : 1) * demo_between(&state, 70, 140) / 100 + 1;
		left -= per;
	}
}

void ae_rs_demo_kinds(long kind[AE_RS_KINDS][5], long games, long wins, long kills, long deaths, long assists,
	unsigned long seed)
{
	ae_rs_demo_kinds_over(kind, games, wins, kills, deaths, assists, seed, 3);
}

void ae_rs_demo_kinds_over(long kind[AE_RS_KINDS][5], long games, long wins, long kills, long deaths, long assists,
	unsigned long seed, int kinds)
{
	unsigned long state = seed * 2654435761ul + 11ul;
	long totals[5], left[5];
	int most = kinds <= 0 ? 3 : kinds > AE_RS_KINDS ? AE_RS_KINDS : kinds;
	int part, field, kinds_used = games >= most ? most : (int)(games > 0 ? games : 1);
	/* (the first kind's share and each next one's: half and 30% over three kinds or fewer, 40% and the rest split over more) */
	long first_share = most > 3 ? 40 : 50, next_share = most > 3 ? 60 / (most - 1) : 30;

	memset(kind, 0, sizeof(long) * AE_RS_KINDS * 5);
	totals[0] = games; totals[1] = wins; totals[2] = kills; totals[3] = deaths; totals[4] = assists;
	memcpy(left, totals, sizeof(left));
	if (games <= 0)
		return;
	for (part = 0; part < kinds_used; part++)
	{
		/* (the kinds' order: SLAYER, FLAG, BALL, then HILL, RACE, BOMB, OTHER) */
		int target = part;

		for (field = 0; field < 5; field++)
		{
			long take = part == kinds_used - 1 ? left[field] : totals[field] * (part == 0 ? first_share : next_share) / 100 +
				demo_between(&state, 0, 1);

			if (take > left[field])
				take = left[field];
			kind[target][field] = take;
			left[field] -= take;
		}
		/* (a kind's wins stay within its games) */
		if (kind[target][1] > kind[target][0])
		{
			left[1] += kind[target][1] - kind[target][0];
			kind[target][1] = kind[target][0];
		}
	}
	/* (any wins left over after the kinds' caps go to the first kind with room) */
	for (part = 0; part < AE_RS_KINDS && left[1] > 0; part++)
	{
		long room = kind[part][0] - kind[part][1];
		long give = room < left[1] ? room : left[1];

		kind[part][1] += give;
		left[1] -= give;
	}
}
