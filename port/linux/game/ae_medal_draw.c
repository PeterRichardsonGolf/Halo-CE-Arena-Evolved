/*
AE_MEDAL_DRAW.C

The medals on screen (ae_medals.h), in the player's MEDAL STYLE (display.medal_style, each player's):

MODERN: a stack by the reticle (ae_medal_modern_rules.h): each medal a row, its name right aligned left of the view's centre with its
picture on the right of it, the points of the medals shown above, and a "Killed" row for each of the player's kills. Two views on the
screen use it where it clears the lines of the message list; three or four, and any medal that does not clear, a coloured line in the
player's message list.

MCC: a holder at the stage's 150, 525 (a stage of 1920 by 1080, scaled to the view's height) where each medal's picture slides in,
holds and fades out in a slot of its own (ae_medal_draw_rules.h); past four medals every picture shrinks to 56 and the places' pitches
with it. A medal with no picture is its name as text in its place. With more than one local player a medal is a line in that
player's message list instead, so the pictures never cover a quarter of a split-screen view.

Everything is the view's pixels (x from its left, y from its top), as the markers'. The time is the game's ticks: a paused game
holds its medals as it holds the rest of the HUD, and a new game's clock starting again empties them.
*/

#include <math.h>
#include <string.h>
#include <wchar.h>

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "bitmaps/bitmap_color_conversion.h"
#include "bitmaps/bitmaps.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "interface/hud.h"
#include "interface/hud_draw.h"
#include "interface/hud_messaging.h"
#include "render/render.h"

#include "../src/ae_config_players.h"

#include "ae_hud_digits.h"
#include "ae_hud_messages.h"
#include "ae_hud_text.h"
#include "ae_announcer.h"
#include "ae_killfeed.h"
#include "ae_marker_rules.h"
#include "ae_medal_draw_rules.h"
#include "ae_medal_modern_rules.h"
#include "ae_medal_pack.h"
#include "ae_medals.h"
#include "ae_waypoint_ink.h"

#define MEDAL_NAME_SIZE 64

static struct ae_medal_holder holders[MAXIMUM_LOCAL_PLAYERS];
static struct ae_mm_stack stacks[MAXIMUM_LOCAL_PLAYERS];

/* each view's size as it was last drawn (the stack's room is checked as a medal arrives) */
static struct
{
	boolean known;
	real width, height;
} views[MAXIMUM_LOCAL_PLAYERS];

/* the medals' points, from the list */
static int const medal_points[AE_MEDAL_COUNT] = {
#define AE_MEDAL(name, key, points) points,
#include "ae_medal_ids.h"
#undef AE_MEDAL
};

static boolean valid_local(short local_player_index)
{
	return local_player_index >= 0 && local_player_index < MAXIMUM_LOCAL_PLAYERS;
}

boolean ae_medal_style_is_mcc(short local_player_index)
{
	char text[16];

	if (!valid_local(local_player_index) || !ae_config_player_text(local_player_index, "display.medal_style", text, sizeof(text)))
		return FALSE;
	return (boolean)(strcmp(text, "mcc") == 0);
}

void ae_medal_display_reset(void)
{
	short index;

	for (index = 0; index < MAXIMUM_LOCAL_PLAYERS; index++)
	{
		ae_medal_holder_reset(&holders[index]);
		ae_mm_reset(&stacks[index]);
	}
}

/* the stack's metrics in a view */
static void view_metrics(short local_player_index, struct ae_mm_metrics *metrics)
{
	ae_mm_metrics_of(views[local_player_index].width, views[local_player_index].height, (boolean)(local_player_count() > 1), metrics);
}

/* whether the stack, at its fullest, is clear of the lines of this view's message list by AE_MM_CLEAR: two views use it only then */
static boolean stack_clears(short local_player_index)
{
	struct ae_mm_metrics metrics;
	struct ae_mm_box stack, others[AE_HUD_MESSAGE_ROWS_MOST];
	real boxes[AE_HUD_MESSAGE_ROWS_MOST][4];
	short count, index;

	view_metrics(local_player_index, &metrics);
	ae_mm_box_of(&metrics, AE_MM_WIDEST_EM, &stack);
	count = ae_hud_message_rows(local_player_index, boxes, AE_HUD_MESSAGE_ROWS_MOST);
	for (index = 0; index < count; index++)
	{
		others[index].x0 = boxes[index][0];
		others[index].y0 = boxes[index][1];
		others[index].x1 = boxes[index][2];
		others[index].y1 = boxes[index][3];
	}
	return ae_mm_clears(&stack, others, count, AE_MM_CLEAR);
}

