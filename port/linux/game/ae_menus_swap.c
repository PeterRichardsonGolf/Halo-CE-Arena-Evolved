/*
AE_MENUS_SWAP.C

MENUS on the main menu (item 81; ae_menus_swap.h): the menu style switched
live, from the main menu, in either style.

MENUS opens the styles in place: a screen that is the main menu again (its
logo, its build number, where they were) with the styles' rows where its own
rows begin, drawn as its rows are (pictures of their text, the same glow,
highlight and sounds: tools/menu_row_art.py), the style in use focused and
marked with a dot. B, Esc and BACK go back to the main menu with MENUS
focused (the history's own pop). Picking the style in use goes back too;
picking another switches at once: a short fade to black, display.menus (and
display.arena_menus) written to config.toml, the menus' tags built again for
the new style while the screen is black (as Damnation CE's MENUS does: every
screen closed, the texture cache flushed, menu_tags.c's tags let go and made
again), the main menu opened again (main_screen_shell_load, so its music
plays on: it starts only when it is not playing), MENUS focused, and the fade
back in. REDUCE MOTION (display.arena_menus_reduce_motion) cuts instead.

The PC version's menus (display.menus "pc") are menu files: MENUS is a row of
main_menu.xml above QUIT, and the styles' screen is main_menu.menus_select.xml
(tools/port_settings.py), whose ARENA EVOLVED row is taken out here when this
build has no AE menus.

The Xbox's menus (display.menus "xbox", or the PC version's when they cannot
be built) are the map's own, and menu_tags.c adds nothing to them: their
MENUS is made here, as a few tags of its own added to the map's when ui.map
loads. Its main menu's list has a fourth child that Bungie left for GAME
DEMOS (hidden and disabled on the retail discs by its "disable if no xdemos"
handler), under SETTINGS at their spacing: that child is pointed at a copy of
SETTINGS (its look) with MENUS's picture and the port's handlers. Its styles'
screen is a copy of the main menu with a copy of its list holding the styles'
rows, copies of SETTINGS too. Nothing here depends on the menu files, so a
player whose own files break the PC version's menus still has MENUS.

MENUS is refused (its A does nothing but the failure sound) away from the
plain main menu: in a network game, with a lobby, host or client up, or with
a second player on this machine (pc_menu_split_players).
*/

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "bitmaps/bitmap_group.h"
#include "cache/texture_cache.h"
#include "interface/event_manager.h"
#include "interface/ui_widget.h"
#include "interface/ui_widget_instance.h"
#include "networking/network_game_globals.h"
#include "rasterizer/xbox/rasterizer_xbox_hardware_bitmaps.h"
#include "tag_files/tag_groups.h"

#include "halo_menus.h"
#include "ae_menus_swap.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Arena Evolved's own menus (M4's main menu): the ARENA EVOLVED row is shown
only in a build that has them */
#if defined(__has_include)
#if __has_include("ae_screen_main_menu.h")
#define AE_MENUS_SWAP_ARENA 1
#endif
#endif
#ifndef AE_MENUS_SWAP_ARENA
#define AE_MENUS_SWAP_ARENA 0
#endif

/* the platform layer's and the game's (declared as the port's other units do) */
int platform_restart(void);
void platform_log(char const *format, ...);
char const *config_string(char const *name);
int config_boolean(char const *name);
int config_write(char const *name, char const *value);
boolean network_game_is_active(void);
void *cache_files_tag_instances(long *count);
void cache_files_set_tag_instances(void *instances, long count);
char *tag_get_name(long tag_index);
void menu_tags_loaded(char const *map_name);
void menu_tags_unloaded(void);
char const *pc_menu_function_name(long function_index);
unsigned char pc_menu_split_players(void);
boolean ui_widget_port_dispatch_event(struct widget_instance *widget, short event_type, short controller_index,
	boolean *deleted);

/* ---------- constants */

#define UI_WIDGET_DEFINITION_TAG 'DeLa'
#define TAG_PREFIX "ae\\menus_swap\\"
#define XBOX_MAIN_MENU "ui\\shell\\main_menu\\main_menu"
#define XBOX_MAIN_MENU_LIST "ui\\shell\\main_menu\\main_menu_select_list"
#define XBOX_SETTINGS_ITEM "ui\\shell\\main_menu\\main_menu_item_settings"
#define XBOX_DEMOS_ITEM "ui\\shell\\main_menu\\main_menu_item_game_demos"
/* the PC version's styles' list (main_menu.menus_select.xml) and its ARENA EVOLVED row */
#define PC_STYLES_LIST "pc\\main_menu\\menus_select\\menus_select_list"
#define PC_ARENA_ROW "pc\\main_menu\\menus_select\\menus_item_arena"
/* the Xbox's rows' pictures (tools/menu_row_art.py) */
#define XBOX_ART "ce/port/main_menu/xbox_style/"
/* the main menu's rows' spacing (the styles' rows and BACK too) */
#define ROW_SPACING 36
/* the fade each way (milliseconds) */
#define FADE_TIME 250
#define PLACEHOLDER_SIZE 4

