/*
AE_WHEEL_LAYOUT.C

The callout wheel's ring as numbers (ae_wheel_layout.h). Sizes are at 1080 lines in one view, and a split
view's own (not the same numbers times 0.68 everywhere: the gap, the wedge and the sprite are given whole).
*/

#include <math.h>
#include <wchar.h>

#include "ae_wheel_layout.h"
#include "ae_wheel_rules.h"

/* the HUD's leaning forms (ae_strip_layout.h's AE_STRIP_LEAN), of a form's height */
#define AE_WHEEL_LEAN 0.46f
/* the lines a view has when the ring's sizes are given: one view, and a split view */
#define ONE_VIEW_LINES 1080.0f
#define SPLIT_VIEW_LINES 540.0f
/* a sector's arcs are drawn from this many steps */
#define OUTER_STEPS 20
#define INNER_STEPS 12
#define DEGREES_PER_ENTRY 45.0f
#define RADIANS(degrees) ((degrees) * 3.14159265f / 180.0f)
#define DEGREES(radians) ((radians) * 180.0f / 3.14159265f)

/* the steering wheel's picture fills this much of its square, the SVG's own */
#define RIDE_INK 0.9f

static float whole(float value)
{
	return floorf(value + 0.5f);
}

static float larger(float a, float b)
{
	return a > b ? a : b;
}

void ae_wheel_layout_get(int split, float view_height, struct ae_wheel_layout *layout)
{
	float height_scale, k;

	if (!(view_height > 0.0f))
		view_height = split ? SPLIT_VIEW_LINES : ONE_VIEW_LINES;
	height_scale = view_height / (split ? SPLIT_VIEW_LINES : ONE_VIEW_LINES);
	k = (split ? AE_WHEEL_SPLIT_SCALE : 1.0f) * height_scale;
	layout->split = split != 0;
	layout->scale = k;
	layout->height_scale = height_scale;
	layout->inner = 104.0f * k;
	layout->outer = 224.0f * k;
	layout->gap = split ? 9.0f * height_scale : 12.0f * k;
	layout->corner = 5.0f * k;
	layout->grow = split ? 6.0f * k : 10.0f * k;
	layout->line = larger(2.0f * k, 1.5f);
	layout->keyline = larger(4.0f * k, 3.0f);
	layout->icon = whole(44.0f * k);
	layout->ride = whole(split ? 43.0f * height_scale : 60.0f * k);
	layout->sprite_width = whole(62.0f * height_scale);
	layout->sprite_height = 24.0f * height_scale;
	layout->rifle_boost = split ? 1.4f : 2.5f;
	layout->caption_px = larger(17.0f * k, 16.0f);
	layout->wedge_tip = split ? 62.0f * height_scale : 90.0f * k;
	layout->wedge_base = split ? 52.5f * height_scale : 76.0f * k;
	layout->wedge_half = split ? 7.5f * height_scale : 11.0f * k;
	layout->wedge_notch = 5.0f * k;
	layout->word_height = larger(40.0f * k, 28.0f);
	layout->word_px = larger(22.0f * k, 16.0f);
	layout->word_pad = 44.0f * k;
	layout->word_past = 12.0f * k;
	layout->word_above = split != 0;
	layout->word_nudge = split ? 1.0f : 0.0f;
	layout->legend = !split;
	layout->legend_y = split ? 0.0f : 322.0f * k;
	layout->legend_px = larger(17.0f * k, 16.0f);
	layout->chip_height = whole(24.0f * k);
	layout->chip_gap = 8.0f * k;
	layout->legend_gap = 22.0f * k;
	layout->died_y = 152.0f * k;
	layout->died_px = larger(17.0f * k, 16.0f);
}

float ae_wheel_outer_radius(int split, float view_height)
{
	struct ae_wheel_layout layout;

	ae_wheel_layout_get(split, view_height, &layout);
	return layout.outer + layout.grow;
}

void ae_wheel_centre(float view_width, float view_height, float *x, float *y)
{
	*x = floorf(view_width / 2.0f);
	*y = floorf(view_height / 2.0f);
}

/* ---------- sectors */

/* a sector's half angle at a radius, in degrees: its sides are parallel to the boundary between two entries, a half gap
from it, so the angle is 22.5 less what the half gap covers at that radius */
static float side_angle(float radius, float gap)
{
	return DEGREES_PER_ENTRY / 2.0f - DEGREES(asinf(gap / 2.0f / radius));
}

/* the point at an angle (degrees clockwise from up) and a radius */
static void polar(float degrees, float radius, float point[2])
{
	point[0] = sinf(RADIANS(degrees)) * radius;
	point[1] = -cosf(RADIANS(degrees)) * radius;
}

