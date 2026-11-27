/*
AE_SCREEN_CAREER.C

See ae_screen_career.h. Layout (the approved mockup C-1, u at 1080): the playlists' rows left on AE's flow (headers TOURNAMENT PLAYLISTS /
SKILL RANK, the OVERALL row first, the rank in the value's place and a dash for a playlist not played); the right panel from AE_FLOW_PANEL_X:
the career rank, the overall or focused skill rank with its bar and numbers, and the MATCHMAKING row.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "ae_career_draw.h"
#include "ae_career_view_rules.h"
#include "ae_glue_career.h"
#include "ae_glue_career_xp.h"
#include "ae_forge_maps_rules.h"
#include "ae_screen_forge.h"
#include "../src/port_config.h"
#include "ae_mcc_rank.h"
#include "ae_screen_challenges.h"
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
	char value[AE_SOCIAL_PLAYLISTS + 1][8];
	/* (rc9) the FORGE rows' values: the time in Forge, the maps made */
	char forge_time[24], forge_maps[16];
	/* (0.9.4) the EXPERIENCE line in the panel (layout units, as last drawn; width 0: not drawn), the answer chosen and waiting
	for its confirm (0: none), and the dialog open */
	struct ae_rect experience_rect;
	int experience_wait;
} career;

static struct ae_screen_class const career_class;

/* the focused row's playlist: NULL on the OVERALL row */
static struct ae_social_playlist const *focused(void)
{
	int index = career.flow.focus - ROW_BASE - 1;

	return index >= 0 && index < career.count ? ae_social_playlist_at(index) : NULL;
}

static int on_overall(void)
{
	return career.flow.focus == ROW_BASE;
}

/* the entry of a playlist (NULL: none yet) */
static struct ae_career_social const *entry_of(struct ae_social_playlist const *list)
{
	return career.have_key && list ? ae_career_social_find(ae_career_store(), career.key, list->id) : NULL;
}

/* whether the profile has played a game of the playlist */
static int played(struct ae_social_playlist const *list)
{
	struct ae_career_social const *entry = entry_of(list);

	return entry && entry->games > 0;
}

/* games in every playlist */
static int played_any(void)
{
	int index;

	for (index = 0; index < career.count; index++)
		if (played(ae_social_playlist_at(index)))
			return 1;
	return 0;
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
	/* (the OVERALL row first: every playlist on one longer ladder) */
	if (career.have_key && played_any())
		snprintf(career.value[0], sizeof(career.value[0]), "%d", ae_social_overall_rank(ae_social_profile_exp_total(career.key)));
	else
		snprintf(career.value[0], sizeof(career.value[0]), "%s", AE_CV_DASH);
	/* (MCC's skill picture takes the overall number too, as on the playlist rows: badges_draw draws it) */
	if (career.have_key && played_any() && ae_career_art_on() && ae_mcc_rank_available())
		career.value[0][0] = 0;
	flow->rows[count] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "OVERALL", career.value[0], NULL, NULL };
	flow->rows[count].value_title = career.have_key && played_any();
	flow->rows[count++].status = 1;
	for (index = 0; index < career.count && count < AE_FLOW_ROWS - 3; index++)
	{
		struct ae_social_playlist const *list = ae_social_playlist_at(index);

		if (played(list))
			snprintf(career.value[index + 1], sizeof(career.value[0]), "%d", ae_social_profile_rank(career.key, list->id));
		else
			snprintf(career.value[index + 1], sizeof(career.value[0]), "%s", AE_CV_DASH);
		/* (MCC's skill picture takes a played playlist's number: badges_draw draws it) */
		if (played(list) && ae_career_art_on() && ae_mcc_rank_available())
			career.value[index + 1][0] = 0;
		flow->rows[count] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, list->label, career.value[index + 1], NULL, NULL };
		flow->rows[count].value_title = played(list);
		flow->rows[count++].status = 1;
	}
	/* (rc9: Content Evolved's creator card has FORGE: TIME and MAPS MADE; the build that has Forge shows them) */
	if (ae_screen_forge_available() && count < AE_FLOW_ROWS - 3)
	{
		ae_forge_maps_duration_text(config_integer("forge.seconds"), career.forge_time, (int)sizeof(career.forge_time));
		snprintf(career.forge_maps, sizeof(career.forge_maps), "%ld", (long)config_integer("forge.maps_made"));
		flow->rows[count++] = (struct ae_flow_row){ AE_FLOW_ROW_HEADER, -1, "FORGE", NULL, NULL, NULL };
		flow->rows[count] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "FORGE TIME", career.forge_time, NULL, NULL };
		flow->rows[count++].status = 1;
		flow->rows[count] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "FORGE MAPS MADE", career.forge_maps, NULL, NULL };
		flow->rows[count++].status = 1;
	}
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

