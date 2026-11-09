/*
AE_MAP_AUTO_PICTURE_RULES.C

The automatic map picture's rules (ae_map_auto_picture_rules.h).
*/

#include <string.h>

#include "ae_map_auto_picture_rules.h"

int ae_map_auto_picture_wanted(char const *map_name, int candidate, int stock_art, int local)
{
	return map_name && map_name[0] && candidate && !stock_art && local && !strstr(map_name, "@ws");
}

int ae_map_auto_picture_stock_frame(int frame_index)
{
	return frame_index >= AE_MAP_AUTO_PICTURE_STOCK_BASE && frame_index < AE_MAP_AUTO_PICTURE_STOCK_END &&
		frame_index != AE_MAP_AUTO_PICTURE_STOCK_BASE + AE_MAP_AUTO_PICTURE_UNKNOWN_LEVEL;
}

int ae_map_auto_picture_shot_due(long ticks_alive)
{
	return ticks_alive >= AE_MAP_AUTO_PICTURE_SHOT_TICKS && ticks_alive <= AE_MAP_AUTO_PICTURE_LAST_TICKS;
}

int ae_map_auto_picture_crop(int width, int height, int *x, int *y, int *crop_width, int *crop_height)
{
	long wide;

	if (width < AE_MAP_AUTO_PICTURE_WIDTH / 8 || height < AE_MAP_AUTO_PICTURE_HEIGHT / 8 || width > 16384 || height > 16384)
		return 0;
	/* (wider than 16:9: columns cut off both sides; taller: rows cut off) */
	wide = (long)width * AE_MAP_AUTO_PICTURE_HEIGHT;
	if (wide > (long)height * AE_MAP_AUTO_PICTURE_WIDTH)
	{
		*crop_height = height;
		*crop_width = (int)((long)height * AE_MAP_AUTO_PICTURE_WIDTH / AE_MAP_AUTO_PICTURE_HEIGHT);
	}
	else
	{
		*crop_width = width;
		*crop_height = (int)((long)width * AE_MAP_AUTO_PICTURE_HEIGHT / AE_MAP_AUTO_PICTURE_WIDTH);
	}
	*x = (width - *crop_width) / 2;
	*y = (height - *crop_height) / 2;
	return *crop_width > 0 && *crop_height > 0;
}

int ae_map_auto_picture_scale(unsigned char const *bgra, int width, int height, unsigned char *rgba)
{
	int x0, y0, crop_width, crop_height, ox, oy;

	if (!bgra || !rgba || !ae_map_auto_picture_crop(width, height, &x0, &y0, &crop_width, &crop_height))
		return 0;
	for (oy = 0; oy < AE_MAP_AUTO_PICTURE_HEIGHT; oy++)
	{
		int sy0 = y0 + (int)((long)oy * crop_height / AE_MAP_AUTO_PICTURE_HEIGHT);
		int sy1 = y0 + (int)((long)(oy + 1) * crop_height / AE_MAP_AUTO_PICTURE_HEIGHT);

		if (sy1 <= sy0)
			sy1 = sy0 + 1;
		for (ox = 0; ox < AE_MAP_AUTO_PICTURE_WIDTH; ox++)
		{
			int sx0 = x0 + (int)((long)ox * crop_width / AE_MAP_AUTO_PICTURE_WIDTH);
			int sx1 = x0 + (int)((long)(ox + 1) * crop_width / AE_MAP_AUTO_PICTURE_WIDTH);
			unsigned long sum[3] = { 0, 0, 0 }, count;
			int sx, sy;
			unsigned char *out = rgba + ((long)oy * AE_MAP_AUTO_PICTURE_WIDTH + ox) * 4;

			if (sx1 <= sx0)
				sx1 = sx0 + 1;
			for (sy = sy0; sy < sy1; sy++)
			{
				for (sx = sx0; sx < sx1; sx++)
				{
					unsigned char const *in = bgra + ((long)sy * width + sx) * 4;

					sum[0] += in[2];
					sum[1] += in[1];
					sum[2] += in[0];
				}
			}
			count = (unsigned long)(sy1 - sy0) * (unsigned long)(sx1 - sx0);
			out[0] = (unsigned char)((sum[0] + count / 2) / count);
			out[1] = (unsigned char)((sum[1] + count / 2) / count);
			out[2] = (unsigned char)((sum[2] + count / 2) / count);
			out[3] = 0xff;
		}
	}
	return 1;
}

