/* Bot matching of the offline social career (see the header). Pure code. */
#include <stdio.h>
#include <string.h>

#include "ae_social_match_rules.h"

static struct ae_social_tier const tiers[AE_SOCIAL_TIERS] =
{
	{1, 1, 3, 4}, {1, 2, 4, 6}, {1, 3, 5, 8}, {2, 2, 6, 10}, {2, 3, 7, 12}, {3, 2, 8, 15}, {3, 3, 9, 17},
	{3, 4, 10, 19}, {4, 3, 11, 22}, {4, 4, 12, 24}, {4, 5, 13, 27}, {5, 4, 14, 29}, {5, 5, 15, 32}, {5, 6, 16, 35},
	{6, 5, 17, 38}, {6, 6, 18, 41}, {6, 7, 19, 44}, {7, 6, 20, 47}, {7, 7, 21, 49}
};

static char const *const skill_names[AE_SOCIAL_SKILL_STEPS] =
	{"RECRUIT", "EASY", "NORMAL", "HEROIC", "ELITE", "LEGENDARY", "DEMON"};

struct ae_social_tier const *ae_social_tier(int tier)
{
	return tier >= 1 && tier <= AE_SOCIAL_TIERS ? &tiers[tier - 1] : NULL;
}

char const *ae_social_skill_name(int step)
{
	return step >= 1 && step <= AE_SOCIAL_SKILL_STEPS ? skill_names[step - 1] : "";
}

int ae_social_tier_fixed(int skill_step)
{
	int tier;

	for (tier = 1; tier <= AE_SOCIAL_TIERS; tier++)
		if (tiers[tier - 1].shooting == skill_step && tiers[tier - 1].movement == skill_step)
			return tier;
	return 0;
}

int ae_social_tier_of_level(int level_shown)
{
	int tier, best = 1, best_distance = 1000;

	for (tier = 1; tier <= AE_SOCIAL_TIERS; tier++)
	{
		int distance = tiers[tier - 1].level - level_shown;

		if (distance < 0)
			distance = -distance;
		if (distance < best_distance)
		{
			best = tier;
			best_distance = distance;
		}
	}
	return best;
}

int ae_social_tier_raw_level(int tier)
{
	return tier >= 1 && tier <= AE_SOCIAL_TIERS ? tiers[tier - 1].level - 1 : 0;
}

int ae_social_match_level(long points)
{
	if (points < 0)
		points = 0;
	if (points > AE_SOCIAL_MATCH_POINTS_MAX)
		points = AE_SOCIAL_MATCH_POINTS_MAX;
	return (int)(points / 100);
}

int ae_social_match_tier(long points)
{
	return ae_social_tier_of_level(ae_social_match_level(points) + 1);
}

long ae_social_match_points_of_level(int level_shown)
{
	if (level_shown < 1)
		level_shown = 1;
	if (level_shown > AE_RATING_LEVELS)
		level_shown = AE_RATING_LEVELS;
	return (long)(level_shown - 1) * 100;
}

long ae_social_match_points_of_tier(int tier)
{
	return ae_social_match_points_of_level(tier >= 1 && tier <= AE_SOCIAL_TIERS ? tiers[tier - 1].level : 1);
}

long ae_social_match_pair_change(int own_level, int other_level, int result)
{
	int difference = own_level - other_level + 127;

	if (difference < 0)
		difference = 0;
	if (difference > AE_RATING_DIFFERENCES - 1)
		difference = AE_RATING_DIFFERENCES - 1;
	if (result > 0)
		return ae_rating_win_base(difference);
	if (result < 0)
		return ae_rating_loss_base(difference);
	return 0;
}

long ae_social_match_apply(long points, long change)
{
	points += change;
	if (points < 0)
		points = 0;
	if (points > AE_SOCIAL_MATCH_POINTS_MAX)
		points = AE_SOCIAL_MATCH_POINTS_MAX;
	return points;
}

long ae_social_match_change(long points, long games_played, int const *levels, int const *results, int count)
{
	long total = 0, change;
	int index, own = ae_social_match_level(points);

	if (count <= 0)
		return 0;
	for (index = 0; index < count; index++)
		total += ae_social_match_pair_change(own, levels[index], results[index]);
	change = total / count;
	if (change < -AE_RATING_CHANGE_MAX)
		change = -AE_RATING_CHANGE_MAX;
	if (change > AE_RATING_CHANGE_MAX)
		change = AE_RATING_CHANGE_MAX;
	if (games_played < AE_SOCIAL_PLACEMENT_GAMES)
		change *= 2;
	return ae_social_match_apply(points, change) - points;
}

static char const *const experience_names[NUMBER_OF_AE_SOCIAL_EXPERIENCES] = {"", "NEW", "SOME", "A LOT"};
/* (the skill step each answer starts at: EASY 2, NORMAL 3, HEROIC 4) */
static int const experience_steps[NUMBER_OF_AE_SOCIAL_EXPERIENCES] = {3, 2, 3, 4};

