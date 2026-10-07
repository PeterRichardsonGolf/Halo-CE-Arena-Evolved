/*
HUD_ITEM_TIMERS.C

port: MATCH CLOCK (display.match_clock), CAMPAIGN TIMER and the power
list of the gametype's TIMERS and TRAINING, drawn once per local player's
view from hud_draw_screen (hud.c).

The clock is M:SS in the view's bottom right corner, as the Master Chief
Collection's (game_engine_match_clock: the time left or the time played, in
any multiplayer game; with BOTH and a time limit, the time played smaller
directly above the time left, game_engine_match_clock_elapsed): its right
edge under the right edge of the shield and health meters at the view's top
right, its digits' foot level with the motion sensor's background's foot in
the opposite corner, where the sensor's range ("15m") is. The meters' and
the sensor's places are as hud_unit.c last drew them in this view
(hud_item_timers_set_meters, hud_item_timers_set_motion_sensor). With no
meters seen (or a HUD that has them in the view's left half), the clock is
as far from the view's right edge as the sensor's background is from its
left; with no sensor seen (hidden by a script or the gametype), 4% of the
view's width from its right edge and 6% of its height from its bottom.
On Halo 1: NHE's maps it keeps out of their countdown's titles (g_*). Its
digits are tabular (each in a cell as wide as the widest digit), so that it
keeps still as the seconds tick.

The power list is the rockets', sniper's, shotgun's, overshield's and camo's next
spawns. In every view (one view's by default: display.power_list "clock")
it is a column stacked over the clock (hud_item_timers_draw_column), the
same in every view at every count of views: a line for each class and name
spawning in a second ("R/B ROCKETS", an item at both bases one line, its R
and B in the bases' colours; different classes in one second each their
own line, the time on each), two aligned columns, NAME and TIME. The times
are flush right on the clock's right edge in the same tabular figures (so
"1:20" over "0:50" has one colon line), as wide as the widest (10:00 moves
every name left together); the names flush right a digit's cell before
them. The soonest is at the foot, a cap (of the text scale) over the
clock's digits' cap top (or BOTH's time played's), and each line a cap and
a half over the one under it; with no clock it sits on the clock's
baseline; with the clock hidden a moment (NHE's countdown titles) it stays
where it was. At most four lines: the three soonest and "+N" with more, in
the time column at half alpha. A line ten seconds from its spawn or nearer
is drawn at full alpha (as the voice's "ten"). Never made smaller: at the
view's text scale. Not while the view's scoreboard shows (the clock stays).

With one view, display.power_list "top_left" (TOP LEFT) has it as before:
one line at the text scale in the view's top left corner, in at most half
its width: at the view's top, under the performance overlay's line where
the two would meet (main.c's main_framerate_extent: the overlay at the top
left, drawn at the text scale too, hud_item_timers_overlay_text_scale), and
under the HUD's own elements in that corner (the ammo, the grenades, as
hud.c saw them drawn: hud_item_timers_set_top_left) where it would meet
them (made smaller to stay over them where it nearly fits), the HUD's
messages under them moved down under it (hud_item_timers_messages_offset).
A line that does not fit its room is the shorter one (entries spawning
together grouped), then smaller (down to
HUD_ITEM_TIMERS_POWER_MINIMUM_SCALE); only at the smallest are its latest
entries left off.

With HUD AREA (display.hud_area: hud_area_insets), both keep to its part
of the view: the clock mirrors the sensor (which has moved in with the HUD)
about that part's middle, the power column with it, and the TOP LEFT line
(and the clock with no sensor) keeps to that part's sides.

CAMPAIGN TIMER (display.campaign_timer) draws the time played on a
campaign level as the clock, in the same place (hud_draw_campaign_timer).

Neither is drawn once the game is over (game_engine_game_over: the
postgame's "You won"/"You lost" view and the scores).

Both are drawn like the performance overlay (main.c frame_statistics_draw):
the HUD's smaller font, its blue, 0.7 alpha, smaller; placed to fractions of
a unit (hud_item_timers_draw_text_at). Every text of ours in
a view (the power list, the waypoints' labels and their distances, BOTH's
time played, and in the first view the performance overlay) is drawn at one
size, the view's text scale: HUD_ITEM_TIMERS_TEXT_SCALE in one view,
smaller in a smaller view (hud_item_timers_view_factor); the clock a step
larger, HUD_ITEM_TIMERS_CLOCK_SCALE (the Master Chief Collection's size in
one view), by the same rule. Its right edge is about 10 pixels inside the
meters' frame at 1080p, the Collection's about 9, being under the bar's own
end. Unlike the overlay, drawn in the HUD's pass, the bounds are the view's
relative to itself.

TRAINING's waypoints (hud_draw_item_waypoints) are the game's own nav
points, as Halo 1: NHE's Training mode's: the "default" arrow (CTF's over
a flag carrier's own base, where it scores) with the distance by it, over
each power entry's spawn point while item_timer_waypoint_shown, drawn as
the game engine draws its goals' (game_engine_render_nav_points): occluded
or not by a line of sight test from the player's head each frame, at the
view's edge pointing to it when off screen. For every player (NHE turns
them on for both teams), only while alive, as the game's nav points, and
not while the view's scoreboard shows (game_engine_scoreboard_shown). Each
has a label by its arrow (hud_waypoint_draw_labels): the item's name and
the time to its spawn, the name alone once it is on the map, in its base's
colour (item_timers.c's sides). Every arrow is drawn first; the labels are
then placed soonest first clear of the arrows, their distances, the clock
and power list and each other, arrows together at the view's edge sharing
one list beside them.
*/

/* ---------- headers */

#include <math.h>

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
#include "interface/ui_widget.h"
#include "main/main.h"
#include "rasterizer/rasterizer.h"
#include "render/render.h"
#include "text/draw_string.h"
#include "text/font_group.h"
#include "text/unicode.h"
#include "units/units.h"

/* ---------- constants */

