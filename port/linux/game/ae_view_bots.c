/*
AE_VIEW_BOTS.C

(0.7.9) DEV MODE's bot view (ae_view_bots.h). The numbers are the spec's at 1080p (section 12, ae_view_bots_rules.h); the
colours are ae_style.h's tokens and the TEST RUN card's (ice, reason, pill, rec). Each view's pass in the HUD keeps its
camera and the names over heads it drew; once a frame, at main.c's frame render, each view with a pass this frame gets
its board, its tags, the focused bot's goal line and its pips, in layout units of the whole frame.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "math/real_math.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "networking/network_game_globals.h"
#include "objects/objects.h"
#include "render/render.h"
#include "units/units.h"

#include "ae_bots.h"
#include "ae_dev_mode.h"
#include "ae_glue_pause.h"
#include "ae_hud_digits.h"
#include "ae_panels.h"
#include "ae_power_list.h"
#include "ae_score_strip.h"
#include "ae_sensor.h"
#include "ae_style.h"
#include "ae_test_run_card.h"
#include "ae_view_bots.h"
#include "ae_view_bots_rules.h"
#include "../src/ae_draw.h"

void platform_log(char const *format, ...);
struct ae_screen *ae_ui_top(void);
/* source/interface/hud_item_timers.c's (its AE hook): the names over heads a view drew this frame */
short hud_item_timers_ae_name_tags(short local_player_index, rectangle2d *bounds, short maximum);

/* the TEST RUN card's tokens (ae_test_run_card.c) */
#define COLOR_REASON 0xA9B6C1FFu
#define COLOR_ICE 0xB8D6FFFFu
#define COLOR_PILL 0x0A1628C7u
#define COLOR_REC 0xFF5A4EFFu
#define COLOR_WHITE 0xFFFFFFFFu
/* the board: the menus' view panel's colour at .60 (see-through, owner 11-07); the glyphs' dark edge, the HUD messages' */
#define COLOR_BOARD 0x10161C99u
#define COLOR_EDGE 0x00000096u

#define VIEWS MAXIMUM_LOCAL_PLAYERS
#define LABELS 32
#define CLEAR 8.0f		/* the clearance rule: 8 at 1080p from every neighbour */
#define TAG_H 22.0f
#define TAG_PAD 9.0f
#define PIP_H 24.0f
#define PIP_TRIANGLE 12.0f	/* the 9 u triangle and its 3 gap */
#define PIP_INSET 34.0f
#define PIPS 12
#define REBUILD_TICKS 8		/* the rows' text 4 times a second */
#define DOT " \xC2\xB7 "

static struct
{
	boolean on;
	boolean enemy;
	short pinned;		/* a brain, or -1 */
} state = { FALSE, FALSE, -1 };

/* each view's pass this frame */
static struct
{
	unsigned int frame;	/* ae_draw_frame() of the pass (0: none yet) */
	unsigned int labels_frame;
	short noted;		/* the names hud_item_timers had noted when the last was recorded */
	short labels;
	struct { long player_index; rectangle2d bounds; } label[LABELS];
	real_matrix4x3 world_to_view;
	real p00, p20, p11, p21;
	real_point3d camera;
	real viewport_w, viewport_h, ppu;
	/* the aim's focus and since when */
	short aimed;
	long aimed_tick;
	/* the board as last drawn (layout units), for the label fade */
	boolean extent_valid;
	float extent[4];
	float rect[4];
} views[VIEWS];

/* the boards, rebuilt 4 times a second (one a team: two views of one team share it) */
static struct
{
	boolean valid;
	long team;
	long built;
	struct ae_vb_board board;
} boards[2];

/* ---------- the console */

int ae_view_bots_on(void)
{
	return state.on;
}

int ae_view_bots_command(char const *word, long number, char *text, int size)
{
	if (!state.on)
	{
		short v;

		for (v = 0; v < VIEWS; v++)
			views[v].aimed = -1;
	}
	if (number > 0)
	{
		short brain = (short)(number - 1);

		state.on = TRUE;
		if (state.pinned == brain)
		{
			state.pinned = -1;
			snprintf(text, (size_t)size, "view bots: bot %ld unpinned, the aim picks again", number);
		}
		else
		{
			state.pinned = brain;
			snprintf(text, (size_t)size, "view bots: bot %ld pinned (view bots %ld again or view bots aim unpins)", number, number);
		}
		return TRUE;
	}
	if (!strcmp(word, "off"))
	{
		state.on = FALSE;
		state.pinned = -1;
		snprintf(text, (size_t)size, "view bots off");
		return TRUE;
	}
	if (!strcmp(word, "aim"))
	{
		state.on = TRUE;
		state.pinned = -1;
		snprintf(text, (size_t)size, "view bots: the aim picks the bot shown");
		return TRUE;
	}
	if (!strcmp(word, "enemy") || !strcmp(word, "own"))
	{
		state.on = TRUE;
		state.enemy = !strcmp(word, "enemy");
		state.pinned = -1;
		snprintf(text, (size_t)size, state.enemy ? "view bots: the other team's bots (no tags: none through walls)" :
			"view bots: your team's bots");
		return TRUE;
	}
	state.on = TRUE;
	snprintf(text, (size_t)size, "view bots on: %s bots (view bots <n> pins one, view bots off hides it)",
		state.enemy ? "the other team's" : "your team's");
	return TRUE;
}

/* ---------- the HUD's passes */

void ae_view_bots_name_noted(long player_index)
{
	short v = render.local_player_index;
	unsigned int frame = ae_draw_frame();
	rectangle2d bounds[LABELS];
	short count;

	if (!state.on || v < 0 || v >= VIEWS)
		return;
	if (views[v].labels_frame != frame)
	{
		views[v].labels_frame = frame;
		views[v].labels = 0;
		views[v].noted = 0;
	}
	count = hud_item_timers_ae_name_tags(v, bounds, LABELS);
	if (count < views[v].noted)
		views[v].noted = 0; /* (the names were begun again in this frame) */
	if (count > views[v].noted && views[v].labels < LABELS)
	{
		views[v].label[views[v].labels].player_index = player_index;
		views[v].label[views[v].labels].bounds = bounds[count - 1];
		views[v].labels++;
	}
	views[v].noted = count;
}

