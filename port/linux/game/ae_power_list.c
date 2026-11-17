/*
AE_POWER_LIST.C

HUD STYLE AE's timers list, T1 (ae_power_list.h).
*/

#include <string.h>

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "game/game_engine.h"
#include "game/item_timers.h"
#include "game/players.h"
#include "interface/hud_draw.h"
#include "render/render.h"
#include "text/unicode.h"

#include "ae_hud_digits.h"
#include "ae_hud_style.h"
#include "ae_hud_text.h"
#include "ae_panel_engine.h"
#include "ae_panel_layout.h"
#include "ae_power_list.h"
#include "ae_score_strip.h"
#include "ae_strip_engine.h"
#include "ae_strip_layout.h"

const char *config_string(const char *name);
unsigned long config_changes(void);
unsigned long ae_config_player_changes(void);
/* hud_item_timers.c's (AE hooks): the clock's place (HUD units); the HUD's corner elements' extent; a rectangle
TRAINING's labels keep clear of */
void hud_item_timers_ae_place(short *inset, long *baseline);
boolean hud_item_timers_ae_corner(short local_player_index, rectangle2d *bounds);
void hud_item_timers_ae_record(real x0, real y0, real x1, real y1);

#define AE_LIST_TEXT_PX 14.0f
#define AE_LIST_TIME_FLOOR 14.0f
#define AE_LIST_MORE_PX 13.0f
#define AE_LIST_MORE_ALPHA 0.60f
#define AE_LIST_PITCH 20.0f
#define AE_LIST_LIFT 8.0f       /* px x scale over the shared bottom line (owner 10-15: it hung lower than the sensor) */
#define AE_LIST_NAME_GAP 10.0f
#define AE_LIST_LINES 4
#define AE_LIST_MAXIMUM 32
#define AE_LIST_LINE_LENGTH 128
#define AE_LIST_CLOCK_GLOW_ALPHA 0.85f  /* the strip's (AE_STRIP_GLOW_ALPHA) */

static const real white_rgb[3] = { 229.0f / 255.0f, 240.0f / 255.0f, 1.0f };
static const real grey_rgb[3] = { 188.0f / 255.0f, 198.0f / 255.0f, 212.0f / 255.0f };
static const real clock_rgb[3] = { 232.0f / 255.0f, 236.0f / 255.0f, 242.0f / 255.0f };  /* the strip's clock */
static const real red_rgb[3] = { 1.0f, 0.3f, 0.3f };       /* hud_item_timers_side_color */
static const real blue_rgb[3] = { 0.35f, 0.55f, 1.0f };

static struct { boolean drawn; real foot, left, top, right; } list_views[MAXIMUM_LOCAL_PLAYERS];

static boolean valid_view(short view)
{
	return (boolean)(view >= 0 && view < MAXIMUM_LOCAL_PLAYERS);
}

static void set_color(real const rgb[3], real alpha, real_argb_color *color)
{
	color->alpha = alpha;
	color->red = rgb[0];
	color->green = rgb[1];
	color->blue = rgb[2];
}

boolean ae_power_list_hides_clock(short local_player_index)
{
	struct ae_strip_snapshot snapshot;

	return (boolean)(ae_hud_style_is_ae() && ae_strip_engine_snapshot(local_player_index, &snapshot));
}

real ae_power_list_right_x(short local_player_index)
{
	struct ae_panel_view view;

	/* (one right column: the AE panels' own, spec 14.15 part 3) */
	ae_panel_engine_view(local_player_index, &view);
	return ae_panel_right_column_x(&view);
}

real ae_power_list_left_x(short local_player_index)
{
	short area_left = 0, area_right = 0;

	if (valid_view(local_player_index) && list_views[local_player_index].drawn)
		return list_views[local_player_index].left;
	/* (no list: the view's right, HUD AREA's side with it) */
	hud_area_insets(&area_left, &area_right);
	return (real)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0 - area_right) *
		ae_hud_pixels_per_unit();
}