#define HUD_ITEM_TIMERS_ALPHA 0.7f
#define HUD_WAYPOINT_ARROW_SIZE 0.6f	/* (our waypoint arrows, of CE's nav point size, in one view) */
#define HUD_ITEM_TIMERS_CLOCK_SCALE 0.9f	/* (the clock in one view: the Master Chief Collection's size, digits 20 pixels tall at 1080p) */
#define HUD_ITEM_TIMERS_TEXT_SCALE 0.7f	/* (all our other text in one view: the power list, the waypoints' labels, BOTH's time played, the performance overlay) */
#define HUD_ITEM_TIMERS_TOP 2	/* (the overlay's line's, from the view's top) */
#define HUD_ITEM_TIMERS_SIDE 4	/* (the power list's room from a view's sides, as the overlay's) */
#define HUD_ITEM_TIMERS_GAP 12	/* (between the TOP LEFT power list and the overlay) */
#define HUD_ITEM_TIMERS_CLOCK_RIGHT 0.04f	/* (of the view's width, with no motion sensor) */
#define HUD_ITEM_TIMERS_CLOCK_BOTTOM 0.06f	/* (of the view's height, with no motion sensor) */
#define HUD_ITEM_TIMERS_MAXIMUM_ENTRIES 64
#define HUD_ITEM_TIMERS_LINE_LENGTH 160
#define HUD_ITEM_TIMERS_LABEL_ALPHA 0.9f	/* (a waypoint's label, over the world) */
#define HUD_ITEM_TIMERS_LABEL_GAP 2	/* (between a waypoint's arrow and its label) */
#define HUD_WAYPOINT_MERGE_DISTANCE 24	/* (arrows this near each other share their labels) */
#define HUD_ITEM_TIMERS_POWER_MINIMUM_SCALE 0.4f	/* (the smallest it is made to fit) */
#define HUD_ITEM_TIMERS_POWER_WIDTH 0.5f	/* (the most of the view's width it takes in its top left corner) */
#define HUD_ITEM_TIMERS_POWER_ABOVE_SCALE 0.55f	/* (the smallest it is made to stay over the HUD's top left elements) */
#define HUD_ITEM_TIMERS_HUD_GAP 3	/* (between the power list and the HUD's top left elements) */
#define HUD_ITEM_TIMERS_COLUMN_LINES 4	/* (the most lines of the power column: the three soonest and "+N" with more) */
#define HUD_ITEM_TIMERS_COLUMN_PITCH 1.5f	/* (from one line of the column to the next, in caps) */
#define HUD_ITEM_TIMERS_COLUMN_GAP 1.0f	/* (from the column's foot to the clock's cap top, in caps) */
#define HUD_ITEM_TIMERS_SOON_SECONDS 10	/* (a line this near its spawn drawn brighter) */
#define HUD_ITEM_TIMERS_MORE_ALPHA 0.5f	/* (the column's "+N") */

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

/* where each local player's view last had its shield and health meters
(hud_item_timers_set_meters), and the view's size then */
static struct
{
	boolean valid;
	short view_width;
	short view_height;
	rectangle2d bounds;
} hud_item_timers_meters[MAXIMUM_LOCAL_PLAYERS];

/* where each local player's view has had the HUD's elements in its top
left corner on this map (hud_item_timers_set_top_left): around all of them
(another weapon's ammo display adds to it), so that the power list under
them does not move up and down; and the view's size then */
static struct
{
	boolean valid;
	short view_width;
	short view_height;
	rectangle2d bounds;
} hud_item_timers_top_left[MAXIMUM_LOCAL_PLAYERS];

/* how far the power list under the HUD's top left elements moves the
HUD's messages under it down in each local player's view this frame
(hud_item_timers_messages_offset) */
static short hud_item_timers_messages_offsets[MAXIMUM_LOCAL_PLAYERS];

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

static long hud_item_timers_line_width_scaled(long font_index, wchar_t const *text, real scale);

static long hud_item_timers_font_index(
	void)
{
	/* (the overlay's: the HUD's smaller font, split screen's, else its own) */
	return hud_globals->messaging.multi_player_font.index != NONE ?
		hud_globals->messaging.multi_player_font.index : hud_globals->messaging.single_player_font.index;
}

/* how much smaller a view's text is than one view's: by the view's size
on the screen, half way between the same size and the square root of its
share of the screen's area (a half-screen view 0.85, a quarter 0.75), so
that every text of ours in a view is drawn at one size (the text scale),
the clock a step larger, both smaller in smaller views */
static real hud_item_timers_view_factor_for(
	long width,
	long height)
{
	long screen_width = rasterizer_globals.reserved04.frame_bounds.x1 - rasterizer_globals.reserved04.frame_bounds.x0;
	long screen_height = rasterizer_globals.reserved04.frame_bounds.y1 - rasterizer_globals.reserved04.frame_bounds.y0;
	real share;

	if (screen_width <= 0 || screen_height <= 0 || width <= 0 || height <= 0)
		return 1.0f;
	share = ((real)width * (real)height) / ((real)screen_width * (real)screen_height);
	share = PIN(share, 0.0f, 1.0f);

	return 0.5f + 0.5f * square_root(share);
}

static real hud_item_timers_view_factor(
	void)
{
	return hud_item_timers_view_factor_for(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0,
		render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0);
}

/* this view's text scale (the power list's before it is made smaller to
fit, the waypoints' labels', BOTH's time played's) and its clock's */
static real hud_item_timers_text_scale(
	void)
{
	return HUD_ITEM_TIMERS_TEXT_SCALE * hud_item_timers_view_factor();
}

static real hud_item_timers_clock_scale(
	void)
{
	return HUD_ITEM_TIMERS_CLOCK_SCALE * hud_item_timers_view_factor();
}

/* the performance overlay's text scale (main.c): the text scale of the
first view, in whose top corner it is drawn */
real hud_item_timers_overlay_text_scale(
	void)
{
	long count = PIN(local_player_count(), 1, MAXIMUM_LOCAL_PLAYERS);
	rectangle2d view;
	rectangle2d safe;

	compute_window_bounds(0, count, &view, &safe);

	return HUD_ITEM_TIMERS_TEXT_SCALE * hud_item_timers_view_factor_for(view.x1 - view.x0, view.y1 - view.y0);
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

static void hud_item_timers_draw_line_scaled(long font_index, short top, short left, short right,
	wchar_t const *text, real scale);

/* a line (the TOP LEFT power list's) from top, flush left, inset left and
right from the view's edges, at a scale */
static void hud_item_timers_draw_line_scaled(
	long font_index,
	short top,
	short left,
	short right,
	wchar_t const *text,
	real scale)
{
	rectangle2d bounds;
	real pivot_x;

	/* (the view's own bounds, relative to it: where this pass draws) */
	bounds.x0 = left;
	bounds.x1 = (short)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0 - right);
	bounds.y0 = top;
	bounds.y1 = (short)(top + hud_item_timers_line_height(font_index));
	/* (laid out at full size, so as wide as the line's room is at its
	scale: a line that fits the room scaled is not cut at its bounds) */
	pivot_x = (real)bounds.x0;
	bounds.x1 = (short)(bounds.x0 + (bounds.x1 - bounds.x0) / scale);
	hud_item_timers_set_draw_mode(font_index, _text_justification_left);
	/* (smaller, about the line's top left corner it hangs from) */
	rasterizer_text_set_scale(scale, pivot_x, (real)bounds.y0);
	rasterizer_draw_unicode_string(&bounds, NULL, NULL, 0, text);
	rasterizer_text_set_scale(1.0f, 0.0f, 0.0f);

	/* (where it was drawn, for TRAINING's labels to keep clear of) */
	if (render.local_player_index >= 0 && render.local_player_index < MAXIMUM_LOCAL_PLAYERS &&
		hud_item_timers_lines[render.local_player_index].count < HUD_ITEM_TIMERS_MAXIMUM_LINES)
	{
		rectangle2d *drawn = &hud_item_timers_lines[render.local_player_index].bounds[
			hud_item_timers_lines[render.local_player_index].count++];

		drawn->x0 = (short)pivot_x;
		drawn->x1 = (short)(drawn->x0 + hud_item_timers_line_width_scaled(font_index, text, scale));
		drawn->y0 = bounds.y0;
		drawn->y1 = (short)(bounds.y0 + hud_item_timers_line_height(font_index) * scale + 0.5f);
	}
}

