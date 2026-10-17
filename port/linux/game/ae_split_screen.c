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

void ae_split_screen_lobby_update(struct widget_instance *list, boolean teams, boolean slayer)
{
	boolean local = network_game_is_splitscreen_local();
	short row;

	for (row = 0; row < NUMBER_OF_BOT_ROWS; row++)
	{
		struct widget_instance *widget = child_named(list, bot_rows[row]);
		char spinner_name[48];

		if (!widget)
			continue;
		widget->visible = local && slayer && (row != BOT_ROW_TEAMS || teams);
		if (!widget->visible)
			continue;
		strcpy(spinner_name, bot_rows[row]);
		strcat(spinner_name, "_spinner");
		spinner_save(child_named(widget, spinner_name));
	}
}

/* Server Setup's rows a split screen game has no use for (a network game's), and the gametype's, in their order
(tools/ce_menus.py and port_settings.py place them 22 apart from y 73) */
static char const *const network_rows[] = { "op_server_name", "op_max_players", "op_invite", "op_listing", "op_password",
	"op_map_share_options" };
static char const *const gametype_rows[] = { "op_game_type", "op_player_options", "op_item_options",
	"op_vehicle_options", "op_indicator_options", "op_team_options", "op_arena_options" };
#define SERVER_SETTINGS_FIRST_ROW 73
#define SERVER_SETTINGS_ROW_SPACING 22

void ae_split_screen_server_settings(struct widget_instance *list)
{
	struct widget_instance *child;
	short row, index;

	if (!list || !network_game_is_splitscreen_local())
		return;
	for (row = 0; row < (short)NUMBEROF(network_rows); row++)
	{
		struct widget_instance *widget = child_named(list, network_rows[row]);

		if (widget)
			widget->visible = FALSE;
	}
	for (row = 0; row < (short)NUMBEROF(gametype_rows); row++)
	{
		struct widget_instance *widget = child_named(list, gametype_rows[row]);

		if (widget)
			widget->vertical_offset = (short)(SERVER_SETTINGS_FIRST_ROW + SERVER_SETTINGS_ROW_SPACING * row);
	}
	/* (the focus off a hidden row, onto the first that shows: menu_functions.c's focus_off_hidden) */
	if (list->focused_child && !list->focused_child->visible)
	{
		for (child = list->child, index = 0; child; child = child->next, index++)
		{
			if (child->visible)
			{
				list->focused_child = child;
				list->parameters.list.selected_index = index;
				break;
			}
		}
	}
}

boolean ae_split_screen_server_start(void)
{
	if (network_game_is_splitscreen_local())
	{
		network_game_server_port_set_settings(NULL, MAXIMUM_PLAYERS);
		platform_log("split screen: %d players at most", MAXIMUM_PLAYERS);
	}
	return TRUE;
}
