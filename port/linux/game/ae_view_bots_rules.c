/*
AE_VIEW_BOTS_RULES.C

DEV MODE's bot view, its pure rules (ae_view_bots_rules.h).
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ae_view_bots_rules.h"

/* the board's band: 664 wide, its right 16 left of the TEST RUN card (1356 at 1080p) */
#define BAND_WIDTH 664.0f
#define CARD_WIDTH 530.0f
#define CARD_RIGHT_IN 34.0f
#define CARD_CLEAR 16.0f
/* the reticle (46 across) and 40 round it */
#define CROSSHAIR_HALF 63.0f
/* the strip shows the team's newest WHY this long */
#define NEWEST_TICKS (30 * AE_VB_TICKS_PER_SECOND)
/* a squad of this many in a split view drops the strip */
#define SPLIT_STRIP_SQUAD 5
/* the aim focus: within this many degrees, held this long after the aim leaves */
#define AIM_DEGREES 6.0f
#define AIM_HOLD_TICKS AE_VB_TICKS_PER_SECOND
/* a squad's bots within this many metres of each other share one pip */
#define TOGETHER_METRES 30.0f
#define METRES_PER_UNIT 3.048f

/* ---------- names */

char const *ae_vb_source_word(int source, int goal_kind)
{
	static char const *const words[AE_VB_SOURCES] = { "", "VEHICLE", "ZONE", "SQUAD", "MARK", "OBJECTIVE", "REVENGE", "POWER",
		"VANTAGE", "ROUTE", "TEAM", "ROAM" };

	if (source == AE_VB_SOURCE_BASE)
		return goal_kind == AE_VB_GOAL_ENEMY ? "HUNT" : goal_kind == AE_VB_GOAL_ITEM ? "ITEM" :
			goal_kind == AE_VB_GOAL_ROAM ? "ROAM" : "";
	return source > 0 && source < AE_VB_SOURCES ? words[source] : "";
}

char const *ae_vb_source_code(int source)
{
	static char const *const codes[AE_VB_SOURCES] = { "", "VEH", "ZONE", "SQD", "MRK", "OBJ", "REV", "PWR", "VAN", "RTE",
		"TEAM", "BASE" };

	return source > 0 && source < AE_VB_SOURCES ? codes[source] : "";
}

char const *ae_vb_role_name(int role)
{
	static char const *const names[] = { "", "DEFEND OURS", "PUSH THEIRS", "ROVE", "SITE A", "SITE B", "ENTRY", "HILL",
		"APPROACH", "PROTECT", "HUNT", "POWER", "ROAM" };

	return role > 0 && role < (int)(sizeof(names) / sizeof(names[0])) ? names[role] : "";
}

char const *ae_vb_mark_name(int kind)
{
	static char const *const names[] = { "ENEMY", "ITEM", "PLACE", "VEHICLE", "RIDE" };

	return kind >= 0 && kind < (int)(sizeof(names) / sizeof(names[0])) ? names[kind] : "";
}

char const *ae_vb_order_name(int order)
{
	static char const *const names[] = { "NONE", "FOLLOW", "REGROUP", "ATTACK", "DEFEND", "WATCH", "GO" };

	return order >= 0 && order < (int)(sizeof(names) / sizeof(names[0])) ? names[order] : "NONE";
}

int ae_vb_state_pick(int fighting, int stuck, int no_path, int tethered)
{
	return fighting ? AE_VB_STATE_FIGHT : stuck ? AE_VB_STATE_STUCK : no_path ? AE_VB_STATE_NO_PATH :
		tethered ? AE_VB_STATE_TETHER : AE_VB_STATE_NONE;
}

char const *ae_vb_state_word(int state)
{
	static char const *const words[AE_VB_STATES] = { "", "FIGHT", "STUCK", "NO PATH", "TETHER" };

	return state > 0 && state < AE_VB_STATES ? words[state] : "";
}

