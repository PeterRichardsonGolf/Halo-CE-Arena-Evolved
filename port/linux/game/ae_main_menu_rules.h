/*
AE_MAIN_MENU_RULES.H

Arena Evolved: the menus' phase 1 rows (owner 2026-11-10 decisions, layout A). The main menu's rows in order, with the
COMING SOON group last (FIREFIGHT, FORGE, THEATER, FRIENDS: focusable, they open nothing yet); MULTIPLAYER's rows in
order; and where the focus starts: MULTIPLAYER on the first launch, else the row the player last opened (config
menus.last_main_row). Pure (port/linux/tests/ae_main_menu_rules_test.c); the screen is ae_screen_main_menu.c.
*/

#ifndef __AE_MAIN_MENU_RULES_H
#define __AE_MAIN_MENU_RULES_H

enum ae_main_row
{
	AE_MAIN_CAMPAIGN,
	AE_MAIN_MULTIPLAYER,
	AE_MAIN_HOST_GAME,
	AE_MAIN_SETTINGS,
	AE_MAIN_MOTD,
	AE_MAIN_WHATS_NEW,
	AE_MAIN_FIREFIGHT,
	AE_MAIN_FORGE,
	AE_MAIN_THEATER,
	AE_MAIN_FRIENDS,
	AE_MAIN_ROWS
};

enum ae_mp_row
{
	AE_MP_BROWSER,
	AE_MP_MATCHMAKING,
	AE_MP_JOIN_ADDRESS,
	AE_MP_LAN,
	AE_MP_INVITE,
	AE_MP_HOST_SERVER,
	AE_MP_ROWS
};

/* 1 for the COMING SOON rows (FIREFIGHT..FRIENDS), 0 otherwise (out of range too) */
int ae_main_row_coming_soon(int row);

/* the row the main menu opens on: last_used_row when it is a main row (a COMING SOON row too), else MULTIPLAYER
(the first launch stores -1) */
int ae_main_first_focus(int last_used_row);

/* the rows' labels as drawn; NULL out of range */
const char *ae_main_row_label(int row);
const char *ae_mp_row_label(int row);

#endif
