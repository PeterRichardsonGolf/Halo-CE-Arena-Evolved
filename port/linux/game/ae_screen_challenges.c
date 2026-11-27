/*
AE_SCREEN_CHALLENGES.C

See ae_screen_challenges.h. Layout (the approved spec, u at 1080): the six challenges as rows on AE's flow (TODAY and THIS WEEK headers
with their reset times, each row's reward chip after the label, its counter at the right); the right panel from AE_FLOW_PANEL_X: the
career rank block (with the focused reward as the bar's light part) over the focused challenge's card.
*/

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "ae_career_challenge_rules.h"
#include "ae_career_draw.h"
#include "ae_career_view_rules.h"
#include "ae_glue_career_xp.h"
#include "ae_glue_social.h"
#include "ae_glue_social_game.h"
#include "ae_input.h"
#include "ae_screen_challenges.h"
#include "ae_screen_main_menu.h"
#include "ae_setup_flow.h"
#include "ae_social_rules.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_style.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum
{
	CHALLENGES_HIT = 0x7790,
	HIT_PROMPTS = CHALLENGES_HIT + 1,
	/* the rows: TODAY, three, THIS WEEK, three */
	ROW_TODAY = 0,
	ROW_WEEK = 4,
	ROWS = 8
};

static struct
{
	struct ae_flow_screen flow;
	char crumb[64];
	short player;
	int have;
	struct ae_career_view view;
} challenges;

static struct ae_screen_class const challenges_class;

/* the slot (0 .. 5) of a row, -1 for a header */
static int slot_of(int row)
{
	return row >= 1 && row <= 3 ? row - 1 : row >= 5 && row <= 7 ? row - 2 : -1;
}

static void crumb_read(void)
{
	char name[48];
	int at;

	if (ae_social_profile_name(challenges.player, name, sizeof(name)))
	{
		for (at = 0; name[at]; at++)
			if (name[at] >= 'a' && name[at] <= 'z')
				name[at] = (char)(name[at] - 32);
		snprintf(challenges.crumb, sizeof(challenges.crumb), "%s \xC2\xB7 PLAYER %d", name, (int)challenges.player + 1);
	}
	else
		snprintf(challenges.crumb, sizeof(challenges.crumb), "PLAYER %d", (int)challenges.player + 1);
}

static void rows_build(void)
{
	struct ae_flow_screen *flow = &challenges.flow;
	int row;

	challenges.have = ae_career_xp_view(challenges.player, &challenges.view);
	for (row = 0; row < ROWS; row++)
	{
		int slot = slot_of(row);

		if (slot < 0)
		{
			flow->rows[row] = (struct ae_flow_row){ AE_FLOW_ROW_HEADER, -1, row == ROW_TODAY ? "TODAY" : "THIS WEEK", NULL, NULL, NULL };
			continue;
		}
		/* (the value is drawn by the screen: its colour is the state's) */
		flow->rows[row] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, challenges.view.slot[slot].label[0] ? challenges.view.slot[slot].label : "-",
			"", NULL, NULL };
		flow->rows[row].status = 1;
	}
	flow->count = ROWS;
}

static void ctext(int font, float size_u, float x, float center_y, int align, unsigned int color, char const *text)
{
	float size = ae_flow_u(&challenges.flow, size_u);

	ae_draw_text(font, size, x, ae_career_cap_top(font, size, center_y), align, color, text);
}

static void tracked(float size_u, float x, float center_y, int align, unsigned int color, char const *text)
{
	float size = ae_flow_u(&challenges.flow, size_u);

	ae_draw_text_tracked(AE_FONT_BODY, size, 0.09f, x, ae_career_cap_top(AE_FONT_BODY, size, center_y), align, color, text);
}

/* whole minutes to the next local midnight */
static int minutes_to_midnight(void)
{
	time_t now = time(NULL);
	struct tm *local = localtime(&now);

	return local ? (23 - local->tm_hour) * 60 + (59 - local->tm_min) + 1 : 0;
}

