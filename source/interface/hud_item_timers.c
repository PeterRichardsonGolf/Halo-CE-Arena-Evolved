/*
HUD_ITEM_TIMERS.C

port: MATCH CLOCK (display.match_clock) and the power list of the
gametype's TIMERS and TRAINING, drawn once per local player's view from
hud_draw_screen (hud.c).

The clock is M:SS in the view's bottom right corner, as the Master Chief
Collection's: 4% of the view's width from its right edge, its digits' foot 6%
of its height from its bottom (game_engine_match_clock: the time left or the
time played, in any multiplayer game). Room is left above it for a score box
(upstream's always-on score, not merged).

The power list is one line of the rockets', sniper's, overshield's and
camo's next spawns, soonest first (entries of one class spawning together
are one), centred in the margin the HUD leaves at the screen's edge, in
every view (one view, or 2, 3 or 4 in split screen): the HUD's window
(render.camera.window_bounds, which its meters hang from) is inset from the
screen's edges, not from a split screen's middle, so a view at the screen's
top has the margin above its HUD and a lower view has it below (where its
HUD's top meters would meet a line at the view's top). At the screen's top
it goes under the performance overlay's line when that shows (main.c); at a
lower view's bottom it keeps clear of the clock in the corner. Entries that
would take the line wider than that are left off (the latest).

Both are drawn like the performance overlay (main.c frame_statistics_draw):
the HUD's smaller font, its blue, 0.7 alpha, at four fifths size. Unlike the
overlay, drawn in the HUD's pass, the bounds are the view's relative to
itself.
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
#include "main/main.h"
#include "rasterizer/rasterizer.h"
#include "render/render.h"
#include "text/draw_string.h"
#include "text/font_group.h"
#include "text/unicode.h"

/* ---------- constants */

#define HUD_ITEM_TIMERS_ALPHA 0.7f
#define HUD_ITEM_TIMERS_SCALE 0.8f	/* (as main.c's FRAME_STATISTICS_SCALE) */
#define HUD_ITEM_TIMERS_TOP 2
#define HUD_ITEM_TIMERS_SIDE 4	/* (the power list's room from a view's sides, as the overlay's) */
#define HUD_ITEM_TIMERS_GAP 12	/* (between the power list and the clock) */
#define HUD_ITEM_TIMERS_CLOCK_RIGHT 0.04f	/* (of the view's width) */
#define HUD_ITEM_TIMERS_CLOCK_BOTTOM 0.06f	/* (of the view's height) */
#define HUD_ITEM_TIMERS_MAXIMUM_ENTRIES 64
#define HUD_ITEM_TIMERS_LINE_LENGTH 160

/* (the HUD's text justifications: left, right, centre) */
enum
{
	_hud_item_timers_left = 0,
	_hud_item_timers_right,
	_hud_item_timers_center
};

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

static void hud_item_timers_set_draw_mode(
	long font_index,
	short justification)
{
	real_argb_color color = hud_globals->messaging.state_color;

	color.alpha = HUD_ITEM_TIMERS_ALPHA;
	draw_string_set_draw_mode(font_index, NONE, justification, 0, &color);
}

/* a line from top, inset (left and right) from the view's edges */
static void hud_item_timers_draw_line(
	long font_index,
	short justification,
	short top,
	short inset,
	wchar_t const *text)
{
	rectangle2d bounds;
	real pivot_x;

	/* (the view's own bounds, relative to it: where this pass draws) */
	bounds.x0 = inset;
	bounds.x1 = (short)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0 - inset);
	bounds.y0 = top;
	bounds.y1 = (short)(top + hud_item_timers_line_height(font_index));
	/* (laid out at full size, so as wide as the line's room is at four
	fifths: a line that fits the room scaled is not cut at its bounds) */
	if (justification == _hud_item_timers_left)
	{
		pivot_x = (real)bounds.x0;
		bounds.x1 = (short)(bounds.x0 + (bounds.x1 - bounds.x0) / HUD_ITEM_TIMERS_SCALE);
	}
	else if (justification == _hud_item_timers_right)
	{
		pivot_x = (real)bounds.x1;
		bounds.x0 = (short)(bounds.x1 - (bounds.x1 - bounds.x0) / HUD_ITEM_TIMERS_SCALE);
	}
	else
	{
		short half = (short)((bounds.x1 - bounds.x0) / HUD_ITEM_TIMERS_SCALE / 2.0f);

		pivot_x = (real)(bounds.x0 + bounds.x1) / 2.0f;
		bounds.x0 = (short)(pivot_x - half);
		bounds.x1 = (short)(pivot_x + half);
	}
	hud_item_timers_set_draw_mode(font_index, justification);
	/* (smaller, about the line's top corner it hangs from) */
	rasterizer_text_set_scale(HUD_ITEM_TIMERS_SCALE, pivot_x, (real)bounds.y0);
	rasterizer_draw_unicode_string(&bounds, NULL, NULL, 0, text);
	rasterizer_text_set_scale(1.0f, 0.0f, 0.0f);
}