enum
{
	_event_a = 0,
	_event_b = 1,
	_event_start = 12,
	_event_back = 13,
	_event_created = 24,
	_event_custom_activation = 32,
};

enum
{
	_handler_open_widget_bit = 3,
	_handler_run_function_bit = 7,
	_handler_go_back_bit = 9,
};

enum
{
	_bitmap_type_2d = 0,
	_bitmap_format_a8r8g8b8 = 11,
	_bitmap_has_power_of_two_dimensions_bit = 0,
};

/* the audio feedback sounds (ui_widget.c's private enum) */
enum
{
	_feedback_forward = 2,
	_feedback_failure = 4,
};

enum style
{
	STYLE_PC,
	STYLE_XBOX,
	STYLE_ARENA,
	NUMBER_OF_STYLES
};

static char const *const style_names[NUMBER_OF_STYLES] = { "pc", "xbox", "arena" };
/* (each row's tag name ends with its style's: menus_item_<style>) */
static char const *const style_rows[NUMBER_OF_STYLES] =
{
	"\\menus_item_pc", "\\menus_item_xbox", "\\menus_item_arena"
};

/* ---------- structures (the widget definition as ui_widget.c has it: menu_tags.c's) */

struct cache_file_tag_instance
{
	long group_tag;
	long parent_group_tags[2];
	long tag_index;
	XPTR(char) name;
	XPTR(void) base_address;
	unsigned long unused[2];
};

struct ui_widget_event_handler_reference
{
	long flags;
	short event_type;
	short function;
	struct tag_reference widget_tag;
	struct tag_reference sound_effect;
	char script[32];
};

struct ui_widget_child_reference
{
	struct tag_reference widget_tag;
	char name[32];
	long flags;
	short custom_controller_index;
	short vertical_offset;
	short horizontal_offset;
	byte unknown03A[0x50 - 0x3A];
};

struct ui_widget_definition
{
	short type;
	short controller_index;
	char name[32];
	rectangle2d bounds;
	long flags;
	long milliseconds_to_auto_close;
	long auto_close_fade_time;
	struct tag_reference background_bitmap;
	struct tag_block game_data_inputs;
	struct tag_block event_handlers;
	struct tag_block search_and_replace_functions;
	byte unknown06C[0xEC - 0x6C];
	struct tag_reference text_label_string_list;
	struct tag_reference text_font;
	real_argb_color text_color;
	short justification;
	word text_box_flags;
	byte unknown120[0x12E - 0x120];
	short string_list_index;
	short horizontal_offset;
	short vertical_offset;
	byte unknown134[0x150 - 0x134];
	long list_flags;
	struct tag_reference list_header_bitmap;
	struct tag_reference list_footer_bitmap;
	rectangle2d list_header_bounds;
	rectangle2d list_footer_bounds;
	byte unknown184[0x1A4 - 0x184];
	struct tag_reference extended_description_widget;
	byte unknown1B4[0x2D4 - 0x1B4];
	struct tag_block conditional_widgets;
	byte unknown2E0[0x3E0 - 0x2E0];
	struct tag_block child_widgets;
};

typedef char verify_menus_swap_instance_size[sizeof(struct cache_file_tag_instance) == 0x20 ? 1 : -1];
typedef char verify_menus_swap_handler_size[sizeof(struct ui_widget_event_handler_reference) == 0x48 ? 1 : -1];
typedef char verify_menus_swap_child_size[sizeof(struct ui_widget_child_reference) == 0x50 ? 1 : -1];
typedef char verify_menus_swap_definition_size[sizeof(struct ui_widget_definition) == 0x3EC ? 1 : -1];
typedef char verify_menus_swap_children_offset[
	offsetof(struct ui_widget_definition, child_widgets) == 0x3E0 ? 1 : -1];

/* ---------- the Xbox's MENUS: its tags */

/* the Xbox's tags made: their pictures and widgets */
enum
{
	ART_MENUS,
	ART_PC,
	ART_XBOX,
	ART_ARENA,
	ART_BACK,
	NUMBER_OF_ART
};

static char const *const art_names[NUMBER_OF_ART] =
{
	"menu_menus", "menu_style_pc", "menu_style_xbox_current", "menu_style_arena", "menu_back"
};

enum
{
	WIDGET_MENUS,
	WIDGET_SCREEN,
	WIDGET_LIST,
	WIDGET_PC,
	WIDGET_XBOX,
	WIDGET_ARENA,
	WIDGET_BACK,
	NUMBER_OF_WIDGETS
};

static char const *const widget_names[NUMBER_OF_WIDGETS] =
{
	"main_menu_item_menus", "menus_screen", "menus_select_list", "menus_item_pc", "menus_item_xbox",
	"menus_item_arena", "menus_item_back"
};

static struct
{
	/* the Xbox's are up (this module's tags in the table) */
	boolean built;
	/* what is let go */
	void **blocks;
	long block_count;
	struct bitmap_data *bitmaps[NUMBER_OF_ART * 2];
	long bitmap_count;
	struct cache_file_tag_instance *original_instances;
	long original_count;
	char *empty_name;
	/* the first of this module's tags, and how many */
	long first_index;
	long count;
	/* the main menu's list's children as the map had them */
	struct ui_widget_definition *list;
	struct tag_block original_children;
	boolean failed;
} swap_tags;

