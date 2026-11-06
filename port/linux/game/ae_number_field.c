/* ae_number_field.c: the typed whole-number fields' ranges and texts (see ae_number_field.h). No engine includes. */

#include <stdio.h>
#include <string.h>

#include "ae_number_field.h"
#include "ae_setup_options.h"

int ae_number_typing_ok(const char *typed, int digits)
{
	int length = 0;

	if (!typed)
		return 0;
	for (; *typed; typed++, length++)
		if (*typed < '0' || *typed > '9' || length >= digits)
			return 0;
	return 1;
}

int ae_number_parse(const char *typed, long *value)
{
	long result = 0;
	int length = 0;

	if (!typed || !*typed)
		return 0;
	for (; *typed; typed++, length++)
	{
		if (*typed < '0' || *typed > '9' || length >= 9)
			return 0;
		result = result * 10 + (*typed - '0');
	}
	if (value)
		*value = result;
	return 1;
}

long ae_number_clamp(struct ae_number_range const *range, long value)
{
	if (value == 0 && range->zero_word)
		return 0;
	return value < range->minimum ? range->minimum : value > range->maximum ? range->maximum : value;
}

void ae_number_format(struct ae_number_range const *range, long value, char *text, size_t size)
{
	const char *unit = value == 1 ? range->unit_one : range->unit_many;

	if (value == 0 && range->zero_word)
		snprintf(text, size, "%s", range->zero_word);
	else if (unit)
		snprintf(text, size, "%ld %s", value, unit);
	else
		snprintf(text, size, "%ld", value);
}

void ae_number_hint(struct ae_number_range const *range, char *text, size_t size)
{
	if (range->zero_word)
		snprintf(text, size, "0 FOR %s, OR %ld TO %ld", range->zero_word, range->minimum, range->maximum);
	else
		snprintf(text, size, "%ld TO %ld", range->minimum, range->maximum);
}

/* ---------- GAME OPTIONS' rows. The stored fields (glue: ae_glue_setup_options.c's encodings): the score is the
gametype's long, LIVES and the respawn times its longs (ticks), TIME LIMIT a short of the PC options. The scores run
to what the lists reach, no further: the HUDs draw a score in three digits */

enum { LONG_MAXIMUM = 0x7FFFFFFF, SHORT_MAXIMUM = 0x7FFF, TICKS = 30 };

static struct { int option; struct ae_number_option entry; } const options[] =
{
	{ AE_SO_KILLS_TO_WIN, { { 1, 500, NULL, NULL, NULL }, 1, LONG_MAXIMUM } },
	{ AE_SO_CAPTURES_TO_WIN, { { 1, 99, NULL, NULL, NULL }, 1, LONG_MAXIMUM } },
	{ AE_SO_HILL_SCORE, { { 1, 60, "MINUTE", "MINUTES", NULL }, 1, LONG_MAXIMUM } },
	{ AE_SO_BALL_SCORE, { { 1, 60, "MINUTE", "MINUTES", NULL }, 1, LONG_MAXIMUM } },
	{ AE_SO_LAPS_TO_WIN, { { 1, 99, NULL, NULL, NULL }, 1, LONG_MAXIMUM } },
	{ AE_SO_ROUNDS_TO_WIN, { { 1, 50, NULL, NULL, NULL }, 1, LONG_MAXIMUM } },
	{ AE_SO_TIME_LIMIT, { { 1, 99, "MINUTE", "MINUTES", "NONE" }, 1, SHORT_MAXIMUM } },
	{ AE_SO_LIVES, { { 1, 25, NULL, NULL, "UNLIMITED" }, 1, LONG_MAXIMUM } },
	{ AE_SO_RESPAWN_TIME, { { 1, 60, "SECOND", "SECONDS", "INSTANT" }, TICKS, LONG_MAXIMUM } },
	{ AE_SO_RESPAWN_GROWTH, { { 1, 60, "SECOND", "SECONDS", "NONE" }, TICKS, LONG_MAXIMUM } },
	{ AE_SO_SUICIDE_PENALTY, { { 1, 60, "SECOND", "SECONDS", "NONE" }, TICKS, LONG_MAXIMUM } },
};

int ae_number_option(int option, struct ae_number_option *out)
{
	size_t index;

	for (index = 0; index < sizeof(options) / sizeof(options[0]); index++)
		if (options[index].option == option)
		{
			if (out)
				*out = options[index].entry;
			return 1;
		}
	return 0;
}
