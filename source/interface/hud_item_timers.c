/*
HUD_ITEM_TIMERS.C

port: MATCH CLOCK (display.match_clock) and the power list of the
gametype's TIMERS and TRAINING, drawn once per local player's view from
hud_draw_screen (hud.c).

The clock is M:SS in the view's bottom right corner, as the Master Chief
Collection's (game_engine_match_clock: the time left or the time played, in
any multiplayer game), lined up with the motion sensor in the opposite
corner: as far from the view's right edge as the sensor's background is
from its left, its digits' foot at the background's foot, where the
sensor's range ("15m") is. The sensor's place is as hud_unit.c last drew it
in this view (hud_item_timers_set_motion_sensor); where it has not been
drawn (hidden by a script or the gametype), 4% of the view's width from its
right edge and 6% of its height from its bottom. Room is left above it for
a score box (upstream's always-on score, not merged). On Halo 1: NHE's
maps it keeps out of their countdown's titles (g_*).

The power list is one line of the rockets', sniper's, overshield's and
camo's next spawns, soonest first (entries of one class spawning together
are one). With one view, in the top corner opposite the performance
overlay (display.performance_position; the overlay off puts it at the
left), on the overlay's line. In split screen, centred in the margin the
HUD leaves at the screen's edge (2, 3 or 4 views): the HUD's window
(render.camera.window_bounds, which its meters hang from) is inset from the
screen's edges, not from a split screen's middle, so a view at the screen's
top has the margin above its HUD and a lower view has it below (where its
HUD's top meters would meet a line at the view's top, or its corner the
ammo's). A line at the screen's top goes under the overlay's line only
where the two would meet (main.c's main_framerate_extent); at a lower
view's bottom it keeps clear of the clock in the corner. Entries that would
take the line wider than its room are left off (the latest).

Neither is drawn once the game is over (game_engine_game_over: the
postgame's "You won"/"You lost" view and the scores).

Both are drawn like the performance overlay (main.c frame_statistics_draw):
the HUD's smaller font, its blue, 0.7 alpha, at four fifths size. Unlike the
overlay, drawn in the HUD's pass, the bounds are the view's relative to
itself.

TRAINING's waypoints (hud_draw_item_waypoints) are the game's own nav
points, as Halo 1: NHE's Training mode's: the "default" arrow (CTF's over
a flag carrier's own base, where it scores) with the distance by it, over
each power entry's spawn point while item_timer_waypoint_shown, drawn as
the game engine draws its goals' (game_engine_render_nav_points): occluded
or not by a line of sight test from the player's head each frame, at the
view's edge pointing to it when off screen. For every player (NHE turns
them on for both teams), only while alive, as the game's nav points.
*/

/* ---------- headers */

#include "cseries/cseries.h"

#include "game/game.h"
#include "cutscene/cinematics.h"
#include "game/game_engine.h"
#include "game/item_timers.h"
#include "game/players.h"
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
#include "units/units.h"

/* ---------- constants */

#define HUD_ITEM_TIMERS_ALPHA 0.7f
#define HUD_ITEM_TIMERS_SCALE 0.8f	/* (as main.c's FRAME_STATISTICS_SCALE) */
#define HUD_ITEM_TIMERS_TOP 2	/* (the overlay's line's, from the view's top) */
#define HUD_ITEM_TIMERS_SIDE 4	/* (the power list's room from a view's sides, as the overlay's) */
#define HUD_ITEM_TIMERS_GAP 12	/* (between the power list and the clock, or the overlay) */
#define HUD_ITEM_TIMERS_CLOCK_RIGHT 0.04f	/* (of the view's width, with no motion sensor) */
#define HUD_ITEM_TIMERS_CLOCK_BOTTOM 0.06f	/* (of the view's height, with no motion sensor) */
#define HUD_ITEM_TIMERS_MAXIMUM_ENTRIES 64
#define HUD_ITEM_TIMERS_LINE_LENGTH 160

/* ---------- globals */