/* the width a line takes, drawn at a scale */
static long hud_item_timers_line_width_scaled(
	long font_index,
	wchar_t const *text,
	real scale)
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

	return (long)((text_bounds.x1 - text_bounds.x0) * scale + 0.5f);
}

/* a character's height over the baseline (its glyph's top: a capital's or
a digit's, the cap height), at full size */
static real hud_item_timers_cap_height(
	long font_index,
	wchar_t character)
{
	struct font_header *font = font_definition_get(font_index);
	struct font_character const *glyph = font_get_character_by_ascii_code(font, (word)character);

	if (glyph && glyph->bitmap_origin_y > 0 && glyph->bitmap_origin_y <= font->ascending_height + 2)
		return (real)glyph->bitmap_origin_y;

	return (real)font->ascending_height;
}

/* how far drawing text moves along at full size (its advance: without the
font's leading width, which every line drawn starts with), by laying it out */
static long hud_item_timers_layout_advance(
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

	return cursor_bounds.x0 - bounds.x0 - font_definition_get(font_index)->leading_width;
}

/* each ASCII character's advance in our font, laid out once (a line's
advance is its characters' added up: the layout moves each one its own
width), so that the clock's and the power column's cells and names are
measured without laying text out every frame; again for another font or
map (hud_item_timers_initialize_for_new_map) */
static struct
{
	long font_index;
	short advances[128];	/* (NONE: not laid out yet) */
} hud_item_timers_advances = { NONE };

static real hud_item_timers_advance(
	long font_index,
	wchar_t const *text,
	real scale)
{
	long advance = 0;
	wchar_t const *character;

	if (hud_item_timers_advances.font_index != font_index)
	{
		short index;

		hud_item_timers_advances.font_index = font_index;
		for (index = 0; index < (short)NUMBEROF(hud_item_timers_advances.advances); index++)
			hud_item_timers_advances.advances[index] = NONE;
	}
	for (character = text; *character; character++)
	{
		if ((unsigned long)*character >= NUMBEROF(hud_item_timers_advances.advances))
			return (real)hud_item_timers_layout_advance(font_index, text) * scale;
		if (hud_item_timers_advances.advances[*character] == NONE)
		{
			wchar_t one[2];

			one[0] = *character;
			one[1] = 0;
			hud_item_timers_advances.advances[*character] = (short)hud_item_timers_layout_advance(font_index, one);
		}
		advance += hud_item_timers_advances.advances[*character];
	}

	return (real)advance * scale;
}

/* text at a scale, its first character at x and its line's top at top (the
view's coordinates, in fractions of a unit: laid out from whole units, and
scaled about the point that moves that layout to exactly there), in a
colour */
static void hud_item_timers_draw_text_at(
	long font_index,
	real x,
	real top,
	wchar_t const *text,
	real scale,
	real_argb_color const *color)
{
	rectangle2d bounds;
	real pivot_x;
	real pivot_y;

	if (!text[0])
		return;
	/* (the line's layout starts a leading width before its first character) */
	x -= (real)font_definition_get(font_index)->leading_width * scale;
	if (scale < 0.999f || scale > 1.001f)
	{
		bounds.x0 = (short)floor(x);
		bounds.y0 = (short)floor(top);
		pivot_x = (x - (real)bounds.x0 * scale) / (1.0f - scale);
		pivot_y = (top - (real)bounds.y0 * scale) / (1.0f - scale);
	}
	else
	{
		bounds.x0 = (short)floor(x + 0.5f);
		bounds.y0 = (short)floor(top + 0.5f);
		pivot_x = (real)bounds.x0;
		pivot_y = (real)bounds.y0;
		scale = 1.0f;
	}
	/* (laid out as wide as it needs, at full size) */
	bounds.x1 = (short)(bounds.x0 + hud_item_timers_advance(font_index, text, 1.0f) + 16.0f);
	bounds.y1 = (short)(bounds.y0 + hud_item_timers_line_height(font_index));
	draw_string_set_draw_mode(font_index, NONE, _text_justification_left, 0, color);
	rasterizer_text_set_scale(scale, pivot_x, pivot_y);
	rasterizer_draw_unicode_string(&bounds, NULL, NULL, 0, text);
	rasterizer_text_set_scale(1.0f, 0.0f, 0.0f);
}

/* tabular figures: the cells every digit and the colon of a time are drawn
in at a scale (a digit's as wide as the widest digit), so that the colons
of times drawn one over another line up, and a ticking clock keeps still */
struct hud_item_timers_cells
{
	real digit;
	real colon;
};

static void hud_item_timers_cells_get(
	long font_index,
	real scale,
	struct hud_item_timers_cells *cells)
{
	wchar_t digit[2];

	cells->digit = 0.0f;
	digit[1] = 0;
	for (digit[0] = L'0'; digit[0] <= L'9'; digit[0]++)
		cells->digit = MAX(cells->digit, hud_item_timers_advance(font_index, digit, scale));
	cells->colon = hud_item_timers_advance(font_index, L":", scale);
}

/* a character's cell (a digit's, the colon's, else its own advance) */
static real hud_item_timers_cell_width(
	long font_index,
	wchar_t character,
	real scale,
	struct hud_item_timers_cells const *cells)
{
	wchar_t text[2];

	if (character >= L'0' && character <= L'9')
		return cells->digit;
	if (character == L':')
		return cells->colon;
	text[0] = character;
	text[1] = 0;

	return hud_item_timers_advance(font_index, text, scale);
}

/* a time's width in tabular figures */
static real hud_item_timers_tabular_width(
	long font_index,
	wchar_t const *text,
	real scale,
	struct hud_item_timers_cells const *cells)
{
	real width = 0.0f;

