/*
AE_VOTE_LOOP.H

The part of a map vote's loop a dedicated server (server_director.c's server_map_vote) and a game a player hosts
(ae_host_vote.c) share: one frame of the running vote, and the choice that plays next. Both begin their vote with
network_map_vote_begin (the host's own with network_map_vote_begin_local); the netcode, the clients' messages and the
rule that nobody voting plays the first choice that is not the game just played are the same.
*/

#ifndef AE_VOTE_LOOP_H
#define AE_VOTE_LOOP_H

#include "network_map_vote.h"

/* one frame of the running vote: NONE while it runs, else the choice (an index into entries, count of them) that plays
next. entries are whatever the caller numbers its choices by; played is the entry just played (NONE for none) */
short ae_vote_loop_step(long const *entries, short count, long played);

#endif
