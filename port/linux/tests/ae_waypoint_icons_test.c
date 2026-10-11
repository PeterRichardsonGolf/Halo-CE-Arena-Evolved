/*
AE_WAYPOINT_ICONS_TEST.C

The item waypoints' pictures' rules (port/linux/game/ae_waypoint_icons.c): the
style's names, which picture an item has and when it falls back to text, a
map's sprite checked, the size drawn, a line's parts, and the glyphs' tables.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../game/ae_waypoint_icons.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* a line's parts as text: N name, I picture, T time, _ space, + separator, each member's number after N and I */
static const char *line(int style, int members, const int *icons, int has_time, int max_parts)
{
	static char text[128];
	struct ae_waypoint_part parts[32];
	int count = ae_waypoint_line_parts(style, members, icons, has_time, parts, max_parts);
	int index, used = 0;

	for (index = 0; index < count; index++)
	{
		switch (parts[index].kind)
		{
		case AE_WAYPOINT_PART_NAME: used += sprintf(text + used, "N%d", parts[index].member); break;
		case AE_WAYPOINT_PART_ICON: used += sprintf(text + used, "I%d", parts[index].member); break;
		case AE_WAYPOINT_PART_TIME: used += sprintf(text + used, "T"); break;
		case AE_WAYPOINT_PART_SPACE: used += sprintf(text + used, "_"); break;
		case AE_WAYPOINT_PART_SEPARATOR: used += sprintf(text + used, "+"); break;
		default: used += sprintf(text + used, "?"); break;
		}
	}
	text[used] = 0;
	return text;
}

static void style_tests(void)
{
	CHECK(ae_waypoint_style_from_text("text") == AE_WAYPOINT_TEXT);
	CHECK(ae_waypoint_style_from_text("icons") == AE_WAYPOINT_ICONS);
	CHECK(ae_waypoint_style_from_text("both") == AE_WAYPOINT_BOTH);
	CHECK(ae_waypoint_style_from_text("ICONS") == AE_WAYPOINT_ICONS && ae_waypoint_style_from_text("Text") == AE_WAYPOINT_TEXT);
	/* (anything else: the default) */
	CHECK(ae_waypoint_style_from_text("") == AE_WAYPOINT_BOTH && ae_waypoint_style_from_text(NULL) == AE_WAYPOINT_BOTH);
	CHECK(ae_waypoint_style_from_text("icon") == AE_WAYPOINT_BOTH && ae_waypoint_style_from_text("texts") == AE_WAYPOINT_BOTH);
	CHECK(!strcmp(ae_waypoint_style_name(AE_WAYPOINT_TEXT), "text") && !strcmp(ae_waypoint_style_name(AE_WAYPOINT_ICONS), "icons"));
	CHECK(!strcmp(ae_waypoint_style_name(AE_WAYPOINT_BOTH), "both") && !strcmp(ae_waypoint_style_name(99), "both"));
}

static void source_tests(void)
{
	int glyph = -1;

	/* (the weapons: the map's sprite, or none: text) */
	CHECK(ae_waypoint_icon_source(AE_WAYPOINT_ROCKETS, 1, 1, &glyph) == AE_WAYPOINT_ICON_STOCK && glyph == -1);
	CHECK(ae_waypoint_icon_source(AE_WAYPOINT_SNIPER, 1, 0, NULL) == AE_WAYPOINT_ICON_STOCK);
	CHECK(ae_waypoint_icon_source(AE_WAYPOINT_SHOTGUN, 1, 1, NULL) == AE_WAYPOINT_ICON_STOCK);
	CHECK(ae_waypoint_icon_source(AE_WAYPOINT_SNIPER, 0, 1, NULL) == AE_WAYPOINT_ICON_NONE);
	CHECK(ae_waypoint_icon_source(AE_WAYPOINT_ROCKETS, 0, 0, NULL) == AE_WAYPOINT_ICON_NONE);
	/* (ours, whatever the map has; none with no renderer to draw them) */
	CHECK(ae_waypoint_icon_source(AE_WAYPOINT_OVERSHIELD, 0, 1, &glyph) == AE_WAYPOINT_ICON_GLYPH &&
		glyph == AE_WAYPOINT_GLYPH_OVERSHIELD);
	CHECK(ae_waypoint_icon_source(AE_WAYPOINT_CAMO, 1, 1, &glyph) == AE_WAYPOINT_ICON_GLYPH && glyph == AE_WAYPOINT_GLYPH_CAMO);
	CHECK(ae_waypoint_icon_source(AE_WAYPOINT_CAMO, 1, 0, &glyph) == AE_WAYPOINT_ICON_NONE);
	CHECK(ae_waypoint_icon_source(AE_WAYPOINT_OVERSHIELD, 1, 1, NULL) == AE_WAYPOINT_ICON_GLYPH);
	/* (a mixed entry not seen yet, another item, a bad class) */
	CHECK(ae_waypoint_icon_source(-1, 1, 1, NULL) == AE_WAYPOINT_ICON_NONE);
	CHECK(ae_waypoint_icon_source(5, 1, 1, NULL) == AE_WAYPOINT_ICON_NONE && ae_waypoint_icon_source(1000, 1, 1, NULL) == AE_WAYPOINT_ICON_NONE);
}

