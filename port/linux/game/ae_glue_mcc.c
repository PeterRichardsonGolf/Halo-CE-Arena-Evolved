/*
AE_GLUE_MCC.C

The PC menus' side of MCC's Custom Edition files (ae_glue_mcc.h): the
question when a Halo PC map is picked and MCC has the files the player's own
folders lack (main_menu/multiplayer_type_select/mp_map_select/mcc_found_modal,
tools/port_settings.py), its answers, and Settings > MAP FILES' BROWSE. The detection, the settings and the reads are
port/linux/src/ae_mcc_platform.c's.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "halo_map_families.h"
#include "ae_glue_mcc.h"

void platform_log(char const *format, ...);
/* (saved_game_files.c: a fixed 256-byte field) */
void saved_game_file_remember_last_used_multiplayer_map(char const *map_name);
/* (ui_widget_event_handler_functions.c) */
boolean ui_widget_port_multiplayer_map_choose(short level_index);
short ui_widget_port_multiplayer_maps(char const *const **names, short *last_used);
boolean ui_widget_port_open(struct widget_instance *widget, char const *name, boolean *widget_deleted);

#define MCC_QUESTION_NAME "pc\\main_menu\\multiplayer_type_select\\mp_map_select\\mcc_found_modal"

/* the map the question is about */
static char question_map[256];

int ae_mcc_own_files_present(void)
{
	int type, present = 1;

	/* (map_family_resource's own places only: its MCC place gives nothing, and logs nothing, meanwhile) */
	ae_mcc_probe_own_files(1);
	for (type = 0; present && ae_mcc_resource_name(type); type++)
	{
		char path[256];

		present = map_family_resource(ae_mcc_resource_name(type), path, sizeof(path)) != 0;
	}
	ae_mcc_probe_own_files(0);
	return present;
}

int ae_mcc_map_chosen(char const *map_name, short chosen, struct widget_instance *list, unsigned char *widget_deleted)
{
	boolean deleted = FALSE;

	if (!map_name || map_family_parse(map_name, NULL, 0) == _map_family_xbox || ae_mcc_use() != AE_MCC_USE_ASK ||
		!ae_mcc_root() || ae_mcc_own_files_present())
	{
		return 0;
	}
	platform_log("mcc: %s needs Custom Edition's resource maps; asking to read MCC's", map_name);
	/* (remembered as the map used last, so the list comes back on it and USE MCC FILES' A picks it; not set as the
	game's map, which would load it now, without its resource maps) */
	{
		char stored[256];

		memset(stored, 0, sizeof(stored));
		strncpy(stored, map_name, sizeof(stored) - 1);
		saved_game_file_remember_last_used_multiplayer_map(stored);
	}
	(void)chosen;
	snprintf(question_map, sizeof(question_map), "%s", map_name);
	if (!ui_widget_port_open(list, MCC_QUESTION_NAME, &deleted))
		return 0;
	*widget_deleted = deleted;
	return 1;
}

int ae_mcc_menu_event(char const *name, short controller)
{
	(void)controller;
	if (!strcmp(name, "ae mcc use"))
	{
		char const *const *names;
		short count, index;

		ae_mcc_answer_use();
		/* (the map picked, as the Map screen's OK picks it: then the gametypes open, the question's handler's) */
		count = ui_widget_port_multiplayer_maps(&names, NULL);
		for (index = 0; index < count && _stricmp(names[index], question_map); index++)
			;
		if (index >= count || !ui_widget_port_multiplayer_map_choose(index))
		{
			platform_log("mcc: USE MCC FILES: %s is no longer listed", question_map);
			return 0;
		}
	}
	else if (!strcmp(name, "ae mcc not now"))
	{
		ae_mcc_answer_not_now();
	}
	else if (!strcmp(name, "ae mcc never ask"))
	{
		ae_mcc_answer_never();
	}
	else if (!strcmp(name, "ae mcc browse"))
	{
		ae_mcc_browse();
	}
	return 1;
}
