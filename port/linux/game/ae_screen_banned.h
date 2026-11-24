/*
AE_SCREEN_BANNED.H

(0.7.6 HOST MODERATION) AE's BANNED PLAYERS (ae_screen_banned.c): every ban of bans.txt that still holds, with the name
it was made under and when. A on one asks, CANCEL first, to UNBAN it (its line taken out of bans.txt). Opened from
SETTINGS > ONLINE > HOSTING > BANNED PLAYERS.
*/

#ifndef __AE_SCREEN_BANNED_H
#define __AE_SCREEN_BANNED_H

/* opens it over the stack; 0 when the stack is full */
int ae_screen_banned_open(void);

/* the row's summary in SETTINGS: how many players are banned ("NONE", "2 PLAYERS") */
void ae_screen_banned_summary(char *text, int size);

#endif
