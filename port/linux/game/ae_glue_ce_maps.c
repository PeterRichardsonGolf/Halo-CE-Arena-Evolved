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

A Custom Edition map is anyone's (a download, map sharing), and its own
checks (ce_map_checks.c) cover what the port reads, not the menu widgets,
which the game reads only through guarded accessors. menu_tags.c and this
file walk and write them directly, so first every widget they can reach
(the multiplayer collection's screens and the campaign's pause screens, all
the way down) is checked whole in the map's tag cache: tag indices in the
table and of their group, every block's count bounded and its elements in
the tags, no loop (ae_ce_ui_check.c). If any is not, nothing is patched: no
SETTINGS on that map, one log line saying why.

Three things stood in the way, all handled here:
- a Custom Edition map's tags are read only inside its tag cache
  (ce_map_checks.c, ce_tags_pointer: anything else reads as zeroes), and the
  menus' tags are made in memory of their own (menu_tags.c's allocate): the
  added buttons' texts read as none. Each block menu_tags.c makes for a
  Custom Edition map is noted, and a read inside one is let through while
  those tags are loaded; they are forgotten when the menus' tags are let go
  (menu_tags_release, as the map unloads, or when making them fails), and no
  other map's blocks are noted.
- Halo PC's multiplayer pause list has a CHANGE SETTINGS row whose A runs a
  function of the PC version's that this engine has not (the log's "event
  handler function 137 '?' failed"): it does nothing. A pause list's row whose
  A runs a function past this engine's table (and below the menus' own) is
  taken out before the menus' tags are
  added, so the SETTINGS they add takes its place (and the list keeps the
  room it has).
- Halo PC never had split screen: its 2p and 4p pause screens are its one
  player's box and list moved down, run past the bottom of a half or quarter
  view (the list's last rows, and the button prompts drawn over its rows), and
  more so with the rows the menus add. Once the rows are added, each pause
  screen shorter than a full screen gets the one player screen's layout: its
  box with the list as far into it as there, the prompts under the box's
  frame, the whole kept clear of the view's bottom (ce_pause_split_layout).
*/

#include "cseries.h"
#include "math/integer_math.h"
#include "tag_files/tag_groups.h"
#include "halo_menus.h"
#include "ae_glue_ce_maps.h"
#include "ae_ce_ui_check.h"
#include "ce_map_checks.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* (port/linux/game/menu_tags.c, hud_hires_tags.c; the game's) */
void menu_tags_loaded(char const *map_name);
void hud_hires_tags_loaded(long scenario_index);
void platform_log(char const *format, ...);
char const *ui_widget_event_handler_function_name(long function_index);
char *tag_get_name(long tag_index);
void *cache_files_tag_instances(long *count);

/* ---------- constants */

#define UI_WIDGET_DEFINITION_TAG 'DeLa'
#define UI_WIDGET_COLLECTION_TAG 'Soul'
/* the multiplayer maps' widget collection (menu_tags.c's MULTIPLAYER_COLLECTION) */
#define MULTIPLAYER_COLLECTION "ui\\shell\\multiplayer"
/* the campaign's pause screens, which menu_tags.c patches too (its
solo_pause_screens) */
static char const *const solo_pause_screens[] =
{
	"ui\\shell\\solo_game\\pause_game\\pause_game",
	"ui\\shell\\solo_game\\pause_game\\pause_game_split_screen",
};

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
	/* the room kept between a split view's pause screen and its top, and
	between the box's frame and the prompts under it */
	SPLIT_MARGIN = 4,
	PROMPT_GAP = 4,
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
	/* noting blocks (only while a Custom Edition map's menus' tags are made),
	and answering for them (while they are, and once they are, until they are
	let go) */
	boolean collecting;
	boolean active;
} menu_memory;

/* ---------- private code */

/* how many event handler functions this engine has: its table's names run
without a gap to the first NULL (ui_widget_event_handler_function_name: NULL
past the last; tools/test_ae_ce_maps.py checks the table has no unnamed entry
and that its size is the dispatcher's) */
static long ce_engine_function_count(void)
{
	static long count = -1;

	if (count < 0)
	{
		for (count = 0; count < PC_MENU_FUNCTION_BASE && ui_widget_event_handler_function_name(count); count++)
			;
	}
	return count;
}

