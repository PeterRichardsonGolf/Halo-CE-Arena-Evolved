/*
HUD_ITEM_TIMERS.C

port: the match clock and the power list of the gametype's TIMERS and
TRAINING, drawn once per local player's view from hud_draw_screen (hud.c).

The clock is one line, centred at the top of the view, one line above the
game's own target-name line (game_engine_rasterize_message): the time played
as M:SS, or with a time limit the time left as M:SS LEFT.

The power list is one line of the rockets', sniper's, overshield's and
camo's next spawns, soonest first (entries of one class spawning together
are one), in the top corner opposite the performance overlay
(display.performance_position; the overlay off puts it at the left).

Both are drawn like the performance overlay (main.c frame_statistics_draw):
the HUD's smaller font, its blue, 0.7 alpha, at four fifths size, inset 4
pixels from the view's top. Unlike the overlay, drawn in the HUD's pass, the
bounds are the view's relative to itself.
*/

/* ---------- headers */

#include "cseries/cseries.h"

#include "game/game.h"
#include "game/game_engine.h"
#include "game/item_timers.h"
#include "interface/hud.h"
#include "interface/hud_definitions.h"
#include "interface/hud_item_timers.h"
#include "interface/hud_messaging.h"
#include "rasterizer/rasterizer.h"
#include "render/render.h"
#include "text/draw_string.h"
#include "text/font_group.h"
#include "text/unicode.h"

/* ---------- constants */

#define HUD_ITEM_TIMERS_ALPHA 0.7f
#define HUD_ITEM_TIMERS_SCALE 0.8f	/* (as main.c's FRAME_STATISTICS_SCALE) */
#define HUD_ITEM_TIMERS_INSET 4
#define HUD_ITEM_TIMERS_TOP 2
#define HUD_ITEM_TIMERS_MAXIMUM_ENTRIES 64
#define HUD_ITEM_TIMERS_LINE_LENGTH 160

/* (the HUD's text justifications: left, right, centre) */
enum
{
	_hud_item_timers_left = 0,
	_hud_item_timers_right,
	_hud_item_timers_center
};

/* ---------- prototypes */

const char *config_string(const char *name);

/* ---------- code */

static long hud_item_timers_font_index(
	void)
{
	/* (the overlay's: the HUD's smaller font, split screen's, else its own) */
	return hud_globals->messaging.multi_player_font.index != NONE ?
		hud_globals->messaging.multi_player_font.index : hud_globals->messaging.single_player_font.index;
}

static long hud_item_timers_line_height(
	long font_index)
{
	struct font_header *font = font_definition_get(font_index);

	return MAX(font->ascending_height + font->descending_height + font->leading_height, 10);
}

/* (M:SS: the engine's time string leaves the minute blank under one) */
static void hud_item_timers_time_string(
	long ticks,
	boolean round_up,
	wchar_t *string,
	long count)
{
	/* (countdowns round up: 0:00 only at the spawn, as the voice counts) */
	long seconds = (MAX(ticks, 0) + (round_up ? TICKS_PER_SECOND - 1 : 0)) / TICKS_PER_SECOND;

	usnprintf(string, count, L"%d:%02d", (int)(seconds / 60), (int)(seconds % 60));
	string[count - 1] = 0;
}

static void hud_item_timers_draw_line(
	long font_index,
	short justification,
	short top,
	wchar_t const *text)
{
	rectangle2d bounds;
	real_argb_color color;
	real pivot_x;

	/* (the view's own bounds, relative to it: where this pass draws) */
	bounds.x0 = HUD_ITEM_TIMERS_INSET;
	bounds.x1 = (short)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0 - HUD_ITEM_TIMERS_INSET);
	bounds.y0 = top;
	bounds.y1 = (short)(top + hud_item_timers_line_height(font_index));
	color = hud_globals->messaging.state_color;
	color.alpha = HUD_ITEM_TIMERS_ALPHA;
	if (justification == _hud_item_timers_left)
		pivot_x = (real)bounds.x0;
	else if (justification == _hud_item_timers_right)
		pivot_x = (real)bounds.x1;
	else
		pivot_x = (real)(bounds.x0 + bounds.x1) / 2.0f;
	draw_string_set_draw_mode(font_index, NONE, justification, 0, &color);
	/* (smaller, about the line's top corner it hangs from) */
	rasterizer_text_set_scale(HUD_ITEM_TIMERS_SCALE, pivot_x, (real)bounds.y0);
	rasterizer_draw_unicode_string(&bounds, NULL, NULL, 0, text);
	rasterizer_text_set_scale(1.0f, 0.0f, 0.0f);
}

