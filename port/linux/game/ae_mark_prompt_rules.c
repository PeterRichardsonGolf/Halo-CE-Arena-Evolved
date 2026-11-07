/*
AE_MARK_PROMPT_RULES.C

Pure rules for the AIM PROMPT (ae_mark_prompt_rules.h). No engine includes.
*/

#include "ae_mark_prompt_rules.h"
#include "ae_mark_rules.h"

#include <math.h>
#include <wchar.h>

/* ---------- the words */

/* the items and places whose own word is longer than the one the waypoints and the markers give them */
static struct { wchar_t const *name; wchar_t const *word; } const shortened[] =
{
	{ L"SNIPER RIFLE", L"SNIPER" },
	{ L"ROCKET LAUNCHER", L"ROCKETS" },
	{ L"ACTIVE CAMO", L"CAMO" },
	{ L"BOMB SITE", L"BOMB" },
};

static wchar_t const *short_word(wchar_t const *name)
{
	int index;

	for (index = 0; index < (int)(sizeof(shortened) / sizeof(shortened[0])); index++)
	{
		if (!wcscmp(name, shortened[index].name))
			return shortened[index].word;
	}
	return name;
}

wchar_t const *ae_prompt_word(int tier, int verb, wchar_t const *item_name)
{
	int named = item_name && item_name[0];

	switch (verb)
	{
	case AE_CALL_OK: return L"OK";
	case AE_CALL_NEVER_MIND: return L"NEVER MIND";
	case AE_CALL_DIBS: return L"DIBS";
	case AE_CALL_CANCEL: return L"CANCEL";
	case AE_CALL_MARK:
	case AE_CALL_WHEEL:
		break;
	default:
		return NULL;
	}
	switch (tier)
	{
	case AE_TIER_ENEMY: return L"ENEMY";
	case AE_TIER_BODY: return L"FIGHT HERE";
	case AE_TIER_GROUND: return L"GO HERE";
	case AE_TIER_NAMED: return named ? short_word(item_name) : L"GO HERE";
	case AE_TIER_PICKUP: return named ? short_word(item_name) : L"ITEM";
	}
	return NULL;
}

int ae_prompt_word_is_enemy(int tier, int verb)
{
	return tier == AE_TIER_ENEMY && (verb == AE_CALL_MARK || verb == AE_CALL_WHEEL);
}

/* ---------- the row */

static float whole(float value)
{
	float rounded = floorf(value + 0.5f);

	return rounded < 1.0f ? 1.0f : rounded;
}

void ae_prompt_metrics_get(int split, float hud_scale, struct ae_prompt_metrics *metrics)
{
	if (!(hud_scale > 0.0f))
		hud_scale = 0.0f;
	metrics->chip_height = whole((split ? 20.0f : 24.0f) * hud_scale);
	metrics->word_px = whole((split ? 16.0f : 20.0f) * hud_scale);
	metrics->chip_gap = whole(8.0f * hud_scale);
	metrics->halo = whole(2.0f * hud_scale);
}

/* ---------- the place */

static int box_ok(struct ae_prompt_box const *b)
{
	return __builtin_isfinite(b->x0) && __builtin_isfinite(b->y0) && __builtin_isfinite(b->x1) && __builtin_isfinite(b->y1) && b->x1 >= b->x0 && b->y1 >= b->y0;
}

/* whether the boxes come within margin of each other */
static int meets(struct ae_prompt_box const *a, struct ae_prompt_box const *b, float margin)
{
	return a->x0 < b->x1 + margin && b->x0 - margin < a->x1 && a->y0 < b->y1 + margin && b->y0 - margin < a->y1;
}

/* how far a box's nearest edge is from a point */
static float box_distance(struct ae_prompt_box const *b, float x, float y)
{
	float dx = x < b->x0 ? b->x0 - x : x > b->x1 ? x - b->x1 : 0.0f;
	float dy = y < b->y0 ? b->y0 - y : y > b->y1 ? y - b->y1 : 0.0f;

	return hypotf(dx, dy);
}

enum { FIT_OK, FIT_ZONE, FIT_CAP, FIT_BOUNDS, FIT_TARGET, FIT_ROWS };

