/*
AE_POSTGAME.C

Arena Evolved: the carnage report's buttons and its confirm, the game's side
(ae_postgame.h). game_engine.c hands the carnage report's buttons to it
(ae_postgame_buttons) and its prompt's place (ae_postgame_confirm_draw).

Only player 1 controls the carnage report (owner, 2026-10-15: "p1/host only"):
the first local player of this machine, whatever pad that is (in a game the
local player's gamepad is its index: player_control.c). The stock code took
any of the four pads. On the host, player 1's A picks the game, as the stock
A did (the server back to the pregame); B asks "LEAVE THE GAME?", answered by
player 1's A (leave, network_game_abort, as the stock B did) or B (stay). On a
guest machine the same player 1 rule holds for its own leaving (A picks
nothing there: no server, as before): the machine leaves as a whole, so one
player decides, as on the host.

The confirm's look (confirm_look): the game's own alert panel, the quit
screen's (the PC menus' bitmaps/alert_bkd, added to every map's tags with the
PC menus), else a plain box (no such tag: the Xbox menus). An AE-menus look
goes there later.
*/

#include "cseries.h"
#include "cseries_windows.h"
#include "bitmaps/bitmap_group.h"
#include "bitmaps/bitmap_group_lookup.h"
#include "bitmaps/bitmaps.h"
#include "game/players.h"
#include "input/input.h"
#include "interface/ui_widget.h"
#include "math/integer_math.h"
#include "math/real_math.h"
#include "networking/network_game_globals.h"
#include "networking/network_server_manager.h"
#include "rasterizer/rasterizer.h"
#include "tag_files/tag_groups.h"
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
	BOX_LEFT = 150,
	BOX_TOP = 165,
	BOX_RIGHT = 490,
	BOX_BOTTOM = 315,
	/* the gamepad buttons the stock code read (input.h's order): A, B, START, BACK */
	BUTTON_A = 0,
	BUTTON_B = 1,
	BUTTON_START = 12,
	BUTTON_BACK = 13,
};

/* the PC menus' alert panel (menu_tags.c names their bitmaps "pc\\" + their name) and the part of its picture the
panel is (956 by 636 texels of 1024: alert_bkd__0.png), with its title band and its button band (of 636) */
#define ALERT_BITMAP "pc\\bitmaps\\alert_bkd"
#define ALERT_WIDTH_FRACTION (956.0f / 1024.0f)
#define ALERT_HEIGHT_FRACTION (636.0f / 1024.0f)

