/*
AE_SCREEN_COOP_OPTIONS.C

See ae_screen_coop_options.h. Rows on the AE flow (ae_pve_menus_ui.h's rows screen) in two groups, PLAYERS and MORE PLAYERS, MORE
TO FIGHT; on the right what the options come to for two and four players, and that they are for games hosted online.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "../src/port_config.h"
#include "ae_pve_menus_rules.h"
#include "ae_pve_menus_ui.h"
#include "ae_screen_coop_options.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_style.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum { COOP_HIT = 0x7A80 };
/* the rows: two headers and the nine options */
enum { ROW_HEAD_PLAYERS, ROW_FRIENDLY, ROW_ALLY, ROW_COLLIDE, ROW_WIPE, ROW_HEAD_MORE, ROW_ENEMIES, ROW_PICKUPS, ROW_VEHICLES, ROW_GROWTH,
	ROW_FIXED, ROWS };
/* the option (ae_pve_menus_coop_option's index) behind each row */
static const int option_of_row[ROWS] = { -1, 0, 1, 2, 3, -1, 4, 5, 6, 7, 8 };

static struct ae_pvem_rows coop;
static char values[ROWS][24], helps[ROWS][260];

static const char *get(void *context, const char *key)
{
	return ae_pve_menus_ui_read(context, key);
}

int ae_screen_coop_options_standard(void)
{
	return ae_pve_menus_coop_is_standard(get, NULL);
}

static int bool_now(int option)
{
	return !strcmp(get(NULL, ae_pve_menus_coop_option(option)->key), "true");
}

/* the index among a kind's values of an integer setting: the one it is, else the nearest */
static int number_index(int kind, int option)
{
	long value = config_integer(ae_pve_menus_coop_option(option)->key), best_gap = 1000000;
	int each, best = 0;

	for (each = 0; each < ae_pve_menus_coop_value_count(kind); each++)
	{
		long number = 0, gap;

		sscanf(ae_pve_menus_coop_value_word(kind, each), "%ld", &number);
		gap = value > number ? value - number : number - value;
		if (gap < best_gap)
			best_gap = gap, best = each;
	}
	return best;
}

static int word_index(int kind, int option)
{
	int index = ae_pve_menus_coop_value_index(kind, get(NULL, ae_pve_menus_coop_option(option)->key));

	return index < 0 ? 0 : index;
}

/* whether any of the three extras uses the per-player growth, or the fixed size */
static int uses(const char *mode)
{
	return !strcmp(get(NULL, "network.coop_enemies_mode"), mode) || !strcmp(get(NULL, "network.coop_pickups"), mode) ||
		!strcmp(get(NULL, "network.coop_vehicles"), mode);
}

