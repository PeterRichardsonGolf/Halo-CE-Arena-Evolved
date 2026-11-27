/*
AE_SCREEN_ITEM_SETS.C

ITEM SETS (ae_screen_item_sets.h): the hub's look (ae_setup_flow.h's title, rule and 46 % rows column, the card from 1095 / 1920,
the help line over the footer) under the hub's tab strip. Five steps, B going up one: the sets, a set's items, the maps to take
items from, an open map's nine kinds, a kind's items. A set plays through the randomizer's two settings (see ae_glue_item_sets.c):
a set that plays adds its items to campaign levels played alone and to no multiplayer game.
*/

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "cseries.h"
#include "../src/ae_draw.h"
#include "../src/ae_platform.h"
#include "ae_hooks.h"
#include "ae_dev_mode.h"
#include "ae_input.h"
#include "ae_item_sets_data.h"
#include "ae_item_swaps_rules.h"
#include "tag_campaign.h"
#include "ae_maps_hub_data.h"
#include "ae_screen_item_sets.h"
#include "ae_screen_item_picker.h"
#include "ae_screen_maps_hub.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

#define DOT " \xE2\x80\xA2 "
#define CHEVRON "\xE2\x80\xBA"

enum { HIT = 0x7D00, HIT_LIST, HIT_CTL, HIT_PROMPTS, HIT_FIELD, HIT_POP, HIT_BOX };
enum { OVERLAY_NONE, OVERLAY_OPTIONS, OVERLAY_NAME, OVERLAY_SETPICK };
enum { NAME_NEW, NAME_RENAME, NAME_COPY, NAME_ITEM, NAME_GROUP, NAME_SWAP_NEW, NAME_SWAP_RENAME };
enum { OP_NONE, OP_OPEN_MAP, OP_ADD };
enum { CTL_SEARCH, CTL_SHOW };
enum { SHOW_ALL, SHOW_CE, SHOW_MCC, SHOW_WORKSHOP, SHOWS };
enum { ROW_ADD, ROW_RANGE, ROW_HEADER, ROW_ENTRY };
/* the first screen's rows after the sets and NEW SET: HOW SETS PLAY's head and its two doors */
enum { HOME_NEW, HOME_HEAD, HOME_SWAPS, HOME_MULTIPLAYER, HOME_ROWS };
enum { OPTION_OPEN, OPTION_PLAY, OPTION_RENAME, OPTION_COPY, OPTION_DELETE, OPTIONS };

struct content_row { int type, index; };
/* a row of an item's settings page: a section's head (row = the section) or a setting (row = its AE_IS_ROW_*) */
struct setting_row { int header, row; };
enum { HANDS_MOST = 16, SWAP_VALUE = 320, SLOT_LINES_MOST = 300 };
/* a line of a swap's page: a section's head (row = the section) or a row (row AE_IS_SW_*, slot = the weapon's or enemy's index) */
struct swap_line { int header, row, slot; };
/* a line of a slot's screen: a section's head, or a pick (the path the list keeps, the words it is shown by) */
struct slot_line { int head, section, on; char path[256], label[96], sub[96]; };

static struct
{
	struct ae_flow_screen flow;
	struct ae_is_backend const *be;
	int staged, scene, started, first_frame;
	int step;
	struct ae_list_view list;
	short list_count;
	int ctl, device;
	/* the sets */
	struct ae_is_set sets[AE_IS_SETS_MOST];
	int set_count;
	/* the set open (its folder, its name) and its items */
	char folder[64], set_name[48];
	struct ae_is_entry entries[AE_IS_ENTRIES_MOST];
	int entry_count;
	struct content_row rows[AE_IS_ENTRIES_MOST + AE_IS_KINDS + 2];
	int row_count;
	/* an item's settings page: the item (its engine index, kind, path, map), its rows, and the values read from the set's file */
	long set_tag;
	int set_kind, set_failed, set_on, set_name_row;
	char set_path[256], set_map[48];
	struct setting_row srows[2 * AE_IS_ROWS];
	int srow_count;
	/* (by row: the kept text; the typed rows' text, a value row's key) */
	char value[AE_IS_ROWS][64];
	char body_biped[256], body_hands[320], body_movement[16];
	struct ae_is_fit fit;
	int replace_known;
	/* (the hands the set has: their kept text and name) */
	int hands_count;
	char hands_key[HANDS_MOST][320], hands_name[HANDS_MOST][64];
	/* the card of the biped focused in a set's items: what the fitness check and the set's body say of it (read once a focus) */
	long card_tag;
	int card_valid;
	struct ae_is_fit card_fit;
	char card_hands[320], card_movement[16];
	/* the set an options list or a name is for */
	char option_folder[64], option_name[48];
	int option_playing;
	/* the maps */
	struct ae_is_map maps[AE_IS_MAPS_MOST];
	int map_count, view[AE_IS_MAPS_MOST], view_count, show;
	unsigned long maps_generation;
	int map_open, map_row;
	/* the open map's kind and its items */
	int kind;
	long kind_counts[AE_IS_KINDS];
	struct ae_is_item items[AE_IS_ITEMS_MOST];
	int item_count, item_view[AE_IS_ITEMS_MOST], item_view_count;
	long result_index;
	struct ae_is_added result;
	int result_valid;
	long looked_for;
	/* search over the maps and the items */
	struct ae_text search;
	struct ae_field_edit edit;
	struct ae_rect well;
	unsigned long caret_since;
	int typing, keyboard;
	/* the popovers */
	int overlay, overlay_focus, name_mode;
	struct ae_text name_text;
	struct ae_field_edit name_edit;
	struct ae_rect name_well;
	int naming, naming_keyboard;
	/* a slow step waiting for its "ADDING..." frames */
	int op, op_wait, op_row, frozen;
	char notice[200];
	int notice_warning;
	unsigned long notice_until;
	char pending_name[64];
	/* a real scene waits for the map library, then opens a map of this PC and a fresh set */
	int real_pending;
	/* TAKES THE CAMPAIGN'S PLACE asks the campaign's items the first time: 0 not yet, 1 a frame or two to wait, 2 asked */
	int replace_state, replace_wait;
	/* the campaign swaps, the one open (its file, its row's data, the values read from its file) and the lines of its page */
	struct ae_is_swap swaps[AE_IS_SWAPS_MOST];
	int swap_count;
	char swap_file[64];
	struct ae_is_swap swap;
	/* the swap's set's items, and the bipeds among them a player can be */
	struct ae_is_entry sw_entries[AE_IS_ENTRIES_MOST];
	int sw_entry_count, sw_bipeds[16], sw_biped_count;
	char swap_val[AE_IS_SW_ROWS][SWAP_VALUE];
	struct swap_line swlines[AE_IS_SWAP_ROWS_MOST + 4];
	int swline_count;
	/* a slot of the swap open: its row (AE_IS_SW_*), the weapon's or enemy's index, the enemy's rank; its lines */
	int slot_row, slot_slot, slot_rank;
	struct slot_line slines[SLOT_LINES_MOST];
	int sline_count;
	/* the set a new swap plays, picked from the sets with an item on */
	int pick_count, pick_focus, pick_sets[AE_IS_SETS_MOST];
	char pending_swap[48];
	/* the page was opened from the lobby's picker: B leaves it for the picker alone */
	int from_picker;
} S;

static struct ae_screen_class const sets_class;
static void home_card(float x, float y, float width, float height);
static void range_card(float x, float y, float width, float height);

static float U(float u)
{
	return ae_flow_u(&S.flow, u);
}

static float Y(float u)
{
	return ae_frame_y(&S.flow.frame, u / 1080.0f);
}

static void notice_set(int warning, unsigned long ms, char const *format, ...)
{
	va_list arguments;

	va_start(arguments, format);
	vsnprintf(S.notice, sizeof(S.notice), format, arguments);
	va_end(arguments);
	S.notice_warning = warning;
	S.notice_until = ae_motion_now() + ms;
}

static int same_text(char const *a, char const *b)
{
	for (; *a && *b; a++, b++)
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	return *a == *b;
}

/* whether a text has the words in it, any case */
static int contains(char const *text, char const *needle)
{
	size_t length = strlen(needle), at, each;

	if (!length)
		return 1;
	for (at = 0; text[at]; at++)
	{
		for (each = 0; each < length && text[at + each] && tolower((unsigned char)text[at + each]) == tolower((unsigned char)needle[each]); each++)
			;
		if (each == length)
			return 1;
	}
	return 0;
}

static void upper_copy(char *to, int size, char const *from)
{
	int each;

	for (each = 0; each < size - 1 && from[each]; each++)
		to[each] = from[each] >= 'a' && from[each] <= 'z' ? (char)(from[each] - 32) : from[each];
	to[each] = 0;
}

/* a name in lower case, for a sentence ("PISTOL" lying in a mission: "pistol") */
static void lower_name_copy(char *to, int size, char const *from)
{
	int each;

	for (each = 0; each < size - 1 && from[each]; each++)
		to[each] = from[each] >= 'A' && from[each] <= 'Z' ? (char)(from[each] + 32) : from[each];
	to[each] = 0;
}

/* ---------- the data */

static struct ae_is_set const *set_named(char const *folder)
{
	int each;

	for (each = 0; each < S.set_count; each++)
		if (same_text(S.sets[each].folder, folder))
			return &S.sets[each];
	return NULL;
}

static void sets_read(void)
{
	S.set_count = S.be->sets(S.sets, AE_IS_SETS_MOST);
}

static void list_to(int count, int focus)
{
	ae_list_view_init(&S.list, 0, 6);
	S.list_count = (short)count;
	ae_list_set_count(&S.list.list, S.list_count);
	if (focus > 0 && focus < count)
		ae_list_set_focus(&S.list.list, (short)focus);
}

static void entries_read(void)
{
	int each, kind, count_of[AE_IS_KINDS];

	S.entry_count = S.be->entries(S.folder, S.entries, AE_IS_ENTRIES_MOST);
	S.row_count = 0;
	S.rows[S.row_count].type = ROW_ADD;
	S.rows[S.row_count++].index = 0;
	S.rows[S.row_count].type = ROW_RANGE;
	S.rows[S.row_count++].index = 0;
	memset(count_of, 0, sizeof(count_of));
	for (each = 0; each < S.entry_count; each++)
		count_of[S.entries[each].kind]++;
	for (kind = 0; kind < AE_IS_KINDS; kind++)
	{
		int pass;

		if (!count_of[kind])
			continue;
		S.rows[S.row_count].type = ROW_HEADER;
		S.rows[S.row_count++].index = kind;
		/* (those the last level turned off listed first in their kind) */
		for (pass = 0; pass < 2; pass++)
			for (each = 0; each < S.entry_count; each++)
				if (S.entries[each].kind == kind && (S.entries[each].failed[0] != 0) == (pass == 0))
				{
					S.rows[S.row_count].type = ROW_ENTRY;
					S.rows[S.row_count++].index = each;
				}
	}
	if (!S.be->body_option(S.folder, "player_biped", S.body_biped, sizeof(S.body_biped)))
		S.body_biped[0] = 0;
	S.card_valid = 0;
}

/* the maps a SHOW value and the search keep */
static void maps_view(void)
{
	int each;

	S.view_count = 0;
	for (each = 0; each < S.map_count; each++)
	{
		struct ae_is_map const *map = &S.maps[each];

		if (S.show == SHOW_CE && map->source != AE_IS_SOURCE_CE)
			continue;
		if (S.show == SHOW_MCC && map->source != AE_IS_SOURCE_MCC)
			continue;
		if (S.show == SHOW_WORKSHOP && map->source != AE_IS_SOURCE_WORKSHOP)
			continue;
		if (S.search.text[0] && !contains(map->name, S.search.text) && !contains(map->file, S.search.text))
			continue;
		S.view[S.view_count++] = each;
	}
}

static void maps_read(void)
{
	S.map_count = S.be->maps(S.folder, S.maps, AE_IS_MAPS_MOST);
	if (!S.staged)
		S.maps_generation = ae_hub_data_generation();
	maps_view();
}

static void items_view(void)
{
	int each;

	S.item_view_count = 0;
	for (each = 0; each < S.item_count; each++)
		if (!S.search.text[0] || contains(S.items[each].path, S.search.text))
			S.item_view[S.item_view_count++] = each;
}

static void items_read(int kind)
{
	S.kind = kind;
	S.item_count = S.be->kind_items(kind, S.folder, S.items, AE_IS_ITEMS_MOST);
	items_view();
}

static void kinds_read(void)
{
	int kind;

	for (kind = 0; kind < AE_IS_KINDS; kind++)
		S.kind_counts[kind] = S.be->kind_count(kind);
}

static long map_items_in_set(void)
{
	return S.map_open >= 0 && S.map_open < S.map_count ? S.maps[S.map_open].in_set : 0;
}

static int step_count(void)
{
	switch (S.step)
	{
	case AE_IS_STEP_SETS: return S.set_count + HOME_ROWS;
	case AE_IS_STEP_SWAPS: return S.swap_count + 1;
	case AE_IS_STEP_SWAP: return S.swline_count;
	case AE_IS_STEP_RANKS: return tag_campaign_enemy_rank_count((short)S.slot_slot);
	case AE_IS_STEP_SLOT: return S.sline_count;
	case AE_IS_STEP_CONTENTS: return S.row_count;
	case AE_IS_STEP_MAPS: return S.view_count;
	case AE_IS_STEP_KINDS: return AE_IS_KINDS;
	case AE_IS_STEP_SETTINGS: return S.srow_count;
	}
	return S.item_view_count;
}

static void step_to(int step, int focus)
{
	S.step = step;
	S.ctl = -1;
	S.result_valid = 0;
	list_to(step_count(), focus);
}

static struct ae_is_item *item_focused(void)
{
	return S.step == AE_IS_STEP_ITEMS && S.list.list.focus >= 0 && S.list.list.focus < S.item_view_count ?
		&S.items[S.item_view[S.list.list.focus]] : NULL;
}

static struct ae_is_map *map_focused(void)
{
	return S.step == AE_IS_STEP_MAPS && S.list.list.focus >= 0 && S.list.list.focus < S.view_count ?
		&S.maps[S.view[S.list.list.focus]] : NULL;
}

static struct ae_is_entry *entry_focused(void)
{
	if (S.step == AE_IS_STEP_CONTENTS && S.list.list.focus >= 0 && S.list.list.focus < S.row_count &&
		S.rows[S.list.list.focus].type == ROW_ENTRY)
		return &S.entries[S.rows[S.list.list.focus].index];
	return NULL;
}

static struct ae_is_set const *set_focused(void)
{
	return S.step == AE_IS_STEP_SETS && S.list.list.focus >= 0 && S.list.list.focus < S.set_count ? &S.sets[S.list.list.focus] : NULL;
}

/* ---------- an item's settings page: the values it shows */

static struct ae_is_entry const *settings_entry(void)
{
	int each;

	for (each = 0; each < S.entry_count; each++)
		if (S.entries[each].tag == S.set_tag)
			return &S.entries[each];
	return NULL;
}

static int play_as_on(void)
{
	return S.set_kind == 4 && ae_is_body_is(S.body_biped, S.set_path);
}

/* the key of the set's file a row keeps its value under ("": not a plain setting) */
static char const *row_key(int row)
{
	switch (row)
	{
	case AE_IS_ROW_NAME: return "name";
	case AE_IS_ROW_FACTION: return "faction";
	case AE_IS_ROW_CLASS: return "class";
	case AE_IS_ROW_COUNTS_AS: return "type";
	case AE_IS_ROW_GROUP: return "custom_type";
	case AE_IS_ROW_HUD: return "hud";
	case AE_IS_ROW_REPLACE: return "replace";
	}
	return "";
}

static char const *const section_names[5] = { "", "IN THE RANDOMIZER", "IN A LEVEL", "YOUR BODY", "IN THE SET" };

/* the rows of the page, a section's head wherever the section changes */
static void settings_build(void)
{
	int rows[AE_IS_ROWS], count = ae_is_setting_rows(S.set_kind, S.replace_known, play_as_on(), rows), each, section = AE_IS_SECTION_NONE;

	S.srow_count = 0;
	for (each = 0; each < count; each++)
	{
		int now = ae_is_row_section(rows[each]);

		if (now != AE_IS_SECTION_NONE && now != section)
		{
			S.srows[S.srow_count].header = 1;
			S.srows[S.srow_count++].row = now;
			section = now;
		}
		S.srows[S.srow_count].header = 0;
		S.srows[S.srow_count++].row = rows[each];
	}
}

/* the values of the page read from the set's file (once, and again after a change: reading rescans the sets) */
static void settings_read(void)
{
	struct ae_is_entry const *entry = settings_entry();
	int row, each;

	memset(S.value, 0, sizeof(S.value));
	for (row = 0; row < AE_IS_ROWS; row++)
		if (row_key(row)[0])
			S.be->entry_option(S.folder, S.set_tag, row_key(row), S.value[row], sizeof(S.value[0]));
	if (!S.be->body_option(S.folder, "player_biped", S.body_biped, sizeof(S.body_biped)))
		S.body_biped[0] = 0;
	if (!S.be->body_option(S.folder, "player_hands", S.body_hands, sizeof(S.body_hands)))
		S.body_hands[0] = 0;
	if (!S.be->body_option(S.folder, "player_movement", S.body_movement, sizeof(S.body_movement)))
		S.body_movement[0] = 0;
	memset(&S.fit, 0, sizeof(S.fit));
	if (S.set_kind == 4)
		S.be->entry_fit(S.folder, S.set_tag, &S.fit);
	/* (TAKES THE CAMPAIGN'S PLACE: shown while the campaign's items are being read, then only for an item of a path a mission has) */
	S.replace_known = S.replace_state == 1 ? 1 : S.be->replace_known() && S.set_kind <= 2 && S.be->entry_stock(S.folder, S.set_tag);
	S.set_on = entry ? entry->on : 0;
	S.set_failed = entry ? entry->failed[0] != 0 : 0;
	S.hands_count = 0;
	for (each = 0; each < S.entry_count && S.hands_count < HANDS_MOST; each++)
		if (S.entries[each].kind == 8)
		{
			char name[96];

			ae_is_hands_key(S.entries[each].path, S.entries[each].map, S.hands_key[S.hands_count], sizeof(S.hands_key[0]));
			ae_is_item_name(S.entries[each].path, name, sizeof(name));
			snprintf(S.hands_name[S.hands_count], sizeof(S.hands_name[0]), "%s", S.entries[each].name[0] ? S.entries[each].name : name);
			S.hands_count++;
		}
}

/* a setting row's value in words, and how it reads: tone 0 a value with arrows, 1 typed (opens its entry), 2 read-only, 3 warning */
static void setting_text(int row, char *text, int size, int *tone)
{
	char name[128];
	int index, each;

	*tone = 0;
	text[0] = 0;
	switch (row)
	{
	case AE_IS_ROW_NAME:
		ae_is_item_name(S.set_path, name, sizeof(name));
		snprintf(text, (size_t)size, "%s", S.value[row][0] ? S.value[row] : name);
		*tone = 1;
		return;
	case AE_IS_ROW_ON:
		snprintf(text, (size_t)size, "%s", S.set_on ? "ON" : "OFF");
		return;
	case AE_IS_ROW_GROUP:
		snprintf(text, (size_t)size, "%s", S.value[row][0] ? S.value[row] : "ITS NAME");
		*tone = 1;
		return;
	case AE_IS_ROW_REPLACE:
		if (S.replace_state == 1)
		{
			snprintf(text, (size_t)size, "LOOKING...");
			*tone = 2;
			return;
		}
		break;
	case AE_IS_ROW_PLAY_AS:
		if (S.fit.known && !S.fit.fits)
		{
			snprintf(text, (size_t)size, "DOESN'T FIT A PLAYER");
			*tone = 3;
			return;
		}
		snprintf(text, (size_t)size, "%s", play_as_on() ? "ON" : "OFF");
		return;
	case AE_IS_ROW_HANDS:
		/* (stored with the set, but the spawn does not play them yet: the row waits) */
		if (!ae_is_body_rows_live())
		{
			snprintf(text, (size_t)size, "COMES NEXT");
			*tone = 2;
			return;
		}
		snprintf(text, (size_t)size, "%s", ae_is_hands_word(0));
		if (!strcmp(S.body_hands, "campaign"))
			snprintf(text, (size_t)size, "%s", ae_is_hands_word(1));
		for (each = 0; each < S.hands_count; each++)
			if (!strcmp(S.body_hands, S.hands_key[each]))
				snprintf(text, (size_t)size, "%s", S.hands_name[each]);
		return;
	case AE_IS_ROW_MOVEMENT:
		if (!ae_is_body_rows_live())
		{
			snprintf(text, (size_t)size, "COMES NEXT");
			*tone = 2;
			return;
		}
		index = ae_is_value_index(row, S.set_kind, S.body_movement);
		snprintf(text, (size_t)size, "%s", ae_is_value_word(row, S.set_kind, index));
		return;
	case AE_IS_ROW_REMOVE:
		return;
	}
	index = ae_is_value_index(row, S.set_kind, S.value[row]);
	snprintf(text, (size_t)size, "%s", ae_is_value_word(row, S.set_kind, index));
	/* (AUTO says what it found) */
	if (index == 0)
	{
		struct ae_is_entry const *entry = settings_entry();
		char answer[80];

		if (ae_is_auto_answer(row, S.set_kind, S.set_path, entry ? entry->name : "", answer, (int)sizeof(answer)))
			snprintf(text, (size_t)size, "AUTO: %s", answer);
	}
}

/* the page's row under the focus: its AE_IS_ROW_*, else -1 (a section's head) */
static int setting_focused(void)
{
	return S.step == AE_IS_STEP_SETTINGS && S.list.list.focus >= 0 && S.list.list.focus < S.srow_count &&
		!S.srows[S.list.list.focus].header ? S.srows[S.list.list.focus].row : -1;
}

/* ---------- campaign swaps: the data */

static void swaps_read(void)
{
	S.swap_count = S.be->swaps(S.swaps, AE_IS_SWAPS_MOST);
}

static struct ae_is_swap const *swap_named(char const *file)
{
	int each;

	for (each = 0; each < S.swap_count; each++)
		if (same_text(S.swaps[each].file, file))
			return &S.swaps[each];
	return NULL;
}

/* the first screen's row under the focus (HOME_*), else -1 (a set) */
static int home_row(void)
{
	int row = S.list.list.focus - S.set_count;

	return S.step == AE_IS_STEP_SETS && S.list.list.focus >= 0 && row >= 0 && row < HOME_ROWS ? row : -1;
}

static struct ae_is_swap const *swap_focused(void)
{
	return S.step == AE_IS_STEP_SWAPS && S.list.list.focus >= 0 && S.list.list.focus < S.swap_count ? &S.swaps[S.list.list.focus] : NULL;
}

static int swap_state_of(struct ae_is_swap const *swap)
{
	return ae_is_swap_state(swap->in_use, swap->set_found, swap->set_items_on);
}

/* the sets with an item on, the one opened last first (Content Evolved's order for a new swap's set): how many */
static int sets_with_items(int *list, int maximum)
{
	int each, count = 0, first = -1;

	for (each = 0; each < S.set_count; each++)
		if (S.folder[0] && same_text(S.sets[each].folder, S.folder) && S.sets[each].on > 0)
			first = each;
	if (first >= 0 && count < maximum)
		list[count++] = first;
	for (each = 0; each < S.set_count && count < maximum; each++)
		if (S.sets[each].on > 0 && each != first)
			list[count++] = each;
	return count;
}

/* an item's shown name: the one typed for it, else its own */
static void entry_shown_name(struct ae_is_entry const *entry, char *name, int size)
{
	char own[128];

	ae_is_item_name(entry->path, own, sizeof(own));
	snprintf(name, (size_t)size, "%s", entry->name[0] ? entry->name : own);
}

static int swap_by_weapon(void)
{
	return !strcmp(S.swap_val[AE_IS_SW_WEAPON_PICK], "manual");
}

static int swap_by_enemy(void)
{
	return !strcmp(S.swap_val[AE_IS_SW_ENEMY_PICK], "manual");
}

/* the swap's values read from its file, its set's items, and the lines of its page built (a head wherever the section changes) */
static void swap_build(void)
{
	struct ae_is_swap_row rows[AE_IS_SWAP_ROWS_MOST];
	int count = ae_is_swap_rows(swap_by_weapon(), swap_by_enemy(), S.swap_val[AE_IS_SW_BIPED][0] != 0, rows), each, section = AE_IS_SWS_NONE;

	S.swline_count = 0;
	for (each = 0; each < count; each++)
	{
		int now = ae_is_swap_row_section(rows[each].row);

		if (now != AE_IS_SWS_NONE && now != section)
		{
			S.swlines[S.swline_count].header = 1;
			S.swlines[S.swline_count].row = now;
			S.swlines[S.swline_count++].slot = 0;
		}
		section = now;
		S.swlines[S.swline_count].header = 0;
		S.swlines[S.swline_count].row = rows[each].row;
		S.swlines[S.swline_count++].slot = rows[each].slot;
	}
}

static void swap_read(void)
{
	struct ae_is_swap const *data = swap_named(S.swap_file);
	int row, each;

	memset(S.swap_val, 0, sizeof(S.swap_val));
	for (row = 0; row < AE_IS_SW_ROWS; row++)
		if (ae_is_swap_row_file_key(row)[0] && row != AE_IS_SW_NAME)
			S.be->swap_option(S.swap_file, ae_is_swap_row_file_key(row), S.swap_val[row], SWAP_VALUE);
	memset(&S.swap, 0, sizeof(S.swap));
	if (data)
		S.swap = *data;
	S.sw_entry_count = 0;
	S.sw_biped_count = 0;
	if (data && data->set_found)
	{
		S.sw_entry_count = S.be->entries(data->set_folder, S.sw_entries, AE_IS_ENTRIES_MOST);
		for (each = 0; each < S.sw_entry_count && S.sw_biped_count < 16; each++)
		{
			struct ae_is_entry const *entry = &S.sw_entries[each];
			struct ae_is_fit fit;

			if (entry->kind != 4 || !entry->on || entry->failed[0])
				continue;
			S.be->entry_fit(data->set_folder, entry->tag, &fit);
			/* (a biped no level has measured is offered: the spawn checks it and falls back) */
			if (fit.known && !fit.fits)
				continue;
			S.sw_bipeds[S.sw_biped_count++] = each;
		}
	}
	swap_build();
}

/* the page read again, the focus kept on a row (AE_IS_SW_*) and slot, else the first row */
static void swap_reload(int keep_row, int keep_slot)
{
	int each, focus = 0;

	swaps_read();
	sets_read();
	swap_read();
	for (each = 0; keep_row >= 0 && each < S.swline_count; each++)
		if (!S.swlines[each].header && S.swlines[each].row == keep_row && S.swlines[each].slot == keep_slot)
			focus = each;
	list_to(S.swline_count, focus);
}

static void swap_open(char const *file)
{
	snprintf(S.swap_file, sizeof(S.swap_file), "%s", file);
	swaps_read();
	swap_read();
	S.step = AE_IS_STEP_SWAP;
	S.ctl = -1;
	S.result_valid = 0;
	list_to(S.swline_count, 0);
	ae_sound_request(AE_SOUND_FORWARD, 0);
	platform_log("item sets: swap %s opened", file);
}

static struct swap_line const *swline_focused(void)
{
	return S.step == AE_IS_STEP_SWAP && S.list.list.focus >= 0 && S.list.list.focus < S.swline_count && !S.swlines[S.list.list.focus].header ?
		&S.swlines[S.list.list.focus] : NULL;
}

