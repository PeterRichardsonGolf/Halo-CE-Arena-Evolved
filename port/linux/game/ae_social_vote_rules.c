/* The AE TOURNAMENT lobby's map vote (see the header). Pure code. */
#include <stdio.h>
#include <string.h>

#include "ae_social_vote_rules.h"

static int same_map(char const *a, char const *b)
{
	return !strcmp(a, b);
}

static unsigned long mix(unsigned long seed, unsigned long salt)
{
	return seed * 2654435761ul + salt * 40503ul + 17ul;
}

int ae_social_vote_pick_choices(struct ae_social_playlist const *playlist, ae_social_present_fn present, void *context, int last,
	unsigned long seed, struct ae_social_vote_choice *out, int max)
{
	int pool[AE_SOCIAL_ENTRIES], count, index, other, distinct = 0, picked = 0;
	struct ae_rating_rng rng;

	if (max > AE_SOCIAL_VOTE_CHOICES)
		max = AE_SOCIAL_VOTE_CHOICES;
	if (max <= 0)
		return 0;
	count = ae_social_playlist_candidates(playlist, present, context, pool);
	for (index = 0; index < count; index++)
	{
		int seen = 0;

		for (other = 0; other < index; other++)
			seen |= same_map(playlist->entry[pool[other]].map, playlist->entry[pool[index]].map);
		distinct += !seen;
	}
	/* (the map just played stays out while two or more others are on offer) */
	if (last >= 0 && last < playlist->count && distinct - 1 >= 2)
	{
		int kept = 0;

		for (index = 0; index < count; index++)
			if (!same_map(playlist->entry[pool[index]].map, playlist->entry[last].map))
				pool[kept++] = pool[index];
		count = kept;
		distinct--;
	}
	if (distinct < 2)
		return distinct;
	ae_rating_rng_seed(&rng, mix(seed, 3ul));
	while (picked < max && picked < distinct)
	{
		unsigned long total = 0, roll;
		int usable[AE_SOCIAL_ENTRIES], usable_count = 0, take;

		for (index = 0; index < count; index++)
		{
			int taken = 0;

			for (other = 0; other < picked; other++)
				taken |= same_map(out[other].map, playlist->entry[pool[index]].map);
			if (taken)
				continue;
			usable[usable_count++] = pool[index];
			total += (unsigned long)playlist->entry[pool[index]].weight;
		}
		if (!usable_count || !total)
			break;
		roll = ae_rating_rng_below(&rng, total);
		take = usable[usable_count - 1];
		for (index = 0; index < usable_count; index++)
		{
			unsigned long weight = (unsigned long)playlist->entry[usable[index]].weight;

			if (roll < weight)
			{
				take = usable[index];
				break;
			}
			roll -= weight;
		}
		out[picked].entry = take;
		snprintf(out[picked].map, sizeof(out[picked].map), "%s", playlist->entry[take].map);
		snprintf(out[picked].gametype, sizeof(out[picked].gametype), "%s", playlist->entry[take].gametype);
		out[picked].weight = playlist->entry[take].weight;
		picked++;
	}
	return picked;
}

static int choice_by_weight(struct ae_social_vote const *vote, struct ae_rating_rng *rng)
{
	unsigned long total = 0, roll;
	int each;

	for (each = 0; each < vote->count; each++)
		total += (unsigned long)(vote->choice[each].weight > 0 ? vote->choice[each].weight : 1);
	roll = ae_rating_rng_below(rng, total);
	for (each = 0; each < vote->count; each++)
	{
		unsigned long weight = (unsigned long)(vote->choice[each].weight > 0 ? vote->choice[each].weight : 1);

		if (roll < weight)
			return each;
		roll -= weight;
	}
	return vote->count - 1;
}

void ae_social_vote_init(struct ae_social_vote *vote, struct ae_social_vote_choice const *choices, int count, int seats, int humans,
	unsigned long seed)
{
	int seat;

	memset(vote, 0, sizeof(*vote));
	if (count > AE_SOCIAL_VOTE_CHOICES)
		count = AE_SOCIAL_VOTE_CHOICES;
	if (count < 0)
		count = 0;
	if (seats > AE_SOCIAL_SEATS)
		seats = AE_SOCIAL_SEATS;
	if (seats < 0)
		seats = 0;
	if (humans > seats)
		humans = seats;
	if (humans < 0)
		humans = 0;
	vote->count = count;
	memcpy(vote->choice, choices, (size_t)count * sizeof(*choices));
	vote->seats = seats;
	vote->humans = humans;
	vote->seed = seed;
	vote->all_ms = -1;
	vote->winner = -1;
	for (seat = 0; seat < AE_SOCIAL_SEATS; seat++)
	{
		vote->ballot[seat] = -1;
		vote->cast_order[seat] = -1;
	}
	for (seat = humans; seat < seats && count > 0; seat++)
	{
		struct ae_rating_rng rng;

		ae_rating_rng_seed(&rng, mix(seed, (unsigned long)seat + 100ul));
		vote->bot_ms[seat] = AE_SOCIAL_VOTE_BOT_MIN_MS + (long)ae_rating_rng_below(&rng, AE_SOCIAL_VOTE_BOT_SPREAD_MS);
		vote->bot_pick[seat] = choice_by_weight(vote, &rng);
	}
}

static void order_remove(struct ae_social_vote *vote, int seat)
{
	int at;

	for (at = 0; at < vote->cast_count; at++)
		if (vote->cast_order[at] == seat)
		{
			for (; at + 1 < vote->cast_count; at++)
				vote->cast_order[at] = vote->cast_order[at + 1];
			vote->cast_order[--vote->cast_count] = -1;
			return;
		}
}

