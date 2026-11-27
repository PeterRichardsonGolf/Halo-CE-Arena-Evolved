/*
AE_SCREEN_SAVED_GAMES.C

See ae_screen_saved_games.h. Rows on the AE flow (ae_pve_menus_ui.h's rows screen): the campaign's name as a header and one row
for each profile with a saved game (its mission as the value, YOUR PROFILE on the active one); on the right the focused saved
game's mission, profile, difficulty, when it was kept and whether the AI squad goes along.
*/

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "ae_campaign_rows_rules.h"
#include "ae_glue_campaign.h"
#include "ae_glue_pve_menus.h"
#include "ae_pve_menus_rules.h"
#include "ae_pve_menus_ui.h"
#include "ae_screen_saved_games.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_style.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum { SAVED_HIT = 0x7AA0, ROWS_MAX = AE_FLOW_ROWS - 2 };
#define STOCK_TITLE "HALO: COMBAT EVOLVED"

static struct ae_pvem_rows saved_screen;
static struct ae_pve_saved saves[AE_PVE_PROFILES];
static int save_count, staged_empty, staged;
static void (*picked_call)(void);
static char values[AE_FLOW_ROWS][96], helps[AE_FLOW_ROWS][200];
static char delete_body[300], delete_title[96];

/* DEV MODE's scene: three made-up saved games of made-up profiles, none when empty; nothing real is read, picked or deleted */
static void saves_demo(void)
{
	static char const *const names[3] = { "Rook", "Vesper", "Marlow" };
	int each;

	memset(saves, 0, sizeof(saves));
	save_count = staged_empty ? 0 : 3;
	for (each = 0; each < save_count; each++)
	{
		snprintf(saves[each].name, sizeof(saves[each].name), "%s", names[each]);
		saves[each].profile_index = each + 1;
		saves[each].level = (short)(each * 3 + 1);
		saves[each].difficulty = (short)(each == 1 ? 3 : 2);
		saves[each].active = each == 0;
		saves[each].day = each == 0 ? 100 : each == 1 ? 99 : 40;
		saves[each].hour = 21;
		saves[each].minute = 14;
		saves[each].month = 10;
		saves[each].year = 2026;
		saves[each].month_day = 5;
	}
}

static void saves_read(void)
{
	if (staged)
		saves_demo();
	else
		save_count = ae_pve_saved_games(saves, ROWS_MAX);
}

int ae_screen_saved_games_count(void)
{
	return ae_pve_saved_games(saves, ROWS_MAX);
}

static void mission_name(int save, char *text, int size)
{
	ae_campaign_level_name(saves[save].level, text, size);
}

static void build(struct ae_pvem_rows *r)
{
	struct ae_flow_screen *flow = &r->flow;
	int each;
	short n = 0;

	/* (nothing to delete with no save: B only) */
	r->x_hidden = save_count == 0;
	if (save_count == 0)
	{
		flow->rows[n++] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "NO SAVED GAMES YET", NULL,
			"The game saves at each checkpoint of a mission you play. Every profile's saved game shows here.", NULL, 0, 0, 1 };
		flow->count = n;
		return;
	}
	flow->rows[n++] = (struct ae_flow_row){ AE_FLOW_ROW_HEADER, -1, STOCK_TITLE, NULL, NULL, NULL };
	for (each = 0; each < save_count; each++)
	{
		char mission[64];

		mission_name(each, mission, sizeof(mission));
		snprintf(values[n], sizeof(values[0]), "%s%s", saves[each].active ? "YOUR PROFILE \xE2\x80\xA2 " : "", mission);
		snprintf(helps[n], sizeof(helps[0]), "{A} sets the campaign to %s's saved game: player 1 plays on %s's profile, and CONTINUE goes on from there. {X} deletes it.",
			saves[each].name, saves[each].name);
		/* (a status row: A picks it, left / right changes nothing, so no arrows; the name in capitals like every row label) */
		{
			static char labels[AE_FLOW_ROWS][48];
			int c;

			snprintf(labels[n], sizeof(labels[0]), "%s", saves[each].name);
			for (c = 0; labels[n][c]; c++)
				if (labels[n][c] >= 'a' && labels[n][c] <= 'z')
					labels[n][c] = (char)(labels[n][c] - 32);
			flow->rows[n] = (struct ae_flow_row){ .kind = AE_FLOW_ROW_OWN, .option = -1, .label = labels[n], .value = values[n],
				.help = helps[n], .status = 1 };
		}
		n++;
	}
	flow->count = n;
}