void ae_view_bots_view(short local_player_index)
{
	short v = local_player_index;

	if (!state.on || v < 0 || v >= VIEWS || !render.frustum.projection_valid)
		return;
	views[v].frame = ae_draw_frame();
	views[v].world_to_view = render.frustum.world_to_view;
	views[v].p00 = render.frustum.projection_matrix[0][0];
	views[v].p20 = render.frustum.projection_matrix[2][0];
	views[v].p11 = render.frustum.projection_matrix[1][1];
	views[v].p21 = render.frustum.projection_matrix[2][1];
	views[v].camera = render.camera.position;
	views[v].viewport_w = (real)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0);
	views[v].viewport_h = (real)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0);
	views[v].ppu = ae_hud_pixels_per_unit();
}

int ae_view_bots_extent(short local_player_index, float extent[4])
{
	short v = local_player_index;
	float to_px;

	if (!state.on || v < 0 || v >= VIEWS || !views[v].extent_valid || views[v].frame + 2 < ae_draw_frame() ||
		views[v].rect[2] <= 0.0f)
	{
		return 0;
	}
	/* (layout units of the frame to the view's own pixels) */
	to_px = views[v].viewport_w * views[v].ppu / views[v].rect[2];
	extent[0] = (views[v].extent[0] - views[v].rect[0]) * to_px;
	extent[1] = (views[v].extent[1] - views[v].rect[1]) * to_px;
	extent[2] = (views[v].extent[2] - views[v].rect[0]) * to_px;
	extent[3] = (views[v].extent[3] - views[v].rect[1]) * to_px;
	return 1;
}

/* ---------- projection (a view's camera as its pass kept it) */

/* a world point in the view: TRUE on screen, at (x, y) in layout units; else the direction to it (dx, dy) */
static boolean project(short v, real_point3d const *world, float *x, float *y, float *dx, float *dy)
{
	real_point3d point;
	float sx, sy;
	float const *rect = views[v].rect;

	matrix4x3_transform_point(&views[v].world_to_view, world, &point);
	if (point.z < 0.0f)
	{
		float inverse = -1.0f / point.z;

		sx = (views[v].p00 * point.x + views[v].p20 * point.z) * inverse;
		sy = -(views[v].p11 * point.y + views[v].p21 * point.z) * inverse;
		if (sx >= -1.0f && sx <= 1.0f && sy >= -1.0f && sy <= 1.0f)
		{
			*x = rect[0] + (sx + 1.0f) * 0.5f * rect[2];
			*y = rect[1] + (sy + 1.0f) * 0.5f * rect[3];
			return TRUE;
		}
	}
	ae_vb_off_direction(point.x, point.y, point.z, dx, dy);
	return FALSE;
}

/* how far off the aim a point is (degrees) */
static float off_aim(short v, real_point3d const *world)
{
	real_point3d point;
	float across;

	matrix4x3_transform_point(&views[v].world_to_view, world, &point);
	if (point.z >= 0.0f)
		return 180.0f;
	across = (float)sqrt(point.x * point.x + point.y * point.y);
	return (float)(atan2(across, -point.z) * 180.0 / 3.14159265358979);
}

/* a name label in layout units */
static void label_box(short v, rectangle2d const *bounds, struct ae_vb_box *box)
{
	float const *rect = views[v].rect;
	float kx = views[v].viewport_w > 0.0f ? rect[2] / views[v].viewport_w : 1.0f;
	float ky = views[v].viewport_h > 0.0f ? rect[3] / views[v].viewport_h : 1.0f;

	box->x0 = rect[0] + (float)bounds->x0 * kx;
	box->x1 = rect[0] + (float)bounds->x1 * kx;
	box->y0 = rect[1] + (float)bounds->y0 * ky;
	box->y1 = rect[1] + (float)bounds->y1 * ky;
}

/* a box in the view's pixels (the HUD pieces' extents) in layout units */
static void pixel_box(short v, float const extent[4], struct ae_vb_box *box)
{
	float const *rect = views[v].rect;
	float k = views[v].viewport_w * views[v].ppu > 0.0f ? rect[2] / (views[v].viewport_w * views[v].ppu) : 1.0f;

	box->x0 = rect[0] + extent[0] * k;
	box->y0 = rect[1] + extent[1] * k;
	box->x1 = rect[0] + extent[2] * k;
	box->y1 = rect[1] + extent[3] * k;
}

/* ---------- text */

struct font_spec { int font; float size; float tracking; };

static float measure(char const *text, void *context)
{
	struct font_spec const *spec = (struct font_spec const *)context;

	return spec->tracking > 0.0f ? ae_draw_text_tracked_width(spec->font, spec->size, spec->tracking, text) :
		ae_draw_text_width(spec->font, spec->size, text);
}

static float width_of(int font, float size, float tracking, char const *text)
{
	return tracking > 0.0f ? ae_draw_text_tracked_width(font, size, tracking, text) : ae_draw_text_width(font, size, text);
}

static unsigned int with_alpha(unsigned int rgba, float alpha)
{
	unsigned int a = (unsigned int)((float)(rgba & 0xFFu) * alpha + 0.5f);

	return (rgba & 0xFFFFFF00u) | (a > 255u ? 255u : a);
}

