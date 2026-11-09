/*
AE_MEDAL_DRAW_RULES.C

The medals shown on screen as pure rules (ae_medal_draw_rules.h).
*/

#include "ae_medal_draw_rules.h"

#include <math.h>
#include <string.h>
#include <wchar.h>

int ae_medal_show_mode(int local_players, int has_picture)
{
	if (local_players > 1)
		return AE_MEDAL_SHOW_FEED;
	return has_picture ? AE_MEDAL_SHOW_PICTURE : AE_MEDAL_SHOW_TEXT;
}

void ae_medal_holder_reset(struct ae_medal_holder *holder)
{
	memset(holder, 0, sizeof(*holder));
}

/* the oldest medal gone from the front */
static void drop_first(struct ae_medal_holder *holder)
{
	if (holder->count < 1)
		return;
	if (holder->shown[0].counted)
		holder->active--;
	memmove(&holder->shown[0], &holder->shown[1], (size_t)(holder->count - 1) * sizeof(holder->shown[0]));
	holder->count--;
}

void ae_medal_holder_step(struct ae_medal_holder *holder, long now)
{
	int index = 0;

	if (now < holder->last)
	{
		ae_medal_holder_reset(holder);
		holder->last = now;
		return;
	}
	holder->last = now;
	while (index < holder->count)
	{
		struct ae_medal_shown *shown = &holder->shown[index];
		long age = now - shown->born;

		/* (a medal stops counting when its fade starts) */
		if (shown->counted && age >= AE_MEDAL_MOVE_TICKS + AE_MEDAL_HOLD_TICKS)
		{
			shown->counted = 0;
			holder->active--;
		}
		if (age >= AE_MEDAL_LIFE_TICKS)
		{
			memmove(shown, shown + 1, (size_t)(holder->count - index - 1) * sizeof(*shown));
			holder->count--;
		}
		else
			index++;
	}
	if (holder->active <= 0)
	{
		holder->active = 0;
	}
}

int ae_medal_holder_add(struct ae_medal_holder *holder, int medal, long now)
{
	struct ae_medal_shown *shown;
	int slot;

	ae_medal_holder_step(holder, now);
	if (holder->count >= AE_MEDAL_MOST)
		drop_first(holder);
	/* (the lowest place none is in: a place stays taken till its medal is gone, and the rows stay clear of the motion sensor) */
	for (slot = 0; slot < AE_MEDAL_MOST; slot++)
	{
		int index, taken = 0;

		for (index = 0; index < holder->count; index++)
			taken = taken || holder->shown[index].slot == slot;
		if (!taken)
			break;
	}
	shown = &holder->shown[holder->count++];
	shown->medal = medal;
	shown->born = now;
	shown->slot = slot;
	shown->counted = 1;
	holder->active++;
	return slot;
}

float ae_medal_quint_out(float t)
{
	float rest;

	if (t < 0.0f)
		t = 0.0f;
	if (t > 1.0f)
		t = 1.0f;
	rest = 1.0f - t;
	return 1.0f - rest * rest * rest * rest * rest;
}

float ae_medal_shrink(int count)
{
	return count > AE_MEDAL_SHRINK_ABOVE ? AE_MEDAL_SHRUNK_PICTURE / AE_MEDAL_PICTURE : 1.0f;
}

int ae_medal_pose(long age, int slot, int text, float *x, float *y, float *alpha)
{
	return ae_medal_pose_shrunk(age, slot, text, 1.0f, x, y, alpha);
}

int ae_medal_pose_shrunk(long age, int slot, int text, float shrink, float *x, float *y, float *alpha)
{
	float place_x, place_y, in, fade_start = (float)(AE_MEDAL_MOVE_TICKS + AE_MEDAL_HOLD_TICKS);
	float shown_alpha;

	if (age < 0 || age >= AE_MEDAL_LIFE_TICKS)
		return 0;
	if (slot < 0)
		slot = 0;
	if (text)
	{
		place_x = 0.0f;
		place_y = (float)(slot % AE_MEDAL_TEXT_ROWS) * AE_MEDAL_TEXT_PITCH;
	}
	else
	{
		place_x = (float)(slot % AE_MEDAL_COLUMNS) * AE_MEDAL_PITCH * shrink;
		place_y = (float)((slot / AE_MEDAL_COLUMNS) % (AE_MEDAL_MOST / AE_MEDAL_COLUMNS)) * AE_MEDAL_ROW_PITCH * shrink;
	}
	in = ae_medal_quint_out((float)age / (float)AE_MEDAL_MOVE_TICKS);
	shown_alpha = in;
	if ((float)age >= fade_start)
		shown_alpha = 1.0f - ae_medal_quint_out(((float)age - fade_start) / (float)AE_MEDAL_FADE_TICKS);
	*x = AE_MEDAL_HOLDER_X + AE_MEDAL_START_X + (place_x - AE_MEDAL_START_X) * in;
	*y = AE_MEDAL_HOLDER_Y + place_y;
	*alpha = shown_alpha;
	return 1;
}

