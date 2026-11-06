/*
AE_SCORE_STRIP.C

The score strip (ae_score_strip.h; spec 11, 14.15 part 6-7; plan Task 6): a
view's strip from the engine reader, the model and the layout, drawn in the
HUD's pass in the strip's language.

When: HUD STYLE AE, a multiplayer game running with a strip (the reader's),
and this view's unit HUD drawn this frame (hud.c tells us, so the strip obeys
exactly what the unit HUD obeys: cinematics, the death and respawn camera, the
script's hidden HUD, a view with no unit); never in the PRE-GAME COUNTDOWN nor
once the game is over. A held scoreboard fades it to 35 % (1 - 0.65 x the
scoreboard's own fade).

Motion (11.8): a score that changes pops 1.0 -> 1.2 in 80 ms and settles in
170 ms (ease-out) with its bar's edge flashing white (not the times of timed
modes, which change every second); the bars ease to their fill in 250 ms; a new
leader's lead pops; a flag line whose state changes flashes white twice in 600
ms; in the clock's last 10 seconds it turns white and pulses 1.08 on each
second.
*/

#include <math.h>
#include <string.h>

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "cutscene/cinematics.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "interface/hud_draw.h"
#include "rasterizer/rasterizer.h"
#include "render/render.h"

#include "ae_hud_digits.h"
#include "ae_hud_style.h"
#include "ae_hud_text.h"
#include "ae_panel_draw.h"
#include "ae_panel_engine.h"
#include "ae_power_list.h"
#include "ae_score_strip.h"
#include "ae_strip_engine.h"
#include "ae_strip_layout.h"
#include "ae_strip_model.h"

unsigned long system_milliseconds(void);
void platform_video_drawable_size(int *width, int *height);
/* hud_item_timers.c's (an AE hook): the corner clock's place from the motion sensor's and the meters' (HUD units) */
void hud_item_timers_ae_place(short *inset, long *baseline);

#define AE_STRIP_SCOREBOARD_FADE 0.65f
#define AE_STRIP_BACKING_ALPHA 0.45f
#define AE_STRIP_GLOW_ALPHA 0.85f
#define AE_STRIP_GOAL_ALPHA 0.72f

/* the strip's tokens (spec 11.3, 11.10b, 12.2), by enum ae_strip_color */
static const real color_values[][3] = {
	{ 229.0f, 240.0f, 255.0f },   /* white */
	{ 188.0f, 198.0f, 212.0f },   /* grey */
	{ 255.0f, 77.0f, 77.0f },     /* team red */
	{ 89.0f, 140.0f, 255.0f },    /* team blue */
	{ 229.0f, 240.0f, 255.0f },   /* me (each view its own: ae_score_strip_me_color) */
	{ 232.0f, 236.0f, 242.0f },   /* clock */
};
static const real line_rgb[3] = { 178.0f / 255.0f, 224.0f / 255.0f, 1.0f };   /* CE's HUD line */
static const real dark_rgb[3] = { 6.0f / 255.0f, 10.0f / 255.0f, 18.0f / 255.0f };
static const real flash_rgb[3] = { 245.0f / 255.0f, 250.0f / 255.0f, 1.0f };

/* each view's state from frame to frame */
static struct strip_view
{
	boolean unit_hud_drawn;     /* this frame (hud.c) */
	boolean visible;            /* last drawn */
	boolean primed;
	real shared_line_y;
	real alpha;                 /* last drawn */
	struct ae_strip_box clock, left_score, played; /* last drawn (the debug log's) */
	real extent[4];             /* last drawn: left, top, right, bottom (the waypoints' fade) */
	wchar_t scores[2][AE_STRIP_TEXT_LENGTH];
	unsigned long score_pop[2];
	real fill[2];
	unsigned long fill_time;
	short leader;               /* -1 left, 1 right, 0 none */
	unsigned long lead_pop;
	enum ae_strip_flag_state flags[2];
	unsigned long flag_flash[2];
	long clock_second;
	unsigned long clock_tick;
	boolean defusing;           /* (Bomb) the attackers' "BOMB DEFUSING" last drawn */
	unsigned long defusing_pop;
} views[MAXIMUM_LOCAL_PLAYERS];

static boolean valid_view(short local_player_index)
{
	return (boolean)(local_player_index >= 0 && local_player_index < MAXIMUM_LOCAL_PLAYERS);
}

void ae_score_strip_unit_hud_drawn(short local_player_index)
{
	if (valid_view(local_player_index))
		views[local_player_index].unit_hud_drawn = TRUE;
}

boolean ae_score_strip_visible(short local_player_index)
{
	return valid_view(local_player_index) && views[local_player_index].visible;
}

real ae_score_strip_last_line(short local_player_index)
{
	return valid_view(local_player_index) ? views[local_player_index].shared_line_y : 0.0f;
}

