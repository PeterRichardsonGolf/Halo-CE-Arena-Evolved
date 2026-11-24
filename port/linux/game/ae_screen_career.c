/*
AE_SCREEN_CAREER.C

See ae_screen_career.h. Layout (the approved mockup C-1, u at 1080): the playlists' rows left on AE's flow (headers TOURNAMENT PLAYLISTS /
RANKED, the rank in the value's place, an "EXP" badge after each label); the right panel from AE_FLOW_PANEL_X: the overall rank, then
the focused playlist's numbers.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "ae_glue_career.h"
#include "ae_glue_social.h"
#include "ae_glue_social_game.h"
#include "ae_input.h"
#include "ae_screen_bot_records.h"
#include "ae_screen_career.h"
#include "ae_screen_main_menu.h"
#include "ae_screen_records.h"
#include "ae_service_record.h"
#include "ae_setup_flow.h"
#include "ae_social_rules.h"
#include "ae_social_view_rules.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum
{
	CAREER_HIT = 0x7780,
	HIT_PROMPTS = CAREER_HIT + 1,
	ROW_HEADER_SOCIAL = 0,
	ROW_BASE = 1
};

static struct
{
	struct ae_flow_screen flow;
	char crumb[64];
	char key[AE_SR_KEY];
	char name[48];
	short player;
	int have_key;
	int count;
	char value[AE_SOCIAL_PLAYLISTS][8];
	/* (0.9.4) the EXPERIENCE line in the panel (layout units, as last drawn; width 0: not drawn), the answer chosen and waiting
	for its confirm (0: none), and the dialog open */
	struct ae_rect experience_rect;
	int experience_wait;
} career;

static struct ae_screen_class const career_class;

static struct ae_social_playlist const *focused(void)
{
	int index = career.flow.focus - ROW_BASE;

	return index >= 0 && index < career.count ? ae_social_playlist_at(index) : NULL;
}

static void profile_read(void)
{
	char name[48];

	career.have_key = ae_social_screen_key(career.player, career.key, sizeof(career.key));
	if (ae_social_profile_name(career.player, name, sizeof(name)))
		snprintf(career.name, sizeof(career.name), "%s", name);
	else
		snprintf(career.name, sizeof(career.name), "PLAYER %d", (int)career.player + 1);
	{
		int at;

		for (at = 0; career.name[at]; at++)
			if (career.name[at] >= 'a' && career.name[at] <= 'z')
				career.name[at] = (char)(career.name[at] - 32);
	}
	if (ae_social_profile_name(career.player, name, sizeof(name)))
		snprintf(career.crumb, sizeof(career.crumb), "%s \xC2\xB7 PLAYER %d", career.name, (int)career.player + 1);
	else
		snprintf(career.crumb, sizeof(career.crumb), "PLAYER %d", (int)career.player + 1);
}

static void rows_build(void)
{
	struct ae_flow_screen *flow = &career.flow;
	short count = 0;
	int index;

	flow->rows[count++] = (struct ae_flow_row){ AE_FLOW_ROW_HEADER, -1, "TOURNAMENT PLAYLISTS", NULL, NULL, NULL };
	career.count = ae_social_playlist_count();
	for (index = 0; index < career.count && count < AE_FLOW_ROWS - 3; index++)
	{
		struct ae_social_playlist const *list = ae_social_playlist_at(index);

		snprintf(career.value[index], sizeof(career.value[index]), "%d", career.have_key ? ae_social_profile_rank(career.key, list->id) : 1);
		flow->rows[count] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, list->label, career.value[index], NULL, NULL };
		flow->rows[count++].status = 1;
	}
	/* (0.9.4: no RANKED placeholder rows: with eleven playlists they ran into the help line at the bottom) */
	flow->count = count;
}

static void ctext(int font, float size_u, float x, float center_y, int align, unsigned int color, char const *text)
{
	float size = ae_flow_u(&career.flow, size_u);

	ae_draw_text(font, size, x, ae_flow_cap_top(font, size, center_y), align, color, text);
}

static void tracked(float size_u, float x, float center_y, int align, unsigned int color, char const *text)
{
	float size = ae_flow_u(&career.flow, size_u);

	ae_draw_text_tracked(AE_FONT_BODY, size, 0.09f, x, ae_flow_cap_top(AE_FONT_BODY, size, center_y), align, color, text);
}

