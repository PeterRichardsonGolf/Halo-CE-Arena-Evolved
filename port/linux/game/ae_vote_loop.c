/*
AE_VOTE_LOOP.C

See ae_vote_loop.h.
*/

#include "cseries.h"
#include "ae_vote_loop.h"
#include "ae_rotation_shuffle.h"

short ae_vote_loop_step(long const *entries, short count, long played)
{
	short winner = network_map_vote_host_update();

	if (winner == NONE)
		return NONE;
	/* (nobody voted: the first choice that is not the game just played) */
	if (network_map_vote_votes(winner) == 0)
		winner = (short)ae_rotation_vote_default(entries, count, played);
	return winner;
}
