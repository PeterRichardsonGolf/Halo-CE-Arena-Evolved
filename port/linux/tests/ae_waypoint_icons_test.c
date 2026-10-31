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

	/* (the weapons: the map's sprite, else our burst, else none) */
	CHECK(ae_waypoint_icon_source(AE_WAYPOINT_ROCKETS, 1, 1, &glyph) == AE_WAYPOINT_ICON_STOCK && glyph == -1);
	CHECK(ae_waypoint_icon_source(AE_WAYPOINT_SNIPER, 1, 0, NULL) == AE_WAYPOINT_ICON_STOCK);
	CHECK(ae_waypoint_icon_source(AE_WAYPOINT_SHOTGUN, 1, 1, NULL) == AE_WAYPOINT_ICON_STOCK);
	CHECK(ae_waypoint_icon_source(AE_WAYPOINT_SNIPER, 0, 1, &glyph) == AE_WAYPOINT_ICON_GLYPH &&
		glyph == AE_WAYPOINT_GLYPH_POWER_WEAPON);
	CHECK(ae_waypoint_icon_source(AE_WAYPOINT_ROCKETS, 0, 0, NULL) == AE_WAYPOINT_ICON_NONE);
	/* (ours for the powerups, whatever the map has; none with no renderer to draw them) */
	CHECK(ae_waypoint_icon_source(AE_WAYPOINT_OVERSHIELD, 0, 1, &glyph) == AE_WAYPOINT_ICON_GLYPH &&
		glyph == AE_WAYPOINT_GLYPH_OVERSHIELD);
	CHECK(ae_waypoint_icon_source(AE_WAYPOINT_CAMO, 1, 1, &glyph) == AE_WAYPOINT_ICON_GLYPH && glyph == AE_WAYPOINT_GLYPH_CAMO);
	CHECK(ae_waypoint_icon_source(AE_WAYPOINT_CAMO, 1, 0, &glyph) == AE_WAYPOINT_ICON_NONE);
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

static void stock_layout_tests(void)
{
	int width, height;
	float scale;

	/* the sniper's 123 by 27 at a cap of 10: 14 high (1.4 capitals), its width following; capped it is smaller */
	CHECK(ae_waypoint_stock_height(10) == 14 && ae_waypoint_stock_height(0) == 0);
	CHECK(ae_waypoint_stock_layout(10, 123, 27, 0, &width, &height, &scale) && height == 14 && width == 64);
	CHECK(scale > 0.51f && scale < 0.53f);
	CHECK(ae_waypoint_stock_layout(10, 123, 27, 40, &width, &height, &scale) && width == 40 && height == 9);
	CHECK(ae_waypoint_stock_layout(10, 101, 33, 150, &width, &height, NULL) && height == 14 && width == 43);
	CHECK(ae_waypoint_stock_layout(10, 5, 5, 0, &width, &height, NULL) && width == 14 && height == 14);
	CHECK(!ae_waypoint_stock_layout(10, 0, 5, 0, &width, &height, NULL) && !ae_waypoint_stock_layout(0, 5, 5, 0, &width, &height, NULL));
}

/* parts as text: N name, T time, _ space, + separator, each name's member number after N */
static const char *parts_text(const struct ae_waypoint_part *parts, int count)
{
	static char text[128];
	int index, used = 0;

	for (index = 0; index < count; index++)
	{
		switch (parts[index].kind)
		{
		case AE_WAYPOINT_PART_NAME: used += sprintf(text + used, "N%d", parts[index].member); break;
		case AE_WAYPOINT_PART_TIME: used += sprintf(text + used, "T"); break;
		case AE_WAYPOINT_PART_SPACE: used += sprintf(text + used, "_"); break;
		case AE_WAYPOINT_PART_SEPARATOR: used += sprintf(text + used, "+"); break;
		default: used += sprintf(text + used, "?"); break;
		}
	}
	text[used] = 0;
	return text;
}

static const char *line(int members, int has_time, int max_parts)
{
	struct ae_waypoint_part parts[32];

	return parts_text(parts, ae_waypoint_line_parts(members, has_time, parts, max_parts));
}