static void all_voted_check(struct ae_social_vote *vote)
{
	long latest = -1000000L;
	int seat;

	for (seat = 0; seat < vote->seats; seat++)
	{
		if (vote->ballot[seat] < 0)
		{
			vote->all_ms = -1;
			return;
		}
		if (vote->cast_ms[seat] > latest)
			latest = vote->cast_ms[seat];
	}
	vote->all_ms = vote->seats > 0 ? latest : -1;
}

static int cast_at(struct ae_social_vote *vote, int seat, int choice, long ms)
{
	if (seat < 0 || seat >= vote->seats || choice < 0 || choice >= vote->count || vote->ballot[seat] == choice)
		return 0;
	if (vote->ballot[seat] >= 0)
		order_remove(vote, seat);
	vote->ballot[seat] = choice;
	vote->cast_ms[seat] = ms;
	vote->cast_order[vote->cast_count++] = seat;
	all_voted_check(vote);
	return 1;
}

int ae_social_vote_cast(struct ae_social_vote *vote, int seat, int choice)
{
	if (vote->closed || seat < 0 || seat >= vote->humans)
		return 0;
	return cast_at(vote, seat, choice, vote->elapsed);
}

/* the bots that have not voted, in the order of their times, up to ms (-1: all of them, at the current time) */
static void bots_vote(struct ae_social_vote *vote, long up_to, int all)
{
	for (;;)
	{
		int seat, best = -1;

		for (seat = vote->humans; seat < vote->seats; seat++)
			if (vote->ballot[seat] < 0 && (all || vote->bot_ms[seat] <= up_to) &&
				(best < 0 || vote->bot_ms[seat] < vote->bot_ms[best]))
				best = seat;
		if (best < 0)
			return;
		cast_at(vote, best, vote->bot_pick[best], all && vote->bot_ms[best] > up_to ? up_to : vote->bot_ms[best]);
	}
}

static void decide(struct ae_social_vote *vote)
{
	struct ae_rating_rng rng;
	int tally[AE_SOCIAL_VOTE_CHOICES] = {0, 0, 0}, most = 0, tied[AE_SOCIAL_VOTE_CHOICES], tied_count = 0, seat, each;

	for (seat = 0; seat < vote->seats; seat++)
		if (vote->ballot[seat] >= 0)
			tally[vote->ballot[seat]]++;
	for (each = 0; each < vote->count; each++)
		if (tally[each] > most)
			most = tally[each];
	for (each = 0; each < vote->count; each++)
		if (tally[each] == most)
			tied[tied_count++] = each;
	ae_rating_rng_seed(&rng, mix(vote->seed, 7ul));
	vote->winner = tied_count > 0 ? tied[ae_rating_rng_below(&rng, (unsigned long)tied_count)] : -1;
	vote->tie = tied_count > 1;
}

int ae_social_vote_close(struct ae_social_vote *vote)
{
	if (vote->closed)
		return 0;
	bots_vote(vote, vote->elapsed, 1);
	vote->closed = 1;
	decide(vote);
	return 1;
}

void ae_social_vote_advance(struct ae_social_vote *vote, long elapsed_ms)
{
	if (vote->closed)
		return;
	if (elapsed_ms > AE_SOCIAL_VOTE_MS)
		elapsed_ms = AE_SOCIAL_VOTE_MS;
	if (elapsed_ms <= vote->elapsed)
		return;
	vote->elapsed = elapsed_ms;
	bots_vote(vote, elapsed_ms, 0);
	if (vote->elapsed >= AE_SOCIAL_VOTE_MS || (vote->all_ms >= 0 && vote->elapsed >= vote->all_ms + AE_SOCIAL_VOTE_LINGER_MS))
		ae_social_vote_close(vote);
}

void ae_social_vote_preset(struct ae_social_vote *vote, int const *counts, int count)
{
	int each, seat = vote->humans;

	for (each = 0; each < count && each < vote->count; each++)
	{
		int number;

		for (number = 0; number < counts[each] && seat < vote->seats; number++, seat++)
			cast_at(vote, seat, each, -AE_SOCIAL_VOTE_FRESH_MS);
	}
}

long ae_social_vote_left_ms(struct ae_social_vote const *vote)
{
	long left = AE_SOCIAL_VOTE_MS - vote->elapsed;

	return vote->closed || left < 0 ? 0 : left;
}

int ae_social_vote_voted(struct ae_social_vote const *vote)
{
	return vote->cast_count;
}

int ae_social_vote_count(struct ae_social_vote const *vote, int choice)
{
	int seat, total = 0;

	for (seat = 0; seat < vote->seats; seat++)
		total += vote->ballot[seat] == choice && choice >= 0;
	return total;
}

int ae_social_vote_voter(struct ae_social_vote const *vote, int choice, int index)
{
	int at;

	for (at = 0; at < vote->cast_count; at++)
		if (vote->ballot[vote->cast_order[at]] == choice && index-- == 0)
			return vote->cast_order[at];
	return -1;
}

int ae_social_vote_fresh(struct ae_social_vote const *vote, int seat)
{
	return seat >= vote->humans && seat < vote->seats && vote->ballot[seat] >= 0 && !vote->closed &&
		vote->elapsed - vote->cast_ms[seat] < AE_SOCIAL_VOTE_FRESH_MS;
}
