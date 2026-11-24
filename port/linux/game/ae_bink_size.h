#ifndef AE_BINK_SIZE_H
#define AE_BINK_SIZE_H

/* whether a movie's frame fits what bink_playback.c makes for it: a frame buffer of width x height 32-bit pixels at
the end of its memory pool (with room left for the texture), and a texture whose Size word holds width - 1 and
height - 1 in 12 bits each and the pitch in 64-byte units less one in 8 bits (so the width is a multiple of 16).
A movie that does not fit is not played: its frames would be written outside the buffer */
static inline int ae_bink_size_fits(unsigned long width, unsigned long height, unsigned long pool_size)
{
	if (width < 16 || width > 4096 || width % 16 || height < 1 || height > 4096)
		return 0;
	return (unsigned long long)width * height * 4 + 4096 + 65536 <= pool_size;
}

#endif
