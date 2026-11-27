/*
AE_SCREEN_FORGE.C

See ae_screen_forge.h. Three screens on the AE flow (ae_pve_menus_ui.h's rows screen): the FORGE hub, NEW MAP's list of the
multiplayer maps, and FORGE KEYS. The hub asks for Forge's tag pack when it opens and shows its progress; the rows that need
the pack wait with the percent. Forge's own binds are read and written through ae_forge_dev.c (Forge's table and its file,
Faber_data/forge_controls.ini, stay Content Evolved's).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "../src/port_config.h"
#include "opence_features.h"
#include "ae_forge_doors_rules.h"
#include "ae_forge_dev.h"
#include "ae_forge_maps.h"
#include "ae_motion.h"
#include "ae_pve_menus_ui.h"
#include "ae_screen_forge.h"

#if OPENCE_ENABLE_FORGE

#include "halo_ui_map_list.h"
#include "ae_scoreboard_header.h"
#include "ae_screen_create_game.h"
#include "ae_screen_main_menu.h"
#include "ae_text_edit.h"
#include "../src/ae_platform.h"
#include "ae_screen_maps_hub.h"
#include "ae_sound.h"
#include "ae_stage_map.h"
#include "ae_style.h"
#include "ae_ui.h"
#include "ae_widgets.h"
#include "tag_pack.h"

void platform_log(char const *format, ...);
short ui_widget_port_multiplayer_maps(char const *const **names, short *last_used);
void platform_binding_capture_begin(void);
void platform_binding_capture_cancel(void);
int platform_binding_capture_poll(int *input);
int platform_gamepad_buttons(int port, unsigned long *buttons);
void halo_input_name(int input, char *name, size_t size);
unsigned long system_milliseconds(void);

#define FORGE_PACK "watanoge_forge"

enum { HUB_HIT = 0x7AE0, PICK_HIT = 0x7AE4, KEYS_HIT = 0x7AE8, MAPS_HIT = 0x7AEC, PICK_SHOWN = 22, MAPS_MAX = 256, CAPTURE_MS = 10000 };

/* the pack as a staged scene says (-1: the real one): 0 ready, 1 being built at 40 %, 2 without the game's maps */
static int staged_pack = -1;
static int staged;

static struct ae_pvem_rows hub_screen, pick_screen, keys_screen, mymaps_screen;
static int keys_open(void);
static int mymaps_open(void);

/* ---------- the maps NEW MAP lists (the multiplayer maps this PC can start) */

struct map_entry { char path[96]; char title[48]; };
static struct map_entry maps[MAPS_MAX];
static int map_count;

static void upper_ascii(wchar_t const *wide, char *out, int size)
{
	int at;

	for (at = 0; at < size - 1 && wide[at]; at++)
		out[at] = wide[at] >= 'a' && wide[at] <= 'z' ? (char)(wide[at] - 32) : wide[at] >= 0x20 && wide[at] < 0x7F ? (char)wide[at] : '?';
	out[at] = 0;
}

static void upper_ascii_text(char const *text, char *out, int size)
{
	int at;

	for (at = 0; at < size - 1 && text[at]; at++)
		out[at] = text[at] >= 'a' && text[at] <= 'z' ? (char)(text[at] - 32) : text[at] == '_' ? ' ' : text[at];
	out[at] = 0;
}

static int map_compare(void const *a, void const *b)
{
	return strcmp(((struct map_entry const *)a)->title, ((struct map_entry const *)b)->title);
}

static void maps_read(void)
{
	char const *const *names = NULL;
	short count = ui_widget_port_multiplayer_maps(&names, NULL), index;

	map_count = 0;
	for (index = 0; index < count && map_count < MAPS_MAX; index++)
	{
		wchar_t wide[64];
		char leaf[96];
		char const *base = strrchr(names[index], '\\');

		snprintf(leaf, sizeof(leaf), "%s", base ? base + 1 : names[index]);
		/* (the menus' own map is not one Forge edits) */
		if (!_stricmp(leaf, "ui") || !ae_stage_map_file_present(names[index]))
			continue;
		snprintf(maps[map_count].path, sizeof(maps[0].path), "%s", names[index]);
		if (!ae_pack_map_caption(names[index], wide, 64))
			ui_map_list_caption(names[index], wide, 64);
		upper_ascii(wide, maps[map_count].title, (int)sizeof(maps[0].title));
		if (!maps[map_count].title[0])
			ae_forge_doors_map_title(names[index], maps[map_count].title, (int)sizeof(maps[0].title));
		map_count++;
	}
	qsort(maps, (size_t)map_count, sizeof(maps[0]), map_compare);
}

/* ---------- the pack */

struct pack_view
{
	int state, percent, done, wanted;
	char reason[160];
};

static void pack_read(struct pack_view *view)
{
	static struct pack_view kept;
	static unsigned long read_at;
	struct tag_pack_state state;
	unsigned long now;

	memset(view, 0, sizeof(*view));
	if (staged_pack >= 0)
	{
		view->wanted = 17;
		view->state = staged_pack == 0 ? AE_FORGE_PACK_READY : staged_pack == 1 ? AE_FORGE_PACK_BUILDING : AE_FORGE_PACK_NEEDS_MAPS;
		view->done = staged_pack == 0 ? 17 : staged_pack == 1 ? 7 : 0;
		if (staged_pack == 2)
			snprintf(view->reason, sizeof(view->reason), "none of its maps is there");
		view->percent = staged_pack == 1 ? 40 : ae_forge_doors_percent(view->done, view->wanted);
		return;
	}
	/* (the pack's state reads the maps' headers: asked four times a second, not every frame) */
	now = system_milliseconds();
	if (read_at && now - read_at < 250)
	{
		*view = kept;
		return;
	}
	read_at = now ? now : 1;
	tag_pack_state(FORGE_PACK, &state);
	view->state = ae_forge_doors_pack_state(tag_pack_ready(FORGE_PACK), state.writing, state.failed, state.maps_wanted);
	view->done = state.maps_done;
	view->wanted = state.maps_wanted;
	view->percent = view->state == AE_FORGE_PACK_READY ? 100 : ae_forge_doors_percent(state.maps_done, state.maps_wanted);
	snprintf(view->reason, sizeof(view->reason), "%s", state.reason);
	kept = *view;
}

const char *ae_screen_forge_badge(void)
{
	static char badge[16];
	struct pack_view view;

	if (staged_pack >= 0 || !tag_pack_busy() || tag_pack_ready(FORGE_PACK))
		return NULL;
	pack_read(&view);
	if (view.state != AE_FORGE_PACK_BUILDING)
		return NULL;
	snprintf(badge, sizeof(badge), "%d %%", view.percent);
	return badge;
}

int ae_screen_forge_available(void)
{
	return 1;
}

/* ---------- the panel on the right */

static void upper(char *text)
{
	for (; *text; text++)
		if (*text >= 'a' && *text <= 'z')
			*text = (char)(*text - 32);
}

static void side_title(struct ae_flow_screen const *flow, float x, char const *title, char const *line)
{
	float u = ae_flow_u(flow, 1.0f), top = flow->frame.rect.y;

	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 28.0f, x, top + 218.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, title);
	if (line && line[0])
		ae_pve_menus_ui_text(flow, AE_FONT_BODY, 15.0f, x, top + 250.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, line);
}