/* text in a line box (the capitals centred in it), with the HUD messages' dark edge round each glyph when edge is set;
returns its right */
static float put(int font, float size, float tracking, float x, float top, float line_h, int align, unsigned int color,
	int edge, float alpha, char const *text)
{
	static float const offsets[5][2] = { { 1.0f, 1.0f }, { -1.0f, 1.0f }, { 1.0f, -1.0f }, { -1.0f, -1.0f }, { 0.0f, 1.5f } };
	float y = top + (line_h - ae_draw_cap_height(font, size)) * 0.5f;
	float width = width_of(font, size, tracking, text);
	int i;

	if (edge)
		for (i = 0; i < 5; i++)
		{
			if (tracking > 0.0f)
				ae_draw_text_tracked(font, size, tracking, x + offsets[i][0], y + offsets[i][1], align,
					with_alpha(COLOR_EDGE, alpha), text);
			else
				ae_draw_text(font, size, x + offsets[i][0], y + offsets[i][1], align, with_alpha(COLOR_EDGE, alpha), text);
		}
	if (tracking > 0.0f)
		ae_draw_text_tracked(font, size, tracking, x, y, align, with_alpha(color, alpha), text);
	else
		ae_draw_text(font, size, x, y, align, with_alpha(color, alpha), text);
	return align == AE_ALIGN_RIGHT ? x : align == AE_ALIGN_CENTER ? x + width * 0.5f : x + width;
}

static void upper(char *text)
{
	for (; *text; text++)
		if (*text >= 'a' && *text <= 'z')
			*text = (char)(*text - 'a' + 'A');
}

/* ---------- the board */

static struct ae_vb_board const *board_of(long team)
{
	long now = game_time_get();
	int slot, free_slot = -1;

	for (slot = 0; slot < 2; slot++)
	{
		if (boards[slot].valid && boards[slot].team == team && now >= boards[slot].built &&
			now - boards[slot].built < REBUILD_TICKS)
		{
			return &boards[slot].board;
		}
		if (free_slot < 0 && (!boards[slot].valid || boards[slot].team == team))
			free_slot = slot;
	}
	if (free_slot < 0)
		free_slot = boards[0].built <= boards[1].built ? 0 : 1;
	boards[free_slot].valid = ae_bots_view_board(team, &boards[free_slot].board);
	boards[free_slot].team = team;
	boards[free_slot].built = now;
	return boards[free_slot].valid ? &boards[free_slot].board : NULL;
}

static struct ae_vb_row const *row_of(struct ae_vb_board const *board, short brain)
{
	short r;

	for (r = 0; r < board->rows; r++)
		if (board->row[r].brain == brain)
			return &board->row[r];
	return NULL;
}

static unsigned int state_color(short s)
{
	return s == AE_VB_STATE_FIGHT ? COLOR_REC : s == AE_VB_STATE_STUCK || s == AE_VB_STATE_NO_PATH ? AE_COLOR_DESTRUCTIVE_INK :
		AE_COLOR_TEXT;
}

/* one bot's row in its column */
static void draw_row(struct ae_vb_geometry const *g, struct ae_vb_row const *row, float cx, float cr, float ry, int focus)
{
	char number[8], text[AE_VB_DETAIL + 8];
	struct font_spec detail_font;
	float room;
	unsigned int ink = row->dead ? COLOR_REASON : AE_COLOR_TITLE;

	if (focus)
		ae_draw_outline(cx - 6.0f, ry, cr + 4.0f - (cx - 6.0f), g->row_h, 4.0f, 2.0f, COLOR_ICE);
	snprintf(number, sizeof(number), "%d", (int)row->brain + 1);
	put(AE_FONT_ROW, g->num_px, 0.0f, cx + g->col_num, ry, g->row_h, AE_ALIGN_RIGHT, row->dead ? COLOR_REASON : COLOR_ICE, 1,
		1.0f, number);
	detail_font.font = AE_FONT_ROW;
	detail_font.size = g->name_px;
	detail_font.tracking = 0.0f;
	ae_vb_fit(row->name, g->col_src - g->col_name - 6.0f, measure, &detail_font, text, sizeof(text));
	put(AE_FONT_ROW, g->name_px, 0.0f, cx + g->col_name, ry, g->row_h, AE_ALIGN_LEFT, ink, 1, 1.0f, text);
	detail_font.font = AE_FONT_BODY;
	detail_font.size = g->det_px;
	if (row->dead)
	{
		char said[AE_VB_DETAIL];

		if (row->back_in > 0)
			snprintf(said, sizeof(said), g->header ? "dead" DOT "back in %d s" : "dead" DOT "%d s", (int)row->back_in);
		else
			snprintf(said, sizeof(said), "dead");
		ae_vb_fit(said, cr - (cx + g->col_src), measure, &detail_font, text, sizeof(text));
		put(AE_FONT_BODY, g->det_px, 0.0f, cx + g->col_src, ry, g->row_h, AE_ALIGN_LEFT, COLOR_REASON, 1, 1.0f, text);
		return;
	}
	{
		char const *word = ae_vb_source_word(row->source, row->goal_kind);

		/* (a word too long for its column, OBJECTIVE, is said by its code) */
		if (width_of(AE_FONT_ROW, g->src_px, 0.08f, word) > g->col_det - g->col_src - CLEAR)
			word = ae_vb_source_code(row->source);
		put(AE_FONT_ROW, g->src_px, 0.08f, cx + g->col_src, ry, g->row_h, AE_ALIGN_LEFT, AE_COLOR_ACCENT, 1, 1.0f, word);
	}
	room = cr - (cx + g->col_det) - (row->state != AE_VB_STATE_NONE ? g->state_w + 6.0f : 0.0f);
	ae_vb_fit(row->detail, room, measure, &detail_font, text, sizeof(text));
	put(AE_FONT_BODY, g->det_px, 0.0f, cx + g->col_det, ry, g->row_h, AE_ALIGN_LEFT, AE_COLOR_TEXT, 1, 1.0f, text);
	if (row->state != AE_VB_STATE_NONE)
		put(AE_FONT_ROW, g->state_px, 0.06f, cr, ry, g->row_h, AE_ALIGN_RIGHT, state_color(row->state), 1, 1.0f,
			ae_vb_state_word(row->state));
}