/* the reset times at the headers' right, each row's reward chip and counter */
static void rows_extra_draw(void)
{
	struct ae_flow_screen *flow = &challenges.flow;
	float label_size = ae_size_text(&flow->density), small = ae_flow_u(flow, 15.0f);
	int row;
	char text[40];

	for (row = 0; row < ROWS; row++)
	{
		struct ae_rect const *r = &flow->rects[row];
		int slot = slot_of(row), on = row == flow->focus;
		unsigned int ink;

		if (slot < 0 || r->width <= 0.0f || !challenges.view.slot[slot].label[0])
			continue;
		{
			int done = challenges.view.slot[slot].done;
			float label_w = ae_draw_text_width(AE_FONT_ROW, label_size, challenges.view.slot[slot].label), w, h = ae_flow_u(flow, 22.0f), x, cy;

			snprintf(text, sizeof(text), "+%ld XP", challenges.view.slot[slot].xp);
			w = ae_draw_text_tracked_width(AE_FONT_ROW, small, 0.06f, text) + ae_flow_u(flow, 16.0f);
			x = r->x + ae_flow_u(flow, 20.0f) + label_w + ae_flow_u(flow, 14.0f);
			cy = r->y + r->height * 0.5f;
			ink = on ? (done ? AE_COLOR_SELECTION_TEXT : AE_COLOR_SELECTION_TEXT) : (done ? AE_COLOR_ACCENT : AE_COLOR_MUTED);
			ae_draw_outline(x, cy - h * 0.5f, w, h, 0.0f, ae_flow_u(flow, 1.0f), ink);
			ae_draw_text_tracked(AE_FONT_ROW, small, 0.06f, x + ae_flow_u(flow, 8.0f), ae_flow_cap_top(AE_FONT_ROW, small, cy), AE_ALIGN_LEFT, ink, text);
			if (challenges.view.guest)
				snprintf(text, sizeof(text), "%s", AE_CV_DASH);
			else if (done)
				snprintf(text, sizeof(text), "DONE");
			else
				snprintf(text, sizeof(text), "%ld / %ld", challenges.view.slot[slot].progress, challenges.view.slot[slot].target);
			ae_draw_text(AE_FONT_ROW, ae_flow_u(flow, 24.0f), r->x + r->width - ae_flow_u(flow, 20.0f),
				ae_flow_cap_top(AE_FONT_ROW, ae_flow_u(flow, 24.0f), cy), AE_ALIGN_RIGHT, on ? AE_COLOR_SELECTION_TEXT : (done ? AE_COLOR_ACCENT : AE_COLOR_TITLE), text);
		}
	}
	/* (the headers' right ends: when the day's and the week's new ones come) */
	{
		int minutes = minutes_to_midnight();
		struct ae_rect const *today = &flow->rects[1], *week = &flow->rects[5];
		float right = today->x + today->width - ae_flow_u(flow, 10.0f);

		if (today->width > 0.0f)
		{
			if (minutes >= 60)
				snprintf(text, sizeof(text), "NEW IN %d H %02d MIN", minutes / 60, minutes % 60);
			else
				snprintf(text, sizeof(text), "NEW IN %d MIN", minutes);
			tracked(16.0f, right, today->y - ae_flow_u(flow, 16.0f), AE_ALIGN_RIGHT, AE_COLOR_MUTED, text);
		}
		if (week->width > 0.0f)
			tracked(16.0f, right, week->y - ae_flow_u(flow, 16.0f), AE_ALIGN_RIGHT, AE_COLOR_MUTED, "NEW ON MONDAY");
	}
}

