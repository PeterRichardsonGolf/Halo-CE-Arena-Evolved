/*
AE_PICKER_COMMON.C

What CHANGE MAP and CHANGE GAMETYPE share. See ae_picker_common.h.
*/

#include <stdio.h>
#include <string.h>

#include "../src/ae_draw.h"
#include "ae_glue_picker_stores.h"
#include "ae_picker_common.h"
#include "ae_sound.h"
#include "ae_ui.h"

void platform_log(char const *format, ...);

/* the search field (sizes u) */
#define SEARCH_HEIGHT_U 42.0f
#define SEARCH_PAD_U 20.0f
#define SEARCH_GLYPH_U 24.0f
#define SEARCH_WORD_GAP_U 10.0f
#define SEARCH_MINIMUM_U 320.0f
#define SEARCH_CHIPS_GAP_U 32.0f
#define SEARCH_RB_GAP_U 16.0f
#define RING_U 2.0f
#define CARET_U 2.0f
#define CARET_MS 530u
/* the folder menu (the value picker's look, ae_widgets_pick.c) */
#define MENU_TEXT_U 22.0f
#define MENU_EXTRA_U 76.0f
#define MENU_MINIMUM_U 240.0f
#define MENU_NAMING_U 400.0f
#define MENU_ITEM_PER_ROW (42.0f / 50.0f)
#define MENU_VISIBLE 8
#define MENU_TEXT_X_U 48.0f
#define MENU_CHECK_X_U 24.0f
#define MENU_CHECK_U 6.0f
#define MENU_CHECK_STROKE_U 3.0f
#define MENU_TITLE_U 32.0f
#define MENU_TITLE_TEXT_U 13.0f
#define MENU_QUESTION_U 40.0f
#define MENU_QUESTION_TEXT_U 17.0f
#define MENU_TITLE_X_U 18.0f
#define MENU_TITLE_SIDES_U 36.0f
#define NOTCH_U 6.0f
#define TRACK_U 6.0f
/* (a row's words: its middle 60 %) */
#define WORDS_SHARE 0.3f
/* the name field in the menu: a 50 u slot, its well inset 8 u at the sides and 4 u at the top; the help line 10 u
under it */
#define FIELD_SLOT_U 50.0f
#define FIELD_INSET_U 8.0f
#define FIELD_TOP_U 4.0f
#define FIELD_HEIGHT_U 42.0f
#define FIELD_PAD_U 20.0f
#define FIELD_SELECT 0.6f
#define HELP_GAP_U 10.0f
#define HELP_TEXT_U 17.0f
#define HELP_BAND_U 38.0f
#define EMPTY_TEXT_U 21.0f
#define EMPTY_GLYPH_U 24.0f
/* (the menu's hits: past the pickers' and the keyboards' ids, ae_widgets_pick.c / ae_widgets_text.c) */
enum { MENU_HIT = 0x7E21 };
enum { MENU_ITEMS = AE_PF_FOLDERS + 3, MENU_LABEL = 64 };

static float units(struct ae_density const *density, float spec_units)
{
	return spec_units * density->unit;
}

static float text_top(int font, float size, float center_y)
{
	return center_y - ae_draw_cap_height(font, size) * 0.5f;
}

static void upper_copy(char *out, int size, const char *text)
{
	int index;

	for (index = 0; index < size - 1 && text && text[index]; index++)
		out[index] = text[index] >= 'a' && text[index] <= 'z' ? (char)(text[index] - 32) : text[index];
	out[index] = 0;
}

/* ---------- the search field */

static void search_typing_done(int keep, void *context)
{
	struct ae_picker_search *search = context;

	(void)keep;
	search->typing = 0;
}

static void search_keyboard_done(int keep, void *context)
{
	struct ae_picker_search *search = context;

	(void)keep;
	search->keyboard_open = 0;
	/* (a physical key closed it into the field's own typing: that goes on) */
	if (search->edit.active && search->edit.text == &search->text)
	{
		search->typing = 1;
		search->caret_since = ae_motion_now();
	}
}

void ae_picker_search_init(struct ae_picker_search *search, const char *log, const char *initial)
{
	memset(search, 0, sizeof(*search));
	search->log = log;
	ae_text_init(&search->text, initial ? initial : "", AE_PICKER_QUERY - 1, 0);
}

void ae_picker_search_begin(struct ae_picker_search *search, int device, struct ae_density const *density,
	struct ae_rect const *bounds, void const *holder)
{
	search->edit.done = search_typing_done;
	search->edit.context = search;
	search->edit.holder = holder;
	if (device == AE_DEVICE_KEYBOARD_MOUSE)
	{
		ae_field_edit_begin(&search->edit, &search->text);
		ae_text_end(&search->text, 0);
		search->typing = 1;
		search->caret_since = ae_motion_now();
		platform_log("%s: typing", search->log);
		ae_sound_request(AE_SOUND_FORWARD, 0);
		return;
	}
	{
		struct ae_keyboard_spec spec;

		memset(&spec, 0, sizeof(spec));
		spec.text = &search->text;
		spec.field = search->well;
		spec.bounds = *bounds;
		spec.density = *density;
		spec.done = search_keyboard_done;
		spec.context = search;
		spec.edit = &search->edit;
		if (ae_keyboard_open(&spec, AE_OWNER_ANY))
		{
			search->keyboard_open = 1;
			platform_log("%s: keyboard open", search->log);
		}
	}
}

