/*
AE_SCREEN_CROSSHAIR.C

(0.7.7) AE's CROSSHAIR screen (ae_screen_crosshair.h), the design's layout C (the workbench with tabs), with its
picks: the stage and tiles' column narrower and the tab panel wider than drawn, GAP for the
lines' offsets, rows under an OFF switch dimmed but still stepped, no VIEW SIZE switch, NEW a fresh default crosshair and
DUPLICATE a copy. GAME OPTIONS' layout: the title and the crumb, the rule, the content, the help over the footer. Positions
are fractions of the frame (ae_frame_compute), sizes u (1080 to the frame's height, times UI SCALE).

The content: the SAVED strip (the saved crosshairs in slot order, NEW while a slot is free, then RENAME, DUPLICATE,
SHARE CODE, DELETE: one row the focus moves along), and under it two columns: the stage over the six tiles (one row of six
where they fit, else 3 x 2 at 4:3), and the tab panel (LB / RB: its tabs as GAME OPTIONS' chips). A tab's items: rows (the
settings model's rows: ae_settings_rows.c's AE_SETTINGS_CROSSHAIR ones, their labels, values and help) in one or two
columns, and grids (the shapes under their families, the palette). Up / down moves through a tab (a column, then the next;
up from its top goes to the strip), left / right steps a row or moves on a grid; LT / RT: a shape family, or the other
column. The values are the saved crosshair's (ae_crosshair_edit.c): a change is in the profile and on the HUD at once.
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "interface/player_ui.h"
#include "saved games/player_profile.h"
#include "../src/ae_draw.h"
#include "../src/ae_platform.h"
#include "ae_crosshair.h"
#include "ae_crosshair_edit.h"
#include "ae_crosshair_rules.h"
#include "ae_glue_player_settings.h"
#include "ae_glue_profiles.h"
#include "ae_hooks.h"
#include "ae_reticle_preview.h"
#include "ae_screen_crosshair.h"
#include "ae_settings_rows.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum
{
	XH_HIT = 0x7C20,
	HIT_TOP = XH_HIT + 1, HIT_TABS, HIT_PANEL, HIT_BACKGROUND, HIT_PROMPTS, HIT_FIELD,
	PLAYERS = 4,
	PROMPTS = 8,
	TOP_MAXIMUM = AE_XH_SLOTS + 5,
	ITEMS = 72,
	LABELS = 8,
	BLOCKS = 3,
	VALUE_TEXT = 64
};

/* the frame's positions (fractions of 1920 x 1080 at 100 %): GAME OPTIONS' */
#define TITLE_Y (70.0f / 1080.0f)
#define RULE_Y (156.0f / 1080.0f)
#define CONTENT_Y (186.0f / 1080.0f)
/* sizes, u */
#define TITLE_U 60.0f
#define CRUMB_U 20.0f
#define CRUMB_TRACKING 0.09f
#define CRUMB_GAP_U 14.0f
#define CRUMB_DOT_U 5.0f
#define EMBLEM_U 30.0f
#define EMBLEM_NUMBER_U 20.0f
#define EMBLEM_GAP_U 12.0f
#define RULE_U 1.0f
#define LABEL_U 15.0f
#define LABEL_TRACKING 0.12f
#define LABEL_GAP_U 12.0f
#define STRIP_TILE_U 104.0f
#define STRIP_TILE_TALL_U 92.0f
#define STRIP_GAP_U 8.0f
#define STRIP_ART_U 52.0f
#define STRIP_NAME_U 13.0f
#define STRIP_ACTIONS_GAP_U 32.0f
#define ACTION_HEIGHT_U 40.0f
#define ACTION_PAD_U 14.0f
#define ACTION_TEXT_U 16.0f
#define ACTION_GAP_U 13.0f
#define ACTION_GLYPH_U 24.0f
#define CONTENT_GAP_U 40.0f
#define COLUMN_GAP_U 32.0f
#define LEFT_SHARE 0.42f
#define TILES_GAP_U 8.0f
#define STAGE_TILES_GAP_U 20.0f
#define TILE_SWATCH_MAX_U 92.0f
#define TILE_LABEL_U 26.0f
#define PILL_HEIGHT_U 36.0f
#define PILL_TEXT_U 15.0f
#define PANEL_PAD_U 16.0f
#define TABS_TO_CONTENT_U 18.0f
#define TWO_COLUMNS_U 820.0f
#define ROWS_GAP_U 16.0f
#define BLOCK_GAP_U 18.0f
#define SECTION_U 32.0f
#define GRID_TILE_U 112.0f
#define GRID_TALL 0.84f
#define GRID_GAP_U 8.0f
#define GRID_HEADER_U 34.0f
#define GRID_GROUP_GAP_U 10.0f
#define COLOR_TILE_U 78.0f
#define COLOR_TILE_TALL_U 84.0f
#define NOTCH_U 6.0f
#define MARK_U 3.0f
#define HELP_LINES 2
#define HELP_GAP_U 28.0f
#define FOOTER_ABOVE_U 62.0f
#define FOOTER_ROOM_U 40.0f
#define DIM_ALPHA 0.45f
#define RGB_STEP 8
/* LT / RT on a column of rows: this many rows a press */
#define PAGE_ROWS 4

enum { TAB_SHAPE, TAB_COLOR, TAB_LINES, TAB_FRAME, TAB_MOTION, TABS };
static const char *const tab_names[TABS] = { "SHAPE", "COLOR", "LINES", "FRAME", "MOTION" };
enum { ITEM_ROW, ITEM_SHAPE, ITEM_COLOR, ITEM_HEX, ITEM_RGB };
enum { REGION_TOP, REGION_PANEL };
enum { TOP_SLOT, TOP_NEW, TOP_RENAME, TOP_DUPLICATE, TOP_SHARE, TOP_DELETE };
enum { TYPING_NONE, TYPING_NAME, TYPING_HEX };
enum { DIALOG_NONE, DIALOG_KEEP, DIALOG_SHARE, DIALOG_PASTED, DIALOG_DELETE, DIALOG_NOTE };

/* the player colours (the gallery's), by player */
static unsigned int const player_colors[PLAYERS] = { 0xC0392BFFu, 0x2E86DEFFu, 0x7FA035FFu, 0xE6B422FFu };
static const char *const channel_names[3] = { "RED", "GREEN", "BLUE" };

/* a tab's items: rows and grid entries, placed relative to the tab content's top-left (layout units) */
struct item { short kind, value, block; struct ae_rect rect; };
struct label { const char *text; struct ae_rect rect; };
struct block
{
	short first, count, columns, grid;
	/* a grid's: its entries (headers among them, AE_PICKER_HEADER first), the entry's item (-1 a header) */
	struct ae_picker_grid layout;
	const char *values[AE_PICKER_GRID_MAXIMUM];
	short item_of[AE_PICKER_GRID_MAXIMUM], entry_of[ITEMS];
	struct ae_rect header_rect[AE_PICKER_GRID_MAXIMUM];
};
struct tab_layout
{
	struct item items[ITEMS];
	short item_count;
	struct label labels[LABELS];
	short label_count;
	struct block blocks[BLOCKS];
	short block_count;
	float height;
};

/* the strip's entries */
struct top_entry { short kind, slot; struct ae_rect rect; int disabled; };

static struct
{
	short player;
	char player_name[48];
	struct ae_density density;
	struct ae_frame frame;
	float logged_width, logged_scale;
	/* the profile as the screen opened (UNDO), and the slot it edited */
	struct ae_crosshair_edit_profile opened;
	int opened_slot;
	/* the focus: the strip or the panel; the strip's entry; the tab, its item and how far it is scrolled */
	short region, top_focus, tab, item, logged_tab;
	char logged_focus[64];
	float scroll;
	int scroll_to_focus;
	struct top_entry top[TOP_MAXIMUM];
	short top_count;
	struct tab_layout layout;
	struct ae_rect panel_content;        /* the tab content's window, layout units (as last drawn) */
	float panel_width;                   /* the tab content's width the layout was made for */
	int two_columns;
	struct ae_rect actions_rect;         /* the actions' place in the strip (RENAME's field over it) */
	struct ae_rect stage_rect;
	int background;
	/* typing: a name (RENAME) or a colour (CUSTOM COLOR) */
	int typing;
	struct ae_text typed;
	struct ae_field_edit typed_edit;
	unsigned long caret_since;
	struct ae_rect field_rect;
	int keyboard_open;
	/* a colour channel's slider held by the pointer: its item + 1, 0 none */
	short drag_item;
	/* the dialog open; the one to open once it is gone */
	int dialog, then_dialog;
	struct ae_crosshair pasted;
	int pasted_unknown, pasted_result;
	char dialog_title[64], dialog_body[480];
	struct ae_prompt prompts[PROMPTS];
	short prompt_count;
	/* (the clearance check's) the parts as last drawn, logged once a frame size: the pill, the tiles, the panel, the tabs,
	the help's top and the footer's middle */
	struct ae_rect pill_rect, tile_rects[6], panel_rect, tab_rects[TABS];
	float help_top, footer_middle;
	int layout_logged;
	/* shape and palette names for the grids (outlive the draws) */
	char shape_headers[4][24];
} screen;

static struct ae_screen_class const crosshair_class;

/* ---------- sizes */

static float gu(float spec_units)
{
	return spec_units * screen.frame.unit;
}

static float cap_top(int font, float size, float center_y)
{
	return center_y - ae_draw_cap_height(font, size) * 0.5f;
}

static void density_read(void)
{
	struct ae_layout layout;
	float scale = ae_settings_ui_scale();

	ae_draw_current_layout(&layout);
	ae_density_full(layout.height * layout.scale, scale, &screen.density);
	ae_frame_compute(layout.width, scale, &screen.frame);
	if (layout.width != screen.logged_width || layout.scale != screen.logged_scale)
	{
		screen.logged_width = layout.width;
		screen.logged_scale = layout.scale;
		screen.layout_logged = 0;
		platform_log("ae crosshair screen: frame %.0fx%.0f unit %.2f", screen.frame.rect.width * layout.scale,
			screen.frame.rect.height * layout.scale, screen.frame.unit);
	}
}

static float left_x(void)
{
	return screen.frame.rect.x + screen.frame.margin;
}

static float right_x(void)
{
	return screen.frame.rect.x + screen.frame.rect.width - screen.frame.margin;
}

static struct ae_prompt prompt_of(int button, const char *label, int action, int pair)
{
	struct ae_prompt prompt;

	prompt.button = button;
	prompt.label = label;
	prompt.key = NULL;
	prompt.action = action;
	prompt.pair = pair;
	return prompt;
}

/* ---------- the crosshairs */

static void player_name_read(void)
{
	struct player_profile profile;
	int at;

	memset(&profile, 0, sizeof(profile));
	player_ui_get_active_player_profile(screen.player, &profile);
	for (at = 0; at < (int)sizeof(screen.player_name) - 1 && at < MAXIMUM_PLAYER_PROFILE_NAME_LENGTH && profile.player_name[at];
		at++)
		screen.player_name[at] = profile.player_name[at] >= 'a' && profile.player_name[at] <= 'z' ?
			(char)(profile.player_name[at] - 'a' + 'A') : profile.player_name[at] >= 0x20 && profile.player_name[at] < 0x7F ?
			(char)profile.player_name[at] : '?';
	screen.player_name[at] = 0;
	if (!screen.player_name[0])
		snprintf(screen.player_name, sizeof(screen.player_name), "PLAYER %d", screen.player + 1);
}

static int current(struct ae_crosshair *xh)
{
	return ae_crosshair_edit_current(screen.player, xh);
}

static int changed(void)
{
	struct ae_crosshair_edit_profile now;

	ae_crosshair_edit_profile_take(screen.player, &now);
	return !ae_crosshair_edit_profile_same(&now, &screen.opened);
}

/* a row of the settings model by key (its labels, values and help) */
static struct ae_settings_row const *row_of(const char *key)
{
	short index = ae_settings_row_of(key);

	return index >= 0 ? ae_settings_row(index) : NULL;
}

static void row_value(struct ae_settings_row const *row, char *text, int size)
{
	text[0] = 0;
	ae_crosshair_edit_get(screen.player, row->key, text, size);
}

/* a row's value as shown ("2 PX", "ON") */
static void row_shown(struct ae_settings_row const *row, char *text, int size)
{
	char value[VALUE_TEXT];
	short index;

	row_value(row, value, sizeof(value));
	index = ae_settings_value_index(row, value);
	snprintf(text, (size_t)size, "%s", index >= 0 ? ae_settings_value_text(row, index) : value);
}

/* (the design's pick) a row under an OFF switch is dimmed, and still stepped: its set's switch is off */
static int row_dimmed(struct ae_settings_row const *row)
{
	static const char *const sets[] = { "xh.inner_", "xh.outer_", "xh.frame_", "xh.outline_" };
	char key[40], value[16];
	short index;

	if (!strcmp(row->key, "xh.centre_size"))
		return ae_crosshair_edit_get(screen.player, "xh.centre", value, sizeof(value)) && !strcmp(value, "0");
	for (index = 0; index < 4; index++)
	{
		size_t length = strlen(sets[index]);

		if (strncmp(row->key, sets[index], length) || !strcmp(row->key + length, "on"))
			continue;
		snprintf(key, sizeof(key), "%son", sets[index]);
		return ae_crosshair_edit_get(screen.player, key, value, sizeof(value)) && !strcmp(value, "false");
	}
	return 0;
}

static unsigned long rgb_of(const struct ae_crosshair *xh)
{
	return ((unsigned long)xh->color[0] << 16) | ((unsigned long)xh->color[1] << 8) | xh->color[2];
}

/* ---------- the strip */

