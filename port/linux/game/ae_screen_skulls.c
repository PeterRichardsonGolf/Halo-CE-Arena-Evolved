/*
AE_SCREEN_SKULLS.C

See ae_screen_skulls.h. Layout (u at 1080): the title and crumb, SKULL RULES as a full-width sub-screen row, then the 40 skulls
as tiles, 8 across and 5 down in Content Evolved's order, 8 u apart; under them the focused skull's help line and, at its right,
how many are on. A tile is its picture (ae_pve_menus_ui_skull_picture) over the skull's name: off dims the
picture, on has an accent tint and frame with an ON chip, a skull not found yet is a silhouette marked NOT FOUND, and the
focused tile has a white frame with its name on a bar.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "halo_skulls.h"
#include "../src/ae_draw.h"
#include "../src/port_config.h"
#include "ae_input.h"
#include "ae_pve_menus_rules.h"
#include "ae_pve_menus_ui.h"
#include "ae_glue_setup_options.h"
#include "ae_screen_skull_rules.h"
#include "ae_screen_skulls.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_style.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);
void tag_pack_request(char const *name);
unsigned long system_milliseconds(void);

enum
{
	SKULLS_HIT = 0x7A20,
	HIT_PROMPTS = SKULLS_HIT + 1, HIT_RULES, HIT_TILES,
	COLUMNS = 8,
	ROWS = 5,
	ON_FOCUS_RULES = -1
};

/* what the first-time questions are doing: none, the first question over the screen, the second */
enum { ASK_NONE, ASK_HOW, ASK_DEATH };

static struct
{
	struct ae_flow_screen flow;
	char crumb[64], help[400], summary[64], rules_value[64];
	short focus, column;
	int ask, ask_pending, found, mouse_hover;
	struct skull_set offered, on;
	/* (Firefight's STARTING SKULLS, opened from its rules or lobby: the copy's skulls, those its variant carries kept on) */
	int firefight;
	struct skull_set locked;
	/* the roll's line under the grid, kept until the next move */
	char note[48];
	/* the tiles' rectangles as last drawn (layout units) */
	struct ae_rect tile[NUMBER_OF_SKULLS];
	unsigned long roll_state;
} sk;

static struct ae_screen_class const skulls_class;

static int skull_count(struct skull_set const *set)
{
	int skull, count = 0;

	for (skull = 0; skull < NUMBER_OF_SKULLS; skull++)
		count += skull_set_has(set, skull) ? 1 : 0;
	return count;
}

/* the sets as the settings have them now */
static void sets_read(void)
{
	if (sk.firefight)
	{
		unsigned long words[2], locked[2];
		int word;

		ae_setup_edit_firefight_skulls(words, locked);
		sk.offered = skull_set_all();
		sk.on = skull_set_none();
		sk.locked = skull_set_none();
		for (word = 0; word < 2 && word < SKULL_SET_WORDS; word++)
		{
			sk.on.words[word] = words[word] | locked[word];
			sk.locked.words[word] = locked[word];
		}
		sk.found = AE_PVEM_FOUND_ALL;
		return;
	}
	sk.offered = skulls_unlocked_set();
	sk.on = skull_set_intersection(skulls_set_from_text(config_string("game.skulls_active")), sk.offered);
	sk.found = ae_pve_menus_found_from_keys(config_boolean("game.skulls_all_unlocked"), config_boolean("game.skulls_instant_unlock"));
}

/* the turned on skulls written (and game.skulls with them: on while any skull is) */
static void on_write(struct skull_set set)
{
	char text[SKULLS_TEXT_LENGTH];
	int count = skull_count(&set);

	if (sk.firefight)
	{
		unsigned long words[2] = { 0UL, 0UL };
		int word;

		/* (the skulls the variant carries stay on) */
		for (word = 0; word < 2 && word < SKULL_SET_WORDS; word++)
			words[word] = set.words[word] | sk.locked.words[word];
		ae_setup_edit_firefight_skulls_set(words);
		sets_read();
		platform_log("ae skulls: firefight %d on", skull_count(&sk.on));
		return;
	}

	skulls_text_from_set(set, text, sizeof(text));
	ae_pve_menus_ui_write("game.skulls_active", text);
	ae_pve_menus_ui_write("game.skulls", count > 0 ? "true" : "false");
	if (skulls_pack_wanted())
		tag_pack_request(SKULLS_PACK);
	sk.on = set;
	platform_log("ae skulls: %d on", count);
}

