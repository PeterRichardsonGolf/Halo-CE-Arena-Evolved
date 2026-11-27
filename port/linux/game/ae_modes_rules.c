/*
AE_MODES_RULES.C

Arena Evolved's modes' rules (ae_modes_rules.h). No game dependency, so the
unit test builds it alone.
*/

#include <string.h>

#include "ae_modes_rules.h"

/* whether text has part in it, whatever its case */
static int ae_modes_rules_has(
	char const *text,
	char const *part)
{
	char lower[256];
	size_t index;

	for (index = 0; index < sizeof(lower) - 1 && text[index]; index++)
		lower[index] = (char)(text[index] >= 'A' && text[index] <= 'Z' ? text[index] - 'A' + 'a' : text[index]);
	lower[index] = 0;
	return strstr(lower, part) != NULL;
}

void ae_modes_rules_round_for_kill(
	short *loaded,
	short *carried,
	short loaded_maximum)
{
	if (*loaded < loaded_maximum)
		(*loaded)++;
	else
		(*carried)++;
}

void ae_modes_rules_grenades_for_kill(
	char const *damage_tag_path,
	short *frags,
	short *plasmas)
{
	*frags = 0;
	*plasmas = 0;
	if (!damage_tag_path)
		return;
	/* (the stock tags: weapons\frag grenade\explosion, weapons\plasma
	grenade\explosion, weapons\plasma grenade\attached) */
	if (ae_modes_rules_has(damage_tag_path, "frag"))
		*frags = 1;
	else if (ae_modes_rules_has(damage_tag_path, "plasma grenade") || ae_modes_rules_has(damage_tag_path, "plasma_grenade"))
		*plasmas = ae_modes_rules_has(damage_tag_path, "attached") ? 2 : 1;
}

short ae_modes_rules_add_capped(
	short count,
	short add,
	short maximum)
{
	if (count >= maximum)
		return count;
	return (short)(count + add > maximum ? maximum : count + add);
}
