/* The career surfaces' words and numbers (see the header). Pure code. */
#include <stdio.h>
#include <string.h>

#include "ae_career_challenge_rules.h"
#include "ae_career_view_rules.h"
#include "ae_career_xp_rules.h"

static void put(char *out, int size, char const *format, ...) __attribute__((format(printf, 3, 4)));

#include <stdarg.h>

static void put(char *out, int size, char const *format, ...)
{
	va_list args;

	if (!out || size < 1)
		return;
	va_start(args, format);
	vsnprintf(out, (size_t)size, format, args);
	va_end(args);
}

void ae_cv_number(long value, char *out, int size)
{
	char digits[24], grouped[32];
	int length, each, at = 0;

	if (!out || size < 1)
		return;
	if (value < 0)
		value = 0;
	snprintf(digits, sizeof(digits), "%ld", value);
	length = (int)strlen(digits);
	for (each = 0; each < length; each++)
	{
		if (each > 0 && (length - each) % 3 == 0)
			grouped[at++] = ',';
		grouped[at++] = digits[each];
	}
	grouped[at] = 0;
	put(out, size, "%s", grouped);
}

long ae_cv_losses(long games, long wins, long ties, long quits)
{
	long losses = games - wins - ties - quits;

	return losses < 0 ? 0 : losses;
}

void ae_cv_win_rate(long games, long wins, char *out, int size)
{
	if (games <= 0)
		put(out, size, "%s", AE_CV_DASH);
	else
		put(out, size, "%ld%%", (wins * 100 + games / 2) / games);
}

int ae_cv_fit_size(float width40, float width34, float width30, float room)
{
	if (width40 <= room)
		return 40;
	if (width34 <= room)
		return 34;
	(void)width30;
	return 30;
}

void ae_cv_meta(int step, long xp, char *rank, int rank_size, char *tour, int tour_size, char *xp_text, int xp_size)
{
	char grouped[24];

	put(rank, rank_size, "RANK %d OF %d", ae_xp_rank(step), AE_XP_RANKS);
	put(tour, tour_size, "TOUR %d OF %d", ae_xp_tour(step), AE_XP_TOURS);
	ae_cv_number(xp, grouped, (int)sizeof(grouped));
	put(xp_text, xp_size, "%s XP", grouped);
}

static void upper_name(int rank, char *out, int size)
{
	char const *name = ae_xp_rank_name(rank);
	int each;

	for (each = 0; name[each] && each < size - 1; each++)
		out[each] = name[each] >= 'a' && name[each] <= 'z' ? (char)(name[each] - 32) : name[each];
	out[each] = 0;
}

void ae_cv_career_captions(int step, long to_next, char *left, int left_size, char *right, int right_size)
{
	if (step >= AE_XP_STEPS - 1)
	{
		put(left, left_size, "THE LAST RANK");
		put(right, right_size, "XP KEEPS COUNTING");
		return;
	}
	{
		char grouped[24], name[48];
		int rank = ae_xp_rank(step);

		ae_cv_number(to_next, grouped, (int)sizeof(grouped));
		put(left, left_size, "%s XP TO GO", grouped);
		if (rank >= AE_XP_RANKS)
			put(right, right_size, "NEXT: TOUR %d", ae_xp_tour(step) + 1);
		else
		{
			upper_name(rank + 1, name, (int)sizeof(name));
			put(right, right_size, "NEXT: %s", name);
		}
	}
}

static void ladder_captions(char const *what, int rank, long wins_to_go, int top, char *left, int left_size, char *right, int right_size)
{
	if (rank >= top || wins_to_go <= 0)
	{
		put(left, left_size, "THE TOP RANK");
		put(right, right_size, "%s", "");
		return;
	}
	if (wins_to_go == 1)
		put(left, left_size, "1 WIN OR TIE TO GO");
	else
		put(left, left_size, "%ld WINS OR TIES TO GO", wins_to_go);
	put(right, right_size, "NEXT: %s%d", what, rank + 1);
}

void ae_cv_skill_captions(int rank, long wins_to_go, char *left, int left_size, char *right, int right_size)
{
	ladder_captions("RANK ", rank, wins_to_go, 50, left, left_size, right, right_size);
}

void ae_cv_overall_captions(int rank, long wins_to_go, char *left, int left_size, char *right, int right_size)
{
	ladder_captions("OVERALL RANK ", rank, wins_to_go, 50, left, left_size, right, right_size);
}

void ae_cv_challenge_to_go(int metric, long left, char *out, int size)
{
	char const *unit = "GAMES", *one = "GAME";

	switch (metric)
	{
	case AE_CH_KILLS:
		unit = "KILLS"; one = "KILL";
		break;
	case AE_CH_ASSISTS:
		unit = "ASSISTS"; one = "ASSIST";
		break;
	case AE_CH_WINS:
		unit = "WINS"; one = "WIN";
		break;
	case AE_CH_MEDALS:
		unit = "MEDALS"; one = "MEDAL";
		break;
	case AE_CH_OBJECTIVES:
		unit = "OBJECTIVES"; one = "OBJECTIVE";
		break;
	case AE_CH_MINUTES:
		unit = "MINUTES"; one = "MINUTE";
		break;
	case AE_CH_CAMPAIGN_LEVELS:
		unit = "LEVELS"; one = "LEVEL";
		break;
	}
	if (left < 0)
		left = 0;
	put(out, size, "%ld %s TO GO", left, left == 1 ? one : unit);
}