/* a plan as text: S stacked or L line; T time row; n<members> names; p<members> pictures */
static const char *plan_text(int style, int members, const int *icons, int has_time)
{
	static char text[128];
	struct ae_waypoint_plan plan;
	int index, used;

	ae_waypoint_line_plan(style, members, icons, has_time, &plan);
	used = sprintf(text, "%c", plan.stacked ? 'S' : 'L');
	if (plan.distance_row)
		used += sprintf(text + used, "d");
	for (index = 0; index < plan.name_count; index++)
		used += sprintf(text + used, "n%d", plan.names[index]);
	for (index = 0; index < plan.picture_count; index++)
		used += sprintf(text + used, "p%d", plan.pictures[index]);
	if (plan.time_row)
		used += sprintf(text + used, "T");
	return text;
}

static void line_tests(void)
{
	static const int none[4] = { 0, 0, 0, 0 };
	static const int all[4] = { 1, 1, 1, 1 };
	static const int second[4] = { 0, 1, 0, 0 };
	static const int first_two[4] = { 1, 1, 0, 0 };
	struct ae_waypoint_part parts[4];
	struct ae_waypoint_plan plan;
	int names[3] = { 0, 2, 3 };
	int many[40];
	int index;

	/* the unstacked line */
	CHECK(!strcmp(line(1, 1, 32), "N0_T"));
	CHECK(!strcmp(line(1, 0, 32), "N0"));
	CHECK(!strcmp(line(3, 1, 32), "T_N0+N1+N2"));
	CHECK(!strcmp(line(2, 0, 32), "N0+N1"));
	CHECK(!strcmp(line(40, 1, 8), "T_N0+N1+N2"));
	CHECK(!strcmp(line(40, 0, 7), "N0+N1+N2+N3"));
	memset(parts, 0x55, sizeof(parts));
	CHECK(ae_waypoint_line_parts(1, 1, parts, 3) == 3 && parts[3].kind == 0x5555);
	CHECK(ae_waypoint_line_parts(1, 1, parts, 2) == 2);
	CHECK(ae_waypoint_line_parts(0, 1, parts, 4) == 0 && ae_waypoint_line_parts(2, 1, NULL, 4) == 0);
	CHECK(ae_waypoint_line_parts(2, 1, parts, 0) == 0);
	/* the names row */
	memset(parts, 0, sizeof(parts));
	CHECK(!strcmp(parts_text(parts, ae_waypoint_name_parts(3, names, parts, 4)), "N0+N2"));
	CHECK(ae_waypoint_name_parts(0, names, parts, 4) == 0 && ae_waypoint_name_parts(1, names, NULL, 4) == 0);
	/* TEXT: never stacked, whatever pictures there are */
	CHECK(!strcmp(plan_text(AE_WAYPOINT_TEXT, 1, all, 1), "L"));
	CHECK(!strcmp(plan_text(AE_WAYPOINT_TEXT, 3, all, 0), "L"));
	/* BOTH: distance, name, picture, timer (while there is a time); no picture: no picture row; several: names on one row, pictures side by side */
	CHECK(!strcmp(plan_text(AE_WAYPOINT_BOTH, 1, all, 1), "Sdn0p0T"));
	CHECK(!strcmp(plan_text(AE_WAYPOINT_BOTH, 1, all, 0), "Sdn0p0"));
	CHECK(!strcmp(plan_text(AE_WAYPOINT_BOTH, 1, none, 1), "Sdn0T"));
	CHECK(!strcmp(plan_text(AE_WAYPOINT_BOTH, 1, NULL, 1), "Sdn0T"));
	CHECK(!strcmp(plan_text(AE_WAYPOINT_BOTH, 2, all, 1), "Sdn0n1p0p1T"));
	CHECK(!strcmp(plan_text(AE_WAYPOINT_BOTH, 3, second, 1), "Sdn0n1n2p1T"));
	/* ICONS: no name for a member with a picture; the names of members with no picture between; no picture at all: the line */
	CHECK(!strcmp(plan_text(AE_WAYPOINT_ICONS, 1, all, 1), "Sdp0T"));
	CHECK(!strcmp(plan_text(AE_WAYPOINT_ICONS, 1, all, 0), "Sdp0"));
	CHECK(!strcmp(plan_text(AE_WAYPOINT_ICONS, 3, all, 1), "Sdp0p1p2T"));
	CHECK(!strcmp(plan_text(AE_WAYPOINT_ICONS, 3, first_two, 1), "Sdn2p0p1T"));
	CHECK(!strcmp(plan_text(AE_WAYPOINT_ICONS, 1, none, 1), "L"));
	CHECK(!strcmp(plan_text(AE_WAYPOINT_ICONS, 2, NULL, 0), "L"));
	/* (the plan: always every picture member in the row; a member with no picture named once, in BOTH all are) */
	for (index = 0; index < 40; index++)
		many[index] = index % 3 == 0;
	ae_waypoint_line_plan(AE_WAYPOINT_BOTH, 40, many, 1, &plan);
	CHECK(plan.name_count == AE_WAYPOINT_MAXIMUM_MEMBERS && plan.picture_count == 6);
	ae_waypoint_line_plan(AE_WAYPOINT_ICONS, 40, many, 1, &plan);
	CHECK(plan.picture_count == 6 && plan.name_count == AE_WAYPOINT_MAXIMUM_MEMBERS - 6);
	CHECK(ae_waypoint_line_plan(AE_WAYPOINT_BOTH, 0, all, 1, &plan) == 0 && !plan.stacked);
	CHECK(ae_waypoint_line_plan(AE_WAYPOINT_BOTH, 1, all, 1, NULL) == 0);
}

