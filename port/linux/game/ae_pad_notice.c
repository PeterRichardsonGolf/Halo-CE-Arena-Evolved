/*
AE_PAD_NOTICE.C

The controller notice (ae_pad_notice.h): the state machines of the pads' batteries, the menus' notice and each player's
HUD notice. The words and rules are ae_pad_notice_rules.h's.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "cseries/cseries.h"
#include "math/real_math.h"

#include "../src/ae_draw.h"
#include "../src/ae_pad_notice_rules.h"
#include "../src/ae_steam_pad.h"
#include "ae_hooks.h"
#include "ae_hud_text.h"
#include "ae_motion.h"
#include "ae_pad_notice.h"
#include "ae_panel_engine.h"
#include "ae_panel_layout.h"
#include "ae_panels.h"
#include "ae_score_strip.h"
#include "ae_screen_settings.h"
#include "ae_style.h"

void platform_log(char const *format, ...);
unsigned long system_milliseconds(void);
unsigned char pc_menu_split_players(void);

enum { PAD_PLAYERS = 4, READ_MS = 250 };

static struct
{
	struct ae_pad_battery_state battery[PAD_PLAYERS];
	int kind[PAD_PLAYERS];                  /* AE_PAD_NOTICE_*: the notice each player has now (or had last) */
	int percent[PAD_PLAYERS];
	unsigned long began[PAD_PLAYERS];
	int shown[PAD_PLAYERS];                 /* a notice of this player's is on its way (or held) */
	unsigned long read_at;
	int world_said;                     /* the Linux access notice was given this session */
	unsigned long world_began;
	int world_shown;
	int hold;
} state;

void ae_pad_notice_hold(int on)
{
	state.hold = on != 0;
}

void ae_pad_notice_reset(void)
{
	memset(&state, 0, sizeof(state));
}

/* whether a notice that began at began is still to be seen */
static int visible(unsigned long began, unsigned long now)
{
	return state.hold || ae_pad_notice_alpha(now - began) > 0.0;
}

/* the pads read each READ_MS: the notice each battery's change gives */
static void update(unsigned long now)
{
	int player;

	if (state.read_at && now - state.read_at < READ_MS)
		return;
	state.read_at = now ? now : 1;
	for (player = 0; player < PAD_PLAYERS; player++)
	{
		struct ae_pad_info pad;
		int notice;

		ae_steam_pad_player_info(player, &pad);
		/* (another pad on the port, or none: the old pad's notice state and notice go) */
		if (ae_pad_battery_rules_pad(&state.battery[player], pad.present ? pad.id : 0))
			state.shown[player] = 0;
		/* (a staged charging pad stands for one that was low before) */
		if (pad.after_low)
			state.battery[player].low_given = 1;
		notice = pad.present ? ae_pad_battery_rules_step(&state.battery[player], pad.power, pad.percent) : AE_PAD_NOTICE_NONE;
		if (notice != AE_PAD_NOTICE_NONE)
		{
			state.kind[player] = notice;
			state.percent[player] = pad.percent;
			state.began[player] = now ? now : 1;
			state.shown[player] = 1;
			platform_log("pad notice: player %d %s, battery %d %%", player + 1,
				notice == AE_PAD_NOTICE_LOW ? "battery low" : "charging", pad.percent);
		}
		/* (Linux has not let the game open a Steam Controller: said once a session, in the menus) */
		if (pad.world == AE_STEAM_WORLD_NO_ACCESS && !state.world_said)
		{
			state.world_said = 1;
			state.world_shown = 1;
			state.world_began = now ? now : 1;
			platform_log("pad notice: a Steam Controller acts as a mouse (no access)");
		}
	}
}

/* ---------- drawing shapes */

static real_argb_color color_of(unsigned int rgba, real alpha)
{
	real_argb_color color;

	color.alpha = alpha;
	color.red = (real)((rgba >> 24) & 255u) / 255.0f;
	color.green = (real)((rgba >> 16) & 255u) / 255.0f;
	color.blue = (real)((rgba >> 8) & 255u) / 255.0f;
	return color;
}

/* a battery's outline, level and terminal, in its height h, wide 1.8 h, the level a quarter of the body when low */
#define BATTERY_ASPECT 1.8f
#define BATTERY_LEVEL_LOW 0.25f

/* ---------- the menus' notice */

#define MENU_CAP_CENTRE_UNITS 128.0f
#define MENU_TEXT_UNITS 18.0f
#define MENU_ICON_UNITS 12.0f
#define MENU_ICON_GAP_UNITS 12.0f
#define ICE_BLUE 0xA1CEFFFFu

/* the colour between two (0xRRGGBBAA, the alpha ignored) at t, 0 to 1 */
static unsigned int mix(unsigned int from, unsigned int to, double t)
{
	unsigned int out = 0;
	int channel;

	for (channel = 1; channel <= 3; channel++)
	{
		unsigned int shift = (unsigned int)(32 - 8 * channel);
		double a = (double)((from >> shift) & 255u), b = (double)((to >> shift) & 255u);

		out |= (unsigned int)(a + (b - a) * t + 0.5) << shift;
	}
	return out;
}