long ae_social_calibration_start(int experience)
{
	if (experience <= AE_SOCIAL_EXPERIENCE_UNSET || experience >= NUMBER_OF_AE_SOCIAL_EXPERIENCES)
		return AE_SOCIAL_MATCH_START;
	return ae_social_match_points_of_tier(ae_social_tier_fixed(experience_steps[experience]));
}

char const *ae_social_experience_name(int experience)
{
	return experience >= 0 && experience < NUMBER_OF_AE_SOCIAL_EXPERIENCES ? experience_names[experience] : "";
}

char const *ae_social_experience_skill(int experience)
{
	if (experience <= AE_SOCIAL_EXPERIENCE_UNSET || experience >= NUMBER_OF_AE_SOCIAL_EXPERIENCES)
		return ae_social_skill_name(3);
	return ae_social_skill_name(experience_steps[experience]);
}

long ae_social_placement_change(long change, long games_played, int kills, int deaths)
{
	long kd;

	if (games_played >= AE_SOCIAL_PLACEMENT_GAMES)
		return change;
	if (kills < 0)
		kills = 0;
	if (deaths < 0)
		deaths = 0;
	kd = (long)(kills - deaths) * AE_SOCIAL_PLACEMENT_KD_POINTS;
	if (kd > AE_SOCIAL_PLACEMENT_KD_MAX)
		kd = AE_SOCIAL_PLACEMENT_KD_MAX;
	if (kd < -AE_SOCIAL_PLACEMENT_KD_MAX)
		kd = -AE_SOCIAL_PLACEMENT_KD_MAX;
	return change + kd;
}

long ae_social_party_points(long const *points, int count)
{
	long total = 0;
	int index;

	if (!points || count <= 0)
		return AE_SOCIAL_MATCH_START;
	for (index = 0; index < count; index++)
		total += points[index];
	return total / count;
}

static int window_levels(int raw_level, int *low, int *high)
{
	struct ae_rating_window window;

	ae_rating_window_default(&window);
	if (raw_level < 0)
		raw_level = 0;
	if (raw_level > AE_RATING_LEVELS - 1)
		raw_level = AE_RATING_LEVELS - 1;
	*low = ae_rating_window_low(&window, raw_level);
	*high = ae_rating_window_upper(&window, raw_level);
	if (*high > AE_RATING_LEVELS - 1)
		*high = AE_RATING_LEVELS - 1;
	return raw_level;
}

void ae_social_match_window(int raw_level, int *low_shown, int *high_shown)
{
	int low, high;

	window_levels(raw_level, &low, &high);
	*low_shown = low + 1;
	*high_shown = high + 1;
}

int ae_social_shape_seats(struct ae_social_shape const *shape)
{
	int seats;

	if (!shape || shape->size < 1)
		return 0;
	if (shape->teams < 0 || shape->teams > AE_RATING_BALANCE_TEAMS)
		return 0;
	seats = shape->teams ? shape->teams * shape->size : shape->size;
	return seats >= 1 && seats <= AE_SOCIAL_SEATS ? seats : 0;
}

/* the offset weights: -2 .. +2 */
static int const offset_weight[5] = {5, 15, 60, 15, 5};

static int draw_tier(struct ae_rating_rng *rng, int player_tier, int low_raw, int high_raw)
{
	int weight[5], offset;
	unsigned long total = 0, pick;

	for (offset = -2; offset <= 2; offset++)
	{
		int tier = player_tier + offset;

		weight[offset + 2] = 0;
		if (tier < 1 || tier > AE_SOCIAL_TIERS)
			continue;
		if (ae_social_tier_raw_level(tier) < low_raw || ae_social_tier_raw_level(tier) > high_raw)
			continue;
		weight[offset + 2] = offset_weight[offset + 2];
		total += (unsigned long)weight[offset + 2];
	}
	if (!total)
		return player_tier;
	pick = ae_rating_rng_below(rng, total);
	for (offset = 0; offset < 5; offset++)
	{
		if (pick < (unsigned long)weight[offset])
			return player_tier + offset - 2;
		pick -= (unsigned long)weight[offset];
	}
	return player_tier;
}

/* the teams by hand: the humans on team 0, then the seats in turn to the team with the most room, when the balance gives
   uneven teams */
static void deal_simple(struct ae_social_roster *roster, int size)
{
	int fill[AE_RATING_BALANCE_TEAMS] = {0}, index, team;

	for (index = 0; index < roster->count; index++)
	{
		for (team = 0; team < roster->teams; team++)
			if (fill[team] < size)
				break;
		if (index < roster->humans)
			team = 0;
		roster->seat[index].team = team;
		fill[team]++;
	}
}