/* the strip's line: who and what (the focus's last WHY, or the team's newest) */
struct strip
{
	int kind;
	long age;
	char who[24];
	char const *text;
};

static void strip_of(struct ae_vb_board const *board, short focus, struct strip *strip)
{
	struct ae_vb_row const *row = focus >= 0 ? row_of(board, focus) : NULL;

	memset(strip, 0, sizeof(*strip));
	strip->kind = ae_vb_strip_pick(row != NULL, row && row->why, board->newest_why != NULL,
		board->newest_why ? board->now - board->newest_tick : -1);
	if (strip->kind == AE_VB_STRIP_FOCUS)
	{
		strip->age = board->now - row->why_tick;
		snprintf(strip->who, sizeof(strip->who), "%d %s", (int)row->brain + 1, row->name);
		strip->text = row->why;
	}
	else if (strip->kind == AE_VB_STRIP_NEWEST)
	{
		struct ae_vb_row const *newest = board->newest_brain >= 0 ? row_of(board, board->newest_brain) : NULL;

		strip->age = board->now - board->newest_tick;
		if (board->newest_brain < 0)
			snprintf(strip->who, sizeof(strip->who), "CALL");
		else if (newest)
			snprintf(strip->who, sizeof(strip->who), "%d %s", (int)newest->brain + 1, newest->name);
		else
			snprintf(strip->who, sizeof(strip->who), "%d", (int)board->newest_brain + 1);
		strip->text = board->newest_why;
	}
}

/* the board of one view; its box */
static void draw_board(short v, struct ae_vb_board const *board, short focus, boolean pinned, boolean split,
	boolean half_width, float bottom, struct ae_vb_box *box)
{
	struct ae_vb_geometry g;
	struct ae_vb_layout l;
	struct strip strip;
	struct font_spec why_font;
	char lines[2][AE_VB_LINE], age[16], text[AE_VB_HEAD + 8];
	short rows[AE_VB_SQUADS];
	float x0, top, cx0, content_w, pre = 0.0f, y;
	int s, line_count = 0, first_row[AE_VB_SQUADS];
	float const *rect = views[v].rect;
	struct ae_layout layout;

	ae_draw_current_layout(&layout);
	ae_vb_geometry_of(split, &g);
	x0 = ae_vb_board_left(layout.width, rect[0], rect[2], half_width);
	top = ae_vb_board_top(rect[1], split);
	cx0 = x0 + g.bar + g.pad_l;
	content_w = g.band - g.bar - g.pad_l - g.pad_r;
	for (s = 0; s < board->squads && s < AE_VB_SQUADS; s++)
	{
		rows[s] = board->squad[s].rows;
		first_row[s] = s ? first_row[s - 1] + rows[s - 1] : 0;
	}
	/* (the strip's lines: the text wrapped once in the room after its age, WHY and who) */
	strip_of(board, focus, &strip);
	if (strip.kind != AE_VB_STRIP_NONE)
	{
		ae_vb_age_text(strip.age, age, sizeof(age));
		pre = width_of(AE_FONT_ROW, g.strip_small_px, 0.0f, age) + 8.0f +
			width_of(AE_FONT_ROW, g.strip_small_px, 0.10f, "WHY") + 10.0f +
			width_of(AE_FONT_ROW, g.strip_px, 0.0f, strip.who) + 10.0f;
		why_font.font = AE_FONT_BODY;
		why_font.size = g.strip_px;
		why_font.tracking = 0.0f;
		line_count = ae_vb_wrap(strip.text, content_w - pre, 2, measure, &why_font, lines);
	}
	ae_vb_layout(&g, split, x0, top, bottom, rows, board->squads, line_count, &l);
	box->x0 = x0;
	box->y0 = top;
	box->x1 = x0 + l.width;
	box->y1 = top + l.height;
	ae_draw_rect(x0, top, l.width, l.height, 0.0f, COLOR_BOARD);
	ae_draw_rect(x0, top, g.bar, l.height, 0.0f, AE_COLOR_ACCENT);
	y = top + g.pad_t;
	if (g.header)
	{
		char right[32];
		unsigned int right_color = COLOR_ICE;

		snprintf(text, sizeof(text), "BOTS" DOT "%s" DOT "%d", board->team_name, (int)board->count);
		put(AE_FONT_ROW, g.head_px, 0.14f, cx0, y, g.head_h, AE_ALIGN_LEFT, AE_COLOR_ACCENT, 1, 1.0f, text);
		if (focus >= 0)
			snprintf(right, sizeof(right), "%s %d", pinned ? "PINNED" : "AIMED", (int)focus + 1);
		else
		{
			snprintf(right, sizeof(right), "AIM TO INSPECT");
			right_color = COLOR_REASON;
		}
		put(AE_FONT_ROW, g.head_right_px, 0.10f, x0 + l.width - g.pad_r, y, g.head_h, AE_ALIGN_RIGHT, right_color, 1, 1.0f,
			right);
	}
	y = top + l.body_y;
	if (l.columns == 2)
		ae_draw_rect(cx0 + l.col_x[1] - g.gutter * 0.5f, y + 3.0f, 1.0f, l.body_h - 6.0f, 0.0f, AE_COLOR_RULE);
	for (s = 0; s < board->squads && s < AE_VB_SQUADS; s++)
	{
		struct ae_vb_place const *place = &l.place[s];
		float cx = cx0 + l.col_x[place->column], cr = cx + l.col_w, sy = y + place->y, ry;
		struct font_spec head_font;
		int r;

		if (place->shown < 0)
			continue;
		if (place->y > 0.0f)
			ae_draw_rect(cx, sy - g.stack_gap * 0.5f, l.col_w, 1.0f, 0.0f, AE_COLOR_RULE);
		/* (the header cut before its note) */
		head_font.font = AE_FONT_ROW;
		head_font.size = g.sq_px;
		head_font.tracking = 0.10f;
		ae_vb_fit(board->squad[s].head, l.col_w - (board->squad[s].note[0] ?
			width_of(AE_FONT_BODY, g.note_px, 0.0f, board->squad[s].note) + 10.0f : 0.0f), measure, &head_font, text,
			sizeof(text));
		put(AE_FONT_ROW, g.sq_px, 0.10f, cx, sy, g.sq_h, AE_ALIGN_LEFT, COLOR_ICE, 1, 1.0f, text);
		if (board->squad[s].note[0])
			put(AE_FONT_BODY, g.note_px, 0.0f, cr, sy, g.sq_h, AE_ALIGN_RIGHT, COLOR_REASON, 1, 1.0f, board->squad[s].note);
		ry = sy + g.sq_h;
		for (r = 0; r < place->shown; r++)
		{
			struct ae_vb_row const *row = &board->row[first_row[s] + r];

			draw_row(&g, row, cx, cr, ry, row->brain == focus);
			ry += g.row_h;
		}
		if (place->more > 0)
		{
			snprintf(text, sizeof(text), "+%d MORE", (int)place->more);
			put(AE_FONT_ROW, g.src_px, 0.08f, cx + g.col_name, ry, g.row_h, AE_ALIGN_LEFT, COLOR_REASON, 1, 1.0f, text);
		}
	}
	if (l.strip_lines > 0 && line_count > 0)
	{
		float sx, first_x;
		int i;

		y = top + l.strip_y;
		ae_draw_rect(cx0, y + g.strip_sep * 0.5f, content_w, 1.0f, 0.0f, AE_COLOR_RULE);
		y += g.strip_sep;
		sx = put(AE_FONT_ROW, g.strip_small_px, 0.0f, cx0, y, g.strip_h, AE_ALIGN_LEFT, COLOR_REASON, 1, 1.0f, age) + 8.0f;
		sx = put(AE_FONT_ROW, g.strip_small_px, 0.10f, sx, y, g.strip_h, AE_ALIGN_LEFT, COLOR_REASON, 1, 1.0f, "WHY") + 10.0f;
		first_x = put(AE_FONT_ROW, g.strip_px, 0.0f, sx, y, g.strip_h, AE_ALIGN_LEFT, AE_COLOR_TITLE, 1, 1.0f, strip.who) +
			10.0f;
		for (i = 0; i < line_count && i < l.strip_lines; i++)
		{
			put(AE_FONT_BODY, g.strip_px, 0.0f, first_x, y, g.strip_h, AE_ALIGN_LEFT, AE_COLOR_TEXT, 1, 1.0f, lines[i]);
			y += g.strip_h;
		}
	}
}

