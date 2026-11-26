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
#include "ae_maps_hub_data.h"
#include "ae_screen_item_sets.h"
#include "ae_screen_maps_hub.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

#define DOT " \xE2\x80\xA2 "
#define CHEVRON "\xE2\x80\xBA"

enum { HIT = 0x7D00, HIT_LIST, HIT_CTL, HIT_PROMPTS, HIT_FIELD, HIT_POP };
enum { OVERLAY_NONE, OVERLAY_OPTIONS, OVERLAY_NAME };
enum { NAME_NEW, NAME_RENAME, NAME_COPY };
enum { OP_NONE, OP_OPEN_MAP, OP_ADD };
enum { CTL_SEARCH, CTL_SHOW };
enum { SHOW_ALL, SHOW_CE, SHOW_MCC, SHOW_WORKSHOP, SHOWS };
enum { ROW_ADD, ROW_HEADER, ROW_ENTRY };
enum { OPTION_OPEN, OPTION_PLAY, OPTION_RENAME, OPTION_COPY, OPTION_DELETE, OPTIONS };

struct content_row { int type, index; };

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
	struct content_row rows[AE_IS_ENTRIES_MOST + AE_IS_KINDS + 1];
	int row_count;
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
} S;

static struct ae_screen_class const sets_class;

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
	memset(count_of, 0, sizeof(count_of));
	for (each = 0; each < S.entry_count; each++)
		count_of[S.entries[each].kind]++;
	for (kind = 0; kind < AE_IS_KINDS; kind++)
	{
		if (!count_of[kind])
			continue;
		S.rows[S.row_count].type = ROW_HEADER;
		S.rows[S.row_count++].index = kind;
		for (each = 0; each < S.entry_count; each++)
			if (S.entries[each].kind == kind)
			{
				S.rows[S.row_count].type = ROW_ENTRY;
				S.rows[S.row_count++].index = each;
			}
	}
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
	case AE_IS_STEP_SETS: return S.set_count + 1;
	case AE_IS_STEP_CONTENTS: return S.row_count;
	case AE_IS_STEP_MAPS: return S.view_count;
	case AE_IS_STEP_KINDS: return AE_IS_KINDS;
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
			(flags & AE_ROW_FOCUSED) ? (kind == 3 ? AE_COLOR_WARNING : AE_COLOR_SELECTION_TEXT) :
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
			page_row(x, y, width, HIT_LIST, item, "NEW SET...", NULL, 0, flags | AE_ROW_SUBSCREEN);
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
			page_row(x, y, width, HIT_LIST, item, "ADD ITEMS FROM A MAP", NULL, 0, flags | AE_ROW_SUBSCREEN);
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
			char name[128];

			ae_is_item_name(entry->path, name, sizeof(name));
			snprintf(sub, sizeof(sub), "from %s", entry->map);
			if (entry->failed[0])
			{
				snprintf(value, sizeof(value), "FAILED");
				tone = 3;
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

static void contents_card(float x, float y, float width, float height)
{
	struct ae_is_entry const *entry = entry_focused();
	char name[128], line[200], text[200], size[32];
	float cy;

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
	if (entry->bytes)
	{
		ae_hub_size_text(entry->bytes, size, sizeof(size));
		cy += card_fact(x, cy, width, "IN THE SET", size, AE_COLOR_TITLE);
	}
	cy += card_fact(x, cy, width, "WORKS", entry->works ? "It has every part it needs" : "A part it needs is missing",
		entry->works ? AE_COLOR_TITLE : AE_COLOR_WARNING);
	cy += U(10.0f);
	if (!entry->failed[0])
	{
		cy += card_path(x, cy, width, entry->path);
		card_body(x, cy, width, "Changes play from the next level you load.", 2, AE_COLOR_MUTED);
	}
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
		if (set)
		{
			snprintf(text, sizeof(text), set->playing ? "%s plays in your levels. {A} opens its options; {Y} stops it; {X} makes a new set." :
				"%s is off. {A} opens its options; {Y} plays it; {X} makes a new set.", set->name);
			return text;
		}
		return S.set_count ? "NEW SET asks for its name, then opens it so you can add items from a map." :
			"NEW SET asks for its name, then opens it so you can add items from a map.";
	case AE_IS_STEP_CONTENTS:
		if (entry)
		{
			ae_is_item_name(entry->path, name, sizeof(name));
			if (entry->failed[0])
			{
				*warning = 1;
				snprintf(text, sizeof(text), "%s was turned off: the last level couldn't play it. {A} tries it again; {Y} removes it.", name);
			}
			else
				snprintf(text, sizeof(text), "{A} turns %s on or off; {Y} removes it from the set. Changes play from the next level.", name);
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
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, S.name_mode == NAME_NEW ? "Make it" : "Done", AE_ACTION_ACCEPT);
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, "Cancel", AE_ACTION_BACK);
		return;
	}
	if (S.overlay == OVERLAY_OPTIONS)
	{
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Select", AE_ACTION_ACCEPT);
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
		snprintf(title, sizeof(title), S.name_mode == NAME_NEW ? "New set" : S.name_mode == NAME_RENAME ? "Rename %s" : "Copy of %s",
			S.option_name);
		ae_draw_text(AE_FONT_ROW, U(30.0f), x + U(24.0f), y + U(26.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE, title);
		memset(&field, 0, sizeof(field));
		field.label = "SET NAME";
		field.placeholder = "Letters, numbers, spaces";
		field.text = &S.name_text;
		field.flags = AE_FIELD_FOCUSED | (S.naming || S.naming_keyboard ? AE_FIELD_EDITING : 0);
		field.caret_since = S.caret_since;
		ae_field_well(d, x + U(24.0f), y + U(76.0f), width - U(48.0f), &field, &S.name_well);
		ae_widget_field(d, x + U(24.0f), y + U(76.0f), width - U(48.0f), &field, HIT_POP);
		ae_draw_text(AE_FONT_BODY, ae_size_minor(d) * 0.9f, x + width - U(24.0f), y + height - U(34.0f), AE_ALIGN_RIGHT, AE_COLOR_MUTED,
			S.name_mode == NAME_NEW ? "A makes it. B cancels." : "A keeps the name. B cancels.");
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
enum { ASK_DELETE_SET, ASK_REMOVE_ENTRY, ASK_REMOVE_ITEM };

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
		entries_read();
		list_to(S.row_count, S.list.list.focus < S.row_count ? S.list.list.focus : S.row_count - 1);
		sets_read();
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
	spec.choices[1] = what == ASK_DELETE_SET ? "DELETE" : "REMOVE";
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

static void name_begin(void);

static void name_commit(void)
{
	char text[64], folder[64];
	int done = 0;

	snprintf(text, sizeof(text), "%s", S.name_text.text);
	if (S.naming)
		ae_field_edit_end(&S.name_edit, 1);
	S.naming = 0;
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
		ae_text_init(&S.name_text, S.name_mode == NAME_RENAME ? S.option_name : S.name_mode == NAME_COPY ? S.option_name :
			S.staged && S.scene == AE_IS_SCENE_NEW ? "Test Kit" : "", 40, 0);
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

/* ---------- accept */

static void accept(void)
{
	switch (S.step)
	{
	case AE_IS_STEP_SETS:
	{
		struct ae_is_set const *set = set_focused();

		if (!set)
		{
			S.name_mode = NAME_NEW;
			snprintf(S.option_name, sizeof(S.option_name), "%s", "");
			name_begin();
		}
		else
			options_open(set);
		break;
	}
	case AE_IS_STEP_CONTENTS:
	{
		struct ae_is_entry *entry = entry_focused();

		if (S.list.list.focus >= 0 && S.list.list.focus < S.row_count && S.rows[S.list.list.focus].type == ROW_ADD)
		{
			maps_read();
			step_to(AE_IS_STEP_MAPS, 0);
			ae_sound_request(AE_SOUND_FORWARD, 0);
		}
		else if (entry)
		{
			short focus = S.list.list.focus;

			S.be->entry_enable(S.folder, entry->tag, !entry->on || entry->failed[0]);
			entries_read();
			sets_read();
			list_to(S.row_count, focus < S.row_count ? focus : 0);
			ae_sound_request(AE_SOUND_FORWARD, 0);
			notice_set(0, 4000, "Changes play from the next level you load.");
		}
		break;
	}
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
		/* (B leaves MAPS & MODS: this page, then the hub under it) */
		ae_ui_pop();
		ae_ui_pop();
		return;
	case AE_IS_STEP_CONTENTS:
		sets_read();
		step_to(AE_IS_STEP_SETS, 0);
		focus_set(S.folder);
		break;
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
			S.name_mode = NAME_NEW;
			S.option_name[0] = 0;
			name_begin();
		}
		else if (S.step == AE_IS_STEP_CONTENTS)
		{
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
		else if (S.step == AE_IS_STEP_CONTENTS)
		{
			struct ae_is_entry const *entry = entry_focused();

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
			if (hit.id == HIT_POP && hit.part != AE_PART_CARD)
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
	item = ae_list_view_pointer(&S.list, pointer_event, HIT_LIST);
	if (item >= 0 && item < S.list_count)
	{
		if (S.step == AE_IS_STEP_CONTENTS && S.rows[item].type == ROW_HEADER)
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

static void draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &S.flow;
	struct ae_density const *d;
	float x, width, card_x, card_width, top, height, y, body, line;
	char left[96], right[64], upper[96], expanded[400], lines_text[2][160], number[32], size[32];
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
	ae_flow_title(flow, "MAPS & MODS", "MAIN MENU");
	if (ae_dev_mode_on())
		ae_screen_maps_hub_strip(flow, AE_HUB_TAB_ITEM_SETS);
	current = S.step >= AE_IS_STEP_CONTENTS ? set_named(S.folder) : NULL;
	if (S.step == AE_IS_STEP_SETS)
	{
		struct ae_is_set const *set = set_focused();

		playing = set ? set->playing : 0;
		has = S.set_count ? 1 : -1;
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
		if (S.step >= AE_IS_STEP_KINDS)
			parts[count++] = file_upper;
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
			page_list(x, y, width);
			contents_card(card_x, top, card_width, height);
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

	if (!ae_dev_mode_on())
	{
		ae_ui_pop();
		ae_screen_maps_hub_step(1);
		return;
	}
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
	int real = staged && scene >= AE_IS_SCENE_REAL_KINDS;

	if (!ae_dev_mode_on())
		return 0;
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
		if (S.step >= AE_IS_STEP_CONTENTS && !set_named(S.folder))
			step_to(AE_IS_STEP_SETS, 0);
		else if (S.step >= AE_IS_STEP_KINDS && S.be->kind_count(0) + S.be->kind_count(1) + S.be->kind_count(2) < 0)
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
