/*
AE_HOST_VOTE.H

(rc2) The map and gametype vote of a game a player hosts (CREATE GAME's playlist with MAP VOTE on). Once a game is over and
the lobby is back, ae_screen_create_game.c opens it with ae_host_vote_begin: the playlist's choices
(ae_host_vote_rules.h) go to the netcode's vote (network_map_vote.c), which tells every client as it tells them on a
dedicated server, and counts this machine's own vote from the vote popover (ae_screen_vote.c) on the spot. Each frame
ae_host_vote_tick runs the shared loop (ae_vote_loop.h) and gives the playlist game that won.
*/

#ifndef AE_HOST_VOTE_H
#define AE_HOST_VOTE_H

#include "ae_playlist.h"

enum { AE_HOST_VOTE_IDLE, AE_HOST_VOTE_RUNNING, AE_HOST_VOTE_DECIDED };

/* a vote among the playlist's games after the game current (-1: none); playable[i] 0 leaves a game out (NULL: all). 1
when a vote opened, 0 when there is none to hold (MAP VOTE off, too few games on other maps, or a vote already runs) */
int ae_host_vote_begin(struct ae_playlist const *list, int current, unsigned char const *playable);
/* one frame; DECIDED once, with the playlist game that plays next in *entry */
int ae_host_vote_tick(int *entry);
int ae_host_vote_running(void);
/* the vote dropped (the lobby left, or the host started the game) */
void ae_host_vote_cancel(void);

#endif
