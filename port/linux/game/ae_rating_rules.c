/* The rating core's rules (see the header). Pure code. */
#include <string.h>

#include "ae_rating_rules.h"

/* ---- levels and points ---- */

static long const level_start_early[8] = {0, 100, 200, 400, 600, 900, 1200, 1600};
static long const level_start_late[10] = {18100, 18900, 20000, 21500, 23500, 26100, 29400, 33500, 38500, 44500};

long ae_rating_level_points(int level)
{
	if (level < 0 || level >= AE_RATING_LEVELS)
		return AE_RATING_POINTS_MAX;
	if (level < 8)
		return level_start_early[level];
	if (level < 40)
		return 2000 + 500L * (level - 8);
	return level_start_late[level - 40];
}

int ae_rating_level_from_points(long points, int previous)
{
	int level = 0, index;

	for (index = 1; index < AE_RATING_LEVELS; index++)
	{
		if (points < ae_rating_level_points(index))
		{
			if (previous > 0 && level == previous - 1 &&
				points > (ae_rating_level_points(level) + ae_rating_level_points(level + 1)) / 2)
				level = previous;
			return level;
		}
		level++;
	}
	return level;
}

int ae_rating_level_shown(int level)
{
	return level + 1;
}

/* ---- one game ---- */

int ae_rating_win_base(int difference)
{
	if (difference <= 119)
		return 180;
	if (difference >= 135)
		return 20;
	return 180 - 10 * (difference - 119);
}

int ae_rating_loss_base(int difference)
{
	if (difference <= 119)
		return -20;
	if (difference >= 135)
		return -180;
	return -20 - 10 * (difference - 119);
}

int ae_rating_win_weight(int level)
{
	static int const late[10] = {95, 90, 85, 80, 70, 65, 60, 55, 50, 50};

	if (level < 0)
		return 0;
	if (level <= 40)
		return 100;
	if (level <= 50)
		return late[level - 41];
	return 0;
}

int ae_rating_loss_weight(int level)
{
	static int const early[15] = {0, 5, 10, 20, 35, 45, 55, 60, 65, 70, 75, 80, 85, 90, 95};

	if (level < 0)
		return 0;
	if (level < 15)
		return early[level];
	return 100;
}

long ae_rating_pair_change(int own_level, int other_level, int own_place, int other_place)
{
	int difference = own_level - other_level + 127;

	if (difference < 0)
		difference = 0;
	if (difference > AE_RATING_DIFFERENCES - 1)
		difference = AE_RATING_DIFFERENCES - 1;
	if (own_place >= 0 && (other_place < 0 || own_place < other_place))
		return (long)ae_rating_win_base(difference) * ae_rating_win_weight(own_level) / 100;
	if (other_place >= 0 && (own_place < 0 || other_place < own_place))
		return (long)ae_rating_loss_base(difference) * ae_rating_loss_weight(own_level) / 100;
	return 0;
}

static long game_change(struct ae_rating_entry const *entries, int count, int self, long points, int own_place)
{
	long total = 0;
	int opponents = 0, index;
	struct ae_rating_entry const *me = &entries[self];

	for (index = 0; index < count; index++)
	{
		struct ae_rating_entry const *other = &entries[index];

		if (index == self || !other->present || !other->rated || other->team < 0 || other->team == me->team)
			continue;
		total += ae_rating_pair_change(me->level, other->level, own_place, other->quit ? AE_RATING_NONE : other->place);
		opponents++;
	}
	if (opponents > 0)
		total /= opponents;
	if (points + total < 0)
		total = -points;
	if (points + total > AE_RATING_POINTS_MAX)
		total = AE_RATING_POINTS_MAX - points;
	if (total < -AE_RATING_CHANGE_MAX)
		total = -AE_RATING_CHANGE_MAX;
	if (total > AE_RATING_CHANGE_MAX)
		total = AE_RATING_CHANGE_MAX;
	return total;
}

