/*
AE_TARGET_FRAMERATE_RULES.C

TARGET FRAMERATE: the choices, the earlier boolean's carry-over and the rate held (ae_target_framerate_rules.h).
*/

#include <ctype.h>
#include <stdlib.h>

#include "ae_target_framerate_rules.h"

static const char *const words[] = { AE_TARGET_FRAMERATE_WORDS };

static int same_word(const char *a, const char *b)
{
	for (; *a && *b; a++, b++)
	{
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	}
	return !*a && !*b;
}

int ae_target_framerate_choice(const char *word)
{
	int index;

	if (!word || !*word)
		return 0;
	for (index = 0; index < AE_TARGET_FRAMERATE_COUNT; index++)
	{
		if (!same_word(word, words[index]))
			continue;
		if (same_word(word, "display"))
			return AE_TARGET_FRAMERATE_DISPLAY;
		return atoi(words[index]);
	}
	return 0;
}

const char *ae_target_framerate_carry_over(int earlier_on)
{
	return AE_TARGET_FRAMERATE_CARRY_OVER(earlier_on);
}

float ae_target_framerate_hz(int choice, int vsync, float refresh_hz, long max_fps)
{
	float refresh = refresh_hz > 0.0f ? refresh_hz : AE_TARGET_FRAMERATE_UNKNOWN_HZ;
	float hz, reach;

	if (choice == AE_TARGET_FRAMERATE_DISPLAY)
		hz = refresh;
	else if (choice > 0)
		hz = (float)choice;
	else
		return 0.0f;
	/* (what the frames can reach: V-SYNC shows no more than the refresh; without it FRAME RATE LIMIT's cap) */
	if (vsync)
		reach = refresh;
	else if (max_fps > 0)
		reach = (float)max_fps;
	else if (max_fps == 0)
		reach = 2.0f * refresh;
	else
		reach = 0.0f;
	return reach > 0.0f && hz > reach ? reach : hz;
}
