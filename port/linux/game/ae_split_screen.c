/*
AE_SPLIT_SCREEN.C

Multiplayer > SPLIT SCREEN in the PC menus (ae_split_screen.h).
*/

#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "game/players.h"
#include "interface/ui_widget.h"
#include "interface/ui_widget_instance.h"
#include "networking/network_game_globals.h"
#include "networking/network_server_manager.h"

#include "ae_split_screen.h"
#include "bots.h"
#include "ae_bots_objective.h"

int config_write(const char *name, const char *value);
void platform_log(char const *format, ...);

/* menu_functions.c's (and menu_tags.c's) */
#define MAXIMUM_STRINGS 128
struct pc_menu_setting
{
	long definition_index;
	char const *setting;
	long value_count;
	char const *values[MAXIMUM_STRINGS];
	short loaded_index;
};

struct pc_menu_setting *pc_menu_setting_get(long definition_index);

/* the most players of a split screen game: the four people and the most bots */
#define MAXIMUM_PLAYERS (MAXIMUM_LOCAL_PLAYERS + BOTS_MAXIMUM)

/* the bot rows (tools/port_settings.py's _lobby), each with its spinner (its name + "_spinner") */
static char const *const bot_rows[] = { "ae_bots_count_row", "ae_bots_skill_row", "ae_bots_playfight_row",
	"ae_bots_teams_row" };
enum { BOT_ROW_COUNT, BOT_ROW_SKILL, BOT_ROW_PLAYFIGHT, BOT_ROW_TEAMS, NUMBER_OF_BOT_ROWS };

static boolean split_screen_chosen;

void ae_split_screen_chosen(char const *item_name)
{
	split_screen_chosen = item_name && strstr(item_name, "split_scr") != NULL;
}

boolean ae_split_screen_hosting(void)
{
	return split_screen_chosen;
}

short ae_split_screen_lobby_rows(short rows)
{
	return network_game_is_splitscreen_local() ? AE_SPLIT_SCREEN_LOBBY_ROWS : rows;
}

boolean ae_split_screen_lobby_open(struct widget_instance *widget, boolean chosen, boolean *widget_deleted)
{
	struct widget_instance *topmost;

	if (!chosen)
		return FALSE;
	network_game_server_port_set_settings(NULL, MAXIMUM_PLAYERS);
	topmost = widget_instance_get_topmost_parent(widget);
	if (!ui_widget_load_by_name_or_tag(AE_SPLIT_SCREEN_LOBBY, NONE, NULL, NONE, topmost->definition_tag_index,
		widget->parent ? widget->parent->definition_tag_index : NONE, (short)widget_instance_get_child_index_from_parent(widget)))
	{
		platform_log("split screen: the lobby could not be opened");
		return FALSE;
	}
	platform_log("split screen: the lobby, %d players at most", MAXIMUM_PLAYERS);
	*widget_deleted = TRUE;
	return TRUE;
}

static struct widget_instance *child_named(struct widget_instance *widget, char const *name)
{
	struct widget_instance *child;

	for (child = widget ? widget->child : NULL; child; child = child->next)
	{
		struct widget_instance *found;

		if (child->name && !strcmp(child->name, name))
			return child;
		if ((found = child_named(child, name)) != NULL)
			return found;
	}
	return NULL;
}

/* a setting spinner's choice written to config.toml when it changed (as menu_functions.c's setting_save, but at
once: the lobby has no OK) */
static void spinner_save(struct widget_instance *spinner)
{
	struct pc_menu_setting *setting = spinner ? pc_menu_setting_get(spinner->definition_tag_index) : NULL;
	short index;

	if (!setting || setting->loaded_index == NONE)
		return;
	index = spinner->parameters.list.selected_index;
	if (index < 0 || index >= setting->value_count || index == setting->loaded_index)
		return;
	if (!config_write(setting->setting, setting->values[index]))
	{
		platform_log("split screen: could not write %s to config.toml", setting->setting);
		return;
	}
	platform_log("split screen: %s = %s", setting->setting, setting->values[index]);
	setting->loaded_index = index;
}

void ae_split_screen_lobby_update(struct widget_instance *list, boolean teams, long game_engine_index)
{
	boolean local = network_game_is_splitscreen_local();
	short row;

	for (row = 0; row < NUMBER_OF_BOT_ROWS; row++)
	{
		struct widget_instance *widget = child_named(list, bot_rows[row]);
		char spinner_name[48];

		if (!widget)
			continue;
		widget->visible = local && ae_bots_engine_played(game_engine_index) && (row != BOT_ROW_TEAMS || teams);
		if (!widget->visible)
			continue;
		strcpy(spinner_name, bot_rows[row]);
		strcat(spinner_name, "_spinner");
		spinner_save(child_named(widget, spinner_name));
	}
}
