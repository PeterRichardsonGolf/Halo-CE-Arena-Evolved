/*
AE_HOST_VOTE_RULES.H

The map and gametype vote of a game a player hosts (rc2; ae_host_vote.c): which playlist games the vote offers. Pure C
(port/linux/tests/ae_host_vote_rules_test.c): up to AE_PLAYLIST_VOTE_GAMES games from the playlist, each on a map of
its own and none on the map just played (a dedicated server's vote and AE TOURNAMENT's keep the last map out the same
way), drawn by weight, then SAME AGAIN (the game just played) as the last choice.
*/

#ifndef AE_HOST_VOTE_RULES_H
#define AE_HOST_VOTE_RULES_H

#include "ae_playlist.h"

enum { AE_HOST_VOTE_MAX = AE_PLAYLIST_VOTE_CHOICES };

/* the vote's choices: entry indices of the playlist, current (the game just played, -1 none) last as SAME AGAIN when it
is a valid entry. playable[i] 0 leaves a game out (this PC cannot play it; NULL: all can). state is a nonzero xorshift
state. Only 2 games or more make a vote: 0 when MAP VOTE is off or fewer than 2 games on other maps can be offered.
Returns the count (games, and SAME AGAIN) */
int ae_host_vote_choose(struct ae_playlist const *list, int current, unsigned char const *playable,
	unsigned long *state, int *choices);

#endif