/* whether the button's A runs a function this engine has not (one of the PC
version's): an index past the engine's table, below the menus' own */
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
			function >= ce_engine_function_count() && function < PC_MENU_FUNCTION_BASE)
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

/* the one player's pause screen, measured before the menus' rows are added:
its box, how far into the box the list starts, the list's room (the box is
drawn that far again below it: Halo PC's pausebox5, 196 units for its 140 of
list) and the box's distance from the bottom of the screen */
struct split_template
{
	long box_tag;
	short box_to_list;
	short box_height;
	short bottom_margin;
};

static boolean ce_pause_split_template(struct split_template *template)
{
	long collection = tag_loaded(UI_WIDGET_COLLECTION_TAG, MULTIPLAYER_COLLECTION);
	struct tag_block const *screens;
	struct tag_reference const *references;
	long screen;

	template->box_tag = NONE;
	if (collection == NONE)
		return FALSE;
	screens = tag_get(UI_WIDGET_COLLECTION_TAG, collection);
	references = xbox_pointer(screens->address);
	for (screen = 0; screen < screens->count && template->box_tag == NONE; screen++)
	{
		struct ce_widget_definition const *definition, *list_definition;
		struct ce_widget_child const *children;
		long list, child, box = NONE;
		short height;

		if (references[screen].index == NONE)
			continue;
		definition = tag_get(UI_WIDGET_DEFINITION_TAG, references[screen].index);
		height = (short)(definition->bounds.y1 - definition->bounds.y0);
		list = ce_screen_list(definition);
		if (list == NONE || height < FULL_SCREEN_HEIGHT)
			continue;
		children = xbox_pointer(definition->child_widgets.address);
		/* (its box: the child nearest above its list) */
		for (child = 0; child < definition->child_widgets.count; child++)
		{
			short above = (short)(children[list].vertical_offset - children[child].vertical_offset);

			if (child != list && children[child].widget_tag.index != NONE && above > 0 &&
				(box == NONE || above < template->box_to_list))
			{
				box = child;
				template->box_to_list = above;
			}
		}
		if (box == NONE)
			continue;
		list_definition = tag_get(UI_WIDGET_DEFINITION_TAG, children[list].widget_tag.index);
		template->box_tag = children[box].widget_tag.index;
		template->box_height = (short)(2 * template->box_to_list +
			list_definition->bounds.y1 - list_definition->bounds.y0);
		template->bottom_margin = (short)(height - children[box].vertical_offset - template->box_height);
	}
	return template->box_tag != NONE;
}