int ae_vb_fighting(long now, long last_fight_tick)
{
	return now >= last_fight_tick && now - last_fight_tick < AE_VB_TICKS_PER_SECOND;
}

long ae_vb_metres(float world_units)
{
	return world_units > 0.0f ? (long)(world_units * METRES_PER_UNIT + 0.5f) : 0L;
}

void ae_vb_age_text(long ticks, char *text, int size)
{
	long seconds = ticks > 0 ? ticks / AE_VB_TICKS_PER_SECOND : 0;

	if (size <= 0)
		return;
	if (seconds < 60)
		snprintf(text, (size_t)size, "%ld s", seconds);
	else
		snprintf(text, (size_t)size, "%ld m", seconds / 60);
}

/* ---------- geometry */

void ae_vb_geometry_of(int split, struct ae_vb_geometry *g)
{
	memset(g, 0, sizeof(*g));
	g->band = BAND_WIDTH;
	g->bar = 4.0f;
	g->pad_t = split ? 6.0f : 8.0f;
	g->pad_b = split ? 6.0f : 8.0f;
	g->pad_l = 12.0f;
	g->pad_r = 12.0f;
	g->gutter = 24.0f;
	g->col = 306.0f;
	g->header = !split;
	g->head_h = 20.0f;
	g->head_px = 13.0f;
	g->head_right_px = 11.0f;
	g->head_gap = 2.0f;
	g->sq_h = split ? 20.0f : 22.0f;
	g->sq_px = 12.0f;
	g->note_px = split ? 12.0f : 13.0f;
	g->stack_gap = 6.0f;
	g->row_h = split ? 20.0f : 22.0f;
	g->num_px = 14.0f;
	g->name_px = split ? 14.0f : 15.0f;
	g->src_px = 11.0f;
	g->det_px = split ? 14.0f : 15.0f;
	g->state_px = 11.0f;
	g->col_num = 18.0f;
	g->col_name = 24.0f;
	g->col_src = 100.0f;
	g->col_det = 170.0f;
	g->state_w = 52.0f;
	g->strip_sep = split ? 5.0f : 7.0f;
	g->strip_h = split ? 20.0f : 22.0f;
	g->strip_px = split ? 14.0f : 15.0f;
	g->strip_small_px = 11.0f;
}

float ae_vb_board_left(float layout_width, float view_x, float view_width, int half_width_view)
{
	float left;

	if (half_width_view)
		left = view_x + (view_width - BAND_WIDTH) * 0.5f;
	else
	{
		/* (the card's right edge as ae_test_run_card.c places it: 34 in from the 16:9 frame's right on a wider screen, the
		screen's own on a narrower one) */
		float card_right = layout_width > 1920.0f ? (layout_width - 1920.0f) * 0.5f + 1920.0f - CARD_RIGHT_IN :
			layout_width - CARD_RIGHT_IN;

		left = card_right - CARD_WIDTH - CARD_CLEAR - BAND_WIDTH;
	}
	return left < view_x + 8.0f ? view_x + 8.0f : left;
}

float ae_vb_board_top(float view_y, int split)
{
	return view_y + (split ? 48.0f : 64.0f);
}

void ae_vb_crosshair_area(float view_x, float view_y, float view_width, float view_height, float box[4])
{
	float cx = view_x + view_width * 0.5f, cy = view_y + view_height * 0.5f;

	box[0] = cx - CROSSHAIR_HALF;
	box[1] = cy - CROSSHAIR_HALF;
	box[2] = cx + CROSSHAIR_HALF;
	box[3] = cy + CROSSHAIR_HALF;
}

/* ---------- layout */

static float squad_height(struct ae_vb_geometry const *g, int rows)
{
	return g->sq_h + (float)rows * g->row_h;
}