/* where each local player's view last had its motion sensor's background
(hud_item_timers_set_motion_sensor), and the view's size then */
static struct
{
	boolean valid;
	short view_width;
	short view_height;
	rectangle2d bounds;
} hud_item_timers_motion_sensors[MAXIMUM_LOCAL_PLAYERS];

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
	if (justification == _text_justification_left)
	{
		pivot_x = (real)bounds.x0;
		bounds.x1 = (short)(bounds.x0 + (bounds.x1 - bounds.x0) / HUD_ITEM_TIMERS_SCALE);
	}
	else if (justification == _text_justification_right)
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
	hud_item_timers_set_draw_mode(font_index, _text_justification_left);
	draw_unicode_string_compute_bounds(&bounds, text, &text_bounds, &cursor_bounds);
	if (text_bounds.x1 <= text_bounds.x0)
		return 0;

	return (long)((text_bounds.x1 - text_bounds.x0) * HUD_ITEM_TIMERS_SCALE + 0.5f);
}

/* the clock (text) in the view's bottom right corner, by the motion
sensor's place; returns the room it takes from the view's right edge */
static long hud_item_timers_draw_clock(
	long font_index,
	wchar_t const *text)
{
	struct font_header *font = font_definition_get(font_index);
	short view_width = (short)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0);
	short view_height = (short)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0);
	short local_player_index = render.local_player_index;
	short inset;
	long baseline;

	if (local_player_index >= 0 && local_player_index < MAXIMUM_LOCAL_PLAYERS &&
		hud_item_timers_motion_sensors[local_player_index].valid &&
		hud_item_timers_motion_sensors[local_player_index].view_width == view_width &&
		hud_item_timers_motion_sensors[local_player_index].view_height == view_height)
	{
		rectangle2d const *sensor = &hud_item_timers_motion_sensors[local_player_index].bounds;

		/* (the sensor's left inset, mirrored; its background's foot) */
		inset = (short)MAX(sensor->x0, 0);
		baseline = MIN(sensor->y1, view_height);
	}
	else
	{
		inset = (short)(HUD_ITEM_TIMERS_CLOCK_RIGHT * view_width + 0.5f);
		baseline = view_height - (long)(HUD_ITEM_TIMERS_CLOCK_BOTTOM * view_height + 0.5f);
	}
	/* (the digits' foot, their baseline, there: the line's top an ascent
	(at four fifths) above it) */
	hud_item_timers_draw_line(font_index, _text_justification_right,
		(short)(baseline - (long)(font->ascending_height * HUD_ITEM_TIMERS_SCALE + 0.5f)),
		inset, text);

	return inset + hud_item_timers_line_width(font_index, text);
}

/* the power list's line, as many entries as fit in maximum_width; returns
its width, 0 for none */
static long hud_item_timers_power_line(
	long font_index,
	long maximum_width,
	wchar_t *line)
{
	struct item_timer const *entries[HUD_ITEM_TIMERS_MAXIMUM_ENTRIES];
	long left[HUD_ITEM_TIMERS_MAXIMUM_ENTRIES];
	short count = 0;
	short total = item_timers_count();
	short index, other;
	long used = 0;

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
		game_engine_format_clock(left[index], TRUE, time_string, NUMBEROF(time_string));
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

	return used ? hud_item_timers_line_width(font_index, line) : 0;
}

/* clock_room: the room the clock takes from the view's right edge, 0 with
none */
static void hud_item_timers_draw_powers(
	long font_index,
	long clock_room)
{
	wchar_t line[HUD_ITEM_TIMERS_LINE_LENGTH];
	long view_width = render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0;
	long line_height = (long)(hud_item_timers_line_height(font_index) * HUD_ITEM_TIMERS_SCALE);
	/* (the margin the HUD leaves at the screen's edge: above the HUD's
	window in a view at the screen's top, below it in a lower one) */
	long top_margin = render.camera.window_bounds.y0 - render.camera.viewport_bounds.y0;
	long bottom_margin = render.camera.viewport_bounds.y1 - render.camera.window_bounds.y1;
	boolean one_view = local_player_count() <= 1;
	/* (one view: the top corner opposite the overlay's; split screen:
	centred) */
	short justification = !one_view ? _text_justification_center :
		main_framerate_shown() && main_framerate_corner() == _text_justification_left ?
			_text_justification_right : _text_justification_left;
	long width = hud_item_timers_power_line(font_index, view_width - 2 * HUD_ITEM_TIMERS_SIDE, line);
	long overlay = 0;
	long top;
	boolean bottom;
	short inset;

	if (!width)
		return;
	/* (the screen's top line: under the overlay's line where they would
	meet, in the screen's coordinates) */
	{
		rectangle2d extent;

		if (render.camera.viewport_bounds.y0 <= 0 && main_framerate_shown() && main_framerate_extent(&extent))
		{
			long x0 = justification == _text_justification_left ? HUD_ITEM_TIMERS_SIDE :
				justification == _text_justification_right ? view_width - HUD_ITEM_TIMERS_SIDE - width :
				(view_width - width) / 2;
			long x1 = x0 + width;

			x0 += render.camera.viewport_bounds.x0;
			x1 += render.camera.viewport_bounds.x0;
			if (x0 < extent.x1 + HUD_ITEM_TIMERS_GAP && x1 > extent.x0 - HUD_ITEM_TIMERS_GAP)
				overlay = MAX(extent.y1 - render.camera.viewport_bounds.y0, 0);
		}
	}
	/* (a lower split-screen view: below its HUD) */
	bottom = !one_view && top_margin < overlay + HUD_ITEM_TIMERS_TOP + line_height &&
		bottom_margin >= HUD_ITEM_TIMERS_TOP + line_height;
	if (bottom)
	{
		top = render.camera.window_bounds.y1 - render.camera.viewport_bounds.y0 + HUD_ITEM_TIMERS_TOP;
		/* (clear of the clock on both sides) */
		if (clock_room > 0)
		{
			width = hud_item_timers_power_line(font_index, view_width - 2 * (clock_room + HUD_ITEM_TIMERS_GAP), line);
			if (!width)
				return;
		}
	}
	else
		top = overlay + HUD_ITEM_TIMERS_TOP;
	inset = justification == _text_justification_center ? 0 : HUD_ITEM_TIMERS_SIDE;
	hud_item_timers_draw_line(font_index, justification, (short)top, inset, line);
}