/* the key a slot keeps its picks under in the swap's file ("pistol", "elite_major") */
static void slot_key(int row, int slot, int rank, char *key, int size)
{
	if (row == AE_IS_SW_WEAPON_SLOT)
		snprintf(key, (size_t)size, "%s", tag_campaign_weapon_key((short)slot));
	else
		tag_campaign_slot_key((short)slot, (short)rank, key, size);
}

static void slot_name(int row, int slot, int rank, char *name, int size)
{
	if (row == AE_IS_SW_WEAPON_SLOT)
		snprintf(name, (size_t)size, "%s", tag_campaign_weapon_name((short)slot));
	else
		tag_campaign_slot_name((short)slot, (short)rank, name, size);
}

/* how many picks a slot has (an enemy's: the enemies it becomes and the weapons it carries) */
static int slot_picks(int row, int slot, int rank)
{
	char key[64];
	int count;

	slot_key(row, slot, rank, key, sizeof(key));
	if (row == AE_IS_SW_WEAPON_SLOT)
		return S.be->swap_list_count(S.swap_file, "weapons", key);
	count = S.be->swap_list_count(S.swap_file, "enemies", key);
	if (tag_campaign_enemy_armed((short)slot))
		count += S.be->swap_list_count(S.swap_file, "loadout", key);
	return count;
}

/* an enemy's picks over its ranks, and the name of the one rank that has them (when only one does) */
static int enemy_picks(int enemy, char *rank_word, int size)
{
	int ranks = tag_campaign_enemy_rank_count((short)enemy), each, total = 0, with = 0, last = 0;

	rank_word[0] = 0;
	if (!ranks)
		return slot_picks(AE_IS_SW_ENEMY_SLOT, enemy, 0);
	for (each = 0; each < ranks; each++)
	{
		int picks = slot_picks(AE_IS_SW_ENEMY_SLOT, enemy, tag_campaign_enemy_rank((short)enemy, (short)each));

		total += picks;
		if (picks)
		{
			with++;
			last = tag_campaign_enemy_rank((short)enemy, (short)each);
		}
	}
	if (with == 1)
		snprintf(rank_word, (size_t)size, "%s", tag_campaign_rank_name((short)last));
	return total;
}

/* the biped choices of the swap: 0 the campaign's own, then the set's bipeds a player can be */
static int swap_biped_index(void)
{
	int each;

	for (each = 0; each < S.sw_biped_count; each++)
		if (ae_is_body_is(S.swap_val[AE_IS_SW_BIPED], S.sw_entries[S.sw_bipeds[each]].path))
			return each + 1;
	return 0;
}

/* a swap row's value in words, and how it reads: tone 0 a value with arrows, 1 opens something (a typed row, a slot), 2 read-only, 3 warning */
static void swap_text(int row, int slot, char *text, int size, int *tone)
{
	char word[48];
	int index;

	*tone = 0;
	text[0] = 0;
	switch (row)
	{
	case AE_IS_SW_NAME:
		snprintf(text, (size_t)size, "%s", S.swap.name);
		*tone = 1;
		return;
	case AE_IS_SW_SET:
		if (!S.swap.set_found)
		{
			snprintf(text, (size_t)size, "SET MISSING");
			*tone = 3;
			return;
		}
		upper_copy(text, size, S.swap.set_name);
		return;
	case AE_IS_SW_WEAPON_SLOT:
		ae_is_slot_value(slot_picks(row, slot, 0), text, size);
		*tone = 1;
		return;
	case AE_IS_SW_ENEMY_SLOT:
	{
		int picks = enemy_picks(slot, word, sizeof(word));

		ae_is_slot_value(picks, text, size);
		if (picks && word[0])
			snprintf(text, (size_t)size, "%s: %d PICKED", word, picks);
		*tone = 1;
		return;
	}
	case AE_IS_SW_BIPED:
		index = swap_biped_index();
		if (S.swap_val[AE_IS_SW_BIPED][0] && !index)
			snprintf(text, (size_t)size, "NOT IN THE SET");
		else if (!index)
			snprintf(text, (size_t)size, "THE CAMPAIGN'S OWN");
		else
			entry_shown_name(&S.sw_entries[S.sw_bipeds[index - 1]], text, size);
		return;
	case AE_IS_SW_HANDS:
		/* (stored with the swap, but the spawn does not play them yet: the row waits) */
		snprintf(text, (size_t)size, "COMES NEXT");
		*tone = 2;
		return;
	case AE_IS_SW_MOVEMENT:
		if (!ae_is_body_rows_live())
		{
			snprintf(text, (size_t)size, "COMES NEXT");
			*tone = 2;
			return;
		}
		break;
	}
	index = ae_is_swap_value_index(row, S.swap_val[row]);
	snprintf(text, (size_t)size, "%s", ae_is_swap_value_word(row, index));
}

/* ---------- a slot's screen: what the set and the campaign offer, and what the swap keeps */

static void slot_line_add(int head, int section, char const *path, char const *label, char const *sub, int on)
{
	struct slot_line *line;

	if (S.sline_count >= SLOT_LINES_MOST)
		return;
	line = &S.slines[S.sline_count++];
	line->head = head;
	line->section = section;
	line->on = on;
	snprintf(line->path, sizeof(line->path), "%s", path);
	snprintf(line->label, sizeof(line->label), "%s", label);
	snprintf(line->sub, sizeof(line->sub), "%s", sub);
}

static char const *slot_list_section(int section)
{
	if (S.slot_row == AE_IS_SW_WEAPON_SLOT)
		return "weapons";
	return section == 0 ? "enemies" : "loadout";
}

/* a section's picks: the own word first, then the set's items of that kind (on and working), then the campaign's weapons for the loadout */
static void slot_section(int section, int kind, int campaign_weapons)
{
	char key[64], label[96], sub[96];
	int each;
	char const *list = slot_list_section(section);

	slot_key(S.slot_row, S.slot_slot, S.slot_rank, key, sizeof(key));
	slot_line_add(0, section, "original", ae_is_slot_own_word(section), "",
		S.be->swap_list_has(S.swap_file, list, key, "original"));
	for (each = 0; each < S.sw_entry_count; each++)
	{
		struct ae_is_entry const *entry = &S.sw_entries[each];

		if (entry->kind != kind || !entry->on || entry->failed[0])
			continue;
		entry_shown_name(entry, label, sizeof(label));
		snprintf(sub, sizeof(sub), "from %s", entry->map);
		slot_line_add(0, section, entry->path, label, sub, S.be->swap_list_has(S.swap_file, list, key, entry->path));
	}
	for (each = 0; campaign_weapons && each < AE_IS_SWAP_WEAPON_SLOTS; each++)
	{
		char stock[64];

		snprintf(stock, sizeof(stock), "stock:%s", tag_campaign_weapon_key((short)each));
		snprintf(sub, sizeof(sub), "the campaign's own");
		slot_line_add(0, section, stock, tag_campaign_weapon_name((short)each), sub, S.be->swap_list_has(S.swap_file, list, key, stock));
	}
}

static void slot_build(void)
{
	int weapon = S.slot_row == AE_IS_SW_WEAPON_SLOT;

	S.sline_count = 0;
	slot_line_add(1, 0, "", ae_is_slot_replaced_head(weapon), "", 0);
	slot_section(0, weapon ? 0 : 1, 0);
	if (!weapon && tag_campaign_enemy_armed((short)S.slot_slot))
	{
		slot_line_add(1, 1, "", ae_is_slot_armed_head(), "", 0);
		slot_section(1, 0, 1);
	}
}

static void slot_reload(int focus)
{
	swaps_read();
	swap_read();
	slot_build();
	list_to(S.sline_count, focus);
}

