/*
AE_SCREEN_RECORD_DETAILS.H

(rc2) A SERVICE RECORD box's DETAILS page (A on the focused box of a player's or a bot's record, ae_record_focus.h): the box's table
(ae_record_view.h's ae_rv_table) over the whole frame on the record pages' grid: the head ("SERVICE RECORD", a dot, the box's name; the
record's name; its sub line) and one pane of every row with its totals and per-game numbers, a muted "FROM n GAMES" after the
title when they cover fewer games than the record has. B goes back to the box.
*/

#ifndef __AE_SCREEN_RECORD_DETAILS_H
#define __AE_SCREEN_RECORD_DETAILS_H

#include "ae_record_view.h"

/* opens the page over the stack (the table is copied); bot: the BOT tag after the name. 0 when the stack is full or a page is open */
int ae_screen_record_details_open(short owner, struct ae_rv_table const *table, char const *name, int bot, char const *sub);
/* whether the page is open */
int ae_screen_record_details_is_open(void);
/* the page off the stack at once (DEV MODE) */
void ae_screen_record_details_remove(void);

#endif