void hud_draw_item_timers(
	void)
{
	long font_index = hud_item_timers_font_index();
	wchar_t clock[32];
	long clock_room = 0;

	if (font_index == NONE)
		return;
	/* (not over Halo 1: NHE's maps' countdown) */
	if (game_engine_match_clock(clock, NUMBEROF(clock)) && !cinematic_nhe_countdown_title_showing())
		clock_room = hud_item_timers_draw_clock(font_index, clock);
	/* (the gametype's TIMERS and TRAINING; not once the game is over, over
	the postgame's view, as the clock is not) */
	if (game_engine_item_timers() && !game_engine_game_over())
		hud_item_timers_draw_powers(font_index, clock_room);

	return;
}

/* TRAINING's waypoints over the power entries, in this local player's
view (from hud_draw_screen, where the game's nav points are drawn) */
void hud_draw_item_waypoints(
	short local_player_index)
{
	short count = item_timers_count();
	short nav_index = NONE;
	long player_index;
	long unit_index;
	real_point3d head_position;
	short index;

	if (!count || local_player_index == NONE || !hud_globals ||
		hud_globals->waypoint.arrow_bitmap.index == NONE)
	{
		return;
	}
	player_index = local_player_get_player_index(local_player_index);
	unit_index = player_index == NONE ? NONE : player_get(player_index)->unit_index;
	if (unit_index == NONE)
		return;
	unit_get_head_position(unit_index, &head_position);

	for (index = 0; index < count; index++)
	{
		struct item_timer const *timer = item_timers_get(index);
		real_point3d position;
		short render_type;

		if (!item_timer_waypoint_shown(timer))
			continue;
		/* (the arrow NHE's calls have on a stock map's HUD globals) */
		if (nav_index == NONE)
		{
			nav_index = hud_nav_point_index("default");
			if (nav_index == NONE)
				return;
		}
		position = timer->position;
		position.z += ITEM_TIMER_WAYPOINT_HEIGHT;
		render_type = hud_get_nav_point_render_type(local_player_index, &head_position, &position, NONE);
		custom_render_nav_point(local_player_index, &position, nav_index, render_type);
	}
}

/* where hud_unit.c drew the motion sensor's background in this local
player's view (the view's coordinates) */
void hud_item_timers_set_motion_sensor(
	short local_player_index,
	rectangle2d const *bounds)
{
	if (local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS)
		return;
	hud_item_timers_motion_sensors[local_player_index].valid = TRUE;
	hud_item_timers_motion_sensors[local_player_index].view_width =
		(short)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0);
	hud_item_timers_motion_sensors[local_player_index].view_height =
		(short)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0);
	hud_item_timers_motion_sensors[local_player_index].bounds = *bounds;
}

/* a new map: no motion sensor seen yet */
void hud_item_timers_initialize_for_new_map(
	void)
{
	csmemset(hud_item_timers_motion_sensors, 0, sizeof(hud_item_timers_motion_sensors));
}