static void slot_open(int row, int slot, int rank)
{
	S.slot_row = row;
	S.slot_slot = slot;
	S.slot_rank = rank;
	slot_build();
	S.step = AE_IS_STEP_SLOT;
	S.ctl = -1;
	list_to(S.sline_count, 1);
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static struct slot_line const *slot_focused(void)
{
	return S.step == AE_IS_STEP_SLOT && S.list.list.focus >= 0 && S.list.list.focus < S.sline_count && !S.slines[S.list.list.focus].head ?
		&S.slines[S.list.list.focus] : NULL;
}

/* how many of the slot's lines of a section are on */
static int slot_section_on(int section)
{
	int each, count = 0;

	for (each = 0; each < S.sline_count; each++)
		count += !S.slines[each].head && S.slines[each].section == section && S.slines[each].on;
	return count;
}

/* the firing range's wait for the set open ("" when it can start) */
static char const *range_wait(void)
{
	struct ae_is_set const *set = set_named(S.folder);

	return ae_is_range_wait(set ? set->on : 0, S.be->range_ready());
}

/* ---------- drawing parts (the hub's, for this screen) */

static void text_shadow(int font, float size, float x, float y, int align, unsigned int color, char const *text)
{
	ae_draw_text(font, size, x + U(2.0f), y + U(2.0f), align, 0x99000000u, text);
	ae_draw_text(font, size, x, y, align, color, text);
}

static void card_frame(float x, float y, float width, float height, int warning)
{
	ae_draw_rect(x, y, width, height, 0.0f, AE_COLOR_PANEL);
	ae_draw_outline(x, y, width, height, 0.0f, U(1.0f), AE_COLOR_RULE);
	ae_draw_rect(x, y, width, U(4.0f), 0.0f, warning ? AE_COLOR_WARNING : AE_COLOR_ACCENT);
}

static float card_fact(float x, float y, float width, char const *label, char const *value, unsigned int color)
{
	struct ae_density const *d = &S.flow.density;
	float inset = U(AE_FLOW_INSET_U), label_size = ae_size_minor(d) * 0.84f, value_size = ae_size_text(d) * 0.83f;
	char fitted[160];
	float label_width = ae_draw_text_tracked_width(AE_FONT_BODY, label_size, 0.09f, label);

	ae_draw_text_tracked(AE_FONT_BODY, label_size, 0.09f, x + inset, y + U(4.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED, label);
	ae_fit_text(AE_FONT_ROW, value_size, value, width - 2.0f * inset - label_width - U(20.0f), fitted, sizeof(fitted));
	ae_draw_text(AE_FONT_ROW, value_size, x + width - inset, y, AE_ALIGN_RIGHT, color, fitted);
	ae_draw_rect(x + inset, y + U(31.0f), width - 2.0f * inset, U(1.0f), 0.0f, AE_COLOR_RULE);
	return U(40.0f);
}

static float card_body(float x, float y, float width, char const *text, int lines, unsigned int color)
{
	struct ae_density const *d = &S.flow.density;
	float inset = U(AE_FLOW_INSET_U), body = ae_size_text(d) * 0.88f, line = U(31.0f);
	char wrapped[8][160];
	int count, each;

	count = ae_wrap_text(AE_FONT_BODY, body, text, width - 2.0f * inset, wrapped, lines > 8 ? 8 : lines);
	for (each = 0; each < count; each++)
		ae_draw_text(AE_FONT_BODY, body, x + inset, y + line * (float)each, AE_ALIGN_LEFT, color, wrapped[each]);
	return line * (float)count;
}

static float card_label(float x, float y, char const *label)
{
	ae_draw_text_tracked(AE_FONT_BODY, ae_size_minor(&S.flow.density) * 0.84f, 0.09f, x + U(AE_FLOW_INSET_U), y, AE_ALIGN_LEFT,
		AE_COLOR_MUTED, label);
	return U(34.0f);
}

/* a card's title: the face 40 u; a long name is fitted (a set's or a map's) */
static float card_title(float x, float y, float width, char const *text)
{
	char fitted[100];

	ae_fit_text(AE_FONT_TITLE, U(40.0f), text, width - 2.0f * U(AE_FLOW_INSET_U), fitted, sizeof(fitted));
	ae_draw_text(AE_FONT_TITLE, U(40.0f), x + U(AE_FLOW_INSET_U), y, AE_ALIGN_LEFT, AE_COLOR_TITLE, fitted);
	return U(58.0f);
}

/* an item's name in the card: Overpass 900 28 u; too wide, two lines broken after a "_" or "-", never cut */
static float card_item_name(float x, float y, float width, char const *name)
{
	float size = U(28.0f), room = width - 2.0f * U(AE_FLOW_INSET_U);
	char first[96], second[96];
	int at, cut = -1;

	if (ae_draw_text_width(AE_FONT_ROW, size, name) <= room)
	{
		ae_draw_text(AE_FONT_ROW, size, x + U(AE_FLOW_INSET_U), y, AE_ALIGN_LEFT, AE_COLOR_TITLE, name);
		return U(44.0f);
	}
	/* (the last break whose first half fits) */
	for (at = 0; name[at] && at < 90; at++)
	{
		if (name[at] == '_' || name[at] == '-')
		{
			char probe[96];

			snprintf(probe, sizeof(probe), "%.*s", at + 1, name);
			if (ae_draw_text_width(AE_FONT_ROW, size, probe) <= room)
				cut = at + 1;
		}
	}
	if (cut < 0)
		cut = (int)(strlen(name) / 2);
	snprintf(first, sizeof(first), "%.*s", cut, name);
	snprintf(second, sizeof(second), "%s", name + cut);
	ae_draw_text(AE_FONT_ROW, size, x + U(AE_FLOW_INSET_U), y, AE_ALIGN_LEFT, AE_COLOR_TITLE, first);
	ae_draw_text(AE_FONT_ROW, size, x + U(AE_FLOW_INSET_U), y + U(34.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE, second);
	return U(78.0f);
}

/* the state line under a name: Overpass 750 17 u tracked .06 */
static float card_state(float x, float y, char const *text, unsigned int color)
{
	ae_draw_text_tracked(AE_FONT_BODY, U(17.0f), 0.06f, x + U(AE_FLOW_INSET_U), y, AE_ALIGN_LEFT, color, text);
	return U(36.0f);
}

/* the whole path, pitch 28 u, folders joined by the chevron, broken only between folders */
static float card_path(float x, float y, float width, char const *path)
{
	char lines[8][96];
	float size = U(19.0f), cy = y;
	int chars = (int)((width - 2.0f * U(AE_FLOW_INSET_U)) / (size * 0.5f)), count, each;

	cy += card_label(x, cy, "WHERE IT IS IN THE MAP");
	count = ae_is_path_lines(path, chars > 20 ? chars : 20, lines, 8);
	for (each = 0; each < count; each++)
		ae_draw_text(AE_FONT_BODY, size, x + U(AE_FLOW_INSET_U), cy + U(28.0f) * (float)each, AE_ALIGN_LEFT, AE_COLOR_TEXT, lines[each]);
	return cy - y + U(28.0f) * (float)count + U(8.0f);
}

static void count_line(float x, float y, float width, char const *left, char const *right)
{
	float size = ae_size_minor(&S.flow.density) * 0.9f;

	ae_draw_text_tracked(AE_FONT_BODY, size, 0.09f, x + U(11.0f), y + U(1.5f), AE_ALIGN_LEFT, 0x99000000u, left);
	ae_draw_text_tracked(AE_FONT_BODY, size, 0.09f, x + U(10.0f), y, AE_ALIGN_LEFT, AE_COLOR_MUTED, left);
	if (right && right[0])
	{
		ae_draw_text_tracked(AE_FONT_BODY, size, 0.09f, x + width - U(9.0f), y + U(1.5f), AE_ALIGN_RIGHT, 0x99000000u, right);
		ae_draw_text_tracked(AE_FONT_BODY, size, 0.09f, x + width - U(10.0f), y, AE_ALIGN_RIGHT, AE_COLOR_MUTED, right);
	}
}

/* the place line at the content's top: earlier steps muted, the current one in the text colour, a chevron between with 8 u each side */
static void place_line(float x, float width, char const *const *parts, int count, char const *right)
{
	float size = U(16.2f), tracking = 0.09f, at = x + U(10.0f), middle = Y(258.0f), top = ae_flow_cap_top(AE_FONT_BODY, size, middle);
	int each;

	for (each = 0; each < count; each++)
	{
		unsigned int color = each == count - 1 ? AE_COLOR_TEXT : AE_COLOR_MUTED;

		if (each)
		{
			at += U(8.0f);
			ae_draw_text_tracked(AE_FONT_BODY, size, tracking, at, top, AE_ALIGN_LEFT, AE_COLOR_MUTED, CHEVRON);
			at += ae_draw_text_tracked_width(AE_FONT_BODY, size, tracking, CHEVRON) + U(8.0f);
		}
		ae_draw_text_tracked(AE_FONT_BODY, size, tracking, at + U(1.0f), top + U(2.0f), AE_ALIGN_LEFT, 0x99000000u, parts[each]);
		at += ae_draw_text_tracked(AE_FONT_BODY, size, tracking, at, top, AE_ALIGN_LEFT, color, parts[each]);
	}
	if (right && right[0])
		ae_draw_text_tracked(AE_FONT_BODY, size, tracking, x + width - U(10.0f), top, AE_ALIGN_RIGHT, AE_COLOR_ACCENT, right);
}

/* a row of a list: label and value (the value in a colour when it says something: 1 muted, 2 accent, 3 WARNING) */
static float page_row(float x, float y, float width, short hit_id, short index, char const *label, char const *value, int kind,
	unsigned int flags)
{
	struct ae_density const *d = &S.flow.density;
	struct ae_row row;
	char fitted[120];
	float height, size = ae_size_text(d);

	memset(&row, 0, sizeof(row));
	if (kind && value && value[0])
	{
		float value_width = ae_draw_text_width(AE_FONT_ROW, size, value);

		ae_fit_text(AE_FONT_ROW, size, label, width - value_width - U(60.0f), fitted, sizeof(fitted));
		row.label = fitted;
		row.flags = flags | AE_ROW_STATUS;
		height = ae_widget_row(d, x, y, width, &row, hit_id, index);
		ae_draw_text(AE_FONT_ROW, size, x + width - U(20.0f), ae_flow_cap_top(AE_FONT_ROW, size, y + height * 0.5f), AE_ALIGN_RIGHT,
			(flags & AE_ROW_FOCUSED) ? (kind == 3 ? AE_COLOR_SELECTION_WARNING : AE_COLOR_SELECTION_TEXT) :
			kind == 3 ? AE_COLOR_WARNING : kind == 2 ? AE_COLOR_ACCENT : AE_COLOR_MUTED, value);
		return height;
	}
	row.label = label;
	row.value = value && value[0] ? value : NULL;
	row.flags = flags;
	return ae_widget_row(d, x, y, width, &row, hit_id, index);
}

/* a name cut in the middle until it fits */
static void fit_middle(int font, float size, char const *text, float room, char *out, int out_size)
{
	int chars = (int)strlen(text);

	snprintf(out, (size_t)out_size, "%s", text);
	while (chars > 6 && ae_draw_text_width(font, size, out) > room)
	{
		chars--;
		ae_is_middle_cut(text, chars, out, out_size);
	}
}

/* the folders above an item as many as fit: all, else the first, "...", the last that fit */
static void fit_folders(char const *path, float size, float room, char *out, int out_size)
{
	int count = ae_is_folder_count(path), keep;

	ae_is_item_folders(path, -1, out, out_size);
	for (keep = count - 2; keep >= 0 && ae_draw_text_width(AE_FONT_BODY, size, out) > room; keep--)
		ae_is_item_folders(path, keep, out, out_size);
	if (ae_draw_text_width(AE_FONT_BODY, size, out) > room)
	{
		char cut[200];

		ae_fit_text(AE_FONT_BODY, size, out, room, cut, sizeof(cut));
		snprintf(out, (size_t)out_size, "%s", cut);
	}
}

/* a two-line row: the label over a sub-line, a value right, a check box (box: -1 none, 0 off, 1 on); the pair centred in the row */
static void row_two(float x, float y, float width, short index, unsigned int flags, char const *label, char const *sub, int sub_is_path,
	char const *value, int kind, int box, int muted)
{
	struct ae_density const *d = &S.flow.density;
	float row_height = U(50.0f), label_size = U(22.0f), sub_size = U(15.0f), value_width = 0.0f, left = x + U(20.0f), room;
	float label_cap, sub_cap, block, top;
	int on_bar = (flags & AE_ROW_FOCUSED) != 0;
	char fitted[200];

	if (value && value[0])
		value_width = ae_draw_text_width(AE_FONT_ROW, ae_size_text(d), value);
	page_row(x, y, width, HIT_LIST, index, "", value, kind, flags);
	if (box >= 0)
	{
		float side = U(20.0f), by = y + (row_height - side) * 0.5f;

		if (box)
			ae_draw_rect(left, by, side, side, U(3.0f), AE_COLOR_ACCENT);
		else
			ae_draw_outline(left, by, side, side, U(3.0f), U(2.0f), on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_TEXT);
		/* (a click on the box itself turns the item on or off) */
		if ((S.step == AE_IS_STEP_CONTENTS || S.step == AE_IS_STEP_SLOT) && S.ctl < 0 && !S.overlay)
			ae_hit_add(left - U(6.0f), by - U(6.0f), side + U(12.0f), side + U(12.0f), HIT_BOX, 0, index);
		left += side + U(16.0f);
	}
	room = x + width - left - value_width - U(28.0f);
	label_cap = ae_draw_cap_height(AE_FONT_ROW, label_size);
	sub_cap = ae_draw_cap_height(AE_FONT_BODY, sub_size);
	block = label_cap + U(4.4f) + U(8.0f) + sub_cap;
	top = y + (row_height - block) * 0.5f;
	fit_middle(AE_FONT_ROW, label_size, label, room, fitted, sizeof(fitted));
	ae_draw_text(AE_FONT_ROW, label_size, left, ae_flow_cap_top(AE_FONT_ROW, label_size, top + label_cap * 0.5f), AE_ALIGN_LEFT,
		on_bar ? AE_COLOR_SELECTION_TEXT : muted ? AE_COLOR_MUTED : AE_COLOR_TEXT, fitted);
	if (sub && sub[0])
	{
		char line[240];

		if (sub_is_path)
			fit_folders(sub, sub_size, room, line, sizeof(line));
		else
			ae_fit_text(AE_FONT_BODY, sub_size, sub, room, line, sizeof(line));
		ae_draw_text(AE_FONT_BODY, sub_size, left, ae_flow_cap_top(AE_FONT_BODY, sub_size,
			top + label_cap + U(4.4f) + U(8.0f) + sub_cap * 0.5f), AE_ALIGN_LEFT, on_bar ? AE_COLOR_SELECTION_MUTED : AE_COLOR_MUTED, line);
	}
}

/* ---------- the left column of each step */

struct list_context { int dummy; };

static void list_item(void *context, short item, float x, float y, float width, float height, unsigned int flags)
{
	char sub[160], value[48], megabytes[24];
	int tone = 0;

	(void)context;
	(void)height;
	if (S.ctl >= 0 || S.overlay)
		flags &= ~(unsigned int)(AE_ROW_FOCUSED | AE_ROW_ON_BAR);
	switch (S.step)
	{
	case AE_IS_STEP_SETS:
	{
		if (item >= S.set_count)
		{
			int home = item - S.set_count;

			if (home == HOME_NEW)
			{
				char const *limit = ae_is_limit_word(0, S.set_count);

				page_row(x, y, width, HIT_LIST, item, "NEW SET...", limit, 1, limit[0] ? flags : flags | AE_ROW_SUBSCREEN);
			}
			else if (home == HOME_HEAD)
				ae_draw_text_tracked(AE_FONT_BODY, ae_size_minor(&S.flow.density) * 0.9f, 0.09f, x + U(10.0f),
					ae_flow_cap_top(AE_FONT_BODY, ae_size_minor(&S.flow.density) * 0.9f, y + U(36.0f)), AE_ALIGN_LEFT, AE_COLOR_MUTED, "HOW SETS PLAY");
			else if (home == HOME_SWAPS)
			{
				snprintf(value, sizeof(value), "%d " CHEVRON, S.swap_count);
				row_two(x, y, width, item, flags, "CAMPAIGN SWAPS", "A set in the missions, without the randomizer", 0, value, 1, -1, 0);
			}
			else
				row_two(x, y, width, item, flags, "MULTIPLAYER SWAPS", "The host's set in a game with others", 0, "COMES WITH FORGE", 1, -1, 1);
			break;
		}
		ae_is_set_line(S.sets[item].items, S.sets[item].bytes, S.sets[item].maps, sub, sizeof(sub));
		snprintf(value, sizeof(value), "%s", S.sets[item].playing ? "PLAYING" : "OFF");
		row_two(x, y, width, item, flags, S.sets[item].name, sub, 0, value, S.sets[item].playing ? 2 : 1, -1, 0);
		break;
	}
	case AE_IS_STEP_CONTENTS:
	{
		struct content_row const *row = &S.rows[item];

		if (row->type == ROW_ADD)
		{
			char const *limit = ae_is_limit_word(1, S.entry_count);

			page_row(x, y, width, HIT_LIST, item, "ADD ITEMS FROM A MAP", limit, 1, limit[0] ? flags : flags | AE_ROW_SUBSCREEN);
		}
		else if (row->type == ROW_RANGE)
		{
			char const *wait = range_wait();

			page_row(x, y, width, HIT_LIST, item, "TRY IT ON THE FIRING RANGE", wait, 1, wait[0] ? flags : flags | AE_ROW_SUBSCREEN);
		}
		else if (row->type == ROW_HEADER)
		{
			int each, count = 0;
			char text[64];

			for (each = 0; each < S.entry_count; each++)
				count += S.entries[each].kind == row->index;
			snprintf(text, sizeof(text), "%s" DOT "%d", ae_is_kind_name(row->index), count);
			ae_draw_text_tracked(AE_FONT_BODY, ae_size_minor(&S.flow.density) * 0.9f, 0.09f, x + U(10.0f),
				ae_flow_cap_top(AE_FONT_BODY, ae_size_minor(&S.flow.density) * 0.9f, y + U(36.0f)), AE_ALIGN_LEFT, AE_COLOR_MUTED, text);
		}
		else
		{
			struct ae_is_entry const *entry = &S.entries[row->index];
			char name[128], own[128];

			ae_is_item_name(entry->path, own, sizeof(own));
			/* (a name typed for it is what the menus show; its own name goes in the sub-line) */
			snprintf(name, sizeof(name), "%s", entry->name[0] ? entry->name : own);
			if (entry->name[0])
				snprintf(sub, sizeof(sub), "%s" DOT "from %s", own, entry->map);
			else
				snprintf(sub, sizeof(sub), "from %s", entry->map);
			if (entry->failed[0])
			{
				snprintf(value, sizeof(value), "FAILED");
				tone = 3;
			}
			else if (entry->on && entry->kind == 4 && ae_is_body_is(S.body_biped, entry->path))
			{
				snprintf(value, sizeof(value), "YOUR BODY");
				tone = 2;
			}
			else
			{
				snprintf(value, sizeof(value), "%s", entry->on ? "ON" : "OFF");
				tone = entry->on ? 2 : 1;
			}
			row_two(x, y, width, item, flags, name, sub, 0, value, tone, entry->on && !entry->failed[0], 0);
		}
		break;
	}
	case AE_IS_STEP_SETTINGS:
	{
		struct setting_row const *row = &S.srows[item];
		int row_tone;

		if (row->header)
		{
			ae_draw_text_tracked(AE_FONT_BODY, ae_size_minor(&S.flow.density) * 0.9f, 0.09f, x + U(10.0f),
				ae_flow_cap_top(AE_FONT_BODY, ae_size_minor(&S.flow.density) * 0.9f, y + U(36.0f)), AE_ALIGN_LEFT, AE_COLOR_MUTED,
				section_names[row->row]);
			break;
		}
		setting_text(row->row, value, sizeof(value), &row_tone);
		if (row->row == AE_IS_ROW_REMOVE)
			page_row(x, y, width, HIT_LIST, item, ae_is_row_label(row->row), NULL, 0, flags | AE_ROW_DESTRUCTIVE_FRAMED);
		else if (row_tone == 3)
			page_row(x, y, width, HIT_LIST, item, ae_is_row_label(row->row), value, 3, flags);
		else if (row_tone == 2)
			page_row(x, y, width, HIT_LIST, item, ae_is_row_label(row->row), value, 1, flags);
		else if (row_tone == 1)
			page_row(x, y, width, HIT_LIST, item, ae_is_row_label(row->row), value, 0, flags | AE_ROW_SUBSCREEN);
		else
			page_row(x, y, width, HIT_LIST, item, ae_is_row_label(row->row), value, 0, flags);
		break;
	}
	case AE_IS_STEP_SWAPS:
	{
		if (item >= S.swap_count)
		{
			char const *wait = ae_is_limit_word(2, S.swap_count);
			int none_on[AE_IS_SETS_MOST];

			if (!wait[0] && !sets_with_items(none_on, AE_IS_SETS_MOST))
				wait = "NO SET WITH ITEMS ON";
			page_row(x, y, width, HIT_LIST, item, "NEW SWAP...", wait, 1, wait[0] ? flags : flags | AE_ROW_SUBSCREEN);
			break;
		}
		else
		{
			int state = swap_state_of(&S.swaps[item]);

			ae_is_swap_sub(S.swaps[item].set_found, S.swaps[item].set_name, S.swaps[item].weapon_replace, S.swaps[item].enemy_replace, sub, sizeof(sub));
			row_two(x, y, width, item, flags, S.swaps[item].name, sub, 0, ae_is_swap_state_word(state), ae_is_swap_state_tone(state), -1, 0);
		}
		break;
	}
	case AE_IS_STEP_SWAP:
	{
		struct swap_line const *line = &S.swlines[item];
		int row_tone;

		if (line->header)
		{
			ae_draw_text_tracked(AE_FONT_BODY, ae_size_minor(&S.flow.density) * 0.9f, 0.09f, x + U(10.0f),
				ae_flow_cap_top(AE_FONT_BODY, ae_size_minor(&S.flow.density) * 0.9f, y + U(36.0f)), AE_ALIGN_LEFT, AE_COLOR_MUTED,
				ae_is_swap_section_name(line->row));
			break;
		}
		swap_text(line->row, line->slot, value, sizeof(value), &row_tone);
		if (line->row == AE_IS_SW_WEAPON_SLOT || line->row == AE_IS_SW_ENEMY_SLOT)
		{
			char label[64];

			slot_name(line->row, line->slot, 0, label, sizeof(label));
			page_row(x, y, width, HIT_LIST, item, label, value, 0, flags | AE_ROW_SUBSCREEN);
		}
		else if (row_tone == 3)
			page_row(x, y, width, HIT_LIST, item, ae_is_swap_row_label(line->row), value, 3, flags);
		else if (row_tone == 2)
			page_row(x, y, width, HIT_LIST, item, ae_is_swap_row_label(line->row), value, 1, flags);
		else if (row_tone == 1)
			page_row(x, y, width, HIT_LIST, item, ae_is_swap_row_label(line->row), value, 0, flags | AE_ROW_SUBSCREEN);
		else
			page_row(x, y, width, HIT_LIST, item, ae_is_swap_row_label(line->row), value, 0, flags);
		break;
	}
	case AE_IS_STEP_RANKS:
	{
		int rank = tag_campaign_enemy_rank((short)S.slot_slot, (short)item);
		char label[64];

		tag_campaign_slot_name((short)S.slot_slot, (short)rank, label, sizeof(label));
		ae_is_slot_value(slot_picks(AE_IS_SW_ENEMY_SLOT, S.slot_slot, rank), value, sizeof(value));
		page_row(x, y, width, HIT_LIST, item, label, value, 0, flags | AE_ROW_SUBSCREEN);
		break;
	}
	case AE_IS_STEP_SLOT:
	{
		struct slot_line const *line = &S.slines[item];

		if (line->head)
		{
			ae_draw_text_tracked(AE_FONT_BODY, ae_size_minor(&S.flow.density) * 0.9f, 0.09f, x + U(10.0f),
				ae_flow_cap_top(AE_FONT_BODY, ae_size_minor(&S.flow.density) * 0.9f, y + U(36.0f)), AE_ALIGN_LEFT, AE_COLOR_MUTED, line->label);
			break;
		}
		row_two(x, y, width, item, flags, line->label, line->sub, 0, "", 0, line->on, 0);
		break;
	}
	case AE_IS_STEP_MAPS:
	{
		struct ae_is_map const *map = &S.maps[S.view[item]];
		char const *reason = S.be->maps_reason(map->source);

		snprintf(sub, sizeof(sub), "%s%s%s", map->file, map->where[0] ? DOT : "", map->where);
		value[0] = 0;
		if (S.op == OP_OPEN_MAP && S.op_row == item)
		{
			snprintf(value, sizeof(value), "OPENING...");
			tone = 2;
		}
		else if (reason)
		{
			snprintf(value, sizeof(value), "CAN'T NOW");
			tone = 1;
		}
		else if (map->in_set > 0)
		{
			snprintf(value, sizeof(value), "%ld IN THE SET", map->in_set);
			tone = 2;
		}
		else if (map->campaign)
		{
			snprintf(value, sizeof(value), "CAMPAIGN");
			tone = 1;
		}
		row_two(x, y, width, item, flags, map->name, sub, 0, value, tone, -1, reason != NULL);
		break;
	}
	case AE_IS_STEP_KINDS:
	{
		char count[24];

		snprintf(count, sizeof(count), "%ld", S.kind_counts[item]);
		row_two(x, y, width, item, flags, ae_is_kind_name(item), ae_is_kind_blurb(item), 0, S.kind_counts[item] > 0 ? count : "NONE",
			1, -1, S.kind_counts[item] == 0);
		break;
	}
	default:
	{
		struct ae_is_item const *entry = &S.items[S.item_view[item]];
		char name[128];

		ae_is_item_name(entry->path, name, sizeof(name));
		value[0] = 0;
		if (S.op == OP_ADD && S.op_row == item)
		{
			snprintf(value, sizeof(value), "ADDING...");
			tone = 2;
		}
		else if (entry->in_set)
		{
			snprintf(value, sizeof(value), "IN THE SET");
			tone = 2;
		}
		else if (!S.be->kind_addable(S.kind) || (entry->parts >= 0 && entry->refused))
		{
			snprintf(value, sizeof(value), "CAN'T ADD");
			tone = 3;
		}
		else if (entry->parts >= 0)
		{
			snprintf(megabytes, sizeof(megabytes), "%ld PART%s", entry->parts, entry->parts == 1 ? "" : "S");
			snprintf(value, sizeof(value), "%s", megabytes);
			tone = 1;
		}
		row_two(x, y, width, item, flags, name, entry->path, 1, value, tone, -1, 0);
		break;
	}
	}
}

static void page_list(float x, float y, float width)
{
	float bottom = ae_flow_content_bottom(&S.flow) - U(96.0f);
	struct list_context context = { 0 };

	if (S.list.list.count != S.list_count)
		ae_list_set_count(&S.list.list, S.list_count);
	if (S.ctl >= 0)
		S.list.hover = -1;
	ae_widget_list(&S.flow.density, &S.list, x, y, width, bottom - y, list_item, &context, HIT_LIST);
}

static float search_field(float x, float y, float width, char const *placeholder)
{
	struct ae_field field;

	memset(&field, 0, sizeof(field));
	field.label = "SEARCH";
	field.placeholder = placeholder;
	field.text = &S.search;
	field.flags = (S.ctl == CTL_SEARCH ? AE_FIELD_FOCUSED : 0) | (S.typing || S.keyboard ? AE_FIELD_EDITING : 0);
	field.caret_since = S.caret_since;
	ae_field_well(&S.flow.density, x, y, width, &field, &S.well);
	return ae_widget_field(&S.flow.density, x, y, width, &field, HIT_FIELD);
}

static char const *show_name(int show)
{
	static char const *const names[SHOWS] = { "ALL", "CUSTOM EDITION", "MCC", "WORKSHOP" };

	return names[show];
}

/* ---------- the cards */

static void how_it_works_card(float x, float y, float width, float height)
{
	static char const *const heads[3] = { "MAKE A SET", "ADD ITEMS FROM A MAP", "PLAY THE SET" };
	static char const *const bodies[3] = { "Name it after what goes in it.", "Open a Custom Edition, MCC or Workshop map and pick what to take.",
		"Its items join the campaign levels you play alone, through the randomizer." };
	float cy = y + U(26.0f), ring = U(26.0f);
	int each;

	card_frame(x, y, width, height, 0);
	cy += card_label(x, cy, "HOW IT WORKS");
	for (each = 0; each < 3; each++)
	{
		char digit[4];
		float left = x + U(AE_FLOW_INSET_U);

		snprintf(digit, sizeof(digit), "%d", each + 1);
		ae_draw_outline(left, cy, ring, ring, ring * 0.5f, U(2.0f), AE_COLOR_ACCENT);
		ae_draw_text(AE_FONT_ROW, U(16.0f), left + ring * 0.5f, ae_flow_cap_top(AE_FONT_ROW, U(16.0f), cy + ring * 0.5f), AE_ALIGN_CENTER,
			AE_COLOR_ACCENT, digit);
		ae_draw_text(AE_FONT_ROW, U(20.0f), left + ring + U(14.0f), cy, AE_ALIGN_LEFT, AE_COLOR_TITLE, heads[each]);
		card_body(x + ring + U(14.0f), cy + U(30.0f), width, bodies[each], 2, AE_COLOR_TEXT);
		cy += U(96.0f);
	}
	card_body(x, cy, width, "The maps stay as they are: items are copied, never moved.", 2, AE_COLOR_MUTED);
}

static void sets_card(float x, float y, float width, float height)
{
	struct ae_is_set const *set = set_focused();
	char line[160], text[200];
	long other = 0, shown;
	float cy;
	static int const order[4] = { 0, 1, 3, 4 };
	int each;

	if (!set)
	{
		if (home_row() == HOME_SWAPS || home_row() == HOME_MULTIPLAYER)
			home_card(x, y, width, height);
		else
			how_it_works_card(x, y, width, height);
		return;
	}
	card_frame(x, y, width, height, 0);
	cy = y + U(26.0f);
	cy += card_title(x, cy, width, set->name);
	ae_is_set_state(set->playing, set->items, set->bytes, line, sizeof(line));
	cy += card_state(x, cy, line, set->playing ? AE_COLOR_ACCENT : AE_COLOR_MUTED);
	for (each = 0; each < AE_IS_KINDS; each++)
		other += set->kinds[each];
	for (each = 0; each < 4; each++)
	{
		shown = set->kinds[order[each]];
		if (shown > 0)
		{
			snprintf(text, sizeof(text), "%ld", shown);
			cy += card_fact(x, cy, width, ae_is_kind_name(order[each]), text, AE_COLOR_TITLE);
			other -= shown;
		}
	}
	if (other > 0)
	{
		snprintf(text, sizeof(text), "%ld", other);
		cy += card_fact(x, cy, width, "EVERYTHING ELSE", text, AE_COLOR_TITLE);
	}
	if (set->maps > 0)
	{
		if (set->maps > 1)
			snprintf(text, sizeof(text), "%ld: %s and %ld more", set->maps, set->first_map, set->maps - 1);
		else
			snprintf(text, sizeof(text), "1: %s", set->first_map);
		cy += card_fact(x, cy, width, "TAKEN FROM", text, AE_COLOR_TITLE);
	}
	cy += U(12.0f);
	cy += card_body(x, cy, width, ae_is_plays_words(S.be->randomizer_on()), 4, set->playing && !S.be->randomizer_on() ?
		AE_COLOR_WARNING : AE_COLOR_TEXT) + U(14.0f);
	cy += card_label(x, cy, "WITH OTHER PLAYERS");
	card_body(x, cy, width, ae_is_others_words(), 2, AE_COLOR_TEXT);
}

static unsigned int kind_state_color(int on, int failed)
{
	return failed ? AE_COLOR_WARNING : on ? AE_COLOR_ACCENT : AE_COLOR_MUTED;
}

static float body_card(float x, float y, float width, struct ae_is_entry const *entry);
static float settings_lines(float x, float y, float width, struct ae_is_entry const *entry);

static void contents_card(float x, float y, float width, float height)
{
	struct ae_is_entry const *entry = entry_focused();
	char name[128], line[200], text[200], size[32];
	float cy;

	if (!entry && S.list.list.focus >= 0 && S.list.list.focus < S.row_count && S.rows[S.list.list.focus].type == ROW_RANGE)
	{
		range_card(x, y, width, height);
		return;
	}
	if (!entry)
	{
		card_frame(x, y, width, height, 0);
		cy = y + U(26.0f);
		cy += card_title(x, cy, width, "ADD ITEMS");
		card_body(x, cy, width, "Pick a map on this PC, then a kind of item, then the item. It is copied into this set; the map does not change.", 4,
			AE_COLOR_TEXT);
		return;
	}
	card_frame(x, y, width, height, entry->failed[0]);
	cy = y + U(26.0f);
	ae_is_item_name(entry->path, name, sizeof(name));
	cy += card_item_name(x, cy, width, name);
	if (entry->failed[0])
		snprintf(line, sizeof(line), "%s " DOT "TURNED OFF", ae_is_kind_word(entry->kind));
	else
	{
		upper_copy(text, sizeof(text), entry->map);
		if (entry->on && entry->kind == 4 && ae_is_body_is(S.body_biped, entry->path))
		{
			char set_upper[64];

			upper_copy(set_upper, sizeof(set_upper), S.set_name);
			snprintf(line, sizeof(line), "%s" DOT "ON" DOT "YOUR BODY IN %s", ae_is_kind_word(entry->kind), set_upper);
		}
		else
			snprintf(line, sizeof(line), "%s" DOT "%s" DOT "FROM %s", ae_is_kind_word(entry->kind), entry->on ? "ON" : "OFF", text);
	}
	cy += card_state(x, cy, line, kind_state_color(entry->on, entry->failed[0]));
	if (entry->failed[0])
	{
		ae_is_failed_words(entry->failed, text, sizeof(text));
		cy += card_body(x, cy, width, text, 4, AE_COLOR_TEXT) + U(8.0f);
		cy += card_label(x, cy, "WHAT TO DO");
		ae_is_failed_todo(entry->failed, text, sizeof(text));
		cy += card_body(x, cy, width, text, 3, AE_COLOR_TEXT) + U(10.0f);
	}
	cy += card_fact(x, cy, width, "FROM", entry->map, AE_COLOR_TITLE);
	if (entry->bytes && entry->kind != 4)
	{
		ae_hub_size_text(entry->bytes, size, sizeof(size));
		cy += card_fact(x, cy, width, "IN THE SET", size, AE_COLOR_TITLE);
	}
	cy += card_fact(x, cy, width, "WORKS", entry->works ? "It has every part it needs" : "A part it needs is missing",
		entry->works ? AE_COLOR_TITLE : AE_COLOR_WARNING);
	cy += U(10.0f);
	if (!entry->failed[0] && entry->kind == 4)
	{
		/* (the player model card: its path is on its settings page) */
		cy += body_card(x, cy, width, entry);
	}
	else if (!entry->failed[0])
	{
		cy += settings_lines(x, cy, width, entry) + U(6.0f);
		card_path(x, cy, width, entry->path);
	}
}

/* the facts of a biped's fit as card lines: whether it fits, and what it is measured by (the rule's own words for the reasons) */
static float fit_facts(float x, float y, float width, struct ae_is_fit const *fit)
{
	char text[96];
	float cy = y;

	if (!fit->known)
	{
		cy += card_fact(x, cy, width, "FITS A PLAYER", "NOT CHECKED YET", AE_COLOR_MUTED);
		return cy - y;
	}
	cy += card_fact(x, cy, width, "FITS A PLAYER", fit->fits ? "Yes" : "No", fit->fits ? AE_COLOR_ACCENT : AE_COLOR_WARNING);
	if (fit->size_percent > 0)
	{
		snprintf(text, sizeof(text), "%d %% of the stock player", fit->size_percent);
		cy += card_fact(x, cy, width, "SIZE", text, fit->size_percent >= (int)(AE_BIPED_SIZE_RATIO_MINIMUM * 100.0f) &&
			fit->size_percent <= (int)(AE_BIPED_SIZE_RATIO_MAXIMUM * 100.0f) ? AE_COLOR_TITLE : AE_COLOR_WARNING);
		cy += card_fact(x, cy, width, "PLAYER PHYSICS", fit->physics ? "Yes" : "No", fit->physics ? AE_COLOR_TITLE : AE_COLOR_WARNING);
		if (fit->weapons_total > 0)
		{
			if (fit->weapons_held == fit->weapons_total)
				snprintf(text, sizeof(text), "All %d campaign weapons", fit->weapons_total);
			else
				snprintf(text, sizeof(text), "%d of %d campaign weapons", fit->weapons_held, fit->weapons_total);
			cy += card_fact(x, cy, width, "HOLDS", text, fit->weapons_held == fit->weapons_total ? AE_COLOR_TITLE : AE_COLOR_WARNING);
		}
		if (fit->seats_total > 0)
		{
			if (fit->seats_have == fit->seats_total)
				snprintf(text, sizeof(text), "All %d", fit->seats_total);
			else
				snprintf(text, sizeof(text), "%d of %d", fit->seats_have, fit->seats_total);
			cy += card_fact(x, cy, width, "VEHICLE SEATS", text, fit->seats_have == fit->seats_total ? AE_COLOR_TITLE : AE_COLOR_WARNING);
		}
	}
	return cy - y;
}

/* what a set's body is while its biped is focused in the items: read from the set's file once a focus */
static void card_body_read(struct ae_is_entry const *entry)
{
	if (S.card_valid && S.card_tag == entry->tag)
		return;
	S.card_tag = entry->tag;
	S.card_valid = 1;
	memset(&S.card_fit, 0, sizeof(S.card_fit));
	S.be->entry_fit(S.folder, entry->tag, &S.card_fit);
	if (!S.be->body_option(S.folder, "player_hands", S.card_hands, sizeof(S.card_hands)))
		S.card_hands[0] = 0;
	if (!S.be->body_option(S.folder, "player_movement", S.card_movement, sizeof(S.card_movement)))
		S.card_movement[0] = 0;
}

/* the first letter a capital, the rest lower case (a fixed word of the page said as the card's other facts are) */
static void sentence_word(char *text)
{
	int each;

	for (each = 1; text[each]; each++)
		text[each] = (char)tolower((unsigned char)text[each]);
}

/* an item's shown name: the one typed for it, else its own from the map's path */
static void shown_name(struct ae_is_entry const *entry, char *name, int size)
{
	char own[128];

	ae_is_item_name(entry->path, own, sizeof(own));
	snprintf(name, (size_t)size, "%s", entry->name[0] ? entry->name : own);
}

/* a body's words: hands and movement as the page names them (the fixed words in sentence case, as the card's other facts) */
static void body_words(char const *hands, char const *movement, char *hands_text, int hands_size, char *movement_text, int movement_size)
{
	int each;

	snprintf(hands_text, (size_t)hands_size, "%s", !strcmp(hands, "campaign") ? ae_is_hands_word(1) : ae_is_hands_word(0));
	sentence_word(hands_text);
	for (each = 0; each < S.entry_count; each++)
		if (S.entries[each].kind == 8)
		{
			char key[320];

			ae_is_hands_key(S.entries[each].path, S.entries[each].map, key, sizeof(key));
			if (!strcmp(hands, key))
				shown_name(&S.entries[each], hands_text, hands_size);
		}
	snprintf(movement_text, (size_t)movement_size, "%s", ae_is_value_word(AE_IS_ROW_MOVEMENT, 4, ae_is_value_index(AE_IS_ROW_MOVEMENT, 4, movement)));
	sentence_word(movement_text);
}

/* whether the set has hands of its own (the hands a biped brings) */
static int set_has_hands(void)
{
	int each;

	for (each = 0; each < S.entry_count; each++)
		if (S.entries[each].kind == 8)
			return 1;
	return 0;
}

/* the player model card (a biped focused in a set's items): FITS A PLAYER, whether you play as it, its hands and its movement */
static float body_card(float x, float y, float width, struct ae_is_entry const *entry)
{
	char text[240], hands[96], movement[48], other[96], name[128];
	float cy = y;
	int is_body = entry->on && ae_is_body_is(S.body_biped, entry->path);

	card_body_read(entry);
	shown_name(entry, name, sizeof(name));
	cy += card_label(x, cy, "YOUR BODY");
	if (!S.card_fit.known)
		cy += card_fact(x, cy, width, "FITS A PLAYER", "NOT CHECKED YET", AE_COLOR_MUTED);
	else
		cy += card_fact(x, cy, width, "FITS A PLAYER", S.card_fit.fits ? "Yes" : "No", S.card_fit.fits ? AE_COLOR_ACCENT : AE_COLOR_WARNING);
	if (is_body)
		snprintf(text, sizeof(text), "Yes");
	else if (S.body_biped[0])
	{
		ae_is_item_name(S.body_biped, other, sizeof(other));
		snprintf(text, sizeof(text), "No: %s is this set's body", other);
	}
	else
		snprintf(text, sizeof(text), "No");
	cy += card_fact(x, cy, width, "YOU PLAY AS IT", text, is_body ? AE_COLOR_ACCENT : AE_COLOR_TITLE);
	if (is_body)
	{
		body_words(S.card_hands, S.card_movement, hands, sizeof(hands), movement, sizeof(movement));
		cy += card_fact(x, cy, width, "HANDS", hands, AE_COLOR_TITLE);
		cy += card_fact(x, cy, width, "MOVEMENT", movement, AE_COLOR_TITLE);
	}
	cy += card_fact(x, cy, width, "CARRIED", set_has_hands() ? "Its first-person hands" : "The campaign's hands", AE_COLOR_TITLE);
	snprintf(text, sizeof(text), "While %s plays, you and split-screen players start every mission as %s. A campaign swap of this set can pick another body.",
		S.set_name, name);
	cy += U(6.0f);
	cy += card_body(x, cy, width, text, 3, AE_COLOR_TEXT) + U(4.0f);
	cy += card_body(x, cy, width, "AI that use it keep it. Multiplayer games: GAME OPTIONS " CHEVRON " PLAYER MODEL.", 2, AE_COLOR_MUTED);
	return cy - y;
}

/* an AUTO value in words with what AUTO found ("Human (auto)"), else the value's word in sentence case */
static void value_now(int row, int kind, struct ae_is_entry const *entry, char const *key, char *text, int size)
{
	char answer[80];
	int index = ae_is_value_index(row, kind, key);

	if (index == 0 && ae_is_auto_answer(row, kind, entry->path, entry->name, answer, (int)sizeof(answer)))
	{
		sentence_word(answer);
		snprintf(text, (size_t)size, "%s (auto)", answer);
	}
	else
	{
		snprintf(text, (size_t)size, "%s", ae_is_value_word(row, kind, index));
		sentence_word(text);
	}
}

/* ITS SETTINGS on a set's items card: what the randomizer pools it joins, its group, its HUD or its kind of enemy, its place in the campaign;
always shown for a weapon or an AI, the others only the place when it is known */
static float settings_lines(float x, float y, float width, struct ae_is_entry const *entry)
{
	char faction[32] = "", klass[32] = "", type[32] = "", group[64] = "", hud[32] = "", replace[16] = "", text[160], name[128];
	float cy = y;
	int begun = 0;

	S.be->entry_option(S.folder, entry->tag, "faction", faction, sizeof(faction));
	S.be->entry_option(S.folder, entry->tag, "class", klass, sizeof(klass));
	S.be->entry_option(S.folder, entry->tag, "type", type, sizeof(type));
	S.be->entry_option(S.folder, entry->tag, "custom_type", group, sizeof(group));
	S.be->entry_option(S.folder, entry->tag, "hud", hud, sizeof(hud));
	S.be->entry_option(S.folder, entry->tag, "replace", replace, sizeof(replace));
	shown_name(entry, name, sizeof(name));
	if (entry->kind == 0 || entry->kind == 1)
	{
		cy += card_label(x, cy, "ITS SETTINGS");
		begun = 1;
		ae_is_pools_words(entry->kind, entry->path, entry->name, faction, klass, type, 1, text, sizeof(text));
		cy += card_fact(x, cy, width, "POOLS", text, AE_COLOR_ACCENT);
		if (entry->kind == 1)
		{
			value_now(AE_IS_ROW_COUNTS_AS, 1, entry, type, text, sizeof(text));
			cy += card_fact(x, cy, width, "COUNTS AS", text, AE_COLOR_TITLE);
		}
		cy += card_fact(x, cy, width, "GROUP", group[0] ? group : name, AE_COLOR_TITLE);
		if (entry->kind == 0)
		{
			value_now(AE_IS_ROW_HUD, 0, entry, hud, text, sizeof(text));
			cy += card_fact(x, cy, width, "HUD", text, AE_COLOR_TITLE);
		}
	}
	if (S.be->replace_known())
	{
		if (!begun)
			cy += card_label(x, cy, "ITS SETTINGS");
		cy += card_fact(x, cy, width, "TAKES THE CAMPAIGN'S PLACE", replace[0] ? "Yes" : "No", replace[0] ? AE_COLOR_ACCENT : AE_COLOR_TITLE);
	}
	return cy - y;
}

/* an item's settings page's card: the item's head, what the focused row means (with NOW, the pools it joins and its choices for a value row)
and for a biped what the fitness check found (the card turns to the WARNING stripe, with why in plain words and what to do, when it does not fit) */
static void settings_card(float x, float y, float width, float height)
{
	struct ae_is_entry const *entry = settings_entry();
	char name[128], line[240], text[320], size[32], upper[64], other[96];
	float cy;
	int row = setting_focused(), nofit;

	if (!entry)
	{
		card_frame(x, y, width, height, 0);
		return;
	}
	nofit = S.set_kind == 4 && S.fit.known && !S.fit.fits;
	card_frame(x, y, width, height, entry->failed[0] || nofit);
	cy = y + U(26.0f);
	shown_name(entry, name, sizeof(name));
	cy += card_item_name(x, cy, width, name);
	upper_copy(upper, sizeof(upper), entry->map);
	if (entry->failed[0])
		snprintf(line, sizeof(line), "%s" DOT "TURNED OFF", ae_is_kind_word(entry->kind));
	else if (nofit)
		snprintf(line, sizeof(line), "%s" DOT "DOESN'T FIT A PLAYER", ae_is_kind_word(entry->kind));
	else if (entry->kind == 4 && entry->on && play_as_on())
	{
		char set_upper[64];

		upper_copy(set_upper, sizeof(set_upper), S.set_name);
		snprintf(line, sizeof(line), "%s" DOT "ON" DOT "YOUR BODY IN %s", ae_is_kind_word(entry->kind), set_upper);
	}
	else
		snprintf(line, sizeof(line), "%s" DOT "%s" DOT "FROM %s", ae_is_kind_word(entry->kind), entry->on ? "ON" : "OFF", upper);
	cy += card_state(x, cy, line, entry->failed[0] || nofit ? AE_COLOR_WARNING : entry->on ? AE_COLOR_ACCENT : AE_COLOR_MUTED);
	if (entry->failed[0])
	{
		ae_is_failed_words(entry->failed, text, sizeof(text));
		cy += card_body(x, cy, width, text, 4, AE_COLOR_TEXT) + U(8.0f);
		cy += card_label(x, cy, "WHAT TO DO");
		ae_is_failed_todo(entry->failed, text, sizeof(text));
		cy += card_body(x, cy, width, text, 3, AE_COLOR_TEXT) + U(8.0f);
	}
	else if (nofit)
	{
		/* (why in plain words, from the facts; the facts with the failing ones in the warning colour; and what to do) */
		ae_is_fit_words(&S.fit, text, sizeof(text));
		cy += card_body(x, cy, width, text, 3, AE_COLOR_TEXT) + U(6.0f);
		cy += fit_facts(x, cy, width, &S.fit);
		cy += U(6.0f);
		cy += card_label(x, cy, "WHAT TO DO");
		if (S.body_biped[0] && !play_as_on())
		{
			ae_is_item_name(S.body_biped, other, sizeof(other));
			snprintf(text, sizeof(text), "Play as %s, or keep %s for the AI: an AI that uses it still plays it.", other, name);
		}
		else
			snprintf(text, sizeof(text), "Play as another biped of the set, or keep %s for the AI: an AI that uses it still plays it.", name);
		cy += card_body(x, cy, width, text, 3, AE_COLOR_TEXT);
		return;
	}
	if (row >= 0 && row != AE_IS_ROW_NAME && row != AE_IS_ROW_ON && row != AE_IS_ROW_REMOVE)
	{
		cy += card_label(x, cy, ae_is_row_label(row));
		snprintf(text, sizeof(text), "%s", ae_is_row_card_text(row));
		if (row == AE_IS_ROW_PLAY_AS)
		{
			/* (the other body of the set is named; with none, there is nothing to turn off) */
			if (S.body_biped[0] && !play_as_on())
			{
				ae_is_item_name(S.body_biped, other, sizeof(other));
				snprintf(text, sizeof(text), "ON: you and split-screen players start every mission as it, while %s plays. One body per set: turning it on "
					"here turns %s's off.", S.set_name, other);
			}
			else
				snprintf(text, sizeof(text), "ON: you and split-screen players start every mission as it, while %s plays.", S.set_name);
		}
		else if (row == AE_IS_ROW_HANDS || row == AE_IS_ROW_MOVEMENT)
			snprintf(text, sizeof(text), "Stored with the set; the next build plays it.");
		cy += card_body(x, cy, width, text, 4, AE_COLOR_TEXT) + U(6.0f);
		if (row == AE_IS_ROW_FACTION || row == AE_IS_ROW_CLASS || row == AE_IS_ROW_COUNTS_AS || row == AE_IS_ROW_HUD || row == AE_IS_ROW_REPLACE)
		{
			/* (a value row: what it is now, the pools it joins, and what it can be) */
			if (row == AE_IS_ROW_REPLACE)
				snprintf(text, sizeof(text), "%s", S.value[row][0] ? "Yes" : "No");
			else
				value_now(row, S.set_kind, entry, S.value[row], text, sizeof(text));
			cy += card_fact(x, cy, width, "NOW", text, AE_COLOR_TITLE);
			if (row != AE_IS_ROW_HUD && row != AE_IS_ROW_REPLACE && (S.set_kind == 0 || S.set_kind == 1))
			{
				ae_is_pools_words(S.set_kind, entry->path, entry->name, S.value[AE_IS_ROW_FACTION], S.value[AE_IS_ROW_CLASS], S.value[AE_IS_ROW_COUNTS_AS],
					0, text, sizeof(text));
				cy += card_fact(x, cy, width, "POOLS IT JOINS", text, AE_COLOR_ACCENT);
			}
			{
				int each, count = ae_is_value_count(row, S.set_kind);

				text[0] = 0;
				for (each = 0; each < count; each++)
				{
					if (each)
						strncat(text, DOT, sizeof(text) - strlen(text) - 1);
					strncat(text, ae_is_value_word(row, S.set_kind, each), sizeof(text) - strlen(text) - 1);
				}
				cy += card_label(x, cy + U(4.0f), "CHOICES") + U(4.0f);
				cy += card_body(x, cy, width, text, 3, AE_COLOR_MUTED);
			}
			return;
		}
	}
	if (S.set_kind == 4)
	{
		cy += fit_facts(x, cy, width, &S.fit);
		if (S.fit.known && S.fit.fits)
			card_body(x, cy + U(8.0f), width, "AI that use it keep it. Multiplayer games: GAME OPTIONS " CHEVRON " PLAYER MODEL.", 2, AE_COLOR_MUTED);
		else if (!S.fit.known)
			card_body(x, cy + U(8.0f), width, "A level checks it the first time you play as it. One that does not fit is not used.", 3, AE_COLOR_MUTED);
		return;
	}
	if (row == AE_IS_ROW_NAME || row == AE_IS_ROW_ON || row < 0)
	{
		cy += card_fact(x, cy, width, "FROM", entry->map, AE_COLOR_TITLE);
		if (entry->bytes)
		{
			ae_hub_size_text(entry->bytes, size, sizeof(size));
			cy += card_fact(x, cy, width, "IN THE SET", size, AE_COLOR_TITLE);
		}
		cy += card_fact(x, cy, width, "WORKS", entry->works ? "It has every part it needs" : "A part it needs is missing",
			entry->works ? AE_COLOR_TITLE : AE_COLOR_WARNING);
		cy += U(10.0f);
	}
	if (!S.replace_known)
		cy += card_body(x, cy, width, "TAKES THE CAMPAIGN'S PLACE appears once this PC has read the campaign's items.", 3, AE_COLOR_MUTED);
	else
		card_body(x, cy, width, "Changes play from the next mission you load.", 2, AE_COLOR_MUTED);
}

static void maps_card(float x, float y, float width, float height)
{
	struct ae_is_map const *map = map_focused();
	char const *reason = map ? S.be->maps_reason(map->source) : NULL;
	char line[200], size[32];
	float cy;

	if (!map)
	{
		card_frame(x, y, width, height, 0);
		return;
	}
	card_frame(x, y, width, height, reason != NULL);
	cy = y + U(26.0f);
	if (reason)
	{
		static char const *const kinds[3] = { "Custom Edition", "MCC", "Workshop" };

		snprintf(line, sizeof(line), "%s maps can't open now", kinds[map->source]);
		ae_draw_text(AE_FONT_ROW, U(26.0f), x + U(AE_FLOW_INSET_U), cy, AE_ALIGN_LEFT, AE_COLOR_TITLE, line);
		cy += U(48.0f);
		cy += card_body(x, cy, width, reason, 4, AE_COLOR_TEXT) + U(14.0f);
		cy += card_label(x, cy, "WHAT TO DO");
		card_body(x, cy, width, map->source == AE_IS_SOURCE_CE ? "Leave the game, then come back. MCC and Workshop maps open now: change SHOW." :
			"Leave the game, then come back. Custom Edition maps open now: change SHOW.", 4, AE_COLOR_TEXT);
		return;
	}
	cy += card_title(x, cy, width, map->name);
	cy += U(12.0f);
	snprintf(line, sizeof(line), "%s%s%s", map->source == AE_IS_SOURCE_CE ? "Custom Edition" : map->source == AE_IS_SOURCE_MCC ? "MCC" :
		"Workshop", map->where[0] ? DOT : "", map->where);
	cy += card_fact(x, cy, width, "SOURCE", line, AE_COLOR_TITLE);
	if (map->size)
	{
		ae_hub_size_text(map->size, size, sizeof(size));
		cy += card_fact(x, cy, width, "SIZE", size, AE_COLOR_TITLE);
	}
	cy += card_fact(x, cy, width, "KIND", map->campaign ? "Campaign" : "Multiplayer", AE_COLOR_TITLE);
	if (map->in_set > 0)
	{
		char label[80], upper[60];

		upper_copy(upper, sizeof(upper), S.set_name);
		snprintf(label, sizeof(label), "IN %s", upper);
		snprintf(line, sizeof(line), "%ld item%s from it", map->in_set, map->in_set == 1 ? "" : "s");
		cy += card_fact(x, cy, width, label, line, AE_COLOR_ACCENT);
	}
	cy += U(12.0f);
	card_body(x, cy, width, "A opens it and lists what it has. The map itself does not change.", 3, AE_COLOR_TEXT);
}

static void kinds_card(float x, float y, float width, float height)
{
	int kind = S.list.list.focus >= 0 && S.list.list.focus < AE_IS_KINDS ? S.list.list.focus : 0;
	char line[200], text[200];
	float cy;

	card_frame(x, y, width, height, 0);
	cy = y + U(26.0f);
	cy += card_title(x, cy, width, ae_is_kind_name(kind));
	snprintf(line, sizeof(line), "%ld IN THIS MAP" DOT "%ld IN %s", S.kind_counts[kind], map_items_in_set(), "THE SET");
	if (S.map_open >= 0 && S.map_open < S.map_count)
		cy += card_state(x, cy, line, AE_COLOR_MUTED);
	snprintf(text, sizeof(text), "%s.%s", ae_is_kind_blurb(kind), kind == 0 ?
		" A weapon a player can't hold (a turret's gun) can't be added on its own: its vehicle brings it." : "");
	cy += card_body(x, cy, width, text, 4, AE_COLOR_TEXT) + U(10.0f);
	if (S.map_open >= 0 && S.map_open < S.map_count)
	{
		cy += card_fact(x, cy, width, "MAP", S.maps[S.map_open].file, AE_COLOR_TITLE);
		cy += card_fact(x, cy, width, "SOURCE", S.maps[S.map_open].source == AE_IS_SOURCE_CE ? "Custom Edition" :
			S.maps[S.map_open].source == AE_IS_SOURCE_MCC ? "MCC" : "Workshop", AE_COLOR_TITLE);
	}
	cy += card_fact(x, cy, width, "ADDING TO", S.set_name, AE_COLOR_ACCENT);
	cy += U(12.0f);
	card_body(x, cy, width, "B closes the map. Nothing in it changes: items are copied into the set.", 3, AE_COLOR_MUTED);
}

static void items_card(float x, float y, float width, float height)
{
	struct ae_is_item const *item = item_focused();
	char name[128], line[240], text[200], size[32], upper[64];
	float cy;
	int adding = item && S.op == OP_ADD && S.op_row == S.list.list.focus;
	int refused = item && ((item->parts >= 0 && item->refused) || (S.result_valid && S.result_index == item->index && !S.result.ok));
	int added_now = item && S.result_valid && S.result_index == item->index && S.result.ok;

	if (!item)
	{
		card_frame(x, y, width, height, 0);
		cy = y + U(26.0f);
		card_body(x, cy, width, S.search.text[0] ? "No item matches that search." : "This map has none of this kind.", 2, AE_COLOR_TEXT);
		return;
	}
	card_frame(x, y, width, height, refused);
	cy = y + U(26.0f);
	ae_is_item_name(item->path, name, sizeof(name));
	cy += card_item_name(x, cy, width, name);
	upper_copy(upper, sizeof(upper), S.set_name);
	if (adding)
		snprintf(line, sizeof(line), "%s" DOT "ADDING TO %s...", ae_is_kind_word(S.kind), upper);
	else if (refused)
		snprintf(line, sizeof(line), "%s" DOT "CAN'T BE ADDED", ae_is_kind_word(S.kind));
	else if (item->in_set)
		snprintf(line, sizeof(line), "%s" DOT "IN %s", ae_is_kind_word(S.kind), upper);
	else
		snprintf(line, sizeof(line), "%s" DOT "NOT IN THE SET", ae_is_kind_word(S.kind));
	cy += card_state(x, cy, line, refused ? AE_COLOR_WARNING : adding || item->in_set ? AE_COLOR_ACCENT : AE_COLOR_MUTED);
	if (adding)
	{
		float track = width - 2.0f * U(AE_FLOW_INSET_U), piece = track * 0.25f;
		unsigned long now = ae_motion_now();
		float travel = (float)(now % 1200ul) / 1200.0f * (track - piece);

		ae_draw_rect(x + U(AE_FLOW_INSET_U), cy, track, U(6.0f), 0.0f, AE_COLOR_RULE);
		ae_draw_rect(x + U(AE_FLOW_INSET_U) + travel, cy, piece, U(6.0f), 0.0f, AE_COLOR_ACCENT);
		cy += U(26.0f);
		cy += card_body(x, cy, width, "Reading it and every part it uses out of the map.", 2, AE_COLOR_MUTED) + U(6.0f);
	}
	else if (refused)
	{
		char const *engine = S.result_valid && S.result_index == item->index && S.result.reason[0] ? S.result.reason : item->reason;

		if (!S.be->kind_addable(S.kind) && !engine[0])
			snprintf(text, sizeof(text), "This version can't add %s yet.", ae_is_kind_name(S.kind));
		else
			ae_is_reason_words(engine, text, sizeof(text));
		cy += card_body(x, cy, width, text, 4, AE_COLOR_TEXT) + U(8.0f);
		if (S.kind == 0)
		{
			cy += card_label(x, cy, "WHAT TO DO");
			cy += card_body(x, cy, width, "Add the vehicle it is mounted on from VEHICLES: a vehicle brings the weapons mounted on it.", 3, AE_COLOR_TEXT) + U(8.0f);
		}
	}
	else if (added_now)
	{
		if (S.result.kept)
		{
			ae_hub_size_text(S.result.kept, size, sizeof(size));
			cy += card_fact(x, cy, width, "KEPT IN THE SET", size, AE_COLOR_ACCENT);
		}
		if (S.result.carried > 0)
		{
			snprintf(text, sizeof(text), "%ld: %s", S.result.carried, S.result.carried_names);
			cy += card_fact(x, cy, width, "CARRIED", text, AE_COLOR_TITLE);
		}
		cy += card_body(x, cy, width, "Parts it shares with the set's other items are kept once.", 2, AE_COLOR_MUTED) + U(6.0f);
	}
	cy += card_path(x, cy, width, item->path);
	if (!adding)
	{
		if (!refused && !added_now && item->parts < 0)
			cy += U(4.0f);
		cy += card_fact(x, cy, width, "FROM", S.map_open >= 0 && S.map_open < S.map_count ? S.maps[S.map_open].file : "", AE_COLOR_TITLE);
		if (item->parts >= 0 && !item->refused)
		{
			snprintf(text, sizeof(text), "%ld part%s", item->parts, item->parts == 1 ? "" : "s");
			cy += card_fact(x, cy, width, "SIZE", text, AE_COLOR_TITLE);
			cy += card_fact(x, cy, width, "WORKS", item->works ? "It has every part it needs" : "A part it needs is missing",
				item->works ? AE_COLOR_TITLE : AE_COLOR_WARNING);
		}
	}
	cy += U(8.0f);
	if (added_now || (item->in_set && !adding))
		card_body(x, cy, width, "It plays from the next level you load.", 2, AE_COLOR_TEXT);
	else if (item->made_up)
		card_body(x, cy, width, "Name made up: this map hides its names.", 2, AE_COLOR_MUTED);
	else if (!adding)
		card_body(x, cy, width, "No picture: maps keep none for their items. The name and the folders are what the map calls it.", 3, AE_COLOR_MUTED);
}

/* ---------- the help line and the footer */

/* ---------- the cards of the swaps, the doors of the first screen and the firing range */

/* a card of numbered steps (the empty states): its label, three heads with their text, a closing line */
static void steps_card(float x, float y, float width, float height, char const *label, char const *const *heads, char const *const *bodies,
	char const *foot)
{
	float cy = y + U(26.0f), ring = U(26.0f);
	int each;

	card_frame(x, y, width, height, 0);
	cy += card_label(x, cy, label);
	for (each = 0; each < 3; each++)
	{
		char digit[4];
		float left = x + U(AE_FLOW_INSET_U);

		snprintf(digit, sizeof(digit), "%d", each + 1);
		ae_draw_outline(left, cy, ring, ring, ring * 0.5f, U(2.0f), AE_COLOR_ACCENT);
		ae_draw_text(AE_FONT_ROW, U(16.0f), left + ring * 0.5f, ae_flow_cap_top(AE_FONT_ROW, U(16.0f), cy + ring * 0.5f), AE_ALIGN_CENTER,
			AE_COLOR_ACCENT, digit);
		ae_draw_text(AE_FONT_ROW, U(20.0f), left + ring + U(14.0f), cy, AE_ALIGN_LEFT, AE_COLOR_TITLE, heads[each]);
		card_body(x + ring + U(14.0f), cy + U(30.0f), width, bodies[each], 2, AE_COLOR_TEXT);
		cy += U(96.0f);
	}
	card_body(x, cy, width, foot, 3, AE_COLOR_MUTED);
}

/* the picks of a swap's lists, as the card says them: "3 picked by weapon", "At random" */
static void swap_weapons_fact(char *text, int size)
{
	int each, with = 0;

	for (each = 0; each < AE_IS_SWAP_WEAPON_SLOTS; each++)
		with += slot_picks(AE_IS_SW_WEAPON_SLOT, each, 0) > 0;
	if (swap_by_weapon())
		snprintf(text, (size_t)size, "%s " DOT " %d picked by weapon", ae_is_swap_value_index(AE_IS_SW_WEAPON_ROT, S.swap_val[AE_IS_SW_WEAPON_ROT]) ?
			"All swapped" : "Mixed in", with);
	else
		snprintf(text, (size_t)size, "%s " DOT " at random", ae_is_swap_value_index(AE_IS_SW_WEAPON_ROT, S.swap_val[AE_IS_SW_WEAPON_ROT]) ?
			"All swapped" : "Mixed in");
}

static void swap_enemies_fact(char *text, int size, int *carries)
{
	int each, with = 0, rank, ranks;
	char word[32];

	*carries = 0;
	for (each = 0; each < AE_IS_SWAP_ENEMY_SLOTS; each++)
	{
		ranks = tag_campaign_enemy_rank_count((short)each);
		with += enemy_picks(each, word, sizeof(word)) > 0;
		for (rank = 0; rank < (ranks ? ranks : 1); rank++)
		{
			char key[64];

			slot_key(AE_IS_SW_ENEMY_SLOT, each, ranks ? tag_campaign_enemy_rank((short)each, (short)rank) : 0, key, sizeof(key));
			*carries += S.be->swap_list_count(S.swap_file, "loadout", key) > 0;
		}
	}
	if (swap_by_enemy())
		snprintf(text, (size_t)size, "%s " DOT " %d picked by enemy", ae_is_swap_value_index(AE_IS_SW_ENEMY_ROT, S.swap_val[AE_IS_SW_ENEMY_ROT]) ?
			"All swapped" : "Mixed in", with);
	else
		snprintf(text, (size_t)size, "%s " DOT " at random", ae_is_swap_value_index(AE_IS_SW_ENEMY_ROT, S.swap_val[AE_IS_SW_ENEMY_ROT]) ?
			"All swapped" : "Mixed in");
}

static void swaps_card(float x, float y, float width, float height)
{
	static char const *const heads[3] = { "PICK A SET", "CHOOSE WHAT THEY REPLACE", "PICK IT IN THE LOBBY" };
	static char const *const bodies[3] = { "Any set with an item on.", "The weapons and enemies of a mission, and what enemies carry.",
		"Campaign lobby, RULES, ITEM SETS." };
	struct ae_is_swap const *swap = swap_focused();
	char line[160], text[160], upper[96];
	float cy;
	int state, carries;

	if (!swap)
	{
		steps_card(x, y, width, height, "HOW A SWAP WORKS", heads, bodies, "A swap plays one of your sets that has an item on. Sets with no item on are left out.");
		return;
	}
	state = swap_state_of(swap);
	card_frame(x, y, width, height, state == AE_IS_SWAP_SET_MISSING || state == AE_IS_SWAP_NOTHING_ON);
	cy = y + U(26.0f);
	cy += card_title(x, cy, width, swap->name);
	upper_copy(upper, sizeof(upper), swap->set_name);
	if (state == AE_IS_SWAP_SET_MISSING)
		snprintf(line, sizeof(line), "SET MISSING");
	else if (state == AE_IS_SWAP_NOTHING_ON)
		snprintf(line, sizeof(line), "NOTHING ON IN %s", upper);
	else
		snprintf(line, sizeof(line), "%sPLAYS %s", swap->in_use ? "IN USE IN THE LOBBY" DOT : "", upper);
	cy += card_state(x, cy, line, state == AE_IS_SWAP_IN_USE ? AE_COLOR_ACCENT : ae_is_swap_state_tone(state) == 3 ? AE_COLOR_WARNING : AE_COLOR_MUTED);
	if (state == AE_IS_SWAP_SET_MISSING)
	{
		snprintf(text, sizeof(text), "Its set, %s, is gone. In a mission it plays nothing: the mission is as it is.", swap->set_folder);
		cy += card_body(x, cy, width, text, 3, AE_COLOR_TEXT) + U(8.0f);
		cy += card_label(x, cy, "WHAT TO DO");
		card_body(x, cy, width, "Open it and pick another set, or Y deletes it.", 3, AE_COLOR_TEXT);
		return;
	}
	if (state == AE_IS_SWAP_NOTHING_ON)
	{
		cy += card_body(x, cy, width, "Every item of its set is off. In a mission it plays nothing: the mission is as it is.", 3, AE_COLOR_TEXT) + U(8.0f);
		cy += card_label(x, cy, "WHAT TO DO");
		card_body(x, cy, width, "Turn an item of the set on, or open the swap and pick another set.", 3, AE_COLOR_TEXT);
		return;
	}
	{
		struct ae_is_swap const keep = S.swap;
		char saved_file[64], biped[SWAP_VALUE];

		/* (the facts read the swap's own file: the page's values are this swap's for the length of the card) */
		snprintf(saved_file, sizeof(saved_file), "%s", S.swap_file);
		snprintf(S.swap_file, sizeof(S.swap_file), "%s", swap->file);
		{
			int row;

			for (row = 0; row < AE_IS_SW_ROWS; row++)
				if (ae_is_swap_row_file_key(row)[0] && row != AE_IS_SW_NAME)
					S.be->swap_option(S.swap_file, ae_is_swap_row_file_key(row), S.swap_val[row], SWAP_VALUE);
		}
		swap_weapons_fact(text, sizeof(text));
		cy += card_fact(x, cy, width, "WEAPONS", text, AE_COLOR_TITLE);
		swap_enemies_fact(text, sizeof(text), &carries);
		cy += card_fact(x, cy, width, "ENEMIES", text, AE_COLOR_TITLE);
		snprintf(text, sizeof(text), carries ? "%d picked" : "Their own", carries);
		cy += card_fact(x, cy, width, "WHAT ENEMIES CARRY", text, AE_COLOR_TITLE);
		snprintf(biped, sizeof(biped), "%s", S.swap_val[AE_IS_SW_BIPED]);
		if (biped[0])
			ae_is_item_name(biped, text, sizeof(text));
		else
			snprintf(text, sizeof(text), "The campaign's own");
		cy += card_fact(x, cy, width, "YOUR BODY", text, AE_COLOR_TITLE);
		snprintf(S.swap_file, sizeof(S.swap_file), "%s", saved_file);
		S.swap = keep;
	}
	cy += U(10.0f);
	cy += card_body(x, cy, width, "Each mission start rolls again; a checkpoint keeps what was placed. The randomizer is passed over while it plays.", 4,
		AE_COLOR_TEXT) + U(10.0f);
	cy += card_label(x, cy, "WITH OTHER PLAYERS");
	card_body(x, cy, width, "Alone and in split screen. Not in online co-op yet.", 2, AE_COLOR_TEXT);
}

/* a swap's page: the focused row's card */
static void swap_card(float x, float y, float width, float height)
{
	struct swap_line const *line = swline_focused();
	char name[96], text[200], upper[96];
	float cy;
	int row = line ? line->row : AE_IS_SW_NAME, shown, index;

	card_frame(x, y, width, height, 0);
	cy = y + U(26.0f);
	if (row == AE_IS_SW_WEAPON_SLOT || row == AE_IS_SW_ENEMY_SLOT)
	{
		char key[64];
		int picks, each, listed = 0;
		char names[160];

		slot_name(row, line->slot, 0, name, sizeof(name));
		if (row == AE_IS_SW_ENEMY_SLOT)
			picks = enemy_picks(line->slot, text, sizeof(text));
		else
			picks = slot_picks(row, line->slot, 0);
		cy += card_title(x, cy, width, name);
		ae_is_slot_count_line(picks, text, sizeof(text));
		cy += card_state(x, cy, text, picks ? AE_COLOR_ACCENT : AE_COLOR_MUTED);
		shown = ae_is_swap_value_index(row == AE_IS_SW_WEAPON_SLOT ? AE_IS_SW_WEAPON_ROT : AE_IS_SW_ENEMY_ROT,
			S.swap_val[row == AE_IS_SW_WEAPON_SLOT ? AE_IS_SW_WEAPON_ROT : AE_IS_SW_ENEMY_ROT]);
		lower_name_copy(upper, sizeof(upper), name);
		cy += card_body(x, cy, width, ae_is_slot_card_text(row == AE_IS_SW_WEAPON_SLOT, upper, !shown), 4, AE_COLOR_TEXT) + U(8.0f);
		slot_key(row, line->slot, tag_campaign_enemy_rank_count((short)line->slot) ? tag_campaign_enemy_rank((short)line->slot, 0) : 0, key, sizeof(key));
		names[0] = 0;
		if (row == AE_IS_SW_WEAPON_SLOT)
			for (each = 0; each < S.be->swap_list_count(S.swap_file, "weapons", key) && listed < 2; each++)
			{
				char path[256], item[128];

				if (!S.be->swap_list_get(S.swap_file, "weapons", key, each, path, sizeof(path)))
					continue;
				ae_is_item_name(path, item, sizeof(item));
				snprintf(names + strlen(names), sizeof(names) - strlen(names), "%s%s", listed ? ", " : "", item);
				listed++;
			}
		if (names[0])
			cy += card_fact(x, cy, width, "PICKED", names, AE_COLOR_TITLE);
		if (row == AE_IS_SW_WEAPON_SLOT && !shown)
		{
			snprintf(text, sizeof(text), "The %s (mixed in)", upper);
			cy += card_fact(x, cy, width, "WITH", text, AE_COLOR_TITLE);
		}
		cy += U(8.0f);
		card_body(x, cy, width, "An AI keeps its own weapon when it has no animations to hold the one picked.", 3, AE_COLOR_MUTED);
		return;
	}
	snprintf(name, sizeof(name), "%s", ae_is_swap_row_label(row));
	cy += card_title(x, cy, width, name);
	swap_text(row, 0, text, sizeof(text), &index);
	cy += card_state(x, cy, text, AE_COLOR_ACCENT);
	cy += card_body(x, cy, width, ae_is_swap_row_card_text(row), 5, AE_COLOR_TEXT) + U(8.0f);
	if (row == AE_IS_SW_BIPED)
	{
		int each;
		char choices[200];

		snprintf(choices, sizeof(choices), "The campaign's own");
		for (each = 0; each < S.sw_biped_count; each++)
		{
			entry_shown_name(&S.sw_entries[S.sw_bipeds[each]], name, sizeof(name));
			snprintf(choices + strlen(choices), sizeof(choices) - strlen(choices), "%s%s", DOT, name);
		}
		cy += card_fact(x, cy, width, "CHOICES", choices, AE_COLOR_TITLE);
		for (each = 0; each < S.sw_entry_count; each++)
		{
			struct ae_is_entry const *entry = &S.sw_entries[each];
			struct ae_is_fit fit;
			int offered = 0, other;

			if (entry->kind != 4 || !entry->on)
				continue;
			for (other = 0; other < S.sw_biped_count; other++)
				offered |= S.sw_bipeds[other] == each;
			S.be->entry_fit(S.swap.set_folder, entry->tag, &fit);
			if (!offered && fit.known && !fit.fits)
			{
				entry_shown_name(entry, name, sizeof(name));
				snprintf(text, sizeof(text), "%s: doesn't fit a player", name);
				cy += card_fact(x, cy, width, "LEFT OUT", text, AE_COLOR_WARNING);
			}
		}
		card_body(x, cy + U(6.0f), width, "AI that use a biped keep it. Multiplayer games: GAME OPTIONS \xE2\x80\xBA PLAYER MODEL.", 3, AE_COLOR_MUTED);
		return;
	}
	if (ae_is_swap_value_count(row) > 0 && row != AE_IS_SW_MOVEMENT)
	{
		int each, count = ae_is_swap_value_count(row);

		text[0] = 0;
		for (each = 0; each < count; each++)
			snprintf(text + strlen(text), sizeof(text) - strlen(text), "%s%s", each ? DOT : "", ae_is_swap_value_word(row, each));
		cy += card_fact(x, cy, width, "CHOICES", text, AE_COLOR_TITLE);
	}
	if (row == AE_IS_SW_WEAPON_PICK || row == AE_IS_SW_ENEMY_PICK)
		card_body(x, cy + U(8.0f), width, row == AE_IS_SW_WEAPON_PICK ? "Weapon rows show only while HOW THEY'RE PICKED is BY WEAPON." :
			"Enemy rows show only while HOW THEY'RE PICKED is BY ENEMY.", 3, AE_COLOR_MUTED);
}

/* an enemy's ranks: each rank's picks */
static void ranks_card(float x, float y, float width, float height)
{
	char name[64], text[96];
	float cy;
	int each, ranks = tag_campaign_enemy_rank_count((short)S.slot_slot);

	card_frame(x, y, width, height, 0);
	cy = y + U(26.0f);
	cy += card_title(x, cy, width, tag_campaign_enemy_name((short)S.slot_slot));
	cy += card_body(x, cy, width, "This enemy has ranks, each with a slot of its own: what it becomes and what it carries.", 3, AE_COLOR_TEXT) + U(10.0f);
	for (each = 0; each < ranks; each++)
	{
		int rank = tag_campaign_enemy_rank((short)S.slot_slot, (short)each);

		snprintf(name, sizeof(name), "%s", tag_campaign_rank_name((short)rank));
		ae_is_slot_value(slot_picks(AE_IS_SW_ENEMY_SLOT, S.slot_slot, rank), text, sizeof(text));
		cy += card_fact(x, cy, width, name, text, AE_COLOR_TITLE);
	}
}

/* a slot's screen: what it is and what the focused pick is */
static void slot_card(float x, float y, float width, float height)
{
	struct slot_line const *line = slot_focused();
	char name[96], text[200], lower[96];
	float cy;
	int weapon = S.slot_row == AE_IS_SW_WEAPON_SLOT, picks = slot_section_on(0) + (weapon ? 0 : slot_section_on(1));

	card_frame(x, y, width, height, 0);
	cy = y + U(26.0f);
	slot_name(S.slot_row, S.slot_slot, S.slot_rank, name, sizeof(name));
	cy += card_title(x, cy, width, name);
	ae_is_slot_count_line(picks, text, sizeof(text));
	cy += card_state(x, cy, text, picks ? AE_COLOR_ACCENT : AE_COLOR_MUTED);
	lower_name_copy(lower, sizeof(lower), name);
	cy += card_body(x, cy, width, ae_is_slot_card_text(weapon, lower, !ae_is_swap_value_index(weapon ? AE_IS_SW_WEAPON_ROT : AE_IS_SW_ENEMY_ROT,
		S.swap_val[weapon ? AE_IS_SW_WEAPON_ROT : AE_IS_SW_ENEMY_ROT])), 4, AE_COLOR_TEXT) + U(8.0f);
	if (line && line->sub[0])
		cy += card_fact(x, cy, width, "FROM", line->sub + (strncmp(line->sub, "from ", 5) ? 0 : 5), AE_COLOR_TITLE);
	if (!weapon && !tag_campaign_enemy_armed((short)S.slot_slot))
		card_body(x, cy + U(6.0f), width, ae_is_slot_unarmed_words(), 3, AE_COLOR_MUTED);
}

/* the first screen's CAMPAIGN SWAPS door and the waiting MULTIPLAYER SWAPS row */
static void home_card(float x, float y, float width, float height)
{
	char line[160], text[160];
	float cy;
	int each, shown = 0;

	card_frame(x, y, width, height, 0);
	cy = y + U(26.0f);
	if (home_row() == HOME_MULTIPLAYER)
	{
		cy += card_title(x, cy, width, "Multiplayer swaps");
		cy += card_state(x, cy, "COMES WITH FORGE", AE_COLOR_MUTED);
		cy += card_body(x, cy, width, "A host plays a set's weapons in a game with others, and the game sends the set to every player.", 4, AE_COLOR_TEXT) + U(10.0f);
		cy += card_label(x, cy, "WITH OTHER PLAYERS");
		card_body(x, cy, width, "Not yet. Sets stay in the campaign, alone and in split screen.", 3, AE_COLOR_TEXT);
		return;
	}
	cy += card_title(x, cy, width, "Campaign swaps");
	snprintf(line, sizeof(line), "%d SWAP%s " DOT " ONE PLAYS AT A TIME", S.swap_count, S.swap_count == 1 ? "" : "S");
	cy += card_state(x, cy, line, AE_COLOR_MUTED);
	cy += card_body(x, cy, width, "A swap plays one of your sets in the campaign's missions. You choose which of a mission's weapons and enemies the set's items "
		"take the place of, what the enemies carry, and the body you play as.", 6, AE_COLOR_TEXT) + U(8.0f);
	for (each = 0; each < S.swap_count && shown < 3; each++, shown++)
	{
		snprintf(text, sizeof(text), "plays %s", S.swaps[each].set_found ? S.swaps[each].set_name : "nothing");
		upper_copy(line, sizeof(line), S.swaps[each].name);
		cy += card_fact(x, cy, width, line, text, AE_COLOR_TITLE);
	}
	cy += U(8.0f);
	cy += card_body(x, cy, width, "Pick one in the campaign lobby: RULES \xE2\x80\xBA ITEM SETS. While a swap plays, the randomizer is passed over.", 4, AE_COLOR_TEXT) + U(8.0f);
	cy += card_label(x, cy, "WITH OTHER PLAYERS");
	card_body(x, cy, width, "Alone and in split screen. Not in online co-op yet.", 2, AE_COLOR_TEXT);
}

/* the contents' card for the firing range row */
static void range_card(float x, float y, float width, float height)
{
	char const *wait = range_wait();
	char text[200];
	float cy;

	card_frame(x, y, width, height, wait[0] != 0);
	cy = y + U(26.0f);
	cy += card_title(x, cy, width, "Firing range");
	cy += card_state(x, cy, wait[0] ? wait : "READY", wait[0] ? AE_COLOR_WARNING : AE_COLOR_ACCENT);
	snprintf(text, sizeof(text), "Lays out the weapons and the AI of %s on the firing range, so you can try each one before a mission.", S.set_name);
	cy += card_body(x, cy, width, text, 5, AE_COLOR_TEXT) + U(8.0f);
	if (!strcmp(wait, "NOTHING ON"))
		card_body(x, cy, width, "No item of the set is on. Turn one on first.", 3, AE_COLOR_TEXT);
	else if (wait[0])
		card_body(x, cy, width, "The range is built on Blood Gulch, and this PC has no Xbox-format bloodgulch.map in its maps.", 4, AE_COLOR_TEXT);
	else
		card_body(x, cy, width, "Starts from here: you play it alone. B in the game's pause menu leaves it.", 4, AE_COLOR_MUTED);
}

static char const *help_text(int *warning)
{
	static char text[320];
	struct ae_is_item const *item = item_focused();
	struct ae_is_set const *set = set_focused();
	struct ae_is_entry const *entry = entry_focused();
	struct ae_is_map const *map = map_focused();
	char name[128];

	*warning = 0;
	if (S.notice[0] && ae_motion_now() < S.notice_until)
	{
		*warning = S.notice_warning;
		return S.notice;
	}
	switch (S.step)
	{
	case AE_IS_STEP_SETS:
		if (home_row() == HOME_SWAPS)
			return "A lists your campaign swaps. A swap plays a set in the missions in the randomizer's place.";
		if (home_row() == HOME_MULTIPLAYER)
			return "Multiplayer swaps come with Forge: the host's set, sent to every player. Until then sets stay in the campaign.";
		if (home_row() == HOME_NEW && ae_is_limit_word(0, S.set_count)[0])
			return "64 sets is the most. Delete one to make room.";
		if (set)
		{
			snprintf(text, sizeof(text), set->playing ? "%s plays in your levels. {A} opens its options; {Y} stops it; {X} makes a new set." :
				"%s is off. {A} opens its options; {Y} plays it; {X} makes a new set.", set->name);
			return text;
		}
		return S.set_count ? "NEW SET asks for its name, then opens it so you can add items from a map." :
			"NEW SET asks for its name, then opens it so you can add items from a map.";
	case AE_IS_STEP_SWAPS:
	{
		struct ae_is_swap const *swap = swap_focused();

		if (swap)
		{
			int state = swap_state_of(swap);

			*warning = state == AE_IS_SWAP_SET_MISSING || state == AE_IS_SWAP_NOTHING_ON;
			if (state == AE_IS_SWAP_SET_MISSING)
				snprintf(text, sizeof(text), "Its set is gone. {A} opens %s to pick another set; {Y} deletes it.", swap->name);
			else
				snprintf(text, sizeof(text), "{A} opens %s to change it. {Y} deletes it; %s stays as it is.", swap->name, swap->set_name);
			return text;
		}
		if (ae_is_limit_word(2, S.swap_count)[0])
			return "64 swaps is the most. Delete one to make room.";
		{
			int none_on[AE_IS_SETS_MOST];

			if (!sets_with_items(none_on, AE_IS_SETS_MOST))
				return "Add items to a set first: ITEM SETS \xE2\x80\xBA a set \xE2\x80\xBA ADD ITEMS FROM A MAP.";
		}
		return "NEW SWAP asks for a name, then for the set it plays.";
	}
	case AE_IS_STEP_SWAP:
	{
		struct swap_line const *line = swline_focused();

		if (line && line->row == AE_IS_SW_SET && !S.swap.set_found)
		{
			*warning = 1;
			return "Its set is gone. Left or right picks another set.";
		}
		if (line && (line->row == AE_IS_SW_HANDS || line->row == AE_IS_SW_MOVEMENT))
		{
			snprintf(text, sizeof(text), "%s is stored with the swap; the next build plays it.", ae_is_swap_row_label(line->row));
			return text;
		}
		return line ? ae_is_swap_row_help(line->row) : "";
	}
	case AE_IS_STEP_RANKS:
		return "This enemy has ranks. A opens a rank to pick what it becomes and carries.";
	case AE_IS_STEP_SLOT:
		return ae_is_slot_help(S.slot_row == AE_IS_SW_WEAPON_SLOT);
	case AE_IS_STEP_CONTENTS:
		if (S.list.list.focus >= 0 && S.list.list.focus < S.row_count && S.rows[S.list.list.focus].type == ROW_RANGE)
		{
			char const *wait = range_wait();

			*warning = wait[0] != 0;
			if (!strcmp(wait, "NOTHING ON"))
				return "No item of the set is on. Turn one on to try it on the firing range.";
			if (wait[0])
				return "The firing range is built on Blood Gulch: this PC has no Xbox-format bloodgulch.map in its maps.";
			snprintf(text, sizeof(text), "{A} starts the firing range with %s's items laid out. You play it alone.", S.set_name);
			return text;
		}
		if (entry)
		{
			ae_is_item_name(entry->path, name, sizeof(name));
			if (entry->failed[0])
			{
				*warning = 1;
				snprintf(text, sizeof(text), "%s was turned off: the last level couldn't play it. {A} tries it again; {Y} removes it.", name);
			}
			else if (entry->kind == 4 && entry->on && ae_is_body_is(S.body_biped, entry->path))
				snprintf(text, sizeof(text), "%s is your body while %s plays. {A} opens its settings to change that.", name, S.set_name);
			else
				snprintf(text, sizeof(text), "{A} opens %s's settings. Left or right turns it on or off; {Y} removes it from the set.", name);
			return text;
		}
		return "Pick a map on this PC, then a kind, then the item to copy into the set.";
	case AE_IS_STEP_MAPS:
		if (map && S.be->maps_reason(map->source))
		{
			*warning = 1;
			return map->source == AE_IS_SOURCE_CE ? "A Custom Edition map is being played: leave the game to open one. MCC and Workshop maps still open." :
				"A map of this kind is open elsewhere: leave the game to open one. Custom Edition maps still open.";
		}
		return "Pick the map to take items from. {A} opens it; it takes about a second.";
	case AE_IS_STEP_SETTINGS:
	{
		int row = setting_focused();
		struct ae_is_entry const *entry = settings_entry();

		if (entry && entry->failed[0])
		{
			*warning = 1;
			return "It was turned off: the last level couldn't play it. Turn it on to try again; {Y} removes it.";
		}
		if (row == AE_IS_ROW_PLAY_AS && S.fit.known && !S.fit.fits)
		{
			char why[200];

			*warning = 1;
			ae_is_fit_short(&S.fit, why, sizeof(why));
			snprintf(text, sizeof(text), "It can't be your body: %s. An AI that uses it still plays it.", why);
			return text;
		}
		if (row == AE_IS_ROW_REMOVE)
			return "{A} removes it from the set. The map it came from still has it.";
		if (row >= 0 && ae_is_row_typed(row))
		{
			snprintf(text, sizeof(text), "{A} types %s. Empty puts it back.", ae_is_row_label(row));
			return text;
		}
		if (row == AE_IS_ROW_HANDS || row == AE_IS_ROW_MOVEMENT)
		{
			snprintf(text, sizeof(text), "%s is stored with the set; the next build plays it.", ae_is_row_label(row));
			return text;
		}
		if (row >= 0)
		{
			snprintf(text, sizeof(text), "Left or right changes %s. %s", ae_is_row_label(row), ae_is_row_help(row));
			return text;
		}
		return "Left or right changes the row; {Y} removes the item from the set.";
	}
	case AE_IS_STEP_KINDS:
		snprintf(text, sizeof(text), "{A} lists the map's %ld %s. Each row is an item's own name, with the folders it sits in under it.",
			S.kind_counts[S.list.list.focus < AE_IS_KINDS ? S.list.list.focus : 0], S.list.list.focus < AE_IS_KINDS ?
			ae_is_kind_name(S.list.list.focus) : "items");
		return text;
	}
	if (item)
	{
		ae_is_item_name(item->path, name, sizeof(name));
		if (S.op == OP_ADD)
			snprintf(text, sizeof(text), "Adding %s to %s...", name, S.set_name);
		else if (item->parts >= 0 && item->refused)
		{
			*warning = 1;
			snprintf(text, sizeof(text), "%s can't be added. See the card.", name);
		}
		else if (item->in_set)
			snprintf(text, sizeof(text), "%s is in %s. {Y} removes it. It plays from the next level.", name, S.set_name);
		else
			snprintf(text, sizeof(text), "{A} adds %s to %s. Parts are counted as you move down the list.", name, S.set_name);
		return text;
	}
	return "";
}

static void prompts_build(int playing, int has)
{
	struct ae_flow_screen *flow = &S.flow;
	struct ae_is_hint hints[AE_IS_HINTS_MOST];
	int count, each;

	flow->prompt_count = 0;
	if (S.overlay == OVERLAY_NAME)
	{
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, S.name_mode == NAME_NEW ? "Make it" : S.name_mode == NAME_SWAP_NEW ? "Next" : "Done",
			AE_ACTION_ACCEPT);
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, "Cancel", AE_ACTION_BACK);
		return;
	}
	if (S.overlay == OVERLAY_OPTIONS || S.overlay == OVERLAY_SETPICK)
	{
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, S.overlay == OVERLAY_SETPICK ? "Make the swap" : "Select", AE_ACTION_ACCEPT);
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, "Cancel", AE_ACTION_BACK);
		return;
	}
	if (S.typing)
	{
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Search", AE_ACTION_ACCEPT);
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, "Stop typing", AE_ACTION_BACK);
		return;
	}
	if (S.op != OP_NONE)
		return;
	count = ae_is_hints(S.step, playing, has, hints);
	for (each = 0; each < count; each++)
	{
		struct ae_prompt prompt;

		switch (hints[each].hint)
		{
		case AE_IS_HINT_NEW:
		case AE_IS_HINT_ADD_ITEMS:
		case AE_IS_HINT_SEARCH:
			prompt = ae_flow_prompt(AE_BUTTON_X, hints[each].label, AE_ACTION_X);
			break;
		case AE_IS_HINT_PLAY:
		case AE_IS_HINT_STOP:
		case AE_IS_HINT_REMOVE:
			prompt = ae_flow_prompt(AE_BUTTON_Y, hints[each].label, AE_ACTION_Y);
			break;
		case AE_IS_HINT_PAGE:
			prompt = ae_flow_prompt(AE_BUTTON_LEFT_TRIGGER, hints[each].label, AE_ACTION_PAGE_UP);
			prompt.pair = 1;
			break;
		case AE_IS_HINT_ON_OFF:
		case AE_IS_HINT_CHANGE:
			/* (Content Evolved's left and right: the d-pad's pair) */
			prompt = ae_flow_prompt(AE_BUTTON_DPAD_LEFT, hints[each].label, AE_ACTION_RIGHT);
			prompt.pair = 1;
			break;
		case AE_IS_HINT_BACK:
		case AE_IS_HINT_CLOSE_MAP:
			prompt = ae_flow_prompt(AE_BUTTON_B, hints[each].label, AE_ACTION_BACK);
			break;
		default:
			prompt = ae_flow_prompt(AE_BUTTON_A, hints[each].label, AE_ACTION_ACCEPT);
			break;
		}
		flow->prompts[flow->prompt_count++] = prompt;
	}
	if ((S.step == AE_IS_STEP_SETS || S.step == AE_IS_STEP_CONTENTS) && flow->prompt_count < AE_FLOW_PROMPTS - 1)
	{
		struct ae_prompt prompt = ae_flow_prompt(AE_BUTTON_LEFT_SHOULDER, "Pages", AE_ACTION_TAB_PREVIOUS);

		prompt.pair = 1;
		flow->prompts[flow->prompt_count] = flow->prompts[flow->prompt_count - 1];
		flow->prompts[flow->prompt_count - 1] = prompt;
		flow->prompt_count++;
	}
}

