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

With HUD AREA (display.hud_area: hud_area_insets), both keep to its part
of the view: the clock mirrors the sensor (which has moved in with the HUD)
about that part's middle, and the power list's line (and the clock with no
sensor) keeps to that part's sides, centred on its middle.

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
them on for both teams), only while alive, as the game's nav points. Each
has a label by its arrow (hud_waypoint_draw_labels): the item's name and
the time to its spawn, the name alone once it is on the map, in its base's
colour (item_timers.c's sides). Every arrow is drawn first; the labels are
then placed soonest first clear of the arrows, their distances, the clock
and power list and each other, arrows together at the view's edge sharing
one list beside them.
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
#include "interface/hud_draw.h"
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
#define HUD_ITEM_TIMERS_LABEL_ALPHA 0.9f	/* (a waypoint's label, over the world) */
#define HUD_ITEM_TIMERS_LABEL_GAP 2	/* (between a waypoint's arrow and its label) */
#define HUD_WAYPOINT_MERGE_DISTANCE 24	/* (arrows this near each other share their labels) */

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

/* the lines (MATCH CLOCK, the power list) each local player's view last
drew (hud_item_timers_draw_line), the view's coordinates: TRAINING's labels,
drawn before them, keep clear of where they were */
#define HUD_ITEM_TIMERS_MAXIMUM_LINES 4
static struct
{
	short count;
	rectangle2d bounds[HUD_ITEM_TIMERS_MAXIMUM_LINES];
} hud_item_timers_lines[MAXIMUM_LOCAL_PLAYERS];

/* ---------- code */

static long hud_item_timers_line_width(long font_index, wchar_t const *text);

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

/* a line from top, inset left and right from the view's edges */
static void hud_item_timers_draw_line(
	long font_index,
	short justification,
	short top,
	short left,
	short right,
	wchar_t const *text)
{
	rectangle2d bounds;
	real pivot_x;

	/* (the view's own bounds, relative to it: where this pass draws) */
	bounds.x0 = left;
	bounds.x1 = (short)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0 - right);
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

	/* (where it was drawn, for TRAINING's labels to keep clear of) */
	if (render.local_player_index >= 0 && render.local_player_index < MAXIMUM_LOCAL_PLAYERS &&
		hud_item_timers_lines[render.local_player_index].count < HUD_ITEM_TIMERS_MAXIMUM_LINES)
	{
		rectangle2d *drawn = &hud_item_timers_lines[render.local_player_index].bounds[
			hud_item_timers_lines[render.local_player_index].count++];
		long width = hud_item_timers_line_width(font_index, text);

		drawn->x0 = (short)(justification == _text_justification_left ? pivot_x :
			justification == _text_justification_right ? pivot_x - width : pivot_x - width / 2);
		drawn->x1 = (short)(drawn->x0 + width);
		drawn->y0 = bounds.y0;
		drawn->y1 = (short)(bounds.y0 + hud_item_timers_line_height(font_index) * HUD_ITEM_TIMERS_SCALE + 0.5f);
	}
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
	/* (port: HUD AREA's insets of the view's HUD, hud_area_insets) */
	short area_left;
	short area_right;

	hud_area_insets(&area_left, &area_right);

	if (local_player_index >= 0 && local_player_index < MAXIMUM_LOCAL_PLAYERS &&
		hud_item_timers_motion_sensors[local_player_index].valid &&
		hud_item_timers_motion_sensors[local_player_index].view_width == view_width &&
		hud_item_timers_motion_sensors[local_player_index].view_height == view_height)
	{
		rectangle2d const *sensor = &hud_item_timers_motion_sensors[local_player_index].bounds;

		/* (the sensor's left inset, mirrored; its background's foot. port:
		mirrored about the middle of HUD AREA's part of the view, which is
		the view's own moved by half the difference of its insets) */
		inset = (short)MAX(sensor->x0 + area_right - area_left, 0);
		baseline = MIN(sensor->y1, view_height);
	}
	else
	{
		/* (port: of HUD AREA's part of the view) */
		inset = (short)(area_right + HUD_ITEM_TIMERS_CLOCK_RIGHT * (view_width - area_left - area_right) + 0.5f);
		baseline = view_height - (long)(HUD_ITEM_TIMERS_CLOCK_BOTTOM * view_height + 0.5f);
	}
	/* (the digits' foot, their baseline, there: the line's top an ascent
	(at four fifths) above it) */
	hud_item_timers_draw_line(font_index, _text_justification_right,
		(short)(baseline - (long)(font->ascending_height * HUD_ITEM_TIMERS_SCALE + 0.5f)),
		inset, inset, text);

	return inset + hud_item_timers_line_width(font_index, text);
}