static void side_body(struct ae_flow_screen const *flow, float x, float width, float y, char const *text, int lines)
{
	float u = ae_flow_u(flow, 1.0f);

	ae_pve_menus_ui_paragraph(flow, 16.0f, 22.0f, x, flow->frame.rect.y + y * u, width, lines, AE_COLOR_TEXT, text);
}

/* the pack's bar: a 6 u track with the accent's fill, its percent and the maps read */
static void side_progress(struct ae_flow_screen const *flow, float x, float width, float y, struct pack_view const *view)
{
	float u = ae_flow_u(flow, 1.0f), top = flow->frame.rect.y;
	char line[48];

	ae_draw_rect(x, top + y * u, width, 6.0f * u, 0.0f, AE_COLOR_TRACK);
	ae_draw_rect(x, top + y * u, width * (float)view->percent / 100.0f, 6.0f * u, 0.0f, AE_COLOR_ACCENT);
	snprintf(line, sizeof(line), "%d %%", view->percent);
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 20.0f, x + width, top + (y + 28.0f) * u, AE_ALIGN_RIGHT, AE_COLOR_TEXT, line);
	ae_forge_doors_pack_progress(view->done, view->wanted, line, sizeof(line));
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, top + (y + 28.0f) * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, line);
}

/* ---------- NEW MAP */

static int pick_first;

static void pick_build(struct ae_pvem_rows *r)
{
	struct ae_flow_screen *flow = &r->flow;
	int each, last = pick_first + PICK_SHOWN < map_count ? pick_first + PICK_SHOWN : map_count;
	short n = 0;

	r->x_hidden = 1;
	r->y_hidden = 1;
	if (map_count == 0)
	{
		flow->rows[n++] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "NO MAPS TO START FROM", NULL,
			"MAPS & MODS gets more.", NULL, 0, 0, 1 };
		flow->count = n;
		return;
	}
	for (each = pick_first; each < last; each++)
		flow->rows[n++] = (struct ae_flow_row){ .kind = AE_FLOW_ROW_OWN, .option = -1, .label = maps[each].title, .value = NULL,
			.help = "Starts Forge on this map, alone: you fly as the Monitor." };
	flow->count = n;
	if (flow->focus >= n)
		flow->focus = (short)(n - 1);
}

static int pick_focused(struct ae_pvem_rows *r)
{
	int at = pick_first + r->flow.focus;

	return map_count > 0 && r->flow.focus >= 0 && at < map_count ? at : -1;
}

/* the list is longer than its rows: the focus at an end moves the window */
static int pick_handle(struct ae_pvem_rows *r, struct ae_event const *event)
{
	int step = event->action == AE_ACTION_PAGE_UP ? -PICK_SHOWN / 2 : event->action == AE_ACTION_PAGE_DOWN ? PICK_SHOWN / 2 : 0;
	int old = pick_first;

	if (event->action == AE_ACTION_UP && r->flow.focus == 0 && pick_first > 0)
		step = -1;
	else if (event->action == AE_ACTION_DOWN && r->flow.focus == r->flow.count - 1 && pick_first + r->flow.count < map_count)
		step = 1;
	else if (!step)
		return 0;
	pick_first += step;
	if (pick_first > map_count - PICK_SHOWN)
		pick_first = map_count - PICK_SHOWN;
	if (pick_first < 0)
		pick_first = 0;
	if (pick_first != old)
		ae_sound_request(AE_SOUND_CURSOR, 0);
	return 1;
}

static void start_forge(struct ae_pvem_rows *r, char const *path)
{
	char text[200];

	if (staged)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		ae_flow_error(&r->flow, "THIS IS A MADE-UP SCENE", "Nothing starts from it.");
		return;
	}
	if (ae_forge_dev_start(path, text, (int)sizeof(text)) <= 0)
	{
		platform_log("ae forge: did not start on %s: %s", path, text);
		ae_sound_request(AE_SOUND_FAILURE, 0);
		ae_flow_error(&r->flow, "FORGE DID NOT START", text);
		return;
	}
	config_write("forge.last_map", path);
	platform_log("ae forge: started on %s", path);
	ae_sound_request(AE_SOUND_FORWARD, 0);
	/* (the menus go with it: the level loads at the next frame) */
	ae_screen_forge_close();
	ae_screen_main_menu_close();
}

static void pick_step(struct ae_pvem_rows *r, int row, int direction)
{
	int at = pick_focused(r);

	(void)row;
	(void)direction;
	if (at >= 0)
		start_forge(r, maps[at].path);
}

static const char *pick_a_word(struct ae_pvem_rows *r, int row)
{
	(void)r;
	(void)row;
	return "Forge alone";
}

static void pick_side(struct ae_pvem_rows *r, float x, float width)
{
	struct ae_flow_screen const *flow = &r->flow;
	float u = ae_flow_u(flow, 1.0f), top = flow->frame.rect.y;
	int at = pick_focused(r);
	char line[80];

	ae_pve_menus_ui_plate(flow, x, width, 176.0f, 470.0f);
	if (at < 0)
	{
		side_title(flow, x, "NEW MAP", NULL);
		side_body(flow, x, width, 262.0f, "No maps to start from. MAPS & MODS gets more.", 3);
		return;
	}
	snprintf(line, sizeof(line), "MULTIPLAYER \xE2\x80\xA2 %d OF %d", at + 1, map_count);
	side_title(flow, x, maps[at].title, line);
	ae_draw_rect(x, top + 272.0f * u, width, u, 0.0f, AE_COLOR_RULE);
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, top + 306.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, "STARTS AS");
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 20.0f, x + width, top + 306.0f * u, AE_ALIGN_RIGHT, AE_COLOR_TEXT, "THE MAP'S OWN OBJECTS");
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, top + 344.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, "ONLINE");
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 20.0f, x + width, top + 344.0f * u, AE_ALIGN_RIGHT, AE_COLOR_MUTED, "NOT YET");
	side_body(flow, x, width, 392.0f, "You fly as the Monitor: pick an object up, turn it, copy it, put it down. Hosting a Forge game with "
		"others comes after the editing.", 4);
}

static int pick_open(void)
{
	if (ae_ui_holds(&pick_screen))
		ae_pve_menus_rows_close(&pick_screen);
	memset(&pick_screen, 0, sizeof(pick_screen));
	pick_screen.name = "forge new map";
	pick_screen.title = "NEW FORGE MAP";
	snprintf(pick_screen.crumb, sizeof(pick_screen.crumb), "FORGE");
	pick_screen.build = pick_build;
	pick_screen.step = pick_step;
	pick_screen.a_word = pick_a_word;
	pick_screen.side = pick_side;
	pick_screen.handle_first = pick_handle;
	pick_screen.hit_base = PICK_HIT;
	pick_screen.backing = 1;
	pick_first = 0;
	maps_read();
	return ae_pve_menus_rows_open(&pick_screen);
}

/* ---------- the hub */

/* the hub's rows as drawn: what each is (AE_FORGE_HUB_*), -1 for a heading */
static int hub_row_kind[AE_FLOW_ROWS];
static char hub_values[AE_FLOW_ROWS][32], hub_reasons[AE_FLOW_ROWS][32];
static char last_path[96];
static int last_listed, maps_listed, saved_listed;

