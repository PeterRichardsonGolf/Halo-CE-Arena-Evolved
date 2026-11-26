/*
AE_SCREEN_SCORING.C

See ae_screen_scoring.h. Rows on the AE flow (ae_pve_menus_ui.h's rows screen) and the mission's panel on the right: the
multipliers the difficulty and the skulls on give, the par time, the best run on this PC for player 1's profile, how a co-op
game is scored, and eight of the medals that can be won.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "halo_skulls.h"
#include "../src/ae_draw.h"
#include "../src/port_config.h"
#include "campaign_score.h"
#include "ae_glue_campaign.h"
#include "ae_glue_pve_menus.h"
#include "ae_pve_menus_rules.h"
#include "ae_pve_menus_ui.h"
#include "ae_pve_score_menus.h"
#include "ae_screen_scoring.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_style.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum { SCORING_HIT = 0x7A60 };
enum { ROW_SCORING, ROW_TEAM, ROW_MEDALS, ROW_REVERT, ROWS };

static struct ae_pvem_rows scoring;
static short mission_level = -1, mission_difficulty = 1;
static char values[ROWS][24], helps[ROWS][220];

static int is_team(void)
{
	const char *value = config_string("network.coop_score_type");

	return !value || strcmp(value, "competitive");
}

static void build(struct ae_pvem_rows *r)
{
	struct ae_flow_screen *flow = &r->flow;
	int on = config_boolean("game.campaign_score"), team = is_team(), medals = config_boolean("game.campaign_medals"),
		revert = config_boolean("game.campaign_score_revert_cost");
	short n = 0;

	snprintf(values[ROW_SCORING], sizeof(values[0]), "%s", on ? "ON" : "OFF");
	snprintf(values[ROW_TEAM], sizeof(values[0]), "%s", team ? "TEAM" : "EACH PLAYER");
	snprintf(values[ROW_MEDALS], sizeof(values[0]), "%s", medals ? "ON" : "OFF");
	snprintf(values[ROW_REVERT], sizeof(values[0]), "%s", revert ? "ON" : "OFF");
	snprintf(helps[ROW_SCORING], sizeof(helps[0]), "%s", on ?
		"Every kill scores, with your time, at the bottom right; medals as you win them; a card with your score at the end of each mission." :
		"The mission plays as the campaign: no score, no card.");
	snprintf(helps[ROW_TEAM], sizeof(helps[0]), "%s%s", on ? "" : "Turn SCORING on first. ", team ? "Everyone's points add up to one score." :
		"Each player keeps a score of their own. The host's setting counts online.");
	snprintf(helps[ROW_MEDALS], sizeof(helps[0]), "%sShow the medals you win at the centre left as you win them. Medals always count with SCORING on.",
		on ? "" : "Turn SCORING on first. ");
	snprintf(helps[ROW_REVERT], sizeof(helps[0]), "%s%s", on ? "" : "Turn SCORING on first. ", revert ?
		"Going back to a checkpoint from the pause menu costs points." : "A revert costs nothing.");
	flow->rows[n++] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "SCORING", values[ROW_SCORING], helps[ROW_SCORING], NULL };
	flow->rows[n++] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "CO-OP SCORING", values[ROW_TEAM], helps[ROW_TEAM],
		ae_pve_menus_scoring_reason(AE_PVEM_SCORING_ROW_TEAM, on) };
	flow->rows[n++] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "MEDAL POPUPS", values[ROW_MEDALS], helps[ROW_MEDALS],
		ae_pve_menus_scoring_reason(AE_PVEM_SCORING_ROW_MEDALS, on) };
	flow->rows[n++] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "CHECKPOINT COSTS POINTS", values[ROW_REVERT], helps[ROW_REVERT],
		ae_pve_menus_scoring_reason(AE_PVEM_SCORING_ROW_REVERT, on) };
	flow->count = n;
}

static void step(struct ae_pvem_rows *r, int row, int direction)
{
	(void)r;
	(void)direction;
	switch (row)
	{
	case ROW_SCORING:
		ae_pve_menus_ui_write("game.campaign_score", config_boolean("game.campaign_score") ? "false" : "true");
		break;
	case ROW_TEAM:
		ae_pve_menus_ui_write("network.coop_score_type", is_team() ? "competitive" : "cooperative");
		break;
	case ROW_MEDALS:
		ae_pve_menus_ui_write("game.campaign_medals", config_boolean("game.campaign_medals") ? "false" : "true");
		break;
	case ROW_REVERT:
		ae_pve_menus_ui_write("game.campaign_score_revert_cost", config_boolean("game.campaign_score_revert_cost") ? "false" : "true");
		break;
	default:
		return;
	}
	ae_sound_request(AE_SOUND_CURSOR, 0);
	platform_log("ae scoring: %s", config_boolean("game.campaign_score") ? "on" : "off");
}

/* a multiplier as "1.5" (the shortest of two decimals) */
static void multiplier_text(float value, char *text, size_t size)
{
	size_t length;

	snprintf(text, size, "%.2f", (double)value);
	length = strlen(text);
	while (length > 1 && text[length - 1] == '0')
		text[--length] = 0;
	if (length > 1 && text[length - 1] == '.')
		text[length - 1] = 0;
}

