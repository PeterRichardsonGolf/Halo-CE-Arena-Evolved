/*
AE_WAYPOINT_ICONS.C

The item waypoints' pictures: the rules with no game types (ae_waypoint_icons.h).
*/

#include <stddef.h>
#include <string.h>

#include "ae_waypoint_icons.h"

/* (a picture at most this many times as wide as high: the stock sniper rifle's is 4.6) */
#define AE_WAYPOINT_ICON_MAXIMUM_ASPECT 6
/* (a bitmap larger than this each way is not a HUD sheet) */
#define AE_WAYPOINT_MAXIMUM_BITMAP 4096

static int same_text(char const *text, char const *name)
{
	for (; *text && *name; text++, name++)
	{
		char character = *text >= 'A' && *text <= 'Z' ? (char)(*text - 'A' + 'a') : *text;

		if (character != *name)
			return 0;
	}
	return !*text && !*name;
}

int ae_waypoint_style_from_text(char const *text)
{
	if (text && same_text(text, "text"))
		return AE_WAYPOINT_TEXT;
	if (text && same_text(text, "icons"))
		return AE_WAYPOINT_ICONS;
	return AE_WAYPOINT_BOTH;
}

char const *ae_waypoint_style_name(int style)
{
	return style == AE_WAYPOINT_TEXT ? "text" : style == AE_WAYPOINT_ICONS ? "icons" : "both";
}

int ae_waypoint_icon_source(int item_class, int stock_sprite_drawable, int glyphs_drawable, int *glyph)
{
	switch (item_class)
	{
	case AE_WAYPOINT_ROCKETS:
	case AE_WAYPOINT_SNIPER:
	case AE_WAYPOINT_SHOTGUN:
		return stock_sprite_drawable ? AE_WAYPOINT_ICON_STOCK : AE_WAYPOINT_ICON_NONE;

	case AE_WAYPOINT_OVERSHIELD:
	case AE_WAYPOINT_CAMO:
		if (!glyphs_drawable)
			return AE_WAYPOINT_ICON_NONE;
		if (glyph)
			*glyph = item_class == AE_WAYPOINT_OVERSHIELD ? AE_WAYPOINT_GLYPH_OVERSHIELD : AE_WAYPOINT_GLYPH_CAMO;
		return AE_WAYPOINT_ICON_GLYPH;
	}
	return AE_WAYPOINT_ICON_NONE;
}

long ae_waypoint_class_definition(const long *class_definitions, int count, int item_class)
{
	if (!class_definitions || item_class < 0 || item_class >= count)
		return -1;
	return class_definitions[item_class];
}

int ae_waypoint_sprite_size(int bitmap_width, int bitmap_height, float x0, float x1, float y0, float y1,
	int *width, int *height)
{
	int sprite_width, sprite_height;

	if (bitmap_width < 2 || bitmap_height < 2 || bitmap_width > AE_WAYPOINT_MAXIMUM_BITMAP ||
		bitmap_height > AE_WAYPOINT_MAXIMUM_BITMAP)
	{
		return 0;
	}
	/* (written so that a NaN fails every test) */
	if (!(x0 >= 0.0f && x1 <= 1.0f && x0 < x1 && y0 >= 0.0f && y1 <= 1.0f && y0 < y1))
		return 0;
	sprite_width = (int)((x1 - x0) * (float)bitmap_width + 0.5f);
	sprite_height = (int)((y1 - y0) * (float)bitmap_height + 0.5f);
	if (sprite_width < 2 || sprite_height < 2)
		return 0;
	if (width)
		*width = sprite_width;
	if (height)
		*height = sprite_height;
	return 1;
}

int ae_waypoint_row_height(int text_line_height)
{
	return text_line_height > 0 ? (text_line_height * 11 + 5) / 10 : 0;
}

int ae_waypoint_icon_size(int sprite_width, int sprite_height, int row_height, int *width, int *height, float *scale)
{
	int drawn_height = row_height;
	int drawn_width;

	if (sprite_width <= 0 || sprite_height <= 0 || row_height <= 0)
		return 0;
	drawn_width = (int)(((long long)sprite_width * drawn_height + sprite_height / 2) / sprite_height);
	if (drawn_width > AE_WAYPOINT_ICON_MAXIMUM_ASPECT * row_height)
	{
		drawn_width = AE_WAYPOINT_ICON_MAXIMUM_ASPECT * row_height;
		drawn_height = (int)(((long long)sprite_height * drawn_width + sprite_width / 2) / sprite_width);
	}
	if (drawn_width < 1)
		drawn_width = 1;
	if (drawn_height < 1)
		drawn_height = 1;
	if (width)
		*width = drawn_width;
	if (height)
		*height = drawn_height;
	/* (by the side that was fitted, so that neither passes its room) */
	if (scale)
	{
		*scale = drawn_height == row_height ? (float)drawn_height / (float)sprite_height :
			(float)drawn_width / (float)sprite_width;
	}
	return 1;
}

