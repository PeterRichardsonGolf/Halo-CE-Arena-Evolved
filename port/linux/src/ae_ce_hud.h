/*
AE_CE_HUD.H

The high-res HUD on Custom Edition maps. Such a map draws Halo PC's HUD
bitmaps (bitmaps.map's), whose pixels are not the Xbox's that each texture's
CRC names (hud_hires.h): mostly the same art, at the same size and in the same
layout, in other formats (a8r8g8b8 or DXT where the Xbox has ay8 or a8y8).
tools/ae_ce_hud.py finds those a texture can stand for, and writes their CRCs
into ae_ce_hud_table.c; the AE hook lines of hud_hires.c let a texture stand
for pixels with one of them. A meter's sheet (sampled in Halo PC's channels on
those maps: ce_hud.c) and the bitmaps Halo PC drew again or placed otherwise
keep the map's own.
*/

#ifndef __AE_CE_HUD_H
#define __AE_CE_HUD_H

/* a Halo PC HUD bitmap a texture stands for: its bitmap group's tag name, its
index there, and the CRC-32 of its first mip level's pixels */
struct ae_ce_hud_bitmap
{
	const char *tag;
	int bitmap;
	unsigned int crc;
};

extern const struct ae_ce_hud_bitmap ae_ce_hud_bitmaps[];
extern const unsigned int ae_ce_hud_bitmap_count;

/* (hud_hires.c, before its own CRC check) whether the pixels at address
(guest virtual; the first mip level level0_size bytes) are the Halo PC
bitmap of this tag and index that the texture stands for: 1, else 0 */
int ae_ce_hud_pixels(const char *tag, int bitmap, unsigned long address, unsigned long level0_size);

#endif
