/*
AE_SERVER_VOTE.H

(0.7.6) A dedicated server's map vote always offers SAME AGAIN as its last choice (owner, mockups-076-ui2 case 7): the game
just played, once more. server_director.c's server_map_vote calls this after it has put its random choices in the vote
(one hook line).
*/

#ifndef AE_SERVER_VOTE_H
#define AE_SERVER_VOTE_H

#include "network_map_vote.h"
#include "../src/server_json.h"
#include "../src/server_rotation.h"

/* the vote's choices (count of them: their rotation entries, map and gametype names) with SAME AGAIN added: the entry
at current (the game just played; NONE for none) when it is a playable entry that is not among them already and there
is room. It is added to the vote's JSON event as a choice marked "same_again". Returns the new count */
short ae_server_vote_same_again(struct server_rotation const *rotation, long current, long *entries, short count,
	char (*map_names)[NETWORK_MAP_VOTE_MAP_SIZE], char (*gametypes)[NETWORK_MAP_VOTE_GAMETYPE_SIZE],
	struct server_json *json);

#endif
