/*
AE_WIDE_ROWS.H

Arena Evolved: the rows of a 32-bit bitmap averaged to a narrower one, in place (ae_wide_bitmaps.c).
*/

#ifndef __AE_WIDE_ROWS_H
#define __AE_WIDE_ROWS_H

/* rows of width 4-byte pixels each, every run of factor pixels made their per-channel mean (width / factor pixels a row,
the rows following each other; width is a multiple of factor) */
void ae_wide_rows_average(unsigned char *rows, long width, long height, long factor);

/* bitmap formats of bitmap_group.c (private there) a level's size is known for */
enum { AE_WIDE_FORMAT_X8R8G8B8 = 10, AE_WIDE_FORMAT_A8R8G8B8 = 11, AE_WIDE_FORMAT_DXT1 = 14, AE_WIDE_FORMAT_DXT3 = 15,
	AE_WIDE_FORMAT_DXT5 = 16 };

/* the bytes of one level width by height of a bitmap of this format, as a Custom Edition map stores its levels one
after the other (32-bit pixels, or 4 by 4 blocks of 8 or 16 bytes); -1 for a format not listed */
long ae_wide_level_bytes(int format, long width, long height);

/* how many of its top levels a bitmap with mipmap_count levels below its first leaves out so that what is left is no
wider or higher than maximum (each level half the one above, at least 1); 0 when it fits or has too few levels */
int ae_wide_levels_to_skip(long width, long height, int mipmap_count, long maximum);

#endif