boolean ae_score_strip_last_drawn(short local_player_index, real *alpha, real clock[4], real left_score[4])
{
	struct strip_view const *state;

	if (!valid_view(local_player_index) || !views[local_player_index].visible)
		return FALSE;
	state = &views[local_player_index];
	*alpha = state->alpha;
	clock[0] = state->clock.x; clock[1] = state->clock.y; clock[2] = state->clock.w; clock[3] = state->clock.h;
	left_score[0] = state->left_score.x; left_score[1] = state->left_score.y;
	left_score[2] = state->left_score.w; left_score[3] = state->left_score.h;
	return TRUE;
}

boolean ae_score_strip_last_played(short local_player_index, real played[4])
{
	struct strip_view const *state;

	if (!valid_view(local_player_index) || !views[local_player_index].visible ||
		!views[local_player_index].played.visible)
		return FALSE;
	state = &views[local_player_index];
	played[0] = state->played.x; played[1] = state->played.y; played[2] = state->played.w; played[3] = state->played.h;
	return TRUE;
}

/* ---------- the view's geometry */

/* the view's factor (hud_item_timers.c's: half way between 1 and the square root of its share of the screen: a
half screen 0.85, a quarter 0.75) */
static real view_factor(void)
{
	/* (the view's share of the drawable, both in screen pixels: the rasterizer's frame bounds read 0.70 of the
	screen for a half view, which gave 2p 0.92 and 4p 0.79 where the spec's chain is 0.85 and 0.75) */
	int drawable_width = 0, drawable_height = 0;
	real ppu = ae_hud_pixels_per_unit();
	real width = (real)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0) * ppu;
	real height = (real)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0) * ppu;
	real share;

	platform_video_drawable_size(&drawable_width, &drawable_height);
	if (drawable_width <= 0 || drawable_height <= 0 || width <= 0.0f || height <= 0.0f)
		return 1.0f;
	share = (width * height) / ((real)drawable_width * (real)drawable_height);
	share = share < 0.0f ? 0.0f : share > 1.0f ? 1.0f : share;
	return 0.5f + 0.5f * (real)sqrt(share);
}

real ae_score_strip_view_scale(void)
{
	int drawable_width = 0, drawable_height = 0;

	platform_video_drawable_size(&drawable_width, &drawable_height);
	return view_factor() * (drawable_height > 0 ? (real)drawable_height / 1080.0f : 1.0f);
}

real ae_score_strip_shared_line_y(short local_player_index)
{
	short inset = 0;
	long baseline = 0;

	if (valid_view(local_player_index) && local_player_index != render.local_player_index)
		return views[local_player_index].shared_line_y;
	hud_item_timers_ae_place(&inset, &baseline);
	return (real)baseline * ae_hud_pixels_per_unit();
}

/* the geometry the layout takes, in the view's screen pixels: the HUD area's centre, the shared bottom line (the
motion sensor's foot, 11.12), the right-hand column */
static void view_geometry(short view, struct ae_strip_geometry *geometry)
{
	real ppu = ae_hud_pixels_per_unit();
	short width = (short)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0);
	short height = (short)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0);
	short area_left = 0, area_right = 0, inset = 0;
	long baseline = 0;

	hud_area_insets(&area_left, &area_right);
	hud_item_timers_ae_place(&inset, &baseline);
	geometry->view_left = 0.0f;
	geometry->view_top = 0.0f;
	geometry->view_width = (real)width * ppu;
	geometry->view_height = (real)height * ppu;
	geometry->s = ae_score_strip_view_scale();
	geometry->shared_line_y = (real)baseline * ppu;
	/* (the timers list's left edge, drawn just before the strip: ae_power_list.c; the view's right without one) */
	geometry->list_left_x = ae_power_list_left_x(view);
	geometry->centre_x = ((real)area_left + ((real)width - (real)area_right)) / 2.0f * ppu;
}

static real measure_digits(const wchar_t *text, real cap_px)
{
	return ae_hud_digits_measure(text, cap_px);
}

static real measure_text(const wchar_t *text, real px)
{
	return ae_hud_text_measure(text, px);
}

/* ---------- colours */

static void set_rgb(real const rgb[3], real alpha, real_argb_color *color)
{
	color->alpha = alpha;
	color->red = rgb[0];
	color->green = rgb[1];
	color->blue = rgb[2];
}

static void role_color(enum ae_strip_color role, struct ae_strip_rgb const *me, real alpha, real_argb_color *color)
{
	long index = (long)role >= 0 && (long)role < (long)NUMBEROF(color_values) ? (long)role : 0;

	color->alpha = alpha;
	if (role == _ae_strip_color_me && me)
	{
		color->red = me->red;
		color->green = me->green;
		color->blue = me->blue;
		return;
	}
	color->red = color_values[index][0] / 255.0f;
	color->green = color_values[index][1] / 255.0f;
	color->blue = color_values[index][2] / 255.0f;
}

