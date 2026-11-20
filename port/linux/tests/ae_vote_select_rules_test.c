#include <stdio.h>

#include "ae_vote_select_rules.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* the first press selects, the second casts */
	CHECK(ae_vote_select_press(2, 4, 0, -1, 0) == _ae_vote_select_focus);
	CHECK(ae_vote_select_press(2, 4, 2, -1, 0) == _ae_vote_select_cast);
	/* a cast changed: another entry selects, then casts */
	CHECK(ae_vote_select_press(1, 4, 2, 2, 0) == _ae_vote_select_focus);
	CHECK(ae_vote_select_press(1, 4, 1, 2, 0) == _ae_vote_select_cast);
	/* the voted entry again: nothing to change */
	CHECK(ae_vote_select_press(2, 4, 2, 2, 0) == _ae_vote_select_nothing);
	/* out of range, or the vote over */
	CHECK(ae_vote_select_press(-1, 4, 0, -1, 0) == _ae_vote_select_nothing);
	CHECK(ae_vote_select_press(4, 4, 0, -1, 0) == _ae_vote_select_nothing);
	CHECK(ae_vote_select_press(1, 4, 1, -1, 1) == _ae_vote_select_nothing);
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