/* the squads into columns, every row shown; the body's height */
static float columns_place(struct ae_vb_geometry const *g, short const *rows, int squads, struct ae_vb_layout *l)
{
	float height[2] = { 0.0f, 0.0f };
	int s;

	for (s = 0; s < squads && s < AE_VB_SQUADS; s++)
	{
		int column = s < 2 ? s : height[1] < height[0] ? 1 : 0;

		if (l->columns == 1)
			column = 0;
		l->place[s].column = (short)column;
		l->place[s].y = height[column] > 0.0f ? height[column] + g->stack_gap : 0.0f;
		l->place[s].shown = rows[s];
		l->place[s].more = 0;
		height[column] = l->place[s].y + squad_height(g, rows[s]);
	}
	return height[0] > height[1] ? height[0] : height[1];
}

/* a column cut to room: its squads drawn while their header and a line fit; the last line drawn says how many rows are not
("+N MORE"), and the squads after it are not drawn */
static float column_cut(struct ae_vb_geometry const *g, short const *rows, int squads, int column, float room,
	struct ae_vb_layout *l)
{
	float y = 0.0f;
	int s, previous = -1, cut = 0;

	for (s = 0; s < squads && s < AE_VB_SQUADS; s++)
	{
		float top;
		int can;

		if (l->place[s].column != column)
			continue;
		if (cut)
		{
			l->place[s].shown = -1;
			l->place[s].more = 0;
			l->place[previous].more = (short)(l->place[previous].more + rows[s]);
			continue;
		}
		top = y > 0.0f ? y + g->stack_gap : 0.0f;
		can = room - top - g->sq_h < 0.0f ? -1 : (int)((room - top - g->sq_h) / g->row_h);
		if (can >= rows[s])
		{
			l->place[s].y = top;
			l->place[s].shown = rows[s];
			l->place[s].more = 0;
			y = top + squad_height(g, rows[s]);
			previous = s;
			continue;
		}
		cut = 1;
		if (can >= 1)
		{
			/* (its last line is "+N MORE") */
			l->place[s].y = top;
			l->place[s].shown = (short)(can - 1);
			l->place[s].more = (short)(rows[s] - (can - 1));
			y = top + squad_height(g, can);
			previous = s;
			continue;
		}
		/* (not even its header and a line: not drawn; the squad above gives its last row to the count) */
		l->place[s].shown = -1;
		l->place[s].more = 0;
		if (previous < 0)
			continue;
		if (l->place[previous].more == 0)
		{
			if (l->place[previous].shown > 0)
			{
				l->place[previous].shown--;
				l->place[previous].more = 1;
			}
		}
		if (l->place[previous].more > 0)
			l->place[previous].more = (short)(l->place[previous].more + rows[s]);
	}
	return y;
}

float ae_vb_layout(struct ae_vb_geometry const *g, int split, float x0, float top, float bottom, short const *rows,
	int squads, int strip_lines, struct ae_vb_layout *l)
{
	float head = g->header ? g->head_h + g->head_gap : 0.0f;
	int s, biggest = 0;

	memset(l, 0, sizeof(*l));
	if (squads > AE_VB_SQUADS)
		squads = AE_VB_SQUADS;
	if (squads < 0)
		squads = 0;
	l->x0 = x0;
	l->y0 = top;
	l->width = g->band;
	l->columns = (short)(squads >= 2 ? 2 : 1);
	l->col_w = l->columns == 2 ? g->col : 2.0f * g->col + g->gutter;
	l->col_x[0] = 0.0f;
	l->col_x[1] = g->col + g->gutter;
	l->body_y = g->pad_t + head;
	for (s = 0; s < squads; s++)
		if (rows[s] > biggest)
			biggest = rows[s];
	l->strip_lines = (short)(strip_lines < 0 ? 0 : strip_lines > 2 ? 2 : strip_lines);
	if (split && biggest >= SPLIT_STRIP_SQUAD)
		l->strip_lines = 0;
	l->body_h = columns_place(g, rows, squads, l);
	l->height = l->body_y + l->body_h + (l->strip_lines ? g->strip_sep + g->strip_h * (float)l->strip_lines : 0.0f) + g->pad_b;
	if (top + l->height > bottom && l->strip_lines)
	{
		l->strip_lines = 0;
		l->height = l->body_y + l->body_h + g->pad_b;
	}
	if (top + l->height > bottom)
	{
		float room = bottom - top - l->body_y - g->pad_b, used0, used1;

		used0 = column_cut(g, rows, squads, 0, room, l);
		used1 = column_cut(g, rows, squads, 1, room, l);
		l->body_h = used0 > used1 ? used0 : used1;
		l->height = l->body_y + l->body_h + g->pad_b;
	}
	l->strip_y = l->body_y + l->body_h;
	return l->height;
}