/* ---------- motion */

static unsigned long stamp(unsigned long now)
{
	return now ? now : 1;
}

/* a pop's scale: up to peak in 80 ms, back to 1 in 170 ms (ease-out) */
static real pop_scale(unsigned long started, unsigned long now, real peak)
{
	unsigned long t = now - started;

	if (!started || t >= 250)
		return 1.0f;
	if (t < 80)
		return 1.0f + (peak - 1.0f) * (real)t / 80.0f;
	{
		real u = (real)(t - 80) / 170.0f;

		return 1.0f + (peak - 1.0f) * (1.0f - u) * (1.0f - u);
	}
}

/* white twice in 600 ms (CE's message flash): on, off, on, off */
static boolean flash_on(unsigned long started, unsigned long now)
{
	unsigned long t = now - started;

	return (boolean)(started && t < 600 && (t < 150 || (t >= 300 && t < 450)));
}

/* ---------- pieces */

/* a strip text in its characters' colours, from x, at px (white while it flashes) */
static void draw_runs(short view, struct ae_strip_text const *text, real x, real baseline_y, real px,
	struct ae_strip_rgb const *me, real alpha, boolean flash)
{
	wchar_t run[AE_STRIP_TEXT_LENGTH];
	wchar_t prefix[AE_STRIP_TEXT_LENGTH];
	long start = 0;

	while (start < AE_STRIP_TEXT_LENGTH && text->text[start])
	{
		long end = start;
		real_argb_color color;

		/* (room for the terminator, should a text fill its array unterminated) */
		while (end < AE_STRIP_TEXT_LENGTH - 1 && text->text[end] && text->colors[end] == text->colors[start])
			end++;
		if (end == start)
			break;
		memcpy(run, text->text + start, (size_t)(end - start) * sizeof(wchar_t));
		run[end - start] = 0;
		memcpy(prefix, text->text, (size_t)start * sizeof(wchar_t));
		prefix[start] = 0;
		role_color(flash ? _ae_strip_color_white : (enum ae_strip_color)text->colors[start], me, alpha, &color);
		ae_hud_text_draw(view, x + (start ? ae_hud_text_measure(prefix, px) : 0.0f), baseline_y, run, px, &color,
			TRUE);
		start = end;
	}
}

/* a centred line (the context, the lead, the goal, a small clock) */
static void draw_centred(short view, struct ae_strip_text const *text, struct ae_strip_box const *box,
	struct ae_strip_rgb const *me, real alpha, real scale)
{
	real px = box->h * scale;

	if (box->visible)
		draw_runs(view, text, box->x + box->w / 2.0f - ae_hud_text_measure(text->text, px) / 2.0f, box->y + box->h,
			px, me, alpha, FALSE);
}

/* a score: counter digits (with the glow) about its box's middle, a place's letters 2 px after them, or a name */
static void draw_score(short view, struct ae_strip_text const *score, struct ae_strip_box const *box, real k, real s,
	real pop, struct ae_strip_rgb const *me, real alpha)
{
	wchar_t digits[AE_STRIP_TEXT_LENGTH];
	long count = 0;
	real_argb_color color;
	real baseline = box->y + box->h;

	if (!box->visible)
		return;
	role_color(score->color, me, alpha, &color);
	while (count < AE_STRIP_TEXT_LENGTH - 1 && score->text[count] &&
		((score->text[count] >= L'0' && score->text[count] <= L'9') || score->text[count] == L':' ||
		score->text[count] == L'-'))
	{
		digits[count] = score->text[count];
		count++;
	}
	digits[count] = 0;
	if (!count)
	{
		draw_runs(view, score, box->x, baseline, box->h, me, alpha, FALSE);
		return;
	}
	{
		real width = ae_hud_digits_measure(digits, box->h);
		real cap = box->h * pop;

		ae_hud_digits_draw(view, box->x + (width - ae_hud_digits_measure(digits, cap)) / 2.0f, baseline, digits,
			cap, &color, AE_STRIP_GLOW_ALPHA * alpha);
		if (score->text[count])
			ae_hud_text_draw(view, box->x + width + 2.0f * k, baseline, score->text + count,
				ae_strip_text_px(AE_STRIP_SUFFIX_PX, k, s), &color, TRUE);
	}
}

/* a parallelogram's outline: the dark under-stroke, then CE's line */
static void outline4(real const points[4][2], real k, real alpha)
{
	real_argb_color color;
	short i;

	set_rgb(dark_rgb, 0.78f * alpha, &color);
	for (i = 0; i < 4; i++)
		ae_hud_line(points[i][0], points[i][1], points[(i + 1) % 4][0], points[(i + 1) % 4][1], 3.6f * k, &color);
	set_rgb(line_rgb, 0.85f * alpha, &color);
	for (i = 0; i < 4; i++)
		ae_hud_line(points[i][0], points[i][1], points[(i + 1) % 4][0], points[(i + 1) % 4][1], 1.6f * k, &color);
}