/* ---------- the PNG */

static unsigned long crc_table[256];
static int crc_ready;

static unsigned long crc_update(unsigned long crc, unsigned char const *data, unsigned long size)
{
	unsigned long index;

	if (!crc_ready)
	{
		unsigned long n, k;

		for (n = 0; n < 256; n++)
		{
			unsigned long c = n;

			for (k = 0; k < 8; k++)
				c = (c & 1) ? 0xedb88320ul ^ (c >> 1) : c >> 1;
			crc_table[n] = c;
		}
		crc_ready = 1;
	}
	for (index = 0; index < size; index++)
		crc = crc_table[(crc ^ data[index]) & 0xff] ^ (crc >> 8);
	return crc;
}

static void put32(unsigned char *out, unsigned long value)
{
	out[0] = (unsigned char)(value >> 24);
	out[1] = (unsigned char)(value >> 16);
	out[2] = (unsigned char)(value >> 8);
	out[3] = (unsigned char)value;
}

/* a chunk's bytes from out + at: length, type, data (already in place at +8), crc; its size */
static unsigned int finish_chunk(unsigned char *chunk, char const *type, unsigned long data_size)
{
	put32(chunk, data_size);
	memcpy(chunk + 4, type, 4);
	put32(chunk + 8 + data_size, crc_update(0xfffffffful, chunk + 4, 4 + data_size) ^ 0xfffffffful);
	return (unsigned int)(12 + data_size);
}

/* the zlib stream: 2 bytes, stored blocks of at most 65535 bytes (5 bytes each), the adler32 */
static unsigned long raw_size(int width, int height)
{
	return (unsigned long)height * (1 + (unsigned long)width * 4);
}

static unsigned long stream_size(int width, int height)
{
	unsigned long raw = raw_size(width, height);

	return 2 + raw + 5 * ((raw + 65534) / 65535) + 4;
}

unsigned int ae_map_auto_picture_png_size(int width, int height)
{
	if (width < 1 || height < 1 || width > 2048 || height > 2048)
		return 0;
	return (unsigned int)(8 + 25 + (12 + stream_size(width, height)) + 12);
}

int ae_map_auto_picture_png(unsigned char const *rgba, int width, int height, unsigned char *out, unsigned int size,
	unsigned int *written)
{
	static unsigned char const signature[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
	unsigned int total = ae_map_auto_picture_png_size(width, height);
	unsigned long raw = raw_size(width, height), left, a = 1, b = 0, done = 0;
	unsigned char *at, *data;
	int row;

	if (!rgba || !out || !total || size < total)
		return 0;
	memcpy(out, signature, 8);
	at = out + 8;
	put32(at + 8, (unsigned long)width);
	put32(at + 12, (unsigned long)height);
	at[16] = 8;  /* bits a channel */
	at[17] = 6;  /* RGBA */
	at[18] = at[19] = at[20] = 0;
	at += finish_chunk(at, "IHDR", 13);
	data = at + 8;
	*data++ = 0x78;
	*data++ = 0x01;
	left = raw;
	{
		unsigned long block_left = 0;

		for (row = 0; row < height; row++)
		{
			unsigned long x;
			unsigned char filter = 0;

			for (x = 0; x < 1 + (unsigned long)width * 4; x++)
			{
				unsigned char value = x == 0 ? filter : rgba[(unsigned long)row * width * 4 + x - 1];

				if (!block_left)
				{
					unsigned long length = left > 65535 ? 65535 : left;

					*data++ = left <= 65535 ? 1 : 0;
					*data++ = (unsigned char)length;
					*data++ = (unsigned char)(length >> 8);
					*data++ = (unsigned char)~length;
					*data++ = (unsigned char)(~length >> 8);
					block_left = length;
				}
				*data++ = value;
				a = (a + value) % 65521ul;
				b = (b + a) % 65521ul;
				block_left--;
				left--;
				done++;
			}
		}
	}
	put32(data, (b << 16) | a);
	data += 4;
	at += finish_chunk(at, "IDAT", (unsigned long)(data - (at + 8)));
	at += finish_chunk(at, "IEND", 0);
	if (done != raw || (unsigned int)(at - out) != total)
		return 0;
	*written = total;
	return 1;
}
