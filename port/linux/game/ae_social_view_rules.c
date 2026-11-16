/* The words the offline social career's screens show (see the header). */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "ae_social_match_rules.h"
#include "ae_social_rules.h"
#include "ae_social_view_rules.h"

#define DOT "\xC2\xB7"

static int put(char *text, int size, char const *format, ...)
{
	va_list args;
	int length;

	if (!text || size <= 0)
		return 0;
	va_start(args, format);
	length = vsnprintf(text, (size_t)size, format, args);
	va_end(args);
	if (length < 0)
	{
		text[0] = 0;
		return 0;
	}
	return length >= size ? size - 1 : length;
}

int ae_social_view_rank(int rank, char *text, int size)
{
	return put(text, size, "RANK %d", rank);
}

int ae_social_view_exp(long exp, char *text, int size)
{
	return put(text, size, "%ld EXP", exp);
}

int ae_social_view_wins_to(long exp, char *text, int size)
{
	long left = ae_social_exp_to_next(exp);

	if (ae_social_rank(exp) >= AE_SOCIAL_RANKS || left <= 0)
		return put(text, size, "TOP RANK");
	return put(text, size, "%ld %s TO RANK %d", left, left == 1 ? "WIN" : "WINS", ae_social_rank(exp) + 1);
}

int ae_social_view_overall_to(long total, char *text, int size)
{
	long left = ae_social_overall_exp_to_next(total);

	if (ae_social_overall_rank(total) >= AE_SOCIAL_RANKS || left <= 0)
		return put(text, size, "TOP OVERALL RANK");
	return put(text, size, "%ld EXP TO OVERALL RANK %d", left, ae_social_overall_rank(total) + 1);
}

int ae_social_view_shape(struct ae_social_playlist const *playlist, int maps, char *text, int size)
{
	char head[32];

	if (!playlist)
		return put(text, size, "%s", "");
	if (playlist->teams == 2)
		put(head, (int)sizeof(head), "%d V %d", playlist->size, playlist->size);
	else if (playlist->teams > 2)
		put(head, (int)sizeof(head), "%d TEAMS OF %d", playlist->teams, playlist->size);
	else
		put(head, (int)sizeof(head), "%d PLAYERS", playlist->size);
	return put(text, size, "%s " DOT " %d %s", head, maps, maps == 1 ? "MAP" : "MAPS");
}

int ae_social_view_maps(struct ae_social_counts const *c, char *text, int size)
{
	if (c->pack_total <= 0)
		return put(text, size, "%d STOCK %s", c->stock_present, c->stock_present == 1 ? "MAP" : "MAPS");
	if (c->stock_in_use)
		return put(text, size, "%d of %d SOCIAL MAP PACK + %d STOCK", c->pack_present, c->pack_total, c->stock_present);
	return put(text, size, "%d of %d SOCIAL MAP PACK", c->pack_present, c->pack_total);
}

int ae_social_view_maps_missing(struct ae_social_counts const *c)
{
	return c->pack_total > 0 && c->pack_present < c->pack_total;
}

int ae_social_view_get_rest(struct ae_social_playlist const *playlist, struct ae_social_counts const *c, char *first,
	int first_size, char *second, int second_size)
{
	int classic = playlist && playlist->weapons_own;

	put(first, first_size, "%s", "");
	put(second, second_size, "%s", "");
	if (!playlist || !c)
		return 0;
	if (classic)
	{
		/* (no pack map: the classic maps are community files this PC may lack; AE #5 is the classic 2v2 server) */
		if (c->pack_total > 0 || c->stock_present >= c->stock_total)
			return 0;
		put(first, first_size, "%s", "Get the rest of the classic 2v2 maps: join AE #5 once,");
		put(second, second_size, "%s", "its maps download to this PC.");
		return 1;
	}
	if (!ae_social_view_maps_missing(c))
		return 0;
	put(first, first_size, "%s", "Get the rest: subscribe to the Social Map Pack in MCC's");
	put(second, second_size, "%s", "Workshop and convert it. Joining an AE server downloads them.");
	return 1;
}

int ae_social_view_bot_line(int mode, char *text, int size)
{
	if (mode <= 0)
		return put(text, size, "Matched to how you play");
	return put(text, size, "Every bot %s", ae_social_skill_name(mode));
}

int ae_social_view_dev_line(long points, char *text, int size)
{
	int tier = ae_social_match_tier(points), low = 0, high = 0;
	struct ae_social_tier const *info = ae_social_tier(tier);

	ae_social_match_window(ae_social_match_level(points), &low, &high);
	return put(text, size, "DEV " DOT " MATCH %ld.%ld " DOT " TIER %d %s / %s " DOT " RANGE %d-%d", points / 100 + 1,
		(points % 100) / 10, tier, info ? ae_social_skill_name(info->shooting) : "", info ? ae_social_skill_name(info->movement) : "",
		low, high);
}

int ae_social_view_ordinal(int place, char *text, int size)
{
	int n = place + 1, last = n % 100;

	return put(text, size, "%d%s", n, (last >= 11 && last <= 13) ? "TH" : n % 10 == 1 ? "ST" : n % 10 == 2 ? "ND" :
		n % 10 == 3 ? "RD" : "TH");
}

int ae_social_view_postgame_sub(char const *label, int ffa, int earns, int players, long exp_after, char *text, int size)
{
	int rank = ae_social_rank(exp_after);
	long left = ae_social_exp_to_next(exp_after);
	char place[16];

	if (ffa && !earns)
	{
		int top = players / 2 > 1 ? players / 2 : 1;

		ae_social_view_ordinal(top - 1, place, (int)sizeof(place));
		return put(text, size, "%s " DOT " %s OR BETTER EARNS EXP " DOT " RANK %d", label, place, rank);
	}
	if (rank >= AE_SOCIAL_RANKS || left <= 0)
		return put(text, size, "%s " DOT " RANK %d", label, rank);
	return put(text, size, "%s " DOT " RANK %d " DOT " %ld TO RANK %d", label, rank, left, rank + 1);
}

int ae_social_view_quit_body(char const *label, long exp, char *text, int size)
{
	if (exp <= 0)
		return put(text, size, "This is a tournament %s game. You have no EXP to lose, but leaving before the end counts as left early. "
			"Leaving at the post-game costs nothing.", label);
	return put(text, size, "This is a tournament %s game. Leaving before the end costs 1 EXP (%ld to %ld) and counts as left early. "
		"Leaving at the post-game costs nothing.", label, exp, exp - 1);
}

int ae_social_view_quit_help(char const *label, char *text, int size)
{
	return put(text, size, "Leave the game: -1 EXP in %s.", label);
}
