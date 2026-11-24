/*
AE_CLASSIC_SCREEN.C

A screen of the PC menus opened by name (ae_classic_screen.h): the menus' root is set to it for one load of the main
screen shell (menu_tags.c's pc_menus_root_name, which also begins the profile's edit for a settings screen), then
the usual root again.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "interface/ui_widget.h"
#include "interface/ui_widget_definitions.h"
#include "tag_files/tag_groups.h"

#include "ae_classic_screen.h"
#include "ae_hooks.h"

void platform_log(char const *format, ...);

#define PROFILE_EDIT "main_menu/settings_select/player_setup/player_profile_edit"

static char asked[300];

char const *ae_classic_screen_root(
	char const *configured)
{
	return asked[0] ? asked : configured;
}

/* whether the PC menus have a widget with the path (as port/assets/menus names it) */
static boolean widget_exists(char const *path)
{
	char name[320];
	long index;

	snprintf(name, sizeof(name), "pc\\%s", path);
	for (index = 0; name[index]; index++)
	{
		if (name[index] == '/')
			name[index] = '\\';
	}
	return tag_loaded(UI_WIDGET_DEFINITION_TAG, name) != NONE;
}

boolean ae_classic_screen_open(
	char const *page)
{
	static char const *const screens[] = { "ae_settings_page_screen", "ae_settings_group_screen", "ae_settings_screen" };
	char path[300];
	short index;
	boolean found = FALSE;

	if (!page || !page[0])
		return FALSE;
	if (!strcmp(page, "main_menu"))
	{
		path[0] = 0;
		found = TRUE;
	}
	else if (!strcmp(page, "profile"))
	{
		snprintf(path, sizeof(path), PROFILE_EDIT "/player_profile_edit_screen");
		found = widget_exists(path);
	}
	else if (!strncmp(page, "main_menu/", 10))
	{
		snprintf(path, sizeof(path), "%s", page);
		found = widget_exists(path);
	}
	else
	{
		for (index = 0; !found && index < (short)(sizeof(screens) / sizeof(screens[0])); index++)
		{
			snprintf(path, sizeof(path), PROFILE_EDIT "/%s/%s", page, screens[index]);
			found = widget_exists(path);
		}
	}
	if (!found)
	{
		platform_log("classic screen: no screen \"%s\"", page);
		return FALSE;
	}
	/* (AE's screens closed and its main menu kept away, as for a test screen) */
	ae_ui_classic_prepare();
	snprintf(asked, sizeof(asked), "%s", path);
	/* (pc_menus_root_name reads it as it does debug.menu_open) */
	main_screen_shell_load();
	asked[0] = 0;
	platform_log("classic screen: %s open", page);
	return TRUE;
}
