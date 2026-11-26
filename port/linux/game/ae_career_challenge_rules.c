/* The daily and weekly challenges' rules (see the header). Pure code. */
#include <stdio.h>
#include <string.h>

#include "ae_career_challenge_rules.h"

#define D AE_CH_DAILY
#define W AE_CH_WEEKLY

static struct ae_ch_kind const kinds[] =
{
	{ "d_play3",     D, AE_CH_GAMES,           AE_CH_ANY_KIND,        3,   225 },
	{ "d_win2",      D, AE_CH_WINS,            AE_CH_ANY_KIND,        2,   300 },
	{ "d_kills40",   D, AE_CH_KILLS,           AE_CH_ANY_KIND,        40,  225 },
	{ "d_slay25",    D, AE_CH_KILLS,           AE_CH_SLAYER_ONLY,     25,  225 },
	{ "d_medals10",  D, AE_CH_MEDALS,          AE_CH_ANY_KIND,        10,  225 },
	{ "d_objplay2",  D, AE_CH_GAMES,           AE_CH_OBJECTIVE_ONLY,  2,   300 },
	{ "d_assist10",  D, AE_CH_ASSISTS,         AE_CH_ANY_KIND,        10,  225 },
	{ "d_camp1",     D, AE_CH_CAMPAIGN_LEVELS, AE_CH_ANY_KIND,        1,   450 },
	{ "d_time30",    D, AE_CH_MINUTES,         AE_CH_ANY_KIND,        30,  225 },
	{ "d_obj6",      D, AE_CH_OBJECTIVES,      AE_CH_ANY_KIND,        6,   300 },
	{ "w_win10",     W, AE_CH_WINS,            AE_CH_ANY_KIND,        10,  900 },
	{ "w_kills300",  W, AE_CH_KILLS,           AE_CH_ANY_KIND,        300, 900 },
	{ "w_medals60",  W, AE_CH_MEDALS,          AE_CH_ANY_KIND,        60,  750 },
	{ "w_play15",    W, AE_CH_GAMES,           AE_CH_ANY_KIND,        15,  750 },
	{ "w_objwin3",   W, AE_CH_WINS,            AE_CH_OBJECTIVE_ONLY,  3,   900 },
	{ "w_camp3",     W, AE_CH_CAMPAIGN_LEVELS, AE_CH_ANY_KIND,        3,   1200 },
	{ "w_time180",   W, AE_CH_MINUTES,         AE_CH_ANY_KIND,        180, 900 },
	{ "w_slaywin5",  W, AE_CH_WINS,            AE_CH_SLAYER_ONLY,     5,   750 },
	{ "w_assist60",  W, AE_CH_ASSISTS,         AE_CH_ANY_KIND,        60,  750 },
	{ "w_obj30",     W, AE_CH_OBJECTIVES,      AE_CH_ANY_KIND,        30,  900 }
};

#define KIND_COUNT ((int)(sizeof(kinds) / sizeof(kinds[0])))

int ae_ch_kind_count(void)
{
	return KIND_COUNT;
}

struct ae_ch_kind const *ae_ch_kind_at(int index)
{
	return index >= 0 && index < KIND_COUNT ? &kinds[index] : NULL;
}

int ae_ch_kind_find(char const *id)
{
	int each;

	if (!id)
		return -1;
	for (each = 0; each < KIND_COUNT; each++)
		if (!strcmp(kinds[each].id, id))
			return each;
	return -1;
}

long ae_ch_day_of(long local_seconds)
{
	/* (floor, for a clock before the epoch) */
	return local_seconds >= 0 ? local_seconds / 86400 : -((-local_seconds + 86399) / 86400);
}

long ae_ch_day_from_date(int year, int month, int day)
{
	/* (days from civil: the months counted from March, so that the leap day ends the year) */
	long y = year, era, year_of_era, day_of_year;

	if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31)
		return 0;
	if (month <= 2)
		y--;
	era = y / 400;
	year_of_era = y - era * 400;
	day_of_year = (153L * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
	return era * 146097 + year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year - 719468;
}

long ae_ch_week_of(long day)
{
	/* (day 0, 1970-01-01, was a Thursday: three days put the Monday at the start of a week) */
	long shifted = day + 3;

	return shifted >= 0 ? shifted / 7 : -((-shifted + 6) / 7);
}

static unsigned long hash_step(unsigned long state)
{
	/* (xorshift on 32 bits) */
	state &= 0xffffffffu;
	state ^= (state << 13) & 0xffffffffu;
	state ^= state >> 17;
	state ^= (state << 5) & 0xffffffffu;
	return state & 0xffffffffu;
}

int ae_ch_pick(int period, long period_id, int *picks)
{
	int pool[sizeof(kinds) / sizeof(kinds[0])], count = 0, taken = 0, each;
	int metrics_used = 0;
	unsigned long state = ((unsigned long)period_id * 2654435761ul + (unsigned long)period * 40503ul + 12345ul) & 0xffffffffu;

	if ((period != AE_CH_DAILY && period != AE_CH_WEEKLY) || !picks)
		return 0;
	for (each = 0; each < KIND_COUNT; each++)
		if (kinds[each].period == period)
			pool[count++] = each;
	if (state == 0)
		state = 1;
	/* (a shuffle of the period's kinds from the seed, then the first three with a metric not yet picked) */
	for (each = count - 1; each > 0; each--)
	{
		int swap, held;

		state = hash_step(state);
		swap = (int)(state % (unsigned long)(each + 1));
		held = pool[each];
		pool[each] = pool[swap];
		pool[swap] = held;
	}
	for (each = 0; each < count && taken < AE_CH_PER_PERIOD; each++)
		if (!(metrics_used & (1 << kinds[pool[each]].metric)))
		{
			metrics_used |= 1 << kinds[pool[each]].metric;
			picks[taken++] = pool[each];
		}
	return taken;
}