static void hub_facts_read(void)
{
	int each;

	maps_read();
	maps_listed = map_count;
	saved_listed = staged ? 0 : ae_forge_maps_scan();
	last_path[0] = 0;
	last_listed = 0;
	if (!config_text("forge.last_map", last_path, sizeof(last_path)))
		last_path[0] = 0;
	for (each = 0; last_path[0] && each < map_count; each++)
		if (!_stricmp(maps[each].path, last_path))
			last_listed = 1;
}

static void hub_build(struct ae_pvem_rows *r)
{
	struct ae_flow_screen *flow = &r->flow;
	struct pack_view view;
	short n = 0;
	int row;

	pack_read(&view);
	r->x_hidden = 1;
	r->y_hidden = 1;
	for (row = 0; row < AE_FORGE_HUB_ROWS; row++)
	{
		char title[48];
		char *value = hub_values[n], *reason = hub_reasons[n];
		int waits = ae_forge_doors_row_needs_pack(row) && view.state != AE_FORGE_PACK_READY;

		if (row == AE_FORGE_HUB_CONTINUE && !last_listed)
			continue;
		if (row == AE_FORGE_HUB_ITEM_SETS)
		{
			hub_row_kind[n] = -1;
			flow->rows[n++] = (struct ae_flow_row){ AE_FLOW_ROW_HEADER, -1, "SETUP", NULL, NULL, NULL };
			value = hub_values[n];
			reason = hub_reasons[n];
		}
		value[0] = reason[0] = 0;
		ae_forge_doors_pack_reason(view.state, view.percent, reason, sizeof(hub_reasons[0]));
		hub_row_kind[n] = row;
		flow->rows[n] = (struct ae_flow_row){ .kind = AE_FLOW_ROW_OWN, .option = -1, .subscreen = 1 };
		switch (row)
		{
		case AE_FORGE_HUB_CONTINUE:
			ae_forge_doors_map_title(last_path, title, sizeof(title));
			snprintf(value, sizeof(hub_values[0]), "%s", title);
			flow->rows[n].label = "CONTINUE";
			flow->rows[n].help = "Starts Forge again on the map you started it on last.";
			break;
		case AE_FORGE_HUB_NEW_MAP:
			snprintf(value, sizeof(hub_values[0]), "%d MAP%s", maps_listed, maps_listed == 1 ? "" : "S");
			flow->rows[n].label = "NEW MAP";
			flow->rows[n].help = "Pick a multiplayer map and start Forge on it, alone.";
			break;
		case AE_FORGE_HUB_MY_MAPS:
			waits = 0;
			snprintf(value, sizeof(hub_values[0]), "%d MAP%s", saved_listed, saved_listed == 1 ? "" : "S");
			flow->rows[n].label = "MY MAPS";
			flow->rows[n].help = "The maps you saved and the ones others shared with you: edit, play, rename, copy or delete.";
			break;
		case AE_FORGE_HUB_ITEM_SETS:
			flow->rows[n].label = "ITEM SETS";
			flow->rows[n].help = "The sets of items taken out of maps. Their items join Forge's palette.";
			break;
		default:
			snprintf(value, sizeof(hub_values[0]), "FORGE KEYS");
			flow->rows[n].label = "CONTROLS";
			flow->rows[n].help = "The keys and buttons Forge uses while you fly as the Monitor.";
			break;
		}
		flow->rows[n].value = value;
		if (waits)
			flow->rows[n].reason = reason;
		n++;
	}
	flow->count = n;
}

static void hub_step(struct ae_pvem_rows *r, int row, int direction)
{
	(void)direction;
	if (row < 0 || row >= r->flow.count)
		return;
	switch (hub_row_kind[row])
	{
	case AE_FORGE_HUB_CONTINUE:
		start_forge(r, last_path);
		break;
	case AE_FORGE_HUB_NEW_MAP:
		if (pick_open())
			ae_sound_request(AE_SOUND_FORWARD, 0);
		break;
	case AE_FORGE_HUB_MY_MAPS:
		if (mymaps_open())
			ae_sound_request(AE_SOUND_FORWARD, 0);
		break;
	case AE_FORGE_HUB_ITEM_SETS:
		/* (MAPS & MODS on its ITEM SETS page: B leaves both and comes back here) */
		if (staged)
			ae_flow_error(&r->flow, "THIS IS A MADE-UP SCENE", "ITEM SETS opens in the real hub.");
		else if (ae_screen_maps_hub_open(AE_HUB_TAB_ITEM_SETS))
			ae_sound_request(AE_SOUND_FORWARD, 0);
		break;
	case AE_FORGE_HUB_CONTROLS:
		if (keys_open())
			ae_sound_request(AE_SOUND_FORWARD, 0);
		break;
	default:
		break;
	}
}

static const char *hub_a_word(struct ae_pvem_rows *r, int row)
{
	(void)r;
	(void)row;
	return "Open";
}

static void hub_side(struct ae_pvem_rows *r, float x, float width)
{
	struct ae_flow_screen const *flow = &r->flow;
	struct pack_view view;
	int kind = flow->focus >= 0 && flow->focus < flow->count ? hub_row_kind[flow->focus] : -1;
	char title[48];

	pack_read(&view);
	ae_pve_menus_ui_plate(flow, x, width, 176.0f, 470.0f);
	if (view.state == AE_FORGE_PACK_BUILDING && ae_forge_doors_row_needs_pack(kind))
	{
		side_title(flow, x, "Getting Forge ready", "BUILT ONCE FROM THE GAME'S MAPS");
		side_progress(flow, x, width, 300.0f, &view);
		side_body(flow, x, width, 372.0f, "Forge's palette is built from the maps of the game on this PC. You can leave this screen: "
			"it carries on.", 4);
		return;
	}
	if (view.state == AE_FORGE_PACK_NEEDS_MAPS && ae_forge_doors_row_needs_pack(kind))
	{
		side_title(flow, x, "NEEDS THE GAME'S MAPS", NULL);
		side_body(flow, x, width, 290.0f, "Forge needs these maps of the game to build its palette.", 3);
		if (view.reason[0])
			side_body(flow, x, width, 370.0f, view.reason, 3);
		return;
	}
	switch (kind)
	{
	case AE_FORGE_HUB_CONTINUE:
		ae_forge_doors_map_title(last_path, title, sizeof(title));
		side_title(flow, x, title, "THE MAP YOU STARTED FORGE ON LAST");
		side_body(flow, x, width, 306.0f, "Forge starts again on this map, alone. What you saved is under MY MAPS once saving is in.", 4);
		break;
	case AE_FORGE_HUB_NEW_MAP:
		side_title(flow, x, "NEW MAP", "MULTIPLAYER MAPS");
		side_body(flow, x, width, 306.0f, "Pick a map and fly over it as the Monitor. The map's own objects are yours to move, copy and "
			"delete, and the palette adds more.", 5);
		break;
	case AE_FORGE_HUB_MY_MAPS:
		side_title(flow, x, "MY MAPS", "SAVED IN FORGE");
		side_body(flow, x, width, 306.0f, "The maps you saved in Forge, and the ones others shared with you. Pick one to edit it, play it, rename, copy or delete it.", 5);
		break;
	case AE_FORGE_HUB_ITEM_SETS:
		side_title(flow, x, "ITEM SETS", "FROM MAPS & MODS");
		side_body(flow, x, width, 306.0f, "A set holds items taken out of maps. Its items join Forge's palette in a folder of their own.", 4);
		break;
	default:
		side_title(flow, x, "FORGE KEYS", "THE MONITOR'S CONTROLS");
		side_body(flow, x, width, 306.0f, "Twelve actions and two pad speeds, kept in the file Forge itself reads.", 4);
		break;
	}
}

