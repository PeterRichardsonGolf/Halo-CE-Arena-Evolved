/*
AE_WAYPOINT_ICONS.C

The item waypoints' pictures: the rules with no game types (ae_waypoint_icons.h).
*/

#include <stddef.h>

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

int ae_waypoint_line_parts(int style, int members, const int *member_has_icon, int has_time,
	struct ae_waypoint_part *parts, int max_parts)
{
	int count = 0;
	int member;
	/* (the room the time after the members takes: a space and it) */
	int after;

	if (!parts || max_parts <= 0 || members <= 0)
		return 0;
	if (style == AE_WAYPOINT_ICONS)
	{
		int previous_icon = 0;

		after = has_time ? 2 : 0;
		for (member = 0; member < members; member++)
		{
			int icon = member_has_icon && member_has_icon[member];

			/* (a member takes a part between and itself) */
			if (count + (member ? 2 : 1) + after > max_parts)
				break;
			if (member)
				count = part_add(parts, count, max_parts, previous_icon && icon ? AE_WAYPOINT_PART_SPACE :
					AE_WAYPOINT_PART_SEPARATOR, 0);
			count = part_add(parts, count, max_parts, icon ? AE_WAYPOINT_PART_ICON : AE_WAYPOINT_PART_NAME, member);
			previous_icon = icon;
		}
		if (has_time)
		{
			count = part_add(parts, count, max_parts, AE_WAYPOINT_PART_SPACE, 0);
			count = part_add(parts, count, max_parts, AE_WAYPOINT_PART_TIME, 0);
		}
		return count;
	}
	if (members == 1)
	{
		int icon = style == AE_WAYPOINT_BOTH && member_has_icon && member_has_icon[0];

		count = part_add(parts, count, max_parts, AE_WAYPOINT_PART_NAME, 0);
		if (has_time)
		{
			count = part_add(parts, count, max_parts, AE_WAYPOINT_PART_SPACE, 0);
			count = part_add(parts, count, max_parts, AE_WAYPOINT_PART_TIME, 0);
		}
		if (icon)
		{
			count = part_add(parts, count, max_parts, AE_WAYPOINT_PART_SPACE, 0);
			count = part_add(parts, count, max_parts, AE_WAYPOINT_PART_ICON, 0);
		}
		return count;
	}
	/* (several, in text: the time first) */
	if (has_time)
	{
		count = part_add(parts, count, max_parts, AE_WAYPOINT_PART_TIME, 0);
		count = part_add(parts, count, max_parts, AE_WAYPOINT_PART_SPACE, 0);
	}
	for (member = 0; member < members; member++)
	{
		if (count + (member ? 2 : 1) > max_parts)
			break;
		if (member)
			count = part_add(parts, count, max_parts, AE_WAYPOINT_PART_SEPARATOR, 0);
		count = part_add(parts, count, max_parts, AE_WAYPOINT_PART_NAME, member);
	}
	return count;
}