int ae_vb_strip_pick(int focus, int focus_has_why, int has_newest, long newest_age_ticks)
{
	if (focus && focus_has_why)
		return AE_VB_STRIP_FOCUS;
	if (has_newest && newest_age_ticks >= 0 && newest_age_ticks < NEWEST_TICKS)
		return AE_VB_STRIP_NEWEST;
	return AE_VB_STRIP_NONE;
}

/* ---------- tags */

float ae_vb_box_gap(struct ae_vb_box const *a, struct ae_vb_box const *b)
{
	float dx = a->x1 < b->x0 ? b->x0 - a->x1 : b->x1 < a->x0 ? a->x0 - b->x1 : 0.0f;
	float dy = a->y1 < b->y0 ? b->y0 - a->y1 : b->y1 < a->y0 ? a->y0 - b->y1 : 0.0f;

	return dx > dy ? dx : dy;
}

/* a place is free: inside the view, clear of every block */
static int box_free(struct ae_vb_box const *box, struct ae_vb_box const *blocks, int count, struct ae_vb_box const *view,
	float clear)
{
	int index;

	if (view && (box->x0 < view->x0 || box->y0 < view->y0 || box->x1 > view->x1 || box->y1 > view->y1))
		return 0;
	for (index = 0; index < count; index++)
		if (ae_vb_box_gap(box, &blocks[index]) < clear)
			return 0;
	return 1;
}

static void above(struct ae_vb_box const *label, float width, float height, float clear, struct ae_vb_box *box)
{
	float centre = (label->x0 + label->x1) * 0.5f;

	box->x0 = centre - width * 0.5f;
	box->x1 = centre + width * 0.5f;
	box->y1 = label->y0 - clear;
	box->y0 = box->y1 - height;
}

static void after(struct ae_vb_box const *label, float width, float height, float clear, struct ae_vb_box *box)
{
	float middle = (label->y0 + label->y1) * 0.5f;

	box->x0 = label->x1 + clear;
	box->x1 = box->x0 + width;
	box->y0 = middle - height * 0.5f;
	box->y1 = middle + height * 0.5f;
}

int ae_vb_tag_place(struct ae_vb_box const *label, float width, float number_width, float height,
	struct ae_vb_box const *blocks, int count, struct ae_vb_box const *view, float clear, struct ae_vb_box *box)
{
	float circle = number_width > height ? number_width : height;

	above(label, width, height, clear, box);
	if (box_free(box, blocks, count, view, clear))
		return AE_VB_TAG_ABOVE;
	after(label, width, height, clear, box);
	if (box_free(box, blocks, count, view, clear))
		return AE_VB_TAG_AFTER;
	above(label, circle, height, clear, box);
	if (box_free(box, blocks, count, view, clear))
		return AE_VB_TAG_NUMBER;
	after(label, circle, height, clear, box);
	if (box_free(box, blocks, count, view, clear))
		return AE_VB_TAG_NUMBER_AFTER;
	above(label, circle, height, clear, box);
	return AE_VB_TAG_FAINT;
}

