/*
AE_GLUE_CE_MAPS.C

Arena Evolved's features on Custom Edition maps (ae_glue_ce_maps.h; item 79).

An Xbox map's tags get the menus' tags (the pause menu's SETTINGS:
menu_tags.c) and the high-res HUD's bitmaps (hud_hires_tags.c) at the end of
scenario_tags_load; a Custom Edition map's load returns before that (its own
branch, Chupa's), so it got neither: no SETTINGS in its pause menu, and only
its own HUD. Both are made here, after its indexed tags are copied in and its
HUD interfaces repaired (ce_hud.c), as an Xbox map's are made after its tags
are checked. The high-res HUD stands for the Halo PC bitmaps it was checked
against (port/linux/src/ae_ce_hud.h); anything else keeps the map's own.

Three things stood in the way, all handled here:
- a Custom Edition map's tags are read only inside its tag cache
  (ce_map_checks.c, ce_tags_pointer: anything else reads as zeroes), and the
  menus' tags are made in memory of their own (menu_tags.c's allocate): the
  added buttons' texts read as none. Each block menu_tags.c makes is noted,
  and a read inside one is let through.
- Halo PC's multiplayer pause list has a CHANGE SETTINGS row whose A runs a
  function of the PC version's that this engine has not (the log's "event
  handler function 137 '?' failed"): it does nothing. A pause list's row whose
  A runs a function this engine lacks is taken out before the menus' tags are
  added, so the SETTINGS they add takes its place (and the list keeps the
  room it has).
- Halo PC never had split screen: its 2p and 4p pause screens are its one
  player's box and list moved down, run past the bottom of a half or quarter
  view (the list's last rows, and the button prompts drawn over its rows), and
  more so with the rows the menus add. Once the rows are added, each pause
  screen shorter than a full screen gets the one player screen's layout: its
  box with the list as far into it as there, centred in the view, and the
  prompts under the list.
*/

#include "cseries.h"
#include "math/integer_math.h"
#include "tag_files/tag_groups.h"
#include "halo_menus.h"
#include "ae_glue_ce_maps.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* (port/linux/game/menu_tags.c, hud_hires_tags.c; the game's) */
void menu_tags_loaded(char const *map_name);
void hud_hires_tags_loaded(long scenario_index);
void platform_log(char const *format, ...);
char const *ui_widget_event_handler_function_name(long function_index);
char *tag_get_name(long tag_index);

/* ---------- constants */

#define UI_WIDGET_DEFINITION_TAG 'DeLa'
#define UI_WIDGET_COLLECTION_TAG 'Soul'
/* the multiplayer maps' widget collection (menu_tags.c's MULTIPLAYER_COLLECTION) */
#define MULTIPLAYER_COLLECTION "ui\\shell\\multiplayer"

enum
{
	_widget_type_column_list = 3,
	/* (menu_tags.c's: the controller's A, and an event handler's flag) */
	_event_a = 0,
	_event_handler_run_function_bit = 7,
	MAXIMUM_LISTS = 8,
	/* a full screen's height, in the menus' units (a split-screen view's
	screens are shorter) */
	FULL_SCREEN_HEIGHT = 480,
	/* the room kept between a split view's pause screen and its edges */
	SPLIT_MARGIN = 4,
};

/* ---------- structures */

/* the parts of a widget definition this reads (menu_tags.c has it whole) */
struct ce_widget_event_handler
{
	long flags;
	short event_type;
	short function;
	byte unknown008[0x48 - 0x08];
};

struct ce_widget_child
{
	struct tag_reference widget_tag;
	byte unknown010[0x36 - 0x10];
	short vertical_offset;
	byte unknown038[0x50 - 0x38];
};

struct ce_widget_definition
{
	short type;
	short controller_index;
	char name[32];
	rectangle2d bounds;
	byte unknown02C[0x54 - 0x2C];
	struct tag_block event_handlers;
	byte unknown060[0x3E0 - 0x60];
	struct tag_block child_widgets;
};