static void toggle(int skull)
{
	struct skull_set set = sk.on;

	if (!skull_set_has(&sk.offered, skull))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	if (sk.firefight && skull_set_has(&sk.locked, skull))
	{
		snprintf(sk.note, sizeof(sk.note), "The variant carries this skull: it stays on.");
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	if (skull_set_has(&set, skull))
		set = skull_set_difference(set, skull_set_of(skull));
	else
		skull_set_add(&set, skull);
	on_write(set);
	ae_sound_request(skull_set_has(&set, skull) ? AE_SOUND_FORWARD : AE_SOUND_BACK, 0);
	sk.note[0] = 0;
}

static void all_toggle(void)
{
	if (sk.firefight ? skull_count(&sk.on) > skull_count(&sk.locked) : skull_count(&sk.on) > 0)
		on_write(skull_set_none());
	else
		on_write(sk.offered);
	sk.note[0] = 0;
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

/* Y: a number of the skulls had, between the two limits, picked at random; the rest off */
static void roll(void)
{
	long had = skull_count(&sk.offered), wanted, index, count = 0;
	int pool[NUMBER_OF_SKULLS], skull;
	struct skull_set set = skull_set_none();

	if (sk.firefight)
		return;

	for (skull = 0; skull < NUMBER_OF_SKULLS; skull++)
		if (skull_set_has(&sk.offered, skull))
			pool[count++] = skull;
	sk.roll_state ^= system_milliseconds() * 2654435761UL;
	if (!(sk.roll_state & 0xFFFFFFFFUL))
		sk.roll_state = 1;
#define NEXT() (sk.roll_state ^= (sk.roll_state << 13) & 0xFFFFFFFFUL, sk.roll_state ^= (sk.roll_state & 0xFFFFFFFFUL) >> 17, \
	sk.roll_state ^= (sk.roll_state << 5) & 0xFFFFFFFFUL, sk.roll_state & 0xFFFFFFFFUL)
	wanted = ae_pve_menus_random_pick(config_integer("game.skulls_random_min"), config_integer("game.skulls_random_max"), had, NEXT());
	for (index = 0; index < wanted; index++)
	{
		long other = index + (long)(NEXT() % (unsigned long)(count - index));
		int swap = pool[index];

		pool[index] = pool[other];
		pool[other] = swap;
		skull_set_add(&set, pool[index]);
	}
#undef NEXT
	on_write(set);
	snprintf(sk.note, sizeof(sk.note), "Rolled %ld skull%s.", wanted, wanted == 1 ? "" : "s");
	platform_log("ae skulls: rolled %ld", wanted);
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

/* ---------- the first-time questions */

static void ask_open(int which);

/* the answers kept: how the skulls are had, and whether dying loses them, as Content Evolved writes them */
static void answers_write(int found, int lose)
{
	const char *mode, *all, *instant;

	ae_pve_menus_found_to_keys(found, &mode, &all, &instant);
	ae_pve_menus_ui_write("game.skulls_unlock_mode", mode);
	ae_pve_menus_ui_write("game.skulls_all_unlocked", all);
	ae_pve_menus_ui_write("game.skulls_instant_unlock", instant);
	ae_pve_menus_ui_write("game.skulls_lose_on_death", lose ? "true" : "false");
	if (skulls_pack_wanted())
		tag_pack_request(SKULLS_PACK);
	platform_log("ae skulls: answered: %s%s", ae_pve_menus_found_name(found), found == AE_PVEM_FOUND_LEVELS ? (lose ? ", lose on death" : ", keep on death") : "");
	sk.ask = ASK_NONE;
	sets_read();
}

static void how_picked(short choice, void *context)
{
	(void)context;
	if (choice < 0 || choice >= AE_PVEM_FOUND_COUNT)
	{
		/* (B: nothing was answered, and the grid is not opened) */
		sk.ask = ASK_NONE;
		ae_ui_remove(&sk);
		return;
	}
	if (choice == AE_PVEM_FOUND_LEVELS)
	{
		sk.ask = ASK_DEATH;
		sk.ask_pending = 1;
		return;
	}
	answers_write(choice, 1);
}

static void death_picked(short choice, void *context)
{
	(void)context;
	if (choice < 0)
	{
		/* (B: back to the first question) */
		sk.ask = ASK_HOW;
		sk.ask_pending = 1;
		return;
	}
	answers_write(AE_PVEM_FOUND_LEVELS, choice == 0);
}

static void ask_open(int which)
{
	struct ae_dialog_spec spec;
	int each;

	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.density = sk.flow.density;
	spec.bounds = sk.flow.frame.rect;
	spec.context = &sk;
	spec.has_cancel = 1;
	spec.cancel_choice = -1;
	if (which == ASK_HOW)
	{
		spec.title = "HOW SHOULD THE SKULLS BE UNLOCKED?";
		spec.body = "ALL UNLOCKED: every skull can be turned on right away.\n"
			"FOUND IN LEVELS: enemies carry the skulls. Pick one up to turn it on for that level, and finish the level to keep it.\n"
			"FOUND ON PICKUP: enemies carry the skulls. One you pick up is yours at once.";
		for (each = 0; each < AE_PVEM_FOUND_COUNT; each++)
			spec.choices[each] = ae_pve_menus_found_name(each);
		spec.choice_count = AE_PVEM_FOUND_COUNT;
		spec.picked = how_picked;
	}
	else
	{
		spec.title = "SHOULD DYING LOSE THE SKULLS YOU PICKED UP?";
		spec.body = "LOSE THEM ON DEATH: a skull you picked up is lost if you die or go back to a checkpoint before you finish the level. "
			"It drops again in another level.\nKEEP THEM: a skull you picked up stays with you until you finish the level, however often you die.";
		spec.choices[0] = "LOSE THEM ON DEATH";
		spec.choices[1] = "KEEP THEM";
		spec.choice_count = 2;
		spec.picked = death_picked;
	}
	spec.safe_choice = 0;
	ae_dialog_open(&spec, AE_OWNER_ANY);
}

/* ---------- drawing */

static void tile_rect(int index, float *x, float *y, float *w, float *h, float top, float tile_h)
{
	float gap = ae_flow_u(&sk.flow, 8.0f), left = ae_flow_left_x(&sk.flow), total = ae_flow_right_x(&sk.flow) - left;
	int column = index % COLUMNS, row = index / COLUMNS;

	*w = (total - gap * (COLUMNS - 1)) / COLUMNS;
	*h = tile_h;
	*x = left + (*w + gap) * (float)column;
	*y = top + (tile_h + gap) * (float)row;
}

static void tile_draw(int skull, int use_mcc, float x, float y, float w, float h, int focused)
{
	struct ae_flow_screen const *flow = &sk.flow;
	float u = ae_flow_u(flow, 1.0f), picture = h * (52.0f / 116.0f), name_size = 17.0f, band_h = h * (28.0f / 116.0f);
	int on = skull_set_has(&sk.on, skull), found = skull_set_has(&sk.offered, skull);
	float name_y = y + h * (78.0f / 116.0f);
	unsigned int ink = focused ? AE_COLOR_SELECTION_TEXT : !found ? AE_COLOR_MUTED : on ? AE_COLOR_TITLE : AE_COLOR_TEXT;
	char fitted[64];
	const char *label = skull_descriptions[skull].label;

	ae_draw_rect(x, y, w, h, 0.0f, AE_COLOR_ROW);
	if (on)
	{
		ae_draw_rect(x, y, w, h, 0.0f, 0x5FC8EC29u);
		ae_draw_outline(x, y, w, h, 0.0f, 3.0f * u, AE_COLOR_ACCENT);
	}
	if (focused)
		ae_draw_rect(x, name_y - band_h * 0.5f, w, band_h, 0.0f, AE_COLOR_SELECTION);
	ae_pve_menus_ui_skull_picture(skull, use_mcc, x + (w - picture) * 0.5f, y + h * (10.0f / 116.0f), picture, picture,
		!found ? 0x0F171E73u : on ? 0xFFFFFFFFu : 0xFFFFFF52u);
	ae_fit_text(AE_FONT_ROW, ae_flow_u(flow, name_size), label, w - 16.0f * u, fitted, (int)sizeof(fitted));
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, name_size, x + w * 0.5f, name_y, AE_ALIGN_CENTER, ink, fitted);
	if (!found)
		ae_pve_menus_ui_text(flow, AE_FONT_BODY, 14.0f, x + w * 0.5f, y + h * (100.0f / 116.0f), AE_ALIGN_CENTER, AE_COLOR_WARNING,
			"NOT FOUND");
	if (on)
	{
		float chip_w = 34.0f * u, chip_h = 16.0f * u;

		ae_draw_rect(x + w - chip_w - 8.0f * u, y + 8.0f * u, chip_w, chip_h, chip_h * 0.5f, AE_COLOR_ACCENT);
		ae_pve_menus_ui_text(flow, AE_FONT_ROW, 11.0f, x + w - chip_w * 0.5f - 8.0f * u, y + 8.0f * u + chip_h * 0.5f, AE_ALIGN_CENTER,
			AE_COLOR_SELECTION_TEXT, "ON");
	}
	if (focused)
		ae_draw_outline(x, y, w, h, 0.0f, 3.0f * u, AE_COLOR_SELECTION);
}

static void summary_make(void)
{
	int total = skull_count(&sk.offered), on = skull_count(&sk.on);

	if (sk.found == AE_PVEM_FOUND_ALL)
		snprintf(sk.rules_value, sizeof(sk.rules_value), "ALL UNLOCKED \xE2\x80\xA2 RANDOM %ld TO %ld", config_integer("game.skulls_random_min"),
			config_integer("game.skulls_random_max"));
	else
		snprintf(sk.rules_value, sizeof(sk.rules_value), "%s \xE2\x80\xA2 %d OF %d FOUND", ae_pve_menus_found_name(sk.found), total, NUMBER_OF_SKULLS);
	snprintf(sk.summary, sizeof(sk.summary), "%d SKULL%s ON", on, on == 1 ? "" : "S");
}

static void skulls_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &sk.flow;
	float u, top, tile_h, left, right, help_y, avail;
	struct ae_row row;
	short pictures[NUMBER_OF_SKULLS];
	int skull, use_mcc;
	char expanded[480];

	(void)instance;
	ae_flow_full_density(flow);
	if (sk.ask_pending)
	{
		sk.ask_pending = 0;
		ask_open(sk.ask);
	}
	ae_flow_title(flow, sk.firefight ? "STARTING SKULLS" : "SKULLS", sk.crumb);
	flow->prompt_count = 0;
	if (sk.ask != ASK_NONE)
	{
		/* (the questions are over an empty screen: the grid is not shown before they are answered, and the dialog's own prompts are the only ones) */
		return;
	}
	sets_read();
	summary_make();
	for (skull = 0; skull < NUMBER_OF_SKULLS; skull++)
		pictures[skull] = (short)skull;
	use_mcc = ae_pve_menus_ui_skulls_use_mcc(pictures, NUMBER_OF_SKULLS);
	u = ae_flow_u(flow, 1.0f);
	left = ae_flow_left_x(flow);
	right = ae_flow_right_x(flow);
	memset(&row, 0, sizeof(row));
	row.label = "SKULL RULES";
	row.value = sk.rules_value;
	row.flags = AE_ROW_SUBSCREEN | AE_ROW_MARQUEE | (sk.focus == ON_FOCUS_RULES ? AE_ROW_FOCUSED : 0);
	top = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y);
	/* (Firefight's has no SKULL RULES: those are the campaign's) */
	if (!sk.firefight)
		top += ae_widget_row(&flow->density, left, top, right - left, &row, HIT_RULES, 0) + 16.0f * u;
	avail = ae_flow_content_bottom(flow) - 78.0f * u - top;
	tile_h = 116.0f * u;
	if (tile_h * ROWS + 8.0f * u * (ROWS - 1) > avail)
		tile_h = (avail - 8.0f * u * (ROWS - 1)) / ROWS;
	for (skull = 0; skull < NUMBER_OF_SKULLS; skull++)
	{
		float x, y, w, h;

		tile_rect(skull, &x, &y, &w, &h, top, tile_h);
		sk.tile[skull] = (struct ae_rect){ x, y, w, h };
		tile_draw(skull, use_mcc, x, y, w, h, sk.focus == skull);
		ae_hit_add(x, y, w, h, HIT_TILES, AE_PART_ITEM, (short)skull);
	}
	/* (the help line under the grid, and how many are on at its right) */
	help_y = ae_flow_content_bottom(flow) - 62.0f * u;
	ae_draw_rect(left, help_y - 12.0f * u, right - left, u, 0.0f, AE_COLOR_RULE);
	if (sk.focus >= 0)
	{
		int skull2 = sk.focus;

		ae_pve_menus_skull_help(skull_descriptions[skull2].label, skull_descriptions[skull2].description,
			skull_set_has(&sk.on, skull2), skull_set_has(&sk.offered, skull2), sk.help, sizeof(sk.help));
		{
			char *newline;

			while ((newline = strchr(sk.help, '\n')) != NULL)
				*newline = ' ';
		}
		if (sk.firefight && skull_set_has(&sk.locked, skull2))
			snprintf(sk.help + strlen(sk.help), sizeof(sk.help) - strlen(sk.help), " The variant carries it: it stays on.");
	}
	else
		snprintf(sk.help, sizeof(sk.help), "How the skulls are had, how many drop, and the random roll. {A} opens SKULL RULES.");
	if (sk.note[0])
		snprintf(sk.help, sizeof(sk.help), "%s", sk.note);
	ae_help_expand(sk.help, expanded, sizeof(expanded));
	{
		char summary[96];
		float summary_w;

		snprintf(summary, sizeof(summary), "%s", sk.summary);
		summary_w = ae_draw_text_tracked_width(AE_FONT_ROW, ae_flow_u(flow, 20.0f), 0.04f, summary);
		ae_draw_text_tracked(AE_FONT_ROW, ae_flow_u(flow, 20.0f), 0.04f, right, ae_flow_cap_top(AE_FONT_ROW, ae_flow_u(flow, 20.0f), help_y + 16.0f * u),
			AE_ALIGN_RIGHT, AE_COLOR_TITLE, summary);
		ae_pve_menus_ui_paragraph(flow, 20.0f, 28.0f, left, help_y + 16.0f * u, right - left - summary_w - 40.0f * u, 2, AE_COLOR_TEXT, expanded);
	}
	if (sk.focus >= 0)
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, skull_set_has(&sk.on, sk.focus) ? "Turn off" : "Turn on",
			AE_ACTION_ACCEPT);
	else
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Open", AE_ACTION_ACCEPT);
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_X, (sk.firefight ? skull_count(&sk.on) > skull_count(&sk.locked) :
		skull_count(&sk.on) > 0) ? "All off" : "All on", AE_ACTION_X);
	if (!sk.firefight)
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_Y, "Random", AE_ACTION_Y);
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	ae_flow_draw_prompts(flow);
}

