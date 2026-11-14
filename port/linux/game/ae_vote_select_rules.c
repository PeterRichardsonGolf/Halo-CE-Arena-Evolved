/*
AE_VOTE_SELECT_RULES.C

See ae_vote_select_rules.h.
*/

#include "ae_vote_select_rules.h"

int ae_vote_select_press(int entry, int count, int selected, int voted, int over)
{
	if (over || entry < 0 || entry >= count)
		return _ae_vote_select_nothing;
	if (entry != selected)
		return _ae_vote_select_focus;
	/* (the selected entry already holds the vote: a press again has nothing to change) */
	if (entry == voted)
		return _ae_vote_select_nothing;
	return _ae_vote_select_cast;
}
