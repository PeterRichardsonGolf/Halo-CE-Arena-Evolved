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
  more so with the rows the menus add. Their GAME OPTIONS targets are full
  screen and force player one. Once the rows are added, the split pauses and
  every screen their rows open inherit the pausing player and fit that view
  (ce_pause_split_layout).
*/

#include "cseries.h"
#include "game/game_engine.h"
#include "interface/event_manager.h"
#include "interface/ui_widget_event_handler_functions.h"
#include "main/main.h"
#include "math/integer_math.h"
#include "networking/network_client_manager.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "networking/network_server_manager.h"
#include "tag_files/tag_groups.h"
#include "items/projectile_definitions.h"
#include "errors.h"
#include "halo_menus.h"
#include "ae_glue_ce_maps.h"
#include "ae_layout.h"
#include "ae_map_look_fixes.h"
#include "ae_ce_ui_check.h"
#include "ce_map_checks.h"
#include "ae_weapon_pack.h"

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
/* the maps' small "=CANCEL" prompt (ui\shell\main_menu\=cancel_sm, beside B in the pause box) is 63 wide in their
tags, which the port's high resolution text outgrows (its L cut at 1080p); 72 still ends before the A button that
button_key_sm puts 90 along, 16 past it */
#define PROMPT_CANCEL_WIDTH 72
/* the campaign's pause screens, which menu_tags.c patches too (its
solo_pause_screens) */
static char const *const solo_pause_screens[] =
{
	"ui\\shell\\solo_game\\pause_game\\pause_game",
	"ui\\shell\\solo_game\\pause_game\\pause_game_split_screen",
};

enum
{
	_widget_type_container = 0,
	_widget_type_column_list = 3,
	/* (menu_tags.c's: the controller's A, and an event handler's flag) */
	_event_a = 0,
	_event_handler_open_widget_bit = 3,
	_event_handler_run_function_bit = 7,
	_widget_always_use_tag_controller_index_bit = 12,
	_widget_controller_any = 4,
	MAXIMUM_LISTS = 32,
	MAXIMUM_PAUSE_WIDGETS = 512,
	MAXIMUM_SPLIT_SCREENS = 64,
	PC_FUNCTION_PAUSE_LIST_INITIALIZE = 155,
	PC_FUNCTION_GAME_OPTIONS_INITIALIZE = 156,
	PC_FUNCTION_CHOOSE_TEAM = 157,
	PC_FUNCTION_RESTART_GAME = 163,
	XBOX_FUNCTION_SWAP_TEAM = 32,
	AE_CLIENT_STATE_PREGAME = 2,
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
	struct tag_reference widget_tag;
	byte unknown018[0x48 - 0x18];
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
	long flags;
	byte unknown030[0x54 - 0x30];
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

static boolean ce_pause_active;
static short restart_state;

typedef boolean (*ce_pc_function)(struct widget_instance *widget, struct event_record *event,
	boolean *widget_deleted);

static boolean ce_list_initialize(struct widget_instance *widget, struct event_record *event,
	boolean *widget_deleted);
static boolean ce_choose_team(struct widget_instance *widget, struct event_record *event,
	boolean *widget_deleted);
static boolean ce_restart_game(struct widget_instance *widget, struct event_record *event,
	boolean *widget_deleted);

static struct
{
	short pc_function;
	ce_pc_function function;
} const ce_pc_functions[] =
{
	{ PC_FUNCTION_PAUSE_LIST_INITIALIZE, ce_list_initialize },
	{ PC_FUNCTION_GAME_OPTIONS_INITIALIZE, ce_list_initialize },
	{ PC_FUNCTION_CHOOSE_TEAM, ce_choose_team },
	{ PC_FUNCTION_RESTART_GAME, ce_restart_game },
};

struct split_screen
{
	long tag;
	short height;
	/* the screen's own width before it was laid out: the width it is centred as */
	short design_width;
	boolean pause;
};

static struct
{
	struct split_screen screens[MAXIMUM_SPLIT_SCREENS];
	long count;
} split_screens;

/* ---------- private code */

static boolean ce_list_initialize(struct widget_instance *widget, struct event_record *event,
	boolean *widget_deleted)
{
	(void)widget;
	(void)event;
	(void)widget_deleted;
	return TRUE;
}

/* The Xbox pause menu's function 32 finds this machine's player by the
event's controller, changes its team, and sends the same client update. */
static boolean ce_choose_team(struct widget_instance *widget, struct event_record *event,
	boolean *widget_deleted)
{
	boolean result = ui_widget_event_handler_function_invoke(widget, event, XBOX_FUNCTION_SWAP_TEAM, widget_deleted);

	if (result)
		platform_log("Custom Edition map: controller %d changed team from CHOOSE TEAM",
			(int)event->controller_index + 1);
	return result;
}

static boolean ce_restart_game(struct widget_instance *widget, struct event_record *event,
	boolean *widget_deleted)
{
	(void)widget;
	(void)event;
	(void)widget_deleted;
	if (!global_network_game_server_get() || !game_engine_running() || game_engine_showing_postgame())
		return FALSE;
	/* (the lobby the server returns to is set up from the game's "current stage": this game's map and gametype,
	not the menu background's) */
	{
		struct network_game const *game = network_game_get_game();

		if (game)
		{
			main_set_multiplayer_map_name(game->map.name);
			game_engine_override_map_name(game->map.name);
			game_engine_override_game_variant(&game->variant);
		}
	}
	restart_state = 1;
	game_engine_end_game();
	platform_log("Custom Edition map: restarting the current game");
	return TRUE;
}

static boolean ce_pc_function_supported(long function)
{
	long index;

	for (index = 0; index < (long)NUMBEROF(ce_pc_functions); index++)
	{
		if (ce_pc_functions[index].pc_function == function)
			return TRUE;
	}
	return FALSE;
}

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
			return !ce_pc_function_supported(function);
		}
	}
	return FALSE;
}