void ae_picker_search_clear(struct ae_picker_search *search)
{
	ae_text_init(&search->text, "", AE_PICKER_QUERY - 1, 0);
}

void ae_picker_search_end(struct ae_picker_search *search, int keep)
{
	if (!search->typing)
		return;
	search->typing = 0;
	ae_field_edit_end(&search->edit, 1);
	if (!keep)
		ae_picker_search_clear(search);
	ae_sound_request(keep ? AE_SOUND_FORWARD : AE_SOUND_BACK, 0);
}

int ae_picker_search_keys(struct ae_picker_search *search)
{
	return !search->typing || !search->edit.active || ae_field_edit_keys(&search->edit);
}

void ae_picker_search_leave(struct ae_picker_search *search)
{
	if (search->typing)
	{
		search->typing = 0;
		ae_field_edit_end(&search->edit, 0);
	}
}

float ae_picker_search_chip_room(struct ae_density const *density, float chips_x, float right, float rb_width)
{
	return right - units(density, SEARCH_MINIMUM_U) - units(density, SEARCH_CHIPS_GAP_U) - rb_width -
		units(density, SEARCH_RB_GAP_U) - chips_x;
}

float ae_picker_search_left(struct ae_density const *density, float first_line_end, float panel_x)
{
	float left = first_line_end + units(density, SEARCH_CHIPS_GAP_U);

	return left > panel_x ? left : panel_x;
}

void ae_picker_search_draw(struct ae_picker_search *search, struct ae_density const *density, float x, float right,
	float center_y, short hit_id)
{
	float width = right - x, height = units(density, SEARCH_HEIGHT_U), top = center_y - height * 0.5f;
	float pad = units(density, SEARCH_PAD_U), minor = ae_size_minor(density), glyph = units(density, SEARCH_GLYPH_U);
	float corner = units(density, density->metrics->corner);
	int device = ae_ui_last_device(), editing = ae_picker_search_editing(search);
	char clears[32];

	/* (the back button's word for the device, upper case: ESC, B, CIRCLE) */
	snprintf(clears, sizeof(clears), "%s CLEARS", ae_help_button_word(device, AE_BUTTON_B));
	upper_copy(clears, sizeof(clears), clears);
	search->well.x = x;
	search->well.y = top;
	search->well.width = width;
	search->well.height = height;
	ae_draw_rect(x, top, width, height, corner, AE_COLOR_WELL);
	if (editing)
		ae_draw_outline(x, top, width, height, corner, units(density, RING_U), AE_COLOR_RING);
	if (!editing && !search->text.length)
	{
		/* (idle: the Y button, or R on a keyboard, and the placeholder) */
		float mark = ae_widget_button_mark(density, AE_BUTTON_Y, x + pad, center_y, glyph, 0, 1);

		ae_draw_text_tracked(AE_FONT_BODY, minor, 0.09f, x + pad + mark + units(density, SEARCH_WORD_GAP_U),
			text_top(AE_FONT_BODY, minor, center_y), AE_ALIGN_LEFT, AE_COLOR_MUTED, "SEARCH");
	}
	else
	{
		float clears_width = ae_draw_text_tracked_width(AE_FONT_BODY, minor * 0.85f, 0.09f, clears);
		float room = width - 3.0f * pad - clears_width, text = ae_size_text(density), drawn;
		char shown[AE_TEXT_MAXIMUM + 8];

		ae_fit_text(AE_FONT_BODY, text, search->text.text, room, shown, sizeof(shown));
		drawn = ae_draw_text(AE_FONT_BODY, text, x + pad, text_top(AE_FONT_BODY, text, center_y), AE_ALIGN_LEFT,
			AE_COLOR_TITLE, shown);
		/* (the caret: 2 u in the accent, blinking as the fields' own) */
		if (editing && ((ae_motion_now() - search->caret_since) / CARET_MS) % 2u == 0u)
			ae_draw_rect(x + pad + drawn + units(density, 2.0f), center_y - ae_draw_cap_height(AE_FONT_BODY, text) * 0.75f,
				units(density, CARET_U), ae_draw_cap_height(AE_FONT_BODY, text) * 1.5f, 0.0f, AE_COLOR_ACCENT);
		ae_draw_text_tracked(AE_FONT_BODY, minor * 0.85f, 0.09f, x + width - pad,
			text_top(AE_FONT_BODY, minor * 0.85f, center_y), AE_ALIGN_RIGHT, AE_COLOR_MUTED, clears);
	}
	ae_hit_add(x, top, width, height, hit_id, AE_PART_FIELD, 0);
}

/* ---------- the folder menu */

enum { MODE_FOLDERS, MODE_ACTIONS, MODE_CONFIRM };
enum { ITEM_FOLDER, ITEM_NEW, ITEM_REMOVE, ITEM_RENAME, ITEM_DELETE, ITEM_KEEP, ITEM_DELETE_SURE };

static struct
{
	int open;
	struct ae_folder_menu_spec spec;
	char item[MENU_LABEL];
	int mode;
	short count;
	short kinds[MENU_ITEMS], folders[MENU_ITEMS];
	char labels[MENU_ITEMS][MENU_LABEL];
	unsigned char ticked[MENU_ITEMS];
	struct ae_list list;
	/* layout units: the items' box, its title band over it, the field's slot and help band under the items */
	struct ae_rect popover;
	float band, item_height, extra;
	/* the name field (NEW FOLDER... or RENAME in place): its item, its text, typed or AE's keyboard; why it was refused
	(AE_PF_NAME_*, 0 none) */
	int naming;
	short naming_item;
	struct ae_text name;
	struct ae_field_edit edit;
	int typing, keyboard_open, refused, live;
	char refused_name[AE_TEXT_MAXIMUM];
	unsigned long caret_since;
	struct ae_rect well;
} menu;