/* whether this player's medals and kills are drawn as the MODERN stack now: one view, or two where it clears. A view not drawn yet
cannot be placed in */
static boolean modern_stack(short local_player_index)
{
	short views_on_screen = local_player_count();

	if (!views[local_player_index].known || ae_medal_style_is_mcc(local_player_index) || views_on_screen > 2)
		return FALSE;
	return (boolean)(views_on_screen < 2 || stack_clears(local_player_index));
}

/* a medal as a line of the player's message list, in its colour */
static void medal_word_line(short local_player_index, int medal)
{
	wchar_t name[MEDAL_NAME_SIZE], line[MEDAL_NAME_SIZE + 8];
	real_argb_color color;
	float rgb[3];

	ae_medal_pack_name(medal, name, MEDAL_NAME_SIZE, FALSE);
	if (!name[0])
		return;
	color.red = color.green = color.blue = 1.0f;
	color.alpha = 1.0f;
	ae_medal_pack_caption(medal, &color);
	rgb[0] = color.red;
	rgb[1] = color.green;
	rgb[2] = color.blue;
	if (ae_mm_word_line(line, (int)NUMBEROF(line), name, rgb))
		hud_print_feed_message(local_player_index, line, 0);
}

void ae_medal_display_reset_local(short local_player_index)
{
	if (!valid_local(local_player_index))
		return;
	ae_medal_holder_reset(&holders[local_player_index]);
	ae_mm_reset(&stacks[local_player_index]);
}

void ae_medal_display_add(short local_player_index, int medal)
{
	if (!valid_local(local_player_index) || medal < 0 || medal >= AE_MEDAL_COUNT)
		return;
	/* (this player's MEDALS: off, nothing is added) */
	if (!ae_config_player_boolean(local_player_index, "display.medals"))
		return;
	ae_announcer_medal(medal);
	if (modern_stack(local_player_index))
	{
		struct ae_mm_metrics metrics;
		struct ae_mm_row row;

		memset(&row, 0, sizeof(row));
		row.kind = AE_MM_KIND_MEDAL;
		row.medal = medal;
		row.points = medal_points[medal];
		row.has_picture = (boolean)(ae_medal_pack_has_picture(medal) != 0);
		view_metrics(local_player_index, &metrics);
		ae_mm_add(&stacks[local_player_index], &metrics, &row, game_time_get());
		return;
	}
	if (!ae_medal_style_is_mcc(local_player_index) ||
		ae_medal_show_mode(local_player_count(), ae_medal_pack_has_picture(medal)) == AE_MEDAL_SHOW_FEED)
	{
		medal_word_line(local_player_index, medal);
		return;
	}
	ae_medal_holder_add(&holders[local_player_index], medal, game_time_get());
}

void ae_medal_display_killed(short local_player_index, long victim_player_index, short victim_team, wchar_t const *victim_name,
	boolean betrayal)
{
	struct ae_mm_metrics metrics;
	struct ae_mm_row row;
	short index;

	if (!valid_local(local_player_index) || betrayal || !victim_name ||
		!ae_config_player_boolean(local_player_index, "display.medals") || !modern_stack(local_player_index))
		return;
	memset(&row, 0, sizeof(row));
	row.kind = AE_MM_KIND_KILLED;
	row.victim = victim_player_index;
	row.victim_team = victim_team;
	for (index = 0; index < AE_MM_NAME - 1 && victim_name[index]; index++)
		row.victim_name[index] = victim_name[index];
	row.victim_name[index] = 0;
	view_metrics(local_player_index, &metrics);
	ae_mm_add(&stacks[local_player_index], &metrics, &row, game_time_get());
}

static pixel32 premultiplied_white(real alpha)
{
	real_argb_color color;

	color.alpha = alpha > 0.0f ? (alpha > 1.0f ? 1.0f : alpha) : 0.0f;
	color.red = color.alpha;
	color.green = color.alpha;
	color.blue = color.alpha;
	return real_argb_color_to_pixel32(&color);
}

/* ---------- MODERN */

static void white(real alpha, real_argb_color *color)
{
	color->alpha = alpha;
	color->red = 229.0f / 255.0f;
	color->green = 240.0f / 255.0f;
	color->blue = 1.0f;
}

