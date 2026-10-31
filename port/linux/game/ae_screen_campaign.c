/*
AE_SCREEN_CAMPAIGN.C

AE's CAMPAIGN (ae_screen_campaign.h; the mockup's board 7): CONTINUE
(the saved game, when there is one) and the ten levels on the left, a level not reached yet disabled; on the right the
level's name, the difficulties it is finished on, DIFFICULTY, and what the game takes as information, not choices:
PLAYERS (THIS PC, a status's muted value) and CO-OP (disabled: IN CLASSIC MENUS), with a line saying where co-op is
for now (SETTINGS > SYSTEM > MENUS > CLASSIC; AE's own co-op is 0.4.0's work). A plays the focused level at the
difficulty shown; left / right or Y change the difficulty; B back.

Its log: the glue's ("ae campaign: ..."), "ae campaign: open (<profile>)".
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "ae_glue_campaign.h"
#include "ae_hooks.h"
#include "ae_screen_campaign.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum
{
	CAMPAIGN_HIT = 0x7A00,
	HIT_ROWS = CAMPAIGN_HIT + 1, HIT_PROMPTS, HIT_SIDE
};
enum { ROW_HEADER = AE_FLOW_ROW_HEADER, ROW_CONTINUE = AE_FLOW_ROW_OWN, ROW_LEVEL };

static struct
{
	struct ae_flow_screen flow;
	struct ae_campaign campaign;
	short difficulty;
	short level_of_row[AE_FLOW_ROWS];
	char continue_text[48];
} screen;

static void rows_make(void)
{
	struct ae_flow_screen *flow = &screen.flow;
	short count = 0, level;

	if (screen.campaign.saved_level >= 0)
	{
		snprintf(screen.continue_text, sizeof(screen.continue_text), "LEVEL %d \xE2\x80\xA2 %s",
			screen.campaign.saved_level + 1, ae_campaign_difficulty_name(screen.campaign.saved_difficulty));
		screen.level_of_row[count] = -1;
		/* (its difficulty is the saved game's: shown, never changed here) */
		flow->rows[count++] = (struct ae_flow_row){ ROW_CONTINUE, -1, "CONTINUE", screen.continue_text,
			"Goes on from your saved game, at its difficulty.", NULL, 0, 1 };
	}
	screen.level_of_row[count] = -1;
	flow->rows[count++] = (struct ae_flow_row){ ROW_HEADER, -1, "LEVELS", NULL, NULL, NULL };
	for (level = 0; level < AE_CAMPAIGN_LEVELS && count < AE_FLOW_ROWS; level++)
	{
		screen.level_of_row[count] = level;
		flow->rows[count++] = (struct ae_flow_row){ ROW_LEVEL, -1, screen.campaign.levels[level].name, NULL,
			"{A} plays it at the difficulty on the right; left / right change the difficulty.",
			screen.campaign.levels[level].reached ? NULL : "NOT REACHED" };
	}
	flow->count = count;
	if (flow->focus >= count)
		flow->focus = (short)(count - 1);
	if (flow->rows[flow->focus].kind == ROW_HEADER)
		flow->focus++;
}

static short focused_level(void)
{
	short focus = screen.flow.focus;

	return focus >= 0 && focus < screen.flow.count ? screen.level_of_row[focus] : -1;
}

static int continue_focused(void);

