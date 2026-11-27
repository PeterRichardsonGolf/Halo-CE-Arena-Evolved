/*
AE_SCREEN_LEADERBOARD.C

See ae_screen_leaderboard.h. Layout (u at 1080): the title and crumb, the mission's name and a line of what the board is, then a
table from the left: rank, PROFILE, DIFFICULTY, SKULLS, MEDALS, TIME and SCORE; the focused row's words under it. A profile with no
record of the mission is not on the board. Empty: a line that says how to get on it.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "ae_glue_campaign.h"
#include "ae_glue_pve_menus.h"
#include "ae_input.h"
#include "ae_pve_menus_rules.h"
#include "ae_pve_menus_ui.h"
#include "ae_screen_leaderboard.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_style.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum { BOARD_HIT = 0x7AC0, HIT_PROMPTS = BOARD_HIT + 1, HIT_ROW, ROWS_SHOWN = 10 };

static struct
{
	struct ae_flow_screen flow;
	char crumb[64];
	short level, focus;
	int by_time, count, staged, empty;
	struct ae_pve_run runs[AE_PVE_PROFILES];
	struct ae_pve_menus_run order[AE_PVE_PROFILES];
	long me;
} board;

/* DEV MODE's scene: four made-up runs of made-up profiles (none when empty) */
static void runs_demo(void)
{
	static char const *const names[4] = { "Rook", "Vesper", "Marlow", "Quill" };
	int each;

	memset(board.runs, 0, sizeof(board.runs));
	board.count = board.empty ? 0 : 4;
	for (each = 0; each < board.count; each++)
	{
		snprintf(board.runs[each].name, sizeof(board.runs[each].name), "%s", names[each]);
		board.runs[each].profile_index = each + 1;
		board.runs[each].score = 65100 - 9000 * each;
		board.runs[each].seconds = 1512 + 260 * ((each * 3) % 4);
		board.runs[each].difficulty = each == 1 ? 3 : 2;
		board.runs[each].skulls = each == 0 ? 4 : 0;
		board.runs[each].medals = 27 - 5 * each;
	}
}

static void runs_read(void)
{
	int each;
	struct ae_pve_menus_run *order = board.order;

	if (board.staged)
		runs_demo();
	else
		board.count = ae_pve_level_runs(board.level, board.runs, AE_PVE_PROFILES);
	for (each = 0; each < board.count; each++)
		order[each] = (struct ae_pve_menus_run){ board.runs[each].score, board.runs[each].seconds, board.runs[each].difficulty,
			board.runs[each].skulls, board.runs[each].medals, each };
	ae_pve_menus_runs_sort(order, board.count, board.by_time);
	if (board.focus >= board.count)
		board.focus = (short)(board.count > 0 ? board.count - 1 : 0);
}

int ae_screen_leaderboard_has_runs(short level)
{
	struct ae_pve_run runs[AE_PVE_PROFILES];

	return level >= 0 && level < AE_CAMPAIGN_LEVELS && ae_pve_level_runs(level, runs, AE_PVE_PROFILES) > 0;
}

static float column_x(float left, float right, int column)
{
	/* rank, profile, difficulty, skulls, medals, time, score (fractions of the table's width) */
	static const float at[7] = { 0.0f, 0.07f, 0.34f, 0.50f, 0.66f, 0.78f, 1.0f };

	return left + (right - left) * at[column];
}

/* the right edge of a number column (MEDALS and TIME read right-aligned), as a fraction of the table's width */
static float column_right(float left, float right, int column)
{
	return left + (right - left) * (column == 4 ? 0.74f : 0.89f);
}

static int column_numeric(int column)
{
	return column == 4 || column == 5;
}

