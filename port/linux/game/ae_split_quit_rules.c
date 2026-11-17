/*
AE_SPLIT_QUIT_RULES.C

See ae_split_quit_rules.h.
*/

#include "ae_split_quit_rules.h"

int ae_split_quit_leaves(int is_local, int local_players, int coop, int film)
{
	return is_local && local_players >= 2 && !coop && !film;
}