static void hub_prepare(void)
{
	struct pack_view view;

	hub_facts_read();
	pack_read(&view);
	if (staged_pack < 0 && view.state != AE_FORGE_PACK_READY && !tag_pack_busy())
	{
		tag_pack_request(FORGE_PACK);
		platform_log("ae forge: Forge's tag pack asked for");
	}
}

static int hub_open(void)
{
	if (ae_ui_holds(&hub_screen))
		ae_pve_menus_rows_close(&hub_screen);
	memset(&hub_screen, 0, sizeof(hub_screen));
	hub_screen.name = "forge";
	hub_screen.title = "FORGE";
	snprintf(hub_screen.crumb, sizeof(hub_screen.crumb), AE_TESTING_CRUMB "MAIN MENU");
	hub_screen.build = hub_build;
	hub_screen.step = hub_step;
	hub_screen.a_word = hub_a_word;
	hub_screen.side = hub_side;
	hub_screen.hit_base = HUB_HIT;
	hub_screen.backing = 1;
	hub_prepare();
	return ae_pve_menus_rows_open(&hub_screen);
}

/* ---------- FORGE KEYS */

enum { KEYS_ROW_PAD = AE_FORGE_KEYS_ACTIONS };

static struct
{
	int capturing, action, column;
	unsigned long started, held;
	char note[160];
	int note_row;
	int restore;
	char values[AE_FLOW_ROWS][64];
} K;

/* row kinds in the flow: an action's number, or KEYS_ROW_PAD + 0 / 1 for the pad speeds, -1 for a heading */
static int keys_row_kind[AE_FLOW_ROWS];

static void keys_note(char const *text)
{
	snprintf(K.note, sizeof(K.note), "%s", text);
}

static void keys_save(void)
{
	if (!staged)
		ae_forge_binds_save();
}

static void keys_capture_end(void)
{
	K.capturing = 0;
	platform_binding_capture_cancel();
}

