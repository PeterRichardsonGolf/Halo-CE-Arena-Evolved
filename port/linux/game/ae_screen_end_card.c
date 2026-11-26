/*
AE_SCREEN_END_CARD.C

See ae_screen_end_card.h. Layout (u at 1080): the title MISSION COMPLETE on the frame's title line with the player's place at the
right, the mission's sub-line, a strip with the player's name and who plays; the left half's rows TIME, KILLS, DEATHS and the
medals; the right half's SCORE panel (a 4 u accent top) with the multipliers, the final score and, in co-op, each player's share.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "halo_skulls.h"
#include "../src/ae_draw.h"
#include "ae_glue_campaign.h"
#include "ae_input.h"
#include "ae_pve_menus_rules.h"
#include "ae_pve_menus_ui.h"
#include "ae_pve_score_menus.h"
#include "ae_screen_end_card.h"
#include "ae_screen_campaign.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_style.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);
boolean ae_menus_active(void);

enum { CARD_HIT = 0x7AE0, HIT_PROMPTS = CARD_HIT + 1 };

static struct
{
	struct ae_flow_screen flow;
	struct ae_pve_card card;
	int index, count, last_mission, on_stack;
	struct ae_pve_menus_press press;
	unsigned long opened;
} end;

/* a made-up summary (DEV MODE's `menu endcard`): its cards, which one is shown, and whether it is the last mission's */
static struct
{
	int on, count, shown, last;
	struct ae_pve_card card[4];
} staged;

static void shown_read(void)
{
	if (staged.on)
	{
		end.card = staged.card[staged.shown];
		end.index = staged.shown;
		end.count = staged.count;
		return;
	}
	if (!ae_pve_card_shown(&end.card, &end.index, &end.count))
		end.count = 0;
}

int ae_end_card_wanted(void)
{
	return ae_menus_active() ? 1 : 0;
}

/* a row of the left side: its label, value and a quiet line under the value */
static void stat_row(float x, float width, float y, float height, const char *label, const char *value, const char *sub, int badge)
{
	struct ae_flow_screen const *flow = &end.flow;
	float u = ae_flow_u(flow, 1.0f), mid = y + height * 0.5f;

	ae_draw_rect(x, y, width, height, 0.0f, AE_COLOR_ROW);
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 17.0f, x + 22.0f * u, sub ? mid - 12.0f * u : mid, AE_ALIGN_LEFT, AE_COLOR_MUTED, label);
	if (sub)
		ae_pve_menus_ui_text(flow, AE_FONT_BODY, 15.0f, x + 22.0f * u, mid + 14.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, sub);
	if (badge)
	{
		float w = 108.0f * u, h = 22.0f * u;

		ae_draw_rect(x + width - 220.0f * u - w, mid - h * 0.5f, w, h, h * 0.5f, AE_COLOR_ACCENT);
		ae_pve_menus_ui_text(flow, AE_FONT_ROW, 13.0f, x + width - 220.0f * u - w * 0.5f, mid, AE_ALIGN_CENTER, AE_COLOR_SELECTION_TEXT, "BEST TIME");
	}
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 34.0f, x + width - 22.0f * u, mid, AE_ALIGN_RIGHT, AE_COLOR_TITLE, value);
}