/* ---------- input */

static void focus_move(int dx, int dy, int wrap_press)
{
	int index = sk.focus;

	(void)wrap_press;
	if (index == ON_FOCUS_RULES)
	{
		if (dy > 0)
			sk.focus = (short)sk.column;
		return;
	}
	if (dy < 0)
	{
		if (index < COLUMNS && sk.firefight)
			return;
		if (index < COLUMNS)
		{
			sk.column = (short)(index % COLUMNS);
			sk.focus = ON_FOCUS_RULES;
		}
		else
			sk.focus = (short)(index - COLUMNS);
	}
	else if (dy > 0)
	{
		if (index + COLUMNS < NUMBER_OF_SKULLS)
			sk.focus = (short)(index + COLUMNS);
	}
	else if (dx)
		sk.focus = (short)((index + dx + NUMBER_OF_SKULLS) % NUMBER_OF_SKULLS);
	if (sk.focus >= 0)
		sk.column = (short)(sk.focus % COLUMNS);
	sk.note[0] = 0;
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

static void accept(void)
{
	if (sk.focus == ON_FOCUS_RULES)
	{
		ae_sound_request(AE_SOUND_FORWARD, 0);
		ae_screen_skull_rules_open();
	}
	else
		toggle(sk.focus);
}

static int skulls_handle(struct ae_screen *instance, struct ae_event const *event)
{
	(void)instance;
	if (sk.ask != ASK_NONE)
		return event->action == AE_ACTION_BACK ? 0 : 1;
	switch (event->action)
	{
	case AE_ACTION_BACK:
		return 0;
	case AE_ACTION_UP:
		focus_move(0, -1, 0);
		return 1;
	case AE_ACTION_DOWN:
		focus_move(0, 1, 0);
		return 1;
	case AE_ACTION_LEFT:
		focus_move(-1, 0, 0);
		return 1;
	case AE_ACTION_RIGHT:
		focus_move(1, 0, 0);
		return 1;
	case AE_ACTION_ACCEPT:
		accept();
		return 1;
	case AE_ACTION_X:
		all_toggle();
		return 1;
	case AE_ACTION_Y:
		roll();
		return 1;
	default:
		return 1;
	}
}

static void skulls_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;

	if (sk.ask != ASK_NONE)
		return;
	if (ae_prompts_pointer(sk.flow.prompts, sk.flow.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK)
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_pop();
		}
		else
			skulls_handle(instance, &event);
		return;
	}
	if (!ae_hit_at(pointer->x, pointer->y, 0, &hit))
		return;
	if (hit.id == HIT_TILES && hit.part == AE_PART_ITEM)
	{
		if (pointer->moved)
			sk.focus = (short)hit.index, sk.column = (short)(hit.index % COLUMNS), sk.note[0] = 0;
		if (pointer->left_clicks)
		{
			sk.focus = (short)hit.index;
			toggle(hit.index);
		}
	}
	else if (hit.id == HIT_RULES)
	{
		if (pointer->moved)
			sk.focus = ON_FOCUS_RULES;
		if (pointer->left_clicks)
		{
			sk.focus = ON_FOCUS_RULES;
			accept();
		}
	}
}

