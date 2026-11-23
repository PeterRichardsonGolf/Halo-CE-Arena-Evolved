/*
AE_MAP_SKINS_RULES.C

A map's own look for AE's weapons (ae_map_skins_rules.h).
*/

#include <string.h>

#include "ae_map_skins_rules.h"

static int lower(int c)
{
	return c >= 'A' && c <= 'Z' ? c + 32 : c == '/' ? '\\' : c;
}

/* whether a and b are the same, case aside and a slash a backslash (b of length bytes) */
static int same(char const *a, char const *b, size_t length)
{
	size_t at;

	for (at = 0; at < length; at++)
	{
		if (!a[at] || lower((unsigned char)a[at]) != lower((unsigned char)b[at]))
			return 0;
	}
	return 1;
}

int ae_map_skins_rules_digsite(char const *map_name)
{
	size_t length, suffix = strlen(AE_MAP_SKINS_DIGSITE_SUFFIX);

	if (!map_name)
		return 0;
	length = strlen(map_name);
	return length > suffix && same(map_name + length - suffix, AE_MAP_SKINS_DIGSITE_SUFFIX, suffix);
}

int ae_map_skins_rules_keeps_own(char const *map_name, int needler)
{
	if (!map_name || !strstr(map_name, "@ws"))
		return 0;
	return needler || !ae_map_skins_rules_digsite(map_name);
}

int ae_map_skins_rules_is_look(char const *weapon, char const *candidate)
{
	size_t length, look = strlen(AE_MAP_SKINS_DIGSITE_LOOK);

	if (!weapon || !candidate || !weapon[0])
		return 0;
	length = strlen(weapon);
	return strlen(candidate) == length + look && same(candidate, weapon, length) &&
		same(candidate + length, AE_MAP_SKINS_DIGSITE_LOOK, look);
}

int ae_map_skins_rules_digsite_rifle_bullet(char const *projectile)
{
	size_t length = strlen(AE_MAP_SKINS_DIGSITE_RIFLE_BULLET);

	return projectile && strlen(projectile) == length && same(projectile, AE_MAP_SKINS_DIGSITE_RIFLE_BULLET, length);
}