/* ---------- the switch */

enum
{
	SWITCH_NONE,
	SWITCH_FADING_OUT,
	SWITCH_FADING_IN,
};

static struct
{
	int state;
	/* the style picked, and the one shown before (put back if the picked one's menus cannot be built) */
	enum style style;
	enum style previous;
	unsigned long started;
	/* MENUS's instance, as the main menu opened after a switch made it
	("ae menus item"): focused there */
	boolean noting;
	struct widget_instance *menus_item;
} swap = { SWITCH_NONE, STYLE_PC, STYLE_PC, 0, FALSE, NULL };

/* ---------- private code */

/* (the game's allocator, inside the Xbox address space: cseries.h) */
static void *allocate(long size)
{
	void *block;
	void **blocks;

	if (swap_tags.failed)
		return NULL;
	block = malloc(size > 0 ? size : 1);
	blocks = block ? realloc(swap_tags.blocks, (swap_tags.block_count + 1) * sizeof(*swap_tags.blocks)) : NULL;
	if (!blocks)
	{
		if (block)
			free(block);
		swap_tags.failed = TRUE;
		return NULL;
	}
	memset(block, 0, size > 0 ? size : 1);
	swap_tags.blocks = blocks;
	swap_tags.blocks[swap_tags.block_count++] = block;
	return block;
}

static void reference_set(struct tag_reference *reference, long group_tag, long tag_index)
{
	reference->group_tag = group_tag;
	if (tag_index == NONE)
	{
		if (!swap_tags.empty_name)
			swap_tags.empty_name = allocate(1);
		reference->name = swap_tags.empty_name ? XBOX_ADDRESS(swap_tags.empty_name) : reference->name;
		reference->name_length = 0;
		reference->index = NONE;
		return;
	}
	reference->name = XBOX_ADDRESS(tag_get_name(tag_index));
	reference->name_length = (long)strlen(xbox_pointer(reference->name));
	reference->index = tag_index;
}

static boolean tag_name_ends(long tag_index, char const *end)
{
	char const *name = tag_index != NONE ? tag_get_name(tag_index) : NULL;
	size_t length = name ? strlen(name) : 0, end_length = strlen(end);

	return name && length >= end_length && !strcmp(name + length - end_length, end);
}

/* the port's function of that name, as a handler runs it (PC_MENU_FUNCTION_BASE on) */
static short port_function(char const *name)
{
	long index;
	char const *each;

	for (index = 0; (each = pc_menu_function_name(index)) != NULL; index++)
	{
		if (!strcmp(each, name))
			return (short)(PC_MENU_FUNCTION_BASE + index);
	}
	platform_log("menus: no function %s for MENUS", name);
	swap_tags.failed = TRUE;
	return 0;
}

static void instance_set(struct cache_file_tag_instance *instances, long tag_index, long group_tag, char const *name,
	void *definition)
{
	struct cache_file_tag_instance *instance = &instances[DATUM_INDEX_TO_ABSOLUTE_INDEX(tag_index)];
	char *copy = allocate((long)(strlen(TAG_PREFIX) + strlen(name) + 1));

	if (copy)
	{
		strcpy(copy, TAG_PREFIX);
		strcat(copy, name);
	}
	instance->group_tag = group_tag;
	instance->parent_group_tags[0] = NONE;
	instance->parent_group_tags[1] = NONE;
	instance->tag_index = tag_index;
	instance->name = XBOX_ADDRESS(copy ? copy : tag_empty_string());
	instance->base_address = XBOX_ADDRESS(definition);
}

/* a picture of two frames (unselected, selected), drawn from the PNGs */
static struct bitmap_group *picture_build(char const *name, long tag_index)
{
	struct bitmap_group *group = allocate(sizeof(struct bitmap_group));
	struct bitmap_group_sequence *sequence = allocate(sizeof(struct bitmap_group_sequence));
	struct bitmap_data *bitmaps = allocate(2 * sizeof(struct bitmap_data));
	long frame;

