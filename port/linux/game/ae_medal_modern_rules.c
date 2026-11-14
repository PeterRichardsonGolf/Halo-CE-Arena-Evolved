/*
AE_MEDAL_MODERN_RULES.C

The MODERN medal stack's pure rules (ae_medal_modern_rules.h).
*/

#include <math.h>
#include <string.h>

#include "ae_marker_rules.h"
#include "ae_medal_modern_rules.h"

static real larger(real a, real b)
{
	return a > b ? a : b;
}

static real clamp01(real value)
{
	return value < 0.0f ? 0.0f : value > 1.0f ? 1.0f : value;
}

void ae_mm_metrics_of(real view_width, real view_height, boolean split, struct ae_mm_metrics *metrics)
{
	real s = view_height / 1080.0f;

	metrics->name_px = larger(AE_MM_NAME_PX * s, split ? AE_MM_SPLIT_NAME_PX : AE_MM_NAME_FLOOR_PX);
	metrics->score_px = larger(AE_MM_SCORE_PX * s, split ? AE_MM_SCORE_FLOOR_PX + 2.0f : AE_MM_SCORE_FLOOR_PX);
	metrics->picture = larger(AE_MM_PICTURE * s, AE_MM_PICTURE_FLOOR);
	metrics->row_picture = larger(AE_MM_ROW_PICTURE * s, AE_MM_ROW_FLOOR);
	if (metrics->row_picture < metrics->picture + 2.0f)
		metrics->row_picture = metrics->picture + 2.0f;
	metrics->row_text = larger(AE_MM_ROW_TEXT * s, AE_MM_ROW_FLOOR);
	metrics->centre_x = view_width / 2.0f;
	metrics->name_right = metrics->centre_x - AE_MM_NAME_RIGHT * s;
	metrics->picture_left = metrics->name_right + AE_MM_PICTURE_GAP * s;
	metrics->picture_right = metrics->picture_left + metrics->picture;
	metrics->score_middle = AE_MM_SCORE_MIDDLE * s;
	metrics->rows_top = metrics->score_middle + 0.65f * metrics->score_px + AE_MM_ROWS_GAP * s;
	metrics->limit = AE_MM_LIMIT * s;
	metrics->zone = AE_MM_ZONE * s;
}

void ae_mm_reset(struct ae_mm_stack *stack)
{
	memset(stack, 0, sizeof(*stack));
	stack->last = -1;
}

real ae_mm_row_height(struct ae_mm_metrics const *metrics, struct ae_mm_row const *row)
{
	return row->kind == AE_MM_KIND_MEDAL && row->has_picture ? metrics->row_picture : metrics->row_text;
}

static boolean counted(struct ae_mm_row const *row, long now)
{
	return row->used && row->gone_at < 0 && now < row->life_end + AE_MM_FADE_TICKS;
}

static real y_at(struct ae_mm_row const *row, long now)
{
	real t = clamp01((real)(now - row->moved_at) / (real)AE_MM_CLOSE_TICKS);

	return row->y_from + (row->y_to - row->y_from) * t;
}

/* each row's place is under the rows holding a place before it */
static void retarget(struct ae_mm_stack *stack, struct ae_mm_metrics const *metrics, long now)
{
	int index;
	real top = 0.0f;

	for (index = 0; index < AE_MM_ROWS; index++)
	{
		struct ae_mm_row *row = &stack->rows[index];

		if (!counted(row, now))
			continue;
		if (row->y_to != top)
		{
			row->y_from = y_at(row, now);
			row->y_to = top;
			row->moved_at = now;
		}
		top += ae_mm_row_height(metrics, row);
	}
}

static void compact(struct ae_mm_stack *stack, long now)
{
	int from, to = 0;

	for (from = 0; from < AE_MM_ROWS; from++)
	{
		struct ae_mm_row const *row = &stack->rows[from];
		boolean alive = row->used && now < row->life_end + AE_MM_FADE_TICKS &&
			(row->gone_at < 0 || now < row->gone_at + AE_MM_GONE_TICKS);

		if (!alive)
			continue;
		if (to != from)
			stack->rows[to] = *row;
		to++;
	}
	for (; to < AE_MM_ROWS; to++)
		memset(&stack->rows[to], 0, sizeof(stack->rows[to]));
}

