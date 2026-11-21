/*
AE_SWAP_INTERVAL_RULES.C

The swap interval of a frame limit in step with the display (ae_swap_interval_rules.h).
*/

#include "ae_swap_interval_rules.h"

int ae_swap_interval_in_step(float refresh, long maximum, int x11)
{
	int interval;
	int most = x11 ? AE_SWAP_INTERVAL_MOST : 1;

	if (maximum <= 0 || !(refresh > 0.0f))
		return 0;
	for (interval = 1; interval <= most; interval++)
	{
		float apart = refresh / (float)interval - (float)maximum;

		if (apart >= -1.0f && apart <= 1.0f)
			return interval;
	}
	return 0;
}