long ae_rating_game_change(struct ae_rating_entry const *entries, int count, int self, long points)
{
	if (self < 0 || self >= count)
		return 0;
	return game_change(entries, count, self, points, entries[self].quit ? AE_RATING_NONE : entries[self].place);
}

void ae_rating_state_init(struct ae_rating_state *state, long points)
{
	if (points < 0)
		points = 0;
	if (points > AE_RATING_POINTS_MAX)
		points = AE_RATING_POINTS_MAX;
	state->points = points;
	state->level = ae_rating_level_from_points(points, 0);
	state->highest = state->level;
}

void ae_rating_apply(struct ae_rating_state *state, long change)
{
	long points = state->points + change;

	if (points < 0)
		points = 0;
	if (points > AE_RATING_POINTS_MAX)
		points = AE_RATING_POINTS_MAX;
	state->points = points;
	state->level = ae_rating_level_from_points(points, state->level);
	if (state->level > state->highest)
		state->highest = state->level;
}

long ae_rating_quit_change(int policy, struct ae_rating_entry const *entries, int count, int self, long points)
{
	if (policy != AE_RATING_QUIT_AS_LAST || self < 0 || self >= count)
		return 0;
	return game_change(entries, count, self, points, AE_RATING_NONE);
}

/* ---- the window and the search ---- */

void ae_rating_window_default(struct ae_rating_window *window)
{
	int level;

	for (level = 0; level < AE_RATING_WINDOW_LEVELS; level++)
	{
		int step = level <= 5 ? 5 : level <= 15 ? 6 : level <= 25 ? 7 : level <= 30 ? 8 : level <= 35 ? 9 : 10;
		int upper = level + step;

		window->upper[level] = (unsigned char)(upper > AE_RATING_WINDOW_LEVELS - 1 ? AE_RATING_WINDOW_LEVELS - 1 : upper);
	}
}

static int window_clamp(int level)
{
	if (level < 0)
		return 0;
	if (level > AE_RATING_WINDOW_LEVELS - 1)
		return AE_RATING_WINDOW_LEVELS - 1;
	return level;
}

int ae_rating_window_upper(struct ae_rating_window const *window, int level)
{
	return window->upper[window_clamp(level)];
}

int ae_rating_window_low(struct ae_rating_window const *window, int level)
{
	int index = window_clamp(level);

	while (index > 0 && window->upper[index - 1] >= level)
		index--;
	return index;
}

int ae_rating_party_level(struct ae_rating_window const *window, int own_level, int party_best)
{
	int low = ae_rating_window_low(window, party_best);

	return low > own_level ? low : own_level;
}

void ae_rating_search_setup(struct ae_rating_window const *window, int party_low, int party_high,
	struct ae_rating_search *search)
{
	int a, b;

	search->party_low = party_low;
	search->low = ae_rating_window_low(window, party_low);
	search->high = party_high > search->low ? party_high : search->low;
	search->upper = ae_rating_window_upper(window, search->high);
	a = search->high - search->low;
	b = search->upper - party_low;
	search->span = a > b ? a : b;
}

void ae_rating_search_range(struct ae_rating_search const *search, int step, int *minimum, int *maximum)
{
	int low, high;

	if (step == AE_RATING_STEP_OPEN)
	{
		*minimum = 0;
		*maximum = AE_RATING_WINDOW_LEVELS - 1;
		return;
	}
	low = search->high - step;
	if (low < search->low)
		low = search->low;
	high = search->party_low + step;
	if (high > search->upper)
		high = search->upper;
	*minimum = low;
	*maximum = high;
}

int ae_rating_search_next(struct ae_rating_search const *search, int step, int ranked)
{
	if (step == AE_RATING_STEP_OPEN || step < 0)
		return AE_RATING_STEP_END;
	if (step < search->span)
		return step + 1;
	return ranked ? AE_RATING_STEP_END : AE_RATING_STEP_OPEN;
}

int ae_rating_search_minimum_score(int step, int span)
{
	if (step >= 0 && span > 0)
		return (int)((float)AE_RATING_SCORE_MINIMUM_START +
			((float)step / (float)span) * ((float)AE_RATING_SCORE_MINIMUM_END - (float)AE_RATING_SCORE_MINIMUM_START));
	return AE_RATING_SCORE_MINIMUM_END;
}