/* the confirm's looks */
enum confirm_look
{
	/* the game's alert panel (the quit screen's), with the PC menus */
	CONFIRM_LOOK_ALERT,
	/* a plain box, without them */
	CONFIRM_LOOK_PLAIN,
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

/* player 1: the first local player of this machine, and its pad (a local player's gamepad is its index); NONE if
none */
static short player_1_gamepad(
	void)
{
	short local_player_index;

	for (local_player_index = 0; local_player_index < MAXIMUM_LOCAL_PLAYERS; local_player_index++)
	{
		if (local_player_get_player_index(local_player_index) != NONE)
			return local_player_index;
	}
	return NONE;
}

static boolean button_down(
	struct gamepad_state const *gamepad,
	short button)
{
	return gamepad && gamepad->buttons[button] != 0;
}

/* the alert panel's bitmap, or NULL */
static struct bitmap_data *alert_bitmap(
	void)
{
	long group_index = tag_loaded(BITMAP_GROUP_TAG, ALERT_BITMAP);

	return group_index == NONE ? NULL : bitmap_group_try_and_get_bitmap(group_index, 0);
}

static enum confirm_look confirm_look(
	void)
{
	return alert_bitmap() ? CONFIRM_LOOK_ALERT : CONFIRM_LOOK_PLAIN;
}

/* the panel: the alert's picture, or a dark box with the quit screen's blue edge */
static void draw_panel(
	enum confirm_look look,
	short ox,
	short oy)
{
	real_argb_color color;

	if (look == CONFIRM_LOOK_ALERT)
	{
		struct bitmap_data *bitmap = alert_bitmap();
		rectangle2d rect, texels;

		/* (the picture's bands let the report's rows through: a dark ground under it, inside its round corners) */
		set_color(0.9f, 0.0f, 0.0f, 0.0f, &color);
		fill(ox, oy, BOX_LEFT + 4.0f, BOX_TOP + 4.0f, BOX_RIGHT - 4.0f, BOX_BOTTOM - 4.0f, &color);
		text_rect(ox, oy, BOX_LEFT, BOX_TOP, BOX_RIGHT, BOX_BOTTOM, &rect);
		texels.x0 = 0;
		texels.y0 = 0;
		texels.x1 = (short)(bitmap->width * ALERT_WIDTH_FRACTION);
		texels.y1 = (short)(bitmap->height * ALERT_HEIGHT_FRACTION);
		draw_bitmap_in_rect(bitmap, &rect, &texels, NULL, (pixel32)0xFFFFFFFF, NULL, TRUE);
		return;
	}
	set_color(0.94f, 0.02f, 0.06f, 0.14f, &color);
	fill(ox, oy, BOX_LEFT, BOX_TOP, BOX_RIGHT, BOX_BOTTOM, &color);
	set_color(1.0f, 0.157f, 0.588f, 1.0f, &color);
	fill(ox, oy, BOX_LEFT, BOX_TOP, BOX_RIGHT, BOX_TOP + 2.0f, &color);
	fill(ox, oy, BOX_LEFT, BOX_BOTTOM - 2.0f, BOX_RIGHT, BOX_BOTTOM, &color);
	fill(ox, oy, BOX_LEFT, BOX_TOP, BOX_LEFT + 2.0f, BOX_BOTTOM, &color);
	fill(ox, oy, BOX_RIGHT - 2.0f, BOX_TOP, BOX_RIGHT, BOX_BOTTOM, &color);
}

/* ---------- public code */

boolean ae_postgame_buttons(
	void)
{
	unsigned long now = system_milliseconds();
	short gamepad_index = player_1_gamepad();
	struct gamepad_state const *gamepad = gamepad_index != NONE && input_has_gamepad(gamepad_index) ?
		input_get_gamepad_state(gamepad_index) : NULL;
	boolean accept_down = button_down(gamepad, BUTTON_A) || button_down(gamepad, BUTTON_START);
	boolean back_down = button_down(gamepad, BUTTON_B) || button_down(gamepad, BUTTON_BACK);
	boolean was_open;

	/* (a new carnage report: the confirm closed, nothing waited for) */
	if (!have_frame || now - last_frame_ms > POSTGAME_GAP_MS)
	{
		csmemset(&confirm, 0, sizeof(confirm));
		platform_log("postgame: the carnage report's buttons are player 1's (gamepad %d)", (int)gamepad_index);
	}
	have_frame = TRUE;
	last_frame_ms = now;
	was_open = confirm.open;
	switch (ae_postgame_confirm_step(&confirm, accept_down, back_down))
	{
	case AE_POSTGAME_STOCK:
		/* (player 1's A: the host's pick game, as the stock A, each frame it is held) */
		if (accept_down && global_network_game_server_get())
			network_game_server_reset_to_pregame(global_network_game_server_get());
		break;
	case AE_POSTGAME_LEAVE:
		platform_log("postgame: leaving the game (A on the confirm)");
		network_game_abort();
		break;
	default:
		if (confirm.open && !was_open)
			platform_log("postgame: leave the game? asked over the carnage report");
		else if (!confirm.open && was_open)
			platform_log("postgame: stayed on the carnage report (B on the confirm)");
		break;
	}
	return TRUE;
}

boolean ae_postgame_confirm_draw(
	rectangle2d const *prompt_bounds)
{
	static wchar_t const *const buttons = L"%a-button =yes        %b-button =no";
	enum confirm_look look;
	real_argb_color color;
	rectangle2d rect;
	short ox, oy;

	if (!confirm.open || !have_frame || system_milliseconds() - last_frame_ms > POSTGAME_GAP_MS)
		return FALSE;
	look = confirm_look();
	/* (the carnage report's origin, as its prompt is offset) */
	ox = (short)(prompt_bounds->x0 - PROMPT_X);
	oy = (short)(prompt_bounds->y0 - PROMPT_Y);
	/* the report dimmed, the panel over it */
	set_color(0.55f, 0.0f, 0.0f, 0.0f, &color);
	fill(ox, oy, 0.0f, 0.0f, 640.0f, 480.0f, &color);
	draw_panel(look, ox, oy);
	/* the question in the title band, as the quit screen's (left, its blue); what leaving does; the buttons in the
	button band (the report's font, set by game_engine.c) */
	set_color(1.0f, 0.157f, 0.588f, 1.0f, &color);
	draw_string_set_color(&color);
	if (look == CONFIRM_LOOK_ALERT)
	{
		draw_string_set_format(NONE, 0, 0);
		text_rect(ox, oy, BOX_LEFT + 12, BOX_TOP + 6, BOX_RIGHT, BOX_TOP + 30, &rect);
	}
	else
	{
		draw_string_set_format(NONE, _text_justification_center, 0);
		text_rect(ox, oy, BOX_LEFT, BOX_TOP + 6, BOX_RIGHT, BOX_TOP + 30, &rect);
	}
	rasterizer_draw_unicode_string(&rect, NULL, NULL, 0, L"LEAVE THE GAME?");
	draw_string_set_format(NONE, _text_justification_center, 0);
	set_color(1.0f, 0.80f, 0.84f, 0.88f, &color);
	draw_string_set_color(&color);
	text_rect(ox, oy, BOX_LEFT, BOX_TOP + 62, BOX_RIGHT, BOX_TOP + 86, &rect);
	rasterizer_draw_unicode_string(&rect, NULL, NULL, 0,
		global_network_game_server_get() ? L"This quits the game for everyone." : L"Back to the main menu.");
	draw_string_set_format(NONE, 0, 0);
	set_color(1.0f, 1.0f, 1.0f, 1.0f, &color);
	draw_string_set_color(&color);
	text_rect(ox, oy, BOX_LEFT + 96, BOX_BOTTOM - 26, BOX_RIGHT, BOX_BOTTOM, &rect);
	draw_string_and_hack_in_icons(&rect, NULL, NULL, 0, buttons, FALSE);
	return TRUE;
}
