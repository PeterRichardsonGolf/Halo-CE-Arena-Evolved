/*
AE_SCREEN_TAB_MENU.H

Arena Evolved's TAB / START menu (menus phase 1, owner Q3; spec §9's later sketch, its first rows): a popover over the
main menu, opened by START (player 1) or Tab. PLAYER (PROFILES), CONTROLLER (SETTINGS' controls for that player),
KEYBOARD (the keys), SETTINGS, then under a rule QUIT (the main menu's QUIT confirm). B / Esc / Tab / START close it.
Its log: "ae tab menu: open", "ae tab menu: <ROW>", "ae tab menu: closed".
*/

#ifndef __AE_SCREEN_TAB_MENU_H
#define __AE_SCREEN_TAB_MENU_H

struct ae_flow_screen;

/* over the main menu (under: its flow, for the density and frame), for that local player */
int ae_screen_tab_menu_open(struct ae_flow_screen const *under, short local_player);
void ae_screen_tab_menu_close(void);
/* whether a screen's data is the TAB menu's */
int ae_screen_tab_menu_is(void const *data);

#endif