int ae_rating_search_allows(struct ae_rating_search const *search, int step, int game_minimum, int game_maximum)
{
	int minimum, maximum;

	if (step == AE_RATING_STEP_OPEN)
		return 1;
	ae_rating_search_range(search, step, &minimum, &maximum);
	return game_minimum >= minimum && game_maximum <= maximum;
}

int ae_rating_search_exhausted(int results_seen)
{
	return results_seen >= AE_RATING_SEARCH_RESULT_LIMIT;
}

int ae_rating_language_score(int mine, int theirs, unsigned strict_languages)
{
	int mine_strict = mine >= 0 && mine < AE_RATING_LANGUAGES && ((strict_languages >> mine) & 1u);
	int theirs_strict = theirs >= 0 && theirs < AE_RATING_LANGUAGES && ((strict_languages >> theirs) & 1u);

	if (mine == theirs)
		return mine_strict ? 100 : 0;
	return mine_strict || theirs_strict ? -100 : -50;
}

int ae_rating_round_trip_score(long milliseconds)
{
	static int const table[10] = {100, 80, 50, 0, -100, -200, -200, -200, -200, -200};
	float value = (float)milliseconds * 0.01f;
	int index;

	if (value < 0.0f)
		value = 0.0f;
	if (value > 9.0f)
		value = 9.0f;
	index = (int)value;
	if (index < 9)
		return (int)((float)table[index] + (value - (float)index) * (float)(table[index + 1] - table[index]));
	return table[9];
}

long ae_rating_candidate_score(struct ae_rating_candidate const *candidate, int my_level, unsigned strict_languages)
{
	long score = 0;
	int distance = candidate->level - my_level;
	long players = candidate->players;

	if (distance < 0)
		distance = -distance;
	if (distance <= AE_RATING_SCORE_CLOSE_LEVELS)
		score += AE_RATING_SCORE_CLOSE_BEST - ((AE_RATING_SCORE_CLOSE_BEST - AE_RATING_SCORE_CLOSE_WORST) /
			(AE_RATING_SCORE_CLOSE_LEVELS + 1)) * distance;
	if (candidate->same_playlist)
		score += AE_RATING_SCORE_SAME_PLAYLIST;
	if (players < 0)
		players = 0;
	if (players > AE_RATING_SCORE_POPULATION_CAP)
		players = AE_RATING_SCORE_POPULATION_CAP;
	score += players * AE_RATING_SCORE_POPULATION_PER_PLAYER;
	if (candidate->has_details)
	{
		if ((candidate->host_up < AE_RATING_BANDWIDTH_LOW || candidate->host_down < AE_RATING_BANDWIDTH_LOW) &&
			candidate->my_up >= AE_RATING_BANDWIDTH_LOW && candidate->my_down >= AE_RATING_BANDWIDTH_LOW)
			score += AE_RATING_SCORE_WEAK_HOST;
		score += ae_rating_language_score(candidate->my_language, candidate->host_language, strict_languages);
	}
	if (candidate->round_trip_known)
		score += ae_rating_round_trip_score(candidate->round_trip_ms);
	return score;
}

int ae_rating_search_ready(int candidates, int answered, long elapsed_ms)
{
	if (candidates <= 0)
		return 0;
	return answered == candidates ||
		(answered >= (candidates >> 2) && elapsed_ms > AE_RATING_SEARCH_READY_WAIT_MS);
}

/* ---- the start delay ---- */

long ae_rating_start_delay_players(long longest, int minimum, int maximum, int players)
{
	if (players < minimum || players > maximum)
		return AE_RATING_NEVER;
	if (players == maximum)
		return 0;
	return longest * (maximum - players) / (maximum - minimum);
}

