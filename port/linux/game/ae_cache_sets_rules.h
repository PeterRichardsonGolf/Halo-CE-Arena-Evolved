/*
AE_CACHE_SETS_RULES.H

How a cache tag set's tags are named for a lookup, pure (port/linux/tests/ae_cache_sets_rules_test.c). The weapon cache holds the
vehicle damage and the bomb as TAG SETS (ae_weapon_cache_format.h, AE_CACHE_ID_VEHICLE_DAMAGE / AE_CACHE_ID_BOMB). The code
that shows them knows a tag by its name inside the set ("vehicles\_shared\warthog\effects\warthog_body_damaged"), not by the
cache's prefix: the linker puts every fragment under ae\packs\<key>\, and a tag whose content equals another set's merges into
the first one's, so the same effect can lie under the vehicle damage prefix though the bomb asked for it. A lookup therefore
asks the SET (its closure range and root list in the DIRECTORY) for a group and a name inside the set, and compares the names
with their ae\packs\<key>\ first part left out.
*/

#ifndef AE_CACHE_SETS_RULES_H
#define AE_CACHE_SETS_RULES_H

#include <string.h>

static inline char ae_cache_set_fold(char c)
{
	return c == '/' ? '\\' : (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

/* the name inside a set: "ae\packs\bomb\bomb\bomb" is "bomb\bomb", the name of a stock tag the set holds (no such prefix)
is its own */
static inline char const *ae_cache_set_relative_name(char const *name)
{
	static char const prefix[] = "ae\\packs\\";
	char const *c;

	if (!name)
		return "";
	if (strlen(name) <= sizeof(prefix) - 1)
		return name;
	for (c = prefix; *c; c++)
	{
		if (ae_cache_set_fold(name[c - prefix]) != *c)
			return name;
	}
	c = name + sizeof(prefix) - 1;
	while (*c && *c != '\\' && *c != '/')
		c++;
	return *c ? c + 1 : name;
}

/* whether two tag names are the same tag of a set (the sets' prefixes aside, ASCII case aside, '/' and '\' alike) */
static inline int ae_cache_set_name_equal(char const *a, char const *b)
{
	a = ae_cache_set_relative_name(a);
	b = ae_cache_set_relative_name(b);
	while (*a && *b)
	{
		if (ae_cache_set_fold(*a) != ae_cache_set_fold(*b))
			return 0;
		a++;
		b++;
	}
	return !*a && !*b;
}

#endif /* AE_CACHE_SETS_RULES_H */
