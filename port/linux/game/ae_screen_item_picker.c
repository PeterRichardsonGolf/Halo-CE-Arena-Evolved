/*
AE_SCREEN_ITEM_PICKER.C

The campaign lobby's ITEM SETS picker (ae_screen_item_picker.h): one list with two section heads (CAMPAIGN SWAPS, WITH THE RANDOMIZER), the focused
choice described on the right. A row that cannot be picked says why in its value place. The words and the writes are ae_item_swaps_rules.h's.
*/

#include <stdio.h>
#include <string.h>
#include "cseries.h"
#include "../src/ae_draw.h"
#include "../src/ae_platform.h"
#include "ae_dev_mode.h"
#include "ae_item_sets_data.h"
#include "ae_motion.h"
#include "ae_pve_menus_ui.h"
#include "ae_screen_item_picker.h"
#include "ae_screen_item_sets.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_ui.h"

void platform_log(char const *format, ...);

#define PICKER_HIT 0x7D40
#define DOT " \xE2\x80\xA2 "

/* a row: what it is (NONE, a swap or a set) and its index in the table's list */
struct pick_row { int what, index; };

static struct
{
	struct ae_pvem_rows rows;
	struct ae_is_backend const *be;
	int staged, waits, randomizer_on, mcc;
	struct ae_is_set sets[AE_IS_SETS_MOST];
	int set_count;
	struct ae_is_swap swaps[AE_IS_SWAPS_MOST];
	int swap_count;
	int picked;
	char picked_id[64];
	struct pick_row row_of[AE_FLOW_ROWS];
	char value[AE_FLOW_ROWS][48], help[AE_FLOW_ROWS][260];
} P;

/* the lobby row's data, kept for half a second (the lobby asks every frame) */
static struct { unsigned long at; int shown, swap_playing; char value[48]; int demo; } lobby_cache;
static struct ae_is_backend const *demo_be;

static int same(char const *a, char const *b)
{
	return !strcmp(a, b);
}

static void data_read(void)
{
	P.set_count = P.be->sets(P.sets, AE_IS_SETS_MOST);
	P.swap_count = P.be->swaps(P.swaps, AE_IS_SWAPS_MOST);
	P.picked = P.be->picked(P.picked_id, sizeof(P.picked_id));
	P.randomizer_on = P.waits ? 0 : P.be->randomizer_on();
}

static void build(struct ae_pvem_rows *rows)
{
	struct ae_flow_screen *flow = &rows->flow;
	int n = 0, each;

	/* (NONE, then each swap under its head, then each set under its head) */
	P.row_of[n].what = AE_IS_PICK_NONE;
	P.row_of[n].index = 0;
	snprintf(P.value[n], sizeof(P.value[0]), "%s", ae_is_pick_none_value(P.picked == AE_IS_PICK_NONE));
	snprintf(P.help[n], sizeof(P.help[0]), "%s", ae_is_pick_none_help());
	flow->rows[n] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "NONE", P.value[n], P.help[n], NULL };
	n++;
	if (P.swap_count && n < AE_FLOW_ROWS)
	{
		P.row_of[n].what = -1;
		flow->rows[n++] = (struct ae_flow_row){ AE_FLOW_ROW_HEADER, -1, "CAMPAIGN SWAPS", NULL, NULL, NULL };
	}
	for (each = 0; each < P.swap_count && n < AE_FLOW_ROWS; each++)
	{
		struct ae_is_swap const *swap = &P.swaps[each];
		int state = ae_is_swap_state(P.picked == AE_IS_PICK_SWAP && same(P.picked_id, swap->file), swap->set_found, swap->set_items_on);
		char const *wait = ae_is_pick_swap_wait(state);

		P.row_of[n].what = AE_IS_PICK_SWAP;
		P.row_of[n].index = each;
		ae_is_pick_swap_value(state, swap->set_name, P.value[n], sizeof(P.value[0]));
		ae_is_pick_swap_help(swap->name, swap->set_name, P.help[n], sizeof(P.help[0]));
		flow->rows[n] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, swap->name, wait[0] ? NULL : P.value[n], P.help[n], wait[0] ? wait : NULL };
		n++;
	}
	if (P.set_count && n < AE_FLOW_ROWS)
	{
		P.row_of[n].what = -1;
		flow->rows[n++] = (struct ae_flow_row){ AE_FLOW_ROW_HEADER, -1, "WITH THE RANDOMIZER", NULL, NULL, NULL };
	}
	for (each = 0; each < P.set_count && n < AE_FLOW_ROWS; each++)
	{
		struct ae_is_set const *set = &P.sets[each];
		char const *wait = ae_is_pick_set_wait(set->on, P.randomizer_on, P.mcc);

		P.row_of[n].what = AE_IS_PICK_SET;
		P.row_of[n].index = each;
		ae_is_pick_set_value(P.picked == AE_IS_PICK_SET && same(P.picked_id, set->folder), set->on, P.value[n], sizeof(P.value[0]));
		ae_is_pick_set_help(set->name, P.help[n], sizeof(P.help[0]));
		flow->rows[n] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, set->name, wait[0] ? NULL : P.value[n], P.help[n], wait[0] ? wait : NULL };
		n++;
	}
	flow->count = (short)n;
	rows->y_hidden = !(ae_dev_mode_on() && !P.staged && flow->focus > 0 && flow->focus < n && P.row_of[flow->focus].what > 0);
}