static void picture_row_tests(void)
{
	int offsets[4];
	int widths[3] = { 20, 30, 10 };
	int total;

	CHECK(ae_waypoint_picture_row(3, widths, 4, offsets, &total) == 3);
	CHECK(offsets[0] == 0 && offsets[1] == 24 && offsets[2] == 58 && total == 68);
	CHECK(ae_waypoint_picture_row(1, widths, 4, offsets, &total) == 1 && offsets[0] == 0 && total == 20);
	CHECK(ae_waypoint_picture_row(0, widths, 4, offsets, &total) == 0 && total == 0);
	CHECK(ae_waypoint_picture_row(2, widths, -3, offsets, &total) == 2 && offsets[1] == 20 && total == 50);
}

static void badge_tests(void)
{
	struct ae_waypoint_badge b;
	unsigned char fill[52 * 60], line[52 * 60];
	int x, y, ring = 0, inside = 0;

	CHECK(ae_waypoint_badge_height(10) == 18 && ae_waypoint_badge_height(0) == 0);
	/* a regular hexagon: its width is 0.866 of its height */
	CHECK(ae_waypoint_badge_layout(10, 64, 64, &b) && b.badge_height == 18 && b.badge_width == 16);
	/* a square glyph fills the hexagon's inside: about half its height, never past it */
	CHECK(b.icon_width == b.icon_height && b.icon_height >= 18 * 45 / 100 && b.icon_height <= 18 * 62 / 100);
	/* a view twice the size: the glyph twice as large */
	{
		struct ae_waypoint_badge big;

		CHECK(ae_waypoint_badge_layout(20, 64, 64, &big) && big.icon_height >= 2 * b.icon_height - 1);
		CHECK(big.badge_height == 36);
	}
	/* a wide sprite is small, its shape kept; a tall one fitted by its height */
	CHECK(ae_waypoint_badge_layout(10, 70, 15, &b) && b.icon_width <= b.badge_width - 2 && b.icon_height < b.icon_width / 3);
	CHECK(ae_waypoint_badge_layout(10, 10, 30, &b) && b.icon_height <= 18 && b.icon_width < b.icon_height);
	CHECK(!ae_waypoint_badge_layout(10, 0, 15, &b) && !ae_waypoint_badge_layout(0, 70, 15, &b));
	CHECK(!ae_waypoint_badge_layout(10, 70, 15, NULL) && !ae_waypoint_badge_layout(1, 70, 15, &b));
	/* the coverage: filled in the middle, clear at the corners, flat at the sides, pointed at the top and bottom */
	ae_waypoint_badge_coverage(52, 60, 0, fill);
	ae_waypoint_badge_coverage(52, 60, 1, line);
	CHECK(fill[30 * 52 + 26] == 255 && fill[0] == 0 && fill[59 * 52 + 51] == 0 && fill[51] == 0);
	CHECK(fill[30 * 52 + 1] == 255 && fill[0 * 52 + 26] > 0 && fill[5 * 52 + 2] == 0);
	CHECK(line[30 * 52 + 26] == 0 && line[30 * 52 + 0] > 0 && line[0 * 52 + 26] > 0);
	for (y = 0; y < 60; y++)
	{
		for (x = 0; x < 26; x++)
		{
			CHECK(fill[y * 52 + x] == fill[y * 52 + 51 - x] && fill[y * 52 + x] == fill[(59 - y) * 52 + x]);
			ring += line[y * 52 + x] > 0;
			inside += fill[y * 52 + x] == 255;
			CHECK(line[y * 52 + x] <= fill[y * 52 + x]);
		}
	}
	CHECK(ring > 0 && inside > 2 * ring);
}