/* the multiplayer pause lists' rows whose A does nothing here, taken out: the
rows below each move up into its place */
static char const *ce_pause_row_remove_reason(long row_tag, boolean host, boolean teams)
{
	char const *name = row_tag != NONE ? tag_get_name(row_tag) : NULL;

	if (!name)
		return NULL;
	if (strstr(name, "\\new_game_button"))
		return "its PC lobby widget is not safe in this engine";
	if (!host && strstr(name, "\\restart_game_button"))
		return "only the host may restart the game";
	if (!teams && strstr(name, "\\choose_team_button"))
		return "there are no teams in this game";
	return NULL;
}

/* whether a widget opened by a handler belongs to the map's multiplayer pause family (the screens under
multiplayer_game): its handlers also reach the settings screens and the main menu, whose rows are not the pause's */
static boolean ce_pause_family(long tag)
{
	char const *name = tag != NONE ? tag_get_name(tag) : NULL;

	return name && strstr(name, "ui\\shell\\multiplayer_game\\") == name;
}

static void ce_pause_lists_collect(long tag, long *lists, long *list_count, long *widgets, long *widget_count)
{
	struct ce_widget_definition const *definition;
	struct ce_widget_event_handler const *handlers;
	struct ce_widget_child const *children;
	long index;

	if (tag == NONE)
		return;
	for (index = 0; index < *widget_count; index++)
	{
		if (widgets[index] == tag)
			return;
	}
	if (*widget_count >= MAXIMUM_PAUSE_WIDGETS)
		return;
	widgets[(*widget_count)++] = tag;
	definition = tag_get(UI_WIDGET_DEFINITION_TAG, tag);
	if (definition->type == _widget_type_column_list && *list_count < MAXIMUM_LISTS)
		lists[(*list_count)++] = tag;
	handlers = xbox_pointer(definition->event_handlers.address);
	for (index = 0; index < definition->event_handlers.count; index++)
	{
		if (TEST_FLAG(handlers[index].flags, _event_handler_open_widget_bit) &&
			ce_pause_family(handlers[index].widget_tag.index))
			ce_pause_lists_collect(handlers[index].widget_tag.index, lists, list_count, widgets, widget_count);
	}
	children = xbox_pointer(definition->child_widgets.address);
	for (index = 0; index < definition->child_widgets.count; index++)
		ce_pause_lists_collect(children[index].widget_tag.index, lists, list_count, widgets, widget_count);
}