int ae_medal_caption_lines(wchar_t const *name, ae_medal_measure measure, float px, float limit, wchar_t *first, wchar_t *second,
	int size)
{
	int length, index, best = -1;
	float best_width = 0.0f;

	if (!first || !second || size < 1)
		return 0;
	first[0] = second[0] = 0;
	if (!name || !name[0])
		return 0;
	length = (int)wcslen(name);
	if (length >= size)
	{
		/* (no room for the name: the part that fits, one line) */
		wmemcpy(first, name, (size_t)(size - 1));
		first[size - 1] = 0;
		return 1;
	}
	wmemcpy(first, name, (size_t)length + 1);
	if (!measure || measure(name, px) <= limit)
		return 1;
	/* (the fixed entry: no space in it to break at) */
	if (!wcscmp(name, L"UNFRIGGENBELIEVABLE"))
	{
		wcsncpy(first, L"UNFRIGGEN", (size_t)size);
		first[size - 1] = 0;
		wcsncpy(second, L"BELIEVABLE", (size_t)size);
		second[size - 1] = 0;
		return 2;
	}
	for (index = 1; index < length - 1; index++)
	{
		wchar_t head[128], tail[128];
		float head_width, tail_width, widest;

		if (name[index] != L' ' || index >= 128 || length - index - 1 >= 128)
			continue;
		wmemcpy(head, name, (size_t)index);
		head[index] = 0;
		wcscpy(tail, name + index + 1);
		head_width = measure(head, px);
		tail_width = measure(tail, px);
		widest = head_width > tail_width ? head_width : tail_width;
		if (best < 0 || widest < best_width)
		{
			best = index;
			best_width = widest;
		}
	}
	if (best < 0)
		return 1;
	first[best] = 0;
	wcscpy(second, name + best + 1);
	return 2;
}

float ae_medal_caption_pixels(float scale)
{
	float pixels = AE_MEDAL_CAPTION_SIZE * scale;

	return pixels < 16.0f ? 16.0f : pixels;
}

float ae_medal_caption_pitch(float scale)
{
	return ae_medal_caption_pixels(scale) * AE_MEDAL_CAPTION_LINE_PITCH / AE_MEDAL_CAPTION_SIZE;
}

float ae_medal_caption_fit_pixels(wchar_t const *first, wchar_t const *second, ae_medal_measure measure, float px, float limit)
{
	float fitted = px;
	int guard;

	if (!measure || !first || !(limit > 0.0f))
		return px;
	for (guard = 0; guard < 32 && fitted > AE_MEDAL_CAPTION_MIN; guard++)
	{
		float widest = measure(first, fitted), other = second && second[0] ? measure(second, fitted) : 0.0f;

		if (other > widest)
			widest = other;
		if (widest <= limit)
			break;
		/* (a whole pixel under what the width says, so the text clears the limit; one step at least) */
		{
			float next = (float)floor(fitted * limit / widest);

			if (next >= fitted)
				next = fitted - 1.0f;
			fitted = next < AE_MEDAL_CAPTION_MIN ? AE_MEDAL_CAPTION_MIN : next;
		}
	}
	return fitted > px ? px : fitted;
}

void ae_medal_caption_color(unsigned char const *rgba, unsigned int width, unsigned int height, float color[3])
{
	unsigned long totals[12][4];
	unsigned int x, y;
	int best = -1, hue;

	memset(totals, 0, sizeof(totals));
	color[0] = color[1] = color[2] = 1.0f;
	if (!rgba || !width || !height)
		return;
	for (y = 0; y < height; y++)
	{
		for (x = 0; x < width; x++)
		{
			unsigned char const *pixel = rgba + ((size_t)y * width + x) * 4;
			int r = pixel[0], g = pixel[1], b = pixel[2];
			int most = r > g ? (r > b ? r : b) : (g > b ? g : b);
			int least = r < g ? (r < b ? r : b) : (g < b ? g : b);
			int range = most - least;
			float h;

			/* (a coloured part: not its clear corners, its greys or its dark rim) */
			if (pixel[3] < 128 || most < 90 || range * 10 < most * 3)
				continue;
			h = most == r ? (float)(g - b) / (float)range : most == g ? 2.0f + (float)(b - r) / (float)range :
				4.0f + (float)(r - g) / (float)range;
			if (h < 0.0f)
				h += 6.0f;
			hue = (int)(h * 2.0f) % 12;
			totals[hue][0] += (unsigned long)r;
			totals[hue][1] += (unsigned long)g;
			totals[hue][2] += (unsigned long)b;
			totals[hue][3]++;
		}
	}
	for (hue = 0; hue < 12; hue++)
	{
		if (totals[hue][3] && (best < 0 || totals[hue][3] > totals[best][3]))
			best = hue;
	}
	if (best < 0)
		return;
	for (x = 0; x < 3; x++)
		color[x] = (float)totals[best][x] / (float)totals[best][3] / 255.0f;
	/* (on any ground, behind its outline: toward white until it is light enough to read) */
	for (y = 0; y < 16; y++)
	{
		float light = 0.299f * color[0] + 0.587f * color[1] + 0.114f * color[2];

		if (light >= AE_MEDAL_CAPTION_LIGHT)
			break;
		for (x = 0; x < 3; x++)
			color[x] += (1.0f - color[x]) * 0.15f;
	}
}

float ae_medal_text_pixels(float scale)
{
	float pixels = AE_MEDAL_TEXT_SIZE * scale;

	return pixels < 16.0f ? 16.0f : pixels;
}