static void build(struct ae_pvem_rows *r)
{
	struct ae_flow_screen *flow = &r->flow;
	int each;
	const char *mode = get(NULL, "network.coop_enemies_mode");

	snprintf(values[ROW_FRIENDLY], sizeof(values[0]), "%s", ae_pve_menus_coop_value_label(AE_PVEM_COOP_FRIENDLY, word_index(AE_PVEM_COOP_FRIENDLY, 0)));
	snprintf(values[ROW_ALLY], sizeof(values[0]), "%s", bool_now(1) ? "ON" : "OFF");
	snprintf(values[ROW_COLLIDE], sizeof(values[0]), "%s", bool_now(2) ? "ON" : "OFF");
	snprintf(values[ROW_WIPE], sizeof(values[0]), "%s", ae_pve_menus_coop_value_label(AE_PVEM_COOP_WIPE, word_index(AE_PVEM_COOP_WIPE, 3)));
	snprintf(values[ROW_ENEMIES], sizeof(values[0]), "%s", ae_pve_menus_coop_value_label(AE_PVEM_COOP_MODE, word_index(AE_PVEM_COOP_MODE, 4)));
	snprintf(values[ROW_PICKUPS], sizeof(values[0]), "%s", ae_pve_menus_coop_value_label(AE_PVEM_COOP_MODE, word_index(AE_PVEM_COOP_MODE, 5)));
	snprintf(values[ROW_VEHICLES], sizeof(values[0]), "%s", ae_pve_menus_coop_value_label(AE_PVEM_COOP_MODE, word_index(AE_PVEM_COOP_MODE, 6)));
	snprintf(values[ROW_GROWTH], sizeof(values[0]), "%s", ae_pve_menus_coop_value_label(AE_PVEM_COOP_GROWTH, number_index(AE_PVEM_COOP_GROWTH, 7)));
	snprintf(values[ROW_FIXED], sizeof(values[0]), "%s", ae_pve_menus_coop_value_label(AE_PVEM_COOP_FIXED, number_index(AE_PVEM_COOP_FIXED, 8)));
	snprintf(helps[ROW_FRIENDLY], sizeof(helps[0]), "Whether the players of a hosted game hurt each other: OFF, ON, only shields, or only explosives.");
	snprintf(helps[ROW_ALLY], sizeof(helps[0]), "%s", bool_now(1) ? "The players' weapons, grenades and vehicles hurt their AI allies, as in the campaign." :
		"The players do their AI allies no harm.");
	snprintf(helps[ROW_COLLIDE], sizeof(helps[0]), "%s", bool_now(2) ? "The players bump into each other." : "The players walk through each other.");
	snprintf(helps[ROW_WIPE], sizeof(helps[0]), "%s", ae_pve_menus_coop_wipe_help(get(NULL, "network.coop_team_wipe")));
	snprintf(helps[ROW_ENEMIES], sizeof(helps[0]), "%s", ae_pve_menus_coop_mode_help(mode));
	snprintf(helps[ROW_PICKUPS], sizeof(helps[0]), "Extra weapons, grenades, health and power-ups, set by the same growth (PER PLAYER) or size (FIXED) as the enemies.");
	snprintf(helps[ROW_VEHICLES], sizeof(helps[0]), "Extra vehicles, set by the same growth (PER PLAYER) or size (FIXED) as the enemies.");
	snprintf(helps[ROW_GROWTH], sizeof(helps[0]), "How much of itself each enemy squad adds for each player past the first. 50% with 3 players: twice as many.");
	snprintf(helps[ROW_FIXED], sizeof(helps[0]), "How many times as large each enemy squad is, for any number of players.");
	for (each = 0; each < ROWS; each++)
		flow->rows[each] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, NULL, values[each], helps[each], NULL };
	flow->rows[ROW_HEAD_PLAYERS] = (struct ae_flow_row){ AE_FLOW_ROW_HEADER, -1, "PLAYERS", NULL, NULL, NULL };
	flow->rows[ROW_HEAD_MORE] = (struct ae_flow_row){ AE_FLOW_ROW_HEADER, -1, "MORE PLAYERS, MORE TO FIGHT", NULL, NULL, NULL };
	flow->rows[ROW_FRIENDLY].label = "FRIENDLY FIRE";
	flow->rows[ROW_ALLY].label = "ALLY FRIENDLY FIRE";
	flow->rows[ROW_COLLIDE].label = "PLAYER COLLISIONS";
	flow->rows[ROW_WIPE].label = "TEAM WIPE";
	flow->rows[ROW_ENEMIES].label = "EXTRA ENEMIES";
	flow->rows[ROW_PICKUPS].label = "EXTRA PICKUPS";
	flow->rows[ROW_VEHICLES].label = "EXTRA VEHICLES";
	flow->rows[ROW_GROWTH].label = "PER PLAYER GROWTH";
	flow->rows[ROW_FIXED].label = "FIXED SIZE";
	if (!uses("per_player"))
		flow->rows[ROW_GROWTH].reason = "NEEDS PER PLAYER";
	if (!uses("multiplier"))
		flow->rows[ROW_FIXED].reason = "NEEDS FIXED";
	flow->count = ROWS;
}

static void write_number(const char *key, long value)
{
	char text[16];

	snprintf(text, sizeof(text), "%ld", value);
	ae_pve_menus_ui_write(key, text);
}

/* a word row stepped through its kind's values */
static void word_step(int kind, int option, int direction)
{
	int count = ae_pve_menus_coop_value_count(kind), next = (word_index(kind, option) + direction + count) % count;

	ae_pve_menus_ui_write(ae_pve_menus_coop_option(option)->key, ae_pve_menus_coop_value_word(kind, next));
}

static void number_step(int kind, int option, int direction)
{
	int count = ae_pve_menus_coop_value_count(kind), next = (number_index(kind, option) + direction + count) % count;
	long number = 0;

	sscanf(ae_pve_menus_coop_value_word(kind, next), "%ld", &number);
	write_number(ae_pve_menus_coop_option(option)->key, number);
}

