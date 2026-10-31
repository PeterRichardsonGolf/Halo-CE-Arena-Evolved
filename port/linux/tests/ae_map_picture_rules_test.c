/*
AE_MAP_PICTURE_RULES_TEST.C

A map's own picture (port/linux/game/ae_map_picture_rules.c): its path beside
the map's file, and the PNGs the menus draw.
*/

#include <stdio.h>
#include <string.h>

#include "../game/ae_map_picture_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void header(unsigned char *bytes, unsigned int width, unsigned int height, int depth, int colour, int interlace)
{
	static unsigned char const start[16] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n', 0, 0, 0, 13, 'I', 'H', 'D', 'R' };

	memset(bytes, 0, AE_MAP_PICTURE_HEADER_SIZE);
	memcpy(bytes, start, sizeof(start));
	bytes[16] = (unsigned char)(width >> 24);
	bytes[17] = (unsigned char)(width >> 16);
	bytes[18] = (unsigned char)(width >> 8);
	bytes[19] = (unsigned char)width;
	bytes[20] = (unsigned char)(height >> 24);
	bytes[21] = (unsigned char)(height >> 16);
	bytes[22] = (unsigned char)(height >> 8);
	bytes[23] = (unsigned char)height;
	bytes[24] = (unsigned char)depth;
	bytes[25] = (unsigned char)colour;
	bytes[28] = (unsigned char)interlace;
}

int main(void)
{
	char out[64];
	unsigned char bytes[AE_MAP_PICTURE_HEADER_SIZE];

	/* (beside the map's file, with its separator) */
	CHECK(ae_map_picture_path("d:\\maps\\atlas.map", "atlas", out, sizeof(out)));
	CHECK(!strcmp(out, "d:\\maps\\previews\\atlas.png"));
	CHECK(ae_map_picture_path("d:\\mods\\NHE\\maps\\atlas.map", "atlas", out, sizeof(out)));
	CHECK(!strcmp(out, "d:\\mods\\NHE\\maps\\previews\\atlas.png"));
	CHECK(ae_map_picture_path("/data/maps_ce/xbox_zerohour.map", "xbox_zerohour", out, sizeof(out)));
	CHECK(!strcmp(out, "/data/maps_ce/previews/xbox_zerohour.png"));
	CHECK(ae_map_picture_path("d:/maps\\x.map", "x", out, sizeof(out)) && !strcmp(out, "d:/maps\\previews\\x.png"));
	CHECK(ae_map_picture_path("c:\\a/b/x.map", "x", out, sizeof(out)) && !strcmp(out, "c:\\a/b/previews/x.png"));
	/* (no folder, a name that is not plain, no room) */
	CHECK(!ae_map_picture_path("atlas.map", "atlas", out, sizeof(out)) && !out[0]);
	CHECK(!ae_map_picture_path("d:\\maps\\x.map", "", out, sizeof(out)) && !out[0]);
	CHECK(!ae_map_picture_path("d:\\maps\\x.map", "..", out, sizeof(out)));
	CHECK(!ae_map_picture_path("d:\\maps\\x.map", ".", out, sizeof(out)));
	CHECK(!ae_map_picture_path("d:\\maps\\x.map", "..\\x", out, sizeof(out)));
	CHECK(!ae_map_picture_path("d:\\maps\\x.map", "a/b", out, sizeof(out)));
	CHECK(!ae_map_picture_path("d:\\maps\\x.map", "c:x", out, sizeof(out)));
	CHECK(!ae_map_picture_path(NULL, "x", out, sizeof(out)));
	CHECK(!ae_map_picture_path("d:\\maps\\x.map", NULL, out, sizeof(out)));
	CHECK(ae_map_picture_path("d:\\m\\x.map", "abcdef", out, 25) && !strcmp(out, "d:\\m\\previews\\abcdef.png"));
	CHECK(!ae_map_picture_path("d:\\m\\x.map", "abcdefg", out, 25) && !out[0]);
	CHECK(!ae_map_picture_path("d:\\m\\x.map", "x", out, 0));

	/* (the PNGs the menus draw: 8-bit RGBA, not interlaced) */
	header(bytes, 560, 456, 8, 6, 0);
	CHECK(ae_map_picture_png_usable(bytes, sizeof(bytes), 300000));
	CHECK(!ae_map_picture_png_usable(bytes, sizeof(bytes) - 1, 300000));
	CHECK(!ae_map_picture_png_usable(bytes, sizeof(bytes), 20));
	CHECK(!ae_map_picture_png_usable(bytes, sizeof(bytes), AE_MAP_PICTURE_MAXIMUM_FILE + 1));
	CHECK(ae_map_picture_png_usable(bytes, sizeof(bytes), AE_MAP_PICTURE_MAXIMUM_FILE));
	CHECK(!ae_map_picture_png_usable(NULL, sizeof(bytes), 300000));
	header(bytes, 560, 456, 8, 2, 0);
	CHECK(!ae_map_picture_png_usable(bytes, sizeof(bytes), 300000));
	header(bytes, 560, 456, 16, 6, 0);
	CHECK(!ae_map_picture_png_usable(bytes, sizeof(bytes), 300000));
	header(bytes, 560, 456, 8, 6, 1);
	CHECK(!ae_map_picture_png_usable(bytes, sizeof(bytes), 300000));
	header(bytes, 0, 456, 8, 6, 0);
	CHECK(!ae_map_picture_png_usable(bytes, sizeof(bytes), 300000));
	header(bytes, 2048, 2048, 8, 6, 0);
	CHECK(ae_map_picture_png_usable(bytes, sizeof(bytes), 300000));
	header(bytes, 2049, 456, 8, 6, 0);
	CHECK(!ae_map_picture_png_usable(bytes, sizeof(bytes), 300000));
	header(bytes, 560, 0x80000000u, 8, 6, 0);
	CHECK(!ae_map_picture_png_usable(bytes, sizeof(bytes), 300000));
	header(bytes, 560, 456, 8, 6, 0);
	bytes[1] = 'Q';
	CHECK(!ae_map_picture_png_usable(bytes, sizeof(bytes), 300000));
	header(bytes, 560, 456, 8, 6, 0);
	bytes[11] = 14;
	CHECK(!ae_map_picture_png_usable(bytes, sizeof(bytes), 300000));

	if (failures)
		printf("%d failed\n", failures);
	else
		printf("ae_map_picture_rules: all passed\n");
	return failures != 0;
}