static void top_build(void)
{
	struct ae_crosshair xh;
	int slot, saved = ae_crosshair_edit_saved(screen.player), editing = ae_crosshair_edit_slot(screen.player);
	int editing_saved = ae_crosshair_edit_slot_saved(screen.player, editing, &xh);

	screen.top_count = 0;
	for (slot = 0; slot < AE_XH_SLOTS; slot++)
	{
		/* (the edited slot not saved yet, a fresh profile's first: shown too, so the stage has its tile) */
		if (ae_crosshair_edit_slot_saved(screen.player, slot, &xh) || (slot == editing && !editing_saved))
		{
			screen.top[screen.top_count].kind = TOP_SLOT;
			screen.top[screen.top_count].slot = (short)slot;
			screen.top[screen.top_count++].disabled = 0;
		}
	}
	if (saved < AE_XH_SLOTS && (editing_saved || saved + 1 < AE_XH_SLOTS))
	{
		screen.top[screen.top_count].kind = TOP_NEW;
		screen.top[screen.top_count].slot = -1;
		screen.top[screen.top_count++].disabled = 0;
	}
	for (slot = TOP_RENAME; slot <= TOP_DELETE; slot++)
	{
		screen.top[screen.top_count].kind = (short)slot;
		screen.top[screen.top_count].slot = -1;
		screen.top[screen.top_count].disabled = (slot == TOP_DUPLICATE && (!editing_saved || saved >= AE_XH_SLOTS)) ||
			(slot == TOP_DELETE && !editing_saved);
		screen.top_count++;
	}
	if (screen.top_focus >= screen.top_count)
		screen.top_focus = (short)(screen.top_count - 1);
	if (screen.top_focus < 0)
		screen.top_focus = 0;
}

static const char *action_name(short kind)
{
	switch (kind)
	{
	case TOP_NEW: return "NEW";
	case TOP_RENAME: return "RENAME";
	case TOP_DUPLICATE: return "DUPLICATE";
	case TOP_SHARE: return "SHARE CODE";
	case TOP_DELETE: return "DELETE";
	default: return "";
	}
}

/* ---------- a tab's items and their places */

static short item_add(struct tab_layout *layout, short kind, short value, short block)
{
	struct item *item;

	if (layout->item_count >= ITEMS)
		return -1;
	item = &layout->items[layout->item_count];
	memset(item, 0, sizeof(*item));
	item->kind = kind;
	item->value = value;
	item->block = block;
	return layout->item_count++;
}

static void label_add(struct tab_layout *layout, const char *text, float x, float y, float width, float height)
{
	if (layout->label_count >= LABELS)
		return;
	layout->labels[layout->label_count].text = text;
	layout->labels[layout->label_count].rect.x = x;
	layout->labels[layout->label_count].rect.y = y;
	layout->labels[layout->label_count].rect.width = width;
	layout->labels[layout->label_count].rect.height = height;
	layout->label_count++;
}

/* a block of rows (settings row keys, or the colour's own: kind and value given) in columns, from y; returns its bottom.
sections: a label over each column's rows when the block has one column and two sections (the 4:3 layout's) */
static float rows_block(struct tab_layout *layout, const short *kinds, const short *values, short count, float y, float width,
	int columns, const char *const *sections, short section_split)
{
	struct block *block = &layout->blocks[layout->block_count];
	float row = ae_size_row(&screen.density), gap = gu(screen.density.metrics->gap), column_width, bottom = y;
	short index, per_column;

	memset(block, 0, sizeof(*block));
	block->first = layout->item_count;
	block->columns = (short)columns;
	per_column = (short)((count + columns - 1) / columns);
	column_width = (width - (float)(columns - 1) * gu(ROWS_GAP_U)) / (float)columns;
	/* (two columns of uneven sections: the split is the first column's length) */
	if (columns == 2 && section_split > 0)
		per_column = section_split;
	for (index = 0; index < count; index++)
	{
		short column = columns > 1 ? (short)(index / per_column) : 0, place = columns > 1 ? (short)(index % per_column) : index;
		float x = (float)column * (column_width + gu(ROWS_GAP_U)), top = y + (float)place * (row + gap);
		short item;

		if (columns == 1 && sections && section_split > 0 && index >= section_split)
			top += gu(SECTION_U) + gu(BLOCK_GAP_U);
		if (columns == 1 && sections)
			top += gu(SECTION_U);
		item = item_add(layout, kinds[index], values[index], layout->block_count);
		if (item < 0)
			break;
		layout->items[item].rect.x = x;
		layout->items[item].rect.y = top;
		layout->items[item].rect.width = column_width;
		layout->items[item].rect.height = row;
		if (top + row > bottom)
			bottom = top + row;
	}
	if (columns == 1 && sections)
	{
		label_add(layout, sections[0], 0.0f, y, width, gu(SECTION_U));
		if (section_split > 0 && section_split < count)
			label_add(layout, sections[1], 0.0f, y + (float)section_split * (row + gap) + gu(SECTION_U) + gu(BLOCK_GAP_U), width,
				gu(SECTION_U));
	}
	block->count = (short)(layout->item_count - block->first);
	block->columns = columns > 1 ? (short)per_column : 0;
	layout->block_count++;
	return bottom;
}

/* a grid block: the entries (headers among them), tiles about tile_u wide, tall_u tall (0: GRID_TALL of their width) */
static float grid_block(struct tab_layout *layout, const char *const *values, const short *kinds, const short *item_values,
	short count, float y, float width, float tile_u, float tall_u)
{
	struct block *block = &layout->blocks[layout->block_count];
	float gap = gu(GRID_GAP_U), tile_width, tile_height, header = gu(GRID_HEADER_U), top = y, bottom = y;
	short columns, index, last_row = -1;

	memset(block, 0, sizeof(*block));
	block->first = layout->item_count;
	block->grid = 1;
	columns = (short)((width + gap) / (gu(tile_u) + gap));
	if (columns < 1)
		columns = 1;
	tile_width = (width - (float)(columns - 1) * gap) / (float)columns;
	tile_height = tall_u > 0.0f ? gu(tall_u) : tile_width * GRID_TALL;
	for (index = 0; index < count && index < AE_PICKER_GRID_MAXIMUM; index++)
		block->values[index] = values[index];
	ae_picker_grid_layout(block->values, count, columns, &block->layout);
	for (index = 0; index < count && index < AE_PICKER_GRID_MAXIMUM; index++)
	{
		if (block->layout.row_of[index] < 0)
		{
			if (last_row >= 0)
				top += tile_height + gu(GRID_GROUP_GAP_U);
			last_row = -1;
			block->header_rect[index].x = 0.0f;
			block->header_rect[index].y = top;
			block->header_rect[index].width = width;
			block->header_rect[index].height = header;
			block->item_of[index] = -1;
			top += header;
			continue;
		}
		if (block->layout.row_of[index] != last_row)
		{
			if (last_row >= 0)
				top += tile_height + gap;
			last_row = block->layout.row_of[index];
		}
		{
			short item = item_add(layout, kinds[index], item_values[index], layout->block_count);

			block->item_of[index] = item;
			if (item < 0)
				continue;
			block->entry_of[item - block->first] = index;
			layout->items[item].rect.x = (float)block->layout.column_of[index] * (tile_width + gap);
			layout->items[item].rect.y = top;
			layout->items[item].rect.width = tile_width;
			layout->items[item].rect.height = tile_height;
			bottom = top + tile_height;
		}
	}
	block->count = (short)(layout->item_count - block->first);
	layout->block_count++;
	return bottom;
}

static void layout_build(short tab, float width, struct tab_layout *layout)
{
	short kinds[AE_PICKER_GRID_MAXIMUM], values[AE_PICKER_GRID_MAXIMUM];
	static const char *grid_values[AE_PICKER_GRID_MAXIMUM];
	static char shape_names[AE_PICKER_GRID_MAXIMUM][24];
	int two = width >= gu(TWO_COLUMNS_U);
	float y;
	short count = 0, index;

	memset(layout, 0, sizeof(*layout));
	switch (tab)
	{
	case TAB_SHAPE:
	{
		int family, shape;

		kinds[0] = kinds[1] = ITEM_ROW;
		values[0] = ae_settings_row_of("xh.centre");
		values[1] = ae_settings_row_of("xh.centre_size");
		y = rows_block(layout, kinds, values, 2, 0.0f, width, two ? 2 : 1, NULL, 0) + gu(BLOCK_GAP_U);
		for (family = 0; family < 4; family++)
		{
			snprintf(screen.shape_headers[family], sizeof(screen.shape_headers[family]), "%c%s", AE_PICKER_HEADER,
				ae_xh_family_name(family));
			grid_values[count] = screen.shape_headers[family];
			kinds[count] = ITEM_SHAPE;
			values[count++] = -1;
			for (shape = 1; shape <= ae_xh_shape_count() && count < AE_PICKER_GRID_MAXIMUM; shape++)
			{
				if (ae_xh_shape_family(shape) != family)
					continue;
				snprintf(shape_names[count], sizeof(shape_names[count]), "%s", ae_xh_shape_name(shape));
				grid_values[count] = shape_names[count];
				kinds[count] = ITEM_SHAPE;
				values[count++] = (short)shape;
			}
		}
		layout->height = grid_block(layout, grid_values, kinds, values, count, y, width, GRID_TILE_U, 0.0f);
		break;
	}
	case TAB_COLOR:
	{
		static char color_names[16][24];

		for (index = 0; index < ae_xh_palette_count() && count < 15; index++)
		{
			snprintf(color_names[count], sizeof(color_names[count]), "%s", ae_xh_palette_name(index));
			grid_values[count] = color_names[count];
			kinds[count] = ITEM_COLOR;
			values[count++] = index;
		}
		snprintf(color_names[count], sizeof(color_names[count]), "CUSTOM");
		grid_values[count] = color_names[count];
		kinds[count] = ITEM_COLOR;
		values[count++] = -1;
		y = grid_block(layout, grid_values, kinds, values, count, 0.0f, width, COLOR_TILE_U, COLOR_TILE_TALL_U) + gu(BLOCK_GAP_U);
		kinds[0] = ITEM_HEX;
		values[0] = ae_settings_row_of("action.crosshair_hex");
		for (index = 0; index < 3; index++)
		{
			kinds[1 + index] = ITEM_RGB;
			values[1 + index] = index;
		}
		kinds[4] = ITEM_ROW;
		values[4] = ae_settings_row_of("xh.opacity");
		layout->height = rows_block(layout, kinds, values, 5, y, width, 1, NULL, 0);
		break;
	}
	case TAB_LINES:
	case TAB_FRAME:
	case TAB_MOTION:
	{
		static const char *const lines_keys[] = { "xh.inner_on", "xh.inner_length", "xh.inner_thickness", "xh.inner_offset",
			"xh.inner_opacity", "xh.outer_on", "xh.outer_length", "xh.outer_thickness", "xh.outer_offset", "xh.outer_opacity" };
		static const char *const frame_keys[] = { "xh.frame_on", "xh.frame_size", "xh.frame_thickness", "xh.frame_opacity",
			"xh.outline_on", "xh.outline_thickness", "xh.outline_opacity" };
		static const char *const motion_keys[] = { "xh.movement", "xh.firing" };
		static const char *const lines_sections[] = { "INNER LINES", "OUTER LINES" };
		static const char *const frame_sections[] = { "FRAME", "OUTLINE" };
		const char *const *keys = tab == TAB_LINES ? lines_keys : tab == TAB_FRAME ? frame_keys : motion_keys;
		short split = tab == TAB_LINES ? 5 : tab == TAB_FRAME ? 4 : 0;

		count = tab == TAB_LINES ? 10 : tab == TAB_FRAME ? 7 : 2;
		for (index = 0; index < count; index++)
		{
			kinds[index] = ITEM_ROW;
			values[index] = ae_settings_row_of(keys[index]);
		}
		layout->height = rows_block(layout, kinds, values, count, 0.0f, width, two && split ? 2 : 1,
			!two && split ? (tab == TAB_LINES ? lines_sections : frame_sections) : NULL, split);
		break;
	}
	default:
		break;
	}
}

static struct item const *focused_item(void)
{
	return screen.region == REGION_PANEL && screen.item >= 0 && screen.item < screen.layout.item_count ?
		&screen.layout.items[screen.item] : NULL;
}

static struct ae_settings_row const *item_row(struct item const *item)
{
	return item && (item->kind == ITEM_ROW || item->kind == ITEM_HEX) && item->value >= 0 ? ae_settings_row(item->value) : NULL;
}

/* the crosshair the stage shows: the edited one, with the focused shape or colour on it before it is picked */
static void preview_of(struct ae_crosshair *xh)
{
	struct item const *item = focused_item();

	current(xh);
	if (!item)
		return;
	if (item->kind == ITEM_SHAPE && item->value > 0 && item->value != xh->shape)
		ae_xh_apply_shape(xh, item->value);
	else if (item->kind == ITEM_COLOR && item->value >= 0)
		ae_xh_color_set(xh, ae_xh_palette_rgb(item->value));
}

/* the item of the tab that is the crosshair's own value (its shape's tile, its colour's), else -1 */
static short own_item(void)
{
	struct ae_crosshair xh;
	short index;
	int palette;

	current(&xh);
	palette = ae_xh_palette_of(&xh);
	for (index = 0; index < screen.layout.item_count; index++)
	{
		struct item const *item = &screen.layout.items[index];

		if ((item->kind == ITEM_SHAPE && item->value > 0 && item->value == xh.shape) ||
			(item->kind == ITEM_COLOR && item->value == palette))
			return index;
	}
	return -1;
}

/* a focus that shows a look not picked yet (a shape's or a colour's tile) */
static int previewing(void)
{
	struct item const *item = focused_item();
	short own;

	if (!item || (item->kind != ITEM_SHAPE && item->kind != ITEM_COLOR))
		return 0;
	own = own_item();
	return own >= 0 && own != screen.item && !(item->kind == ITEM_COLOR && item->value < 0);
}

/* ---------- logging what the focus is on (the tests read it) */