/* ---------- the world: tags, the goal line, pips */

/* a pill (round ends) */
static void pill(struct ae_vb_box const *box, unsigned int fill)
{
	float h = box->y1 - box->y0;

	ae_draw_rect(box->x0, box->y0, box->x1 - box->x0, h, h * 0.5f, fill);
}

static void ring(float x, float y, float radius, float thickness, unsigned int color, unsigned int fill)
{
	if (fill)
		ae_draw_rect(x - radius, y - radius, radius * 2.0f, radius * 2.0f, radius, fill);
	ae_draw_outline(x - radius, y - radius, radius * 2.0f, radius * 2.0f, radius, thickness, color);
}

/* the game's off-screen waypoint triangle, pointing out from (x, y) toward side, depth deep and 14 across */
static void triangle(float x, float y, int side, float depth, unsigned int color)
{
	int i;

	/* (filled with strokes from the tip to points along its base) */
	for (i = -3; i <= 3; i++)
	{
		float along = 7.0f * (float)i / 3.0f;

		switch (side)
		{
		case AE_VB_SIDE_LEFT:
			ae_draw_line(x, y, x + depth, y + along, 2.0f, color);
			break;
		case AE_VB_SIDE_RIGHT:
			ae_draw_line(x, y, x - depth, y + along, 2.0f, color);
			break;
		case AE_VB_SIDE_TOP:
			ae_draw_line(x, y, x + along, y + depth, 2.0f, color);
			break;
		default:
			ae_draw_line(x, y, x + along, y - depth, 2.0f, color);
			break;
		}
	}
}

/* a dashed line, 7 on and 5 off */
static void dashed(float x0, float y0, float x1, float y1, unsigned int color)
{
	float dx = x1 - x0, dy = y1 - y0, length = (float)sqrt(dx * dx + dy * dy), at;

	if (length < 1.0f)
		return;
	dx /= length;
	dy /= length;
	for (at = 0.0f; at < length && at < 4000.0f; at += 12.0f)
	{
		float end = at + 7.0f < length ? at + 7.0f : length;

		ae_draw_line(x0 + dx * at, y0 + dy * at, x0 + dx * end, y0 + dy * end, 2.0f, color);
	}
}