/* a bar (11.3): the lean parallelogram over the box's bottom span ('/' left, '\' right), the dark track, the
side's fill from the outer end toward the clock, 5 cells, the outline; the fill's edge flashing white */
static void draw_bar(struct ae_strip_box const *box, boolean left, real fill, real k, enum ae_strip_color role,
	struct ae_strip_rgb const *me, real alpha, boolean flash)
{
	real lean = AE_STRIP_LEAN * box->h * (left ? 1.0f : -1.0f);
	real x0 = box->x, x1 = box->x + box->w, top = box->y, bottom = box->y + box->h;
	real track[4][2];
	real_argb_color color;
	short cell;

	track[0][0] = x0 + lean; track[0][1] = top;
	track[1][0] = x1 + lean; track[1][1] = top;
	track[2][0] = x1; track[2][1] = bottom;
	track[3][0] = x0; track[3][1] = bottom;
	set_rgb(dark_rgb, 0.35f * alpha, &color);
	ae_hud_fill_quad_color(track, &color);
	fill = fill < 0.0f ? 0.0f : fill > 1.0f ? 1.0f : fill;
	if (fill > 0.0f)
	{
		real inner = left ? x0 + box->w * fill : x1 - box->w * fill;
		real a = left ? x0 : inner, b = left ? inner : x1;
		real points[4][2];

		points[0][0] = a + lean; points[0][1] = top;
		points[1][0] = b + lean; points[1][1] = top;
		points[2][0] = b; points[2][1] = bottom;
		points[3][0] = a; points[3][1] = bottom;
		role_color(role, me, 0.92f * alpha, &color);
		ae_hud_fill_quad_color(points, &color);
		if (flash)
		{
			set_rgb(flash_rgb, alpha, &color);
			ae_hud_line(inner + lean, top - 2.0f * k, inner, bottom + 2.0f * k, 3.0f * k, &color);
		}
	}
	set_rgb(dark_rgb, 0.59f * alpha, &color);
	for (cell = 1; cell < 5; cell++)
	{
		real x = x0 + box->w * (real)cell / 5.0f;

		ae_hud_line(x + lean, top, x, bottom, 1.6f * k, &color);
	}
	outline4((real const (*)[2])track, k, alpha);
}

/* round mode: one cell per player in the bar's place, lit (from the clock side) while alive */
static void draw_cells(struct ae_strip_box const *box, boolean left, long cells, long lit, real k,
	enum ae_strip_color role, struct ae_strip_rgb const *me, real alpha)
{
	real width = cells > 0 ? box->w / (real)cells : 0.0f;
	real lean = AE_STRIP_LEAN * box->h * (left ? 1.0f : -1.0f);
	long cell;

	for (cell = 0; cell < cells; cell++)
	{
		long from_clock = left ? cells - 1 - cell : cell;
		real a = box->x + width * (real)cell + 1.5f * k, b = box->x + width * (real)(cell + 1) - 1.5f * k;
		real points[4][2];
		real_argb_color color;

		points[0][0] = a + lean; points[0][1] = box->y;
		points[1][0] = b + lean; points[1][1] = box->y;
		points[2][0] = b; points[2][1] = box->y + box->h;
		points[3][0] = a; points[3][1] = box->y + box->h;
		if (from_clock < lit)
			role_color(role, me, 0.92f * alpha, &color);
		else
			set_rgb(dark_rgb, 0.35f * alpha, &color);
		ae_hud_fill_quad_color(points, &color);
		outline4((real const (*)[2])points, k, alpha);
	}
}

/* rounds to win: slanted pips; won = lit in the side's colour (filling from the clock side), to play = dark with a
faint outline */
static void draw_pips(struct ae_strip_box const *box, boolean left, long total, long lit, real k,
	enum ae_strip_color role, struct ae_strip_rgb const *me, real alpha)
{
	long pip;

	for (pip = 0; pip < total; pip++)
	{
		boolean won = (boolean)(left ? (total - 1 - pip) < lit : pip < lit);
		real cx = box->x + (AE_STRIP_PIP_WIDTH / 2.0f + AE_STRIP_PIP_PITCH * (real)pip) * k;
		real half = AE_STRIP_PIP_WIDTH / 2.0f * k, lean = AE_STRIP_LEAN * box->h / 2.0f;
		real points[4][2];
		real_argb_color color;

		points[0][0] = cx - half + lean; points[0][1] = box->y;
		points[1][0] = cx + half + lean; points[1][1] = box->y;
		points[2][0] = cx + half - lean; points[2][1] = box->y + box->h;
		points[3][0] = cx - half - lean; points[3][1] = box->y + box->h;
		set_rgb(dark_rgb, 0.86f * alpha, &color);
		ae_hud_fill_quad_color(points, &color);
		if (won)
		{
			role_color(role, me, alpha, &color);
			ae_hud_fill_quad_color(points, &color);
		}
		else
		{
			role_color(_ae_strip_color_grey, me, 0.59f * alpha, &color);
			ae_hud_line(points[0][0], points[0][1], points[1][0], points[1][1], 1.2f * k, &color);
			ae_hud_line(points[1][0], points[1][1], points[2][0], points[2][1], 1.2f * k, &color);
			ae_hud_line(points[2][0], points[2][1], points[3][0], points[3][1], 1.2f * k, &color);
			ae_hud_line(points[3][0], points[3][1], points[0][0], points[0][1], 1.2f * k, &color);
		}
	}
}

