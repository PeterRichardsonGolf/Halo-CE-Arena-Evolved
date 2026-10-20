/*
AE_BOTS_FALLS_RULES.C

Arena Evolved: the bots' deaths by their own hand (ae_bots_falls_rules.h).
*/

#include <stddef.h>
#include "ae_bots_falls_rules.h"

static char const *const ae_bots_death_cause_names[AE_BOTS_DEATH_CAUSES] =
{
	"other", "player", "fall", "pit", "barrier", "own_grenade", "own_explosive", "self"
};

char const *ae_bots_death_cause_name(int cause)
{
	if (cause < 0 || cause >= AE_BOTS_DEATH_CAUSES)
		return "other";
	return ae_bots_death_cause_names[cause];
}

static char ae_bots_falls_lower(char c)
{
	return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

/* whether a tag name holds a word (ignoring case) */
static int ae_bots_falls_has(char const *name, char const *word)
{
	char const *start;

	if (!name)
		return 0;
	for (start = name; *start; start++)
	{
		char const *a = start, *b = word;

		while (*a && *b && ae_bots_falls_lower(*a) == *b)
		{
			a++;
			b++;
		}
		if (!*b)
			return 1;
	}
	return 0;
}

/* the last part of a tag name (after its last backslash) */
static char const *ae_bots_falls_leaf(char const *name)
{
	char const *leaf = name, *c;

	for (c = name; *c; c++)
	{
		if (*c == '\\' || *c == '/')
			leaf = c + 1;
	}
	return leaf;
}

int ae_bots_death_cause(int known, char const *damage_name, int owner_self, int owner_none, int kill_instantly,
	int riding)
{
	if (!known)
		return AE_BOTS_DEATH_OTHER;
	/* (a rider dies with its vehicle, an act of god too: not the map's kill) */
	if (riding && !owner_self && owner_none)
		return AE_BOTS_DEATH_OTHER;
	if (owner_self)
	{
		if (ae_bots_falls_has(damage_name, "grenade"))
			return AE_BOTS_DEATH_OWN_GRENADE;
		if (ae_bots_falls_has(damage_name, "explosion") || ae_bots_falls_has(damage_name, "rocket") ||
			ae_bots_falls_has(damage_name, "plasma_cannon") || ae_bots_falls_has(damage_name, "fuel rod"))
		{
			return AE_BOTS_DEATH_OWN_EXPLOSIVE;
		}
		return AE_BOTS_DEATH_SELF;
	}
	if (!owner_none)
		return AE_BOTS_DEATH_PLAYER;
	/* (unit_kill: the globals' falling damage, at once, no owner) */
	if (kill_instantly)
		return AE_BOTS_DEATH_BARRIER;
	if (damage_name)
	{
		char const *leaf = ae_bots_falls_leaf(damage_name);

		if (ae_bots_falls_has(leaf, "distance"))
			return AE_BOTS_DEATH_PIT;
		if (ae_bots_falls_has(leaf, "falling"))
			return AE_BOTS_DEATH_FALL;
	}
	return AE_BOTS_DEATH_OTHER;
}