typedef char verify_ce_widget_event_handler_size[sizeof(struct ce_widget_event_handler) == 0x48 ? 1 : -1];
typedef char verify_ce_widget_child_size[sizeof(struct ce_widget_child) == 0x50 ? 1 : -1];
typedef char verify_ce_widget_child_offset[offsetof(struct ce_widget_child, vertical_offset) == 0x36 ? 1 : -1];
typedef char verify_ce_widget_definition_handlers[
	offsetof(struct ce_widget_definition, event_handlers) == 0x54 ? 1 : -1];
typedef char verify_ce_widget_definition_size[sizeof(struct ce_widget_definition) == 0x3EC ? 1 : -1];
typedef char verify_ce_widget_definition_bounds[offsetof(struct ce_widget_definition, bounds) == 0x24 ? 1 : -1];

/* a block of the menus' tags: its Xbox addresses, [start, end) */
struct menu_block
{
	unsigned long start;
	unsigned long end;
};

/* ---------- globals */

static struct
{
	struct menu_block *blocks;
	long count;
	long room;
	boolean sorted;
} menu_memory;

/* ---------- private code */

/* whether the button's A runs a function this engine has not (one of the PC
version's) */
static boolean ce_button_dead(long button_tag)
{
	struct ce_widget_definition const *button;
	struct ce_widget_event_handler const *handlers;
	long handler;

	if (button_tag == NONE)
		return FALSE;
	button = tag_get(UI_WIDGET_DEFINITION_TAG, button_tag);
	handlers = xbox_pointer(button->event_handlers.address);
	for (handler = 0; handler < button->event_handlers.count; handler++)
	{
		long function = (unsigned short)handlers[handler].function;

		if (handlers[handler].event_type == _event_a &&
			TEST_FLAG(handlers[handler].flags, _event_handler_run_function_bit) &&
			function < PC_MENU_FUNCTION_BASE && !ui_widget_event_handler_function_name(function))
		{
			return TRUE;
		}
	}
	return FALSE;
}

/* the multiplayer pause lists' rows whose A does nothing here, taken out: the
rows below each move up into its place */
static void ce_pause_dead_rows_remove(void)
{
	long collection = tag_loaded(UI_WIDGET_COLLECTION_TAG, MULTIPLAYER_COLLECTION);
	long lists[MAXIMUM_LISTS], list_count = 0;
	struct tag_block const *screens;
	long screen;

	if (collection == NONE)
		return;
	screens = tag_get(UI_WIDGET_COLLECTION_TAG, collection);
	for (screen = 0; screen < screens->count; screen++)
	{
		long screen_tag = XBOX_POINTER(struct tag_reference const, screens->address)[screen].index;
		struct ce_widget_definition const *definition;
		struct ce_widget_child const *children;
		long child;

		if (screen_tag == NONE)
			continue;
		definition = tag_get(UI_WIDGET_DEFINITION_TAG, screen_tag);
		children = xbox_pointer(definition->child_widgets.address);
		for (child = 0; child < definition->child_widgets.count; child++)
		{
			long list_tag = children[child].widget_tag.index, seen, row;
			struct ce_widget_definition *list;
			struct ce_widget_child *rows;

			if (list_tag == NONE)
				continue;
			list = tag_get(UI_WIDGET_DEFINITION_TAG, list_tag);
			if (list->type != _widget_type_column_list)
				continue;
			/* (the screens share their list: once each) */
			for (seen = 0; seen < list_count && lists[seen] != list_tag; seen++)
				;
			if (seen < list_count || list_count == MAXIMUM_LISTS)
				continue;
			lists[list_count++] = list_tag;
			rows = xbox_pointer(list->child_widgets.address);
			for (row = 0; row < list->child_widgets.count; row++)
			{
				long below;
				short spacing;

				if (!ce_button_dead(rows[row].widget_tag.index))
					continue;
				platform_log("Custom Edition map: the pause list's row %s runs a function this game has not: "
					"taken out", tag_get_name(rows[row].widget_tag.index));
				spacing = row + 1 < list->child_widgets.count ?
					(short)(rows[row + 1].vertical_offset - rows[row].vertical_offset) : 0;
				for (below = row; below + 1 < list->child_widgets.count; below++)
				{
					rows[below] = rows[below + 1];
					rows[below].vertical_offset = (short)(rows[below].vertical_offset - spacing);
				}
				list->child_widgets.count--;
				row--;
			}
		}
	}
}