	if (!group || !sequence || !bitmaps)
		return NULL;
	sequence->first_bitmap_index = 0;
	sequence->bitmap_count = 2;
	group->sequences.count = 1;
	group->sequences.address = XBOX_ADDRESS(sequence);
	group->bitmaps.count = 2;
	group->bitmaps.address = XBOX_ADDRESS(bitmaps);
	for (frame = 0; frame < 2; frame++)
	{
		struct bitmap_data *bitmap = &bitmaps[frame];
		char png[128];

		snprintf(png, sizeof(png), "%s%s__%ld.png", XBOX_ART, name, frame);
		/* (a small D3D texture, which the PNG is drawn for: menu_tags.c's bitmap_build) */
		bitmap->signature = BITMAP_GROUP_TAG;
		bitmap->width = PLACEHOLDER_SIZE;
		bitmap->height = PLACEHOLDER_SIZE;
		bitmap->depth = 1;
		bitmap->type = _bitmap_type_2d;
		bitmap->format = _bitmap_format_a8r8g8b8;
		bitmap->flags = FLAG(_bitmap_has_power_of_two_dimensions_bit);
		bitmap->tag_index = tag_index;
		bitmap->cache_block_index = NONE;
		if (!rasterizer_bitmap_new(bitmap))
		{
			platform_log("menus: MENUS: could not make the texture of %s", png);
			swap_tags.failed = TRUE;
			return NULL;
		}
		swap_tags.bitmaps[swap_tags.bitmap_count++] = bitmap;
		bitmap->width = 256;
		bitmap->height = 64;
		/* (none without a renderer: debug.null_renderer) */
		if (bitmap->hardware_format)
			halo_menus_art_register(xbox_pointer(bitmap->hardware_format), png);
	}
	return group;
}

/* a copy of the model widget, with handlers for the events given (each running the function, and opening the
widget unless NONE; a go-back handler for function 0) */
static struct ui_widget_definition *widget_copy(struct ui_widget_definition const *model, short const *events,
	long event_count, short function, long open_tag)
{
	struct ui_widget_definition *copy = allocate(sizeof(*copy));
	struct ui_widget_event_handler_reference *handlers =
		allocate(event_count * (long)sizeof(struct ui_widget_event_handler_reference));
	long index;

	if (!copy || !handlers)
		return NULL;
	memcpy(copy, model, sizeof(*copy));
	for (index = 0; index < event_count; index++)
	{
		handlers[index].event_type = events[index];
		handlers[index].flags = function ? FLAG(_handler_run_function_bit) : FLAG(_handler_go_back_bit);
		handlers[index].function = function;
		reference_set(&handlers[index].widget_tag, UI_WIDGET_DEFINITION_TAG, open_tag);
		reference_set(&handlers[index].sound_effect, 'snd!', NONE);
		if (open_tag != NONE)
			handlers[index].flags |= FLAG(_handler_open_widget_bit);
	}
	copy->event_handlers.count = event_count;
	copy->event_handlers.address = XBOX_ADDRESS(handlers);
	return copy;
}

static void xbox_release(void)
{
	long index;

	if (swap_tags.list)
		swap_tags.list->child_widgets = swap_tags.original_children;
	for (index = 0; index < swap_tags.bitmap_count; index++)
	{
		if (swap_tags.bitmaps[index]->hardware_format)
			rasterizer_bitmap_delete(swap_tags.bitmaps[index]);
	}
	if (swap_tags.bitmap_count)
		halo_menus_art_forget();
	if (swap_tags.original_instances)
		cache_files_set_tag_instances(swap_tags.original_instances, swap_tags.original_count);
	for (index = 0; index < swap_tags.block_count; index++)
		free(swap_tags.blocks[index]);
	if (swap_tags.blocks)
		free(swap_tags.blocks);
	memset(&swap_tags, 0, sizeof(swap_tags));
}