long ae_rating_start_delay_teams(long longest, int team_count, int minimum_size, int maximum_size, int players)
{
	int maximum = team_count * maximum_size, minimum = team_count * minimum_size;

	if (players < minimum || players > maximum)
		return AE_RATING_NEVER;
	if (players == maximum)
		return 0;
	return (long)(maximum_size - players / team_count) * longest / (maximum_size - minimum_size);
}

/* ---- team balance ---- */

struct party
{
	int flagged;
	int count;
	long total;
	int member[AE_RATING_BALANCE_PLAYERS];
};

struct team
{
	int party_count;
	int party[AE_RATING_BALANCE_PARTIES];   /* indices into the party table, in the order they came */
};

struct solution
{
	int team_count;
	struct team team[AE_RATING_BALANCE_TEAMS];
};

static void sort_down(long *values, int count)
{
	int i, j;

	for (i = 1; i < count; i++)
	{
		long value = values[i];

		for (j = i; j > 0 && values[j - 1] < value; j--)
			values[j] = values[j - 1];
		values[j] = value;
	}
}

long ae_rating_balance_cost(int team_count, int count, long const *weights, int const *team_of)
{
	long sorted[AE_RATING_BALANCE_TEAMS][AE_RATING_BALANCE_PLAYERS];
	int size[AE_RATING_BALANCE_TEAMS];
	long total[AE_RATING_BALANCE_TEAMS];
	long total_max, total_min, rank_sum = 0;
	int largest = 0, team, rank, index;

	if (team_count < 1 || team_count > AE_RATING_BALANCE_TEAMS || count > AE_RATING_BALANCE_PLAYERS)
		return 0;
	for (team = 0; team < team_count; team++)
		size[team] = 0, total[team] = 0;
	for (index = 0; index < count; index++)
	{
		team = team_of[index];
		if (team < 0 || team >= team_count)
			continue;
		sorted[team][size[team]++] = weights[index];
		total[team] += weights[index];
	}
	for (team = 0; team < team_count; team++)
	{
		sort_down(sorted[team], size[team]);
		if (size[team] > largest)
			largest = size[team];
	}
	total_max = total_min = total[0];
	for (team = 1; team < team_count; team++)
	{
		if (total[team] > total_max)
			total_max = total[team];
		if (total[team] < total_min)
			total_min = total[team];
	}
	for (rank = 0; rank < largest; rank++)
	{
		long maximum = rank < size[0] ? sorted[0][rank] : 0, minimum = maximum;

		for (team = 1; team < team_count; team++)
		{
			long weight = rank < size[team] ? sorted[team][rank] : 0;

			if (weight > maximum)
				maximum = weight;
			if (weight < minimum)
				minimum = weight;
		}
		rank_sum += maximum - minimum;
	}
	return 7 * (total_max - total_min) + 3 * rank_sum;
}

int ae_rating_balance_count_spread(int team_count, int count, int const *team_of)
{
	int size[AE_RATING_BALANCE_TEAMS], team, index, maximum, minimum;

	if (team_count < 1 || team_count > AE_RATING_BALANCE_TEAMS)
		return 0;
	for (team = 0; team < team_count; team++)
		size[team] = 0;
	for (index = 0; index < count; index++)
		if (team_of[index] >= 0 && team_of[index] < team_count)
			size[team_of[index]]++;
	maximum = minimum = size[0];
	for (team = 1; team < team_count; team++)
	{
		if (size[team] > maximum)
			maximum = size[team];
		if (size[team] < minimum)
			minimum = size[team];
	}
	return maximum - minimum;
}

static int team_size(struct party const *parties, struct team const *team)
{
	int index, count = 0;

	for (index = 0; index < team->party_count; index++)
		count += parties[team->party[index]].count;
	return count;
}

static long team_total(struct party const *parties, struct team const *team)
{
	int index;
	long total = 0;

	for (index = 0; index < team->party_count; index++)
		total += parties[team->party[index]].total;
	return total;
}

/* a solution as a team for each player, to be costed by the public functions */
static void flatten(struct party const *parties, struct solution const *solution, int *team_of)
{
	int team, index, member;

	for (team = 0; team < solution->team_count; team++)
		for (index = 0; index < solution->team[team].party_count; index++)
		{
			struct party const *party = &parties[solution->team[team].party[index]];

			for (member = 0; member < party->count; member++)
				team_of[party->member[member]] = team;
		}
}

