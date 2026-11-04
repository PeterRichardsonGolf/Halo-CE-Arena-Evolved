/*
AE_DEV_PACKS_RULES.C

DEV MODE's weapon packs (ae_dev_packs_rules.h).
*/

#include "ae_dev_packs_rules.h"

unsigned int ae_dev_packs_extra(unsigned int gametype_mask, unsigned int installed_mask, int dev_mode, int joinable)
{
	if (!dev_mode || joinable)
		return 0;
	return installed_mask & ~gametype_mask;
}

unsigned int ae_dev_packs_played(unsigned int loaded_mask, unsigned int gametype_mask)
{
	return loaded_mask & gametype_mask;
}

int ae_dev_packs_release_order(const short *added, int count, short *release)
{
	int index;

	if (!added || !release || count <= 0)
		return 0;
	for (index = 0; index < count; index++)
		release[index] = added[count - 1 - index];
	return count;
}