static struct ae_screen_class const menu_class;

static const char *refusal(int why)
{
	switch (why)
	{
	case AE_PF_NAME_TAKEN: return "You have a folder called that.";
	case AE_PF_NAME_EMPTY: return "Give it a name.";
	case AE_PF_NAME_CHARACTERS: return "Letters, numbers, spaces, - _ and ' only.";
	case AE_PF_NAME_LONG: return "Up to 32 letters, numbers and spaces.";
	case -1: return "You have 64 folders: delete one first.";
	}
	return "Up to 32 letters, numbers and spaces.";
}

static const char *refusal_word(int why)
{
	switch (why)
	{
	case AE_PF_NAME_TAKEN: return "taken";
	case AE_PF_NAME_EMPTY: return "empty";
	case AE_PF_NAME_CHARACTERS: return "characters";
	case AE_PF_NAME_LONG: return "long";
	}
	return "full";
}

static void add_item(int kind, short folder, const char *label, int ticked)
{
	if (menu.count >= MENU_ITEMS)
		return;
	menu.kinds[menu.count] = (short)kind;
	menu.folders[menu.count] = folder;
	snprintf(menu.labels[menu.count], MENU_LABEL, "%s", label);
	menu.ticked[menu.count] = (unsigned char)(ticked != 0);
	menu.count++;
}

/* the items for the mode (the folders' ticks as they are now) */
static void items_make(void)
{
	struct ae_picker_folders const *store = ae_picker_store(menu.spec.kind);
	short folder;
	char label[MENU_LABEL];

	menu.count = 0;
	switch (menu.mode)
	{
	case MODE_FOLDERS:
		for (folder = 0; folder < store->folder_count; folder++)
		{
			if (folder == AE_PF_FAVOURITES)
				snprintf(label, sizeof(label), "FAVOURITES");
			else
				snprintf(label, sizeof(label), "%s", store->names[folder]);
			add_item(ITEM_FOLDER, folder, label, menu.spec.in && menu.spec.in(folder, menu.spec.context));
		}
		add_item(ITEM_NEW, -1, "NEW FOLDER...", 0);
		if (menu.spec.folder >= 0 && menu.spec.folder < store->folder_count)
		{
			char name[MENU_LABEL];

			upper_copy(name, sizeof(name), menu.spec.folder == AE_PF_FAVOURITES ? "FAVOURITES" :
				store->names[menu.spec.folder]);
			snprintf(label, sizeof(label), "REMOVE FROM %s", name);
			add_item(ITEM_REMOVE, menu.spec.folder, label, 0);
		}
		break;
	case MODE_ACTIONS:
		add_item(ITEM_RENAME, menu.spec.folder, "RENAME", 0);
		add_item(ITEM_DELETE, menu.spec.folder, "DELETE", 0);
		break;
	default:
		add_item(ITEM_KEEP, menu.spec.folder, "KEEP IT", 0);
		add_item(ITEM_DELETE_SURE, menu.spec.folder, "DELETE", 0);
		break;
	}
}

static float menu_text(void)
{
	return units(&menu.spec.density, MENU_TEXT_U);
}

/* the title's words: the item's name upper case (a question in its own case) */
static void title_words(char *out, int size)
{
	struct ae_picker_folders const *store = ae_picker_store(menu.spec.kind);

	if (menu.mode == MODE_CONFIRM)
		snprintf(out, (size_t)size, "Delete %s? Its %s stay.", ae_pf_name(store, menu.spec.folder),
			menu.spec.kind == AE_PF_MAPS ? "maps" : "gametypes");
	else
		upper_copy(out, size, menu.item);
}