/* the totals of the focused row: one playlist's, or every playlist's on the OVERALL row */
static void totals(long *games, long *wins, long *ties, long *quits, long *exp)
{
	struct ae_social_playlist const *list = focused();
	int index;

	*games = *wins = *ties = *quits = *exp = 0;
	for (index = 0; index < career.count; index++)
	{
		struct ae_social_playlist const *each = ae_social_playlist_at(index);
		struct ae_career_social const *entry = entry_of(each);

		if (!entry || (list && each != list))
			continue;
		*games += entry->games;
		*wins += entry->wins;
		*ties += entry->ties;
		*quits += entry->quits;
		*exp += entry->exp;
	}
}

/* the help line's text: one sentence of the skill, one of the career */
static char const *help_text(void)
{
	return career.have_key ?
		"Skill rank: a win or a tie moves it up, a loss does not change it, leaving early moves it back. Career XP comes from every "
		"finished game and is never lost. Kept on this PC." :
		"Sign in a profile on the main menu to keep a career. Kept on this PC only.";
}

/* whether the help line is wider than its room and needs two lines */
static int help_wraps(void)
{
	struct ae_flow_screen *flow = &career.flow;

	return ae_draw_text_width(AE_FONT_BODY, ae_flow_u(flow, 21.0f), help_text()) > flow->frame.rect.width - 2.0f * flow->frame.margin;
}

static void grid_cell(float x, float label_y, float value_y, char const *label, char const *value)
{
	tracked(16.0f, x, label_y, AE_ALIGN_LEFT, AE_COLOR_MUTED, label);
	ctext(AE_FONT_ROW, 30.0f, x, value_y, AE_ALIGN_LEFT, AE_COLOR_TITLE, value);
}

static void panel_draw(float x, float width)
{
	struct ae_flow_screen *flow = &career.flow;
	struct ae_social_playlist const *list = focused();
	struct ae_career_view view;
	float u = ae_flow_u(flow, 1.0f), top = 190.0f * u, right = x + width;
	int wrap = help_wraps();
	char text[96], left[64], far[64];

	career.experience_rect.width = 0.0f;
	/* (a card under the panel: its text stays readable over the scene, the ring art included) */
	ae_draw_rect(x - 24.0f * u, top - 14.0f * u, width + 48.0f * u, (wrap ? 672.0f : 702.0f) * u, 0.0f, AE_COLOR_PANEL);
	if (!career.have_key || !ae_career_xp_view(career.player, &view))
	{
		ctext(AE_FONT_ROW, 26.0f, x, 470.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, "PLAYING AS A GUEST");
		ctext(AE_FONT_BODY, 20.0f, x, 512.0f * u, AE_ALIGN_LEFT, AE_COLOR_TEXT, "A guest has no career, challenges or skill ranks.");
		ctext(AE_FONT_BODY, 20.0f, x, 542.0f * u, AE_ALIGN_LEFT, AE_COLOR_TEXT, "Sign in a profile on the main menu to keep them.");
		return;
	}
	/* section A: the career rank */
	ae_career_block_draw(u, x, top, width, &view, AE_CAREER_BLOCK_SUMMARY, 0);
	ae_draw_rect(x, top + 184.0f * u, width, u, 0.0f, AE_COLOR_RULE);
	/* section B: the focused row's skill rank */
	{
		long games, wins, ties, quits, exp;
		char const *names[2];
		int rank, percent;
		long to_go;

		totals(&games, &wins, &ties, &quits, &exp);
		if (list)
		{
			rank = ae_social_rank(exp);
			percent = ae_social_rank_percent(exp);
			to_go = ae_social_exp_to_next(exp);
			names[0] = "SKILL RANK";
			names[1] = list->label;
		}
		else
		{
			long total = ae_social_profile_exp_total(career.key);

			rank = ae_social_overall_rank(total);
			percent = ae_social_overall_percent(total);
			to_go = ae_social_overall_exp_to_next(total);
			names[0] = "OVERALL SKILL RANK";
			names[1] = "EVERY PLAYLIST ON ONE LONGER LADDER";
		}
		ae_career_dots_draw(u, AE_FONT_BODY, 16.0f, 0.09f, x, top + 210.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, names, 2);
		if (games <= 0)
		{
			ctext(AE_FONT_ROW, 26.0f, x, top + 256.0f * u, AE_ALIGN_LEFT, AE_COLOR_TEXT, "NO GAMES YET");
			ctext(AE_FONT_BODY, 18.0f, x, top + 302.0f * u, AE_ALIGN_LEFT, AE_COLOR_TEXT, "FINISH A GAME TO GET A RANK");
		}
		else
		{
			if (!ae_skill_picture_draw(rank, x + 28.0f * u, top + 256.0f * u, 56.0f * u, 1.0f))
			{
				snprintf(text, sizeof(text), "%d", rank);
				ctext(AE_FONT_ROW, 44.0f, x, top + 256.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, text);
			}
			if (list)
				ae_cv_skill_captions(rank, to_go, left, sizeof(left), far, sizeof(far));
			else
				ae_cv_overall_captions(rank, to_go, left, sizeof(left), far, sizeof(far));
			ae_career_bar_draw(u, x, top + 294.0f * u, width, percent, 0, left, far, AE_COLOR_MUTED);
			{
				float column = width / 3.0f;
				char grouped[24];

				snprintf(grouped, sizeof(grouped), "%ld", games);
				grid_cell(x, top + 366.0f * u, top + 398.0f * u, "GAMES", grouped);
				snprintf(grouped, sizeof(grouped), "%ld", wins);
				grid_cell(x + column, top + 366.0f * u, top + 398.0f * u, "WINS", grouped);
				ae_cv_win_rate(games, wins, grouped, sizeof(grouped));
				grid_cell(x + 2.0f * column, top + 366.0f * u, top + 398.0f * u, "WIN RATE", grouped);
				snprintf(grouped, sizeof(grouped), "%ld", ties);
				grid_cell(x, top + 442.0f * u, top + 474.0f * u, "TIES", grouped);
				snprintf(grouped, sizeof(grouped), "%ld", ae_cv_losses(games, wins, ties, quits));
				grid_cell(x + column, top + 442.0f * u, top + 474.0f * u, "LOSSES", grouped);
				snprintf(grouped, sizeof(grouped), "%ld", quits);
				grid_cell(x + 2.0f * column, top + 442.0f * u, top + 474.0f * u, "LEFT EARLY", grouped);
			}
		}
	}
	ae_draw_rect(x, top + 508.0f * u, width, u, 0.0f, AE_COLOR_RULE);
	/* section C: the one setting that lives here */
	tracked(16.0f, x, top + 532.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, "MATCHMAKING");
	{
		int answer = ae_social_profile_experience(career.key);
		float row_y = top + 552.0f * u, row_h = 50.0f * u, cy = row_y + row_h * 0.5f, mark = 26.0f * u;
		float mark_x = right - 20.0f * u - mark;

		ae_draw_rect(x, row_y, width, row_h, 0.0f, AE_COLOR_ROW);
		ctext(AE_FONT_ROW, 20.0f, x + 20.0f * u, cy, AE_ALIGN_LEFT, AE_COLOR_TITLE, "MY HALO EXPERIENCE");
		ctext(AE_FONT_ROW, 22.0f, mark_x - 12.0f * u, cy, AE_ALIGN_RIGHT, AE_COLOR_ACCENT,
			answer ? ae_social_experience_name(answer) : "NOT SET");
		ae_widget_button_mark(&flow->density, AE_BUTTON_X, mark_x, cy, mark, 0, 1);
		career.experience_rect.x = x;
		career.experience_rect.y = row_y;
		career.experience_rect.width = width;
		career.experience_rect.height = row_h;
	}
}

