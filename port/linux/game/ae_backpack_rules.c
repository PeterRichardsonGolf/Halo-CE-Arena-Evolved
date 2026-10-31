/*
AE_BACKPACK_RULES.C

BACKPACK WEAPONS' rules (ae_backpack_rules.h): a holstered weapon's kind and
place. No game dependency, so the unit test builds it alone.
*/

#include <string.h>

#include "ae_backpack_rules.h"

/* words in a weapon's tag path that make it small (a holster's), or very
long */
static char const *const ae_backpack_small_words[] =
{
	"pistol", "magnum", "revolver", "smg", "needler", "plasma rifle", "plasma_rifle",
};
static char const *const ae_backpack_long_words[] =
{
	"sniper",
};

/* whether text has part in it, whatever its case */
static int ae_backpack_has(
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

int ae_backpack_rules_kind(
	char const *tag_path,
	float bounding_radius)
{
	size_t index;

	for (index = 0; tag_path && index < sizeof(ae_backpack_small_words) / sizeof(ae_backpack_small_words[0]); index++)
	{
		if (ae_backpack_has(tag_path, ae_backpack_small_words[index]))
			return AE_BACKPACK_KIND_THIGH;
	}
	for (index = 0; tag_path && index < sizeof(ae_backpack_long_words) / sizeof(ae_backpack_long_words[0]); index++)
	{
		if (ae_backpack_has(tag_path, ae_backpack_long_words[index]))
			return AE_BACKPACK_KIND_LONG;
	}
	return bounding_radius < AE_BACKPACK_SMALL_RADIUS ? AE_BACKPACK_KIND_THIGH : AE_BACKPACK_KIND_BACK;
}

int ae_backpack_rules_place(
	int kind,
	int *on_back,
	int *on_thighs)
{
	if (kind == AE_BACKPACK_KIND_THIGH)
	{
		if (*on_thighs > 1)
			return AE_BACKPACK_PLACE_NONE;
		return (*on_thighs)++ == 0 ? AE_BACKPACK_PLACE_THIGH : AE_BACKPACK_PLACE_THIGH_SECOND;
	}
	if (*on_back > 1)
		return AE_BACKPACK_PLACE_NONE;
	if ((*on_back)++ > 0)
		return AE_BACKPACK_PLACE_BACK_SECOND;
	return kind == AE_BACKPACK_KIND_LONG ? AE_BACKPACK_PLACE_LONG : AE_BACKPACK_PLACE_BACK;
}