static void medals_draw(float x, float width, float y)
{
	struct ae_flow_screen const *flow = &end.flow;
	float u = ae_flow_u(flow, 1.0f), cell = width / 3.0f, picture = 56.0f * u;
	int each, shown = 0;

	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, y, AE_ALIGN_LEFT, AE_COLOR_MUTED, "MEDALS");
	y += 22.0f * u;
	for (each = 0; each < ae_pve_card_medal_count() && each < AE_PVE_CARD_MEDALS && shown < 12; each++)
	{
		char key[96], text[32];
		float cx, cy;
		int image;

		if (end.card.medal_counts[each] <= 0)
			continue;
		cx = x + cell * (float)(shown % 3);
		cy = y + 70.0f * u * (float)(shown / 3);
		snprintf(key, sizeof(key), "medal_%s", ae_pve_card_medal_key(each));
		image = ae_pve_menus_ui_ce_picture(key);
		if (image)
			ae_draw_image(image, cx, cy, picture, picture, 0xFFFFFFFFu);
		snprintf(text, sizeof(text), "x%d", (int)end.card.medal_counts[each]);
		ae_pve_menus_ui_text(flow, AE_FONT_ROW, 24.0f, cx + picture + 10.0f * u, cy + 18.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, text);
		{
			char fitted[48];

			ae_fit_text(AE_FONT_BODY, 14.0f * u, ae_pve_card_medal_label(each), cell - picture - 22.0f * u, fitted, (int)sizeof(fitted));
			ae_pve_menus_ui_text(flow, AE_FONT_BODY, 14.0f, cx + picture + 10.0f * u, cy + 42.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, fitted);
		}
		shown++;
	}
}

static void multiplier_line(float x, float width, float y, const char *label, float value, const char *note)
{
	struct ae_flow_screen const *flow = &end.flow;
	char text[48];
	size_t length;

	snprintf(text, sizeof(text), "\xC3\x97%.2f", (double)value);
	length = strlen(text);
	while (length > 3 && text[length - 1] == '0')
		text[--length] = 0;
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 17.0f, x, y, AE_ALIGN_LEFT, AE_COLOR_TEXT, label);
	if (note)
		ae_pve_menus_ui_text(flow, AE_FONT_BODY, 14.0f, x + 190.0f * ae_flow_u(flow, 1.0f), y, AE_ALIGN_LEFT, AE_COLOR_MUTED, note);
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 22.0f, x + width, y, AE_ALIGN_RIGHT, AE_COLOR_TEXT, text);
}