/* the split-screen pause screens laid out as the one player's (above) */
static void ce_pause_split_layout(struct split_template const *template)
{
	long collection = tag_loaded(UI_WIDGET_COLLECTION_TAG, MULTIPLAYER_COLLECTION);
	struct tag_block const *screens;
	struct tag_reference const *references;
	long screen, laid_out = 0;

	if (collection == NONE || template->box_tag == NONE)
		return;
	screens = tag_get(UI_WIDGET_COLLECTION_TAG, collection);
	references = xbox_pointer(screens->address);
	for (screen = 0; screen < screens->count; screen++)
	{
		struct ce_widget_definition const *definition, *list_definition, *last_row;
		struct ce_widget_child *children;
		struct ce_widget_child const *rows;
		long list, box = NONE, child;
		short height, column, top, prompts, prompts_at, content, bottom_margin;

		if (references[screen].index == NONE)
			continue;
		definition = tag_get(UI_WIDGET_DEFINITION_TAG, references[screen].index);
		height = (short)(definition->bounds.y1 - definition->bounds.y0);
		list = ce_screen_list(definition);
		if (list == NONE || height >= FULL_SCREEN_HEIGHT || height <= 0)
			continue;
		children = xbox_pointer(definition->child_widgets.address);
		for (child = 0; child < definition->child_widgets.count; child++)
		{
			if (children[child].widget_tag.index == template->box_tag)
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
		/* the prompts under the box, clear of its frame (or of the rows, where
		they run past it); the whole as far from the view's bottom as the one
		player's box is from the screen's, at the view's scale, as far as the
		view has room for, and no closer to its top than SPLIT_MARGIN */
		prompts_at = (short)(MAX(template->box_height, template->box_to_list + column) + PROMPT_GAP);
		content = (short)(prompts_at + prompts);
		bottom_margin = (short)(template->bottom_margin * height / FULL_SCREEN_HEIGHT);
		top = (short)MAX(SPLIT_MARGIN, MIN((height - content) / 2, height - content - bottom_margin));
		for (child = 0; child < definition->child_widgets.count; child++)
		{
			if (child == box)
				children[child].vertical_offset = top;
			else if (child == list)
				children[child].vertical_offset = (short)(top + template->box_to_list);
			else if (children[child].widget_tag.index != NONE &&
				children[child].vertical_offset > children[list].vertical_offset)
			{
				children[child].vertical_offset = (short)(top + prompts_at);
			}
		}
		laid_out++;
	}
	if (laid_out)
		platform_log("Custom Edition map: %ld split-screen pause screens laid out as the one player's", laid_out);
}

/* whether the pause widgets the menus' tags and this file patch are whole in
the map's tag cache (ae_ce_ui_check.c); FALSE with why */
static boolean ce_pause_widgets_whole(char *why, size_t why_size)
{
#ifdef HALO_CUSTOM_EDITION
	struct ae_ce_ui_image image;
	long instance_count = 0;
	int32_t screens[NUMBEROF(solo_pause_screens)];
	long screen;

	image.bytes = xbox_pointer(CE_IMAGE_TAG_CACHE_BASE);
	image.base = CE_IMAGE_TAG_CACHE_BASE;
	image.size = CE_IMAGE_TAG_CACHE_SIZE;
	image.instances = cache_files_tag_instances(&instance_count);
	image.instance_count = (int32_t)instance_count;
	for (screen = 0; screen < (long)NUMBEROF(solo_pause_screens); screen++)
		screens[screen] = (int32_t)tag_loaded(UI_WIDGET_DEFINITION_TAG, solo_pause_screens[screen]);
	return ae_ce_ui_check(&image, (int32_t)tag_loaded(UI_WIDGET_COLLECTION_TAG, MULTIPLAYER_COLLECTION), screens,
		(int)NUMBEROF(solo_pause_screens), why, why_size) != 0;
#else
	snprintf(why, why_size, "no Custom Edition support");
	return FALSE;
#endif
}

static int menu_block_compare(void const *a, void const *b)
{
	unsigned long first = ((struct menu_block const *)a)->start, second = ((struct menu_block const *)b)->start;

	return first < second ? -1 : first > second;
}

/* ---------- public code */

void ae_ce_map_tags_loaded(char const *map_name, long scenario_tag_index)
{
	char why[160];
	struct split_template template;

	/* (the map is anyone's: its pause widgets walked and patched only once
	every one of them is found whole in its tags) */
	if (ce_pause_widgets_whole(why, sizeof(why)))
	{
		ce_pause_dead_rows_remove();
		ce_pause_split_template(&template);
		ae_ce_map_menu_blocks_forget();
		menu_memory.collecting = TRUE;
		menu_tags_loaded(map_name);
		menu_memory.collecting = FALSE;
		menu_memory.active = menu_memory.count > 0;
		ce_pause_split_layout(&template);
	}
	else
	{
		platform_log("Custom Edition map: its pause menu's widgets are not whole (%s): no SETTINGS on this map", why);
	}
	hud_hires_tags_loaded(scenario_tag_index);
}

void ae_ce_map_menu_block(void const *block, long size)
{
	if (!menu_memory.collecting || !block || size <= 0)
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
	/* (the game's free takes no NULL) */
	if (menu_memory.blocks)
		free(menu_memory.blocks);
	memset(&menu_memory, 0, sizeof(menu_memory));
}

int ae_ce_map_menu_memory(unsigned long address, long size)
{
	long low = 0, high;

	if (size <= 0 || !menu_memory.count || !(menu_memory.collecting || menu_memory.active))
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
