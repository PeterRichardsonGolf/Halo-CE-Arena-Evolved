/*
AE_POSTGAME.C

Arena Evolved: the confirm over the carnage report, the game's side
(ae_postgame.h). game_engine.c hands it the carnage report's buttons (as
the stock code reads them: held on any pad) and its prompt's place; it asks
"LEAVE THE GAME?" over the carnage report in the report's own font, with the
map's button icons (the PlayStation ones while a PlayStation pad is the last
used; Custom Edition maps': ae_buttons_tags.c), and leaves as the stock B
did (network_game_abort) only on A.
*/

#include "cseries.h"
#include "cseries_windows.h"
#include "interface/ui_widget.h"
#include "math/integer_math.h"
#include "math/real_math.h"
#include "networking/network_game_globals.h"
#include "rasterizer/rasterizer.h"
#include "text/draw_string.h"

#include "ae_hud_digits.h"
#include "ae_hud_text.h"
#include "ae_postgame.h"

void platform_log(char const *format, ...);

/* ---------- constants */

enum
{
	/* a gap this long (ms) between the carnage report's frames: another one (the confirm starts closed) */
	POSTGAME_GAP_MS = 500,
	/* the prompt's place in the carnage report's 640 by 480 (game_engine.c) */
	PROMPT_X = 70,
	PROMPT_Y = 410,
	/* the confirm's box in it */
	BOX_LEFT = 160,
	BOX_TOP = 186,
	BOX_RIGHT = 480,
	BOX_BOTTOM = 294,
};

/* ---------- globals */

static struct ae_postgame_confirm confirm;
static unsigned long last_frame_ms;
static boolean have_frame;

/* ---------- private code */

static void set_color(real alpha, real red, real green, real blue, real_argb_color *color)
{
	color->alpha = alpha;
	color->red = red;
	color->green = green;
	color->blue = blue;
}

/* a rectangle of the carnage report's units, filled */
static void fill(real ox, real oy, real x0, real y0, real x1, real y1, real_argb_color const *color)
{
	real ppu = ae_hud_pixels_per_unit();
	real points[4][2];

	points[0][0] = (ox + x0) * ppu; points[0][1] = (oy + y0) * ppu;
	points[1][0] = (ox + x1) * ppu; points[1][1] = (oy + y0) * ppu;
	points[2][0] = (ox + x1) * ppu; points[2][1] = (oy + y1) * ppu;
	points[3][0] = (ox + x0) * ppu; points[3][1] = (oy + y1) * ppu;
	ae_hud_fill_quad_color(points, color);
}

static void text_rect(short ox, short oy, short x0, short y0, short x1, short y1, rectangle2d *rect)
{
	rect->x0 = (short)(ox + x0);
	rect->y0 = (short)(oy + y0);
	rect->x1 = (short)(ox + x1);
	rect->y1 = (short)(oy + y1);
}

/* ---------- public code */

boolean ae_postgame_buttons(
	boolean accept_down,
	boolean back_down)
{
	unsigned long now = system_milliseconds();
	boolean was_open;

	/* (a new carnage report: the confirm closed, nothing waited for) */
	if (!have_frame || now - last_frame_ms > POSTGAME_GAP_MS)
		csmemset(&confirm, 0, sizeof(confirm));
	have_frame = TRUE;
	last_frame_ms = now;
	was_open = confirm.open;
	switch (ae_postgame_confirm_step(&confirm, accept_down, back_down))
	{
	case AE_POSTGAME_STOCK:
		return FALSE;
	case AE_POSTGAME_LEAVE:
		platform_log("postgame: leaving the game (A on the confirm)");
		network_game_abort();
		return TRUE;
	default:
		if (confirm.open && !was_open)
			platform_log("postgame: leave the game? asked over the carnage report");
		else if (!confirm.open && was_open)
			platform_log("postgame: stayed on the carnage report (B on the confirm)");
		return TRUE;
	}
}

boolean ae_postgame_confirm_draw(
	rectangle2d const *prompt_bounds)
{
	static wchar_t const *const buttons = L"%a-button =yes        %b-button =no";
	real_argb_color color;
	rectangle2d rect;
	short ox, oy;

	if (!confirm.open || !have_frame || system_milliseconds() - last_frame_ms > POSTGAME_GAP_MS)
		return FALSE;
	/* (the carnage report's origin, as its prompt is offset) */
	ox = (short)(prompt_bounds->x0 - PROMPT_X);
	oy = (short)(prompt_bounds->y0 - PROMPT_Y);
	/* the report dimmed, the box over it: dark, with the quit screen's blue edge */
	set_color(0.55f, 0.0f, 0.0f, 0.0f, &color);
	fill(ox, oy, 0.0f, 0.0f, 640.0f, 480.0f, &color);
	set_color(0.94f, 0.02f, 0.06f, 0.14f, &color);
	fill(ox, oy, BOX_LEFT, BOX_TOP, BOX_RIGHT, BOX_BOTTOM, &color);
	set_color(1.0f, 0.157f, 0.588f, 1.0f, &color);
	fill(ox, oy, BOX_LEFT, BOX_TOP, BOX_RIGHT, BOX_TOP + 2.0f, &color);
	fill(ox, oy, BOX_LEFT, BOX_BOTTOM - 2.0f, BOX_RIGHT, BOX_BOTTOM, &color);
	fill(ox, oy, BOX_LEFT, BOX_TOP, BOX_LEFT + 2.0f, BOX_BOTTOM, &color);
	fill(ox, oy, BOX_RIGHT - 2.0f, BOX_TOP, BOX_RIGHT, BOX_BOTTOM, &color);
	/* the question, what leaving does, and the buttons (the report's font, set by game_engine.c) */
	draw_string_set_format(NONE, _text_justification_center, 0);
	draw_string_set_color(&color);
	text_rect(ox, oy, BOX_LEFT, BOX_TOP + 14, BOX_RIGHT, BOX_TOP + 40, &rect);
	rasterizer_draw_unicode_string(&rect, NULL, NULL, 0, L"LEAVE THE GAME?");
	set_color(1.0f, 0.80f, 0.84f, 0.88f, &color);
	draw_string_set_color(&color);
	text_rect(ox, oy, BOX_LEFT, BOX_TOP + 42, BOX_RIGHT, BOX_TOP + 66, &rect);
	rasterizer_draw_unicode_string(&rect, NULL, NULL, 0,
		global_network_game_server_get() ? L"The game ends for everyone." : L"Back to the main menu.");
	draw_string_set_format(NONE, 0, 0);
	set_color(1.0f, 1.0f, 1.0f, 1.0f, &color);
	draw_string_set_color(&color);
	text_rect(ox, oy, BOX_LEFT + 72, BOX_TOP + 74, BOX_RIGHT, BOX_TOP + 100, &rect);
	draw_string_and_hack_in_icons(&rect, NULL, NULL, 0, buttons, FALSE);
	return TRUE;
}
