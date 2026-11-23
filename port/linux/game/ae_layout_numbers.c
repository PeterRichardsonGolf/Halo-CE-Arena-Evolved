/*
AE_LAYOUT_NUMBERS.C

ae_layout_numbers.h.
*/

#include "ae_layout_numbers.h"

int ae_layout_number_from_04x(int layout)
{
	/* (DEFAULT to BUMPER JUMPER: the same; RECON 6 to 8; HERO 7 stays, OpenCE's RECON; THE DUKE 8 to CUSTOM 14 one on) */
	if (layout <= 5)
		return layout;
	if (layout == 6)
		return 8;
	if (layout == 7)
		return 7;
	if (layout <= 14)
		return layout + 1;
	return layout;
}
