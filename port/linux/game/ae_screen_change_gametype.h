/*
AE_SCREEN_CHANGE_GAMETYPE.H

AE's CHANGE GAMETYPE screen (ae_screen_change_gametype.c): the saved gametypes in folders that open in place, by mode
(LB / RB) and by name (Y), a battle rifle pair on one row. CREATE GAME's and the LOBBY's GAMETYPE row open it; alone,
it is debug.ae_test_screen 33.
*/

#ifndef __AE_SCREEN_CHANGE_GAMETYPE_H
#define __AE_SCREEN_CHANGE_GAMETYPE_H

/* opens the screen over the stack on the filter given (ae_gametype_picker.h's AE_GTF_*; out of range: ALL) and a query
(NULL: none); closes upstream's menus. A pick starts the GAME OPTIONS edit on it. 0 when the stack is full */
int ae_screen_change_gametype_open(int filter, const char *query);
/* opens it over the stack for a caller (CREATE GAME, the LOBBY): the folder of the edit's gametype open, it focused; A
on a gametype tells picked (its saved file) and closes, choosing nothing itself (a RECENT row too: RECENT is the
picker's own, the gametypes played; the map stays) */
int ae_screen_change_gametype_pick(void (*picked)(long profile));

#endif