/* the save under the focus, -1 for none */
static int focused_save(struct ae_pvem_rows *r)
{
	int save = r->flow.focus - 1;

	return save_count > 0 && save >= 0 && save < save_count ? save : -1;
}

static void step(struct ae_pvem_rows *r, int row, int direction)
{
	int save = focused_save(r);

	(void)row;
	(void)direction;
	if (save < 0)
		return;
	if (staged || ae_pve_saved_pick(saves[save].profile_index))
	{
		ae_sound_request(AE_SOUND_FORWARD, 0);
		if (picked_call && !staged)
			picked_call();
		ae_pve_menus_rows_close(r);
	}
	else
		ae_sound_request(AE_SOUND_FAILURE, 0);
}

static const char *a_word(struct ae_pvem_rows *r, int row)
{
	static char word[80];
	int save = focused_save(r);

	(void)row;
	if (save < 0)
		return "Pick";
	snprintf(word, sizeof(word), "Pick as %s", saves[save].name);
	return word;
}

/* ---------- delete */

static int delete_target = -1;

static void delete_picked(short choice, void *context)
{
	struct ae_pvem_rows *r = context;

	if (choice != 1 || delete_target < 0 || delete_target >= save_count)
		return;
	if (staged)
	{
		/* (the made-up list loses the row: no file is touched) */
		memmove(&saves[delete_target], &saves[delete_target + 1], (size_t)(save_count - delete_target - 1) * sizeof(saves[0]));
		save_count--;
		ae_sound_request(AE_SOUND_BACK, 0);
	}
	else
	{
		if (ae_pve_saved_delete(saves[delete_target].profile_index))
			ae_sound_request(AE_SOUND_BACK, 0);
		else
			ae_sound_request(AE_SOUND_FAILURE, 0);
		saves_read();
	}
	if (r->flow.focus >= r->flow.count)
		r->flow.focus = (short)(r->flow.count - 1);
	delete_target = -1;
}

static void delete_open(struct ae_pvem_rows *r)
{
	struct ae_dialog_spec spec;
	char mission[64], upper[48];
	int save = focused_save(r), each;

	if (save < 0)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	delete_target = save;
	mission_name(save, mission, sizeof(mission));
	snprintf(upper, sizeof(upper), "%s", saves[save].name);
	for (each = 0; upper[each]; each++)
		if (upper[each] >= 'a' && upper[each] <= 'z')
			upper[each] = (char)(upper[each] - 32);
	snprintf(delete_title, sizeof(delete_title), "DELETE %s'S SAVED GAME?", upper);
	snprintf(delete_body, sizeof(delete_body), "%s's saved game on %s (%s) is deleted. %s's finished missions and best runs stay. This cannot be undone.",
		saves[save].name, mission, ae_campaign_difficulty_name(saves[save].difficulty), saves[save].name);
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = delete_title;
	spec.body = delete_body;
	spec.choices[0] = "CANCEL";
	spec.choices[1] = "DELETE";
	spec.choice_count = 2;
	spec.safe_choice = 0;
	spec.cancel_choice = 0;
	spec.has_cancel = 1;
	spec.destructive_choices = 1u << 1;
	spec.density = r->flow.density;
	spec.bounds = r->flow.frame.rect;
	spec.picked = delete_picked;
	spec.context = r;
	ae_dialog_open(&spec, AE_OWNER_ANY);
}

static void delete_asked(struct ae_pvem_rows *r)
{
	r->dialog_pending = 1;
}

/* ---------- the right-hand panel */

static void fact(struct ae_flow_screen const *flow, float x, float width, float y, const char *label, const char *value)
{
	float u = ae_flow_u(flow, 1.0f);

	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, flow->frame.rect.y + y * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, label);
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 20.0f, x + width, flow->frame.rect.y + y * u, AE_ALIGN_RIGHT, AE_COLOR_TEXT, value);
}

