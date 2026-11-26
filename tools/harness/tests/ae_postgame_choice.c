/*
AE_POSTGAME_CHOICE.C (test)

The carnage report's confirm (port/linux/game/ae_postgame_rules.c): B asks
"LEAVE THE GAME?" instead of leaving; A on it leaves, B on it stays; a press
is a run of frames held (the stock code reads held buttons), so each press
the confirm takes is waited out before the next counts; A with the confirm
closed is the stock code's (the host's pick game). test_ae_postgame.py
takes the types (config.inc) and the code (under_test.inc).
*/

#include "harness.h"
#include "config.inc"
#include "under_test.inc"

/* a press held for frames frames, then released for one: the actions it gave, each frame's in order */
static int press(struct ae_postgame_confirm *state, boolean accept, boolean back, int frames, int *actions)
{
	int frame, count = 0;

	for (frame = 0; frame < frames; frame++)
		actions[count++] = ae_postgame_confirm_step(state, accept, back);
	actions[count++] = ae_postgame_confirm_step(state, FALSE, FALSE);
	return count;
}

static int count_of(int const *actions, int count, int action)
{
	int index, found = 0;

	for (index = 0; index < count; index++)
		found += actions[index] == action;
	return found;
}

int main(int argc, char **argv)
{
	char const *case_name = argc > 1 ? argv[1] : "";
	struct ae_postgame_confirm state;
	int actions[64], count;

	memset(&state, 0, sizeof(state));
	/* B held for many frames opens the confirm once and never leaves */
	CASE("b_asks")
	{
		count = press(&state, FALSE, TRUE, 20, actions);
		CHECK(state.open, "B did not open the confirm");
		CHECK(!count_of(actions, count, AE_POSTGAME_LEAVE), "B left the game");
		CHECK(!count_of(actions, count, AE_POSTGAME_STOCK), "B reached the stock code (which leaves at once)");
		CHECK(actions[0] == AE_POSTGAME_HELD, "B's first frame was not the confirm's");
		return 0;
	}
	/* B, then A: leaves once, on A's first frame; A's held frames after it are the confirm's */
	CASE("a_leaves")
	{
		press(&state, FALSE, TRUE, 5, actions);
		count = press(&state, TRUE, FALSE, 10, actions);
		CHECK(actions[0] == AE_POSTGAME_LEAVE, "A on the confirm did not leave");
		CHECK(count_of(actions, count, AE_POSTGAME_LEAVE) == 1, "A on the confirm left more than once");
		CHECK(!count_of(actions, 10, AE_POSTGAME_STOCK), "A's held frames reached the stock code (pick game)");
		CHECK(!state.open, "the confirm stayed up after leaving");
		return 0;
	}
	/* B, then B again: stays on the carnage report, the confirm closed, and it does not open again while B is held */
	CASE("b_stays")
	{
		press(&state, FALSE, TRUE, 5, actions);
		count = press(&state, FALSE, TRUE, 10, actions);
		CHECK(!state.open, "B on the confirm did not close it");
		CHECK(!count_of(actions, count, AE_POSTGAME_LEAVE), "B on the confirm left the game");
		CHECK(!count_of(actions, 10, AE_POSTGAME_STOCK), "B's held frames reached the stock code (which leaves)");
		/* (and a third B asks again) */
		press(&state, FALSE, TRUE, 3, actions);
		CHECK(state.open, "a third B did not ask again");
		return 0;
	}
	/* A with the confirm closed is the stock code's (the host's pick game), as before */
	CASE("a_stock")
	{
		count = press(&state, TRUE, FALSE, 3, actions);
		CHECK(count_of(actions, count, AE_POSTGAME_STOCK) == count, "A without the confirm was not the stock code's");
		CHECK(!state.open, "A opened the confirm");
		count = press(&state, TRUE, TRUE, 3, actions);
		CHECK(actions[0] == AE_POSTGAME_STOCK && !state.open, "A with B was not A first, as the stock code reads them");
		return 0;
	}
	/* the confirm's B still held: A pressed meanwhile does nothing until both are up */
	CASE("release_first")
	{
		ae_postgame_confirm_step(&state, FALSE, TRUE);
		CHECK(ae_postgame_confirm_step(&state, TRUE, TRUE) == AE_POSTGAME_HELD, "A while the opening B is held counted");
		CHECK(state.open, "the confirm closed while its B was held");
		CHECK(ae_postgame_confirm_step(&state, FALSE, FALSE) == AE_POSTGAME_HELD, "nothing pressed was not the confirm's");
		CHECK(ae_postgame_confirm_step(&state, TRUE, FALSE) == AE_POSTGAME_LEAVE, "A after the release did not leave");
		return 0;
	}
	fprintf(stderr, "no case %s\n", case_name);
	return 2;
}