/* the Xbox's main menu: MENUS in its list's fourth slot, and its styles' screen */
static void xbox_build(void)
{
	static short const press[] = { _event_a, _event_start };
	static short const back[] = { _event_b, _event_back };
	static short const created[] = { _event_created };
	long screen_tag = tag_loaded(UI_WIDGET_DEFINITION_TAG, XBOX_MAIN_MENU);
	long list_tag = tag_loaded(UI_WIDGET_DEFINITION_TAG, XBOX_MAIN_MENU_LIST);
	long model_tag = tag_loaded(UI_WIDGET_DEFINITION_TAG, XBOX_SETTINGS_ITEM);
	struct ui_widget_definition *screen, *list, *model, *made[NUMBER_OF_WIDGETS];
	struct ui_widget_child_reference *children, *grown, *screen_children, *rows;
	struct cache_file_tag_instance *instances, *grown_instances;
	long existing, index, salt = 0, slot, list_child = NONE, row_count, art[NUMBER_OF_ART], widgets[NUMBER_OF_WIDGETS];
	short open_function, pick_function, focus_function, item_function, back_function;

	/* (the menu files read, which the pictures are found among: the PC version's menus read them as they are built,
	the Xbox's not; nothing else is taken from them, so MENUS is there when they cannot be built) */
	halo_menus_load();
	if (screen_tag == NONE || list_tag == NONE || model_tag == NONE)
	{
		platform_log("menus: the Xbox's main menu is not this map's: no MENUS");
		return;
	}
	screen = tag_get(UI_WIDGET_DEFINITION_TAG, screen_tag);
	list = tag_get(UI_WIDGET_DEFINITION_TAG, list_tag);
	model = tag_get(UI_WIDGET_DEFINITION_TAG, model_tag);
	children = xbox_pointer(list->child_widgets.address);
	screen_children = xbox_pointer(screen->child_widgets.address);
	if (list->child_widgets.count < 3 || list->child_widgets.count > 4 || !children || !screen_children)
	{
		platform_log("menus: the Xbox's main menu list has %ld rows: no MENUS", (long)list->child_widgets.count);
		return;
	}
	for (index = 0; index < screen->child_widgets.count; index++)
	{
		if (screen_children[index].widget_tag.index == list_tag)
			list_child = index;
	}
	if (list_child == NONE)
		return;
	/* MENUS's place: GAME DEMOS's (hidden on the retail discs), else a fourth row under SETTINGS */
	slot = list->child_widgets.count == 4 && children[3].widget_tag.index != NONE &&
		tag_name_ends(children[3].widget_tag.index, "\\main_menu_item_game_demos") ? 3 : NONE;
	if (list->child_widgets.count == 4 && slot == NONE)
	{
		platform_log("menus: the Xbox's main menu's fourth row is not GAME DEMOS: no MENUS");
		return;
	}

	open_function = port_function("ae menus open");
	pick_function = port_function("ae menus pick");
	focus_function = port_function("ae menus focus");
	item_function = port_function("ae menus item");
	back_function = port_function("mouse emit back event");
	instances = cache_files_tag_instances(&existing);
	if (swap_tags.failed || !instances || existing < 0 || existing > UNSIGNED_SHORT_MAX - (NUMBER_OF_ART + NUMBER_OF_WIDGETS))
		goto failed;
	grown_instances = allocate((existing + NUMBER_OF_ART + NUMBER_OF_WIDGETS) * (long)sizeof(*grown_instances));
	if (!grown_instances)
		goto failed;
	memcpy(grown_instances, instances, existing * sizeof(*grown_instances));
	for (index = 0; index < existing; index++)
	{
		long instance_salt = (long)((unsigned long)instances[index].tag_index >> 16);

		if (instance_salt > salt)
			salt = instance_salt;
	}
	salt++;
	for (index = 0; index < NUMBER_OF_ART + NUMBER_OF_WIDGETS; index++)
	{
		long tag_index = ((salt + index) << 16) | (existing + index);

		if (index < NUMBER_OF_ART)
			art[index] = tag_index;
		else
			widgets[index - NUMBER_OF_ART] = tag_index;
	}
	/* (the names first: references read them) */
	for (index = 0; index < NUMBER_OF_ART; index++)
		instance_set(grown_instances, art[index], BITMAP_GROUP_TAG, art_names[index], NULL);
	for (index = 0; index < NUMBER_OF_WIDGETS; index++)
		instance_set(grown_instances, widgets[index], UI_WIDGET_DEFINITION_TAG, widget_names[index], NULL);
	if (swap_tags.failed)
		goto failed;
	swap_tags.original_instances = instances;
	swap_tags.original_count = existing;
	swap_tags.first_index = existing;
	swap_tags.count = NUMBER_OF_ART + NUMBER_OF_WIDGETS;
	cache_files_set_tag_instances(grown_instances, existing + swap_tags.count);
	for (index = 0; index < NUMBER_OF_ART; index++)
	{
		grown_instances[DATUM_INDEX_TO_ABSOLUTE_INDEX(art[index])].base_address =
			XBOX_ADDRESS(picture_build(art_names[index], art[index]));
	}
	if (swap_tags.failed)
		goto failed;

	/* MENUS: SETTINGS's look, its picture, A and Start asking for the styles ("ae menus open": its custom activation
	opens them, or it refuses quietly away from the plain main menu), its creation noted (focused after a switch) */
	made[WIDGET_MENUS] = widget_copy(model, press, NUMBEROF(press), open_function, NONE);
	/* the styles' rows (a style's A picks it; BACK's goes back), the list holding them focused on the style in use,
	and the screen: the main menu with that list for its own */
	made[WIDGET_PC] = widget_copy(model, press, NUMBEROF(press), pick_function, NONE);
	made[WIDGET_XBOX] = widget_copy(model, press, NUMBEROF(press), pick_function, NONE);
	made[WIDGET_ARENA] = widget_copy(model, press, NUMBEROF(press), pick_function, NONE);
	made[WIDGET_BACK] = widget_copy(model, press, NUMBEROF(press), back_function, NONE);
	made[WIDGET_LIST] = widget_copy(list, created, NUMBEROF(created), focus_function, NONE);
	made[WIDGET_SCREEN] = widget_copy(screen, back, NUMBEROF(back), 0, NONE);
	for (index = 0; index < NUMBER_OF_WIDGETS; index++)
	{
		if (!made[index])
			goto failed;
	}
	{
		/* (MENUS's creation, and its custom activation opening the styles: two more handlers) */
		struct ui_widget_event_handler_reference *handlers =
			allocate(4 * (long)sizeof(struct ui_widget_event_handler_reference));

		if (!handlers)
			goto failed;
		memcpy(handlers, xbox_pointer(made[WIDGET_MENUS]->event_handlers.address), 2 * sizeof(*handlers));
		handlers[2] = handlers[0];
		handlers[2].event_type = _event_created;
		handlers[2].function = item_function;
		handlers[2].flags = FLAG(_handler_run_function_bit);
		handlers[3] = handlers[0];
		handlers[3].event_type = _event_custom_activation;
		handlers[3].function = 0;
		handlers[3].flags = FLAG(_handler_open_widget_bit);
		reference_set(&handlers[3].widget_tag, UI_WIDGET_DEFINITION_TAG, widgets[WIDGET_SCREEN]);
		made[WIDGET_MENUS]->event_handlers.count = 4;
		made[WIDGET_MENUS]->event_handlers.address = XBOX_ADDRESS(handlers);
	}
	reference_set(&made[WIDGET_MENUS]->background_bitmap, BITMAP_GROUP_TAG, art[ART_MENUS]);
	reference_set(&made[WIDGET_PC]->background_bitmap, BITMAP_GROUP_TAG, art[ART_PC]);
	reference_set(&made[WIDGET_XBOX]->background_bitmap, BITMAP_GROUP_TAG, art[ART_XBOX]);
	reference_set(&made[WIDGET_ARENA]->background_bitmap, BITMAP_GROUP_TAG, art[ART_ARENA]);
	reference_set(&made[WIDGET_BACK]->background_bitmap, BITMAP_GROUP_TAG, art[ART_BACK]);
	for (index = 0; index < NUMBER_OF_WIDGETS; index++)
	{
		snprintf(made[index]->name, sizeof(made[index]->name), "%s", widget_names[index]);
		made[index]->conditional_widgets.count = 0;
		if (index != WIDGET_LIST)
			made[index]->game_data_inputs.count = 0;
	}
	/* the rows, where the main menu's own begin, at their spacing, BACK straight under the last */
	row_count = AE_MENUS_SWAP_ARENA ? 4 : 3;
	rows = allocate(row_count * (long)sizeof(*rows));
	if (!rows)
		goto failed;
	{
		long const row_widgets[] = { WIDGET_PC, WIDGET_XBOX, WIDGET_ARENA, WIDGET_BACK };
		long row;

		for (row = 0; row < row_count; row++)
		{
			long widget = row == row_count - 1 ? WIDGET_BACK : row_widgets[row];

			rows[row] = children[1];
			reference_set(&rows[row].widget_tag, UI_WIDGET_DEFINITION_TAG, widgets[widget]);
			snprintf(rows[row].name, sizeof(rows[row].name), "%s", widget_names[widget]);
			rows[row].vertical_offset = (short)(children[0].vertical_offset + row * ROW_SPACING);
		}
	}
	made[WIDGET_LIST]->child_widgets.count = row_count;
	made[WIDGET_LIST]->child_widgets.address = XBOX_ADDRESS(rows);
	{
		struct ui_widget_child_reference *copy = allocate(screen->child_widgets.count * (long)sizeof(*copy));

		if (!copy)
			goto failed;
		memcpy(copy, screen_children, screen->child_widgets.count * sizeof(*copy));
		reference_set(&copy[list_child].widget_tag, UI_WIDGET_DEFINITION_TAG, widgets[WIDGET_LIST]);
		made[WIDGET_SCREEN]->child_widgets.address = XBOX_ADDRESS(copy);
	}
	for (index = 0; index < NUMBER_OF_WIDGETS; index++)
		grown_instances[DATUM_INDEX_TO_ABSOLUTE_INDEX(widgets[index])].base_address = XBOX_ADDRESS(made[index]);

	/* MENUS in the main menu's list (the map's: put back as it was when these tags go) */
	grown = allocate(4 * (long)sizeof(*grown));
	if (!grown)
		goto failed;
	memcpy(grown, children, list->child_widgets.count * sizeof(*grown));
	if (slot == NONE)
	{
		slot = 3;
		grown[3] = children[2];
		grown[3].vertical_offset = (short)(children[2].vertical_offset + ROW_SPACING);
	}
	reference_set(&grown[slot].widget_tag, UI_WIDGET_DEFINITION_TAG, widgets[WIDGET_MENUS]);
	snprintf(grown[slot].name, sizeof(grown[slot].name), "%s", widget_names[WIDGET_MENUS]);
	swap_tags.list = list;
	swap_tags.original_children = list->child_widgets;
	list->child_widgets.address = XBOX_ADDRESS(grown);
	list->child_widgets.count = 4;
	swap_tags.built = TRUE;
	platform_log("menus: the Xbox's main menu has MENUS (%ld tags added to the map's %ld)", swap_tags.count, existing);
	return;

failed:
	platform_log("menus: MENUS could not be added to the Xbox's main menu");
	xbox_release();
}