/* a key or a button put on an action's cell: any other action's in the same column loses it, as Forge's own screen does */
static void keys_bind(int action, int column, int value)
{
	int each, vk, gp, other_vk, other_gp, from = -1;
	char key[32], pad[32], note[160];

	for (each = 0; each < AE_FORGE_KEYS_ACTIONS; each++)
	{
		if (each == action || !ae_forge_binds_get(each, &other_vk, &other_gp))
			continue;
		if (value && (column == 0 ? other_vk == value : other_gp == value))
		{
			ae_forge_binds_set(each, column == 0 ? 0 : other_vk, column == 0 ? other_gp : 0);
			from = each;
		}
	}
	if (!ae_forge_binds_get(action, &vk, &gp))
		return;
	ae_forge_binds_set(action, column == 0 ? value : vk, column == 0 ? gp : value);
	keys_save();
	K.note[0] = 0;
	if (from >= 0)
	{
		ae_forge_binds_get(action, &vk, &gp);
		ae_forge_bind_names(vk, gp, key, (int)sizeof(key), pad, (int)sizeof(pad));
		snprintf(note, sizeof(note), "%s", column == 0 ? key : pad);
		ae_forge_keys_taken_text(note, ae_forge_keys_label(from), K.note, sizeof(K.note));
		K.note_row = action;
	}
	platform_log("ae forge keys: %s %s = %d%s", ae_forge_keys_label(action), column == 0 ? "key" : "button", value,
		from >= 0 ? " (moved)" : "");
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

static void keys_capture_begin(int action)
{
	K.capturing = 1;
	K.action = action;
	K.started = system_milliseconds();
	K.held = 0;
	platform_gamepad_buttons(0, &K.held);
	K.note[0] = 0;
	platform_binding_capture_begin();
	platform_log("ae forge keys: press a key or a button for %s", ae_forge_keys_label(action));
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

/* the capture, each frame: a key or mouse button goes to the KEY cell, a pad button to the CONTROLLER cell; Escape or the
timeout cancels, Delete and Backspace clear the cell the focus is on */
static void keys_capture_poll(void)
{
	unsigned long buttons = 0;
	int input = -1, result, button;
	char name[32];

	if (!K.capturing)
		return;
	platform_gamepad_buttons(0, &buttons);
	K.held &= buttons;
	if (system_milliseconds() - K.started > CAPTURE_MS)
	{
		keys_capture_end();
		platform_log("ae forge keys: nothing pressed");
		ae_sound_request(AE_SOUND_BACK, 0);
		return;
	}
	for (button = 0; button < 16; button++)
	{
		if ((buttons & (1ul << button)) && !(K.held & (1ul << button)))
		{
			keys_capture_end();
			keys_bind(K.action, 1, ae_forge_keys_gp_of_button(button));
			return;
		}
	}
	result = platform_binding_capture_poll(&input);
	if (!result)
		return;
	K.capturing = 0;
	if (result == 3)
	{
		platform_log("ae forge keys: cancelled");
		ae_sound_request(AE_SOUND_BACK, 0);
		return;
	}
	if (result == 2)
	{
		keys_bind(K.action, K.column, 0);
		return;
	}
	halo_input_name(input, name, sizeof(name));
	{
		int vk = input >= 0 && input < 512 ? ae_forge_keys_vk_of_scancode(input) :
			!_stricmp(name, "Mouse Left") ? ae_forge_keys_vk_of_mouse(1) : !_stricmp(name, "Mouse Middle") ? ae_forge_keys_vk_of_mouse(2) :
			!_stricmp(name, "Mouse Right") ? ae_forge_keys_vk_of_mouse(3) : 0;

		if (!vk)
		{
			keys_note("Forge cannot use that key. Pick another.");
			ae_sound_request(AE_SOUND_FAILURE, 0);
			return;
		}
		keys_bind(K.action, 0, vk);
	}
}

static void keys_build(struct ae_pvem_rows *r)
{
	struct ae_flow_screen *flow = &r->flow;
	short n = 0;
	int action;

	keys_capture_poll();
	r->x_hidden = 0;
	r->y_hidden = 0;
	flow->rows[n] = (struct ae_flow_row){ AE_FLOW_ROW_HEADER, -1, "KEY  /  CONTROLLER", NULL, NULL, NULL };
	keys_row_kind[n++] = -1;
	for (action = 0; action < AE_FORGE_KEYS_ACTIONS; action++)
	{
		int vk = 0, gp = 0, focused = flow->focus == n;
		char key[32], pad[32], *value = K.values[n];

		ae_forge_binds_get(action, &vk, &gp);
		ae_forge_bind_names(vk, gp, key, (int)sizeof(key), pad, (int)sizeof(pad));
		if (K.capturing && K.action == action)
			snprintf(value, sizeof(K.values[0]), "PRESS A KEY OR A BUTTON");
		else if (focused)
			snprintf(value, sizeof(K.values[0]), "%s%s%s   %s%s%s", K.column == 0 ? "[ " : "", key, K.column == 0 ? " ]" : "",
				K.column == 1 ? "[ " : "", pad, K.column == 1 ? " ]" : "");
		else
			snprintf(value, sizeof(K.values[0]), "%s   %s", key, pad);
		flow->rows[n] = (struct ae_flow_row){ .kind = AE_FLOW_ROW_OWN, .option = -1, .label = ae_forge_keys_label(action),
			.value = value, .help = ae_forge_keys_help(action), .status = 0 };
		keys_row_kind[n++] = action;
	}
	flow->rows[n] = (struct ae_flow_row){ AE_FLOW_ROW_HEADER, -1, "FORGE PAD", NULL, NULL, NULL };
	keys_row_kind[n++] = -1;
	for (action = 0; action < 2; action++)
	{
		char *value = K.values[n];

		snprintf(value, sizeof(K.values[0]), "%d %%", ae_forge_keys_pad_percent(ae_forge_pad_get(action)));
		flow->rows[n] = (struct ae_flow_row){ .kind = AE_FLOW_ROW_OWN, .option = -1,
			.label = action == 0 ? "PAD LOOK SPEED" : "PAD FLY SPEED", .value = value,
			.help = action == 0 ? "How fast the right stick turns the Monitor. Left and right change it." :
				"How fast the Monitor flies with the pad. Left and right change it." };
		keys_row_kind[n++] = KEYS_ROW_PAD + action;
	}
	flow->count = n;
}

static int keys_pad_step(int which, int direction)
{
	float now = ae_forge_pad_get(which), next = ae_forge_keys_pad_step(now, direction);

	if (next == now && direction)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return 1;
	}
	ae_forge_pad_set(which, next);
	keys_save();
	platform_log("ae forge keys: pad %s speed %d %%", which ? "fly" : "look", ae_forge_keys_pad_percent(next));
	ae_sound_request(AE_SOUND_CURSOR, 0);
	return 1;
}

static void keys_restore_picked(short choice, void *context)
{
	(void)context;
	if (choice != 1)
		return;
	ae_forge_binds_defaults();
	keys_save();
	K.note[0] = 0;
	platform_log("ae forge keys: restored");
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void keys_dialog_open(struct ae_pvem_rows *r)
{
	struct ae_dialog_spec spec;

	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = "RESTORE FORGE KEYS?";
	spec.body = "Every Forge key, button and pad speed goes back to what Forge starts with.";
	spec.choices[0] = "CANCEL";
	spec.choices[1] = "RESTORE";
	spec.choice_count = 2;
	spec.safe_choice = 0;
	spec.cancel_choice = 0;
	spec.has_cancel = 1;
	spec.density = r->flow.density;
	spec.bounds = r->flow.frame.rect;
	spec.picked = keys_restore_picked;
	spec.context = r;
	ae_dialog_open(&spec, AE_OWNER_ANY);
}

static void keys_restore_asked(struct ae_pvem_rows *r)
{
	r->dialog_pending = 1;
}

static void keys_clear_asked(struct ae_pvem_rows *r)
{
	int kind = r->flow.focus >= 0 && r->flow.focus < r->flow.count ? keys_row_kind[r->flow.focus] : -1;

	if (kind < 0 || kind >= AE_FORGE_KEYS_ACTIONS)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	keys_bind(kind, K.column, 0);
}

static int keys_handle(struct ae_pvem_rows *r, struct ae_event const *event)
{
	int kind = r->flow.focus >= 0 && r->flow.focus < r->flow.count ? keys_row_kind[r->flow.focus] : -1;

	/* (waiting for a key or a button: the pad's buttons are the capture's, Escape cancels it) */
	if (K.capturing)
		return 1;
	if (event->action == AE_ACTION_LEFT || event->action == AE_ACTION_RIGHT)
	{
		int direction = event->action == AE_ACTION_RIGHT ? 1 : -1;

		if (kind >= KEYS_ROW_PAD)
			return keys_pad_step(kind - KEYS_ROW_PAD, direction);
		if (kind >= 0 && K.column != (direction > 0))
		{
			K.column = direction > 0;
			ae_sound_request(AE_SOUND_CURSOR, 0);
		}
		return 1;
	}
	return 0;
}

static void keys_step(struct ae_pvem_rows *r, int row, int direction)
{
	int kind = row >= 0 && row < r->flow.count ? keys_row_kind[row] : -1;

	(void)direction;
	if (K.capturing)
		return;
	if (kind >= KEYS_ROW_PAD)
		keys_pad_step(kind - KEYS_ROW_PAD, 1);
	else if (kind >= 0)
		keys_capture_begin(kind);
}

static const char *keys_a_word(struct ae_pvem_rows *r, int row)
{
	int kind = row >= 0 && row < r->flow.count ? keys_row_kind[row] : -1;

	return kind >= KEYS_ROW_PAD ? "Faster" : "Change";
}

static void keys_side(struct ae_pvem_rows *r, float x, float width)
{
	struct ae_flow_screen const *flow = &r->flow;
	int kind = flow->focus >= 0 && flow->focus < flow->count ? keys_row_kind[flow->focus] : -1;

	ae_pve_menus_ui_plate(flow, x, width, 176.0f, 470.0f);
	side_title(flow, x, "FORGE KEYS", "IN A FORGE GAME ONLY");
	if (K.capturing)
		side_body(flow, x, width, 306.0f, "Press the key or the button now. Escape cancels; Delete clears the cell you are on.", 4);
	else if (K.note[0] && K.note_row == (kind >= 0 && kind < AE_FORGE_KEYS_ACTIONS ? kind : -1))
		side_body(flow, x, width, 306.0f, K.note, 3);
	else if (kind >= 0 && kind < AE_FORGE_KEYS_ACTIONS)
		side_body(flow, x, width, 306.0f, "A starts the capture for the cell you are on; left and right pick the key or the "
			"controller button. X clears it.", 4);
	else
		side_body(flow, x, width, 306.0f, "Left and right change the speed in steps of 25 %.", 3);
	side_body(flow, x, width, 410.0f, "Some actions share a button on purpose: Forge tells them apart by whether you hold something.", 3);
}

static int keys_open(void)
{
	if (ae_ui_holds(&keys_screen))
		ae_pve_menus_rows_close(&keys_screen);
	memset(&keys_screen, 0, sizeof(keys_screen));
	memset(&K, 0, sizeof(K));
	K.note_row = -1;
	ae_forge_binds_load();
	keys_screen.name = "forge keys";
	keys_screen.title = "FORGE KEYS";
	snprintf(keys_screen.crumb, sizeof(keys_screen.crumb), "FORGE");
	keys_screen.build = keys_build;
	keys_screen.step = keys_step;
	keys_screen.a_word = keys_a_word;
	keys_screen.x_action = keys_clear_asked;
	keys_screen.x_word = "Clear";
	keys_screen.y_action = keys_restore_asked;
	keys_screen.y_word = "Restore";
	keys_screen.side = keys_side;
	keys_screen.dialog_open = keys_dialog_open;
	keys_screen.handle_first = keys_handle;
	keys_screen.hit_base = KEYS_HIT;
	keys_screen.backing = 1;
	return ae_pve_menus_rows_open(&keys_screen);
}


/* ---------- MY MAPS (the maps Forge saved: SHOW and SORT, the list, a card with the map's picture) */

enum { MM_HEAD = 3, MM_SHOWN = 14, MM_WORDS = 3, MM_DIALOG_OPTIONS = 0, MM_DIALOG_DELETE = 1, MM_NAME_RENAME = 0, MM_NAME_COPY = 1 };
enum { MM_EDIT, MM_PLAY, MM_RENAME, MM_COPY, MM_DELETE };

static int mm_show, mm_sort, mm_first, mm_count;
static char mm_file[64];              /* the map the dialog or the name entry is about */
static int mm_dialog_kind;
static char mm_notice[160];
static int mm_notice_bad;
static struct ae_text mm_text;
static struct ae_field_edit mm_edit;
static int mm_naming, mm_keyboard, mm_name_mode, mm_device;
static struct ae_rect mm_well;
static unsigned long mm_caret;
static char mm_values[AE_FLOW_ROWS][96];

static void mm_read(void)
{
	ae_forge_maps_scan();
	mm_count = ae_forge_maps_view(mm_show, mm_sort);
	if (mm_first > mm_count - MM_SHOWN)
		mm_first = mm_count - MM_SHOWN;
	if (mm_first < 0)
		mm_first = 0;
}

static void mm_say(int bad, char const *text)
{
	snprintf(mm_notice, sizeof(mm_notice), "%s", text);
	mm_notice_bad = bad;
}

static int mm_place(struct ae_pvem_rows const *r)
{
	int place = mm_first + (r->flow.focus - MM_HEAD);

	return mm_count > 0 && r->flow.focus >= MM_HEAD && place < mm_count ? place : -1;
}

static void mm_build(struct ae_pvem_rows *r)
{
	static char const *const shows[MM_WORDS] = { "ALL", "MADE HERE", "DOWNLOADED" };
	static char const *const sorts[MM_WORDS] = { "LAST SAVED", "NAME", "BASE MAP" };
	static char head[80];
	struct ae_flow_screen *flow = &r->flow;
	short n = 0;
	int each, last;

	if (mm_naming && mm_edit.active && !ae_field_edit_keys(&mm_edit))
		ae_sound_request(AE_SOUND_FAILURE, 0);
	r->x_hidden = r->y_hidden = 1;
	if (ae_forge_maps_total() == 0)
	{
		flow->rows[n++] = (struct ae_flow_row){ .kind = AE_FLOW_ROW_OWN, .option = -1, .label = "NEW MAP", .subscreen = 1,
			.help = "Pick a multiplayer map and start Forge on it, alone." };
		flow->count = n;
		flow->focus = 0;
		return;
	}
	flow->rows[n++] = (struct ae_flow_row){ .kind = AE_FLOW_ROW_OWN, .option = -1, .label = "SHOW", .value = shows[mm_show],
		.help = "Left and right: all the maps, the ones you made here, or the ones downloaded." };
	flow->rows[n++] = (struct ae_flow_row){ .kind = AE_FLOW_ROW_OWN, .option = -1, .label = "SORT", .value = sorts[mm_sort],
		.help = "Left and right: the last saved first, by name, or by the map each was made on." };
	snprintf(head, sizeof(head), "%d MAP%s%s", mm_count, mm_count == 1 ? "" : "S",
		ae_forge_maps_downloaded() ? "  \xE2\x80\xA2  SOME DOWNLOADED" : "");
	flow->rows[n++] = (struct ae_flow_row){ AE_FLOW_ROW_HEADER, -1, head, NULL, NULL, NULL };
	last = mm_first + MM_SHOWN < mm_count ? mm_first + MM_SHOWN : mm_count;
	for (each = mm_first; each < last; each++)
	{
		struct ae_forge_map const *map = ae_forge_maps_at(each);
		char *value = mm_values[n];

		if (!map)
			break;
		upper_ascii_text(map->name, value, (int)sizeof(mm_values[0]));
		flow->rows[n] = (struct ae_flow_row){ .kind = AE_FLOW_ROW_OWN, .option = -1, .label = value,
			.help = map->readable ? "A edits it in Forge. X plays it. Y has rename, copy and delete." :
				"This map's saved state can't be read: its file is damaged or not one this game plays." };
		if (!map->readable)
			flow->rows[n].reason = "CAN'T BE READ";
		else
		{
			snprintf(mm_values[n] + 48, sizeof(mm_values[0]) - 48, "%s%s", map->base[0] ? map->base : "",
				map->downloaded ? (map->base[0] ? "  \xE2\x80\xA2  DOWNLOADED" : "DOWNLOADED") : "");
			flow->rows[n].value = mm_values[n] + 48;
		}
		n++;
	}
	flow->count = n;
	if (flow->focus >= n)
		flow->focus = (short)(n - 1);
	if (flow->focus < 0)
		flow->focus = 0;
	{
		struct ae_forge_map const *map = ae_forge_maps_at(mm_place(r));

		r->x_hidden = r->y_hidden = !map;
		if (map && !map->readable)
			r->x_hidden = 1;
	}
}

static void mm_close_all(void)
{
	ae_screen_forge_close();
	ae_screen_main_menu_close();
}

static void mm_edit_map(struct ae_pvem_rows *r, struct ae_forge_map const *map)
{
	char text[200];

	if (!map->readable || !map->level[0])
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		ae_flow_error(&r->flow, "THIS MAP CAN'T BE READ", "Its file is damaged or not one this game plays.");
		return;
	}
	if (ae_forge_dev_start(map->level, text, (int)sizeof(text)) <= 0)
	{
		platform_log("ae forge: did not start on %s: %s", map->level, text);
		ae_sound_request(AE_SOUND_FAILURE, 0);
		ae_flow_error(&r->flow, "FORGE DID NOT START", text);
		return;
	}
	ae_forge_dev_resave(map->file);
	platform_log("ae forge: editing the saved map %s", map->file);
	ae_sound_request(AE_SOUND_FORWARD, 0);
	mm_close_all();
}

static void mm_play_map(struct ae_pvem_rows *r, struct ae_forge_map const *map)
{
	if (!map->readable || !map->level[0] || !ae_screen_create_game_open_on_map(map->level))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		ae_flow_error(&r->flow, "THIS MAP IS NOT IN THE GAME'S LIST", "CREATE GAME could not pick it: leave and open MY MAPS again.");
		return;
	}
	platform_log("ae forge: playing the saved map %s", map->level);
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void mm_step(struct ae_pvem_rows *r, int row, int direction)
{
	struct ae_forge_map *map;

	if (ae_forge_maps_total() == 0)
	{
		if (pick_open())
			ae_sound_request(AE_SOUND_FORWARD, 0);
		return;
	}
	if (row == 0 || row == 1)
	{
		int step = direction < 0 ? MM_WORDS - 1 : 1;

		if (row == 0)
			mm_show = (mm_show + step) % MM_WORDS;
		else
			mm_sort = (mm_sort + step) % MM_WORDS;
		mm_first = 0;
		mm_count = ae_forge_maps_view(mm_show, mm_sort);
		ae_sound_request(AE_SOUND_CURSOR, 0);
		return;
	}
	map = ae_forge_maps_at(mm_place(r));
	if (map && direction > 0)
		mm_edit_map(r, map);
}

static const char *mm_a_word(struct ae_pvem_rows *r, int row)
{
	(void)r;
	return row == 0 || row == 1 ? "Change" : "Edit";
}

static void mm_x_action(struct ae_pvem_rows *r)
{
	struct ae_forge_map *map = ae_forge_maps_at(mm_place(r));

	if (map)
		mm_play_map(r, map);
}

static void mm_y_action(struct ae_pvem_rows *r)
{
	struct ae_forge_map const *map = ae_forge_maps_at(mm_place(r));

	if (!map)
		return;
	snprintf(mm_file, sizeof(mm_file), "%s", map->file);
	mm_dialog_kind = MM_DIALOG_OPTIONS;
	r->dialog_pending = 1;
}

/* ---- rename and make a copy: the name typed in the card's field, the keyboard for a pad */

static void mm_typing_done(int keep, void *context)
{
	(void)keep;
	(void)context;
	mm_naming = 0;
}

static void mm_name_cancel(void)
{
	if (mm_naming)
		ae_field_edit_end(&mm_edit, 0);
	mm_naming = mm_keyboard = 0;
	mm_name_mode = -1;
	ae_sound_request(AE_SOUND_BACK, 0);
}

static void mm_name_begin(int device);

static void mm_name_commit(void)
{
	char typed[64], name[64], text[160];
	int done;

	snprintf(typed, sizeof(typed), "%s", mm_text.text);
	if (mm_naming)
		ae_field_edit_end(&mm_edit, 1);
	mm_naming = 0;
	ae_forge_maps_name_clean(typed, name, (int)sizeof(name));
	done = mm_name_mode == MM_NAME_COPY ? ae_forge_maps_copy(mm_file, name, text, (int)sizeof(text)) :
		ae_forge_maps_rename(mm_file, name, text, (int)sizeof(text));
	if (!done)
	{
		mm_say(1, text);
		ae_sound_request(AE_SOUND_FAILURE, 0);
		mm_name_begin(mm_device);
		return;
	}
	mm_say(0, mm_name_mode == MM_NAME_COPY ? "The copy is made." : "The map is renamed.");
	mm_name_mode = -1;
	mm_read();
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void mm_keyboard_done(int keep, void *context)
{
	(void)context;
	mm_keyboard = 0;
	if (mm_edit.active && mm_edit.text == &mm_text)
	{
		mm_naming = 1;
		return;
	}
	if (keep)
		mm_name_commit();
	else
		mm_name_cancel();
}

static void mm_name_begin(int device)
{
	if (mm_name_mode < 0)
		return;
	mm_edit.done = mm_typing_done;
	mm_edit.context = NULL;
	mm_edit.holder = &mm_text;
	if (device == AE_DEVICE_KEYBOARD_MOUSE)
	{
		ae_field_edit_begin(&mm_edit, &mm_text);
		ae_text_select_all(&mm_text);
		mm_naming = 1;
		mm_caret = ae_motion_now();
	}
	else
	{
		struct ae_keyboard_spec spec;

		memset(&spec, 0, sizeof(spec));
		spec.text = &mm_text;
		spec.field = mm_well;
		spec.bounds = mymaps_screen.flow.frame.rect;
		spec.density = mymaps_screen.flow.density;
		spec.done = mm_keyboard_done;
		spec.edit = &mm_edit;
		if (ae_keyboard_open(&spec, AE_OWNER_ANY))
			mm_keyboard = 1;
	}
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void mm_name_start(int mode, int device)
{
	char initial[64];

	mm_name_mode = mode;
	snprintf(initial, sizeof(initial), "%s%s", mm_file, mode == MM_NAME_COPY ? "_copy" : "");
	ae_text_init(&mm_text, initial, AE_FORGE_MAP_NAME_MAX, 0);
	mm_notice[0] = 0;
	mm_name_begin(device);
}

static void mm_delete_picked(short choice, void *context)
{
	char text[160];

	(void)context;
	if (choice != 1)
		return;
	if (ae_forge_maps_delete(mm_file, text, (int)sizeof(text)))
		mm_say(0, "The map is deleted.");
	else
		mm_say(1, text);
	mm_read();
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void mm_options_picked(short choice, void *context)
{
	struct ae_pvem_rows *r = context;
	struct ae_forge_map *map = ae_forge_maps_get(ae_forge_maps_find(mm_file));

	if (!map)
		return;
	switch (choice)
	{
	case MM_EDIT:
		mm_edit_map(r, map);
		break;
	case MM_PLAY:
		mm_play_map(r, map);
		break;
	case MM_RENAME:
		mm_name_start(MM_NAME_RENAME, mm_device);
		break;
	case MM_COPY:
		mm_name_start(MM_NAME_COPY, mm_device);
		break;
	case MM_DELETE:
		mm_dialog_kind = MM_DIALOG_DELETE;
		r->dialog_pending = 1;
		break;
	default:
		break;
	}
}

static void mm_dialog_open(struct ae_pvem_rows *r)
{
	struct ae_dialog_spec spec;
	struct ae_forge_map const *map = ae_forge_maps_get(ae_forge_maps_find(mm_file));
	char title[120], name[64];

	if (!map)
		return;
	upper_ascii_text(map->name, name, (int)sizeof(name));
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.density = r->flow.density;
	spec.bounds = r->flow.frame.rect;
	spec.context = r;
	spec.has_cancel = 1;
	if (mm_dialog_kind == MM_DIALOG_DELETE)
	{
		snprintf(title, sizeof(title), "DELETE %s?", name);
		spec.title = title;
		spec.body = "Its map file is deleted from this PC. Players who have a copy keep it. This can't be undone.";
		spec.choices[0] = "CANCEL";
		spec.choices[1] = "DELETE";
		spec.choice_count = 2;
		spec.destructive_choices = 2u;
		spec.picked = mm_delete_picked;
	}
	else
	{
		spec.title = name;
		spec.body = map->downloaded ? "A downloaded map keeps its name: it can be edited, played or copied." :
			"Edit it in Forge, play it, or give it another name.";
		spec.choices[0] = "EDIT IN FORGE";
		spec.choices[1] = "PLAY IT";
		spec.choices[2] = "RENAME";
		spec.choices[3] = "MAKE A COPY";
		spec.choices[4] = "DELETE";
		spec.choice_count = 5;
		spec.destructive_choices = 16u;
		spec.cancel_choice = -1;
		spec.picked = mm_options_picked;
	}
	ae_dialog_open(&spec, AE_OWNER_ANY);
}

static int mm_handle(struct ae_pvem_rows *r, struct ae_event const *event)
{
	int old = mm_first, step = 0;

	mm_device = event->device;
	if (mm_name_mode >= 0 && (mm_naming || mm_keyboard))
	{
		if (event->action == AE_ACTION_ACCEPT || event->action == AE_ACTION_START)
		{
			if (mm_naming)
				mm_name_commit();
		}
		else if (event->action == AE_ACTION_BACK)
			mm_name_cancel();
		return 1;
	}
	if (ae_forge_maps_total() == 0)
		return 0;
	if (event->action == AE_ACTION_PAGE_UP)
		step = -MM_SHOWN / 2;
	else if (event->action == AE_ACTION_PAGE_DOWN)
		step = MM_SHOWN / 2;
	else if (event->action == AE_ACTION_UP && r->flow.focus == MM_HEAD && mm_first > 0)
		step = -1;
	else if (event->action == AE_ACTION_DOWN && r->flow.focus == r->flow.count - 1 && mm_first + r->flow.count - MM_HEAD < mm_count)
		step = 1;
	else
		return 0;
	mm_first += step;
	if (mm_first > mm_count - MM_SHOWN)
		mm_first = mm_count - MM_SHOWN;
	if (mm_first < 0)
		mm_first = 0;
	if (mm_first != old)
		ae_sound_request(AE_SOUND_CURSOR, 0);
	return 1;
}

static void mm_time_text(unsigned long seconds, char *text, int size)
{
	time_t when = (time_t)seconds;
	struct tm *local = seconds ? localtime(&when) : NULL;

	if (!local || !strftime(text, (size_t)size, "%b %d  %H:%M", local))
		snprintf(text, (size_t)size, "-");
	upper_ascii_text(text, text, size);
}

static void mm_side(struct ae_pvem_rows *r, float x, float width)
{
	struct ae_flow_screen const *flow = &r->flow;
	float u = ae_flow_u(flow, 1.0f), top = flow->frame.rect.y;
	struct ae_forge_map *map = ae_forge_maps_total() ? ae_forge_maps_at(mm_place(r)) : NULL;
	char line[96], name[64];

	ae_pve_menus_ui_plate(flow, x, width, 176.0f, 470.0f);
	if (mm_name_mode >= 0 && (mm_naming || mm_keyboard))
	{
		struct ae_field field;

		side_title(flow, x, mm_name_mode == MM_NAME_COPY ? "MAKE A COPY" : "RENAME", "A NAME FOR THE MAP");
		memset(&field, 0, sizeof(field));
		field.label = "MAP NAME";
		field.placeholder = "a-z, 0-9 and _";
		field.text = &mm_text;
		field.flags = AE_FIELD_FOCUSED | AE_FIELD_EDITING;
		field.caret_since = mm_caret;
		ae_field_well(&flow->density, x, top + 300.0f * u, width, &field, &mm_well);
		ae_widget_field(&flow->density, x, top + 300.0f * u, width, &field, MAPS_HIT);
		side_body(flow, x, width, 410.0f, mm_notice_bad && mm_notice[0] ? mm_notice :
			"Letters, numbers and _, up to 48. A keeps the name. B cancels.", 3);
		return;
	}
	if (!map)
	{
		side_title(flow, x, "NO FORGE MAPS YET", NULL);
		side_body(flow, x, width, 290.0f, "Start one from NEW MAP, place objects, then SAVE MAP in the pause menu. Maps others share with you show here too.", 6);
		return;
	}
	upper_ascii_text(map->name, name, (int)sizeof(name));
	{
		int image = ae_forge_maps_picture(map);
		float well_w = width, well_h = width * 114.0f / 140.0f;

		if (well_h > 200.0f * u)
		{
			well_h = 200.0f * u;
			well_w = well_h * 140.0f / 114.0f;
		}
		ae_draw_rect(x, top + 190.0f * u, width, 200.0f * u, 0.0f, AE_COLOR_TRACK);
		if (image)
			ae_draw_image_whole(image, x + (width - well_w) * 0.5f, top + 190.0f * u + (200.0f * u - well_h) * 0.5f, well_w, well_h, 0xFFFFFFFFu);
		else
			ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x + width * 0.5f, top + 290.0f * u, AE_ALIGN_CENTER, AE_COLOR_MUTED,
				"NO PICTURE");
	}
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 24.0f, x, top + 414.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, name);
	snprintf(line, sizeof(line), "%s  \xE2\x80\xA2  %s", map->base[0] ? map->base : "FORGE MAP", map->downloaded ? "DOWNLOADED" : "MADE HERE");
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 15.0f, x, top + 440.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, line);
	mm_time_text(map->saved, line, (int)sizeof(line));
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, top + 476.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, "SAVED");
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 18.0f, x + width, top + 476.0f * u, AE_ALIGN_RIGHT, AE_COLOR_TEXT, line);
	snprintf(line, sizeof(line), "%d OF 2048", map->objects);
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, top + 508.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, "OBJECTS");
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 18.0f, x + width, top + 508.0f * u, AE_ALIGN_RIGHT, AE_COLOR_TEXT, line);
	snprintf(line, sizeof(line), "%d", map->squads);
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, top + 540.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, "AI SQUADS");
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 18.0f, x + width, top + 540.0f * u, AE_ALIGN_RIGHT, AE_COLOR_TEXT, line);
	{
		char size[32];

		ae_hub_size_text((unsigned long long)map->size, size, (int)sizeof(size));
		snprintf(line, sizeof(line), "%s.map  %s", map->file, size);
	}
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, top + 572.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, "FILE");
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 16.0f, x + width, top + 572.0f * u, AE_ALIGN_RIGHT, AE_COLOR_TEXT, line);
	if (mm_notice[0])
		ae_pve_menus_ui_text(flow, AE_FONT_BODY, 15.0f, x, top + 606.0f * u, AE_ALIGN_LEFT, mm_notice_bad ? AE_COLOR_TEXT : AE_COLOR_MUTED, mm_notice);
}

