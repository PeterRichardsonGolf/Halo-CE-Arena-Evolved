/*
AE_SCOREBOARD_HEADER.C

HUD STYLE AE's scoreboard header (ae_scoreboard_header.h).
*/

#include <string.h>

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "main/main.h"
#include "rasterizer/rasterizer.h"
#include "render/render.h"
#include "saved games/arena_gametypes.h"

#include "ae_bomb_assault.h"
#include "ae_header_lines.h"
#include "halo_map_packs.h"
#include "ae_hud_digits.h"
#include "ae_hud_style.h"
#include "ae_hud_text.h"
#include "ae_score_strip.h"
#include "ae_scoreboard_header.h"

#define AE_HEADER_GAP 4.0f           /* the header's foot over the scoreboard (px) */
#define AE_HEADER_QUARTER_SHIFT 18.0f /* quarter views: the header and the scoreboard move down (px at 1080) */
/* split views: the board's text at the full-screen scoreboard's scale (game_engine.c SCOREBOARD_SCALE), about the
view's centre and the title's top: a half view's rows end above the score strip, a quarter view's too, and the
text is as large as the one-player board's */
#define AE_SPLIT_BOARD_SCALE 0.75f
#define AE_HEADER_LINE1 24.0f
#define AE_HEADER_LINE2 15.0f
#define AE_HEADER_FLOOR 12.0f
#define AE_ODDBALL_TERMINATOR 2
#define AE_RACE_RALLY 2

static const real white_rgb[3] = { 229.0f / 255.0f, 240.0f / 255.0f, 1.0f };
static const real grey_rgb[3] = { 188.0f / 255.0f, 198.0f / 255.0f, 212.0f / 255.0f };
static const real hud_blue_rgb[3] = { 117.0f / 255.0f, 186.0f / 255.0f, 1.0f };
static const real line_rgb[3] = { 178.0f / 255.0f, 224.0f / 255.0f, 1.0f };
static const real dark_rgb[3] = { 6.0f / 255.0f, 10.0f / 255.0f, 18.0f / 255.0f };

static boolean header_on(void)
{
	return (boolean)(game_engine && ae_hud_style_is_ae());
}

long ae_scoreboard_header_rows(long gametype_rows)
{
	return header_on() ? 0 : gametype_rows;
}

static void set_color(real const rgb[3], real alpha, real_argb_color *color)
{
	color->alpha = alpha;
	color->red = rgb[0];
	color->green = rgb[1];
	color->blue = rgb[2];
}

boolean ae_pack_map_caption(char const *map_path, wchar_t *out, long count)
{
	char title[64], pack[64], tag[16];
	long i;

	if (count <= 0 || !map_path ||
		!map_pack_listing(map_path, title, sizeof(title), pack, sizeof(pack), tag, sizeof(tag)) || !title[0])
		return FALSE;
	for (i = 0; i < count - 1 && title[i]; i++)
		out[i] = (wchar_t)(unsigned char)title[i];
	out[i] = 0;
	return TRUE;
}

void ae_scoreboard_map_name(char const *map_path, wchar_t *out, long count)
{
	long i;

	if (!ae_pack_map_caption(map_path, out, count))
	{
		ae_scoreboard_header_map_name(map_path, out, count);
		return;
	}
	for (i = 0; out[i]; i++)
		if (out[i] >= L'a' && out[i] <= L'z')
			out[i] = (wchar_t)(out[i] - L'a' + L'A');
}