static void class_definition_tests(void)
{
	/* (a mixed weapon entry: rockets and a sniper rifle, no other class) */
	static const long mixed[5] = { 101, 202, -1, -1, -1 };

	CHECK(ae_waypoint_class_definition(mixed, 5, AE_WAYPOINT_ROCKETS) == 101);
	/* (the rifle spawned: the rifle's item, not the rocket launcher's) */
	CHECK(ae_waypoint_class_definition(mixed, 5, AE_WAYPOINT_SNIPER) == 202);
	CHECK(ae_waypoint_class_definition(mixed, 5, AE_WAYPOINT_SHOTGUN) == -1);
	/* (not seen yet, out of range, no table) */
	CHECK(ae_waypoint_class_definition(mixed, 5, -1) == -1 && ae_waypoint_class_definition(mixed, 5, 5) == -1);
	CHECK(ae_waypoint_class_definition(NULL, 5, 1) == -1);
}

static void sprite_tests(void)
{
	int width = -1, height = -1;

	/* (the stock sniper rifle's: 123 by 27 of a 128 by 128 sheet) */
	CHECK(ae_waypoint_sprite_size(128, 128, 2.0f / 128, 125.0f / 128, 40.0f / 128, 67.0f / 128, &width, &height) &&
		width == 123 && height == 27);
	CHECK(ae_waypoint_sprite_size(128, 128, 0.0f, 1.0f, 0.0f, 1.0f, &width, &height) && width == 128 && height == 128);
	CHECK(ae_waypoint_sprite_size(64, 32, 0.0f, 0.5f, 0.5f, 1.0f, NULL, NULL));
	/* (bounds turned round, empty, outside the bitmap) */
	width = height = -1;
	CHECK(!ae_waypoint_sprite_size(128, 128, 0.5f, 0.25f, 0.0f, 1.0f, &width, &height) && width == -1 && height == -1);
	CHECK(!ae_waypoint_sprite_size(128, 128, 0.5f, 0.5f, 0.0f, 1.0f, NULL, NULL));
	CHECK(!ae_waypoint_sprite_size(128, 128, 0.0f, 1.0f, 0.75f, 0.25f, NULL, NULL));
	CHECK(!ae_waypoint_sprite_size(128, 128, -0.1f, 0.5f, 0.0f, 1.0f, NULL, NULL));
	CHECK(!ae_waypoint_sprite_size(128, 128, 0.0f, 1.5f, 0.0f, 1.0f, NULL, NULL));
	CHECK(!ae_waypoint_sprite_size(128, 128, 0.0f, 1.0f, 0.0f, 40.0f, NULL, NULL));
	CHECK(!ae_waypoint_sprite_size(128, 128, 0.0f, 1.0f, -3.0f, 0.5f, NULL, NULL));
	/* (not numbers) */
	CHECK(!ae_waypoint_sprite_size(128, 128, NAN, 1.0f, 0.0f, 1.0f, NULL, NULL));
	CHECK(!ae_waypoint_sprite_size(128, 128, 0.0f, NAN, 0.0f, 1.0f, NULL, NULL));
	CHECK(!ae_waypoint_sprite_size(128, 128, 0.0f, 1.0f, NAN, NAN, NULL, NULL));
	CHECK(!ae_waypoint_sprite_size(128, 128, 0.0f, INFINITY, 0.0f, 1.0f, NULL, NULL));
	/* (under two texels either way; a bitmap of no size, or too large to be a HUD sheet) */
	CHECK(!ae_waypoint_sprite_size(128, 128, 0.0f, 1.0f / 128, 0.0f, 1.0f, NULL, NULL));
	CHECK(!ae_waypoint_sprite_size(128, 128, 0.0f, 1.0f, 0.5f, 0.5f + 1.0f / 128, NULL, NULL));
	CHECK(!ae_waypoint_sprite_size(0, 128, 0.0f, 1.0f, 0.0f, 1.0f, NULL, NULL));
	CHECK(!ae_waypoint_sprite_size(128, -4, 0.0f, 1.0f, 0.0f, 1.0f, NULL, NULL));
	CHECK(!ae_waypoint_sprite_size(8192, 128, 0.0f, 1.0f, 0.0f, 1.0f, NULL, NULL));
	CHECK(!ae_waypoint_sprite_size(1, 1, 0.0f, 1.0f, 0.0f, 1.0f, NULL, NULL));
}

