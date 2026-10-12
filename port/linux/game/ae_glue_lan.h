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

#endif