/* the my-side marker (11.1): CE's teammate triangle shape, outline only, grey at 80 %, its tip down */
static void draw_marker(struct ae_strip_box const *box, real k, real alpha)
{
	real cx = box->x + box->w / 2.0f, top = box->y, tip = box->y + box->h;
	real points[4][2];
	real_argb_color color;

	points[0][0] = box->x; points[0][1] = top;
	points[1][0] = box->x + box->w; points[1][1] = top;
	points[2][0] = cx; points[2][1] = tip;
	points[3][0] = cx; points[3][1] = tip;
	set_rgb(dark_rgb, 0.59f * 0.8f * alpha, &color);
	ae_hud_fill_quad_color(points, &color);
	role_color(_ae_strip_color_grey, NULL, 0.8f * alpha, &color);
	ae_hud_line(box->x, top, box->x + box->w, top, 1.8f * k, &color);
	ae_hud_line(box->x + box->w, top, cx, tip, 1.8f * k, &color);
	ae_hud_line(cx, tip, box->x, top, 1.8f * k, &color);
}

/* CTF's flag glyph (mock4.py flag_icon, our own): a pole and a swallow-tail pennant leaning like the HUD, in the
team's colour over a dark stroke; (a, b) = along, up from the foot of the pole */
static void flag_point(real x, real y, real a, real b, real const lean, real point[2])
{
	point[0] = x + a + b * lean;
	point[1] = y - b;
}

static void draw_flag(real x, real baseline_y, real px, real k, real_argb_color const *color, real alpha)
{
	real h = px * 0.74f, w = h * 0.72f, lean = AE_STRIP_LEAN * 0.35f;
	real pole_x = x + w * 0.1f;
	real top[2], foot[2], upper[4][2], lower[4][2];
	real_argb_color dark;

	flag_point(pole_x, baseline_y, 0.0f, 0.0f, lean, foot);
	flag_point(pole_x, baseline_y, 0.0f, h, lean, top);
	flag_point(pole_x, baseline_y, 0.0f, h, lean, upper[0]);
	flag_point(pole_x, baseline_y, w, h * 0.80f, lean, upper[1]);
	flag_point(pole_x, baseline_y, w * 0.62f, h * 0.62f, lean, upper[2]);
	flag_point(pole_x, baseline_y, 0.0f, h * 0.62f, lean, upper[3]);
	flag_point(pole_x, baseline_y, 0.0f, h * 0.62f, lean, lower[0]);
	flag_point(pole_x, baseline_y, w * 0.62f, h * 0.62f, lean, lower[1]);
	flag_point(pole_x, baseline_y, w, h * 0.44f, lean, lower[2]);
	flag_point(pole_x, baseline_y, 0.0f, h * 0.40f, lean, lower[3]);
	set_rgb(dark_rgb, 0.88f * alpha, &dark);
	ae_hud_line(foot[0], foot[1], top[0], top[1], 4.2f * k, &dark);
	ae_hud_fill_quad_color(upper, &dark);
	ae_hud_fill_quad_color(lower, &dark);
	ae_hud_line(foot[0], foot[1], top[0], top[1], 2.0f * k, color);
	ae_hud_fill_quad_color(upper, color);
	ae_hud_fill_quad_color(lower, color);
}

/* a sub-line; CTF's with its flag glyph first, in the flag's team's colour */
static void draw_sub(short view, struct ae_strip_model const *model, struct ae_strip_text const *sub,
	struct ae_strip_box const *box, enum ae_strip_color team, real k, struct ae_strip_rgb const *me, real alpha,
	boolean flash)
{
	real x = box->x;

	if (!box->visible)
		return;
	if (model->flag_glyphs)
	{
		real_argb_color color;

		role_color(flash ? _ae_strip_color_white : team, me, alpha, &color);
		draw_flag(x, box->y + box->h, box->h, k, &color, alpha);
		x += (AE_STRIP_FLAG_GLYPH + AE_STRIP_FLAG_GAP) * box->h;
	}
	draw_runs(view, sub, x, box->y + box->h, box->h, me, alpha, flash);
}