float ae_vb_tag_alpha(struct ae_vb_box const *tag, float const crosshair[4], int faint)
{
	if (faint)
		return AE_VB_FADE;
	if (tag->x0 < crosshair[2] && crosshair[0] < tag->x1 && tag->y0 < crosshair[3] && crosshair[1] < tag->y1)
		return AE_VB_FADE;
	return 1.0f;
}

/* ---------- pips */

void ae_vb_off_direction(float x, float y, float z, float *dx, float *dy)
{
	float ax, ay, larger;

	if (z < 0.0f)
	{
		/* (in front: the projection's direction, which is x over depth and y over depth, y turned down) */
		ax = x / -z;
		ay = -y / -z;
	}
	else
	{
		/* (behind: the bottom edge, at its side) */
		float lateral = fabsf(x) + z + 0.001f;

		ax = x / lateral;
		ay = 1.0f;
	}
	larger = fabsf(ax) > fabsf(ay) ? fabsf(ax) : fabsf(ay);
	if (larger <= 0.0f)
	{
		ax = 0.0f;
		ay = 1.0f;
		larger = 1.0f;
	}
	*dx = ax / larger;
	*dy = ay / larger;
}

int ae_vb_pip_anchor(float dx, float dy, struct ae_vb_box const *view, float inset, float *x, float *y)
{
	float cx = (view->x0 + view->x1) * 0.5f, cy = (view->y0 + view->y1) * 0.5f;
	float half_w = (view->x1 - view->x0) * 0.5f - inset, half_h = (view->y1 - view->y0) * 0.5f - inset;
	float tx, ty, t;

	if (half_w < 1.0f)
		half_w = 1.0f;
	if (half_h < 1.0f)
		half_h = 1.0f;
	/* (the ray from the centre to the inset rectangle's edge) */
	tx = dx != 0.0f ? half_w / fabsf(dx) : 1e30f;
	ty = dy != 0.0f ? half_h / fabsf(dy) : 1e30f;
	t = tx < ty ? tx : ty;
	*x = cx + dx * t;
	*y = cy + dy * t;
	if (tx < ty)
		return dx < 0.0f ? AE_VB_SIDE_LEFT : AE_VB_SIDE_RIGHT;
	return dy < 0.0f ? AE_VB_SIDE_TOP : AE_VB_SIDE_BOTTOM;
}

static void pip_box(int side, float x, float y, float width, float height, float triangle, struct ae_vb_box *box)
{
	switch (side)
	{
	case AE_VB_SIDE_LEFT:
		box->x0 = x;
		box->x1 = x + triangle + width;
		box->y0 = y - height * 0.5f;
		box->y1 = y + height * 0.5f;
		break;
	case AE_VB_SIDE_RIGHT:
		box->x1 = x;
		box->x0 = x - triangle - width;
		box->y0 = y - height * 0.5f;
		box->y1 = y + height * 0.5f;
		break;
	case AE_VB_SIDE_TOP:
		box->x0 = x - width * 0.5f;
		box->x1 = x + width * 0.5f;
		box->y0 = y;
		box->y1 = y + triangle + height;
		break;
	default:
		box->x0 = x - width * 0.5f;
		box->x1 = x + width * 0.5f;
		box->y1 = y;
		box->y0 = y - triangle - height;
		break;
	}
}

int ae_vb_pip_place(int side, float x, float y, float width, float height, float triangle,
	struct ae_vb_box const *blocks, int count, struct ae_vb_box const *view, float clear, struct ae_vb_box *box)
{
	int vertical = side == AE_VB_SIDE_LEFT || side == AE_VB_SIDE_RIGHT;
	float edge = vertical ? view->y1 - view->y0 : view->x1 - view->x0;
	float step;
	int sign;

	for (step = 0.0f; step <= edge * 0.5f; step += 8.0f)
	{
		for (sign = 1; sign >= -1; sign -= 2)
		{
			float ox = vertical ? 0.0f : step * (float)sign, oy = vertical ? step * (float)sign : 0.0f;

			pip_box(side, x + ox, y + oy, width, height, triangle, box);
			if (box_free(box, blocks, count, view, clear))
				return 1;
			if (step == 0.0f)
				break;
		}
	}
	return 0;
}