/* a full stack's oldest row, if it is not leaving already, has AE_MM_FULL_LIFE to live */
static void squeeze(struct ae_mm_stack *stack, long now)
{
	int index, live = 0, oldest = -1;

	for (index = 0; index < AE_MM_ROWS; index++)
	{
		if (!counted(&stack->rows[index], now))
			continue;
		live++;
		if (oldest < 0)
			oldest = index;
	}
	if (live >= AE_MM_MOST && oldest >= 0 && now < stack->rows[oldest].life_end)
	{
		long leave = stack->rows[oldest].born + AE_MM_FULL_LIFE;

		if (leave < now)
			leave = now;
		if (leave < stack->rows[oldest].life_end)
			stack->rows[oldest].life_end = leave;
	}
}

int ae_mm_score(struct ae_mm_stack const *stack, long now)
{
	int index, sum = 0;

	for (index = 0; index < AE_MM_ROWS; index++)
		if (counted(&stack->rows[index], now) && stack->rows[index].kind == AE_MM_KIND_MEDAL)
			sum += stack->rows[index].points;
	return sum;
}

static void note_score(struct ae_mm_stack *stack, long now)
{
	int score = ae_mm_score(stack, now);

	if (score != stack->score)
	{
		stack->score = score;
		stack->score_at = now;
	}
}

void ae_mm_step(struct ae_mm_stack *stack, struct ae_mm_metrics const *metrics, long now)
{
	if (stack->last >= 0 && now < stack->last)
		ae_mm_reset(stack);
	stack->last = now;
	compact(stack, now);
	squeeze(stack, now);
	compact(stack, now);
	retarget(stack, metrics, now);
	note_score(stack, now);
}

int ae_mm_add(struct ae_mm_stack *stack, struct ae_mm_metrics const *metrics, struct ae_mm_row const *row, long now)
{
	struct ae_mm_row *added;
	real room = metrics->limit - metrics->rows_top, height = ae_mm_row_height(metrics, row);
	int pushed = 0, index;

	ae_mm_step(stack, metrics, now);
	for (;;)
	{
		int live = 0, oldest = -1;
		real total = 0.0f;

		for (index = 0; index < AE_MM_ROWS; index++)
		{
			if (!counted(&stack->rows[index], now))
				continue;
			live++;
			total += ae_mm_row_height(metrics, &stack->rows[index]);
			if (oldest < 0)
				oldest = index;
		}
		if (oldest < 0 || (live < AE_MM_MOST && total + height <= room))
			break;
		/* (the oldest goes at once; a row the stack cannot hold at all still shows, alone) */
		stack->rows[oldest].gone_at = now;
		pushed++;
	}
	if (stack->rows[AE_MM_ROWS - 1].used)
	{
		memmove(&stack->rows[0], &stack->rows[1], sizeof(stack->rows[0]) * (AE_MM_ROWS - 1));
		memset(&stack->rows[AE_MM_ROWS - 1], 0, sizeof(stack->rows[0]));
	}
	for (index = 0; index < AE_MM_ROWS && stack->rows[index].used; index++)
		;
	added = &stack->rows[index];
	*added = *row;
	added->used = TRUE;
	added->born = now;
	added->life_end = now + AE_MM_LIFE;
	added->gone_at = -1;
	added->y_from = 0.0f;
	added->y_to = 0.0f;
	added->moved_at = now;
	squeeze(stack, now);
	/* (the new row's place is where it stands: it does not slide into it) */
	{
		real top = 0.0f;

		for (index = 0; index < AE_MM_ROWS; index++)
		{
			if (&stack->rows[index] == added)
			{
				added->y_from = top;
				added->y_to = top;
				break;
			}
			if (counted(&stack->rows[index], now))
				top += ae_mm_row_height(metrics, &stack->rows[index]);
		}
	}
	retarget(stack, metrics, now);
	note_score(stack, now);
	return pushed;
}

int ae_mm_count(struct ae_mm_stack const *stack)
{
	int index, count = 0;

	for (index = 0; index < AE_MM_ROWS; index++)
		if (stack->rows[index].used)
			count++;
	return count;
}