/* a played playlist's rank as MCC's picture where MCC's files have it (the list's number otherwise) */
static void badges_draw(void)
{
	struct ae_flow_screen *flow = &career.flow;
	int index;

	if (!ae_career_art_shown())
		return;
	if (career.have_key && played_any())
	{
		struct ae_rect const *r = &flow->rects[ROW_BASE];
		float size = ae_flow_u(flow, 34.0f);

		if (r->width > 0.0f)
			ae_skill_picture_draw(ae_social_overall_rank(ae_social_profile_exp_total(career.key)),
				r->x + r->width - ae_flow_u(flow, 16.0f) - size * 0.5f, r->y + r->height * 0.5f, size, 1.0f);
	}
	for (index = 0; index < career.count; index++)
	{
		struct ae_social_playlist const *list = ae_social_playlist_at(index);
		struct ae_rect const *r = &flow->rects[ROW_BASE + 1 + index];
		float size = ae_flow_u(flow, 34.0f);

		if (r->width <= 0.0f || !played(list))
			continue;
		ae_skill_picture_draw(ae_social_profile_rank(career.key, list->id), r->x + r->width - ae_flow_u(flow, 16.0f) - size * 0.5f,
			r->y + r->height * 0.5f, size, 1.0f);
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
		"from 0. Skill ranks and XP stay.", name, ae_social_experience_skill(answer), (int)AE_SOCIAL_PLACEMENT_GAMES);
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
	float width = flow->frame.rect.width * AE_FLOW_ROWS_WIDTH, left, panel_x, help_y, u;
	int wrap;

	(void)instance;
	ae_flow_full_density(flow);
	profile_read();
	rows_build();
	u = ae_flow_u(flow, 1.0f);
	wrap = help_wraps();
	ae_flow_title(flow, "CAREER", career.crumb);
	left = ae_flow_left_x(flow);
	/* (the help line's rule stands 76 u above the footer's room, 106 u when the line wraps; the rows end 12 u above the rule: the last
	rows scroll with the focus) */
	help_y = ae_flow_content_bottom(flow) - ae_flow_u(flow, wrap ? 106.0f : 76.0f);
	flow->rows_bottom_reserve_u = wrap ? 118.0f : 88.0f;
	ae_flow_draw_rows(flow, width);
	badges_draw();
	/* (the column header on the list's own header row, at the baseline of "TOURNAMENT PLAYLISTS") */
	tracked(18.0f, left + width - ae_flow_u(flow, 10.0f), flow->rects[0].y + flow->rects[0].height - ae_flow_u(flow, 10.0f) -
		ae_draw_cap_height(AE_FONT_BODY, ae_flow_u(flow, 18.0f)) * 0.5f, AE_ALIGN_RIGHT, AE_COLOR_MUTED, "SKILL RANK");
	panel_x = flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X;
	panel_draw(panel_x, flow->frame.rect.x + flow->frame.rect.width - flow->frame.margin - panel_x);
	ae_draw_rect(left, help_y, flow->frame.rect.width - 2.0f * flow->frame.margin, ae_flow_u(flow, 1.0f), 0.0f, AE_COLOR_RULE);
	{
		char const *text = help_text();
		char first[200], second[200];
		char const *split = wrap ? strstr(text, ". ") : NULL;
		float room = flow->frame.rect.width - 2.0f * flow->frame.margin;

		/* Prefer the sentence break while it fits; narrower frames wrap both lines to their available width. */
		while (split && strstr(split + 2, ". ") && !strstr(split, "back. "))
			split = strstr(split + 2, ". ");
		if (split && wrap)
		{
			snprintf(first, sizeof(first), "%.*s", (int)(split - text) + 1, text);
			snprintf(second, sizeof(second), "%s", split + 2);
			if (ae_draw_text_width(AE_FONT_BODY, ae_flow_u(flow, 21.0f), first) <= room)
			{
				ctext(AE_FONT_BODY, 21.0f, left, help_y + 22.0f * u, AE_ALIGN_LEFT, AE_COLOR_TEXT, first);
				ctext(AE_FONT_BODY, 21.0f, left, help_y + 50.0f * u, AE_ALIGN_LEFT, AE_COLOR_TEXT, second);
			}
			else
			{
				char lines[2][160];
				int count = ae_wrap_text(AE_FONT_BODY, ae_flow_u(flow, 21.0f), text, room, lines, 2), line;

				for (line = 0; line < count; line++)
					ctext(AE_FONT_BODY, 21.0f, left, help_y + (22.0f + 28.0f * line) * u, AE_ALIGN_LEFT,
						AE_COLOR_TEXT, lines[line]);
			}
		}
		else
			ctext(AE_FONT_BODY, 21.0f, left, help_y + 22.0f * u, AE_ALIGN_LEFT, AE_COLOR_TEXT, text);
	}
	flow->prompt_count = 0;
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	/* Y: RECORDS, the players and the bots this PC has a service record of */
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_Y, "Records", AE_ACTION_Y);
	if (career.have_key)
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_X, "Experience", AE_ACTION_X);
	flow->prompts[flow->prompt_count] = ae_flow_prompt(AE_BUTTON_LEFT_SHOULDER, "Player", AE_ACTION_TAB_PREVIOUS);
	flow->prompts[flow->prompt_count++].pair = 1;
	if (career.have_key)
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_LEFT_TRIGGER, "Challenges", AE_ACTION_PAGE_UP);
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
		if (flow->focus - ROW_BASE > career.count)
			flow->focus = (short)(ROW_BASE + (event->action == AE_ACTION_UP ? career.count : 0));
		return 1;
	case AE_ACTION_TAB_PREVIOUS:
	case AE_ACTION_TAB_NEXT:
		player_step(event->action == AE_ACTION_TAB_NEXT ? 1 : -1);
		return 1;
	case AE_ACTION_X:
		experience_ask();
		return 1;
	case AE_ACTION_PAGE_UP:
		if (career.have_key && ae_screen_challenges_open(career.crumb, career.player))
			ae_sound_request(AE_SOUND_FORWARD, 0);
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
	if (flow->focus - ROW_BASE > career.count)
		flow->focus = (short)(ROW_BASE + career.count);
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
			career.flow.focus = (short)(ROW_BASE + 1 + index);
	if (!ae_ui_push(&career_class, AE_OWNER_ANY, &career))
		return 0;
	platform_log("ae career: open (player %d, %s)", (int)career.player + 1, career.have_key ? career.key : "no profile");
	return 1;
}

void ae_screen_career_close(void)
{
	(void)ae_ui_remove(&career);
}