/* the focused bot's walk: from its feet to its goal, a ring at the goal and its caption on the side away from the
crosshair */
static void draw_goal_line(short v, struct ae_vb_row const *row, struct ae_vb_box const *view_box)
{
	struct player_datum const *player = player_try_and_get(row->player_index);
	struct object_datum const *object = player && player->unit_index != NONE ? object_try_and_get(player->unit_index) : NULL;
	real_point3d goal;
	float fx, fy, gx, gy, dx, dy, w, cx;
	boolean feet_on, goal_on;
	char caption[AE_VB_DETAIL + 8];
	struct ae_vb_box box;

	if (!object || !row->has_goal || row->dead)
		return;
	goal.x = row->goal[0];
	goal.y = row->goal[1];
	goal.z = row->goal[2];
	feet_on = project(v, &object->object.position, &fx, &fy, &dx, &dy);
	goal_on = project(v, &goal, &gx, &gy, &dx, &dy);
	if (!goal_on)
		ae_vb_pip_anchor(dx, dy, view_box, PIP_INSET, &gx, &gy);
	if (feet_on)
	{
		dashed(fx, fy, gx, gy, with_alpha(COLOR_ICE, 0.9f));
		ring(fx, fy, 3.0f, 2.0f, COLOR_ICE, 0u);
	}
	if (goal_on)
		ring(gx, gy, 7.0f, 2.0f, COLOR_ICE, with_alpha(COLOR_PILL, 0.8f));
	snprintf(caption, sizeof(caption), "%s", row->detail);
	upper(caption);
	w = width_of(AE_FONT_ROW, 12.0f, 0.08f, caption) + 18.0f;
	cx = (view_box->x0 + view_box->x1) * 0.5f;
	box.y0 = gy - 10.0f;
	box.y1 = gy + 10.0f;
	if (gx < cx)
	{
		box.x1 = gx - 12.0f;
		box.x0 = box.x1 - w;
	}
	else
	{
		box.x0 = gx + 12.0f;
		box.x1 = box.x0 + w;
	}
	/* (held inside the view) */
	if (box.x0 < view_box->x0 + PIP_INSET)
	{
		box.x1 += view_box->x0 + PIP_INSET - box.x0;
		box.x0 = view_box->x0 + PIP_INSET;
	}
	if (box.x1 > view_box->x1 - PIP_INSET)
	{
		box.x0 -= box.x1 - (view_box->x1 - PIP_INSET);
		box.x1 = view_box->x1 - PIP_INSET;
	}
	pill(&box, COLOR_PILL);
	put(AE_FONT_ROW, 12.0f, 0.08f, box.x0 + 9.0f, box.y0, 20.0f, AE_ALIGN_LEFT, COLOR_ICE, 0, 1.0f, caption);
}

/* the blocks the tags and pips keep clear of: the board, the card, the HUD's pieces in this view */
static int hud_blocks(short v, short local, struct ae_vb_box const *board_box, struct ae_vb_box *blocks, int most)
{
	int count = 0;
	float card[4], extent[4];
	struct ae_panel_rect panels[2];
	short panel_count = 0, i;

	blocks[count++] = *board_box;
	if (ae_test_run_card_box(card) && count < most)
	{
		blocks[count].x0 = card[0];
		blocks[count].y0 = card[1];
		blocks[count].x1 = card[2];
		blocks[count].y1 = card[3];
		count++;
	}
	if (ae_score_strip_extent(local, extent) && count < most)
		pixel_box(v, extent, &blocks[count++]);
	if (ae_power_list_extent(local, extent) && count < most)
		pixel_box(v, extent, &blocks[count++]);
	if (ae_sensor_rectangle(local, &extent[0], &extent[1], &extent[2], &extent[3]) && count < most)
		pixel_box(v, extent, &blocks[count++]);
	ae_panels_rectangles(local, panels, &panel_count);
	for (i = 0; i < panel_count && i < 2 && count < most; i++)
	{
		extent[0] = panels[i].x0;
		extent[1] = panels[i].y0;
		extent[2] = panels[i].x1;
		extent[3] = panels[i].y1;
		pixel_box(v, extent, &blocks[count++]);
	}
	return count;
}

/* the numbered tags by the names over the board's bots in view */
static void draw_tags(short v, struct ae_vb_board const *board, short focus, struct ae_vb_box const *view_box,
	struct ae_vb_box const *blocks_in, int block_count, float const crosshair[4])
{
	struct ae_vb_box blocks[LABELS * 2 + 16];
	int count = 0, i, l;

	for (i = 0; i < block_count && count < (int)(sizeof(blocks) / sizeof(blocks[0])); i++)
		blocks[count++] = blocks_in[i];
	for (l = 0; l < views[v].labels && count < (int)(sizeof(blocks) / sizeof(blocks[0])); l++)
		label_box(v, &views[v].label[l].bounds, &blocks[count++]);
	for (i = 0; i < board->rows; i++)
	{
		struct ae_vb_row const *row = &board->row[i];
		struct ae_vb_box label, box;
		char number[8];
		char const *word;
		float number_w, width, alpha;
		int kind, own = -1, k, own_block;
		struct ae_vb_box kept;

		if (row->dead)
			continue;
		for (l = 0; l < views[v].labels; l++)
			if (views[v].label[l].player_index == row->player_index)
				own = l;
		if (own < 0)
			continue;
		label_box(v, &views[v].label[own].bounds, &label);
		snprintf(number, sizeof(number), "%d", (int)row->brain + 1);
		word = ae_vb_source_word(row->source, row->goal_kind);
		number_w = width_of(AE_FONT_ROW, 14.0f, 0.0f, number);
		width = TAG_PAD + number_w + (word[0] ? 8.0f + width_of(AE_FONT_ROW, 12.0f, 0.08f, word) : 0.0f) + TAG_PAD;
		/* (its own label is not a block to it: set aside while it is placed) */
		own_block = -1;
		for (k = block_count; k < block_count + views[v].labels && k < count; k++)
			if (k - block_count == own)
				own_block = k;
		if (own_block >= 0)
		{
			kept = blocks[own_block];
			blocks[own_block].x0 = blocks[own_block].x1 = -10000.0f;
			blocks[own_block].y0 = blocks[own_block].y1 = -10000.0f;
		}
		kind = ae_vb_tag_place(&label, width, number_w + 2.0f * TAG_PAD - 4.0f, TAG_H, blocks, count, view_box, CLEAR, &box);
		if (own_block >= 0)
			blocks[own_block] = kept;
		alpha = ae_vb_tag_alpha(&box, crosshair, kind == AE_VB_TAG_FAINT);
		pill(&box, with_alpha(COLOR_PILL, alpha));
		if (row->brain == focus)
			ae_draw_outline(box.x0 - 1.0f, box.y0 - 1.0f, box.x1 - box.x0 + 2.0f, box.y1 - box.y0 + 2.0f,
				(box.y1 - box.y0) * 0.5f + 1.0f, 2.0f, COLOR_ICE);
		if (kind == AE_VB_TAG_ABOVE || kind == AE_VB_TAG_AFTER)
		{
			float x = put(AE_FONT_ROW, 14.0f, 0.0f, box.x0 + TAG_PAD, box.y0, TAG_H, AE_ALIGN_LEFT, COLOR_ICE, 0, alpha, number);

			if (word[0])
				put(AE_FONT_ROW, 12.0f, 0.08f, x + 8.0f, box.y0, TAG_H, AE_ALIGN_LEFT, COLOR_WHITE, 0, alpha, word);
		}
		else
			put(AE_FONT_ROW, 14.0f, 0.0f, (box.x0 + box.x1) * 0.5f, box.y0, TAG_H, AE_ALIGN_CENTER, COLOR_ICE, 0, alpha, number);
		if (count < (int)(sizeof(blocks) / sizeof(blocks[0])))
			blocks[count++] = box;
	}
}