static int mymaps_open(void)
{
	if (ae_ui_holds(&mymaps_screen))
		ae_pve_menus_rows_close(&mymaps_screen);
	memset(&mymaps_screen, 0, sizeof(mymaps_screen));
	mymaps_screen.name = "forge my maps";
	mymaps_screen.title = "MY MAPS";
	snprintf(mymaps_screen.crumb, sizeof(mymaps_screen.crumb), "FORGE");
	mymaps_screen.build = mm_build;
	mymaps_screen.step = mm_step;
	mymaps_screen.a_word = mm_a_word;
	mymaps_screen.x_action = mm_x_action;
	mymaps_screen.x_word = "Play";
	mymaps_screen.y_action = mm_y_action;
	mymaps_screen.y_word = "Options";
	mymaps_screen.side = mm_side;
	mymaps_screen.dialog_open = mm_dialog_open;
	mymaps_screen.handle_first = mm_handle;
	mymaps_screen.hit_base = MAPS_HIT;
	mymaps_screen.backing = 1;
	mm_first = 0;
	mm_name_mode = -1;
	mm_naming = mm_keyboard = 0;
	mm_notice[0] = 0;
	mm_read();
	return ae_pve_menus_rows_open(&mymaps_screen);
}

/* ---------- the doors */

int ae_screen_forge_open(void)
{
	staged = 0;
	staged_pack = -1;
	return hub_open();
}