	for (; *text; text++)
		width += hud_item_timers_cell_width(font_index, *text, scale, cells);

	return width;
}

/* a time in tabular figures, flush right on right (each character at the
middle of its cell), its line's top at top; returns its width */
static real hud_item_timers_draw_tabular(
	long font_index,
	real right,
	real top,
	wchar_t const *text,
	real scale,
	real_argb_color const *color,
	struct hud_item_timers_cells const *cells)
{
	real width = hud_item_timers_tabular_width(font_index, text, scale, cells);
	real x = right - width;

	for (; *text; text++)
	{
		wchar_t character[2];
		real cell = hud_item_timers_cell_width(font_index, *text, scale, cells);

		character[0] = *text;
		character[1] = 0;
		hud_item_timers_draw_text_at(font_index,
			x + (cell - hud_item_timers_advance(font_index, character, scale)) / 2.0f, top, character, scale, color);
		x += cell;
	}

	return width;
}

/* where the clock (and the power column over it) was drawn in this view,
for TRAINING's labels to keep clear of (hud_item_timers_lines) */
static void hud_item_timers_record(
	real x0,
	real y0,
	real x1,
	real y1)
{
	if (render.local_player_index >= 0 && render.local_player_index < MAXIMUM_LOCAL_PLAYERS &&
		hud_item_timers_lines[render.local_player_index].count < HUD_ITEM_TIMERS_MAXIMUM_LINES)
	{
		rectangle2d *drawn = &hud_item_timers_lines[render.local_player_index].bounds[
			hud_item_timers_lines[render.local_player_index].count++];

		drawn->x0 = (short)floor(x0);
		drawn->y0 = (short)floor(y0);
		drawn->x1 = (short)ceil(x1);
		drawn->y1 = (short)ceil(y1);
	}
}

/* the clock's place in the view's bottom right corner, by the meters' and
the motion sensor's places: its right edge's inset from the view's right
edge, and its digits' baseline (the power column stands on it) */
static void hud_item_timers_clock_place(
	short *inset_out,
	long *baseline_out)
{
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
	/* (the meters' right edge, where they are in this view's right half;
	they have moved in with HUD AREA's part of the view already) */
	if (local_player_index >= 0 && local_player_index < MAXIMUM_LOCAL_PLAYERS &&
		hud_item_timers_meters[local_player_index].valid &&
		hud_item_timers_meters[local_player_index].view_width == view_width &&
		hud_item_timers_meters[local_player_index].view_height == view_height &&
		hud_item_timers_meters[local_player_index].bounds.x1 > view_width / 2 &&
		hud_item_timers_meters[local_player_index].bounds.x1 <= view_width)
	{
		inset = (short)(view_width - hud_item_timers_meters[local_player_index].bounds.x1);
	}
	*inset_out = inset;
	*baseline_out = baseline;
}

/* our HUD text's colour (the clock's, the power column's): the HUD's
blue */
static void hud_item_timers_color(
	real alpha,
	real_argb_color *color)
{
	*color = hud_globals->messaging.state_color;
	color->alpha = alpha;
}

/* a base's colour (red, blue: TRAINING's labels', the power column's R and
B), else the HUD's text's */
static void hud_item_timers_side_color(
	short side,
	real alpha,
	real_argb_color *color)
{
	switch (side)
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
	color->alpha = alpha;
}

/* port_config.c's */
const char *config_string(const char *name);
int config_boolean(const char *name);
unsigned long config_changes(void);

/* whether one view's power list is the TOP LEFT line (display.power_list
"top_left"), not the column over the clock ("clock", as split screen's
always is) */
static boolean hud_item_timers_power_list_top_left(
	void)
{
	static unsigned long read_at = (unsigned long)-1;
	static boolean top_left = FALSE;

	/* (read again when Settings changes it) */
	if (read_at != config_changes())
	{
		read_at = config_changes();
		top_left = !csstrcmp(config_string("display.power_list"), "top_left");
	}

	return top_left;
}

/* the clock (text) at its place (hud_item_timers_clock_place) in tabular
figures, flush right, and above it a smaller line (above: MATCH CLOCK
BOTH's time played; NULL for none) flush right to the same edge, its
baseline at the clock's line's top */
static void hud_item_timers_draw_clock(
	long font_index,
	wchar_t const *text,
	wchar_t const *above)
{
	struct font_header *font = font_definition_get(font_index);
	real view_width = (real)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0);
	real clock_scale = hud_item_timers_clock_scale();
	real text_scale = hud_item_timers_text_scale();
	struct hud_item_timers_cells cells;
	real_argb_color color;
	short inset;
	long baseline;
	real right;
	real top;
	real top_drawn;
	real width;

	hud_item_timers_clock_place(&inset, &baseline);
	right = view_width - (real)inset;
	hud_item_timers_color(HUD_ITEM_TIMERS_ALPHA, &color);
	/* (the digits' foot, their baseline, there: the line's top an ascent
	(at the clock's scale) above it) */
	top = (real)baseline - (real)font->ascending_height * clock_scale;
	hud_item_timers_cells_get(font_index, clock_scale, &cells);
	width = hud_item_timers_draw_tabular(font_index, right, top, text, clock_scale, &color, &cells);
	top_drawn = top;
	if (above && above[0])
	{
		real above_top = top - (real)font->ascending_height * text_scale;

		hud_item_timers_cells_get(font_index, text_scale, &cells);
		width = MAX(width, hud_item_timers_draw_tabular(font_index, right, above_top, above, text_scale, &color, &cells));
		top_drawn = above_top;
	}
	hud_item_timers_record(right - width, top_drawn, right,
		top + (real)hud_item_timers_line_height(font_index) * clock_scale);
}

/* the power entries (TIMERS' and TRAINING's), soonest first, and the time
left to each: their count */
static short hud_item_timers_power_entries(
	struct item_timer const **entries,
	long *left)
{
	short count = 0;
	short total = item_timers_count();
	short index, other;

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

	return count;
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

/* the power list's shorter line, entries spawning together (the same
second) grouped: the time once for each such group and its names after it
("0:10 OS/CAMO ROCKETS R/B SNIPER OS"), an item at both bases as one; as
many as fit in maximum_width at scale (the latest left off). FALSE when it
shortens nothing (no two entries share a second), unless always (a group
for each time then, as the 3 and 4 player views' line has) */
static boolean hud_item_timers_compact_line(
	long font_index,
	long maximum_width,
	real scale,
	boolean always,
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
	if (!shared && !always)
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
		if (before && hud_item_timers_line_width_scaled(font_index, line, scale) > maximum_width)
		{
			line[before] = 0;
			break;
		}
		used = (long)ustrlen(line);
	}

	return used > 0;
}