/* (the glyphs are drawn on a 32 grid) */
#define K (AE_WAYPOINT_GLYPH_SIZE / 32)

static void centring_tests(void)
{
	int top, height, i, left, right;
	float in[4] = { 1.0f, 0.3f, 0.3f, 0.8f }, out[4];
	/* rows of different widths and ink offsets (the font's leading, scaled with each row): every row's ink centre is
	the axis, exactly (no whole-unit rounding) */
	float widths[4] = { 120.0f, 62.4f, 31.0f, 40.9f };
	float offsets[4] = { 7.0f, 0.6f, 5.0f, 2.45f };
	/* an arrow sheet 8 wide, 3 high: ink in columns 1..4 (the 30 is under the threshold), its middle 3 and the
	sheet's 4 */
	static const unsigned char sheet[24] = {
		0, 200, 0, 0, 0, 0, 0, 0,
		0, 0, 255, 0, 90, 30, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0 };

	for (i = 0; i < 4; i++)
	{
		float ink_left = ae_waypoint_row_left(57.25f, widths[i], offsets[i]) + offsets[i];

		CHECK(fabsf(ink_left + widths[i] / 2.0f - 57.25f) < 0.001f);
	}
	/* a block's axis: on its anchor where it is not pinned, moved by whole units where it is, its fraction kept */
	CHECK(fabsf(ae_waypoint_block_axis(39, 21, 50.3f) - 50.3f) < 0.001f);
	CHECK(fabsf(ae_waypoint_block_axis(40, 20, 50.3f) - 50.3f) < 0.001f);
	CHECK(fabsf(ae_waypoint_block_axis(45, 20, 50.3f) - 55.3f) < 0.001f);
	CHECK(fabsf(ae_waypoint_block_axis(0, 20, -3.75f) - 10.25f) < 0.001f);
	/* text scaled about its origin moves by the origin's move times one less the scale */
	CHECK(fabsf(ae_waypoint_scaled_shift(0.3f, 0.4f) * (1.0f - 0.4f) - 0.3f) < 0.001f);
	CHECK(fabsf(ae_waypoint_scaled_shift(0.5f, 1.6f) * (1.0f - 1.6f) - 0.5f) < 0.001f);
	CHECK(ae_waypoint_scaled_shift(0.5f, 1.0f) == 0.0f && ae_waypoint_scaled_shift(0.5f, 0.97f) == 0.0f);
	CHECK(ae_waypoint_scaled_shift(0.5f, nanf("")) == 0.0f);
	/* a sprite's ink: its columns, and their middle from the sprite's as a fraction of its width */
	CHECK(ae_waypoint_ink_columns(sheet, 8, 3, 64, &left, &right) && left == 1 && right == 4);
	CHECK(fabsf(ae_waypoint_ink_middle(left, right, 8) - (-1.0f / 8.0f)) < 0.0001f);
	CHECK(!ae_waypoint_ink_columns(sheet, 8, 3, 256, &left, &right));
	CHECK(!ae_waypoint_ink_columns(NULL, 8, 3, 64, &left, &right) && !ae_waypoint_ink_columns(sheet, 0, 3, 64, NULL, NULL));
	CHECK(ae_waypoint_ink_middle(0, 7, 8) == 0.0f && ae_waypoint_ink_middle(3, 2, 8) == 0.0f &&
		ae_waypoint_ink_middle(0, 8, 8) == 0.0f);
	/* a row's ink: capitals start below the line's top and the row is as high as they are */
	ae_waypoint_row_ink(14, 10, 1.0f, &top, &height);
	CHECK(top == 4 && height == 11);
	ae_waypoint_row_ink(14, 10, 0.8f, &top, &height);
	CHECK(top == 3 && height == 9);
	ae_waypoint_row_ink(8, 12, 1.0f, &top, &height);
	CHECK(top == 0 && height == 13);
	/* a respawning item: the mark much fainter and grey, the name at 85%, same colour */
	ae_waypoint_fade(AE_WAYPOINT_FADE_MARK, in, out);
	CHECK(out[3] > 0.31f && out[3] < 0.33f && out[0] < in[0] && out[1] > in[1] && out[2] > in[2]);
	CHECK(out[0] > 0.7f && out[0] < 0.75f && out[1] > 0.4f && out[1] < 0.5f);
	ae_waypoint_fade(AE_WAYPOINT_FADE_NAME, in, out);
	CHECK(out[0] == in[0] && out[1] == in[1] && out[3] > 0.67f && out[3] < 0.69f);
	ae_waypoint_fade(AE_WAYPOINT_FADE_MARK, in, in);
	CHECK(in[3] < 0.33f);
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
		CHECK(covered > 120 * K * K && covered < 700 * K * K && solid > 40 * K * K);
		CHECK(right - left >= 20 * K && bottom - top >= 16 * K && left >= K && right <= 31 * K - 1 && top >= K && bottom <= 31 * K - 1);
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

static void dead_ally_tests(void)
{
	int distance = -1, name = -1;

	/* (a teammate, dead, the body there, in a team game: marked; any one of those not so, not) */
	CHECK(ae_waypoint_dead_ally_marked(1, 0, 1, 0, 1));
	CHECK(!ae_waypoint_dead_ally_marked(0, 0, 1, 0, 1));
	CHECK(!ae_waypoint_dead_ally_marked(1, 1, 1, 0, 1));
	CHECK(!ae_waypoint_dead_ally_marked(1, 0, 0, 0, 1));
	CHECK(!ae_waypoint_dead_ally_marked(1, 0, 1, 1, 1));
	CHECK(!ae_waypoint_dead_ally_marked(1, 0, 1, 0, 0));
	/* (TEXT: the name; ICONS: the distance; BOTH: both; the cross always) */
	ae_waypoint_dead_ally_rows(AE_WAYPOINT_TEXT, &distance, &name);
	CHECK(!distance && name);
	ae_waypoint_dead_ally_rows(AE_WAYPOINT_ICONS, &distance, &name);
	CHECK(distance && !name);
	ae_waypoint_dead_ally_rows(AE_WAYPOINT_BOTH, &distance, &name);
	CHECK(distance && name);
	ae_waypoint_dead_ally_rows(AE_WAYPOINT_BOTH, NULL, NULL);
	/* (the cross and its outline: the outline covers the cross, and is more) */
	{
		int index, covered = 0, wider = 0;

		for (index = 0; index < AE_WAYPOINT_GLYPH_SIZE * AE_WAYPOINT_GLYPH_SIZE; index++)
		{
			covered += ae_waypoint_glyphs[AE_WAYPOINT_GLYPH_DEAD_ALLY][index] == 255 &&
				ae_waypoint_glyphs[AE_WAYPOINT_GLYPH_DEAD_ALLY_OUTLINE][index] != 255;
			wider += ae_waypoint_glyphs[AE_WAYPOINT_GLYPH_DEAD_ALLY_OUTLINE][index] >
				ae_waypoint_glyphs[AE_WAYPOINT_GLYPH_DEAD_ALLY][index];
		}
		CHECK(covered == 0 && wider > 200);
	}
}

int main(void)
{
	style_tests();
	source_tests();
	class_definition_tests();
	sprite_tests();
	stock_layout_tests();
	line_tests();
	picture_row_tests();
	badge_tests();
	centring_tests();
	glyph_tests();
	dead_ally_tests();
	if (failures)
	{
		printf("ae_waypoint_icons: %d failed\n", failures);
		return 1;
	}
	printf("ae_waypoint_icons: all passed\n");
	return 0;
}