/* a whole number as text */
static void number_text(int value, wchar_t *text)
{
	wchar_t digits[12];
	int count = 0, index = 0;

	if (value < 0)
		value = 0;
	do
	{
		digits[count++] = (wchar_t)(L'0' + value % 10);
		value /= 10;
	} while (value > 0 && count < 11);
	while (count > 0)
		text[index++] = digits[--count];
	text[index] = 0;
}

static void draw_stack(short local_player_index, long now)
{
	struct ae_mm_stack *stack = &stacks[local_player_index];
	struct ae_mm_metrics m;
	real points_alpha, points_scale;
	int index;

	view_metrics(local_player_index, &m);
	/* (the points above the rows, right aligned with the names, their middle on the stage's line) */
	if (ae_mm_score_pose(stack, now, &points_alpha, &points_scale))
	{
		wchar_t text[16];
		real_argb_color color;
		real px = m.score_px * points_scale;

		number_text(ae_mm_score(stack, now), text);
		white(points_alpha, &color);
		ae_hud_text_draw(local_player_index, (real)floor(m.name_right - ae_hud_text_measure(text, px) + 0.5f),
			(real)floor(m.score_middle + AE_HUD_TEXT_CAP * px / 2.0f + 0.5f), text, px, &color, TRUE);
	}
	for (index = 0; index < AE_MM_ROWS; index++)
	{
		struct ae_mm_row const *row = &stack->rows[index];
		real y, alpha, pop, px, height, middle, baseline;
		real_argb_color color;
		wchar_t name[MEDAL_NAME_SIZE];

		if (!row->used || !ae_mm_row_pose(row, now, &y, &alpha, &pop))
			continue;
		height = ae_mm_row_height(&m, row);
		middle = m.rows_top + y + height / 2.0f;
		px = m.name_px * pop;
		baseline = (real)floor(middle + AE_HUD_TEXT_CAP * px / 2.0f + 0.5f);
		/* (the names end where the stage says, a picture or not: the picture is on the right of them) */
		if (row->kind == AE_MM_KIND_MEDAL)
		{
			real_rectangle2d clip;
			struct bitmap_data const *bitmap = row->has_picture ? ae_medal_pack_picture(row->medal, &clip) : NULL;

			ae_medal_pack_name(row->medal, name, MEDAL_NAME_SIZE, FALSE);
			white(alpha, &color);
			ae_hud_text_draw(local_player_index, (real)floor(m.name_right - ae_hud_text_measure(name, px) + 0.5f), baseline, name, px,
				&color, TRUE);
			if (bitmap)
			{
				real x = (real)floor(m.picture_left + 0.5f), top = (real)floor(middle - m.picture / 2.0f + 0.5f);

				ae_waypoint_sprite_draw_pixels(bitmap, &clip, x, top, x + m.picture, top + m.picture, premultiplied_white(alpha));
			}
		}
		else
		{
			static wchar_t const killed[] = L"Killed ";
			real name_width = ae_hud_text_measure(row->victim_name, px);
			long viewer = local_player_get_player_index(local_player_index);

			/* ("Killed" in white, the player's name in the colour the feed gives it) */
			white(alpha, &color);
			ae_hud_text_draw(local_player_index, (real)floor(m.name_right - name_width - ae_hud_text_measure(killed, px) + 0.5f),
				baseline, killed, px, &color, TRUE);
			ae_killfeed_name_color(local_player_index, viewer, row->victim, row->victim_team, FALSE, &color);
			color.alpha *= alpha;
			ae_hud_text_draw(local_player_index, (real)floor(m.name_right - name_width + 0.5f), baseline, row->victim_name, px, &color,
				TRUE);
		}
	}
}

/* ---------- MCC */

static float caption_measure(wchar_t const *text, float px)
{
	return (float)ae_hud_text_measure(text, px);
}