static void focus_name(char *text, int size)
{
	struct item const *item = focused_item();

	text[0] = 0;
	if (screen.region == REGION_TOP)
	{
		struct top_entry const *entry = &screen.top[screen.top_focus];
		struct ae_crosshair xh;

		if (entry->kind == TOP_SLOT)
		{
			if (!ae_crosshair_edit_slot_saved(screen.player, entry->slot, &xh))
				current(&xh);
			snprintf(text, (size_t)size, "slot %d %s", entry->slot + 1, xh.name);
		}
		else
			snprintf(text, (size_t)size, "%s", action_name(entry->kind));
		return;
	}
	if (!item)
		return;
	switch (item->kind)
	{
	case ITEM_ROW:
	case ITEM_HEX:
		snprintf(text, (size_t)size, "%s", item_row(item) ? item_row(item)->label : "?");
		break;
	case ITEM_SHAPE:
		snprintf(text, (size_t)size, "shape %s", ae_xh_shape_name(item->value));
		break;
	case ITEM_COLOR:
		snprintf(text, (size_t)size, "color %s", item->value >= 0 ? ae_xh_palette_name(item->value) : "CUSTOM");
		break;
	case ITEM_RGB:
		snprintf(text, (size_t)size, "%s", channel_names[item->value]);
		break;
	default:
		break;
	}
}

static void focus_log(void)
{
	char name[64];

	if (screen.tab != screen.logged_tab)
	{
		screen.logged_tab = screen.tab;
		platform_log("ae crosshair screen: tab %s", tab_names[screen.tab]);
	}
	focus_name(name, sizeof(name));
	if (strcmp(name, screen.logged_focus))
	{
		snprintf(screen.logged_focus, sizeof(screen.logged_focus), "%s", name);
		if (name[0])
			platform_log("ae crosshair screen: focus %s", name);
	}
}

/* ---------- values */

static void value_put(struct ae_settings_row const *row, const char *value)
{
	if (!ae_crosshair_edit_set(screen.player, row->key, value))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		platform_log("ae crosshair screen: %s %s not set", row->key, value);
		return;
	}
	platform_log("ae crosshair screen: set %s %s", row->key, value);
}

