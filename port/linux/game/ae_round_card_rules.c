/* The round card's rules (see the header) */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ae_round_card_rules.h"

int ae_round_card_verdict(unsigned char last_result, int teams, int viewer_team, int viewer_slot)
{
	if (last_result == AE_ROUND_WIRE_RESULT_TIE)
		return AE_ROUND_CARD_TIED;
	if (last_result >= AE_ROUND_WIRE_PLAYERS)
		return AE_ROUND_CARD_NONE;
	if (teams)
		return viewer_team == (int)last_result ? AE_ROUND_CARD_WON : AE_ROUND_CARD_LOST;
	return viewer_slot == (int)last_result ? AE_ROUND_CARD_WON : AE_ROUND_CARD_LOST;
}

int ae_round_card_reason(int won, int winner, int teams, int exterminated, int timed_out)
{
	if (timed_out)
		return AE_ROUND_REASON_TIME_RAN_OUT;
	if (!won)
		return AE_ROUND_REASON_NONE;
	if (!teams)
		return AE_ROUND_REASON_LAST_STANDING;
	if (exterminated)
		return AE_ROUND_REASON_EXTERMINATION;
	/* (the winning side's enemy is the one eliminated) */
	if (winner == 0)
		return AE_ROUND_REASON_BLUE_ELIMINATED;
	if (winner == 1)
		return AE_ROUND_REASON_RED_ELIMINATED;
	return AE_ROUND_REASON_NONE;
}

char const *ae_round_card_reason_text(int reason)
{
	static char const *const text[AE_ROUND_REASON_COUNT] =
	{
		NULL, "RED TEAM ELIMINATED", "BLUE TEAM ELIMINATED", "BOMB DETONATED", "BOMB DEFUSED", "TIME RAN OUT",
		"EXTERMINATION", "LAST ONE STANDING", "LEVEL ON KILLS"
	};

	return reason > 0 && reason < AE_ROUND_REASON_COUNT ? text[reason] : NULL;
}

int ae_round_card_mvp(int count, long const *slot, long const *kills, long const *deaths, long const *side,
	long winner_side)
{
	int best = -1, index;

	for (index = 0; index < count; index++)
	{
		if (kills[index] <= 0)
			continue;
		if (best < 0)
		{
			best = index;
			continue;
		}
		if (kills[index] != kills[best])
		{
			if (kills[index] > kills[best])
				best = index;
			continue;
		}
		if (deaths[index] != deaths[best])
		{
			if (deaths[index] < deaths[best])
				best = index;
			continue;
		}
		{
			int mine = winner_side >= 0 && side[index] == winner_side, held = winner_side >= 0 && side[best] == winner_side;

			if (mine != held)
			{
				if (mine)
					best = index;
				continue;
			}
		}
		if (slot[index] < slot[best])
			best = index;
	}
	return best;
}

void ae_round_card_footer(int next_round, int max_rounds, int starts_in, char *out, int size)
{
	(void)starts_in;
	if (max_rounds > 0)
		snprintf(out, (size_t)size, "NEXT  \xC2\xB7  ROUND %d OF %d", next_round, max_rounds);
	else
		snprintf(out, (size_t)size, "NEXT  \xC2\xB7  ROUND %d", next_round);
}

void ae_round_card_kills_text(int kills, char *out, int size)
{
	snprintf(out, (size_t)size, kills == 1 ? "%d kill" : "%d kills", kills);
}

/* an exponential ease-out, 0 at x 0 and 1 at x 1 */
static float ease_out(float x)
{
	if (x <= 0.0f)
		return 0.0f;
	if (x >= 1.0f)
		return 1.0f;
	return (1.0f - powf(2.0f, -10.0f * x)) / (1.0f - powf(2.0f, -10.0f));
}

float ae_round_card_alpha(float seconds)
{
	float const fade_in = 0.15f, fade_out = 0.25f, total = (float)AE_ROUND_CARD_SECONDS;

	if (!(seconds >= 0.0f) || seconds >= total)
		return 0.0f;
	if (seconds < fade_in)
		return ease_out(seconds / fade_in);
	if (seconds > total - fade_out)
		return 1.0f - ease_out((seconds - (total - fade_out)) / fade_out);
	return 1.0f;
}

int ae_round_build_has_card(char const *build)
{
	static char const prefix[] = "Arena Evolved ";
	int part[3] = { 0, 0, 0 }, at = 0, digits = 0;

	if (!build)
		return 0;
	for (char const *c = prefix; *c; c++, build++)
		if (*build != *c)
			return 0;
	for (; *build && at < 3; build++)
	{
		if (*build >= '0' && *build <= '9')
		{
			part[at] = part[at] * 10 + (*build - '0');
			digits++;
			if (part[at] > 9999)
				return 0;
		}
		else if (*build == '.' && digits && at < 2)
			at++, digits = 0;
		else
			break;
	}
	if (!digits || at < 2)
		return 0;
	if (part[0] != 0)
		return part[0] > 0;
	if (part[1] != 7)
		return part[1] > 7;
	if (part[2] != 9)
	{
		size_t length;

		if (part[2] > 9)
			return 1;
		/* (0.7.8: only the dev builds, whose name ends -dev) */
		length = strlen(build);
		return part[2] == 8 && length >= 4 && !strcmp(build + length - 4, "-dev");
	}
	return 1;
}
