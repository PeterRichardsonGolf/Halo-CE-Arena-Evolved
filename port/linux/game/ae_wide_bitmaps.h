/*
AE_WIDE_BITMAPS.H

Arena Evolved: a Custom Edition map's bitmap wider than the device's 4096 (Danger Canyon's teleporter shield: 15360
by 256, a 120-frame strip) is made narrower as it loads, so that it is drawn.
*/

#ifndef __AE_WIDE_BITMAPS_H
#define __AE_WIDE_BITMAPS_H

struct bitmap_data;

/* before the texture cache checks a bitmap: one too wide to be cached has its width cut by a whole factor (the bitmap
keeps its frames: the strip's U scale is a fraction of its width), and is remembered */
void ae_wide_bitmap_prepare(struct bitmap_data *bitmap);

/* the bitmap's pixels into buffer (at once), the rows' pixels averaged by that factor; FALSE for any other bitmap */
boolean ae_wide_bitmap_read(struct bitmap_data *bitmap, void *buffer);

#endif