/* the row with its top left at (x, y), moved out of the zone if it is in it, and whether it stands */
static int fit(struct ae_prompt_in const *in, int rows, float x, float y, float *placed_x, float *placed_y)
{
	float s = in->scale, gap = AE_PROMPT_GAP * s - 0.001f;
	float zone = AE_PROMPT_ZONE * s, cap = AE_PROMPT_CAP * s;
	float cx = in->view_width / 2.0f, cy = in->view_height / 2.0f;
	struct ae_prompt_box row;
	int index;

	row.x0 = x;
	row.y0 = y;
	row.x1 = x + in->width;
	row.y1 = y + in->height;
	if (box_distance(&row, cx, cy) < zone)
	{
		/* (out along the line from the middle through the row's own middle, a pixel at a time, rounded away) */
		float ux = (row.x0 + row.x1) / 2.0f - cx, uy = (row.y0 + row.y1) / 2.0f - cy;
		float length = hypotf(ux, uy);
		int step;

		if (length < 0.5f)
		{
			ux = 1.0f;
			uy = 0.0f;
			length = 1.0f;
		}
		ux /= length;
		uy /= length;
		for (step = 1; step <= 2000; step++)
		{
			float sx = ux * (float)step, sy = uy * (float)step;

			sx = sx >= 0.0f ? ceilf(sx) : floorf(sx);
			sy = sy >= 0.0f ? ceilf(sy) : floorf(sy);
			row.x0 = x + sx;
			row.y0 = y + sy;
			row.x1 = row.x0 + in->width;
			row.y1 = row.y0 + in->height;
			if (box_distance(&row, cx, cy) >= zone)
				break;
		}
		if (box_distance(&row, cx, cy) < zone)
			return FIT_ZONE;
	}
	if (box_distance(&row, cx, cy) > cap)
		return FIT_CAP;
	if (row.x0 < 0.0f || row.y0 < 0.0f || row.x1 > in->view_width || row.y1 > in->view_height)
		return FIT_BOUNDS;
	if (meets(&row, &in->target, gap))
		return FIT_TARGET;
	for (index = 0; index < rows; index++)
	{
		if (meets(&row, &in->rows[index], gap))
			return FIT_ROWS;
	}
	*placed_x = row.x0;
	*placed_y = row.y0;
	return FIT_OK;
}

/* centred on a box, its foot a gap above its top */
static void above_of(struct ae_prompt_in const *in, struct ae_prompt_box const *b, float *x, float *y)
{
	*x = floorf((b->x0 + b->x1) / 2.0f - in->width / 2.0f + 0.5f);
	*y = floorf(b->y0 - AE_PROMPT_GAP * in->scale - in->height);
}

static void below_of(struct ae_prompt_in const *in, struct ae_prompt_box const *b, float *x, float *y)
{
	*x = floorf((b->x0 + b->x1) / 2.0f - in->width / 2.0f + 0.5f);
	*y = ceilf(b->y1 + AE_PROMPT_GAP * in->scale);
}

/* by the target, on its right or left, centred on it */
static void beside_of(struct ae_prompt_in const *in, int right, float *x, float *y)
{
	*x = right ? ceilf(in->target.x1 + AE_PROMPT_GAP * in->scale) :
		floorf(in->target.x0 - AE_PROMPT_GAP * in->scale - in->width);
	*y = floorf((in->target.y0 + in->target.y1) / 2.0f - in->height / 2.0f + 0.5f);
}

int ae_prompt_pin_keep(float target_x, float target_y, float view_x, float view_y, float scale, float row_height, int pinned)
{
	float within = (AE_PROMPT_ZONE + 2.0f * AE_PROMPT_GAP) * scale + row_height;
	float distance = hypotf(target_x - view_x, target_y - view_y);

	return pinned ? distance < within * 1.25f : distance < within;
}