static void size_tests(void)
{
	int width = 0, height = 0;
	float scale = 0.0f;

	/* (a row: a text line and a tenth; one view's 14, a half screen's 12, a quarter's 11) */
	CHECK(ae_waypoint_row_height(14) == 15 && ae_waypoint_row_height(12) == 13 && ae_waypoint_row_height(11) == 12);
	CHECK(ae_waypoint_row_height(10) == 11 && ae_waypoint_row_height(1) == 1);
	CHECK(ae_waypoint_row_height(0) == 0 && ae_waypoint_row_height(-5) == 0);
	/* (as high as the row, its shape kept: the sniper rifle 123 by 27 in a row of 15) */
	CHECK(ae_waypoint_icon_size(123, 27, 15, &width, &height, &scale) && width == 68 && height == 15);
	CHECK(fabsf(scale - 15.0f / 27.0f) < 0.0001f);
	/* (the rocket launcher 101 by 33, the shotgun 123 by 27, a glyph 32 by 32) */
	CHECK(ae_waypoint_icon_size(101, 33, 15, &width, &height, &scale) && width == 46 && height == 15);
	CHECK(ae_waypoint_icon_size(32, 32, 15, &width, &height, &scale) && width == 15 && height == 15);
	CHECK(fabsf(scale - 15.0f / 32.0f) < 0.0001f);
	/* (a smaller view's row) */
	CHECK(ae_waypoint_icon_size(123, 27, 11, &width, &height, NULL) && width == 50 && height == 11);
	/* (wider than six times its height: made lower to that width) */
	CHECK(ae_waypoint_icon_size(400, 20, 15, &width, &height, &scale) && width == 90 && height == 5);
	CHECK(fabsf(scale - 90.0f / 400.0f) < 0.0001f);
	CHECK(ae_waypoint_icon_size(4000, 2, 15, &width, &height, &scale) && width == 90 && height == 1);
	/* (a tall one: never under a unit wide) */
	CHECK(ae_waypoint_icon_size(2, 4000, 15, &width, &height, NULL) && width == 1 && height == 15);
	/* (nothing to draw) */
	CHECK(!ae_waypoint_icon_size(0, 27, 15, &width, &height, &scale) && !ae_waypoint_icon_size(123, 0, 15, NULL, NULL, NULL));
	CHECK(!ae_waypoint_icon_size(123, 27, 0, NULL, NULL, NULL) && !ae_waypoint_icon_size(-1, -1, -1, NULL, NULL, NULL));
}