static void skulls_leave(struct ae_screen *instance)
{
	(void)instance;
	platform_log("ae skulls: closed");
}

static struct ae_screen_class const skulls_class =
{
	.name = "skulls", .leave = skulls_leave, .handle = skulls_handle, .draw = skulls_draw, .pointer = skulls_pointer,
};

static int open_over(int questions)
{
	memset(&sk, 0, sizeof(sk));
	{
		char name[48];

		ae_pve_menus_ui_profile(name, sizeof(name));
		snprintf(sk.crumb, sizeof(sk.crumb), "CAMPAIGN \xC2\xB7 %s", name);
	}
	sk.flow.hit_rows = HIT_RULES;
	sk.flow.hit_prompts = HIT_PROMPTS;
	sk.focus = 0;
	/* (the questions come first the first time: with no answer on file, as Content Evolved asks them) */
	if (questions || skulls_unlock_mode() == _skulls_unlock_unset)
	{
		sk.ask = ASK_HOW;
		sk.ask_pending = 1;
	}
	if (!ae_ui_push(&skulls_class, AE_OWNER_ANY, &sk))
		return 0;
	sets_read();
	platform_log("ae skulls: open (%s)%s", sk.crumb, sk.ask ? ", asking" : "");
	return 1;
}

int ae_screen_skulls_open(void)
{
	return open_over(0);
}

int ae_screen_skulls_open_staged(int questions)
{
	if (ae_ui_holds(&sk))
		ae_ui_remove(&sk);
	if (questions)
	{
		/* (the answers not given: as a first run has them) */
		ae_pve_menus_ui_write("game.skulls_unlock_mode", "");
	}
	return open_over(questions);
}

int ae_screen_skulls_open_firefight(void)
{
	if (!ae_setup_edit_is_firefight())
		return 0;
	memset(&sk, 0, sizeof(sk));
	sk.firefight = 1;
	snprintf(sk.crumb, sizeof(sk.crumb), "FIREFIGHT");
	sk.flow.hit_rows = HIT_RULES;
	sk.flow.hit_prompts = HIT_PROMPTS;
	sk.focus = 0;
	if (!ae_ui_push(&skulls_class, AE_OWNER_ANY, &sk))
		return 0;
	sets_read();
	platform_log("ae skulls: open (firefight)");
	return 1;
}
