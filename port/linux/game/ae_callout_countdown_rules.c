/*
AE_CALLOUT_COUNTDOWN_RULES.C

CALLOUTS: ITEMS + COUNTDOWN's calls by the time left (ae_callout_countdown_rules.h).
*/

#include "ae_callout_countdown_rules.h"

#include <stddef.h>

/* the marks before the end, in seconds: their beep and their words */
static long const countdown_mark_seconds[] = { 5 * 60, 60, 30 };
static int const countdown_mark_beeps[] = { AE_CALLOUT_COUNTDOWN_BEEP_MINUTE, AE_CALLOUT_COUNTDOWN_BEEP_MINUTE,
	AE_CALLOUT_COUNTDOWN_BEEP_TICK };
static int const countdown_mark_words[] = { AE_CALLOUT_COUNTDOWN_FIVE_MINUTES, AE_CALLOUT_COUNTDOWN_ONE_MINUTE,
	AE_CALLOUT_COUNTDOWN_THIRTY_SECONDS };

/* the last seconds counted */
#define COUNTDOWN_COUNT_SECONDS 10

long ae_callout_countdown_end(
	long time_limit_minutes,
	long extra_ticks,
	long ticks_per_second)
{
	if (time_limit_minutes <= 0 || ticks_per_second <= 0)
		return AE_CALLOUT_COUNTDOWN_NO_END;
	return time_limit_minutes * 60L * ticks_per_second + extra_ticks;
}

int ae_callout_countdown_at(
	long end,
	long tick,
	long ticks_per_second,
	long speech_ticks,
	int minimal,
	int *number)
{
	long left = end - tick;
	size_t index;

	if (number)
		*number = 0;
	if (end == AE_CALLOUT_COUNTDOWN_NO_END || ticks_per_second <= 0 || left <= 0)
		return AE_CALLOUT_COUNTDOWN_NONE;
	for (index = 0; index < sizeof(countdown_mark_seconds) / sizeof(countdown_mark_seconds[0]); index++)
	{
		long mark = countdown_mark_seconds[index] * ticks_per_second;

		/* (a mark at or before the game's start is not called) */
		if (end - mark <= 0)
			continue;
		if (left == mark)
			return minimal ? AE_CALLOUT_COUNTDOWN_NONE : countdown_mark_beeps[index];
		if (left == mark - speech_ticks)
			return countdown_mark_words[index];
	}
	if (!minimal && left % ticks_per_second == 0 && left / ticks_per_second <= COUNTDOWN_COUNT_SECONDS)
	{
		if (number)
			*number = (int)(left / ticks_per_second);
		return AE_CALLOUT_COUNTDOWN_NUMBER;
	}

	return AE_CALLOUT_COUNTDOWN_NONE;
}