int ae_wheel_sector_polygon(struct ae_wheel_layout const *layout, int entry, int grown, float points[][2])
{
	float axis = (float)entry * DEGREES_PER_ENTRY;
	float inner = layout->inner;
	float outer = layout->outer + (grown ? layout->grow : 0.0f);
	float cut = layout->corner;
	float half_outer = side_angle(outer, layout->gap), half_inner = side_angle(inner, layout->gap);
	float cut_outer = DEGREES(cut / outer), cut_inner = DEGREES(cut / inner);
	int count = 0, step;

	for (step = 0; step <= OUTER_STEPS; step++)
	{
		polar(axis - half_outer + cut_outer + (2.0f * half_outer - 2.0f * cut_outer) * (float)step / (float)OUTER_STEPS,
			outer, points[count++]);
	}
	polar(axis + side_angle(outer - cut, layout->gap), outer - cut, points[count++]);
	polar(axis + side_angle(inner + cut, layout->gap), inner + cut, points[count++]);
	for (step = 0; step <= INNER_STEPS; step++)
	{
		polar(axis + half_inner - cut_inner - (2.0f * half_inner - 2.0f * cut_inner) * (float)step / (float)INNER_STEPS,
			inner, points[count++]);
	}
	polar(axis - side_angle(inner + cut, layout->gap), inner + cut, points[count++]);
	polar(axis - side_angle(outer - cut, layout->gap), outer - cut, points[count++]);
	return count;
}

int ae_wheel_sector_contains(struct ae_wheel_layout const *layout, int entry, int grown, float x, float y)
{
	float radius = hypotf(x, y);
	float axis = RADIANS((float)entry * DEGREES_PER_ENTRY);
	float along = x * sinf(axis) - y * cosf(axis);
	float across = x * cosf(axis) + y * sinf(axis);
	float angle;

	if (radius < layout->inner || radius > layout->outer + (grown ? layout->grow : 0.0f))
		return 0;
	angle = DEGREES(atan2f(fabsf(across), along));
	if (angle >= DEGREES_PER_ENTRY / 2.0f)
		return 0;
	return radius * sinf(RADIANS(DEGREES_PER_ENTRY / 2.0f - angle)) >= layout->gap / 2.0f;
}

int ae_wheel_wedge_polygon(struct ae_wheel_layout const *layout, int entry, float points[AE_WHEEL_WEDGE_POINTS][2])
{
	float angle = RADIANS((float)entry * DEGREES_PER_ENTRY);
	float dx = sinf(angle), dy = -cosf(angle);
	float base = layout->wedge_base, half = layout->wedge_half;
	float tip_x = dx * layout->wedge_tip, tip_y = dy * layout->wedge_tip;
	float wing_x[2], wing_y[2];

	wing_x[0] = dx * base - dy * half;
	wing_y[0] = dy * base + dx * half;
	wing_x[1] = dx * base + dy * half;
	wing_y[1] = dy * base - dx * half;
	points[0][0] = tip_x;
	points[0][1] = tip_y;
	points[1][0] = (tip_x + wing_x[0]) / 2.0f;
	points[1][1] = (tip_y + wing_y[0]) / 2.0f;
	points[2][0] = wing_x[0];
	points[2][1] = wing_y[0];
	points[3][0] = dx * (base + layout->wedge_notch);
	points[3][1] = dy * (base + layout->wedge_notch);
	points[4][0] = wing_x[1];
	points[4][1] = wing_y[1];
	points[5][0] = (tip_x + wing_x[1]) / 2.0f;
	points[5][1] = (tip_y + wing_y[1]) / 2.0f;
	return AE_WHEEL_WEDGE_POINTS;
}

/* ---------- places */

void ae_wheel_split_offset(int entry, int *x, int *y)
{
	static int const offsets[AE_WHEEL_COUNT][2] =
	{
		{ 0, 6 },		/* SMART */
		{ -1, 8 },		/* ATTACK */
		{ 2, 13 },		/* ENEMY */
		{ 5, -7 },		/* REGROUP */
		{ 0, 18 },		/* DEFEND */
		{ 1, 19 },		/* RIDE */
		{ -3, 14 },		/* GOING */
		{ 3, 8 },		/* WATCH */
	};

	*x = 0;
	*y = 0;
	if (entry >= 0 && entry < AE_WHEEL_COUNT)
	{
		*x = offsets[entry][0];
		*y = offsets[entry][1];
	}
}

void ae_wheel_entry_place(struct ae_wheel_layout const *layout, int entry, struct ae_wheel_place *place)
{
	float k = layout->scale, hs = layout->height_scale;
	float angle = RADIANS((float)entry * DEGREES_PER_ENTRY);
	float middle = (layout->inner + layout->outer) / 2.0f;
	float caption_x, caption_y, icon_y;

	place->middle_x = sinf(angle) * middle;
	place->middle_y = -cosf(angle) * middle;
	place->caption_over_sprite = 0;
	if (!layout->split)
	{
		/* the icon 11 above the sector's middle, its caption's capitals' middle 26 under it */
		caption_x = place->middle_x;
		caption_y = place->middle_y + 26.0f * k;
		icon_y = place->middle_y - 11.0f * k;
		if (entry == AE_WHEEL_RIDE)
			icon_y = whole(caption_y) - (8.5f + 6.0f) * k - RIDE_INK * layout->ride / 2.0f;
	}
	else
	{
		int offset_x, offset_y;

		ae_wheel_split_offset(entry, &offset_x, &offset_y);
		caption_x = place->middle_x + (float)offset_x * hs;
		caption_y = place->middle_y + (float)offset_y * hs;
		if (entry == AE_WHEEL_REGROUP)
		{
			/* its caption over the sprite: the sprite is wide and the sector narrows toward the hub */
			place->caption_over_sprite = 1;
			icon_y = whole(caption_y) + (0.35f * layout->caption_px + 3.0f * hs) + layout->sprite_height / 2.0f;
		}
		else if (entry == AE_WHEEL_RIDE)
			icon_y = whole(caption_y) - (8.5f + 3.0f) * hs - RIDE_INK * layout->ride / 2.0f;
		else
			icon_y = whole(caption_y) - (7.0f + 3.0f) * hs - layout->icon / 2.0f;
	}
	place->caption_x = whole(caption_x);
	place->caption_y = whole(caption_y);
	place->icon_x = whole(caption_x);
	place->icon_y = whole(icon_y);
}