static int kind_counts(struct ae_ch_kind const *kind, int game_kind)
{
	int objective = game_kind == 1 || game_kind == 2 || game_kind == 3 || game_kind == 5;

	return kind->filter == AE_CH_ANY_KIND || (kind->filter == AE_CH_SLAYER_ONLY && game_kind == 0) ||
		(kind->filter == AE_CH_OBJECTIVE_ONLY && objective);
}

long ae_ch_game_gain(struct ae_ch_kind const *kind, struct ae_ch_game const *game)
{
	if (!kind || !game || game->quit || !kind_counts(kind, game->kind))
		return 0;
	switch (kind->metric)
	{
	case AE_CH_GAMES:
		return 1;
	case AE_CH_WINS:
		return game->won ? 1 : 0;
	case AE_CH_KILLS:
		return game->kills > 0 ? game->kills : 0;
	case AE_CH_ASSISTS:
		return game->assists > 0 ? game->assists : 0;
	case AE_CH_MEDALS:
		return game->medals > 0 ? game->medals : 0;
	case AE_CH_OBJECTIVES:
		return game->objectives > 0 ? game->objectives : 0;
	case AE_CH_MINUTES:
		return game->seconds > 0 ? game->seconds / 60 : 0;
	}
	return 0;
}

long ae_ch_campaign_gain(struct ae_ch_kind const *kind)
{
	return kind && kind->metric == AE_CH_CAMPAIGN_LEVELS ? 1 : 0;
}

void ae_ch_label(struct ae_ch_kind const *kind, char *out, size_t size)
{
	char const *games;

	if (!out || size < 1)
		return;
	out[0] = 0;
	if (!kind)
		return;
	games = kind->filter == AE_CH_SLAYER_ONLY ? "SLAYER GAMES" : kind->filter == AE_CH_OBJECTIVE_ONLY ? "OBJECTIVE GAMES" : "GAMES";
	switch (kind->metric)
	{
	case AE_CH_GAMES:
		snprintf(out, size, "PLAY %ld %s", kind->target, games);
		break;
	case AE_CH_WINS:
		snprintf(out, size, "WIN %ld %s", kind->target, games);
		break;
	case AE_CH_KILLS:
		snprintf(out, size, kind->filter == AE_CH_SLAYER_ONLY ? "GET %ld KILLS IN SLAYER" : "GET %ld KILLS", kind->target);
		break;
	case AE_CH_ASSISTS:
		snprintf(out, size, "GET %ld ASSISTS", kind->target);
		break;
	case AE_CH_MEDALS:
		snprintf(out, size, "EARN %ld MEDALS", kind->target);
		break;
	case AE_CH_OBJECTIVES:
		snprintf(out, size, "SCORE %ld OBJECTIVES", kind->target);
		break;
	case AE_CH_MINUTES:
		snprintf(out, size, "PLAY %ld MINUTES", kind->target);
		break;
	case AE_CH_CAMPAIGN_LEVELS:
		snprintf(out, size, "FINISH %ld CAMPAIGN LEVEL%s", kind->target, kind->target == 1 ? "" : "S");
		break;
	}
}

void ae_ch_description(struct ae_ch_kind const *kind, char *out, size_t size)
{
	char const *which, *quit = " A game you leave early does not count.";

	if (!out || size < 1)
		return;
	out[0] = 0;
	if (!kind)
		return;
	which = kind->filter == AE_CH_SLAYER_ONLY ? "Slayer games" : kind->filter == AE_CH_OBJECTIVE_ONLY ? "flag, ball, hill or bomb games" :
		"games of any kind";
	switch (kind->metric)
	{
	case AE_CH_GAMES:
		snprintf(out, size, "Finish %ld %s.%s", kind->target, which, quit);
		break;
	case AE_CH_WINS:
		snprintf(out, size, "Win %ld %s.%s", kind->target, which, quit);
		break;
	case AE_CH_KILLS:
		snprintf(out, size, "Get %ld kills%s.%s", kind->target, kind->filter == AE_CH_SLAYER_ONLY ? " in Slayer games" : " across your games", quit);
		break;
	case AE_CH_ASSISTS:
		snprintf(out, size, "Get %ld assists across your games.%s", kind->target, quit);
		break;
	case AE_CH_MEDALS:
		snprintf(out, size, "Earn %ld medals across your games.%s", kind->target, quit);
		break;
	case AE_CH_OBJECTIVES:
		snprintf(out, size, "Score %ld objectives: flag captures and returns, bomb plants and defuses.%s", kind->target, quit);
		break;
	case AE_CH_MINUTES:
		snprintf(out, size, "Play %ld minutes of finished games.%s", kind->target, quit);
		break;
	case AE_CH_CAMPAIGN_LEVELS:
		snprintf(out, size, "Win %ld campaign level%s.", kind->target, kind->target == 1 ? "" : "s");
		break;
	}
}