/* the PC version's styles' list without its ARENA EVOLVED row (a build without AE menus): BACK moves up into
its place */
static void pc_without_arena(void)
{
	long list_tag = tag_loaded(UI_WIDGET_DEFINITION_TAG, PC_STYLES_LIST);
	struct ui_widget_definition *list;
	struct ui_widget_child_reference *children;
	long index, count;

	if (AE_MENUS_SWAP_ARENA || list_tag == NONE)
		return;
	list = tag_get(UI_WIDGET_DEFINITION_TAG, list_tag);
	children = xbox_pointer(list->child_widgets.address);
	count = list->child_widgets.count;
	for (index = 0; index < count; index++)
	{
		if (tag_name_ends(children[index].widget_tag.index, "\\menus_item_arena"))
		{
			long after;

			for (after = index + 1; after < count; after++)
				children[after].vertical_offset = (short)(children[after].vertical_offset - ROW_SPACING);
			memmove(&children[index], &children[index + 1], (count - index - 1) * sizeof(*children));
			list->child_widgets.count = count - 1;
			return;
		}
	}
}

/* the style the main menu shows now */
static enum style style_shown(void)
{
	return swap_tags.built ? STYLE_XBOX : STYLE_PC;
}

/* why MENUS cannot switch now (NULL: it can) */
static char const *refusal(void)
{
	if (swap.state != SWITCH_NONE)
		return "a switch is under way";
	if (!main_menu_is_active())
		return "not at the main menu";
	if (network_game_is_active() || global_network_game_client_get() || global_network_game_server_get())
		return "a network game, lobby, host or client is up";
	if (pc_menu_split_players())
		return "a second player is on this machine";
	return NULL;
}