/* the width a line takes, drawn (at four fifths) */
static long hud_item_timers_line_width(
	long font_index,
	wchar_t const *text)
{
	rectangle2d bounds;
	rectangle2d text_bounds;
	rectangle2d cursor_bounds;

	bounds.x0 = 0;
	bounds.y0 = 0;
	bounds.x1 = SHORT_MAX / 2;
	bounds.y1 = (short)hud_item_timers_line_height(font_index);
	hud_item_timers_set_draw_mode(font_index, _hud_item_timers_left);
	draw_unicode_string_compute_bounds(&bounds, text, &text_bounds, &cursor_bounds);
	if (text_bounds.x1 <= text_bounds.x0)
		return 0;

	return (long)((text_bounds.x1 - text_bounds.x0) * HUD_ITEM_TIMERS_SCALE + 0.5f);
}

/* the clock (text) in the view's bottom right corner; returns the room it
takes from the view's right edge */
static long hud_item_timers_draw_clock(
	long font_index,
	wchar_t const *text)
{
	struct font_header *font = font_definition_get(font_index);
	short view_width = (short)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0);
	short view_height = (short)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0);
	short inset = (short)(HUD_ITEM_TIMERS_CLOCK_RIGHT * view_width + 0.5f);
	long baseline;

	/* (the digits' foot, their baseline, at the corner's height: the line's
	top an ascent (at four fifths) above it) */
	baseline = view_height - (long)(HUD_ITEM_TIMERS_CLOCK_BOTTOM * view_height + 0.5f);
	hud_item_timers_draw_line(font_index, _hud_item_timers_right,
		(short)(baseline - (long)(font->ascending_height * HUD_ITEM_TIMERS_SCALE + 0.5f)),
		inset, text);

	return inset + hud_item_timers_line_width(font_index, text);
}

/* clock_room: the room the clock takes from the view's right edge, 0 with
none */
static void hud_item_timers_draw_powers(
	long font_index,
	long clock_room)
{
	struct item_timer const *entries[HUD_ITEM_TIMERS_MAXIMUM_ENTRIES];
	long left[HUD_ITEM_TIMERS_MAXIMUM_ENTRIES];
	short count = 0;
	short total = item_timers_count();
	short index, other;
	wchar_t line[HUD_ITEM_TIMERS_LINE_LENGTH];
	long used = 0;
	long view_width = render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0;
	long line_height = (long)(hud_item_timers_line_height(font_index) * HUD_ITEM_TIMERS_SCALE);
	/* (the margin the HUD leaves at the screen's edge: above the HUD's
	window in a view at the screen's top, below it in a lower one; the
	performance overlay's line, the same font and size, first at the
	screen's top) */
	long top_margin = render.camera.window_bounds.y0 - render.camera.viewport_bounds.y0;
	long bottom_margin = render.camera.viewport_bounds.y1 - render.camera.window_bounds.y1;
	long overlay = main_framerate_shown() ? HUD_ITEM_TIMERS_TOP + line_height : 0;
	boolean bottom = top_margin < overlay + HUD_ITEM_TIMERS_TOP + line_height &&
		bottom_margin >= HUD_ITEM_TIMERS_TOP + line_height;
	long top = bottom ?
		render.camera.window_bounds.y1 - render.camera.viewport_bounds.y0 + HUD_ITEM_TIMERS_TOP :
		overlay + HUD_ITEM_TIMERS_TOP;
	/* (centred, as wide as the view leaves; at the bottom, clear of the
	clock on both sides) */
	long side = bottom && clock_room > 0 ? clock_room + HUD_ITEM_TIMERS_GAP : HUD_ITEM_TIMERS_SIDE;
	long maximum_width = view_width - 2 * side;

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
		/* (not beyond the line's width: this entry and the later left off) */
		if (used && hud_item_timers_line_width(font_index, line) > maximum_width)
		{
			line[used] = 0;
			break;
		}
		used = (long)ustrlen(line);
	}
	if (used)
		hud_item_timers_draw_line(font_index, _hud_item_timers_center, (short)top, 0, line);
}

void hud_draw_item_timers(
	void)
{
	long font_index = hud_item_timers_font_index();
	wchar_t clock[32];
	long clock_room = 0;

	if (font_index == NONE)
		return;
	if (game_engine_match_clock(clock, NUMBEROF(clock)))
		clock_room = hud_item_timers_draw_clock(font_index, clock);
	/* (the gametype's TIMERS and TRAINING) */
	if (game_engine_item_timers())
		hud_item_timers_draw_powers(font_index, clock_room);

	return;
}