/* the focused challenge's card */
static void detail_draw(float x, float width)
{
	struct ae_flow_screen *flow = &challenges.flow;
	float u = ae_flow_u(flow, 1.0f), right = x + width;
	int slot = slot_of(flow->focus);
	char text[160], left[64], far[64];
	long ghost = 0;

	ae_draw_rect(x - 24.0f * u, 176.0f * u, width + 48.0f * u, 216.0f * u, 0.0f, AE_COLOR_PANEL);
	ae_draw_rect(x - 24.0f * u, 404.0f * u, width + 48.0f * u, 336.0f * u, 0.0f, AE_COLOR_PANEL);
	if (challenges.view.guest)
	{
		ctext(AE_FONT_ROW, 26.0f, x, 232.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, "PLAYING AS A GUEST");
		ctext(AE_FONT_BODY, 20.0f, x, 274.0f * u, AE_ALIGN_LEFT, AE_COLOR_TEXT, "A guest has no career.");
		ctext(AE_FONT_BODY, 20.0f, x, 304.0f * u, AE_ALIGN_LEFT, AE_COLOR_TEXT, "Sign in a profile on the main menu to keep them.");
	}
	else if (challenges.have)
	{
		if (slot >= 0 && !challenges.view.slot[slot].done)
			ghost = challenges.view.slot[slot].xp;
		ae_career_block_draw(u, x, 190.0f * u, width, &challenges.view, 0, ghost);
	}
	if (slot < 0 || !challenges.view.slot[slot].label[0])
		return;
	{
		struct ae_career_view const *v = &challenges.view;
		long target = v->slot[slot].target, progress = v->slot[slot].progress > target ? target : v->slot[slot].progress, reward = v->slot[slot].xp;
		int done = v->slot[slot].done;
		struct ae_ch_kind const *kind = NULL;
		int each;

		for (each = 0; each < ae_ch_kind_count(); each++)
		{
			char label[48];

			ae_ch_label(ae_ch_kind_at(each), label, sizeof(label));
			if (!strcmp(label, v->slot[slot].label) && ae_ch_kind_at(each)->target == target)
				kind = ae_ch_kind_at(each);
		}
		tracked(16.0f, x, 430.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, slot < AE_CH_PER_PERIOD ? "TODAY'S CHALLENGE" : "THIS WEEK'S CHALLENGE");
		ctext(AE_FONT_ROW, 30.0f, x, 472.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, v->slot[slot].label);
		ae_ch_description(kind, text, sizeof(text));
		{
			/* (the description wraps to the panel: two lines at most, closer together when it takes two) */
			char lines[2][160];
			int count = ae_wrap_text(AE_FONT_BODY, ae_flow_u(flow, 20.0f), text, width, lines, 2), line;

			for (line = 0; line < count; line++)
				ctext(AE_FONT_BODY, 20.0f, x, (count > 1 ? 502.0f + 26.0f * line : 508.0f) * u, AE_ALIGN_LEFT, AE_COLOR_TEXT, lines[line]);
		}
		if (v->guest)
		{
			ctext(AE_FONT_ROW, 24.0f, x, 566.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, "NOT COUNTED FOR A GUEST");
			snprintf(text, sizeof(text), "PAYS +%ld XP", reward);
			ctext(AE_FONT_BODY, 18.0f, x, 628.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, text);
			return;
		}
		if (done)
			ctext(AE_FONT_ROW, 34.0f, x, 566.0f * u, AE_ALIGN_LEFT, AE_COLOR_ACCENT, "DONE");
		else
		{
			snprintf(text, sizeof(text), "%ld / %ld", progress, target);
			ctext(AE_FONT_ROW, 34.0f, x, 566.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, text);
		}
		if (done)
		{
			snprintf(left, sizeof(left), "%ld OF %ld %s", target, target, "DONE");
			ae_cv_challenge_to_go(v->slot[slot].metric, 0, far, sizeof(far));
			snprintf(left, sizeof(left), "%ld OF %ld", target, target);
			snprintf(far, sizeof(far), "PAID +%ld XP", reward);
		}
		else
		{
			ae_cv_challenge_to_go(v->slot[slot].metric, target - progress, left, sizeof(left));
			snprintf(far, sizeof(far), "PAYS +%ld XP", reward);
		}
		ae_career_bar_draw(u, x, 600.0f * u, width, target > 0 ? (int)(progress * 100 / target) : 0, 0, left, far, AE_COLOR_ACCENT);
		if (!done)
		{
			char upper[48], grouped[24];
			int each2;

			ae_draw_rect(x, 656.0f * u, width, u, 0.0f, AE_COLOR_RULE);
			tracked(16.0f, x, 682.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, "AFTER THIS CHALLENGE");
			for (each2 = 0; v->next[each2] && each2 < (int)sizeof(upper) - 1; each2++)
				upper[each2] = v->next[each2] >= 'a' && v->next[each2] <= 'z' ? (char)(v->next[each2] - 32) : v->next[each2];
			upper[each2] = 0;
			if (v->step >= AE_XP_STEPS - 1)
				snprintf(text, sizeof(text), "THE LAST RANK");
			else if (v->to_next > reward)
			{
				ae_cv_number(v->to_next - reward, grouped, sizeof(grouped));
				snprintf(text, sizeof(text), "%s XP TO %s", grouped, upper);
			}
			else
				snprintf(text, sizeof(text), "RANK UP TO %s", upper);
			ctext(AE_FONT_ROW, 22.0f, x, 714.0f * u, AE_ALIGN_LEFT, AE_COLOR_TEXT, text);
		}
	}
	(void)right;
}

static char const *help_text(void)
{
	return "Each challenge pays its XP once. Every finished game pays XP too: 50 to finish, 50 for a win, 25 for a tie, plus kills, "
		"medals and objectives.";
}