/* the pips for the board's living bots out of view: one a squad whose bots out of view are together, else one a bot */
static void draw_pips(short v, struct ae_vb_board const *board, struct ae_vb_box const *view_box,
	struct ae_vb_box const *blocks_in, int block_count)
{
	struct ae_vb_box blocks[32];
	int count = 0, i, s, pips = 0;
	for (i = 0; i < block_count && count < 24; i++)
		blocks[count++] = blocks_in[i];
	for (s = 0; s < board->squads && pips < PIPS; s++)
	{
		float points[AE_VB_ROWS][3];
		short members[AE_VB_ROWS];
		int n = 0, m, grouped;

		for (i = 0; i < board->rows; i++)
		{
			struct ae_vb_row const *row = &board->row[i];
			struct player_datum const *player;
			struct object_datum const *object;
			real_point3d head;
			float x, y, dx, dy;

			if (row->squad != s || row->dead)
				continue;
			player = player_try_and_get(row->player_index);
			object = player && player->unit_index != NONE ? object_try_and_get(player->unit_index) : NULL;
			if (!object)
				continue;
			unit_get_head_position(player->unit_index, &head);
			if (project(v, &head, &x, &y, &dx, &dy))
				continue;
			points[n][0] = object->object.position.x;
			points[n][1] = object->object.position.y;
			points[n][2] = object->object.position.z;
			members[n++] = (short)i;
		}
		/* (a squad's own group: the bots in no squad never share a pip) */
		grouped = n >= 2 && board->squad[s].letter[0] && ae_vb_together((float const (*)[3])points, n);
		for (m = 0; m < (grouped ? 1 : n) && pips < PIPS; m++)
		{
			real_point3d at;
			float x, y, dx, dy, width, metres;
			char text[48];
			struct ae_vb_box box;
			int side, k;

			if (grouped)
			{
				at.x = at.y = at.z = 0.0f;
				for (k = 0; k < n; k++)
				{
					at.x += points[k][0] / (float)n;
					at.y += points[k][1] / (float)n;
					at.z += points[k][2] / (float)n;
				}
			}
			else
			{
				at.x = points[m][0];
				at.y = points[m][1];
				at.z = points[m][2];
			}
			metres = (float)ae_vb_metres(distance3d(&views[v].camera, &at));
			if (grouped)
				snprintf(text, sizeof(text), "SQUAD %s" DOT "%d" DOT "%ld M", board->squad[s].letter, n, (long)metres);
			else
			{
				struct ae_vb_row const *row = &board->row[members[m]];

				snprintf(text, sizeof(text), "%d %s" DOT "%ld M", (int)row->brain + 1, row->name, (long)metres);
				upper(text);
			}
			if (project(v, &at, &x, &y, &dx, &dy))
			{
				/* (the centre is in view though each bot is not: toward it from the middle) */
				float cx = (view_box->x0 + view_box->x1) * 0.5f, cy = (view_box->y0 + view_box->y1) * 0.5f;
				float larger = (float)(fabs(x - cx) > fabs(y - cy) ? fabs(x - cx) : fabs(y - cy));

				dx = larger > 0.0f ? (x - cx) / larger : 0.0f;
				dy = larger > 0.0f ? (y - cy) / larger : 1.0f;
			}
			side = ae_vb_pip_anchor(dx, dy, view_box, PIP_INSET, &x, &y);
			width = width_of(AE_FONT_ROW, 13.0f, 0.08f, text) + 20.0f;
			if (!ae_vb_pip_place(side, x, y, width, PIP_H, PIP_TRIANGLE, blocks, count, view_box, CLEAR, &box))
				continue;
			{
				struct ae_vb_box body = box;

				switch (side)
				{
				case AE_VB_SIDE_LEFT:
					body.x0 += PIP_TRIANGLE;
					triangle(box.x0, (box.y0 + box.y1) * 0.5f, side, 9.0f, COLOR_ICE);
					break;
				case AE_VB_SIDE_RIGHT:
					body.x1 -= PIP_TRIANGLE;
					triangle(box.x1, (box.y0 + box.y1) * 0.5f, side, 9.0f, COLOR_ICE);
					break;
				case AE_VB_SIDE_TOP:
					body.y0 += PIP_TRIANGLE;
					triangle((box.x0 + box.x1) * 0.5f, box.y0, side, 9.0f, COLOR_ICE);
					break;
				default:
					body.y1 -= PIP_TRIANGLE;
					triangle((box.x0 + box.x1) * 0.5f, box.y1, side, 9.0f, COLOR_ICE);
					break;
				}
				pill(&body, COLOR_PILL);
				ae_draw_outline(body.x0, body.y0, body.x1 - body.x0, body.y1 - body.y0, (body.y1 - body.y0) * 0.5f, 1.5f,
					with_alpha(COLOR_ICE, 0.55f));
				put(AE_FONT_ROW, 13.0f, 0.08f, body.x0 + 10.0f, body.y0, body.y1 - body.y0, AE_ALIGN_LEFT, COLOR_ICE, 0, 1.0f,
					text);
			}
			if (count < 32)
				blocks[count++] = box;
			pips++;
		}
	}
}

