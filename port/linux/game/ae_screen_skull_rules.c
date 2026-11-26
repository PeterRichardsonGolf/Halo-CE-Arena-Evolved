/*
AE_SCREEN_SKULL_RULES.C

See ae_screen_skull_rules.h. Rows on the AE flow (ae_pve_menus_ui.h's rows screen): HOW SKULLS ARE FOUND, LOSE ON DEATH, SKULLS PER
LEVEL, DROP CHANCE, the RANDOM ROLL group (FEWEST SKULLS, MOST SKULLS) and RESET SKULL PROGRESS. The right-hand panel says how many
of the 40 this PC has found, what is on now, and the skulls still to find.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "halo_skulls.h"
#include "../src/ae_draw.h"
#include "../src/port_config.h"
#include "ae_pve_menus_rules.h"
#include "ae_pve_menus_ui.h"
#include "ae_screen_skull_rules.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_style.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);
void tag_pack_request(char const *name);

enum { RULES_HIT = 0x7A40 };
enum { ROW_FOUND, ROW_LOSE, ROW_COUNT, ROW_CHANCE, ROW_HEAD, ROW_FEWEST, ROW_MOST, ROW_RESET, ROWS };

static struct ae_pvem_rows rules;
static char found_text[ROWS][40];
static char help_text[ROWS][200];
static char reset_value[32];

/* the skulls this PC has found: the ones kept, and Grunt Birthday Party that is had from the start */
static struct skull_set found_set(void)
{
	return skull_set_union(skulls_set_from_text(config_string("game.skulls_unlocked")), skull_set_of(_skull_grunt_birthday_party));
}

static int count_of(struct skull_set set)
{
	int skull, count = 0;

	for (skull = 0; skull < NUMBER_OF_SKULLS; skull++)
		count += skull_set_has(&set, skull) ? 1 : 0;
	return count;
}

/* the skulls this PC has found, not the one it has from the start */
static int found_count(void)
{
	struct skull_set set = found_set();

	return count_of(set) - (skull_set_has(&set, _skull_grunt_birthday_party) ? 1 : 0);
}

/* how the keys say the skulls are had; a game with no answer yet has every skull */
static int found_now(void)
{
	if (skulls_unlock_mode() == _skulls_unlock_unset)
		return AE_PVEM_FOUND_ALL;
	return ae_pve_menus_found_from_keys(config_boolean("game.skulls_all_unlocked"), config_boolean("game.skulls_instant_unlock"));
}

static void write_number(const char *key, long value)
{
	char text[16];

	snprintf(text, sizeof(text), "%ld", value);
	ae_pve_menus_ui_write(key, text);
}