boolean ae_power_list_extent(short local_player_index, real extent[4])
{
	if (!valid_view(local_player_index) || !list_views[local_player_index].drawn)
		return FALSE;
	extent[0] = list_views[local_player_index].left;
	extent[1] = list_views[local_player_index].top;
	extent[2] = list_views[local_player_index].right;
	extent[3] = list_views[local_player_index].foot + 2.0f;
	return TRUE;
}

boolean ae_power_list_last(short local_player_index, real *foot_baseline, real *left_x)
{
	if (!valid_view(local_player_index) || !list_views[local_player_index].drawn)
		return FALSE;
	*foot_baseline = list_views[local_player_index].foot;
	*left_x = list_views[local_player_index].left;
	return TRUE;
}

/* ---------- the lines (hud_item_timers.c's column's grouping: one class and name spawning in the same second is
one line; an item at both bases R/B) */

struct list_line { struct item_timer const *timer; long ticks; boolean red, blue; short entries; };

static long second_of(long ticks)
{
	return (MAX(ticks, 0) + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND;
}

static wchar_t const *base_name(struct item_timer const *timer)
{
	if (timer->side_prefix)
	{
		wchar_t const *space = ustrchr(timer->label, L' ');

		if (space)
			return space + 1;
	}
	return timer->label;
}

static short list_lines(struct list_line *lines)
{
	struct item_timer const *entries[AE_LIST_MAXIMUM];
	long left[AE_LIST_MAXIMUM];
	short count = 0, total = item_timers_count(), index, other, line_count = 0;

	for (index = 0; index < total && count < AE_LIST_MAXIMUM; index++)
	{
		struct item_timer const *timer = item_timers_get(index);
		long ticks;

		if (!timer || timer->timer_class >= NUMBER_OF_ITEM_TIMER_POWER_CLASSES)
			continue;
		ticks = item_timer_ticks_left(timer);
		for (other = count; other > 0 && left[other - 1] > ticks; other--)
		{
			entries[other] = entries[other - 1];
			left[other] = left[other - 1];
		}
		entries[other] = timer;
		left[other] = ticks;
		count++;
	}
	for (index = 0; index < count; index++)
	{
		short line;

		for (line = 0; line < line_count; line++)
		{
			if (second_of(lines[line].ticks) == second_of(left[index]) &&
				lines[line].timer->timer_class == entries[index]->timer_class &&
				lines[line].timer->side_prefix == entries[index]->side_prefix &&
				!ustrcmp(base_name(lines[line].timer), base_name(entries[index])))
			{
				break;
			}
		}
		if (line == line_count)
		{
			lines[line].timer = entries[index];
			lines[line].ticks = left[index];
			lines[line].red = lines[line].blue = FALSE;
			lines[line].entries = 0;
			line_count++;
		}
		lines[line].entries++;
		if (entries[index]->side_prefix && entries[index]->side == _item_timer_side_red)
			lines[line].red = TRUE;
		if (entries[index]->side_prefix && entries[index]->side == _item_timer_side_blue)
			lines[line].blue = TRUE;
	}
	return line_count;
}

/* a line's name parts: R / "/" / B in their colours, then the name (caps, grey) */
struct name_part { wchar_t text[32]; real const *rgb; };

static short name_parts(struct list_line const *line, struct name_part *parts)
{
	short count = 0;

	if (line->red || line->blue)
	{
		if (line->red)
		{
			ustrncpy(parts[count].text, L"R", 31);
			parts[count++].rgb = red_rgb;
		}
		if (line->red && line->blue)
		{
			ustrncpy(parts[count].text, L"/", 31);
			parts[count++].rgb = grey_rgb;
		}
		if (line->blue)
		{
			ustrncpy(parts[count].text, L"B", 31);
			parts[count++].rgb = blue_rgb;
		}
		usnprintf(parts[count].text, 32, L" %s", base_name(line->timer));
	}
	else
		usnprintf(parts[count].text, 32, L"%s", line->timer->label);
	parts[count].text[31] = 0;
	if (line->entries == 1 && line->timer->number > 0)
	{
		short used = (short)ustrlen(parts[count].text);

		usnprintf(parts[count].text + used, 32 - used, L" %d", (int)line->timer->number);
		parts[count].text[31] = 0;
	}
	parts[count++].rgb = grey_rgb;
	return count;
}

/* tabular times: each digit in the widest digit's cell, the colon its own width; right-aligned on right */
static real digit_cell(real px)
{
	static const wchar_t *digits[] = { L"0", L"1", L"2", L"3", L"4", L"5", L"6", L"7", L"8", L"9" };
	real widest = 0.0f;
	short i;

	for (i = 0; i < 10; i++)
		widest = MAX(widest, ae_hud_text_measure(digits[i], px));
	return widest;
}

static real time_width(wchar_t const *time, real px, real cell)
{
	real width = 0.0f;

	for (; *time; time++)
	{
		wchar_t one[2] = { *time, 0 };

		width += (*time >= L'0' && *time <= L'9') ? cell : ae_hud_text_measure(one, px);
	}
	return width;
}

static void draw_time(short view, wchar_t const *time, real right, real baseline, real px, real cell,
	real_argb_color const *color)
{
	real x = right - time_width(time, px, cell);

	for (; *time; time++)
	{
		wchar_t one[2] = { *time, 0 };
		boolean digit = (boolean)(*time >= L'0' && *time <= L'9');
		real width = digit ? cell : ae_hud_text_measure(one, px);

		ae_hud_text_draw(view, x + (digit ? (cell - ae_hud_text_measure(one, px)) / 2.0f : 0.0f), baseline, one, px,
			color, TRUE);
		x += width;
	}
}

static real draw_name(short view, struct list_line const *line, real right, real baseline, real px)
{
	struct name_part parts[4];
	short count = name_parts(line, parts), i;
	real width = 0.0f, x;

	for (i = 0; i < count; i++)
		width += ae_hud_text_measure(parts[i].text, px);
	x = right - width;
	for (i = 0; i < count; i++)
	{
		real_argb_color color;

		set_color(parts[i].rgb, 1.0f, &color);
		ae_hud_text_draw(view, x, baseline, parts[i].text, px, &color, TRUE);
		x += ae_hud_text_measure(parts[i].text, px);
	}
	return right - width;
}

/* the view's scale: the strip's (view factor x the screen's height / 1080) */
static real view_scale(void)
{
	return ae_score_strip_view_scale();
}

/* the list on the right: rows up from the shared line, right-aligned on the right-hand column */
static void draw_column(short view)
{
	struct list_line lines[AE_LIST_MAXIMUM];
	short count = list_lines(lines), shown, line;
	real s = view_scale(), ppu = ae_hud_pixels_per_unit();
	real name_px = ae_strip_text_px(AE_LIST_TEXT_PX, s, s);
	real time_px = MAX(AE_LIST_TEXT_PX * s, AE_LIST_TIME_FLOOR);
	real cell = digit_cell(time_px);
	real right = ae_power_list_right_x(view);
	real foot = ae_score_strip_shared_line_y(view) - AE_LIST_LIFT * s;
	real pitch = AE_LIST_PITCH * s;
	real widest = 0.0f, name_right, left = right, top;
	real_argb_color white;
	wchar_t time[32];

	if (!count)
		return;
	shown = count > AE_LIST_LINES ? AE_LIST_LINES - 1 : count;
	for (line = 0; line < shown; line++)
	{
		game_engine_format_clock(lines[line].ticks, TRUE, time, NUMBEROF(time));
		widest = MAX(widest, time_width(time, time_px, cell));
	}
	name_right = right - widest - AE_LIST_NAME_GAP * s;
	set_color(white_rgb, 1.0f, &white);
	for (line = 0; line < shown; line++)
	{
		real baseline = foot - pitch * (real)line;

		game_engine_format_clock(lines[line].ticks, TRUE, time, NUMBEROF(time));
		draw_time(view, time, right, baseline, time_px, cell, &white);
		left = MIN(left, draw_name(view, &lines[line], name_right, baseline, name_px));
	}
	top = foot - pitch * (real)(shown - 1) - name_px * AE_HUD_TEXT_CAP;
	if (count > shown)
	{
		real more_px = ae_strip_text_px(AE_LIST_MORE_PX, s, s);
		real baseline = foot - pitch * (real)shown;
		real_argb_color grey;
		real width;

		usnprintf(time, NUMBEROF(time), L"+%d MORE", (int)(count - shown));
		time[NUMBEROF(time) - 1] = 0;
		width = ae_hud_text_measure(time, more_px);
		set_color(grey_rgb, AE_LIST_MORE_ALPHA, &grey);
		ae_hud_text_draw(view, name_right - width, baseline, time, more_px, &grey, TRUE);
		left = MIN(left, name_right - width);
		top = baseline - more_px * AE_HUD_TEXT_CAP;
	}
	list_views[view].drawn = TRUE;
	list_views[view].foot = foot;
	list_views[view].left = left;
	list_views[view].top = top;
	list_views[view].right = right;
	/* (for TRAINING's labels to keep clear of, in the HUD's units) */
	hud_item_timers_ae_record(left / ppu, top / ppu, right / ppu, (foot + 2.0f) / ppu);
}

/* TOP LEFT (one view; 0.7.0, the owner-approved HUD corners): the bottom-right column mirrored, hanging under the ammo
cluster on its left edge: a time column on the outside (left), the names after it, "+N MORE" under the shown rows.
With no ammo cluster drawn (none recorded): at the screen's top left */
static void draw_top_left(short view)
{
	struct list_line lines[AE_LIST_MAXIMUM];
	short count = list_lines(lines), shown, line;
	real s = view_scale(), ppu = ae_hud_pixels_per_unit();
	real name_px = ae_strip_text_px(AE_LIST_TEXT_PX, s, s);
	real time_px = MAX(AE_LIST_TEXT_PX * s, AE_LIST_TIME_FLOOR);
	real cell = digit_cell(time_px);
	real pitch = AE_LIST_PITCH * s;
	short area_left = 0, area_right = 0;
	rectangle2d corner;
	real left, first, widest = 0.0f, time_right, name_left, right = 0.0f, foot;
	real_argb_color white;
	wchar_t time[32];

	if (!count)
		return;
	hud_area_insets(&area_left, &area_right);
	left = (real)(area_left + 4) * ppu;
	first = 8.0f * s + name_px * AE_HUD_TEXT_CAP;
	if (hud_item_timers_ae_corner(view, &corner))
	{
		left = (real)corner.x0 * ppu;
		first = (real)corner.y1 * ppu + 8.0f * s + name_px * AE_HUD_TEXT_CAP;
	}
	shown = count > AE_LIST_LINES ? AE_LIST_LINES - 1 : count;
	for (line = 0; line < shown; line++)
	{
		game_engine_format_clock(lines[line].ticks, TRUE, time, NUMBEROF(time));
		widest = MAX(widest, time_width(time, time_px, cell));
	}
	time_right = left + widest;
	name_left = time_right + AE_LIST_NAME_GAP * s;
	set_color(white_rgb, 1.0f, &white);
	for (line = 0; line < shown; line++)
	{
		real baseline = first + pitch * (real)line;
		struct name_part parts[4];
		short parts_count = name_parts(&lines[line], parts), i;
		real x = name_left;

		game_engine_format_clock(lines[line].ticks, TRUE, time, NUMBEROF(time));
		draw_time(view, time, time_right, baseline, time_px, cell, &white);
		for (i = 0; i < parts_count; i++)
		{
			real_argb_color color;

			set_color(parts[i].rgb, 1.0f, &color);
			ae_hud_text_draw(view, x, baseline, parts[i].text, name_px, &color, TRUE);
			x += ae_hud_text_measure(parts[i].text, name_px);
		}
		right = MAX(right, x);
	}
	foot = first + pitch * (real)(shown - 1);
	if (count > shown)
	{
		real more_px = ae_strip_text_px(AE_LIST_MORE_PX, s, s);
		real_argb_color grey;

		foot = first + pitch * (real)shown;
		usnprintf(time, NUMBEROF(time), L"+%d MORE", (int)(count - shown));
		time[NUMBEROF(time) - 1] = 0;
		set_color(grey_rgb, AE_LIST_MORE_ALPHA, &grey);
		ae_hud_text_draw(view, name_left, foot, time, more_px, &grey, TRUE);
		right = MAX(right, name_left + ae_hud_text_measure(time, more_px));
	}
	/* (for TRAINING's labels to keep clear of, in the HUD's units) */
	hud_item_timers_ae_record(left / ppu, (first - name_px * AE_HUD_TEXT_CAP) / ppu, right / ppu, (foot + 4.0f) / ppu);
}

static boolean top_left_setting(void)
{
	static unsigned long read_at = (unsigned long)-1;
	static boolean top_left = FALSE;

	if (read_at != ae_config_player_changes())
	{
		read_at = ae_config_player_changes();
		top_left = (boolean)!strcmp(config_string("display.power_list"), "top_left");
	}
	return top_left;
}

boolean ae_power_list_draw(long font_index)
{
	short view = render.local_player_index;

	(void)font_index;
	if (!valid_view(view) || !ae_power_list_hides_clock(view))
	{
		if (valid_view(view))
			list_views[view].drawn = FALSE;
		return FALSE;
	}
	/* (the gametype's TIMERS and TRAINING; not once the game is over) */
	if (!game_engine_item_timers() || game_engine_game_over())
	{
		list_views[view].drawn = FALSE;
		return TRUE;
	}
	/* (nor while this view's scoreboard shows: its last place kept, so the strip's gap does not jump) */
	if (game_engine_scoreboard_shown(view) > 0.0f || game_engine_scoreboard_held(view))
		return TRUE;
	list_views[view].drawn = FALSE;
	if (local_player_count() <= 1 && top_left_setting())
		draw_top_left(view);
	else
		draw_column(view);
	return TRUE;
}

/* CAMPAIGN TIMER in HUD STYLE AE (display.campaign_timer; owner 10-23: the corner clock as today, in AE's look; a
campaign HUD with scoring is later): the time played in AE's digits, the score strip clock's size, colour and glow,
right-aligned on the right-hand column at the power list's foot, where multiplayer's corner clock sits. FALSE in HUD
STYLE CE (hud_item_timers.c draws its own) */
boolean ae_power_list_campaign_clock(short view, long ticks)
{
	real s, ppu, cap, width, right, foot;
	real_argb_color color;
	wchar_t text[32];

	if (!valid_view(view) || !ae_hud_style_is_ae())
		return FALSE;
	s = view_scale();
	ppu = ae_hud_pixels_per_unit();
	cap = ae_strip_digits_px(AE_STRIP_CLOCK_CAP, s);
	game_engine_format_clock(ticks, FALSE, text, NUMBEROF(text));
	width = ae_hud_digits_measure(text, cap);
	right = ae_power_list_right_x(view);
	foot = ae_score_strip_shared_line_y(view) - AE_LIST_LIFT * s;
	set_color(clock_rgb, 1.0f, &color);
	ae_hud_digits_draw(view, right - width, foot, text, cap, &color, AE_LIST_CLOCK_GLOW_ALPHA);
	/* (for the HUD's messages and TRAINING's labels to keep clear of, in the HUD's units) */
	hud_item_timers_ae_record((right - width) / ppu, (foot - cap) / ppu, right / ppu, (foot + 2.0f) / ppu);
	return TRUE;
}

void ae_power_list_forget(void)
{
	memset(list_views, 0, sizeof(list_views));
}