static void challenges_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &challenges.flow;
	float width = flow->frame.rect.width * AE_FLOW_ROWS_WIDTH, left, panel_x, u, help_y;
	int wrap;

	(void)instance;
	ae_flow_full_density(flow);
	crumb_read();
	rows_build();
	u = ae_flow_u(flow, 1.0f);
	wrap = ae_draw_text_width(AE_FONT_BODY, ae_flow_u(flow, 21.0f), help_text()) > flow->frame.rect.width - 2.0f * flow->frame.margin;
	ae_flow_title(flow, "CHALLENGES", challenges.crumb);
	left = ae_flow_left_x(flow);
	help_y = ae_flow_content_bottom(flow) - (wrap ? 106.0f : 76.0f) * u;
	flow->rows_bottom_reserve_u = wrap ? 118.0f : 88.0f;
	ae_flow_draw_rows(flow, width);
	rows_extra_draw();
	panel_x = flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X;
	detail_draw(panel_x, flow->frame.rect.x + flow->frame.rect.width - flow->frame.margin - panel_x);
	ae_draw_rect(left, help_y, flow->frame.rect.width - 2.0f * flow->frame.margin, u, 0.0f, AE_COLOR_RULE);
	{
		char const *text = help_text();
		char first[160];
		char const *split = wrap ? strstr(text, "once. ") : NULL;

		if (split)
		{
			snprintf(first, sizeof(first), "%.*s", (int)(split - text) + 5, text);
			ctext(AE_FONT_BODY, 21.0f, left, help_y + 22.0f * u, AE_ALIGN_LEFT, AE_COLOR_TEXT, first);
			ctext(AE_FONT_BODY, 21.0f, left, help_y + 50.0f * u, AE_ALIGN_LEFT, AE_COLOR_TEXT, split + 6);
		}
		else
			ctext(AE_FONT_BODY, 21.0f, left, help_y + 22.0f * u, AE_ALIGN_LEFT, AE_COLOR_TEXT, text);
	}
	flow->prompt_count = 0;
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	flow->prompts[flow->prompt_count] = ae_flow_prompt(AE_BUTTON_LEFT_SHOULDER, "Player", AE_ACTION_TAB_PREVIOUS);
	flow->prompts[flow->prompt_count++].pair = 1;
	ae_flow_draw_prompts(flow);
}

static void player_step(int direction)
{
	short each, candidate = challenges.player;

	for (each = 0; each < AE_SOCIAL_LOCAL; each++)
	{
		candidate = (short)((candidate + direction + AE_SOCIAL_LOCAL) % AE_SOCIAL_LOCAL);
		if (candidate == 0 || ae_screen_main_menu_signed_in(candidate))
		{
			challenges.player = candidate;
			ae_sound_request(AE_SOUND_CURSOR, 0);
			return;
		}
	}
	ae_sound_request(AE_SOUND_FAILURE, 0);
}

static int challenges_handle(struct ae_screen *instance, struct ae_event const *event)
{
	struct ae_flow_screen *flow = &challenges.flow;

	(void)instance;
	switch (event->action)
	{
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		ae_flow_focus_step(flow, event->action == AE_ACTION_UP ? -1 : 1, event->repeat);
		if (slot_of(flow->focus) < 0)
			flow->focus = (short)(event->action == AE_ACTION_UP ? 3 : 1);
		return 1;
	case AE_ACTION_TAB_PREVIOUS:
	case AE_ACTION_TAB_NEXT:
		player_step(event->action == AE_ACTION_TAB_NEXT ? 1 : -1);
		return 1;
	case AE_ACTION_BACK:
		return 0;
	default:
		return 1;
	}
}

static void challenges_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_flow_screen *flow = &challenges.flow;
	struct ae_event event;

	if (ae_prompts_pointer(flow->prompts, flow->prompt_count, pointer, HIT_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK)
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_pop();
		}
		else
			challenges_handle(instance, &event);
		return;
	}
	(void)ae_flow_rows_pointer(flow, pointer);
	if (slot_of(flow->focus) < 0)
		flow->focus = 1;
}

static void challenges_leave(struct ae_screen *instance)
{
	(void)instance;
	platform_log("ae challenges: closed");
}

static struct ae_screen_class const challenges_class =
{
	.name = "challenges", .leave = challenges_leave, .handle = challenges_handle, .draw = challenges_draw, .pointer = challenges_pointer,
};

int ae_screen_challenges_open(char const *crumb, short local_player)
{
	int row, first = 1;

	(void)crumb;
	if (ae_ui_holds(&challenges))
		return 0;
	memset(&challenges, 0, sizeof(challenges));
	challenges.player = local_player < 0 || local_player >= AE_SOCIAL_LOCAL ? 0 : local_player;
	crumb_read();
	challenges.flow.hit_rows = CHALLENGES_HIT;
	challenges.flow.hit_prompts = HIT_PROMPTS;
	rows_build();
	/* (focus starts on the first challenge not done, today's first) */
	for (row = 1; row < ROWS; row++)
		if (slot_of(row) >= 0 && !challenges.view.slot[slot_of(row)].done)
		{
			first = row;
			break;
		}
	challenges.flow.focus = (short)first;
	if (!ae_ui_push(&challenges_class, AE_OWNER_ANY, &challenges))
		return 0;
	platform_log("ae challenges: open (player %d%s)", (int)challenges.player + 1, challenges.view.guest ? ", guest" : "");
	return 1;
}

void ae_screen_challenges_close(void)
{
	(void)ae_ui_remove(&challenges);
}
