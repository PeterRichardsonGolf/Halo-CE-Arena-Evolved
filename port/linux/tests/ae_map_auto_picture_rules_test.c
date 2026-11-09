/*
AE_MAP_AUTO_PICTURE_RULES_TEST.C

The automatic map picture's rules (port/linux/game/ae_map_auto_picture_rules.c): which maps get one, when the shot is
due, the 16:9 crop and scale, and the PNG (read back by the menus' own check).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../game/ae_map_auto_picture_rules.h"
#include "../game/ae_map_picture_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static unsigned long be32(unsigned char const *p)
{
	return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) | p[3];
}

int main(void)
{
	int x, y, w, h;

	/* (which maps) */
	CHECK(ae_map_auto_picture_wanted("homobox@ce", 1, 0, 1));
	CHECK(ae_map_auto_picture_wanted("atlas_x@pack", 1, 0, 1));
	CHECK(!ae_map_auto_picture_wanted("atlas_x@pack", 0, 0, 1));
	CHECK(!ae_map_auto_picture_wanted("bloodgulch@ce", 1, 1, 1));
	CHECK(!ae_map_auto_picture_wanted("x@ce", 1, 0, 0));
	CHECK(!ae_map_auto_picture_wanted("digsite@ws3477809506", 1, 0, 1));
	CHECK(!ae_map_auto_picture_wanted("", 1, 0, 1));
	CHECK(!ae_map_auto_picture_wanted(NULL, 1, 0, 1));
	CHECK(ae_map_auto_picture_stock_frame(0x4000));
	CHECK(!ae_map_auto_picture_stock_frame(0x4000 + 19));
	CHECK(!ae_map_auto_picture_stock_frame(9));
	CHECK(!ae_map_auto_picture_stock_frame(0x5000));
	/* (when) */
	CHECK(!ae_map_auto_picture_shot_due(0));
	CHECK(!ae_map_auto_picture_shot_due(11));
	CHECK(ae_map_auto_picture_shot_due(12));
	CHECK(ae_map_auto_picture_shot_due(90));
	CHECK(!ae_map_auto_picture_shot_due(91));
	CHECK(!ae_map_auto_picture_shot_due(-5));
	/* (the crop) */
	CHECK(ae_map_auto_picture_crop(1920, 1080, &x, &y, &w, &h) && x == 0 && y == 0 && w == 1920 && h == 1080);
	CHECK(ae_map_auto_picture_crop(1920, 1200, &x, &y, &w, &h) && x == 0 && y == 60 && w == 1920 && h == 1080);
	CHECK(ae_map_auto_picture_crop(2560, 1080, &x, &y, &w, &h) && x == 320 && y == 0 && w == 1920 && h == 1080);
	CHECK(ae_map_auto_picture_crop(640, 480, &x, &y, &w, &h) && w == 640 && h == 360 && y == 60);
	CHECK(!ae_map_auto_picture_crop(10, 10, &x, &y, &w, &h));
	CHECK(!ae_map_auto_picture_crop(20000, 1080, &x, &y, &w, &h));
	{
		/* (the scale: blue, green, red in; red, green, blue, opaque out; left half red, right half blue) */
		int fw = 1280, fh = 720, row, col;
		unsigned char *frame = malloc((size_t)fw * fh * 4), *rgba = malloc(640 * 360 * 4), *png;
		unsigned int written = 0, size = ae_map_auto_picture_png_size(640, 360);

		for (row = 0; row < fh; row++)
			for (col = 0; col < fw; col++)
			{
				unsigned char *p = frame + ((size_t)row * fw + col) * 4;

				p[0] = col < fw / 2 ? 0 : 200;  /* blue */
				p[1] = 10;
				p[2] = col < fw / 2 ? 200 : 0;  /* red */
				p[3] = 0;                        /* (the game's scratch alpha) */
			}
		CHECK(ae_map_auto_picture_scale(frame, fw, fh, rgba));
		CHECK(rgba[0] == 200 && rgba[1] == 10 && rgba[2] == 0 && rgba[3] == 0xff);
		CHECK(rgba[(639) * 4] == 0 && rgba[(639) * 4 + 2] == 200 && rgba[(639) * 4 + 3] == 0xff);
		CHECK(!ae_map_auto_picture_scale(NULL, fw, fh, rgba));
		/* (the PNG: the menus' check passes, its chunks are as the format has them) */
		png = malloc(size);
		CHECK(size > 640 * 360 * 4 && size < 640 * 360 * 4 + 4096);
		CHECK(!ae_map_auto_picture_png(rgba, 640, 360, png, size - 1, &written));
		CHECK(ae_map_auto_picture_png(rgba, 640, 360, png, size, &written) && written == size);
		CHECK(ae_map_picture_png_usable(png, AE_MAP_PICTURE_HEADER_SIZE, written));
		CHECK(ae_map_picture_png_decodable(png, AE_MAP_PICTURE_HEADER_SIZE, written));
		CHECK(be32(png + 16) == 640 && be32(png + 20) == 360 && png[24] == 8 && png[25] == 6);
		CHECK(!memcmp(png + written - 8, "IEND", 4) || !memcmp(png + written - 12 + 4, "IEND", 4));
		free(png);
		free(frame);
		free(rgba);
		/* (a tiny one: a single stored block) */
		{
			unsigned char small[2 * 2 * 4] = { 1, 2, 3, 255, 4, 5, 6, 255, 7, 8, 9, 255, 10, 11, 12, 255 }, out[256];

			size = ae_map_auto_picture_png_size(2, 2);
			CHECK(size && size <= sizeof(out));
			CHECK(ae_map_auto_picture_png(small, 2, 2, out, sizeof(out), &written) && written == size);
			CHECK(ae_map_picture_png_usable(out, AE_MAP_PICTURE_HEADER_SIZE, written));
		}
		CHECK(!ae_map_auto_picture_png_size(0, 5) && !ae_map_auto_picture_png_size(5000, 5));
	}
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ok\n");
	return failures ? 1 : 0;
}