/* MENUS's A: the styles opened (its custom activation's handler opens them, so the history goes back to MENUS),
or refused quietly (the failure sound and a line of the log; a handler that fails would log the engine's error too) */
static boolean menus_open(struct widget_instance *item, short controller, boolean *widget_deleted)
{
	char const *why = refusal();
	boolean deleted = FALSE;

	if (why)
	{
		platform_log("menus: MENUS refused: %s", why);
		ui_play_audio_feedback_sound(_feedback_failure);
		return TRUE;
	}
	platform_log("menus: MENUS opened");
	ui_widget_port_dispatch_event(item, _event_custom_activation, controller, &deleted);
	/* (the open plays the forward sound) */
	if (deleted)
		*widget_deleted = TRUE;
	return TRUE;
}

/* the row's style (its tag's name), NUMBER_OF_STYLES for none */
static enum style row_style(struct widget_instance const *row)
{
	int style;

	for (style = 0; style < NUMBER_OF_STYLES; style++)
	{
		if (tag_name_ends(row->definition_tag_index, style_rows[style]))
			return (enum style)style;
	}
	return NUMBER_OF_STYLES;
}

/* the style written to config.toml (memory first, then the file: a crash after it keeps the choice). PC and XBOX
also turn AE's menus off in a build that has them: only when they are on, and first, so that the file between the
two writes is a whole earlier choice (the old style without AE's menus), never the new style with AE's still on */
static void style_write(enum style style)
{
	int written;

	if (style == STYLE_ARENA)
	{
		written = config_write("display.arena_menus", "true");
	}
	else
	{
		written = 1;
		if (AE_MENUS_SWAP_ARENA && config_boolean("display.arena_menus"))
			written = config_write("display.arena_menus", "false");
		written = config_write("display.menus", style_names[style]) && written;
	}
	if (!written)
		platform_log("menus: the style could not be written to config.toml (it applies until the game closes)");
}

/* a style's row's A: the switch started (the style in use goes back, as B) */
static boolean menus_pick(struct widget_instance *row, short controller)
{
	enum style style = row_style(row);
	char const *why;

	if (style == NUMBER_OF_STYLES)
		return FALSE;
	if (style == style_shown())
	{
		event_manager_post_button(controller, 1);
		return TRUE;
	}
	/* (refused quietly, as MENUS) */
	why = refusal();
	if (why)
	{
		platform_log("menus: switch refused: %s", why);
		ui_play_audio_feedback_sound(_feedback_failure);
		return TRUE;
	}
	ui_play_audio_feedback_sound(_feedback_forward);
	style_write(style);
	/* (0.7.0: AE's menus are made at the start, as their own MENUS row says: the game starts again into them) */
	if (style == STYLE_ARENA)
	{
		platform_log("menus: switching to arena (display.arena_menus), starting again");
		if (!platform_restart())
			platform_log("menus: cannot start again here; AE's menus come at the next start");
		return TRUE;
	}
	platform_log("menus: switching to %s (%s), rebuilding", style_names[style],
		style == STYLE_ARENA ? "display.arena_menus" : "display.menus");
	swap.style = style;
	swap.previous = style_shown();
	swap.state = SWITCH_FADING_OUT;
	swap.started = system_milliseconds();
	/* (nothing more is taken until the main menu is back: a second press cannot switch twice) */
	ui_widgets_inhibit_processing(TRUE);
	return TRUE;
}

/* the list's row of the style in use focused (the list just made: its first row is focused) */
static boolean menus_focus(struct widget_instance *list)
{
	struct widget_instance *child;
	short index = 0;

	for (child = list->child; child; child = child->next, index++)
	{
		if (row_style(child) == style_shown())
		{
			list->focused_child = child;
			list->parameters.list.selected_index = index;
			break;
		}
	}
	platform_log("menus: the styles shown, %s focused", style_names[style_shown()]);
	return TRUE;
}

/* the menus' tags let go and made again for the style in config.toml */
static void tags_build(void)
{
	menu_tags_unloaded();
	menu_tags_loaded("ui");
	ae_menus_swap_tags_loaded("ui");
}

