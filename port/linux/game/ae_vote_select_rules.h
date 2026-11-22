/*
AE_VOTE_SELECT_RULES.H

The map vote popover's two-step pick (0.7.8): a click or a press on an entry SELECTS it, a second one on the
same entry (or ACCEPT on the selected one) casts the vote. A cast can be changed until the vote closes. No engine
includes: the unit test in port/linux/tests/ae_vote_select_rules_test.c compiles these alone.
*/

#ifndef __AE_VOTE_SELECT_RULES_H
#define __AE_VOTE_SELECT_RULES_H

enum ae_vote_select_result
{
	_ae_vote_select_nothing = 0,
	_ae_vote_select_focus, /* the entry becomes the selected one, no vote */
	_ae_vote_select_cast,  /* the vote goes to the entry */
};

/* a click or press on `entry` (of `count`) with `selected` the entry already selected (-1: none), `voted` the
entry already voted for (-1: none), `over` set once the vote has a winner */
int ae_vote_select_press(int entry, int count, int selected, int voted, int over);

#endif