static void draw_side(void)
{
	struct ae_flow_screen *flow = &screen.flow;
	struct ae_density const *d = &flow->density;
	float x = flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X, width = ae_flow_right_x(flow) - x;
	float y = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y), text = ae_size_text(d), minor = ae_size_minor(d);
	float gap = ae_flow_u(flow, d->metrics->gap);
	short level = focused_level();
	char finished[96];
	struct ae_row row;
	int difficulty, length = 0;

	if (level < 0 && screen.campaign.saved_level >= 0)
		level = screen.campaign.saved_level;
	if (level < 0)
		return;
	ae_draw_text(AE_FONT_ROW, text * 1.5f, x, y, AE_ALIGN_LEFT, AE_COLOR_TITLE, screen.campaign.levels[level].name);
	y += ae_draw_cap_height(AE_FONT_ROW, text * 1.5f) + ae_flow_u(flow, 18.0f);
	finished[0] = 0;
	for (difficulty = 1; difficulty < AE_CAMPAIGN_DIFFICULTIES; difficulty++)
	{
		if (screen.campaign.levels[level].finished[difficulty])
			length += snprintf(finished + length, sizeof(finished) - (size_t)length, "%s%s", length ? ", " : "",
				ae_campaign_difficulty_name((short)difficulty));
	}
	ae_draw_text(AE_FONT_BODY, minor, x, y, AE_ALIGN_LEFT, finished[0] ? AE_COLOR_ACCENT : AE_COLOR_MUTED,
		finished[0] ? finished : screen.campaign.levels[level].reached ? "Not finished yet" : "Not reached yet");
	if (finished[0])
		ae_draw_text(AE_FONT_BODY, minor, x + ae_draw_text_width(AE_FONT_BODY, minor, finished) + ae_flow_u(flow, 10.0f), y,
			AE_ALIGN_LEFT, AE_COLOR_MUTED, "finished");
	y += ae_flow_u(flow, 44.0f);
	memset(&row, 0, sizeof(row));
	row.label = "DIFFICULTY";
	row.value = ae_campaign_difficulty_name(continue_focused() ? screen.campaign.saved_difficulty : screen.difficulty);
	row.flags = continue_focused() ? AE_ROW_STATUS : AE_ROW_FOCUSED;
	y += ae_widget_row(d, x, y, width, &row, HIT_SIDE, 0) + gap;
	/* (0.3.9: information, not a choice: a status's muted value, no arrows, as nothing can change it here) */
	memset(&row, 0, sizeof(row));
	row.label = "PLAYERS";
	row.value = "THIS PC";
	row.flags = AE_ROW_STATUS;
	y += ae_widget_row(d, x, y, width, &row, HIT_SIDE, 1) + gap;
	memset(&row, 0, sizeof(row));
	row.label = "CO-OP";
	row.flags = AE_ROW_DISABLED;
	/* (co-op is in the classic menus for now: SETTINGS > SYSTEM > MENUS) */
	row.reason = "IN CLASSIC MENUS";
	y += ae_widget_row(d, x, y, width, &row, HIT_SIDE, 2);
	/* (where it is: the line under the rows, muted, the rows' own left) */
	y += ae_flow_u(flow, 16.0f);
	ae_draw_text(AE_FONT_BODY, minor, x + ae_flow_u(flow, d->metrics->pad), y, AE_ALIGN_LEFT, AE_COLOR_MUTED,
		"Co-op: SETTINGS > SYSTEM > MENUS > CLASSIC, for now.");
}

/* whether CONTINUE has the focus: it plays the saved game's difficulty, so DIFFICULTY shows that one and doesn't change
(0.3.9: it showed the next new game's, LEGENDARY beside CONTINUE's NORMAL) */
static int continue_focused(void)
{
	short focus = screen.flow.focus;

	return focus >= 0 && focus < screen.flow.count && screen.flow.rows[focus].kind == ROW_CONTINUE;
}

static void difficulty_step(short direction)
{
	short next = ae_value_step(AE_CAMPAIGN_DIFFICULTIES, screen.difficulty, direction);

	if (continue_focused())
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}

	ae_sound_request(next == screen.difficulty ? AE_SOUND_FAILURE : AE_SOUND_CURSOR, 0);
	screen.difficulty = next;
}