static int part_add(struct ae_waypoint_part *parts, int count, int max_parts, int kind, int member)
{
	if (count < max_parts)
	{
		parts[count].kind = (short)kind;
		parts[count].member = (short)member;
		count++;
	}
	return count;
}

int ae_waypoint_line_plan(int style, int members, const int *member_has_icon, int has_time,
	struct ae_waypoint_plan *plan)
{
	int member;

	if (!plan)
		return 0;
	memset(plan, 0, sizeof(*plan));
	if (members <= 0)
		return 0;
	if (members > AE_WAYPOINT_MAXIMUM_MEMBERS)
		members = AE_WAYPOINT_MAXIMUM_MEMBERS;
	for (member = 0; member < members; member++)
	{
		if (style != AE_WAYPOINT_TEXT && member_has_icon && member_has_icon[member])
			plan->pictures[plan->picture_count++] = member;
	}
	/* (TEXT, and ICONS with no picture to show: one line of text, as before pictures) */
	if (style == AE_WAYPOINT_TEXT || (style == AE_WAYPOINT_ICONS && !plan->picture_count))
	{
		plan->picture_count = 0;
		return 0;
	}
	plan->stacked = 1;
	plan->time_row = has_time ? 1 : 0;
	for (member = 0; member < members; member++)
	{
		if (style == AE_WAYPOINT_BOTH || !(member_has_icon && member_has_icon[member]))
			plan->names[plan->name_count++] = member;
	}
	return 1;
}

int ae_waypoint_picture_row(int count, const int *widths, int gap, int *offsets, int *total)
{
	int index;
	int x = 0;

	if (total)
		*total = 0;
	if (count <= 0 || !widths)
		return 0;
	for (index = 0; index < count; index++)
	{
		if (index)
			x += gap > 0 ? gap : 0;
		if (offsets)
			offsets[index] = x;
		x += widths[index] > 0 ? widths[index] : 0;
	}
	if (total)
		*total = x;
	return count;
}

int ae_waypoint_name_parts(int count, const int *names, struct ae_waypoint_part *parts, int max_parts)
{
	int used = 0;
	int index;

	if (!parts || !names || max_parts <= 0)
		return 0;
	for (index = 0; index < count; index++)
	{
		if (used + (index ? 2 : 1) > max_parts)
			break;
		if (index)
			used = part_add(parts, used, max_parts, AE_WAYPOINT_PART_SEPARATOR, 0);
		used = part_add(parts, used, max_parts, AE_WAYPOINT_PART_NAME, names[index]);
	}
	return used;
}

int ae_waypoint_line_parts(int members, int has_time, struct ae_waypoint_part *parts, int max_parts)
{
	int count = 0;
	int index;

	if (!parts || max_parts <= 0 || members <= 0)
		return 0;
	if (members == 1)
	{
		/* (one label: its name, then the time) */
		count = part_add(parts, count, max_parts, AE_WAYPOINT_PART_NAME, 0);
		if (has_time)
		{
			count = part_add(parts, count, max_parts, AE_WAYPOINT_PART_SPACE, 0);
			count = part_add(parts, count, max_parts, AE_WAYPOINT_PART_TIME, 0);
		}
		return count;
	}
	/* (several: the time first) */
	if (has_time)
	{
		count = part_add(parts, count, max_parts, AE_WAYPOINT_PART_TIME, 0);
		count = part_add(parts, count, max_parts, AE_WAYPOINT_PART_SPACE, 0);
	}
	for (index = 0; index < members; index++)
	{
		if (count + (index ? 2 : 1) > max_parts)
			break;
		if (index)
			count = part_add(parts, count, max_parts, AE_WAYPOINT_PART_SEPARATOR, 0);
		count = part_add(parts, count, max_parts, AE_WAYPOINT_PART_NAME, index);
	}
	return count;
}