/* ---------- the strip */

/* this view's YOU colour (11.4): my armour's, lifted */
static void me_color(short view, struct ae_strip_rgb *me)
{
	union real_rgb_color armour;
	struct ae_strip_rgb rgb;
	long player_index = local_player_get_player_index(view);

	me->red = color_values[0][0] / 255.0f;
	me->green = color_values[0][1] / 255.0f;
	me->blue = color_values[0][2] / 255.0f;
	if (player_index != NONE && game_engine_player_get_change_color(&armour, player_index))
	{
		rgb.red = armour.red;
		rgb.green = armour.green;
		rgb.blue = armour.blue;
		ae_score_strip_me_color(&rgb, me);
	}
}

/* what changed since the last frame: pops, flashes, the bars' easing */
static void update_motion(struct strip_view *state, struct ae_strip_snapshot const *snapshot,
	struct ae_strip_model const *model, unsigned long now)
{
	short side;
	short leader;
	real step;

	if (!state->primed)
	{
		state->primed = TRUE;
		state->fill[0] = model->left_fill;
		state->fill[1] = model->right_fill;
		state->flags[0] = snapshot->me.flag;
		state->flags[1] = snapshot->other.flag;
		memcpy(state->scores[0], model->left_score.text, sizeof(state->scores[0]));
		memcpy(state->scores[1], model->right_score.text, sizeof(state->scores[1]));
		state->fill_time = now;
		state->clock_second = -1;
	}
	for (side = 0; side < 2; side++)
	{
		struct ae_strip_text const *score = side ? &model->right_score : &model->left_score;
		enum ae_strip_flag_state flag = side ? snapshot->other.flag : snapshot->me.flag;

		if (memcmp(state->scores[side], score->text, sizeof(state->scores[side])) != 0)
		{
			memcpy(state->scores[side], score->text, sizeof(state->scores[side]));
			if (!model->clock_small)
				state->score_pop[side] = stamp(now);
		}
		if (model->flag_glyphs && flag != state->flags[side])
		{
			state->flags[side] = flag;
			state->flag_flash[side] = stamp(now);
		}
	}
	leader = (short)(snapshot->me.score > snapshot->other.score ? -1 : snapshot->me.score < snapshot->other.score ? 1 :
		0);
	if (leader != state->leader && leader != 0)
		state->lead_pop = stamp(now);
	state->leader = leader;
	step = (real)(now - state->fill_time) / 250.0f;
	step = step > 1.0f ? 1.0f : step < 0.0f ? 0.0f : step;
	state->fill[0] += (model->left_fill - state->fill[0]) * step;
	state->fill[1] += (model->right_fill - state->fill[1]) * step;
	state->fill_time = now;
}

static void draw_clock(short view, struct strip_view *state, struct ae_strip_snapshot const *snapshot,
	struct ae_strip_model const *model, struct ae_strip_layout const *layout, struct ae_strip_rgb const *me,
	real alpha, unsigned long now)
{
	real_argb_color color;
	real scale = 1.0f;
	boolean hot = (boolean)(!snapshot->clock_counts_up && snapshot->clock_seconds <= 10);

	if (!layout->clock.visible)
		return;
	if (model->clock_small)
	{
		draw_centred(view, &model->clock, &layout->clock, me, alpha, 1.0f);
		return;
	}
	if (hot && snapshot->clock_seconds != state->clock_second)
		state->clock_tick = stamp(now);
	state->clock_second = snapshot->clock_seconds;
	if (hot)
	{
		scale = pop_scale(state->clock_tick, now, 1.08f);
		color.alpha = alpha;
		color.red = color.green = color.blue = 1.0f;
	}
	else
		role_color(_ae_strip_color_clock, me, alpha, &color);
	{
		real cap = layout->clock.h * scale;
		real width = ae_hud_digits_measure(model->clock.text, cap);

		ae_hud_digits_draw(view, layout->clock.x + layout->clock.w / 2.0f - width / 2.0f,
			layout->clock.y + layout->clock.h, model->clock.text, cap, &color, AE_STRIP_GLOW_ALPHA * alpha);
	}
}

/* (0.3.9) Bomb's PLANTING / DEFUSING bar (mockups ~/Pictures/AE-HUD-review/bomb-hud): where Chupa's arming meter was,
centred a sixth of the view under its middle; the strip's bar language (the lean track, 5 cells, the dark and
CE-line outline), filled from the left in the viewer's team colour, a white label 8 px over it on the strip's soft
backing. Sizes at 1080p x the view's factor */
#define AE_BOMB_BAR_WIDTH 280.0f
#define AE_BOMB_BAR_HEIGHT 10.0f
#define AE_BOMB_BAR_LABEL_PX 18.0f
#define AE_BOMB_BAR_LABEL_GAP 8.0f

