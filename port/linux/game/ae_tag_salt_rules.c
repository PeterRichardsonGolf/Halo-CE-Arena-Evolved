/*
AE_TAG_SALT_RULES.C (ae_tag_salt_rules.h)
*/

#include "ae_tag_salt_rules.h"

long ae_tag_salt_after(long highest, long added)
{
	long const span = AE_TAG_SALT_LAST - AE_TAG_SALT_FIRST + 1;
	long salt;

	if (highest < 0)
		highest = 0;
	/* (a map's tag of salt 0xffff: the salts after it are the ones after 0xfffe) */
	if (highest > AE_TAG_SALT_LAST)
		highest = AE_TAG_SALT_LAST;
	if (added < 0)
		added = 0;
	salt = highest + 1 + added;
	if (salt <= AE_TAG_SALT_LAST)
		return salt;

	return AE_TAG_SALT_FIRST + (salt - (AE_TAG_SALT_LAST + 1)) % span;
}
