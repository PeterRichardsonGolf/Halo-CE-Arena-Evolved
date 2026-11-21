/*
AE_SCREEN_RECORDS.H

(rc2) CAREER > Y: RECORDS, everyone this PC has a service record of, in two lists side by side and a card: PLAYERS ON THIS PC
(the profile's own record first with its YOU tag, then the others from the one seen most recently: the order LT / RT steps
through inside a record, ae_record_step_rules.h) | BOTS YOU HAVE MET (most games together first, Y the sort) | the focused
record's card in the detail window's column look (ae_record_columns_rules.h's ae_rc_card). Up / down move in a list, left /
right between the lists; A opens the focused record (a player's SERVICE RECORD, a bot's BOT RECORD with LT / RT through the
bots' list); LB / RB the next local profile; B back. Drawn in the record pages' 1920 x 1080 design units.

Log: "ae records: list (player <n>, <p> players, <b> bots)", "ae records: open <name>".
*/

#ifndef __AE_SCREEN_RECORDS_H
#define __AE_SCREEN_RECORDS_H

/* opens RECORDS for local player (0-3) over the stack. 0 when it is open already or the stack is full */
int ae_screen_records_open(short local_player);
/* (DEV MODE menu records) an open one leaves */
void ae_screen_records_close(void);

#endif