/* the multiplayer pause screen's column list (its child's index), NONE for none */
static long ce_screen_list(struct ce_widget_definition const *screen)
{
	struct ce_widget_child const *children = xbox_pointer(screen->child_widgets.address);
	long child;

	for (child = 0; child < screen->child_widgets.count; child++)
	{
		if (children[child].widget_tag.index != NONE &&
			((struct ce_widget_definition const *)tag_get(UI_WIDGET_DEFINITION_TAG,
				children[child].widget_tag.index))->type == _widget_type_column_list)
		{
			return child;
		}
	}
	return NONE;
}

/* the split-screen pause screens laid out as the one player's (above) */
static void ce_pause_split_layout(void)
{
	long collection = tag_loaded(UI_WIDGET_COLLECTION_TAG, MULTIPLAYER_COLLECTION);
	struct tag_block const *screens;
	struct tag_reference const *references;
	long screen, template_box_tag = NONE, laid_out = 0;
	short box_to_list = 0;

	if (collection == NONE)
		return;
	screens = tag_get(UI_WIDGET_COLLECTION_TAG, collection);
	references = xbox_pointer(screens->address);
	/* the template: the full screen's, its box the child nearest above its list */
	for (screen = 0; screen < screens->count && template_box_tag == NONE; screen++)
	{
		struct ce_widget_definition const *definition;
		struct ce_widget_child const *children;
		long list, child;

		if (references[screen].index == NONE)
			continue;
		definition = tag_get(UI_WIDGET_DEFINITION_TAG, references[screen].index);
		list = ce_screen_list(definition);
		if (list == NONE || definition->bounds.y1 - definition->bounds.y0 < FULL_SCREEN_HEIGHT)
			continue;
		children = xbox_pointer(definition->child_widgets.address);
		for (child = 0; child < definition->child_widgets.count; child++)
		{
			short above = (short)(children[list].vertical_offset - children[child].vertical_offset);

			if (child != list && children[child].widget_tag.index != NONE && above > 0 &&
				(template_box_tag == NONE || above < box_to_list))
			{
				template_box_tag = children[child].widget_tag.index;
				box_to_list = above;
			}
		}
	}
	if (template_box_tag == NONE)
		return;
	for (screen = 0; screen < screens->count; screen++)
	{
		struct ce_widget_definition const *definition, *list_definition, *last_row;
		struct ce_widget_child *children;
		struct ce_widget_child const *rows;
		long list, box = NONE, child;
		short height, column, top, prompts;

		if (references[screen].index == NONE)
			continue;
		definition = tag_get(UI_WIDGET_DEFINITION_TAG, references[screen].index);
		height = (short)(definition->bounds.y1 - definition->bounds.y0);
		list = ce_screen_list(definition);
		if (list == NONE || height >= FULL_SCREEN_HEIGHT)
			continue;
		children = xbox_pointer(definition->child_widgets.address);
		for (child = 0; child < definition->child_widgets.count; child++)
		{
			if (children[child].widget_tag.index == template_box_tag)
				box = child;
		}
		list_definition = tag_get(UI_WIDGET_DEFINITION_TAG, children[list].widget_tag.index);
		rows = xbox_pointer(list_definition->child_widgets.address);
		if (box == NONE || !list_definition->child_widgets.count ||
			rows[list_definition->child_widgets.count - 1].widget_tag.index == NONE)
		{
			continue;
		}
		/* (the list's rows from its top to the last one's bottom) */
		last_row = tag_get(UI_WIDGET_DEFINITION_TAG, rows[list_definition->child_widgets.count - 1].widget_tag.index);
		column = (short)(rows[list_definition->child_widgets.count - 1].vertical_offset +
			last_row->bounds.y1 - last_row->bounds.y0);
		/* (the prompts: what was below the list's top, the box and list aside) */
		prompts = 0;
		for (child = 0; child < definition->child_widgets.count; child++)
		{
			struct ce_widget_definition const *prompt;

			if (child == box || child == list || children[child].widget_tag.index == NONE ||
				children[child].vertical_offset <= children[list].vertical_offset)
			{
				continue;
			}
			prompt = tag_get(UI_WIDGET_DEFINITION_TAG, children[child].widget_tag.index);
			prompts = (short)MAX(prompts, prompt->bounds.y1 - prompt->bounds.y0);
		}
		top = (short)MAX(SPLIT_MARGIN, (height - (box_to_list + column + prompts)) / 2);
		for (child = 0; child < definition->child_widgets.count; child++)
		{
			if (child == box)
				children[child].vertical_offset = top;
			else if (child == list)
				children[child].vertical_offset = (short)(top + box_to_list);
			else if (children[child].widget_tag.index != NONE && children[child].vertical_offset > children[list].vertical_offset)
				children[child].vertical_offset = (short)(top + box_to_list + column);
		}
		laid_out++;
	}
	if (laid_out)
		platform_log("Custom Edition map: %ld split-screen pause screens laid out as the one player's", laid_out);
}