static void build(struct ae_pvem_rows *r)
{
	struct ae_flow_screen *flow = &r->flow;
	int found = found_now(), all = found == AE_PVEM_FOUND_ALL, lose = config_boolean("game.skulls_lose_on_death"), nothing;
	long count = config_integer("game.skulls_drop_count"), chance = config_integer("game.skulls_drop_chance");
	short n = 0;
	int found_total = found_count();

	nothing = all || found_count() < 1;
	snprintf(found_text[ROW_FOUND], sizeof(found_text[0]), "%s", ae_pve_menus_found_name(found));
	snprintf(found_text[ROW_LOSE], sizeof(found_text[0]), "%s", lose ? "ON" : "OFF");
	ae_pve_menus_count_text(count, found_text[ROW_COUNT], sizeof(found_text[0]));
	snprintf(found_text[ROW_CHANCE], sizeof(found_text[0]), "%s", ae_pve_menus_chance_name(ae_pve_menus_chance_index(chance)));
	snprintf(found_text[ROW_FEWEST], sizeof(found_text[0]), "%ld", ae_pve_menus_random_limit(config_integer("game.skulls_random_min")));
	snprintf(found_text[ROW_MOST], sizeof(found_text[0]), "%ld", ae_pve_menus_random_limit(config_integer("game.skulls_random_max")));
	snprintf(reset_value, sizeof(reset_value), "%d FOUND", found_total);
	snprintf(help_text[ROW_FOUND], sizeof(help_text[0]), "%s {A} or left / right: the next way.", ae_pve_menus_found_help(found));
	snprintf(help_text[ROW_LOSE], sizeof(help_text[0]), "%s", found == AE_PVEM_FOUND_LEVELS ? ae_pve_menus_lose_help(lose) :
		"Only when skulls are found in levels: dying then loses the skulls you picked up and have not kept yet.");
	snprintf(help_text[ROW_COUNT], sizeof(help_text[0]), "How many of the skulls you have not found a level's enemies drop. By difficulty: 1 on EASY up to 4 on LEGENDARY.");
	snprintf(help_text[ROW_CHANCE], sizeof(help_text[0]), "How likely a kill drops a skull. The level's first skull drops at the first kill.");
	snprintf(help_text[ROW_FEWEST], sizeof(help_text[0]), "Y on SKULLS turns on a number of skulls between FEWEST and MOST, at random.");
	snprintf(help_text[ROW_MOST], sizeof(help_text[0]), "Y on SKULLS turns on a number of skulls between FEWEST and MOST, at random.");
	snprintf(help_text[ROW_RESET], sizeof(help_text[0]), "Forgets every skull this PC has found, for every profile, and turns the skulls off. {A} asks first.");
	flow->rows[n++] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "HOW SKULLS ARE FOUND", found_text[ROW_FOUND], help_text[ROW_FOUND], NULL };
	flow->rows[n++] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "LOSE ON DEATH", found_text[ROW_LOSE], help_text[ROW_LOSE],
		found == AE_PVEM_FOUND_LEVELS ? NULL : "NEEDS FOUND IN LEVELS" };
	flow->rows[n++] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "SKULLS PER LEVEL", found_text[ROW_COUNT], help_text[ROW_COUNT],
		all ? "ALL UNLOCKED" : NULL };
	flow->rows[n++] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "DROP CHANCE", found_text[ROW_CHANCE], help_text[ROW_CHANCE],
		all ? "ALL UNLOCKED" : NULL };
	flow->rows[n++] = (struct ae_flow_row){ AE_FLOW_ROW_HEADER, -1, "RANDOM ROLL", NULL, NULL, NULL };
	flow->rows[n++] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "FEWEST SKULLS", found_text[ROW_FEWEST], help_text[ROW_FEWEST], NULL };
	flow->rows[n++] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "MOST SKULLS", found_text[ROW_MOST], help_text[ROW_MOST], NULL };
	flow->rows[n] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "RESET SKULL PROGRESS", reset_value, help_text[ROW_RESET],
		nothing ? "NOTHING FOUND YET" : NULL };
	flow->rows[n].danger = 1;
	n++;
	flow->count = n;
}

/* ---------- the forget dialog */

static void forget_picked(short choice, void *context)
{
	(void)context;
	if (choice != 1)
		return;
	/* (Content Evolved's reset: what was found, what was on, how they are had and the death rule; the questions come again) */
	ae_pve_menus_ui_write("game.skulls_unlocked", "");
	ae_pve_menus_ui_write("game.skulls_active", "");
	ae_pve_menus_ui_write("game.skulls", "false");
	ae_pve_menus_ui_write("game.skulls_all_unlocked", "false");
	ae_pve_menus_ui_write("game.skulls_unlock_mode", "");
	ae_pve_menus_ui_write("game.skulls_lose_on_death", "true");
	platform_log("ae skull rules: skull progress forgotten");
	ae_sound_request(AE_SOUND_BACK, 0);
	/* (back to SKULLS, which asks how skulls are had again) */
	ae_pve_menus_rows_close(&rules);
}