static void side(struct ae_pvem_rows *r, float x, float width)
{
	struct ae_flow_screen const *flow = &r->flow;
	static const char *const best_medals[8] = { "first_strike", "headshot", "double_kill", "killing_spree", "assassin", "beat_down",
		"close_call", "grenade_stick" };
	float u = ae_flow_u(flow, 1.0f), top = flow->frame.rect.y;
	char title[96], text[128], number[24], skulls[24];
	int on = ae_pve_menus_ui_skulls_on_count(), each;
	struct skull_set set = skull_set_intersection(skulls_set_from_text(config_string("game.skulls_active")), skulls_unlocked_set());

	ae_pve_menus_ui_plate(flow, x, width, 176.0f, 400.0f);
	if (mission_level >= 0)
		ae_campaign_level_name(mission_level, title, sizeof(title));
	else
		snprintf(title, sizeof(title), "THE CAMPAIGN");
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 30.0f, x, top + 218.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, title);
	if (mission_level >= 0)
		snprintf(text, sizeof(text), "MISSION %d OF %d \xE2\x80\xA2 HALO: COMBAT EVOLVED", mission_level + 1, AE_CAMPAIGN_LEVELS);
	else
		snprintf(text, sizeof(text), "HALO: COMBAT EVOLVED");
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, top + 250.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, text);
	ae_draw_rect(x, top + 272.0f * u, width, u, 0.0f, AE_COLOR_RULE);
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, top + 298.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, "MULTIPLIERS");
	multiplier_text(campaign_score_difficulty_multiplier_for(mission_difficulty), number, sizeof(number));
	multiplier_text(ae_pve_score_skulls_multiplier(set), skulls, sizeof(skulls));
	if (on > 0)
		snprintf(text, sizeof(text), "%s \xC3\x97%s \xE2\x80\xA2 %d SKULL%s \xC3\x97%s", ae_campaign_difficulty_name(mission_difficulty), number, on,
			on == 1 ? "" : "S", skulls);
	else
		snprintf(text, sizeof(text), "%s \xC3\x97%s \xE2\x80\xA2 NO SKULLS", ae_campaign_difficulty_name(mission_difficulty), number);
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 20.0f, x + width, top + 298.0f * u, AE_ALIGN_RIGHT, AE_COLOR_TEXT, text);
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, top + 336.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, "PAR TIME");
	{
		long par = mission_level >= 0 ? ae_pve_score_par_seconds(ae_pve_level_short(mission_level), mission_difficulty) : -1;

		ae_pve_menus_time_text(par, text, sizeof(text));
		ae_pve_menus_ui_text(flow, AE_FONT_ROW, 22.0f, x + width, top + 336.0f * u, AE_ALIGN_RIGHT, AE_COLOR_TEXT, text);
	}
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, top + 374.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, "YOUR BEST");
	snprintf(text, sizeof(text), "NONE YET");
	if (mission_level >= 0)
	{
		struct ae_pve_run runs[AE_PVE_PROFILES];
		int count = ae_pve_level_runs(mission_level, runs, AE_PVE_PROFILES);

		for (each = 0; each < count; each++)
		{
			if (runs[each].profile_index == ae_pve_player_profile())
			{
				ae_pve_menus_number_text(runs[each].score, number, sizeof(number));
				ae_pve_menus_time_text(runs[each].seconds, skulls, sizeof(skulls));
				snprintf(text, sizeof(text), "%s \xE2\x80\xA2 %s", number, skulls);
			}
		}
	}
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 22.0f, x + width, top + 374.0f * u, AE_ALIGN_RIGHT, AE_COLOR_TEXT, text);
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, top + 412.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, "IN CO-OP");
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 22.0f, x + width, top + 412.0f * u, AE_ALIGN_RIGHT, AE_COLOR_TEXT, is_team() ? "ONE TEAM SCORE" : "EACH PLAYER'S OWN");
	ae_draw_rect(x, top + 440.0f * u, width, u, 0.0f, AE_COLOR_RULE);
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, top + 466.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, "MEDALS YOU CAN WIN");
	{
		/* (one tight row of eight, a fixed 12 u gap, the tile 48 u or what the width leaves) */
		float gap = 12.0f * u, tile = (width - 7.0f * gap) / 8.0f;

		if (tile > 48.0f * u)
			tile = 48.0f * u;
		for (each = 0; each < 8; each++)
		{
			char name[64];
			int image;

			snprintf(name, sizeof(name), "medal_%s", best_medals[each]);
			image = ae_pve_menus_ui_ce_picture(name);
			if (image)
				ae_draw_image(image, x + (float)each * (tile + gap), top + 490.0f * u, tile, tile,
					config_boolean("game.campaign_score") ? 0xFFFFFFFFu : 0xFFFFFF59u);
		}
	}
}

static int open_over(short level, short difficulty)
{
	if (ae_ui_holds(&scoring))
		ae_pve_menus_rows_close(&scoring);
	mission_level = level;
	mission_difficulty = difficulty < 0 || difficulty > 3 ? 1 : difficulty;
	memset(&scoring, 0, sizeof(scoring));
	scoring.name = "scoring";
	scoring.title = "SCORING";
	{
		char name[48];

		ae_pve_menus_ui_profile(name, sizeof(name));
		snprintf(scoring.crumb, sizeof(scoring.crumb), "CAMPAIGN \xC2\xB7 %s", name);
	}
	scoring.build = build;
	scoring.step = step;
	scoring.side = side;
	scoring.hit_base = SCORING_HIT;
	return ae_pve_menus_rows_open(&scoring);
}

int ae_screen_scoring_open(short level, short difficulty)
{
	return open_over(level, difficulty);
}

int ae_screen_scoring_open_staged(int scoring_on)
{
	ae_pve_menus_ui_write("game.campaign_score", scoring_on ? "true" : "false");
	return open_over(0, 2);
}
