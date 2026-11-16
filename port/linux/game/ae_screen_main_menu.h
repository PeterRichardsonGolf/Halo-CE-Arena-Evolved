/*
AE_SCREEN_MAIN_MENU.H

AE's MAIN MENU and its MULTIPLAYER (ae_screen_main_menu.c; notes/plans/2026-10-21-game-setup-spike.md, the 0.3.7
menus): the order MULTIPLAYER (CREATE GAME, JOIN WITH INVITE, SERVER BROWSER, LAN GAMES),
CAMPAIGN, SETTINGS, PROFILES, QUIT, in AE's look. While display.arena_menus is on it takes the game's main menu's place
(ae_hooks.c); an item whose AE screen is not built yet is shown disabled with why, never the game's old screen.
*/

#ifndef __AE_SCREEN_MAIN_MENU_H
#define __AE_SCREEN_MAIN_MENU_H

/* opens AE's main menu over an empty stack, the game's menus closed; 0 when a screen is up or the stack is full */
int ae_screen_main_menu_open(void);
/* closes it (and its MULTIPLAYER) wherever it is on the stack: the game has left its main menu */
void ae_screen_main_menu_close(void);
/* (menus phase 1) MY PARTY drawn on another screen's right column (MATCHMAKING's lobby), place its head line ("In
Matchmaking"); and its pads' presses there (START refused outside MY PARTY, a signed-in pad's B signs it out): 1 when
taken. Include after ae_setup_flow.h */
struct ae_flow_screen;
struct ae_event;
struct ae_pointer;
void ae_screen_main_menu_party_draw(struct ae_flow_screen *flow, const char *place);
int ae_screen_main_menu_party_press(struct ae_flow_screen const *flow, struct ae_event const *event);
/* (menus phase 1) the PARTY manager's party (ae_screen_party.c): its size (1-4); a seat's name, sub-line and colour (0:
the controller's seat is free); a guest taken off (logs "ae party: NAME left (controller N)"); a seat's profile changed
(player 1: PROFILES; a guest: its card's pick again); its presses where a pad's START joins; lobby_fill's party */
int ae_screen_main_menu_party_count(void);
int ae_screen_main_menu_party_seat(short controller, char *name, int name_size, char *sub, int sub_size,
	unsigned int *color);
void ae_screen_main_menu_party_remove(short controller);
int ae_screen_main_menu_party_change(struct ae_flow_screen const *flow, short controller);
int ae_screen_main_menu_party_press_here(struct ae_flow_screen const *flow, struct ae_event const *event);
int ae_screen_main_menu_party_fill(int count);
/* once a frame where MY PARTY shows (the main menu, MATCHMAKING, the PARTY manager): a guest's held B, which removes
them at 0.8 s */
void ae_screen_main_menu_party_update(void);
/* (owner 11-11) asks before a guest leaves (own: its own B, STAY / LEAVE) or is removed (CANCEL / REMOVE); 0: not asked */
int ae_screen_main_menu_party_remove_ask(struct ae_flow_screen const *flow, short controller, int own);
/* (MATCHMAKING) the pointer over MY PARTY drawn by ae_screen_main_menu_party_draw: a pad's profile card's rows, a click on
MY PARTY opens the PARTY manager; 1 when taken */
int ae_screen_main_menu_party_pointer(struct ae_flow_screen const *flow, struct ae_pointer const *pointer);
/* (menus phase 1) the QUIT confirm over the main menu (the TAB menu's QUIT); 0 when the main menu is not up */
int ae_screen_main_menu_quit_ask(void);
/* (menus phase 1) whether the keyboard's Tab is the TAB menu's key now: the main menu on top, or the TAB menu over it */
int ae_screen_main_menu_tab_opens(void);
/* a split-screen player signed in here but not yet in the lobby; cancelling its profile pick signs it out */
short ae_screen_main_menu_signed_in_waiting(void);
/* (split screen) a player 2-4 signed in here with the profile they picked */
int ae_screen_main_menu_signed_in(short local_player);
void ae_screen_main_menu_signed_in_cancel(short local_player);
void ae_screen_main_menu_signed_in_joined(short local_player);

#endif