static void forget_open(struct ae_pvem_rows *r)
{
	struct ae_dialog_spec spec;
	static char body[240];

	snprintf(body, sizeof(body), "This PC's %d found skulls are forgotten, for every profile, and must be found again. Skulls turned on "
		"now are turned off. This cannot be undone.", found_count());
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = "FORGET EVERY SKULL FOUND?";
	spec.body = body;
	spec.choices[0] = "CANCEL";
	spec.choices[1] = "FORGET THEM";
	spec.choice_count = 2;
	spec.safe_choice = 0;
	spec.cancel_choice = 0;
	spec.has_cancel = 1;
	spec.destructive_choices = 1u << 1;
	spec.density = r->flow.density;
	spec.bounds = r->flow.frame.rect;
	spec.picked = forget_picked;
	spec.context = r;
	platform_log("ae skull rules: forget dialog %s", ae_dialog_open(&spec, AE_OWNER_ANY) ? "opened" : "refused");
}

/* ---------- changing a row */

static void step(struct ae_pvem_rows *r, int row, int direction)
{
	int found = found_now();

	(void)r;
	switch (row)
	{
	case ROW_FOUND:
	{
		const char *mode, *all, *instant;

		found = (found + direction + AE_PVEM_FOUND_COUNT) % AE_PVEM_FOUND_COUNT;
		ae_pve_menus_found_to_keys(found, &mode, &all, &instant);
		ae_pve_menus_ui_write("game.skulls_unlock_mode", mode);
		ae_pve_menus_ui_write("game.skulls_all_unlocked", all);
		ae_pve_menus_ui_write("game.skulls_instant_unlock", instant);
		if (skulls_pack_wanted())
			tag_pack_request(SKULLS_PACK);
		break;
	}
	case ROW_LOSE:
		ae_pve_menus_ui_write("game.skulls_lose_on_death", config_boolean("game.skulls_lose_on_death") ? "false" : "true");
		break;
	case ROW_COUNT:
		write_number("game.skulls_drop_count", (config_integer("game.skulls_drop_count") + direction + 9) % 9);
		break;
	case ROW_CHANCE:
		write_number("game.skulls_drop_chance", ae_pve_menus_chance_percent((ae_pve_menus_chance_index(config_integer("game.skulls_drop_chance")) +
			direction + AE_PVEM_CHANCES) % AE_PVEM_CHANCES));
		break;
	case ROW_FEWEST:
	case ROW_MOST:
	{
		long fewest = ae_pve_menus_random_limit(config_integer("game.skulls_random_min")),
			most = ae_pve_menus_random_limit(config_integer("game.skulls_random_max"));

		if (row == ROW_FEWEST)
			fewest = (fewest + direction + 41) % 41;
		else
			most = (most + direction + 41) % 41;
		ae_pve_menus_random_clamp(&fewest, &most, row == ROW_FEWEST ? 0 : 1);
		write_number("game.skulls_random_min", fewest);
		write_number("game.skulls_random_max", most);
		break;
	}
	case ROW_RESET:
		rules.dialog_pending = 1;
		return;
	default:
		return;
	}
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

static const char *a_word(struct ae_pvem_rows *r, int row)
{
	(void)r;
	return row == ROW_RESET ? "Forget" : "Change";
}

/* ---------- the right-hand panel */

static void side(struct ae_pvem_rows *r, float x, float width)
{
	struct ae_flow_screen const *flow = &r->flow;
	struct skull_set found = found_set();
	float u = ae_flow_u(flow, 1.0f), y = 190.0f;
	int all = found_now() == AE_PVEM_FOUND_ALL, skull, on = ae_pve_menus_ui_skulls_on_count();
	char text[96];

	ae_pve_menus_ui_plate(flow, x, width, y - 14.0f, 690.0f);
	if (all)
		snprintf(text, sizeof(text), "ALL %d UNLOCKED", NUMBER_OF_SKULLS);
	else
		snprintf(text, sizeof(text), "%d OF %d FOUND", found_count(), NUMBER_OF_SKULLS);
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 34.0f, x, flow->frame.rect.y + (y + 24.0f) * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, text);
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 15.0f, x, flow->frame.rect.y + (y + 62.0f) * u, AE_ALIGN_LEFT, AE_COLOR_MUTED,
		"THIS PC'S SKULLS \xE2\x80\xA2 EVERY CAMPAIGN AND PROFILE");
	ae_draw_rect(x, flow->frame.rect.y + (y + 86.0f) * u, width, u, 0.0f, AE_COLOR_RULE);
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, flow->frame.rect.y + (y + 112.0f) * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, "ON NOW");
	/* (none, one or two named, else the count) */
	if (on == 0)
		snprintf(text, sizeof(text), "NONE");
	else if (on <= 2)
	{
		struct skull_set now = skulls_set_from_text(config_string("game.skulls_active"));
		int each;

		text[0] = 0;
		for (each = 0; each < NUMBER_OF_SKULLS; each++)
			if (skull_set_has(&now, each) && strlen(text) + strlen(skull_descriptions[each].label) + 3 < sizeof(text))
				snprintf(text + strlen(text), sizeof(text) - strlen(text), "%s%s", text[0] ? ", " : "", skull_descriptions[each].label);
	}
	else
		snprintf(text, sizeof(text), "%d SKULLS", on);
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 22.0f, x + width, flow->frame.rect.y + (y + 112.0f) * u, AE_ALIGN_RIGHT, AE_COLOR_TITLE, text);
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, flow->frame.rect.y + (y + 150.0f) * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, "IN CO-OP");
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 18.0f, x + width, flow->frame.rect.y + (y + 150.0f) * u, AE_ALIGN_RIGHT, AE_COLOR_TEXT,
		"the host's rules and skulls");
	if (all)
		return;
	ae_draw_rect(x, flow->frame.rect.y + (y + 174.0f) * u, width, u, 0.0f, AE_COLOR_RULE);
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, flow->frame.rect.y + (y + 200.0f) * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, "STILL TO FIND");
	{
		float tile = 56.0f * u, gap = 12.0f * u, left = x;
		short pictures[28];
		int columns = (int)((width + gap) / (tile + gap)), index, picture_count = 0, use_mcc;

		if (columns < 1)
			columns = 1;
		for (skull = 0; skull < NUMBER_OF_SKULLS && picture_count < 28; skull++)
		{
			if (!skull_set_has(&found, skull))
				pictures[picture_count++] = (short)skull;
		}
		use_mcc = ae_pve_menus_ui_skulls_use_mcc(pictures, picture_count);
		for (index = 0; index < picture_count; index++)
			ae_pve_menus_ui_skull_picture(pictures[index], use_mcc, left + (float)(index % columns) * (tile + gap),
				flow->frame.rect.y + (y + 222.0f) * u + (float)(index / columns) * (tile + gap), tile, tile, 0x84939F4Du);
	}
}

static int open_over(int reset)
{
	if (ae_ui_holds(&rules))
		ae_pve_menus_rows_close(&rules);
	memset(&rules, 0, sizeof(rules));
	rules.name = "skull rules";
	rules.title = "SKULL RULES";
	{
		char name[48];

		ae_pve_menus_ui_profile(name, sizeof(name));
		snprintf(rules.crumb, sizeof(rules.crumb), "SKULLS \xC2\xB7 %s", name);
	}
	rules.build = build;
	rules.step = step;
	rules.a_word = a_word;
	rules.side = side;
	rules.dialog_open = forget_open;
	rules.hit_base = RULES_HIT;
	rules.dialog_pending = reset;
	if (!ae_pve_menus_rows_open(&rules))
		return 0;
	return 1;
}

int ae_screen_skull_rules_open(void)
{
	return open_over(0);
}

int ae_screen_skull_rules_open_staged(int reset)
{
	return open_over(reset);
}
