/*
AE_SCREEN_NETWORK_STATUS.H

Arena Evolved's NETWORK STATUS page (0.9.4, the TAB menu's): one read-only list of what the game already knows of its
network: the online name, whether internet play is on, whether a master server answered (and how long ago), how many
servers the last answer listed, the internet play port and what the game knows of it, the network version and the build.
It asks the network nothing itself: what the game does not measure reads "unknown". B / Esc / right click go back to the
TAB menu. Its log: "ae network status: open (<state>)", "ae network status: closed".
*/

#ifndef __AE_SCREEN_NETWORK_STATUS_H
#define __AE_SCREEN_NETWORK_STATUS_H

/* over the main menu, for that local player (the TAB menu opens again on B); 0 when the stack is full */
int ae_screen_network_status_open(short local_player);
/* DEV MODE's `menu network reached|unreached|unknown`: the page with made-up facts, to stage the cases (the real ones
need a master to answer or not); 0 for another word or a full stack */
int ae_screen_network_status_open_staged(short local_player, char const *state);

#endif