/* (the picture: a TEXT outline, the level in the warning colour) */
static void menu_battery(float right, float middle, float height, float alpha)
{
	float width = height * BATTERY_ASPECT, x = right - width, y = middle - height * 0.5f, line = height * 0.12f;
	float nub = height * 0.3f;
	unsigned int text = (AE_COLOR_TEXT & 0xFFFFFF00u) | (unsigned int)(255.0f * alpha), level = (AE_COLOR_WARNING & 0xFFFFFF00u) |
		(unsigned int)(255.0f * alpha);

	ae_draw_outline(x, y, width - nub * 0.5f, height, height * 0.14f, line, text);
	ae_draw_rect(x + width - nub * 0.5f, middle - nub * 0.5f, nub * 0.5f, nub, nub * 0.12f, text);
	ae_draw_rect(x + line * 1.8f, y + line * 1.8f, (width - nub * 0.5f - line * 3.6f) * BATTERY_LEVEL_LOW, height - line * 3.6f,
		height * 0.06f, level);
}

/* the dark outline's reach, u: about 1.4 px at 1080 */
#define MENU_OUTLINE_UNITS 1.4f

/* (the picture's dark outline: its body grown by the reach, drawn under it) */
static void menu_battery_outline(float right, float middle, float height, float reach, unsigned int rgba)
{
	float width = height * BATTERY_ASPECT, x = right - width, y = middle - height * 0.5f;

	ae_draw_rect(x - reach, y - reach, width + reach * 2.0f, height + reach * 2.0f, height * 0.2f, rgba);
}

void ae_pad_notice_menu_draw(void)
{
	unsigned long now = system_milliseconds();
	struct ae_layout layout;
	struct ae_frame frame;
	char text[AE_PAD_NOTICE_TEXT];
	unsigned long began = 0;
	int player, pick = -1, with_icon = 1;
	double alpha, pulse;
	float size, icon, middle, right, pen_x;
	unsigned int color;

	update(now);
	/* (the newest notice of the players' and the Linux one wins) */
	for (player = 0; player < PAD_PLAYERS; player++)
	{
		if (state.shown[player] && visible(state.began[player], now) && (pick < 0 || (long)(state.began[player] - began) > 0))
		{
			pick = player;
			began = state.began[player];
		}
	}
	if (state.world_shown && visible(state.world_began, now) && (pick < 0 || (long)(state.world_began - began) > 0))
	{
		pick = PAD_PLAYERS;
		began = state.world_began;
	}
	if (pick < 0)
		return;
	if (pick == PAD_PLAYERS)
	{
		snprintf(text, sizeof(text), "%s", ae_screen_settings_is_open() ? AE_PAD_NOTICE_WORLD_NO_ACCESS_SETTINGS :
			AE_PAD_NOTICE_WORLD_NO_ACCESS_MENU);
		with_icon = 0;
	}
	else
		ae_pad_notice_text(state.kind[pick], pc_menu_split_players() >= 2 ? pick + 1 : 0, state.percent[pick], text);
	alpha = state.hold ? 1.0 : ae_pad_notice_alpha(now - began);
	/* (a held notice is a staged scene's: no pulse, so it is looked at in its own colour) */
	pulse = state.hold ? 0.0 : ae_pad_notice_pulse(now - began, ae_motion_reduced());
	/* (over every screen at full strength: a screen sliding in has left the drawing's alpha lower) */
	ae_draw_set_alpha(1.0f);
	ae_draw_current_layout(&layout);
	ae_frame_compute(layout.width, ae_settings_ui_scale(), &frame);
	size = MENU_TEXT_UNITS * frame.unit;
	icon = MENU_ICON_UNITS * frame.unit;
	/* (1824 and the cap centre 128 are 1080-line layout units: the frame's right edge less its margin, and its fraction) */
	right = frame.rect.x + frame.rect.width - frame.margin;
	middle = ae_frame_y(&frame, MENU_CAP_CENTRE_UNITS / 1080.0f);
	color = mix(AE_COLOR_TITLE, ICE_BLUE, pulse);
	color = (color & 0xFFFFFF00u) | (unsigned int)(255.0 * alpha);
	pen_x = right - ae_draw_text_width(AE_FONT_ROW, size, text);
	{
		/* (the HUD notice's dark outline (6,10,18) at 0.88, drawn as eight offset copies under the text and the picture) */
		static const float around[8][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 }, { -0.7f, -0.7f }, { 0.7f, -0.7f },
			{ -0.7f, 0.7f }, { 0.7f, 0.7f } };
		unsigned int outline = 0x060A1200u | (unsigned int)(255.0 * 0.88 * alpha);
		float reach = MENU_OUTLINE_UNITS * frame.unit, top = middle - ae_draw_cap_height(AE_FONT_ROW, size) * 0.5f;
		int at;

		for (at = 0; at < 8; at++)
			ae_draw_text(AE_FONT_ROW, size, pen_x + around[at][0] * reach, top + around[at][1] * reach, AE_ALIGN_LEFT, outline, text);
		if (with_icon)
			menu_battery_outline(pen_x - MENU_ICON_GAP_UNITS * frame.unit, middle, icon, reach, outline);
	}
	ae_draw_text(AE_FONT_ROW, size, pen_x, middle - ae_draw_cap_height(AE_FONT_ROW, size) * 0.5f, AE_ALIGN_LEFT, color, text);
	if (with_icon)
		menu_battery(pen_x - MENU_ICON_GAP_UNITS * frame.unit, middle, icon * 1.0f, (float)alpha);
}