/* ---------- the word's cell and the legend */

void ae_wheel_word_cell(struct ae_wheel_layout const *layout, float text_width, float points[8][2], float *x, float *y)
{
	float height = layout->word_height;
	float lean = AE_WHEEL_LEAN * height;
	float width = text_width + layout->word_pad + lean;
	float offset = layout->outer + layout->grow + layout->word_past + height / 2.0f;
	float cut = 4.0f * layout->scale;
	float x0 = -width / 2.0f, x1 = width / 2.0f, y0, y1, middle;

	middle = layout->word_above ? -offset - layout->word_nudge : offset;
	y0 = middle - height / 2.0f;
	y1 = middle + height / 2.0f;
	points[0][0] = x0 + lean + cut; points[0][1] = y0;
	points[1][0] = x1 - cut; points[1][1] = y0;
	points[2][0] = x1; points[2][1] = y0 + cut * 0.6f;
	points[3][0] = x1 - lean; points[3][1] = y1 - cut * 0.6f;
	points[4][0] = x1 - lean - cut; points[4][1] = y1;
	points[5][0] = x0 + cut; points[5][1] = y1;
	points[6][0] = x0; points[6][1] = y1 - cut * 0.6f;
	points[7][0] = x0 + lean; points[7][1] = y0 + cut * 0.6f;
	*x = 0.0f;
	*y = middle;
}

float ae_wheel_legend_place(struct ae_wheel_layout const *layout, int count, float const *chip_widths,
	float const *word_widths, float *lefts)
{
	float total = 0.0f, at;
	int item;

	for (item = 0; item < count; item++)
		total += (chip_widths[item] > 0.0f ? chip_widths[item] + layout->chip_gap : 0.0f) + word_widths[item];
	if (count > 1)
		total += layout->legend_gap * (float)(count - 1);
	at = whole(-total / 2.0f);
	for (item = 0; item < count; item++)
	{
		lefts[item] = at;
		at += (chip_widths[item] > 0.0f ? chip_widths[item] + layout->chip_gap : 0.0f) + word_widths[item] +
			layout->legend_gap;
	}
	return total;
}

/* ---------- words and motion */

wchar_t const *ae_wheel_caption(int entry, int dead)
{
	static wchar_t const *const captions[AE_WHEEL_COUNT] =
		{ L"SMART", L"ATTACK", L"ENEMY", L"REGROUP", L"DEFEND", L"RIDE", L"GOING", L"WATCH" };

	if (entry < 0 || entry >= AE_WHEEL_COUNT)
		return L"";
	/* (dead: the top says HELP, and nobody dead is going anywhere: GO) */
	if (dead && entry == AE_WHEEL_SMART)
		return L"HELP";
	if (dead && entry == AE_WHEEL_GOING)
		return L"GO";
	return captions[entry];
}

wchar_t const *ae_wheel_word(int entry, int dead)
{
	static wchar_t const *const words[AE_WHEEL_COUNT] =
	{
		L"SMART CALL", L"ATTACKING HERE", L"ENEMY SPOTTED", L"REGROUP", L"DEFENDING HERE", L"NEED A RIDE",
		L"GOING HERE", L"WATCHING HERE"
	};

	if (entry < 0 || entry >= AE_WHEEL_COUNT)
		return L"";
	/* (dead: the top asks for help, and the place call is an order: nobody dead is going anywhere) */
	if (dead && entry == AE_WHEEL_SMART)
		return L"HELP ME";
	if (dead && entry == AE_WHEEL_GOING)
		return L"GO HERE";
	return words[entry];
}

int ae_wheel_motion(long since_ms, float *flash, float *alpha)
{
	*flash = 0.0f;
	*alpha = 0.0f;
	if (since_ms < 0 || since_ms >= AE_WHEEL_FLASH_MS + AE_WHEEL_CLOSE_MS)
		return 0;
	if (since_ms < AE_WHEEL_FLASH_MS)
	{
		*flash = (float)since_ms / (float)AE_WHEEL_FLASH_MS;
		*alpha = 1.0f;
		return 1;
	}
	*flash = 1.0f;
	*alpha = 1.0f - (float)(since_ms - AE_WHEEL_FLASH_MS) / (float)AE_WHEEL_CLOSE_MS;
	return 1;
}
