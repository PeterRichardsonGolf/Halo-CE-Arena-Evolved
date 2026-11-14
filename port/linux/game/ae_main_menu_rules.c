/*
AE_MAIN_MENU_RULES.C

See ae_main_menu_rules.h.
*/

#include <stddef.h>

#include "ae_main_menu_rules.h"

static const char *const main_labels[AE_MAIN_ROWS] = {
	"CAMPAIGN", "MULTIPLAYER", "HOST GAME", "SETTINGS", "MESSAGE OF THE DAY", "WHAT'S NEW",
	"FIREFIGHT", "FORGE", "THEATER", "FRIENDS"
};

static const char *const mp_labels[AE_MP_ROWS] = {
	"SERVER BROWSER", "MATCHMAKING", "JOIN BY ADDRESS", "JOIN LAN GAMES", "JOIN WITH INVITE", "HOST A SERVER"
};

int ae_main_row_coming_soon(int row)
{
	return row >= AE_MAIN_FIREFIGHT && row < AE_MAIN_ROWS;
}

int ae_main_first_focus(int last_used_row)
{
	return last_used_row >= 0 && last_used_row < AE_MAIN_ROWS ? last_used_row : AE_MAIN_MULTIPLAYER;
}

const char *ae_main_row_label(int row)
{
	return row >= 0 && row < AE_MAIN_ROWS ? main_labels[row] : NULL;
}

const char *ae_mp_row_label(int row)
{
	return row >= 0 && row < AE_MP_ROWS ? mp_labels[row] : NULL;
}
