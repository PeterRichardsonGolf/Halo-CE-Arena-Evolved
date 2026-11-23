/*
AE_TAB_MENU_RULES.C

See ae_tab_menu_rules.h.
*/

#include <stdio.h>

#include "ae_tab_menu_rules.h"

static struct ae_tab_row const rows[AE_TAB_ROWS] =
{
	{ AE_TAB_ROW_PLAYER, "PLAYER", "Your profile: name, colours and controls.", 0 },
	{ AE_TAB_ROW_CONTROLLER, "CONTROLLER", "Your controller's buttons, sticks and feel.", 0 },
	{ AE_TAB_ROW_KEYBOARD, "KEYBOARD", "The keys and the mouse.", 0 },
	{ AE_TAB_ROW_SETTINGS, "SETTINGS", "Video, audio, controls, network and menu settings.", 0 },
	{ AE_TAB_ROW_CREDITS, "CREDITS", "Who made what, and the licences.", 0 },
	{ AE_TAB_ROW_NETWORK_STATUS, "NETWORK STATUS", "Whether the master server answers, your port and the versions.", 0 },
	{ AE_TAB_ROW_QUIT, "QUIT", "Asks first, then the game closes.", 1 },
};

struct ae_tab_row const *ae_tab_menu_rows(int *count)
{
	*count = AE_TAB_ROWS;
	return rows;
}

void ae_tab_menu_summary(char *text, int size)
{
	int index, used = 0;

	text[0] = 0;
	for (index = 0; index < AE_TAB_ROWS && used < size; index++)
		used += snprintf(text + used, (size_t)(size - used), "%s%s", index ? " | " : "", rows[index].label);
}