static void ce_pause_dead_rows_remove(void)
{
	long collection = tag_loaded(UI_WIDGET_COLLECTION_TAG, MULTIPLAYER_COLLECTION);
	long lists[MAXIMUM_LISTS], list_count = 0, widgets[MAXIMUM_PAUSE_WIDGETS], widget_count = 0;
	struct tag_block const *screens;
	struct network_game const *game = network_game_get_game();
	boolean host = global_network_game_server_get() != NULL;
	boolean teams = game && game->variant.universal_variant.teams;
	long screen, list_index;

	if (collection == NONE)
		return;
	screens = tag_get(UI_WIDGET_COLLECTION_TAG, collection);
	for (screen = 0; screen < screens->count; screen++)
		ce_pause_lists_collect(XBOX_POINTER(struct tag_reference const, screens->address)[screen].index,
			lists, &list_count, widgets, &widget_count);
	for (list_index = 0; list_index < list_count; list_index++)
	{
		struct ce_widget_definition *list = tag_get(UI_WIDGET_DEFINITION_TAG, lists[list_index]);
		struct ce_widget_child *rows = xbox_pointer(list->child_widgets.address);
		long row;

		for (row = 0; row < list->child_widgets.count; row++)
		{
			char const *reason = ce_pause_row_remove_reason(rows[row].widget_tag.index, host, teams);
			long below;
			short spacing;

			if (!reason && !ce_button_dead(rows[row].widget_tag.index))
				continue;
			if (reason)
				platform_log("Custom Edition map: the pause list's row %s taken out: %s",
					tag_get_name(rows[row].widget_tag.index), reason);
			else
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

/* whether a row of the multiplayer pause lists runs a function this engine has not (Halo PC's pause menus: CHANGE
SETTINGS), as ce_pause_dead_rows_remove finds them */
static boolean ce_pause_has_dead_rows(void)
{
	long collection = tag_loaded(UI_WIDGET_COLLECTION_TAG, MULTIPLAYER_COLLECTION);
	long lists[MAXIMUM_LISTS], list_count = 0, widgets[MAXIMUM_PAUSE_WIDGETS], widget_count = 0;
	struct tag_block const *screens;
	long screen, list_index;

	if (collection == NONE)
		return FALSE;
	screens = tag_get(UI_WIDGET_COLLECTION_TAG, collection);
	for (screen = 0; screen < screens->count; screen++)
		ce_pause_lists_collect(XBOX_POINTER(struct tag_reference const, screens->address)[screen].index,
			lists, &list_count, widgets, &widget_count);
	for (list_index = 0; list_index < list_count; list_index++)
	{
		struct ce_widget_definition const *list = tag_get(UI_WIDGET_DEFINITION_TAG, lists[list_index]);
		struct ce_widget_child const *rows = xbox_pointer(list->child_widgets.address);
		long row;

		for (row = 0; row < list->child_widgets.count; row++)
		{
			if (ce_button_dead(rows[row].widget_tag.index))
				return TRUE;
		}
	}
	return FALSE;
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

static void ce_split_screen_add(long tag, short height, boolean pause)
{
	long screen;

	if (tag == NONE)
		return;
	for (screen = 0; screen < split_screens.count; screen++)
	{
		if (split_screens.screens[screen].tag == tag)
		{
			if (pause)
				split_screens.screens[screen].pause = TRUE;
			return;
		}
	}
	if (split_screens.count < MAXIMUM_SPLIT_SCREENS)
	{
		split_screens.screens[split_screens.count].tag = tag;
		split_screens.screens[split_screens.count].height = height;
		split_screens.screens[split_screens.count].pause = pause;
		split_screens.count++;
	}
}

/* screens opened from a split pause, including handlers on its rows */
static void ce_split_screens_collect(long tag, short height, boolean screen, boolean pause)
{
	struct ce_widget_definition const *definition;
	struct ce_widget_event_handler const *handlers;
	struct ce_widget_child const *children;
	long handler, child;

	if (tag == NONE)
		return;
	if (screen)
	{
		long before = split_screens.count;

		ce_split_screen_add(tag, height, pause);
		if (before == split_screens.count)
			return;
	}
	definition = tag_get(UI_WIDGET_DEFINITION_TAG, tag);
	handlers = xbox_pointer(definition->event_handlers.address);
	for (handler = 0; handler < definition->event_handlers.count; handler++)
	{
		if (TEST_FLAG(handlers[handler].flags, _event_handler_open_widget_bit) &&
			ce_pause_family(handlers[handler].widget_tag.index))
			ce_split_screens_collect(handlers[handler].widget_tag.index, height, TRUE, FALSE);
	}
	children = xbox_pointer(definition->child_widgets.address);
	for (child = 0; child < definition->child_widgets.count; child++)
		ce_split_screens_collect(children[child].widget_tag.index, height, FALSE, FALSE);
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

/* the split-screen pause screens and the screens they open, fitted to a view */
static void ce_pause_split_layout(struct split_template const *template)
{
	long collection = tag_loaded(UI_WIDGET_COLLECTION_TAG, MULTIPLAYER_COLLECTION);
	struct tag_block const *screens;
	struct tag_reference const *references;
	long screen, laid_out = 0, children_laid_out = 0;

	if (collection == NONE || template->box_tag == NONE)
		return;
	screens = tag_get(UI_WIDGET_COLLECTION_TAG, collection);
	references = xbox_pointer(screens->address);
	memset(&split_screens, 0, sizeof(split_screens));
	for (screen = 0; screen < screens->count; screen++)
	{
		struct ce_widget_definition const *definition;
		struct ce_widget_child const *children;
		long child;
		short height;
		boolean pause_screen = FALSE;

		if (references[screen].index == NONE)
			continue;
		definition = tag_get(UI_WIDGET_DEFINITION_TAG, references[screen].index);
		height = (short)(definition->bounds.y1 - definition->bounds.y0);
		children = xbox_pointer(definition->child_widgets.address);
		for (child = 0; child < definition->child_widgets.count; child++)
		{
			if (children[child].widget_tag.index == template->box_tag)
				pause_screen = TRUE;
		}
		if (pause_screen && height > 0 && height < FULL_SCREEN_HEIGHT && ce_screen_list(definition) != NONE)
			ce_split_screens_collect(references[screen].index, height, TRUE, TRUE);
	}
	for (screen = 0; screen < split_screens.count; screen++)
	{
		struct ce_widget_definition *definition;
		struct ce_widget_definition const *list_definition, *last_row;
		struct ce_widget_child *children;
		struct ce_widget_child const *rows;
		long list, box = NONE, child;
		short box_top, list_top, height, column, top, prompts, prompts_at, content, bottom_margin, box_to_list, box_height;
		boolean pause_screen;

		definition = tag_get(UI_WIDGET_DEFINITION_TAG, split_screens.screens[screen].tag);
		height = split_screens.screens[screen].height;
		list = ce_screen_list(definition);
		split_screens.screens[screen].design_width = (short)(definition->bounds.x1 - definition->bounds.x0);
		definition->controller_index = _widget_controller_any;
		SET_FLAG(definition->flags, _widget_always_use_tag_controller_index_bit, FALSE);
		if (list == NONE)
			continue;
		children = xbox_pointer(definition->child_widgets.address);
		pause_screen = split_screens.screens[screen].pause;
		for (child = 0; pause_screen && child < definition->child_widgets.count; child++)
		{
			if (children[child].widget_tag.index == template->box_tag)
				box = child;
		}
		if (box == NONE)
		{
			/* (the frame: the biggest background above the list; a title or text over it is not) */
			short biggest = 0;

			for (child = 0; child < definition->child_widgets.count; child++)
			{
				if (child != list && children[child].widget_tag.index != NONE &&
					children[child].vertical_offset <= children[list].vertical_offset)
				{
					struct ce_widget_definition const *candidate =
						tag_get(UI_WIDGET_DEFINITION_TAG, children[child].widget_tag.index);
					short size = (short)(candidate->bounds.y1 - candidate->bounds.y0);

					if (candidate->type == _widget_type_container && size > biggest)
					{
						box = child;
						biggest = size;
					}
				}
			}
		}
		list_definition = tag_get(UI_WIDGET_DEFINITION_TAG, children[list].widget_tag.index);
		rows = xbox_pointer(list_definition->child_widgets.address);
		if (box == NONE || !list_definition->child_widgets.count ||
			rows[list_definition->child_widgets.count - 1].widget_tag.index == NONE)
		{
			continue;
		}
		box_to_list = pause_screen ? template->box_to_list :
			(short)(children[list].vertical_offset - children[box].vertical_offset);
		box_height = pause_screen ? template->box_height :
			(short)(2 * box_to_list + list_definition->bounds.y1 - list_definition->bounds.y0);
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
		prompts_at = (short)(MAX(box_height, box_to_list + column) + PROMPT_GAP);
		content = (short)(prompts_at + prompts);
		bottom_margin = pause_screen ? (short)(template->bottom_margin * height / FULL_SCREEN_HEIGHT) : SPLIT_MARGIN;
		top = (short)MAX(SPLIT_MARGIN, MIN((height - content) / 2, height - content - bottom_margin));
		box_top = children[box].vertical_offset;
		list_top = children[list].vertical_offset;
		for (child = 0; child < definition->child_widgets.count; child++)
		{
			if (child == box)
				children[child].vertical_offset = top;
			else if (child != list && children[child].widget_tag.index != NONE &&
				children[child].vertical_offset <= list_top)
			{
				/* (a title over the frame keeps its place in it) */
				children[child].vertical_offset = (short)(top + children[child].vertical_offset - box_top);
			}
			else if (child == list)
				children[child].vertical_offset = (short)(top + box_to_list);
			else if (children[child].widget_tag.index != NONE &&
				children[child].vertical_offset > list_top)
			{
				children[child].vertical_offset = (short)(top + prompts_at);
			}
		}
		/* A 640x480 flat background takes the renderer's whole-screen path,
		which widens its bounds and clip around the menu centering. */
		if (definition->bounds.x0 == 0 && (definition->bounds.x1 == 640 || definition->bounds.x1 == 320))
		{
			/* (a quarter-screen pause screen too: its dim must cover the whole view; its centring in the view
			is by its own width: ae_ce_map_view_offset) */
			definition->bounds.x1 = 640;
			definition->bounds.y0 = 0;
			definition->bounds.y1 = FULL_SCREEN_HEIGHT;
		}
		if (pause_screen)
			laid_out++;
		else
			children_laid_out++;
	}
	if (laid_out)
		platform_log("Custom Edition map: %ld split-screen pause screens laid out as the one player's", laid_out);
	if (children_laid_out)
		platform_log("Custom Edition map: %ld screens opened from a split-screen pause fitted to its player's view",
			children_laid_out);
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
		ce_pause_active = TRUE;
	}
	else
	{
		platform_log("Custom Edition map: its pause menu's widgets are not whole (%s): no SETTINGS on this map", why);
	}
	hud_hires_tags_loaded(scenario_tag_index);
}

/* (cache_files.c, an AE hook line before the menus' tags of an Xbox map) An Xbox map that carries Halo PC's pause
menus (Halo OG's Digsite maps: a CHANGE SETTINGS row whose function this engine has not, and Halo PC's split screen
pauses that run past the bottom of a half view) gets the Custom Edition maps' repair: the dead rows taken out, the
menus' tags added, the split pauses fitted to their view. TRUE when it added the menus' tags itself; a stock map (no
such row) is left to the usual call */
int ae_xbox_map_menu_tags_loaded(char const *map_name)
{
	struct split_template template;

	if (!map_name || !strcmp(map_name, "ui") || !ce_pause_has_dead_rows())
		return FALSE;
	platform_log("Xbox map %s: Halo PC's pause menus: fitted as a Custom Edition map's", map_name);
	ce_pause_dead_rows_remove();
	ce_pause_split_template(&template);
	menu_tags_loaded(map_name);
	ce_pause_split_layout(&template);
	ce_pause_active = TRUE;
	return TRUE;
}

int ae_ce_map_building_menu_tags(void)
{
	return menu_memory.collecting;
}

int ae_ce_map_event_function(struct widget_instance *widget, struct event_record *event,
	word function_index, boolean *widget_deleted, boolean *result)
{
	long index;

	if (!ce_pause_active)
		return 0;
	for (index = 0; index < (long)NUMBEROF(ce_pc_functions); index++)
	{
		if ((word)ce_pc_functions[index].pc_function == function_index)
		{
			*result = ce_pc_functions[index].function(widget, event, widget_deleted);
			return 1;
		}
	}
	return 0;
}

void ae_ce_map_prompts_fit(void)
{
	long tag = tag_loaded(UI_WIDGET_DEFINITION_TAG, "ui\\shell\\main_menu\\=cancel_sm");
	struct ce_widget_definition *definition;

	if (tag == NONE)
		return;
	definition = tag_get(UI_WIDGET_DEFINITION_TAG, tag);
	if (definition->bounds.x1 - definition->bounds.x0 < PROMPT_CANCEL_WIDTH)
		definition->bounds.x1 = (short)(definition->bounds.x0 + PROMPT_CANCEL_WIDTH);
}

void ae_ce_map_update(void)
{
	struct network_game_server *server;
	struct network_game_client *client;

	if (!restart_state)
		return;
	server = global_network_game_server_get();
	client = global_network_game_client_get();
	if (!server || !client)
	{
		platform_log("Custom Edition map: restart stopped because the network game ended");
		restart_state = 0;
		return;
	}
	if (restart_state == 1 && game_engine_showing_postgame())
	{
		if (network_game_server_reset_to_pregame(server))
		{
			restart_state = 2;
			platform_log("Custom Edition map: restart returned the server to the lobby");
		}
	}
	else if (restart_state == 2 && network_game_client_get_state(client, NULL) == AE_CLIENT_STATE_PREGAME)
	{
		/* (the postgame's map choice held the countdown, as network_test.c's next game frees it) */
		network_game_server_pause_countdown(server, FALSE);
		network_game_client_request_immediate_start();
		restart_state = 3;
		platform_log("Custom Edition map: restart requested START");
	}
	else if (restart_state == 3 && network_game_client_get_state(client, NULL) > AE_CLIENT_STATE_PREGAME &&
		game_engine_can_score())
	{
		restart_state = 0;
		platform_log("Custom Edition map: current game restarted");
	}
}

/* the menus add (screen width - 640) / 2 to a widget's place; a native screen of its own width in a view of
less than the screen is centred in the view instead: the view's width, less the screen's, halved. */
void ae_ce_map_view_place(long tag_index, rectangle2d const *window_bounds, point2d *offset, rectangle2d *bounds)
{
	long screen;

	if (!ce_pause_active || !window_bounds)
		return;
	for (screen = 0; screen < split_screens.count; screen++)
	{
		if (split_screens.screens[screen].tag == tag_index)
		{
			/* (the view's bounds are in the menus' units already) */
			long units = window_bounds->x1 - window_bounds->x0;
			short shift;

			if (units <= 0)
				return;
			shift = (short)((units - split_screens.screens[screen].design_width) / 2 - (halo_screen_width() - 640) / 2);
			offset->x = (short)(offset->x + shift);
			/* (the bitmaps' clip is the bounds moved with the offset: opened out by the shift, it ends where the view does) */
			if (shift < 0)
				bounds->x1 = (short)(bounds->x1 - shift);
			else
				bounds->x0 = (short)(bounds->x0 - shift);
			return;
		}
	}
}

void ae_ce_map_widget_opened(long tag_index, short local_player_index)
{
	long screen;

	for (screen = 0; screen < split_screens.count; screen++)
	{
		if (split_screens.screens[screen].tag == tag_index)
		{
			platform_log("Custom Edition map: split-pause screen %s opened for local player %d",
				tag_get_name(tag_index), (int)local_player_index + 1);
			return;
		}
	}
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
	memset(&split_screens, 0, sizeof(split_screens));
	ce_pause_active = FALSE;
}

int ae_ce_map_menu_memory(unsigned long address, long size)
{
	long low = 0, high;

	/* (a map layout's item list, read in place of the map's: ae_layout.c) */
	if (ae_layout_memory(address, size))
		return 1;
	/* (a pack's own tags, added to the map in memory of their own: ae_weapon_pack.c) */
	if (ae_weapon_pack_memory(address, size))
		return 1;
	/* (a map's BSP's portal list given by a look fix, in place of the map's: ae_map_look_fixes.c) */
	if (ae_map_look_fixes_memory(address, size))
		return 1;
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

/* (ce_repairs.c's ce_repairs_tags_loaded, AE hook) PROJECTILES TOO FAST FOR 32-BIT FLOATS: some Custom Edition maps
give a bullet a speed of 1e31 world units a second ("Halo 2 for CE"'s H2 Blood Gulch: three projectiles), which Halo
PC's x87 registers carried as a hitscan; in 32-bit floats its square overflows (projectiles.c's deceleration from
distances: inf - inf), the bullet's velocity and place turn NaN on its first tick, and it never hits anything (owner
2026-10-16: the map's battle rifle and SMG hurt nobody; upstream's bug too, OpenCE build-206 and ChupathingyCE 0.7.5b).
Both speeds of such a projectile are scaled down to AE_CE_PROJECTILE_SPEED_LIMIT (world units a tick, as the tags hold
them once loaded), their ratio kept: at that speed a bullet still crosses any map in one tick, its damage and its
slowing curve are as before. Both machines of a game repair the same map alike: no network change */
#ifdef HALO_CUSTOM_EDITION
#define AE_CE_PROJECTILE_SPEED_LIMIT 1.0e12f

void ae_ce_projectile_speeds(void *tag_instances, long tag_count)
{
	long index, repaired = 0;

	for (index = 0; tag_instances && index < tag_count; index++)
	{
		struct ce_tag_instance const *instance =
			(struct ce_tag_instance const *)((byte const *)tag_instances + index * CE_TAG_INSTANCE_SIZE);
		struct projectile_definition *definition;
		real fastest, scale;

		if (instance->group_tag != PROJECTILE_DEFINITION_TAG || instance->base_address < CE_IMAGE_TAG_CACHE_BASE ||
			instance->base_address - CE_IMAGE_TAG_CACHE_BASE >
			(unsigned long)CE_IMAGE_TAG_CACHE_SIZE - sizeof(struct projectile_definition))
		{
			continue;
		}
		definition = (struct projectile_definition *)xbox_pointer(instance->base_address);
		fastest = definition->projectile.initial_velocity > definition->projectile.final_velocity ?
			definition->projectile.initial_velocity : definition->projectile.final_velocity;
		/* (NaN fails every comparison: such a tag is left alone) */
		if (!(fastest > AE_CE_PROJECTILE_SPEED_LIMIT))
			continue;
		scale = AE_CE_PROJECTILE_SPEED_LIMIT / fastest;
		error(_error_silent, "Custom Edition map: projectile tag %ld's speed %g (initial %g, final %g, a tick) scaled to "
			"%g, too fast for 32-bit floats", index, (double)fastest, (double)definition->projectile.initial_velocity,
			(double)definition->projectile.final_velocity, (double)AE_CE_PROJECTILE_SPEED_LIMIT);
		definition->projectile.initial_velocity *= scale;
		definition->projectile.final_velocity *= scale;
		repaired++;
	}
	if (repaired)
		error(_error_silent, "Custom Edition map: %ld projectile%s scaled to the float range", repaired,
			repaired == 1 ? "" : "s");
}
#endif /* (only the builds that read Custom Edition maps: the web build has none) */