/* whether the style's menus are up (the PC version's main menu's tag made; the Xbox's MENUS made) */
static boolean style_built(enum style style)
{
	if (style == STYLE_XBOX)
		return swap_tags.built;
	if (style == STYLE_PC)
		return tag_loaded(UI_WIDGET_DEFINITION_TAG, "pc\\main_menu\\main_menu") != NONE;
	return TRUE;
}

/* the menus built again in the style picked (the screen black): every screen closed, the texture cache flushed
(its textures point at the menus' bitmaps, which go), the tags let go and made again, the main menu opened again
with MENUS focused. Menus that cannot be built put the style shown before back, once (written back too): the PC
version's falling back to the Xbox's as they always do, the Xbox's MENUS failing to the PC version's */
static void rebuild(void)
{
	ui_widgets_close_all();
	texture_cache_flush();
	tags_build();
	if (swap.style != STYLE_ARENA && !style_built(swap.style))
	{
		platform_log("menus: the %s menus could not be built: back to %s", style_names[swap.style],
			style_names[swap.previous]);
		style_write(swap.previous);
		if (!style_built(swap.previous))
			tags_build();
	}
	swap.noting = TRUE;
	swap.menus_item = NULL;
	main_screen_shell_load();
	swap.noting = FALSE;
	if (swap.menus_item && swap.menus_item->parent)
	{
		struct widget_instance *list = swap.menus_item->parent;
		struct widget_instance *child;
		short index = 0;

		for (child = list->child; child && child != swap.menus_item; child = child->next)
			index++;
		list->focused_child = swap.menus_item;
		list->parameters.list.selected_index = index;
		platform_log("menus: MENUS focused (row %d)", (int)index + 1);
	}
	else
	{
		platform_log("menus: the main menu has no MENUS to focus (a menus folder's own main menu?)");
	}
	swap.menus_item = NULL;
	platform_log("menus: %s menus are up", style_shown() == STYLE_XBOX ? "the Xbox's" : "the PC version's");
}

/* ---------- public code */

void ae_menus_swap_tags_loaded(
	char const *map_name)
{
	long pc_root = tag_loaded(UI_WIDGET_DEFINITION_TAG, "pc\\main_menu\\main_menu");

	if (strcmp(map_name, "ui") != 0)
		return;
	if (swap_tags.built)
		xbox_release();
	/* (the PC version's are up when their main menu's tag was made: menu_tags.c opens it; else the Xbox's,
	also when the menu files could not be built) */
	if (pc_root != NONE)
		pc_without_arena();
	else
		xbox_build();
}

void ae_menus_swap_tags_unloaded(
	void)
{
	if (swap_tags.built || swap_tags.block_count)
		xbox_release();
}

boolean ae_menus_swap_tag(
	long tag_index)
{
	long absolute = tag_index != NONE ? DATUM_INDEX_TO_ABSOLUTE_INDEX(tag_index) : NONE;

	return swap_tags.built && absolute >= swap_tags.first_index && absolute < swap_tags.first_index + swap_tags.count;
}

boolean ae_menus_swap_event(
	char const *name,
	struct widget_instance *widget,
	short controller,
	boolean *widget_deleted)
{
	if (!strcmp(name, "ae menus open"))
		return menus_open(widget, controller, widget_deleted);
	if (!strcmp(name, "ae menus pick"))
		return menus_pick(widget, controller);
	if (!strcmp(name, "ae menus focus"))
		return menus_focus(widget);
	if (!strcmp(name, "ae menus item"))
	{
		if (swap.noting)
			swap.menus_item = widget;
		return TRUE;
	}
	return FALSE;
}

void ae_menus_swap_update(
	void)
{
	unsigned long now, elapsed;
	boolean cut;

	if (swap.state == SWITCH_NONE)
		return;
	cut = config_boolean("display.arena_menus_reduce_motion") != 0;
	now = system_milliseconds();
	elapsed = now - swap.started;
	if (swap.state == SWITCH_FADING_OUT)
	{
		if (!cut && elapsed < FADE_TIME)
		{
			ui_widgets_set_fade_value((real)elapsed / FADE_TIME);
			return;
		}
		ui_widgets_set_fade_value(cut ? -1.0f : 1.0f);
		/* (a game may not be lost under it: only from the main menu) */
		if (main_menu_is_active())
			rebuild();
		else
			platform_log("menus: the switch's rebuild skipped: not at the main menu");
		swap.state = SWITCH_FADING_IN;
		swap.started = system_milliseconds();
		ui_widgets_inhibit_processing(TRUE);
		return;
	}
	if (!cut && elapsed < FADE_TIME)
	{
		ui_widgets_set_fade_value(1.0f - (real)elapsed / FADE_TIME);
		return;
	}
	ui_widgets_set_fade_value(-1.0f);
	ui_widgets_inhibit_processing(FALSE);
	/* (presses made during the switch are dropped) */
	event_manager_flush();
	swap.state = SWITCH_NONE;
}
