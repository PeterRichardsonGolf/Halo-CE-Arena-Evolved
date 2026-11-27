/* ae_forge_maps_rules.c: a saved map's name, its text's facts, its level name, and MY MAPS' SHOW and SORT */
#include <stdio.h>
#include <string.h>

#include "ae_forge_maps_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	char name[64], level[64];
	struct ae_forge_maps_facts facts;
	char long_name[80];

	/* Forge's save box: 1 to 48 of a-z 0-9 _ */
	CHECK(ae_forge_maps_name_valid("s3test") && ae_forge_maps_name_valid("gulch_remix_2") && ae_forge_maps_name_valid("_"));
	CHECK(!ae_forge_maps_name_valid("") && !ae_forge_maps_name_valid("Gulch") && !ae_forge_maps_name_valid("a b") &&
		!ae_forge_maps_name_valid("a-b") && !ae_forge_maps_name_valid("a.map") && !ae_forge_maps_name_valid(0));
	memset(long_name, 'a', 48);
	long_name[48] = 0;
	CHECK(ae_forge_maps_name_valid(long_name));
	long_name[48] = 'a';
	long_name[49] = 0;
	CHECK(!ae_forge_maps_name_valid(long_name));
	CHECK(ae_forge_maps_name_clean(long_name, name, sizeof(name)) == 48 && ae_forge_maps_name_valid(name));
	CHECK(ae_forge_maps_name_clean("Gulch Remix-2!", name, sizeof(name)) == 13 && !strcmp(name, "gulch_remix_2"));
	CHECK(ae_forge_maps_name_clean("!!", name, sizeof(name)) == 0 && !name[0]);
	CHECK(ae_forge_maps_name_clean("abcdef", name, 4) == 3 && !strcmp(name, "abc"));
	CHECK(ae_forge_maps_name_clean("x", name, 0) == 0);

	/* the text a save carries */
	CHECK(ae_forge_maps_facts_read("[OpenCE Forge Map]\r\nForgeSource=UserCreated\r\nMap Name: s3test\r\nBase Map: bloodgulch.map\r\n"
		"Author: x\r\nObjects Placed: 12\r\nAI Squads: 3\r\n[ForgeState]\r\nOBJECT|Objects Placed: 99\r\n", &facts));
	CHECK(!strcmp(facts.name, "s3test") && !strcmp(facts.base, "bloodgulch.map") && facts.objects == 12 && facts.squads == 3);
	CHECK(!ae_forge_maps_facts_read("hello", &facts) && !facts.name[0] && facts.objects == 0);
	CHECK(!ae_forge_maps_facts_read(0, &facts));
	CHECK(ae_forge_maps_facts_read("[OpenCE Forge Map]\n", &facts) && !facts.base[0]);

	/* the level a file is played as */
	CHECK(ae_forge_maps_kind(5) == AE_FORGE_MAP_XBOX && ae_forge_maps_kind(609) == AE_FORGE_MAP_CE &&
		ae_forge_maps_kind(7) == AE_FORGE_MAP_UNREADABLE && ae_forge_maps_kind(0) == AE_FORGE_MAP_UNREADABLE);
	ae_forge_maps_level_name(AE_FORGE_MAP_XBOX, "s3test", level, sizeof(level));
	CHECK(!strcmp(level, "s3test"));
	ae_forge_maps_level_name(AE_FORGE_MAP_CE, "s3test", level, sizeof(level));
	CHECK(!strcmp(level, "s3test@ce"));
	ae_forge_maps_level_name(AE_FORGE_MAP_UNREADABLE, "s3test", level, sizeof(level));
	CHECK(!level[0]);
	ae_forge_maps_level_name(AE_FORGE_MAP_CE, "s3test", level, 8);
	CHECK(!level[0]);
	ae_forge_maps_level_name(AE_FORGE_MAP_XBOX, "", level, sizeof(level));
	CHECK(!level[0]);

	/* SHOW */
	CHECK(ae_forge_maps_shown(AE_FORGE_SHOW_ALL, 0) && ae_forge_maps_shown(AE_FORGE_SHOW_ALL, 1));
	CHECK(ae_forge_maps_shown(AE_FORGE_SHOW_MADE, 0) && !ae_forge_maps_shown(AE_FORGE_SHOW_MADE, 1));
	CHECK(!ae_forge_maps_shown(AE_FORGE_SHOW_DOWNLOADED, 0) && ae_forge_maps_shown(AE_FORGE_SHOW_DOWNLOADED, 1));

	/* SORT */
	CHECK(ae_forge_maps_compare(AE_FORGE_SORT_SAVED, "b", "x", 200, "a", "x", 100) < 0);
	CHECK(ae_forge_maps_compare(AE_FORGE_SORT_SAVED, "b", "x", 100, "a", "x", 100) > 0);
	CHECK(ae_forge_maps_compare(AE_FORGE_SORT_NAME, "Alpha", "z", 1, "beta", "a", 9) < 0);
	CHECK(ae_forge_maps_compare(AE_FORGE_SORT_NAME, "alpha", "z", 1, "ALPHA", "z", 9) == 0);
	CHECK(ae_forge_maps_compare(AE_FORGE_SORT_BASE, "z", "chillout", 1, "a", "wizard", 9) < 0);
	CHECK(ae_forge_maps_compare(AE_FORGE_SORT_BASE, "b", "wizard", 1, "a", "wizard", 9) > 0);

	/* a stock map's name */
	CHECK(ae_forge_maps_name_is_stock("bloodgulch") && ae_forge_maps_name_is_stock("B30") && ae_forge_maps_name_is_stock("ui"));
	CHECK(!ae_forge_maps_name_is_stock("s3test") && !ae_forge_maps_name_is_stock("") && !ae_forge_maps_name_is_stock(0));
	{
		/* a 2 by 2 picture, 24 bit, bottom row first (rows padded to 8 bytes): the top row is red then green */
		unsigned char bmp[54 + 16];
		unsigned char rgba[16];
		int w = 0, h = 0;

		memset(bmp, 0, sizeof(bmp));
		bmp[0] = 'B'; bmp[1] = 'M'; bmp[10] = 54; bmp[14] = 40; bmp[18] = 2; bmp[22] = 2; bmp[26] = 1; bmp[28] = 24;
		/* bottom row: blue, white */
		bmp[54] = 255; bmp[57] = 255; bmp[58] = 255; bmp[59] = 255;
		/* top row: red, green (b g r) */
		bmp[62 + 2] = 255; bmp[62 + 3 + 1] = 255;
		CHECK(ae_forge_maps_bmp_rgba(bmp, sizeof(bmp), rgba, sizeof(rgba), &w, &h) && w == 2 && h == 2);
		CHECK(rgba[0] == 255 && rgba[1] == 0 && rgba[2] == 0 && rgba[3] == 255);
		CHECK(rgba[4] == 0 && rgba[5] == 255 && rgba[6] == 0);
		CHECK(rgba[8] == 0 && rgba[9] == 0 && rgba[10] == 255);
		CHECK(rgba[12] == 255 && rgba[13] == 255 && rgba[14] == 255);
		/* a height below zero is top down: the rows read the other way up */
		bmp[22] = 0xFE; bmp[23] = 0xFF; bmp[24] = 0xFF; bmp[25] = 0xFF;
		CHECK(ae_forge_maps_bmp_rgba(bmp, sizeof(bmp), rgba, sizeof(rgba), &w, &h) && rgba[2] == 255 && rgba[10] == 0);
		/* (the lowest height there is, whose sign cannot be taken off, and one past the largest picture: refused) */
		bmp[22] = 0; bmp[23] = 0; bmp[24] = 0; bmp[25] = 0x80;
		CHECK(!ae_forge_maps_bmp_rgba(bmp, sizeof(bmp), rgba, sizeof(rgba), &w, &h));
		bmp[22] = 0xFF; bmp[23] = 0xFB; bmp[24] = 0xFF; bmp[25] = 0xFF;
		CHECK(!ae_forge_maps_bmp_rgba(bmp, sizeof(bmp), rgba, sizeof(rgba), &w, &h));
		bmp[22] = 2; bmp[23] = bmp[24] = bmp[25] = 0;
		CHECK(!ae_forge_maps_bmp_rgba(bmp, sizeof(bmp) - 1, rgba, sizeof(rgba), &w, &h));
		CHECK(!ae_forge_maps_bmp_rgba(bmp, sizeof(bmp), rgba, 15, &w, &h));
		bmp[30] = 1;
		CHECK(!ae_forge_maps_bmp_rgba(bmp, sizeof(bmp), rgba, sizeof(rgba), &w, &h));
		bmp[30] = 0;
		bmp[0] = 'X';
		CHECK(!ae_forge_maps_bmp_rgba(bmp, sizeof(bmp), rgba, sizeof(rgba), &w, &h) && !ae_forge_maps_bmp_rgba(0, 0, rgba, 16, &w, &h));
	}
	{
		char words[32];

		ae_forge_maps_duration_text(0, words, sizeof(words));
		CHECK(!strcmp(words, "0 MIN"));
		ae_forge_maps_duration_text(59, words, sizeof(words));
		CHECK(!strcmp(words, "0 MIN"));
		ae_forge_maps_duration_text(2520, words, sizeof(words));
		CHECK(!strcmp(words, "42 MIN"));
		ae_forge_maps_duration_text(8040, words, sizeof(words));
		CHECK(!strcmp(words, "2 H 14 MIN"));
		ae_forge_maps_duration_text(108000, words, sizeof(words));
		CHECK(!strcmp(words, "30 H"));
		ae_forge_maps_duration_text(-5, words, sizeof(words));
		CHECK(!strcmp(words, "0 MIN"));
	}
	if (failures)
	{
		printf("%d failures\n", failures);
		return 1;
	}
	printf("ae_forge_maps_rules_test: all passed\n");

	return 0;
}
