/*
AE_WIDE_ROWS.C

Arena Evolved: the rows of a 32-bit bitmap averaged to a narrower one, in place.
*/

#include <string.h>
#include "ae_wide_rows.h"

void ae_wide_rows_average(unsigned char *rows, long width, long height, long factor)
{
	long out_width = width / factor, row, x, channel;

	/* (in place: a row's output lies before its input, and an output pixel before the pixels it reads ends) */
	for (row = 0; row < height; row++)
	{
		unsigned char const *in = rows + (size_t)row * width * 4;
		unsigned char *out = rows + (size_t)row * out_width * 4;

		for (x = 0; x < out_width; x++)
		{
			unsigned char pixel[4];

			for (channel = 0; channel < 4; channel++)
			{
				long sum = 0, step;

				for (step = 0; step < factor; step++)
					sum += in[(x * factor + step) * 4 + channel];
				pixel[channel] = (unsigned char)(sum / factor);
			}
			memcpy(out + x * 4, pixel, 4);
		}
	}
}

long ae_wide_level_bytes(int format, long width, long height)
{
	long blocks;

	width = width < 1 ? 1 : width;
	height = height < 1 ? 1 : height;
	blocks = ((width + 3) / 4) * ((height + 3) / 4);
	switch (format)
	{
	case AE_WIDE_FORMAT_X8R8G8B8:
	case AE_WIDE_FORMAT_A8R8G8B8:
		return width * height * 4;
	case AE_WIDE_FORMAT_DXT1:
		return blocks * 8;
	case AE_WIDE_FORMAT_DXT3:
	case AE_WIDE_FORMAT_DXT5:
		return blocks * 16;
	default:
		return -1;
	}
}

int ae_wide_levels_to_skip(long width, long height, int mipmap_count, long maximum)
{
	int skip = 0;

	while ((width > maximum || height > maximum) && skip < mipmap_count)
	{
		width = width / 2 < 1 ? 1 : width / 2;
		height = height / 2 < 1 ? 1 : height / 2;
		skip++;
	}
	return width > maximum || height > maximum ? 0 : skip;
}