/* the power list's line (one view's TOP LEFT), every entry ("SNIPER 0:22
CAMO 0:52"), no wider than maximum_width: at base scale where it fits; else
the shorter line (hud_item_timers_compact_line) there; else smaller, as far
as HUD_ITEM_TIMERS_POWER_MINIMUM_SCALE; only then are the latest groups
left off. Returns its width, 0 for none, and the scale it is drawn at */
static long hud_item_timers_power_line(
	long font_index,
	long maximum_width,
	real base,
	wchar_t *line,
	real *scale)
{
	struct item_timer const *entries[HUD_ITEM_TIMERS_MAXIMUM_ENTRIES];
	long left[HUD_ITEM_TIMERS_MAXIMUM_ENTRIES];
	short count = hud_item_timers_power_entries(entries, left);
	short index, other;
	long used = 0;
	long width;

	*scale = base;
	line[0] = 0;
	if (!count || maximum_width <= 0)
		return 0;

	for (index = 0; index < count; index++)
	{
		wchar_t time_string[32];
		boolean merged = FALSE;

		/* (one of a class spawning with another, as the same second, is
		one; not one at the other team's base: RED SNIPER, BLUE SNIPER) */
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
		used = (long)ustrlen(line);
	}
	width = hud_item_timers_line_width_scaled(font_index, line, base);
	if (width <= maximum_width)
		return width;
	/* (the shorter line, where it is shorter: else the whole line made
	smaller) */
	{
		wchar_t whole[HUD_ITEM_TIMERS_LINE_LENGTH];

		ustrncpy(whole, line, HUD_ITEM_TIMERS_LINE_LENGTH - 1);
		whole[HUD_ITEM_TIMERS_LINE_LENGTH - 1] = 0;
		if (!hud_item_timers_compact_line(font_index, 0x7FFFFFFFL, base, FALSE, entries, left, count, line))
		{
			ustrncpy(line, whole, HUD_ITEM_TIMERS_LINE_LENGTH - 1);
			line[HUD_ITEM_TIMERS_LINE_LENGTH - 1] = 0;
		}
	}
	width = hud_item_timers_line_width_scaled(font_index, line, base);
	if (width <= maximum_width)
		return width;
	/* (smaller, to fit) */
	*scale = MAX(base * (real)(maximum_width - 1) / (real)width, HUD_ITEM_TIMERS_POWER_MINIMUM_SCALE);
	width = hud_item_timers_line_width_scaled(font_index, line, *scale);
	if (width <= maximum_width)
		return width;
	/* (at the smallest: the latest groups left off) */
	hud_item_timers_compact_line(font_index, maximum_width, *scale, TRUE, entries, left, count, line);

	return hud_item_timers_line_width_scaled(font_index, line, *scale);
}

/* port: one view's TOP LEFT power list (display.power_list "top_left";
see this file's head): a line in its top left corner, under the
performance overlay's line where they would meet and under the HUD's own
elements there (the ammo, the grenades) where it would meet them */
static void hud_item_timers_draw_powers_top_left(
	long font_index)
{
	wchar_t line[HUD_ITEM_TIMERS_LINE_LENGTH];
	short local_player_index = render.local_player_index;
	long view_width = render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0;
	long view_height = render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0;
	boolean in_range = local_player_index >= 0 && local_player_index < MAXIMUM_LOCAL_PLAYERS;
	/* (the room from the view's sides: in HUD AREA's part of it,
	hud_area_insets) */
	short area_left;
	short area_right;
	long left;
	long right;
	long width;
	long line_height;
	long top;
	real scale;

	hud_area_insets(&area_left, &area_right);
	left = HUD_ITEM_TIMERS_SIDE + area_left;
	right = HUD_ITEM_TIMERS_SIDE + area_right;

	/* (one view: the top left corner) */
	width = hud_item_timers_power_line(font_index,
		(long)((view_width - area_left - area_right) * HUD_ITEM_TIMERS_POWER_WIDTH) - HUD_ITEM_TIMERS_SIDE,
		hud_item_timers_text_scale(), line, &scale);
	if (!width)
		return;
	line_height = (long)(hud_item_timers_line_height(font_index) * scale + 0.5f);
	top = HUD_ITEM_TIMERS_TOP;
	/* (the screen's top view: under the overlay's line where they would
	meet, in the screen's coordinates) */
	{
		rectangle2d extent;

		if (render.camera.viewport_bounds.y0 <= 0 && main_framerate_shown() && main_framerate_extent(&extent))
		{
			long x0 = left + render.camera.viewport_bounds.x0;
			long x1 = x0 + width;

			if (x0 < extent.x1 + HUD_ITEM_TIMERS_GAP && x1 > extent.x0 - HUD_ITEM_TIMERS_GAP)
				top = MAX(extent.y1 - render.camera.viewport_bounds.y0, 0) + HUD_ITEM_TIMERS_TOP;
		}
	}
	/* (under the HUD's elements in the corner, where it would meet them) */
	if (in_range && hud_item_timers_top_left[local_player_index].valid &&
		hud_item_timers_top_left[local_player_index].view_width == view_width &&
		hud_item_timers_top_left[local_player_index].view_height == view_height)
	{
		rectangle2d const *corner = &hud_item_timers_top_left[local_player_index].bounds;

		if (left < corner->x1 + HUD_ITEM_TIMERS_GAP && left + width > corner->x0 - HUD_ITEM_TIMERS_GAP &&
			top < corner->y1 + HUD_ITEM_TIMERS_HUD_GAP && top + line_height > corner->y0 - HUD_ITEM_TIMERS_HUD_GAP)
		{
			/* (over them where it has nearly room, made smaller to fit;
			else under them, the HUD's messages under those ("Hold BACK
			for score") moved down under it where they would meet:
			hud_item_timers_messages_offset) */
			long full_height = hud_item_timers_line_height(font_index);
			long above = corner->y0 - HUD_ITEM_TIMERS_HUD_GAP - top;
			long messages = hud_messaging_top(local_player_index);
			long room;

			if (above >= full_height * HUD_ITEM_TIMERS_POWER_ABOVE_SCALE)
			{
				scale = MIN(scale, (real)above / (real)full_height);
				line_height = (long)(full_height * scale + 0.5f);
			}
			else
			{
				top = corner->y1 + HUD_ITEM_TIMERS_HUD_GAP;
				room = top + line_height + HUD_ITEM_TIMERS_HUD_GAP - messages;
				if (in_range && messages > corner->y1 && room > 0)
					hud_item_timers_messages_offsets[local_player_index] = (short)room;
			}
		}
	}
	hud_item_timers_draw_line_scaled(font_index, (short)top, (short)left, (short)right,
		line, scale);
}