/* ---------- the HUD's notice */

#define HUD_TEXT_PX 22.0f
#define HUD_TEXT_FLOOR 16.0f
#define HUD_CAP_CENTRE_UNDER 33.0f
#define HUD_ICON_UNITS 14.0f
#define HUD_ICON_GAP 8.0f
/* the shield group's height under its box row's top, 1080 lines: the bar, the gap and the health cells (ae_panel_layout.c) */
#define HUD_SHIELD_GROUP_HEIGHT (20.0f + 6.0f + 10.0f)

static void fill(real x0, real y0, real x1, real y1, real_argb_color const *color)
{
	real points[4][2];

	points[0][0] = x0; points[0][1] = y0;
	points[1][0] = x1; points[1][1] = y0;
	points[2][0] = x1; points[2][1] = y1;
	points[3][0] = x0; points[3][1] = y1;
	ae_hud_fill_quad_color(points, color);
}

static void hud_battery(real right, real middle, real height, real alpha)
{
	real width = height * BATTERY_ASPECT, x = right - width, y = middle - height * 0.5f, line = height * 0.12f;
	real nub = height * 0.3f, body_right = x + width - nub * 0.5f;
	real_argb_color text = color_of(0xE5F0FFFFu, alpha), level = color_of(AE_COLOR_WARNING, alpha);

	fill(x, y, body_right, y + line, &text);
	fill(x, y + height - line, body_right, y + height, &text);
	fill(x, y, x + line, y + height, &text);
	fill(body_right - line, y, body_right, y + height, &text);
	fill(body_right, middle - nub * 0.5f, x + width, middle + nub * 0.5f, &text);
	fill(x + line * 1.8f, y + line * 1.8f, x + line * 1.8f + (body_right - x - line * 3.6f) * BATTERY_LEVEL_LOW, y + height - line * 1.8f,
		&level);
}

void ae_pad_notice_hud_draw(short local_player_index)
{
	unsigned long now = system_milliseconds();
	struct ae_panel_view view;
	struct ae_panel_rect rects[2];
	short count, at;
	char text[AE_PAD_NOTICE_TEXT];
	wchar_t wide[AE_PAD_NOTICE_TEXT];
	real scale, px, bottom, right, width, cap, baseline, alpha;
	real_argb_color color;

	if (local_player_index < 0 || local_player_index >= PAD_PLAYERS)
		return;
	update(now);
	if (!state.shown[local_player_index] || !visible(state.began[local_player_index], now))
		return;
	if (!ae_panel_engine_view(local_player_index, &view) || view.scale <= 0.0f)
		return;
	scale = view.scale;
	/* (the shield group's right edge and bottom: the layout the panels last drew when AE's HUD drew one, else the same
	group's size from the meter's own corner) */
	right = view.shield_right_x;
	bottom = view.shield_top_y + HUD_SHIELD_GROUP_HEIGHT * scale;
	ae_panels_rectangles(local_player_index, rects, &count);
	for (at = 0; at < count; at++)
	{
		if (rects[at].x1 >= view.shield_right_x - 8.0f * scale && rects[at].y1 > bottom - 4.0f * scale)
		{
			right = rects[at].x1;
			bottom = rects[at].y1;
		}
	}
	ae_pad_notice_text(state.kind[local_player_index], 0, state.percent[local_player_index], text);
	for (at = 0; text[at] && at < AE_PAD_NOTICE_TEXT - 1; at++)
		wide[at] = (wchar_t)(unsigned char)text[at];
	wide[at] = 0;
	px = HUD_TEXT_PX * scale;
	if (px < HUD_TEXT_FLOOR)
		px = HUD_TEXT_FLOOR;
	alpha = state.hold ? 1.0f : (real)ae_pad_notice_alpha(now - state.began[local_player_index]);
	color = color_of(0xE5F0FFFFu, alpha);
	width = ae_hud_text_measure(wide, px);
	cap = px * AE_HUD_TEXT_CAP;
	baseline = bottom + HUD_CAP_CENTRE_UNDER * scale + cap * 0.5f;
	ae_hud_text_draw(local_player_index, (real)floor(right - width + 0.5f), (real)floor(baseline + 0.5f), wide, px, &color, TRUE);
	hud_battery(right - width - HUD_ICON_GAP * scale, baseline - cap * 0.5f, HUD_ICON_UNITS * scale, alpha);
}
