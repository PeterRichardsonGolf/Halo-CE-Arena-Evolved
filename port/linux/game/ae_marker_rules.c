/*
AE_MARKER_RULES.C

Pure rules for the markers and the feed (ae_marker_rules.h). No engine includes.
*/

#include "ae_marker_rules.h"
#include "ae_wheel_rules.h"
#include "ae_mark_rules.h"
#include "ae_waypoint_icons.h"

#include <math.h>
#include <wchar.h>

/* ---------- the marker's size */

/* the marks' ellipse, of the view's width and height (840 across and 340 down at 1920 x 1080) */
#define ELLIPSE_X 0.4375f
#define ELLIPSE_Y (340.0f / 1080.0f)

#define GLYPH_BASE 36.0f
#define GLYPH_FLOOR 20.0f

float ae_marker_e(float dx, float dy, float view_width, float view_height)
{
	float across, down;

	if (!(view_width > 0.0f) || !(view_height > 0.0f))
		return 1.0f;
	across = dx / (ELLIPSE_X * view_width);
	down = dy / (ELLIPSE_Y * view_height);
	return sqrtf(across * across + down * down);
}

float ae_marker_f(float e)
{
	if (!(e > 0.30f))
		return e != e ? 1.0f : 0.72f;
	if (e < 0.50f)
		return 0.72f + (e - 0.30f) / 0.20f * 0.28f;
	if (e <= 0.70f)
		return 1.0f;
	if (e < 1.00f)
		return 1.0f + (e - 0.70f) / 0.30f * 0.20f;
	return 1.20f;
}

float ae_marker_size(float waypoint_scale, float e)
{
	float size;

	if (!(waypoint_scale > 0.0f))
		return GLYPH_FLOOR;
	size = GLYPH_BASE * waypoint_scale * ae_marker_f(e);
	/* (whole pixels, a half rounding down; the small allowance keeps a half that arithmetic left a hair over where it was) */
	size = ceilf(size - 0.5f - 0.0001f);
	return size < GLYPH_FLOOR ? GLYPH_FLOOR : size;
}

float ae_marker_ease(float current, float target, long dt_ms)
{
	if (dt_ms <= 0)
		return current;
	/* (98% of the way in 120 ms) */
	return target + (current - target) * expf(-4.0f * (float)dt_ms / 120.0f);
}

float ae_marker_repeat_scale(int repeats)
{
	float scale = 1.0f;
	int step;

	if (repeats > 2)
		repeats = 2;
	for (step = 0; step < repeats; step++)
		scale *= 1.12f;
	return scale;
}

#define FLASH_RING_MS 240
#define FLASH_LIFT_MS 160
#define FLASH_RADIUS 38.0f
#define FLASH_LIFT 1.15f

int ae_marker_flash(long since_ms, float *radius, float *alpha, float *lift)
{
	float t, p;

	if (since_ms < 0 || since_ms >= FLASH_RING_MS || !radius || !alpha || !lift)
		return 0;
	t = (float)since_ms / (float)FLASH_RING_MS;
	*radius = FLASH_RADIUS * (1.0f - (1.0f - t) * (1.0f - t));
	*alpha = 1.0f - t;
	if (since_ms >= FLASH_LIFT_MS)
		*lift = 1.0f;
	else
	{
		p = (float)since_ms / (float)FLASH_LIFT_MS;
		*lift = FLASH_LIFT - (FLASH_LIFT - 1.0f) * (1.0f - (1.0f - p) * (1.0f - p));
	}
	return 1;
}

void ae_ok_names_split(int count, int *shown, int *more)
{
	if (count < 0)
		count = 0;
	*shown = count < AE_OK_NAMES_MOST ? count : AE_OK_NAMES_MOST;
	*more = count - *shown;
}

/* ---------- the style */