int ae_vb_together(float const (*points)[3], int count)
{
	float most = TOGETHER_METRES / METRES_PER_UNIT;
	int a, b;

	for (a = 0; a < count; a++)
		for (b = a + 1; b < count; b++)
		{
			float dx = points[a][0] - points[b][0], dy = points[a][1] - points[b][1], dz = points[a][2] - points[b][2];

			if (dx * dx + dy * dy + dz * dz > most * most)
				return 0;
		}
	return 1;
}

/* ---------- aim */

int ae_vb_aim_pick(float const *degrees, int const *eligible, int count)
{
	int index, best = -1;

	for (index = 0; index < count; index++)
		if (eligible[index] && degrees[index] <= AIM_DEGREES && (best < 0 || degrees[index] < degrees[best]))
			best = index;
	return best;
}

int ae_vb_aim_hold(int held, long held_tick, int picked, long now)
{
	if (picked >= 0)
		return picked;
	if (held >= 0 && now >= held_tick && now - held_tick < AIM_HOLD_TICKS)
		return held;
	return -1;
}

/* ---------- text */

/* the length of text's first n bytes cut back to a whole UTF-8 character */
static int whole(char const *text, int n)
{
	while (n > 0 && ((unsigned char)text[n] & 0xC0) == 0x80)
		n--;
	return n;
}

void ae_vb_fit(char const *text, float room, ae_vb_measure measure, void *context, char *out, int size)
{
	int length;

	if (size <= 0)
		return;
	if (!text)
		text = "";
	snprintf(out, (size_t)size, "%s", text);
	if (measure(out, context) <= room)
		return;
	length = whole(out, (int)strlen(out));
	while (length > 0)
	{
		char buffer[256];

		length = whole(text, length - 1);
		/* (no space or dot left hanging before the ellipsis) */
		while (length > 0 && (text[length - 1] == ' ' || (length >= 2 && (unsigned char)text[length - 2] == 0xC2 &&
			(unsigned char)text[length - 1] == 0xB7)))
			length -= text[length - 1] == ' ' ? 1 : 2;
		snprintf(buffer, sizeof(buffer), "%.*s\xE2\x80\xA6", length, text);
		if (measure(buffer, context) <= room || length == 0)
		{
			snprintf(out, (size_t)size, "%s", buffer);
			return;
		}
	}
	snprintf(out, (size_t)size, "\xE2\x80\xA6");
}

int ae_vb_wrap(char const *text, float room, int lines, ae_vb_measure measure, void *context, char out[][AE_VB_LINE])
{
	int used = 0;

	if (!text)
		return 0;
	while (*text == ' ')
		text++;
	while (*text && used < lines)
	{
		int length = (int)strlen(text), cut = length;
		char buffer[AE_VB_LINE];

		if (used == lines - 1)
		{
			ae_vb_fit(text, room, measure, context, out[used], AE_VB_LINE);
			return used + 1;
		}
		/* (the longest run of words that fits; a first word too long for the line is cut at the last line) */
		for (;;)
		{
			snprintf(buffer, sizeof(buffer), "%.*s", cut, text);
			if (measure(buffer, context) <= room)
				break;
			while (cut > 0 && text[cut - 1] != ' ')
				cut--;
			while (cut > 0 && text[cut - 1] == ' ')
				cut--;
			if (cut <= 0)
				break;
		}
		if (cut <= 0)
		{
			ae_vb_fit(text, room, measure, context, out[used], AE_VB_LINE);
			return used + 1;
		}
		snprintf(out[used], AE_VB_LINE, "%.*s", cut, text);
		used++;
		text += cut;
		while (*text == ' ')
			text++;
	}
	return used;
}