static void row_step(struct ae_settings_row const *row, short direction)
{
	char now[VALUE_TEXT];
	short index, next, count = ae_settings_value_count(row);

	row_value(row, now, sizeof(now));
	index = ae_settings_value_index(row, now);
	if (index < 0)
		index = 0;
	next = ae_value_step(count, index, direction);
	if (next == index)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	value_put(row, ae_settings_value_of(row, next));
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

/* RED / GREEN / BLUE: a channel of the colour set (0 to 255) */
static int channel_set(short channel, int next)
{
	struct ae_crosshair xh;
	char hex[16];

	current(&xh);
	next = next < 0 ? 0 : next > 255 ? 255 : next;
	if (next == xh.color[channel])
		return 0;
	xh.color[channel] = (unsigned char)next;
	ae_xh_hex_format(rgb_of(&xh), hex, sizeof(hex));
	if (ae_crosshair_edit_set(screen.player, "xh.color_hex", hex))
		platform_log("ae crosshair screen: set xh.color_hex %s (%s %d)", hex, channel_names[channel], next);
	return 1;
}

/* a channel's slider: 8 a step from the pad and the keyboard (to 255 at the top), the pointer's place on the track */
static void channel_step(short channel, short direction)
{
	struct ae_crosshair xh;
	int value, next;

	current(&xh);
	value = xh.color[channel];
	if (direction > 0)
		next = value >= 255 ? 255 : value + RGB_STEP > 255 ? 255 : (value / RGB_STEP + 1) * RGB_STEP;
	else
		next = value <= 0 ? 0 : value == 255 ? 248 : ((value - 1) / RGB_STEP) * RGB_STEP;
	if (!channel_set(channel, next))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

/* the pointer on a channel's track: its place, 0 to 255 */
static void channel_at(short channel, struct ae_hit const *hit, float pointer_x)
{
	float fraction;

	if (hit->rect.width <= 0.0f)
		return;
	fraction = (pointer_x - hit->rect.x) / hit->rect.width;
	fraction = fraction < 0.0f ? 0.0f : fraction > 1.0f ? 1.0f : fraction;
	if (channel_set(channel, (int)(fraction * 255.0f + 0.5f)))
		ae_sound_request(AE_SOUND_CURSOR, 1);
}

static void shape_pick(short shape)
{
	if (shape > 0 && ae_crosshair_edit_shape_set(screen.player, shape))
	{
		platform_log("ae crosshair screen: shape %s", ae_xh_shape_name(shape));
		ae_sound_request(AE_SOUND_FORWARD, 0);
	}
	else
		ae_sound_request(AE_SOUND_FAILURE, 0);
}

static void color_pick(short palette)
{
	char value[8];

	snprintf(value, sizeof(value), "%d", palette);
	if (ae_crosshair_edit_set(screen.player, "xh.color", value))
	{
		platform_log("ae crosshair screen: color %s", ae_xh_palette_name(palette));
		ae_sound_request(AE_SOUND_FORWARD, 0);
	}
	else
		ae_sound_request(AE_SOUND_FAILURE, 0);
}

/* ---------- typing: RENAME's name, CUSTOM COLOR's hex */

enum { HEX_TYPED = 7 };

static int hex_typing_ok(const char *text)
{
	int index;

	for (index = 0; text[index]; index++)
	{
		char c = text[index];

		if (index >= HEX_TYPED || !((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') ||
			(c == '#' && index == 0)))
			return 0;
	}
	return 1;
}

static void typing_finish(int keep)
{
	int kind = screen.typing;

	screen.typing = TYPING_NONE;
	screen.keyboard_open = 0;
	if (!keep)
	{
		platform_log("ae crosshair screen: typing dropped");
		return;
	}
	if (kind == TYPING_HEX)
	{
		if (ae_crosshair_edit_set(screen.player, "xh.color_hex", screen.typed.text))
			platform_log("ae crosshair screen: custom color %s", screen.typed.text);
		else
		{
			platform_log("ae crosshair screen: custom color '%s' is not #RRGGBB", screen.typed.text);
			ae_sound_request(AE_SOUND_FAILURE, 0);
		}
	}
	else if (kind == TYPING_NAME)
	{
		if (ae_crosshair_edit_name_set(screen.player, screen.typed.text))
			platform_log("ae crosshair screen: named '%s'", screen.typed.text);
		else
			ae_sound_request(AE_SOUND_FAILURE, 0);
	}
}

static void typing_done(int keep, void *context)
{
	(void)context;
	typing_finish(keep);
}

static void typing_end(int keep)
{
	if (screen.typing == TYPING_NONE)
		return;
	if (screen.typed_edit.active)
		ae_field_edit_end(&screen.typed_edit, keep);
	typing_finish(keep);
	ae_sound_request(keep ? AE_SOUND_FORWARD : AE_SOUND_BACK, 0);
}

/* AE's keyboard closed: kept or dropped, unless a key on a physical keyboard took over the field */
static void keyboard_done(int keep, void *context)
{
	(void)context;
	screen.keyboard_open = 0;
	if (screen.typing == TYPING_NONE)
		return;
	if (screen.typed_edit.active && screen.typed_edit.text == &screen.typed)
	{
		screen.caret_since = ae_motion_now();
		return;
	}
	typing_finish(keep);
}

/* typing begins: on the keyboard in place (the text all selected); a pad's A opens AE's keyboard over the field */
static void typing_begin(int kind, int from_pad)
{
	struct ae_crosshair xh;
	char now[32];

	if (screen.typing != TYPING_NONE)
		return;
	current(&xh);
	if (kind == TYPING_HEX)
		ae_xh_hex_format(rgb_of(&xh), now, sizeof(now));
	else
		snprintf(now, sizeof(now), "%s", xh.name);
	ae_text_init(&screen.typed, now, kind == TYPING_HEX ? HEX_TYPED : AE_XH_NAME, 0);
	screen.typed_edit.done = typing_done;
	screen.typed_edit.context = NULL;
	screen.typed_edit.holder = &screen;
	screen.typing = kind;
	screen.caret_since = ae_motion_now();
	if (from_pad)
	{
		struct ae_keyboard_spec spec;

		memset(&spec, 0, sizeof(spec));
		spec.text = &screen.typed;
		spec.field = screen.field_rect;
		spec.bounds = screen.frame.rect;
		spec.density = screen.density;
		spec.done = keyboard_done;
		spec.edit = &screen.typed_edit;
		if (!ae_keyboard_open(&spec, AE_OWNER_PARENT))
		{
			screen.typing = TYPING_NONE;
			ae_sound_request(AE_SOUND_FAILURE, 0);
			return;
		}
		screen.keyboard_open = 1;
	}
	else
	{
		ae_field_edit_begin(&screen.typed_edit, &screen.typed);
		ae_text_select_all(&screen.typed);
	}
	ae_sound_request(AE_SOUND_FORWARD, 0);
	platform_log("ae crosshair screen: typing %s from '%s'", kind == TYPING_HEX ? "CUSTOM COLOR" : "NAME", now);
}

/* ---------- the dialogs */

static void dialog_picked(short choice, void *context);

static void dialog_open(int dialog, const char *const *choices, short count, short safe, short cancel, unsigned int destructive)
{
	struct ae_dialog_spec spec;
	short index;

	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = screen.dialog_title;
	spec.body = screen.dialog_body;
	for (index = 0; index < count && index < AE_DIALOG_CHOICES; index++)
		spec.choices[index] = choices[index];
	spec.choice_count = count;
	spec.safe_choice = safe;
	spec.cancel_choice = cancel;
	spec.has_cancel = 1;
	spec.destructive_choices = destructive;
	spec.density = screen.density;
	spec.bounds = screen.frame.rect;
	spec.picked = dialog_picked;
	if (ae_dialog_open(&spec, AE_OWNER_PARENT))
		screen.dialog = dialog;
	else
		ae_sound_request(AE_SOUND_FAILURE, 0);
}

static void note_open(const char *title, const char *body)
{
	static const char *const ok[] = { "OK" };

	snprintf(screen.dialog_title, sizeof(screen.dialog_title), "%s", title);
	snprintf(screen.dialog_body, sizeof(screen.dialog_body), "%s", body);
	dialog_open(DIALOG_NOTE, ok, 1, 0, 0, 0);
}

static void share_open(void)
{
	static const char *const choices[] = { "COPY", "PASTE A CODE", "CLOSE" };
	char code[AE_XH_CODE_TEXT];
	struct ae_crosshair xh;

	current(&xh);
	ae_crosshair_edit_code(screen.player, code, sizeof(code));
	snprintf(screen.dialog_title, sizeof(screen.dialog_title), "Share code: %s", xh.name);
	snprintf(screen.dialog_body, sizeof(screen.dialog_body), "%s  Copy this to give %s to a friend, or paste a code a friend "
		"gave you. A code only ever sets how a crosshair looks.", code, xh.name);
	dialog_open(DIALOG_SHARE, choices, 3, 0, 2, 0);
	platform_log("ae crosshair screen: share code %s", code);
}

static void pasted_open(void)
{
	static const char *const choices[] = { "SAVE AS NEW", "APPLY", "CANCEL" };
	struct ae_crosshair xh;
	char line[160];

	if (screen.pasted_result != AE_XH_CODE_OK)
	{
		char body[160];

		snprintf(body, sizeof(body), "%s. Nothing changed.", ae_xh_code_why(screen.pasted_result));
		note_open("That code did not work", body);
		return;
	}
	current(&xh);
	ae_crosshair_edit_summary(&screen.pasted, line, sizeof(line));
	snprintf(screen.dialog_title, sizeof(screen.dialog_title), "Use this crosshair?");
	snprintf(screen.dialog_body, sizeof(screen.dialog_body), "%s%s. SAVE AS NEW keeps %s and adds this one; APPLY puts it in %s's "
		"place.", line, screen.pasted_unknown ? " (a newer shape: shown without one)" : "", xh.name, xh.name);
	dialog_open(DIALOG_PASTED, choices, 3, 0, 2, 0);
}

/* DELETE asks first, CANCEL focused, and says what falls back: the main crosshair to each weapon's own reticle, the weapons
that name it to the main one */
static void delete_open(void)
{
	static const char *const choices[] = { "CANCEL", "DELETE" };
	char keys[AE_XH_WEAPON_ENTRIES][AE_XH_WEAPON_KEY], first[48], row_key[48];
	struct ae_crosshair xh;
	int slot = ae_crosshair_edit_slot(screen.player), weapons, is_main;
	size_t at;

	if (!ae_crosshair_edit_slot_saved(screen.player, slot, &xh))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	is_main = ae_crosshair_edit_main(screen.player) == slot;
	weapons = ae_crosshair_edit_weapons_using(screen.player, slot, keys, AE_XH_WEAPON_ENTRIES);
	first[0] = 0;
	if (weapons > 0)
	{
		struct ae_settings_row const *row;

		snprintf(row_key, sizeof(row_key), "xw.%s", keys[0]);
		row = row_of(row_key);
		snprintf(first, sizeof(first), "%s", row ? row->label : keys[0]);
	}
	snprintf(screen.dialog_title, sizeof(screen.dialog_title), "Delete %s?", xh.name);
	at = (size_t)snprintf(screen.dialog_body, sizeof(screen.dialog_body), "It leaves your saved crosshairs.");
	if (is_main && at < sizeof(screen.dialog_body))
		at += (size_t)snprintf(screen.dialog_body + at, sizeof(screen.dialog_body) - at, " It is your main crosshair: "
			"every weapon goes back to its own reticle.");
	if (weapons == 1 && at < sizeof(screen.dialog_body))
		at += (size_t)snprintf(screen.dialog_body + at, sizeof(screen.dialog_body) - at, " The %s uses it: it goes back to "
			"the main crosshair.", first);
	else if (weapons > 1 && at < sizeof(screen.dialog_body))
		at += (size_t)snprintf(screen.dialog_body + at, sizeof(screen.dialog_body) - at, " The %s and %d more use it: they "
			"go back to the main crosshair.", first, weapons - 1);
	if (!is_main && !weapons && at < sizeof(screen.dialog_body))
		snprintf(screen.dialog_body + at, sizeof(screen.dialog_body) - at, " Nothing uses it now.");
	dialog_open(DIALOG_DELETE, choices, 2, 0, 0, 1u << 1);
}

static int keep_ask(void)
{
	static const char *const choices[] = { "KEEP CHANGES", "UNDO CHANGES", "KEEP EDITING" };

	if (!changed())
		return 0;
	snprintf(screen.dialog_title, sizeof(screen.dialog_title), "Keep your changes?");
	snprintf(screen.dialog_body, sizeof(screen.dialog_body), "Your crosshairs changed for %s. They are in use already. UNDO "
		"CHANGES puts every crosshair back as it was when you opened this.", screen.player_name);
	dialog_open(DIALOG_KEEP, choices, 3, 0, 2, 0);
	return 1;
}

static void dialog_picked(short choice, void *context)
{
	int dialog = screen.dialog;

	(void)context;
	screen.dialog = DIALOG_NONE;
	switch (dialog)
	{
	case DIALOG_KEEP:
		if (choice == 0)
		{
			ae_player_settings_flush();
			platform_log("ae crosshair screen: keep");
			ae_ui_pop();
		}
		else if (choice == 1)
		{
			ae_crosshair_edit_profile_restore(screen.player, &screen.opened);
			ae_crosshair_edit_select(screen.player, screen.opened_slot);
			platform_log("ae crosshair screen: undo");
			ae_ui_pop();
		}
		break;
	case DIALOG_SHARE:
		if (choice == 0)
		{
			char code[AE_XH_CODE_TEXT];

			ae_crosshair_edit_code(screen.player, code, sizeof(code));
			ae_host_clipboard_set(code);
			platform_log("ae crosshair: code copied %s", code);
			ae_sound_request(AE_SOUND_FORWARD, 0);
		}
		else if (choice == 1)
		{
			char clip[256];

			clip[0] = 0;
			ae_host_clipboard_get(clip, sizeof(clip));
			screen.pasted_result = ae_crosshair_edit_code_read(clip, &screen.pasted, &screen.pasted_unknown);
			platform_log("ae crosshair: pasted code %s", screen.pasted_result == AE_XH_CODE_OK ? "read" :
				ae_xh_code_why(screen.pasted_result));
			screen.then_dialog = DIALOG_PASTED;
		}
		break;
	case DIALOG_PASTED:
		if (choice == 0)
		{
			int slot = ae_crosshair_edit_code_save_new(screen.player, &screen.pasted);

			platform_log("ae crosshair: pasted code saved as slot %d", slot + 1);
			if (slot < 0)
				screen.then_dialog = DIALOG_NOTE;
		}
		else if (choice == 1)
		{
			ae_crosshair_edit_code_apply(screen.player, &screen.pasted);
			platform_log("ae crosshair: pasted code applied");
		}
		break;
	case DIALOG_DELETE:
		if (choice == 1)
		{
			ae_crosshair_edit_delete(screen.player);
			platform_log("ae crosshair screen: deleted");
			ae_sound_request(AE_SOUND_FORWARD, 0);
			/* (none saved any more: the screen goes, so a later edit starts a new one on purpose) */
			if (!ae_crosshair_edit_saved(screen.player))
			{
				ae_player_settings_flush();
				ae_ui_pop();
			}
		}
		break;
	default:
		break;
	}
}

/* ---------- the panel's focus */

static void tab_set(short tab)
{
	if (tab < 0 || tab >= TABS || tab == screen.tab)
		return;
	screen.tab = tab;
	screen.scroll = 0.0f;
	screen.layout_logged = 0;
	/* (before the panel's first draw its width is not known: the draw makes the layout) */
	if (screen.panel_width > 0.0f)
		layout_build(screen.tab, screen.panel_width, &screen.layout);
	else
		screen.layout.item_count = 0;
	screen.item = 0;
	screen.scroll_to_focus = 1;
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

static void item_set(short item)
{
	if (item < 0 || item >= screen.layout.item_count)
		return;
	if (item != screen.item || screen.region != REGION_PANEL)
		ae_sound_request(AE_SOUND_CURSOR, 0);
	screen.region = REGION_PANEL;
	screen.item = item;
	screen.scroll_to_focus = 1;
}

/* up / down: a rows block's next (down its column, then the next column), a grid's row above or below; past a block's end
the next block; up from the tab's first: the strip */
static void move_vertical(short direction)
{
	struct item const *item = focused_item();
	struct block const *block;
	short in_block, next = -1;

	if (!item)
		return;
	block = &screen.layout.blocks[item->block];
	in_block = (short)(screen.item - block->first);
	if (block->grid)
	{
		short entry = block->entry_of[in_block];
		short moved = ae_picker_grid_move(&block->layout, block->values, entry, 0, direction, 0, 0);

		if (moved != entry && moved >= 0 && block->item_of[moved] >= 0)
			next = block->item_of[moved];
	}
	else if (in_block + direction >= 0 && in_block + direction < block->count)
		next = (short)(screen.item + direction);
	if (next < 0)
	{
		if (direction < 0 && item->block > 0)
			next = (short)(screen.layout.blocks[item->block - 1].first + screen.layout.blocks[item->block - 1].count - 1);
		else if (direction > 0 && item->block + 1 < screen.layout.block_count)
			next = screen.layout.blocks[item->block + 1].first;
		/* (up from a grid's first row: the block above's first item, as a list's up) */
		if (direction < 0 && item->block > 0 && screen.layout.blocks[item->block - 1].grid == 0)
			next = screen.layout.blocks[item->block - 1].first;
	}
	if (next >= 0)
	{
		item_set(next);
		return;
	}
	if (direction < 0)
	{
		screen.region = REGION_TOP;
		ae_sound_request(AE_SOUND_CURSOR, 0);
	}
}

/* LT / RT: on the shapes, a family's first; on two columns of rows, the other column's row at the same place */
static void move_page(short direction)
{
	struct item const *item = focused_item();
	struct block const *block;
	short in_block;

	if (!item)
		return;
	block = &screen.layout.blocks[item->block];
	in_block = (short)(screen.item - block->first);
	if (block->grid)
	{
		short entry = block->entry_of[in_block];
		short moved = ae_picker_grid_move(&block->layout, block->values, entry, 0, 0, direction, 0);

		if (moved != entry && moved >= 0 && block->item_of[moved] >= 0)
			item_set(block->item_of[moved]);
		return;
	}
	if (block->columns > 0)
	{
		short target = (short)(in_block + direction * block->columns);

		if (target >= 0 && target < block->count)
			item_set((short)(block->first + target));
		else if (direction > 0 && in_block < block->columns && block->columns < block->count)
			item_set((short)(block->count - 1 + block->first));
		return;
	}
	/* (one column of rows: a page of PAGE_ROWS rows; at the block's end, the next block's first row, or the one before's last) */
	{
		short target = (short)(in_block + direction * PAGE_ROWS);

		if (target >= 0 && target < block->count)
			item_set((short)(block->first + target));
		else if (direction > 0 && in_block < block->count - 1)
			item_set((short)(block->first + block->count - 1));
		else if (direction > 0 && item->block + 1 < screen.layout.block_count)
			item_set(screen.layout.blocks[item->block + 1].first);
		else if (direction < 0 && in_block > 0)
			item_set(block->first);
		else if (direction < 0 && item->block > 0)
			item_set((short)(screen.layout.blocks[item->block].first - 1));
		else
			ae_sound_request(AE_SOUND_FAILURE, 0);
	}
}

static void move_horizontal(short direction)
{
	struct item const *item = focused_item();

	if (!item)
		return;
	switch (item->kind)
	{
	case ITEM_ROW:
		row_step(item_row(item), direction);
		break;
	case ITEM_RGB:
		channel_step(item->value, direction);
		break;
	case ITEM_HEX:
		ae_sound_request(AE_SOUND_FAILURE, 0);
		break;
	case ITEM_SHAPE:
	case ITEM_COLOR:
	{
		struct block const *block = &screen.layout.blocks[item->block];
		short entry = block->entry_of[screen.item - block->first];
		short moved = ae_picker_grid_move(&block->layout, block->values, entry, direction, 0, 0, 0);

		if (moved != entry && moved >= 0 && block->item_of[moved] >= 0)
			item_set(block->item_of[moved]);
		break;
	}
	default:
		break;
	}
}

/* a row's ALL VALUES (A on a row, a click on it) */
static void all_values_picked(short index, void *context)
{
	struct ae_settings_row const *row = ae_settings_row((short)(intptr_t)context);

	if (row && index >= 0 && index < ae_settings_value_count(row))
		value_put(row, ae_settings_value_of(row, index));
}

static struct ae_rect item_frame_rect(struct item const *item)
{
	struct ae_rect rect = item->rect;

	rect.x += screen.panel_content.x;
	rect.y += screen.panel_content.y - screen.scroll;
	return rect;
}

static void all_values_open(struct ae_settings_row const *row, struct item const *item)
{
	struct ae_picker_spec spec;
	char now[VALUE_TEXT];

	if (!row || !row->count)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	row_value(row, now, sizeof(now));
	memset(&spec, 0, sizeof(spec));
	spec.values = row->texts;
	spec.count = ae_settings_value_count(row);
	spec.current = ae_settings_value_index(row, now);
	spec.row = item_frame_rect(item);
	spec.bounds = screen.frame.rect;
	spec.density = screen.density;
	spec.title = row->label;
	spec.picked = all_values_picked;
	spec.context = (void *)(intptr_t)(row - ae_settings_row(0));
	if (ae_picker_open(&spec, AE_OWNER_PARENT))
		ae_sound_request(AE_SOUND_FORWARD, 0);
}

/* A on the panel's focus */
static void panel_accept(int from_pad)
{
	struct item const *item = focused_item();

	if (!item)
		return;
	switch (item->kind)
	{
	case ITEM_ROW:
		all_values_open(item_row(item), item);
		break;
	case ITEM_HEX:
	case ITEM_RGB:
		screen.field_rect = item_frame_rect(item);
		typing_begin(TYPING_HEX, from_pad);
		break;
	case ITEM_SHAPE:
		shape_pick(item->value);
		break;
	case ITEM_COLOR:
		if (item->value >= 0)
			color_pick(item->value);
		else
		{
			/* (CUSTOM: the colour typed) */
			short index;

			for (index = 0; index < screen.layout.item_count; index++)
				if (screen.layout.items[index].kind == ITEM_HEX)
					item_set(index);
			screen.field_rect = item_frame_rect(focused_item());
			typing_begin(TYPING_HEX, from_pad);
		}
		break;
	default:
		break;
	}
}

/* B on a shape or colour not yet picked: the focus back on the one picked (it "keeps" it); else nothing done here */
static int keep_own(void)
{
	short own;

	if (!previewing())
		return 0;
	own = own_item();
	if (own < 0)
		return 0;
	item_set(own);
	platform_log("ae crosshair screen: kept the one picked");
	return 1;
}

/* A on the strip's focus */
static void top_accept(int from_pad)
{
	struct top_entry const *entry = &screen.top[screen.top_focus];
	short kind = entry->kind;
	int slot;

	if (entry->disabled)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	switch (entry->kind)
	{
	case TOP_SLOT:
		if (entry->slot != ae_crosshair_edit_slot(screen.player))
		{
			ae_crosshair_edit_select(screen.player, entry->slot);
			platform_log("ae crosshair screen: edit slot %d", entry->slot + 1);
		}
		ae_sound_request(AE_SOUND_FORWARD, 0);
		break;
	case TOP_NEW:
		slot = ae_crosshair_edit_new(screen.player);
		if (slot < 0)
		{
			ae_sound_request(AE_SOUND_FAILURE, 0);
			return;
		}
		platform_log("ae crosshair screen: new slot %d", slot + 1);
		/* (the design's call: a new crosshair made while the main one is STOCK becomes the main one, so it shows at once) */
		{
			char main_text[16], slot_text[8];

			if (ae_crosshair_edit_get(screen.player, "xh.main", main_text, sizeof(main_text)) && !strcmp(main_text, "own"))
			{
				snprintf(slot_text, sizeof(slot_text), "%d", slot + 1);
				if (ae_crosshair_edit_set(screen.player, "xh.main", slot_text))
					platform_log("ae crosshair screen: slot %d is the main crosshair now (it was STOCK)", slot + 1);
			}
		}
		ae_sound_request(AE_SOUND_FORWARD, 0);
		break;
	case TOP_DUPLICATE:
		slot = ae_crosshair_edit_duplicate(screen.player);
		if (slot < 0)
		{
			ae_sound_request(AE_SOUND_FAILURE, 0);
			return;
		}
		platform_log("ae crosshair screen: duplicate slot %d", slot + 1);
		ae_sound_request(AE_SOUND_FORWARD, 0);
		break;
	case TOP_RENAME:
		screen.field_rect = screen.actions_rect;
		typing_begin(TYPING_NAME, from_pad);
		break;
	case TOP_SHARE:
		share_open();
		ae_sound_request(AE_SOUND_FORWARD, 0);
		break;
	case TOP_DELETE:
		delete_open();
		break;
	default:
		break;
	}
	top_build();
	/* (the strip's focus follows the slot made or chosen) */
	if (kind == TOP_NEW || kind == TOP_DUPLICATE)
	{
		short index;

		for (index = 0; index < screen.top_count; index++)
			if (screen.top[index].kind == TOP_SLOT && screen.top[index].slot == ae_crosshair_edit_slot(screen.player))
				screen.top_focus = index;
	}
}

static void background_next(void)
{
	screen.background = (screen.background + 1) % ae_reticle_preview_backgrounds();
	platform_log("ae crosshair screen: preview background %s", ae_reticle_preview_background_name(screen.background));
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

/* ---------- drawing */

static float emblem(float x, float center_y, float size, float number_size, short player)
{
	char number[4];

	ae_draw_rect(x, center_y - size * 0.5f, size, size, size * 0.07f, player_colors[player % PLAYERS]);
	snprintf(number, sizeof(number), "%d", player + 1);
	ae_draw_text(AE_FONT_ROW, number_size, x + size * 0.5f, cap_top(AE_FONT_ROW, number_size, center_y), AE_ALIGN_CENTER,
		ae_emblem_number_color(player_colors[player % PLAYERS]), number);
	return size;
}

/* the title and the crumb: the player's emblem, their name, PLAYER n, EDITING <name> in the accent */
static void header_draw(void)
{
	struct ae_crosshair xh;
	float size = gu(TITLE_U), top_y = ae_frame_y(&screen.frame, TITLE_Y), middle, x = right_x(), crumb = gu(CRUMB_U);
	char player[16], editing[40];
	const char *parts[3];
	short index;

	current(&xh);
	middle = top_y + ae_draw_cap_height(AE_FONT_TITLE, size) * 0.5f;
	ae_draw_text(AE_FONT_TITLE, size, left_x(), top_y, AE_ALIGN_LEFT, AE_COLOR_TITLE, "CROSSHAIR");
	snprintf(player, sizeof(player), "PLAYER %d", screen.player + 1);
	snprintf(editing, sizeof(editing), "EDITING %s", xh.name);
	parts[0] = screen.player_name;
	parts[1] = player;
	parts[2] = editing;
	for (index = 2; index >= 0; index--)
	{
		x -= ae_draw_text_tracked(AE_FONT_BODY, crumb, CRUMB_TRACKING, x, cap_top(AE_FONT_BODY, crumb, middle), AE_ALIGN_RIGHT,
			index == 2 ? AE_COLOR_ACCENT : AE_COLOR_TEXT, parts[index]);
		if (index > 0)
		{
			x -= gu(CRUMB_GAP_U);
			ae_widget_separator(&screen.density, x, middle, CRUMB_DOT_U, AE_COLOR_MUTED);
			x -= gu(CRUMB_GAP_U);
		}
	}
	x -= gu(EMBLEM_GAP_U) + gu(EMBLEM_U);
	emblem(x, middle, gu(EMBLEM_U), gu(EMBLEM_NUMBER_U), screen.player);
	ae_draw_rect(left_x(), ae_frame_y(&screen.frame, RULE_Y), right_x() - left_x(), gu(RULE_U), 0.0f, AE_COLOR_RULE);
}

/* a crosshair's picture in a tile: its look alone, fitted to half from the middle, in ink (the tile's text colour) or its
own colour */
static void crosshair_art(const struct ae_crosshair *source, int shape, float center_x, float center_y, float half,
	unsigned long ink, int own_color)
{
	struct ae_crosshair xh = *source;
	struct ae_xh_motion rest = { 0.0f, 0.0f, 0.0f };
	float extent, scale = 1.5f * screen.frame.unit;

	if (shape > 0)
	{
		ae_xh_default(&xh);
		ae_xh_apply_shape(&xh, shape);
		xh.outline_on = 0;
		xh.outer.opacity = 100;
	}
	extent = ae_xh_extent(&xh);
	if (extent > 0.0f && extent * scale > half)
		scale = half / extent;
	ae_crosshair_paint(&xh, &rest, own_color ? rgb_of(&xh) : ink, center_x, center_y, scale, 1.0f, 1.0f);
}

/* an action of the strip: a square chip (the live look), its words tracked; SHARE CODE with Y's mark; DELETE in the strong
red ink; focused: the selection's fill */
static float action_width(short kind)
{
	float text = gu(ACTION_TEXT_U), width = ae_draw_text_tracked_width(AE_FONT_ROW, text, 0.08f, action_name(kind)) +
		2.0f * gu(ACTION_PAD_U);

	if (kind == TOP_SHARE)
		width += ae_widget_button_mark(&screen.density, AE_BUTTON_Y, 0.0f, 0.0f, gu(ACTION_GLYPH_U), 0, 0) + gu(10.0f);
	return width;
}

static void action_draw(short index, float x, float y)
{
	struct top_entry *entry = &screen.top[index];
	float width = action_width(entry->kind), height = gu(ACTION_HEIGHT_U), text = gu(ACTION_TEXT_U), center = y + height * 0.5f;
	float corner = gu(screen.density.metrics->corner), label_x = x + gu(ACTION_PAD_U);
	int focused = screen.region == REGION_TOP && screen.top_focus == index && screen.typing == TYPING_NONE;
	unsigned int ink = entry->disabled ? AE_COLOR_CHIP_DISABLED_TEXT : focused ? AE_COLOR_SELECTION_TEXT :
		entry->kind == TOP_DELETE ? AE_COLOR_DESTRUCTIVE_INK : AE_COLOR_TEXT;

	if (focused && !entry->disabled)
		ae_draw_rect(x, y, width, height, corner, AE_COLOR_SELECTION);
	else if (focused)
		ae_draw_rect(x, y, width, height, corner, AE_COLOR_DISABLED_BAR);
	else
	{
		ae_draw_rect(x, y, width, height, corner, AE_COLOR_ROW);
		ae_draw_outline(x, y, width, height, corner, gu(1.0f), entry->disabled ? AE_COLOR_CHIP_DISABLED :
			entry->kind == TOP_DELETE ? 0xC0392B80u : AE_COLOR_RULE);
	}
	if (entry->kind == TOP_SHARE)
		label_x += ae_widget_button_mark(&screen.density, AE_BUTTON_Y, label_x, center, gu(ACTION_GLYPH_U), focused, 1) +
			gu(10.0f);
	ae_draw_text_tracked(AE_FONT_ROW, text, 0.08f, label_x, cap_top(AE_FONT_ROW, text, center), AE_ALIGN_LEFT, ink,
		action_name(entry->kind));
	entry->rect.x = x;
	entry->rect.y = y;
	entry->rect.width = width;
	entry->rect.height = height;
	ae_hit_add(x, y, width, height, HIT_TOP, AE_PART_ITEM, index);
}

/* a saved crosshair's tile (or NEW): its look in its own colour over the name; the edited one ringed in the accent, the main
one's accent dot in the corner, the focus the selection's fill */
static void slot_tile_draw(short index, float x, float y, float width, float height)
{
	struct top_entry *entry = &screen.top[index];
	struct ae_crosshair xh;
	int focused = screen.region == REGION_TOP && screen.top_focus == index && screen.typing == TYPING_NONE;
	int edited = entry->kind == TOP_SLOT && entry->slot == ae_crosshair_edit_slot(screen.player);
	int saved = entry->kind == TOP_SLOT && ae_crosshair_edit_slot_saved(screen.player, entry->slot, &xh);
	float name_size = gu(STRIP_NAME_U), art = gu(STRIP_ART_U), label_band = gu(26.0f);
	unsigned int ink = focused ? AE_COLOR_SELECTION_TEXT : AE_COLOR_TEXT;
	char name[AE_XH_NAME + 1];

	if (entry->kind == TOP_SLOT && !saved)
		current(&xh);
	ae_draw_rect(x, y, width, height, 0.0f, focused ? AE_COLOR_SELECTION : AE_COLOR_ROW);
	if (focused)
		ae_draw_rect(x, y, gu(NOTCH_U), height, 0.0f, AE_COLOR_ACCENT);
	if (entry->kind == TOP_SLOT)
	{
		crosshair_art(&xh, 0, x + width * 0.5f, y + (height - label_band) * 0.5f + gu(4.0f), art * 0.5f,
			focused ? 0x0F171Eu : rgb_of(&xh), !focused);
		snprintf(name, sizeof(name), "%s", xh.name);
	}
	else
	{
		/* (NEW: a plus) */
		float cx = x + width * 0.5f, cy = y + (height - label_band) * 0.5f + gu(4.0f), arm = gu(14.0f), stroke = gu(4.0f);
		unsigned int plus = focused ? AE_COLOR_SELECTION_TEXT : AE_COLOR_TITLE;

		ae_draw_rect(cx - arm, cy - stroke * 0.5f, arm * 2.0f, stroke, 0.0f, plus);
		ae_draw_rect(cx - stroke * 0.5f, cy - arm, stroke, arm * 2.0f, 0.0f, plus);
		snprintf(name, sizeof(name), "NEW");
	}
	{
		float size = name_size, room = width - gu(10.0f), text_width = ae_draw_text_tracked_width(AE_FONT_ROW, size, 0.08f, name);

		if (text_width > room && text_width > 0.0f)
			size *= room / text_width;
		ae_draw_text_tracked(AE_FONT_ROW, size, 0.08f, x + width * 0.5f, cap_top(AE_FONT_ROW, size, y + height - label_band *
			0.5f - gu(2.0f)), AE_ALIGN_CENTER, entry->kind == TOP_SLOT && !saved ? (focused ? AE_COLOR_SELECTION_MUTED :
			AE_COLOR_MUTED) : ink, name);
	}
	if (edited)
		ae_draw_outline(x - gu(1.0f), y - gu(1.0f), width + gu(2.0f), height + gu(2.0f), 0.0f, gu(2.0f), AE_COLOR_ACCENT);
	if (entry->kind == TOP_SLOT && saved && ae_crosshair_edit_main(screen.player) == entry->slot)
		ae_widget_separator(&screen.density, x + width - gu(12.0f), y + gu(12.0f), 8.0f, AE_COLOR_ACCENT);
	entry->rect.x = x;
	entry->rect.y = y;
	entry->rect.width = width;
	entry->rect.height = height;
	ae_hit_add(x, y, width, height, HIT_TOP, AE_PART_ITEM, index);
}

/* a text being typed: the well, its 2 u accent ring, the text, its selection and the caret */
static void field_draw(float x, float y, float width, float height)
{
	float text = ae_size_text(&screen.density), cap = ae_draw_cap_height(AE_FONT_ROW, text), center = y + height * 0.5f;
	float text_x = x + gu(12.0f);
	short from, to;

	ae_draw_rect(x, y, width, height, 0.0f, AE_COLOR_WELL);
	ae_draw_outline(x, y, width, height, 0.0f, gu(2.0f), AE_COLOR_ACCENT);
	if (ae_text_selection(&screen.typed, &from, &to))
	{
		char part[AE_TEXT_MAXIMUM];
		float left, right;

		snprintf(part, sizeof(part), "%.*s", (int)from, screen.typed.text);
		left = part[0] ? ae_draw_text_width(AE_FONT_ROW, text, part) : 0.0f;
		snprintf(part, sizeof(part), "%.*s", (int)to, screen.typed.text);
		right = part[0] ? ae_draw_text_width(AE_FONT_ROW, text, part) : 0.0f;
		ae_draw_rect(text_x + left, center - height * 0.3f, right - left, height * 0.6f, 0.0f, AE_COLOR_TEXT_SELECT);
	}
	if (screen.typed.text[0])
		ae_draw_text(AE_FONT_ROW, text, text_x, center - cap * 0.5f, AE_ALIGN_LEFT, AE_COLOR_TITLE, screen.typed.text);
	if (ae_caret_visible(screen.caret_since))
	{
		char part[AE_TEXT_MAXIMUM];

		snprintf(part, sizeof(part), "%.*s", (int)screen.typed.caret, screen.typed.text);
		ae_draw_rect(text_x + (part[0] ? ae_draw_text_width(AE_FONT_ROW, text, part) : 0.0f), center - height * 0.3f, gu(2.0f),
			height * 0.6f, 0.0f, AE_COLOR_ACCENT);
	}
	ae_hit_add(x, y, width, height, HIT_FIELD, AE_PART_FIELD, 0);
}

/* the strip: SAVED n OF 8 and IN USE over the tiles, the actions after them (wrapping in the room left, a field for RENAME) */
static float strip_draw(float y)
{
	float label = gu(LABEL_U), tile_width = gu(STRIP_TILE_U), tile_height = gu(STRIP_TILE_TALL_U), gap = gu(STRIP_GAP_U);
	float x = left_x(), tiles_y = y + label + gu(LABEL_GAP_U), actions_x, room, row_x, row_y, action_height = gu(ACTION_HEIGHT_U);
	int saved = ae_crosshair_edit_saved(screen.player);
	char line[32];
	short index, actions = 0, slots = 0;

	top_build();
	snprintf(line, sizeof(line), saved ? "SAVED  \xE2\x80\xA2  %d OF 8" : "NONE SAVED YET", saved);
	ae_draw_text_tracked(AE_FONT_BODY, label, LABEL_TRACKING, x + gu(8.0f), y, AE_ALIGN_LEFT, AE_COLOR_MUTED, line);
	{
		float w = ae_draw_text_tracked(AE_FONT_BODY, label, LABEL_TRACKING, right_x(), y, AE_ALIGN_RIGHT, AE_COLOR_MUTED, "IN USE");

		ae_widget_separator(&screen.density, right_x() - w - gu(14.0f), y + ae_draw_cap_height(AE_FONT_BODY, label) * 0.5f,
			8.0f, AE_COLOR_ACCENT);
	}
	for (index = 0; index < screen.top_count; index++)
	{
		if (screen.top[index].kind != TOP_SLOT && screen.top[index].kind != TOP_NEW)
			continue;
		slot_tile_draw(index, x + (float)slots * (tile_width + gap), tiles_y, tile_width, tile_height);
		slots++;
	}
	actions_x = x + (float)slots * (tile_width + gap) - gap + gu(STRIP_ACTIONS_GAP_U);
	room = right_x() - actions_x;
	/* (the actions' rows: as many as the room takes, all of them centred on the tiles' middle) */
	{
		float widths[4], total = 0.0f, rows = 1.0f;

		for (index = 0; index < screen.top_count; index++)
			if (screen.top[index].kind >= TOP_RENAME)
				widths[actions++] = action_width(screen.top[index].kind);
		row_x = 0.0f;
		for (index = 0; index < actions; index++)
		{
			if (row_x > 0.0f && row_x + widths[index] > room)
			{
				rows += 1.0f;
				row_x = 0.0f;
			}
			row_x += widths[index] + gu(ACTION_GAP_U);
			total = row_x > total ? row_x : total;
		}
		row_y = tiles_y + (tile_height - (rows * action_height + (rows - 1.0f) * gu(ACTION_GAP_U))) * 0.5f;
		screen.actions_rect.x = actions_x;
		screen.actions_rect.y = row_y;
		screen.actions_rect.width = total - gu(ACTION_GAP_U) > gu(300.0f) ? total - gu(ACTION_GAP_U) : gu(300.0f);
		if (screen.actions_rect.width > room)
			screen.actions_rect.width = room;
		screen.actions_rect.height = rows * action_height + (rows - 1.0f) * gu(ACTION_GAP_U);
	}
	if (screen.typing == TYPING_NAME)
	{
		/* (RENAME: the name's field in the actions' place, NAME over it) */
		float field_height = action_height + gu(8.0f), field_y = tiles_y + (tile_height - field_height) * 0.5f;

		screen.field_rect.x = actions_x;
		screen.field_rect.y = field_y;
		screen.field_rect.width = gu(320.0f) < room ? gu(320.0f) : room;
		screen.field_rect.height = field_height;
		ae_draw_text_tracked(AE_FONT_BODY, gu(13.0f), LABEL_TRACKING, actions_x, field_y - gu(22.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
			"NAME");
		field_draw(screen.field_rect.x, screen.field_rect.y, screen.field_rect.width, screen.field_rect.height);
	}
	else
	{
		row_x = actions_x;
		for (index = 0; index < screen.top_count; index++)
		{
			float width;

			if (screen.top[index].kind < TOP_RENAME)
				continue;
			width = action_width(screen.top[index].kind);
			if (row_x > actions_x && row_x + width > right_x())
			{
				row_x = actions_x;
				row_y += action_height + gu(ACTION_GAP_U);
			}
			action_draw(index, row_x, row_y);
			row_x += width + gu(ACTION_GAP_U);
		}
	}
	return tiles_y + tile_height;
}

/* the background's switch on the stage's foot: "X  BACKGROUND  SKY  3/7" in a dark pill, the mark the device's */
static void background_pill(float right, float bottom)
{
	float height = gu(PILL_HEIGHT_U), text = gu(PILL_TEXT_U), glyph = gu(28.0f), pad = gu(14.0f), x, mark, words_width;
	char words[48];

	snprintf(words, sizeof(words), "BACKGROUND  %s  %d/%d", ae_reticle_preview_background_name(screen.background),
		screen.background + 1, ae_reticle_preview_backgrounds());
	mark = ae_widget_button_mark(&screen.density, AE_BUTTON_X, 0.0f, 0.0f, glyph, 0, 0);
	words_width = ae_draw_text_tracked_width(AE_FONT_BODY, text, 0.1f, words);
	x = right - (pad + mark + gu(10.0f) + words_width + pad);
	screen.pill_rect.x = x;
	screen.pill_rect.y = bottom - height;
	screen.pill_rect.width = right - x;
	screen.pill_rect.height = height;
	ae_draw_rect(x, bottom - height, right - x, height, height * 0.5f, 0x10161CE6u);
	ae_widget_button_mark(&screen.density, AE_BUTTON_X, x + pad * 0.7f, bottom - height * 0.5f, glyph, 0, 1);
	ae_draw_text_tracked(AE_FONT_BODY, text, 0.1f, x + pad * 0.7f + mark + gu(10.0f), cap_top(AE_FONT_BODY, text,
		bottom - height * 0.5f), AE_ALIGN_LEFT, AE_COLOR_TITLE, words);
	ae_hit_add(x, bottom - height, right - x, height, HIT_BACKGROUND, AE_PART_ITEM, 0);
}

/* the stage over the tiles: the tiles one row of six where each is at least 96 u wide, else 3 x 2 */
static void stage_draw(float x, float y, float width, float height)
{
	struct ae_crosshair xh;
	float gap = gu(TILES_GAP_U), label = gu(TILE_LABEL_U), tile_width, swatch, tiles_height, stage_height;
	int columns, pass;

	preview_of(&xh);
	/* (one row of six where each is at least 96 u wide; else 3 x 2 (4:3), unless that leaves the stage under 240 u (a larger
	UI scale's short column): one row of six then, narrower) */
	columns = (width - 5.0f * gap) / 6.0f >= gu(96.0f) ? 6 : 3;
	for (pass = 0; pass < 2; pass++)
	{
		tile_width = (width - (float)(columns - 1) * gap) / (float)columns;
		swatch = tile_width * 0.78f;
		if (swatch > gu(TILE_SWATCH_MAX_U))
			swatch = gu(TILE_SWATCH_MAX_U);
		tiles_height = (swatch + label) * (columns == 6 ? 1.0f : 2.0f) + (columns == 6 ? 0.0f : gap);
		stage_height = height - tiles_height - gu(STAGE_TILES_GAP_U);
		if (columns == 6 || stage_height >= gu(240.0f))
			break;
		columns = 6;
	}
	if (stage_height < gu(120.0f))
		stage_height = gu(120.0f);
	screen.stage_rect.x = x;
	screen.stage_rect.y = y;
	screen.stage_rect.width = width;
	screen.stage_rect.height = stage_height;
	ae_crosshair_state_colors_apply(screen.player);
	ae_reticle_preview_stage(x, y, width, stage_height, &xh, screen.background, screen.frame.unit);
	background_pill(x + width - gu(16.0f), y + stage_height - gu(16.0f));
	ae_reticle_preview_tiles(x, y + stage_height + gu(STAGE_TILES_GAP_U), width, swatch + label, gap, columns, &xh,
		screen.background, screen.frame.unit);
	{
		int index;

		for (index = 0; index < 6; index++)
		{
			screen.tile_rects[index].x = x + (float)(index % columns) * (tile_width + gap);
			screen.tile_rects[index].y = y + stage_height + gu(STAGE_TILES_GAP_U) + (float)(index / columns) * (swatch + label + gap);
			screen.tile_rects[index].width = tile_width;
			screen.tile_rects[index].height = swatch + label;
		}
	}
}

static void tabs_draw(float x, float y, float width)
{
	struct ae_chip chips[TABS];
	struct ae_rect rects[TABS];
	float glyph = gu(screen.density.metrics->chip) * 0.8f, chips_x, end, room, center, left_mark, right_mark;
	short index;

	for (index = 0; index < TABS; index++)
	{
		chips[index].label = tab_names[index];
		chips[index].count = -1;
		chips[index].flags = index == screen.tab ? AE_CHIP_ON : 0;
	}
	/* (LB and RB marked as the footer marks them, the device's own, at the chips' ends) */
	left_mark = ae_widget_button_mark(&screen.density, AE_BUTTON_LEFT_SHOULDER, 0.0f, 0.0f, glyph, 0, 0);
	right_mark = ae_widget_button_mark(&screen.density, AE_BUTTON_RIGHT_SHOULDER, 0.0f, 0.0f, glyph, 0, 0);
	chips_x = x + left_mark + gu(12.0f);
	room = x + width - chips_x - right_mark - gu(12.0f);
	ae_chips_layout(&screen.density, room, chips, TABS, rects);
	center = y + rects[0].height * 0.5f;
	ae_widget_button_mark(&screen.density, AE_BUTTON_LEFT_SHOULDER, x, center, glyph, 0, 1);
	ae_widget_chips(&screen.density, chips_x, y, room, chips, TABS, HIT_TABS);
	for (index = 0; index < TABS; index++)
	{
		screen.tab_rects[index] = rects[index];
		screen.tab_rects[index].x += chips_x;
		screen.tab_rects[index].y += y;
	}
	end = chips_x + rects[TABS - 1].x + rects[TABS - 1].width + gu(12.0f);
	ae_widget_button_mark(&screen.density, AE_BUTTON_RIGHT_SHOULDER, end, y + rects[TABS - 1].y + rects[TABS - 1].height * 0.5f,
		glyph, 0, 1);
}

/* a value row of the panel (a settings row, CUSTOM COLOR, RED / GREEN / BLUE): the settings' row; dimmed under an OFF switch
(the focus shows it whole); CUSTOM COLOR's swatch of the colour left of the value */
static void row_draw(short index, struct item const *item, float x, float y)
{
	struct ae_settings_row const *row = item_row(item);
	struct ae_crosshair xh;
	struct ae_row widget;
	char shown[VALUE_TEXT];
	int focused = screen.region == REGION_PANEL && screen.item == index;
	int dim = row && item->kind == ITEM_ROW && row_dimmed(row) && !focused;
	float alpha = ae_ui_drawing_alpha();

	current(&xh);
	memset(&widget, 0, sizeof(widget));
	if (item->kind == ITEM_RGB)
	{
		widget.label = channel_names[item->value];
		snprintf(shown, sizeof(shown), "%d", (int)xh.color[item->value]);
		/* (a slider, 0 to 255: the settings' row; the colour itself shows on the stage and by CUSTOM COLOR's swatch) */
		widget.flags |= AE_ROW_SLIDER;
		widget.slider = (float)xh.color[item->value] / 255.0f;
	}
	else if (item->kind == ITEM_HEX)
	{
		widget.label = row ? row->label : "CUSTOM COLOR";
		ae_xh_hex_format(rgb_of(&xh), shown, sizeof(shown));
	}
	else if (row)
	{
		widget.label = row->label;
		row_shown(row, shown, sizeof(shown));
	}
	widget.value = shown;
	widget.flags |= focused ? AE_ROW_FOCUSED : 0;
	if (item->kind == ITEM_HEX)
		widget.flags |= AE_ROW_SUBSCREEN;
	if (item->kind == ITEM_HEX && screen.typing == TYPING_HEX)
	{
		float height = item->rect.height, well_width = ae_draw_text_width(AE_FONT_ROW, ae_size_text(&screen.density),
			"#DDDDDDD") + gu(26.0f), inset = gu(8.0f);

		widget.value = NULL;
		widget.flags &= ~(unsigned int)AE_ROW_SUBSCREEN;
		ae_widget_row(&screen.density, x, y, item->rect.width, &widget, HIT_PANEL, index);
		screen.field_rect.x = x + item->rect.width - gu(screen.density.metrics->pad) - well_width;
		screen.field_rect.y = y + inset;
		screen.field_rect.width = well_width;
		screen.field_rect.height = height - 2.0f * inset;
		field_draw(screen.field_rect.x, screen.field_rect.y, screen.field_rect.width, screen.field_rect.height);
		return;
	}
	if (dim)
		ae_draw_set_alpha(alpha * DIM_ALPHA);
	ae_widget_row(&screen.density, x, y, item->rect.width, &widget, HIT_PANEL, index);
	if (dim)
		ae_draw_set_alpha(alpha);
	if (item->kind == ITEM_HEX)
	{
		struct ae_row_layout layout;
		float side = item->rect.height * 0.42f, sx, sy;
		unsigned long rgb = rgb_of(&xh);

		ae_row_layout(&screen.density, item->rect.width, &widget, &layout);
		sx = x + layout.value_x - gu(12.0f) - side;
		sy = y + (item->rect.height - side) * 0.5f;
		ae_draw_rect(sx, sy, side, side, 0.0f, (unsigned int)(rgb << 8) | 0xFFu);
		ae_draw_outline(sx, sy, side, side, 0.0f, gu(1.5f), focused ? AE_COLOR_SELECTION_TEXT : AE_COLOR_MUTED);
	}
}

/* a shape's or a colour's tile: the selection's fill and the accent notch on the focus; the crosshair's own (its shape, its
colour) marked with an accent bar along the foot; the name under the picture */
static void grid_tile_draw(short index, struct item const *item, float x, float y)
{
	struct ae_crosshair xh;
	float width = item->rect.width, height = item->rect.height, name_size = gu(13.0f), name_band = gu(28.0f);
	int focused = screen.region == REGION_PANEL && screen.item == index;
	unsigned int ink = focused ? AE_COLOR_SELECTION_TEXT : AE_COLOR_TITLE;
	const char *name;
	int own;

	current(&xh);
	own = item->kind == ITEM_SHAPE ? item->value == xh.shape : item->value == ae_xh_palette_of(&xh);
	ae_draw_rect(x, y, width, height, 0.0f, focused ? AE_COLOR_SELECTION : AE_COLOR_ROW);
	if (focused)
		ae_draw_rect(x, y, gu(NOTCH_U), height, 0.0f, AE_COLOR_ACCENT);
	if (item->kind == ITEM_SHAPE)
	{
		float art = height - name_band, half = (art < width ? art : width) * 0.5f - gu(12.0f);

		crosshair_art(&xh, item->value, x + width * 0.5f, y + gu(4.0f) + art * 0.5f, half > 4.0f ? half : 4.0f,
			(unsigned long)(ink >> 8), 0);
		name = ae_xh_shape_name(item->value);
	}
	else
	{
		float side = gu(34.0f), sx = x + (width - side) * 0.5f, sy = y + (height - name_band - side) * 0.5f + gu(4.0f);
		unsigned long rgb = item->value >= 0 ? ae_xh_palette_rgb(item->value) : rgb_of(&xh);

		ae_draw_rect(sx, sy, side, side, 0.0f, (unsigned int)(rgb << 8) | 0xFFu);
		ae_draw_outline(sx, sy, side, side, 0.0f, gu(1.5f), focused ? AE_COLOR_SELECTION_TEXT : 0xC8D6E059u);
		if (item->value < 0)
		{
			float size = gu(18.0f);

			ae_draw_rect(sx + side * 0.2f, sy + side * 0.2f, side * 0.6f, side * 0.6f, 0.0f, 0x10161CCCu);
			ae_draw_text(AE_FONT_ROW, size, sx + side * 0.5f, cap_top(AE_FONT_ROW, size, sy + side * 0.5f), AE_ALIGN_CENTER,
				AE_COLOR_TITLE, "#");
		}
		name = item->value >= 0 ? ae_xh_palette_name(item->value) : "CUSTOM";
	}
	{
		float size = name_size, room = width - gu(12.0f), text_width = ae_draw_text_tracked_width(AE_FONT_ROW, size, 0.06f, name);

		if (text_width > room && text_width > 0.0f)
			size *= room / text_width;
		ae_draw_text_tracked(AE_FONT_ROW, size, 0.06f, x + width * 0.5f, cap_top(AE_FONT_ROW, size, y + height - name_band *
			0.5f - gu(2.0f)), AE_ALIGN_CENTER, focused ? AE_COLOR_SELECTION_TEXT : AE_COLOR_TEXT, name);
	}
	if (own)
		ae_draw_rect(x, y + height - gu(MARK_U), width, gu(MARK_U), 0.0f, focused ? AE_COLOR_SELECTION_TEXT : AE_COLOR_ACCENT);
	ae_hit_add(x, y, width, height, HIT_PANEL, AE_PART_ITEM, index);
}

/* the panel: the tabs, then the tab's content in its window (scrolled to keep the focus in view; a muted "n more" under it
when it scrolls) */
static void panel_draw(float x, float y, float width, float height)
{
	float pad = gu(PANEL_PAD_U), content_x = x + pad, content_y, content_width = width - 2.0f * pad, content_height, maximum;
	short index, block;

	screen.panel_rect.x = x;
	screen.panel_rect.y = y;
	screen.panel_rect.width = width;
	screen.panel_rect.height = height;
	ae_draw_rect(x, y, width, height, 0.0f, AE_COLOR_PANEL);
	tabs_draw(content_x, y + pad, content_width);
	content_y = y + pad + gu(screen.density.metrics->chip) + gu(TABS_TO_CONTENT_U);
	content_height = y + height - pad - content_y;
	if (content_width != screen.panel_width || screen.layout.item_count == 0)
	{
		screen.panel_width = content_width;
		layout_build(screen.tab, content_width, &screen.layout);
		if (screen.item >= screen.layout.item_count)
			screen.item = 0;
	}
	/* (a tab longer than its window: a "▼ N more" line under it while items are below) */
	if (screen.layout.height > content_height)
		content_height -= ae_widget_more_line(&screen.density);
	screen.panel_content.x = content_x;
	screen.panel_content.y = content_y;
	screen.panel_content.width = content_width;
	screen.panel_content.height = content_height;
	maximum = screen.layout.height > content_height ? screen.layout.height - content_height : 0.0f;
	if (screen.scroll_to_focus && screen.region == REGION_PANEL && screen.item < screen.layout.item_count)
	{
		struct item const *item = &screen.layout.items[screen.item];
		float top = item->rect.y, bottom = item->rect.y + item->rect.height;
		struct block const *grid = &screen.layout.blocks[item->block];

		/* (a grid's first row shows its family's header too; the tab's first item, the top) */
		if (grid->grid)
		{
			short entry = grid->entry_of[screen.item - grid->first];

			while (entry > 0 && grid->layout.row_of[entry - 1] == grid->layout.row_of[entry])
				entry--;
			if (entry > 0 && grid->layout.row_of[entry - 1] < 0)
				top = grid->header_rect[entry - 1].y;
		}
		if (screen.item == 0)
			top = 0.0f;
		if (top < screen.scroll)
			screen.scroll = top;
		if (bottom > screen.scroll + content_height)
			screen.scroll = bottom - content_height;
		screen.scroll_to_focus = 0;
	}
	if (screen.scroll > maximum)
		screen.scroll = maximum;
	if (screen.scroll < 0.0f)
		screen.scroll = 0.0f;
	ae_draw_clip_push(content_x - gu(2.0f), content_y, content_width + gu(4.0f), content_height);
	for (index = 0; index < screen.layout.label_count; index++)
	{
		struct label const *label = &screen.layout.labels[index];
		float size = gu(14.0f);

		ae_draw_text_tracked(AE_FONT_BODY, size, LABEL_TRACKING, content_x + gu(4.0f), cap_top(AE_FONT_BODY, size,
			content_y + label->rect.y - screen.scroll + label->rect.height * 0.55f), AE_ALIGN_LEFT, AE_COLOR_MUTED, label->text);
	}
	for (block = 0; block < screen.layout.block_count; block++)
	{
		struct block const *b = &screen.layout.blocks[block];

		if (!b->grid)
			continue;
		for (index = 0; index < b->layout.count; index++)
		{
			float size = gu(14.0f), top = content_y + b->header_rect[index].y - screen.scroll;

			if (b->layout.row_of[index] >= 0 || top + b->header_rect[index].height < content_y || top > content_y + content_height)
				continue;
			ae_draw_text_tracked(AE_FONT_BODY, size, LABEL_TRACKING, content_x + gu(4.0f), cap_top(AE_FONT_BODY, size, top +
				b->header_rect[index].height * 0.55f), AE_ALIGN_LEFT, AE_COLOR_MUTED, b->values[index] + 1);
		}
	}
	for (index = 0; index < screen.layout.item_count; index++)
	{
		struct item const *item = &screen.layout.items[index];
		float ix = content_x + item->rect.x, iy = content_y + item->rect.y - screen.scroll;

		if (iy + item->rect.height < content_y || iy > content_y + content_height)
			continue;
		if (item->kind == ITEM_SHAPE || item->kind == ITEM_COLOR)
			grid_tile_draw(index, item, ix, iy);
		else
			row_draw(index, item, ix, iy);
	}
	ae_draw_clip_pop();
	/* (more than it shows: the items under the window fade into the panel, counted on the line under it; the list's 6 u
	scrollbar on the right, inside the panel's pad) */
	if (maximum > 0.0f)
	{
		short below = 0;
		float fade = gu(36.0f);

		for (index = 0; index < screen.layout.item_count; index++)
			if (screen.layout.items[index].rect.y + screen.layout.items[index].rect.height > screen.scroll + content_height + 1.0f)
				below++;
		if (screen.scroll < maximum - 1.0f)
			ae_draw_gradient(content_x - gu(2.0f), content_y + content_height - fade, content_width + gu(4.0f), fade, 0.0f,
				0x181F2600u, 0x181F26F2u);
		if (below > 0)
			ae_widget_more(&screen.density, content_x + content_width * 0.5f, content_y + content_height, 0, below);
	}
	if (maximum > 0.0f)
	{
		float track = gu(6.0f), bar_x = x + width - pad * 0.5f - track * 0.5f, thumb_height = content_height * content_height /
			screen.layout.height, thumb_y = (content_height - thumb_height) * screen.scroll / maximum;

		ae_draw_rect(bar_x, content_y, track, content_height, track * 0.5f, AE_COLOR_TRACK);
		ae_draw_rect(bar_x, content_y + thumb_y, track, thumb_height, track * 0.5f, AE_COLOR_MUTED);
	}
}

static void help_text(char *text, int size)
{
	struct item const *item = focused_item();
	struct ae_crosshair xh, own;

	current(&own);
	text[0] = 0;
	if (screen.typing == TYPING_NAME)
	{
		snprintf(text, (size_t)size, "Up to 12 letters, digits and spaces (- and . too). Enter keeps it, Esc goes back.");
		return;
	}
	if (screen.typing == TYPING_HEX)
	{
		snprintf(text, (size_t)size, "Six hex digits, #RRGGBB. Enter keeps it, Esc goes back. Red over an enemy and green over "
			"an ally still win.");
		return;
	}
	if (screen.region == REGION_TOP)
	{
		struct top_entry const *entry = &screen.top[screen.top_focus];
		int saved = ae_crosshair_edit_saved(screen.player);

		switch (entry->kind)
		{
		case TOP_SLOT:
			if (!ae_crosshair_edit_slot_saved(screen.player, entry->slot, &xh))
				snprintf(text, (size_t)size, "%s is new: it is saved as you change it.", own.name);
			else if (entry->slot == ae_crosshair_edit_slot(screen.player))
				snprintf(text, (size_t)size, "%s: you are editing it%s.", xh.name, ae_crosshair_edit_main(screen.player) ==
					entry->slot ? ", and it is your main crosshair (IN USE)" : "");
			else
				snprintf(text, (size_t)size, "%s%s. {A} edits it.", xh.name, ae_crosshair_edit_main(screen.player) == entry->slot ?
					" is your main crosshair (IN USE)" : "");
			break;
		case TOP_NEW:
			snprintf(text, (size_t)size, "A fresh crosshair with the default look, in a free slot (%d of 8 left). {A} makes it.",
				AE_XH_SLOTS - saved);
			break;
		case TOP_RENAME:
			snprintf(text, (size_t)size, "Name %s: up to 12 letters, digits and spaces. {A} types it.", own.name);
			break;
		case TOP_DUPLICATE:
			if (entry->disabled)
				snprintf(text, (size_t)size, saved >= AE_XH_SLOTS ? "All 8 slots are used: delete one first." :
					"Change %s first: a new crosshair is saved as you change it.", own.name);
			else
				snprintf(text, (size_t)size, "A copy of %s in a free slot. You edit the copy; %s stays as it is.", own.name,
					own.name);
			break;
		case TOP_SHARE:
			snprintf(text, (size_t)size, "Copy %s as a short code, or paste one a friend gave you. {Y} opens this from anywhere "
				"here.", own.name);
			break;
		case TOP_DELETE:
			snprintf(text, (size_t)size, entry->disabled ? "%s is not saved yet." : "Remove %s from your saved crosshairs. It asks "
				"first.", own.name);
			break;
		default:
			break;
		}
		return;
	}
	if (!item)
		return;
	switch (item->kind)
	{
	case ITEM_ROW:
	case ITEM_HEX:
		if (item_row(item))
			snprintf(text, (size_t)size, "%s%s", item_row(item)->help, item->kind == ITEM_ROW && row_dimmed(item_row(item)) ?
				" Its switch is OFF: it shows once that is on." : "");
		break;
	case ITEM_RGB:
		snprintf(text, (size_t)size, "The colour's %s, 0 to 255 in steps of 8. {A} types the colour as #RRGGBB.",
			channel_names[item->value]);
		break;
	case ITEM_SHAPE:
		if (item->value == own.shape)
			snprintf(text, (size_t)size, "%s: the shape picked. Its centre, lines and frame are on the other tabs.",
				ae_xh_shape_name(item->value));
		else
			snprintf(text, (size_t)size, "%s: shown on the stage while focused. {A} picks it (its centre, lines and frame; your "
				"colour and outline stay), {B} keeps %s.", ae_xh_shape_name(item->value), own.shape ?
				ae_xh_shape_name(own.shape) : "the one you have");
		break;
	case ITEM_COLOR:
		if (item->value < 0)
			snprintf(text, (size_t)size, "Any colour, typed as #RRGGBB or stepped as RED, GREEN and BLUE below. {A} types it.");
		else if (item->value == ae_xh_palette_of(&own))
			snprintf(text, (size_t)size, "The crosshair's colour. Red over an enemy and green over an ally stay, whatever you "
				"pick.");
		else
			snprintf(text, (size_t)size, "%s: shown on the stage while focused. {A} picks it, {B} keeps the colour you have.",
				ae_xh_palette_name(item->value));
		break;
	default:
		break;
	}
}

static void help_draw(float x, float y, float width)
{
	char text[320], expanded[400], lines[HELP_LINES][160];
	float body = gu(screen.density.metrics->body), line = gu(screen.density.metrics->body_line);
	int count, index;

	help_text(text, sizeof(text));
	count = ae_wrap_text(AE_FONT_BODY, body, ae_help_expand(text, expanded, sizeof(expanded)), width, lines, HELP_LINES);
	for (index = 0; index < count; index++)
		ae_draw_text(AE_FONT_BODY, body, x, y + line * (float)index, AE_ALIGN_LEFT, AE_COLOR_TEXT, lines[index]);
}

static void footer_draw(void)
{
	struct item const *item = focused_item();
	short count = 0;

	if (screen.typing != TYPING_NONE)
	{
		screen.prompts[count++] = prompt_of(AE_BUTTON_A, "Keep", AE_ACTION_ACCEPT, 0);
		screen.prompts[count++] = prompt_of(AE_BUTTON_B, "Cancel", AE_ACTION_BACK, 0);
	}
	else
	{
		const char *accept = "Select";

		if (screen.region == REGION_TOP)
		{
			short kind = screen.top[screen.top_focus].kind;

			accept = kind == TOP_SLOT ? "Edit" : kind == TOP_NEW ? "New" : kind == TOP_RENAME ? "Rename" : kind == TOP_DUPLICATE ?
				"Duplicate" : kind == TOP_SHARE ? "Share" : "Delete";
		}
		else if (item)
			accept = item->kind == ITEM_ROW ? ae_string(AE_STR_ALL_VALUES) : item->kind == ITEM_HEX || item->kind == ITEM_RGB ||
				(item->kind == ITEM_COLOR && item->value < 0) ? "Type a color" : "Pick";
		screen.prompts[count++] = prompt_of(AE_BUTTON_A, accept, AE_ACTION_ACCEPT, 0);
		screen.prompts[count++] = prompt_of(AE_BUTTON_B, previewing() ? "Keep" : ae_string(AE_STR_BACK), AE_ACTION_BACK, 0);
		if (item && (item->kind == ITEM_ROW || item->kind == ITEM_RGB))
			screen.prompts[count++] = prompt_of(AE_BUTTON_DPAD_LEFT, "Change", AE_ACTION_LEFT, 1);
		screen.prompts[count++] = prompt_of(AE_BUTTON_LEFT_SHOULDER, "Tab", AE_ACTION_TAB_PREVIOUS, 1);
		/* (every tab: the triggers move a family on the shapes, a page of rows or the other column elsewhere) */
		if (screen.region == REGION_PANEL && item)
			screen.prompts[count++] = prompt_of(AE_BUTTON_LEFT_TRIGGER, item->kind == ITEM_SHAPE ? "Family" :
				screen.layout.blocks[item->block].grid ? "Page" : screen.layout.blocks[item->block].columns > 0 ? "Column" :
				"Page", AE_ACTION_PAGE_UP, 1);
		screen.prompts[count++] = prompt_of(AE_BUTTON_X, "Background", AE_ACTION_X, 0);
		screen.prompts[count++] = prompt_of(AE_BUTTON_Y, "Share code", AE_ACTION_Y, 0);
	}
	screen.prompt_count = count;
	ae_widget_prompts(&screen.density, left_x(), screen.frame.rect.height - gu(FOOTER_ABOVE_U), screen.prompts, count, NULL,
		right_x(), -1, HIT_PROMPTS);
}

/* (the clearance check, tools/ae_crosshair_clearance.py) every part's rectangle as drawn, layout units, once a frame size and
a tab: "ae crosshair screen: rect <kind> <name> x y w h" */
static void rect_log(const char *kind, const char *name, struct ae_rect const *rect)
{
	platform_log("ae crosshair screen: rect %s %s %.1f %.1f %.1f %.1f", kind, name, rect->x, rect->y, rect->width, rect->height);
}

static void layout_log(void)
{
	static const char *const tile_names[6] = { "NORMAL", "ENEMY", "ALLY", "AT_REST", "MOVING", "FIRING" };
	struct ae_rect rect;
	char name[48], *at;
	short index;

	screen.layout_logged = 1;
	for (index = 0; index < screen.top_count; index++)
	{
		struct top_entry const *entry = &screen.top[index];

		if (screen.typing == TYPING_NAME && entry->kind >= TOP_RENAME)
			continue;
		snprintf(name, sizeof(name), "%s%d", entry->kind == TOP_SLOT ? "slot" : action_name(entry->kind), entry->kind == TOP_SLOT ?
			entry->slot + 1 : 0);
		for (at = name; *at; at++)
			if (*at == ' ')
				*at = '_';
		rect_log(entry->kind <= TOP_NEW ? "strip" : "action", name, &entry->rect);
	}
	rect_log("stage", "stage", &screen.stage_rect);
	rect_log("pill", "background", &screen.pill_rect);
	for (index = 0; index < 6; index++)
		rect_log("tile", tile_names[index], &screen.tile_rects[index]);
	rect_log("panel", "panel", &screen.panel_rect);
	for (index = 0; index < TABS; index++)
		rect_log("tab", tab_names[index], &screen.tab_rects[index]);
	for (index = 0; index < screen.layout.item_count; index++)
	{
		struct item const *item = &screen.layout.items[index];

		rect = item_frame_rect(item);
		if (rect.y < screen.panel_content.y || rect.y + rect.height > screen.panel_content.y + screen.panel_content.height)
			continue;
		snprintf(name, sizeof(name), "%s%d", item->kind == ITEM_SHAPE ? "shape" : item->kind == ITEM_COLOR ? "color" : "row", index);
		rect_log("item", name, &rect);
	}
	rect.x = left_x();
	rect.y = screen.help_top;
	rect.width = right_x() - left_x();
	rect.height = gu(screen.density.metrics->body_line);
	rect_log("help", "help", &rect);
	platform_log("ae crosshair screen: rect footer middle %.1f scale %.3f", screen.footer_middle, ae_draw_units_per_pixel());
}

static void crosshair_draw(struct ae_screen *instance)
{
	float top, bottom, help_y, line, width, left_width, content_top;

	(void)instance;
	density_read();
	if (!ae_ui_drawing_ghost())
		focus_log();
	header_draw();
	top = ae_frame_y(&screen.frame, CONTENT_Y);
	content_top = strip_draw(top) + gu(CONTENT_GAP_U);
	bottom = screen.frame.rect.height - gu(FOOTER_ABOVE_U) - gu(FOOTER_ROOM_U);
	line = gu(screen.density.metrics->body_line);
	help_y = bottom - line * (float)HELP_LINES;
	width = right_x() - left_x();
	left_width = width * LEFT_SHARE;
	stage_draw(left_x(), content_top, left_width, help_y - gu(HELP_GAP_U) - content_top);
	panel_draw(left_x() + left_width + gu(COLUMN_GAP_U), content_top, width - left_width - gu(COLUMN_GAP_U),
		help_y - gu(HELP_GAP_U) - content_top);
	ae_draw_rect(left_x(), help_y - gu(HELP_GAP_U) * 0.5f, width, gu(RULE_U), 0.0f, AE_COLOR_RULE);
	help_draw(left_x(), help_y, width);
	footer_draw();
	screen.help_top = help_y;
	screen.footer_middle = screen.frame.rect.height - gu(FOOTER_ABOVE_U);
	if (!screen.layout_logged && !ae_ui_drawing_ghost())
		layout_log();
}

/* ---------- input */

static void back(void)
{
	if (keep_ask())
		return;
	ae_player_settings_flush();
	ae_sound_request(AE_SOUND_BACK, 0);
	ae_ui_pop();
}

static int crosshair_handle(struct ae_screen *instance, struct ae_event const *event)
{
	(void)instance;
	top_build();
	if (screen.typing != TYPING_NONE)
	{
		switch (event->action)
		{
		case AE_ACTION_ACCEPT:
		case AE_ACTION_START:
			typing_end(1);
			break;
		case AE_ACTION_BACK:
			typing_end(0);
			break;
		case AE_ACTION_LEFT:
		case AE_ACTION_RIGHT:
			ae_text_move(&screen.typed, event->action == AE_ACTION_RIGHT ? 1 : -1,
				(ae_platform_modifiers() & AE_MODIFIER_SHIFT) != 0);
			screen.caret_since = ae_motion_now();
			break;
		default:
			break;
		}
		return 1;
	}
	switch (event->action)
	{
	case AE_ACTION_BACK:
		/* (a shape or colour shown, not picked: B keeps the one picked; else the screen goes, KEEP / UNDO first) */
		if (screen.region == REGION_PANEL && keep_own())
			return 1;
		back();
		return 1;
	case AE_ACTION_TAB_PREVIOUS:
	case AE_ACTION_TAB_NEXT:
		tab_set((short)((screen.tab + (event->action == AE_ACTION_TAB_NEXT ? 1 : TABS - 1)) % TABS));
		return 1;
	case AE_ACTION_X:
		background_next();
		return 1;
	case AE_ACTION_Y:
		share_open();
		ae_sound_request(AE_SOUND_FORWARD, 0);
		return 1;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		if (screen.region == REGION_TOP)
		{
			if (event->action == AE_ACTION_DOWN)
			{
				screen.region = REGION_PANEL;
				if (screen.item >= screen.layout.item_count)
					screen.item = 0;
				screen.scroll_to_focus = 1;
				ae_sound_request(AE_SOUND_CURSOR, 0);
			}
		}
		else
			move_vertical(event->action == AE_ACTION_UP ? -1 : 1);
		return 1;
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
	{
		short direction = event->action == AE_ACTION_RIGHT ? 1 : -1;

		if (screen.region == REGION_TOP)
		{
			short next = (short)(screen.top_focus + direction);

			if (next >= 0 && next < screen.top_count)
			{
				screen.top_focus = next;
				ae_sound_request(AE_SOUND_CURSOR, 0);
			}
		}
		else
			move_horizontal(direction);
		return 1;
	}
	case AE_ACTION_PAGE_UP:
	case AE_ACTION_PAGE_DOWN:
		if (screen.region == REGION_PANEL)
			move_page(event->action == AE_ACTION_PAGE_DOWN ? 1 : -1);
		return 1;
	case AE_ACTION_ACCEPT:
		if (screen.region == REGION_TOP)
			top_accept(event->device != AE_DEVICE_KEYBOARD_MOUSE);
		else
			panel_accept(event->device != AE_DEVICE_KEYBOARD_MOUSE);
		return 1;
	default:
		return 1;
	}
}

static void crosshair_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;

	if (screen.typing != TYPING_NONE)
	{
		int kind = ae_typing_pointer(screen.prompts, screen.prompt_count, pointer, HIT_PROMPTS, &screen.field_rect, &event);

		if (kind == 1)
			crosshair_handle(instance, &event);
		else if (kind == 2)
			typing_end(1);
		return;
	}
	if (ae_prompts_pointer(screen.prompts, screen.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		crosshair_handle(instance, &event);
		return;
	}
	/* (the wheel over the panel scrolls it, a row's height a notch) */
	if (pointer->wheel_steps && ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_PANEL)
	{
		screen.scroll -= (float)pointer->wheel_steps * (ae_size_row(&screen.density) + gu(screen.density.metrics->gap));
		return;
	}
	/* (a colour slider held: it follows the pointer until the button is let go of) */
	if (screen.drag_item > 0)
	{
		struct item const *held = screen.drag_item - 1 < screen.layout.item_count ?
			&screen.layout.items[screen.drag_item - 1] : NULL;

		if (!pointer->left_held || !held || held->kind != ITEM_RGB)
			screen.drag_item = 0;
		else if (ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_PANEL && hit.part == AE_PART_TRACK &&
			hit.index == screen.drag_item - 1)
			channel_at(held->value, &hit, pointer->x);
		return;
	}
	if (!ae_hit_at(pointer->x, pointer->y, 0, &hit))
		return;
	/* (hover: the focus follows the pointer once it moves) */
	if (pointer->moved && !pointer->left_clicks)
	{
		if (hit.id == HIT_TOP && hit.index >= 0 && hit.index < screen.top_count &&
			(screen.region != REGION_TOP || screen.top_focus != hit.index))
		{
			screen.region = REGION_TOP;
			screen.top_focus = hit.index;
		}
		else if (hit.id == HIT_PANEL && hit.index >= 0 && hit.index < screen.layout.item_count &&
			(screen.region != REGION_PANEL || screen.item != hit.index))
		{
			screen.region = REGION_PANEL;
			screen.item = hit.index;
		}
		return;
	}
	if (!pointer->left_clicks)
		return;
	switch (hit.id)
	{
	case HIT_TOP:
		if (hit.index >= 0 && hit.index < screen.top_count)
		{
			screen.region = REGION_TOP;
			screen.top_focus = hit.index;
			top_accept(0);
		}
		break;
	case HIT_TABS:
		if (hit.part == AE_PART_CHIP && hit.index >= 0 && hit.index < TABS)
			tab_set(hit.index);
		break;
	case HIT_BACKGROUND:
		background_next();
		break;
	case HIT_PANEL:
		if (hit.index < 0 || hit.index >= screen.layout.item_count)
			break;
		screen.region = REGION_PANEL;
		screen.item = hit.index;
		if (hit.part == AE_PART_TRACK && screen.layout.items[hit.index].kind == ITEM_RGB)
		{
			channel_at(screen.layout.items[hit.index].value, &hit, pointer->x);
			screen.drag_item = (short)(hit.index + 1);
		}
		else if (hit.part == AE_PART_ARROW_LEFT || hit.part == AE_PART_ARROW_RIGHT)
			move_horizontal(hit.part == AE_PART_ARROW_RIGHT ? 1 : -1);
		else
			panel_accept(pointer->touch != 0);
		break;
	default:
		break;
	}
}

/* once a frame: a dialog waiting for the one closing; the typed keys (a colour: hex digits and a leading #, at most 7);
the shapes' type-ahead (letters and digits jump to the next shape that starts with them) */
static void crosshair_update(struct ae_screen *instance)
{
	char chars[16];
	int count;

	(void)instance;
	if (screen.then_dialog)
	{
		int dialog = screen.then_dialog;

		screen.then_dialog = DIALOG_NONE;
		if (dialog == DIALOG_PASTED)
			pasted_open();
		else
			note_open("All 8 slots are used", "Delete a crosshair first, or APPLY to put the code in the open one's place.");
		return;
	}
	/* (letters typed while typing a field, or off the shapes, are not a jump) */
	count = ae_platform_take_typeahead(chars, (int)sizeof(chars));
	if (screen.typing != TYPING_NONE && screen.typed_edit.active)
	{
		struct ae_text before = screen.typed;
		int ok = ae_field_edit_keys(&screen.typed_edit);

		if (screen.typing == TYPING_HEX && !hex_typing_ok(screen.typed.text))
		{
			screen.typed = before;
			ok = 0;
		}
		if (memcmp(&before, &screen.typed, sizeof(before)))
			screen.caret_since = ae_motion_now();
		if (!ok)
			ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	if (count > 0 && screen.typing == TYPING_NONE && screen.region == REGION_PANEL && screen.tab == TAB_SHAPE)
	{
		struct item const *item = focused_item();
		short index, start, found = -1;
		int at;

		for (at = 0; at < count && found < 0; at++)
		{
			char letter = chars[at];

			/* (Q, E and R are LB, RB and Y on the keyboard) */
			if (letter == 'Q' || letter == 'E' || letter == 'R')
				continue;
			start = item && item->kind == ITEM_SHAPE ? (short)(screen.item + 1) : 0;
			for (index = 0; index < screen.layout.item_count && found < 0; index++)
			{
				short probe = (short)((start + index) % screen.layout.item_count);
				struct item const *candidate = &screen.layout.items[probe];

				if (candidate->kind == ITEM_SHAPE && candidate->value > 0 && ae_xh_shape_name(candidate->value)[0] == letter)
					found = probe;
			}
		}
		if (found >= 0)
			item_set(found);
	}
}

static void crosshair_leave(struct ae_screen *instance)
{
	(void)instance;
	if (screen.typing != TYPING_NONE)
	{
		if (screen.typed_edit.active)
			ae_field_edit_end(&screen.typed_edit, 0);
		screen.typing = TYPING_NONE;
	}
	ae_player_settings_flush();
	ae_crosshair_edit_lists(screen.player);
	platform_log("ae crosshair screen: closed (%s)", changed() ? "changed" : "no changes");
}

static struct ae_screen_class const crosshair_class =
{
	.name = "crosshair", .leave = crosshair_leave, .handle = crosshair_handle, .draw = crosshair_draw,
	.pointer = crosshair_pointer, .update = crosshair_update,
};

int ae_screen_crosshair_open(short player)
{
	struct ae_crosshair xh;
	int slot;

	memset(&screen, 0, sizeof(screen));
	screen.player = player >= 0 && player < PLAYERS ? player : 0;
	screen.logged_tab = -1;
	screen.logged_width = -1.0f;
	screen.region = REGION_PANEL;
	screen.tab = TAB_SHAPE;
	ae_player_settings_update();
	player_name_read();
	/* (the main crosshair; else the first saved one; else a new one, saved as it changes) */
	slot = ae_crosshair_edit_open(screen.player);
	if (!ae_crosshair_edit_slot_saved(screen.player, slot, &xh))
	{
		for (slot = 0; slot < AE_XH_SLOTS; slot++)
			if (ae_crosshair_edit_slot_saved(screen.player, slot, &xh))
			{
				ae_crosshair_edit_select(screen.player, slot);
				break;
			}
	}
	screen.opened_slot = ae_crosshair_edit_slot(screen.player);
	ae_crosshair_edit_profile_take(screen.player, &screen.opened);
	ae_crosshair_edit_lists(screen.player);
	top_build();
	if (!ae_ui_push(&crosshair_class, AE_OWNER_PARENT, &screen))
		return 0;
	platform_log("ae crosshair screen: open player %d slot %d", screen.player + 1, screen.opened_slot + 1);
	return 1;
}