int ae_social_roster_draw(struct ae_social_shape const *shape, int humans, long match_points, int fixed, unsigned long seed,
	struct ae_social_roster *roster)
{
	struct ae_rating_rng rng;
	struct ae_rating_window window;
	struct ae_rating_search search;
	int seats = ae_social_shape_seats(shape), index, player_raw, low, high;
	int fixed_tier = 0;

	memset(roster, 0, sizeof(*roster));
	if (!seats || humans < 1 || humans > 4 || humans > shape->size || humans > seats)
		return 0;
	if (fixed < 0 || fixed > AE_SOCIAL_SKILL_STEPS)
		return 0;
	if (fixed)
		fixed_tier = ae_social_tier_fixed(fixed);
	ae_rating_rng_seed(&rng, seed);
	ae_rating_window_default(&window);
	player_raw = window_levels(ae_social_match_level(match_points), &low, &high);
	ae_rating_search_setup(&window, player_raw, player_raw, &search);
	roster->count = seats;
	roster->humans = humans;
	roster->teams = shape->teams;
	roster->span = search.span;
	roster->fixed = fixed;
	roster->player_level = player_raw + 1;
	roster->player_tier = ae_social_tier_of_level(roster->player_level);
	for (index = 0; index < seats; index++)
	{
		struct ae_social_seat *seat = &roster->seat[index];

		if (index < humans)
		{
			seat->human = 1;
			seat->tier = roster->player_tier;
			seat->level = roster->player_level;
		}
		else
		{
			int tier = fixed ? fixed_tier : draw_tier(&rng, roster->player_tier, low, high);
			int raw = ae_social_tier_raw_level(tier), step;

			seat->tier = tier;
			seat->shooting = tiers[tier - 1].shooting;
			seat->movement = tiers[tier - 1].movement;
			seat->level = tiers[tier - 1].level;
			seat->join_step = 0;
			if (!fixed)
			{
				seat->join_step = search.span + 1;
				for (step = 0; step <= search.span; step++)
				{
					int minimum, maximum;

					ae_rating_search_range(&search, step, &minimum, &maximum);
					if (raw >= minimum && raw <= maximum)
					{
						seat->join_step = step;
						break;
					}
				}
			}
		}
		seat->team = index;
	}
	if (shape->teams > 0)
	{
		long weights[AE_SOCIAL_SEATS];
		int party[AE_SOCIAL_SEATS], team_of[AE_SOCIAL_SEATS], uneven = 0;
		int fill[AE_RATING_BALANCE_TEAMS] = {0};

		for (index = 0; index < seats; index++)
		{
			weights[index] = roster->seat[index].level;
			party[index] = index < humans ? 0 : index - humans + 1;
		}
		if (ae_rating_balance_teams(shape->teams, shape->size, seats, weights, party, 1, 1, 0, team_of, &roster->cost))
		{
			for (index = 0; index < seats; index++)
			{
				roster->seat[index].team = team_of[index];
				fill[team_of[index]]++;
			}
			for (index = 0; index < shape->teams; index++)
				if (fill[index] != shape->size)
					uneven = 1;
		}
		else
			uneven = 1;
		if (uneven)
		{
			deal_simple(roster, shape->size);
			roster->cost = 0;
		}
	}
	return 1;
}

int ae_social_roster_join_order(struct ae_social_roster const *roster, int *order)
{
	int count = 0, index, at;

	for (index = roster->humans; index < roster->count; index++)
	{
		/* (insertion by step, ties by seat index: stable) */
		for (at = count; at > 0 && roster->seat[order[at - 1]].join_step > roster->seat[index].join_step; at--)
			order[at] = order[at - 1];
		order[at] = index;
		count++;
	}
	return count;
}

int ae_social_roster_text(struct ae_social_roster const *roster, char const *playlist, unsigned long seed, char *text, int size)
{
	int at = 0, team, seat;

	if (size < 1)
		return 0;
	text[0] = 0;
#define text_printf(...) do { int n_ = snprintf(text + at, (size_t)(size - at), __VA_ARGS__); if (n_ < 0 || n_ >= size - at) { at = size - 1; } else at += n_; } while (0)
	text_printf("roster %s seed %lu: you L%d T%d", playlist, seed, roster->player_level, roster->player_tier);
	if (roster->teams > 0)
	{
		for (team = 0; team < roster->teams; team++)
		{
			if (roster->teams == 2)
				text_printf("; %s", team == 0 ? "red" : "blue");
			else
				text_printf("; team %d", team + 1);
			for (seat = roster->humans; seat < roster->count; seat++)
				if (roster->seat[seat].team == team)
					text_printf(" T%d", roster->seat[seat].tier);
		}
	}
	else
	{
		text_printf("; players");
		for (seat = roster->humans; seat < roster->count; seat++)
			text_printf(" T%d", roster->seat[seat].tier);
	}
#undef text_printf
	return at;
}