static void solution_measure(struct party const *parties, struct solution const *solution, int count,
	long const *weights, int *count_spread, long *cost)
{
	int team_of[AE_RATING_BALANCE_PLAYERS];

	flatten(parties, solution, team_of);
	*count_spread = ae_rating_balance_count_spread(solution->team_count, count, team_of);
	*cost = ae_rating_balance_cost(solution->team_count, count, weights, team_of);
}

static void team_remove_party(struct team *team, int id)
{
	int index = 0;

	while (index < team->party_count && team->party[index] != id)
		index++;
	if (index == team->party_count)
		return;
	team->party_count--;
	for (; index < team->party_count; index++)
		team->party[index] = team->party[index + 1];
}

static void team_add_party(struct team *team, int id)
{
	team->party[team->party_count++] = id;
}

int ae_rating_balance_teams(int team_count, int maximum_team_size, int count, long const *weights, int const *party,
	int optimize, int flag_minimum, int flag_maximum, int *team_of, long *cost)
{
	struct party parties[AE_RATING_BALANCE_PARTIES];
	int remaining[AE_RATING_BALANCE_PARTIES];
	int remaining_count = 0, index;
	struct solution current, best;
	int improved;

	if (team_count < 1 || team_count > AE_RATING_BALANCE_TEAMS || count < 0 || count > AE_RATING_BALANCE_PLAYERS)
		return 0;
	memset(parties, 0, sizeof(parties));
	for (index = 0; index < count; index++)
	{
		struct party *p;

		if (party[index] < 0 || party[index] >= AE_RATING_BALANCE_PARTIES)
			return 0;
		p = &parties[party[index]];
		p->member[p->count++] = index;
		p->total += weights[index];
	}
	for (index = 0; index < AE_RATING_BALANCE_PARTIES; index++)
		if (parties[index].count > 0)
		{
			if (parties[index].count >= flag_minimum && parties[index].count <= flag_maximum)
				parties[index].flagged = 1;
			remaining[remaining_count++] = index;
		}

	/* the deal: the largest party (most players, then most weight, the first of equals) to the smallest team */
	memset(&current, 0, sizeof(current));
	current.team_count = team_count;
	while (remaining_count > 0)
	{
		int pick = 0, target = 0, team;

		for (index = 1; index < remaining_count; index++)
		{
			struct party const *a = &parties[remaining[index]], *b = &parties[remaining[pick]];

			if (a->count > b->count || (a->count == b->count && a->total > b->total))
				pick = index;
		}
		for (team = 1; team < team_count; team++)
		{
			int size = team_size(parties, &current.team[team]), best_size = team_size(parties, &current.team[target]);

			if (size < best_size || (size == best_size &&
				team_total(parties, &current.team[team]) < team_total(parties, &current.team[target])))
				target = team;
		}
		team_add_party(&current.team[target], remaining[pick]);
		remaining_count--;
		for (index = pick; index < remaining_count; index++)
			remaining[index] = remaining[index + 1];
	}
	{
		int largest = 0, team;

		for (team = 0; team < team_count; team++)
			if (team_size(parties, &current.team[team]) > largest)
				largest = team_size(parties, &current.team[team]);
		if (largest > maximum_team_size)
			return 0;
	}

	/* the swaps */
	if (optimize)
	{
		for (;;)
		{
			int a_team, b_team;

			improved = 0;
			for (a_team = 0; a_team < team_count - 1; a_team++)
				for (b_team = a_team + 1; b_team < team_count; b_team++)
				{
					struct team const *a = &current.team[a_team], *b = &current.team[b_team];
					int a_count = team_size(parties, a), b_count = team_size(parties, b);
					int difference = a_count > b_count ? a_count - b_count : b_count - a_count;
					int i, j;

					for (i = 0; i < a->party_count; i++)
						for (j = 0; j < b->party_count; j++)
						{
							struct party const *pa = &parties[a->party[i]], *pb = &parties[b->party[j]];
							int delta, new_difference;
							struct solution candidate;
							struct solution const *reference;
							int candidate_spread, reference_spread;
							long candidate_cost, reference_cost;

							if (pa->flagged != pb->flagged)
								continue;
							delta = pa->count - pb->count;
							new_difference = (a_count - delta) - (b_count + delta);
							if (new_difference < 0)
								new_difference = -new_difference;
							if (new_difference > difference)
								continue;
							candidate = current;
							team_remove_party(&candidate.team[a_team], a->party[i]);
							team_remove_party(&candidate.team[b_team], b->party[j]);
							team_add_party(&candidate.team[a_team], b->party[j]);
							team_add_party(&candidate.team[b_team], a->party[i]);
							reference = improved ? &best : &current;
							solution_measure(parties, &candidate, count, weights, &candidate_spread, &candidate_cost);
							solution_measure(parties, reference, count, weights, &reference_spread, &reference_cost);
							if (candidate_spread < reference_spread ||
								(candidate_spread == reference_spread && reference_cost > candidate_cost))
							{
								improved = 1;
								best = candidate;
							}
						}
				}
			if (!improved)
				break;
			current = best;
		}
	}
	flatten(parties, &current, team_of);
	if (cost)
		*cost = ae_rating_balance_cost(team_count, count, weights, team_of);
	return 1;
}

