/*
AE_CLASSIC_SCREEN.H

A screen of the PC menus (the classic widget menus) opened by name while AE's menus are the menus: the dev command
"menu classic <page>" (TEST RUN stages the classic pages with it). Include after cseries.h.
*/

#ifndef __AE_CLASSIC_SCREEN_H
#define __AE_CLASSIC_SCREEN_H

/* the screen opened as the menus' root: "profile" (the profile menu), "main_menu", a path under the profile menu's folder
("ae_settings/hud/hud": its page screen is found by name), or a whole "main_menu/..." widget path. AE's screens close,
the main menu stays out until the next TEST RUN setup or a restart. FALSE when there is no such screen */
boolean ae_classic_screen_open(char const *page);
/* (menu_tags.c, pc_menus_root_name) the screen asked for, else debug.menu_open's */
char const *ae_classic_screen_root(char const *configured);

#endif