/* ---------- the power column */

/* a line of the power column: the entries of one class and name spawning
in the same second (an item at both bases one line, R/B) */
struct hud_item_timers_column_line
{
	struct item_timer const *timer;	/* (its first) */
	long ticks;
	boolean red;
	boolean blue;
	short entries;	/* (how many: one of a numbered label's shows its number, RED ROCKETS 2) */
};

/* the second a time left is shown as (rounded up, as the clock's) */
static long hud_item_timers_second(
	long ticks)
{
	return (MAX(ticks, 0) + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND;
}

/* an entry's name without its base's word (RED SNIPER: SNIPER) */
static wchar_t const *hud_item_timers_base_name(
	struct item_timer const *timer)
{
	if (timer->side_prefix)
	{
		wchar_t const *space = ustrchr(timer->label, L' ');

		if (space)
			return space + 1;
	}

	return timer->label;
}

/* the power column's lines, soonest first: their count */
static short hud_item_timers_column_lines(
	struct hud_item_timers_column_line *lines)
{
	struct item_timer const *entries[HUD_ITEM_TIMERS_MAXIMUM_ENTRIES];
	long left[HUD_ITEM_TIMERS_MAXIMUM_ENTRIES];
	short count = hud_item_timers_power_entries(entries, left);
	short line_count = 0;
	short index;

	for (index = 0; index < count; index++)
	{
		struct item_timer const *timer = entries[index];
		short line;

		for (line = 0; line < line_count; line++)
		{
			if (hud_item_timers_second(lines[line].ticks) == hud_item_timers_second(left[index]) &&
				lines[line].timer->timer_class == timer->timer_class &&
				lines[line].timer->side_prefix == timer->side_prefix &&
				!ustrcmp(hud_item_timers_base_name(lines[line].timer), hud_item_timers_base_name(timer)))
			{
				break;
			}
		}
		if (line == line_count)
		{
			lines[line].timer = timer;
			lines[line].ticks = left[index];
			lines[line].red = FALSE;
			lines[line].blue = FALSE;
			lines[line].entries = 0;
			line_count++;
		}
		lines[line].entries++;
		if (timer->side_prefix && timer->side == _item_timer_side_red)
			lines[line].red = TRUE;
		if (timer->side_prefix && timer->side == _item_timer_side_blue)
			lines[line].blue = TRUE;
	}

	return line_count;
}

/* a line's name ("R/B ROCKETS", "R SNIPER", "OS/CAMO", "B SHOTGUN 2"), flush right on
right, its R and B in their bases' colours and the rest the HUD's; returns
its left edge */
static real hud_item_timers_draw_column_name(
	long font_index,
	real right,
	real top,
	struct hud_item_timers_column_line const *line,
	real alpha,
	real scale)
{
	wchar_t texts[4][24];
	real_argb_color colors[4];
	short count = 0;
	short index;
	real width = 0.0f;
	real x;

	if (line->red || line->blue)
	{
		if (line->red)
		{
			ustrncpy(texts[count], L"R", NUMBEROF(texts[count]));
			hud_item_timers_side_color(_item_timer_side_red, alpha, &colors[count++]);
		}
		if (line->red && line->blue)
		{
			ustrncpy(texts[count], L"/", NUMBEROF(texts[count]));
			hud_item_timers_color(alpha, &colors[count++]);
		}
		if (line->blue)
		{
			ustrncpy(texts[count], L"B", NUMBEROF(texts[count]));
			hud_item_timers_side_color(_item_timer_side_blue, alpha, &colors[count++]);
		}
		usnprintf(texts[count], NUMBEROF(texts[count]), L" %s", hud_item_timers_base_name(line->timer));
	}
	else
	{
		usnprintf(texts[count], NUMBEROF(texts[count]), L"%s", line->timer->label);
	}
	texts[count][NUMBEROF(texts[count]) - 1] = 0;
	/* (one entry of a label the map has twice on a side: its number) */
	if (line->entries == 1 && line->timer->number > 0)
	{
		short used = (short)ustrlen(texts[count]);

		usnprintf(texts[count] + used, NUMBEROF(texts[count]) - used, L" %d", (int)line->timer->number);
		texts[count][NUMBEROF(texts[count]) - 1] = 0;
	}
	hud_item_timers_color(alpha, &colors[count++]);

	for (index = 0; index < count; index++)
		width += hud_item_timers_advance(font_index, texts[index], scale);
	x = right - width;
	for (index = 0; index < count; index++)
	{
		hud_item_timers_draw_text_at(font_index, x, top, texts[index], scale, &colors[index]);
		x += hud_item_timers_advance(font_index, texts[index], scale);
	}

	return right - width;
}

/* port: the power column (see this file's head), stacked over the clock in
the view's bottom right corner: two aligned columns, NAME  TIME, the times
flush right on the clock's right edge in tabular figures (their colons in
one line), the names flush right a digit's cell before the widest time;
the soonest at the foot, a cap over the clock's digits (or BOTH's time
played), each line a cap and a half over the one under it; at most four
lines, the three soonest and "+N" over them with more. Not while this
view's scoreboard shows (the clock stays) */
static void hud_item_timers_draw_column(
	long font_index)
{
	struct hud_item_timers_column_line lines[HUD_ITEM_TIMERS_MAXIMUM_ENTRIES];
	struct font_header *font = font_definition_get(font_index);
	real view_width = (real)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0);
	real scale = hud_item_timers_text_scale();
	real clock_scale = hud_item_timers_clock_scale();
	real cap = hud_item_timers_cap_height(font_index, L'H') * scale;
	real ascent = (real)font->ascending_height * scale;
	real pitch = HUD_ITEM_TIMERS_COLUMN_PITCH * cap;
	struct hud_item_timers_cells cells;
	real_argb_color color;
	wchar_t clock[32];
	wchar_t elapsed[32];
	wchar_t time[32];
	short count;
	short shown;
	short line;
	short inset;
	long baseline;
	real right;
	real lowest;
	real time_width = 0.0f;
	real name_right;
	real x0;
	real y0;

	if (render.local_player_index < 0 || render.local_player_index >= MAXIMUM_LOCAL_PLAYERS ||
		game_engine_scoreboard_shown(render.local_player_index) > 0.0f ||
		game_engine_scoreboard_held(render.local_player_index))
	{
		return;
	}
	count = hud_item_timers_column_lines(lines);
	if (!count)
		return;
	shown = count > HUD_ITEM_TIMERS_COLUMN_LINES ? HUD_ITEM_TIMERS_COLUMN_LINES - 1 : count;

	/* (the clock's right edge, and its place: its digits' cap top, or BOTH's
	time played's, whether the clock is drawn this moment or not (Halo 1:
	NHE's countdown titles hide it), so that the column keeps still; with
	no clock, its baseline) */
	hud_item_timers_clock_place(&inset, &baseline);
	right = view_width - (real)inset;
	lowest = (real)baseline;
	if (game_engine_match_clock(clock, NUMBEROF(clock)))
	{
		real cap_top = (real)baseline - hud_item_timers_cap_height(font_index, L'0') * clock_scale;

		if (game_engine_match_clock_elapsed(elapsed, NUMBEROF(elapsed)))
		{
			cap_top = (real)baseline - (real)font->ascending_height * clock_scale -
				hud_item_timers_cap_height(font_index, L'0') * scale;
		}
		lowest = cap_top - HUD_ITEM_TIMERS_COLUMN_GAP * cap;
	}

	/* (the widest time's cell: all the names a digit's cell before it) */
	hud_item_timers_cells_get(font_index, scale, &cells);
	for (line = 0; line < shown; line++)
	{
		game_engine_format_clock(lines[line].ticks, TRUE, time, NUMBEROF(time));
		time_width = MAX(time_width, hud_item_timers_tabular_width(font_index, time, scale, &cells));
	}
	name_right = right - time_width - cells.digit;
	x0 = right - time_width;
	y0 = lowest;

	for (line = 0; line < shown; line++)
	{
		real top = lowest - pitch * (real)line - ascent;
		/* (one ten seconds away or nearer, as the voice's "ten", brighter) */
		real alpha = hud_item_timers_second(lines[line].ticks) <= HUD_ITEM_TIMERS_SOON_SECONDS ? 1.0f :
			HUD_ITEM_TIMERS_ALPHA;

		hud_item_timers_color(alpha, &color);
		game_engine_format_clock(lines[line].ticks, TRUE, time, NUMBEROF(time));
		hud_item_timers_draw_tabular(font_index, right, top, time, scale, &color, &cells);
		x0 = MIN(x0, hud_item_timers_draw_column_name(font_index, name_right, top, &lines[line], alpha, scale));
		y0 = top;
	}
	if (count > shown)
	{
		real top = lowest - pitch * (real)shown - ascent;
		real width;

		usnprintf(time, NUMBEROF(time), L"+%d", (int)(count - shown));
		time[NUMBEROF(time) - 1] = 0;
		hud_item_timers_color(HUD_ITEM_TIMERS_MORE_ALPHA, &color);
		width = hud_item_timers_draw_tabular(font_index, right, top, time, scale, &color, &cells);
		x0 = MIN(x0, right - width);
		y0 = top;
	}
	/* (one rectangle round it all, for TRAINING's labels to keep clear of) */
	hud_item_timers_record(x0, y0, right, lowest + 1.0f);
}

