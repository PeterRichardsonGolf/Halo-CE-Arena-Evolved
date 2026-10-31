/*
AE_SCREEN_JOIN_INVITE.H

AE's JOIN WITH INVITE (ae_screen_join_invite.c; the 0.3.7 menus): an internet game's invite link or code, pasted or
typed into a field, joined, then the joiner's LOBBY (ae_screen_create_game.h). Opened from AE's MULTIPLAYER, CREATE
GAME's JOIN WITH INVITE and SERVER BROWSER's Y.
*/

#ifndef __AE_SCREEN_JOIN_INVITE_H
#define __AE_SCREEN_JOIN_INVITE_H

/* opens it over the stack, its field holding the clipboard's invite when the clipboard has one (else empty); crumb:
where it was opened from ("MULTIPLAYER"). Leaving the joined game's lobby comes back here; 0 when the stack is full */
int ae_screen_join_invite_open(const char *crumb);

#endif
