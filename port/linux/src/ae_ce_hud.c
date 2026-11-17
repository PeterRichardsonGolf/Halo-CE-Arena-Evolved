/*
AE_CE_HUD.C

The Halo PC HUD bitmaps the high-res HUD's textures stand for on Custom
Edition maps (ae_ce_hud.h): found by their tag and index, then checked by
their pixels' CRC, as hud_hires.c checks the Xbox's.
*/

#include "ae_ce_hud.h"
#include "platform.h"

#include "zlib_prefixed.h"

#include <string.h>

int ae_ce_hud_pixels(const char *tag, int bitmap, unsigned long address, unsigned long level0_size)
{
	unsigned int index;
	unsigned long crc;

	for (index = 0; index < ae_ce_hud_bitmap_count; index++)
	{
		if (ae_ce_hud_bitmaps[index].bitmap == bitmap && ae_ce_hud_bitmaps[index].tag &&
			!strcmp(ae_ce_hud_bitmaps[index].tag, tag))
		{
			break;
		}
	}
	if (index == ae_ce_hud_bitmap_count || !address || !level0_size)
		return 0;
#ifdef HALO_64BIT
	/* (an Xbox address, as hud_hires.c's) */
	crc = crc32(0L, (const Bytef *)xbox_pointer(address), (uInt)level0_size);
#else
	crc = crc32(0L, (const Bytef *)address, (uInt)level0_size);
#endif
	return crc == ae_ce_hud_bitmaps[index].crc;
}