/* ---------- the popovers */

static struct { char label[48]; int muted; } option_rows[OPTIONS];

static void options_build(void)
{
	snprintf(option_rows[OPTION_OPEN].label, sizeof(option_rows[0].label), "OPEN");
	snprintf(option_rows[OPTION_PLAY].label, sizeof(option_rows[0].label), "%s", S.option_playing ? "STOP PLAYING IT" : "PLAY IT IN YOUR LEVELS");
	snprintf(option_rows[OPTION_RENAME].label, sizeof(option_rows[0].label), "RENAME");
	snprintf(option_rows[OPTION_COPY].label, sizeof(option_rows[0].label), "MAKE A COPY");
	snprintf(option_rows[OPTION_DELETE].label, sizeof(option_rows[0].label), "DELETE");
}

static void popover_box(float width, float height, float *x, float *y)
{
	*x = S.flow.frame.rect.x + (S.flow.frame.rect.width - width) * 0.5f;
	*y = S.flow.frame.rect.y + (S.flow.frame.rect.height - height) * 0.5f;
	ae_draw_rect(S.flow.frame.rect.x, S.flow.frame.rect.y, S.flow.frame.rect.width, S.flow.frame.rect.height, 0.0f, AE_COLOR_SCRIM);
	ae_draw_rect(*x, *y, width, height, 0.0f, AE_COLOR_POPOVER);
	ae_draw_outline(*x, *y, width, height, 0.0f, U(1.0f), AE_COLOR_RULE);
	ae_draw_rect(*x, *y, width, U(4.0f), 0.0f, AE_COLOR_ACCENT);
	ae_hit_add(*x, *y, width, height, HIT_POP, AE_PART_CARD, -1);
}

