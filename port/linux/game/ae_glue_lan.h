/*
AE_GLUE_LAN.H

What the AE hook lines of Join Game > LAN's list call in menu_functions.c (ae_hooks.txt lists those lines): the
firewall hint of item 73 (ae_lan_hint.h). Plain types, but the game's wide text.
*/

#ifndef __AE_GLUE_LAN_H
#define __AE_GLUE_LAN_H

#include <stddef.h>

struct widget_instance;

/* (browser_initialize, the list opened or come back to) the search's start: the hint waits AE_LAN_HINT_DELAY_MS from
here, and a firewall is looked for again */
void ae_lan_hint_begin(void);
/* (browser_update, LAN only, each frame) the list's line (0: the first, under PLAYERS; 1: the second) with nothing
found yet (empty): when the hint is due, its line in text (size: the line's room), TRUE; else text as it was, FALSE.
Line 0 decides for the frame (and logs the hint once a run, to debug.txt) */
int ae_lan_hint_line(int line, int empty, wchar_t *text, short size);
/* (browser_update, LAN only, after the second line) the second line's label: MESSAGE under the hint, else RULES as
the list has it */
void ae_lan_hint_label(struct widget_instance *label);
/* (the PC lobby's "port lobby leave", item 108) B from a pad with no player and no ADD PLAYER under way: every
player of this machine leaves and the machine leaves the lobby, as its last player's B does; TRUE: back from the lobby,
FALSE: not in a lobby's pregame (nothing done) */
int ae_lobby_leave_all(short controller);
/* Server Setup's START GAME with no game hosted any more (it closed after the lobby was left, LAN party 2026-10-17:
START GAME failed without a word): a line saying so; FALSE */
int ae_lobby_server_gone(void);
/* Server Setup's START GAME after the lobby was left (its game closed): the game hosted again, player 1 back in on
their active profile, as Create Game does; FALSE (and the message) if it cannot be */
struct widget_instance;
struct event_record;
unsigned char ae_lobby_server_rehost(struct widget_instance *widget, struct event_record *event,
	unsigned char *widget_deleted);
/* (menu_functions.c's lobby B, an AE hook line) whether the controller's player is the last person of this machine,
the others of it LAN bots (ae_lan_bots.c: the host's spare controllers): their B then leaves as the last player's does
(ae_lobby_leave_all, the bots too). The game counted the bots as this machine's players, so the person left alone,
the bots left after them, and the host stayed in an empty lobby it could not back out of */
int ae_lobby_only_bots_stay(short controller);

#endif