void ae_marker_ink_get(int ae, float const ce_line[3], struct ae_marker_ink *ink)
{
	static float const ae_line[3] = { 178.0f / 255.0f, 224.0f / 255.0f, 1.0f };
	static float const ae_white[3] = { 229.0f / 255.0f, 240.0f / 255.0f, 1.0f };
	static float const ae_blue[3] = { 117.0f / 255.0f, 186.0f / 255.0f, 1.0f };
	static float const ae_red[3] = { 1.0f, 77.0f / 255.0f, 77.0f / 255.0f };
	static float const ae_dark[3] = { 6.0f / 255.0f, 10.0f / 255.0f, 18.0f / 255.0f };
	static float const ae_flash[3] = { 245.0f / 255.0f, 250.0f / 255.0f, 1.0f };
	static float const ce_white[3] = { 0.898f, 0.941f, 1.0f };
	static float const ce_red[3] = { 1.0f, 0.298f, 0.251f };
	static float const ce_dark[3] = { 0.0f, 0.0f, 0.0f };
	int i;

	for (i = 0; i < 3; i++)
	{
		float line = ce_line ? ce_line[i] : ae_line[i];

		ink->line[i] = ae ? ae_line[i] : line;
		ink->white[i] = ae ? ae_white[i] : ce_white[i];
		ink->blue[i] = ae ? ae_blue[i] : line;
		ink->red[i] = ae ? ae_red[i] : ce_red[i];
		ink->dark[i] = ae ? ae_dark[i] : ce_dark[i];
		ink->flash[i] = ae ? ae_flash[i] : ce_white[i];
	}
}

int ae_marker_name_row_shown(int waypoint_style)
{
	return waypoint_style != AE_WAYPOINT_ICONS;
}

/* ---------- the lead line and the block of rows */

#define LEAD_OFF 6.0f
#define LEAD_ACROSS 16.0f
#define LEAD_RISE 22.0f
#define LEAD_RISE_LONG 36.0f
#define LEAD_FOOT 9.0f
#define LEAD_PAST 4.0f
#define LEAD_EDGE 8.0f

/* the lead of this direction and rise */
static void lead_place(struct ae_marker_lead_in const *in, int dir, float rise, struct ae_marker_lead_out *out)
{
	float s = in->s, half = in->glyph_size / 2.0f;
	float corner_x = in->glyph_x + (float)dir * half, corner_y = in->glyph_y - half;
	float length = in->block_width + LEAD_PAST * s;

	out->dir = dir;
	out->start_x = corner_x + (float)dir * LEAD_OFF * s;
	out->start_y = corner_y - LEAD_OFF * s;
	out->elbow_x = out->start_x + (float)dir * LEAD_ACROSS * s;
	out->elbow_y = out->start_y - rise * s;
	out->shelf_end_x = out->elbow_x + (float)dir * length;
	out->rows_bottom = out->elbow_y - LEAD_FOOT * s;
	out->box[0] = dir > 0 ? out->elbow_x : out->shelf_end_x;
	out->box[2] = dir > 0 ? out->shelf_end_x : out->elbow_x;
	out->box[1] = out->rows_bottom - in->block_height;
	out->box[3] = out->elbow_y;
}

/* whether the box meets the circle */
static int box_meets_zone(float const box[4], float cx, float cy, float radius)
{
	float dx = cx < box[0] ? box[0] - cx : cx > box[2] ? cx - box[2] : 0.0f;
	float dy = cy < box[1] ? box[1] - cy : cy > box[3] ? cy - box[3] : 0.0f;

	return hypotf(dx, dy) < radius;
}

void ae_marker_lead(struct ae_marker_lead_in const *in, struct ae_marker_lead_out *out)
{
	float s = in->s, cx = in->view_width / 2.0f, cy = in->view_height / 2.0f;
	int dir = in->glyph_x >= cx ? 1 : -1;

	out->raised = 0;
	out->flipped = 0;
	lead_place(in, dir, LEAD_RISE, out);
	/* (at the view's edge: toward the middle) */
	if ((dir > 0 && out->box[2] > in->view_width - LEAD_EDGE * s) || (dir < 0 && out->box[0] < LEAD_EDGE * s))
	{
		struct ae_marker_lead_out other;

		lead_place(in, -dir, LEAD_RISE, &other);
		if (other.box[0] >= LEAD_EDGE * s && other.box[2] <= in->view_width - LEAD_EDGE * s)
		{
			*out = other;
			out->flipped = 1;
			dir = -dir;
		}
	}
	if (box_meets_zone(out->box, cx, cy, AE_MARKER_ZONE * s))
	{
		lead_place(in, dir, LEAD_RISE_LONG, out);
		out->raised = 1;
		out->flipped = dir != (in->glyph_x >= cx ? 1 : -1);
	}
}