static void overlay_draw(void)
{
	struct ae_density const *d = &S.flow.density;
	float width = U(620.0f), x, y;

	if (S.overlay == OVERLAY_NAME)
	{
		struct ae_field field;
		char title[96];
		float height = U(190.0f);

		popover_box(width, height, &x, &y);
		if (S.name_mode == NAME_ITEM || S.name_mode == NAME_GROUP)
			snprintf(title, sizeof(title), "%s", S.name_mode == NAME_ITEM ? "Name this item" : "Group name");
		else if (S.name_mode == NAME_SWAP_NEW || S.name_mode == NAME_SWAP_RENAME)
			snprintf(title, sizeof(title), "%s", S.name_mode == NAME_SWAP_NEW ? "New swap" : "Rename swap");
		else
			snprintf(title, sizeof(title), S.name_mode == NAME_NEW ? "New set" : S.name_mode == NAME_RENAME ? "Rename %s" : "Copy of %s",
				S.option_name);
		ae_draw_text(AE_FONT_ROW, U(30.0f), x + U(24.0f), y + U(26.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE, title);
		memset(&field, 0, sizeof(field));
		field.label = S.name_mode == NAME_ITEM ? "NAME" : S.name_mode == NAME_GROUP ? "GROUP NAME" :
			S.name_mode == NAME_SWAP_NEW || S.name_mode == NAME_SWAP_RENAME ? "SWAP NAME" : "SET NAME";
		field.placeholder = S.name_mode == NAME_ITEM || S.name_mode == NAME_GROUP ? "Empty puts it back" : "Letters, numbers, spaces";
		field.text = &S.name_text;
		field.flags = AE_FIELD_FOCUSED | (S.naming || S.naming_keyboard ? AE_FIELD_EDITING : 0);
		field.caret_since = S.caret_since;
		ae_field_well(d, x + U(24.0f), y + U(76.0f), width - U(48.0f), &field, &S.name_well);
		ae_widget_field(d, x + U(24.0f), y + U(76.0f), width - U(48.0f), &field, HIT_POP);
		ae_draw_text(AE_FONT_BODY, ae_size_minor(d) * 0.9f, x + width - U(24.0f), y + height - U(34.0f), AE_ALIGN_RIGHT, AE_COLOR_MUTED,
			S.name_mode == NAME_NEW ? "A makes it. B cancels." : S.name_mode == NAME_SWAP_NEW ? "A goes on to the set. B cancels." :
			"A keeps the text. B cancels.");
		return;
	}
	if (S.overlay == OVERLAY_SETPICK)
	{
		float row_h = U(56.0f), title_h = U(100.0f);
		int shown = S.pick_count < 6 ? S.pick_count : 6, first = 0, each;
		float height = title_h + row_h * (float)shown + U(24.0f);

		if (S.pick_focus >= shown)
			first = S.pick_focus - shown + 1;
		popover_box(width, height, &x, &y);
		ae_draw_text(AE_FONT_ROW, U(30.0f), x + U(24.0f), y + U(26.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE, "Which set does it play?");
		ae_draw_text(AE_FONT_BODY, ae_size_minor(d) * 0.9f, x + U(24.0f), y + U(66.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
			"Only sets with an item on are offered.");
		for (each = 0; each < shown; each++)
		{
			struct ae_is_set const *set = &S.sets[S.pick_sets[first + each]];
			struct ae_row row;
			char value[32];

			memset(&row, 0, sizeof(row));
			snprintf(value, sizeof(value), "%ld ON", set->on);
			row.label = set->name;
			row.value = value;
			row.flags = S.pick_focus == first + each ? AE_ROW_FOCUSED : 0;
			ae_widget_row(d, x + U(16.0f), y + title_h + row_h * (float)each, width - U(32.0f), &row, HIT_POP, (short)(first + each));
		}
		return;
	}
	{
		float row_h = U(56.0f), title_h = U(100.0f), height = title_h + row_h * (float)OPTIONS + U(24.0f);
		char body[120];
		int each;

		options_build();
		popover_box(width, height, &x, &y);
		ae_draw_text(AE_FONT_ROW, U(30.0f), x + U(24.0f), y + U(26.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE, S.option_name);
		{
			struct ae_is_set const *set = set_named(S.option_folder);
			char size[32];

			if (set)
			{
				ae_hub_size_text(set->bytes, size, sizeof(size));
				snprintf(body, sizeof(body), "%ld item%s, %s. %s", set->items, set->items == 1 ? "" : "s", size,
					set->playing ? "It plays in your levels now." : "It is off.");
				ae_draw_text(AE_FONT_BODY, ae_size_minor(d) * 0.9f, x + U(24.0f), y + U(66.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED, body);
			}
		}
		for (each = 0; each < OPTIONS; each++)
		{
			struct ae_row row;

			memset(&row, 0, sizeof(row));
			row.label = option_rows[each].label;
			row.flags = (S.overlay_focus == each ? AE_ROW_FOCUSED : 0) | (each == OPTION_DELETE ? AE_ROW_DESTRUCTIVE_FRAMED : 0);
			ae_widget_row(d, x + U(16.0f), y + title_h + row_h * (float)each, width - U(32.0f), &row, HIT_POP, (short)each);
		}
	}
}

/* ---------- what a press does */

static int plays_now(void)
{
	struct ae_is_set const *set = set_focused();

	return set ? set->playing : 0;
}

static void focus_set(char const *folder)
{
	int each;

	for (each = 0; each < S.set_count; each++)
		if (same_text(S.sets[each].folder, folder))
		{
			ae_list_set_focus(&S.list.list, (short)each);
			return;
		}
}

/* the set opened: its items listed */
static void set_open(char const *folder, char const *name)
{
	snprintf(S.folder, sizeof(S.folder), "%s", folder);
	snprintf(S.set_name, sizeof(S.set_name), "%s", name);
	S.map_open = -1;
	entries_read();
	step_to(AE_IS_STEP_CONTENTS, 0);
	ae_sound_request(AE_SOUND_FORWARD, 0);
	platform_log("item sets: %s opened (%d items)", folder, S.entry_count);
}

static void set_play_toggle(char const *folder, char const *name, int play)
{
	if (S.be->play(play ? folder : NULL))
	{
		notice_set(0, 5000, play ? "%s plays in your levels, from the next level you load." : "%s stops playing: your levels are as they were.", name);
		ae_sound_request(AE_SOUND_FORWARD, 0);
	}
	else
	{
		notice_set(1, 5000, "%s can't be switched: the settings can't be written.", name);
		ae_sound_request(AE_SOUND_FAILURE, 0);
	}
	sets_read();
	if (S.step == AE_IS_STEP_SETS)
	{
		list_to(step_count(), S.list.list.focus);
		focus_set(folder);
	}
}

static struct { char folder[64], name[48]; int what; long tag; int kind, index; } question;
enum { ASK_DELETE_SET, ASK_REMOVE_ENTRY, ASK_REMOVE_ITEM, ASK_DELETE_SWAP };

static void question_picked(short choice, void *context)
{
	(void)context;
	if (choice != 1)
		return;
	if (question.what == ASK_DELETE_SET)
	{
		if (S.be->remove_set(question.folder))
		{
			notice_set(0, 4000, "%s deleted.", question.name);
			ae_sound_request(AE_SOUND_FORWARD, 0);
		}
		else
		{
			notice_set(1, 5000, "Can't delete %s.", question.name);
			ae_sound_request(AE_SOUND_FAILURE, 0);
		}
		sets_read();
		list_to(step_count(), 0);
	}
	else if (question.what == ASK_REMOVE_ENTRY)
	{
		if (S.be->entry_remove(S.folder, question.tag))
		{
			notice_set(0, 4000, "%s removed from %s.", question.name, S.set_name);
			ae_sound_request(AE_SOUND_FORWARD, 0);
		}
		else
		{
			notice_set(1, 5000, "Can't remove %s.", question.name);
			ae_sound_request(AE_SOUND_FAILURE, 0);
		}
		if (S.step == AE_IS_STEP_SETTINGS)
		{
			S.step = AE_IS_STEP_CONTENTS;
			S.list.list.focus = 0;
		}
		entries_read();
		list_to(S.row_count, S.list.list.focus < S.row_count ? S.list.list.focus : S.row_count - 1);
		sets_read();
	}
	else if (question.what == ASK_DELETE_SWAP)
	{
		if (S.be->swap_remove(question.folder))
		{
			notice_set(0, 4000, "%s deleted.", question.name);
			ae_sound_request(AE_SOUND_FORWARD, 0);
		}
		else
		{
			notice_set(1, 5000, "Can't delete %s.", question.name);
			ae_sound_request(AE_SOUND_FAILURE, 0);
		}
		swaps_read();
		sets_read();
		S.step = AE_IS_STEP_SWAPS;
		list_to(step_count(), 0);
	}
	else
	{
		if (S.be->item_remove(S.folder, question.kind, question.index))
		{
			notice_set(0, 4000, "%s removed from %s.", question.name, S.set_name);
			ae_sound_request(AE_SOUND_FORWARD, 0);
		}
		S.result_valid = 0;
		items_read(S.kind);
		maps_read();
		list_to(S.item_view_count, S.list.list.focus < S.item_view_count ? S.list.list.focus : S.item_view_count - 1);
	}
}

static void question_ask(int what, char const *title, char const *body)
{
	struct ae_dialog_spec spec;
	static char title_text[160], body_text[320];

	snprintf(title_text, sizeof(title_text), "%s", title);
	snprintf(body_text, sizeof(body_text), "%s", body);
	question.what = what;
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = title_text;
	spec.body = body_text;
	spec.choices[0] = "CANCEL";
	spec.choices[1] = what == ASK_DELETE_SET || what == ASK_DELETE_SWAP ? "DELETE" : "REMOVE";
	spec.choice_count = 2;
	spec.safe_choice = 0;
	spec.cancel_choice = 0;
	spec.has_cancel = 1;
	spec.destructive_choices = 2u;
	spec.density = S.flow.density;
	spec.bounds = S.flow.frame.rect;
	spec.picked = question_picked;
	if (!ae_dialog_open(&spec, AE_OWNER_ANY))
		platform_log("item sets: the question was refused");
}

static void delete_set_ask(char const *folder, char const *name)
{
	struct ae_is_set const *set = set_named(folder);
	char title[96], body[240], size[32];

	snprintf(question.folder, sizeof(question.folder), "%s", folder);
	snprintf(question.name, sizeof(question.name), "%s", name);
	ae_hub_size_text(set ? set->bytes : 0, size, sizeof(size));
	snprintf(title, sizeof(title), "Delete %s?", name);
	snprintf(body, sizeof(body), "Its %ld item%s (%s) leave this PC. The maps they came from still have them: you can add them again.",
		set ? set->items : 0L, set && set->items == 1 ? "" : "s", size);
	question_ask(ASK_DELETE_SET, title, body);
}

static void remove_entry_ask(struct ae_is_entry const *entry)
{
	char name[128], title[160], body[240], size[32];

	ae_is_item_name(entry->path, name, sizeof(name));
	snprintf(question.name, sizeof(question.name), "%s", name);
	question.tag = entry->tag;
	ae_hub_size_text(entry->bytes, size, sizeof(size));
	snprintf(title, sizeof(title), "Remove %s?", name);
	if (entry->bytes)
		snprintf(body, sizeof(body), "It leaves %s (%s). %s still has it: you can add it again.", S.set_name, size, entry->map);
	else
		snprintf(body, sizeof(body), "It leaves %s. %s still has it: you can add it again.", S.set_name, entry->map);
	question_ask(ASK_REMOVE_ENTRY, title, body);
}

static void remove_item_ask(struct ae_is_item const *item)
{
	char name[128], title[160], body[240];

	ae_is_item_name(item->path, name, sizeof(name));
	snprintf(question.name, sizeof(question.name), "%s", name);
	question.kind = S.kind;
	question.index = (int)item->index;
	snprintf(title, sizeof(title), "Remove %s?", name);
	snprintf(body, sizeof(body), "It leaves %s. The map still has it: you can add it again.", S.set_name);
	question_ask(ASK_REMOVE_ITEM, title, body);
}

/* ---------- an item's settings page: what a press does */

static void name_begin(void);
static void setpick_begin(void);

/* the page's values read again, its rows built again, the focus kept on a row (AE_IS_ROW_*, or -1 for the first) */
static void settings_reload(int keep_row)
{
	int each, focus = 0;

	entries_read();
	sets_read();
	settings_read();
	settings_build();
	for (each = 0; keep_row >= 0 && each < S.srow_count; each++)
		if (!S.srows[each].header && S.srows[each].row == keep_row)
			focus = each;
	list_to(S.srow_count, focus);
}

/* the campaign's items are read the first time an item's page asks (a moment: its row says LOOKING... meanwhile) */
static int stock_asked;

static void settings_open(struct ae_is_entry const *entry, int keep_row)
{
	S.replace_state = !stock_asked && !S.staged && S.be->replace_known() && entry->kind <= 2 ? 1 : 2;
	S.replace_wait = 2;
	S.set_tag = entry->tag;
	S.set_kind = entry->kind;
	snprintf(S.set_path, sizeof(S.set_path), "%s", entry->path);
	snprintf(S.set_map, sizeof(S.set_map), "%s", entry->map);
	S.step = AE_IS_STEP_SETTINGS;
	S.ctl = -1;
	S.result_valid = 0;
	settings_reload(keep_row);
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void settings_changed(char const *text)
{
	notice_set(0, 4000, "%s", text);
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

/* PLAY AS IT: the set's body is this biped (and the set's one body only), or none; a biped that does not fit a player is refused */
static void settings_play_as(int on)
{
	if (on && S.fit.known && !S.fit.fits)
	{
		{
			char why[200], line[260];

			ae_is_fit_short(&S.fit, why, sizeof(why));
			snprintf(line, sizeof(line), "It can't be your body: %s.", why);
			notice_set(1, 6000, "%s", line);
		}
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	if (!S.be->body_set_option(S.folder, "player_biped", on ? S.set_path : "") || !S.be->body_set_option(S.folder, "player_hands", "") ||
		!S.be->body_set_option(S.folder, "player_movement", ""))
	{
		notice_set(1, 6000, "The set can't be written: the log says why.");
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	settings_reload(AE_IS_ROW_PLAY_AS);
	settings_changed(on ? "You play as it from the next level you load, while the set plays." : "The campaign's player plays again from the next level.");
}

/* a value row stepped (direction +1 or -1) round its values; ON and PLAY AS IT turn over */
static void settings_step(int direction)
{
	int row = setting_focused(), index, count, next;
	char const *key;

	if (row < 0 || (row == AE_IS_ROW_REPLACE && S.replace_state == 1))
		return;
	switch (row)
	{
	case AE_IS_ROW_ON:
		if (S.be->entry_enable(S.folder, S.set_tag, !S.set_on || S.set_failed))
		{
			settings_reload(row);
			settings_changed("Changes play from the next level you load.");
		}
		else
		{
			notice_set(1, 5000, "The set can't be written: the log says why.");
			ae_sound_request(AE_SOUND_FAILURE, 0);
		}
		return;
	case AE_IS_ROW_PLAY_AS:
		settings_play_as(!play_as_on());
		return;
	case AE_IS_ROW_HANDS:
	{
		char const *kept = S.body_hands;

		if (!ae_is_body_rows_live())
			return;
		int choices = 2 + S.hands_count, each;

		index = !kept[0] ? 0 : !strcmp(kept, "campaign") ? 1 : 0;
		for (each = 0; each < S.hands_count; each++)
			if (!strcmp(kept, S.hands_key[each]))
				index = 2 + each;
		next = ae_is_value_step(choices, index, direction);
		key = next == 0 ? "" : next == 1 ? "campaign" : S.hands_key[next - 2];
		if (S.be->body_set_option(S.folder, "player_hands", key))
		{
			settings_reload(row);
			settings_changed("Changes play from the next level you load.");
		}
		return;
	}
	case AE_IS_ROW_NAME:
	case AE_IS_ROW_GROUP:
	case AE_IS_ROW_REMOVE:
		return;
	}
	count = ae_is_value_count(row, S.set_kind);
	if (!count)
		return;
	if (row == AE_IS_ROW_MOVEMENT)
	{
		if (!ae_is_body_rows_live())
			return;
		index = ae_is_value_index(row, S.set_kind, S.body_movement);
		next = ae_is_value_step(count, index, direction);
		if (S.be->body_set_option(S.folder, "player_movement", ae_is_value_key(row, S.set_kind, next)))
		{
			settings_reload(row);
			settings_changed("Changes play from the next level you load.");
		}
		return;
	}
	index = ae_is_value_index(row, S.set_kind, S.value[row]);
	next = ae_is_value_step(count, index, direction);
	if (S.be->entry_set_option(S.folder, S.set_tag, row_key(row), ae_is_value_key(row, S.set_kind, next)))
	{
		settings_reload(row);
		settings_changed("Changes play from the next level you load.");
	}
	else
	{
		notice_set(1, 5000, "The set can't be written: the log says why.");
		ae_sound_request(AE_SOUND_FAILURE, 0);
	}
}

/* A on a row: a typed one asks for its text, the removal asks, PLAY AS IT turns on (it does nothing while it doesn't fit), the others step on */
static void settings_accept(void)
{
	int row = setting_focused();
	struct ae_is_entry const *entry = settings_entry();

	if (row < 0 || !entry)
		return;
	if (row == AE_IS_ROW_REMOVE)
		remove_entry_ask(entry);
	else if (ae_is_row_typed(row))
	{
		S.name_mode = row == AE_IS_ROW_NAME ? NAME_ITEM : NAME_GROUP;
		snprintf(S.option_name, sizeof(S.option_name), "%s", S.value[row]);
		name_begin();
	}
	else if (row == AE_IS_ROW_PLAY_AS && S.fit.known && !S.fit.fits)
	{
		{
			char why[200], line[260];

			ae_is_fit_short(&S.fit, why, sizeof(why));
			snprintf(line, sizeof(line), "It can't be your body: %s.", why);
			notice_set(1, 6000, "%s", line);
		}
		ae_sound_request(AE_SOUND_FAILURE, 0);
	}
	else
		settings_step(1);
}

/* the text typed for a name or a group: kept (empty puts it back); text with other characters is asked for again */
static int settings_typed_commit(char const *text)
{
	int row = S.name_mode == NAME_ITEM ? AE_IS_ROW_NAME : AE_IS_ROW_GROUP;

	if (!ae_is_typed_ok(text))
	{
		notice_set(1, 6000, "Letters, numbers, spaces, - and _ only, up to 31 of them.");
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return 0;
	}
	if (!S.be->entry_set_option(S.folder, S.set_tag, row_key(row), text))
	{
		notice_set(1, 5000, "The set can't be written: the log says why.");
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return 1;
	}
	settings_reload(row);
	settings_changed(row == AE_IS_ROW_NAME ? "Renamed. The menus show it by this name." : "Items with the same group name are played together.");
	return 1;
}

/* a set's name typed (the hub's text entry) */
static void name_typing_done(int keep, void *context)
{
	(void)keep;
	(void)context;
	S.naming = 0;
}

static void name_cancel(void)
{
	if (S.naming)
		ae_field_edit_end(&S.name_edit, 0);
	S.naming = 0;
	S.overlay = OVERLAY_NONE;
	ae_sound_request(AE_SOUND_BACK, 0);
}

static void name_commit(void)
{
	char text[64], folder[64];
	int done = 0;

	snprintf(text, sizeof(text), "%s", S.name_text.text);
	if (S.naming)
		ae_field_edit_end(&S.name_edit, 1);
	S.naming = 0;
	if (S.name_mode == NAME_ITEM || S.name_mode == NAME_GROUP)
	{
		if (!settings_typed_commit(text))
		{
			name_begin();
			return;
		}
		S.overlay = OVERLAY_NONE;
		return;
	}
	if (S.name_mode == NAME_SWAP_NEW || S.name_mode == NAME_SWAP_RENAME)
	{
		if (!ae_is_set_name_ok(text))
		{
			notice_set(1, 6000, "A swap's name needs a letter or a number, and can have only letters, numbers, spaces, - and _.");
			ae_sound_request(AE_SOUND_FAILURE, 0);
			name_begin();
			return;
		}
		S.overlay = OVERLAY_NONE;
		if (S.name_mode == NAME_SWAP_NEW)
		{
			snprintf(S.pending_swap, sizeof(S.pending_swap), "%s", text);
			setpick_begin();
		}
		else if (S.be->swap_rename(S.swap_file, text))
		{
			swap_reload(AE_IS_SW_NAME, 0);
			notice_set(0, 4000, "Renamed to %s.", text);
			ae_sound_request(AE_SOUND_FORWARD, 0);
		}
		else
		{
			notice_set(1, 5000, "The swap could not be renamed (the log says why).");
			ae_sound_request(AE_SOUND_FAILURE, 0);
		}
		return;
	}
	if (!ae_is_set_name_ok(text))
	{
		notice_set(1, 6000, "A set's name needs a letter or a number, and can have only letters, numbers, spaces, - and _.");
		ae_sound_request(AE_SOUND_FAILURE, 0);
		name_begin();
		return;
	}
	S.overlay = OVERLAY_NONE;
	if (S.name_mode == NAME_NEW)
		done = S.be->create(text, folder, sizeof(folder));
	else if (S.name_mode == NAME_COPY)
		done = S.be->copy(S.option_folder, text, folder, sizeof(folder));
	else
		done = S.be->rename(S.option_folder, text, folder, sizeof(folder));
	if (!done)
	{
		notice_set(1, 6000, "The set could not be %s (the log says why).", S.name_mode == NAME_NEW ? "made" : S.name_mode == NAME_COPY ? "copied" :
			"renamed");
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	sets_read();
	if (S.name_mode == NAME_NEW)
	{
		set_open(folder, text);
		return;
	}
	list_to(step_count(), 0);
	focus_set(folder);
	notice_set(0, 4000, S.name_mode == NAME_COPY ? "%s made." : "Renamed to %s.", text);
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void name_keyboard_done(int keep, void *context)
{
	(void)context;
	S.naming_keyboard = 0;
	if (S.name_edit.active && S.name_edit.text == &S.name_text)
	{
		S.naming = 1;
		return;
	}
	if (keep)
		name_commit();
	else
		name_cancel();
}

static void name_begin(void)
{
	if (S.overlay != OVERLAY_NAME)
	{
		S.overlay = OVERLAY_NAME;
		ae_text_init(&S.name_text, S.name_mode != NAME_NEW ? S.option_name : S.staged && S.scene == AE_IS_SCENE_NEW ? "Test Kit" : "", 40, 0);
	}
	S.name_edit.done = name_typing_done;
	S.name_edit.context = NULL;
	S.name_edit.holder = &S;
	if (S.device == AE_DEVICE_KEYBOARD_MOUSE || S.staged)
	{
		ae_field_edit_begin(&S.name_edit, &S.name_text);
		ae_text_select_all(&S.name_text);
		S.naming = 1;
		S.caret_since = ae_motion_now();
	}
	else
	{
		struct ae_keyboard_spec spec;

		memset(&spec, 0, sizeof(spec));
		spec.text = &S.name_text;
		spec.field = S.name_well;
		spec.bounds = S.flow.frame.rect;
		spec.density = S.flow.density;
		spec.done = name_keyboard_done;
		spec.edit = &S.name_edit;
		if (ae_keyboard_open(&spec, AE_OWNER_ANY))
			S.naming_keyboard = 1;
	}
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void options_open(struct ae_is_set const *set)
{
	snprintf(S.option_folder, sizeof(S.option_folder), "%s", set->folder);
	snprintf(S.option_name, sizeof(S.option_name), "%s", set->name);
	S.option_playing = set->playing;
	S.overlay = OVERLAY_OPTIONS;
	S.overlay_focus = 0;
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void options_pick(int row)
{
	if (row < 0 || row >= OPTIONS)
		return;
	S.overlay = OVERLAY_NONE;
	switch (row)
	{
	case OPTION_OPEN:
		set_open(S.option_folder, S.option_name);
		break;
	case OPTION_PLAY:
		set_play_toggle(S.option_folder, S.option_name, !S.option_playing);
		break;
	case OPTION_RENAME:
		S.name_mode = NAME_RENAME;
		name_begin();
		break;
	case OPTION_COPY:
		S.name_mode = NAME_COPY;
		name_begin();
		break;
	case OPTION_DELETE:
		delete_set_ask(S.option_folder, S.option_name);
		break;
	}
}

/* ---------- typing in the search field */

static void typing_done(int keep, void *context)
{
	(void)keep;
	(void)context;
	S.typing = 0;
}

static void keyboard_done(int keep, void *context)
{
	(void)keep;
	(void)context;
	S.keyboard = 0;
}

static void search_changed(void)
{
	if (S.step == AE_IS_STEP_MAPS)
	{
		maps_view();
		list_to(S.view_count, 0);
	}
	else if (S.step == AE_IS_STEP_ITEMS)
	{
		items_view();
		list_to(S.item_view_count, 0);
	}
}

static void typing_begin(int device)
{
	S.ctl = CTL_SEARCH;
	S.edit.done = typing_done;
	S.edit.context = NULL;
	S.edit.holder = &S;
	if (device == AE_DEVICE_KEYBOARD_MOUSE)
	{
		ae_field_edit_begin(&S.edit, &S.search);
		ae_text_select_all(&S.search);
		S.typing = 1;
		S.caret_since = ae_motion_now();
		ae_sound_request(AE_SOUND_FORWARD, 0);
	}
	else
	{
		struct ae_keyboard_spec spec;

		memset(&spec, 0, sizeof(spec));
		spec.text = &S.search;
		spec.field = S.well;
		spec.bounds = S.flow.frame.rect;
		spec.density = S.flow.density;
		spec.done = keyboard_done;
		spec.edit = &S.edit;
		if (ae_keyboard_open(&spec, AE_OWNER_ANY))
			S.keyboard = 1;
	}
}

static void typing_end(int keep)
{
	if (!S.typing)
		return;
	S.typing = 0;
	ae_field_edit_end(&S.edit, keep);
	ae_sound_request(keep ? AE_SOUND_FORWARD : AE_SOUND_BACK, 0);
}

/* ---------- the slow steps */

static void op_start(int op, int row)
{
	S.op = op;
	S.op_row = row;
	S.op_wait = 1;
}

static void op_run(void)
{
	int op = S.op;

	if (S.frozen)
		return;
	if (op == OP_OPEN_MAP)
	{
		char reason[200];
		struct ae_is_map const *map = map_focused();

		S.op = OP_NONE;
		if (map && S.be->map_open((int)(map - S.maps), reason, sizeof(reason)))
		{
			S.map_open = (int)(map - S.maps);
			S.map_row = S.list.list.focus;
			kinds_read();
			step_to(AE_IS_STEP_KINDS, 0);
			ae_sound_request(AE_SOUND_FORWARD, 0);
			platform_log("item sets: %s opened", map->file);
		}
		else
		{
			char words[240];

			ae_is_reason_words(reason, words, sizeof(words));
			notice_set(1, 7000, "%s", words[0] ? words : "The map can't be opened (the log says why).");
			ae_sound_request(AE_SOUND_FAILURE, 0);
		}
	}
	else if (op == OP_ADD)
	{
		struct ae_is_item *item = item_focused();
		long index = item ? item->index : -1;
		char name[128];

		S.op = OP_NONE;
		if (!item)
			return;
		ae_is_item_name(item->path, name, sizeof(name));
		S.be->item_add(S.folder, S.kind, (int)index, &S.result);
		S.result_index = index;
		S.result_valid = 1;
		platform_log("item sets: %s %s %s", name, S.result.ok ? "added to" : "not added to", S.folder);
		ae_sound_request(S.result.ok ? AE_SOUND_FORWARD : AE_SOUND_FAILURE, 0);
		{
			short focus = S.list.list.focus;

			items_read(S.kind);
			maps_read();
			sets_read();
			list_to(S.item_view_count, focus < S.item_view_count ? focus : 0);
		}
	}
}

/* ---------- campaign swaps: what a press does */

static void swap_delete_ask(struct ae_is_swap const *swap)
{
	char title[96], body[200];

	snprintf(question.folder, sizeof(question.folder), "%s", swap->file);
	snprintf(question.name, sizeof(question.name), "%s", swap->name);
	snprintf(title, sizeof(title), "Delete %s?", swap->name);
	if (swap->set_found)
		snprintf(body, sizeof(body), "%s stays as it is. Only the swap's choices of what to replace go.", swap->set_name);
	else
		snprintf(body, sizeof(body), "Its set is already gone. Only the swap's choices of what to replace go.");
	question_ask(ASK_DELETE_SWAP, title, body);
}

static void swaps_open_list(int focus)
{
	swaps_read();
	sets_read();
	S.step = AE_IS_STEP_SWAPS;
	S.ctl = -1;
	list_to(step_count(), focus);
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

/* NEW SWAP: its name, then the set it plays; it waits with no set that has an item on, and at the limit */
static void swap_new_begin(void)
{
	int sets[AE_IS_SETS_MOST];

	if (ae_is_limit_word(2, S.swap_count)[0])
	{
		notice_set(1, 5000, "64 swaps is the most. Delete one to make room.");
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	if (!sets_with_items(sets, AE_IS_SETS_MOST))
	{
		notice_set(1, 6000, "Add items to a set first: ITEM SETS \xE2\x80\xBA a set \xE2\x80\xBA ADD ITEMS FROM A MAP.");
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	S.name_mode = NAME_SWAP_NEW;
	S.option_name[0] = 0;
	name_begin();
}

static void setpick_begin(void)
{
	S.pick_count = sets_with_items(S.pick_sets, AE_IS_SETS_MOST);
	S.pick_focus = 0;
	S.overlay = OVERLAY_SETPICK;
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void setpick_done(int row)
{
	char file[64];

	if (row < 0 || row >= S.pick_count)
		return;
	S.overlay = OVERLAY_NONE;
	if (!S.be->swap_create(S.pending_swap, S.sets[S.pick_sets[row]].folder, file, sizeof(file)))
	{
		notice_set(1, 6000, "The swap could not be made (the log says why).");
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	swap_open(file);
	notice_set(0, 5000, "%s plays %s. Pick what it replaces below.", S.pending_swap, S.sets[S.pick_sets[row]].name);
}

/* a swap's value row stepped (direction +1 or -1) */
static void swap_step(int direction)
{
	struct swap_line const *line = swline_focused();
	int row, index, count, next, each;
	char const *key;

	if (!line)
		return;
	row = line->row;
	switch (row)
	{
	case AE_IS_SW_SET:
	{
		int sets[AE_IS_SETS_MOST], found = sets_with_items(sets, AE_IS_SETS_MOST);

		index = -1;
		for (each = 0; each < found; each++)
			if (same_text(S.sets[sets[each]].folder, S.swap.set_folder))
				index = each;
		if (!found)
			return;
		next = index < 0 ? 0 : ae_is_value_step(found, index, direction);
		if (!S.be->swap_set_option(S.swap_file, "profile", S.sets[sets[next]].folder))
			return;
		/* (another set's items are not the old set's: the picks of its items stay in the file, and a body of the old set goes) */
		S.be->swap_set_option(S.swap_file, "player_biped", "");
		S.be->swap_set_option(S.swap_file, "player_hands", "");
		S.be->swap_set_option(S.swap_file, "player_movement", "");
		break;
	}
	case AE_IS_SW_BIPED:
		count = 1 + S.sw_biped_count;
		index = swap_biped_index();
		next = ae_is_value_step(count, index, direction);
		if (!S.be->swap_set_option(S.swap_file, "player_biped", next == 0 ? "" : S.sw_entries[S.sw_bipeds[next - 1]].path))
			return;
		S.be->swap_set_option(S.swap_file, "player_hands", "");
		S.be->swap_set_option(S.swap_file, "player_movement", "");
		break;
	case AE_IS_SW_HANDS:
		return;
	default:
		/* (MOVEMENT is stored with the swap, but the spawn does not play it yet: its row waits) */
		count = ae_is_swap_value_count(row);
		if (!count || (row == AE_IS_SW_MOVEMENT && !ae_is_body_rows_live()))
			return;
		index = ae_is_swap_value_index(row, S.swap_val[row]);
		next = ae_is_value_step(count, index, direction);
		key = ae_is_swap_value_key(row, next);
		if (!S.be->swap_set_option(S.swap_file, ae_is_swap_row_file_key(row), key))
			return;
		break;
	}
	swap_reload(row, line->slot);
	settings_changed("Changes play from the next level you load.");
}

/* an enemy's slot: its ranks first when it has them, else the slot itself */
static void ranks_open(int enemy)
{
	S.slot_row = AE_IS_SW_ENEMY_SLOT;
	S.slot_slot = enemy;
	if (tag_campaign_enemy_rank_count((short)enemy) > 0)
	{
		S.step = AE_IS_STEP_RANKS;
		S.ctl = -1;
		list_to(step_count(), 0);
		ae_sound_request(AE_SOUND_FORWARD, 0);
	}
	else
		slot_open(AE_IS_SW_ENEMY_SLOT, enemy, 0);
}

static void swap_accept(void)
{
	struct swap_line const *line = swline_focused();

	if (!line)
		return;
	if (line->row == AE_IS_SW_NAME)
	{
		S.name_mode = NAME_SWAP_RENAME;
		snprintf(S.option_name, sizeof(S.option_name), "%s", S.swap.name);
		name_begin();
	}
	else if (line->row == AE_IS_SW_WEAPON_SLOT)
		slot_open(AE_IS_SW_WEAPON_SLOT, line->slot, 0);
	else if (line->row == AE_IS_SW_ENEMY_SLOT)
		ranks_open(line->slot);
	else
		swap_step(1);
}

/* a pick of a slot turned on or off */
static void slot_toggle(int direction)
{
	struct slot_line const *line = slot_focused();
	char key[64];
	short focus = S.list.list.focus;

	(void)direction;
	if (!line)
		return;
	slot_key(S.slot_row, S.slot_slot, S.slot_rank, key, sizeof(key));
	if (!S.be->swap_list_set(S.swap_file, slot_list_section(line->section), key, line->path, !line->on))
	{
		notice_set(1, 5000, "The swap can't be written: the log says why.");
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	slot_reload(focus);
	settings_changed("Changes play from the next mission you load.");
}

/* TRY IT ON THE FIRING RANGE: the DEV `range` command's start, with the set open */
static void range_try(void)
{
	char const *wait = range_wait(), *why_wait;
	char why[160];

	if (wait[0])
	{
		why_wait = !strcmp(wait, "NOTHING ON") ? "No item of the set is on: turn one on first." :
			"The firing range is built on Blood Gulch, and this PC has no Xbox-format bloodgulch.map.";
		notice_set(1, 6000, "%s", why_wait);
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	if (!S.be->range_start(S.folder, why, sizeof(why)))
	{
		notice_set(1, 6000, "The firing range did not start: %s.", why);
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	platform_log("item sets: the firing range starts with %s", S.folder);
	notice_set(0, 4000, "The firing range is loading with %s.", S.set_name);
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

/* ---------- accept */

static void accept(void)
{
	switch (S.step)
	{
	case AE_IS_STEP_SETS:
	{
		struct ae_is_set const *set = set_focused();

		if (home_row() == HOME_SWAPS)
		{
			swaps_open_list(0);
			break;
		}
		if (home_row() == HOME_MULTIPLAYER || home_row() == HOME_HEAD)
		{
			ae_sound_request(AE_SOUND_FAILURE, 0);
			break;
		}
		if (!set)
		{
			if (ae_is_limit_word(0, S.set_count)[0])
			{
				notice_set(1, 5000, "64 sets is the most. Delete one to make room.");
				ae_sound_request(AE_SOUND_FAILURE, 0);
				break;
			}
			S.name_mode = NAME_NEW;
			snprintf(S.option_name, sizeof(S.option_name), "%s", "");
			name_begin();
		}
		else
			options_open(set);
		break;
	}
	case AE_IS_STEP_SWAPS:
	{
		struct ae_is_swap const *swap = swap_focused();

		if (swap)
			swap_open(swap->file);
		else
			swap_new_begin();
		break;
	}
	case AE_IS_STEP_SWAP:
		swap_accept();
		break;
	case AE_IS_STEP_RANKS:
	{
		int rank = tag_campaign_enemy_rank((short)S.slot_slot, (short)S.list.list.focus);

		slot_open(AE_IS_SW_ENEMY_SLOT, S.slot_slot, rank);
		break;
	}
	case AE_IS_STEP_SLOT:
		slot_toggle(1);
		break;
	case AE_IS_STEP_CONTENTS:
	{
		struct ae_is_entry *entry = entry_focused();

		if (S.list.list.focus >= 0 && S.list.list.focus < S.row_count && S.rows[S.list.list.focus].type == ROW_RANGE)
			range_try();
		else if (S.list.list.focus >= 0 && S.list.list.focus < S.row_count && S.rows[S.list.list.focus].type == ROW_ADD)
		{
			if (ae_is_limit_word(1, S.entry_count)[0])
			{
				notice_set(1, 5000, "256 items is the most a set holds. Remove one to make room.");
				ae_sound_request(AE_SOUND_FAILURE, 0);
				break;
			}
			maps_read();
			step_to(AE_IS_STEP_MAPS, 0);
			ae_sound_request(AE_SOUND_FORWARD, 0);
		}
		else if (entry)
			settings_open(entry, -1);
		break;
	}
	case AE_IS_STEP_SETTINGS:
		settings_accept();
		break;
	case AE_IS_STEP_MAPS:
	{
		struct ae_is_map const *map = map_focused();
		char const *reason;

		if (!map)
			break;
		reason = S.be->maps_reason(map->source);
		if (reason)
		{
			notice_set(1, 6000, "%s", reason);
			ae_sound_request(AE_SOUND_FAILURE, 0);
			break;
		}
		op_start(OP_OPEN_MAP, S.list.list.focus);
		break;
	}
	case AE_IS_STEP_KINDS:
		if (S.list.list.focus >= 0 && S.list.list.focus < AE_IS_KINDS && S.kind_counts[S.list.list.focus] > 0)
		{
			items_read(S.list.list.focus);
			S.search.text[0] = 0;
			step_to(AE_IS_STEP_ITEMS, 0);
			ae_sound_request(AE_SOUND_FORWARD, 0);
		}
		else
			ae_sound_request(AE_SOUND_FAILURE, 0);
		break;
	default:
	{
		struct ae_is_item *item = item_focused();

		if (!item)
			break;
		if (item->in_set)
		{
			notice_set(0, 4000, "Already in %s. Y removes it.", S.set_name);
			ae_sound_request(AE_SOUND_FAILURE, 0);
		}
		else if (!S.be->kind_addable(S.kind))
		{
			S.result_valid = 0;
			item->parts = 0;
			item->refused = 1;
			item->reason[0] = 0;
			ae_sound_request(AE_SOUND_FAILURE, 0);
		}
		else
		{
			if (item->parts < 0)
				S.be->item_look(S.kind, (int)item->index, item);
			if (item->refused)
			{
				S.result_valid = 0;
				ae_sound_request(AE_SOUND_FAILURE, 0);
			}
			else
				op_start(OP_ADD, S.list.list.focus);
		}
		break;
	}
	}
}

static void back_up(void)
{
	switch (S.step)
	{
	case AE_IS_STEP_SETS:
		ae_sound_request(AE_SOUND_BACK, 0);
		/* (B leaves MAPS & MODS: this page, then the hub under it; from the lobby's picker, this page alone) */
		ae_ui_pop();
		if (!S.from_picker)
			ae_ui_pop();
		S.from_picker = 0;
		return;
	case AE_IS_STEP_CONTENTS:
		sets_read();
		step_to(AE_IS_STEP_SETS, 0);
		focus_set(S.folder);
		break;
	case AE_IS_STEP_SWAPS:
		if (S.from_picker)
		{
			/* (opened from the lobby's picker on a swap: B goes back to it) */
			S.from_picker = 0;
			ae_ui_pop();
			break;
		}
		sets_read();
		swaps_read();
		step_to(AE_IS_STEP_SETS, S.set_count + HOME_SWAPS);
		break;
	case AE_IS_STEP_SWAP:
	{
		int each, focus = 0;

		swaps_read();
		for (each = 0; each < S.swap_count; each++)
			if (same_text(S.swaps[each].file, S.swap_file))
				focus = each;
		step_to(AE_IS_STEP_SWAPS, focus);
		break;
	}
	case AE_IS_STEP_RANKS:
	case AE_IS_STEP_SLOT:
	{
		int each, focus = 0;

		if (S.step == AE_IS_STEP_SLOT && S.slot_row == AE_IS_SW_ENEMY_SLOT && tag_campaign_enemy_rank_count((short)S.slot_slot) > 0)
		{
			S.step = AE_IS_STEP_RANKS;
			list_to(step_count(), 0);
			for (each = 0; each < tag_campaign_enemy_rank_count((short)S.slot_slot); each++)
				if (tag_campaign_enemy_rank((short)S.slot_slot, (short)each) == S.slot_rank)
					focus = each;
			ae_list_set_focus(&S.list.list, (short)focus);
			break;
		}
		swap_read();
		S.step = AE_IS_STEP_SWAP;
		list_to(S.swline_count, 0);
		for (each = 0; each < S.swline_count; each++)
			if (!S.swlines[each].header && S.swlines[each].row == S.slot_row && S.swlines[each].slot == S.slot_slot)
				focus = each;
		ae_list_set_focus(&S.list.list, (short)focus);
		break;
	}
	case AE_IS_STEP_SETTINGS:
	{
		int each, focus = 0;

		entries_read();
		sets_read();
		for (each = 0; each < S.row_count; each++)
			if (S.rows[each].type == ROW_ENTRY && S.entries[S.rows[each].index].tag == S.set_tag)
				focus = each;
		step_to(AE_IS_STEP_CONTENTS, focus);
		break;
	}
	case AE_IS_STEP_MAPS:
		S.search.text[0] = 0;
		step_to(AE_IS_STEP_CONTENTS, 0);
		break;
	case AE_IS_STEP_KINDS:
		S.be->map_close();
		S.map_open = -1;
		step_to(AE_IS_STEP_MAPS, S.map_row);
		break;
	default:
	{
		int kind = S.kind;

		S.search.text[0] = 0;
		step_to(AE_IS_STEP_KINDS, kind);
		break;
	}
	}
	ae_sound_request(AE_SOUND_BACK, 0);
}

/* ---------- events */

static void nav(struct ae_event const *event)
{
	int direction = event->action == AE_ACTION_UP ? -1 : 1;
	int has_controls = S.step == AE_IS_STEP_MAPS;

	if (has_controls && S.ctl >= 0)
	{
		if (direction > 0)
		{
			if (S.ctl < CTL_SHOW)
				S.ctl++;
			else if (S.list_count > 0)
				S.ctl = -1;
		}
		else if (S.ctl > CTL_SEARCH)
			S.ctl--;
		ae_sound_request(AE_SOUND_CURSOR, event->repeat);
		return;
	}
	if (has_controls && direction < 0 && S.list.list.focus <= 0)
	{
		S.ctl = CTL_SHOW;
		ae_sound_request(AE_SOUND_CURSOR, event->repeat);
		return;
	}
	ae_list_view_event(&S.list, event);
	/* (a section label of a set's items is not a stop: on past it) */
	if (S.step == AE_IS_STEP_CONTENTS)
	{
		int guard = 3;

		while (guard-- > 0 && S.list.list.focus >= 0 && S.list.list.focus < S.row_count && S.rows[S.list.list.focus].type == ROW_HEADER)
			ae_list_view_event(&S.list, event);
	}
	else if (S.step == AE_IS_STEP_SETTINGS)
	{
		int guard = 3;

		while (guard-- > 0 && S.list.list.focus >= 0 && S.list.list.focus < S.srow_count && S.srows[S.list.list.focus].header)
			ae_list_view_event(&S.list, event);
	}
	else if (S.step == AE_IS_STEP_SETS)
	{
		/* (HOW SETS PLAY's head is no stop) */
		if (home_row() == HOME_HEAD)
			ae_list_view_event(&S.list, event);
	}
	else if (S.step == AE_IS_STEP_SWAP)
	{
		int guard = 3;

		while (guard-- > 0 && S.list.list.focus >= 0 && S.list.list.focus < S.swline_count && S.swlines[S.list.list.focus].header)
			ae_list_view_event(&S.list, event);
	}
	else if (S.step == AE_IS_STEP_SLOT)
	{
		int guard = 3;

		while (guard-- > 0 && S.list.list.focus >= 0 && S.list.list.focus < S.sline_count && S.slines[S.list.list.focus].head)
			ae_list_view_event(&S.list, event);
	}
}

static void show_step(int direction)
{
	S.show = (S.show + direction + SHOWS) % SHOWS;
	maps_view();
	list_to(S.view_count, 0);
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

static int handle(struct ae_screen *instance, struct ae_event const *event)
{
	(void)instance;
	S.device = event->device;
	/* (a staged ADDING scene holds still; B lets it go) */
	if (S.frozen && event->action == AE_ACTION_BACK)
	{
		S.frozen = 0;
		S.op = OP_NONE;
		return 1;
	}
	if (S.op != OP_NONE)
		return 1;
	if (S.overlay == OVERLAY_NAME)
	{
		if (event->action == AE_ACTION_ACCEPT || event->action == AE_ACTION_START)
		{
			if (!S.naming && !S.naming_keyboard)
				name_begin();
			else if (S.naming)
				name_commit();
		}
		else if (event->action == AE_ACTION_BACK)
			name_cancel();
		return 1;
	}
	if (S.overlay == OVERLAY_SETPICK)
	{
		switch (event->action)
		{
		case AE_ACTION_BACK:
			S.overlay = OVERLAY_NONE;
			ae_sound_request(AE_SOUND_BACK, 0);
			break;
		case AE_ACTION_UP:
		case AE_ACTION_DOWN:
			if (S.pick_count)
				S.pick_focus = (S.pick_focus + (event->action == AE_ACTION_UP ? S.pick_count - 1 : 1)) % S.pick_count;
			ae_sound_request(AE_SOUND_CURSOR, event->repeat);
			break;
		case AE_ACTION_ACCEPT:
			setpick_done(S.pick_focus);
			break;
		default:
			break;
		}
		return 1;
	}
	if (S.overlay == OVERLAY_OPTIONS)
	{
		switch (event->action)
		{
		case AE_ACTION_BACK:
			S.overlay = OVERLAY_NONE;
			ae_sound_request(AE_SOUND_BACK, 0);
			break;
		case AE_ACTION_UP:
		case AE_ACTION_DOWN:
			S.overlay_focus = (S.overlay_focus + (event->action == AE_ACTION_UP ? OPTIONS - 1 : 1)) % OPTIONS;
			ae_sound_request(AE_SOUND_CURSOR, event->repeat);
			break;
		case AE_ACTION_ACCEPT:
			options_pick(S.overlay_focus);
			break;
		default:
			break;
		}
		return 1;
	}
	if (S.typing)
	{
		if (event->action == AE_ACTION_START || event->action == AE_ACTION_ACCEPT)
		{
			typing_end(1);
			search_changed();
		}
		else if (event->action == AE_ACTION_BACK)
		{
			typing_end(0);
			search_changed();
		}
		else if (event->action == AE_ACTION_UP || event->action == AE_ACTION_DOWN)
		{
			typing_end(1);
			search_changed();
			nav(event);
		}
		return 1;
	}
	switch (event->action)
	{
	case AE_ACTION_BACK:
		back_up();
		return 1;
	case AE_ACTION_TAB_PREVIOUS:
	case AE_ACTION_TAB_NEXT:
		ae_ui_pop();
		ae_screen_maps_hub_step(event->action == AE_ACTION_TAB_NEXT ? 1 : -1);
		return 1;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		nav(event);
		return 1;
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
		if (S.step == AE_IS_STEP_MAPS && S.ctl == CTL_SHOW)
			show_step(event->action == AE_ACTION_RIGHT ? 1 : -1);
		else if (S.step == AE_IS_STEP_CONTENTS && entry_focused())
		{
			/* (Content Evolved's keys: left or right turns the item on or off) */
			struct ae_is_entry const *entry = entry_focused();
			short focus = S.list.list.focus;

			S.be->entry_enable(S.folder, entry->tag, !entry->on || entry->failed[0]);
			entries_read();
			sets_read();
			list_to(S.row_count, focus < S.row_count ? focus : 0);
			ae_sound_request(AE_SOUND_FORWARD, 0);
			notice_set(0, 4000, "Changes play from the next level you load.");
		}
		else if (S.step == AE_IS_STEP_SETTINGS)
			settings_step(event->action == AE_ACTION_RIGHT ? 1 : -1);
		else if (S.step == AE_IS_STEP_SWAP)
			swap_step(event->action == AE_ACTION_RIGHT ? 1 : -1);
		else if (S.step == AE_IS_STEP_SLOT)
			slot_toggle(event->action == AE_ACTION_RIGHT ? 1 : -1);
		return 1;
	case AE_ACTION_PAGE_UP:
	case AE_ACTION_PAGE_DOWN:
		if (S.ctl < 0)
			ae_list_view_event(&S.list, event);
		return 1;
	case AE_ACTION_ACCEPT:
		if (S.ctl == CTL_SEARCH)
			typing_begin(event->device);
		else if (S.ctl == CTL_SHOW)
			show_step(1);
		else
			accept();
		return 1;
	case AE_ACTION_X:
	case AE_ACTION_FIND:
		if (S.step == AE_IS_STEP_SETS)
		{
			if (ae_is_limit_word(0, S.set_count)[0])
			{
				notice_set(1, 5000, "64 sets is the most. Delete one to make room.");
				ae_sound_request(AE_SOUND_FAILURE, 0);
				return 1;
			}
			S.name_mode = NAME_NEW;
			S.option_name[0] = 0;
			name_begin();
		}
		else if (S.step == AE_IS_STEP_SWAPS)
			swap_new_begin();
		else if (S.step == AE_IS_STEP_CONTENTS)
		{
			if (ae_is_limit_word(1, S.entry_count)[0])
			{
				notice_set(1, 5000, "256 items is the most a set holds. Remove one to make room.");
				ae_sound_request(AE_SOUND_FAILURE, 0);
				return 1;
			}
			maps_read();
			step_to(AE_IS_STEP_MAPS, 0);
			ae_sound_request(AE_SOUND_FORWARD, 0);
		}
		else if (S.step == AE_IS_STEP_MAPS || S.step == AE_IS_STEP_ITEMS)
			typing_begin(event->device);
		return 1;
	case AE_ACTION_Y:
	case AE_ACTION_DELETE:
		if (S.step == AE_IS_STEP_SETS)
		{
			struct ae_is_set const *set = set_focused();

			if (set && event->action == AE_ACTION_Y)
				set_play_toggle(set->folder, set->name, !set->playing);
			else if (set)
				delete_set_ask(set->folder, set->name);
		}
		else if (S.step == AE_IS_STEP_SWAPS)
		{
			struct ae_is_swap const *swap = swap_focused();

			if (swap)
				swap_delete_ask(swap);
		}
		else if (S.step == AE_IS_STEP_SWAP)
			swap_delete_ask(&S.swap);
		else if (S.step == AE_IS_STEP_CONTENTS)
		{
			struct ae_is_entry const *entry = entry_focused();

			if (entry)
				remove_entry_ask(entry);
		}
		else if (S.step == AE_IS_STEP_SETTINGS)
		{
			struct ae_is_entry const *entry = settings_entry();

			if (entry)
				remove_entry_ask(entry);
		}
		else if (S.step == AE_IS_STEP_ITEMS)
		{
			struct ae_is_item const *item = item_focused();

			if (item && item->in_set)
				remove_item_ask(item);
		}
		return 1;
	default:
		return 1;
	}
}

static void pointer(struct ae_screen *instance, struct ae_pointer const *pointer_event)
{
	struct ae_event event;
	struct ae_hit hit;
	short item;

	if (ae_prompts_pointer(S.flow.prompts, S.flow.prompt_count, pointer_event, HIT_PROMPTS, &event))
	{
		handle(instance, &event);
		return;
	}
	if (pointer_event->right_clicks)
	{
		event.player = pointer_event->player;
		event.action = AE_ACTION_BACK;
		event.device = AE_DEVICE_KEYBOARD_MOUSE;
		event.repeat = 0;
		handle(instance, &event);
		return;
	}
	if (S.op != OP_NONE)
		return;
	if (S.overlay == OVERLAY_NAME)
	{
		if (pointer_event->left_clicks && !S.naming_keyboard && ae_hit_at(pointer_event->x, pointer_event->y, 0, &hit) && hit.id != HIT_POP)
			name_cancel();
		return;
	}
	if (S.overlay)
	{
		if (pointer_event->left_clicks && ae_hit_at(pointer_event->x, pointer_event->y, 0, &hit))
		{
			if (hit.id == HIT_POP && hit.part != AE_PART_CARD && S.overlay == OVERLAY_SETPICK)
			{
				S.pick_focus = hit.index;
				setpick_done(hit.index);
			}
			else if (hit.id == HIT_POP && hit.part != AE_PART_CARD)
			{
				S.overlay_focus = hit.index;
				options_pick(hit.index);
			}
			else if (hit.id != HIT_POP)
				S.overlay = OVERLAY_NONE;
		}
		return;
	}
	if (ae_screen_maps_hub_strip_pointer(pointer_event))
		return;
	if (pointer_event->left_clicks && ae_hit_at(pointer_event->x, pointer_event->y, 0, &hit))
	{
		if (hit.id == HIT_FIELD)
		{
			if (!S.typing)
				typing_begin(AE_DEVICE_KEYBOARD_MOUSE);
			return;
		}
		if (S.typing)
		{
			typing_end(1);
			search_changed();
		}
		if (hit.id == HIT_CTL)
		{
			S.ctl = hit.index;
			if (hit.index == CTL_SHOW)
				show_step(hit.part == AE_PART_ARROW_LEFT ? -1 : 1);
			return;
		}
	}
	if (pointer_event->left_clicks && S.step == AE_IS_STEP_CONTENTS && ae_hit_at(pointer_event->x, pointer_event->y, 0, &hit) &&
		hit.id == HIT_BOX && hit.index >= 0 && hit.index < S.row_count && S.rows[hit.index].type == ROW_ENTRY)
	{
		/* (the check box itself: the item turned on or off) */
		struct ae_is_entry const *entry = &S.entries[S.rows[hit.index].index];

		S.ctl = -1;
		S.be->entry_enable(S.folder, entry->tag, !entry->on || entry->failed[0]);
		entries_read();
		sets_read();
		list_to(S.row_count, hit.index);
		ae_sound_request(AE_SOUND_FORWARD, 0);
		notice_set(0, 4000, "Changes play from the next level you load.");
		return;
	}
	if (pointer_event->left_clicks && S.step == AE_IS_STEP_SLOT && ae_hit_at(pointer_event->x, pointer_event->y, 0, &hit) &&
		hit.id == HIT_BOX && hit.index >= 0 && hit.index < S.sline_count && !S.slines[hit.index].head)
	{
		/* (the check box itself: the pick turned on or off) */
		S.ctl = -1;
		ae_list_set_focus(&S.list.list, (short)hit.index);
		slot_toggle(1);
		return;
	}
	if (pointer_event->left_clicks && S.step == AE_IS_STEP_SWAP && ae_hit_at(pointer_event->x, pointer_event->y, 0, &hit) &&
		hit.id == HIT_LIST && hit.index >= 0 && hit.index < S.swline_count && !S.swlines[hit.index].header &&
		(hit.part == AE_PART_ARROW_LEFT || hit.part == AE_PART_ARROW_RIGHT))
	{
		S.ctl = -1;
		ae_list_set_focus(&S.list.list, (short)hit.index);
		swap_step(hit.part == AE_PART_ARROW_LEFT ? -1 : 1);
		return;
	}
	if (pointer_event->left_clicks && S.step == AE_IS_STEP_SETTINGS && ae_hit_at(pointer_event->x, pointer_event->y, 0, &hit) &&
		hit.id == HIT_LIST && hit.index >= 0 && hit.index < S.srow_count && !S.srows[hit.index].header &&
		(hit.part == AE_PART_ARROW_LEFT || hit.part == AE_PART_ARROW_RIGHT))
	{
		/* (a value's arrow: the row steps that way) */
		S.ctl = -1;
		ae_list_set_focus(&S.list.list, (short)hit.index);
		settings_step(hit.part == AE_PART_ARROW_LEFT ? -1 : 1);
		return;
	}
	item = ae_list_view_pointer(&S.list, pointer_event, HIT_LIST);
	if (item >= 0 && item < S.list_count)
	{
		if (S.step == AE_IS_STEP_CONTENTS && S.rows[item].type == ROW_HEADER)
			return;
		if (S.step == AE_IS_STEP_SETTINGS && S.srows[item].header)
			return;
		if (S.step == AE_IS_STEP_SETS && item - S.set_count == HOME_HEAD)
			return;
		if (S.step == AE_IS_STEP_SWAP && S.swlines[item].header)
			return;
		if (S.step == AE_IS_STEP_SLOT && S.slines[item].head)
			return;
		if (S.ctl < 0 && item == S.list.list.focus)
			accept();
		else
		{
			S.ctl = -1;
			ae_list_set_focus(&S.list.list, item);
		}
	}
}

/* ---------- drawing */

/* the swaps' steps: their list, a swap's page, an enemy's ranks and a slot; the place line, the count line, the rows and the card.
Returns what the footer needs (ae_is_hints' has_items) */
static int draw_swap_steps(float x, float width, float card_x, float card_width, float top, float height)
{
	char const *parts[5];
	char swap_upper[96], slot_upper[96], rank_upper[64], left[120], number[32], name[96];
	float y = Y(290.0f);
	int count = 2, has = 0;
	struct ae_density const *d = &S.flow.density;

	upper_copy(swap_upper, sizeof(swap_upper), S.step >= AE_IS_STEP_SWAP ? S.swap.name : "");
	slot_upper[0] = 0;
	rank_upper[0] = 0;
	if (S.step >= AE_IS_STEP_RANKS)
		upper_copy(slot_upper, sizeof(slot_upper), S.slot_row == AE_IS_SW_ENEMY_SLOT ? tag_campaign_enemy_name((short)S.slot_slot) :
			tag_campaign_weapon_name((short)S.slot_slot));
	if (S.step == AE_IS_STEP_SLOT && S.slot_row == AE_IS_SW_ENEMY_SLOT && tag_campaign_enemy_rank_count((short)S.slot_slot) > 0)
		upper_copy(rank_upper, sizeof(rank_upper), tag_campaign_rank_name((short)S.slot_rank));
	parts[0] = "ITEM SETS";
	parts[1] = "CAMPAIGN SWAPS";
	if (S.step >= AE_IS_STEP_SWAP)
		parts[count++] = swap_upper;
	if (S.step >= AE_IS_STEP_RANKS)
		parts[count++] = slot_upper;
	if (rank_upper[0])
		parts[count++] = rank_upper;
	place_line(x, width, parts, count, "");
	switch (S.step)
	{
	case AE_IS_STEP_SWAPS:
		if (S.swap_count)
		{
			ae_hub_count_text((unsigned long long)S.swap_count, number, sizeof(number));
			snprintf(left, sizeof(left), "YOUR SWAPS" DOT "%s", number);
		}
		else
			snprintf(left, sizeof(left), "YOUR SWAPS" DOT "NONE");
		count_line(x, y, width, left, ae_is_swaps_count_right());
		y += U(32.0f);
		if (!S.swap_count)
		{
			char wrapped[5][160];
			int lines = ae_wrap_text(AE_FONT_BODY, ae_size_text(d) * 0.9f, ae_is_swaps_empty_words(), width - U(20.0f), wrapped, 5), each;

			for (each = 0; each < lines; each++)
				text_shadow(AE_FONT_BODY, ae_size_text(d) * 0.9f, x + U(10.0f), y + U(8.0f) + U(31.0f) * (float)each, AE_ALIGN_LEFT, AE_COLOR_TEXT,
					wrapped[each]);
			y += U(8.0f) + U(31.0f) * (float)lines + U(20.0f);
		}
		has = swap_focused() != NULL;
		page_list(x, y, width);
		swaps_card(card_x, top, card_width, height);
		break;
	case AE_IS_STEP_SWAP:
	{
		struct swap_line const *line = swline_focused();

		has = line ? ae_is_swap_row_change(line->row) : 0;
		page_list(x, y, width);
		swap_card(card_x, top, card_width, height);
		break;
	}
	case AE_IS_STEP_RANKS:
		snprintf(left, sizeof(left), "%s" DOT "%d RANKS", slot_upper, tag_campaign_enemy_rank_count((short)S.slot_slot));
		count_line(x, y, width, left, "");
		y += U(32.0f);
		page_list(x, y, width);
		ranks_card(card_x, top, card_width, height);
		break;
	default:
	{
		int picks = slot_section_on(0) + (S.slot_row == AE_IS_SW_WEAPON_SLOT ? 0 : slot_section_on(1));

		slot_name(S.slot_row, S.slot_slot, S.slot_rank, name, sizeof(name));
		ae_is_slot_count_line(picks, number, sizeof(number));
		snprintf(left, sizeof(left), "%s" DOT "%s", name, number);
		count_line(x, y, width, left, "");
		y += U(32.0f);
		has = 1;
		page_list(x, y, width);
		slot_card(card_x, top, card_width, height);
		break;
	}
	}
	return has;
}

static void draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &S.flow;
	struct ae_density const *d;
	float x, width, card_x, card_width, top, height, y, body, line;
	char left[96], right[64], upper[96], expanded[400], lines_text[2][160], number[32], size[32], item_upper[128];
	char const *parts[5], *help;
	int warning, count, each, playing = 0, has = 0;
	struct ae_is_set const *current;

	(void)instance;
	ae_flow_full_density(flow);
	d = &flow->density;
	x = ae_flow_left_x(flow);
	width = flow->frame.rect.width * AE_FLOW_ROWS_WIDTH;
	card_x = ae_frame_x(&flow->frame, AE_FLOW_PANEL_X);
	card_width = ae_flow_right_x(flow) - card_x;
	top = Y(252.0f);
	height = ae_flow_content_bottom(flow) - U(96.0f) - top;
	/* (a testing feature: the crumb starts with the mark) */
	ae_flow_title(flow, "MAPS & MODS", AE_TESTING_CRUMB "MAIN MENU");
	ae_screen_maps_hub_strip(flow, AE_HUB_TAB_ITEM_SETS);
	current = S.step >= AE_IS_STEP_CONTENTS ? set_named(S.folder) : NULL;
	if (S.step == AE_IS_STEP_SETS)
	{
		struct ae_is_set const *set = set_focused();

		playing = set ? set->playing : 0;
		has = S.set_count ? 1 : -1;
		if (home_row() == HOME_SWAPS)
			has = -2;
		else if (home_row() == HOME_MULTIPLAYER || home_row() == HOME_HEAD)
			has = -3;
		else if (!set)
			has = -1;
		if (S.set_count)
		{
			ae_hub_count_text((unsigned long long)S.set_count, number, sizeof(number));
			snprintf(left, sizeof(left), "YOUR SETS" DOT "%s", number);
		}
		else
			snprintf(left, sizeof(left), "YOUR SETS" DOT "NONE");
		count_line(x, y = top, width, left, S.set_count ? "ONE PLAYS AT A TIME" : "");
		y += U(32.0f);
		if (!S.set_count)
		{
			char wrapped[5][160];
			int lines = ae_wrap_text(AE_FONT_BODY, ae_size_text(d) * 0.9f, "Take weapons, vehicles, characters and more out of the maps on this PC, "
				"and play them in your campaign levels. What you take goes into a set; one set plays at a time.", width - U(20.0f), wrapped, 5);

			for (each = 0; each < lines; each++)
				text_shadow(AE_FONT_BODY, ae_size_text(d) * 0.9f, x + U(10.0f), y + U(8.0f) + U(31.0f) * (float)each, AE_ALIGN_LEFT, AE_COLOR_TEXT,
					wrapped[each]);
			y += U(8.0f) + U(31.0f) * (float)lines + U(20.0f);
		}
		page_list(x, y, width);
		sets_card(card_x, top, card_width, height);
	}
	else if (S.step >= AE_IS_STEP_SWAPS)
	{
		has = draw_swap_steps(x, width, card_x, card_width, top, height);
	}
	else
	{
		char kind_upper[48], file_upper[96];

		upper_copy(upper, sizeof(upper), S.set_name);
		upper_copy(kind_upper, sizeof(kind_upper), S.step == AE_IS_STEP_ITEMS ? ae_is_kind_name(S.kind) : "");
		upper_copy(file_upper, sizeof(file_upper), S.map_open >= 0 && S.map_open < S.map_count ? S.maps[S.map_open].file : "");
		parts[0] = "ITEM SETS";
		parts[1] = upper;
		count = 2;
		if (S.step == AE_IS_STEP_MAPS)
			parts[count++] = "ADD ITEMS";
		if (S.step >= AE_IS_STEP_KINDS && S.step <= AE_IS_STEP_ITEMS)
			parts[count++] = file_upper;
		if (S.step == AE_IS_STEP_SETTINGS)
		{
			struct ae_is_entry const *item_entry = settings_entry();
			char own[128];

			ae_is_item_name(S.set_path, own, sizeof(own));
			upper_copy(item_upper, sizeof(item_upper), item_entry && item_entry->name[0] ? item_entry->name : own);
			parts[count++] = item_upper;
		}
		if (S.step == AE_IS_STEP_ITEMS)
			parts[count++] = kind_upper;
		place_line(x, width, parts, count, S.step == AE_IS_STEP_CONTENTS && current && current->playing ? "PLAYING" : "");
		y = Y(290.0f);
		switch (S.step)
		{
		case AE_IS_STEP_CONTENTS:
		{
			char megabytes[32];

			ae_hub_size_text(current ? current->bytes : 0, megabytes, sizeof(megabytes));
			snprintf(left, sizeof(left), "%d ITEM%s" DOT "%s", S.entry_count, S.entry_count == 1 ? "" : "S", megabytes);
			count_line(x, y, width, left, "ON PLAYS" DOT "OFF STAYS IN THE SET");
			y += U(32.0f);
			has = entry_focused() != NULL;
			if (S.list.list.focus >= 0 && S.list.list.focus < S.row_count && S.rows[S.list.list.focus].type == ROW_RANGE)
				has = 2;
			page_list(x, y, width);
			contents_card(card_x, top, card_width, height);
			break;
		}
		case AE_IS_STEP_SETTINGS:
		{
			struct ae_is_entry const *item_entry = settings_entry();

			snprintf(left, sizeof(left), "ITS SETTINGS" DOT "%s", ae_is_kind_word(S.set_kind));
			snprintf(right, sizeof(right), "%s", item_entry && item_entry->failed[0] ? "TURNED OFF" : S.set_on ? "ON PLAYS" : "OFF STAYS IN THE SET");
			count_line(x, y, width, left, right);
			y += U(32.0f);
			has = ae_is_row_change(setting_focused(), S.set_kind == 4 && S.fit.known && !S.fit.fits);
			page_list(x, y, width);
			settings_card(card_x, top, card_width, height);
			break;
		}
		case AE_IS_STEP_MAPS:
		{
			int ready = S.staged || ae_hub_data_ready();

			y += search_field(x, y, width, "Search maps on this PC") + U(18.0f);
			y += page_row(x, y, width, HIT_CTL, CTL_SHOW, "SHOW", show_name(S.show), 0, S.ctl == CTL_SHOW ? AE_ROW_FOCUSED : 0) + U(10.0f);
			if (!ready)
			{
				count_line(x, y, width, "READING THE MAP FOLDERS...", "");
				card_frame(card_x, top, card_width, height, 0);
				break;
			}
			ae_hub_count_text((unsigned long long)S.view_count, number, sizeof(number));
			upper_copy(upper, sizeof(upper), show_name(S.show));
			snprintf(left, sizeof(left), "%s" DOT "%s MAPS", upper, number);
			count_line(x, y, width, left, "A TO Z");
			y += U(32.0f);
			if (S.view_count == 0)
			{
				char wrapped[3][160];
				int lines = ae_wrap_text(AE_FONT_BODY, ae_size_text(d) * 0.9f, S.search.text[0] ? "No map matches that search." :
					"No maps of this kind on this PC. INSTALLED lists every map this PC has.", width - U(20.0f), wrapped, 3);

				for (each = 0; each < lines; each++)
					ae_draw_text(AE_FONT_BODY, ae_size_text(d) * 0.9f, x + U(10.0f), y + U(8.0f) + U(31.0f) * (float)each, AE_ALIGN_LEFT,
						AE_COLOR_TEXT, wrapped[each]);
				card_frame(card_x, top, card_width, height, 0);
				break;
			}
			page_list(x, y, width);
			maps_card(card_x, top, card_width, height);
			break;
		}
		case AE_IS_STEP_KINDS:
		{
			long total = 0;

			for (each = 0; each < AE_IS_KINDS; each++)
				total += S.kind_counts[each];
			snprintf(left, sizeof(left), "WHAT IT HAS" DOT "%ld ITEMS", total);
			snprintf(right, sizeof(right), "%ld IN %s", map_items_in_set(), "THE SET");
			count_line(x, y, width, left, right);
			y += U(32.0f);
			page_list(x, y, width);
			kinds_card(card_x, top, card_width, height);
			break;
		}
		default:
		{
			struct ae_is_item const *item = item_focused();
			long in_set = 0;

			has = item && item->in_set;
			for (each = 0; each < S.item_count; each++)
				in_set += S.items[each].in_set != 0;
			if (S.typing || S.search.text[0])
				y += search_field(x, y, width, "Search this map's items") + U(14.0f);
			snprintf(left, sizeof(left), "%s" DOT "%d", ae_is_kind_name(S.kind), S.item_view_count);
			snprintf(right, sizeof(right), "%ld IN THE SET", in_set);
			count_line(x, y, width, left, right);
			y += U(32.0f);
			page_list(x, y, width);
			items_card(card_x, top, card_width, height);
			break;
		}
		}
	}
	(void)size;
	help = help_text(&warning);
	body = ae_flow_u(flow, d->metrics->body);
	line = ae_flow_u(flow, d->metrics->body_line);
	ae_draw_rect(x, ae_flow_content_bottom(flow) - line * 2.0f - U(14.0f), ae_flow_right_x(flow) - x, U(AE_FLOW_RULE_U), 0.0f, AE_COLOR_RULE);
	count = ae_wrap_text(AE_FONT_BODY, body, ae_help_expand(help, expanded, sizeof(expanded)), ae_flow_right_x(flow) - x, lines_text, 2);
	for (each = 0; each < count; each++)
		text_shadow(AE_FONT_BODY, body, x, ae_flow_content_bottom(flow) - line * 2.0f + line * (float)each, AE_ALIGN_LEFT,
			warning ? AE_COLOR_WARNING : AE_COLOR_TEXT, lines_text[each]);
	prompts_build(playing, has);
	ae_flow_draw_prompts(flow);
	if (S.overlay)
		overlay_draw();
}

/* ---------- the staged scenes */

static void entry_focus_named(char const *part)
{
	int each;

	for (each = 0; each < S.row_count; each++)
		if (S.rows[each].type == ROW_ENTRY && strstr(S.entries[S.rows[each].index].path, part))
		{
			ae_list_set_focus(&S.list.list, (short)each);
			return;
		}
}

static void item_focus_named(char const *part)
{
	int each;

	for (each = 0; each < S.item_view_count; each++)
		if (strstr(S.items[S.item_view[each]].path, part))
		{
			ae_list_set_focus(&S.list.list, (short)each);
			return;
		}
}

/* a set's page, as the scenes begin: the first set opened */
static void scene_to_map(char const *file, int open)
{
	int each;

	set_open(S.sets[0].folder, S.sets[0].name);
	maps_read();
	S.show = SHOW_CE;
	maps_view();
	step_to(AE_IS_STEP_MAPS, 0);
	for (each = 0; each < S.view_count; each++)
		if (!strcmp(S.maps[S.view[each]].file, file))
		{
			ae_list_set_focus(&S.list.list, (short)each);
			break;
		}
	if (open && S.view_count)
	{
		char reason[200];
		struct ae_is_map const *map = map_focused();

		if (map && S.be->map_open((int)(map - S.maps), reason, sizeof(reason)))
		{
			S.map_open = (int)(map - S.maps);
			S.map_row = S.list.list.focus;
			kinds_read();
			step_to(AE_IS_STEP_KINDS, 0);
		}
	}
}

static void scene_apply(void)
{
	sets_read();
	list_to(step_count(), 0);
	switch (S.scene)
	{
	case AE_IS_SCENE_SWITCHED:
		set_play_toggle(S.sets[1].folder, S.sets[1].name, 1);
		focus_set(S.sets[1].folder);
		break;
	case AE_IS_SCENE_OPTIONS:
		if (S.set_count)
			options_open(&S.sets[0]);
		break;
	case AE_IS_SCENE_DELETE:
		if (S.set_count)
			delete_set_ask(S.sets[0].folder, S.sets[0].name);
		break;
	case AE_IS_SCENE_NEW:
		S.name_mode = NAME_NEW;
		name_begin();
		break;
	case AE_IS_SCENE_PICK:
		scene_to_map("tsce_multiplayerv1", 0);
		break;
	case AE_IS_SCENE_CANT_OPEN:
		ae_is_staged_block_ce(1);
		scene_to_map("tsce_multiplayerv1", 0);
		break;
	case AE_IS_SCENE_KINDS:
		scene_to_map("tsce_multiplayerv1", 1);
		break;
	case AE_IS_SCENE_ITEMS:
	case AE_IS_SCENE_REFUSED:
		scene_to_map("tsce_multiplayerv1", 1);
		items_read(0);
		step_to(AE_IS_STEP_ITEMS, 0);
		item_focus_named(S.scene == AE_IS_SCENE_ITEMS ? "pistol_spirit_variant_mp" : "gun_turret_mounted");
		if (S.scene == AE_IS_SCENE_REFUSED && item_focused())
		{
			struct ae_is_item *item = item_focused();

			S.be->item_look(0, (int)item->index, item);
			S.be->item_add(S.folder, 0, (int)item->index, &S.result);
			S.result_index = item->index;
			S.result_valid = 1;
		}
		else if (item_focused())
			S.be->item_look(0, (int)item_focused()->index, item_focused());
		break;
	case AE_IS_SCENE_ADDING:
	case AE_IS_SCENE_ADDED:
		scene_to_map("tsce_multiplayerv1", 1);
		items_read(4);
		step_to(AE_IS_STEP_ITEMS, 0);
		item_focus_named("cyborg_mp");
		if (S.scene == AE_IS_SCENE_ADDING)
		{
			S.op = OP_ADD;
			S.op_row = S.list.list.focus;
			S.frozen = 1;
		}
		else
		{
			/* (the staged set has it: the scene removes it first so the add is seen) */
			if (item_focused() && item_focused()->in_set)
			{
				S.be->item_remove(S.folder, 4, (int)item_focused()->index);
				items_read(4);
				step_to(AE_IS_STEP_ITEMS, 0);
				item_focus_named("cyborg_mp");
			}
			S.op = OP_ADD;
			op_run();
		}
		break;
	case AE_IS_SCENE_CONTENTS:
	case AE_IS_SCENE_REMOVE:
	case AE_IS_SCENE_FAILED:
	case AE_IS_SCENE_BIPED:
		set_open(S.sets[0].folder, S.sets[0].name);
		entry_focus_named(S.scene == AE_IS_SCENE_FAILED ? "rocket_pod" : S.scene == AE_IS_SCENE_BIPED ? "cyborg_mp" : "pistol_spirit_variant_mp");
		if (S.scene == AE_IS_SCENE_REMOVE && entry_focused())
			remove_entry_ask(entry_focused());
		break;
	case AE_IS_SCENE_SETTINGS:
	case AE_IS_SCENE_BODY:
	case AE_IS_SCENE_BODY_CARD:
	case AE_IS_SCENE_BODY_NOFIT:
	{
		struct ae_is_set const *from = &S.sets[S.scene == AE_IS_SCENE_BODY_NOFIT ? 1 : 0];

		set_open(from->folder, from->name);
		entry_focus_named(S.scene == AE_IS_SCENE_SETTINGS ? "pistol_spirit_variant_mp" : S.scene == AE_IS_SCENE_BODY_NOFIT ? "elite_mp" : "cyborg_mp");
		if (!entry_focused())
			break;
		if (S.scene != AE_IS_SCENE_SETTINGS && S.scene != AE_IS_SCENE_BODY_NOFIT)
		{
			/* (the set's body is the cyborg: PLAY AS IT is on) */
			S.be->body_set_option(S.folder, "player_biped", entry_focused()->path);
			entries_read();
			entry_focus_named("cyborg_mp");
		}
		if (S.scene != AE_IS_SCENE_BODY_CARD)
			settings_open(entry_focused(), S.scene == AE_IS_SCENE_SETTINGS ? AE_IS_ROW_FACTION : AE_IS_ROW_PLAY_AS);
		break;
	}
	case AE_IS_SCENE_HOME:
		swaps_read();
		ae_list_set_focus(&S.list.list, (short)(S.set_count + HOME_SWAPS));
		break;
	case AE_IS_SCENE_SWAPS:
	case AE_IS_SCENE_SWAPS_EMPTY:
	case AE_IS_SCENE_SWAP_DELETE:
	case AE_IS_SCENE_SWAP_NEW:
		swaps_read();
		if (S.scene == AE_IS_SCENE_SWAPS_EMPTY)
		{
			int each;

			for (each = S.swap_count - 1; each >= 0; each--)
				S.be->swap_remove(S.swaps[each].file);
		}
		swaps_open_list(S.scene == AE_IS_SCENE_SWAPS_EMPTY ? 0 : 0);
		if (S.scene == AE_IS_SCENE_SWAP_DELETE && S.swap_count)
			swap_delete_ask(&S.swaps[0]);
		else if (S.scene == AE_IS_SCENE_SWAP_NEW)
		{
			snprintf(S.pending_swap, sizeof(S.pending_swap), "Test Swap");
			setpick_begin();
		}
		break;
	case AE_IS_SCENE_SWAP_WEAPONS:
	case AE_IS_SCENE_SWAP_ENEMIES:
	case AE_IS_SCENE_SWAP_RANKS:
	case AE_IS_SCENE_SWAP_SLOT:
	{
		int each, row = S.scene == AE_IS_SCENE_SWAP_ENEMIES ? AE_IS_SW_BIPED : AE_IS_SW_WEAPON_SLOT, slot = S.scene == AE_IS_SCENE_SWAP_ENEMIES ? 0 : 1;

		swaps_read();
		if (S.scene != AE_IS_SCENE_SWAP_WEAPONS)
		{
			/* (the enemies picked by enemy, so their rows are there to see) */
			S.be->swap_set_option(S.swaps[0].file, "enemy_replace", "manual");
			S.be->swap_set_option(S.swaps[0].file, "enemy_rotation", "add");
			S.be->swap_list_set(S.swaps[0].file, "enemies", "elite_major", "cmt\\characters\\covenant\\elite\\elite_zealot", 1);
			S.be->swap_list_set(S.swaps[0].file, "enemies", "elite_major", "original", 1);
			S.be->swap_list_set(S.swaps[0].file, "loadout", "elite_major", "stock:needler", 1);
		}
		swap_open(S.swaps[0].file);
		for (each = 0; each < S.swline_count; each++)
			if (!S.swlines[each].header && S.swlines[each].row == row && (row == AE_IS_SW_BIPED || S.swlines[each].slot == slot))
				ae_list_set_focus(&S.list.list, (short)each);
		if (S.scene == AE_IS_SCENE_SWAP_RANKS)
			ranks_open(1);
		else if (S.scene == AE_IS_SCENE_SWAP_SLOT)
			slot_open(AE_IS_SW_ENEMY_SLOT, 1, 2);
		break;
	}
	case AE_IS_SCENE_RANGE:
	case AE_IS_SCENE_RANGE_WAIT:
		set_open(S.sets[S.scene == AE_IS_SCENE_RANGE_WAIT ? 3 : 0].folder, S.sets[S.scene == AE_IS_SCENE_RANGE_WAIT ? 3 : 0].name);
		ae_list_set_focus(&S.list.list, 1);
		break;
	case AE_IS_SCENE_PICKER:
	case AE_IS_SCENE_PICKER_WAITS:
		ae_screen_item_picker_demo(S.be);
		ae_screen_item_picker_open(1, S.scene == AE_IS_SCENE_PICKER_WAITS, 0);
		break;
	default:
		break;
	}
}

/* the real scenes: a fresh set, the first map of Custom Edition this PC lists, its kinds, a kind's items, and for the last one an item added.
Each says what it did in the log, which the test's expect lines read. 1 when done (or given up), 0 to try again next frame */
static int real_apply(void)
{
	static int const order[4] = { 0, 1, 3, 4 };
	char name[48], folder[64], reason[200];
	int each, pick = -1, kind = -1;

	ae_hub_data_update();
	if (!ae_hub_data_ready())
		return 0;
	snprintf(name, sizeof(name), "Preflight %06lu", (unsigned long)time(NULL) % 1000000ul);
	if (!S.be->create(name, folder, sizeof(folder)))
	{
		platform_log("item sets: real scene: the set %s could not be made", name);
		return 1;
	}
	sets_read();
	set_open(folder, name);
	maps_read();
	S.show = SHOW_CE;
	maps_view();
	for (each = 0; each < S.view_count && pick < 0; each++)
		if (!S.be->maps_reason(S.maps[S.view[each]].source))
			pick = each;
	if (pick < 0)
	{
		platform_log("item sets: real scene: no Custom Edition map to open (%d listed)", S.view_count);
		step_to(AE_IS_STEP_MAPS, 0);
		return 1;
	}
	step_to(AE_IS_STEP_MAPS, pick);
	if (!S.be->map_open(S.view[pick], reason, sizeof(reason)))
	{
		platform_log("item sets: real scene: %s did not open: %s", S.maps[S.view[pick]].file, reason);
		return 1;
	}
	S.map_open = S.view[pick];
	S.map_row = pick;
	kinds_read();
	for (each = 0; each < 4 && kind < 0; each++)
		if (S.kind_counts[order[each]] > 0)
			kind = order[each];
	platform_log("item sets: real scene: %s open, %ld weapons, %ld AI, %ld vehicles, %ld bipeds", S.maps[S.map_open].file, S.kind_counts[0],
		S.kind_counts[1], S.kind_counts[3], S.kind_counts[4]);
	step_to(AE_IS_STEP_KINDS, 0);
	if (S.scene == AE_IS_SCENE_REAL_KINDS || kind < 0)
		return 1;
	items_read(kind);
	step_to(AE_IS_STEP_ITEMS, 0);
	platform_log("item sets: real scene: %s, %d items listed", ae_is_kind_name(kind), S.item_count);
	if (S.scene == AE_IS_SCENE_REAL_ITEMS)
	{
		if (item_focused())
			S.be->item_look(kind, (int)item_focused()->index, item_focused());
		return 1;
	}
	for (each = 0; each < S.item_view_count && each < 30; each++)
	{
		struct ae_is_item *item = &S.items[S.item_view[each]];

		S.be->item_look(kind, (int)item->index, item);
		if (!item->refused && !item->in_set)
		{
			ae_list_set_focus(&S.list.list, (short)each);
			S.op = OP_ADD;
			op_run();
			return 1;
		}
	}
	platform_log("item sets: real scene: no item of %s could be added", ae_is_kind_name(kind));
	return 1;
}

/* ---------- once a frame */

static void update(struct ae_screen *instance)
{
	static char previous[64];
	(void)instance;

	if (S.typing && S.edit.active && !ae_field_edit_keys(&S.edit))
		ae_sound_request(AE_SOUND_FAILURE, 0);
	if (S.naming && S.name_edit.active && !ae_field_edit_keys(&S.name_edit))
		ae_sound_request(AE_SOUND_FAILURE, 0);
	/* (the search narrowed as it is typed) */
	if (strcmp(previous, S.search.text))
	{
		snprintf(previous, sizeof(previous), "%s", S.search.text);
		if (S.typing || S.keyboard)
			search_changed();
	}
	if (S.first_frame && S.flow.density.metrics)
	{
		S.first_frame = 0;
		if (S.staged)
			scene_apply();
	}
	if (S.real_pending && S.flow.density.metrics && real_apply())
		S.real_pending = 0;
	if (S.step == AE_IS_STEP_SETTINGS && S.replace_state == 1 && S.flow.density.metrics)
	{
		if (S.replace_wait > 0)
			S.replace_wait--;
		else
		{
			stock_asked = 1;
			S.replace_state = 2;
			settings_reload(setting_focused());
			platform_log("item sets: the campaign's items were read for %s", S.set_path);
		}
	}
	if (S.op != OP_NONE)
	{
		if (S.op_wait > 0)
			S.op_wait--;
		else
			op_run();
		return;
	}
	if (!S.staged && S.step == AE_IS_STEP_MAPS)
	{
		ae_hub_data_update();
		if (ae_hub_data_ready() && ae_hub_data_generation() != S.maps_generation)
		{
			maps_read();
			list_to(S.view_count, 0);
		}
	}
	/* (an item's parts counted when its row gets the focus) */
	if (S.step == AE_IS_STEP_ITEMS && !S.frozen && !S.overlay)
	{
		struct ae_is_item *item = item_focused();

		if (item && item->parts < 0 && S.be->kind_addable(S.kind))
			S.be->item_look(S.kind, (int)item->index, item);
	}
}

static void leave(struct ae_screen *instance)
{
	(void)instance;
	if (S.typing)
		ae_field_edit_end(&S.edit, 0);
	S.typing = 0;
}

static struct ae_screen_class const sets_class =
{
	.name = "item_sets", .leave = leave, .handle = handle, .draw = draw, .pointer = pointer, .update = update,
};

/* ---------- opening */

static void init(int staged, int scene)
{
	memset(&S, 0, sizeof(S));
	S.staged = staged;
	S.scene = scene;
	S.be = staged ? ae_is_backend_staged(scene == AE_IS_SCENE_EMPTY) : ae_is_backend_engine();
	S.flow.hit_rows = HIT_LIST;
	S.flow.hit_prompts = HIT_PROMPTS;
	ae_text_init(&S.search, "", 48, 0);
	S.ctl = -1;
	S.map_open = -1;
	S.show = SHOW_ALL;
	S.first_frame = 1;
}

int ae_screen_item_sets_open(int staged, int scene)
{
	int real = staged && scene >= AE_IS_SCENE_REAL_KINDS && scene <= AE_IS_SCENE_REAL_ADD;

	if (staged || S.staged || !S.started)
	{
		init(staged && !real, scene);
		S.real_pending = real;
		sets_read();
		step_to(AE_IS_STEP_SETS, 0);
		S.started = !staged || real;
	}
	else
	{
		/* (the real page comes back to the step it was left on; a set that is gone, or the map closed from elsewhere, is the sets) */
		sets_read();
		if (S.step >= AE_IS_STEP_SWAPS)
		{
			/* (the swaps' steps come back as their list) */
			swaps_read();
			S.step = AE_IS_STEP_SWAPS;
			list_to(step_count(), 0);
		}
		if (S.step == AE_IS_STEP_SETTINGS)
		{
			/* (an item's page is left as its set's items) */
			entries_read();
			step_to(AE_IS_STEP_CONTENTS, 0);
		}
		if (S.step >= AE_IS_STEP_CONTENTS && S.step < AE_IS_STEP_SWAPS && !set_named(S.folder))
			step_to(AE_IS_STEP_SETS, 0);
		else if (S.step >= AE_IS_STEP_KINDS && S.step < AE_IS_STEP_SWAPS && S.be->kind_count(0) + S.be->kind_count(1) + S.be->kind_count(2) < 0)
			step_to(AE_IS_STEP_MAPS, 0);
		else
		{
			short focus = S.list.list.focus;

			if (S.step == AE_IS_STEP_CONTENTS)
				entries_read();
			list_to(step_count(), focus);
		}
		S.first_frame = 0;
		S.overlay = OVERLAY_NONE;
		S.op = OP_NONE;
	}
	S.from_picker = 0;
	if (!ae_ui_push(&sets_class, AE_OWNER_ANY, &S))
		return 0;
	platform_log("ae item sets: open%s, step %d", staged ? " (staged)" : "", S.step);
	return 1;
}

void ae_screen_item_sets_close(void)
{
	if (ae_ui_depth() && ae_ui_top() && ae_ui_top()->screen_class == &sets_class)
		ae_ui_pop();
}

/* the page opened from the lobby's picker on a swap (kind 0, by its file) or a set (kind 1, by its folder); B comes back to the picker */
int ae_screen_item_sets_open_from(int kind, char const *id)
{
	int each, focus = 0;

	if (!ae_screen_item_sets_open(0, 0))
		return 0;
	S.from_picker = 1;
	sets_read();
	swaps_read();
	S.search.text[0] = 0;
	if (kind == 0)
	{
		for (each = 0; each < S.swap_count; each++)
			if (same_text(S.swaps[each].file, id))
				focus = each;
		S.step = AE_IS_STEP_SWAPS;
		S.ctl = -1;
		list_to(step_count(), focus);
		return 1;
	}
	for (each = 0; each < S.set_count; each++)
		if (same_text(S.sets[each].folder, id))
			focus = each;
	step_to(AE_IS_STEP_SETS, focus);
	return 1;
}
