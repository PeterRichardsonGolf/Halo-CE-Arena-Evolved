/*
AE_SCREEN_BOT_RECORDS.H

(0.9.4) CAREER > Y: BOTS, the bots a profile has met in AE TOURNAMENT games on this PC (notes/specs/
2026-11-15-bot-service-records.md section 5, mockup C-2), CAREER's look: the rows left (the bot's name, its play style's
outlined badge, the RIVAL tag; the games together with a chevron), 10 shown and the rest under "N more", the list
scrolling with the focus; the focused bot's card right (the rival line, VS YOU's numbers, its career totals in short, its
skill and favourite weapon). A opens its BOT RECORD page (ae_screen_bot_record.h) with LB / RB through the list's order;
X sorts by GAMES (together), LAST MET or RECORD (the bot's lead over you); LB / RB the next local player.

Log: "ae botrecord: list (player <n>, <count> bots, <sort>)" and "ae botrecord: list closed"; DEV MODE event
bot_records_list.
*/

#ifndef __AE_SCREEN_BOT_RECORDS_H
#define __AE_SCREEN_BOT_RECORDS_H

/* opens BOTS for local player (0-3) over the stack, the focus on the bot called focus (NULL: the first), sorted by sort
   (AE_BOT_RECORD_SORT_*; -1: GAMES). 0 when it is open already or the stack is full */
int ae_screen_bot_records_open(short local_player, char const *focus, int sort);
/* (DEV MODE menu bots) an open list leaves */
void ae_screen_bot_records_close(void);

#endif
