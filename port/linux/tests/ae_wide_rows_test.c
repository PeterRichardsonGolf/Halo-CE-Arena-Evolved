#include <stdio.h>
#include <string.h>
#include "ae_wide_rows.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* 3 rows of 8 pixels, averaged by 4: pixel p of row r is (r * 40 + p * 4 + channel) */
	unsigned char rows[3 * 8 * 4], expect[3 * 2 * 4];
	long r, p, c;

	for (r = 0; r < 3; r++)
		for (p = 0; p < 8; p++)
			for (c = 0; c < 4; c++)
				rows[(r * 8 + p) * 4 + c] = (unsigned char)(r * 40 + p * 4 + c);
	for (r = 0; r < 3; r++)
		for (p = 0; p < 2; p++)
			for (c = 0; c < 4; c++)
			{
				long sum = 0, s;

				for (s = 0; s < 4; s++)
					sum += r * 40 + (p * 4 + s) * 4 + c;
				expect[(r * 2 + p) * 4 + c] = (unsigned char)(sum / 4);
			}
	ae_wide_rows_average(rows, 8, 3, 4);
	CHECK(!memcmp(rows, expect, sizeof(expect)));

	/* a flat colour stays that colour, 255 does not overflow */
	memset(rows, 255, sizeof(rows));
	ae_wide_rows_average(rows, 8, 3, 4);
	for (r = 0; r < 3 * 2 * 4; r++)
		CHECK(rows[r] == 255);

	/* factor 1: unchanged */
	for (r = 0; r < 32; r++)
		rows[r] = (unsigned char)r;
	ae_wide_rows_average(rows, 8, 1, 1);
	for (r = 0; r < 32; r++)
		CHECK(rows[r] == r);
	/* level sizes: Danger Canyon's refined teleporter shield, 15360 by 256 a8r8g8b8 with 13 levels below, is 20971664
	bytes, its levels one after the other */
	{
		long w = 15360, h = 256, total = 0;
		int level;

		for (level = 0; level <= 13; level++)
		{
			total += ae_wide_level_bytes(AE_WIDE_FORMAT_A8R8G8B8, w, h);
			w = w / 2 < 1 ? 1 : w / 2;
			h = h / 2 < 1 ? 1 : h / 2;
		}
		CHECK(total == 20971664);
	}
	CHECK(ae_wide_level_bytes(AE_WIDE_FORMAT_DXT1, 7680, 128) == 7680 / 4 * 128 / 4 * 8);
	CHECK(ae_wide_level_bytes(AE_WIDE_FORMAT_DXT5, 2, 1) == 16);
	CHECK(ae_wide_level_bytes(AE_WIDE_FORMAT_X8R8G8B8, 0, 0) == 4);
	CHECK(ae_wide_level_bytes(1, 64, 64) == -1);
	/* levels left out: 2 for the shield (3840 by 64), 1 for a 7680 strip, none when it fits or too few levels */
	CHECK(ae_wide_levels_to_skip(15360, 256, 13, 4096) == 2);
	CHECK(ae_wide_levels_to_skip(7680, 128, 12, 4096) == 1);
	CHECK(ae_wide_levels_to_skip(4096, 4096, 12, 4096) == 0);
	CHECK(ae_wide_levels_to_skip(15360, 256, 1, 4096) == 0);
	CHECK(ae_wide_levels_to_skip(256, 16384, 6, 4096) == 2);
	if (!failures)
		printf("ok\n");
	return failures != 0;
}