static void step(struct ae_pvem_rows *rows, int row, int direction)
{
	char const *id = "";

	(void)direction;
	if (row < 0 || row >= rows->flow.count || P.row_of[row].what < 0)
		return;
	if (P.row_of[row].what == AE_IS_PICK_SWAP)
		id = P.swaps[P.row_of[row].index].file;
	else if (P.row_of[row].what == AE_IS_PICK_SET)
		id = P.sets[P.row_of[row].index].folder;
	if (!P.be->pick(P.row_of[row].what, id))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		platform_log("item picker: the pick could not be written");
		return;
	}
	lobby_cache.at = 0;
	ae_sound_request(AE_SOUND_FORWARD, 0);
	ae_pve_menus_rows_close(rows);
}

static void edit(struct ae_pvem_rows *rows)
{
	int row = rows->flow.focus;

	if (row < 0 || row >= rows->flow.count || P.row_of[row].what <= 0 || P.staged)
		return;
	if (ae_screen_item_sets_open_from(P.row_of[row].what == AE_IS_PICK_SWAP ? 0 : 1,
		P.row_of[row].what == AE_IS_PICK_SWAP ? P.swaps[P.row_of[row].index].file : P.sets[P.row_of[row].index].folder))
		ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void fact(struct ae_flow_screen const *flow, float x, float width, float y, char const *label, char const *value, unsigned int color)
{
	float u = ae_flow_u(flow, 1.0f), top = flow->frame.rect.y;

	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, top + y * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, label);
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 20.0f, x + width, top + y * u, AE_ALIGN_RIGHT, color, value);
}

/* the right side: the focused choice */
static void side(struct ae_pvem_rows *rows, float x, float width)
{
	struct ae_flow_screen const *flow = &rows->flow;
	float u = ae_flow_u(flow, 1.0f), top = flow->frame.rect.y;
	int focus = flow->focus;
	char text[160];

	ae_pve_menus_ui_plate(flow, x, width, 176.0f, 520.0f);
	if (focus < 0 || focus >= flow->count || P.row_of[focus].what <= 0)
	{
		ae_pve_menus_ui_text(flow, AE_FONT_ROW, 30.0f, x, top + 218.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, "The campaign's own");
		ae_draw_rect(x, top + 250.0f * u, width, u, 0.0f, AE_COLOR_RULE);
		ae_pve_menus_ui_paragraph(flow, 16.0f, 24.0f, x, top + 290.0f * u, width, 4, AE_COLOR_MUTED,
			"The mission plays with its own enemies and weapons only.");
	}
	else if (P.row_of[focus].what == AE_IS_PICK_SWAP)
	{
		struct ae_is_swap const *swap = &P.swaps[P.row_of[focus].index];
		char value[96];

		ae_pve_menus_ui_text(flow, AE_FONT_ROW, 30.0f, x, top + 218.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, swap->name);
		ae_draw_rect(x, top + 250.0f * u, width, u, 0.0f, AE_COLOR_RULE);
		snprintf(value, sizeof(value), "%s", swap->set_found ? swap->set_name : "Its set is gone");
		fact(flow, x, width, 282.0f, "PLAYS", value, swap->set_found ? AE_COLOR_TEXT : AE_COLOR_WARNING);
		fact(flow, x, width, 320.0f, "WEAPONS", !strcmp(swap->weapon_replace, "manual") ? "By weapon" : "At random", AE_COLOR_TEXT);
		fact(flow, x, width, 358.0f, "ENEMIES", !strcmp(swap->enemy_replace, "manual") ? "By enemy" : "At random", AE_COLOR_TEXT);
		{
			char biped[256];

			biped[0] = 0;
			P.be->swap_option(swap->file, "player_biped", biped, sizeof(biped));
			if (biped[0])
			{
				char name[96];

				ae_is_item_name(biped, name, sizeof(name));
				fact(flow, x, width, 396.0f, "YOUR BODY", name, AE_COLOR_TEXT);
			}
			else
				fact(flow, x, width, 396.0f, "YOUR BODY", "The campaign's own", AE_COLOR_TEXT);
		}
		fact(flow, x, width, 434.0f, "RANDOMIZER", "Passed over", AE_COLOR_MUTED);
	}
	else
	{
		struct ae_is_set const *set = &P.sets[P.row_of[focus].index];
		static int const order[4] = { 0, 1, 3, 4 };
		float y = 282.0f;
		int each;

		ae_pve_menus_ui_text(flow, AE_FONT_ROW, 30.0f, x, top + 218.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, set->name);
		ae_draw_rect(x, top + 250.0f * u, width, u, 0.0f, AE_COLOR_RULE);
		for (each = 0; each < 4; each++)
			if (set->kinds[order[each]] > 0)
			{
				snprintf(text, sizeof(text), "%ld", set->kinds[order[each]]);
				fact(flow, x, width, y, ae_is_kind_name(order[each]), text, AE_COLOR_TEXT);
				y += 38.0f;
			}
		ae_pve_menus_ui_paragraph(flow, 15.0f, 22.0f, x, top + (y + 30.0f) * u, width, 3, AE_COLOR_MUTED,
			"Its weapons and AI join the randomizer's pools, and the pools get an ITEM SET choice.");
	}
	ae_pve_menus_ui_paragraph(flow, 15.0f, 22.0f, x, top + 560.0f * u, width, 3, AE_COLOR_MUTED, "Alone and in split screen only.");
}