static void card_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &end.flow;
	struct ae_pve_card const *card = &end.card;
	float u, left, right, top, side_x, side_w, y, row_h;
	char text[160], sub[96], number[32], time[24], par[24];
	int action;

	(void)instance;
	ae_flow_full_density(flow);
	shown_read();
	/* (the first frame drawn: from now a press counts) */
	ae_pve_menus_press_step(&end.press, 0);
	end.last_mission = staged.on ? staged.last : ae_pve_card_level_index() == AE_CAMPAIGN_LEVELS - 1;
	u = ae_flow_u(flow, 1.0f);
	ae_draw_rect(flow->frame.rect.x - 2000.0f, -2000.0f, flow->frame.rect.width + 4000.0f, flow->frame.rect.height + 4000.0f, 0.0f, 0x05080BF2u);
	if (end.count <= 0)
		return;
	text[0] = 0;
	if (end.count > 1)
		snprintf(text, sizeof(text), "PLAYER %d OF %d", end.index + 1, end.count);
	ae_flow_title(flow, "MISSION COMPLETE", end.count > 1 ? text : NULL);
	left = ae_flow_left_x(flow);
	right = ae_flow_right_x(flow);
	top = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y);
	{
		int skulls = ae_pve_menus_ui_skulls_on_count();

		ae_pve_menus_card_sub(card->level, ae_campaign_difficulty_name(card->difficulty), skulls, sub, sizeof(sub));
	}
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 26.0f, left, top + 8.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, sub);
	/* (the player strip) */
	{
		float chip_w = 74.0f * u, chip_h = 26.0f * u;
		char slot[16];

		/* (the slot's chip, then the name) */
		snprintf(slot, sizeof(slot), "P%d", card->player_number > 0 ? (int)card->player_number : end.index + 1);
		ae_draw_rect(left, top + 46.0f * u - chip_h * 0.5f, chip_w * 0.5f, chip_h, chip_h * 0.5f, AE_COLOR_ACCENT);
		ae_pve_menus_ui_text(flow, AE_FONT_ROW, 14.0f, left + chip_w * 0.25f, top + 46.0f * u, AE_ALIGN_CENTER, AE_COLOR_SELECTION_TEXT, slot);
		if (card->player[0])
			ae_pve_menus_ui_text(flow, AE_FONT_ROW, 20.0f, left + chip_w * 0.5f + 14.0f * u, top + 46.0f * u, AE_ALIGN_LEFT, AE_COLOR_TEXT,
				card->player);
	}
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 15.0f, right, top + 46.0f * u, AE_ALIGN_RIGHT, AE_COLOR_MUTED,
		card->player_count > 1 ? (card->competitive ? "EACH PLAYER'S OWN SCORE" : "TEAM SCORE") : "SOLO");
	/* (how you played: the left half) */
	side_w = (right - left) * 0.46f;
	y = top + 78.0f * u;
	row_h = 78.0f * u;
	ae_pve_menus_time_text(card->seconds, time, sizeof(time));
	ae_pve_menus_time_text(card->par_seconds, par, sizeof(par));
	snprintf(sub, sizeof(sub), "Par %s", card->par_seconds > 0 ? par : "-");
	stat_row(left, side_w, y, row_h, "TIME", time, sub, card->record_time);
	y += row_h + 8.0f * u;
	snprintf(number, sizeof(number), "%ld", card->kills);
	snprintf(sub, sizeof(sub), "%ld headshot%s \xE2\x80\xA2 %ld melee kill%s", card->headshots, card->headshots == 1 ? "" : "s", card->melee,
		card->melee == 1 ? "" : "s");
	stat_row(left, side_w, y, row_h, "KILLS", number, sub, 0);
	y += row_h + 8.0f * u;
	snprintf(number, sizeof(number), "%ld", card->deaths);
	snprintf(sub, sizeof(sub), "%ld betrayal%s \xE2\x80\xA2 %ld %s killed", card->betrayals, card->betrayals == 1 ? "" : "s", card->allies,
		card->allies == 1 ? "ally" : "allies");
	stat_row(left, side_w, y, row_h, "DEATHS", number, sub, 0);
	y += row_h + 24.0f * u;
	if (card->medals > 0)
		medals_draw(left, side_w, y + 8.0f * u);
	/* (how the score was made: the right half) */
	side_x = left + side_w + 48.0f * u;
	side_w = right - side_x;
	/* (a co-op game's team table sits inside the panel, under the score: the panel grows by the table's rows) */
	ae_draw_rect(side_x, top + 78.0f * u, side_w,
		(440.0f + (card->player_count > 1 ? 24.0f + 34.0f * (float)card->player_count : 0.0f)) * u, 0.0f, AE_COLOR_PANEL);
	ae_draw_rect(side_x, top + 78.0f * u, side_w, 4.0f * u, 0.0f, AE_COLOR_ACCENT);
	{
		float inner_x = side_x + 28.0f * u, inner_w = side_w - 56.0f * u, line_y = top + 124.0f * u;

		ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, inner_x, line_y, AE_ALIGN_LEFT, AE_COLOR_MUTED, "SCORE");
		line_y += 40.0f * u;
		ae_pve_menus_number_text(card->points, number, sizeof(number));
		ae_pve_menus_ui_text(flow, AE_FONT_BODY, 17.0f, inner_x, line_y, AE_ALIGN_LEFT, AE_COLOR_TEXT, "Points");
		ae_pve_menus_ui_text(flow, AE_FONT_ROW, 22.0f, inner_x + inner_w, line_y, AE_ALIGN_RIGHT, AE_COLOR_TEXT, number);
		line_y += 38.0f * u;
		multiplier_line(inner_x, inner_w, line_y, ae_campaign_difficulty_name(card->difficulty), card->difficulty_multiplier, NULL);
		line_y += 38.0f * u;
		multiplier_line(inner_x, inner_w, line_y, "Skulls", card->skulls_multiplier, NULL);
		line_y += 38.0f * u;
		multiplier_line(inner_x, inner_w, line_y, "Time", card->time_multiplier,
			card->time_multiplier > 1.0f ? "(under par)" : card->time_multiplier < 1.0f ? "(over par)" : NULL);
		line_y += 30.0f * u;
		ae_draw_rect(inner_x, line_y, inner_w, u, 0.0f, AE_COLOR_RULE);
		line_y += 44.0f * u;
		ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, inner_x, line_y, AE_ALIGN_LEFT, AE_COLOR_MUTED,
			card->player_count > 1 && !card->competitive ? "TEAM SCORE" : "FINAL SCORE");
		ae_pve_menus_number_text(card->score, number, sizeof(number));
		ae_pve_menus_ui_text(flow, AE_FONT_ROW, 54.0f, inner_x + inner_w, line_y + 4.0f * u, AE_ALIGN_RIGHT, AE_COLOR_ACCENT, number);
		line_y += 56.0f * u;
		if (card->record_score)
		{
			ae_draw_rect(inner_x, line_y - 18.0f * u, inner_w, 36.0f * u, 0.0f, 0x5FC8EC29u);
			ae_pve_menus_ui_text(flow, AE_FONT_ROW, 16.0f, inner_x + 14.0f * u, line_y, AE_ALIGN_LEFT, AE_COLOR_ACCENT, "NEW BEST SCORE");
		}
	}
	/* (the players of a co-op game: each one's share and kills) */
	if (card->player_count > 1)
	{
		int each;
		float ty = top + 550.0f * u;

		ae_draw_rect(side_x + 28.0f * u, top + 518.0f * u, side_w - 56.0f * u, u, 0.0f, AE_COLOR_RULE);

		for (each = 0; each < card->player_count; each++)
		{
			char score[24];

			ae_pve_menus_number_text(card->players[each].score, score, sizeof(score));
			snprintf(text, sizeof(text), "%s", card->players[each].name);
			ae_pve_menus_ui_text(flow, AE_FONT_BODY, 18.0f, side_x + 28.0f * u, ty, AE_ALIGN_LEFT, AE_COLOR_TEXT, text);
			snprintf(text, sizeof(text), "%s \xE2\x80\xA2 %ld KILLS", score, card->players[each].kills);
			ae_pve_menus_ui_text(flow, AE_FONT_ROW, 20.0f, side_x + side_w - 28.0f * u, ty, AE_ALIGN_RIGHT, AE_COLOR_TITLE, text);
			ty += 34.0f * u;
		}
	}
	flow->prompt_count = 0;
	action = ae_pve_menus_card_a(end.index, end.count, end.last_mission);
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, ae_pve_menus_card_a_word(action), AE_ACTION_ACCEPT);
	/* (the last card: B dismisses it and opens the CAMPAIGN screen) */
	if (end.index >= end.count - 1)
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, "Campaign", AE_ACTION_BACK);
	ae_flow_draw_prompts(flow);
}

