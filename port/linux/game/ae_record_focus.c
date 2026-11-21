/* The SERVICE RECORD pages' boxes and the focus between them (see the header). */
#include <stdlib.h>

#include "ae_record_focus.h"

static struct { float x, y, w, h; char const *name; int page; } const boxes[AE_RF_BOXES] =
{
	{ 96, 232, 520, 440, "LAST GAME", 0 },
	{ 96, 696, 1728, 232, "VS YOU", 0 },
	{ 96, 232, 520, 440, "ACCURACY", 1 },
	{ 648, 232, 664, 440, "KILL KINDS", 1 },
	{ 1344, 232, 480, 440, "ASSISTS", 1 },
	{ 96, 696, 1104, 264, "DAMAGE", 1 },
	{ 1224, 696, 600, 264, "PER GAME", 1 },
	{ 96, 232, 520, 440, "OBJECTIVES", 2 },
	{ 648, 232, 664, 440, "BY KIND OF GAME", 2 },
	{ 1344, 232, 480, 440, "MEDALS", 2 },
	{ 96, 696, 1104, 264, "BY MAP", 2 },
	{ 1224, 696, 600, 264, "TOP WEAPONS", 2 },
	{ 648, 232, 664, 440, "CAREER TOTALS", 0 },
	{ 1344, 232, 480, 440, "HIGHLIGHTS", 0 },
	{ 1344, 232, 480, 440, "IDENTITY", 0 },
	{ 96, 696, 1728, 232, "YOUR RIVALS", 0 },
};

struct ae_rf_rect ae_rf_rect_of(int box)
{
	struct ae_rf_rect rect = { 0, 0, 0, 0 };

	if (box >= 0 && box < AE_RF_BOXES)
	{
		rect.x = boxes[box].x; rect.y = boxes[box].y; rect.w = boxes[box].w; rect.h = boxes[box].h;
	}
	return rect;
}

char const *ae_rf_name(int box)
{
	return box >= 0 && box < AE_RF_BOXES ? boxes[box].name : "";
}

int ae_rf_page_of(int box)
{
	return box >= 0 && box < AE_RF_BOXES ? boxes[box].page : -1;
}

int ae_rf_list(int page, unsigned long has, int *out)
{
	int box, count = 0;

	for (box = 0; box < AE_RF_BOXES; box++)
		if (boxes[box].page == page && (has & (1ul << box)))
			out[count++] = box;
	/* (top to bottom, then left to right: an insertion over the few) */
	{
		int a, b;

		for (a = 1; a < count; a++)
			for (b = a; b > 0; b--)
			{
				struct ae_rf_rect p = ae_rf_rect_of(out[b - 1]), q = ae_rf_rect_of(out[b]);

				if (q.y < p.y || (q.y == p.y && q.x < p.x))
				{
					int swap = out[b];

					out[b] = out[b - 1];
					out[b - 1] = swap;
				}
			}
	}
	return count;
}

int ae_rf_move(int page, unsigned long has, int current, int direction)
{
	int ids[AE_RF_BOXES], count = ae_rf_list(page, has, ids), index, best = -1;
	float best_score = 0.0f;
	struct ae_rf_rect from;
	float fx, fy;

	if (!count)
		return AE_RF_NONE;
	for (index = 0; index < count && ids[index] != current; index++)
		;
	if (index == count)
		return ids[0];
	from = ae_rf_rect_of(current);
	fx = from.x + from.w * 0.5f;
	fy = from.y + from.h * 0.5f;
	for (index = 0; index < count; index++)
	{
		struct ae_rf_rect to = ae_rf_rect_of(ids[index]);
		float cx = to.x + to.w * 0.5f, cy = to.y + to.h * 0.5f, along, across, score;
		int overlap;

		if (ids[index] == current)
			continue;
		if (direction == AE_RF_UP || direction == AE_RF_DOWN)
		{
			along = direction == AE_RF_DOWN ? cy - fy : fy - cy;
			across = cx - fx;
			overlap = to.x < from.x + from.w && to.x + to.w > from.x;
		}
		else
		{
			along = direction == AE_RF_RIGHT ? cx - fx : fx - cx;
			across = cy - fy;
			overlap = to.y < from.y + from.h && to.y + to.h > from.y;
		}
		if (along <= 1.0f)
			continue;
		/* (a box sharing the row or column first, then the nearest) */
		score = along + 2.0f * (across < 0 ? -across : across) + (overlap ? 0.0f : 10000.0f);
		if (best < 0 || score < best_score)
		{
			best = index;
			best_score = score;
		}
	}
	return best < 0 ? current : ids[best];
}

int ae_rf_keep(int page, unsigned long has, int current)
{
	int ids[AE_RF_BOXES], count = ae_rf_list(page, has, ids), index;

	for (index = 0; index < count; index++)
		if (ids[index] == current)
			return current;
	return count ? ids[0] : AE_RF_NONE;
}