/* the running game's header text */
void ae_scoreboard_header_text(struct ae_header_lines *lines)
{
	wchar_t map[AE_HEADER_TEXT_LENGTH];
	struct game_variant *variant = game_engine_get_variant();
	struct game_variant_options const *options = game_variant_options_get();
	wchar_t stored[NUMBEROF(variant->human_readable_game_description) + 1];
	wchar_t display[48], set[24];
	struct arena_gametype_info info;
	struct ae_header_input input;
	char const *set_name = "";
	short i;

	csmemcpy(stored, variant->human_readable_game_description, sizeof(variant->human_readable_game_description));
	stored[NUMBEROF(stored) - 1] = 0;
	if (!arena_gametype_own_display_name_for_stored_name(stored, variant, display, NUMBEROF(display)))
		arena_gametype_display_name(stored, display, NUMBEROF(display));
	display[NUMBEROF(display) - 1] = 0;
	if (arena_gametype_info(stored, &info))
		set_name = arena_gametype_set_name(info.set);
	for (i = 0; i < (short)NUMBEROF(set) - 1 && set_name[i]; i++)
		set[i] = (wchar_t)(unsigned char)set_name[i];
	set[i] = 0;
	csmemset(&input, 0, sizeof(input));
	input.display_name = display;
	input.set_name = set;
	/* (the multiplayer map's name; main_get_map_name is the campaign's) */
	input.map_path = main_get_multiplayer_map_name();
	if (!input.map_path || !input.map_path[0])
		input.map_path = main_get_map_name();
	ae_scoreboard_map_name(input.map_path, map, NUMBEROF(map));
	input.map_name = map;
	input.score_to_win = variant->universal_variant.score_to_win;
	/* (SEARCH AND DESTROY's game.bomb_snd_rounds_to_win over the gametype's, as the game plays it) */
	if (variant->game_engine_index == game_engine_bomb)
		ae_bomb_snd_score_to_win(&input.score_to_win);
	input.time_limit_minutes = options ? options->time_limit : 0;
	input.clock_counts_up = (boolean)(game_engine_match_clock_setting() == _match_clock_up);
	/* (the time left counting down while the game runs: the scoreboard's own line no longer says it, owner 2026-10-20) */
	if (input.time_limit_minutes > 0 && game_engine_running() && !game_engine_game_over())
	{
		long left = input.time_limit_minutes * 60L * TICKS_PER_SECOND - game_time_get();

		input.time_left_shown = TRUE;
		input.time_left_seconds = left <= 0 ? 0 : (left + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND;
	}
	switch (variant->game_engine_index)
	{
	case game_engine_ctf: input.goal = _ae_goal_captures; break;
	case game_engine_king: input.goal = _ae_goal_time; input.score_to_win *= 60; break;
	case game_engine_oddball:
		if (variant->game_engine_variant.oddball.oddball_ball_type == AE_ODDBALL_TERMINATOR)
			input.goal = _ae_goal_points;
		else
		{
			input.goal = _ae_goal_time;
			input.score_to_win *= 60;
		}
		break;
	case game_engine_race:
		input.goal = variant->game_engine_variant.race.race_type == AE_RACE_RALLY ? _ae_goal_rally_points :
			_ae_goal_laps;
		break;
	default: input.goal = _ae_goal_points; break;
	}
	ae_scoreboard_header_lines(&input, lines);
}

/* the header with its foot at bottom (px), its text from x, the rule from x to rule_right (0: the text's width) */
static void draw_header(real x, real bottom, real rule_right, real alpha)
{
	short view = render.local_player_index;
	struct ae_header_lines lines;
	real s = ae_score_strip_view_scale();
	real px1 = MAX(AE_HEADER_LINE1 * s, AE_HEADER_FLOOR), px2 = MAX(AE_HEADER_LINE2 * s, AE_HEADER_FLOOR);
	real rule_y = bottom - 1.8f * s;
	real line2 = rule_y - 6.0f * s;
	real line1 = line2 - px2 * AE_HUD_TEXT_CAP - 8.0f * s;
	real width1, width2, cursor;
	real_argb_color color;
	const wchar_t *separator = AE_HEADER_SEPARATOR;

	ae_scoreboard_header_text(&lines);
	cursor = x;
	set_color(white_rgb, alpha, &color);
	ae_hud_text_draw(view, cursor, line1, lines.mode, px1, &color, TRUE);
	cursor += ae_hud_text_measure(lines.mode, px1);
	set_color(grey_rgb, alpha, &color);
	ae_hud_text_draw(view, cursor, line1, separator, px1, &color, TRUE);
	cursor += ae_hud_text_measure(separator, px1);
	set_color(hud_blue_rgb, alpha, &color);
	ae_hud_text_draw(view, cursor, line1, lines.map, px1, &color, TRUE);
	cursor += ae_hud_text_measure(lines.map, px1);
	if (lines.tag[0])
	{
		ae_hud_text_draw(view, cursor, line1, L" - ", px1, &color, TRUE);
		cursor += ae_hud_text_measure(L" - ", px1);
		ae_hud_text_draw(view, cursor, line1, lines.tag, px1, &color, TRUE);
		cursor += ae_hud_text_measure(lines.tag, px1);
	}
	width1 = cursor - x;
	set_color(hud_blue_rgb, 0.9f * alpha, &color);
	ae_hud_text_draw(view, x, line2, lines.goal_time, px2, &color, TRUE);
	width2 = ae_hud_text_measure(lines.goal_time, px2);
	if (rule_right <= x)
		rule_right = x + MAX(width1, width2);
	/* (the thin rule: CE's line at 50 % over a dark under-stroke) */
	set_color(dark_rgb, 0.6f * alpha, &color);
	ae_hud_line(x, rule_y, rule_right, rule_y, 3.5f * s, &color);
	set_color(line_rgb, 0.5f * alpha, &color);
	ae_hud_line(x, rule_y, rule_right, rule_y, 1.5f * s, &color);
}

void ae_scoreboard_header_full(short x0, short left, short top, short block_width, long line_height, real scale,
	real ping_room, real alpha)
{
	real ppu = ae_hud_pixels_per_unit();
	real block_left = (real)x0 + (real)(left - x0) * scale;
	real padding = 0.5f * (real)line_height * scale;
	real panel_left = (block_left - padding) * ppu;
	real panel_right = (block_left + (real)block_width * scale + padding + ping_room * scale) * ppu;
	real panel_top = ((real)top - padding) * ppu;

	if (!header_on() || alpha <= 0.0f)
		return;
	(void)panel_left;
	draw_header(block_left * ppu, panel_top - AE_HEADER_GAP, panel_right, alpha);
}

/* a quarter view: about a quarter of the screen (a 960x540 view of 1920x1080) */
static boolean quarter_view(void)
{
	return (boolean)(ae_score_strip_view_scale() > 0.0f &&
		(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0) <=
		(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0) * 2 &&
		(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0) < 300);
}

/* the split board's text scale is set (ae_scoreboard_header_row) until its row is drawn */
static boolean split_scale_set = FALSE;

void ae_scoreboard_header_row(long row, union rectangle2d *bounds, real alpha)
{
	real ppu = ae_hud_pixels_per_unit();
	real centre_x, title_y;

	if (!header_on() || !bounds)
		return;
	if (quarter_view())
	{
		short shift = (short)(AE_HEADER_QUARTER_SHIFT * ae_score_strip_view_scale() / 0.75f / ppu + 0.5f);

		bounds->y0 = (short)(bounds->y0 + shift);
		bounds->y1 = (short)(bounds->y1 + shift);
	}
	/* (the view's centre, where the layout centres the columns; the title's top, rows being as tall as this one) */
	centre_x = (real)(render.camera.window_bounds.x0 + render.camera.window_bounds.x1) / 2.0f -
		(real)render.camera.viewport_bounds.x0;
	title_y = (real)bounds->y0 - (real)row * (real)(bounds->y1 - bounds->y0);
	if (row == 0 && alpha > 0.0f)
		draw_header((centre_x + ((real)bounds->x0 - centre_x) * AE_SPLIT_BOARD_SCALE) * ppu, title_y * ppu - AE_HEADER_GAP,
			0.0f, alpha);
	rasterizer_text_set_scale(AE_SPLIT_BOARD_SCALE, centre_x, title_y);
	split_scale_set = TRUE;
}

void ae_scoreboard_header_row_drawn(void)
{
	if (!split_scale_set)
		return;
	rasterizer_text_set_scale(1.0f, 0.0f, 0.0f);
	split_scale_set = FALSE;
}