static void draw_holder(short local_player_index, real scale, real left, long now)
{
	struct ae_medal_holder *holder = &holders[local_player_index];
	float shrink;
	int index;

	ae_medal_holder_step(holder, now);
	shrink = ae_medal_shrink(holder->count);
	for (index = 0; index < holder->count; index++)
	{
		struct ae_medal_shown const *shown = &holder->shown[index];
		struct bitmap_data const *bitmap;
		real_rectangle2d clip;
		float x, y, alpha;
		real px, size;

		bitmap = ae_medal_pack_picture(shown->medal, &clip);
		/* (a medal with no picture keeps its slot, its name where a picture's name goes, so no two names meet; the places keep their
		pitch whatever the size of the pictures, so that a long name's two lines have room) */
		if (!ae_medal_pose(now - shown->born, shown->slot, 0, &x, &y, &alpha))
			continue;
		size = AE_MEDAL_PICTURE * shrink * scale;
		x = (real)floor(left + x * scale + 0.5f);
		y = (real)floor(y * scale + 0.5f);
		if (bitmap)
			ae_waypoint_sprite_draw_pixels(bitmap, &clip, x, y, x + size, y + size, premultiplied_white(alpha));
		/* (its name under it, as it fades) */
		{
			wchar_t whole[MEDAL_NAME_SIZE], name[MEDAL_NAME_SIZE], second[MEDAL_NAME_SIZE];
			real_argb_color color;
			int lines;

			ae_medal_pack_name(shown->medal, whole, MEDAL_NAME_SIZE, TRUE);
			/* (a name wider than 142 stage pixels breaks at a word to two lines, the same size) */
			lines = ae_medal_caption_lines(whole, caption_measure, ae_medal_caption_pixels(scale), AE_MEDAL_CAPTION_WIDTH * scale, name,
				second, MEDAL_NAME_SIZE);
			if (lines > 0)
			{
				int line;
				real nominal = ae_medal_caption_pixels(scale);

				/* (one word wider than the limit with no break is fitted down, a whole pixel at a time, the lines keeping the baseline
				and pitch of the nominal size so a row of captions stays level) */
				px = ae_medal_caption_fit_pixels(name, second, caption_measure, nominal, AE_MEDAL_CAPTION_WIDTH * scale);
				color.alpha = alpha > 0.0f ? (alpha > 1.0f ? 1.0f : alpha) : 0.0f;
				color.red = color.green = color.blue = 1.0f;
				if (bitmap)
					ae_medal_pack_caption(shown->medal, &color);
				for (line = 0; line < lines; line++)
				{
					wchar_t const *text = line ? second : name;

					ae_hud_text_draw(local_player_index, (real)floor(x + (size - ae_hud_text_measure(text, px)) / 2.0f + 0.5f),
						(real)floor(y + size + AE_MEDAL_CAPTION_GAP * scale + AE_HUD_TEXT_CAP * nominal +
							ae_medal_caption_pitch(scale) * line + 0.5f),
						text, px, &color, TRUE);
				}
			}
		}
	}
}

void ae_medal_display_draw(short local_player_index)
{
	real ppu = ae_hud_pixels_per_unit();
	real view_width, view_height, scale, left;
	short area_left = 0, area_right = 0;
	long now = game_time_get();
	struct ae_mm_metrics metrics;

	ae_announcer_update();
	if (!valid_local(local_player_index))
		return;
	view_width = (real)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0) * ppu;
	view_height = (real)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0) * ppu;
	if (view_width > 0.0f && view_height > 0.0f)
	{
		views[local_player_index].known = TRUE;
		views[local_player_index].width = view_width;
		views[local_player_index].height = view_height;
	}
	if (!views[local_player_index].known)
		return;
	view_metrics(local_player_index, &metrics);
	/* (the scoreboard up, or the HUD hidden by a script: nothing; the clock goes on, so a medal does not wait for it) */
	if (game_engine_scoreboard_shown(local_player_index) > 0.0f || game_engine_scoreboard_held(local_player_index) ||
		(hud_scripted_globals && !hud_scripted_globals->show_hud))
	{
		ae_medal_holder_step(&holders[local_player_index], now);
		ae_mm_step(&stacks[local_player_index], &metrics, now);
		return;
	}
	/* (MEDAL STYLE changed in a game: the other layout's medals go, so the two never overlap) */
	if (ae_medal_style_is_mcc(local_player_index))
		ae_mm_reset(&stacks[local_player_index]);
	else
		ae_medal_holder_reset(&holders[local_player_index]);
	ae_mm_step(&stacks[local_player_index], &metrics, now);
	/* (two views: a message line that came after the stack can take its room; the stack waits, its clock going on) */
	if (stacks[local_player_index].rows[0].used && (local_player_count() < 2 || stack_clears(local_player_index)))
		draw_stack(local_player_index, now);
	if (holders[local_player_index].count < 1 || !(ppu > 0.0f))
		return;
	scale = view_height / 1080.0f;
	if (!(scale > 0.0f))
		return;
	/* (HUD AREA: the stage's left is the HUD area's) */
	hud_area_insets(&area_left, &area_right);
	left = (real)area_left * ppu;
	draw_holder(local_player_index, scale, left, now);
}