static int card_handle(struct ae_screen *instance, struct ae_event const *event)
{
	(void)instance;
	/* (B on the last card: the card is dismissed as A does, then the CAMPAIGN screen opens) */
	if (event->action == AE_ACTION_BACK && end.press.primed && end.index >= end.count - 1)
	{
		ae_sound_request(AE_SOUND_BACK, 0);
		if (staged.on)
		{
			staged.on = 0;
			ae_ui_remove(&end);
		}
		else
			ae_pve_card_advance();
		platform_log("ae end card: last card left for the campaign screen");
		ae_screen_campaign_open();
		return 1;
	}
	/* (a press before the card was drawn does nothing: A goes on) */
	if (event->action == AE_ACTION_ACCEPT && end.press.primed)
	{
		ae_sound_request(AE_SOUND_FORWARD, 0);
		if (staged.on)
		{
			if (++staged.shown >= staged.count)
			{
				staged.on = 0;
				ae_ui_remove(&end);
			}
		}
		else
			ae_pve_card_advance();
		platform_log("ae end card: card %d of %d dismissed", end.index + 1, end.count);
	}
	return 1;
}

static void card_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;

	if (ae_prompts_pointer(end.flow.prompts, end.flow.prompt_count, pointer, HIT_PROMPTS, &event) &&
		(event.action == AE_ACTION_ACCEPT || event.action == AE_ACTION_BACK))
		card_handle(instance, &event);
}