int ae_screen_forge_open_staged(char const *scene)
{
	/* (hub: the real hub, as the FORGE row opens it; the others are made-up scenes that start and write nothing) */
	if (!_stricmp(scene, "hub"))
		return ae_screen_forge_open();
	staged = 1;
	staged_pack = !_stricmp(scene, "building") ? 1 : !_stricmp(scene, "needs") ? 2 : 0;
	if (!_stricmp(scene, "maps"))
		return pick_open();
	if (!_stricmp(scene, "keys"))
		return keys_open();
	if (!_stricmp(scene, "mymaps"))
	{
		staged = 0;
		return mymaps_open();
	}
	return hub_open();
}

void ae_screen_forge_close(void)
{
	if (ae_ui_holds(&mymaps_screen))
		ae_pve_menus_rows_close(&mymaps_screen);
	if (ae_ui_holds(&keys_screen))
		ae_pve_menus_rows_close(&keys_screen);
	if (ae_ui_holds(&pick_screen))
		ae_pve_menus_rows_close(&pick_screen);
	if (ae_ui_holds(&hub_screen))
		ae_pve_menus_rows_close(&hub_screen);
}

#else /* OPENCE_ENABLE_FORGE */

int ae_screen_forge_available(void)
{
	return 0;
}

const char *ae_screen_forge_badge(void)
{
	return NULL;
}

int ae_screen_forge_open(void)
{
	return 0;
}

int ae_screen_forge_open_staged(char const *scene)
{
	(void)scene;
	return 0;
}

void ae_screen_forge_close(void)
{
}

#endif /* OPENCE_ENABLE_FORGE */
