/*
AE_SCREEN_STEAM_FIRST.H

(1.0) The first start's MCC FROM STEAM page (ae_screen_steam_first.c): after COMMUNITY SERVICES, in the main menu's place, for a player with no MCC install who is not signed in
to Steam: sign in (STEAM ACCOUNT opens), say MCC is installed (the system's folder dialog), or NOT NOW. Any answer is
kept at once (game.steam_first_start_asked) and the page is never shown again.
*/

#ifndef __AE_SCREEN_STEAM_FIRST_H
#define __AE_SCREEN_STEAM_FIRST_H

/* whether the page is to be shown now (the build has the Steam client, not answered yet, no MCC found, not signed in) */
int ae_steam_first_start_asking(void);

/* opens it over an empty stack; 0 when a screen is up or the stack is full */
int ae_screen_steam_first_open(void);

/* DEV MODE's `menu steam first-start`: opened whatever the key says, replacing a screen of the menus' */
int ae_screen_steam_first_open_staged(void);

#endif