/* ---- bots and the offline roster ---- */

int ae_rating_bot_anchor(int difficulty)
{
	/* shown 6, 18, 31, 37, 43 and 49 (ELITE's 36 halfway between HARD's 30 and LEGENDARY's 42) */
	static int const anchor[AE_RATING_BOT_DIFFICULTIES] = {5, 17, 30, 36, 42, 48};

	if (difficulty < 0)
		difficulty = 0;
	if (difficulty >= AE_RATING_BOT_DIFFICULTIES)
		difficulty = AE_RATING_BOT_DIFFICULTIES - 1;
	return anchor[difficulty];
}

int ae_rating_bot_difficulty(int level)
{
	int best = 0, difficulty, best_distance = 1000;

	for (difficulty = 0; difficulty < AE_RATING_BOT_DIFFICULTIES; difficulty++)
	{
		int distance = level - ae_rating_bot_anchor(difficulty);

		if (distance < 0)
			distance = -distance;
		if (distance < best_distance)
		{
			best_distance = distance;
			best = difficulty;
		}
	}
	return best;
}

void ae_rating_rng_seed(struct ae_rating_rng *rng, unsigned long seed)
{
	rng->state = seed & 0xffffffffu;
	if (!rng->state)
		rng->state = 0x9e3779b9u;
}

unsigned long ae_rating_rng_below(struct ae_rating_rng *rng, unsigned long bound)
{
	unsigned long x = rng->state;

	x ^= (x << 13) & 0xffffffffu;
	x ^= x >> 17;
	x ^= (x << 5) & 0xffffffffu;
	rng->state = x & 0xffffffffu;
	return bound ? (rng->state >> 3) % bound : 0;
}

int ae_rating_roster_draw_level(struct ae_rating_window const *window, struct ae_rating_rng *rng, int player_level)
{
	int low = ae_rating_window_low(window, player_level), high = ae_rating_window_upper(window, player_level);
	int farthest, level;
	unsigned long total = 0, pick;

	if (high > AE_RATING_LEVELS - 1)
		high = AE_RATING_LEVELS - 1;
	if (low > high)
		low = high;
	farthest = player_level - low > high - player_level ? player_level - low : high - player_level;
	for (level = low; level <= high; level++)
		total += (unsigned long)(farthest - (level > player_level ? level - player_level : player_level - level) + 1);
	pick = ae_rating_rng_below(rng, total);
	for (level = low; level <= high; level++)
	{
		unsigned long weight = (unsigned long)(farthest - (level > player_level ? level - player_level : player_level - level) + 1);

		if (pick < weight)
			return level;
		pick -= weight;
	}
	return high;
}