static void side(struct ae_pvem_rows *r, float x, float width)
{
	struct ae_flow_screen const *flow = &r->flow;
	float u = ae_flow_u(flow, 1.0f), top = flow->frame.rect.y;
	int save = focused_save(r);
	char mission[64], text[96];

	ae_pve_menus_ui_plate(flow, x, width, 176.0f, 470.0f);
	if (save < 0)
	{
		ae_pve_menus_ui_text(flow, AE_FONT_ROW, 28.0f, x, top + 218.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, "NO SAVED GAMES");
		return;
	}
	mission_name(save, mission, sizeof(mission));
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 30.0f, x, top + 218.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, mission);
	snprintf(text, sizeof(text), "MISSION %d OF %d \xE2\x80\xA2 %s", saves[save].level + 1, AE_CAMPAIGN_LEVELS, STOCK_TITLE);
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 15.0f, x, top + 250.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, text);
	ae_draw_rect(x, top + 272.0f * u, width, u, 0.0f, AE_COLOR_RULE);
	{
		char upper[48];
		int each;

		snprintf(upper, sizeof(upper), "%s", saves[save].name);
		for (each = 0; upper[each]; each++)
			if (upper[each] >= 'a' && upper[each] <= 'z')
				upper[each] = (char)(upper[each] - 32);
		fact(flow, x, width, 300.0f, "PLAYS AS", upper);
	}
	fact(flow, x, width, 338.0f, "DIFFICULTY", ae_campaign_difficulty_name(saves[save].difficulty));
	if (saves[save].day >= 0)
	{
		time_t now = time(NULL);
		struct tm *local = localtime(&now);
		long today = local ? ae_pve_menus_day(local->tm_year + 1900, local->tm_mon, local->tm_mday) : -1;

		ae_pve_menus_when_text(saves[save].day, today, saves[save].hour, saves[save].minute, saves[save].month, saves[save].year,
			saves[save].month_day, text, sizeof(text));
	}
	else
		snprintf(text, sizeof(text), "-");
	fact(flow, x, width, 376.0f, "SAVED", text);
	fact(flow, x, width, 414.0f, "AI SQUAD", AE_CAMPAIGN_SAVE_TAKES_SQUAD ? "TAKES THE SQUAD" : "SOLO ONLY");
}

static int open_over(void)
{
	if (ae_ui_holds(&saved_screen))
		ae_pve_menus_rows_close(&saved_screen);
	memset(&saved_screen, 0, sizeof(saved_screen));
	saved_screen.name = "saved games";
	saved_screen.title = "SAVED GAMES";
	{
		char name[48];

		ae_pve_menus_ui_profile(name, sizeof(name));
		snprintf(saved_screen.crumb, sizeof(saved_screen.crumb), "SELECT MISSION \xC2\xB7 %s", name);
	}
	saved_screen.build = build;
	saved_screen.step = step;
	saved_screen.a_word = a_word;
	saved_screen.x_action = delete_asked;
	saved_screen.x_word = "Delete";
	saved_screen.side = side;
	saved_screen.dialog_open = delete_open;
	saved_screen.hit_base = SAVED_HIT;
	saved_screen.backing = 1;
	saves_read();
	if (!ae_pve_menus_rows_open(&saved_screen))
		return 0;
	/* (the active profile's saved game has the focus) */
	{
		int each;

		for (each = 0; each < save_count; each++)
			if (saves[each].active)
				saved_screen.flow.focus = (short)(each + 1);
	}
	return 1;
}

int ae_screen_saved_games_open(void (*picked)(void))
{
	picked_call = picked;
	staged_empty = 0;
	staged = 0;
	return open_over();
}

int ae_screen_saved_games_open_staged(int empty, int delete_dialog)
{
	picked_call = NULL;
	staged_empty = empty;
	staged = 1;
	if (!open_over())
		return 0;
	saved_screen.dialog_pending = delete_dialog && save_count > 0;
	return 1;
}
