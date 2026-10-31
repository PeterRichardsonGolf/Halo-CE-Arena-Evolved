/*
AE_WAYPOINT_ICONS.C

The item waypoints' pictures: the rules with no game types (ae_waypoint_icons.h).
*/

#include <stddef.h>
#include <math.h>
#include <string.h>

#include "ae_waypoint_icons.h"

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
		/* (the map's sprite; else, not to leave the row empty, our burst in an outlined badge) */
		if (stock_sprite_drawable)
			return AE_WAYPOINT_ICON_STOCK;
		if (!glyphs_drawable)
			return AE_WAYPOINT_ICON_NONE;
		if (glyph)
			*glyph = AE_WAYPOINT_GLYPH_POWER_WEAPON;
		return AE_WAYPOINT_ICON_GLYPH;

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
	plan->distance_row = 1;
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

int ae_waypoint_stock_height(int cap_height)
{
	return cap_height > 0 ? (cap_height * 14 + 5) / 10 : 0;
}

int ae_waypoint_stock_layout(int cap_height, int sprite_width, int sprite_height, int max_width, int *width, int *height,
	float *scale)
{
	int drawn_height = ae_waypoint_stock_height(cap_height);
	int drawn_width;

	if (sprite_width <= 0 || sprite_height <= 0 || drawn_height < 1)
		return 0;
	drawn_width = (int)(((long long)sprite_width * drawn_height + sprite_height / 2) / sprite_height);
	if (max_width > 0 && drawn_width > max_width)
	{
		/* (smaller, its shape kept) */
		drawn_width = max_width;
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
	if (scale)
		*scale = (float)drawn_width / (float)sprite_width;
	return 1;
}

int ae_waypoint_badge_height(int cap_height)
{
	return cap_height > 0 ? (cap_height * 18 + 5) / 10 : 0;
}

int ae_waypoint_badge_layout(int cap_height, int sprite_width, int sprite_height, struct ae_waypoint_badge *badge)
{
	int badge_height = ae_waypoint_badge_height(cap_height);
	int badge_width = (int)((float)badge_height * 0.8660254f + 0.5f);
	/* (the hexagon's circumradius, less the padding) */
	float radius = (float)badge_height / 2.0f - (float)(badge_height / 10 > 1 ? badge_height / 10 : 1);
	float apothem = 0.8660254f * radius;
	float t;
	float half_height;
	float half_width;
	int width, height;

	if (!badge || badge_height < 4 || radius < 1.0f || sprite_width <= 0 || sprite_height <= 0)
		return 0;
	/* (the largest rectangle of the sprite's shape inside the hexagon: its half height y0 and half width x0 = t y0;
	x0 <= apothem while y0 <= radius / 2, else x0 <= 2 apothem (radius - y0) / radius) */
	t = (float)sprite_width / (float)sprite_height;
	half_height = t > 1.7320508f ? apothem / t : 1.7320508f * radius / (t + 1.7320508f);
	half_width = t * half_height;
	width = (int)(2.0f * half_width + 0.5f);
	height = (int)(2.0f * half_height + 0.5f);
	if (width < 1)
		width = 1;
	if (height < 1)
		height = 1;
	badge->icon_width = width;
	badge->icon_height = height;
	badge->scale = 2.0f * half_height / (float)sprite_height;
	badge->badge_height = badge_height;
	badge->badge_width = badge_width;
	return 1;
}

static float clamp01(float value)
{
	return value < 0.0f ? 0.0f : value > 1.0f ? 1.0f : value;
}

void ae_waypoint_badge_coverage(int width, int height, int outline, unsigned char *coverage)
{
	float line = (float)height / 12.0f;
	float centre_y = (float)height / 2.0f;
	float k = (float)height / (2.0f * (float)width);
	float norm = 1.0f / sqrtf(1.0f + k * k);
	int x, y;

	if (!coverage || width < 1 || height < 1)
		return;
	if (line < 1.0f)
		line = 1.0f;
	for (y = 0; y < height; y++)
	{
		for (x = 0; x < width; x++)
		{
			float cx = (float)x + 0.5f - (float)width / 2.0f;
			float cy = (float)y + 0.5f - centre_y;
			float side, slant, edge, value;

			cx = cx < 0.0f ? -cx : cx;
			cy = cy < 0.0f ? -cy : cy;
			/* (the distances to the vertical side and to the slanted edge, inwards) */
			side = (float)width / 2.0f - cx;
			slant = (centre_y - cy - k * cx) * norm;
			edge = side < slant ? side : slant;
			value = clamp01(edge + 0.5f);
			if (outline)
				value *= 1.0f - clamp01(edge - line + 0.5f);
			coverage[y * width + x] = (unsigned char)(value * 255.0f + 0.5f);
		}
	}
}

float ae_waypoint_row_left(float axis, float ink_width, float ink_offset)
{
	return axis - ink_width / 2.0f - ink_offset;
}

float ae_waypoint_block_axis(int left, int width, float anchor_x)
{
	float unpinned = anchor_x - (float)width / 2.0f;

	return (float)left + (float)width / 2.0f + (unpinned - floorf(unpinned));
}

float ae_waypoint_scaled_shift(float shift, float scale)
{
	float moved = 1.0f - scale;

	/* (written so that a NaN gives no move) */
	if (!(moved > 0.05f || moved < -0.05f))
		return 0.0f;
	return shift / moved;
}

int ae_waypoint_ink_columns(const unsigned char *weights, int width, int height, int threshold, int *left, int *right)
{
	int first = -1;
	int last = -1;
	int x, y;

	if (!weights || width < 1 || height < 1)
		return 0;
	for (x = 0; x < width; x++)
	{
		for (y = 0; y < height; y++)
		{
			if (weights[y * width + x] >= threshold)
			{
				if (first < 0)
					first = x;
				last = x;
				break;
			}
		}
	}
	if (first < 0)
		return 0;
	if (left)
		*left = first;
	if (right)
		*right = last;
	return 1;
}

float ae_waypoint_ink_middle(int left, int right, int width)
{
	if (width < 1 || left < 0 || right < left || right >= width)
		return 0.0f;
	return ((float)(left + right + 1) / 2.0f - (float)width / 2.0f) / (float)width;
}

void ae_waypoint_row_ink(int ascent, int cap_height, float scale, int *top_offset, int *height)
{
	int top = (int)((float)(ascent - cap_height) * scale + 0.5f);
	int ink = (int)((float)cap_height * scale + 0.5f) + 1;

	if (top_offset)
		*top_offset = top < 0 ? 0 : top;
	if (height)
		*height = ink < 1 ? 1 : ink;
}

void ae_waypoint_fade(int part, const float *in, float *out)
{
	int index;

	if (!in || !out)
		return;
	if (part == AE_WAYPOINT_FADE_MARK)
	{
		for (index = 0; index < 3; index++)
			out[index] = in[index] * 0.4f + 0.55f * 0.6f;
		out[3] = in[3] * 0.4f;
		return;
	}
	for (index = 0; index < 3; index++)
		out[index] = in[index];
	out[3] = in[3] * 0.85f;
}

int ae_waypoint_dead_ally_marked(int team_game, int is_viewer, int teammate, int alive, int body)
{
	return team_game && !is_viewer && teammate && !alive && body;
}

void ae_waypoint_dead_ally_rows(int style, int *distance, int *name)
{
	if (distance)
		*distance = style == AE_WAYPOINT_ICONS || style == AE_WAYPOINT_BOTH;
	if (name)
		*name = style != AE_WAYPOINT_ICONS;
}