/* ---------- the feed */

int ae_feed_merge(struct ae_feed_merge *merge, int caller, int call, long now)
{
	if (!merge)
		return 0;
	if (merge->count > 0 && merge->caller == caller && merge->call == call && now >= merge->at &&
		now - merge->at <= AE_FEED_MERGE_TICKS)
	{
		if (merge->count < AE_FEED_MERGE_MOST)
			merge->count++;
		merge->at = now;
		return merge->count;
	}
	merge->caller = caller;
	merge->call = call;
	merge->at = now;
	merge->count = 1;
	return 0;
}

/* a bounded writer: text goes on at *length while it fits (one left for the end) */
static void put(wchar_t *out, int size, int *length, wchar_t const *text)
{
	for (; text && *text && *length < size - 1; text++)
		out[(*length)++] = *text;
	out[*length] = 0;
}

/* the same without the colour runs: a name is anyone's text, and only the line's own runs may colour it */
static void put_plain(wchar_t *out, int size, int *length, wchar_t const *text)
{
	for (; text && *text && *length < size - 1; text++)
	{
		if (*text != AE_RUN_WHITE && *text != AE_RUN_BLUE)
			out[(*length)++] = *text;
	}
	out[*length] = 0;
}

static void put_char(wchar_t *out, int size, int *length, wchar_t c)
{
	if (*length < size - 1)
		out[(*length)++] = c;
	out[*length] = 0;
}

static void put_number(wchar_t *out, int size, int *length, int number, int digits)
{
	int divisor = 1, count;

	for (count = 1; count < digits; count++)
		divisor *= 10;
	for (; divisor > 0; divisor /= 10)
		put_char(out, size, length, (wchar_t)(L'0' + (number / divisor) % 10));
}

int ae_feed_words(wchar_t *out, int size, int entry, int kind, int dead, wchar_t const *noun, int seconds)
{
	wchar_t const *words = NULL;
	int length = 0;

	if (!out || size < 1)
		return 0;
	out[0] = 0;
	if (entry < 0 || entry >= AE_WHEEL_COUNT)
		entry = AE_WHEEL_SMART;
	switch (entry)
	{
	case AE_WHEEL_ENEMY: words = L"Enemy spotted"; break;
	case AE_WHEEL_WEAPON: words = L"Need weapon"; break;
	case AE_WHEEL_RIDE: words = L"Need a ride"; break;
	case AE_WHEEL_ATTACK: words = L"Attacking here"; break;
	case AE_WHEEL_DEFEND: words = L"Defending here"; break;
	case AE_WHEEL_WATCH: words = L"Watching here"; break;
	case AE_WHEEL_GOING: words = dead ? L"Go here" : L"Going here"; break;
	default:
		if (dead)
			words = L"Help me";
		else if (kind == AE_MARK_KIND_ENEMY)
			words = L"Enemy spotted";
		else if (!noun || !noun[0])
			words = L"Go here";
		break;
	}
	if (words)
	{
		/* (a call's words whole or not at all) */
		if (wcslen(words) >= (size_t)size)
			return 0;
		put(out, size, &length, words);
		return length;
	}
	put(out, size, &length, noun);
	if (seconds >= 0)
	{
		if (seconds > 5999)
			seconds = 5999;
		put_char(out, size, &length, L' ');
		put_number(out, size, &length, seconds / 60, seconds >= 600 ? 2 : 1);
		put_char(out, size, &length, L':');
		put_number(out, size, &length, seconds % 60, 2);
	}
	else
		put(out, size, &length, L" here");
	return length;
}