static int menu_block_compare(void const *a, void const *b)
{
	unsigned long first = ((struct menu_block const *)a)->start, second = ((struct menu_block const *)b)->start;

	return first < second ? -1 : first > second;
}

/* ---------- public code */

void ae_ce_map_tags_loaded(char const *map_name, long scenario_tag_index)
{
	ce_pause_dead_rows_remove();
	menu_tags_loaded(map_name);
	ce_pause_split_layout();
	hud_hires_tags_loaded(scenario_tag_index);
}

void ae_ce_map_menu_block(void const *block, long size)
{
	if (!block || size <= 0)
		return;
	if (menu_memory.count == menu_memory.room)
	{
		long room = menu_memory.room ? menu_memory.room * 2 : 1024;
		struct menu_block *blocks = realloc(menu_memory.blocks, (size_t)room * sizeof(*blocks));

		/* (a block not noted reads as zeroes on a Custom Edition map, as before) */
		if (!blocks)
			return;
		menu_memory.blocks = blocks;
		menu_memory.room = room;
	}
	menu_memory.blocks[menu_memory.count].start = (unsigned long)XBOX_ADDRESS(block);
	menu_memory.blocks[menu_memory.count].end = (unsigned long)XBOX_ADDRESS(block) + (unsigned long)size;
	menu_memory.count++;
	menu_memory.sorted = FALSE;
}

void ae_ce_map_menu_blocks_forget(void)
{
	free(menu_memory.blocks);
	memset(&menu_memory, 0, sizeof(menu_memory));
}

int ae_ce_map_menu_memory(unsigned long address, long size)
{
	long low = 0, high;

	if (size <= 0 || !menu_memory.count)
		return 0;
	if (!menu_memory.sorted)
	{
		qsort(menu_memory.blocks, (size_t)menu_memory.count, sizeof(*menu_memory.blocks), menu_block_compare);
		menu_memory.sorted = TRUE;
	}
	/* (the last block starting at or below the address) */
	high = menu_memory.count - 1;
	while (low < high)
	{
		long middle = (low + high + 1) / 2;

		if (menu_memory.blocks[middle].start <= address)
			low = middle;
		else
			high = middle - 1;
	}
	return menu_memory.blocks[low].start <= address && address < menu_memory.blocks[low].end &&
		(unsigned long)size <= menu_memory.blocks[low].end - address;
}