static void bar(float x, float y, float width, int percent)
{
	float h = ae_flow_u(&career.flow, 6.0f);

	ae_draw_rect(x, y, width, h, 0.0f, AE_COLOR_RULE);
	ae_draw_rect(x, y, width * (float)percent / 100.0f, h, 0.0f, AE_COLOR_ACCENT);
}

static void career_stat(float x, float right, float y, char const *label, char const *value, unsigned int color)
{
	tracked(18.0f, x, y, AE_ALIGN_LEFT, AE_COLOR_MUTED, label);
	ctext(AE_FONT_ROW, 28.0f, right, y, AE_ALIGN_RIGHT, color, value);
}

static void panel_draw(float x, float width)
{
	struct ae_flow_screen *flow = &career.flow;
	struct ae_social_playlist const *list = focused();
	long total = career.have_key ? ae_social_profile_exp_total(career.key) : 0;
	float top = ae_flow_u(flow, 190.0f), right = x + width, col2 = x + ae_flow_u(flow, 365.0f);
	char text[96];

	career.experience_rect.width = 0.0f;
	tracked(18.0f, x, top + ae_flow_u(flow, 12.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED, "OVERALL RANK");
	snprintf(text, sizeof(text), "%d", ae_social_overall_rank(total));
	ctext(AE_FONT_TITLE, 72.0f, x, top + ae_flow_u(flow, 66.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE, text);
	{
		float rank_w = ae_draw_text_width(AE_FONT_TITLE, ae_flow_u(flow, 72.0f), text);

		ae_social_view_exp(total, text, (int)sizeof(text));
		ctext(AE_FONT_ROW, 26.0f, x + rank_w + ae_flow_u(flow, 26.0f), top + ae_flow_u(flow, 50.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE, text);
		ctext(AE_FONT_BODY, 20.0f, x + rank_w + ae_flow_u(flow, 26.0f), top + ae_flow_u(flow, 84.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
			"Every playlist added up");
	}
	bar(x, top + ae_flow_u(flow, 126.0f), width, ae_social_overall_percent(total));
	ae_social_view_overall_to(total, text, (int)sizeof(text));
	tracked(16.0f, right, top + ae_flow_u(flow, 150.0f), AE_ALIGN_RIGHT, AE_COLOR_MUTED, text);
	ae_draw_rect(x, top + ae_flow_u(flow, 182.0f), width, ae_flow_u(flow, 1.0f), 0.0f, AE_COLOR_RULE);
	if (!list)
		return;
	{
		struct ae_career_social const *entry = career.have_key ? ae_career_social_find(ae_career_store(), career.key, list->id) : NULL;
		long exp = entry ? entry->exp : 0, games = entry ? entry->games : 0, wins = entry ? entry->wins : 0,
			ties = entry ? entry->ties : 0, quits = entry ? entry->quits : 0, losses = games - wins - ties;
		float y = top + ae_flow_u(flow, 220.0f), bar_x;
		int mode = ae_social_mode(list->id);

		if (losses < 0)
			losses = 0;
		ctext(AE_FONT_TITLE, 40.0f, x, y, AE_ALIGN_LEFT, AE_COLOR_TITLE, list->label);
		y += ae_flow_u(flow, 50.0f);
		snprintf(text, sizeof(text), "RANK %d", ae_social_rank(exp));
		ctext(AE_FONT_ROW, 28.0f, x, y + ae_flow_u(flow, 10.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE, text);
		{
			float rank_w = ae_draw_text_width(AE_FONT_ROW, ae_flow_u(flow, 28.0f), text);

			ae_social_view_exp(exp, text, (int)sizeof(text));
			tracked(18.0f, x + rank_w + ae_flow_u(flow, 14.0f), y + ae_flow_u(flow, 12.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED, text);
			bar_x = x + width * 0.42f;
		}
		bar(bar_x, y + ae_flow_u(flow, 6.0f), right - bar_x, ae_social_rank_percent(exp));
		ae_social_view_wins_to(exp, text, (int)sizeof(text));
		tracked(16.0f, right, y + ae_flow_u(flow, 30.0f), AE_ALIGN_RIGHT, AE_COLOR_MUTED, text);
		y += ae_flow_u(flow, 80.0f);
		snprintf(text, sizeof(text), "%ld", games);
		career_stat(x, x + ae_flow_u(flow, 339.0f), y, "GAMES", text, AE_COLOR_TITLE);
		snprintf(text, sizeof(text), "%ld", wins);
		career_stat(col2, right - ae_flow_u(flow, 24.0f), y, "WINS", text, AE_COLOR_TITLE);
		y += ae_flow_u(flow, 48.0f);
		snprintf(text, sizeof(text), "%ld", ties);
		career_stat(x, x + ae_flow_u(flow, 339.0f), y, "TIES", text, AE_COLOR_TITLE);
		snprintf(text, sizeof(text), "%ld", losses);
		career_stat(col2, right - ae_flow_u(flow, 24.0f), y, "LOSSES", text, AE_COLOR_TITLE);
		y += ae_flow_u(flow, 48.0f);
		snprintf(text, sizeof(text), "%ld", quits);
		career_stat(x, x + ae_flow_u(flow, 339.0f), y, "LEFT EARLY", text, AE_COLOR_TITLE);
		snprintf(text, sizeof(text), "%ld %%", games > 0 ? (wins + ties) * 100 / games : 0);
		career_stat(col2, right - ae_flow_u(flow, 24.0f), y, "WIN RATE", text, AE_COLOR_TITLE);
		y += ae_flow_u(flow, 60.0f);
		career_stat(x, x + ae_flow_u(flow, 339.0f), y, "BOT SKILL", mode ? ae_social_skill_name(mode) : "AUTO", AE_COLOR_ACCENT);
		/* (0.9.4) EXPERIENCE: the calibration answer, changed with X or a click (the panel, not a list row: the list is full) */
		if (career.have_key)
		{
			int answer = ae_social_profile_experience(career.key);

			y += ae_flow_u(flow, 48.0f);
			career_stat(x, x + ae_flow_u(flow, 339.0f), y, "EXPERIENCE", answer ? ae_social_experience_name(answer) : "NOT SET",
				AE_COLOR_ACCENT);
			career.experience_rect.x = x;
			career.experience_rect.y = y - ae_flow_u(flow, 24.0f);
			career.experience_rect.width = ae_flow_u(flow, 339.0f);
			career.experience_rect.height = ae_flow_u(flow, 48.0f);
		}
	}
}

static void badges_draw(void)
{
	struct ae_flow_screen *flow = &career.flow;
	float label_size = ae_size_text(&flow->density), small = ae_flow_u(flow, 15.0f);
	int index;

	for (index = 0; index < career.count; index++)
	{
		struct ae_social_playlist const *list = ae_social_playlist_at(index);
		struct ae_rect const *r = &flow->rects[ROW_BASE + index];
		char text[24];
		float label_w, w, h = ae_flow_u(flow, 22.0f), x, cy;
		int on = (ROW_BASE + index) == flow->focus;

		if (r->width <= 0.0f || !list)
			continue;
		ae_social_view_exp(career.have_key ? ae_social_profile_exp(career.key, list->id) : 0, text, (int)sizeof(text));
		label_w = ae_draw_text_width(AE_FONT_ROW, label_size, list->label);
		w = ae_draw_text_tracked_width(AE_FONT_ROW, small, 0.06f, text) + ae_flow_u(flow, 16.0f);
		x = r->x + ae_flow_u(flow, 20.0f) + label_w + ae_flow_u(flow, 14.0f);
		cy = r->y + r->height * 0.5f;
		ae_draw_outline(x, cy - h * 0.5f, w, h, 0.0f, ae_flow_u(flow, 1.0f), on ? AE_COLOR_SELECTION_TEXT : AE_COLOR_MUTED);
		ae_draw_text_tracked(AE_FONT_ROW, small, 0.06f, x + ae_flow_u(flow, 8.0f), ae_flow_cap_top(AE_FONT_ROW, small, cy), AE_ALIGN_LEFT,
			on ? AE_COLOR_SELECTION_TEXT : AE_COLOR_MUTED, text);
	}
}

/* ---------- (0.9.4) EXPERIENCE: the calibration answer, changed later */

static void experience_picked(short choice, void *context)
{
	(void)context;
	if (choice < 0 || choice > 2 || !career.have_key)
		return;
	/* (the confirm opens from the next update: this dialog is still closing) */
	career.experience_wait = AE_SOCIAL_EXPERIENCE_NEW + choice;
}

static void experience_confirmed(short choice, void *context)
{
	int answer = career.experience_wait;

	(void)context;
	career.experience_wait = 0;
	if (choice != 1 || !career.have_key || answer <= 0)
		return;
	if (ae_social_profile_experience_set(career.key, answer, 1))
		ae_sound_request(AE_SOUND_FORWARD, 0);
	else
		ae_sound_request(AE_SOUND_FAILURE, 0);
}

static void experience_ask(void)
{
	static char labels[3][48];
	struct ae_dialog_spec spec;
	int experience;

	if (!career.have_key)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	for (experience = AE_SOCIAL_EXPERIENCE_NEW; experience < NUMBER_OF_AE_SOCIAL_EXPERIENCES; experience++)
		snprintf(labels[experience - 1], sizeof(labels[0]), "%s: starts at %s bots", ae_social_experience_name(experience),
			ae_social_experience_skill(experience));
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = "HOW MUCH HALO HAVE YOU PLAYED?";
	spec.body = "Changing it starts every AE TOURNAMENT playlist again. You are asked to confirm next.";
	for (experience = 0; experience < 3; experience++)
		spec.choices[experience] = labels[experience];
	spec.choice_count = 3;
	spec.safe_choice = ae_social_profile_experience(career.key) > 0 ? ae_social_profile_experience(career.key) - 1 : 1;
	spec.cancel_choice = -1;
	spec.has_cancel = 1;
	spec.density = career.flow.density;
	spec.bounds = career.flow.frame.rect;
	spec.picked = experience_picked;
	if (ae_dialog_open(&spec, AE_OWNER_ANY))
		ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void experience_confirm(void)
{
	static char title[64], body[320];
	char const *name = career.name;
	struct ae_dialog_spec spec;
	int answer = career.experience_wait;

	if (answer == ae_social_profile_experience(career.key))
	{
		career.experience_wait = 0;
		return;
	}
	snprintf(title, sizeof(title), "CHANGE EXPERIENCE TO %s?", ae_social_experience_name(answer));
	snprintf(body, sizeof(body), "Every AE TOURNAMENT playlist of %s starts again at %s bots, with its %d placement games counted "
		"from 0. EXP and ranks stay.", name, ae_social_experience_skill(answer), (int)AE_SOCIAL_PLACEMENT_GAMES);
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = title;
	spec.body = body;
	spec.choices[0] = ae_string(AE_STR_CANCEL_CAPS);
	spec.choices[1] = "CHANGE";
	spec.choice_count = 2;
	spec.safe_choice = 0;
	spec.cancel_choice = 0;
	spec.has_cancel = 1;
	spec.density = career.flow.density;
	spec.bounds = career.flow.frame.rect;
	spec.picked = experience_confirmed;
	if (!ae_dialog_open(&spec, AE_OWNER_ANY))
		career.experience_wait = 0;
}

static void career_update(struct ae_screen *instance)
{
	(void)instance;
	if (career.experience_wait > 0)
		experience_confirm();
}

static void career_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &career.flow;
	float width = flow->frame.rect.width * AE_FLOW_ROWS_WIDTH, left, panel_x, help_y;

	(void)instance;
	ae_flow_full_density(flow);
	profile_read();
	rows_build();
	ae_flow_title(flow, "CAREER", career.crumb);
	left = ae_flow_left_x(flow);
	ae_flow_draw_rows(flow, width);
	badges_draw();
	tracked(18.0f, left + width - ae_flow_u(flow, 10.0f), ae_flow_u(flow, 190.0f) + ae_flow_u(flow, 22.0f), AE_ALIGN_RIGHT, AE_COLOR_MUTED,
		"RANK");
	panel_x = flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X;
	panel_draw(panel_x, flow->frame.rect.x + flow->frame.rect.width - flow->frame.margin - panel_x);
	help_y = ae_flow_u(flow, 902.0f);
	ae_draw_rect(left, help_y, flow->frame.rect.width - 2.0f * flow->frame.margin, ae_flow_u(flow, 1.0f), 0.0f, AE_COLOR_RULE);
	ctext(AE_FONT_BODY, 21.0f, left, help_y + ae_flow_u(flow, 22.0f), AE_ALIGN_LEFT, AE_COLOR_TEXT,
		"+1 EXP for a win or a tie, 0 for a loss, -1 for leaving a game before its end. Your rank comes from your EXP. Kept on this PC only.");
	flow->prompt_count = 0;
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	/* Y: RECORDS, the players and the bots this PC has a service record of */
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_Y, "Records", AE_ACTION_Y);
	if (career.have_key)
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_X, "Experience", AE_ACTION_X);
	flow->prompts[flow->prompt_count] = ae_flow_prompt(AE_BUTTON_LEFT_SHOULDER, "Player", AE_ACTION_TAB_PREVIOUS);
	flow->prompts[flow->prompt_count++].pair = 1;
	ae_flow_draw_prompts(flow);
}

static void player_step(int direction)
{
	short each, candidate = career.player;

	for (each = 0; each < AE_SOCIAL_LOCAL; each++)
	{
		candidate = (short)((candidate + direction + AE_SOCIAL_LOCAL) % AE_SOCIAL_LOCAL);
		if (candidate == 0 || ae_screen_main_menu_signed_in(candidate))
		{
			career.player = candidate;
			ae_sound_request(AE_SOUND_CURSOR, 0);
			platform_log("ae career: player %d", (int)candidate + 1);
			return;
		}
	}
	ae_sound_request(AE_SOUND_FAILURE, 0);
}

static int career_handle(struct ae_screen *instance, struct ae_event const *event)
{
	struct ae_flow_screen *flow = &career.flow;

	(void)instance;
	switch (event->action)
	{
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		ae_flow_focus_step(flow, event->action == AE_ACTION_UP ? -1 : 1, event->repeat);
		/* (the RANKED row is a status: the focus stays on the playlists) */
		if (flow->focus - ROW_BASE >= career.count)
			flow->focus = (short)(ROW_BASE + (event->action == AE_ACTION_UP ? career.count - 1 : 0));
		return 1;
	case AE_ACTION_TAB_PREVIOUS:
	case AE_ACTION_TAB_NEXT:
		player_step(event->action == AE_ACTION_TAB_NEXT ? 1 : -1);
		return 1;
	case AE_ACTION_X:
		experience_ask();
		return 1;
	case AE_ACTION_Y:
		if (ae_screen_records_open(career.player))
			ae_sound_request(AE_SOUND_FORWARD, 0);
		return 1;
	case AE_ACTION_BACK:
		return 0;
	default:
		return 1;
	}
}

static void career_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_flow_screen *flow = &career.flow;
	struct ae_event event;
	short row;

	if (ae_prompts_pointer(flow->prompts, flow->prompt_count, pointer, HIT_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK)
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_pop();
		}
		else
			career_handle(instance, &event);
		return;
	}
	if (pointer->left_clicks && career.experience_rect.width > 0.0f && pointer->x >= career.experience_rect.x &&
		pointer->x < career.experience_rect.x + career.experience_rect.width && pointer->y >= career.experience_rect.y &&
		pointer->y < career.experience_rect.y + career.experience_rect.height)
	{
		experience_ask();
		return;
	}
	row = ae_flow_rows_pointer(flow, pointer);
	(void)row;
	if (flow->focus - ROW_BASE >= career.count)
		flow->focus = (short)(ROW_BASE + career.count - 1);
}

static void career_leave(struct ae_screen *instance)
{
	(void)instance;
	platform_log("ae career: closed");
}

static struct ae_screen_class const career_class =
{
	.name = "career", .leave = career_leave, .handle = career_handle, .draw = career_draw, .pointer = career_pointer,
	.update = career_update,
};

int ae_screen_career_open(char const *crumb, short local_player, char const *playlist)
{
	int index;

	(void)crumb;
	if (ae_ui_holds(&career))
		return 0;
	memset(&career, 0, sizeof(career));
	career.player = local_player < 0 || local_player >= AE_SOCIAL_LOCAL ? 0 : local_player;
	career.flow.hit_rows = CAREER_HIT;
	career.flow.hit_prompts = HIT_PROMPTS;
	profile_read();
	rows_build();
	career.flow.focus = ROW_BASE;
	for (index = 0; playlist && index < career.count; index++)
		if (!strcmp(ae_social_playlist_at(index)->id, playlist))
			career.flow.focus = (short)(ROW_BASE + index);
	if (!ae_ui_push(&career_class, AE_OWNER_ANY, &career))
		return 0;
	platform_log("ae career: open (player %d, %s)", (int)career.player + 1, career.have_key ? career.key : "no profile");
	return 1;
}

void ae_screen_career_close(void)
{
	(void)ae_ui_remove(&career);
}