static void board_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &board.flow;
	float u, left, right, top, row_h, y;
	char text[96], mission[64];
	int each;
	static const char *const heads[7] = { "#", "PROFILE", "DIFFICULTY", "SKULLS", "MEDALS", "TIME", "SCORE" };

	(void)instance;
	ae_flow_full_density(flow);
	runs_read();
	ae_flow_title(flow, "LEADERBOARD", board.crumb);
	u = ae_flow_u(flow, 1.0f);
	left = ae_flow_left_x(flow);
	right = ae_flow_right_x(flow);
	top = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y);
	ae_campaign_level_name(board.level, mission, sizeof(mission));
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 34.0f, left, top + 22.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, mission);
	snprintf(text, sizeof(text), "MISSION %d OF %d \xE2\x80\xA2 THIS PC'S PROFILES \xE2\x80\xA2 %s FIRST", board.level + 1, AE_CAMPAIGN_LEVELS,
		board.by_time ? "BEST TIME" : "BEST SCORE");
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, left, top + 58.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, text);
	y = top + 96.0f * u;
	row_h = 50.0f * u;
	if (board.count == 0)
	{
		ae_draw_rect(left, y, right - left, row_h * 2.0f, 0.0f, AE_COLOR_PANEL);
		ae_pve_menus_ui_text(flow, AE_FONT_ROW, 24.0f, left + 24.0f * u, y + row_h * 0.7f, AE_ALIGN_LEFT, AE_COLOR_TITLE, "NO RUNS ON RECORD");
		ae_pve_menus_ui_text(flow, AE_FONT_BODY, 19.0f, left + 24.0f * u, y + row_h * 1.35f, AE_ALIGN_LEFT, AE_COLOR_TEXT,
			"Finish this mission with SCORING on to put a run here.");
	}
	else
	{
		for (each = 0; each < 7; each++)
			ae_pve_menus_ui_text(flow, AE_FONT_BODY, 14.0f,
				column_numeric(each) ? column_right(left + 24.0f * u, right - 24.0f * u, each) : column_x(left + 24.0f * u, right - 24.0f * u, each),
				y + 14.0f * u, each == 6 || column_numeric(each) ? AE_ALIGN_RIGHT : AE_ALIGN_LEFT, AE_COLOR_MUTED, heads[each]);
		y += 34.0f * u;
		for (each = 0; each < board.count && each < ROWS_SHOWN; each++)
		{
			struct ae_pve_run const *run = &board.runs[board.order[each].profile];
			int on = each == board.focus, mine = run->profile_index == ae_pve_player_profile();
			unsigned int ink = on ? AE_COLOR_SELECTION_TEXT : AE_COLOR_TEXT;
			float cx = left + 24.0f * u, cr = right - 24.0f * u, mid = y + row_h * 0.5f;

			ae_draw_rect(left, y, right - left, row_h, 0.0f, on ? AE_COLOR_SELECTION : AE_COLOR_ROW);
			if (on)
				ae_draw_rect(left, y, 3.0f * u, row_h, 0.0f, AE_COLOR_ACCENT);
			ae_hit_add(left, y, right - left, row_h, HIT_ROW, AE_PART_ROW, (short)each);
			snprintf(text, sizeof(text), "%d", each + 1);
			ae_pve_menus_ui_text(flow, AE_FONT_ROW, 22.0f, column_x(cx, cr, 0), mid, AE_ALIGN_LEFT, ink, text);
			snprintf(text, sizeof(text), "%s%s", run->name, mine ? "  (YOU)" : "");
			ae_pve_menus_ui_text(flow, AE_FONT_ROW, 22.0f, column_x(cx, cr, 1), mid, AE_ALIGN_LEFT, ink, text);
			ae_pve_menus_ui_text(flow, AE_FONT_BODY, 19.0f, column_x(cx, cr, 2), mid, AE_ALIGN_LEFT, ink,
				run->difficulty >= 0 && run->difficulty < AE_CAMPAIGN_DIFFICULTIES ? ae_campaign_difficulty_name((short)run->difficulty) : "-");
			if (run->skulls > 0)
				snprintf(text, sizeof(text), "%ld", run->skulls);
			else
				snprintf(text, sizeof(text), "none");
			ae_pve_menus_ui_text(flow, AE_FONT_BODY, 19.0f, column_x(cx, cr, 3), mid, AE_ALIGN_LEFT, ink, text);
			snprintf(text, sizeof(text), "%ld", run->medals > 0 ? run->medals : 0);
			ae_pve_menus_ui_text(flow, AE_FONT_BODY, 19.0f, column_right(cx, cr, 4), mid, AE_ALIGN_RIGHT, ink, text);
			ae_pve_menus_time_text(run->seconds, text, sizeof(text));
			ae_pve_menus_ui_text(flow, AE_FONT_BODY, 19.0f, column_right(cx, cr, 5), mid, AE_ALIGN_RIGHT, ink, text);
			ae_pve_menus_number_text(run->score, text, sizeof(text));
			ae_pve_menus_ui_text(flow, AE_FONT_ROW, 22.0f, column_x(cx, cr, 6), mid, AE_ALIGN_RIGHT, ink, text);
			y += row_h + 6.0f * u;
		}
		/* (the focused run in words) */
		{
			struct ae_pve_run const *run = &board.runs[board.order[board.focus].profile];
			char time[24], score[24];

			ae_pve_menus_time_text(run->seconds, time, sizeof(time));
			ae_pve_menus_number_text(run->score, score, sizeof(score));
			snprintf(text, sizeof(text), "%s: %s points on %s in %s, %ld medal%s.", run->name, score,
				run->difficulty >= 0 && run->difficulty < AE_CAMPAIGN_DIFFICULTIES ? ae_campaign_difficulty_name((short)run->difficulty) : "an unknown difficulty",
				run->seconds > 0 ? time : "no recorded time", run->medals > 0 ? run->medals : 0L, run->medals == 1 ? "" : "s");
			/* (a rule and the panel behind the line: it stays readable over a bright backdrop) */
			ae_draw_rect(left - 24.0f * u, ae_flow_content_bottom(flow) - 58.0f * u, right - left + 48.0f * u, 64.0f * u, 0.0f, AE_COLOR_PANEL);
			ae_draw_rect(left, ae_flow_content_bottom(flow) - 58.0f * u, right - left, u, 0.0f, AE_COLOR_RULE);
			ae_pve_menus_ui_text(flow, AE_FONT_BODY, 20.0f, left, ae_flow_content_bottom(flow) - 26.0f * u, AE_ALIGN_LEFT, AE_COLOR_TEXT, text);
		}
	}
	flow->prompt_count = 0;
	if (board.count > 0)
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_Y, board.by_time ? "Best score" : "Best time", AE_ACTION_Y);
	flow->prompts[flow->prompt_count] = ae_flow_prompt(AE_BUTTON_LEFT_SHOULDER, "Mission", AE_ACTION_TAB_PREVIOUS);
	flow->prompts[flow->prompt_count++].pair = 1;
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	ae_flow_draw_prompts(flow);
}