static void hud_item_timers_draw_clock(
	long font_index)
{
	wchar_t time_string[32];
	wchar_t text[48];
	long line_height = hud_item_timers_line_height(font_index);
	long target_y1, clock_y1;
	short window_height = (short)(render.camera.window_bounds.y1 - render.camera.window_bounds.y0);
	long time_limit = game_variant_options_get()->time_limit;

	if (time_limit > 0)
	{
		hud_item_timers_time_string(time_limit * 60L * TICKS_PER_SECOND - game_time_get(), TRUE, time_string, NUMBEROF(time_string));
		usnprintf(text, NUMBEROF(text), L"%s LEFT", time_string);
	}
	else
	{
		hud_item_timers_time_string(game_time_get(), FALSE, time_string, NUMBEROF(time_string));
		usnprintf(text, NUMBEROF(text), L"%s", time_string);
	}
	text[NUMBEROF(text) - 1] = 0;
	/* (the target-name line's bottom: game_engine_rasterize_message's, with
	the window's top at its offset from the view's) */
	target_y1 = (5L * (render.camera.window_bounds.y0 - render.camera.viewport_bounds.y0) +
		(render.camera.window_bounds.y0 - render.camera.viewport_bounds.y0 + window_height)) / 6 + 9;
	/* (not over the aimed-at name's line: the message box's top is 15 above
	its bottom) */
	clock_y1 = MIN(target_y1 - line_height - 2, target_y1 - 15);
	hud_item_timers_draw_line(font_index, _hud_item_timers_center,
		(short)(clock_y1 - line_height), text);
}

static void hud_item_timers_draw_powers(
	long font_index)
{
	struct item_timer const *entries[HUD_ITEM_TIMERS_MAXIMUM_ENTRIES];
	long left[HUD_ITEM_TIMERS_MAXIMUM_ENTRIES];
	short count = 0;
	short total = item_timers_count();
	short index, other;
	wchar_t line[HUD_ITEM_TIMERS_LINE_LENGTH];
	long used = 0;
	const char *position = config_string("display.performance_position");
	const char *level = config_string("display.performance");
	boolean overlay_left = level && strcmp(level, "off") && position && !strcmp(position, "top_left");

	for (index = 0; index < total && count < HUD_ITEM_TIMERS_MAXIMUM_ENTRIES; index++)
	{
		struct item_timer const *timer = item_timers_get(index);
		long ticks;

		if (!timer || timer->timer_class >= NUMBER_OF_ITEM_TIMER_POWER_CLASSES)
			continue;
		ticks = item_timer_ticks_left(timer);
		/* (insertion sort, soonest first) */
		for (other = count; other > 0 && left[other - 1] > ticks; other--)
		{
			entries[other] = entries[other - 1];
			left[other] = left[other - 1];
		}
		entries[other] = timer;
		left[other] = ticks;
		count++;
	}

	line[0] = 0;
	for (index = 0; index < count; index++)
	{
		wchar_t time_string[32];
		boolean merged = FALSE;

		/* (one of a class spawning with another, as the same second, is one) */
		for (other = 0; other < index; other++)
		{
			if (entries[other]->timer_class == entries[index]->timer_class &&
				(left[other] + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND == (left[index] + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND)
			{
				merged = TRUE;
				break;
			}
		}
		if (merged)
			continue;
		hud_item_timers_time_string(left[index], TRUE, time_string, NUMBEROF(time_string));
		usnprintf(line + used, HUD_ITEM_TIMERS_LINE_LENGTH - used, L"%s%s %s",
			used ? L"   " : L"", entries[index]->label, time_string);
		line[HUD_ITEM_TIMERS_LINE_LENGTH - 1] = 0;
		used = (long)ustrlen(line);
	}
	if (used)
	{
		hud_item_timers_draw_line(font_index, overlay_left ? _hud_item_timers_right : _hud_item_timers_left,
			HUD_ITEM_TIMERS_TOP, line);
	}
}

void hud_draw_item_timers(
	void)
{
	long font_index = hud_item_timers_font_index();

	if (font_index == NONE)
		return;
	hud_item_timers_draw_clock(font_index);
	hud_item_timers_draw_powers(font_index);

	return;
}
