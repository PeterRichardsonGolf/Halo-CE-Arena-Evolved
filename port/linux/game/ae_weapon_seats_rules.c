/*
AE_WEAPON_SEATS_RULES.C

Which seats decide whether a weapon pack's weapon can be held by its label (ae_weapon_seats_rules.h).
*/

#include <ctype.h>
#include <stddef.h>

#include "ae_weapon_seats_rules.h"

int ae_weapon_seats_counted(char const *name)
{
	char const *alert = "alert";

	if (!name)
		return 1;
	for (; *alert && *name; alert++, name++)
		if (tolower((unsigned char)*alert) != tolower((unsigned char)*name))
			return 1;
	return *alert || *name;
}

short ae_weapon_seats_lacking(short count, char const *const *names, unsigned char const *holds,
	unsigned char const *has, short *holding, short *first)
{
	short index, missing = 0;

	*holding = 0;
	*first = -1;
	for (index = 0; index < count; index++)
	{
		if (!ae_weapon_seats_counted(names ? names[index] : NULL) || (!holds[index] && !has[index]))
			continue;
		(*holding)++;
		if (!has[index])
		{
			if (!missing)
				*first = index;
			missing++;
		}
	}
	return missing;
}