int ae_mm_live(struct ae_mm_stack const *stack, long now)
{
	int index, count = 0;

	for (index = 0; index < AE_MM_ROWS; index++)
		if (counted(&stack->rows[index], now))
			count++;
	return count;
}

static real row_alpha(struct ae_mm_row const *row, long now)
{
	real alpha = 1.0f;

	if (now >= row->life_end)
		alpha = 1.0f - (real)(now - row->life_end) / (real)AE_MM_FADE_TICKS;
	if (row->gone_at >= 0)
	{
		real gone = 1.0f - (real)(now - row->gone_at) / (real)AE_MM_GONE_TICKS;

		alpha = alpha < gone ? alpha : gone;
	}
	return clamp01(alpha);
}

boolean ae_mm_row_pose(struct ae_mm_row const *row, long now, real *y, real *alpha, real *name_scale)
{
	if (!row->used || now < row->born || now >= row->life_end + AE_MM_FADE_TICKS ||
		(row->gone_at >= 0 && now >= row->gone_at + AE_MM_GONE_TICKS))
		return FALSE;
	*y = y_at(row, now);
	*alpha = row_alpha(row, now);
	*name_scale = 1.0f + (AE_MM_POP - 1.0f) * (1.0f - clamp01((real)(now - row->born) / (real)AE_MM_NAME_POP_TICKS));
	return TRUE;
}

boolean ae_mm_score_pose(struct ae_mm_stack const *stack, long now, real *alpha, real *scale)
{
	int index;
	real best = -1.0f;

	for (index = 0; index < AE_MM_ROWS; index++)
	{
		struct ae_mm_row const *row = &stack->rows[index];

		if (counted(row, now) && row->kind == AE_MM_KIND_MEDAL && row->points > 0 && row_alpha(row, now) > best)
			best = row_alpha(row, now);
	}
	if (best <= 0.0f)
		return FALSE;
	*alpha = best;
	*scale = 1.0f + (AE_MM_POP - 1.0f) * (1.0f - clamp01((real)(now - stack->score_at) / (real)AE_MM_SCORE_POP_TICKS));
	return TRUE;
}

real ae_mm_gap(struct ae_mm_box const *a, struct ae_mm_box const *b)
{
	real gx = larger(a->x0 - b->x1, b->x0 - a->x1);
	real gy = larger(a->y0 - b->y1, b->y0 - a->y1);

	if (gx > 0.0f && gy > 0.0f)
		return (real)sqrt(gx * gx + gy * gy);
	return larger(gx, gy);
}

void ae_mm_box_of(struct ae_mm_metrics const *metrics, real widest_em, struct ae_mm_box *box)
{
	box->x0 = metrics->name_right - widest_em * metrics->name_px;
	box->x1 = metrics->picture_right;
	box->y0 = metrics->score_middle - 0.65f * metrics->score_px;
	box->y1 = metrics->limit;
}

real ae_mm_zone_gap(struct ae_mm_metrics const *metrics)
{
	/* the nearest the stack comes to the centre is its pictures' right edge, on the row's level with the zone's edge */
	return metrics->centre_x - metrics->picture_right - metrics->zone;
}

boolean ae_mm_clears(struct ae_mm_box const *stack, struct ae_mm_box const *others, int count, real clear)
{
	int index;

	for (index = 0; index < count; index++)
		if (ae_mm_gap(stack, &others[index]) < clear)
			return FALSE;
	return TRUE;
}

int ae_mm_word_line(wchar_t *out, int size, wchar_t const *name, float const rgb[3])
{
	int length = 0, channel;

	if (!out || size < 1)
		return 0;
	out[0] = 0;
	if (!name || size < 5 + (int)wcslen(name))
		return 0;
	out[length++] = AE_RUN_RGB;
	for (channel = 0; channel < 3; channel++)
	{
		float value = rgb ? rgb[channel] : 1.0f;
		int byte = (int)(clamp01(value) * 255.0f + 0.5f);

		out[length++] = AE_RUN_BYTE(byte);
	}
	while (*name)
		out[length++] = *name++;
	out[length] = 0;
	return length;
}