static void accept(short row)
{
	struct ae_flow_row const *r;

	if (row < 0 || row >= screen.flow.count)
		return;
	r = &screen.flow.rows[row];
	if (r->reason)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	if (r->kind == ROW_CONTINUE ? ae_campaign_continue(&screen.campaign) :
		ae_campaign_start(&screen.campaign, screen.level_of_row[row], screen.difficulty))
	{
		ae_sound_request(AE_SOUND_FORWARD, 0);
		/* (the level loads: no AE screen over it, and the main menu does not open again in the frames before the
		game leaves its own; AE's main menu comes back at the main menu) */
		ae_ui_game_starting();
		while (ae_ui_depth() && ae_ui_top())
		{
			if (!ae_ui_top()->data || !ae_ui_remove(ae_ui_top()->data))
				ae_ui_pop();
		}
		return;
	}
	ae_flow_error(&screen.flow, "The level did not start", "No profile for player 1, or the level is not reached yet.");
}

static void campaign_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &screen.flow;
	char crumb[64];

	(void)instance;
	ae_flow_full_density(flow);
	rows_make();
	snprintf(crumb, sizeof(crumb), "%s", screen.campaign.have_profile ? screen.campaign.profile : "NO PROFILE");
	ae_flow_title(flow, "CAMPAIGN", crumb);
	ae_flow_draw_rows(flow, flow->frame.rect.width * AE_FLOW_ROWS_WIDTH);
	draw_side();
	flow->prompt_count = 0;
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Play", AE_ACTION_ACCEPT);
	if (!continue_focused())
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_Y, "Difficulty", AE_ACTION_Y);
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	ae_flow_draw_prompts(flow);
}

static int campaign_handle(struct ae_screen *instance, struct ae_event const *event)
{
	(void)instance;
	switch (event->action)
	{
	case AE_ACTION_BACK:
		return 0;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		ae_flow_focus_step(&screen.flow, event->action == AE_ACTION_UP ? -1 : 1, event->repeat);
		return 1;
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
		difficulty_step(event->action == AE_ACTION_RIGHT ? 1 : -1);
		return 1;
	case AE_ACTION_Y:
		if (continue_focused())
		{
			ae_sound_request(AE_SOUND_FAILURE, 0);
			return 1;
		}
		screen.difficulty = (short)((screen.difficulty + 1) % AE_CAMPAIGN_DIFFICULTIES);
		ae_sound_request(AE_SOUND_CURSOR, 0);
		return 1;
	case AE_ACTION_ACCEPT:
		accept(screen.flow.focus);
		return 1;
	default:
		return 1;
	}
}

static void campaign_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;
	short row;

	if (ae_prompts_pointer(screen.flow.prompts, screen.flow.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK)
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_pop();
		}
		else
			campaign_handle(instance, &event);
		return;
	}
	/* (DIFFICULTY's arrows on the right) */
	if (pointer->left_clicks && ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_SIDE && hit.index == 0 &&
		(hit.part == AE_PART_ARROW_LEFT || hit.part == AE_PART_ARROW_RIGHT))
	{
		difficulty_step(hit.part == AE_PART_ARROW_RIGHT ? 1 : -1);
		return;
	}
	row = ae_flow_rows_pointer(&screen.flow, pointer);
	if (row >= 0)
		accept(row);
}

static struct ae_screen_class const campaign_class =
{
	.name = "campaign", .handle = campaign_handle, .draw = campaign_draw, .pointer = campaign_pointer,
};

int ae_screen_campaign_open(void)
{
	memset(&screen, 0, sizeof(screen));
	screen.flow.hit_rows = HIT_ROWS;
	screen.flow.hit_prompts = HIT_PROMPTS;
	ae_campaign_read(&screen.campaign);
	screen.difficulty = screen.campaign.difficulty >= 0 && screen.campaign.difficulty < AE_CAMPAIGN_DIFFICULTIES ?
		screen.campaign.difficulty : 1;
	if (!ae_ui_push(&campaign_class, AE_OWNER_ANY, &screen))
		return 0;
	platform_log("ae campaign: open (%s)", screen.campaign.have_profile ? screen.campaign.profile : "no profile");
	return 1;
}