static void line_tests(void)
{
	static const int none[4] = { 0, 0, 0, 0 };
	static const int all[4] = { 1, 1, 1, 1 };
	static const int second[4] = { 0, 1, 0, 0 };
	static const int first_two[4] = { 1, 1, 0, 0 };
	struct ae_waypoint_part parts[4];
	int many[40];
	int index;

	/* TEXT: as before pictures, whatever pictures there are */
	CHECK(!strcmp(line(AE_WAYPOINT_TEXT, 1, all, 1, 32), "N0_T"));
	CHECK(!strcmp(line(AE_WAYPOINT_TEXT, 1, all, 0, 32), "N0"));
	CHECK(!strcmp(line(AE_WAYPOINT_TEXT, 3, all, 1, 32), "T_N0+N1+N2"));
	CHECK(!strcmp(line(AE_WAYPOINT_TEXT, 2, none, 0, 32), "N0+N1"));
	CHECK(!strcmp(line(AE_WAYPOINT_TEXT, 1, NULL, 1, 32), "N0_T"));
	/* BOTH: the name and time, then the picture; no picture: the text alone; several: TEXT's line */
	CHECK(!strcmp(line(AE_WAYPOINT_BOTH, 1, all, 1, 32), "N0_T_I0"));
	CHECK(!strcmp(line(AE_WAYPOINT_BOTH, 1, all, 0, 32), "N0_I0"));
	CHECK(!strcmp(line(AE_WAYPOINT_BOTH, 1, none, 1, 32), "N0_T"));
	CHECK(!strcmp(line(AE_WAYPOINT_BOTH, 1, NULL, 0, 32), "N0"));
	CHECK(!strcmp(line(AE_WAYPOINT_BOTH, 2, all, 1, 32), "T_N0+N1"));
	CHECK(!strcmp(line(AE_WAYPOINT_BOTH, 2, all, 0, 32), "N0+N1"));
	/* ICONS: the picture, then the time; an item with no picture is never an empty marker: its name */
	CHECK(!strcmp(line(AE_WAYPOINT_ICONS, 1, all, 1, 32), "I0_T"));
	CHECK(!strcmp(line(AE_WAYPOINT_ICONS, 1, all, 0, 32), "I0"));
	CHECK(!strcmp(line(AE_WAYPOINT_ICONS, 1, none, 1, 32), "N0_T"));
	CHECK(!strcmp(line(AE_WAYPOINT_ICONS, 1, none, 0, 32), "N0"));
	CHECK(!strcmp(line(AE_WAYPOINT_ICONS, 1, NULL, 1, 32), "N0_T"));
	CHECK(!strcmp(line(AE_WAYPOINT_ICONS, 3, all, 1, 32), "I0_I1_I2_T"));
	CHECK(!strcmp(line(AE_WAYPOINT_ICONS, 3, second, 1, 32), "N0+I1+N2_T"));
	CHECK(!strcmp(line(AE_WAYPOINT_ICONS, 3, first_two, 0, 32), "I0_I1+N2"));
	CHECK(!strcmp(line(AE_WAYPOINT_ICONS, 2, none, 1, 32), "N0+N1_T"));
	/* (every line names every member once, and has the time once when there is one) */
	for (index = 0; index < 40; index++)
		many[index] = index % 3 == 0;
	{
		int style, members, has_time;

		for (style = 0; style < AE_WAYPOINT_STYLE_COUNT; style++)
		{
			for (members = 1; members <= 5; members++)
			{
				for (has_time = 0; has_time < 2; has_time++)
				{
					struct ae_waypoint_part all_parts[32];
					int count = ae_waypoint_line_parts(style, members, many, has_time, all_parts, 32);
					int seen[5] = { 0, 0, 0, 0, 0 };
					int times = 0;

					for (index = 0; index < count; index++)
					{
						if (all_parts[index].kind == AE_WAYPOINT_PART_NAME || all_parts[index].kind == AE_WAYPOINT_PART_ICON)
						{
							CHECK(all_parts[index].member >= 0 && all_parts[index].member < members);
							/* (a picture only where the member has one, and the style draws them) */
							if (all_parts[index].kind == AE_WAYPOINT_PART_ICON)
								CHECK(style != AE_WAYPOINT_TEXT && many[all_parts[index].member]);
							/* (BOTH's one label has its name and its picture) */
							if (!(style == AE_WAYPOINT_BOTH && members == 1 && all_parts[index].kind == AE_WAYPOINT_PART_ICON))
								seen[all_parts[index].member]++;
						}
						times += all_parts[index].kind == AE_WAYPOINT_PART_TIME;
					}
					for (index = 0; index < members; index++)
						CHECK(seen[index] == 1);
					CHECK(times == has_time);
					/* (never a space or a separator first or last) */
					CHECK(count > 0 && all_parts[0].kind != AE_WAYPOINT_PART_SPACE && all_parts[0].kind != AE_WAYPOINT_PART_SEPARATOR);
					CHECK(all_parts[count - 1].kind != AE_WAYPOINT_PART_SPACE && all_parts[count - 1].kind != AE_WAYPOINT_PART_SEPARATOR);
				}
			}
		}
	}
	/* (no room for every member: the last are left out, the time stays, nothing is written past the room) */
	CHECK(!strcmp(line(AE_WAYPOINT_ICONS, 40, many, 1, 8), "I0+N1+N2_T"));
	CHECK(!strcmp(line(AE_WAYPOINT_TEXT, 40, many, 1, 8), "T_N0+N1+N2"));
	CHECK(!strcmp(line(AE_WAYPOINT_TEXT, 40, many, 0, 7), "N0+N1+N2+N3"));
	memset(parts, 0x55, sizeof(parts));
	CHECK(ae_waypoint_line_parts(AE_WAYPOINT_BOTH, 1, all, 1, parts, 3) == 3);
	CHECK(parts[3].kind == 0x5555 && parts[3].member == 0x5555);
	CHECK(ae_waypoint_line_parts(AE_WAYPOINT_ICONS, 3, all, 1, parts, 3) == 3 && parts[3].kind == 0x5555);
	CHECK(parts[0].kind == AE_WAYPOINT_PART_ICON && parts[2].kind == AE_WAYPOINT_PART_TIME);
	CHECK(ae_waypoint_line_parts(AE_WAYPOINT_ICONS, 0, all, 1, parts, 4) == 0);
	CHECK(ae_waypoint_line_parts(AE_WAYPOINT_ICONS, 2, all, 1, NULL, 4) == 0);
	CHECK(ae_waypoint_line_parts(AE_WAYPOINT_ICONS, 2, all, 1, parts, 0) == 0);
}