int ae_feed_line(wchar_t *out, int size, wchar_t const *caller, wchar_t const *words, int count)
{
	int length = 0;

	if (!out || size < 4)
		return 0;
	out[0] = 0;
	put_char(out, size, &length, AE_RUN_BLUE);
	put_plain(out, size, &length, caller);
	put_char(out, size, &length, L':');
	put_char(out, size, &length, AE_RUN_WHITE);
	put_char(out, size, &length, L' ');
	put_plain(out, size, &length, words);
	if (count > 1)
	{
		if (count > AE_FEED_MERGE_MOST)
			count = AE_FEED_MERGE_MOST;
		put_char(out, size, &length, L' ');
		put_char(out, size, &length, AE_RUN_BLUE);
		put_char(out, size, &length, L'x');
		put_number(out, size, &length, count, count >= 10 ? 2 : 1);
	}
	return length;
}

void ae_feed_strip_runs(wchar_t *text)
{
	wchar_t *to = text;

	if (!text)
		return;
	for (; *text; text++)
	{
		if (*text != AE_RUN_WHITE && *text != AE_RUN_BLUE && *text != AE_RUN_RGB && *text != AE_RUN_NOWRAP &&
			!AE_RUN_IS_BYTE(*text))
			*to++ = *text;
	}
	*to = 0;
}

void ae_feed_noun(wchar_t *out, int size, wchar_t const *name)
{
	int length = 0, slash = 0, index;

	if (!out || size < 1)
		return;
	out[0] = 0;
	for (index = 0; name && name[index]; index++)
		slash = slash || name[index] == L'/';
	for (index = 0; name && name[index] && length < size - 1; index++)
	{
		wchar_t c = name[index];

		if (!slash)
		{
			if (index == 0 && c >= L'a' && c <= L'z')
				c = (wchar_t)(c - L'a' + L'A');
			else if (index > 0 && c >= L'A' && c <= L'Z')
				c = (wchar_t)(c - L'A' + L'a');
		}
		out[length++] = c;
	}
	out[length] = 0;
}

/* ---------- the message slots */

short ae_message_slot_choice(struct ae_message_slot const *slots, short count, int new_is_feed)
{
	short index, oldest_feed = AE_SLOT_NONE, oldest_kill = AE_SLOT_NONE;

	if (!slots || count <= 0)
		return AE_SLOT_NONE;
	for (index = 0; index < count; index++)
	{
		if (!slots[index].valid)
			return index;
		if (slots[index].is_feed)
		{
			if (oldest_feed == AE_SLOT_NONE || slots[index].time < slots[oldest_feed].time)
				oldest_feed = index;
		}
		else if (oldest_kill == AE_SLOT_NONE || slots[index].time < slots[oldest_kill].time)
			oldest_kill = index;
	}
	if (oldest_feed != AE_SLOT_NONE)
		return oldest_feed;
	return new_is_feed ? AE_SLOT_NONE : oldest_kill;
}

/* ---------- the open wheel and the text rows */

static int box_ok(float const box[4])
{
	return box && __builtin_isfinite(box[0]) && __builtin_isfinite(box[1]) && __builtin_isfinite(box[2]) && __builtin_isfinite(box[3]);
}

int ae_row_hidden_by_wheel(float const row_box[4], float centre_x, float centre_y, float radius, float const word_box[4],
	float limit)
{
	if (!box_ok(row_box) || row_box[2] < row_box[0] || row_box[3] < row_box[1])
		return 0;
	if (!(limit > 0.0f))
		limit = 0.0f;
	if (__builtin_isfinite(centre_x) && __builtin_isfinite(centre_y) && radius > 0.0f)
	{
		float dx = centre_x < row_box[0] ? row_box[0] - centre_x : centre_x > row_box[2] ? centre_x - row_box[2] : 0.0f;
		float dy = centre_y < row_box[1] ? row_box[1] - centre_y : centre_y > row_box[3] ? centre_y - row_box[3] : 0.0f;

		if (hypotf(dx, dy) < radius + limit)
			return 1;
	}
	if (box_ok(word_box) && word_box[2] > word_box[0] && word_box[3] > word_box[1] &&
		row_box[0] - limit < word_box[2] && word_box[0] < row_box[2] + limit &&
		row_box[1] - limit < word_box[3] && word_box[1] < row_box[3] + limit)
	{
		return 1;
	}
	return 0;
}