static void draw_bomb_bar(short view, struct ae_strip_geometry const *geometry, enum ae_strip_color role,
	struct ae_strip_rgb const *me, real alpha)
{
	real fraction, s = geometry->s;
	boolean defusing;
	wchar_t const *label;
	real cx, cy, w, h, lean, px, x0, x1, top, bottom;
	real track[4][2];
	real_argb_color color;
	short cell;

	if (!ae_strip_engine_bomb_progress(view, &fraction, &defusing))
		return;
	label = defusing ? L"DEFUSING" : L"PLANTING";
	cx = geometry->view_left + geometry->view_width / 2.0f;
	cy = geometry->view_top + geometry->view_height / 2.0f + geometry->view_height / 6.0f;
	w = AE_BOMB_BAR_WIDTH * s;
	h = AE_BOMB_BAR_HEIGHT * s;
	lean = AE_STRIP_LEAN * h;
	px = ae_strip_text_px(AE_BOMB_BAR_LABEL_PX, 1.0f, s);
	x0 = cx - w / 2.0f; x1 = cx + w / 2.0f; top = cy - h / 2.0f; bottom = cy + h / 2.0f;
	ae_hud_backing_draw(view, cx, top - AE_BOMB_BAR_LABEL_GAP * s, w * 0.62f, 34.0f * s, AE_STRIP_BACKING_ALPHA * alpha);
	track[0][0] = x0 + lean; track[0][1] = top;
	track[1][0] = x1 + lean; track[1][1] = top;
	track[2][0] = x1; track[2][1] = bottom;
	track[3][0] = x0; track[3][1] = bottom;
	set_rgb(dark_rgb, 0.35f * alpha, &color);
	ae_hud_fill_quad_color(track, &color);
	fraction = fraction < 0.0f ? 0.0f : fraction > 1.0f ? 1.0f : fraction;
	if (fraction > 0.0f)
	{
		real inner = x0 + w * fraction;
		real points[4][2];

		points[0][0] = x0 + lean; points[0][1] = top;
		points[1][0] = inner + lean; points[1][1] = top;
		points[2][0] = inner; points[2][1] = bottom;
		points[3][0] = x0; points[3][1] = bottom;
		role_color(role, me, 0.92f * alpha, &color);
		ae_hud_fill_quad_color(points, &color);
	}
	set_rgb(dark_rgb, 0.59f * alpha, &color);
	for (cell = 1; cell < 5; cell++)
	{
		real x = x0 + w * (real)cell / 5.0f;

		ae_hud_line(x + lean, top, x, bottom, 1.6f * s, &color);
	}
	outline4((real const (*)[2])track, s, alpha);
	role_color(_ae_strip_color_white, me, alpha, &color);
	ae_hud_text_draw(view, cx - ae_hud_text_measure(label, px) / 2.0f, top - AE_BOMB_BAR_LABEL_GAP * s, label, px,
		&color, TRUE);
}