/* the box again (the items, the mode, the field): placed over the row, its edges off the rows' words behind it */
static void menu_place(short current)
{
	struct ae_density const *d = &menu.spec.density;
	struct ae_rect bounds = menu.spec.bounds;
	float width = units(d, MENU_MINIMUM_U), title_width, top, bottom, band;
	short index, first, visible;
	char title[128];
	int question = menu.mode == MODE_CONFIRM;

	menu.item_height = ae_size_row(d) * MENU_ITEM_PER_ROW;
	band = units(d, question ? MENU_QUESTION_U : MENU_TITLE_U);
	title_words(title, sizeof(title));
	for (index = 0; index < menu.count; index++)
	{
		float label = ae_draw_text_width(AE_FONT_ROW, menu_text(), menu.labels[index]) + units(d, MENU_EXTRA_U);

		width = label > width ? label : width;
	}
	title_width = (question ? ae_draw_text_width(AE_FONT_BODY, units(d, MENU_QUESTION_TEXT_U), title) :
		ae_draw_text_tracked_width(AE_FONT_BODY, units(d, MENU_TITLE_TEXT_U), 0.09f, title)) + units(d, MENU_TITLE_SIDES_U);
	width = title_width > width ? title_width : width;
	if (menu.naming && width < units(d, MENU_NAMING_U))
		width = units(d, MENU_NAMING_U);
	/* (the field's slot is taller than an item, and its help line under it) */
	menu.extra = menu.naming ? units(d, FIELD_SLOT_U) - menu.item_height + units(d, HELP_BAND_U) : 0.0f;
	bounds.y += band;
	bounds.height -= band + menu.extra;
	ae_picker_place(&menu.spec.row, &bounds, menu.count, current, menu.item_height, width, &menu.popover, &first);
	/* (the box's right on the list rows' right) */
	menu.popover.x = menu.spec.row.x + menu.spec.row.width - menu.popover.width;
	visible = (short)(menu.popover.height / menu.item_height + 0.5f);
	ae_list_init(&menu.list, menu.count, visible > 0 ? visible : 1);
	ae_list_set_focus(&menu.list, current >= 0 && current < menu.count ? current : 0);
	menu.list.first = first;
	top = menu.popover.y - band;
	bottom = menu.popover.y + menu.popover.height + menu.extra;
	/* the edge rule: a top or bottom edge that would cut a row's words behind it (the middle of the row, where its
	text sits) goes out to the middle of that row's gap (the title band grows; the bottom takes the space) */
	if (menu.spec.pitch > 0.0f)
	{
		float gap = menu.spec.pitch - menu.spec.row_height, words = menu.spec.row_height * WORDS_SHARE, at, center;
		int row;

		row = (int)((top - menu.spec.rows_top) / menu.spec.pitch);
		at = menu.spec.rows_top + (float)row * menu.spec.pitch;
		center = at + menu.spec.row_height * 0.5f;
		if (top >= menu.spec.rows_top && top > center - words && top < center + words)
			top = at - gap * 0.5f;
		row = (int)((bottom - menu.spec.rows_top) / menu.spec.pitch);
		at = menu.spec.rows_top + (float)row * menu.spec.pitch;
		center = at + menu.spec.row_height * 0.5f;
		if (bottom > center - words && bottom < center + words && at + menu.spec.row_height <= menu.spec.rows_bottom)
			bottom = at + menu.spec.row_height + gap * 0.5f;
	}
	menu.band = menu.popover.y - top;
	menu.extra = bottom - (menu.popover.y + menu.popover.height);
}

static void menu_close(void)
{
	ae_ui_pop();
}

/* the name field: NEW FOLDER... (a free name) or RENAME (the folder's), all selected; a keyboard types, a pad gets AE's
keyboard beside it */
static void keyboard_open(void);