/* how far the HUD's messages move down in this local player's view, under
the power list (drawn before them, hud_draw_screen), this frame: read once,
then 0 until the power list moves them again */
short hud_item_timers_messages_offset(
	short local_player_index)
{
	short offset;

	if (local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS)
		return 0;
	offset = hud_item_timers_messages_offsets[local_player_index];
	hud_item_timers_messages_offsets[local_player_index] = 0;

	return offset;
}

void hud_draw_item_timers(
	void)
{
	long font_index = hud_item_timers_font_index();
	wchar_t clock[32];
	wchar_t elapsed[32];

	if (render.local_player_index >= 0 && render.local_player_index < MAXIMUM_LOCAL_PLAYERS)
	{
		hud_item_timers_lines[render.local_player_index].count = 0;
		hud_item_timers_messages_offsets[render.local_player_index] = 0;
	}
	if (font_index == NONE)
		return;
	/* (not over Halo 1: NHE's maps' countdown) */
	if (game_engine_match_clock(clock, NUMBEROF(clock)) && !cinematic_nhe_countdown_title_showing())
	{
		hud_item_timers_draw_clock(font_index, clock,
			game_engine_match_clock_elapsed(elapsed, NUMBEROF(elapsed)) ? elapsed : NULL);
	}
	/* (the gametype's TIMERS and TRAINING; not once the game is over, over
	the postgame's view, as the clock is not) */
	if (game_engine_item_timers() && !game_engine_game_over())
	{
		if (local_player_count() <= 1 && hud_item_timers_power_list_top_left())
			hud_item_timers_draw_powers_top_left(font_index);
		else
			hud_item_timers_draw_column(font_index);
	}

	return;
}

/* ---------- CAMPAIGN TIMER */

/* port: CAMPAIGN TIMER (display.campaign_timer), the Master Chief
Collection's campaign play clock: the time played on this level, M:SS where
MATCH CLOCK's clock is. Game time: a tick at a time (game_tick), so it
stops with the game (the pause menu, loading) and counts the cutscenes
(drawn with no HUD over them, as MCC's scoreboard is not). Kept outside the
game state, so a revert to a checkpoint does not take back the time played
since it (played again, it is played twice); a new level, or the mission
started again, starts it at 0:00, and a saved game resumed from the main
menu at its game time (hud_campaign_timer_game_state_loaded). Every view
of co-op has it, the same */
static long hud_campaign_timer_ticks = 0;

void hud_campaign_timer_tick(
	void)
{
	if (!game_engine_running() && !main_menu_is_active())
		hud_campaign_timer_ticks++;

	return;
}

/* a game state loaded (game_state.c's after load procs): a revert keeps the
time played, but a saved game loaded before any tick of this map (the
campaign's resume) starts from the saved game's time */
void hud_campaign_timer_game_state_loaded(
	void)
{
	if (hud_campaign_timer_ticks == 0)
		hud_campaign_timer_ticks = MAX(game_time_get(), 0);

	return;
}

static boolean hud_campaign_timer_shown(
	void)
{
	static unsigned long read_at = (unsigned long)-1;
	static boolean shown = FALSE;

	/* (read again when Settings changes it) */
	if (read_at != config_changes())
	{
		read_at = config_changes();
		shown = config_boolean("display.campaign_timer") != 0;
	}

	return shown;
}

/* whether a clock is drawn now: MATCH CLOCK's (game_engine_match_clock),
or CAMPAIGN TIMER's (hud_draw_campaign_timer) */
boolean hud_item_timers_clock_shown(
	void)
{
	wchar_t clock[32];

	if (game_engine_running())
		return game_engine_match_clock(clock, NUMBEROF(clock));
	return !main_menu_is_active() && hud_campaign_timer_shown();
}