int ae_rating_roster_make(struct ae_rating_window const *window, struct ae_rating_rng *rng, int player_level, int team_count,
	int team_size_in, struct ae_rating_roster *roster)
{
	int teams = team_count > 1 ? team_count : 0;
	int total = teams ? teams * team_size_in : team_size_in;
	long weights[AE_RATING_BALANCE_PLAYERS];
	int party[AE_RATING_BALANCE_PLAYERS], team_of[AE_RATING_BALANCE_PLAYERS];
	int index;

	memset(roster, 0, sizeof(*roster));
	if (total < 1 || total > AE_RATING_BALANCE_PLAYERS || teams > AE_RATING_BALANCE_TEAMS || team_size_in < 1)
		return 0;
	roster->count = total;
	roster->team_count = teams;
	for (index = 0; index < total; index++)
	{
		struct ae_rating_bot *bot = &roster->bot[index];

		if (index == 0)
		{
			bot->level = player_level;
			bot->difficulty = -1;
		}
		else
		{
			bot->level = ae_rating_roster_draw_level(window, rng, player_level);
			bot->difficulty = ae_rating_bot_difficulty(bot->level);
			bot->shooting_jitter = (int)ae_rating_rng_below(rng, 3) - 1;
			bot->movement_jitter = (int)ae_rating_rng_below(rng, 3) - 1;
		}
		weights[index] = bot->level;
		party[index] = index;
	}
	if (teams)
	{
		if (!ae_rating_balance_teams(teams, team_size_in, total, weights, party, 1, 1, 0, team_of, &roster->cost))
			return 0;
	}
	else
		for (index = 0; index < total; index++)
			team_of[index] = index;
	for (index = 0; index < total; index++)
		roster->bot[index].team = team_of[index];
	return 1;
}

/* ---- quit protection ---- */

static int earlier_party_quit(struct ae_rating_entry const *entries, int count, int self)
{
	int index;

	if (entries[self].party < 0)
		return 0;
	for (index = 0; index < count; index++)
		if (index != self && entries[index].quit && entries[index].quit_order >= 0 &&
			entries[index].quit_order < entries[self].quit_order && entries[index].party == entries[self].party)
			return 1;
	return 0;
}

static int first_quitter(struct ae_rating_entry const *entries, int count)
{
	int index;

	for (index = 0; index < count; index++)
		if (entries[index].quit && entries[index].quit_order == 0)
			return index;
	return -1;
}

long ae_rating_protected_change(struct ae_rating_entry const *entries, int count, int self, long points, long first_quit_ms)
{
	int first = first_quitter(entries, count);
	long change;

	if (self < 0 || self >= count)
		return 0;
	if (first < 0)
		return ae_rating_game_change(entries, count, self, points);
	if (self == first)
		return game_change(entries, count, self, points, AE_RATING_NONE);
	if (entries[self].quit)
	{
		change = 0;
		if (earlier_party_quit(entries, count, self))
			change = game_change(entries, count, self, points, AE_RATING_NONE) * AE_RATING_QUIT_PARTY_PERCENT / 100;
	}
	else
	{
		change = ae_rating_game_change(entries, count, self, points);
		if (change < 0 && (entries[self].party < 0 || entries[self].party != entries[first].party))
			change = 0;
	}
	if (first_quit_ms >= 0 && first_quit_ms < AE_RATING_QUIT_EARLY_MS)
	{
		if (change > AE_RATING_QUIT_EARLY_CAP)
			change = AE_RATING_QUIT_EARLY_CAP;
		if (change < -AE_RATING_QUIT_EARLY_CAP)
			change = -AE_RATING_QUIT_EARLY_CAP;
	}
	return change;
}

int ae_rating_protected_counts_quit(struct ae_rating_entry const *entries, int count, int self)
{
	if (self < 0 || self >= count || !entries[self].quit)
		return 0;
	if (entries[self].quit_order == 0)
		return 1;
	return entries[self].quit_order > 0 && earlier_party_quit(entries, count, self);
}