static void naming_begin(short item, int device)
{
	struct ae_picker_folders const *store = ae_picker_store(menu.spec.kind);
	char initial[AE_PF_NAME];

	if (menu.kinds[item] == ITEM_RENAME)
		snprintf(initial, sizeof(initial), "%s", ae_pf_name(store, menu.spec.folder));
	else
		ae_pf_new_name(store, initial, sizeof(initial));
	ae_text_init(&menu.name, initial, AE_PF_NAME_LETTERS, 0);
	ae_text_select_all(&menu.name);
	menu.naming = 1;
	menu.naming_item = item;
	menu.refused = menu.live = 0;
	menu.caret_since = ae_motion_now();
	menu_place(item);
	if (device == AE_DEVICE_KEYBOARD_MOUSE)
	{
		menu.edit.holder = &menu;
		ae_field_edit_begin(&menu.edit, &menu.name);
		ae_text_select_all(&menu.name);
		menu.typing = 1;
	}
	else
		keyboard_open();
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void naming_end(void)
{
	if (menu.typing)
	{
		menu.typing = 0;
		ae_field_edit_end(&menu.edit, 1);
	}
	menu.naming = 0;
	menu.refused = menu.live = 0;
	menu_place(menu.naming_item);
}

/* the name taken: a folder made (and the item put in it) or renamed; refused: failure, the ring and the help line
say why */
static void naming_commit(void)
{
	struct ae_picker_folders *store = ae_picker_store(menu.spec.kind);
	char name[AE_PF_NAME], old[AE_PF_NAME];
	int why;
	short folder;

	snprintf(name, sizeof(name), "%s", menu.name.text);
	why = ae_pf_name_check(store, name, menu.kinds[menu.naming_item] == ITEM_RENAME ? menu.spec.folder : -1);
	if (why == AE_PF_NAME_OK && menu.kinds[menu.naming_item] == ITEM_NEW && store->folder_count >= AE_PF_FOLDERS)
		why = -1;
	if (why != AE_PF_NAME_OK)
	{
		menu.refused = why;
		snprintf(menu.refused_name, sizeof(menu.refused_name), "%s", menu.name.text);
		platform_log("%s: folder name refused (%s)", menu.spec.log, refusal_word(why));
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	if (menu.kinds[menu.naming_item] == ITEM_RENAME)
	{
		snprintf(old, sizeof(old), "%s", ae_pf_name(store, menu.spec.folder));
		folder = ae_pf_folder_rename(store, menu.spec.folder, name);
		ae_picker_store_save(menu.spec.kind);
		platform_log("%s: folder '%s' renamed '%s'", menu.spec.log, old, name);
		if (menu.typing)
		{
			menu.typing = 0;
			ae_field_edit_end(&menu.edit, 1);
		}
		menu.naming = 0;
		if (menu.spec.changed)
			menu.spec.changed(folder, menu.spec.context);
		ae_sound_request(AE_SOUND_FORWARD, 0);
		menu_close();
		return;
	}
	folder = ae_pf_folder_add(store, name);
	platform_log("%s: folder '%s' made", menu.spec.log, name);
	/* (the item put in it at once; the folders saved either way) */
	if (menu.spec.changed)
		menu.spec.changed(folder, menu.spec.context);
	if (folder < 0 || !menu.spec.toggle || menu.spec.toggle(folder, menu.spec.context) < 0)
		ae_picker_store_save(menu.spec.kind);
	naming_end();
	items_make();
	menu_place(folder >= 0 ? folder : (short)(menu.count - 1));
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void menu_keyboard_done(int keep, void *context)
{
	(void)context;
	menu.keyboard_open = 0;
	/* (a physical key closed it into the field's own typing: that goes on) */
	if (menu.edit.active && menu.edit.text == &menu.name)
	{
		menu.typing = 1;
		return;
	}
	if (!menu.naming)
		return;
	if (keep)
		naming_commit();
	else
		naming_end();
}

static void keyboard_open(void)
{
	struct ae_keyboard_spec spec;
	float right;

	/* (the field's well as it will be drawn: its slot in the box, inset; layout units) */
	menu.well.x = menu.popover.x + units(&menu.spec.density, FIELD_INSET_U);
	menu.well.y = menu.popover.y + (float)(menu.naming_item - menu.list.first) * menu.item_height +
		units(&menu.spec.density, FIELD_TOP_U);
	menu.well.width = menu.popover.width - 2.0f * units(&menu.spec.density, FIELD_INSET_U);
	menu.well.height = units(&menu.spec.density, FIELD_HEIGHT_U);
	memset(&spec, 0, sizeof(spec));
	spec.text = &menu.name;
	/* (beside the field, right of the list and its scrollbar, its bottom on the field's bottom: it leaves the panel's
	name line whole) */
	right = menu.well.x + menu.well.width;
	if (menu.spec.list_right > right)
		right = menu.spec.list_right;
	spec.field = menu.well;
	spec.field.width = right - menu.well.x;
	spec.bottom_on_field = 1;
	spec.bounds = menu.spec.bounds;
	spec.density = menu.spec.density;
	spec.done = menu_keyboard_done;
	spec.edit = &menu.edit;
	menu.edit.holder = &menu;
	if (ae_keyboard_open(&spec, AE_OWNER_PARENT))
	{
		menu.keyboard_open = 1;
		platform_log("%s: keyboard open", menu.spec.log);
	}
}

/* A (or a click) on an item */
static void menu_accept(short item, int device)
{
	struct ae_picker_folders *store = ae_picker_store(menu.spec.kind);
	int now;

	if (item < 0 || item >= menu.count)
		return;
	switch (menu.kinds[item])
	{
	case ITEM_FOLDER:
		now = menu.spec.toggle ? menu.spec.toggle(menu.folders[item], menu.spec.context) : -1;
		if (now < 0)
		{
			ae_sound_request(AE_SOUND_FAILURE, 0);
			return;
		}
		menu.ticked[item] = (unsigned char)now;
		ae_sound_request(AE_SOUND_CURSOR, 0);
		return;
	case ITEM_NEW:
	case ITEM_RENAME:
		naming_begin(item, device);
		return;
	case ITEM_REMOVE:
		if (menu.spec.toggle && menu.spec.toggle(menu.folders[item], menu.spec.context) >= 0)
			ae_sound_request(AE_SOUND_BACK, 0);
		menu_close();
		return;
	case ITEM_DELETE:
		menu.mode = MODE_CONFIRM;
		items_make();
		menu_place(0);
		ae_sound_request(AE_SOUND_FORWARD, 0);
		return;
	case ITEM_KEEP:
		ae_sound_request(AE_SOUND_BACK, 0);
		menu_close();
		return;
	case ITEM_DELETE_SURE:
	{
		char name[AE_PF_NAME];

		snprintf(name, sizeof(name), "%s", ae_pf_name(store, menu.spec.folder));
		if (!ae_pf_folder_delete(store, menu.spec.folder))
		{
			ae_sound_request(AE_SOUND_FAILURE, 0);
			return;
		}
		ae_picker_store_save(menu.spec.kind);
		platform_log("%s: folder '%s' deleted", menu.spec.log, name);
		if (menu.spec.changed)
			menu.spec.changed(-1, menu.spec.context);
		ae_sound_request(AE_SOUND_FORWARD, 0);
		menu_close();
		return;
	}
	}
}

/* ---------- its screen */

/* the box in the current view's drawing units */
static void to_drawing(struct ae_rect const *layout, struct ae_rect *drawing)
{
	struct ae_view view;

	ae_draw_current_view(&view);
	drawing->x = (layout->x - view.x) / view.scale;
	drawing->y = (layout->y - view.y) / view.scale;
	drawing->width = layout->width / view.scale;
	drawing->height = layout->height / view.scale;
}

static void check_mark(float x, float center, unsigned int color)
{
	struct ae_density const *d = &menu.spec.density;
	float half = units(d, MENU_CHECK_U), stroke = units(d, MENU_CHECK_STROKE_U);

	ae_draw_line(x - half, center, x - half * 0.35f, center + half * 0.65f, stroke, color);
	ae_draw_line(x - half * 0.35f, center + half * 0.65f, x + half, center - half * 0.8f, stroke, color);
}

/* the name field in its slot (top: the slot's top) and the help line under it */
static void field_draw(float x, float top, float width)
{
	struct ae_density const *d = &menu.spec.density;
	float inset = units(d, FIELD_INSET_U), pad = units(d, FIELD_PAD_U), text = ae_size_text(d);
	float corner = units(d, d->metrics->corner), help = units(d, HELP_TEXT_U), minor = ae_size_minor(d);
	struct ae_rect well;
	float center, drawn, count_width;
	short from, to;
	char count[16], shown[AE_TEXT_MAXIMUM + 8];
	int why = menu.refused ? menu.refused : menu.live;
	unsigned int ring = why ? AE_COLOR_WARNING : AE_COLOR_ACCENT;
	int full = menu.name.length >= AE_PF_NAME_LETTERS;

	well.x = x + inset;
	well.y = top + units(d, FIELD_TOP_U);
	well.width = width - 2.0f * inset;
	well.height = units(d, FIELD_HEIGHT_U);
	center = well.y + well.height * 0.5f;
	{
		struct ae_view view;

		/* (layout units, for AE's keyboard) */
		ae_draw_current_view(&view);
		menu.well.x = view.x + well.x * view.scale;
		menu.well.y = view.y + well.y * view.scale;
		menu.well.width = well.width * view.scale;
		menu.well.height = well.height * view.scale;
	}
	ae_draw_rect(well.x, well.y, well.width, well.height, corner, AE_COLOR_WELL);
	ae_draw_outline(well.x, well.y, well.width, well.height, corner, units(d, 1.0f), AE_COLOR_RULE);
	ae_draw_outline(well.x, well.y, well.width, well.height, corner, units(d, RING_U), ring);
	snprintf(count, sizeof(count), "%d / %d", menu.name.length, AE_PF_NAME_LETTERS);
	count_width = ae_draw_text_width(AE_FONT_BODY, minor, count);
	ae_draw_text(AE_FONT_BODY, minor, well.x + well.width - pad, text_top(AE_FONT_BODY, minor, center), AE_ALIGN_RIGHT,
		full ? AE_COLOR_WARNING : AE_COLOR_MUTED, count);
	ae_fit_text(AE_FONT_ROW, text, menu.name.text, well.width - 3.0f * pad - count_width, shown, sizeof(shown));
	/* (the selection: the accent wash behind its letters, 60 % of the field's height) */
	if (ae_text_selection(&menu.name, &from, &to) && to <= (short)strlen(shown))
	{
		char before[AE_TEXT_MAXIMUM + 8], part[AE_TEXT_MAXIMUM + 8];
		float select_height = well.height * FIELD_SELECT;

		snprintf(before, sizeof(before), "%.*s", from, shown);
		snprintf(part, sizeof(part), "%.*s", to - from, shown + from);
		ae_draw_rect(well.x + pad + ae_draw_text_width(AE_FONT_ROW, text, before), center - select_height * 0.5f,
			ae_draw_text_width(AE_FONT_ROW, text, part), select_height, 0.0f, AE_COLOR_TEXT_SELECT);
	}
	drawn = ae_draw_text(AE_FONT_ROW, text, well.x + pad, text_top(AE_FONT_ROW, text, center), AE_ALIGN_LEFT,
		AE_COLOR_TITLE, shown);
	{
		char before[AE_TEXT_MAXIMUM + 8];
		float caret_x;

		snprintf(before, sizeof(before), "%.*s", menu.name.caret, shown);
		caret_x = menu.name.caret >= (short)strlen(shown) ? drawn : ae_draw_text_width(AE_FONT_ROW, text, before);
		if (((ae_motion_now() - menu.caret_since) / CARET_MS) % 2u == 0u)
			ae_draw_rect(well.x + pad + caret_x + units(d, 1.0f), center - ae_draw_cap_height(AE_FONT_ROW, text) * 0.75f,
				units(d, CARET_U), ae_draw_cap_height(AE_FONT_ROW, text) * 1.5f, 0.0f, AE_COLOR_ACCENT);
	}
	ae_hit_add(well.x, well.y, well.width, well.height, MENU_HIT, AE_PART_FIELD, menu.naming_item);
	/* the help line: the rule, or why the name was refused */
	ae_draw_text(AE_FONT_BODY, help, x + units(d, MENU_TITLE_X_U), well.y + well.height + units(d, HELP_GAP_U),
		AE_ALIGN_LEFT, why ? AE_COLOR_WARNING : AE_COLOR_MUTED, refusal(why));
}

static void menu_draw(struct ae_screen *screen)
{
	struct ae_density const *d = &menu.spec.density;
	struct ae_rect box, band_layout;
	float item, band, size = menu_text(), track = units(d, TRACK_U), y, extra;
	short row;
	char title[128];
	int question = menu.mode == MODE_CONFIRM;

	(void)screen;
	to_drawing(&menu.popover, &box);
	band_layout = menu.popover;
	band_layout.y -= menu.band;
	band_layout.height = menu.band;
	{
		struct ae_rect b;

		to_drawing(&band_layout, &b);
		band = b.height;
	}
	item = box.height / (float)(menu.list.rows > 0 ? menu.list.rows : 1);
	extra = menu.extra * (box.width / (menu.popover.width > 0.0f ? menu.popover.width : 1.0f));
	/* clicks anywhere else close it */
	ae_hit_add(0.0f, 0.0f, ae_draw_view_width(), (float)AE_LAYOUT_HEIGHT, MENU_HIT, AE_PART_OUTSIDE, -1);
	/* the title band (opaque, part of the box) */
	ae_draw_rect(box.x, box.y - band, box.width, band + box.height + extra, units(d, d->metrics->corner),
		AE_COLOR_POPOVER);
	ae_hit_add(box.x, box.y - band, box.width, band + box.height + extra, MENU_HIT, AE_PART_CARD, -1);
	title_words(title, sizeof(title));
	if (question)
		ae_draw_text(AE_FONT_BODY, units(d, MENU_QUESTION_TEXT_U), box.x + units(d, MENU_TITLE_X_U),
			text_top(AE_FONT_BODY, units(d, MENU_QUESTION_TEXT_U), box.y - band * 0.5f), AE_ALIGN_LEFT, AE_COLOR_TEXT,
			title);
	else
	{
		char fitted[128];

		ae_fit_text(AE_FONT_BODY, units(d, MENU_TITLE_TEXT_U), title, box.width - units(d, MENU_TITLE_SIDES_U), fitted,
			sizeof(fitted));
		ae_draw_text_tracked(AE_FONT_BODY, units(d, MENU_TITLE_TEXT_U), 0.09f, box.x + units(d, MENU_TITLE_X_U),
			text_top(AE_FONT_BODY, units(d, MENU_TITLE_TEXT_U), box.y - band * 0.5f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
			fitted);
	}
	ae_draw_clip_push(box.x, box.y, box.width, box.height + extra);
	y = box.y;
	for (row = 0; row < menu.list.rows; row++)
	{
		short index = ae_list_item_at_row(&menu.list, row);
		float center = y + item * 0.5f;
		int focused = index == menu.list.focus && !(menu.naming && index == menu.naming_item);
		unsigned int color;

		if (index < 0)
			break;
		if (menu.naming && index == menu.naming_item)
		{
			field_draw(box.x, y, box.width);
			y += units(d, FIELD_SLOT_U) + units(d, HELP_BAND_U);
			continue;
		}
		color = focused ? AE_COLOR_SELECTION_TEXT : menu.ticked[index] ? AE_COLOR_ACCENT : AE_COLOR_TEXT;
		if (focused)
		{
			ae_draw_rect(box.x, y, box.width - (menu.list.count > menu.list.rows ? track * 2.0f : 0.0f), item, 0.0f,
				AE_COLOR_SELECTION);
			ae_draw_rect(box.x, y, units(d, NOTCH_U), item, 0.0f, AE_COLOR_ACCENT);
		}
		if (menu.ticked[index])
			check_mark(box.x + units(d, MENU_CHECK_X_U), center, color);
		ae_draw_text(AE_FONT_ROW, size, box.x + units(d, MENU_TEXT_X_U), text_top(AE_FONT_ROW, size, center),
			AE_ALIGN_LEFT, color, menu.labels[index]);
		ae_hit_add(box.x, y, box.width, item, MENU_HIT, AE_PART_ITEM, index);
		y += item;
	}
	ae_draw_clip_pop();
	if (menu.list.count > menu.list.rows)
	{
		float thumb_y, thumb_height, x = box.x + box.width - track * 1.5f;

		ae_scrollbar_thumb(menu.list.count, menu.list.rows, (float)menu.list.first, box.height, item, &thumb_y,
			&thumb_height);
		ae_draw_rect(x, box.y, track, box.height, track * 0.5f, AE_COLOR_TRACK);
		ae_draw_rect(x, box.y + thumb_y, track, thumb_height, track * 0.5f, AE_COLOR_MUTED);
	}
}

static void menu_update(struct ae_screen *screen)
{
	(void)screen;
	if (menu.typing && menu.edit.active && !ae_field_edit_keys(&menu.edit))
		ae_sound_request(AE_SOUND_FAILURE, 0);
	if (!menu.naming)
		return;
	/* (a refusal stands until the name changes; meanwhile the live check: a taken name or a character it can't take,
	said as it is typed, no sound) */
	if (menu.refused && strcmp(menu.refused_name, menu.name.text))
		menu.refused = 0;
	{
		char name[AE_TEXT_MAXIMUM];
		size_t length;
		int why;

		/* (a space at the end is the typing's, not yet the name's) */
		snprintf(name, sizeof(name), "%s", menu.name.text);
		for (length = strlen(name); length && name[length - 1] == ' '; length--)
			name[length - 1] = 0;
		why = ae_pf_name_check(ae_picker_store(menu.spec.kind), name,
			menu.kinds[menu.naming_item] == ITEM_RENAME ? menu.spec.folder : -1);

		menu.live = why == AE_PF_NAME_TAKEN || why == AE_PF_NAME_CHARACTERS || why == AE_PF_NAME_LONG ? why : 0;
	}
}

static int menu_handle(struct ae_screen *screen, struct ae_event const *event)
{
	short before = menu.list.focus;

	(void)screen;
	if (menu.naming)
	{
		/* (typing: Enter or START makes it, Esc or B cancels; a pad: A opens AE's keyboard again) */
		if (event->action == AE_ACTION_START || (menu.typing && event->action == AE_ACTION_ACCEPT))
			naming_commit();
		else if (event->action == AE_ACTION_BACK)
		{
			naming_end();
			ae_sound_request(AE_SOUND_BACK, 0);
		}
		else if (event->action == AE_ACTION_ACCEPT && !menu.keyboard_open)
			keyboard_open();
		return 1;
	}
	switch (event->action)
	{
	case AE_ACTION_UP: ae_list_move(&menu.list, -1, 0); break;
	case AE_ACTION_DOWN: ae_list_move(&menu.list, 1, 0); break;
	case AE_ACTION_ACCEPT:
		menu_accept(menu.list.focus, event->device);
		return 1;
	/* (B: not handled: the stack closes it, with the back sound; on the delete's question it is KEEP IT) */
	case AE_ACTION_BACK: return 0;
	default: return 1;
	}
	if (menu.list.focus != before)
		ae_sound_request(AE_SOUND_CURSOR, event->repeat);
	return 1;
}

static void menu_pointer(struct ae_screen *screen, struct ae_pointer const *pointer)
{
	struct ae_hit hit;
	int mine;

	(void)screen;
	mine = ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == MENU_HIT;
	if (pointer->wheel_steps && mine && hit.part != AE_PART_OUTSIDE && !menu.naming)
		ae_list_scroll(&menu.list, (short)(-3 * pointer->wheel_steps));
	if (pointer->moved && mine && hit.part == AE_PART_ITEM && hit.index >= 0 && hit.index < menu.list.count &&
		!menu.naming)
		menu.list.focus = hit.index;
	if (!pointer->left_clicks)
		return;
	if (mine && hit.part == AE_PART_FIELD && menu.naming && !menu.typing && !menu.keyboard_open)
	{
		menu.edit.holder = &menu;
		ae_field_edit_begin(&menu.edit, &menu.name);
		menu.typing = 1;
		return;
	}
	if (mine && hit.part == AE_PART_ITEM && hit.index >= 0 && hit.index < menu.list.count && !menu.naming)
		menu_accept(hit.index, AE_DEVICE_KEYBOARD_MOUSE);
	else if (!mine || hit.part == AE_PART_OUTSIDE)
	{
		ae_sound_request(AE_SOUND_BACK, 0);
		menu_close();
	}
}

static void menu_leave(struct ae_screen *screen)
{
	(void)screen;
	if (menu.typing)
	{
		menu.typing = 0;
		ae_field_edit_end(&menu.edit, 0);
	}
	menu.naming = 0;
	menu.open = 0;
}

static struct ae_screen_class const menu_class =
{
	.name = "folder menu", .leave = menu_leave, .handle = menu_handle, .draw = menu_draw, .pointer = menu_pointer,
	.popover = 1, .update = menu_update,
};

int ae_folder_menu_open(struct ae_folder_menu_spec const *spec)
{
	if (!spec || (menu.open && ae_ui_holds(&menu)))
		return 0;
	memset(&menu, 0, sizeof(menu));
	menu.spec = *spec;
	snprintf(menu.item, sizeof(menu.item), "%s", spec->item ? spec->item : "");
	menu.spec.item = menu.item;
	menu.mode = spec->actions ? MODE_ACTIONS : MODE_FOLDERS;
	items_make();
	menu_place(0);
	if (!ae_ui_push(&menu_class, AE_OWNER_PARENT, &menu))
		return 0;
	menu.open = 1;
	platform_log("%s: folder menu '%s'", spec->log, menu.item);
	ae_sound_request(AE_SOUND_FORWARD, 0);
	return 1;
}

int ae_folder_menu_is_open(void)
{
	return menu.open && ae_ui_holds(&menu);
}

static struct ae_prompt prompt_of(int button, const char *label, int action)
{
	struct ae_prompt prompt;

	prompt.button = button;
	prompt.label = label;
	prompt.key = NULL;
	prompt.action = action;
	prompt.pair = 0;
	return prompt;
}

short ae_folder_menu_prompts(struct ae_prompt *prompts, short maximum)
{
	short count = 0;

	if (maximum < 2)
		return 0;
	if (menu.naming)
	{
		prompts[count++] = prompt_of(AE_BUTTON_START, menu.kinds[menu.naming_item] == ITEM_RENAME ? "Rename it" :
			"Make the folder", AE_ACTION_START);
		prompts[count++] = prompt_of(AE_BUTTON_B, "Cancel", AE_ACTION_BACK);
	}
	else if (menu.mode == MODE_FOLDERS)
	{
		prompts[count++] = prompt_of(AE_BUTTON_A, "Add or take out", AE_ACTION_ACCEPT);
		prompts[count++] = prompt_of(AE_BUTTON_B, "Done", AE_ACTION_BACK);
	}
	else
	{
		prompts[count++] = prompt_of(AE_BUTTON_A, "Select", AE_ACTION_ACCEPT);
		prompts[count++] = prompt_of(AE_BUTTON_B, menu.mode == MODE_CONFIRM ? "Keep it" : "Back", AE_ACTION_BACK);
	}
	return count;
}

/* ---------- the empty folder's line */

void ae_picker_empty_line(struct ae_density const *density, float x, float center_y, int kind)
{
	float size = units(density, EMPTY_TEXT_U), glyph = units(density, EMPTY_GLYPH_U), top = text_top(AE_FONT_BODY,
		size, center_y);
	float gap = glyph * 0.3f, floor = 8.0f * density->pixel, pen = x;

	if (gap < floor)
		gap = floor;
	pen += ae_draw_text(AE_FONT_BODY, size, pen, top, AE_ALIGN_LEFT, AE_COLOR_MUTED, "Nothing in this folder yet. Press");
	pen += gap;
	pen += ae_widget_button_mark(density, AE_BUTTON_X, pen, center_y, glyph, 0, 1);
	pen += gap;
	ae_draw_text(AE_FONT_BODY, size, pen, top, AE_ALIGN_LEFT, AE_COLOR_MUTED, kind == AE_PF_GAMETYPES ?
		"on a gametype to put it in one." : "on a map to put it in one.");
}