static void glyph_tests(void)
{
	int glyph, x, y;

	for (glyph = 0; glyph < AE_WAYPOINT_GLYPH_COUNT; glyph++)
	{
		long covered = 0, solid = 0;
		int left = AE_WAYPOINT_GLYPH_SIZE, right = -1, top = AE_WAYPOINT_GLYPH_SIZE, bottom = -1;

		for (y = 0; y < AE_WAYPOINT_GLYPH_SIZE; y++)
		{
			for (x = 0; x < AE_WAYPOINT_GLYPH_SIZE; x++)
			{
				unsigned char alpha = ae_waypoint_glyphs[glyph][y * AE_WAYPOINT_GLYPH_SIZE + x];

				if (!alpha)
					continue;
				covered++;
				solid += alpha == 255;
				left = x < left ? x : left;
				right = x > right ? x : right;
				top = y < top ? y : top;
				bottom = y > bottom ? y : bottom;
			}
		}
		/* (a drawing, not an empty or a full square: it fills most of its width, and has solid texels) */
		CHECK(covered > 150 && covered < 700 && solid > 60);
		CHECK(right - left >= 20 && bottom - top >= 16 && left >= 1 && right <= 30 && top >= 1 && bottom <= 30);
	}
	/* (the shield is the same on both sides) */
	for (y = 0; y < AE_WAYPOINT_GLYPH_SIZE; y++)
	{
		for (x = 0; x < AE_WAYPOINT_GLYPH_SIZE / 2; x++)
		{
			int mirrored = ae_waypoint_glyphs[AE_WAYPOINT_GLYPH_OVERSHIELD][y * AE_WAYPOINT_GLYPH_SIZE + AE_WAYPOINT_GLYPH_SIZE - 1 - x];
			int here = ae_waypoint_glyphs[AE_WAYPOINT_GLYPH_OVERSHIELD][y * AE_WAYPOINT_GLYPH_SIZE + x];

			CHECK(here - mirrored <= 16 && mirrored - here <= 16);
		}
	}
	/* (the two are not one picture) */
	CHECK(memcmp(ae_waypoint_glyphs[0], ae_waypoint_glyphs[1], sizeof(ae_waypoint_glyphs[0])) != 0);
}

int main(void)
{
	style_tests();
	source_tests();
	class_definition_tests();
	sprite_tests();
	size_tests();
	line_tests();
	glyph_tests();
	if (failures)
	{
		printf("ae_waypoint_icons: %d failed\n", failures);
		return 1;
	}
	printf("ae_waypoint_icons: all passed\n");
	return 0;
}