int ae_prompt_place_pinned(struct ae_prompt_in const *in, float *x, float *y)
{
	float cx = in->view_width / 2.0f, cy = in->view_height / 2.0f;
	float zone = AE_PROMPT_ZONE * in->scale, gap = AE_PROMPT_GAP * in->scale - 0.001f;
	float top = floorf(cy - in->height / 2.0f + 0.5f);
	int rows = in->row_count < 0 ? 0 : in->row_count > AE_PROMPT_ROWS_MOST ? AE_PROMPT_ROWS_MOST : in->row_count;
	int side;

	for (side = 0; side < 2; side++)
	{
		struct ae_prompt_box row;
		int index, clear = 1;

		row.x0 = side ? floorf(cx - zone - in->width) : ceilf(cx + zone);
		row.x1 = row.x0 + in->width;
		row.y0 = top;
		row.y1 = top + in->height;
		if (row.x0 < 0.0f || row.x1 > in->view_width || row.y0 < 0.0f || row.y1 > in->view_height)
			continue;
		for (index = 0; index < rows; index++)
			clear = clear && !meets(&row, &in->rows[index], gap);
		if (clear)
		{
			*x = row.x0;
			*y = row.y0;
			return AE_PROMPT_BESIDE;
		}
	}
	return AE_PROMPT_NONE;
}

int ae_prompt_side_keep(float target_middle, float view_middle, float scale, int last)
{
	float limit = AE_PROMPT_ZONE * scale;

	if (target_middle > view_middle + limit)
		return 2;
	if (target_middle < view_middle - limit)
		return 1;
	return last ? last : 2;
}

int ae_prompt_place(struct ae_prompt_in const *in, float *x, float *y)
{
	struct ae_prompt_box zone_box;
	float cx, cy, px, py, ox, oy, tag_x;
	int rows, kind, result, right;

	*x = 0.0f;
	*y = 0.0f;
	if (!in || !(in->scale > 0.0f) || !(in->width > 0.0f) || !(in->height > 0.0f) || !(in->view_width > 0.0f) ||
		!(in->view_height > 0.0f) || !box_ok(&in->target))
	{
		return AE_PROMPT_NONE;
	}
	cx = in->view_width / 2.0f;
	cy = in->view_height / 2.0f;
	rows = in->row_count < 0 ? 0 : in->row_count > AE_PROMPT_ROWS_MOST ? AE_PROMPT_ROWS_MOST : in->row_count;
	tag_x = (in->target.x0 + in->target.x1) / 2.0f;
	right = in->side ? in->side == 2 : tag_x < cx + 1.0f && tag_x > cx - 1.0f ? 1 : tag_x > cx;
	/* (the primary place: above the name or the target; a pile, or the zone where it would stand, puts it beside) */
	if (in->tags >= 2)
	{
		kind = AE_PROMPT_BESIDE;
	}
	else
	{
		if (in->tags == 1 && box_ok(&in->tag))
		{
			kind = AE_PROMPT_ABOVE_TAG;
			above_of(in, &in->tag, &px, &py);
		}
		else
		{
			kind = AE_PROMPT_ABOVE;
			above_of(in, &in->target, &px, &py);
		}
		zone_box.x0 = px;
		zone_box.y0 = py;
		zone_box.x1 = px + in->width;
		zone_box.y1 = py + in->height;
		if (box_distance(&zone_box, cx, cy) < AE_PROMPT_ZONE * in->scale)
			kind = AE_PROMPT_BESIDE;
	}
	if (kind == AE_PROMPT_BESIDE)
		beside_of(in, right, &px, &py);
	result = fit(in, rows, px, py, &ox, &oy);
	if (result == FIT_CAP)
		return AE_PROMPT_NONE;
	if (result != FIT_OK && kind == AE_PROMPT_BESIDE)
	{
		/* (the other side of the target) */
		int again;

		beside_of(in, !right, &px, &py);
		again = fit(in, rows, px, py, &ox, &oy);
		if (again == FIT_OK)
			result = FIT_OK;
		else if (again == FIT_ROWS)
			result = FIT_ROWS;
	}
	if (result == FIT_ROWS || result == FIT_BOUNDS)
	{
		/* (the text rows or the view's edge in the way: under the target, the foot rule mirrored) */
		below_of(in, &in->target, &px, &py);
		if (fit(in, rows, px, py, &ox, &oy) != FIT_OK)
			return AE_PROMPT_NONE;
		kind = AE_PROMPT_BELOW;
	}
	else if (result != FIT_OK)
	{
		return AE_PROMPT_NONE;
	}
	*x = ox;
	*y = oy;
	return kind;
}
