/*
AE_COMMUNITY_SCREEN.H

Arena Evolved: the COMMUNITY SERVICES screen (0.3.5; the opt-in mockups' look A3, the owner's pick of 2026-10-20), the
PC menus' first-run opt-in for the services the ChupathingyCE team runs at halo.milenko.org (the game list and match
stats; rules in ../src/ae_community_rules.h). Once, while network.community_services is "ask" (a new install, and an
upgrade from 0.3.4), it opens in front of the PC menus' main menu, which BACK returns to.

The screen is tools/port_settings.py's (main_menu/community_services/community_services_screen): the title picture, the
LIST HOSTED GAMES row (its own opt-in, OFF until the player turns it on; it only answers with USE), and the stock
buttons USE COMMUNITY SERVICES and NOT NOW, NOT NOW focused first. The text beside them (what each service sends) and
the card (who runs them, and the thanks) are drawn with AE's renderer (ae_draw.h) by the screen's game data input.
B, BACK and Esc (START) are NOT NOW. A press in the screen's first half second is not taken.
*/

#ifndef AE_COMMUNITY_SCREEN_H
#define AE_COMMUNITY_SCREEN_H

struct widget_instance;

/* (ui_widget.c's main_screen_shell_load, after the main menu is loaded) the screen, in front of the PC menus' main
menu, when it is to ask (ae_community_should_ask) */
void ae_community_main_menu_loaded(void);
/* (0.7.0, ae_hooks.c) the question is up and not answered: AE's main menu does not take the main menu's place yet */
unsigned char ae_community_screen_asking(void);

/* (menu_functions.c) the screen's functions, "ae community ...": "focus" (its list's created: NOT NOW focused),
"row" (the row's A: OFF / ON), "use" and "not now" (the answers, written at once; then back to the main menu). TRUE
when handled */
unsigned char ae_community_menu_event(char const *name, struct widget_instance *widget, unsigned char *widget_deleted);

/* (menu_functions.c) the screen's game data input, "ae community update": its text and card, once a frame */
void ae_community_screen_update(struct widget_instance *screen);

#endif