static void card_leave(struct ae_screen *instance)
{
	(void)instance;
	end.on_stack = 0;
}

static struct ae_screen_class const card_class =
{
	.name = "end card", .leave = card_leave, .handle = card_handle, .draw = card_draw, .pointer = card_pointer,
};

void ae_screen_end_card_tick(void)
{
	struct ae_pve_card card;
	int index, count, up = ae_menus_active() && ae_pve_card_shown(&card, &index, &count);

	if (end.on_stack && !ae_ui_holds(&end))
		end.on_stack = 0;
	/* (a made-up summary stays until its last card is dismissed) */
	if (staged.on)
	{
		if (!end.on_stack)
			staged.on = 0;
		return;
	}
	if (!up)
	{
		if (end.on_stack)
		{
			ae_ui_remove(&end);
			end.on_stack = 0;
		}
		return;
	}
	if (end.on_stack)
		return;
	/* (over the game only: another AE screen, a dialog or the pause, up first, waits) */
	if (ae_ui_depth() > 0)
		return;
	memset(&end, 0, sizeof(end));
	end.flow.hit_prompts = HIT_PROMPTS;
	end.flow.hit_rows = CARD_HIT;
	if (!ae_ui_push(&card_class, AE_OWNER_ANY, &end))
		return;
	end.on_stack = 1;
	platform_log("ae end card: open (card %d of %d)", index + 1, count);
}

int ae_screen_end_card_open_staged(int players, int last_mission, int best)
{
	static char const *const names[4] = { "Rook", "Vesper", "Marlow", "Quill" };
	int each, medal;

	if (ae_ui_holds(&end))
		ae_ui_remove(&end);
	memset(&staged, 0, sizeof(staged));
	staged.count = players < 1 ? 1 : players > 4 ? 4 : players;
	staged.last = last_mission != 0;
	for (each = 0; each < staged.count; each++)
	{
		struct ae_pve_card *card = &staged.card[each];

		snprintf(card->player, sizeof(card->player), "%s", names[each]);
		snprintf(card->level, sizeof(card->level), "%s", staged.last ? "KEYES" : "THE PILLAR OF AUTUMN");
		card->player_number = (short)(each + 1);
		card->player_total = (short)staged.count;
		card->difficulty = 2;
		card->record_score = best;
		card->record_time = best;
		card->difficulty_multiplier = 1.5f;
		card->skulls_multiplier = 1.0f;
		card->time_multiplier = 1.1f;
		card->seconds = 1512;
		card->par_seconds = 1800;
		card->kills = 142 - 10 * each;
		card->headshots = 31;
		card->melee = 12;
		card->deaths = 3 + each;
		card->betrayals = 1;
		card->allies = 0;
		card->points = 21400 - 1000 * each;
		card->score = 0;
		card->player_count = (short)staged.count;
		for (medal = 0; medal < staged.count; medal++)
		{
			snprintf(card->players[medal].name, sizeof(card->players[medal].name), "%s", names[medal]);
			card->players[medal].score = 35400 - 1500 * medal;
			card->players[medal].kills = 142 - 10 * medal;
		}
		for (medal = 0; medal < staged.count; medal++)
			card->score += 35400 - 1500 * medal;
		for (medal = 0; medal < 6 && medal < ae_pve_card_medal_count(); medal++)
			card->medal_counts[medal * 3] = (short)(medal + 2);
		card->medals = 27;
	}
	staged.on = 1;
	end.on_stack = 0;
	memset(&end, 0, sizeof(end));
	end.flow.hit_prompts = HIT_PROMPTS;
	end.flow.hit_rows = CARD_HIT;
	if (!ae_ui_push(&card_class, AE_OWNER_ANY, &end))
	{
		staged.on = 0;
		return 0;
	}
	end.on_stack = 1;
	platform_log("ae end card: staged (%d card%s%s)", staged.count, staged.count == 1 ? "" : "s", staged.last ? ", last mission" : "");
	return 1;
}
