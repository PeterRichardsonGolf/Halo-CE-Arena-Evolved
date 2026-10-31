/* ae_lobby_profile_choices.c: the profile choices for a player joining AE's lobby. Pure. */

#include "ae_lobby_profile_choices.h"

static int in_use(int profile, int const *used, short count)
{
	short index;

	for (index = 0; used && index < count; index++)
		if (used[index] == profile)
			return 1;
	return 0;
}

short ae_lobby_profile_choices(int const *profiles, short profile_count, int const *used, short used_count,
	int current, int current_guest, int *choices, short maximum, short *focus)
{
	short index, count = 0;

	if (focus)
		*focus = 0;
	if (!choices || maximum <= 0)
		return 0;
	/* Keep one place for GUEST. */
	for (index = 0; profiles && index < profile_count && count < maximum - 1; index++)
	{
		if (profiles[index] == AE_LOBBY_PROFILE_GUEST || in_use(profiles[index], used, used_count))
			continue;
		choices[count++] = profiles[index];
	}
	choices[count] = AE_LOBBY_PROFILE_GUEST;
	if (focus && !current_guest)
	{
		for (index = 0; index < count; index++)
			if (choices[index] == current)
			{
				*focus = index;
				return (short)(count + 1);
			}
	}
	if (focus)
		*focus = count;
	return (short)(count + 1);
}

short ae_lobby_profile_refocus(int const *choices, short count, int focused, short previous)
{
	short index;

	if (!choices || count <= 0)
		return 0;
	for (index = 0; index < count; index++)
		if (choices[index] == focused)
			return index;
	return previous < 0 ? 0 : previous >= count ? (short)(count - 1) : previous;
}