int ae_screen_item_picker_open(int demo, int waits_scene, int mcc_mission)
{
	if (ae_ui_holds(&P.rows))
		ae_pve_menus_rows_close(&P.rows);
	memset(&P, 0, sizeof(P));
	if (demo && !demo_be)
		demo_be = ae_is_backend_staged(0);
	P.be = demo ? demo_be : ae_is_backend_engine();
	P.staged = demo;
	P.waits = waits_scene;
	P.mcc = mcc_mission;
	data_read();
	P.rows.name = "campaign item sets";
	P.rows.title = "ITEM SETS";
	snprintf(P.rows.crumb, sizeof(P.rows.crumb), "CAMPAIGN");
	P.rows.build = build;
	P.rows.step = step;
	P.rows.side = side;
	P.rows.y_action = edit;
	P.rows.y_word = "Edit";
	P.rows.hit_base = PICKER_HIT;
	return ae_pve_menus_rows_open(&P.rows);
}

int ae_screen_item_picker_lobby(int demo, int mcc_mission, char *value, int size, int *swap_playing)
{
	unsigned long now = ae_motion_now();

	if (demo)
	{
		struct ae_is_backend const *be = demo_be ? demo_be : ae_is_backend_staged(0);
		struct ae_is_set sets[AE_IS_SETS_MOST];
		struct ae_is_swap swaps[AE_IS_SWAPS_MOST];
		char id[64];
		int count = be->sets(sets, AE_IS_SETS_MOST), kind = be->picked(id, sizeof(id)), each, swap_count = be->swaps(swaps, AE_IS_SWAPS_MOST);

		demo_be = be;
		(void)count;
		*swap_playing = kind == AE_IS_PICK_SWAP;
		for (each = 0; each < swap_count; each++)
			if (kind == AE_IS_PICK_SWAP && same(swaps[each].file, id))
				ae_is_lobby_value(AE_IS_PICK_SWAP, swaps[each].name, value, size);
		if (kind != AE_IS_PICK_SWAP)
			ae_is_lobby_value(kind, id, value, size);
		return 1;
	}
	if (!lobby_cache.at || now - lobby_cache.at > 500ul || lobby_cache.demo)
	{
		struct ae_is_backend const *be = ae_is_backend_engine();
		struct ae_is_set sets[AE_IS_SETS_MOST];
		struct ae_is_swap swaps[AE_IS_SWAPS_MOST];
		char id[64];
		int count = be->sets(sets, AE_IS_SETS_MOST), kind = be->picked(id, sizeof(id)), each, has_on = 0, swap_count = be->swaps(swaps, AE_IS_SWAPS_MOST);

		for (each = 0; each < count; each++)
			has_on |= sets[each].on > 0;
		lobby_cache.shown = ae_is_lobby_row_shown(has_on, 0, 0);
		lobby_cache.swap_playing = 0;
		snprintf(lobby_cache.value, sizeof(lobby_cache.value), "NONE");
		for (each = 0; each < swap_count && kind == AE_IS_PICK_SWAP; each++)
			if (same(swaps[each].file, id))
			{
				ae_is_lobby_value(AE_IS_PICK_SWAP, swaps[each].name, lobby_cache.value, sizeof(lobby_cache.value));
				lobby_cache.swap_playing = swaps[each].set_found && swaps[each].set_items_on > 0;
			}
		for (each = 0; each < count && kind == AE_IS_PICK_SET; each++)
			if (same(sets[each].folder, id))
				ae_is_lobby_value(AE_IS_PICK_SET, sets[each].name, lobby_cache.value, sizeof(lobby_cache.value));
		lobby_cache.at = now;
		lobby_cache.demo = 0;
	}
	(void)mcc_mission;
	snprintf(value, (size_t)size, "%s", lobby_cache.value);
	*swap_playing = lobby_cache.swap_playing;
	return lobby_cache.shown;
}

void ae_screen_item_picker_demo(struct ae_is_backend const *be)
{
	demo_be = be ? be : ae_is_backend_staged(0);
}
