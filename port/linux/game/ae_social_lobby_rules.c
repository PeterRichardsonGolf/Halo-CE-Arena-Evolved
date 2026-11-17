/* The social lobby's clock (see the header). Pure code. */
#include <stdio.h>
#include <string.h>

#include "ae_social_lobby_rules.h"
#include "ae_social_names_rules.h"

void ae_social_lobby_init(struct ae_social_lobby *lobby, struct ae_social_roster const *roster, unsigned long seed)
{
	struct ae_rating_rng rng;
	long gap_total = 0, last, search;
	int index, seat;

	memset(lobby, 0, sizeof(*lobby));
	ae_rating_rng_seed(&rng, seed ^ 0x5bd1e995ul);
	lobby->seats = roster->count;
	lobby->humans = roster->humans;
	lobby->fixed = roster->fixed != 0;
	lobby->span = roster->fixed ? 0 : roster->span;
	lobby->tier_level = roster->player_level - 1;
	search = AE_SOCIAL_LOBBY_SEARCH_MIN_MS + (long)ae_rating_rng_below(&rng, AE_SOCIAL_LOBBY_SEARCH_SPREAD_MS);
	if (!lobby->fixed && (long)(lobby->span + 1) * AE_SOCIAL_LOBBY_STEP_MS < search)
		search = (long)(lobby->span + 1) * AE_SOCIAL_LOBBY_STEP_MS;
	if (search < AE_SOCIAL_LOBBY_STEP_MS)
		search = AE_SOCIAL_LOBBY_STEP_MS;
	lobby->found_ms = search;
	lobby->order_count = ae_social_roster_join_order(roster, lobby->order);
	for (index = 0; index < lobby->order_count; index++)
	{
		gap_total += AE_SOCIAL_LOBBY_GAP_MIN_MS + (long)ae_rating_rng_below(&rng, AE_SOCIAL_LOBBY_GAP_SPREAD_MS);
		lobby->join_ms[lobby->order[index]] = gap_total;
	}
	/* (all of it inside the cap: the joins squeezed into the time after the search) */
	last = gap_total;
	if (last > 0 && search + last > AE_SOCIAL_LOBBY_CAP_MS)
	{
		long room = AE_SOCIAL_LOBBY_CAP_MS - search;

		if (room < 0)
			room = 0;
		for (index = 0; index < lobby->order_count; index++)
			lobby->join_ms[lobby->order[index]] = lobby->join_ms[lobby->order[index]] * room / last;
		last = room;
	}
	for (index = 0; index < lobby->order_count; index++)
		lobby->join_ms[lobby->order[index]] += search;
	for (seat = 0; seat < lobby->humans; seat++)
		lobby->join_ms[seat] = 0;
	lobby->starting_ms = search + last;
	lobby->stage = AE_SOCIAL_STAGE_SEARCHING;
}

static void follow(struct ae_social_lobby *lobby)
{
	if (lobby->stage == AE_SOCIAL_STAGE_GO || lobby->stage == AE_SOCIAL_STAGE_CANCELLED)
		return;
	if (lobby->forced || lobby->elapsed >= lobby->starting_ms + AE_SOCIAL_LOBBY_STARTING_MS)
		lobby->stage = AE_SOCIAL_STAGE_GO;
	else if (lobby->elapsed >= lobby->starting_ms)
		lobby->stage = AE_SOCIAL_STAGE_STARTING;
	else if (lobby->elapsed >= lobby->found_ms)
		lobby->stage = AE_SOCIAL_STAGE_FOUND;
	else
		lobby->stage = AE_SOCIAL_STAGE_SEARCHING;
}

void ae_social_lobby_advance(struct ae_social_lobby *lobby, long ms)
{
	if (ms < 0)
		ms = 0;
	lobby->elapsed += ms;
	follow(lobby);
}

int ae_social_lobby_skip(struct ae_social_lobby *lobby)
{
	if (lobby->stage == AE_SOCIAL_STAGE_GO || lobby->stage == AE_SOCIAL_STAGE_CANCELLED)
		return 0;
	lobby->forced = 1;
	lobby->stage = AE_SOCIAL_STAGE_GO;
	return 1;
}

int ae_social_lobby_cancel(struct ae_social_lobby *lobby)
{
	if (lobby->stage == AE_SOCIAL_STAGE_GO || lobby->stage == AE_SOCIAL_STAGE_CANCELLED)
		return 0;
	lobby->stage = AE_SOCIAL_STAGE_CANCELLED;
	return 1;
}

int ae_social_lobby_step(struct ae_social_lobby const *lobby)
{
	long step;

	if (lobby->fixed)
		return 0;
	step = (lobby->stage == AE_SOCIAL_STAGE_SEARCHING ? lobby->elapsed : lobby->found_ms) / AE_SOCIAL_LOBBY_STEP_MS;
	if (lobby->forced)
		step = lobby->span;
	return step > lobby->span ? lobby->span : (int)step;
}

int ae_social_lobby_seat_in(struct ae_social_lobby const *lobby, int seat)
{
	if (seat < 0 || seat >= lobby->seats)
		return 0;
	return lobby->stage >= AE_SOCIAL_STAGE_STARTING || lobby->forced || lobby->join_ms[seat] <= lobby->elapsed;
}

int ae_social_lobby_joined(struct ae_social_lobby const *lobby)
{
	int seat, count = 0;

	for (seat = 0; seat < lobby->seats; seat++)
		count += ae_social_lobby_seat_in(lobby, seat);
	return count;
}

int ae_social_lobby_seat_new(struct ae_social_lobby const *lobby, int seat)
{
	if (seat < lobby->humans || seat >= lobby->seats || !ae_social_lobby_seat_in(lobby, seat) || lobby->forced)
		return 0;
	return lobby->elapsed - lobby->join_ms[seat] < AE_SOCIAL_LOBBY_STEP_MS * 6;
}

long ae_social_lobby_countdown_ms(struct ae_social_lobby const *lobby)
{
	long left;

	if (lobby->stage != AE_SOCIAL_STAGE_STARTING)
		return 0;
	left = lobby->starting_ms + AE_SOCIAL_LOBBY_STARTING_MS - lobby->elapsed;
	return left > 0 ? left : 0;
}

int ae_social_lobby_names(unsigned long seed, int count, char out[][AE_SOCIAL_LOBBY_NAME])
{
	int index;

	int picked[AE_SOCIAL_SEATS];
	int got;

	if (count < 0 || count > AE_SOCIAL_SEATS)
		return 0;
	/* the bots' pool (seeded, all different); "BOT n" only if the pool ran short */
	got = count ? ae_social_names_pick(seed, count, NULL, 0, picked) : 0;
	for (index = 0; index < count; index++) {
		char const *name = index < got ? ae_social_names_get(picked[index]) : NULL;

		if (name)
			snprintf(out[index], AE_SOCIAL_LOBBY_NAME, "%s", name);
		else
			snprintf(out[index], AE_SOCIAL_LOBBY_NAME, "BOT %d", index + 1);
	}
	return 1;
}