void hud_draw_campaign_timer(
	void)
{
	long font_index;
	wchar_t clock[32];

	if (render.local_player_index >= 0 && render.local_player_index < MAXIMUM_LOCAL_PLAYERS)
		hud_item_timers_lines[render.local_player_index].count = 0;
	/* (not on the main menu's map, whose player the menus stand over) */
	if (game_engine_running() || main_menu_is_active() || !hud_campaign_timer_shown())
		return;
	font_index = hud_item_timers_font_index();
	if (font_index == NONE)
		return;
	game_engine_format_clock(hud_campaign_timer_ticks, FALSE, clock, NUMBEROF(clock));
	hud_item_timers_draw_clock(font_index, clock, NULL);

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

	return (long)((cursor_bounds.x0 - bounds.x0) * hud_item_timers_text_scale() + 0.5f);
}

/* a label's colour: its base's (red, blue), else the HUD's text */
static void hud_waypoint_label_color(
	struct item_timer const *timer,
	real_argb_color *color)
{
	hud_item_timers_side_color(timer ? timer->side : _item_timer_side_middle, HUD_ITEM_TIMERS_LABEL_ALPHA, color);
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

	return hud_item_timers_line_width_scaled(font_index, text, hud_item_timers_text_scale());
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
	long line_height = (long)(hud_item_timers_line_height(font_index) * hud_item_timers_text_scale() + 0.5f);
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

	/* (an arrow must not be left without a label: one that found no clear
	place goes by the arrow, over or under it, clear of the labels placed
	if it can be, else on them) */
	for (index = 0; index < block_count; index++)
	{
		struct hud_waypoint_block *block = &blocks[index];
		rectangle2d const *room = block->edge ? &area : &view;
		short side;

		if (block->placed)
			continue;
		block->width = hud_waypoint_block_width(font_index, labels, label_count, blocks, index);
		for (other = 0; other < label_count; other++)
		{
			if (labels[other].block == index && labels[other].line >= block->line_count)
				labels[other].block = NONE;
		}
		for (side = 0; side < 4 && !block->placed; side++)
		{
			short direction = (short)(side & 1 ? 1 : -1);
			long shift;

			if (!block->edge)
				block->vertical = direction;
			for (shift = 0; shift <= 4 && !block->placed; shift++)
			{
				rectangle2d bounds;
				short met;

				if (!hud_waypoint_block_bounds(block, &labels[block->first], line_height, direction * shift, room, &bounds))
					continue;
				/* (side 0 and 1: clear of the blocks; 2 and 3: wherever) */
				met = NONE;
				if (side < 2)
				{
					short test;

					for (test = 0; test < block_count; test++)
					{
						if (test != index && blocks[test].placed &&
							hud_waypoint_rectangles_meet(&bounds, &blocks[test].bounds, 1))
						{
							met = test;
							break;
						}
					}
				}
				if (met == NONE)
				{
					block->bounds = bounds;
					block->placed = TRUE;
				}
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
				/* (laid out at full size from its left, drawn at the
				view's text scale about its top left corner) */
				bounds.x0 = (short)(x + starts[part]);
				bounds.y0 = (short)top;
				bounds.x1 = (short)(bounds.x0 + hud_item_timers_line_width_scaled(font_index, piece, hud_item_timers_text_scale()) / hud_item_timers_text_scale() + 8);
				bounds.y1 = (short)(top + hud_item_timers_line_height(font_index));
				draw_string_set_draw_mode(font_index, NONE, _text_justification_left, 0, &color);
				rasterizer_text_set_scale(hud_item_timers_text_scale(), (real)bounds.x0, (real)bounds.y0);
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
	/* (none while this view's scoreboard shows, even fading, or opens this
	frame: they would be drawn over its text) */
	if (game_engine_scoreboard_shown(local_player_index) > 0.0f || game_engine_scoreboard_held(local_player_index))
		return;
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
		/* (no distance by the arrow while its label shows the time to the
		spawn: the label says what matters then, and the game's distance
		digits were larger than it) */
		custom_render_nav_point_placed(local_player_index, &position, nav_index, render_type,
			HUD_WAYPOINT_ARROW_SIZE * hud_item_timers_view_factor(),
			font_index == NONE || item_timer_ticks_left(timer) > ITEM_TIMER_WAYPOINT_BEFORE_TICKS, &label->arrow);
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

/* where hud_unit.c drew the player's shield and health meters in this
local player's view (the view's coordinates) */
void hud_item_timers_set_meters(
	short local_player_index,
	rectangle2d const *bounds)
{
	if (local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS)
		return;
	hud_item_timers_meters[local_player_index].valid = TRUE;
	hud_item_timers_meters[local_player_index].view_width =
		(short)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0);
	hud_item_timers_meters[local_player_index].view_height =
		(short)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0);
	hud_item_timers_meters[local_player_index].bounds = *bounds;
}

/* where the weapon's and unit's HUD drew their elements in this local
player's view's top left corner (hud.c; the view's coordinates), NULL for
none: added to those seen before in a view of this size */
void hud_item_timers_set_top_left(
	short local_player_index,
	rectangle2d const *bounds)
{
	short view_width = (short)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0);
	short view_height = (short)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0);

	if (local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS || !bounds)
		return;
	if (hud_item_timers_top_left[local_player_index].valid &&
		hud_item_timers_top_left[local_player_index].view_width == view_width &&
		hud_item_timers_top_left[local_player_index].view_height == view_height)
	{
		rectangle2d *seen = &hud_item_timers_top_left[local_player_index].bounds;

		seen->x0 = MIN(seen->x0, bounds->x0);
		seen->x1 = MAX(seen->x1, bounds->x1);
		seen->y0 = MIN(seen->y0, bounds->y0);
		seen->y1 = MAX(seen->y1, bounds->y1);
		return;
	}
	hud_item_timers_top_left[local_player_index].valid = TRUE;
	hud_item_timers_top_left[local_player_index].view_width = view_width;
	hud_item_timers_top_left[local_player_index].view_height = view_height;
	hud_item_timers_top_left[local_player_index].bounds = *bounds;
}

/* a new map: no motion sensor or meters seen yet */
void hud_item_timers_initialize_for_new_map(
	void)
{
	csmemset(hud_item_timers_motion_sensors, 0, sizeof(hud_item_timers_motion_sensors));
	csmemset(hud_item_timers_meters, 0, sizeof(hud_item_timers_meters));
	csmemset(hud_item_timers_top_left, 0, sizeof(hud_item_timers_top_left));
	/* (and the meters' texels read for it) */
	hud_element_bounds_new_map();
	/* (our font's advances, laid out again on this map) */
	hud_item_timers_advances.font_index = NONE;
	/* (port: CAMPAIGN TIMER's level starts) */
	hud_campaign_timer_ticks = 0;
	csmemset(hud_item_timers_lines, 0, sizeof(hud_item_timers_lines));
}