/* an entry's name in the shorter power line: its label, RED / BLUE as R /
B (and R/B for both, both TRUE) */
static void hud_item_timers_short_name(
	struct item_timer const *timer,
	boolean both,
	wchar_t *name,
	short size)
{
	wchar_t const *base = timer->label;

	if (timer->side_prefix)
	{
		wchar_t const *space = ustrchr(timer->label, L' ');

		if (space)
			base = space + 1;
		usnprintf(name, size, L"%s %s", both ? L"R/B" : timer->side == _item_timer_side_red ? L"R" : L"B", base);
	}
	else
	{
		usnprintf(name, size, L"%s", base);
	}
	name[size - 1] = 0;
}

/* the power list's shorter line, for entries spawning together (the same
second) that would not fit: the time once for each such group and its
names after it ("0:10 OS/CAMO ROCKETS R/B SNIPER OS"), an item at both
bases as one; as many as fit in maximum_width (the latest left off).
FALSE when it shortens nothing (no two entries share a second) */
static boolean hud_item_timers_compact_line(
	long font_index,
	long maximum_width,
	struct item_timer const *const *entries,
	long const *left,
	short count,
	wchar_t *line)
{
	short index, other;
	boolean shared = FALSE;
	long used = 0;
	long group_second = -1;

	for (index = 1; index < count; index++)
	{
		if ((left[index] + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND ==
			(left[index - 1] + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND)
		{
			shared = TRUE;
		}
	}
	if (!shared)
		return FALSE;

	line[0] = 0;
	for (index = 0; index < count; index++)
	{
		long second = (left[index] + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND;
		wchar_t name[32];
		boolean both = FALSE;
		boolean said = FALSE;
		long before = used;

		/* (said already: the same name in this group, or its other side's
		R/B) */
		for (other = 0; other < index; other++)
		{
			if ((left[other] + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND != second ||
				entries[other]->timer_class != entries[index]->timer_class)
			{
				continue;
			}
			if (!ustrcmp(entries[other]->label, entries[index]->label))
				said = TRUE;
			if (entries[index]->side_prefix && entries[other]->side_prefix &&
				entries[other]->side != entries[index]->side)
			{
				said = TRUE;
			}
		}
		if (said)
			continue;
		if (entries[index]->side_prefix)
		{
			for (other = index + 1; other < count; other++)
			{
				if ((left[other] + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND == second &&
					entries[other]->timer_class == entries[index]->timer_class && entries[other]->side_prefix &&
					entries[other]->side != entries[index]->side)
				{
					both = TRUE;
				}
			}
		}
		hud_item_timers_short_name(entries[index], both, name, NUMBEROF(name));
		if (second != group_second)
		{
			wchar_t time_string[32];

			game_engine_format_clock(left[index], TRUE, time_string, NUMBEROF(time_string));
			usnprintf(line + used, HUD_ITEM_TIMERS_LINE_LENGTH - used, L"%s%s %s",
				used ? L"   " : L"", time_string, name);
			group_second = second;
		}
		else
		{
			usnprintf(line + used, HUD_ITEM_TIMERS_LINE_LENGTH - used, L" %s", name);
		}
		line[HUD_ITEM_TIMERS_LINE_LENGTH - 1] = 0;
		/* (not beyond the line's width: this name and the later left off) */
		if (before && hud_item_timers_line_width(font_index, line) > maximum_width)
		{
			line[before] = 0;
			break;
		}
		used = (long)ustrlen(line);
	}

	return used > 0;
}

/* the power list's line, as many entries as fit in maximum_width (entries
spawning together grouped under one time when they would not:
hud_item_timers_compact_line); returns its width, 0 for none */
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

		/* (one of a class spawning with another, as the same second, is one;
		not one at the other team's base: RED SNIPER, BLUE SNIPER) */
		for (other = 0; other < index; other++)
		{
			if (entries[other]->timer_class == entries[index]->timer_class &&
				!ustrcmp(entries[other]->label, entries[index]->label) &&
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
		/* (not beyond the line's width: the shorter line, else this entry
		and the later left off) */
		if (used && hud_item_timers_line_width(font_index, line) > maximum_width)
		{
			line[used] = 0;
			if (hud_item_timers_compact_line(font_index, maximum_width, entries, left, count, line))
				return hud_item_timers_line_width(font_index, line);
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
	/* (the room from the view's sides: in HUD AREA's part of it,
	hud_area_insets, and the middle of that part) */
	short area_left;
	short area_right;
	long left;
	long right;
	long middle;
	long width;
	long overlay = 0;
	long top;
	boolean bottom;

	hud_area_insets(&area_left, &area_right);
	left = HUD_ITEM_TIMERS_SIDE + area_left;
	right = HUD_ITEM_TIMERS_SIDE + area_right;
	middle = area_left + (view_width - area_left - area_right) / 2;
	width = hud_item_timers_power_line(font_index, view_width - left - right, line);
	if (!width)
		return;
	/* (the screen's top line: under the overlay's line where they would
	meet, in the screen's coordinates) */
	{
		rectangle2d extent;

		if (render.camera.viewport_bounds.y0 <= 0 && main_framerate_shown() && main_framerate_extent(&extent))
		{
			long x0 = justification == _text_justification_left ? left :
				justification == _text_justification_right ? view_width - right - width :
				area_left + (view_width - area_left - area_right - width) / 2;
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
			width = hud_item_timers_power_line(font_index,
				view_width - area_left - area_right - 2 * (clock_room - area_right + HUD_ITEM_TIMERS_GAP), line);
			if (!width)
				return;
		}
	}
	else
		top = overlay + HUD_ITEM_TIMERS_TOP;
	/* (centred: about the middle of HUD AREA's part, as wide on both
	sides) */
	if (justification == _text_justification_center)
	{
		long half = MIN(middle, view_width - middle);

		if (!area_left && !area_right)
			hud_item_timers_draw_line(font_index, justification, (short)top, 0, 0, line);
		else
			hud_item_timers_draw_line(font_index, justification, (short)top,
				(short)(middle - half), (short)(view_width - middle - half), line);
	}
	else
		hud_item_timers_draw_line(font_index, justification, (short)top, (short)left, (short)right, line);
}

void hud_draw_item_timers(
	void)
{
	long font_index = hud_item_timers_font_index();
	wchar_t clock[32];
	long clock_room = 0;

	if (render.local_player_index >= 0 && render.local_player_index < MAXIMUM_LOCAL_PLAYERS)
		hud_item_timers_lines[render.local_player_index].count = 0;
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

/* ---------- TRAINING's waypoint labels */

/* a waypoint's label: its item's name and time (empty once the item is on
the map), where its arrow is, and the block of lines it is drawn in */
struct hud_waypoint_label
{
	struct item_timer const *timer;
	struct hud_nav_point_placement arrow;
	long ticks_left;	/* to the next spawn: the soonest first */
	real distance;	/* from the camera: of a tie, the nearest first */
	wchar_t name[24];
	wchar_t time[12];
	short block;	/* NONE: not drawn (the same as one by it, or no room) */
	short line;	/* its line in the block */
};

/* labels drawn together: one label, or the arrows at the view's edge by
each other as one list, a line per time (soonest first), the first line
by the arrow and the rest toward the view's middle */
struct hud_waypoint_block
{
	short first;	/* its soonest label */
	short line_count;
	boolean edge;
	short horizontal;	/* -1 its lines' left at its left, 1 right at its right, 0 centred */
	short vertical;	/* -1 the first line at its bottom (the rest above), 1 at its top */
	long width;
	rectangle2d arrows;	/* around its labels' arrows */
	rectangle2d bounds;	/* where it is drawn (the view's coordinates) */
	boolean placed;
};

/* the separator between names on a line: a middle dot where the HUD's
font has one */
static wchar_t const *hud_waypoint_separator(
	long font_index)
{
	struct font_header *font = font_definition_get(font_index);

	if (font->character_tables.count > 0)
	{
		struct font_character_table *table = TAG_BLOCK_GET_ELEMENT(
			&font->character_tables,
			0,
			struct font_character_table);

		if (table->character_indices.count == 256 &&
			*TAG_BLOCK_GET_ELEMENT(&table->character_indices, 0xB7, short) != NONE)
		{
			return L" \x00B7 ";
		}
	}

	return L" + ";
}

/* how far drawing text moves along (its advance, trailing spaces too), at
four fifths */
static long hud_item_timers_text_advance(
	long font_index,
	wchar_t const *text)
{
	rectangle2d bounds;
	rectangle2d text_bounds;
	rectangle2d cursor_bounds;

	if (!text[0])
		return 0;
	bounds.x0 = 0;
	bounds.y0 = 0;
	bounds.x1 = SHORT_MAX / 2;
	bounds.y1 = (short)hud_item_timers_line_height(font_index);
	hud_item_timers_set_draw_mode(font_index, _text_justification_left);
	draw_unicode_string_compute_bounds(&bounds, text, &text_bounds, &cursor_bounds);

	return (long)((cursor_bounds.x0 - bounds.x0) * HUD_ITEM_TIMERS_SCALE + 0.5f);
}

/* a label's colour: its base's (red, blue), else the HUD's text */
static void hud_waypoint_label_color(
	struct item_timer const *timer,
	real_argb_color *color)
{
	switch (timer ? timer->side : _item_timer_side_middle)
	{
	case _item_timer_side_red:
		color->red = 1.0f;
		color->green = 0.3f;
		color->blue = 0.3f;
		break;

	case _item_timer_side_blue:
		color->red = 0.35f;
		color->green = 0.55f;
		color->blue = 1.0f;
		break;

	default:
		*color = hud_globals->messaging.text_color;
		break;
	}
	color->alpha = HUD_ITEM_TIMERS_LABEL_ALPHA;
}

/* a block's line: one label's "NAME 0:07" (or its name), several sharing a
time "0:07 A · B" (or "A · B"); with each part's colour (the time's and
the separators' the HUD's) and where it starts. Returns the line's width
and fills text (the whole line) */
#define HUD_WAYPOINT_MAXIMUM_PARTS 16
static long hud_waypoint_line(
	long font_index,
	struct hud_waypoint_label const *labels,
	short label_count,
	short block,
	short line,
	wchar_t *text,
	short text_size,
	short *part_count,
	long *part_starts,
	struct item_timer const **part_timers,
	short *part_ends)
{
	wchar_t const *separator = hud_waypoint_separator(font_index);
	short members = 0;
	short index;
	wchar_t const *time = L"";

	text[0] = 0;
	*part_count = 0;
	for (index = 0; index < label_count; index++)
	{
		if (labels[index].block == block && labels[index].line == line)
		{
			time = labels[index].time;
			members++;
		}
	}
	/* (several: the time first) */
	if (members > 1 && time[0] && *part_count < HUD_WAYPOINT_MAXIMUM_PARTS)
	{
		ustrncpy(text, time, text_size - 1);
		text[text_size - 1] = 0;
		part_timers[*part_count] = NULL;
		part_ends[*part_count] = (short)ustrlen(text);
		(*part_count)++;
	}
	members = 0;
	for (index = 0; index < label_count && *part_count < HUD_WAYPOINT_MAXIMUM_PARTS - 2; index++)
	{
		short used = (short)ustrlen(text);

		if (labels[index].block != block || labels[index].line != line)
			continue;
		if (used)
		{
			usnprintf(text + used, text_size - used, L"%s", members ? separator : L" ");
			text[text_size - 1] = 0;
			part_timers[*part_count] = NULL;
			part_ends[*part_count] = (short)ustrlen(text);
			(*part_count)++;
			used = (short)ustrlen(text);
		}
		usnprintf(text + used, text_size - used, L"%s", labels[index].name);
		text[text_size - 1] = 0;
		part_timers[*part_count] = labels[index].timer;
		part_ends[*part_count] = (short)ustrlen(text);
		(*part_count)++;
		members++;
	}
	/* (one: the time after it) */
	if (members == 1 && time[0] && *part_count < HUD_WAYPOINT_MAXIMUM_PARTS - 1)
	{
		short used = (short)ustrlen(text);

		usnprintf(text + used, text_size - used, L" %s", time);
		text[text_size - 1] = 0;
		part_timers[*part_count] = NULL;
		part_ends[*part_count] = (short)ustrlen(text);
		(*part_count)++;
	}

	/* (where each part starts: the advance of the text before it) */
	for (index = 0; index < *part_count; index++)
	{
		wchar_t before[HUD_ITEM_TIMERS_LINE_LENGTH];
		short length = index ? part_ends[index - 1] : 0;

		ustrncpy(before, text, length);
		before[length] = 0;
		part_starts[index] = hud_item_timers_text_advance(font_index, before);
	}

	return hud_item_timers_line_width(font_index, text);
}

/* a block's width (its widest line) */
static long hud_waypoint_block_width(
	long font_index,
	struct hud_waypoint_label const *labels,
	short label_count,
	struct hud_waypoint_block const *blocks,
	short block)
{
	long width = 0;
	short line;

	for (line = 0; line < blocks[block].line_count; line++)
	{
		wchar_t text[HUD_ITEM_TIMERS_LINE_LENGTH];
		long starts[HUD_WAYPOINT_MAXIMUM_PARTS];
		struct item_timer const *timers[HUD_WAYPOINT_MAXIMUM_PARTS];
		short ends[HUD_WAYPOINT_MAXIMUM_PARTS];
		short parts;
		long line_width = hud_waypoint_line(font_index, labels, label_count, block, line, text, NUMBEROF(text),
			&parts, starts, timers, ends);

		width = MAX(width, line_width);
	}

	return width;
}

/* whether a rectangle meets another, the second widened by margin across */
static boolean hud_waypoint_rectangles_meet(
	rectangle2d const *a,
	rectangle2d const *b,
	long margin)
{
	return a->x0 < b->x1 + margin && b->x0 - margin < a->x1 && a->y0 < b->y1 && b->y0 < a->y1;
}

/* the first of the arrows, their distances, the clock's and power list's
lines (as last drawn) and the blocks placed so far a block's rectangle
meets: the block's index, else -2 for an arrow, a distance or a line, NONE
for none */
static short hud_waypoint_obstacle(
	rectangle2d const *bounds,
	struct hud_waypoint_label const *labels,
	short label_count,
	struct hud_waypoint_block const *blocks,
	short block_count,
	short self,
	long gap)
{
	short index;

	for (index = 0; index < label_count; index++)
	{
		rectangle2d arrow;

		arrow.x0 = (short)(labels[index].arrow.x - labels[index].arrow.half_width);
		arrow.x1 = (short)(labels[index].arrow.x + labels[index].arrow.half_width);
		arrow.y0 = (short)(labels[index].arrow.y - labels[index].arrow.half_height);
		arrow.y1 = (short)(labels[index].arrow.y + labels[index].arrow.half_height);
		if (hud_waypoint_rectangles_meet(bounds, &arrow, 1))
			return -2;
		if (labels[index].arrow.number && hud_waypoint_rectangles_meet(bounds, &labels[index].arrow.number_bounds, 1))
			return -2;
	}
	if (render.local_player_index >= 0 && render.local_player_index < MAXIMUM_LOCAL_PLAYERS)
	{
		for (index = 0; index < hud_item_timers_lines[render.local_player_index].count; index++)
		{
			if (hud_waypoint_rectangles_meet(bounds, &hud_item_timers_lines[render.local_player_index].bounds[index], gap))
				return -2;
		}
	}
	for (index = 0; index < block_count; index++)
	{
		if (index != self && blocks[index].placed && hud_waypoint_rectangles_meet(bounds, &blocks[index].bounds, gap))
			return index;
	}

	return NONE;
}

/* a block's rectangle at a shift (in half lines, down; up negative), kept
in its room: across, pinned to the room; up and down, FALSE where it would
leave it */
static boolean hud_waypoint_block_bounds(
	struct hud_waypoint_block const *block,
	struct hud_waypoint_label const *anchor,
	long line_height,
	long shift,
	rectangle2d const *room,
	rectangle2d *bounds)
{
	long height = block->line_count * line_height;
	long gap = HUD_ITEM_TIMERS_LABEL_GAP;
	long x0;
	long y0;

	/* (beside or over its arrows, all of them) */
	if (block->horizontal < 0)
		x0 = block->arrows.x1 + gap;
	else if (block->horizontal > 0)
		x0 = block->arrows.x0 - gap - block->width;
	else
		x0 = anchor->arrow.x - block->width / 2;
	if (block->edge && block->horizontal)
	{
		/* (beside them: the first line level with the soonest's arrow) */
		y0 = block->vertical > 0 ? anchor->arrow.y - line_height / 2 :
			anchor->arrow.y + line_height / 2 - height;
	}
	else
	{
		/* (over them, or under them) */
		y0 = block->vertical > 0 ? block->arrows.y1 + gap : block->arrows.y0 - gap - height;
	}
	y0 += shift * line_height / 2;

	x0 = PIN(x0, room->x0, MAX(room->x1 - block->width, room->x0));
	if (y0 < room->y0 || y0 + height > room->y1)
		return FALSE;
	bounds->x0 = (short)x0;
	bounds->x1 = (short)(x0 + block->width);
	bounds->y0 = (short)y0;
	bounds->y1 = (short)(y0 + height);

	return TRUE;
}

/* a block placed where it meets nothing, its first place or moved up to
HUD_WAYPOINT_MAXIMUM_SHIFT lines (by half lines) the way it grows, else the
other way (an arrow in sight's: over it, else under it); FALSE for no room,
and
*obstacle the block it met (NONE for none, -2 an arrow or a distance) */
#define HUD_WAYPOINT_MAXIMUM_SHIFT 2
static boolean hud_waypoint_place_block(
	long font_index,
	struct hud_waypoint_label const *labels,
	short label_count,
	struct hud_waypoint_block *blocks,
	short block_count,
	short block,
	long line_height,
	long gap,
	rectangle2d const *room,
	short *obstacle)
{
	struct hud_waypoint_block *self = &blocks[block];
	short vertical = self->vertical;
	short side;

	*obstacle = NONE;
	self->width = hud_waypoint_block_width(font_index, labels, label_count, blocks, block);
	for (side = 0; side < 2; side++)
	{
		/* (an arrow in sight's: over it, else under it; at the edge, moved
		toward the middle, else away from it) */
		short direction = self->edge ? (short)(side ? -vertical : vertical) : (short)(side ? 1 : -1);
		long shift;

		if (!self->edge)
			self->vertical = direction;
		for (shift = 0; shift <= 2 * HUD_WAYPOINT_MAXIMUM_SHIFT; shift++)
		{
			rectangle2d bounds;
			short met;

			if (!hud_waypoint_block_bounds(self, &labels[self->first], line_height, direction * shift, room, &bounds))
				continue;
			met = hud_waypoint_obstacle(&bounds, labels, label_count, blocks, block_count, block, gap);
			if (met == NONE)
			{
				self->bounds = bounds;
				self->placed = TRUE;
				return TRUE;
			}
			if (*obstacle == NONE)
				*obstacle = met;
		}
	}
	self->vertical = vertical;

	return FALSE;
}

/* the labels' blocks: identical labels by each other drawn once, the
arrows at the view's edge by each other as one block (a line per time,
soonest first), the rest a block each; placed soonest first, then nearest,
each clear of every arrow, distance and block placed before it, moved at
most HUD_WAYPOINT_MAXIMUM_SHIFT lines; one that finds no room is put on the
line of the label it meets (sharing its time), else its latest lines are
left off. In the view (an arrow in sight), or HUD AREA's part of it (an
arrow at the edge) */
static void hud_waypoint_draw_labels(
	long font_index,
	struct hud_waypoint_label *labels,
	short label_count)
{
	struct hud_waypoint_block blocks[HUD_ITEM_TIMERS_MAXIMUM_ENTRIES];
	short block_count = 0;
	long line_height = (long)(hud_item_timers_line_height(font_index) * HUD_ITEM_TIMERS_SCALE + 0.5f);
	long gap = hud_item_timers_text_advance(font_index, L" ");
	rectangle2d view;
	rectangle2d area;
	long middle_x;
	long middle_y;
	short index, other;

	view.x0 = 0;
	view.y0 = 0;
	view.x1 = (short)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0);
	view.y1 = (short)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0);
	area.x0 = (short)(render.camera.window_bounds.x0 - render.camera.viewport_bounds.x0);
	area.x1 = (short)(render.camera.window_bounds.x1 - render.camera.viewport_bounds.x0);
	area.y0 = (short)(render.camera.window_bounds.y0 - render.camera.viewport_bounds.y0);
	area.y1 = (short)(render.camera.window_bounds.y1 - render.camera.viewport_bounds.y0);
	middle_x = (area.x0 + area.x1) / 2;
	middle_y = (area.y0 + area.y1) / 2;
	gap = MAX(gap, 2);

	/* (soonest first, then nearest) */
	for (index = 1; index < label_count; index++)
	{
		struct hud_waypoint_label moving = labels[index];

		for (other = index; other > 0 &&
			(labels[other - 1].ticks_left > moving.ticks_left ||
			(labels[other - 1].ticks_left == moving.ticks_left && labels[other - 1].distance > moving.distance));
			other--)
		{
			labels[other] = labels[other - 1];
		}
		labels[other] = moving;
	}

	/* (the blocks) */
	for (index = 0; index < label_count; index++)
		labels[index].block = NONE;
	for (index = 0; index < label_count && block_count < HUD_ITEM_TIMERS_MAXIMUM_ENTRIES; index++)
	{
		struct hud_waypoint_block *block;
		boolean grew;
		short line;
		wchar_t text[HUD_ITEM_TIMERS_LINE_LENGTH];
		long starts[HUD_WAYPOINT_MAXIMUM_PARTS];
		struct item_timer const *timers[HUD_WAYPOINT_MAXIMUM_PARTS];
		short ends[HUD_WAYPOINT_MAXIMUM_PARTS];
		short parts;

		if (labels[index].block != NONE)
			continue;
		/* (the same as an earlier label by it: drawn once) */
		for (other = 0; other < index; other++)
		{
			if (labels[other].block != NONE && labels[other].block != -3 &&
				!ustrcmp(labels[other].name, labels[index].name) && !ustrcmp(labels[other].time, labels[index].time) &&
				ABS(labels[other].arrow.x - labels[index].arrow.x) <= HUD_WAYPOINT_MERGE_DISTANCE &&
				ABS(labels[other].arrow.y - labels[index].arrow.y) <= HUD_WAYPOINT_MERGE_DISTANCE)
			{
				break;
			}
		}
		if (other < index)
		{
			labels[index].block = -3;
			continue;
		}

		block = &blocks[block_count];
		csmemset(block, 0, sizeof(*block));
		block->first = index;
		block->edge = labels[index].arrow.off_screen;
		labels[index].block = block_count;
		/* (the edge's arrows by it, and by those) */
		grew = block->edge;
		while (grew)
		{
			grew = FALSE;
			for (other = index + 1; other < label_count; other++)
			{
				short member;

				if (labels[other].block != NONE || !labels[other].arrow.off_screen)
					continue;
				for (member = index; member < other; member++)
				{
					if (labels[member].block == block_count &&
						ABS(labels[other].arrow.x - labels[member].arrow.x) <= HUD_WAYPOINT_MERGE_DISTANCE &&
						ABS(labels[other].arrow.y - labels[member].arrow.y) <= HUD_WAYPOINT_MERGE_DISTANCE)
					{
						break;
					}
				}
				if (member < other)
				{
					short same;

					/* (drawn once where it is the same as one in the block) */
					for (same = index; same < other; same++)
					{
						if (labels[same].block == block_count && !ustrcmp(labels[same].name, labels[other].name) &&
							!ustrcmp(labels[same].time, labels[other].time))
						{
							break;
						}
					}
					labels[other].block = same < other ? -3 : block_count;
					grew = TRUE;
				}
			}
		}
		/* (around its arrows) */
		block->arrows.x0 = SHORT_MAX;
		block->arrows.y0 = SHORT_MAX;
		block->arrows.x1 = SHORT_MIN;
		block->arrows.y1 = SHORT_MIN;
		for (other = index; other < label_count; other++)
		{
			if (labels[other].block != block_count)
				continue;
			block->arrows.x0 = (short)MIN(block->arrows.x0, labels[other].arrow.x - labels[other].arrow.half_width);
			block->arrows.x1 = (short)MAX(block->arrows.x1, labels[other].arrow.x + labels[other].arrow.half_width);
			block->arrows.y0 = (short)MIN(block->arrows.y0, labels[other].arrow.y - labels[other].arrow.half_height);
			block->arrows.y1 = (short)MAX(block->arrows.y1, labels[other].arrow.y + labels[other].arrow.half_height);
		}
		/* (a line per time, soonest first; one no wider than half the
		room, the rest of that time on the next) */
		block->line_count = 0;
		for (other = index; other < label_count; other++)
		{
			if (labels[other].block != block_count)
				continue;
			labels[other].line = NONE;
			for (line = 0; line < block->line_count; line++)
			{
				short member;

				for (member = index; member < other; member++)
				{
					if (labels[member].block == block_count && labels[member].line == line &&
						!ustrcmp(labels[member].time, labels[other].time))
					{
						break;
					}
				}
				if (member >= other)
					continue;
				labels[other].line = line;
				if (hud_waypoint_line(font_index, labels, label_count, block_count, line, text, NUMBEROF(text),
					&parts, starts, timers, ends) <= (area.x1 - area.x0) / 2)
				{
					break;
				}
				labels[other].line = NONE;
			}
			if (line == block->line_count)
			{
				labels[other].line = line;
				block->line_count++;
			}
		}
		/* (which way it grows: from the arrow toward the middle) */
		if (block->edge)
		{
			long half_width = MAX((area.x1 - area.x0) / 2, 1);
			long half_height = MAX((area.y1 - area.y0) / 2, 1);
			long across = labels[index].arrow.x - middle_x;
			long down = labels[index].arrow.y - middle_y;

			if (ABS(across) * half_height >= ABS(down) * half_width)
				block->horizontal = across > 0 ? 1 : -1;
			block->vertical = down > 0 ? -1 : 1;
		}
		else
		{
			block->vertical = -1;
		}
		block_count++;
	}

	/* (placed soonest first) */
	for (index = 0; index < block_count; index++)
	{
		struct hud_waypoint_block *block = &blocks[index];
		rectangle2d const *room = block->edge ? &area : &view;
		short obstacle;

		if (hud_waypoint_place_block(font_index, labels, label_count, blocks, block_count, index, line_height, gap,
			room, &obstacle))
		{
			continue;
		}
		/* (no room: an arrow in sight's label onto the line of the block it
		met where they share a time, if that still fits) */
		if (!block->edge && block->line_count == 1 && obstacle >= 0 && blocks[obstacle].line_count == 1 &&
			!ustrcmp(labels[blocks[obstacle].first].time, labels[block->first].time))
		{
			short first = block->first;
			short merged_obstacle;

			labels[first].block = obstacle;
			labels[first].line = 0;
			blocks[obstacle].placed = FALSE;
			if (hud_waypoint_place_block(font_index, labels, label_count, blocks, block_count, obstacle, line_height,
				gap, blocks[obstacle].edge ? &area : &view, &merged_obstacle))
			{
				continue;
			}
			/* (not that either: as it was, this one left off) */
			labels[first].block = NONE;
			hud_waypoint_place_block(font_index, labels, label_count, blocks, block_count, obstacle, line_height,
				gap, blocks[obstacle].edge ? &area : &view, &merged_obstacle);
			continue;
		}
		/* (else its latest lines left off, until it fits) */
		while (block->line_count > 1)
		{
			block->line_count--;
			for (other = 0; other < label_count; other++)
			{
				if (labels[other].block == index && labels[other].line >= block->line_count)
					labels[other].block = NONE;
			}
			if (hud_waypoint_place_block(font_index, labels, label_count, blocks, block_count, index, line_height,
				gap, room, &obstacle))
			{
				break;
			}
		}
	}

	/* (drawn: each part of each line in its colour) */
	for (index = 0; index < block_count; index++)
	{
		struct hud_waypoint_block const *block = &blocks[index];
		short line;

		if (!block->placed)
			continue;
		for (line = 0; line < block->line_count; line++)
		{
			wchar_t text[HUD_ITEM_TIMERS_LINE_LENGTH];
			long starts[HUD_WAYPOINT_MAXIMUM_PARTS];
			struct item_timer const *timers[HUD_WAYPOINT_MAXIMUM_PARTS];
			short ends[HUD_WAYPOINT_MAXIMUM_PARTS];
			short parts;
			long width = hud_waypoint_line(font_index, labels, label_count, index, line, text, NUMBEROF(text),
				&parts, starts, timers, ends);
			long x = block->horizontal < 0 ? block->bounds.x0 :
				block->horizontal > 0 ? block->bounds.x1 - width :
				block->bounds.x0 + (block->width - width) / 2;
			long top = block->vertical > 0 ? block->bounds.y0 + line * line_height :
				block->bounds.y1 - (line + 1) * line_height;
			short part;

			for (part = 0; part < parts; part++)
			{
				wchar_t piece[HUD_ITEM_TIMERS_LINE_LENGTH];
				short begin = part ? ends[part - 1] : 0;
				real_argb_color color;
				rectangle2d bounds;

				ustrncpy(piece, text + begin, ends[part] - begin);
				piece[ends[part] - begin] = 0;
				hud_waypoint_label_color(timers[part], &color);
				/* (laid out at full size from its left, drawn at four
				fifths about its top left corner) */
				bounds.x0 = (short)(x + starts[part]);
				bounds.y0 = (short)top;
				bounds.x1 = (short)(bounds.x0 + hud_item_timers_line_width(font_index, piece) / HUD_ITEM_TIMERS_SCALE + 8);
				bounds.y1 = (short)(top + hud_item_timers_line_height(font_index));
				draw_string_set_draw_mode(font_index, NONE, _text_justification_left, 0, &color);
				rasterizer_text_set_scale(HUD_ITEM_TIMERS_SCALE, (real)bounds.x0, (real)bounds.y0);
				rasterizer_draw_unicode_string(&bounds, NULL, NULL, 0, piece);
				rasterizer_text_set_scale(1.0f, 0.0f, 0.0f);
			}
		}
	}
}

/* TRAINING's waypoints over the power entries, in this local player's
view (from hud_draw_screen, where the game's nav points are drawn): every
arrow first, then their labels (hud_waypoint_draw_labels) clear of them */
void hud_draw_item_waypoints(
	short local_player_index)
{
	short count = item_timers_count();
	short nav_index = NONE;
	long font_index;
	struct hud_waypoint_label labels[HUD_ITEM_TIMERS_MAXIMUM_ENTRIES];
	short label_count = 0;
	long player_index;
	long unit_index;
	real_point3d head_position;
	short index;

	if (!count || local_player_index == NONE || !hud_globals ||
		hud_globals->waypoint.arrow_bitmap.index == NONE)
	{
		return;
	}
	font_index = hud_item_timers_font_index();
	player_index = local_player_get_player_index(local_player_index);
	unit_index = player_index == NONE ? NONE : player_get(player_index)->unit_index;
	if (unit_index == NONE)
		return;
	unit_get_head_position(unit_index, &head_position);

	for (index = 0; index < count; index++)
	{
		struct item_timer const *timer = item_timers_get(index);
		struct hud_waypoint_label *label = &labels[label_count];
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
		custom_render_nav_point_placed(local_player_index, &position, nav_index, render_type, &label->arrow);
		if (!label->arrow.drawn || font_index == NONE || label_count >= HUD_ITEM_TIMERS_MAXIMUM_ENTRIES)
			continue;

		label->timer = timer;
		label->ticks_left = item_timer_ticks_left(timer);
		label->distance = distance3d(&render.camera.position, &position);
		item_timer_waypoint_name(timer, label->name, NUMBEROF(label->name));
		label->time[0] = 0;
		if (label->ticks_left <= ITEM_TIMER_WAYPOINT_BEFORE_TICKS)
			game_engine_format_clock(label->ticks_left, TRUE, label->time, NUMBEROF(label->time));
		label_count++;
	}

	if (label_count)
		hud_waypoint_draw_labels(font_index, labels, label_count);
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
	csmemset(hud_item_timers_lines, 0, sizeof(hud_item_timers_lines));
}