static void draw_strip(short view)
{
	struct strip_view *state = &views[view];
	struct ae_strip_snapshot snapshot;
	struct ae_strip_model model;
	struct ae_strip_geometry geometry;
	struct ae_strip_layout layout;
	struct ae_strip_rgb me;
	real alpha, k;
	unsigned long now = system_milliseconds();
	boolean unit_hud = state->unit_hud_drawn;

	state->unit_hud_drawn = FALSE;
	state->visible = FALSE;
	if (!ae_hud_style_is_ae() || !unit_hud || !game_engine_running() || cinematic_in_progress() ||
		game_engine_game_over() || game_engine_pregame_countdown_ticks_left() > 0)
	{
		state->primed = FALSE;
		return;
	}
	if (!ae_strip_engine_snapshot(view, &snapshot))
	{
		state->primed = FALSE;
		return;
	}
	ae_strip_model_build(&snapshot, &model);
	view_geometry(view, &geometry);
	ae_strip_layout_compute(&model, &geometry, measure_digits, measure_text, &layout);
	state->shared_line_y = geometry.shared_line_y;
	k = layout.strip_scale;
	alpha = 1.0f - AE_STRIP_SCOREBOARD_FADE * game_engine_scoreboard_shown(view);
	if (alpha <= 0.0f || k <= 0.0f)
		return;
	me_color(view, &me);
	update_motion(state, &snapshot, &model, now);

	ae_hud_backing_draw(view, layout.backing_cx, layout.backing_cy, layout.backing_rx, layout.backing_ry,
		AE_STRIP_BACKING_ALPHA * alpha);
	if (model.bars)
	{
		draw_bar(&layout.left_bar, TRUE, state->fill[0], k, model.left_score.color, &me, alpha,
			(boolean)(state->score_pop[0] && now - state->score_pop[0] < 250));
		draw_bar(&layout.right_bar, FALSE, state->fill[1], k, model.right_score.color, &me, alpha,
			(boolean)(state->score_pop[1] && now - state->score_pop[1] < 250));
	}
	if (model.round_cells)
	{
		draw_cells(&layout.left_cells, TRUE, model.left_cells, model.left_lit, k, model.left_score.color, &me, alpha);
		draw_cells(&layout.right_cells, FALSE, model.right_cells, model.right_lit, k, model.right_score.color, &me,
			alpha);
	}
	if (layout.left_pips.visible)
	{
		draw_pips(&layout.left_pips, TRUE, model.pips_total, model.left_pips_lit, k, model.left_score.color, &me,
			alpha);
		draw_pips(&layout.right_pips, FALSE, model.pips_total, model.right_pips_lit, k, model.right_score.color, &me,
			alpha);
	}
	if (layout.marker.visible)
		draw_marker(&layout.marker, k, alpha);
	draw_score(view, &model.left_score, &layout.left_score, k, geometry.s, pop_scale(state->score_pop[0], now, 1.2f) *
		(state->leader < 0 ? pop_scale(state->lead_pop, now, 1.1f) : 1.0f), &me, alpha);
	draw_score(view, &model.right_score, &layout.right_score, k, geometry.s, pop_scale(state->score_pop[1], now, 1.2f) *
		(state->leader > 0 ? pop_scale(state->lead_pop, now, 1.1f) : 1.0f), &me, alpha);
	if (layout.left_alive.visible)
		draw_runs(view, &model.left_alive, layout.left_alive.x, layout.left_alive.y + layout.left_alive.h,
			layout.left_alive.h, &me, alpha, FALSE);
	if (layout.right_alive.visible)
		draw_runs(view, &model.right_alive, layout.right_alive.x, layout.right_alive.y + layout.right_alive.h,
			layout.right_alive.h, &me, alpha, FALSE);
	draw_clock(view, state, &snapshot, &model, &layout, &me, alpha, now);
	/* (Bomb: the attackers' DEFUSING warning pops as it starts) */
	if (snapshot.bomb_defusing && !state->defusing)
		state->defusing_pop = stamp(now);
	state->defusing = snapshot.bomb_defusing;
	draw_centred(view, &model.context, &layout.context, &me, alpha,
		snapshot.bomb_defusing ? pop_scale(state->defusing_pop, now, 1.2f) : 1.0f);
	draw_centred(view, &model.lead, &layout.lead, &me, alpha, pop_scale(state->lead_pop, now, 1.15f));
	draw_centred(view, &model.goal, &layout.goal, &me, AE_STRIP_GOAL_ALPHA * alpha, 1.0f);
	draw_centred(view, &model.played, &layout.played, &me, AE_STRIP_GOAL_ALPHA * alpha, 1.0f);
	draw_sub(view, &model, &model.left_sub, &layout.left_sub, model.left_score.color, k, &me, alpha,
		flash_on(state->flag_flash[0], now));
	draw_sub(view, &model, &model.right_sub, &layout.right_sub, model.right_score.color, k, &me, alpha,
		flash_on(state->flag_flash[1], now));
	state->alpha = alpha;
	state->clock = layout.clock;
	state->left_score = layout.left_score;
	state->played = layout.played;
	state->extent[0] = layout.left_end_x;
	state->extent[1] = MIN(layout.clock.y, layout.left_score.y);
	state->extent[2] = layout.right_end_x;
	state->extent[3] = state->shared_line_y + 4.0f;
	/* (the lines above the scores too, when shown: the context and lead line, the marker) */
	{
		struct ae_strip_box const *above[4];
		short i;

		above[0] = &layout.context;
		above[1] = &layout.lead;
		above[2] = &layout.marker;
		above[3] = &layout.played;
		for (i = 0; i < 4; i++)
			if (above[i]->visible && above[i]->w > 0.0f && above[i]->h > 0.0f)
			{
				state->extent[0] = MIN(state->extent[0], above[i]->x);
				state->extent[1] = MIN(state->extent[1], above[i]->y);
				state->extent[2] = MAX(state->extent[2], above[i]->x + above[i]->w);
			}
	}
	state->visible = TRUE;
	draw_bomb_bar(view, &geometry, model.left_score.color, &me, alpha);
}

boolean ae_score_strip_extent(short local_player_index, real extent[4])
{
	if (!valid_view(local_player_index) || !views[local_player_index].visible)
		return FALSE;
	extent[0] = views[local_player_index].extent[0];
	extent[1] = views[local_player_index].extent[1];
	extent[2] = views[local_player_index].extent[2];
	extent[3] = views[local_player_index].extent[3];
	return TRUE;
}

void ae_score_strip_draw(short local_player_index)
{
	ae_hud_digits_debug_draw(local_player_index);
	ae_panel_draw_debug(local_player_index);
	ae_panel_engine_debug_log(local_player_index);
	if (valid_view(local_player_index))
		draw_strip(local_player_index);
}