/* the aim's focus in this view: the board's living bot named in view nearest the aim, within 6 degrees, held 1 s */
static short aim_focus(short v, struct ae_vb_board const *board)
{
	float degrees[AE_VB_ROWS];
	int eligible[AE_VB_ROWS], i, l, picked;
	long now = game_time_get();

	for (i = 0; i < board->rows; i++)
	{
		struct ae_vb_row const *row = &board->row[i];
		struct player_datum const *player = row->dead ? NULL : player_try_and_get(row->player_index);

		eligible[i] = 0;
		degrees[i] = 180.0f;
		if (!player || player->unit_index == NONE)
			continue;
		for (l = 0; l < views[v].labels; l++)
			if (views[v].label[l].player_index == row->player_index)
				eligible[i] = 1;
		if (eligible[i])
		{
			real_point3d head;

			unit_get_head_position(player->unit_index, &head);
			degrees[i] = off_aim(v, &head);
		}
	}
	picked = ae_vb_aim_pick(degrees, eligible, board->rows);
	if (picked >= 0)
	{
		views[v].aimed = board->row[picked].brain;
		views[v].aimed_tick = now;
		return views[v].aimed;
	}
	views[v].aimed = (short)ae_vb_aim_hold(views[v].aimed, views[v].aimed_tick, -1, now);
	return views[v].aimed >= 0 && row_of(board, views[v].aimed) ? views[v].aimed : -1;
}

/* ---------- the frame */

static void draw_view(short v, short local, short views_count, short spans)
{
	struct player_datum const *viewer;
	struct ae_vb_board const *board;
	long player_index = local_player_get_player_index(local), team;
	boolean split = views_count > 1, half_width = views_count > 2 && !spans, pinned;
	struct ae_vb_box view_box, board_box, blocks[16];
	float crosshair[4];
	float const *rect = views[v].rect;
	short focus;
	int block_count;

	viewer = player_index != NONE ? player_try_and_get(player_index) : NULL;
	if (!viewer)
		return;
	if (!game_engine_has_teams())
		team = -1;
	else if (state.enemy)
		team = viewer->team_index == 0 ? 1 : 0;
	else
		team = viewer->team_index;
	board = board_of(team);
	if (!board)
		return;
	view_box.x0 = rect[0];
	view_box.y0 = rect[1];
	view_box.x1 = rect[0] + rect[2];
	view_box.y1 = rect[1] + rect[3];
	ae_vb_crosshair_area(rect[0], rect[1], rect[2], rect[3], crosshair);
	pinned = state.pinned >= 0 && row_of(board, state.pinned) != NULL;
	focus = pinned ? state.pinned : aim_focus(v, board);
	/* (the goal line under everything) */
	if (focus >= 0)
		draw_goal_line(v, row_of(board, focus), &view_box);
	draw_board(v, board, focus, pinned, split, half_width, crosshair[1] - CLEAR, &board_box);
	views[v].extent_valid = TRUE;
	views[v].extent[0] = board_box.x0;
	views[v].extent[1] = board_box.y0;
	views[v].extent[2] = board_box.x1;
	views[v].extent[3] = board_box.y1;
	block_count = hud_blocks(v, local, &board_box, blocks, 16);
	/* (no tags or pips for the other team: none through walls) */
	if (state.enemy && team >= 0)
		return;
	draw_tags(v, board, focus, &view_box, blocks, block_count, crosshair);
	if (team >= 0)
		draw_pips(v, board, &view_box, blocks, block_count);
}

void ae_view_bots_draw(void)
{
	unsigned int frame;
	short local;
	struct ae_layout layout;

	if (!state.on)
		return;
	for (local = 0; local < VIEWS; local++)
		views[local].extent_valid = FALSE;
	if (!ae_dev_mode_on() || !ae_draw_available() || !game_engine_running() || network_game_distributed_client() ||
		ae_ui_top() != NULL)
	{
		return;
	}
	frame = ae_draw_frame();
	ae_draw_view_full();
	ae_draw_set_alpha(1.0f);
	ae_draw_current_layout(&layout);
	for (local = local_player_get_next(NONE); local != NONE; local = local_player_get_next(local))
	{
		short view, views_count, spans, columns, rows_count, slot;

		if (local < 0 || local >= VIEWS || views[local].frame != frame)
			continue;
		if (game_engine_scoreboard_shown(local) > 0.0f)
			continue;
		if (!ae_glue_pause_view(local, &view, &views_count, &spans))
			continue;
		/* (the view's rectangle as the game lays split screens out: compute_window_bounds's, as the pause's) */
		columns = views_count <= 2 ? 1 : 2;
		rows_count = views_count <= 1 ? 1 : 2;
		slot = view;
		if (views_count == 3 && view > 0)
			slot = (short)(view + 1);
		views[local].rect[2] = layout.width / (float)columns * (spans ? 2.0f : 1.0f);
		views[local].rect[3] = layout.height / (float)rows_count;
		views[local].rect[0] = (float)(slot % columns) * (layout.width / (float)columns);
		views[local].rect[1] = (float)(slot / columns) * (layout.height / (float)rows_count);
		draw_view(local, local, views_count, spans);
	}
}
