/* The post-game lobby's rules (see the header). Pure code. */
#include <stdio.h>
#include <string.h>

#include "ae_rematch_rules.h"

static unsigned long mix(unsigned long seed, unsigned long salt)
{
	unsigned long value = seed * 2654435761ul + salt * 40503ul + 17ul;

	value ^= value >> 13;
	value *= 2246822519ul;
	value ^= value >> 15;
	return value;
}

int ae_rematch_leavers(unsigned long seed, int game, int count, int const *may_leave, int *out, int max)
{
	int at, leaving = 0, cap = count / 4;

	if (cap > AE_REMATCH_LEAVE_MAX)
		cap = AE_REMATCH_LEAVE_MAX;
	if (cap > max)
		cap = max;
	for (at = 0; at < count && leaving < cap; at++)
		if (may_leave[at] && mix(seed, (unsigned long)(game * 101 + at)) % 100ul < (unsigned long)AE_REMATCH_LEAVE_PERCENT)
			out[leaving++] = at;
	return leaving;
}

void ae_still_there_init(struct ae_still_there *state)
{
	memset(state, 0, sizeof(*state));
}

void ae_still_there_input(struct ae_still_there *state)
{
	state->idle_games = 0;
	state->asked = 0;
}

int ae_still_there_auto_start(struct ae_still_there *state, int input_seen)
{
	if (input_seen)
		state->idle_games = 0;
	else if (state->idle_games >= AE_REMATCH_IDLE_GAMES)
	{
		state->asked = 1;
		return 0;
	}
	if (!input_seen)
		state->idle_games++;
	return 1;
}

int ae_still_there_asking(struct ae_still_there const *state)
{
	return state->asked;
}

void ae_rematch_staying_text(int staying, int total, char *out, int size)
{
	snprintf(out, (size_t)size, "%d of %d staying", staying, total);
}

void ae_rematch_sub_text(int online, int party, int staying, int total, int votes, int voters, char *out, int size)
{
	size_t used;

	if (online)
		snprintf(out, (size_t)size, "%d of %d staying", staying, total);
	else
		snprintf(out, (size_t)size, "Rematch");
	if (party > 1)
	{
		used = strlen(out);
		snprintf(out + used, (size_t)size - used, "  /  your party of %d stays", party);
	}
	if (voters > 0)
	{
		used = strlen(out);
		snprintf(out + used, (size_t)size - used, "  /  votes in %d / %d", votes, voters);
	}
}

void ae_rematch_news(int kind, char const *name, int tier, char *out, int size)
{
	switch (kind)
	{
	case AE_NEWS_LEFT:
		snprintf(out, (size_t)size, "%s left the lobby.", name);
		break;
	case AE_NEWS_JOINED_BOT:
		snprintf(out, (size_t)size, "%s joined (BOT, tier %d).", name, tier);
		break;
	case AE_NEWS_JOINED:
		snprintf(out, (size_t)size, "%s joined.", name);
		break;
	case AE_NEWS_STAYING:
		snprintf(out, (size_t)size, "%s is staying.", name);
		break;
	case AE_NEWS_INVITE_SENT:
		snprintf(out, (size_t)size, "Invite sent to %s.", name);
		break;
	case AE_NEWS_PARTY_LEFT:
		snprintf(out, (size_t)size, "%s left your party.", name);
		break;
	case AE_NEWS_PARTY_FULL:
		snprintf(out, (size_t)size, "Your party is full (%d seats).", tier);
		break;
	case AE_NEWS_PARTY_JOINED:
		snprintf(out, (size_t)size, "%s joined your party.", name);
		break;
	default:
		out[0] = 0;
		break;
	}
}

char const *ae_rematch_choice_word(int index)
{
	return index == 0 ? "SAME AGAIN" : "";
}

int ae_rematch_fill_in(unsigned long seed, char const taken[][24], int count, char *out)
{
	char pool[AE_SOCIAL_SEATS][AE_SOCIAL_LOBBY_NAME];
	int each, other;

	if (!ae_social_lobby_names(seed, AE_SOCIAL_SEATS, pool))
		return 0;
	for (each = 0; each < AE_SOCIAL_SEATS; each++)
	{
		int used = 0;

		for (other = 0; other < count; other++)
			used |= !strcmp(taken[other], pool[each]);
		if (!used)
		{
			snprintf(out, 24, "%s", pool[each]);
			return 1;
		}
	}
	return 0;
}