static void level_step(int direction)
{
	board.level = (short)((board.level + direction + AE_CAMPAIGN_LEVELS) % AE_CAMPAIGN_LEVELS);
	board.focus = 0;
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

static int board_handle(struct ae_screen *instance, struct ae_event const *event)
{
	(void)instance;
	switch (event->action)
	{
	case AE_ACTION_BACK:
		return 0;
	case AE_ACTION_UP:
		if (board.focus > 0)
			board.focus--, ae_sound_request(AE_SOUND_CURSOR, 0);
		return 1;
	case AE_ACTION_DOWN:
		if (board.focus + 1 < board.count && board.focus + 1 < ROWS_SHOWN)
			board.focus++, ae_sound_request(AE_SOUND_CURSOR, 0);
		return 1;
	case AE_ACTION_Y:
		board.by_time = !board.by_time;
		ae_sound_request(AE_SOUND_CURSOR, 0);
		return 1;
	case AE_ACTION_TAB_PREVIOUS:
		level_step(-1);
		return 1;
	case AE_ACTION_TAB_NEXT:
		level_step(1);
		return 1;
	default:
		return 1;
	}
}

static void board_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;

	if (ae_prompts_pointer(board.flow.prompts, board.flow.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK)
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_pop();
		}
		else
			board_handle(instance, &event);
		return;
	}
	if (pointer->moved && ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_ROW && hit.part == AE_PART_ROW)
		board.focus = (short)hit.index;
}

static void board_leave(struct ae_screen *instance)
{
	(void)instance;
	platform_log("ae leaderboard: closed");
}

static struct ae_screen_class const board_class =
{
	.name = "leaderboard", .leave = board_leave, .handle = board_handle, .draw = board_draw, .pointer = board_pointer,
};

static int open_over(short level, int staged, int empty)
{
	char name[48];

	if (level < 0 || level >= AE_CAMPAIGN_LEVELS)
		return 0;
	memset(&board, 0, sizeof(board));
	board.level = level;
	board.staged = staged;
	board.empty = empty;
	ae_pve_menus_ui_profile(name, sizeof(name));
	snprintf(board.crumb, sizeof(board.crumb), "SELECT MISSION \xC2\xB7 %s", name);
	board.flow.hit_rows = HIT_ROW;
	board.flow.hit_prompts = HIT_PROMPTS;
	if (!ae_ui_push(&board_class, AE_OWNER_ANY, &board))
		return 0;
	platform_log("ae leaderboard: open (level %d)", (int)level + 1);
	return 1;
}

int ae_screen_leaderboard_open(short level)
{
	return open_over(level, 0, 0);
}

int ae_screen_leaderboard_open_staged(short level, int empty)
{
	return open_over(level, 1, empty);
}