static void step(struct ae_pvem_rows *r, int row, int direction)
{
	(void)r;
	switch (row)
	{
	case ROW_FRIENDLY:
		word_step(AE_PVEM_COOP_FRIENDLY, 0, direction);
		break;
	case ROW_ALLY:
		ae_pve_menus_ui_write(ae_pve_menus_coop_option(1)->key, bool_now(1) ? "false" : "true");
		break;
	case ROW_COLLIDE:
		ae_pve_menus_ui_write(ae_pve_menus_coop_option(2)->key, bool_now(2) ? "false" : "true");
		break;
	case ROW_WIPE:
		word_step(AE_PVEM_COOP_WIPE, 3, direction);
		break;
	case ROW_ENEMIES:
		word_step(AE_PVEM_COOP_MODE, 4, direction);
		break;
	case ROW_PICKUPS:
		word_step(AE_PVEM_COOP_MODE, 5, direction);
		break;
	case ROW_VEHICLES:
		word_step(AE_PVEM_COOP_MODE, 6, direction);
		break;
	case ROW_GROWTH:
		number_step(AE_PVEM_COOP_GROWTH, 7, direction);
		break;
	case ROW_FIXED:
		number_step(AE_PVEM_COOP_FIXED, 8, direction);
		break;
	default:
		return;
	}
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

/* X: every option back to its default, at once */
static void standard(struct ae_pvem_rows *r)
{
	int each;

	(void)r;
	for (each = 0; each < AE_PVEM_COOP_OPTIONS; each++)
		ae_pve_menus_ui_write(ae_pve_menus_coop_option(each)->key, ae_pve_menus_coop_option(each)->standard);
	ae_sound_request(AE_SOUND_FORWARD, 0);
	platform_log("ae coop options: standard");
}

static void side(struct ae_pvem_rows *r, float x, float width)
{
	struct ae_flow_screen const *flow = &r->flow;
	float u = ae_flow_u(flow, 1.0f), top = flow->frame.rect.y;
	char text[96], factor[24];
	int players;
	long growth = 0, fixed = 0;
	const char *mode = get(NULL, "network.coop_enemies_mode");

	sscanf(ae_pve_menus_coop_value_word(AE_PVEM_COOP_GROWTH, number_index(AE_PVEM_COOP_GROWTH, 7)), "%ld", &growth);
	sscanf(ae_pve_menus_coop_value_word(AE_PVEM_COOP_FIXED, number_index(AE_PVEM_COOP_FIXED, 8)), "%ld", &fixed);
	ae_pve_menus_ui_plate(flow, x, width, 176.0f, 560.0f);
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 30.0f, x, top + 218.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, "FOR GAMES YOU HOST");
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 15.0f, x, top + 250.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED,
		"ONLINE CO-OP \xE2\x80\xA2 X IN THE LOBBY HOSTS");
	ae_draw_rect(x, top + 272.0f * u, width, u, 0.0f, AE_COLOR_RULE);
	for (players = 2; players <= 4; players += 2)
	{
		ae_pve_menus_coop_factor_text(ae_pve_menus_coop_factor(mode, growth, fixed, players), factor, sizeof(factor));
		snprintf(text, sizeof(text), "WITH %d PLAYERS", players);
		ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, top + (300.0f + (float)(players - 2) * 18.0f) * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, text);
		snprintf(text, sizeof(text), "%s enemy squads", factor);
		ae_pve_menus_ui_text(flow, AE_FONT_ROW, 20.0f, x + width, top + (300.0f + (float)(players - 2) * 18.0f) * u, AE_ALIGN_RIGHT, AE_COLOR_TEXT, text);
	}
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, top + 372.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, "PICKUPS");
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 20.0f, x + width, top + 372.0f * u, AE_ALIGN_RIGHT, AE_COLOR_TEXT, values[ROW_PICKUPS]);
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, top + 408.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, "VEHICLES");
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 20.0f, x + width, top + 408.0f * u, AE_ALIGN_RIGHT, AE_COLOR_TEXT, values[ROW_VEHICLES]);
	ae_draw_rect(x, top + 436.0f * u, width, u, 0.0f, AE_COLOR_RULE);
	ae_pve_menus_ui_paragraph(flow, 18.0f, 26.0f, x, top + 464.0f * u, width, 4, AE_COLOR_TEXT,
		"Alone and in split screen the mission plays as the campaign has it. The host's options count for everyone who joins.");
}

static int open_over(const char *head)
{
	if (ae_ui_holds(&coop))
		ae_pve_menus_rows_close(&coop);
	memset(&coop, 0, sizeof(coop));
	coop.name = "coop options";
	coop.title = "CO-OP OPTIONS";
	{
		char name[48];

		ae_pve_menus_ui_profile(name, sizeof(name));
		snprintf(coop.crumb, sizeof(coop.crumb), "%s \xC2\xB7 %s", head ? head : "CAMPAIGN", name);
	}
	coop.build = build;
	coop.step = step;
	coop.side = side;
	coop.x_action = standard;
	coop.x_word = "Standard";
	coop.hit_base = COOP_HIT;
	return ae_pve_menus_rows_open(&coop);
}

int ae_screen_coop_options_open(const char *crumb_head)
{
	return open_over(crumb_head);
}

int ae_screen_coop_options_open_staged(int changed)
{
	if (changed)
	{
		ae_pve_menus_ui_write("network.coop_friendly_fire", "shields_only");
		ae_pve_menus_ui_write("network.coop_team_wipe", "respawn");
		ae_pve_menus_ui_write("network.coop_pickups", "per_player");
	}
	return open_over("CAMPAIGN");
}
